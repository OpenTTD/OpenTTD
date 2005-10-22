/* $Id$ */

#include "../stdafx.h"
#include "../openttd.h"
#include "win32_m.h"
#include <windows.h>
#include <mmsystem.h>

static struct {
	bool stop_song;
	bool terminate;
	bool playing;
	int new_vol;
	HANDLE wait_obj;
	UINT_PTR devid;
	char start_song[260];
} _midi;

static void Win32MidiPlaySong(const char *filename)
{
	strcpy(_midi.start_song, filename);
	_midi.playing = true;
	_midi.stop_song = false;
	SetEvent(_midi.wait_obj);
}

static void Win32MidiStopSong(void)
{
	if (_midi.playing) {
		_midi.stop_song = true;
		_midi.start_song[0] = '\0';
		SetEvent(_midi.wait_obj);
	}
}

static bool Win32MidiIsSongPlaying(void)
{
	return _midi.playing;
}

static void Win32MidiSetVolume(byte vol)
{
	_midi.new_vol = vol;
	SetEvent(_midi.wait_obj);
}

static MCIERROR CDECL MidiSendCommand(const char* cmd, ...)
{
	va_list va;
	char buf[512];

	va_start(va, cmd);
	vsprintf(buf, cmd, va);
	va_end(va);
	return mciSendStringA(buf, NULL, 0, 0);
}

static bool MidiIntPlaySong(const char *filename)
{
	MidiSendCommand("close all");
	if (MidiSendCommand("open \"%s\" type sequencer alias song", filename) != 0)
		return false;

	if (MidiSendCommand("play song from 0") != 0)
		return false;
	return true;
}

static void MidiIntStopSong(void)
{
	MidiSendCommand("close all");
}

static void MidiIntSetVolume(int vol)
{
	DWORD v = (vol * 65535 / 127);
	midiOutSetVolume((HMIDIOUT)_midi.devid, v + (v << 16));
}

static bool MidiIntIsSongPlaying(void)
{
	char buf[16];
	mciSendStringA("status song mode", buf, sizeof(buf), 0);
	return strcmp(buf, "playing") == 0 || strcmp(buf, "seeking") == 0;
}

static DWORD WINAPI MidiThread(LPVOID arg)
{
	_midi.wait_obj = CreateEvent(NULL, FALSE, FALSE, NULL);

	do {
		char *s;
		int vol;

		vol = _midi.new_vol;
		if (vol != -1) {
			_midi.new_vol = -1;
			MidiIntSetVolume(vol);
		}

		s = _midi.start_song;
		if (s[0] != '\0') {
			_midi.playing = MidiIntPlaySong(s);
			s[0] = '\0';

			// Delay somewhat in case we don't manage to play.
			if (!_midi.playing) {
				Sleep(5000);
			}
		}

		if (_midi.stop_song && _midi.playing) {
			_midi.stop_song = false;
			_midi.playing = false;
			MidiIntStopSong();
		}

		if (_midi.playing && !MidiIntIsSongPlaying())
			_midi.playing = false;

		WaitForMultipleObjects(1, &_midi.wait_obj, FALSE, 1000);
	} while (!_midi.terminate);

	DeleteObject(_midi.wait_obj);
	return 0;
}

static const char *Win32MidiStart(const char * const *parm)
{
	MIDIOUTCAPS midicaps;
	DWORD threadId;
	UINT nbdev;
	UINT_PTR dev;
	char buf[16];

	mciSendStringA("capability sequencer has audio", buf, lengthof(buf), 0);
	if (strcmp(buf, "true") != 0) return "MCI sequencer can't play audio";

	memset(&_midi, 0, sizeof(_midi));
	_midi.new_vol = -1;

	/* Get midi device */
	_midi.devid = MIDI_MAPPER;
	for (dev = 0, nbdev = midiOutGetNumDevs(); dev < nbdev; dev++) {
		if (midiOutGetDevCaps(dev, &midicaps, sizeof(midicaps)) == 0 && (midicaps.dwSupport & MIDICAPS_VOLUME)) {
			_midi.devid = dev;
			break;
		}
	}

	if (CreateThread(NULL, 8192, MidiThread, 0, 0, &threadId) == NULL)
		return "Failed to create thread";

	return NULL;
}

static void Win32MidiStop(void)
{
	_midi.terminate = true;
	SetEvent(_midi.wait_obj);
}

const HalMusicDriver _win32_music_driver = {
	Win32MidiStart,
	Win32MidiStop,
	Win32MidiPlaySong,
	Win32MidiStopSong,
	Win32MidiIsSongPlaying,
	Win32MidiSetVolume,
};
