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
#include "midi.h"

#include "../safeguards.h"

struct PlaybackSegment {
	uint32 start, end;
	size_t start_block;
	bool loop;
};

static struct {
	UINT time_period;      ///< obtained timer precision value
	HMIDIOUT midi_out;     ///< handle to open midiOut
	UINT timer_id;         ///< ID of active multimedia timer
	CRITICAL_SECTION lock; ///< synchronization for playback status fields

	bool playing;        ///< flag indicating that playback is active
	bool do_start;       ///< flag for starting playback of next_file at next opportunity
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
	while (*msg_end != MIDIST_ENDSYSEX) msg_end++;
	msg_end++; /* also include sysex end byte */

	/* prepare header */
	MIDIHDR *hdr = CallocT<MIDIHDR>(1);
	hdr->lpData = (LPSTR)msg_start;
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

static void TransmitSysexConst(byte *msg_start, size_t length)
{
	TransmitSysex(msg_start, length);
}

/**
 * Realtime MIDI playback service routine.
 * This is called by the multimedia timer.
 */
void CALLBACK TimerCallback(UINT uTimerID, UINT, DWORD_PTR dwUser, DWORD_PTR, DWORD_PTR)
{
	/* Try to check playback status changes.
	 * If _midi is already locked, skip checking for this cycle and try again
	 * next cycle, instead of waiting for locks in the realtime callback. */
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
					TransmitChannelMsg(MIDIST_CONTROLLER | ch, MIDICT_CHANVOLUME, vol);
				}
			}
			else {
				volume_throttle--;
			}
		}

		LeaveCriticalSection(&_midi.lock);
	}

	/* skip beginning of file? */
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
			_midi.current_block = 0;
			_midi.playback_start_time = timeGetTime();
		} else {
			_midi.do_stop = true;
		}
	}
}

void MusicDriver_Win32::PlaySong(const char *filename)
{
	DEBUG(driver, 2, "Win32-MIDI: PlaySong: entry");
	EnterCriticalSection(&_midi.lock);

	_midi.next_file.LoadFile(filename);
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

	if (midiOutOpen(&_midi.midi_out, devid, (DWORD_PTR)&MidiOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
		return "could not open midi device";
	}

	midiOutReset(_midi.midi_out);

	/* Standard "Enable General MIDI" message */
	static byte gm_enable_sysex[] = { 0xF0, 0x7E, 0x00, 0x09, 0x01, 0xF7 };
	TransmitSysexConst(&gm_enable_sysex[0], sizeof(gm_enable_sysex));

	/* Roland-specific reverb room control, used by the original game */
	static byte roland_reverb_sysex[] = { 0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x01, 0x30, 0x02, 0x04, 0x00, 0x40, 0x40, 0x00, 0x00, 0x09, 0xF7 };
	TransmitSysexConst(&roland_reverb_sysex[0], sizeof(roland_reverb_sysex));

	/* prepare multimedia timer */
	TIMECAPS timecaps;
	if (timeGetDevCaps(&timecaps, sizeof(timecaps)) == MMSYSERR_NOERROR) {
		_midi.time_period = min(max((UINT)resolution, timecaps.wPeriodMin), timecaps.wPeriodMax);
		if (timeBeginPeriod(_midi.time_period) == MMSYSERR_NOERROR) {
			/* success */
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
