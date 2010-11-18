/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_sound.h Functions related to NewGRF provided sounds. */

#ifndef NEWGRF_SOUND_H
#define NEWGRF_SOUND_H

#include "sound_type.h"
#include "tile_type.h"
#include "vehicle_type.h"

/** Events at which a sound might be played. */
enum VehicleSoundEvent {
	VSE_START         = 1, ///< Vehicle starting, i.e. leaving, the station.
	VSE_TUNNEL        = 2, ///< Train entering a tunnel.
	VSE_BREAKDOWN     = 3, ///< Vehicle breaking down.
	VSE_RUNNING       = 4, ///< Vehicle running normally.
	VSE_TOUCHDOWN     = 5, ///< Whenever a plane touches down.
	VSE_VISUAL_EFFECT = 6, ///< Vehicle visual effect (steam, diesel smoke or electric spark) is shown.
	VSE_RUNNING_16    = 7, ///< Every 16 ticks while the vehicle is running (speed > 0).
	VSE_STOPPED_16    = 8, ///< Every 16 ticks while the vehicle is stopped (speed == 0).
	VSE_LOAD_UNLOAD   = 9, ///< Whenever cargo payment is made for a vehicle.
};


SoundEntry *AllocateSound();
void InitializeSoundPool();
SoundEntry *GetSound(SoundID sound_id);
uint GetNumSounds();
bool PlayVehicleSound(const Vehicle *v, VehicleSoundEvent event);
bool PlayTileSound(const struct GRFFile *file, SoundID sound_id, TileIndex tile);

#endif /* NEWGRF_SOUND_H */
