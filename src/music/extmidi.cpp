/* $Id$ */

#ifndef __MORPHOS__
#include "../stdafx.h"
#include "../openttd.h"
#include "../sound.h"
#include "../string.h"
#include "../variables.h"
#include "../debug.h"
#include "extmidi.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>

static struct {
	char song[MAX_PATH];
	pid_t pid;
} _midi;

static void DoPlay();
static void DoStop();

static FMusicDriver_ExtMidi iFMusicDriver_ExtMidi;

const char* MusicDriver_ExtMidi::Start(const char* const * parm)
{
	_midi.song[0] = '\0';
	_midi.pid = -1;
	return NULL;
}

void MusicDriver_ExtMidi::Stop()
{
	_midi.song[0] = '\0';
	DoStop();
}

void MusicDriver_ExtMidi::PlaySong(const char* filename)
{
	ttd_strlcpy(_midi.song, filename, lengthof(_midi.song));
	DoStop();
}

void MusicDriver_ExtMidi::StopSong()
{
	_midi.song[0] = '\0';
	DoStop();
}

bool MusicDriver_ExtMidi::IsSongPlaying()
{
	if (_midi.pid != -1 && waitpid(_midi.pid, NULL, WNOHANG) == _midi.pid)
		_midi.pid = -1;
	if (_midi.pid == -1 && _midi.song[0] != '\0') DoPlay();
	return _midi.pid != -1;
}

void MusicDriver_ExtMidi::SetVolume(byte vol)
{
	DEBUG(driver, 1, "extmidi: set volume not implemented");
}

static void DoPlay()
{
	_midi.pid = fork();
	switch (_midi.pid) {
		case 0: {
			int d;

			close(0);
			d = open("/dev/null", O_RDONLY);
			if (d != -1 && dup2(d, 1) != -1 && dup2(d, 2) != -1) {
				#if defined(MIDI_ARG)
					execlp(msf.extmidi, "extmidi", MIDI_ARG, _midi.song, (char*)0);
				#else
					execlp(msf.extmidi, "extmidi", _midi.song, (char*)0);
				#endif
			}
			_exit(1);
		}

		case -1:
			DEBUG(driver, 0, "extmidi: couldn't fork: %s", strerror(errno));
			/* FALLTHROUGH */

		default:
			_midi.song[0] = '\0';
			break;
	}
}

static void DoStop()
{
	if (_midi.pid != -1) kill(_midi.pid, SIGTERM);
}

#endif /* __MORPHOS__ */
