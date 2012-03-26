/* $Id$ */

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
#include "fileio_func.h"
#include "debug.h"

static SmallVector<SoundEntry, 8> _sounds;


/**
 * Allocate sound slots.
 * @param num Number of slots to allocate.
 * @return First allocated slot.
 */
SoundEntry *AllocateSound(uint num)
{
	SoundEntry *sound = _sounds.Append(num);
	MemSetT(sound, 0, num);
	return sound;
}


void InitializeSoundPool()
{
	_sounds.Clear();

	/* Copy original sound data to the pool */
	SndCopyToPool();
}


SoundEntry *GetSound(SoundID index)
{
	if (index >= _sounds.Length()) return NULL;
	return &_sounds[index];
}


uint GetNumSounds()
{
	return _sounds.Length();
}


/**
 * Extract meta data from a NewGRF sound.
 * @param sound Sound to load.
 * @return True if a valid sound was loaded.
 */
bool LoadNewGRFSound(SoundEntry *sound)
{
	if (sound->file_offset == SIZE_MAX || sound->file_slot == 0) return false;

	FioSeekToFile(sound->file_slot, sound->file_offset);

	/* Skip ID for container version >= 2 as we only look at the first
	 * entry and ignore any further entries with the same ID. */
	if (sound->grf_container_ver >= 2) FioReadDword();

	/* Format: <num> <FF> <FF> <name_len> <name> '\0' <data> */

	uint32 num = sound->grf_container_ver >= 2 ? FioReadDword() : FioReadWord();
	if (FioReadByte() != 0xFF) return false;
	if (FioReadByte() != 0xFF) return false;

	uint8 name_len = FioReadByte();
	char *name = AllocaM(char, name_len + 1);
	FioReadBlock(name, name_len + 1);

	/* Test string termination */
	if (name[name_len] != 0) {
		DEBUG(grf, 2, "LoadNewGRFSound [%s]: Name not properly terminated", FioGetFilename(sound->file_slot));
		return false;
	}

	DEBUG(grf, 2, "LoadNewGRFSound [%s]: Sound name '%s'...", FioGetFilename(sound->file_slot), name);

	if (FioReadDword() != BSWAP32('RIFF')) {
		DEBUG(grf, 1, "LoadNewGRFSound [%s]: Missing RIFF header", FioGetFilename(sound->file_slot));
		return false;
	}

	uint32 total_size = FioReadDword();
	uint header_size = 11;
	if (sound->grf_container_ver >= 2) header_size++; // The first FF in the sprite is only counted for container version >= 2.
	if (total_size + name_len + header_size > num) {
		DEBUG(grf, 1, "LoadNewGRFSound [%s]: RIFF was truncated", FioGetFilename(sound->file_slot));
		return false;
	}

	if (FioReadDword() != BSWAP32('WAVE')) {
		DEBUG(grf, 1, "LoadNewGRFSound [%s]: Invalid RIFF type", FioGetFilename(sound->file_slot));
		return false;
	}

	while (total_size >= 8) {
		uint32 tag  = FioReadDword();
		uint32 size = FioReadDword();
		total_size -= 8;
		if (total_size < size) {
			DEBUG(grf, 1, "LoadNewGRFSound [%s]: Invalid RIFF", FioGetFilename(sound->file_slot));
			return false;
		}
		total_size -= size;

		switch (tag) {
			case ' tmf': // 'fmt '
				/* Audio format, must be 1 (PCM) */
				if (size < 16 || FioReadWord() != 1) {
					DEBUG(grf, 1, "LoadGRFSound [%s]: Invalid audio format", FioGetFilename(sound->file_slot));
					return false;
				}
				sound->channels = FioReadWord();
				sound->rate = FioReadDword();
				FioReadDword();
				FioReadWord();
				sound->bits_per_sample = FioReadWord();

				/* The rest will be skipped */
				size -= 16;
				break;

			case 'atad': // 'data'
				sound->file_size   = size;
				sound->file_offset = FioGetPos();

				DEBUG(grf, 2, "LoadNewGRFSound [%s]: channels %u, sample rate %u, bits per sample %u, length %u", FioGetFilename(sound->file_slot), sound->channels, sound->rate, sound->bits_per_sample, size);
				return true; // the fmt chunk has to appear before data, so we are finished

			default:
				/* Skip unknown chunks */
				break;
		}

		/* Skip rest of chunk */
		if (size > 0) FioSkipBytes(size);
	}

	DEBUG(grf, 1, "LoadNewGRFSound [%s]: RIFF does not contain any sound data", FioGetFilename(sound->file_slot));

	/* Clear everything that was read */
	MemSetT(sound, 0);
	return false;
}


/**
 * Checks whether a NewGRF wants to play a different vehicle sound effect.
 * @param v Vehicle to play sound effect for.
 * @param event Trigger for the sound effect.
 * @return false if the default sound effect shall be played instead.
 */
bool PlayVehicleSound(const Vehicle *v, VehicleSoundEvent event)
{
	const GRFFile *file = v->GetGRF();
	uint16 callback;

	/* If the engine has no GRF ID associated it can't ever play any new sounds */
	if (file == NULL) return false;

	/* Check that the vehicle type uses the sound effect callback */
	if (!HasBit(EngInfo(v->engine_type)->callback_mask, CBM_VEHICLE_SOUND_EFFECT)) return false;

	callback = GetVehicleCallback(CBID_VEHICLE_SOUND_EFFECT, event, 0, v->engine_type, v);
	/* Play default sound if callback fails */
	if (callback == CALLBACK_FAILED) return false;

	if (callback >= ORIGINAL_SAMPLE_COUNT) {
		callback -= ORIGINAL_SAMPLE_COUNT;

		/* Play no sound if result is out of range */
		if (callback > file->num_sounds) return true;

		callback += file->sound_offset;
	}

	assert(callback < GetNumSounds());
	SndPlayVehicleFx(callback, v);
	return true;
}

/**
 * Play a NewGRF sound effect at the location of a specfic tile.
 * @param file NewGRF triggering the sound effect.
 * @param sound_id Sound effect the NewGRF wants to play.
 * @param tile Location of the effect.
 */
void PlayTileSound(const GRFFile *file, SoundID sound_id, TileIndex tile)
{
	if (sound_id >= ORIGINAL_SAMPLE_COUNT) {
		sound_id -= ORIGINAL_SAMPLE_COUNT;
		if (sound_id > file->num_sounds) return;
		sound_id += file->sound_offset;
	}

	assert(sound_id < GetNumSounds());
	SndPlayTileFx(sound_id, tile);
}
