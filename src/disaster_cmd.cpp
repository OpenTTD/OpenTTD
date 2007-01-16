/* $Id$ */

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

static const SpriteID _disaster_images_1[] = {0xF41, 0xF41, 0xF41, 0xF41, 0xF41, 0xF41, 0xF41, 0xF41};
static const SpriteID _disaster_images_2[] = {0xF44, 0xF44, 0xF44, 0xF44, 0xF44, 0xF44, 0xF44, 0xF44};
static const SpriteID _disaster_images_3[] = {0xF4E, 0xF4E, 0xF4E, 0xF4E, 0xF4E, 0xF4E, 0xF4E, 0xF4E};
static const SpriteID _disaster_images_4[] = {0xF46, 0xF46, 0xF47, 0xF47, 0xF48, 0xF48, 0xF49, 0xF49};
static const SpriteID _disaster_images_5[] = {0xF4A, 0xF4A, 0xF4B, 0xF4B, 0xF4C, 0xF4C, 0xF4D, 0xF4D};
static const SpriteID _disaster_images_6[] = {0xF50, 0xF50, 0xF50, 0xF50, 0xF50, 0xF50, 0xF50, 0xF50};
static const SpriteID _disaster_images_7[] = {0xF51, 0xF51, 0xF51, 0xF51, 0xF51, 0xF51, 0xF51, 0xF51};
static const SpriteID _disaster_images_8[] = {0xF52, 0xF52, 0xF52, 0xF52, 0xF52, 0xF52, 0xF52, 0xF52};
static const SpriteID _disaster_images_9[] = {0xF3E, 0xF3E, 0xF3E, 0xF3E, 0xF3E, 0xF3E, 0xF3E, 0xF3E};

static const SpriteID * const _disaster_images[] = {
	_disaster_images_1, _disaster_images_1,
	_disaster_images_2, _disaster_images_2,
	_disaster_images_3, _disaster_images_3,
	_disaster_images_8, _disaster_images_8, _disaster_images_9,
	_disaster_images_6, _disaster_images_6,
	_disaster_images_7, _disaster_images_7,
	_disaster_images_4, _disaster_images_5,
};

static void DisasterVehicleUpdateImage(Vehicle *v)
{
	int img = v->u.disaster.image_override;
	if (img == 0)
		img = _disaster_images[v->subtype][v->direction];
	v->cur_image = img;
}

static void InitializeDisasterVehicle(Vehicle* v, int x, int y, byte z, Direction direction, byte subtype)
{
	v->type = VEH_Disaster;
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
	v->current_order.type = OT_NOTHING;
	v->current_order.flags = 0;
	v->current_order.dest = 0;

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

	if ( (u=v->next) != NULL) {
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

		if ( (u=u->next) != NULL) {
			BeginVehicleMove(u);
			u->x_pos = x;
			u->y_pos = y;
			u->z_pos = z + 5;
			VehiclePositionChanged(u);
			EndVehicleMove(u);
		}
	}
}


