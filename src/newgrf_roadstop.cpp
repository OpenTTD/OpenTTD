/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file command.cpp Handling of NewGRF road stops. */

#include "stdafx.h"
#include "debug.h"
#include "station_base.h"
#include "roadstop_base.h"
#include "newgrf_roadstop.h"
#include "newgrf_class_func.h"
#include "newgrf_cargo.h"
#include "newgrf_roadtype.h"
#include "gfx_type.h"
#include "company_func.h"
#include "road.h"
#include "window_type.h"
#include "timer/timer_game_calendar.h"
#include "town.h"
#include "viewport_func.h"
#include "newgrf_animation_base.h"
#include "newgrf_sound.h"

#include "safeguards.h"

template <typename Tspec, typename Tid, Tid Tmax>
void NewGRFClass<Tspec, Tid, Tmax>::InsertDefaults()
{
	/* Set up initial data */
	RoadStopClass::Get(RoadStopClass::Allocate('DFLT'))->name = STR_STATION_CLASS_DFLT;
	RoadStopClass::Get(RoadStopClass::Allocate('DFLT'))->Insert(nullptr);
	RoadStopClass::Get(RoadStopClass::Allocate('WAYP'))->name = STR_STATION_CLASS_WAYP;
	RoadStopClass::Get(RoadStopClass::Allocate('WAYP'))->Insert(nullptr);
}

template <typename Tspec, typename Tid, Tid Tmax>
bool NewGRFClass<Tspec, Tid, Tmax>::IsUIAvailable(uint) const
{
	return true;
}

INSTANTIATE_NEWGRF_CLASS_METHODS(RoadStopClass, RoadStopSpec, RoadStopClassID, ROADSTOP_CLASS_MAX)

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

uint32_t RoadStopScopeResolver::GetTriggers() const
{
	return this->st == nullptr ? 0 : this->st->waiting_triggers;
}

uint32_t RoadStopScopeResolver::GetVariable(byte variable, [[maybe_unused]] uint32_t parameter, bool *available) const
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
			if (this->type == STATION_BUS) return 0;
			if (this->type == STATION_TRUCK) return 1;
			return 2;

		/* Terrain type */
		case 0x42: return this->tile == INVALID_TILE ? 0 : (GetTileSlope(this->tile) << 8 | GetTerrainType(this->tile, TCX_NORMAL));

		/* Road type */
		case 0x43: return get_road_type_variable(RTT_ROAD);

		/* Tram type */
		case 0x44: return get_road_type_variable(RTT_TRAM);

		/* Town zone and Manhattan distance of closest town */
		case 0x45: {
			if (this->tile == INVALID_TILE) return HZB_TOWN_EDGE << 16;
			const Town *t = (this->st == nullptr) ? ClosestTownFromTile(this->tile, UINT_MAX) : this->st->town;
			return t != nullptr ? (GetTownRadiusGroup(t, this->tile) << 16 | ClampTo<uint16_t>(DistanceManhattan(this->tile, t->xy))) : HZB_TOWN_EDGE << 16;
		}

		/* Get square of Euclidian distance of closest town */
		case 0x46: {
			if (this->tile == INVALID_TILE) return 0;
			const Town *t = (this->st == nullptr) ? ClosestTownFromTile(this->tile, UINT_MAX) : this->st->town;
			return t != nullptr ? DistanceSquare(this->tile, t->xy) : 0;
		}

		/* Company information */
		case 0x47: return GetCompanyInfo(this->st == nullptr ? _current_company : this->st->owner);

		/* Animation frame */
		case 0x49: return this->tile == INVALID_TILE ? 0 : this->st->GetRoadStopAnimationFrame(this->tile);

		/* Variables which use the parameter */
		/* Variables 0x60 to 0x65 and 0x69 are handled separately below */

		/* Animation frame of nearby tile */
		case 0x66: {
			if (this->tile == INVALID_TILE) return UINT_MAX;
			TileIndex tile = this->tile;
			if (parameter != 0) tile = GetNearbyTile(parameter, tile);
			return (IsRoadStopTile(tile) && GetStationIndex(tile) == this->st->index) ? this->st->GetRoadStopAnimationFrame(tile) : UINT_MAX;
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

			if (!IsRoadStopTile(nearby_tile)) return 0xFFFFFFFF;

			uint32_t grfid = this->st->roadstop_speclist[GetCustomRoadStopSpecIndex(this->tile)].grfid;
			bool same_orientation = GetStationGfx(this->tile) == GetStationGfx(nearby_tile);
			bool same_station = GetStationIndex(nearby_tile) == this->st->index;
			uint32_t res = GetStationGfx(nearby_tile) << 12 | !same_orientation << 11 | !!same_station << 10;
			StationType type = GetStationType(nearby_tile);
			if (type == STATION_TRUCK) res |= (1 << 16);
			if (type == this->type) SetBit(res, 20);

			if (IsCustomRoadStopSpecIndex(nearby_tile)) {
				const RoadStopSpecList ssl = BaseStation::GetByTile(nearby_tile)->roadstop_speclist[GetCustomRoadStopSpecIndex(nearby_tile)];
				res |= 1 << (ssl.grfid != grfid ? 9 : 8) | ClampTo<uint8_t>(ssl.localidx);
			}
			return res;
		}

		/* GRFID of nearby road stop tiles */
		case 0x6A: {
			if (this->tile == INVALID_TILE) return 0xFFFFFFFF;
			TileIndex nearby_tile = GetNearbyTile(parameter, this->tile);

			if (!IsRoadStopTile(nearby_tile)) return 0xFFFFFFFF;
			if (!IsCustomRoadStopSpecIndex(nearby_tile)) return 0;

			const RoadStopSpecList ssl = BaseStation::GetByTile(nearby_tile)->roadstop_speclist[GetCustomRoadStopSpecIndex(nearby_tile)];
			return ssl.grfid;
		}

		case 0xF0: return this->st == nullptr ? 0 : this->st->facilities; // facilities

		case 0xFA: return ClampTo<uint16_t>((this->st == nullptr ? TimerGameCalendar::date : this->st->build_date) - CalendarTime::DAYS_TILL_ORIGINAL_BASE_YEAR); // build date
	}

	if (this->st != nullptr) return this->st->GetNewGRFVariable(this->ro, variable, parameter, available);

	*available = false;
	return UINT_MAX;
}

