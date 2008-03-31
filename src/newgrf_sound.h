/* $Id$ */

/** @file newgrf_sound.h */

#ifndef NEWGRF_SOUND_H
#define NEWGRF_SOUND_H

#include "sound_type.h"
#include "tile_type.h"

enum VehicleSoundEvent {
	VSE_START        = 1,
	VSE_TUNNEL       = 2,
	VSE_BREAKDOWN    = 3,
	VSE_RUNNING      = 4,
	VSE_TOUCHDOWN    = 5,
	VSE_TRAIN_EFFECT = 6,
	VSE_RUNNING_16   = 7,
	VSE_STOPPED_16   = 8,
	VSE_LOAD_UNLOAD  = 9,
};


FileEntry *AllocateFileEntry();
void InitializeSoundPool();
FileEntry *GetSound(uint index);
uint GetNumSounds();
bool PlayVehicleSound(const Vehicle *v, VehicleSoundEvent event);
bool PlayHouseSound(uint16 sound_id, TileIndex tile);

#endif /* NEWGRF_SOUND_H */
