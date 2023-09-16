/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file win32_m.cpp Music playback for Windows. */

#include "../stdafx.h"
#include "../string_func.h"
#include "win32_m.h"
#include <windows.h>
#include <mmsystem.h>
#include "../os/windows/win32.h"
#include "../debug.h"
#include "midifile.hpp"
#include "midi.h"
#include "../base_media_base.h"
#include "../core/mem_func.hpp"
#include <mutex>

#include "../safeguards.h"

struct PlaybackSegment {
	uint32_t start, end;
	size_t start_block;
	bool loop;
};

static struct {
	UINT time_period;    ///< obtained timer precision value
	HMIDIOUT midi_out;   ///< handle to open midiOut
	UINT timer_id;       ///< ID of active multimedia timer
	std::mutex lock;     ///< synchronization for playback status fields

	bool playing;        ///< flag indicating that playback is active
	int do_start;        ///< flag for starting playback of next_file at next opportunity
	bool do_stop;        ///< flag for stopping playback at next opportunity
	byte current_volume; ///< current effective volume setting
	byte new_volume;     ///< volume setting to change to

	MidiFile current_file;           ///< file currently being played from
	PlaybackSegment current_segment; ///< segment info for current playback
	DWORD playback_start_time;       ///< timestamp current file began playback
	size_t current_block;            ///< next block index to send
	MidiFile next_file;              ///< upcoming file to play
	PlaybackSegment next_segment;    ///< segment info for upcoming file

	byte channel_volumes[16]; ///< last seen volume controller values in raw data
} _midi;

static FMusicDriver_Win32 iFMusicDriver_Win32;


static byte ScaleVolume(byte original, byte scale)
{
	return original * scale / 127;
}


void CALLBACK MidiOutProc(HMIDIOUT hmo, UINT wMsg, DWORD_PTR, DWORD_PTR dwParam1, DWORD_PTR)
{
	if (wMsg == MOM_DONE) {
		MIDIHDR *hdr = (LPMIDIHDR)dwParam1;
		midiOutUnprepareHeader(hmo, hdr, sizeof(*hdr));
		free(hdr);
	}
}

static void TransmitChannelMsg(byte status, byte p1, byte p2 = 0)
{
	midiOutShortMsg(_midi.midi_out, status | (p1 << 8) | (p2 << 16));
}

static void TransmitSysex(const byte *&msg_start, size_t &remaining)
{
	/* find end of message */
	const byte *msg_end = msg_start;
	while (*msg_end != MIDIST_ENDSYSEX) msg_end++;
	msg_end++; /* also include sysex end byte */

	/* prepare header */
	MIDIHDR *hdr = CallocT<MIDIHDR>(1);
	hdr->lpData = reinterpret_cast<LPSTR>(const_cast<byte *>(msg_start));
	hdr->dwBufferLength = msg_end - msg_start;
	if (midiOutPrepareHeader(_midi.midi_out, hdr, sizeof(*hdr)) == MMSYSERR_NOERROR) {
		/* transmit - just point directly into the data buffer */
		hdr->dwBytesRecorded = hdr->dwBufferLength;
		midiOutLongMsg(_midi.midi_out, hdr, sizeof(*hdr));
	} else {
		free(hdr);
	}

	/* update position in buffer */
	remaining -= msg_end - msg_start;
	msg_start = msg_end;
}

static void TransmitStandardSysex(MidiSysexMessage msg)
{
	size_t length = 0;
	const byte *data = MidiGetStandardSysexMessage(msg, length);
	TransmitSysex(data, length);
}

/**
 * Realtime MIDI playback service routine.
 * This is called by the multimedia timer.
 */