const SpriteGroup *RoadStopResolverObject::ResolveReal(const RealSpriteGroup *group) const
{
	if (group == nullptr) return nullptr;

	return group->loading[0];
}

RoadStopResolverObject::RoadStopResolverObject(const RoadStopSpec *roadstopspec, BaseStation *st, TileIndex tile, RoadType roadtype, StationType type, uint8_t view,
		CallbackID callback, uint32_t param1, uint32_t param2)
	: ResolverObject(roadstopspec->grf_prop.grffile, callback, param1, param2), roadstop_scope(*this, st, roadstopspec, tile, roadtype, type, view)
	{

	this->town_scope = nullptr;

	CargoID ctype = CT_DEFAULT_NA;

	if (st == nullptr) {
		/* No station, so we are in a purchase list */
		ctype = CT_PURCHASE;
	} else if (Station::IsExpected(st)) {
		const Station *station = Station::From(st);
		/* Pick the first cargo that we have waiting */
		for (const CargoSpec *cs : CargoSpec::Iterate()) {
			if (roadstopspec->grf_prop.spritegroup[cs->Index()] != nullptr &&
					station->goods[cs->Index()].cargo.TotalCount() > 0) {
				ctype = cs->Index();
				break;
			}
		}
	}

	if (roadstopspec->grf_prop.spritegroup[ctype] == nullptr) {
		ctype = CT_DEFAULT;
	}

	/* Remember the cargo type we've picked */
	this->roadstop_scope.cargo_type = ctype;
	this->root_spritegroup = roadstopspec->grf_prop.spritegroup[ctype];
}

RoadStopResolverObject::~RoadStopResolverObject()
{
	delete this->town_scope;
}

TownScopeResolver *RoadStopResolverObject::GetTown()
{
	if (this->town_scope == nullptr) {
		Town *t;
		if (this->roadstop_scope.st != nullptr) {
			t = this->roadstop_scope.st->town;
		} else {
			t = ClosestTownFromTile(this->roadstop_scope.tile, UINT_MAX);
		}
		if (t == nullptr) return nullptr;
		this->town_scope = new TownScopeResolver(*this, t, this->roadstop_scope.st == nullptr);
	}
	return this->town_scope;
}

uint16_t GetRoadStopCallback(CallbackID callback, uint32_t param1, uint32_t param2, const RoadStopSpec *roadstopspec, BaseStation *st, TileIndex tile, RoadType roadtype, StationType type, uint8_t view)
{
	RoadStopResolverObject object(roadstopspec, st, tile, roadtype, type, view, callback, param1, param2);
	return object.ResolveCallback();
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
	const SpriteGroup *group = object.Resolve();
	if (group == nullptr || group->type != SGT_TILELAYOUT) return;
	const DrawTileSprites *dts = ((const TileLayoutSpriteGroup *)group)->ProcessRegisters(nullptr);

	PaletteID palette = COMPANY_SPRITE_COLOUR(_local_company);

	SpriteID image = dts->ground.sprite;
	PaletteID pal  = dts->ground.pal;

	if (GB(image, 0, SPRITE_WIDTH) != 0) {
		DrawSprite(image, GroundSpritePaletteTransform(image, pal, palette), x, y);
	}

	if (view >= 4) {
		/* Drive-through stop */
		uint sprite_offset = 5 - view;

		/* Road underlay takes precedence over tram */
		if (spec->draw_mode & ROADSTOP_DRAW_MODE_OVERLAY) {
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
		if ((spec->draw_mode & ROADSTOP_DRAW_MODE_ROAD) && rti->UsesOverlay()) {
			SpriteID ground = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_ROADSTOP);
			DrawSprite(ground + view, PAL_NONE, x, y);
		}
	}

	DrawCommonTileSeqInGUI(x, y, dts, 0, 0, palette, true);
}

