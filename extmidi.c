#ifndef __BEOS__
#ifndef __MORPHOS__
#include "stdafx.h"

#include "ttd.h"
#include "hal.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>

#ifndef EXTERNAL_PLAYER
#define EXTERNAL_PLAYER "timidity"
#endif

static pid_t _pid;

static void extmidi_kill(void)
{
	if (_pid > 0) {
		kill(_pid, SIGKILL);
		while (waitpid(_pid, NULL, WNOHANG) != _pid);
	}
	_pid = 0;
}

static const char *extmidi_start(const char * const *parm)
{
	_pid = 0;
	return NULL;
}

static void extmidi_stop(void)
{
	extmidi_kill();
}

static void extmidi_play_song(const char *filename)
{
	extmidi_kill();

	_pid = fork();
	if (_pid < 0) {
		fprintf(stderr, "extmidi: couldn't fork: %s\n", strerror(errno));
		_pid = 0;
		return;
	}

	if (_pid == 0) {
		#if defined(MIDI_ARG)
			execlp(EXTERNAL_PLAYER, "extmidi", MIDI_ARG, filename, NULL);
		#else
			execlp(EXTERNAL_PLAYER, "extmidi", filename, NULL);
		#endif
		fprintf(stderr, "extmidi: couldn't execl: %s\n", strerror(errno));
		exit(0);
	}

	usleep(500);

	if (_pid == waitpid(_pid, NULL, WNOHANG)) {
		fprintf(stderr, "extmidi: play song failed\n");
		_pid = 0;

		usleep(5000);
	}
}

static void extmidi_stop_song(void)
{
	extmidi_kill();
}

static bool extmidi_is_playing(void)
{
	if (_pid == 0)
		return 0;

	if (waitpid(_pid, NULL, WNOHANG) == _pid) {
		_pid = 0;
		return 0;
	}

	return 1;
}

static void extmidi_set_volume(byte vol)
{
	fprintf(stderr, "extmidi: set volume not implemented\n");
}

const HalMusicDriver _extmidi_music_driver = {
	extmidi_start,
	extmidi_stop,
	extmidi_play_song,
	extmidi_stop_song,
	extmidi_is_playing,
	extmidi_set_volume,
};

#endif /* __MORPHOS__ */
#endif /* __BEOS__ */
