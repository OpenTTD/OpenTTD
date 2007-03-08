/* $Id$ */

/** @file disaster_cmd.cpp
 * All disaster/easter egg vehicles are handled here.
 * The general flow of control for the disaster vehicles is as follows:
 * <ol>
 * <li>Initialize the disaster in a disaster specific way (eg start position,
 *     possible target, etc.) Disaster_XXX_Init() function
 * <li>Add a subtype to a disaster, which is an index into the function array
 *     that handles the vehicle's ticks.
 * <li>Run the disaster vehicles each tick until their target has been reached,
 *     this happens in the DisasterTick_XXX() functions. In here, a vehicle's
 *     state is kept by v->current_order.dest variable. Each achieved sub-target
 *     will increase this value, and the last one will remove the disaster itself
 * </ol>
 */


#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "industry_map.h"
#include "station_map.h"
#include "table/strings.h"
#include "map.h"
#include "tile.h"
#include "vehicle.h"
#include "command.h"
#include "news.h"
#include "station.h"
#include "waypoint.h"
#include "town.h"
#include "industry.h"
#include "player.h"
#include "airport.h"
#include "sound.h"
#include "variables.h"
#include "table/sprites.h"
#include "date.h"

enum DisasterSubType {
	ST_Zeppeliner,
	ST_Zeppeliner_Shadow,
	ST_Small_Ufo,
	ST_Small_Ufo_Shadow,
	ST_Airplane,
	ST_Airplane_Shadow,
	ST_Helicopter,
	ST_Helicopter_Shadow,
	ST_Helicopter_Rotors,
	ST_Big_Ufo,
	ST_Big_Ufo_Shadow,
	ST_Big_Ufo_Destroyer,
	ST_Big_Ufo_Destroyer_Shadow,
	ST_Small_Submarine,
	ST_Big_Submarine,
};

static void DisasterClearSquare(TileIndex tile)
{
	if (!EnsureNoVehicle(tile)) return;

	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			if (IsHumanPlayer(GetTileOwner(tile)) && !IsRailWaypoint(tile)) {
				PlayerID p = _current_player;
				_current_player = OWNER_WATER;
				DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
				_current_player = p;
			}
			break;

		case MP_HOUSE: {
			PlayerID p = _current_player;
			_current_player = OWNER_NONE;
			DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
			_current_player = p;
			break;
		}

		case MP_TREES:
		case MP_CLEAR:
			DoClearSquare(tile);
			break;

		default:
			break;
	}
}

static const SpriteID _disaster_images_1[] = {SPR_BLIMP, SPR_BLIMP, SPR_BLIMP, SPR_BLIMP, SPR_BLIMP, SPR_BLIMP, SPR_BLIMP, SPR_BLIMP};
static const SpriteID _disaster_images_2[] = {SPR_UFO_SMALL_SCOUT, SPR_UFO_SMALL_SCOUT, SPR_UFO_SMALL_SCOUT, SPR_UFO_SMALL_SCOUT, SPR_UFO_SMALL_SCOUT, SPR_UFO_SMALL_SCOUT, SPR_UFO_SMALL_SCOUT, SPR_UFO_SMALL_SCOUT};
static const SpriteID _disaster_images_3[] = {SPR_F_15, SPR_F_15, SPR_F_15, SPR_F_15, SPR_F_15, SPR_F_15, SPR_F_15, SPR_F_15};
static const SpriteID _disaster_images_4[] = {SPR_SUB_SMALL_NE, SPR_SUB_SMALL_NE, SPR_SUB_SMALL_SE, SPR_SUB_SMALL_SE, SPR_SUB_SMALL_SW, SPR_SUB_SMALL_SW, SPR_SUB_SMALL_NW, SPR_SUB_SMALL_NW};
static const SpriteID _disaster_images_5[] = {SPR_SUB_LARGE_NE, SPR_SUB_LARGE_NE, SPR_SUB_LARGE_SE, SPR_SUB_LARGE_SE, SPR_SUB_LARGE_SW, SPR_SUB_LARGE_SW, SPR_SUB_LARGE_NW, SPR_SUB_LARGE_NW};
static const SpriteID _disaster_images_6[] = {SPR_UFO_HARVESTER, SPR_UFO_HARVESTER, SPR_UFO_HARVESTER, SPR_UFO_HARVESTER, SPR_UFO_HARVESTER, SPR_UFO_HARVESTER, SPR_UFO_HARVESTER, SPR_UFO_HARVESTER};
static const SpriteID _disaster_images_7[] = {SPR_XCOM_SKYRANGER, SPR_XCOM_SKYRANGER, SPR_XCOM_SKYRANGER, SPR_XCOM_SKYRANGER, SPR_XCOM_SKYRANGER, SPR_XCOM_SKYRANGER, SPR_XCOM_SKYRANGER, SPR_XCOM_SKYRANGER};
static const SpriteID _disaster_images_8[] = {SPR_AH_64A, SPR_AH_64A, SPR_AH_64A, SPR_AH_64A, SPR_AH_64A, SPR_AH_64A, SPR_AH_64A, SPR_AH_64A};
static const SpriteID _disaster_images_9[] = {SPR_ROTOR_MOVING_1, SPR_ROTOR_MOVING_1, SPR_ROTOR_MOVING_1, SPR_ROTOR_MOVING_1, SPR_ROTOR_MOVING_1, SPR_ROTOR_MOVING_1, SPR_ROTOR_MOVING_1, SPR_ROTOR_MOVING_1};

