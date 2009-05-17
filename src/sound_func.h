/* $Id$ */

/** @file sound_func.h Functions related to sound. */

#ifndef SOUND_FUNC_H
#define SOUND_FUNC_H

#include "sound_type.h"
#include "vehicle_type.h"
#include "tile_type.h"

extern MusicFileSettings msf;

bool SoundInitialize(const char *filename);

void SndPlayTileFx(SoundID sound, TileIndex tile);
void SndPlayVehicleFx(SoundID sound, const Vehicle *v);
void SndPlayFx(SoundID sound);
void SndCopyToPool();

#endif /* SOUND_FUNC_H */
