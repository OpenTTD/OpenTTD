/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_house.h Functions related to NewGRF houses. */

#ifndef NEWGRF_HOUSE_H
#define NEWGRF_HOUSE_H

#include "newgrf_callbacks.h"
#include "tile_cmd.h"
#include "house_type.h"
#include "newgrf_spritegroup.h"
#include "newgrf_town.h"

/** Scope resolver for houses. */
struct HouseScopeResolver : public ScopeResolver {
	HouseID house_id;              ///< Type of house being queried.
	TileIndex tile;                ///< Tile of this house.
	Town *town;                    ///< Town of this house.
	bool not_yet_constructed;      ///< True for construction check.
	uint16 initial_random_bits;    ///< Random bits during construction checks.
	uint32 watched_cargo_triggers; ///< Cargo types that triggered the watched cargo callback.

	HouseScopeResolver(ResolverObject &ro, HouseID house_id, TileIndex tile, Town *town,
			bool not_yet_constructed, uint8 initial_random_bits, uint32 watched_cargo_triggers);

	/* virtual */ uint32 GetRandomBits() const;
	/* virtual */ uint32 GetVariable(byte variable, uint32 parameter, bool *available) const;
	/* virtual */ uint32 GetTriggers() const;
};

/** Resolver object to be used for houses (feature 07 spritegroups). */
struct HouseResolverObject : public ResolverObject {
	HouseScopeResolver house_scope;
	TownScopeResolver  town_scope;

	HouseResolverObject(HouseID house_id, TileIndex tile, Town *town,
			CallbackID callback = CBID_NO_CALLBACK, uint32 param1 = 0, uint32 param2 = 0,
			bool not_yet_constructed = false, uint8 initial_random_bits = 0, uint32 watched_cargo_triggers = 0);

	/* virtual */ ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0)
	{
		switch (scope) {
			case VSG_SCOPE_SELF:   return &this->house_scope;
			case VSG_SCOPE_PARENT: return &this->town_scope;
			default: return ResolverObject::GetScope(scope, relative);
		}
	}
};

/**
 * Makes class IDs unique to each GRF file.
 * Houses can be assigned class IDs which are only comparable within the GRF
 * file they were defined in. This mapping ensures that if two houses have the
 * same class as defined by the GRF file, the classes are different within the
 * game. An array of HouseClassMapping structs is created, and the array index
 * of the struct that matches both the GRF ID and the class ID is the class ID
 * used in the game.
 *
 * Although similar to the HouseIDMapping struct above, this serves a different
 * purpose. Since the class ID is not saved anywhere, this mapping does not
 * need to be persistent; it just needs to keep class ids unique.
 */
struct HouseClassMapping {
	uint32 grfid;     ///< The GRF ID of the file this class belongs to
	uint8  class_id;  ///< The class id within the grf file
};

HouseClassID AllocateHouseClassID(byte grf_class_id, uint32 grfid);

void InitializeBuildingCounts();
void IncreaseBuildingCount(Town *t, HouseID house_id);
void DecreaseBuildingCount(Town *t, HouseID house_id);

void DrawNewHouseTile(TileInfo *ti, HouseID house_id);
void AnimateNewHouseTile(TileIndex tile);
void AnimateNewHouseConstruction(TileIndex tile);

uint16 GetHouseCallback(CallbackID callback, uint32 param1, uint32 param2, HouseID house_id, Town *town, TileIndex tile,
		bool not_yet_constructed = false, uint8 initial_random_bits = 0, uint32 watched_cargo_triggers = 0);
void WatchedCargoCallback(TileIndex tile, uint32 trigger_cargoes);

bool CanDeleteHouse(TileIndex tile);

bool NewHouseTileLoop(TileIndex tile);

enum HouseTrigger {
	/* The tile of the house has been triggered during the tileloop. */
	HOUSE_TRIGGER_TILE_LOOP     = 0x01,
	/*
	 * The top tile of a (multitile) building has been triggered during and all
	 * the tileloop other tiles of the same building get the same random value.
	 */
	HOUSE_TRIGGER_TILE_LOOP_TOP = 0x02,
};
void TriggerHouse(TileIndex t, HouseTrigger trigger);

#endif /* NEWGRF_HOUSE_H */
