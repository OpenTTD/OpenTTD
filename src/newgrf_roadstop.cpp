/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file command.cpp Handling of NewGRF road stops. */

#include "stdafx.h"
#include "debug.h"
#include "station_base.h"
#include "roadstop_base.h"
#include "newgrf_badge.h"
#include "newgrf_roadstop.h"
#include "newgrf_cargo.h"
#include "newgrf_roadtype.h"
#include "gfx_type.h"
#include "company_func.h"
#include "road.h"
#include "window_type.h"
#include "timer/timer_game_calendar.h"
#include "town.h"
#include "tile_cmd.h"
#include "viewport_func.h"
#include "newgrf_animation_base.h"
#include "newgrf_sound.h"

#include "table/strings.h"

#include "newgrf_class_func.h"

#include "safeguards.h"

template <>
void RoadStopClass::InsertDefaults()
{
	/* Set up initial data */
	RoadStopClass::Get(RoadStopClass::Allocate(ROADSTOP_CLASS_LABEL_DEFAULT))->name = STR_STATION_CLASS_DFLT;
	RoadStopClass::Get(RoadStopClass::Allocate(ROADSTOP_CLASS_LABEL_DEFAULT))->Insert(nullptr);
	RoadStopClass::Get(RoadStopClass::Allocate(ROADSTOP_CLASS_LABEL_WAYPOINT))->name = STR_STATION_CLASS_WAYP;
	RoadStopClass::Get(RoadStopClass::Allocate(ROADSTOP_CLASS_LABEL_WAYPOINT))->Insert(nullptr);
}

template <>
bool RoadStopClass::IsUIAvailable(uint) const
{
	return true;
}

/* Instantiate RoadStopClass. */
template class NewGRFClass<RoadStopSpec, RoadStopClassID, ROADSTOP_CLASS_MAX>;

static const uint NUM_ROADSTOPSPECS_PER_STATION = 63; ///< Maximum number of parts per station.

uint32_t RoadStopScopeResolver::GetRandomBits() const
{
	if (this->st == nullptr) return 0;

	uint32_t bits = this->st->random_bits;
	if (this->tile != INVALID_TILE && Station::IsExpected(this->st)) {
		bits |= Station::From(this->st)->GetRoadStopRandomBits(this->tile) << 16;
	}
	return bits;
}

uint32_t RoadStopScopeResolver::GetRandomTriggers() const
{
	return this->st == nullptr ? 0 : this->st->waiting_random_triggers.base();
}

