/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file extmidi.cpp Playing music via an external player. */

#include "../stdafx.h"
#include "../debug.h"
#include "../string_func.h"
#include "../core/alloc_func.hpp"
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

#include "../safeguards.h"

#ifndef EXTERNAL_PLAYER
/** The default external midi player. */
#define EXTERNAL_PLAYER "timidity"
#endif

/** Factory for the midi player that uses external players. */
static FMusicDriver_ExtMidi iFMusicDriver_ExtMidi;

const char *MusicDriver_ExtMidi::Start(const char * const * parm)
{
	if (strcmp(VideoDriver::GetInstance()->GetName(), "allegro") == 0 ||
			strcmp(SoundDriver::GetInstance()->GetName(), "allegro") == 0) {
		return "the extmidi driver does not work when Allegro is loaded.";
	}

	const char *command = GetDriverParam(parm, "cmd");
#ifndef MIDI_ARG
	if (StrEmpty(command)) command = EXTERNAL_PLAYER;
#else
	if (StrEmpty(command)) command = EXTERNAL_PLAYER " " MIDI_ARG;
#endif

	/* Count number of arguments, but include 3 extra slots: 1st for command, 2nd for song title, and 3rd for terminating NULL. */
	uint num_args = 3;
	for (const char *t = command; *t != '\0'; t++) if (*t == ' ') num_args++;

	this->params = CallocT<char *>(num_args);
	this->params[0] = stredup(command);

	/* Replace space with \0 and add next arg to params */
	uint p = 1;
	while (true) {
		this->params[p] = strchr(this->params[p - 1], ' ');
		if (this->params[p] == NULL) break;

		this->params[p][0] = '\0';
		this->params[p]++;
		p++;
	}

	/* Last parameter is the song file. */
	this->params[p] = this->song;

	this->song[0] = '\0';
	this->pid = -1;
	return NULL;
}

void MusicDriver_ExtMidi::Stop()
{
	free(params[0]);
	free(params);
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
				execvp(this->params[0], this->params);
			}
			_exit(1);
		}

		case -1:
			DEBUG(driver, 0, "extmidi: couldn't fork: %s", strerror(errno));
			FALLTHROUGH;

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
