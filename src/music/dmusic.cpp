/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dmusic.cpp Playing music via DirectMusic. */

#ifdef WIN32_ENABLE_DIRECTMUSIC_SUPPORT

#define INITGUID
#include "../stdafx.h"
#ifdef WIN32_LEAN_AND_MEAN
	#undef WIN32_LEAN_AND_MEAN // Don't exclude rarely-used stuff from Windows headers
#endif
#include "../debug.h"
#include "../os/windows/win32.h"
#include "../core/mem_func.hpp"
#include "../thread/thread.h"
#include "dmusic.h"
#include "midifile.hpp"
#include "midi.h"

#include <windows.h>
#undef FACILITY_DIRECTMUSIC // Needed for newer Windows SDK version.
#include <dmksctrl.h>
#include <dmusici.h>
#include <dmusicc.h>

#include "../safeguards.h"


static const int MS_TO_REFTIME = 1000 * 10; ///< DirectMusic time base is 100 ns.
static const int MIDITIME_TO_REFTIME = 10;  ///< Time base of the midi file reader is 1 us.


struct PlaybackSegment {
	uint32 start, end;
	size_t start_block;
	bool loop;
};

static struct {
	bool shutdown;    ///< flag to indicate playback thread shutdown
	bool playing;     ///< flag indicating that playback is active
	bool do_start;    ///< flag for starting playback of next_file at next opportunity
	bool do_stop;     ///< flag for stopping playback at next opportunity

	int preload_time; ///< preload time for music blocks.
	byte new_volume;  ///< volume setting to change to

	MidiFile next_file;           ///< upcoming file to play
	PlaybackSegment next_segment; ///< segment info for upcoming file
} _playback;

/** Handle to our worker thread. */
static ThreadObject *_dmusic_thread = NULL;
/** Event to signal the thread that it should look at a state change. */
static HANDLE _thread_event = NULL;
/** Lock access to playback data that is not thread-safe. */
static ThreadMutex *_thread_mutex = NULL;

/** The direct music object manages buffers and ports. */
static IDirectMusic *_music = NULL;
/** The port object lets us send MIDI data to the synthesizer. */
static IDirectMusicPort *_port = NULL;
/** The buffer object collects the data to sent. */
static IDirectMusicBuffer *_buffer = NULL;
/** List of downloaded DLS instruments. */
static std::vector<IDirectMusicDownloadedInstrument *> _loaded_instruments;


static FMusicDriver_DMusic iFMusicDriver_DMusic;




static byte ScaleVolume(byte original, byte scale)
{
	return original * scale / 127;
}

static void TransmitChannelMsg(IDirectMusicBuffer *buffer, REFERENCE_TIME rt, byte status, byte p1, byte p2 = 0)
{
	if (buffer->PackStructured(rt, 0, status | (p1 << 8) | (p2 << 16)) == E_OUTOFMEMORY) {
		/* Buffer is full, clear it and try again. */
		_port->PlayBuffer(buffer);
		buffer->Flush();

		buffer->PackStructured(rt, 0, status | (p1 << 8) | (p2 << 16));
	}
}

static void TransmitSysex(IDirectMusicBuffer *buffer, REFERENCE_TIME rt, byte *&msg_start, size_t &remaining)
{
	/* Find end of message. */
	byte *msg_end = msg_start;
	while (*msg_end != MIDIST_ENDSYSEX) msg_end++;
	msg_end++; // Also include SysEx end byte.

	if (buffer->PackUnstructured(rt, 0, msg_end - msg_start, msg_start) == E_OUTOFMEMORY) {
		/* Buffer is full, clear it and try again. */
		_port->PlayBuffer(buffer);
		buffer->Flush();

		buffer->PackUnstructured(rt, 0, msg_end - msg_start, msg_start);
	}

	/* Update position in buffer. */
	remaining -= msg_end - msg_start;
	msg_start = msg_end;
}

static void TransmitSysexConst(IDirectMusicBuffer *buffer, REFERENCE_TIME rt, byte *msg_start, size_t length)
{
	TransmitSysex(buffer, rt, msg_start, length);
}