uint32_t RoadStopScopeResolver::GetVariable(uint8_t variable, [[maybe_unused]] uint32_t parameter, bool &available) const
{
	auto get_road_type_variable = [&](RoadTramType rtt) -> uint32_t {
		RoadType rt;
		if (this->tile == INVALID_TILE) {
			rt = (GetRoadTramType(this->roadtype) == rtt) ? this->roadtype : INVALID_ROADTYPE;
		} else {
			rt = GetRoadType(this->tile, rtt);
		}
		if (rt == INVALID_ROADTYPE) {
			return 0xFFFFFFFF;
		} else {
			return GetReverseRoadTypeTranslation(rt, this->roadstopspec->grf_prop.grffile);
		}
	};

	switch (variable) {
		/* View/rotation */
		case 0x40: return this->view;

		/* Stop type: 0: bus, 1: truck, 2: waypoint */
		case 0x41:
			if (this->type == StationType::Bus) return 0;
			if (this->type == StationType::Truck) return 1;
			return 2;

		/* Terrain type */
		case 0x42: return this->tile == INVALID_TILE ? 0 : (GetTileSlope(this->tile) << 8 | GetTerrainType(this->tile, TCX_NORMAL));

		/* Road type */
		case 0x43: return get_road_type_variable(RTT_ROAD);

		/* Tram type */
		case 0x44: return get_road_type_variable(RTT_TRAM);

		/* Town zone and Manhattan distance of closest town */
		case 0x45: {
			if (this->tile == INVALID_TILE) return to_underlying(HouseZone::TownEdge) << 16;
			const Town *t = (this->st == nullptr) ? ClosestTownFromTile(this->tile, UINT_MAX) : this->st->town;
			return t != nullptr ? (to_underlying(GetTownRadiusGroup(t, this->tile)) << 16 | ClampTo<uint16_t>(DistanceManhattan(this->tile, t->xy))) : to_underlying(HouseZone::TownEdge) << 16;
		}

		/* Get square of Euclidean distance of closest town */
		case 0x46: {
			if (this->tile == INVALID_TILE) return 0;
			const Town *t = (this->st == nullptr) ? ClosestTownFromTile(this->tile, UINT_MAX) : this->st->town;
			return t != nullptr ? DistanceSquare(this->tile, t->xy) : 0;
		}

		/* Company information */
		case 0x47: return GetCompanyInfo(this->st == nullptr ? _current_company : this->st->owner);

		/* Animation frame */
		case 0x49: return this->tile == INVALID_TILE ? 0 : this->st->GetRoadStopAnimationFrame(this->tile);

		/* Misc info */
		case 0x50: {
			uint32_t result = 0;
			if (this->tile == INVALID_TILE) {
				SetBit(result, 4);
			}
			return result;
		}

		/* Variables which use the parameter */
		/* Variables 0x60 to 0x65 and 0x69 are handled separately below */

		/* Animation frame of nearby tile */
		case 0x66: {
			if (this->tile == INVALID_TILE) return UINT_MAX;
			TileIndex tile = this->tile;
			if (parameter != 0) tile = GetNearbyTile(parameter, tile);
			return (IsAnyRoadStopTile(tile) && GetStationIndex(tile) == this->st->index) ? this->st->GetRoadStopAnimationFrame(tile) : UINT_MAX;
		}

		/* Land info of nearby tile */
		case 0x67: {
			if (this->tile == INVALID_TILE) return 0;
			TileIndex tile = this->tile;
			if (parameter != 0) tile = GetNearbyTile(parameter, tile); // only perform if it is required
			return GetNearbyTileInformation(tile, this->ro.grffile->grf_version >= 8);
		}

		/* Road stop info of nearby tiles */
		case 0x68: {
			if (this->tile == INVALID_TILE) return 0xFFFFFFFF;
			TileIndex nearby_tile = GetNearbyTile(parameter, this->tile);

			if (!IsAnyRoadStopTile(nearby_tile)) return 0xFFFFFFFF;

			uint32_t grfid = this->st->roadstop_speclist[GetCustomRoadStopSpecIndex(this->tile)].grfid;
			bool same_orientation = GetStationGfx(this->tile) == GetStationGfx(nearby_tile);
			bool same_station = GetStationIndex(nearby_tile) == this->st->index;
			uint32_t res = GetStationGfx(nearby_tile) << 12 | !same_orientation << 11 | !!same_station << 10;
			StationType type = GetStationType(nearby_tile);
			if (type == StationType::Truck) res |= (1 << 16);
			if (type == StationType::RoadWaypoint) res |= (2 << 16);
			if (type == this->type) SetBit(res, 20);

			if (IsCustomRoadStopSpecIndex(nearby_tile)) {
				const auto &sm = BaseStation::GetByTile(nearby_tile)->roadstop_speclist[GetCustomRoadStopSpecIndex(nearby_tile)];
				res |= 1 << (sm.grfid != grfid ? 9 : 8) | ClampTo<uint8_t>(sm.localidx);
			}
			return res;
		}

		/* GRFID of nearby road stop tiles */
		case 0x6A: {
			if (this->tile == INVALID_TILE) return 0xFFFFFFFF;
			TileIndex nearby_tile = GetNearbyTile(parameter, this->tile);

			if (!IsAnyRoadStopTile(nearby_tile)) return 0xFFFFFFFF;
			if (!IsCustomRoadStopSpecIndex(nearby_tile)) return 0;

			const auto &sm = BaseStation::GetByTile(nearby_tile)->roadstop_speclist[GetCustomRoadStopSpecIndex(nearby_tile)];
			return sm.grfid;
		}

		/* 16 bit road stop ID of nearby tiles */
		case 0x6B: {
			if (this->tile == INVALID_TILE) return 0xFFFFFFFF;
			TileIndex nearby_tile = GetNearbyTile(parameter, this->tile);

			if (!IsAnyRoadStopTile(nearby_tile)) return 0xFFFFFFFF;
			if (!IsCustomRoadStopSpecIndex(nearby_tile)) return 0xFFFE;

			uint32_t grfid = this->st->roadstop_speclist[GetCustomRoadStopSpecIndex(this->tile)].grfid;

			const auto &sm = BaseStation::GetByTile(nearby_tile)->roadstop_speclist[GetCustomRoadStopSpecIndex(nearby_tile)];
			if (sm.grfid == grfid) {
				return sm.localidx;
			}

			return 0xFFFE;
		}

		case 0x7A: return GetBadgeVariableResult(*this->ro.grffile, this->roadstopspec->badges, parameter);

		case 0xF0: return this->st == nullptr ? 0 : this->st->facilities.base(); // facilities

		case 0xFA: return ClampTo<uint16_t>((this->st == nullptr ? TimerGameCalendar::date : this->st->build_date) - CalendarTime::DAYS_TILL_ORIGINAL_BASE_YEAR); // build date
	}

	if (this->st != nullptr) return this->st->GetNewGRFVariable(this->ro, variable, parameter, available);

	available = false;
	return UINT_MAX;
}