static void DisasterTick_Zeppeliner(Vehicle *v)
{
	GetNewVehiclePosResult gp;
	Station *st;
	int x,y;
	byte z;
	TileIndex tile;

	++v->tick_counter;

	if (v->current_order.dest < 2) {
		if (v->tick_counter&1)
			return;

		GetNewVehiclePos(v, &gp);

		SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);

		if (v->current_order.dest == 1) {
			if (++v->age == 38) {
				v->current_order.dest = 2;
				v->age = 0;
			}

			if ((v->tick_counter&7)==0) {
				CreateEffectVehicleRel(v, 0, -17, 2, EV_SMOKE);
			}
		} else if (v->current_order.dest == 0) {
			tile = v->tile; /**/

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
		if (v->y_pos >= ((int)MapSizeY() + 9) * TILE_SIZE - 1)
			DeleteDisasterVeh(v);
		return;
	}

	if (v->current_order.dest > 2) {
		if (++v->age <= 13320)
			return;

		tile = v->tile; /**/

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
	if (z < v->z_pos)
		z = v->z_pos - 1;
	SetDisasterVehiclePos(v, x, y, z);

	if (++v->age == 1) {
		CreateEffectVehicleRel(v, 0, 7, 8, EV_EXPLOSION_LARGE);
		SndPlayVehicleFx(SND_12_EXPLOSION, v);
		v->u.disaster.image_override = SPR_BLIMP_CRASHING;
	} else if (v->age == 70) {
		v->u.disaster.image_override = SPR_BLIMP_CRASHED;
	} else if (v->age <= 300) {
		if (!(v->tick_counter&7)) {
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

	tile = v->tile;/**/
	if (IsValidTile(tile) &&
			IsTileType(tile, MP_STATION) &&
			IsAirport(tile) &&
			IsHumanPlayer(GetTileOwner(tile))) {
		st = GetStationByTile(tile);
		SETBITS(st->airport_flags, RUNWAY_IN_block);
	}
}

// UFO starts in the middle, and flies around a bit until it locates
// a road vehicle which it targets.
static void DisasterTick_UFO(Vehicle *v)
{
	GetNewVehiclePosResult gp;
	Vehicle *u;
	uint dist;
	byte z;

	v->u.disaster.image_override = (++v->tick_counter & 8) ? SPR_UFO_SMALL_SCOUT_DARKER : SPR_UFO_SMALL_SCOUT;

	if (v->current_order.dest == 0) {
// fly around randomly
		int x = TileX(v->dest_tile) * TILE_SIZE;
		int y = TileY(v->dest_tile) * TILE_SIZE;
		if (delta(x, v->x_pos) + delta(y, v->y_pos) >= TILE_SIZE) {
			v->direction = GetDirectionTowards(v, x, y);
			GetNewVehiclePos(v, &gp);
			SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);
			return;
		}
		if (++v->age < 6) {
			v->dest_tile = RandomTile();
			return;
		}
		v->current_order.dest = 1;

		FOR_ALL_VEHICLES(u) {
			if (u->type == VEH_Road && IsHumanPlayer(u->owner)) {
				v->dest_tile = u->index;
				v->age = 0;
				return;
			}
		}

		DeleteDisasterVeh(v);
	} else {
// target a vehicle
		u = GetVehicle(v->dest_tile);
		if (u->type != VEH_Road) {
			DeleteDisasterVeh(v);
			return;
		}

		dist = delta(v->x_pos, u->x_pos) + delta(v->y_pos, u->y_pos);

		if (dist < TILE_SIZE && !(u->vehstatus&VS_HIDDEN) && u->breakdown_ctr==0) {
			u->breakdown_ctr = 3;
			u->breakdown_delay = 140;
		}

		v->direction = GetDirectionTowards(v, u->x_pos, u->y_pos);
		GetNewVehiclePos(v, &gp);

		z = v->z_pos;
		if (dist <= TILE_SIZE && z > u->z_pos) z--;
		SetDisasterVehiclePos(v, gp.x, gp.y, z);

		if (z <= u->z_pos && (u->vehstatus&VS_HIDDEN)==0) {
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

// destroy?
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

// Airplane which destroys an oil refinery
static void DisasterTick_2(Vehicle *v)
{
	GetNewVehiclePosResult gp;

	v->tick_counter++;
	v->u.disaster.image_override =
		(v->current_order.dest == 1 && v->tick_counter & 4) ? SPR_F_15_FIRING : 0;

	GetNewVehiclePos(v, &gp);
	SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);

	if (gp.x < -160) {
		DeleteDisasterVeh(v);
		return;
	}

	if (v->current_order.dest == 2) {
		if (!(v->tick_counter&3)) {
			Industry *i = GetIndustry(v->dest_tile);
			int x = TileX(i->xy) * TILE_SIZE;
			int y = TileY(i->xy) * TILE_SIZE;
			uint32 r = Random();

			CreateEffectVehicleAbove(
				GB(r,  0, 6) + x,
				GB(r,  6, 6) + y,
				GB(r, 12, 4),
				EV_EXPLOSION_SMALL);

			if (++v->age >= 55)
				v->current_order.dest = 3;
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
		int x,y;
		TileIndex tile;
		uint ind;

		x = v->x_pos - 15 * TILE_SIZE;
		y = v->y_pos;

		if ( (uint)x > MapMaxX() * TILE_SIZE - 1)
			return;

		tile = TileVirtXY(x, y);
		if (!IsTileType(tile, MP_INDUSTRY))
			return;

		ind = GetIndustryIndex(tile);
		v->dest_tile = ind;

		if (GetIndustry(ind)->type == IT_OIL_REFINERY) {
			v->current_order.dest = 1;
			v->age = 0;
		}
	}
}

// Helicopter which destroys a factory
static void DisasterTick_3(Vehicle *v)
{
	GetNewVehiclePosResult gp;

	v->tick_counter++;
	v->u.disaster.image_override =
		(v->current_order.dest == 1 && v->tick_counter & 4) ? SPR_AH_64A_FIRING : 0;

	GetNewVehiclePos(v, &gp);
	SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);

	if (gp.x > (int)MapSizeX() * TILE_SIZE + 9 * TILE_SIZE - 1) {
		DeleteDisasterVeh(v);
		return;
	}

	if (v->current_order.dest == 2) {
		if (!(v->tick_counter&3)) {
			Industry *i = GetIndustry(v->dest_tile);
			int x = TileX(i->xy) * TILE_SIZE;
			int y = TileY(i->xy) * TILE_SIZE;
			uint32 r = Random();

			CreateEffectVehicleAbove(
				GB(r,  0, 6) + x,
				GB(r,  6, 6) + y,
				GB(r, 12, 4),
				EV_EXPLOSION_SMALL);

			if (++v->age >= 55)
				v->current_order.dest = 3;
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
		int x,y;
		TileIndex tile;
		uint ind;

		x = v->x_pos + (15 * TILE_SIZE);
		y = v->y_pos;

		if ( (uint)x > MapMaxX() * TILE_SIZE - 1)
			return;

		tile = TileVirtXY(x, y);
		if (!IsTileType(tile, MP_INDUSTRY))
			return;

		ind = GetIndustryIndex(tile);
		v->dest_tile = ind;

		if (GetIndustry(ind)->type == IT_FACTORY) {
			v->current_order.dest = 1;
			v->age = 0;
		}
	}
}

// Helicopter rotor blades
static void DisasterTick_3b(Vehicle *v)
{
	if (++v->tick_counter & 1)
		return;

	if (++v->cur_image > SPR_ROTOR_MOVING_3) v->cur_image = SPR_ROTOR_MOVING_1;

	VehiclePositionChanged(v);
	BeginVehicleMove(v);
	EndVehicleMove(v);
}

// Big UFO which lands on a piece of rail.
// Will be shot down by a plane
static void DisasterTick_4(Vehicle *v)
{
	GetNewVehiclePosResult gp;
	byte z;
	Vehicle *u,*w;
	Town *t;
	TileIndex tile;
	TileIndex tile_org;

	v->tick_counter++;

	if (v->current_order.dest == 1) {
		int x = TileX(v->dest_tile) * TILE_SIZE + TILE_SIZE / 2;
		int y = TileY(v->dest_tile) * TILE_SIZE + TILE_SIZE / 2;
		if (delta(v->x_pos, x) + delta(v->y_pos, y) >= 8) {
			v->direction = GetDirectionTowards(v, x, y);

			GetNewVehiclePos(v, &gp);
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
			if (u->type == VEH_Train || u->type == VEH_Road) {
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

		InitializeDisasterVehicle(u, -6 * TILE_SIZE, v->y_pos, 135, DIR_SW, 11);
		u->u.disaster.unk2 = v->index;

		w = ForceAllocateSpecialVehicle();
		if (w == NULL)
			return;

		u->next = w;
		InitializeDisasterVehicle(w, -6 * TILE_SIZE, v->y_pos, 0, DIR_SW, 12);
		w->vehstatus |= VS_SHADOW;
	} else if (v->current_order.dest < 1) {

		int x = TileX(v->dest_tile) * TILE_SIZE;
		int y = TileY(v->dest_tile) * TILE_SIZE;
		if (delta(x, v->x_pos) + delta(y, v->y_pos) >= TILE_SIZE) {
			v->direction = GetDirectionTowards(v, x, y);
			GetNewVehiclePos(v, &gp);
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
			tile = TILE_MASK(tile+1);
		} while (tile != tile_org);
		v->dest_tile = tile;
		v->age = 0;
	} else {
		return;
	}
}

// The plane which will shoot down the UFO
static void DisasterTick_4b(Vehicle *v)
{
	GetNewVehiclePosResult gp;
	Vehicle *u;
	int i;

	v->tick_counter++;

	GetNewVehiclePos(v, &gp);
	SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);

	if (gp.x > (int)MapSizeX() * TILE_SIZE + 9 * TILE_SIZE - 1) {
		DeleteDisasterVeh(v);
		return;
	}

	if (v->current_order.dest == 0) {
		u = GetVehicle(v->u.disaster.unk2);
		if (delta(v->x_pos, u->x_pos) > TILE_SIZE)
			return;
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

// Submarine handler
static void DisasterTick_5_and_6(Vehicle *v)
{
	uint32 r;
	GetNewVehiclePosResult gp;
	TileIndex tile;

	v->tick_counter++;

	if (++v->age > 8880) {
		VehiclePositionChanged(v);
		BeginVehicleMove(v);
		EndVehicleMove(v);
		DeleteVehicle(v);
		return;
	}

	if (!(v->tick_counter & 1)) return;

	tile = v->tile + TileOffsByDiagDir(DirToDiagDir(v->direction));
	if (IsValidTile(tile) &&
			(r=GetTileTrackStatus(tile,TRANSPORT_WATER),(byte)(r|(r >> 8)) == 0x3F) &&
			!CHANCE16(1,90)) {
		GetNewVehiclePos(v, &gp);
		SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);
		return;
	}

	v->direction = ChangeDir(v->direction, GB(Random(), 0, 1) ? DIRDIFF_90RIGHT : DIRDIFF_90LEFT);
}


static void DisasterTick_NULL(Vehicle *v) {}
typedef void DisasterVehicleTickProc(Vehicle *v);

static DisasterVehicleTickProc * const _disastervehicle_tick_procs[] = {
	DisasterTick_Zeppeliner,DisasterTick_NULL,
	DisasterTick_UFO,DisasterTick_NULL,
	DisasterTick_2,DisasterTick_NULL,
	DisasterTick_3,DisasterTick_NULL,DisasterTick_3b,
	DisasterTick_4,DisasterTick_NULL,
	DisasterTick_4b,DisasterTick_NULL,
	DisasterTick_5_and_6,
	DisasterTick_5_and_6,
};


void DisasterVehicle_Tick(Vehicle *v)
{
	_disastervehicle_tick_procs[v->subtype](v);
}


void OnNewDay_DisasterVehicle(Vehicle *v)
{
	// not used
}

typedef void DisasterInitProc(void);

// Zeppeliner which crashes on a small airport
static void Disaster0_Init(void)
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

	InitializeDisasterVehicle(v, x, 0, 135, DIR_SE, 0);

	// Allocate shadow too?
	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u, x, 0, 0, DIR_SE, 1);
		u->vehstatus |= VS_SHADOW;
	}
}

static void Disaster1_Init(void)
{
	Vehicle *v = ForceAllocateSpecialVehicle(), *u;
	int x;

	if (v == NULL) return;

	x = TileX(Random()) * TILE_SIZE + TILE_SIZE / 2;

	InitializeDisasterVehicle(v, x, 0, 135, DIR_SE, 2);
	v->dest_tile = TileXY(MapSizeX() / 2, MapSizeY() / 2);
	v->age = 0;

	// Allocate shadow too?
	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u, x, 0, 0, DIR_SE, 3);
		u->vehstatus |= VS_SHADOW;
	}
}

static void Disaster2_Init(void)
{
	Industry *i, *found;
	Vehicle *v,*u;
	int x,y;

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

	x = (MapSizeX() + 9) * TILE_SIZE - 1;
	y = TileY(found->xy) * TILE_SIZE + 37;

	InitializeDisasterVehicle(v, x, y, 135, DIR_NE, 4);

	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u, x, y, 0, DIR_SE, 5);
		u->vehstatus |= VS_SHADOW;
	}
}

