#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "map.h"
#include "vehicle.h"
#include "command.h"
#include "news.h"
#include "gfx.h"
#include "station.h"
#include "town.h"
#include "industry.h"
#include "player.h"
#include "airport_movement.h"
#include "sound.h"

static void DisasterClearSquare(uint tile)
{
	int type;

	if (!EnsureNoVehicle(tile))
		return;

	type = _map_type_and_height[tile] >> 4;

	if (type == MP_RAILWAY) {
		if (IS_HUMAN_PLAYER(_map_owner[tile]))
			DoClearSquare(tile);
	} else if (type == MP_HOUSE) {
		byte p = _current_player;
		_current_player = OWNER_NONE;
		DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		_current_player = p;
	} else if (type == MP_TREES || type == MP_CLEAR) {
		DoClearSquare(tile);
	}
}

static const SpriteID _disaster_images_1[] = {0xF41,0xF41,0xF41,0xF41,0xF41,0xF41,0xF41,0xF41};
static const SpriteID _disaster_images_2[] = {0xF44,0xF44,0xF44,0xF44,0xF44,0xF44,0xF44,0xF44};
static const SpriteID _disaster_images_3[] = {0xF4E,0xF4E,0xF4E,0xF4E,0xF4E,0xF4E,0xF4E,0xF4E};
static const SpriteID _disaster_images_4[] = {0xF46,0xF46,0xF47,0xF47,0xF48,0xF48,0xF49,0xF49};
static const SpriteID _disaster_images_5[] = {0xF4A,0xF4A,0xF4B,0xF4B,0xF4C,0xF4C,0xF4D,0xF4D};
static const SpriteID _disaster_images_6[] = {0xF50,0xF50,0xF50,0xF50,0xF50,0xF50,0xF50,0xF50};
static const SpriteID _disaster_images_7[] = {0xF51,0xF51,0xF51,0xF51,0xF51,0xF51,0xF51,0xF51};
static const SpriteID _disaster_images_8[] = {0xF52,0xF52,0xF52,0xF52,0xF52,0xF52,0xF52,0xF52};
static const SpriteID _disaster_images_9[] = {0xF3E,0xF3E,0xF3E,0xF3E,0xF3E,0xF3E,0xF3E,0xF3E};

static const SpriteID * const _disaster_images[] = {
	_disaster_images_1,_disaster_images_1,
	_disaster_images_2,_disaster_images_2,
	_disaster_images_3,_disaster_images_3,
	_disaster_images_8,_disaster_images_8,_disaster_images_9,
	_disaster_images_6,_disaster_images_6,
	_disaster_images_7,_disaster_images_7,
	_disaster_images_4,_disaster_images_5,
};

static void DisasterVehicleUpdateImage(Vehicle *v)
{
	int img = v->u.disaster.image_override;
	if (img == 0)
		img = _disaster_images[v->subtype][v->direction];
	v->cur_image = img;
}