/** Transmit 'Note off' messages to all MIDI channels. */
static void TransmitNotesOff(IDirectMusicBuffer *buffer, REFERENCE_TIME block_time, REFERENCE_TIME cur_time)
{
	for (int ch = 0; ch < 16; ch++) {
		TransmitChannelMsg(_buffer, block_time + 10, MIDIST_CONTROLLER | ch, MIDICT_MODE_ALLNOTESOFF, 0);
		TransmitChannelMsg(_buffer, block_time + 10, MIDIST_CONTROLLER | ch, MIDICT_SUSTAINSW, 0);
		TransmitChannelMsg(_buffer, block_time + 10, MIDIST_CONTROLLER | ch, MIDICT_MODE_RESETALLCTRL, 0);
	}
	/* Explicitly flush buffer to make sure the note off messages are processed
	 * before we send any additional control messages. */
	_port->PlayBuffer(_buffer);
	_buffer->Flush();

	/* Some songs change the "Pitch bend range" registered parameter. If
	 * this doesn't get reset, everything else will start sounding wrong. */
	for (int ch = 0; ch < 16; ch++) {
		/* Running status, only need status for first message
		 * Select RPN 00.00, set value to 02.00, and de-select again */
		TransmitChannelMsg(_buffer, block_time + 10, MIDIST_CONTROLLER | ch, MIDICT_RPN_SELECT_LO, 0x00);
		TransmitChannelMsg(_buffer, block_time + 10, MIDICT_RPN_SELECT_HI, 0x00);
		TransmitChannelMsg(_buffer, block_time + 10, MIDICT_DATAENTRY, 0x02);
		TransmitChannelMsg(_buffer, block_time + 10, MIDICT_DATAENTRY_LO, 0x00);
		TransmitChannelMsg(_buffer, block_time + 10, MIDICT_RPN_SELECT_LO, 0x7F);
		TransmitChannelMsg(_buffer, block_time + 10, MIDICT_RPN_SELECT_HI, 0x7F);

		_port->PlayBuffer(_buffer);
		_buffer->Flush();
	}

	/* Wait until message time has passed. */
	Sleep(Clamp((block_time - cur_time) / MS_TO_REFTIME, 5, 1000));
}