static void Disaster3_Init(void)
{
	Industry *i, *found;
	Vehicle *v,*u,*w;
	int x,y;

	found = NULL;

	FOR_ALL_INDUSTRIES(i) {
		if (i->type == IT_FACTORY &&
				(found==NULL || CHANCE16(1,2))) {
			found = i;
		}
	}

	if (found == NULL) return;

	v = ForceAllocateSpecialVehicle();
	if (v == NULL) return;

	x = -16 * TILE_SIZE;
	y = TileY(found->xy) * TILE_SIZE + 37;

	InitializeDisasterVehicle(v, x, y, 135, DIR_SW, 6);

	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u, x, y, 0, DIR_SW, 7);
		u->vehstatus |= VS_SHADOW;

		w = ForceAllocateSpecialVehicle();
		if (w != NULL) {
			u->next = w;
			InitializeDisasterVehicle(w, x, y, 140, DIR_SW, 8);
		}
	}
}

static void Disaster4_Init(void)
{
	Vehicle *v = ForceAllocateSpecialVehicle(), *u;
	int x,y;

	if (v == NULL) return;

	x = TileX(Random()) * TILE_SIZE + TILE_SIZE / 2;

	y = MapMaxX() * TILE_SIZE - 1;
	InitializeDisasterVehicle(v, x, y, 135, DIR_NW, 9);
	v->dest_tile = TileXY(MapSizeX() / 2, MapSizeY() / 2);
	v->age = 0;

	// Allocate shadow too?
	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u, x, y, 0, DIR_NW, 10);
		u->vehstatus |= VS_SHADOW;
	}
}