RoadStopResolverObject::RoadStopResolverObject(const RoadStopSpec *roadstopspec, BaseStation *st, TileIndex tile, RoadType roadtype, StationType type, uint8_t view,
		CallbackID callback, uint32_t param1, uint32_t param2)
	: SpecializedResolverObject<StationRandomTriggers>(roadstopspec->grf_prop.grffile, callback, param1, param2), roadstop_scope(*this, st, roadstopspec, tile, roadtype, type, view)
{
	CargoType ctype = CargoGRFFileProps::SG_DEFAULT_NA;

	if (st == nullptr) {
		/* No station, so we are in a purchase list */
		ctype = CargoGRFFileProps::SG_PURCHASE;
	} else if (Station::IsExpected(st)) {
		const Station *station = Station::From(st);
		/* Pick the first cargo that we have waiting */
		for (const auto &[cargo, spritegroup] : roadstopspec->grf_prop.spritegroups) {
			if (cargo < NUM_CARGO && station->goods[cargo].HasData() && station->goods[cargo].GetData().cargo.TotalCount() > 0) {
				ctype = cargo;
				this->root_spritegroup = spritegroup;
				break;
			}
		}
	}

	this->root_spritegroup = this->roadstop_scope.roadstopspec->grf_prop.GetSpriteGroup(ctype);
	if (this->root_spritegroup == nullptr) {
		ctype = CargoGRFFileProps::SG_DEFAULT;
		this->root_spritegroup = this->roadstop_scope.roadstopspec->grf_prop.GetSpriteGroup(ctype);
	}

	/* Remember the cargo type we've picked */
	this->roadstop_scope.cargo_type = ctype;
}

TownScopeResolver *RoadStopResolverObject::GetTown()
{
	if (!this->town_scope.has_value()) {
		Town *t;
		if (this->roadstop_scope.st != nullptr) {
			t = this->roadstop_scope.st->town;
		} else {
			t = ClosestTownFromTile(this->roadstop_scope.tile, UINT_MAX);
		}
		if (t == nullptr) return nullptr;
		this->town_scope.emplace(*this, t, this->roadstop_scope.st == nullptr);
	}
	return &*this->town_scope;
}

uint16_t GetRoadStopCallback(CallbackID callback, uint32_t param1, uint32_t param2, const RoadStopSpec *roadstopspec, BaseStation *st, TileIndex tile, RoadType roadtype, StationType type, uint8_t view, std::span<int32_t> regs100)
{
	RoadStopResolverObject object(roadstopspec, st, tile, roadtype, type, view, callback, param1, param2);
	return object.ResolveCallback(regs100);
}

/**
 * Draw representation of a road stop tile for GUI purposes.
 * @param x position x of image.
 * @param y position y of image.
 * @param image an int offset for the sprite.
 * @param roadtype the RoadType of the underlying road.
 * @param spec the RoadStop's spec.
 * @return true of the tile was drawn (allows for fallback to default graphics)
 */
