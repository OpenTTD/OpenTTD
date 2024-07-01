/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bemidi.cpp Support for BeOS midi. */

#include "../stdafx.h"
#include "../openttd.h"
#include "bemidi.h"
#include "../base_media_base.h"
#include "midifile.hpp"

#include "../safeguards.h"

/** Factory for BeOS' midi player. */
static FMusicDriver_BeMidi iFMusicDriver_BeMidi;

std::optional<std::string_view> MusicDriver_BeMidi::Start(const StringList &parm)
{
	return std::nullopt;
}

void MusicDriver_BeMidi::Stop()
{
	this->StopSong();
}

void MusicDriver_BeMidi::PlaySong(const MusicSongInfo &song)
{
	std::string filename = MidiFile::GetSMFFile(song);

	this->Stop();
	this->midi_synth_file = new BMidiSynthFile();
	if (!filename.empty()) {
		entry_ref midiRef;
		get_ref_for_path(filename.c_str(), &midiRef);
		if (this->midi_synth_file->LoadFile(&midiRef) == B_OK) {
			this->midi_synth_file->SetVolume(this->current_volume);
			this->midi_synth_file->Start();
			this->just_started = true;
		} else {
			this->Stop();
		}
	}
}

void MusicDriver_BeMidi::StopSong()
{
	/* Reusing BMidiSynthFile can cause stuck notes when switching
	 * tracks, just delete whole object entirely. */
	delete this->midi_synth_file;
	this->midi_synth_file = nullptr;
}

bool MusicDriver_BeMidi::IsSongPlaying()
{
	if (this->midi_synth_file == nullptr) return false;

	/* IsFinished() returns true for a moment after Start()
	 * but before it really starts playing, use just_started flag
	 * to prevent accidental track skipping. */
	if (this->just_started) {
		if (!this->midi_synth_file->IsFinished()) this->just_started = false;
		return true;
	}
	return !this->midi_synth_file->IsFinished();
}

void MusicDriver_BeMidi::SetVolume(uint8_t vol)
{
	this->current_volume = vol / 128.0;
	if (this->midi_synth_file != nullptr) this->midi_synth_file->SetVolume(this->current_volume);
}