static void MidiThreadProc(void *)
{
	DEBUG(driver, 2, "DMusic: Entering playback thread");

	REFERENCE_TIME last_volume_time = 0; // timestamp of the last volume change
	REFERENCE_TIME block_time = 0;       // timestamp of the last block sent to the port
	REFERENCE_TIME playback_start_time;  // timestamp current file began playback
	MidiFile current_file;               // file currently being played from
	PlaybackSegment current_segment;     // segment info for current playback
	size_t current_block;                // next block index to send
	byte current_volume = 0;             // current effective volume setting
	byte channel_volumes[16];            // last seen volume controller values in raw data

	/* Get pointer to the reference clock of our output port. */
	IReferenceClock *clock;
	_port->GetLatencyClock(&clock);

	REFERENCE_TIME cur_time;
	clock->GetTime(&cur_time);

	/* Standard "Enable General MIDI" message */
	static byte gm_enable_sysex[] = { 0xF0, 0x7E, 0x00, 0x09, 0x01, 0xF7 };
	TransmitSysexConst(_buffer, cur_time, &gm_enable_sysex[0], sizeof(gm_enable_sysex));
	/* Roland-specific reverb room control, used by the original game */
	static byte roland_reverb_sysex[] = { 0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x01, 0x30, 0x02, 0x04, 0x00, 0x40, 0x40, 0x00, 0x00, 0x09, 0xF7 };
	TransmitSysexConst(_buffer, cur_time, &roland_reverb_sysex[0], sizeof(roland_reverb_sysex));

	_port->PlayBuffer(_buffer);
	_buffer->Flush();

	DWORD next_timeout = 1000;
	while (true) {
		/* Wait for a signal from the GUI thread or until the time for the next event has come. */
		DWORD wfso = WaitForSingleObject(_thread_event, next_timeout);

		if (_playback.shutdown) {
			_playback.playing = false;
			break;
		}

		if (_playback.do_stop) {
			DEBUG(driver, 2, "DMusic thread: Stopping playback");

			/* Turn all notes off and wait a bit to allow the messages to be handled. */
			clock->GetTime(&cur_time);
			TransmitNotesOff(_buffer, block_time, cur_time);

			_playback.playing = false;
			_playback.do_stop = false;
			block_time = 0;
			next_timeout = 1000;
			continue;
		}

		if (wfso == WAIT_OBJECT_0) {
			if (_playback.do_start) {
				DEBUG(driver, 2, "DMusic thread: Starting playback");
				{
					/* New scope to limit the time the mutex is locked. */
					ThreadMutexLocker lock(_thread_mutex);

					current_file.MoveFrom(_playback.next_file);
					std::swap(_playback.next_segment, current_segment);
					current_segment.start_block = 0;
					current_block = 0;
					_playback.playing = true;
					_playback.do_start = false;
				}

				/* Turn all notes off in case we are seeking between music titles. */
				clock->GetTime(&cur_time);
				TransmitNotesOff(_buffer, block_time, cur_time);

				MemSetT<byte>(channel_volumes, 127, lengthof(channel_volumes));

				/* Take the current time plus the preload time as the music start time. */
				clock->GetTime(&playback_start_time);
				playback_start_time += _playback.preload_time * MS_TO_REFTIME;
			}
		}

		if (_playback.playing) {
			/* skip beginning of file? */
			if (current_segment.start > 0 && current_block == 0 && current_segment.start_block == 0) {
				/* find first block after start time and pretend playback started earlier
				 * this is to allow all blocks prior to the actual start to still affect playback,
				 * as they may contain important controller and program changes */
				size_t preload_bytes = 0;
				for (size_t bl = 0; bl < current_file.blocks.size(); bl++) {
					MidiFile::DataBlock &block = current_file.blocks[bl];
					preload_bytes += block.data.Length();
					if (block.ticktime >= current_segment.start) {
						if (current_segment.loop) {
							DEBUG(driver, 2, "DMusic: timer: loop from block %d (ticktime %d, realtime %.3f, bytes %d)", (int)bl, (int)block.ticktime, ((int)block.realtime) / 1000.0, (int)preload_bytes);
							current_segment.start_block = bl;
							break;
						} else {
							DEBUG(driver, 2, "DMusic: timer: start from block %d (ticktime %d, realtime %.3f, bytes %d)", (int)bl, (int)block.ticktime, ((int)block.realtime) / 1000.0, (int)preload_bytes);
							playback_start_time -= block.realtime * MIDITIME_TO_REFTIME;
							break;
						}
					}
				}
			}

			/* Get current playback timestamp. */
			REFERENCE_TIME current_time;
			clock->GetTime(&current_time);

			/* Check for volume change. */
			if (current_volume != _playback.new_volume) {
				if (current_time - last_volume_time > 10 * MS_TO_REFTIME) {
					DEBUG(driver, 2, "DMusic thread: volume change");
					current_volume = _playback.new_volume;
					last_volume_time = current_time;
					for (int ch = 0; ch < 16; ch++) {
						int vol = ScaleVolume(channel_volumes[ch], current_volume);
						TransmitChannelMsg(_buffer, block_time + 1, MIDIST_CONTROLLER | ch, MIDICT_CHANVOLUME, vol);
					}
					_port->PlayBuffer(_buffer);
					_buffer->Flush();
				}
			}

			while (current_block < current_file.blocks.size()) {
				MidiFile::DataBlock &block = current_file.blocks[current_block];

				/* check that block is not in the future */
				REFERENCE_TIME playback_time = current_time - playback_start_time;
				if (block.realtime * MIDITIME_TO_REFTIME > playback_time +  3 *_playback.preload_time * MS_TO_REFTIME) {
					/* Stop the thread loop until we are at the preload time of the next block. */
					next_timeout = Clamp(((int64)block.realtime * MIDITIME_TO_REFTIME - playback_time) / MS_TO_REFTIME - _playback.preload_time, 0, 1000);
					DEBUG(driver, 9, "DMusic thread: Next event in %u ms (music %u, ref %lld)", next_timeout, block.realtime * MIDITIME_TO_REFTIME, playback_time);
					break;
				}
				/* check that block isn't at end-of-song override */
				if (current_segment.end > 0 && block.ticktime >= current_segment.end) {
					if (current_segment.loop) {
						DEBUG(driver, 2, "DMusic thread: Looping song");
						current_block = current_segment.start_block;
						playback_start_time = current_time - current_file.blocks[current_block].realtime * MIDITIME_TO_REFTIME;
					} else {
						_playback.do_stop = true;
					}
					next_timeout = 0;
					break;
				}

				/* Timestamp of the current block. */
				block_time = playback_start_time + block.realtime * MIDITIME_TO_REFTIME;
				DEBUG(driver, 9, "DMusic thread: Streaming block %Iu (cur=%lld, block=%lld)", current_block, (long long)(current_time / MS_TO_REFTIME), (long long)(block_time / MS_TO_REFTIME));

				byte *data = block.data.Begin();
				size_t remaining = block.data.Length();
				byte last_status = 0;
				while (remaining > 0) {
					/* MidiFile ought to have converted everything out of running status,
					 * but handle it anyway just to be safe */
					byte status = data[0];
					if (status & 0x80) {
						last_status = status;
						data++;
						remaining--;
					} else {
						status = last_status;
					}
					switch (status & 0xF0) {
						case MIDIST_PROGCHG:
						case MIDIST_CHANPRESS:
							/* 2 byte channel messages */
							TransmitChannelMsg(_buffer, block_time, status, data[0]);
							data++;
							remaining--;
							break;
						case MIDIST_NOTEOFF:
						case MIDIST_NOTEON:
						case MIDIST_POLYPRESS:
						case MIDIST_PITCHBEND:
							/* 3 byte channel messages */
							TransmitChannelMsg(_buffer, block_time, status, data[0], data[1]);
							data += 2;
							remaining -= 2;
							break;
						case MIDIST_CONTROLLER:
							/* controller change */
							if (data[0] == MIDICT_CHANVOLUME) {
								/* volume controller, adjust for user volume */
								channel_volumes[status & 0x0F] = data[1];
								int vol = ScaleVolume(data[1], current_volume);
								TransmitChannelMsg(_buffer, block_time, status, data[0], vol);
							} else {
								/* handle other controllers normally */
								TransmitChannelMsg(_buffer, block_time, status, data[0], data[1]);
							}
							data += 2;
							remaining -= 2;
							break;
						case 0xF0:
							/* system messages */
							switch (status) {
								case MIDIST_SYSEX: /* system exclusive */
									TransmitSysex(_buffer, block_time, data, remaining);
									break;
								case MIDIST_TC_QFRAME: /* time code quarter frame */
								case MIDIST_SONGSEL: /* song select */
									data++;
									remaining--;
									break;
								case MIDIST_SONGPOSPTR: /* song position pointer */
									data += 2;
									remaining -= 2;
									break;
								default: /* remaining have no data bytes */
									break;
							}
							break;
					}
				}

				current_block++;
			}

			/* Anything in the playback buffer? Send it down the port. */
			DWORD used_buffer = 0;
			_buffer->GetUsedBytes(&used_buffer);
			if (used_buffer > 0) {
				_port->PlayBuffer(_buffer);
				_buffer->Flush();
			}

			/* end? */
			if (current_block == current_file.blocks.size()) {
				if (current_segment.loop) {
					current_block = 0;
					clock->GetTime(&playback_start_time);
				} else {
					_playback.do_stop = true;
				}
				next_timeout = 0;
			}
		}
	}

	DEBUG(driver, 2, "DMusic: Exiting playback thread");

	/* Turn all notes off and wait a bit to allow the messages to be handled by real hardware. */
	clock->GetTime(&cur_time);
	TransmitNotesOff(_buffer, block_time, cur_time);
	Sleep(_playback.preload_time * 4);

	clock->Release();
}