void DrawRoadStopTile(int x, int y, RoadType roadtype, const RoadStopSpec *spec, StationType type, int view)
{
	assert(roadtype != INVALID_ROADTYPE);
	assert(spec != nullptr);

	const RoadTypeInfo *rti = GetRoadTypeInfo(roadtype);
	RoadStopResolverObject object(spec, nullptr, INVALID_TILE, roadtype, type, view);
	const auto *group = object.Resolve<TileLayoutSpriteGroup>();
	if (group == nullptr) return;
	auto processor = group->ProcessRegisters(object, nullptr);
	auto dts = processor.GetLayout();

	PaletteID palette = GetCompanyPalette(_local_company);

	SpriteID image = dts.ground.sprite;
	PaletteID pal = dts.ground.pal;

	RoadStopDrawModes draw_mode;
	if (spec->flags.Test(RoadStopSpecFlag::DrawModeRegister)) {
		draw_mode = static_cast<RoadStopDrawModes>(object.GetRegister(0x100));
	} else {
		draw_mode = spec->draw_mode;
	}

	if (type == StationType::RoadWaypoint) {
		DrawSprite(SPR_ROAD_PAVED_STRAIGHT_X, PAL_NONE, x, y);
		if (draw_mode.Test(RoadStopDrawMode::WaypGround) && GB(image, 0, SPRITE_WIDTH) != 0) {
			DrawSprite(image, GroundSpritePaletteTransform(image, pal, palette), x, y);
		}
	} else if (GB(image, 0, SPRITE_WIDTH) != 0) {
		DrawSprite(image, GroundSpritePaletteTransform(image, pal, palette), x, y);
	}

	if (view >= 4) {
		/* Drive-through stop */
		uint sprite_offset = 5 - view;

		/* Road underlay takes precedence over tram */
		if (type == StationType::RoadWaypoint || draw_mode.Test(RoadStopDrawMode::Overlay)) {
			if (rti->UsesOverlay()) {
				SpriteID ground = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_GROUND);
				DrawSprite(ground + sprite_offset, PAL_NONE, x, y);

				SpriteID overlay = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_OVERLAY);
				if (overlay) DrawSprite(overlay + sprite_offset, PAL_NONE, x, y);
			} else if (RoadTypeIsTram(roadtype)) {
				DrawSprite(SPR_TRAMWAY_TRAM + sprite_offset, PAL_NONE, x, y);
			}
		}
	} else {
		/* Bay stop */
		if (draw_mode.Test(RoadStopDrawMode::Road) && rti->UsesOverlay()) {
			SpriteID ground = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_ROADSTOP);
			DrawSprite(ground + view, PAL_NONE, x, y);
		}
	}

	DrawCommonTileSeqInGUI(x, y, &dts, 0, 0, palette, true);
}

std::optional<SpriteLayoutProcessor> GetRoadStopLayout(TileInfo *ti, const RoadStopSpec *spec, BaseStation *st, StationType type, int view, std::span<int32_t> regs100)
{
	RoadStopResolverObject object(spec, st, ti->tile, INVALID_ROADTYPE, type, view);
	auto group = object.Resolve<TileLayoutSpriteGroup>();
	if (group == nullptr) return std::nullopt;
	for (uint i = 0; i < regs100.size(); ++i) {
		regs100[i] = object.GetRegister(0x100 + i);
	}
	return group->ProcessRegisters(object, nullptr);
}

/** Wrapper for animation control, see GetRoadStopCallback. */
uint16_t GetAnimRoadStopCallback(CallbackID callback, uint32_t param1, uint32_t param2, const RoadStopSpec *roadstopspec, BaseStation *st, TileIndex tile, int)
{
	return GetRoadStopCallback(callback, param1, param2, roadstopspec, st, tile, INVALID_ROADTYPE, GetStationType(tile), GetStationGfx(tile));
}

struct RoadStopAnimationFrameAnimationHelper {
	static uint8_t Get(BaseStation *st, TileIndex tile) { return st->GetRoadStopAnimationFrame(tile); }
	static bool Set(BaseStation *st, TileIndex tile, uint8_t frame) { return st->SetRoadStopAnimationFrame(tile, frame); }
};

/** Helper class for animation control. */
struct RoadStopAnimationBase : public AnimationBase<RoadStopAnimationBase, RoadStopSpec, BaseStation, int, GetAnimRoadStopCallback, RoadStopAnimationFrameAnimationHelper> {
	static constexpr CallbackID cb_animation_speed      = CBID_STATION_ANIMATION_SPEED;
	static constexpr CallbackID cb_animation_next_frame = CBID_STATION_ANIMATION_NEXT_FRAME;