static const SpriteID * const _disaster_images[] = {
	_disaster_images_1, _disaster_images_1,                     ///< zeppeliner and zeppeliner shadow
	_disaster_images_2, _disaster_images_2,                     ///< small ufo and small ufo shadow
	_disaster_images_3, _disaster_images_3,                     ///< combat aircraft and shadow
	_disaster_images_8, _disaster_images_8, _disaster_images_9, ///< combat helicopter, shadow and rotor
	_disaster_images_6, _disaster_images_6,                     ///< big ufo and shadow
	_disaster_images_7, _disaster_images_7,                     ///< skyranger and shadow
	_disaster_images_4, _disaster_images_5,                     ///< small and big submarine sprites
};

static void DisasterVehicleUpdateImage(Vehicle *v)
{
	SpriteID img = v->u.disaster.image_override;
	if (img == 0) img = _disaster_images[v->subtype][v->direction];
	v->cur_image = img;
}


/** Initialize a disaster vehicle. These vehicles are of type VEH_DISASTER, are unclickable
 * and owned by nobody */
static void InitializeDisasterVehicle(Vehicle *v, int x, int y, byte z, Direction direction, byte subtype)
{
	v->type = VEH_DISASTER;
	v->x_pos = x;
	v->y_pos = y;
	v->z_pos = z;
	v->tile = TileVirtXY(x, y);
	v->direction = direction;
	v->subtype = subtype;
	v->x_offs = -1;
	v->y_offs = -1;
	v->sprite_width = 2;
	v->sprite_height = 2;
	v->z_height = 5;
	v->owner = OWNER_NONE;
	v->vehstatus = VS_UNCLICKABLE;
	v->u.disaster.image_override = 0;
	v->current_order.Free();

	DisasterVehicleUpdateImage(v);
	VehiclePositionChanged(v);
	BeginVehicleMove(v);
	EndVehicleMove(v);
}

static void DeleteDisasterVeh(Vehicle *v)
{
	DeleteVehicleChain(v);
}

static void SetDisasterVehiclePos(Vehicle *v, int x, int y, byte z)
{
	Vehicle *u;

	BeginVehicleMove(v);
	v->x_pos = x;
	v->y_pos = y;
	v->z_pos = z;
	v->tile = TileVirtXY(x, y);

	DisasterVehicleUpdateImage(v);
	VehiclePositionChanged(v);
	EndVehicleMove(v);

	if ((u = v->next) != NULL) {
		int safe_x = clamp(x, 0, MapMaxX() * TILE_SIZE);
		int safe_y = clamp(y - 1, 0, MapMaxY() * TILE_SIZE);
		BeginVehicleMove(u);

		u->x_pos = x;
		u->y_pos = y - 1 - (max(z - GetSlopeZ(safe_x, safe_y), 0U) >> 3);
		safe_y = clamp(u->y_pos, 0, MapMaxY() * TILE_SIZE);
		u->z_pos = GetSlopeZ(safe_x, safe_y);
		u->direction = v->direction;

		DisasterVehicleUpdateImage(u);
		VehiclePositionChanged(u);
		EndVehicleMove(u);

		if ((u = u->next) != NULL) {
			BeginVehicleMove(u);
			u->x_pos = x;
			u->y_pos = y;
			u->z_pos = z + 5;
			VehiclePositionChanged(u);
			EndVehicleMove(u);
		}
	}
}

/**
 * Zeppeliner handling, v->current_order.dest states:
 * 0: Zeppeliner initialization has found a small airport, go there and crash
 * 1: Create crash and animate falling down for extra dramatic effect
 * 2: Create more smoke and leave debris on ground
 * 2: Clear the runway after some time and remove crashed zeppeliner
 * If not airport was found, only state 0 is reached until zeppeliner leaves map
 */