static const char *LoadDefaultDLSFile()
{
	DMUS_PORTCAPS caps;
	MemSetT(&caps, 0);
	caps.dwSize = sizeof(DMUS_PORTCAPS);
	_port->GetCaps(&caps);

	if ((caps.dwFlags & (DMUS_PC_DLS | DMUS_PC_DLS2)) != 0 && (caps.dwFlags & DMUS_PC_GMINHARDWARE) == 0) {
		/* DLS synthesizer and no built-in GM sound set, need to download one. */
		IDirectMusicLoader *loader = NULL;
		proc.CoCreateInstance(CLSID_DirectMusicLoader, NULL, CLSCTX_INPROC, IID_IDirectMusicLoader, (LPVOID *)&loader);
		if (loader == NULL) return "Can't create DLS loader";

		DMUS_OBJECTDESC desc;
		MemSetT(&desc, 0);
		desc.dwSize = sizeof(DMUS_OBJECTDESC);
		desc.guidClass = CLSID_DirectMusicCollection;
		desc.guidObject = GUID_DefaultGMCollection;
		desc.dwValidData = (DMUS_OBJ_CLASS | DMUS_OBJ_OBJECT);

		IDirectMusicCollection *collection = NULL;
		if (FAILED(loader->GetObjectW(&desc, IID_IDirectMusicCollection, (LPVOID *)&collection))) {
			loader->Release();
			return "Can't load GM DLS collection";
		}

		/* Download instruments to synthesizer. */
		DWORD idx = 0;
		DWORD patch = 0;
		while (collection->EnumInstrument(idx++, &patch, NULL, 0) == S_OK) {
			IDirectMusicInstrument *instrument = NULL;
			if (SUCCEEDED(collection->GetInstrument(patch, &instrument))) {
				IDirectMusicDownloadedInstrument *loaded = NULL;
				if (SUCCEEDED(_port->DownloadInstrument(instrument, &loaded, NULL, 0))) {
					_loaded_instruments.push_back(loaded);
				}
				instrument->Release();
			}
		}

		collection->Release();
		loader->Release();
	}

	return NULL;
}


