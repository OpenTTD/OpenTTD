/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_sound.h Functions related to NewGRF provided sounds. */

#ifndef NEWGRF_SOUND_H
#define NEWGRF_SOUND_H

#include "sound_type.h"
#include "tile_type.h"
#include "vehicle_type.h"

/** Events at which a sound might be played. */
enum class VehicleSoundEvent : uint8_t {
	Start = 1, ///< Vehicle starting, i.e. leaving, the station.
	Tunnel = 2, ///< Train entering a tunnel.
	Breakdown = 3, ///< Vehicle breaking down.
	Running = 4, ///< Vehicle running normally.
	Touchdown = 5, ///< Whenever a plane touches down.
	VisualEffect = 6, ///< Vehicle visual effect (steam, diesel smoke or electric spark) is shown.
	Running16 = 7, ///< Every 16 ticks while the vehicle is running (speed > 0).
	Stopped16 = 8, ///< Every 16 ticks while the vehicle is stopped (speed == 0).
	LoadUnload = 9, ///< Whenever cargo payment is made for a vehicle.
};


SoundEntry *AllocateSound(uint num);
void InitializeSoundPool();
bool LoadNewGRFSound(SoundEntry &sound, SoundID sound_id);
SoundID GetNewGRFSoundID(const struct GRFFile *file, SoundID sound_id);
SoundEntry *GetSound(SoundID sound_id);
uint GetNumSounds();
size_t GetSoundPoolAllocatedMemory();
bool PlayVehicleSound(const Vehicle *v, VehicleSoundEvent event, bool force  = false);
void PlayTileSound(const struct GRFFile *file, SoundID sound_id, TileIndex tile);

#endif /* NEWGRF_SOUND_H */
