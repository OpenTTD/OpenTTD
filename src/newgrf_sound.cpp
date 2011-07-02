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

static SmallVector<SoundEntry, 8> _sounds;


/* Allocate a new Sound */
SoundEntry *AllocateSound()
{
	SoundEntry *sound = _sounds.Append();
	MemSetT(sound, 0);
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
 * Checks whether a NewGRF wants to play a different vehicle sound effect.
 * @param v Vehicle to play sound effect for.
 * @param event Trigger for the sound effect.
 * @return false if the default sound effect shall be played instead.
 */
bool PlayVehicleSound(const Vehicle *v, VehicleSoundEvent event)
{
	const GRFFile *file = GetEngineGRF(v->engine_type);
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
