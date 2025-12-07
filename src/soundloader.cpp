/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file soundloader.cpp Handling of loading sounds. */

#include "stdafx.h"
#include "debug.h"
#include "sound_type.h"
#include "soundloader_type.h"
#include "soundloader_func.h"
#include "string_func.h"
#include "newgrf_sound.h"
#include "random_access_file_type.h"

#include "safeguards.h"

template class ProviderManager<SoundLoader>;

bool LoadSoundData(SoundEntry &sound, bool new_format, SoundID sound_id, const std::string &name)
{
	/* Check for valid sound size. */
	if (sound.file_size == 0 || sound.file_size > SIZE_MAX - 2) return false;

	size_t pos = sound.file->GetPos();
	sound.data = std::make_shared<std::vector<std::byte>>();
	for (auto &loader : ProviderManager<SoundLoader>::GetProviders()) {
		sound.file->SeekTo(pos, SEEK_SET);
		if (loader->Load(sound, new_format, *sound.data)) break;
	}

	if (sound.data->empty()) {
		Debug(grf, 0, "LoadSound [{}]: Failed to load sound '{}' for slot {}", sound.file->GetSimplifiedFilename(), name, sound_id);
		return false;
	}

	assert(sound.bits_per_sample == 8 || sound.bits_per_sample == 16);
	assert(sound.channels == 1);
	assert(sound.rate != 0);

	Debug(grf, 2, "LoadSound [{}]: channels {}, sample rate {}, bits per sample {}, length {}", sound.file->GetSimplifiedFilename(), sound.channels, sound.rate, sound.bits_per_sample, sound.file_size);

	/* Mixer always requires an extra sample at the end for the built-in linear resampler. */
	sound.data->resize(sound.data->size() + sound.channels * sound.bits_per_sample / 8);
	sound.data->shrink_to_fit();

	return true;
}

static bool LoadBasesetSound(SoundEntry &sound, bool new_format, SoundID sound_id)
{
	sound.file->SeekTo(sound.file_offset, SEEK_SET);

	/* Read name of sound for diagnostics. */
	size_t name_len = sound.file->ReadByte();
	std::string name(name_len, '\0');
	sound.file->ReadBlock(name.data(), name_len);

	return LoadSoundData(sound, new_format, sound_id, StrMakeValid(name));
}

bool LoadSound(SoundEntry &sound, SoundID sound_id)
{
	switch (sound.source) {
		case SoundSource::BasesetOldFormat: return LoadBasesetSound(sound, false, sound_id);
		case SoundSource::BasesetNewFormat: return LoadBasesetSound(sound, true, sound_id);
		case SoundSource::NewGRF: return LoadNewGRFSound(sound, sound_id);
		default: NOT_REACHED();
	}
}
