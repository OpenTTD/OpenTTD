/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_station.h Header file for NewGRF stations */

#ifndef NEWGRF_STATION_H
#define NEWGRF_STATION_H

#include "newgrf_animation_type.h"
#include "newgrf_callbacks.h"
#include "newgrf_class.h"
#include "newgrf_commons.h"
#include "cargo_type.h"
#include "station_type.h"
#include "rail_type.h"
#include "newgrf_spritegroup.h"
#include "newgrf_town.h"

/** Scope resolver for stations. */
struct StationScopeResolver : public ScopeResolver {
	TileIndex tile;                     ///< %Tile of the station.
	struct BaseStation *st;             ///< Instance of the station.
	const struct StationSpec *statspec; ///< Station (type) specification.
	CargoID cargo_type;                 ///< Type of cargo of the station.
	Axis axis;                          ///< Station axis, used only for the slope check callback.

	/**
	 * Constructor for station scopes.
	 * @param ro Surrounding resolver.
	 * @param statspec Station (type) specification.
	 * @param st Instance of the station.
	 * @param tile %Tile of the station.
	 */
	StationScopeResolver(ResolverObject &ro, const StationSpec *statspec, BaseStation *st, TileIndex tile)
		: ScopeResolver(ro), tile(tile), st(st), statspec(statspec), cargo_type(CT_INVALID), axis(INVALID_AXIS)
	{
	}

	uint32_t GetRandomBits() const override;
	uint32_t GetTriggers() const override;

	uint32_t GetVariable(byte variable, [[maybe_unused]] uint32_t parameter, bool *available) const override;
};

/** Station resolver. */
struct StationResolverObject : public ResolverObject {
	StationScopeResolver station_scope; ///< The station scope resolver.
	TownScopeResolver *town_scope;      ///< The town scope resolver (created on the first call).

	StationResolverObject(const StationSpec *statspec, BaseStation *st, TileIndex tile,
			CallbackID callback = CBID_NO_CALLBACK, uint32_t callback_param1 = 0, uint32_t callback_param2 = 0);
	~StationResolverObject();

	TownScopeResolver *GetTown();

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0) override
	{
		switch (scope) {
			case VSG_SCOPE_SELF:
				return &this->station_scope;

			case VSG_SCOPE_PARENT: {
				TownScopeResolver *tsr = this->GetTown();
				if (tsr != nullptr) return tsr;
				FALLTHROUGH;
			}

			default:
				return ResolverObject::GetScope(scope, relative);
		}
	}

	const SpriteGroup *ResolveReal(const RealSpriteGroup *group) const override;

	GrfSpecFeature GetFeature() const override;
	uint32_t GetDebugID() const override;
};

enum StationClassID : byte {
	STAT_CLASS_BEGIN = 0,    ///< the lowest valid value
	STAT_CLASS_DFLT = 0,     ///< Default station class.
	STAT_CLASS_WAYP,         ///< Waypoint class.
	STAT_CLASS_MAX = 255,    ///< Maximum number of classes.
};

/** Allow incrementing of StationClassID variables */
DECLARE_POSTFIX_INCREMENT(StationClassID)

enum StationSpecFlags {
	SSF_SEPARATE_GROUND,      ///< Use different sprite set for ground sprites.
	SSF_DIV_BY_STATION_SIZE,  ///< Divide cargo amount by station size.
	SSF_CB141_RANDOM_BITS,    ///< Callback 141 needs random bits.
	SSF_CUSTOM_FOUNDATIONS,   ///< Draw custom foundations.
	SSF_EXTENDED_FOUNDATIONS, ///< Extended foundation block instead of simple.
};

/** Randomisation triggers for stations */
enum StationRandomTrigger {
	SRT_NEW_CARGO,        ///< Trigger station on new cargo arrival.
	SRT_CARGO_TAKEN,      ///< Trigger station when cargo is completely taken.
	SRT_TRAIN_ARRIVES,    ///< Trigger platform when train arrives.
	SRT_TRAIN_DEPARTS,    ///< Trigger platform when train leaves.
	SRT_TRAIN_LOADS,      ///< Trigger platform when train loads/unloads.
	SRT_PATH_RESERVATION, ///< Trigger platform when train reserves path.
};