static void DisasterTick_Zeppeliner(Vehicle *v)
{
	Station *st;
	int x, y;
	byte z;
	TileIndex tile;

	v->tick_counter++;

	if (v->current_order.dest < 2) {
		if (HASBIT(v->tick_counter, 0)) return;

		GetNewVehiclePosResult gp = GetNewVehiclePos(v);

		SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);

		if (v->current_order.dest == 1) {
			if (++v->age == 38) {
				v->current_order.dest = 2;
				v->age = 0;
			}

			if (GB(v->tick_counter, 0, 3) == 0) CreateEffectVehicleRel(v, 0, -17, 2, EV_SMOKE);

		} else if (v->current_order.dest == 0) {
			tile = v->tile;

			if (IsValidTile(tile) &&
					IsTileType(tile, MP_STATION) &&
					IsAirport(tile) &&
					IsHumanPlayer(GetTileOwner(tile))) {
				v->current_order.dest = 1;
				v->age = 0;

				SetDParam(0, GetStationIndex(tile));
				AddNewsItem(STR_B000_ZEPPELIN_DISASTER_AT,
					NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ACCIDENT, 0),
					v->index,
					0);
			}
		}

		if (v->y_pos >= ((int)MapSizeY() + 9) * TILE_SIZE - 1) DeleteDisasterVeh(v);
		return;
	}

	if (v->current_order.dest > 2) {
		if (++v->age <= 13320) return;

		tile = v->tile;

		if (IsValidTile(tile) &&
				IsTileType(tile, MP_STATION) &&
				IsAirport(tile) &&
				IsHumanPlayer(GetTileOwner(tile))) {
			st = GetStationByTile(tile);
			CLRBITS(st->airport_flags, RUNWAY_IN_block);
		}

		SetDisasterVehiclePos(v, v->x_pos, v->y_pos, v->z_pos);
		DeleteDisasterVeh(v);
		return;
	}

	x = v->x_pos;
	y = v->y_pos;
	z = GetSlopeZ(x,y);
	if (z < v->z_pos) z = v->z_pos - 1;
	SetDisasterVehiclePos(v, x, y, z);

	if (++v->age == 1) {
		CreateEffectVehicleRel(v, 0, 7, 8, EV_EXPLOSION_LARGE);
		SndPlayVehicleFx(SND_12_EXPLOSION, v);
		v->u.disaster.image_override = SPR_BLIMP_CRASHING;
	} else if (v->age == 70) {
		v->u.disaster.image_override = SPR_BLIMP_CRASHED;
	} else if (v->age <= 300) {
		if (GB(v->tick_counter, 0, 3) == 0) {
			uint32 r = Random();

			CreateEffectVehicleRel(v,
				GB(r, 0, 4) - 7,
				GB(r, 4, 4) - 7,
				GB(r, 8, 3) + 5,
				EV_EXPLOSION_SMALL);
		}
	} else if (v->age == 350) {
		v->current_order.dest = 3;
		v->age = 0;
	}

	tile = v->tile;
	if (IsValidTile(tile) &&
			IsTileType(tile, MP_STATION) &&
			IsAirport(tile) &&
			IsHumanPlayer(GetTileOwner(tile))) {
		st = GetStationByTile(tile);
		SETBITS(st->airport_flags, RUNWAY_IN_block);
	}
}

/**
 * (Small) Ufo handling, v->current_order.dest states:
 * 0: Fly around to the middle of the map, then randomly, after a while target a road vehicle
 * 1: Home in on a road vehicle and crash it >:)
 * If not road vehicle was found, only state 0 is used and Ufo disappears after a while
 */
