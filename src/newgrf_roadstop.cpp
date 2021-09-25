
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
#include "date_func.h"
#include "town.h"
#include "viewport_func.h"

#include "safeguards.h"

RoadStopOverrideManager _roadstop_mngr(1, NUM_ROADSTOPS, 0xFFFF);

RoadStopSpec _roadstop_specs[NUM_ROADSTOPS];

template <typename Tspec, typename Tid, Tid Tmax>
void NewGRFClass<Tspec, Tid, Tmax>::InsertDefaults()
{
	auto cls = RoadStopClass::Allocate('DFLT'); // Default bus class
	RoadStopClass::Get(cls)->name = STR_STATION_CLASS_DFLT;
	RoadStopClass::Get(cls)->Insert(nullptr);
	_roadstop_specs[0].cls_id = cls;
}

template <typename Tspec, typename Tid, Tid Tmax>
bool NewGRFClass<Tspec, Tid, Tmax>::IsUIAvailable(uint index) const
{
	return true;
}

INSTANTIATE_NEWGRF_CLASS_METHODS(RoadStopClass, RoadStopSpec, RoadStopClassID, ROADSTOP_CLASS_MAX)

static const uint NUM_ROADSTOPSPECS_PER_ROADSTOP = 255; ///< Maximum number of parts per station.

uint32 RoadStopScopeResolver::GetRandomBits() const
{
	return (this->st == nullptr ? 0 : this->st->random_bits) | (this->tile == INVALID_TILE ? 0 : GetStationTileRandomBits(this->tile) << 16);
}

uint32 RoadStopScopeResolver::GetTriggers() const
{
	return this->st == nullptr ? 0 : this->st->waiting_triggers; // TODO
}

uint32 RoadStopScopeResolver::GetVariable(byte variable, uint32 parameter, bool *available) const
{
	if (this->st == nullptr) {
		switch (variable) {
			case 0x40: return this->view;
			case 0x41: return 0;
			case 0x42: { // terrain_type
				if (this->tile == INVALID_TILE) return 0;
				else return GetTerrainType(this->tile);
			}

			case 0x43: return 0;
			case 0x44: return 0;

			case 0x45: { // town_zone
				const Town *t = ClosestTownFromTile(this->tile, UINT_MAX);
				return t != nullptr ? GetTownRadiusGroup(t, this->tile) : HZB_TOWN_EDGE;
			}

			default:
				goto unhandled;
		}
	}

	switch (variable) {
		case 0x40: return this->view; // view

		case 0x41: return 0; // roadtype_label

		case 0x42: return GetTerrainType(this->tile, TCX_NORMAL); // terrain_type

		case 0x43: return GetReverseRoadTypeTranslation(GetRoadTypeRoad(this->tile), this->roadstopspec->grf_prop.grffile); // road_type

		case 0x44: return GetReverseRoadTypeTranslation(GetRoadTypeTram(this->tile), this->roadstopspec->grf_prop.grffile); // tram_type

		case 0x45: { // town_zone
			const Town *t = ClosestTownFromTile(this->tile, UINT_MAX);
			return t != nullptr ? GetTownRadiusGroup(t, this->tile) : HZB_TOWN_EDGE;
		}

		case 0x46: return GetCompanyInfo(this->st->owner); // company_type

		case 0x47: return GetAnimationFrame(this->tile); // animation_frame
	}

	return this->st->GetNewGRFVariable(this->ro, variable, parameter, available);

unhandled:
	*available = false;
	return UINT_MAX;
}

const SpriteGroup *RoadStopResolverObject::ResolveReal(const RealSpriteGroup *group) const
{
	if (group == nullptr) return nullptr;

	return group->loading[0];
}

RoadStopResolverObject::RoadStopResolverObject(const RoadStopSpec *roadstopspec, BaseStation *st, TileIndex tile, const RoadTypeInfo *rti, uint8 view,
		CallbackID callback, uint32 param1, uint32 param2)
	: ResolverObject(roadstopspec->grf_prop.grffile, callback, param1, param2), roadstop_scope(*this, st, tile, rti, view)
	{

	this->town_scope = nullptr;
	this->root_spritegroup = (st == nullptr && roadstopspec->grf_prop.spritegroup[CT_DEFAULT] != nullptr)
		? roadstopspec->grf_prop.spritegroup[CT_DEFAULT] : roadstopspec->grf_prop.spritegroup[CT_DEFAULT];
}

RoadStopResolverObject::~RoadStopResolverObject()
{
	delete this->town_scope;
}

TownScopeResolver* RoadStopResolverObject::GetTown()
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

