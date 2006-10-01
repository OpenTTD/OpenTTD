/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "pool.h"
#include "sound.h"
#include "engine.h"
#include "vehicle.h"
#include "newgrf_callbacks.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"

enum {
	SOUND_POOL_BLOCK_SIZE_BITS = 3, /* (1 << 3) == 8 items */
	SOUND_POOL_MAX_BLOCKS      = 1000,
};

static uint _sound_count = 0;
static MemoryPool _sound_pool = { "Sound", SOUND_POOL_MAX_BLOCKS, SOUND_POOL_BLOCK_SIZE_BITS, sizeof(FileEntry), NULL, NULL, 0, 0, NULL };


/* Allocate a new FileEntry */
FileEntry *AllocateFileEntry(void)
{
	if (_sound_count == _sound_pool.total_items) {
		if (!AddBlockToPool(&_sound_pool)) return NULL;
	}

	return (FileEntry*)GetItemFromPool(&_sound_pool, _sound_count++);
}


void InitializeSoundPool(void)
{
	CleanPool(&_sound_pool);
	_sound_count = 0;

	/* Copy original sound data to the pool */
	SndCopyToPool();
}


FileEntry *GetSound(uint index)
{
	if (index >= _sound_count) return NULL;
	return (FileEntry*)GetItemFromPool(&_sound_pool, index);
}


uint GetNumSounds(void)
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
	if (!HASBIT(EngInfo(v->engine_type)->callbackmask, CBM_SOUND_EFFECT)) return false;

	callback = GetVehicleCallback(CBID_VEHICLE_SOUND_EFFECT, event, 0, v->engine_type, v);
	if (callback == CALLBACK_FAILED) return false;
	if (callback >= GetNumOriginalSounds()) callback += file->sound_offset - GetNumOriginalSounds();

	if (callback < GetNumSounds()) SndPlayVehicleFx(callback, v);
	return true;
}