static void DisasterTick_Ufo(Vehicle *v)
{
	Vehicle *u;
	uint dist;
	byte z;

	v->u.disaster.image_override = (HASBIT(++v->tick_counter, 3)) ? SPR_UFO_SMALL_SCOUT_DARKER : SPR_UFO_SMALL_SCOUT;

	if (v->current_order.dest == 0) {
		/* Fly around randomly */
		int x = TileX(v->dest_tile) * TILE_SIZE;
		int y = TileY(v->dest_tile) * TILE_SIZE;
		if (delta(x, v->x_pos) + delta(y, v->y_pos) >= TILE_SIZE) {
			v->direction = GetDirectionTowards(v, x, y);
			GetNewVehiclePosResult gp = GetNewVehiclePos(v);
			SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);
			return;
		}
		if (++v->age < 6) {
			v->dest_tile = RandomTile();
			return;
		}
		v->current_order.dest = 1;

		FOR_ALL_VEHICLES(u) {
			if (u->type == VEH_ROAD && IsHumanPlayer(u->owner)) {
				v->dest_tile = u->index;
				v->age = 0;
				return;
			}
		}

		DeleteDisasterVeh(v);
	} else {
		/* Target a vehicle */
		u = GetVehicle(v->dest_tile);
		if (u->type != VEH_ROAD) {
			DeleteDisasterVeh(v);
			return;
		}

		dist = delta(v->x_pos, u->x_pos) + delta(v->y_pos, u->y_pos);

		if (dist < TILE_SIZE && !(u->vehstatus & VS_HIDDEN) && u->breakdown_ctr == 0) {
			u->breakdown_ctr = 3;
			u->breakdown_delay = 140;
		}

		v->direction = GetDirectionTowards(v, u->x_pos, u->y_pos);
		GetNewVehiclePosResult gp = GetNewVehiclePos(v);

		z = v->z_pos;
		if (dist <= TILE_SIZE && z > u->z_pos) z--;
		SetDisasterVehiclePos(v, gp.x, gp.y, z);

		if (z <= u->z_pos && (u->vehstatus & VS_HIDDEN)==0) {
			v->age++;
			if (u->u.road.crashed_ctr == 0) {
				u->u.road.crashed_ctr++;
				u->vehstatus |= VS_CRASHED;

				AddNewsItem(STR_B001_ROAD_VEHICLE_DESTROYED,
					NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ACCIDENT, 0),
					u->index,
					0);
			}
		}

		/* Destroy? */
		if (v->age > 50) {
			CreateEffectVehicleRel(v, 0, 7, 8, EV_EXPLOSION_LARGE);
			SndPlayVehicleFx(SND_12_EXPLOSION, v);
			DeleteDisasterVeh(v);
		}
	}
}

static void DestructIndustry(Industry *i)
{
	TileIndex tile;

	for (tile = 0; tile != MapSize(); tile++) {
		if (IsTileType(tile, MP_INDUSTRY) && GetIndustryIndex(tile) == i->index) {
			ResetIndustryConstructionStage(tile);
			MarkTileDirtyByTile(tile);
		}
	}
}

/**
 * Airplane handling, v->current_order.dest states:
 * 0: Fly towards the targetted oil refinery
 * 1: If within 15 tiles, fire away rockets and destroy industry
 * 2: Refinery explosions
 * 3: Fly out of the map
 * If the industry was removed in the meantime just fly to the end of the map
 */
static void DisasterTick_Airplane(Vehicle *v)
{
	v->tick_counter++;
	v->u.disaster.image_override =
		(v->current_order.dest == 1 && HASBIT(v->tick_counter, 2)) ? SPR_F_15_FIRING : 0;

	GetNewVehiclePosResult gp = GetNewVehiclePos(v);
	SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);

	if (gp.x < (-10 * TILE_SIZE)) {
		DeleteDisasterVeh(v);
		return;
	}

	if (v->current_order.dest == 2) {
		if (GB(v->tick_counter, 0, 2) == 0) {
			Industry *i = GetIndustry(v->dest_tile);
			int x = TileX(i->xy) * TILE_SIZE;
			int y = TileY(i->xy) * TILE_SIZE;
			uint32 r = Random();

			CreateEffectVehicleAbove(
				GB(r,  0, 6) + x,
				GB(r,  6, 6) + y,
				GB(r, 12, 4),
				EV_EXPLOSION_SMALL);

			if (++v->age >= 55) v->current_order.dest = 3;
		}
	} else if (v->current_order.dest == 1) {
		if (++v->age == 112) {
			Industry *i;

			v->current_order.dest = 2;
			v->age = 0;

			i = GetIndustry(v->dest_tile);
			DestructIndustry(i);

			SetDParam(0, i->town->index);
			AddNewsItem(STR_B002_OIL_REFINERY_EXPLOSION, NEWS_FLAGS(NM_THIN,NF_VIEWPORT|NF_TILE,NT_ACCIDENT,0), i->xy, 0);
			SndPlayTileFx(SND_12_EXPLOSION, i->xy);
		}
	} else if (v->current_order.dest == 0) {
		int x, y;
		TileIndex tile;
		uint ind;

		x = v->x_pos - (15 * TILE_SIZE);
		y = v->y_pos;

		if ( (uint)x > MapMaxX() * TILE_SIZE - 1) return;

		tile = TileVirtXY(x, y);
		if (!IsTileType(tile, MP_INDUSTRY)) return;

		ind = GetIndustryIndex(tile);
		v->dest_tile = ind;

		if (GetIndustry(ind)->type == IT_OIL_REFINERY) {
			v->current_order.dest = 1;
			v->age = 0;
		}
	}
}

/**
 * Helicopter handling, v->current_order.dest states:
 * 0: Fly towards the targetted factory
 * 1: If within 15 tiles, fire away rockets and destroy industry
 * 2: Factory explosions
 * 3: Fly out of the map
 */
