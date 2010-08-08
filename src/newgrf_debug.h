/* $Id$ */

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
#include "core/smallvec_type.hpp"
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
	uint32 click_time;                  ///< Realtime tick when clicked to detect next frame
	SmallVector<SpriteID, 256> sprites; ///< Sprites found
};

extern NewGrfDebugSpritePicker _newgrf_debug_sprite_picker;

/**
 * Can we inspect the data given a certain feature and index.
 * The index is normally an in-game location/identifier, such
 * as a TileIndex or an IndustryID depending on the feature
 * we want to inspect.
 * @param feature The feature we want to inspect.
 * @param index   The index/identifier of the feature to inspect.
 * @return true if there is something to show.
 */
bool IsNewGRFInspectable(GrfSpecFeature feature, uint index);

/**
 * Show the inspect window for a given feature and index.
 * The index is normally an in-game location/identifier, such
 * as a TileIndex or an IndustryID depending on the feature
 * we want to inspect.
 * @param feature The feature we want to inspect.
 * @param index   The index/identifier of the feature to inspect.
 */
void ShowNewGRFInspectWindow(GrfSpecFeature feature, uint index);

/**
 * Delete inspect window for a given feature and index.
 * The index is normally an in-game location/identifier, such
 * as a TileIndex or an IndustryID depending on the feature
 * we want to inspect.
 * @param feature The feature we want to delete the window for.
 * @param index   The index/identifier of the feature to delete.
 */
void DeleteNewGRFInspectWindow(GrfSpecFeature feature, uint index);

/**
 * Get the GrfSpecFeature associated with the tile.
 * @return the GrfSpecFeature.
 */
GrfSpecFeature GetGrfSpecFeature(TileIndex tile);

/**
 * Get the GrfSpecFeature associated with the vehicle.
 * @return the GrfSpecFeature.
 */
GrfSpecFeature GetGrfSpecFeature(VehicleType type);

/**
 * Show the window for aligning sprites.
 */
void ShowSpriteAlignerWindow();

#endif /* NEWGRF_DEBUG_H */
