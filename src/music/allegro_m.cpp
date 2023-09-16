/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file allegro_m.cpp Playing music via allegro. */

#ifdef WITH_ALLEGRO

#include "../stdafx.h"
#include "../debug.h"
#include "allegro_m.h"
#include "midifile.hpp"
#include <allegro.h>

#include "../safeguards.h"

static FMusicDriver_Allegro iFMusicDriver_Allegro;
static MIDI *_midi = nullptr;

/**
 * There are multiple modules that might be using Allegro and
 * Allegro can only be initiated once.
 */
extern int _allegro_instance_count;

const char *MusicDriver_Allegro::Start(const StringList &)
{
	if (_allegro_instance_count == 0 && install_allegro(SYSTEM_AUTODETECT, &errno, nullptr)) {
		Debug(driver, 0, "allegro: install_allegro failed '{}'", allegro_error);
		return "Failed to set up Allegro";
	}
	_allegro_instance_count++;

	/* Initialise the sound */
	if (install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, nullptr) != 0) {
		Debug(driver, 0, "allegro: install_sound failed '{}'", allegro_error);
		return "Failed to set up Allegro sound";
	}

	/* Okay, there's no soundcard */
	if (midi_card == MIDI_NONE) {
		Debug(driver, 0, "allegro: no midi card found");
		return "No sound card found";
	}

	return nullptr;
}

void MusicDriver_Allegro::Stop()
{
	if (_midi != nullptr) destroy_midi(_midi);
	_midi = nullptr;

	if (--_allegro_instance_count == 0) allegro_exit();
}

void MusicDriver_Allegro::PlaySong(const MusicSongInfo &song)
{
	std::string filename = MidiFile::GetSMFFile(song);

	if (_midi != nullptr) destroy_midi(_midi);
	if (!filename.empty()) {
		_midi = load_midi(filename.c_str());
		play_midi(_midi, false);
	} else {
		_midi = nullptr;
	}
}

void MusicDriver_Allegro::StopSong()
{
	stop_midi();
}

bool MusicDriver_Allegro::IsSongPlaying()
{
	return midi_pos >= 0;
}

void MusicDriver_Allegro::SetVolume(byte vol)
{
	set_volume(-1, vol);
}

#endif /* WITH_ALLEGRO */