const RoadStopSpec *RoadStopSpec::Get(uint16 index)
{
	assert(index < NUM_ROADSTOPS);
	return &_roadstop_specs[index];
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
void DrawRoadStopTile(int x, int y, RoadType roadtype, const RoadStopSpec *spec, int view)
{
	assert(roadtype != INVALID_ROADTYPE);
	assert(spec != nullptr);

	const RoadTypeInfo *rti = GetRoadTypeInfo(roadtype);
	RoadStopResolverObject object(spec, nullptr, INVALID_TILE, rti, view);
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
		if (HasBit(spec->draw_mode, ROADSTOP_DRAW_MODE_OVERLAY)) {
			DrawRoadOverlays(INVALID_TILE, PAL_NONE, rti, rti, sprite_offset, sprite_offset);
		}

		if (rti->UsesOverlay()) {
			SpriteID ground = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_GROUND);
			DrawSprite(ground + sprite_offset, PAL_NONE, x, y);

			SpriteID overlay = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_OVERLAY);
			if (overlay) DrawSprite(overlay + sprite_offset, PAL_NONE, x, y);
		} else if (RoadTypeIsTram(roadtype)) {
			DrawSprite(SPR_TRAMWAY_TRAM + sprite_offset, PAL_NONE, x, y);
		}
	} else {
		/* Drive-in stop */
		if (rti->UsesOverlay()) {
			SpriteID ground = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_ROADSTOP);
			DrawSprite(ground + view, PAL_NONE, x, y);
		}
	}

	DrawCommonTileSeqInGUI(x, y, dts, 0, 0, palette, true);
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
	if (GetTileType(tile) != MP_STATION) return;
	if (st == nullptr) st = Station::GetByTile(tile);

	StationType station_type = GetStationType(tile);
	if (station_type != STATION_TRUCK && station_type != STATION_BUS) return;

	TileArea *roadstop_area = new TileArea(tile);
	st->GetTileArea(roadstop_area, station_type);

	SetBit(st->waiting_triggers, trigger);

	uint32 whole_reseed = 0;
	uint32 used_triggers = 0;
	for (TileIndex cur_tile : *roadstop_area) {
		if (st->TileBelongsToRoadStop(cur_tile)) {
			RoadType road_rt = GetRoadTypeRoad(cur_tile);
			RoadType tram_rt = GetRoadTypeTram(cur_tile);
			const RoadTypeInfo *road_rti = road_rt == INVALID_ROADTYPE ? nullptr : GetRoadTypeInfo(road_rt);
			const RoadTypeInfo *tram_rti = tram_rt == INVALID_ROADTYPE ? nullptr : GetRoadTypeInfo(tram_rt);

			RoadStopType stop_type = GetRoadStopType(cur_tile);
			RoadStop *road_stop = RoadStop::GetByTile(cur_tile, stop_type);
			/* We only need to trigger custom roadstops. */
			if (road_stop->roadstopspec != nullptr) {
				int dir = GetRoadStopDir(cur_tile);
				if (IsDriveThroughStopTile(cur_tile)) dir += 4;

				RoadStopResolverObject object(road_stop->roadstopspec, BaseStation::GetByTile(cur_tile), cur_tile, road_rti, dir);
				object.waiting_triggers = st->waiting_triggers;

				const SpriteGroup *group = object.Resolve();
				if (group == nullptr) continue;

				used_triggers |= object.used_triggers;

				uint32 reseed = object.GetReseedSum();
				if (reseed != 0) {
					whole_reseed |= reseed;
					reseed >>= 16;

					/* Set individual tile random bits */
					uint8 random_bits = GetStationTileRandomBits(cur_tile);
					random_bits &= ~reseed;
					random_bits |= Random() & reseed;
					SetStationTileRandomBits(cur_tile, random_bits);

					MarkTileDirtyByTile(cur_tile);
				}
			}
		}
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
 * @param rs the RoadStopType to check for.
 * @return true if there was any new RoadStopSpec's found for the given RoadStopType, else false.
 */
bool GetIfNewStopsByType(RoadStopType rs)
{
	if (!(RoadStopClass::GetClassCount() > 1 || RoadStopClass::Get(ROADSTOP_CLASS_DFLT)->GetSpecCount() > 1)) return false;
	for (uint i = 0; i < RoadStopClass::GetClassCount(); i++) {
		// We don't want to check the default class. That class is always available.
		if (i == ROADSTOP_CLASS_DFLT) continue;
		RoadStopClass *roadstopclass = RoadStopClass::Get((RoadStopClassID)i);
		if (GetIfClassHasNewStopsByType(roadstopclass, rs)) return true;
	}
	return false;
}

/**
 * Checks if the given RoadStopClass has any specs assigned to it, compatible with the given RoadStopType.
 * @param roadstopclass the RoadStopClass to check.
 * @param rs the RoadStopType to check by.
 * @return true if the roadstopclass has any specs compatible with the given RoadStopType.
 */
bool GetIfClassHasNewStopsByType(RoadStopClass *roadstopclass, RoadStopType rs)
{
	for (uint j = 0; j < roadstopclass->GetSpecCount(); j++) {
		if (GetIfStopIsForType(roadstopclass->GetSpec(j), rs)) return true;
	}
	return false;
}

/**
 * Checks if the given RoadStopSpec is compatible with the given RoadStopType.
 * @param roadstopspec the RoadStopSpec to check.
 * @param rs the RoadStopType to check by.
 * @return true if the roadstopspec is compatible with the given RoadStopType.
 */
bool GetIfStopIsForType(const RoadStopSpec *roadstopspec, RoadStopType rs)
{
	// The roadstopspec is nullptr, must be the default station, always return true.
	if (roadstopspec == nullptr) return true;
	// The stop is available for all types, no need to check, return true.
	if (roadstopspec->stop_type == ROADSTOPTYPE_ALL) return true;
	// We're going to switch by the given RoadStopType.
	// If the road stop we're checking is one of that type, return true.
	switch (rs) {
		case ROADSTOP_BUS:          if (roadstopspec->stop_type == ROADSTOPTYPE_PASSENGER) return true; break;
		case ROADSTOP_TRAM:         if (roadstopspec->stop_type == ROADSTOPTYPE_PASSENGER) return true; break;
		case ROADSTOP_TRUCK:        if (roadstopspec->stop_type == ROADSTOPTYPE_FREIGHT)   return true; break;
		case ROADSTOP_FREIGHT_TRAM: if (roadstopspec->stop_type == ROADSTOPTYPE_FREIGHT)   return true; break;
	}
	return false;
}
