#pragma once

#ifndef NEWGRF_ROADSTATION_H
#define NEWGRF_ROADSTATION_H

#include "newgrf_animation_type.h"
#include "newgrf_spritegroup.h"
#include "newgrf_class.h"
#include "newgrf_commons.h"
#include "newgrf_town.h"
#include "road.h"

/** The maximum amount of roadstops a single GRF is allowed to add */
static const int NUM_ROADSTOPS = 64000U;
static const int NUM_ROADSTOPS_PER_GRF = 255;

enum RoadStopClassID : byte {
	ROADSTOP_CLASS_BEGIN = 0, ///< lowest valid value.
	ROADSTOP_CLASS_DFLT = 0,  ///< default bus class
	ROADSTOP_CLASS_TRCK = 1, ///< truck class
	ROADSTOP_CLASS_MAX = 255, ///< maximum valid value.
};
DECLARE_POSTFIX_INCREMENT(RoadStopClassID)

/* Some Triggers etc. */
enum RoadStopRandomTrigger {
	RSRT_NEW_CARGO,       ///< Trigger roadstop on arrival of new cargo.
	RSRT_CARGO_TAKEN,     ///< Trigger roadstop when cargo is completely taken.
	RSRT_VEH_ARRIVES,     ///< Trigger roadstop when roadvehicle arrives.
	RSRT_VEH_DEPARTS,     ///< Trigger roadstop when roadvehicle arrives.
	RSRT_VEH_LOADS,       ///< Trigger roadstop when roadvehicle loads.
};

/**
 * Various different options for availability, restricting
 * the roadstop to be only for busses or for trucks.
 */
enum RoadStopAvailabilityType : byte {
	ROADSTOPTYPE_PASSENGER,    ///< This RoadStop is for all passenger stops, this includes bus and tram stops.
	ROADSTOPTYPE_FREIGHT,      ///< This RoadStop is for all freight stops, this includes truck and freight tram stops.
	ROADSTOPTYPE_ALL,          ///< This RoadStop is for all types of road stops.

	ROADSTOPTYPE_NONE,
};

/**
 * Different draw modes to disallow rendering of some parts of the stop
 * or road.
 */
enum RoadStopDrawMode : byte {
	ROADSTOP_DRAW_MODE_NONE    = 0,
	ROADSTOP_DRAW_MODE_ROAD    = 1 << 0, ///< 0b01, Draw the road itself
	ROADSTOP_DRAW_MODE_OVERLAY = 1 << 1, ///< 0b10, Draw the road overlay for roadstops, e.g. pavement
};
DECLARE_ENUM_AS_BIT_SET(RoadStopDrawMode)

/** Scope resolver for road stops. */
struct RoadStopScopeResolver : public ScopeResolver {
	TileIndex tile;                             ///< %Tile of the station.
	struct BaseStation *st;                     ///< Instance of the station.
	const struct RoadStopSpec *roadstopspec;    ///< Station (type) specification.
	CargoID cargo_type;                         ///< Type of cargo of the station.
	uint8 view;                                 ///< Station axis.
	const RoadTypeInfo *rti;                    ///< Road type info

	RoadStopScopeResolver(ResolverObject& ro, BaseStation* st, TileIndex tile, const RoadTypeInfo *rti, uint8 view = 0)
		: ScopeResolver(ro), tile(tile), st(st), view(view), rti(rti)
		{

	}

	uint32 GetRandomBits() const override;
	uint32 GetTriggers() const override;

	uint32 GetVariable(byte variable, uint32 parameter, bool *available) const override;
};

/** Road stop resolver. */
struct RoadStopResolverObject : public ResolverObject {
	RoadStopScopeResolver roadstop_scope; ///< The stop scope resolver.
	TownScopeResolver *town_scope;        ///< The town scope resolver (created on the first call).

	RoadStopResolverObject(const RoadStopSpec* roadstopspec, BaseStation* st, TileIndex tile, const RoadTypeInfo *rti, uint8 view, CallbackID callback = CBID_NO_CALLBACK, uint32 param1 = 0, uint32 param2 = 0);
	~RoadStopResolverObject();

	ScopeResolver* GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0) override {
		switch (scope) {
			case VSG_SCOPE_SELF: return &this->roadstop_scope;
			case VSG_SCOPE_PARENT: {
				TownScopeResolver *tsr = this->GetTown();
				if (tsr != nullptr) return tsr;
				FALLTHROUGH;
			}
			default: return ResolverObject::GetScope(scope, relative);
		}
	}

	TownScopeResolver *GetTown();

	const SpriteGroup *ResolveReal(const RealSpriteGroup *group) const override;
};

/** Road stop specification. */
struct RoadStopSpec {
	// We'll have a default and a fence "cargo". Or maybe just a default one?
	GRFFilePropsBase<NUM_CARGO + 1> grf_prop;
	RoadStopClassID cls_id;     ///< The class to which this spec belongs.
	int spec_id;                ///< The ID of this spec inside the class.
	StringID name;              ///< Name of this stop

	RoadStopAvailabilityType stop_type = ROADSTOPTYPE_ALL;
	RoadStopDrawMode draw_mode = ROADSTOP_DRAW_MODE_ROAD | ROADSTOP_DRAW_MODE_OVERLAY;

	static const RoadStopSpec *Get(uint16 index);
};

template <>
struct EnumPropsT<RoadStopClassID> : MakeEnumPropsT<RoadStopClassID, byte, ROADSTOP_CLASS_BEGIN, ROADSTOP_CLASS_MAX, ROADSTOP_CLASS_MAX, 8> {};

typedef NewGRFClass<RoadStopSpec, RoadStopClassID, ROADSTOP_CLASS_MAX> RoadStopClass;

void DrawRoadStopTile(int x, int y, RoadType roadtype, const RoadStopSpec *spec, int view);

void TriggerRoadStopRandomisation(Station *st, TileIndex tile, RoadStopRandomTrigger trigger, CargoID cargo_type = CT_INVALID);

bool GetIfNewStopsByType(RoadStopType rs);
bool GetIfClassHasNewStopsByType(RoadStopClass *roadstopclass, RoadStopType rs);
bool GetIfStopIsForType(const RoadStopSpec *roadstopspec, RoadStopType rs);

uint GetCountOfCompatibleStopsByType(RoadStopClass *roadstopclass, RoadStopType rs);

#endif /* NEWGRF_ROADSTATION_H */
