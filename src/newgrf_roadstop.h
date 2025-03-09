/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file newgrf_roadstop.h NewGRF definitions and structures for road stops.
 */

#ifndef NEWGRF_ROADSTATION_H
#define NEWGRF_ROADSTATION_H

#include "newgrf_animation_type.h"
#include "newgrf_spritegroup.h"
#include "newgrf_badge_type.h"
#include "newgrf_callbacks.h"
#include "newgrf_class.h"
#include "newgrf_commons.h"
#include "newgrf_town.h"
#include "road.h"

/** The maximum amount of roadstops a single GRF is allowed to add */
static const int NUM_ROADSTOPS_PER_GRF = UINT16_MAX - 1;

static const uint32_t ROADSTOP_CLASS_LABEL_DEFAULT = 'DFLT';
static const uint32_t ROADSTOP_CLASS_LABEL_WAYPOINT = 'WAYP';

enum RoadStopClassID : uint16_t {
	ROADSTOP_CLASS_BEGIN = 0, ///< The lowest valid value
	ROADSTOP_CLASS_DFLT = 0, ///< Default road stop class.
	ROADSTOP_CLASS_WAYP, ///< Waypoint class.
	ROADSTOP_CLASS_MAX = UINT16_MAX, ///< Maximum number of classes.
};
DECLARE_INCREMENT_DECREMENT_OPERATORS(RoadStopClassID)

/* Some Triggers etc. */
enum RoadStopRandomTrigger : uint8_t {
	RSRT_NEW_CARGO,       ///< Trigger roadstop on arrival of new cargo.
	RSRT_CARGO_TAKEN,     ///< Trigger roadstop when cargo is completely taken.
	RSRT_VEH_ARRIVES,     ///< Trigger roadstop when road vehicle arrives.
	RSRT_VEH_DEPARTS,     ///< Trigger roadstop when road vehicle leaves.
	RSRT_VEH_LOADS,       ///< Trigger roadstop when road vehicle loads.
};

/**
 * Various different options for availability, restricting
 * the roadstop to be only for busses or for trucks.
 */
enum RoadStopAvailabilityType : uint8_t {
	ROADSTOPTYPE_PASSENGER,    ///< This RoadStop is for passenger (bus) stops.
	ROADSTOPTYPE_FREIGHT,      ///< This RoadStop is for freight (truck) stops.
	ROADSTOPTYPE_ALL,          ///< This RoadStop is for both types of station road stops.

	ROADSTOPTYPE_END,
};

/**
 * Different draw modes to disallow rendering of some parts of the stop
 * or road.
 */
enum class RoadStopDrawMode : uint8_t {
	Road = 0, ///< Bay stops: Draw the road itself
	Overlay = 1, ///< Drive-through stops: Draw the road overlay, e.g. pavement
	WaypGround = 2, ///< Waypoints: Draw the sprite layout ground tile (on top of the road)
};
using RoadStopDrawModes = EnumBitSet<RoadStopDrawMode, uint8_t>;

enum class RoadStopSpecFlag : uint8_t {
	Cb141RandomBits = 0, ///< Callback 141 needs random bits.
	NoCatenary = 2, ///< Do not show catenary.
	DriveThroughOnly = 3, ///< Stop is drive-through only.
	NoAutoRoadConnection = 4, ///< No auto road connection.
	RoadOnly = 5, ///< Only show in the road build menu (not tram).
	TramOnly = 6, ///< Only show in the tram build menu (not road).
	DrawModeRegister = 8, ///< Read draw mode from register 0x100.
};
using RoadStopSpecFlags = EnumBitSet<RoadStopSpecFlag, uint8_t>;

enum RoadStopView : uint8_t {
	RSV_BAY_NE                  = 0, ///< Bay road stop, facing Northeast
	RSV_BAY_SE                  = 1, ///< Bay road stop, facing Southeast
	RSV_BAY_SW                  = 2, ///< Bay road stop, facing Southwest
	RSV_BAY_NW                  = 3, ///< Bay road stop, facing Northwest
	RSV_DRIVE_THROUGH_X         = 4, ///< Drive through road stop, X axis
	RSV_DRIVE_THROUGH_Y         = 5, ///< Drive through road stop, Y axis
};

/** Scope resolver for road stops. */
struct RoadStopScopeResolver : public ScopeResolver {
	TileIndex tile{}; ///< %Tile of the station.
	struct BaseStation *st = nullptr; ///< Instance of the station.
	const struct RoadStopSpec *roadstopspec = nullptr; ///< Station (type) specification.
	CargoType cargo_type{}; ///< Type of cargo of the station.
	StationType type{}; ///< Station type.
	uint8_t view = 0; ///< Station axis.
	RoadType roadtype{}; ///< Road type (used when no tile)

	RoadStopScopeResolver(ResolverObject &ro, BaseStation *st, const RoadStopSpec *roadstopspec, TileIndex tile, RoadType roadtype, StationType type, uint8_t view = 0)
		: ScopeResolver(ro), tile(tile), st(st), roadstopspec(roadstopspec), type(type), view(view), roadtype(roadtype)
	{
	}

	uint32_t GetRandomBits() const override;
	uint32_t GetTriggers() const override;

