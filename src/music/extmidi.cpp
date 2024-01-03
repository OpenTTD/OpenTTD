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
#include "../base_media_base.h"
#include "../thread.h"
#include "midifile.hpp"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

#include "../safeguards.h"

#ifndef EXTERNAL_PLAYER
/** The default external midi player. */
#define EXTERNAL_PLAYER "timidity"
#endif

/** Factory for the midi player that uses external players. */
static FMusicDriver_ExtMidi iFMusicDriver_ExtMidi;

const char *MusicDriver_ExtMidi::Start(const StringList &parm)
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

	this->command_tokens.clear();

	std::string_view view = command;
	for (;;) {
		auto pos = view.find(' ');
		this->command_tokens.emplace_back(view.substr(0, pos));

		if (pos == std::string_view::npos) break;
		view.remove_prefix(pos + 1);
	}

	this->song.clear();
	this->pid = -1;
	return nullptr;
}

void MusicDriver_ExtMidi::Stop()
{
	this->song.clear();
	this->DoStop();
}

void MusicDriver_ExtMidi::PlaySong(const MusicSongInfo &song)
{
	std::string filename = MidiFile::GetSMFFile(song);
	if (!filename.empty()) {
		this->song = std::move(filename);
		this->DoStop();
	}
}

void MusicDriver_ExtMidi::StopSong()
{
	this->song.clear();
	this->DoStop();
}

bool MusicDriver_ExtMidi::IsSongPlaying()
{
	if (this->pid != -1 && waitpid(this->pid, nullptr, WNOHANG) == this->pid) {
		this->pid = -1;
	}
	if (this->pid == -1 && !this->song.empty()) this->DoPlay();
	return this->pid != -1;
}

void MusicDriver_ExtMidi::SetVolume(byte)
{
	Debug(driver, 1, "extmidi: set volume not implemented");
}

void MusicDriver_ExtMidi::DoPlay()
{
	this->pid = fork();
	switch (this->pid) {
		case 0: {
			close(0);
			int d = open("/dev/null", O_RDONLY);
			if (d != -1 && dup2(d, 1) != -1 && dup2(d, 2) != -1) {
				/* execvp is nasty as it *allows* the passed parameters to be written
				 * for backward compatibility, however we are a fork so do not care. */
				std::vector<char *> parameters;
				for (auto &token : this->command_tokens) parameters.emplace_back(token.data());
				parameters.emplace_back(this->song.data());
				parameters.emplace_back(nullptr);

				execvp(parameters[0], parameters.data());
			}
			_exit(1);
		}

		case -1:
			Debug(driver, 0, "extmidi: couldn't fork: {}", strerror(errno));
			FALLTHROUGH;

		default:
			this->song.clear();
			break;
	}
}

/**
 * Try to end child process with kill/waitpid for up to 1 second.
 * @param pid The process ID to end.
 * @param signal The signal type to send.
 * @return True if the process has been ended.
 */
static bool KillWait(pid_t &pid, int signal)
{
	/* First try to stop for about a second;
	 * 1 seconds = 1000 milliseconds, 50 ms per cycle => 20 cycles. */
	for (int i = 0; i < 20; i++) {
		kill(pid, signal);
		if (waitpid(pid, nullptr, WNOHANG) == pid) {
			/* It has shut down, so we are done */
			pid = -1;
			return true;
		}
		/* Wait 50 milliseconds. */
		CSleep(50);
	}

	return false;
}

void MusicDriver_ExtMidi::DoStop()
{
	if (this->pid <= 0) return;

	if (KillWait(this->pid, SIGINT)) return;

	if (KillWait(this->pid, SIGTERM)) return;

	Debug(driver, 0, "extmidi: gracefully stopping failed, trying the hard way");
	/* Gracefully stopping failed. Do it the hard way
	 * and wait till the process finally died. */
	kill(this->pid, SIGKILL);
	waitpid(this->pid, nullptr, 0);
	this->pid = -1;
}
