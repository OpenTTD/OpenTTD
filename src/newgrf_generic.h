/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_generic.h Functions related to generic callbacks. */

#ifndef NEWGRF_GENERIC_H
#define NEWGRF_GENERIC_H

#include "industry_type.h"
#include "newgrf.h"
#include "tile_type.h"

struct SpriteGroup;

/** AI events for asking the NewGRF for information. */
enum AIConstructionEvent {
	AICE_TRAIN_CHECK_RAIL_ENGINE     = 0x00, ///< Check if we should build an engine
	AICE_TRAIN_CHECK_ELRAIL_ENGINE   = 0x01, ///< Check if we should build an engine
	AICE_TRAIN_CHECK_MONORAIL_ENGINE = 0x02, ///< Check if we should build an engine
	AICE_TRAIN_CHECK_MAGLEV_ENGINE   = 0x03, ///< Check if we should build an engine
	AICE_TRAIN_GET_RAIL_WAGON        = 0x08, ///< Check if we should build an engine
	AICE_TRAIN_GET_ELRAIL_WAGON      = 0x09, ///< Check if we should build an engine
	AICE_TRAIN_GET_MONORAIL_WAGON    = 0x0A, ///< Check if we should build an engine
	AICE_TRAIN_GET_MAGLEV_WAGON      = 0x0B, ///< Check if we should build an engine
	AICE_TRAIN_GET_RAILTYPE          = 0x0F, ///< Check if we should build a railtype

	AICE_ROAD_CHECK_ENGINE           = 0x00, ///< Check if we should build an engine
	AICE_ROAD_GET_FIRST_ENGINE       = 0x01, ///< Unused, we check all
	AICE_ROAD_GET_NUMBER_ENGINES     = 0x02, ///< Unused, we check all

	AICE_SHIP_CHECK_ENGINE           = 0x00, ///< Check if we should build an engine
	AICE_SHIP_GET_FIRST_ENGINE       = 0x01, ///< Unused, we check all
	AICE_SHIP_GET_NUMBER_ENGINES     = 0x02, ///< Unused, we check all

	AICE_AIRCRAFT_CHECK_ENGINE       = 0x00, ///< Check if we should build an engine

	AICE_STATION_GET_STATION_ID      = 0x00, ///< Get a station ID to build
};

static const IndustryType IT_AI_UNKNOWN = 0xFE; ///< The AI has no specific industry in mind.
static const IndustryType IT_AI_TOWN    = 0xFF; ///< The AI actually wants to transport to/from a town, not an industry.

void ResetGenericCallbacks();
void AddGenericCallback(uint8_t feature, const GRFFile *file, const SpriteGroup *group);

uint16_t GetAiPurchaseCallbackResult(uint8_t feature, CargoID cargo_type, uint8_t default_selection, IndustryType src_industry, IndustryType dst_industry, uint8_t distance, AIConstructionEvent event, uint8_t count, uint8_t station_size, const GRFFile **file);
void AmbientSoundEffectCallback(TileIndex tile);

/** Play an ambient sound effect for an empty tile. */
static inline void AmbientSoundEffect(TileIndex tile)
{
	/* Only run callback if enabled. */
	if (!HasGrfMiscBit(GMB_AMBIENT_SOUND_CALLBACK)) return;

	AmbientSoundEffectCallback(tile);
}

#endif /* NEWGRF_GENERIC_H */