	static constexpr RoadStopCallbackMask cbm_animation_speed      = RoadStopCallbackMask::AnimationSpeed;
	static constexpr RoadStopCallbackMask cbm_animation_next_frame = RoadStopCallbackMask::AnimationNextFrame;
};

void AnimateRoadStopTile(TileIndex tile)
{
	const RoadStopSpec *ss = GetRoadStopSpec(tile);
	if (ss == nullptr) return;

	RoadStopAnimationBase::AnimateTile(ss, BaseStation::GetByTile(tile), tile, ss->flags.Test(RoadStopSpecFlag::Cb141RandomBits));
}

void TriggerRoadStopAnimation(BaseStation *st, TileIndex trigger_tile, StationAnimationTrigger trigger, CargoType cargo_type)
{
	assert(st != nullptr);

	/* Check the cached animation trigger bitmask to see if we need
	 * to bother with any further processing. */
	if (!st->cached_roadstop_anim_triggers.Test(trigger)) return;

	uint16_t random_bits = Random();
	auto process_tile = [&](TileIndex cur_tile) {
		const RoadStopSpec *ss = GetRoadStopSpec(cur_tile);
		if (ss != nullptr && ss->animation.triggers.Test(trigger)) {
			uint8_t var18_extra = 0;
			if (IsValidCargoType(cargo_type)) {
				var18_extra |= ss->grf_prop.grffile->cargo_map[cargo_type] << 8;
			}
			RoadStopAnimationBase::ChangeAnimationFrame(CBID_STATION_ANIMATION_TRIGGER, ss, st, cur_tile, (random_bits << 16) | GB(Random(), 0, 16), to_underlying(trigger) | var18_extra);
		}
	};

	if (trigger == StationAnimationTrigger::NewCargo || trigger == StationAnimationTrigger::CargoTaken || trigger == StationAnimationTrigger::AcceptanceTick) {
		for (const RoadStopTileData &tile_data : st->custom_roadstop_tile_data) {
			process_tile(tile_data.tile);
		}
	} else {
		process_tile(trigger_tile);
	}
}

/**
 * Trigger road stop randomisation
 *
 * @param st the station being triggered
 * @param tile the exact tile of the station that should be triggered
 * @param trigger trigger type
 * @param cargo_type cargo type causing the trigger
 */
void TriggerRoadStopRandomisation(BaseStation *st, TileIndex tile, StationRandomTrigger trigger, CargoType cargo_type)
{
	assert(st != nullptr);

	/* Check the cached cargo trigger bitmask to see if we need
	 * to bother with any further processing.
	 * Note: cached_roadstop_cargo_triggers must be non-zero even for cargo-independent triggers. */
	if (st->cached_roadstop_cargo_triggers == 0) return;
	if (IsValidCargoType(cargo_type) && !HasBit(st->cached_roadstop_cargo_triggers, cargo_type)) return;

	st->waiting_random_triggers.Set(trigger);

	uint32_t whole_reseed = 0;

	/* Bitmask of completely empty cargo types to be matched. */
	CargoTypes empty_mask{};
	if (trigger == StationRandomTrigger::CargoTaken) {
		empty_mask = GetEmptyMask(Station::From(st));
	}

	StationRandomTriggers used_random_triggers;
	auto process_tile = [&](TileIndex cur_tile) {
		const RoadStopSpec *ss = GetRoadStopSpec(cur_tile);
		if (ss == nullptr) return;

		/* Cargo taken "will only be triggered if all of those
		 * cargo types have no more cargo waiting." */
		if (trigger == StationRandomTrigger::CargoTaken) {
			if ((ss->cargo_triggers & ~empty_mask) != 0) return;
		}

		if (!IsValidCargoType(cargo_type) || HasBit(ss->cargo_triggers, cargo_type)) {
			RoadStopResolverObject object(ss, st, cur_tile, INVALID_ROADTYPE, GetStationType(cur_tile), GetStationGfx(cur_tile));
			object.SetWaitingRandomTriggers(st->waiting_random_triggers);

			object.ResolveRerandomisation();

			used_random_triggers.Set(object.GetUsedRandomTriggers());

			uint32_t reseed = object.GetReseedSum();
			if (reseed != 0) {
				whole_reseed |= reseed;
				reseed >>= 16;

				/* Set individual tile random bits */
				uint8_t random_bits = st->GetRoadStopRandomBits(cur_tile);
				random_bits &= ~reseed;
				random_bits |= Random() & reseed;
				st->SetRoadStopRandomBits(cur_tile, random_bits);

				MarkTileDirtyByTile(cur_tile);
			}
		}
	};
	if (trigger == StationRandomTrigger::NewCargo || trigger == StationRandomTrigger::CargoTaken) {
		for (const RoadStopTileData &tile_data : st->custom_roadstop_tile_data) {
			process_tile(tile_data.tile);
		}
	} else {
		process_tile(tile);
	}

	/* Update whole station random bits */
	st->waiting_random_triggers.Reset(used_random_triggers);
	if ((whole_reseed & 0xFFFF) != 0) {
		st->random_bits &= ~whole_reseed;
		st->random_bits |= Random() & whole_reseed;
	}
}

