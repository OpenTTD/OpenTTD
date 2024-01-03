/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fluidsynth.cpp Playing music via the fluidsynth library. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../sound_type.h"
#include "../debug.h"
#include "fluidsynth.h"
#include "midifile.hpp"
#include <fluidsynth.h>
#include "../mixer.h"
#include <mutex>

static struct {
	fluid_settings_t *settings;    ///< FluidSynth settings handle
	fluid_synth_t *synth;          ///< FluidSynth synthesizer handle
	fluid_player_t *player;        ///< FluidSynth MIDI player handle
	std::mutex synth_mutex;        ///< Guard mutex for synth access
} _midi; ///< Metadata about the midi we're playing.

/** Factory for the FluidSynth driver. */
static FMusicDriver_FluidSynth iFMusicDriver_FluidSynth;

/** List of sound fonts to try by default. */
static const char *default_sf[] = {
	/* FluidSynth preferred */
	/* See: https://www.fluidsynth.org/api/settings_synth.html#settings_synth_default-soundfont */
	"/usr/share/soundfonts/default.sf2",

	/* Debian/Ubuntu preferred */
	/* See: https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=929185 */
	"/usr/share/sounds/sf3/default-GM.sf3",

	/* OpenSUSE preferred */
	"/usr/share/sounds/sf2/FluidR3_GM.sf2",

	/* RedHat/Fedora/Arch preferred */
	"/usr/share/soundfonts/FluidR3_GM.sf2",

	/* Debian/Ubuntu/OpenSUSE alternatives */
	"/usr/share/sounds/sf2/TimGM6mb.sf2",
	"/usr/share/sounds/sf2/FluidR3_GS.sf2",

	nullptr
};

static void RenderMusicStream(int16_t *buffer, size_t samples)
{
	std::unique_lock<std::mutex> lock{ _midi.synth_mutex, std::try_to_lock };

	if (!lock.owns_lock() || _midi.synth == nullptr || _midi.player == nullptr) return;
	fluid_synth_write_s16(_midi.synth, samples, buffer, 0, 2, buffer, 1, 2);
}

const char *MusicDriver_FluidSynth::Start(const StringList &param)
{
	std::lock_guard<std::mutex> lock{ _midi.synth_mutex };

	const char *sfont_name = GetDriverParam(param, "soundfont");
	int sfont_id;

	Debug(driver, 1, "Fluidsynth: sf {}", sfont_name != nullptr ? sfont_name : "(null)");

	/* Create the settings. */
	_midi.settings = new_fluid_settings();
	if (_midi.settings == nullptr) return "Could not create midi settings";
	/* Don't try to lock sample data in memory, OTTD usually does not run with privileges allowing that */
	fluid_settings_setint(_midi.settings, "synth.lock-memory", 0);

	/* Install the music render routine and set up the samplerate */
	uint32_t samplerate = MxSetMusicSource(RenderMusicStream);
	fluid_settings_setnum(_midi.settings, "synth.sample-rate", samplerate);
	Debug(driver, 1, "Fluidsynth: samplerate {:.0f}", (float)samplerate);

	/* Create the synthesizer. */
	_midi.synth = new_fluid_synth(_midi.settings);
	if (_midi.synth == nullptr) return "Could not open synth";

	/* Load a SoundFont and reset presets (so that new instruments
	 * get used from the SoundFont) */
	if (sfont_name == nullptr) {
		sfont_id = FLUID_FAILED;

		/* Try loading the default soundfont registered with FluidSynth. */
		char *default_soundfont;
		fluid_settings_dupstr(_midi.settings, "synth.default-soundfont", &default_soundfont);
		if (fluid_is_soundfont(default_soundfont)) {
			sfont_id = fluid_synth_sfload(_midi.synth, default_soundfont, 1);
		}

		/* If no default soundfont found, try our own list. */
		if (sfont_id == FLUID_FAILED) {
			for (int i = 0; default_sf[i]; i++) {
				if (!fluid_is_soundfont(default_sf[i])) continue;
				sfont_id = fluid_synth_sfload(_midi.synth, default_sf[i], 1);
				if (sfont_id != FLUID_FAILED) break;
			}
		}
		if (sfont_id == FLUID_FAILED) return "Could not open any sound font";
	} else {
		sfont_id = fluid_synth_sfload(_midi.synth, sfont_name, 1);
		if (sfont_id == FLUID_FAILED) return "Could not open sound font";
	}

	_midi.player = nullptr;

	return nullptr;
}

void MusicDriver_FluidSynth::Stop()
{
	MxSetMusicSource(nullptr);

	std::lock_guard<std::mutex> lock{ _midi.synth_mutex };

	if (_midi.player != nullptr) delete_fluid_player(_midi.player);
	_midi.player = nullptr;

	if (_midi.synth != nullptr) delete_fluid_synth(_midi.synth);
	_midi.synth = nullptr;

	if (_midi.settings != nullptr) delete_fluid_settings(_midi.settings);
	_midi.settings = nullptr;
}

void MusicDriver_FluidSynth::PlaySong(const MusicSongInfo &song)
{
	std::string filename = MidiFile::GetSMFFile(song);

	this->StopSong();

	if (filename.empty()) {
		return;
	}

	std::lock_guard<std::mutex> lock{ _midi.synth_mutex };

	_midi.player = new_fluid_player(_midi.synth);
	if (_midi.player == nullptr) {
		Debug(driver, 0, "Could not create midi player");
		return;
	}

	if (fluid_player_add(_midi.player, filename.c_str()) != FLUID_OK) {
		Debug(driver, 0, "Could not open music file");
		delete_fluid_player(_midi.player);
		_midi.player = nullptr;
		return;
	}
	if (fluid_player_play(_midi.player) != FLUID_OK) {
		Debug(driver, 0, "Could not start midi player");
		delete_fluid_player(_midi.player);
		_midi.player = nullptr;
		return;
	}
}

void MusicDriver_FluidSynth::StopSong()
{
	std::lock_guard<std::mutex> lock{ _midi.synth_mutex };

	if (_midi.player == nullptr) return;

	fluid_player_stop(_midi.player);
	/* No fluid_player_join needed */
	delete_fluid_player(_midi.player);
	fluid_synth_system_reset(_midi.synth);
	fluid_synth_all_sounds_off(_midi.synth, -1);
	_midi.player = nullptr;
}

bool MusicDriver_FluidSynth::IsSongPlaying()
{
	std::lock_guard<std::mutex> lock{ _midi.synth_mutex };
	if (_midi.player == nullptr) return false;

	return fluid_player_get_status(_midi.player) == FLUID_PLAYER_PLAYING;
}

void MusicDriver_FluidSynth::SetVolume(byte vol)
{
	std::lock_guard<std::mutex> lock{ _midi.synth_mutex };
	if (_midi.settings == nullptr) return;

	/* Allowed range of synth.gain is 0.0 to 10.0 */
	/* fluidsynth's default gain is 0.2, so use this as "full
	 * volume". Set gain using OpenTTD's volume, as a number between 0
	 * and 0.2. */
	double gain = (1.0 * vol) / (128.0 * 5.0);
	if (fluid_settings_setnum(_midi.settings, "synth.gain", gain) != FLUID_OK) {
		Debug(driver, 0, "Could not set volume");
	}
}
