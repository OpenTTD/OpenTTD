/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_airporttiles.h NewGRF handling of airport tiles. */

#ifndef NEWGRF_AIRPORTTILES_H
#define NEWGRF_AIRPORTTILES_H

#include "airport.h"
#include "station_map.h"
#include "newgrf_animation_type.h"
#include "newgrf_commons.h"
#include "newgrf_spritegroup.h"
#include "station_base.h"

/** Scope resolver for handling the tiles of an airport. */
struct AirportTileScopeResolver : public ScopeResolver {
	struct Station *st;  ///< %Station of the airport for which the callback is run, or \c nullptr for build gui.
	byte airport_id;     ///< Type of airport for which the callback is run.
	TileIndex tile;      ///< Tile for the callback, only valid for airporttile callbacks.
	const AirportTileSpec *ats;

	/**
	 * Constructor of the scope resolver specific for airport tiles.
	 * @param ats Specification of the airport tiles.
	 * @param tile %Tile for the callback, only valid for airporttile callbacks.
	 * @param st Station of the airport for which the callback is run, or \c nullptr for build gui.
	 */
	AirportTileScopeResolver(ResolverObject &ro, const AirportTileSpec *ats, TileIndex tile, Station *st)
		: ScopeResolver(ro), st(st), tile(tile), ats(ats)
	{
		assert(st != nullptr);
		this->airport_id = st->airport.type;
	}

	uint32_t GetRandomBits() const override;
	uint32_t GetVariable(byte variable, [[maybe_unused]] uint32_t parameter, bool *available) const override;
};

/** Resolver for tiles of an airport. */
struct AirportTileResolverObject : public ResolverObject {
	AirportTileScopeResolver tiles_scope; ///< Scope resolver for the tiles.
	AirportScopeResolver airport_scope;   ///< Scope resolver for the airport owning the tile.

	AirportTileResolverObject(const AirportTileSpec *ats, TileIndex tile, Station *st,
			CallbackID callback = CBID_NO_CALLBACK, uint32_t callback_param1 = 0, uint32_t callback_param2 = 0);

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0) override
	{
		switch (scope) {
			case VSG_SCOPE_SELF: return &tiles_scope;
			case VSG_SCOPE_PARENT: return &airport_scope;
			default: return ResolverObject::GetScope(scope, relative);
		}
	}

	GrfSpecFeature GetFeature() const override;
	uint32_t GetDebugID() const override;
};

/**
 * Defines the data structure of each individual tile of an airport.
 */
struct AirportTileSpec {
	AnimationInfo animation;              ///< Information about the animation.
	StringID name;                        ///< Tile Subname string, land information on this tile will give you "AirportName (TileSubname)"
	uint8_t callback_mask;                  ///< Bitmask telling which grf callback is set
	uint8_t animation_special_flags;        ///< Extra flags to influence the animation
	bool enabled;                         ///< entity still available (by default true). newgrf can disable it, though
	GRFFileProps grf_prop;                ///< properties related the the grf file

	static const AirportTileSpec *Get(StationGfx gfx);
	static const AirportTileSpec *GetByTile(TileIndex tile);

	static void ResetAirportTiles();

private:
	static AirportTileSpec tiles[NUM_AIRPORTTILES];

	friend void AirportTileOverrideManager::SetEntitySpec(const AirportTileSpec *airpts);
};

void AnimateAirportTile(TileIndex tile);
void AirportTileAnimationTrigger(Station *st, TileIndex tile, AirpAnimationTrigger trigger, CargoID cargo_type = CT_INVALID);
void AirportAnimationTrigger(Station *st, AirpAnimationTrigger trigger, CargoID cargo_type = CT_INVALID);
bool DrawNewAirportTile(TileInfo *ti, Station *st, const AirportTileSpec *airts);

#endif /* NEWGRF_AIRPORTTILES_H */