const char *MusicDriver_DMusic::Start(const char * const *parm)
{
	/* Initialize COM */
	if (FAILED(CoInitializeEx(NULL, COINITBASE_MULTITHREADED))) return "COM initialization failed";

	/* Create the DirectMusic object */
	if (FAILED(CoCreateInstance(
				CLSID_DirectMusic,
				NULL,
				CLSCTX_INPROC,
				IID_IDirectMusic,
				(LPVOID*)&_music
			))) {
		return "Failed to create the music object";
	}

	/* Assign sound output device. */
	if (FAILED(_music->SetDirectSound(NULL, NULL))) return "Can't set DirectSound interface";

	/* MIDI events need to be send to the synth in time before their playback time
	 * has come. By default, we try send any events at least 50 ms before playback. */
	_playback.preload_time = GetDriverParamInt(parm, "preload", 50);

	int pIdx = GetDriverParamInt(parm, "port", -1);
	if (_debug_driver_level > 0) {
		/* Print all valid output ports. */
		char desc[DMUS_MAX_DESCRIPTION];

		DMUS_PORTCAPS caps;
		MemSetT(&caps, 0);
		caps.dwSize = sizeof(DMUS_PORTCAPS);

		DEBUG(driver, 1, "Detected DirectMusic ports:");
		for (int i = 0; _music->EnumPort(i, &caps) == S_OK; i++) {
			if (caps.dwClass == DMUS_PC_OUTPUTCLASS) {
				/* Description is UNICODE even for ANSI build. */
				DEBUG(driver, 1, " %d: %s%s", i, convert_from_fs(caps.wszDescription, desc, lengthof(desc)), i == pIdx ? " (selected)" : "");
			}
		}
	}

	GUID guidPort;
	if (pIdx >= 0) {
		/* Check if the passed port is a valid port. */
		DMUS_PORTCAPS caps;
		MemSetT(&caps, 0);
		caps.dwSize = sizeof(DMUS_PORTCAPS);
		if (FAILED(_music->EnumPort(pIdx, &caps))) return "Supplied port parameter is not a valid port";
		if (caps.dwClass != DMUS_PC_OUTPUTCLASS) return "Supplied port parameter is not an output port";
		guidPort = caps.guidPort;
	} else {
		if (FAILED(_music->GetDefaultPort(&guidPort))) return "Can't query default music port";
	}

	/* Create new port. */
	DMUS_PORTPARAMS params;
	MemSetT(&params, 0);
	params.dwSize = sizeof(DMUS_PORTPARAMS);
	params.dwValidParams = DMUS_PORTPARAMS_CHANNELGROUPS;
	params.dwChannelGroups = 1;
	if (FAILED(_music->CreatePort(guidPort, &params, &_port, NULL))) return "Failed to create port";
	/* Activate port. */
	if (FAILED(_port->Activate(TRUE))) return "Failed to activate port";

	/* Create playback buffer. */
	DMUS_BUFFERDESC desc;
	MemSetT(&desc, 0);
	desc.dwSize = sizeof(DMUS_BUFFERDESC);
	desc.guidBufferFormat = KSDATAFORMAT_SUBTYPE_DIRECTMUSIC;
	desc.cbBuffer = 1024;
	if (FAILED(_music->CreateMusicBuffer(&desc, &_buffer, NULL))) return "Failed to create music buffer";

	const char *dls = LoadDefaultDLSFile();
	if (dls != NULL) return dls;

	/* Create playback thread and synchronization primitives. */
	_thread_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (_thread_event == NULL) return "Can't create thread shutdown event";
	_thread_mutex = ThreadMutex::New();
	if (_thread_mutex == NULL) return "Can't create thread mutex";

	if (!ThreadObject::New(&MidiThreadProc, this, &_dmusic_thread, "ottd:dmusic")) return "Can't create MIDI output thread";

	return NULL;
}