static void DisasterTick_Helicopter(Vehicle *v)
{
	v->tick_counter++;
	v->u.disaster.image_override =
		(v->current_order.dest == 1 && HASBIT(v->tick_counter, 2)) ? SPR_AH_64A_FIRING : 0;

	GetNewVehiclePosResult gp = GetNewVehiclePos(v);
	SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);

	if (gp.x > (int)MapSizeX() * TILE_SIZE + 9 * TILE_SIZE - 1) {
		DeleteDisasterVeh(v);
		return;
	}

	if (v->current_order.dest == 2) {
		if (GB(v->tick_counter, 0, 2) == 0) {
			Industry *i = GetIndustry(v->dest_tile);
			int x = TileX(i->xy) * TILE_SIZE;
			int y = TileY(i->xy) * TILE_SIZE;
			uint32 r = Random();

			CreateEffectVehicleAbove(
				GB(r,  0, 6) + x,
				GB(r,  6, 6) + y,
				GB(r, 12, 4),
				EV_EXPLOSION_SMALL);

			if (++v->age >= 55) v->current_order.dest = 3;
		}
	} else if (v->current_order.dest == 1) {
		if (++v->age == 112) {
			Industry *i;

			v->current_order.dest = 2;
			v->age = 0;

			i = GetIndustry(v->dest_tile);
			DestructIndustry(i);

			SetDParam(0, i->town->index);
			AddNewsItem(STR_B003_FACTORY_DESTROYED_IN_SUSPICIOUS, NEWS_FLAGS(NM_THIN,NF_VIEWPORT|NF_TILE,NT_ACCIDENT,0), i->xy, 0);
			SndPlayTileFx(SND_12_EXPLOSION, i->xy);
		}
	} else if (v->current_order.dest == 0) {
		int x, y;
		TileIndex tile;
		uint ind;

		x = v->x_pos + (15 * TILE_SIZE);
		y = v->y_pos;

		if ( (uint)x > MapMaxX() * TILE_SIZE - 1) return;

		tile = TileVirtXY(x, y);
		if (!IsTileType(tile, MP_INDUSTRY)) return;

		ind = GetIndustryIndex(tile);
		v->dest_tile = ind;

		if (GetIndustry(ind)->type == IT_FACTORY) {
			v->current_order.dest = 1;
			v->age = 0;
		}
	}
}

/** Helicopter rotor blades; keep these spinning */
static void DisasterTick_Helicopter_Rotors(Vehicle *v)
{
	v->tick_counter++;
	if (HASBIT(v->tick_counter, 0)) return;

	if (++v->cur_image > SPR_ROTOR_MOVING_3) v->cur_image = SPR_ROTOR_MOVING_1;

	VehiclePositionChanged(v);
	BeginVehicleMove(v);
	EndVehicleMove(v);
}

/**
 * (Big) Ufo handling, v->current_order.dest states:
 * 0: Fly around to the middle of the map, then randomly for a while and home in on a piece of rail
 * 1: Land there and breakdown all trains in a radius of 12 tiles; and now we wait...
 *    because as soon as the Ufo lands, a fighter jet, a Skyranger, is called to clear up the mess
 */
static void DisasterTick_Big_Ufo(Vehicle *v)
{
	byte z;
	Vehicle *u, *w;
	Town *t;
	TileIndex tile;
	TileIndex tile_org;

	v->tick_counter++;

	if (v->current_order.dest == 1) {
		int x = TileX(v->dest_tile) * TILE_SIZE + TILE_SIZE / 2;
		int y = TileY(v->dest_tile) * TILE_SIZE + TILE_SIZE / 2;
		if (delta(v->x_pos, x) + delta(v->y_pos, y) >= 8) {
			v->direction = GetDirectionTowards(v, x, y);

			GetNewVehiclePosResult gp = GetNewVehiclePos(v);
			SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);
			return;
		}

		z = GetSlopeZ(v->x_pos, v->y_pos);
		if (z < v->z_pos) {
			SetDisasterVehiclePos(v, v->x_pos, v->y_pos, v->z_pos - 1);
			return;
		}

		v->current_order.dest = 2;

		FOR_ALL_VEHICLES(u) {
			if (u->type == VEH_TRAIN || u->type == VEH_ROAD) {
				if (delta(u->x_pos, v->x_pos) + delta(u->y_pos, v->y_pos) <= 12 * TILE_SIZE) {
					u->breakdown_ctr = 5;
					u->breakdown_delay = 0xF0;
				}
			}
		}

		t = ClosestTownFromTile(v->dest_tile, (uint)-1);
		SetDParam(0, t->index);
		AddNewsItem(STR_B004_UFO_LANDS_NEAR,
			NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_TILE, NT_ACCIDENT, 0),
			v->tile,
			0);

		u = ForceAllocateSpecialVehicle();
		if (u == NULL) {
			DeleteDisasterVeh(v);
			return;
		}

		InitializeDisasterVehicle(u, -6 * TILE_SIZE, v->y_pos, 135, DIR_SW, ST_Big_Ufo_Destroyer);
		u->u.disaster.unk2 = v->index;

		w = ForceAllocateSpecialVehicle();
		if (w == NULL) return;

		u->next = w;
		InitializeDisasterVehicle(w, -6 * TILE_SIZE, v->y_pos, 0, DIR_SW, ST_Big_Ufo_Destroyer_Shadow);
		w->vehstatus |= VS_SHADOW;
	} else if (v->current_order.dest == 0) {
		int x = TileX(v->dest_tile) * TILE_SIZE;
		int y = TileY(v->dest_tile) * TILE_SIZE;
		if (delta(x, v->x_pos) + delta(y, v->y_pos) >= TILE_SIZE) {
			v->direction = GetDirectionTowards(v, x, y);
			GetNewVehiclePosResult gp = GetNewVehiclePos(v);
			SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);
			return;
		}

		if (++v->age < 6) {
			v->dest_tile = RandomTile();
			return;
		}
		v->current_order.dest = 1;

		tile_org = tile = RandomTile();
		do {
			if (IsTileType(tile, MP_RAILWAY) &&
					IsPlainRailTile(tile) &&
					IsHumanPlayer(GetTileOwner(tile))) {
				break;
			}
			tile = TILE_MASK(tile + 1);
		} while (tile != tile_org);
		v->dest_tile = tile;
		v->age = 0;
	} else {
		return;
	}
}