/**
 * Checks if there's any new stations by a specific RoadStopType
 * @param rs the RoadStopType to check.
 * @param roadtype the RoadType to check.
 * @return true if there was any new RoadStopSpec's found for the given RoadStopType and RoadType, else false.
 */
bool GetIfNewStopsByType(RoadStopType rs, RoadType roadtype)
{
	for (const auto &cls : RoadStopClass::Classes()) {
		/* Ignore the waypoint class. */
		if (IsWaypointClass(cls)) continue;
		/* Ignore the default class with only the default station. */
		if (cls.Index() == ROADSTOP_CLASS_DFLT && cls.GetSpecCount() == 1) continue;
		if (GetIfClassHasNewStopsByType(&cls, rs, roadtype)) return true;
	}
	return false;
}

/**
 * Checks if the given RoadStopClass has any specs assigned to it, compatible with the given RoadStopType.
 * @param roadstopclass the RoadStopClass to check.
 * @param rs the RoadStopType to check.
 * @param roadtype the RoadType to check.
 * @return true if the RoadStopSpec has any specs compatible with the given RoadStopType and RoadType.
 */
bool GetIfClassHasNewStopsByType(const RoadStopClass *roadstopclass, RoadStopType rs, RoadType roadtype)
{
	for (const auto spec : roadstopclass->Specs()) {
		if (GetIfStopIsForType(spec, rs, roadtype)) return true;
	}
	return false;
}

/**
 * Checks if the given RoadStopSpec is compatible with the given RoadStopType.
 * @param roadstopspec the RoadStopSpec to check.
 * @param rs the RoadStopType to check.
 * @param roadtype the RoadType to check.
 * @return true if the RoadStopSpec is compatible with the given RoadStopType and RoadType.
 */
bool GetIfStopIsForType(const RoadStopSpec *roadstopspec, RoadStopType rs, RoadType roadtype)
{
	/* The roadstopspec is nullptr, must be the default station, always return true. */
	if (roadstopspec == nullptr) return true;

	if (roadstopspec->flags.Test(RoadStopSpecFlag::RoadOnly) && !RoadTypeIsRoad(roadtype)) return false;
	if (roadstopspec->flags.Test(RoadStopSpecFlag::TramOnly) && !RoadTypeIsTram(roadtype)) return false;

	if (roadstopspec->stop_type == ROADSTOPTYPE_ALL) return true;

	switch (rs) {
		case RoadStopType::Bus:
			if (roadstopspec->stop_type == ROADSTOPTYPE_PASSENGER) return true;
			break;

		case RoadStopType::Truck:
			if (roadstopspec->stop_type == ROADSTOPTYPE_FREIGHT) return true;
			break;

		default:
			NOT_REACHED();
	}
	return false;
}

const RoadStopSpec *GetRoadStopSpec(TileIndex t)
{
	if (!IsCustomRoadStopSpecIndex(t)) return nullptr;

	const BaseStation *st = BaseStation::GetByTile(t);
	uint specindex = GetCustomRoadStopSpecIndex(t);
	return specindex < st->roadstop_speclist.size() ? st->roadstop_speclist[specindex].spec : nullptr;
}