/** Station specification. */
struct StationSpec {
	StationSpec() : cls_id(STAT_CLASS_DFLT), name(0),
		disallowed_platforms(0), disallowed_lengths(0),
		cargo_threshold(0), cargo_triggers(0),
		callback_mask(0), flags(0), pylons(0), wires(0), blocked(0),
		animation({0, 0, 0, 0}) {}
	/**
	 * Properties related the the grf file.
	 * NUM_CARGO real cargo plus three pseudo cargo sprite groups.
	 * Used for obtaining the sprite offset of custom sprites, and for
	 * evaluating callbacks.
	 */
	GRFFilePropsBase<NUM_CARGO + 3> grf_prop;
	StationClassID cls_id;     ///< The class to which this spec belongs.
	StringID name;             ///< Name of this station.

	/**
	 * Bitmask of number of platforms available for the station.
	 * 0..6 correspond to 1..7, while bit 7 corresponds to >7 platforms.
	 */
	byte disallowed_platforms;
	/**
	 * Bitmask of platform lengths available for the station.
	 * 0..6 correspond to 1..7, while bit 7 corresponds to >7 tiles long.
	 */
	byte disallowed_lengths;

	/**
	 * Number of tile layouts.
	 * A minimum of 8 is required is required for stations.
	 * 0-1 = plain platform
	 * 2-3 = platform with building
	 * 4-5 = platform with roof, left side
	 * 6-7 = platform with roof, right side
	 */
	std::vector<NewGRFSpriteLayout> renderdata; ///< Array of tile layouts.

	/**
	 * Cargo threshold for choosing between little and lots of cargo
	 * @note little/lots are equivalent to the moving/loading states for vehicles
	 */
	uint16_t cargo_threshold;

	CargoTypes cargo_triggers; ///< Bitmask of cargo types which cause trigger re-randomizing

	byte callback_mask; ///< Bitmask of station callbacks that have to be called

	byte flags; ///< Bitmask of flags, bit 0: use different sprite set; bit 1: divide cargo about by station size

	byte pylons;  ///< Bitmask of base tiles (0 - 7) which should contain elrail pylons
	byte wires;   ///< Bitmask of base tiles (0 - 7) which should contain elrail wires
	byte blocked; ///< Bitmask of base tiles (0 - 7) which are blocked to trains

	AnimationInfo animation;

	/**
	 * Custom platform layouts.
	 * This is a 2D array containing an array of tiles.
	 * 1st layer is platform lengths.
	 * 2nd layer is tracks (width).
	 * These can be sparsely populated, and the upper limit is not defined but
	 * limited to 255.
	 */
	std::vector<std::vector<std::vector<byte>>> layouts;
};

/** Struct containing information relating to station classes. */
typedef NewGRFClass<StationSpec, StationClassID, STAT_CLASS_MAX> StationClass;

const StationSpec *GetStationSpec(TileIndex t);

/* Evaluate a tile's position within a station, and return the result a bitstuffed format. */
uint32_t GetPlatformInfo(Axis axis, byte tile, int platforms, int length, int x, int y, bool centred);

SpriteID GetCustomStationRelocation(const StationSpec *statspec, BaseStation *st, TileIndex tile, uint32_t var10 = 0);
SpriteID GetCustomStationFoundationRelocation(const StationSpec *statspec, BaseStation *st, TileIndex tile, uint layout, uint edge_info);
uint16_t GetStationCallback(CallbackID callback, uint32_t param1, uint32_t param2, const StationSpec *statspec, BaseStation *st, TileIndex tile);
CommandCost PerformStationTileSlopeCheck(TileIndex north_tile, TileIndex cur_tile, const StationSpec *statspec, Axis axis, byte plat_len, byte numtracks);

/* Allocate a StationSpec to a Station. This is called once per build operation. */
int AllocateSpecToStation(const StationSpec *statspec, BaseStation *st, bool exec);

/* Deallocate a StationSpec from a Station. Called when removing a single station tile. */
void DeallocateSpecFromStation(BaseStation *st, byte specindex);

/* Draw representation of a station tile for GUI purposes. */
bool DrawStationTile(int x, int y, RailType railtype, Axis axis, StationClassID sclass, uint station);

void AnimateStationTile(TileIndex tile);
void TriggerStationAnimation(BaseStation *st, TileIndex tile, StationAnimationTrigger trigger, CargoID cargo_type = CT_INVALID);
void TriggerStationRandomisation(Station *st, TileIndex tile, StationRandomTrigger trigger, CargoID cargo_type = CT_INVALID);
void StationUpdateCachedTriggers(BaseStation *st);

#endif /* NEWGRF_STATION_H */
