/* $Id$ */

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
#include "../base_media_base.h"

#include "../safeguards.h"

struct PlaybackSegment {
	uint32 start, end;
	size_t start_block;
	bool loop;
};

static struct {
	UINT time_period;
	HMIDIOUT midi_out;
	UINT timer_id;
	CRITICAL_SECTION lock;

	bool playing;        ///< flag indicating that playback is active
	bool do_start;       ///< flag for starting playback of next_file at next opportunity
	bool do_stop;        ///< flag for stopping playback at next opportunity
	byte current_volume; ///< current effective volume setting
	byte new_volume;     ///< volume setting to change to

	MidiFile current_file;           ///< file currently being played from
	PlaybackSegment current_segment; ///< segment info for current playback
	DWORD playback_start_time;       ///< timestamp current file started playing back
	size_t current_block;            ///< next block index to send
	MidiFile next_file;              ///< upcoming file to play
	PlaybackSegment next_segment;    ///< segment info for upcoming file

	byte channel_volumes[16];
} _midi;

static FMusicDriver_Win32 iFMusicDriver_Win32;


static byte ScaleVolume(byte original, byte scale)
{
	return original * scale / 127;
}


void CALLBACK MidiOutProc(HMIDIOUT hmo, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
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

static void TransmitSysex(byte *&msg_start, size_t &remaining)
{
	/* find end of message */
	byte *msg_end = msg_start;
	while (*msg_end != 0xF7) msg_end++;
	msg_end++; /* also include sysex end byte */

	/* prepare header */
	MIDIHDR *hdr = CallocT<MIDIHDR>(1);
	if (midiOutPrepareHeader(_midi.midi_out, hdr, sizeof(*hdr)) == MMSYSERR_NOERROR) {
		/* transmit - just point directly into the data buffer */
		hdr->lpData = (LPSTR)msg_start;
		hdr->dwBufferLength = msg_end - msg_start;
		hdr->dwBytesRecorded = hdr->dwBufferLength;
		midiOutLongMsg(_midi.midi_out, hdr, sizeof(*hdr));
	} else {
		free(hdr);
	}

	/* update position in buffer */
	remaining -= msg_end - msg_start;
	msg_start = msg_end;
}

void CALLBACK TimerCallback(UINT uTimerID, UINT, DWORD_PTR dwUser, DWORD_PTR, DWORD_PTR)
{
	if (TryEnterCriticalSection(&_midi.lock)) {
		/* check for stop */
		if (_midi.do_stop) {
			DEBUG(driver, 2, "Win32-MIDI: timer: do_stop is set");
			midiOutReset(_midi.midi_out);
			_midi.playing = false;
			_midi.do_stop = false;
			LeaveCriticalSection(&_midi.lock);
			return;
		}

		/* check for start/restart/change song */
		if (_midi.do_start) {
			DEBUG(driver, 2, "Win32-MIDI: timer: do_start is set");
			if (_midi.playing) {
				midiOutReset(_midi.midi_out);
				/* Some songs change the "Pitch bend range" registered
				 * parameter. If this doesn't get reset, everything else
				 * will start sounding wrong. */
				for (int ch = 0; ch < 16; ch++) {
					TransmitChannelMsg(0xB0 | ch, 0x64, 0x00); // select RPN 00.00 "Pitch bend range"
					TransmitChannelMsg(0xB0 | ch, 0x65, 0x00);
					TransmitChannelMsg(0xB0 | ch, 0x06, 0x02); // set parameter to 02.00 (2 semitones, the default)
					TransmitChannelMsg(0xB0 | ch, 0x26, 0x00);
					TransmitChannelMsg(0xB0 | ch, 0x64, 0x7F); // unselect RPN
					TransmitChannelMsg(0xB0 | ch, 0x65, 0x7F);
				}
			}
			_midi.current_file.MoveFrom(_midi.next_file);
			std::swap(_midi.next_segment, _midi.current_segment);
			_midi.current_segment.start_block = 0;
			_midi.playback_start_time = timeGetTime();
			_midi.playing = true;
			_midi.do_start = false;
			_midi.current_block = 0;

			MemSetT<byte>(_midi.channel_volumes, 127, lengthof(_midi.channel_volumes));
		} else if (!_midi.playing) {
			/* not playing, stop the timer */
			DEBUG(driver, 2, "Win32-MIDI: timer: not playing, stopping timer");
			timeKillEvent(uTimerID);
			_midi.timer_id = 0;
			LeaveCriticalSection(&_midi.lock);
			return;
		}

		/* check for volume change */
		static int volume_throttle = 0;
		if (_midi.current_volume != _midi.new_volume) {
			if (volume_throttle == 0) {
				DEBUG(driver, 2, "Win32-MIDI: timer: volume change");
				_midi.current_volume = _midi.new_volume;
				volume_throttle = 20 / _midi.time_period;
				for (int ch = 0; ch < 16; ch++) {
					int vol = ScaleVolume(_midi.channel_volumes[ch], _midi.current_volume);
					TransmitChannelMsg(0xB0 | ch, 0x07, vol);
				}
			}
			else {
				volume_throttle--;
			}
		}

		LeaveCriticalSection(&_midi.lock);
	}

	if (_midi.current_segment.start > 0 && _midi.current_block == 0 && _midi.current_segment.start_block == 0) {
		/* find first block after start time and pretend playback started earlier
		* this is to allow all blocks prior to the actual start to still affect playback,
		* as they may contain important controller and program changes */
		size_t preload_bytes = 0;
		for (size_t bl = 0; bl < _midi.current_file.blocks.size(); bl++) {
			MidiFile::DataBlock &block = _midi.current_file.blocks[bl];
			preload_bytes += block.data.Length();
			if (block.ticktime >= _midi.current_segment.start) {
				if (_midi.current_segment.loop) {
					DEBUG(driver, 2, "Win32-MIDI: timer: loop from block %d (ticktime %d, realtime %.3f, bytes %d)", (int)bl, (int)block.ticktime, ((int)block.realtime)/1000.0, (int)preload_bytes);
					_midi.current_segment.start_block = bl;
					break;
				} else {
					DEBUG(driver, 2, "Win32-MIDI: timer: start from block %d (ticktime %d, realtime %.3f, bytes %d)", (int)bl, (int)block.ticktime, ((int)block.realtime) / 1000.0, (int)preload_bytes);
					_midi.playback_start_time -= block.realtime / 1000;
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

		/* check that block is not in the future */
		if (block.realtime / 1000 > playback_time) {
			break;
		}
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
			case 0xC0: case 0xD0:
				/* 2 byte channel messages */
				TransmitChannelMsg(status, data[0]);
				data++;
				remaining--;
				break;
			case 0x80: case 0x90: case 0xA0: case 0xE0:
				/* 3 byte channel messages */
				TransmitChannelMsg(status, data[0], data[1]);
				data += 2;
				remaining -= 2;
				break;
			case 0xB0:
				/* controller change */
				if (data[0] == 0x07) {
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
				case 0xF0: /* system exclusive */
					TransmitSysex(data, remaining);
					break;
				case 0xF1: /* time code quarter frame */
				case 0xF3: /* song select */
					data++;
					remaining--;
					break;
				case 0xF2: /* song position pointer */
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

	if (_midi.current_block == _midi.current_file.blocks.size()) {
		if (_midi.current_segment.loop) {
			_midi.current_block = 0;
			_midi.playback_start_time = timeGetTime();
		} else {
			_midi.do_stop = true;
		}
	}
}

void MusicDriver_Win32::PlaySong(const MusicSongInfo &song)
{
	DEBUG(driver, 2, "Win32-MIDI: PlaySong: entry");
	EnterCriticalSection(&_midi.lock);

	if (song.filetype == MTT_STANDARDMIDI) {
		_midi.next_file.LoadFile(song.filename);
	} else if (song.filetype == MTT_MPSMIDI) {
		size_t songdatalen = 0;
		byte *songdata = GetMusicCatEntryData(song.filename, song.cat_index, songdatalen);
		if (songdata == NULL) return; // fail in a more noisy way?
		_midi.next_file.LoadMpsData(songdata, songdatalen);
		free(songdata);
	} else {
		NOT_REACHED();
	}

	_midi.next_segment.start = 0;
	_midi.next_segment.end = 0;
	_midi.next_segment.loop = false;

	DEBUG(driver, 2, "Win32-MIDI: PlaySong: setting flag");
	_midi.do_stop = _midi.playing;
	_midi.do_start = true;

	if (_midi.timer_id == 0) {
		DEBUG(driver, 2, "Win32-MIDI: PlaySong: starting timer");
		_midi.timer_id = timeSetEvent(_midi.time_period, _midi.time_period, TimerCallback, (DWORD_PTR)this, TIME_PERIODIC | TIME_CALLBACK_FUNCTION);
	}

	LeaveCriticalSection(&_midi.lock);
}

void MusicDriver_Win32::StopSong()
{
	DEBUG(driver, 2, "Win32-MIDI: StopSong: entry");
	EnterCriticalSection(&_midi.lock);
	DEBUG(driver, 2, "Win32-MIDI: StopSong: setting flag");
	_midi.do_stop = true;
	LeaveCriticalSection(&_midi.lock);
}

bool MusicDriver_Win32::IsSongPlaying()
{
	return _midi.playing || _midi.do_start;
}

void MusicDriver_Win32::SetVolume(byte vol)
{
	EnterCriticalSection(&_midi.lock);
	_midi.new_volume = vol;
	LeaveCriticalSection(&_midi.lock);
}

const char *MusicDriver_Win32::Start(const char * const *parm)
{
	DEBUG(driver, 2, "Win32-MIDI: Start: initializing");

	InitializeCriticalSection(&_midi.lock);

	int resolution = GetDriverParamInt(parm, "resolution", 5);
	int port = GetDriverParamInt(parm, "port", -1);

	UINT devid;
	if (port < 0) {
		devid = MIDI_MAPPER;
	} else {
		devid = (UINT)port;
	}

	resolution = Clamp(resolution, 1, 20);

	if (midiOutOpen(&_midi.midi_out, devid, (DWORD_PTR)&MidiOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
		return "could not open midi device";

	midiOutReset(_midi.midi_out);

	{
		/* Standard "Enable General MIDI" message */
		static byte gm_enable_sysex[] = { 0xF0, 0x7E, 0x00, 0x09, 0x01, 0xF7 };
		/* Roland-specific reverb room control, used by the original game */
		static byte roland_reverb_sysex[] = { 0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x01, 0x30, 0x02, 0x04, 0x00, 0x40, 0x40, 0x00, 0x00, 0x09, 0xF7 };
		byte *ptr;
		size_t len;
		TransmitSysex((ptr = gm_enable_sysex), (len = sizeof(gm_enable_sysex)));
		TransmitSysex((ptr = roland_reverb_sysex), (len = sizeof(roland_reverb_sysex)));
	}

	TIMECAPS timecaps;
	if (timeGetDevCaps(&timecaps, sizeof(timecaps)) == MMSYSERR_NOERROR) {
		_midi.time_period = min(max((UINT)resolution, timecaps.wPeriodMin), timecaps.wPeriodMax);
		if (timeBeginPeriod(_midi.time_period) == MMSYSERR_NOERROR) {
			DEBUG(driver, 2, "Win32-MIDI: Start: timer resolution is %d", (int)_midi.time_period);
			return NULL;
		}
	}
	midiOutClose(_midi.midi_out);
	return "could not set timer resolution";
}

void MusicDriver_Win32::Stop()
{
	EnterCriticalSection(&_midi.lock);

	if (_midi.timer_id) {
		timeKillEvent(_midi.timer_id);
		_midi.timer_id = 0;
	}

	timeEndPeriod(_midi.time_period);
	midiOutReset(_midi.midi_out);
	midiOutClose(_midi.midi_out);

	LeaveCriticalSection(&_midi.lock);
	DeleteCriticalSection(&_midi.lock);
}
