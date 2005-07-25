/* $Id$ */

#ifndef __BEOS__
#ifndef __MORPHOS__
#include "../stdafx.h"
#include "../openttd.h"
#include "../sound.h"
#include "../string.h"
#include "../variables.h"
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
	int pid;
} _midi;

static void DoPlay(void);
static void DoStop(void);

static const char* ExtMidiStart(const char* const * parm)
{
	_midi.song[0] = '\0';
	_midi.pid = -1;
	return NULL;
}

static void ExtMidiStop(void)
{
	_midi.song[0] = '\0';
	DoStop();
}

static void ExtMidiPlaySong(const char* filename)
{
	ttd_strlcpy(_midi.song, filename, lengthof(_midi.song));
	DoStop();
}

static void ExtMidiStopSong(void)
{
	_midi.song[0] = '\0';
	DoStop();
}

static bool ExtMidiIsPlaying(void)
{
	if (_midi.pid != -1 && waitpid(_midi.pid, NULL, WNOHANG) == _midi.pid)
		_midi.pid = -1;
	if (_midi.pid == -1 && _midi.song[0] != '\0') DoPlay();
	return _midi.pid != -1;
}

static void ExtMidiSetVolume(byte vol)
{
	fprintf(stderr, "extmidi: set volume not implemented\n");
}

static void DoPlay(void)
{
	_midi.pid = fork();
	switch (_midi.pid) {
		case 0: {
			int d;

			close(0);
			close(1);
			close(2);
			d = open("/dev/null", O_RDONLY);
			if (d != -1) {
				if (dup2(d, 1) != -1 && dup2(d, 2) != -1) {
					#if defined(MIDI_ARG)
						execlp(msf.extmidi, "extmidi", MIDI_ARG, _midi.song, NULL);
					#else
						execlp(msf.extmidi, "extmidi", _midi.song, NULL);
					#endif
				}
			}
			exit(1);
		}

		case -1:
			fprintf(stderr, "extmidi: couldn't fork: %s\n", strerror(errno));
			/* FALLTHROUGH */

		default:
			_midi.song[0] = '\0';
			break;
	}
}

static void DoStop(void)
{
	if (_midi.pid != -1) kill(_midi.pid, SIGTERM);
}

const HalMusicDriver _extmidi_music_driver = {
	ExtMidiStart,
	ExtMidiStop,
	ExtMidiPlaySong,
	ExtMidiStopSong,
	ExtMidiIsPlaying,
	ExtMidiSetVolume,
};

#endif /* __MORPHOS__ */
#endif /* __BEOS__ */