/**
 * Skyranger destroying (Big) Ufo handling, v->current_order.dest states:
 * 0: Home in on landed Ufo and shoot it down
 */
static void DisasterTick_Big_Ufo_Destroyer(Vehicle *v)
{
	Vehicle *u;
	int i;

	v->tick_counter++;

	GetNewVehiclePosResult gp = GetNewVehiclePos(v);
	SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);

	if (gp.x > (int)MapSizeX() * TILE_SIZE + 9 * TILE_SIZE - 1) {
		DeleteDisasterVeh(v);
		return;
	}

	if (v->current_order.dest == 0) {
		u = GetVehicle(v->u.disaster.unk2);
		if (delta(v->x_pos, u->x_pos) > TILE_SIZE) return;
		v->current_order.dest = 1;

		CreateEffectVehicleRel(u, 0, 7, 8, EV_EXPLOSION_LARGE);
		SndPlayVehicleFx(SND_12_EXPLOSION, u);

		DeleteDisasterVeh(u);

		for (i = 0; i != 80; i++) {
			uint32 r = Random();
			CreateEffectVehicleAbove(
				GB(r, 0, 6) + v->x_pos - 32,
				GB(r, 5, 6) + v->y_pos - 32,
				0,
				EV_EXPLOSION_SMALL);
		}

		BEGIN_TILE_LOOP(tile, 6, 6, v->tile - TileDiffXY(3, 3))
			tile = TILE_MASK(tile);
			DisasterClearSquare(tile);
		END_TILE_LOOP(tile, 6, 6, v->tile - TileDiffXY(3, 3))
	}
}

/**
 * Submarine, v->current_order.dest states:
 * Unused, just float around aimlessly and pop up at different places, turning around
 */
static void DisasterTick_Submarine(Vehicle *v)
{
	TileIndex tile;

	v->tick_counter++;

	if (++v->age > 8880) {
		VehiclePositionChanged(v);
		BeginVehicleMove(v);
		EndVehicleMove(v);
		DeleteVehicle(v);
		return;
	}

	if (!HASBIT(v->tick_counter, 0)) return;

	tile = v->tile + TileOffsByDiagDir(DirToDiagDir(v->direction));
	if (IsValidTile(tile)) {
		TrackdirBits r = (TrackdirBits)GetTileTrackStatus(tile, TRANSPORT_WATER);

		if (TrackdirBitsToTrackBits(r) == TRACK_BIT_ALL && !CHANCE16(1, 90)) {
			GetNewVehiclePosResult gp = GetNewVehiclePos(v);
			SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);
			return;
		}
	}

	v->direction = ChangeDir(v->direction, GB(Random(), 0, 1) ? DIRDIFF_90RIGHT : DIRDIFF_90LEFT);
}


static void DisasterTick_NULL(Vehicle *v) {}
typedef void DisasterVehicleTickProc(Vehicle *v);

