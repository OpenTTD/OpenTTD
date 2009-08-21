/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_generic.h Functions related to generic callbacks. */

#ifndef NEWGRF_GENERIC_H
#define NEWGRF_GENERIC_H

enum AIConstructionEvent {
	AICE_TRAIN_CHECK_RAIL_ENGINE     = 0x00, ///< Check if we should build an engine
	AICE_TRAIN_CHECK_ELRAIL_ENGINE   = 0x01,
	AICE_TRAIN_CHECK_MONORAIL_ENGINE = 0x02,
	AICE_TRAIN_CHECK_MAGLEV_ENGINE   = 0x03,
	AICE_TRAIN_GET_RAIL_WAGON        = 0x08,
	AICE_TRAIN_GET_ELRAIL_WAGON      = 0x09,
	AICE_TRAIN_GET_MONORAIL_WAGON    = 0x0A,
	AICE_TRAIN_GET_MAGLEV_WAGON      = 0x0B,
	AICE_TRAIN_GET_RAILTYPE          = 0x0F,

	AICE_ROAD_CHECK_ENGINE           = 0x00, ///< Check if we should build an engine
	AICE_ROAD_GET_FIRST_ENGINE       = 0x01, ///< Unused, we check all
	AICE_ROAD_GET_NUMBER_ENGINES     = 0x02, ///< Unused, we check all

	AICE_SHIP_CHECK_ENGINE           = 0x00, ///< Check if we should build an engine
	AICE_SHIP_GET_FIRST_ENGINE       = 0x01, ///< Unused, we check all
	AICE_SHIP_GET_NUMBER_ENGINES     = 0x02, ///< Unused, we check all

	AICE_AIRCRAFT_CHECK_ENGINE       = 0x00, ///< Check if we should build an engine

	AICE_STATION_GET_STATION_ID      = 0x00, ///< Get a station ID to build
};

void ResetGenericCallbacks();
void AddGenericCallback(uint8 feature, const struct GRFFile *file, const struct SpriteGroup *group);

uint16 GetAiPurchaseCallbackResult(uint8 feature, CargoID cargo_type, uint8 default_selection, IndustryType src_industry, IndustryType dst_industry, uint8 distance, AIConstructionEvent event, uint8 count, uint8 station_size, const struct GRFFile **file);

#endif /* NEWGRF_GENERIC_H */