MusicDriver_DMusic::~MusicDriver_DMusic()
{
	this->Stop();
}


void MusicDriver_DMusic::Stop()
{
	if (_dmusic_thread != NULL) {
		_playback.shutdown = true;
		SetEvent(_thread_event);
		_dmusic_thread->Join();
	}

	/* Unloaded any instruments we loaded. */
	for (std::vector<IDirectMusicDownloadedInstrument *>::iterator i = _loaded_instruments.begin(); i != _loaded_instruments.end(); i++) {
		_port->UnloadInstrument(*i);
		(*i)->Release();
	}
	_loaded_instruments.clear();

	if (_buffer != NULL) {
		_buffer->Release();
		_buffer = NULL;
	}

	if (_port != NULL) {
		_port->Activate(FALSE);
		_port->Release();
		_port = NULL;
	}

	if (_music != NULL) {
		_music->Release();
		_music = NULL;
	}

	CloseHandle(_thread_event);
	delete _thread_mutex;

	CoUninitialize();
}


void MusicDriver_DMusic::PlaySong(const char *filename)
{
	ThreadMutexLocker lock(_thread_mutex);

	_playback.next_file.LoadFile(filename);
	_playback.next_segment.start = 0;
	_playback.next_segment.end = 0;
	_playback.next_segment.loop = false;

	_playback.do_start = true;
	SetEvent(_thread_event);
}


void MusicDriver_DMusic::StopSong()
{
	_playback.do_stop = true;
	SetEvent(_thread_event);
}


bool MusicDriver_DMusic::IsSongPlaying()
{
	return _playback.playing || _playback.do_start;
}


void MusicDriver_DMusic::SetVolume(byte vol)
{
	_playback.new_volume = vol;
}


#endif /* WIN32_ENABLE_DIRECTMUSIC_SUPPORT */