static DisasterVehicleTickProc * const _disastervehicle_tick_procs[] = {
	DisasterTick_Zeppeliner, DisasterTick_NULL,
	DisasterTick_Ufo,        DisasterTick_NULL,
	DisasterTick_Airplane,   DisasterTick_NULL,
	DisasterTick_Helicopter, DisasterTick_NULL, DisasterTick_Helicopter_Rotors,
	DisasterTick_Big_Ufo,    DisasterTick_NULL, DisasterTick_Big_Ufo_Destroyer,
	DisasterTick_NULL,
	DisasterTick_Submarine,
	DisasterTick_Submarine,
};


void DisasterVehicle_Tick(Vehicle *v)
{
	_disastervehicle_tick_procs[v->subtype](v);
}


void OnNewDay_DisasterVehicle(Vehicle *v)
{
	// not used
}

typedef void DisasterInitProc();


/** Zeppeliner which crashes on a small airport if one found,
 * otherwise crashes on a random tile */
static void Disaster_Zeppeliner_Init()
{
	Vehicle *v = ForceAllocateSpecialVehicle(), *u;
	Station *st;
	int x;

	if (v == NULL) return;

	/* Pick a random place, unless we find a small airport */
	x = TileX(Random()) * TILE_SIZE + TILE_SIZE / 2;

	FOR_ALL_STATIONS(st) {
		if (st->airport_tile != 0 &&
				st->airport_type <= 1 &&
				IsHumanPlayer(st->owner)) {
			x = (TileX(st->xy) + 2) * TILE_SIZE;
			break;
		}
	}

	InitializeDisasterVehicle(v, x, 0, 135, DIR_SE, ST_Zeppeliner);

	/* Allocate shadow too? */
	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u, x, 0, 0, DIR_SE, ST_Zeppeliner_Shadow);
		u->vehstatus |= VS_SHADOW;
	}
}


/** Ufo which flies around aimlessly from the middle of the map a bit
 * until it locates a road vehicle which it targets and then destroys */
static void Disaster_Small_Ufo_Init()
{
	Vehicle *v = ForceAllocateSpecialVehicle(), *u;
	int x;

	if (v == NULL) return;

	x = TileX(Random()) * TILE_SIZE + TILE_SIZE / 2;

	InitializeDisasterVehicle(v, x, 0, 135, DIR_SE, ST_Small_Ufo);
	v->dest_tile = TileXY(MapSizeX() / 2, MapSizeY() / 2);
	v->age = 0;

	/* Allocate shadow too? */
	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u, x, 0, 0, DIR_SE, ST_Small_Ufo_Shadow);
		u->vehstatus |= VS_SHADOW;
	}
}


/* Combat airplane which destroys an oil refinery */
static void Disaster_Airplane_Init()
{
	Industry *i, *found;
	Vehicle *v, *u;
	int x, y;

	found = NULL;

	FOR_ALL_INDUSTRIES(i) {
		if (i->type == IT_OIL_REFINERY &&
				(found == NULL || CHANCE16(1, 2))) {
			found = i;
		}
	}

	if (found == NULL) return;

	v = ForceAllocateSpecialVehicle();
	if (v == NULL) return;

	/* Start from the bottom (south side) of the map */
	x = (MapSizeX() + 9) * TILE_SIZE - 1;
	y = TileY(found->xy) * TILE_SIZE + 37;

	InitializeDisasterVehicle(v, x, y, 135, DIR_NE, ST_Airplane);

	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u, x, y, 0, DIR_SE, ST_Airplane_Shadow);
		u->vehstatus |= VS_SHADOW;
	}
}


/** Combat helicopter that destroys a factory */
static void Disaster_Helicopter_Init()
{
	Industry *i, *found;
	Vehicle *v, *u, *w;
	int x, y;

	found = NULL;

	FOR_ALL_INDUSTRIES(i) {
		if (i->type == IT_FACTORY &&
				(found == NULL || CHANCE16(1, 2))) {
			found = i;
		}
	}

	if (found == NULL) return;

	v = ForceAllocateSpecialVehicle();
	if (v == NULL) return;

	x = -16 * TILE_SIZE;
	y = TileY(found->xy) * TILE_SIZE + 37;

	InitializeDisasterVehicle(v, x, y, 135, DIR_SW, ST_Helicopter);

	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u, x, y, 0, DIR_SW, ST_Helicopter_Shadow);
		u->vehstatus |= VS_SHADOW;

		w = ForceAllocateSpecialVehicle();
		if (w != NULL) {
			u->next = w;
			InitializeDisasterVehicle(w, x, y, 140, DIR_SW, ST_Helicopter_Rotors);
		}
	}
}


/* Big Ufo which lands on a piece of rail and will consequently be shot
 * down by a combat airplane, destroying the surroundings */