static void InitializeDisasterVehicle(Vehicle *v, int x, int y, byte z, byte direction, byte subtype)
{
	v->type = VEH_Disaster;
	v->x_pos = x;
	v->y_pos = y;
	v->z_pos = z;
	v->tile = TILE_FROM_XY(x,y);
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
	v->current_order.station = 0;

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
	int yt;

	BeginVehicleMove(v);
	v->x_pos = x;
	v->y_pos = y;
	v->z_pos = z;
	v->tile = TILE_FROM_XY(x,y);

	DisasterVehicleUpdateImage(v);
	VehiclePositionChanged(v);
	EndVehicleMove(v);

	if ( (u=v->next) != NULL) {
		BeginVehicleMove(u);

		u->x_pos = x;
		u->y_pos = yt = y - 1 - (max(z - GetSlopeZ(x, y-1), 0) >> 3);
		u->z_pos = GetSlopeZ(x,yt);
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
	uint tile;

	++v->tick_counter;

	if (v->current_order.station < 2) {
		if (v->tick_counter&1)
			return;

		GetNewVehiclePos(v, &gp);

		SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);

		if (v->current_order.station == 1) {
			if (++v->age == 38) {
				v->current_order.station = 2;
				v->age = 0;
			}

			if ((v->tick_counter&7)==0) {
				CreateEffectVehicleRel(v, 0, -17, 2, EV_SMOKE_3);
			}
		} else if (v->current_order.station == 0) {
			tile = v->tile; /**/

			if (IS_TILETYPE(tile, MP_STATION) &&
				IS_BYTE_INSIDE(_map5[tile], 8, 0x43) &&
				IS_HUMAN_PLAYER(_map_owner[tile])) {

				v->current_order.station = 1;
				v->age = 0;

				SetDParam(0, _map2[tile]);
				AddNewsItem(STR_B000_ZEPPELIN_DISASTER_AT,
					NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ACCIDENT, 0),
					v->index,
					0);
			}
		}
		if (v->y_pos >= (MapSizeY() + 9) * 16 - 1)
			DeleteDisasterVeh(v);
		return;
	}

	if (v->current_order.station > 2) {
		if (++v->age <= 13320)
			return;

		tile = v->tile; /**/

		if (IS_TILETYPE(tile, MP_STATION) &&
			IS_BYTE_INSIDE(_map5[tile], 8, 0x43) &&
			IS_HUMAN_PLAYER(_map_owner[tile])) {

			st = DEREF_STATION(_map2[tile]);
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
		CreateEffectVehicleRel(v, 0, 7, 8, EV_CRASHED_SMOKE);
		SndPlayVehicleFx(SND_12_EXPLOSION, v);
		v->u.disaster.image_override = 0xF42;
	} else if (v->age == 70) {
		v->u.disaster.image_override = 0xF43;
	} else if (v->age <= 300) {
		if (!(v->tick_counter&7)) {
			uint32 r = Random();

			CreateEffectVehicleRel(v,
				-7 + (r&0xF),
				-7 + (r>>4&0xF),
				5 + (r>>8&0x7),
				EV_DEMOLISH);
		}
	} else if (v->age == 350) {
		v->current_order.station = 3;
		v->age = 0;
	}

	tile = v->tile;/**/
	if (IS_TILETYPE(tile, MP_STATION) &&
		IS_BYTE_INSIDE(_map5[tile], 8, 0x43) &&
		IS_HUMAN_PLAYER(_map_owner[tile])) {

		st = DEREF_STATION(_map2[tile]);
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

	v->u.disaster.image_override = (++v->tick_counter & 8) ? 0xF45 : 0xF44;

	if (v->current_order.station == 0) {
// fly around randomly
		int x = GET_TILE_X(v->dest_tile)*16;
		int y = GET_TILE_Y(v->dest_tile)*16;
		if (abs(x - v->x_pos) +	abs(y - v->y_pos) >= 16) {
			v->direction = GetDirectionTowards(v, x, y);
			GetNewVehiclePos(v, &gp);
			SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);
			return;
		}
		if (++v->age < 6) {
			v->dest_tile = TILE_MASK(Random());
			return;
		}
		v->current_order.station = 1;

		FOR_ALL_VEHICLES(u) {
			if (u->type == VEH_Road && IS_HUMAN_PLAYER(u->owner)) {
				v->dest_tile = u->index;
				v->age = 0;
				return;
			}
		}

		DeleteDisasterVeh(v);
	} else {
// target a vehicle
		u = &_vehicles[v->dest_tile];
		if (u->type != VEH_Road) {
			DeleteDisasterVeh(v);
			return;
		}

		dist = abs(v->x_pos - u->x_pos) + abs(v->y_pos - u->y_pos);

		if (dist < 16 && !(u->vehstatus&VS_HIDDEN) && u->breakdown_ctr==0) {
			u->breakdown_ctr = 3;
			u->breakdown_delay = 140;
		}

		v->direction = GetDirectionTowards(v, u->x_pos, u->y_pos);
		GetNewVehiclePos(v, &gp);

		z = v->z_pos;
		if (dist <= 16 && z > u->z_pos) z--;
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
			CreateEffectVehicleRel(v, 0, 7, 8, EV_CRASHED_SMOKE);
			SndPlayVehicleFx(SND_12_EXPLOSION, v);
			DeleteDisasterVeh(v);
		}
	}
}

