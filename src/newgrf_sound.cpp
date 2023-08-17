/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_sound.cpp Handling NewGRF provided sounds. */

#include "stdafx.h"
#include "engine_base.h"
#include "newgrf.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"
#include "vehicle_base.h"
#include "sound_func.h"
#include "random_access_file_type.h"
#include "debug.h"
#include "settings_type.h"

#include "safeguards.h"

static std::vector<SoundEntry> _sounds;


/**
 * Allocate sound slots.
 * @param num Number of slots to allocate.
 * @return First allocated slot.
 */
SoundEntry *AllocateSound(uint num)
{
	size_t pos = _sounds.size();
	_sounds.insert(_sounds.end(), num, SoundEntry());
	return &_sounds[pos];
}


void InitializeSoundPool()
{
	_sounds.clear();

	/* Copy original sound data to the pool */
	SndCopyToPool();
}


SoundEntry *GetSound(SoundID index)
{
	if (index >= _sounds.size()) return nullptr;
	return &_sounds[index];
}


uint GetNumSounds()
{
	return (uint)_sounds.size();
}


/**
 * Extract meta data from a NewGRF sound.
 * @param sound Sound to load.
 * @return True if a valid sound was loaded.
 */
bool LoadNewGRFSound(SoundEntry *sound)
{
	if (sound->file_offset == SIZE_MAX || sound->file == nullptr) return false;

	RandomAccessFile &file = *sound->file;
	file.SeekTo(sound->file_offset, SEEK_SET);

	/* Skip ID for container version >= 2 as we only look at the first
	 * entry and ignore any further entries with the same ID. */
	if (sound->grf_container_ver >= 2) file.ReadDword();

	/* Format: <num> <FF> <FF> <name_len> <name> '\0' <data> */

	uint32_t num = sound->grf_container_ver >= 2 ? file.ReadDword() : file.ReadWord();
	if (file.ReadByte() != 0xFF) return false;
	if (file.ReadByte() != 0xFF) return false;

	uint8_t name_len = file.ReadByte();
	std::string name(name_len + 1, '\0');
	file.ReadBlock(name.data(), name_len + 1);

	/* Test string termination */
	if (name[name_len] != 0) {
		Debug(grf, 2, "LoadNewGRFSound [{}]: Name not properly terminated", file.GetSimplifiedFilename());
		return false;
	}

	Debug(grf, 2, "LoadNewGRFSound [{}]: Sound name '{}'...", file.GetSimplifiedFilename(), name);

	if (file.ReadDword() != BSWAP32('RIFF')) {
		Debug(grf, 1, "LoadNewGRFSound [{}]: Missing RIFF header", file.GetSimplifiedFilename());
		return false;
	}

	uint32_t total_size = file.ReadDword();
	uint header_size = 11;
	if (sound->grf_container_ver >= 2) header_size++; // The first FF in the sprite is only counted for container version >= 2.
	if (total_size + name_len + header_size > num) {
		Debug(grf, 1, "LoadNewGRFSound [{}]: RIFF was truncated", file.GetSimplifiedFilename());
		return false;
	}

	if (file.ReadDword() != BSWAP32('WAVE')) {
		Debug(grf, 1, "LoadNewGRFSound [{}]: Invalid RIFF type", file.GetSimplifiedFilename());
		return false;
	}

	while (total_size >= 8) {
		uint32_t tag  = file.ReadDword();
		uint32_t size = file.ReadDword();
		total_size -= 8;
		if (total_size < size) {
			Debug(grf, 1, "LoadNewGRFSound [{}]: Invalid RIFF", file.GetSimplifiedFilename());
			return false;
		}
		total_size -= size;

		switch (tag) {
			case ' tmf': // 'fmt '
				/* Audio format, must be 1 (PCM) */
				if (size < 16 || file.ReadWord() != 1) {
					Debug(grf, 1, "LoadGRFSound [{}]: Invalid audio format", file.GetSimplifiedFilename());
					return false;
				}
				sound->channels = file.ReadWord();
				sound->rate = file.ReadDword();
				file.ReadDword();
				file.ReadWord();
				sound->bits_per_sample = file.ReadWord();

				/* The rest will be skipped */
				size -= 16;
				break;

			case 'atad': // 'data'
				sound->file_size   = size;
				sound->file_offset = file.GetPos();

				Debug(grf, 2, "LoadNewGRFSound [{}]: channels {}, sample rate {}, bits per sample {}, length {}", file.GetSimplifiedFilename(), sound->channels, sound->rate, sound->bits_per_sample, size);
				return true; // the fmt chunk has to appear before data, so we are finished

			default:
				/* Skip unknown chunks */
				break;
		}

		/* Skip rest of chunk */
		if (size > 0) file.SkipBytes(size);
	}

	Debug(grf, 1, "LoadNewGRFSound [{}]: RIFF does not contain any sound data", file.GetSimplifiedFilename());

	/* Clear everything that was read */
	MemSetT(sound, 0);
	return false;
}

/**
 * Resolve NewGRF sound ID.
 * @param file NewGRF to get sound from.
 * @param sound_id GRF-specific sound ID. (GRF-local for IDs above ORIGINAL_SAMPLE_COUNT)
 * @return Translated (global) sound ID, or INVALID_SOUND.
 */
SoundID GetNewGRFSoundID(const GRFFile *file, SoundID sound_id)
{
	/* Global sound? */
	if (sound_id < ORIGINAL_SAMPLE_COUNT) return sound_id;

	sound_id -= ORIGINAL_SAMPLE_COUNT;
	if (file == nullptr || sound_id >= file->num_sounds) return INVALID_SOUND;

	return file->sound_offset  + sound_id;
}

/**
 * Checks whether a NewGRF wants to play a different vehicle sound effect.
 * @param v Vehicle to play sound effect for.
 * @param event Trigger for the sound effect.
 * @param force Should we play the sound effect even if vehicle sound effects are muted?
 * @return false if the default sound effect shall be played instead.
 */
bool PlayVehicleSound(const Vehicle *v, VehicleSoundEvent event, bool force)
{
	if (!_settings_client.sound.vehicle && !force) return true;

	const GRFFile *file = v->GetGRF();
	uint16_t callback;

	/* If the engine has no GRF ID associated it can't ever play any new sounds */
	if (file == nullptr) return false;

	/* Check that the vehicle type uses the sound effect callback */
	if (!HasBit(EngInfo(v->engine_type)->callback_mask, CBM_VEHICLE_SOUND_EFFECT)) return false;

	callback = GetVehicleCallback(CBID_VEHICLE_SOUND_EFFECT, event, 0, v->engine_type, v);
	/* Play default sound if callback fails */
	if (callback == CALLBACK_FAILED) return false;

	callback = GetNewGRFSoundID(file, callback);

	/* Play no sound, if result is invalid */
	if (callback == INVALID_SOUND) return true;

	assert(callback < GetNumSounds());
	SndPlayVehicleFx(callback, v);
	return true;
}

/**
 * Play a NewGRF sound effect at the location of a specific tile.
 * @param file NewGRF triggering the sound effect.
 * @param sound_id Sound effect the NewGRF wants to play.
 * @param tile Location of the effect.
 */
void PlayTileSound(const GRFFile *file, SoundID sound_id, TileIndex tile)
{
	sound_id = GetNewGRFSoundID(file, sound_id);
	if (sound_id == INVALID_SOUND) return;

	assert(sound_id < GetNumSounds());
	SndPlayTileFx(sound_id, tile);
}