	uint32_t GetVariable(uint8_t variable, [[maybe_unused]] uint32_t parameter, bool &available) const override;
};

/** Road stop resolver. */
struct RoadStopResolverObject : public ResolverObject {
	RoadStopScopeResolver roadstop_scope; ///< The stop scope resolver.
	std::optional<TownScopeResolver> town_scope = std::nullopt; ///< The town scope resolver (created on the first call).

	RoadStopResolverObject(const RoadStopSpec *roadstopspec, BaseStation *st, TileIndex tile, RoadType roadtype, StationType type, uint8_t view, CallbackID callback = CBID_NO_CALLBACK, uint32_t param1 = 0, uint32_t param2 = 0);

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, uint8_t relative = 0) override
	{
		switch (scope) {
			case VSG_SCOPE_SELF: return &this->roadstop_scope;
			case VSG_SCOPE_PARENT: {
				TownScopeResolver *tsr = this->GetTown();
				if (tsr != nullptr) return tsr;
				[[fallthrough]];
			}
			default: return ResolverObject::GetScope(scope, relative);
		}
	}

	TownScopeResolver *GetTown();

	const SpriteGroup *ResolveReal(const RealSpriteGroup *group) const override;
};

/** Road stop specification. */
struct RoadStopSpec : NewGRFSpecBase<RoadStopClassID> {
	/**
	 * Properties related the the grf file.
	 * NUM_CARGO real cargo plus three pseudo cargo sprite groups.
	 * Used for obtaining the sprite offset of custom sprites, and for
	 * evaluating callbacks.
	 */
	VariableGRFFileProps grf_prop;
	StringID name;              ///< Name of this stop

	RoadStopAvailabilityType stop_type = ROADSTOPTYPE_ALL;
	RoadStopDrawModes draw_mode = {RoadStopDrawMode::Road, RoadStopDrawMode::Overlay};
	RoadStopCallbackMasks callback_mask{};
	RoadStopSpecFlags flags{};

	CargoTypes cargo_triggers = 0; ///< Bitmask of cargo types which cause trigger re-randomizing

	AnimationInfo animation;

	uint8_t bridge_height[6];             ///< Minimum height for a bridge above, 0 for none
	uint8_t bridge_disallowed_pillars[6]; ///< Disallowed pillar flags for a bridge above

	uint8_t build_cost_multiplier = 16;  ///< Build cost multiplier per tile.
	uint8_t clear_cost_multiplier = 16;  ///< Clear cost multiplier per tile.

	std::vector<BadgeID> badges;

	/**
	 * Get the cost for building a road stop of this type.
	 * @return The cost for building.
	 */
	Money GetBuildCost(Price category) const { return GetPrice(category, this->build_cost_multiplier, this->grf_prop.grffile, -4); }

	/**
	 * Get the cost for clearing a road stop of this type.
	 * @return The cost for clearing.
	 */
	Money GetClearCost(Price category) const { return GetPrice(category, this->clear_cost_multiplier, this->grf_prop.grffile, -4); }

	static const RoadStopSpec *Get(uint16_t index);
};

using RoadStopClass = NewGRFClass<RoadStopSpec, RoadStopClassID, ROADSTOP_CLASS_MAX>;

void DrawRoadStopTile(int x, int y, RoadType roadtype, const RoadStopSpec *spec, StationType type, int view);

uint16_t GetRoadStopCallback(CallbackID callback, uint32_t param1, uint32_t param2, const RoadStopSpec *roadstopspec, BaseStation *st, TileIndex tile, RoadType roadtype, StationType type, uint8_t view);

void AnimateRoadStopTile(TileIndex tile);
uint8_t GetRoadStopTileAnimationSpeed(TileIndex tile);
void TriggerRoadStopAnimation(BaseStation *st, TileIndex tile, StationAnimationTrigger trigger, CargoType cargo_type = INVALID_CARGO);
void TriggerRoadStopRandomisation(Station *st, TileIndex tile, RoadStopRandomTrigger trigger, CargoType cargo_type = INVALID_CARGO);

bool GetIfNewStopsByType(RoadStopType rs, RoadType roadtype);
bool GetIfClassHasNewStopsByType(const RoadStopClass *roadstopclass, RoadStopType rs, RoadType roadtype);
bool GetIfStopIsForType(const RoadStopSpec *roadstopspec, RoadStopType rs, RoadType roadtype);

const RoadStopSpec *GetRoadStopSpec(TileIndex t);
int AllocateSpecToRoadStop(const RoadStopSpec *statspec, BaseStation *st, bool exec);
void DeallocateSpecFromRoadStop(BaseStation *st, uint8_t specindex);
void RoadStopUpdateCachedTriggers(BaseStation *st);

/**
 * Test if a RoadStopClass is the waypoint class.
 * @param cls RoadStopClass to test.
 * @return true if the class is the waypoint class.
 */
inline bool IsWaypointClass(const RoadStopClass &cls)
{
	return cls.global_id == ROADSTOP_CLASS_LABEL_WAYPOINT || GB(cls.global_id, 24, 8) == UINT8_MAX;
}

#endif /* NEWGRF_ROADSTATION_H */