static void DestructIndustry(Industry *i)
{
	uint tile;
	byte index = i - _industries;

	for(tile=0; tile != MapSize(); tile++) {
		if (IS_TILETYPE(tile, MP_INDUSTRY) &&	_map2[tile] == index) {
			_map_owner[tile] = 0;
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
		(v->current_order.station == 1 && v->tick_counter&4) ? 0xF4F : 0;

	GetNewVehiclePos(v, &gp);
	SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);

	if (gp.x < -160) {
		DeleteDisasterVeh(v);
		return;
	}

	if (v->current_order.station == 2) {
		if (!(v->tick_counter&3)) {
			Industry *i = DEREF_INDUSTRY(v->dest_tile);
			int x = GET_TILE_X(i->xy)*16;
			int y = GET_TILE_Y(i->xy)*16;
			uint32 r = Random();

			CreateEffectVehicleAbove(
				x + (r & 0x3F),
				y + (r >> 6 & 0x3F),
				(r >> 12 & 0xF),
				EV_DEMOLISH);

			if (++v->age >= 55)
				v->current_order.station = 3;
		}
	} else if (v->current_order.station == 1) {
		if (++v->age == 112) {
			Industry *i;

			v->current_order.station = 2;
			v->age = 0;

			i = DEREF_INDUSTRY(v->dest_tile);
			DestructIndustry(i);

			SetDParam(0, i->town->index);
			AddNewsItem(STR_B002_OIL_REFINERY_EXPLOSION, NEWS_FLAGS(NM_THIN,NF_VIEWPORT|NF_TILE,NT_ACCIDENT,0), i->xy, 0);
			SndPlayTileFx(SND_12_EXPLOSION, i->xy);
		}
	} else if (v->current_order.station == 0) {
		int x,y;
		uint tile;
		int ind;

		x = v->x_pos - 15*16;
		y = v->y_pos;

		if ( (uint)x > MapMaxX() * 16-1)
			return;

		tile = TILE_FROM_XY(x,y);
		if (!IS_TILETYPE(tile, MP_INDUSTRY))
			return;

		v->dest_tile = ind = _map2[tile];

		if (DEREF_INDUSTRY(ind)->type == IT_OIL_REFINERY) {
			v->current_order.station = 1;
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
		(v->current_order.station == 1 && v->tick_counter&4) ? 0xF53 : 0;

	GetNewVehiclePos(v, &gp);
	SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);

	if (gp.x > MapSizeX() * 16 + 9*16 - 1) {
		DeleteDisasterVeh(v);
		return;
	}

	if (v->current_order.station == 2) {
		if (!(v->tick_counter&3)) {
			Industry *i = DEREF_INDUSTRY(v->dest_tile);
			int x = GET_TILE_X(i->xy)*16;
			int y = GET_TILE_Y(i->xy)*16;
			uint32 r = Random();

			CreateEffectVehicleAbove(
				x + (r & 0x3F),
				y + (r >> 6 & 0x3F),
				(r >> 12 & 0xF),
				EV_DEMOLISH);

			if (++v->age >= 55)
				v->current_order.station = 3;
		}
	} else if (v->current_order.station == 1) {
		if (++v->age == 112) {
			Industry *i;

			v->current_order.station = 2;
			v->age = 0;

			i = DEREF_INDUSTRY(v->dest_tile);
			DestructIndustry(i);

			SetDParam(0, i->town->index);
			AddNewsItem(STR_B003_FACTORY_DESTROYED_IN_SUSPICIOUS, NEWS_FLAGS(NM_THIN,NF_VIEWPORT|NF_TILE,NT_ACCIDENT,0), i->xy, 0);
			SndPlayTileFx(SND_12_EXPLOSION, i->xy);
		}
	} else if (v->current_order.station == 0) {
		int x,y;
		uint tile;
		int ind;

		x = v->x_pos - 15*16;
		y = v->y_pos;

		if ( (uint)x > MapMaxX() * 16-1)
			return;

		tile = TILE_FROM_XY(x,y);
		if (!IS_TILETYPE(tile, MP_INDUSTRY))
			return;

		v->dest_tile = ind = _map2[tile];

		if (DEREF_INDUSTRY(ind)->type == IT_FACTORY) {
			v->current_order.station = 1;
			v->age = 0;
		}
	}
}

// Helicopter rotor blades
static void DisasterTick_3b(Vehicle *v)
{
	if (++v->tick_counter & 1)
		return;

	if (++v->cur_image == 0xF40 + 1)
		v->cur_image = 0xF3E;

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
	uint tile,tile_org;

	v->tick_counter++;

	if (v->current_order.station == 1) {
		int x = GET_TILE_X(v->dest_tile)*16 + 8;
		int y = GET_TILE_Y(v->dest_tile)*16 + 8;
		if (abs(v->x_pos - x) + abs(v->y_pos - y) >= 8) {
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

		v->current_order.station = 2;

		FOR_ALL_VEHICLES(u) {
			if (u->type == VEH_Train || u->type == VEH_Road) {
				if (abs(u->x_pos - v->x_pos) + abs(u->y_pos - v->y_pos) <= 12*16) {
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

		InitializeDisasterVehicle(u, -6*16, v->y_pos, 135, 5, 11);
		u->u.disaster.unk2 = v->index;

		w = ForceAllocateSpecialVehicle();
		if (w == NULL)
			return;

		u->next = w;
		InitializeDisasterVehicle(w, -6*16, v->y_pos, 0, 5, 12);
		w->vehstatus |= VS_DISASTER;
	} else if (v->current_order.station < 1) {

		int x = GET_TILE_X(v->dest_tile)*16;
		int y = GET_TILE_Y(v->dest_tile)*16;
		if (abs(x - v->x_pos) +	abs(y - v->y_pos) >= 16) {
			v->direction = GetDirectionTowards(v, x, y);
			GetNewVehiclePos(v, &gp);
			SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);
			return;
		}

		if (++v->age < 6) {
			v->dest_tile = TILE_MASK(Random());
			return;
		}
		v->current_order.station = 1;

		tile_org = tile = TILE_MASK(Random());
		do {
			if (IS_TILETYPE(tile, MP_RAILWAY) &&
					(_map5[tile]&~3)!=0xC0 &&	IS_HUMAN_PLAYER(_map_owner[tile]))
						break;
			tile = TILE_MASK(tile+1);
		} while (tile != tile_org);
		v->dest_tile = tile;
		v->age = 0;
	} else
		return;
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

	if (gp.x > MapSizeX() * 16 + 9*16 - 1) {
		DeleteDisasterVeh(v);
		return;
	}

	if (v->current_order.station == 0) {
		u = &_vehicles[v->u.disaster.unk2];
		if (abs(v->x_pos - u->x_pos) > 16)
			return;
		v->current_order.station = 1;

		CreateEffectVehicleRel(u, 0, 7, 8, EV_CRASHED_SMOKE);
		SndPlayVehicleFx(SND_12_EXPLOSION, u);

		DeleteDisasterVeh(u);

		for(i=0; i!=80; i++) {
			uint32 r = Random();
			CreateEffectVehicleAbove(
				v->x_pos-32+(r&0x3F),
				v->y_pos-32+(r>>5&0x3F),
				0,
				EV_DEMOLISH);
		}

		BEGIN_TILE_LOOP(tile,6,6,v->tile - TILE_XY(3,3))
			tile = TILE_MASK(tile);
			DisasterClearSquare(tile);
		END_TILE_LOOP(tile,6,6,v->tile - TILE_XY(3,3))
	}
}

// Submarine handler
static void DisasterTick_5_and_6(Vehicle *v)
{
	uint32 r;
	GetNewVehiclePosResult gp;
	uint tile;

	v->tick_counter++;

	if (++v->age > 8880) {
		VehiclePositionChanged(v);
		BeginVehicleMove(v);
		EndVehicleMove(v);
		DeleteVehicle(v);
		return;
	}

	if (!(v->tick_counter&1))
		return;

	tile = v->tile + _tileoffs_by_dir[v->direction >> 1];
	if (IsValidTile(tile) &&
			(r=GetTileTrackStatus(tile,TRANSPORT_WATER),(byte)(r+(r >> 8)) == 0x3F) &&
			!CHANCE16(1,90)) {
		GetNewVehiclePos(v, &gp);
		SetDisasterVehiclePos(v, gp.x, gp.y, v->z_pos);
		return;
	}

	v->direction = (v->direction + ((Random()&1)?2:-2))&7;
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

void HandleClickOnDisasterVeh(Vehicle *v)
{
	// not used
}

void OnNewDay_DisasterVehicle(Vehicle *v)
{
	// not used
}

typedef void DisasterInitProc();

// Zeppeliner which crashes on a small airport
static void Disaster0_Init()
{
	Vehicle *v = ForceAllocateSpecialVehicle(), *u;
	Station *st;
	int x;

	if (v == NULL)
		return;

	for(st=_stations;;) {
		if (st->xy && st->airport_tile != 0 &&
				st->airport_type <= 1 &&
				IS_HUMAN_PLAYER(st->owner)) {
			x = (GET_TILE_X(st->xy) + 2) * 16;
			break;
		}

		if (++st == endof(_stations)) {
			x = (GET_TILE_X(Random())) * 16 + 8;
			break;
		}
	}

	InitializeDisasterVehicle(v, x, 0, 135, 3, 0);

	// Allocate shadow too?
	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u,x,0,0,3,1);
		u->vehstatus |= VS_DISASTER;
	}
}

static void Disaster1_Init()
{
	Vehicle *v = ForceAllocateSpecialVehicle(), *u;
	int x;

	if (v == NULL)
		return;

	x = (GET_TILE_X(Random())) * 16 + 8;

	InitializeDisasterVehicle(v, x, 0, 135, 3, 2);
	v->dest_tile = TILE_XY(MapSizeX() / 2, MapSizeY() / 2);
	v->age = 0;

	// Allocate shadow too?
	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u,x,0,0,3,3);
		u->vehstatus |= VS_DISASTER;
	}
}

static void Disaster2_Init()
{
	Industry *i, *found;
	Vehicle *v,*u;
	int x,y;

	found = NULL;

	FOR_ALL_INDUSTRIES(i) {
		if (i->xy != 0 &&
				i->type == IT_OIL_REFINERY &&
				(found==NULL || CHANCE16(1,2))) {
			found = i;
		}
	}

	if (found == NULL)
		return;

	v = ForceAllocateSpecialVehicle();
	if (v == NULL)
		return;

	x = (MapSizeX() + 9) * 16 - 1;
	y = GET_TILE_Y(found->xy)*16 + 37;

	InitializeDisasterVehicle(v,x,y, 135,1,4);

	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u,x,y,0,3,5);
		u->vehstatus |= VS_DISASTER;
	}
}