/**
 * Allocate a RoadStopSpec to a Station. This is called once per build operation.
 * @param spec RoadStopSpec to allocate.
 * @param st Station to allocate it to.
 * @return Index within the Station's road stop spec list, or std::nullopt if the allocation failed.
 */
std::optional<uint8_t> AllocateSpecToRoadStop(const RoadStopSpec *spec, BaseStation *st)
{
	uint i;

	if (spec == nullptr) return 0;

	/* If station doesn't exist yet then the first slot is available. */
	if (st == nullptr) return 1;

	/* Try to find the same spec and return that one */
	for (i = 1; i < st->roadstop_speclist.size() && i < NUM_ROADSTOPSPECS_PER_STATION; i++) {
		if (st->roadstop_speclist[i].spec == spec) return i;
	}

	/* Try to find an unused spec slot */
	for (i = 1; i < st->roadstop_speclist.size() && i < NUM_ROADSTOPSPECS_PER_STATION; i++) {
		if (st->roadstop_speclist[i].spec == nullptr && st->roadstop_speclist[i].grfid == 0) break;
	}

	if (i == NUM_ROADSTOPSPECS_PER_STATION) {
		/* Full, give up */
		return std::nullopt;
	}

	return i;
}

/**
 * Assign a previously allocated RoadStopSpec specindex to a Station.
 * @param spec RoadStopSpec to assign..
 * @param st Station to allocate it to.
 * @param specindex Spec index of allocation.
 */
void AssignSpecToRoadStop(const RoadStopSpec *spec, BaseStation *st, uint8_t specindex)
{
	if (specindex == 0) return;
	if (specindex >= st->roadstop_speclist.size()) st->roadstop_speclist.resize(specindex + 1);

	st->roadstop_speclist[specindex].spec = spec;
	st->roadstop_speclist[specindex].grfid = spec->grf_prop.grfid;
	st->roadstop_speclist[specindex].localidx = spec->grf_prop.local_id;

	RoadStopUpdateCachedTriggers(st);
}

/**
 * Deallocate a RoadStopSpec from a Station. Called when removing a single roadstop tile.
 * @param st Station to work with.
 * @param specindex Index of the custom roadstop within the Station's roadstop spec list.
 */
void DeallocateSpecFromRoadStop(BaseStation *st, uint8_t specindex)
{
	/* specindex of 0 (default) is never freeable */
	if (specindex == 0) return;

	/* Check custom road stop tiles if the specindex is still in use */
	for (const RoadStopTileData &tile_data : st->custom_roadstop_tile_data) {
		if (GetCustomRoadStopSpecIndex(tile_data.tile) == specindex) {
			return;
		}
	}

	/* This specindex is no longer in use, so deallocate it */
	st->roadstop_speclist[specindex].spec     = nullptr;
	st->roadstop_speclist[specindex].grfid    = 0;
	st->roadstop_speclist[specindex].localidx = 0;

	/* If this was the highest spec index, reallocate */
	if (specindex == st->roadstop_speclist.size() - 1) {
		size_t num_specs;
		for (num_specs = st->roadstop_speclist.size() - 1; num_specs > 0; num_specs--) {
			if (st->roadstop_speclist[num_specs].grfid != 0) break;
		}

		if (num_specs > 0) {
			st->roadstop_speclist.resize(num_specs + 1);
		} else {
			st->roadstop_speclist.clear();
			st->cached_roadstop_anim_triggers = {};
			st->cached_roadstop_cargo_triggers = 0;
			return;
		}
	}

	RoadStopUpdateCachedTriggers(st);
}

/**
 * Update the cached animation trigger bitmask for a station.
 * @param st Station to update.
 */
void RoadStopUpdateCachedTriggers(BaseStation *st)
{
	st->cached_roadstop_anim_triggers = {};
	st->cached_roadstop_cargo_triggers = 0;

	/* Combine animation trigger bitmask for all road stop specs
	 * of this station. */
	for (const auto &sm : GetStationSpecList<RoadStopSpec>(st)) {
		if (sm.spec == nullptr) continue;
		st->cached_roadstop_anim_triggers.Set(sm.spec->animation.triggers);
		st->cached_roadstop_cargo_triggers |= sm.spec->cargo_triggers;
	}
}
