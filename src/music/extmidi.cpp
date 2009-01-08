/* $Id$ */

/** @file extmidi.cpp Playing music via an external player. */

#ifndef __MORPHOS__
#include "../stdafx.h"
#include "../openttd.h"
#include "../sound_func.h"
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

#ifndef EXTERNAL_PLAYER
#define EXTERNAL_PLAYER "timidity"
#endif

static FMusicDriver_ExtMidi iFMusicDriver_ExtMidi;

const char* MusicDriver_ExtMidi::Start(const char* const * parm)
{
	const char *command = GetDriverParam(parm, "cmd");
	if (StrEmpty(command)) command = EXTERNAL_PLAYER;

	this->command = strdup(command);
	this->song[0] = '\0';
	this->pid = -1;
	return NULL;
}

void MusicDriver_ExtMidi::Stop()
{
	free(command);
	this->song[0] = '\0';
	this->DoStop();
}

void MusicDriver_ExtMidi::PlaySong(const char* filename)
{
	strecpy(this->song, filename, lastof(this->song));
	this->DoStop();
}

void MusicDriver_ExtMidi::StopSong()
{
	this->song[0] = '\0';
	this->DoStop();
}

bool MusicDriver_ExtMidi::IsSongPlaying()
{
	if (this->pid != -1 && waitpid(this->pid, NULL, WNOHANG) == this->pid)
		this->pid = -1;
	if (this->pid == -1 && this->song[0] != '\0') this->DoPlay();
	return this->pid != -1;
}

void MusicDriver_ExtMidi::SetVolume(byte vol)
{
	DEBUG(driver, 1, "extmidi: set volume not implemented");
}

void MusicDriver_ExtMidi::DoPlay()
{
	this->pid = fork();
	switch (this->pid) {
		case 0: {
			int d;

			close(0);
			d = open("/dev/null", O_RDONLY);
			if (d != -1 && dup2(d, 1) != -1 && dup2(d, 2) != -1) {
				#if defined(MIDI_ARG)
					execlp(this->command, "extmidi", MIDI_ARG, this->song, (char*)0);
				#else
					execlp(this->command, "extmidi", this->song, (char*)0);
				#endif
			}
			_exit(1);
		}

		case -1:
			DEBUG(driver, 0, "extmidi: couldn't fork: %s", strerror(errno));
			/* FALLTHROUGH */

		default:
			this->song[0] = '\0';
			break;
	}
}

void MusicDriver_ExtMidi::DoStop()
{
	if (this->pid != -1) kill(this->pid, SIGTERM);
}

#endif /* __MORPHOS__ */