/** Wrapper for animation control, see GetRoadStopCallback. */
uint16_t GetAnimRoadStopCallback(CallbackID callback, uint32_t param1, uint32_t param2, const RoadStopSpec *roadstopspec, BaseStation *st, TileIndex tile, int)
{
	return GetRoadStopCallback(callback, param1, param2, roadstopspec, st, tile, INVALID_ROADTYPE, GetStationType(tile), GetStationGfx(tile));
}

struct RoadStopAnimationFrameAnimationHelper {
	static byte Get(BaseStation *st, TileIndex tile) { return st->GetRoadStopAnimationFrame(tile); }
	static void Set(BaseStation *st, TileIndex tile, byte frame) { st->SetRoadStopAnimationFrame(tile, frame); }
};

/** Helper class for animation control. */
struct RoadStopAnimationBase : public AnimationBase<RoadStopAnimationBase, RoadStopSpec, BaseStation, int, GetAnimRoadStopCallback, RoadStopAnimationFrameAnimationHelper> {
	static const CallbackID cb_animation_speed      = CBID_STATION_ANIMATION_SPEED;
	static const CallbackID cb_animation_next_frame = CBID_STATION_ANIM_NEXT_FRAME;

	static const RoadStopCallbackMask cbm_animation_speed      = CBM_ROAD_STOP_ANIMATION_SPEED;
	static const RoadStopCallbackMask cbm_animation_next_frame = CBM_ROAD_STOP_ANIMATION_NEXT_FRAME;
};

void AnimateRoadStopTile(TileIndex tile)
{
	const RoadStopSpec *ss = GetRoadStopSpec(tile);
	if (ss == nullptr) return;

	RoadStopAnimationBase::AnimateTile(ss, BaseStation::GetByTile(tile), tile, HasBit(ss->flags, RSF_CB141_RANDOM_BITS));
}

