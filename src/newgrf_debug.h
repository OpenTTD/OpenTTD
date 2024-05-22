/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_debug.h Functions/types related to NewGRF debugging. */

#ifndef NEWGRF_DEBUG_H
#define NEWGRF_DEBUG_H

#include "newgrf.h"
#include "tile_type.h"
#include "vehicle_type.h"

/** Current state of spritepicker */
enum NewGrfDebugSpritePickerMode {
	SPM_NONE,
	SPM_WAIT_CLICK,
	SPM_REDRAW,
};

/** Spritepicker of SpriteAligner */
struct NewGrfDebugSpritePicker {
	NewGrfDebugSpritePickerMode mode;   ///< Current state
	void *clicked_pixel;                ///< Clicked pixel (pointer to blitter buffer)
	std::vector<SpriteID> sprites;       ///< Sprites found
};

extern NewGrfDebugSpritePicker _newgrf_debug_sprite_picker;

bool IsNewGRFInspectable(GrfSpecFeature feature, uint index);
void ShowNewGRFInspectWindow(GrfSpecFeature feature, uint index, const uint32_t grfid = 0);
void InvalidateNewGRFInspectWindow(GrfSpecFeature feature, uint index);
void DeleteNewGRFInspectWindow(GrfSpecFeature feature, uint index);

GrfSpecFeature GetGrfSpecFeature(TileIndex tile);
GrfSpecFeature GetGrfSpecFeature(VehicleType type);

void ShowSpriteAlignerWindow();

#endif /* NEWGRF_DEBUG_H */