static void Disaster3_Init()
{
	Industry *i, *found;
	Vehicle *v,*u,*w;
	int x,y;

	found = NULL;

	FOR_ALL_INDUSTRIES(i) {
		if (i->xy != 0 &&
				i->type == IT_FACTORY &&
				(found==NULL || CHANCE16(1,2))) {
			found = i;
		}
	}

	if (found == NULL)
		return;

	v = ForceAllocateSpecialVehicle();
	if (v == NULL)
		return;

	x = -16 * 16;
	y = GET_TILE_Y(found->xy)*16 + 37;

	InitializeDisasterVehicle(v,x,y, 135,5,6);

	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u,x,y,0,5,7);
		u->vehstatus |= VS_DISASTER;

		w = ForceAllocateSpecialVehicle();
		if (w != NULL) {
			u->next = w;
			InitializeDisasterVehicle(w,x,y,140,5,8);
		}
	}
}

static void Disaster4_Init()
{
	Vehicle *v = ForceAllocateSpecialVehicle(), *u;
	int x,y;

	if (v == NULL)
		return;

	x = (GET_TILE_X(Random())) * 16 + 8;

	y = MapMaxX() * 16 - 1;
	InitializeDisasterVehicle(v, x, y, 135, 7, 9);
	v->dest_tile = TILE_XY(MapSizeX() / 2, MapSizeY() / 2);
	v->age = 0;

	// Allocate shadow too?
	u = ForceAllocateSpecialVehicle();
	if (u != NULL) {
		v->next = u;
		InitializeDisasterVehicle(u,x,y,0,7,10);
		u->vehstatus |= VS_DISASTER;
	}
}

