/* $Id$ */

/** @file newgrf_sound.cpp Handling NewGRF provided sounds. */

#include "stdafx.h"
#include "engine_base.h"
#include "newgrf.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"
#include "vehicle_base.h"
#include "sound_func.h"
#include "core/smallvec_type.hpp"
#include "core/mem_func.hpp"

static SmallVector<SoundEntry, ORIGINAL_SAMPLE_COUNT> _sounds;


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


bool PlayVehicleSound(const Vehicle *v, VehicleSoundEvent event)
{
	const GRFFile *file = GetEngineGRF(v->engine_type);
	uint16 callback;

	/* If the engine has no GRF ID associated it can't ever play any new sounds */
	if (file == NULL) return false;

	/* Check that the vehicle type uses the sound effect callback */
	if (!HasBit(EngInfo(v->engine_type)->callbackmask, CBM_VEHICLE_SOUND_EFFECT)) return false;

	callback = GetVehicleCallback(CBID_VEHICLE_SOUND_EFFECT, event, 0, v->engine_type, v);
	if (callback == CALLBACK_FAILED) return false;
	if (callback >= ORIGINAL_SAMPLE_COUNT) callback += file->sound_offset - ORIGINAL_SAMPLE_COUNT;

	if (callback < GetNumSounds()) SndPlayVehicleFx(callback, v);
	return true;
}

bool PlayTileSound(const GRFFile *file, SoundID sound_id, TileIndex tile)
{
	if (sound_id >= ORIGINAL_SAMPLE_COUNT) sound_id += file->sound_offset - ORIGINAL_SAMPLE_COUNT;

	if (sound_id < GetNumSounds()) {
		SndPlayTileFx(sound_id, tile);
		return true;
	}
	return false;
}