void CALLBACK TimerCallback(UINT uTimerID, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
	/* Ensure only one timer callback is running at once, and prevent races on status flags */
	std::unique_lock<std::mutex> mutex_lock(_midi.lock, std::defer_lock);
	if (!mutex_lock.try_lock()) return;

	/* check for stop */
	if (_midi.do_stop) {
		Debug(driver, 2, "Win32-MIDI: timer: do_stop is set");
		midiOutReset(_midi.midi_out);
		_midi.playing = false;
		_midi.do_stop = false;
		return;
	}

	/* check for start/restart/change song */
	if (_midi.do_start != 0) {
		/* Have a delay between playback start steps, prevents jumbled-together notes at the start of song */
		if (timeGetTime() - _midi.playback_start_time < 50) {
			return;
		}
		Debug(driver, 2, "Win32-MIDI: timer: do_start step {}", _midi.do_start);

		if (_midi.do_start == 1) {
			/* Send "all notes off" */
			midiOutReset(_midi.midi_out);
			_midi.playback_start_time = timeGetTime();
			_midi.do_start = 2;

			return;
		} else if (_midi.do_start == 2) {
			/* Reset the device to General MIDI defaults */
			TransmitStandardSysex(MidiSysexMessage::ResetGM);
			_midi.playback_start_time = timeGetTime();
			_midi.do_start = 3;

			return;
		} else if (_midi.do_start == 3) {
			/* Set up device-specific effects */
			TransmitStandardSysex(MidiSysexMessage::RolandSetReverb);
			_midi.playback_start_time = timeGetTime();
			_midi.do_start = 4;

			return;
		} else if (_midi.do_start == 4) {
			/* Load the new file */
			_midi.current_file.MoveFrom(_midi.next_file);
			std::swap(_midi.next_segment, _midi.current_segment);
			_midi.current_segment.start_block = 0;
			_midi.playback_start_time = timeGetTime();
			_midi.playing = true;
			_midi.do_start = 0;
			_midi.current_block = 0;

			MemSetT<byte>(_midi.channel_volumes, 127, lengthof(_midi.channel_volumes));
		}
	} else if (!_midi.playing) {
		/* not playing, stop the timer */
		Debug(driver, 2, "Win32-MIDI: timer: not playing, stopping timer");
		timeKillEvent(uTimerID);
		_midi.timer_id = 0;
		return;
	}

	/* check for volume change */
	static int volume_throttle = 0;
	if (_midi.current_volume != _midi.new_volume) {
		if (volume_throttle == 0) {
			Debug(driver, 2, "Win32-MIDI: timer: volume change");
			_midi.current_volume = _midi.new_volume;
			volume_throttle = 20 / _midi.time_period;
			for (int ch = 0; ch < 16; ch++) {
				byte vol = ScaleVolume(_midi.channel_volumes[ch], _midi.current_volume);
				TransmitChannelMsg(MIDIST_CONTROLLER | ch, MIDICT_CHANVOLUME, vol);
			}
		} else {
			volume_throttle--;
		}
	}

	/* skip beginning of file? */
	if (_midi.current_segment.start > 0 && _midi.current_block == 0 && _midi.current_segment.start_block == 0) {
		/* find first block after start time and pretend playback started earlier
		 * this is to allow all blocks prior to the actual start to still affect playback,
		 * as they may contain important controller and program changes */
		size_t preload_bytes = 0;
		for (size_t bl = 0; bl < _midi.current_file.blocks.size(); bl++) {
			MidiFile::DataBlock &block = _midi.current_file.blocks[bl];
			preload_bytes += block.data.size();
			if (block.ticktime >= _midi.current_segment.start) {
				if (_midi.current_segment.loop) {
					Debug(driver, 2, "Win32-MIDI: timer: loop from block {} (ticktime {}, realtime {:.3f}, bytes {})", bl, block.ticktime, ((int)block.realtime)/1000.0, preload_bytes);
					_midi.current_segment.start_block = bl;
					break;
				} else {
					/* Calculate offset start time for playback.
					 * The preload_bytes are used to compensate for delay in transmission over traditional serial MIDI interfaces,
					 * which have a bitrate of 31,250 bits/sec, and transmit 1+8+1 start/data/stop bits per byte.
					 * The delay compensation is needed to avoid time-compression of following messages.
					 */
					Debug(driver, 2, "Win32-MIDI: timer: start from block {} (ticktime {}, realtime {:.3f}, bytes {})", bl, block.ticktime, ((int)block.realtime) / 1000.0, preload_bytes);
					_midi.playback_start_time -= block.realtime / 1000 - (DWORD)(preload_bytes * 1000 / 3125);
					break;
				}
			}
		}
	}


	/* play pending blocks */
	DWORD current_time = timeGetTime();
	DWORD playback_time = current_time - _midi.playback_start_time;
	while (_midi.current_block < _midi.current_file.blocks.size()) {
		MidiFile::DataBlock &block = _midi.current_file.blocks[_midi.current_block];

		/* check that block isn't at end-of-song override */
		if (_midi.current_segment.end > 0 && block.ticktime >= _midi.current_segment.end) {
			if (_midi.current_segment.loop) {
				_midi.current_block = _midi.current_segment.start_block;
				_midi.playback_start_time = timeGetTime() - _midi.current_file.blocks[_midi.current_block].realtime / 1000;
			} else {
				_midi.do_stop = true;
			}
			break;
		}
		/* check that block is not in the future */
		if (block.realtime / 1000 > playback_time) {
			break;
		}

		const byte *data = block.data.data();
		size_t remaining = block.data.size();
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
					TransmitChannelMsg(status, data[0]);
					data++;
					remaining--;
					break;
				case MIDIST_NOTEOFF:
				case MIDIST_NOTEON:
				case MIDIST_POLYPRESS:
				case MIDIST_PITCHBEND:
					/* 3 byte channel messages */
					TransmitChannelMsg(status, data[0], data[1]);
					data += 2;
					remaining -= 2;
					break;
				case MIDIST_CONTROLLER:
					/* controller change */
					if (data[0] == MIDICT_CHANVOLUME) {
						/* volume controller, adjust for user volume */
						_midi.channel_volumes[status & 0x0F] = data[1];
						int vol = ScaleVolume(data[1], _midi.current_volume);
						TransmitChannelMsg(status, data[0], vol);
					} else {
						/* handle other controllers normally */
						TransmitChannelMsg(status, data[0], data[1]);
					}
					data += 2;
					remaining -= 2;
					break;
				case 0xF0:
					/* system messages */
					switch (status) {
						case MIDIST_SYSEX: /* system exclusive */
							TransmitSysex(data, remaining);
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

		_midi.current_block++;
	}

	/* end? */
	if (_midi.current_block == _midi.current_file.blocks.size()) {
		if (_midi.current_segment.loop) {
			_midi.current_block = _midi.current_segment.start_block;
			_midi.playback_start_time = timeGetTime() - _midi.current_file.blocks[_midi.current_block].realtime / 1000;
		} else {
			_midi.do_stop = true;
		}
	}
}

void MusicDriver_Win32::PlaySong(const MusicSongInfo &song)
{
	Debug(driver, 2, "Win32-MIDI: PlaySong: entry");

	MidiFile new_song;
	if (!new_song.LoadSong(song)) return;
	Debug(driver, 2, "Win32-MIDI: PlaySong: Loaded song");

	std::lock_guard<std::mutex> mutex_lock(_midi.lock);

	_midi.next_file.MoveFrom(new_song);
	_midi.next_segment.start = song.override_start;
	_midi.next_segment.end = song.override_end;
	_midi.next_segment.loop = song.loop;

	Debug(driver, 2, "Win32-MIDI: PlaySong: setting flag");
	_midi.do_stop = _midi.playing;
	_midi.do_start = 1;

	if (_midi.timer_id == 0) {
		Debug(driver, 2, "Win32-MIDI: PlaySong: starting timer");
		_midi.timer_id = timeSetEvent(_midi.time_period, _midi.time_period, TimerCallback, (DWORD_PTR)this, TIME_PERIODIC | TIME_CALLBACK_FUNCTION);
	}
}

void MusicDriver_Win32::StopSong()
{
	Debug(driver, 2, "Win32-MIDI: StopSong: entry");
	std::lock_guard<std::mutex> mutex_lock(_midi.lock);
	Debug(driver, 2, "Win32-MIDI: StopSong: setting flag");
	_midi.do_stop = true;
}

bool MusicDriver_Win32::IsSongPlaying()
{
	return _midi.playing || (_midi.do_start != 0);
}

void MusicDriver_Win32::SetVolume(byte vol)
{
	std::lock_guard<std::mutex> mutex_lock(_midi.lock);
	_midi.new_volume = vol;
}

const char *MusicDriver_Win32::Start(const StringList &parm)
{
	Debug(driver, 2, "Win32-MIDI: Start: initializing");

	int resolution = GetDriverParamInt(parm, "resolution", 5);
	uint port = (uint)GetDriverParamInt(parm, "port", UINT_MAX);
	const char *portname = GetDriverParam(parm, "portname");

	/* Enumerate ports either for selecting port by name, or for debug output */
	if (portname != nullptr || _debug_driver_level > 0) {
		uint numports = midiOutGetNumDevs();
		Debug(driver, 1, "Win32-MIDI: Found {} output devices:", numports);
		for (uint tryport = 0; tryport < numports; tryport++) {
			MIDIOUTCAPS moc{};
			if (midiOutGetDevCaps(tryport, &moc, sizeof(moc)) == MMSYSERR_NOERROR) {
				char tryportname[128];
				convert_from_fs(moc.szPname, tryportname, lengthof(tryportname));

				/* Compare requested and detected port name.
				 * If multiple ports have the same name, this will select the last matching port, and the debug output will be confusing. */
				if (portname != nullptr && strncmp(tryportname, portname, lengthof(tryportname)) == 0) port = tryport;

				Debug(driver, 1, "MIDI port {:2d}: {}{}", tryport, tryportname, (tryport == port) ? " [selected]" : "");
			}
		}
	}

	UINT devid;
	if (port == UINT_MAX) {
		devid = MIDI_MAPPER;
	} else {
		devid = (UINT)port;
	}

	resolution = Clamp(resolution, 1, 20);

	if (midiOutOpen(&_midi.midi_out, devid, (DWORD_PTR)&MidiOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
		return "could not open midi device";
	}

	midiOutReset(_midi.midi_out);

	/* prepare multimedia timer */
	TIMECAPS timecaps;
	if (timeGetDevCaps(&timecaps, sizeof(timecaps)) == MMSYSERR_NOERROR) {
		_midi.time_period = std::min(std::max((UINT)resolution, timecaps.wPeriodMin), timecaps.wPeriodMax);
		if (timeBeginPeriod(_midi.time_period) == MMSYSERR_NOERROR) {
			/* success */
			Debug(driver, 2, "Win32-MIDI: Start: timer resolution is {}", _midi.time_period);
			return nullptr;
		}
	}
	midiOutClose(_midi.midi_out);
	return "could not set timer resolution";
}

void MusicDriver_Win32::Stop()
{
	std::lock_guard<std::mutex> mutex_lock(_midi.lock);

	if (_midi.timer_id) {
		timeKillEvent(_midi.timer_id);
		_midi.timer_id = 0;
	}

	timeEndPeriod(_midi.time_period);
	midiOutReset(_midi.midi_out);
	midiOutClose(_midi.midi_out);
}