// Submarine type 1
static void Disaster5_Init()
{
	Vehicle *v = ForceAllocateSpecialVehicle();
	int x,y;
	byte dir;
	uint32 r;

	if (v == NULL)
		return;

	r = Random();
	x = (GET_TILE_X(r)) * 16 + 8;

	y = 8;
	dir = 3;
	if (r & 0x80000000) { y = MapMaxX() * 16 - 8 - 1; dir = 7; }
	InitializeDisasterVehicle(v, x, y, 0, dir,13);
	v->age = 0;
}

// Submarine type 2
static void Disaster6_Init()
{
	Vehicle *v = ForceAllocateSpecialVehicle();
	int x,y;
	byte dir;
	uint32 r;

	if (v == NULL)
		return;

	r = Random();
	x = (GET_TILE_X(r)) * 16 + 8;

	y = 8;
	dir = 3;
	if (r & 0x80000000) { y = MapMaxX() * 16 - 8 - 1; dir = 7; }
	InitializeDisasterVehicle(v, x, y, 0, dir,14);
	v->age = 0;
}

static void Disaster7_Init()
{
	Industry *i;
	int maxloop = 15;
	int index = Random() & 0xF;

	do {
		FOR_ALL_INDUSTRIES(i) {
			if (i->xy != 0 && i->type == IT_COAL_MINE	&& --index < 0) {

				SetDParam(0, i->town->index);
				AddNewsItem(STR_B005_COAL_MINE_SUBSIDENCE_LEAVES,
					NEWS_FLAGS(NM_THIN,NF_VIEWPORT|NF_TILE,NT_ACCIDENT,0), i->xy + TILE_XY(1,1), 0);

				{
					uint tile = i->xy;
					int step = _tileoffs_by_dir[Random() & 3];
					int count = 30;
					do {
						DisasterClearSquare(tile);
						tile = TILE_MASK(tile + step);
					} while (--count);
				}
				return;
			}
		}
	} while (--maxloop != 0);
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

typedef struct {
	byte min,max;
} DisasterYears;

#define MK(a,b) {a-20,b-20}
static const DisasterYears _dis_years[8] = {
	MK(30,55),
	MK(40,70),
	MK(60,90),
	MK(70,100),
	MK(100,200),
	MK(40,65),
	MK(75,110),
	MK(50,85),
};


static void DoDisaster()
{
	byte buf[8];
	byte year = _cur_year;
	int i,j;

	for(i=j=0; i!=lengthof(_dis_years); i++) {
		if (year >= _dis_years[i].min &&
				year < _dis_years[i].max)
					buf[j++] = i;
	}

	if (j == 0)
		return;

	_disaster_initprocs[buf[(uint16)Random() * j >> 16]]();
}


static void ResetDisasterDelay()
{
	_disaster_delay = (int)(Random() & 0x1FF) + 730;
}

void DisasterDailyLoop()
{
	if (--_disaster_delay != 0)
		return;

	ResetDisasterDelay();

	if (_opt.diff.disasters != 0)
		DoDisaster();
}

void StartupDisasters() {
	ResetDisasterDelay();
}