// Submarine type 1
static void Disaster5_Init(void)
{
	Vehicle *v = ForceAllocateSpecialVehicle();
	int x,y;
	Direction dir;
	uint32 r;

	if (v == NULL) return;

	r = Random();
	x = TileX(r) * TILE_SIZE + TILE_SIZE / 2;

	if (r & 0x80000000) {
		y = MapMaxX() * TILE_SIZE - TILE_SIZE / 2 - 1;
		dir = DIR_NW;
	} else {
		y = TILE_SIZE / 2;
		dir = DIR_SE;
	}
	InitializeDisasterVehicle(v, x, y, 0, dir, 13);
	v->age = 0;
}

// Submarine type 2
static void Disaster6_Init(void)
{
	Vehicle *v = ForceAllocateSpecialVehicle();
	int x,y;
	Direction dir;
	uint32 r;

	if (v == NULL) return;

	r = Random();
	x = TileX(r) * TILE_SIZE + TILE_SIZE / 2;

	if (r & 0x80000000) {
		y = MapMaxX() * TILE_SIZE - TILE_SIZE / 2 - 1;
		dir = DIR_NW;
	} else {
		y = TILE_SIZE / 2;
		dir = DIR_SE;
	}
	InitializeDisasterVehicle(v, x, y, 0, dir, 14);
	v->age = 0;
}

static void Disaster7_Init(void)
{
	int index = GB(Random(), 0, 4);
	uint m;

	for (m = 0; m < 15; m++) {
		const Industry* i;

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
	Disaster0_Init,
	Disaster1_Init,
	Disaster2_Init,
	Disaster3_Init,
	Disaster4_Init,
	Disaster5_Init,
	Disaster6_Init,
	Disaster7_Init,
};

static const struct {
	Year min;
	Year max;
} _dis_years[] = {
	{ 1930, 1955 },
	{ 1940, 1970 },
	{ 1960, 1990 },
	{ 1970, 2000 },
	{ 2000, 2100 },
	{ 1940, 1965 },
	{ 1975, 2010 },
	{ 1950, 1985 }
};


static void DoDisaster(void)
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


static void ResetDisasterDelay(void)
{
	_disaster_delay = GB(Random(), 0, 9) + 730;
}

void DisasterDailyLoop(void)
{
	if (--_disaster_delay != 0) return;

	ResetDisasterDelay();

	if (_opt.diff.disasters != 0) DoDisaster();
}

void StartupDisasters(void)
{
	ResetDisasterDelay();
}
