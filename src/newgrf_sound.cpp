/* $Id$ */

/** @file newgrf_sound.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "oldpool.h"
#include "engine_func.h"
#include "newgrf_callbacks.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"
#include "vehicle_base.h"
#include "sound_func.h"

static uint _sound_count = 0;
STATIC_OLD_POOL(SoundInternal, FileEntry, 3, 1000, NULL, NULL)


/* Allocate a new FileEntry */
FileEntry *AllocateFileEntry()
{
	if (_sound_count == GetSoundInternalPoolSize()) {
		if (!_SoundInternal_pool.AddBlockToPool()) return NULL;
	}

	return GetSoundInternal(_sound_count++);
}


void InitializeSoundPool()
{
	_SoundInternal_pool.CleanPool();
	_sound_count = 0;

	/* Copy original sound data to the pool */
	SndCopyToPool();
}


FileEntry *GetSound(uint index)
{
	if (index >= GetNumSounds()) return NULL;
	return GetSoundInternal(index);
}


uint GetNumSounds()
{
	return _sound_count;
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
	if (callback >= GetNumOriginalSounds()) callback += file->sound_offset - GetNumOriginalSounds();

	if (callback < GetNumSounds()) SndPlayVehicleFx((SoundFx)callback, v);
	return true;
}

bool PlayHouseSound(uint16 sound_id, TileIndex tile)
{
	if (sound_id < GetNumOriginalSounds()) {
		SndPlayTileFx((SoundFx)sound_id, tile);
		return true;
	}
	return false;
}