void TriggerRoadStopAnimation(BaseStation *st, TileIndex trigger_tile, StationAnimationTrigger trigger, CargoID cargo_type)
{
	/* Get Station if it wasn't supplied */
	if (st == nullptr) st = BaseStation::GetByTile(trigger_tile);

	/* Check the cached animation trigger bitmask to see if we need
	 * to bother with any further processing. */
	if (!HasBit(st->cached_roadstop_anim_triggers, trigger)) return;

	uint16_t random_bits = Random();
	auto process_tile = [&](TileIndex cur_tile) {
		const RoadStopSpec *ss = GetRoadStopSpec(cur_tile);
		if (ss != nullptr && HasBit(ss->animation.triggers, trigger)) {
			CargoID cargo;
			if (!IsValidCargoID(cargo_type)) {
				cargo = CT_INVALID;
			} else {
				cargo = ss->grf_prop.grffile->cargo_map[cargo_type];
			}
			RoadStopAnimationBase::ChangeAnimationFrame(CBID_STATION_ANIM_START_STOP, ss, st, cur_tile, (random_bits << 16) | Random(), (uint8_t)trigger | (cargo << 8));
		}
	};

	if (trigger == SAT_NEW_CARGO || trigger == SAT_CARGO_TAKEN || trigger == SAT_250_TICKS) {
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
void TriggerRoadStopRandomisation(Station *st, TileIndex tile, RoadStopRandomTrigger trigger, CargoID cargo_type)
{
	if (st == nullptr) st = Station::GetByTile(tile);

	/* Check the cached cargo trigger bitmask to see if we need
	 * to bother with any further processing. */
	if (st->cached_roadstop_cargo_triggers == 0) return;
	if (IsValidCargoID(cargo_type) && !HasBit(st->cached_roadstop_cargo_triggers, cargo_type)) return;

	SetBit(st->waiting_triggers, trigger);

	uint32_t whole_reseed = 0;

	/* Bitmask of completely empty cargo types to be matched. */
	CargoTypes empty_mask = (trigger == RSRT_CARGO_TAKEN) ? GetEmptyMask(st) : 0;

	uint32_t used_triggers = 0;
	auto process_tile = [&](TileIndex cur_tile) {
		const RoadStopSpec *ss = GetRoadStopSpec(cur_tile);
		if (ss == nullptr) return;

		/* Cargo taken "will only be triggered if all of those
		 * cargo types have no more cargo waiting." */
		if (trigger == RSRT_CARGO_TAKEN) {
			if ((ss->cargo_triggers & ~empty_mask) != 0) return;
		}

		if (!IsValidCargoID(cargo_type) || HasBit(ss->cargo_triggers, cargo_type)) {
			RoadStopResolverObject object(ss, st, cur_tile, INVALID_ROADTYPE, GetStationType(cur_tile), GetStationGfx(cur_tile));
			object.waiting_triggers = st->waiting_triggers;

			const SpriteGroup *group = object.Resolve();
			if (group == nullptr) return;

			used_triggers |= object.used_triggers;

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
	if (trigger == RSRT_NEW_CARGO || trigger == RSRT_CARGO_TAKEN) {
		for (const RoadStopTileData &tile_data : st->custom_roadstop_tile_data) {
			process_tile(tile_data.tile);
		}
	} else {
		process_tile(tile);
	}

	/* Update whole station random bits */
	st->waiting_triggers &= ~used_triggers;
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
	if (!(RoadStopClass::GetClassCount() > 1 || RoadStopClass::Get(ROADSTOP_CLASS_DFLT)->GetSpecCount() > 1)) return false;
	for (uint i = 0; i < RoadStopClass::GetClassCount(); i++) {
		// We don't want to check the default or waypoint classes. These classes are always available.
		if (i == ROADSTOP_CLASS_DFLT || i == ROADSTOP_CLASS_WAYP) continue;
		RoadStopClass *roadstopclass = RoadStopClass::Get((RoadStopClassID)i);
		if (GetIfClassHasNewStopsByType(roadstopclass, rs, roadtype)) return true;
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
bool GetIfClassHasNewStopsByType(RoadStopClass *roadstopclass, RoadStopType rs, RoadType roadtype)
{
	for (uint j = 0; j < roadstopclass->GetSpecCount(); j++) {
		if (GetIfStopIsForType(roadstopclass->GetSpec(j), rs, roadtype)) return true;
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
	// The roadstopspec is nullptr, must be the default station, always return true.
	if (roadstopspec == nullptr) return true;

	if (HasBit(roadstopspec->flags, RSF_BUILD_MENU_ROAD_ONLY) && !RoadTypeIsRoad(roadtype)) return false;
	if (HasBit(roadstopspec->flags, RSF_BUILD_MENU_TRAM_ONLY) && !RoadTypeIsTram(roadtype)) return false;

	if (roadstopspec->stop_type == ROADSTOPTYPE_ALL) return true;

	switch (rs) {
		case ROADSTOP_BUS:
			if (roadstopspec->stop_type == ROADSTOPTYPE_PASSENGER) return true;
			break;

		case ROADSTOP_TRUCK:
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

int AllocateSpecToRoadStop(const RoadStopSpec *statspec, BaseStation *st, bool exec)
{
	uint i;

	if (statspec == nullptr || st == nullptr) return 0;

	/* Try to find the same spec and return that one */
	for (i = 1; i < st->roadstop_speclist.size() && i < NUM_ROADSTOPSPECS_PER_STATION; i++) {
		if (st->roadstop_speclist[i].spec == statspec) return i;
	}

	/* Try to find an unused spec slot */
	for (i = 1; i < st->roadstop_speclist.size() && i < NUM_ROADSTOPSPECS_PER_STATION; i++) {
		if (st->roadstop_speclist[i].spec == nullptr && st->roadstop_speclist[i].grfid == 0) break;
	}

	if (i == NUM_ROADSTOPSPECS_PER_STATION) {
		/* Full, give up */
		return -1;
	}

	if (exec) {
		if (i >= st->roadstop_speclist.size()) st->roadstop_speclist.resize(i + 1);
		st->roadstop_speclist[i].spec     = statspec;
		st->roadstop_speclist[i].grfid    = statspec->grf_prop.grffile->grfid;
		st->roadstop_speclist[i].localidx = statspec->grf_prop.local_id;

		RoadStopUpdateCachedTriggers(st);
	}

	return i;
}

void DeallocateSpecFromRoadStop(BaseStation *st, byte specindex)
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
			st->cached_roadstop_anim_triggers = 0;
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
	st->cached_roadstop_anim_triggers = 0;
	st->cached_roadstop_cargo_triggers = 0;

	/* Combine animation trigger bitmask for all road stop specs
	 * of this station. */
	for (uint i = 0; i < st->roadstop_speclist.size(); i++) {
		const RoadStopSpec *ss = st->roadstop_speclist[i].spec;
		if (ss != nullptr) {
			st->cached_roadstop_anim_triggers |= ss->animation.triggers;
			st->cached_roadstop_cargo_triggers |= ss->cargo_triggers;
		}
	}
}