static void Disaster_Big_Ufo_Init()
{
	Vehicle *v = ForceAllocateSpecialVehicle(), *u;
	int x, y;

	if (v == NULL) return;

	x = TileX(Random()) * TILE_SIZE + TILE_SIZE / 2;

	y = MapMaxX() * TILE_SIZE - 1;
	InitializeDisasterVehicle(v, x, y, 135, DIR_NW, ST_Big_Ufo);
	v->dest_tile = TileXY(MapSizeX() / 2, MapSizeY() / 2);
	v->age = 0;

	/* Allocate shadow too? */
	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u, x, y, 0, DIR_NW, ST_Big_Ufo_Shadow);
		u->vehstatus |= VS_SHADOW;
	}
}


/* Curious submarine #1, just floats around */
static void Disaster_Small_Submarine_Init()
{
	Vehicle *v = ForceAllocateSpecialVehicle();
	int x, y;
	Direction dir;
	uint32 r;

	if (v == NULL) return;

	r = Random();
	x = TileX(r) * TILE_SIZE + TILE_SIZE / 2;

	if (HASBIT(r, 31)) {
		y = MapMaxX() * TILE_SIZE - TILE_SIZE / 2 - 1;
		dir = DIR_NW;
	} else {
		y = TILE_SIZE / 2;
		dir = DIR_SE;
	}
	InitializeDisasterVehicle(v, x, y, 0, dir, ST_Small_Submarine);
	v->age = 0;
}


/* Curious submarine #2, just floats around */
static void Disaster_Big_Submarine_Init()
{
	Vehicle *v = ForceAllocateSpecialVehicle();
	int x,y;
	Direction dir;
	uint32 r;

	if (v == NULL) return;

	r = Random();
	x = TileX(r) * TILE_SIZE + TILE_SIZE / 2;

	if (HASBIT(r, 31)) {
		y = MapMaxX() * TILE_SIZE - TILE_SIZE / 2 - 1;
		dir = DIR_NW;
	} else {
		y = TILE_SIZE / 2;
		dir = DIR_SE;
	}
	InitializeDisasterVehicle(v, x, y, 0, dir, ST_Big_Submarine);
	v->age = 0;
}


/** Coal mine catastrophe, destroys a stretch of 30 tiles of
 * land in a certain direction */
static void Disaster_CoalMine_Init()
{
	int index = GB(Random(), 0, 4);
	uint m;

	for (m = 0; m < 15; m++) {
		const Industry *i;

		FOR_ALL_INDUSTRIES(i) {
			if (i->type == IT_COAL_MINE && --index < 0) {
				SetDParam(0, i->town->index);
				AddNewsItem(STR_B005_COAL_MINE_SUBSIDENCE_LEAVES,
					NEWS_FLAGS(NM_THIN,NF_VIEWPORT|NF_TILE,NT_ACCIDENT,0), i->xy + TileDiffXY(1, 1), 0);

				{
					TileIndex tile = i->xy;
					TileIndexDiff step = TileOffsByDiagDir(GB(Random(), 0, 2));
					uint n;

					for (n = 0; n < 30; n++) {
						DisasterClearSquare(tile);
						tile = TILE_MASK(tile + step);
					}
				}
				return;
			}
		}
	}
}

static DisasterInitProc * const _disaster_initprocs[] = {
	Disaster_Zeppeliner_Init,
	Disaster_Small_Ufo_Init,
	Disaster_Airplane_Init,
	Disaster_Helicopter_Init,
	Disaster_Big_Ufo_Init,
	Disaster_Small_Submarine_Init,
	Disaster_Big_Submarine_Init,
	Disaster_CoalMine_Init,
};

static const struct {
	Year min;
	Year max;
} _dis_years[] = {
	{ 1930, 1955 }, ///< zeppeliner
	{ 1940, 1970 }, ///< ufo (small)
	{ 1960, 1990 }, ///< airplane
	{ 1970, 2000 }, ///< helicopter
	{ 2000, 2100 }, ///< ufo (big)
	{ 1940, 1965 }, ///< submarine (small)
	{ 1975, 2010 }, ///< submarine (big)
	{ 1950, 1985 }  ///< coalmine
};


static void DoDisaster()
{
	byte buf[lengthof(_dis_years)];
	uint i;
	uint j;

	j = 0;
	for (i = 0; i != lengthof(_dis_years); i++) {
		if (_cur_year >= _dis_years[i].min && _cur_year < _dis_years[i].max) buf[j++] = i;
	}

	if (j == 0) return;

	_disaster_initprocs[buf[RandomRange(j)]]();
}


static void ResetDisasterDelay()
{
	_disaster_delay = GB(Random(), 0, 9) + 730;
}

void DisasterDailyLoop()
{
	if (--_disaster_delay != 0) return;

	ResetDisasterDelay();

	if (_opt.diff.disasters != 0) DoDisaster();
}

void StartupDisasters()
{
	ResetDisasterDelay();
}
