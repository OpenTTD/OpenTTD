/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file extmidi.cpp Playing music via an external player. */

#ifndef __MORPHOS__
#include "../stdafx.h"
#include "../debug.h"
#include "../string_func.h"
#include "../sound/sound_driver.hpp"
#include "../video/video_driver.hpp"
#include "../gfx_func.h"
#include "extmidi.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>

#ifndef EXTERNAL_PLAYER
/** The default external midi player. */
#define EXTERNAL_PLAYER "timidity"
#endif

/** Factory for the midi player that uses external players. */
static FMusicDriver_ExtMidi iFMusicDriver_ExtMidi;

const char *MusicDriver_ExtMidi::Start(const char * const * parm)
{
	if (strcmp(_video_driver->GetName(), "allegro") == 0 ||
			strcmp(_sound_driver->GetName(), "allegro") == 0) {
		return "the extmidi driver does not work when Allegro is loaded.";
	}

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

void MusicDriver_ExtMidi::PlaySong(const char *filename)
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
	if (this->pid != -1 && waitpid(this->pid, NULL, WNOHANG) == this->pid) {
		this->pid = -1;
	}
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
			close(0);
			int d = open("/dev/null", O_RDONLY);
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
			/* FALL THROUGH */

		default:
			this->song[0] = '\0';
			break;
	}
}

void MusicDriver_ExtMidi::DoStop()
{
	if (this->pid <= 0) return;

	/* First try to gracefully stop for about five seconds;
	 * 5 seconds = 5000 milliseconds, 10 ms per cycle => 500 cycles. */
	for (int i = 0; i < 500; i++) {
		kill(this->pid, SIGTERM);
		if (waitpid(this->pid, NULL, WNOHANG) == this->pid) {
			/* It has shut down, so we are done */
			this->pid = -1;
			return;
		}
		/* Wait 10 milliseconds. */
		CSleep(10);
	}

	DEBUG(driver, 0, "extmidi: gracefully stopping failed, trying the hard way");
	/* Gracefully stopping failed. Do it the hard way
	 * and wait till the process finally died. */
	kill(this->pid, SIGKILL);
	waitpid(this->pid, NULL, 0);
	this->pid = -1;
}

#endif /* __MORPHOS__ */
