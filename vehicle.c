#include "stdafx.h"
#include "ttd.h"
#include "vehicle.h"
#include "gfx.h"
//#include "station.h"
#include "viewport.h"
#include "news.h"
#include "command.h"
#include "saveload.h"
#include "player.h"
#include "engine.h"
#include "sound.h"

#define INVALID_COORD (-0x8000)
#define GEN_HASH(x,y) (((x & 0x1F80)>>7) + ((y & 0xFC0)))


void VehicleInTheWayErrMsg(Vehicle *v)
{
	StringID id;

	(id = STR_8803_TRAIN_IN_THE_WAY,v->type == VEH_Train) ||
	(id = STR_9000_ROAD_VEHICLE_IN_THE_WAY,v->type == VEH_Road) ||
	(id = STR_A015_AIRCRAFT_IN_THE_WAY,v->type == VEH_Aircraft) ||
		(id = STR_980E_SHIP_IN_THE_WAY, true);

	_error_message = id;
}

static void *EnsureNoVehicleProc(Vehicle *v, void *data)
{
	if (v->tile != (TileIndex)(int)data || v->type == VEH_Disaster)
		return NULL;

	VehicleInTheWayErrMsg(v);
	return (void*)1;
}

bool EnsureNoVehicle(TileIndex tile)
{
	return VehicleFromPos(tile, (void*)(int)tile, (VehicleFromPosProc*)EnsureNoVehicleProc) == NULL;
}

static void *EnsureNoVehicleProcZ(Vehicle *v, void *data)
{
	uint32 d = (uint32)data;	// max mapsize 4,096*4,096 if shifted 8 bits for byte
	// with uint64 max mapsize is 2,6843,5456*2,6843,5456 :D
	if (v->tile != (TileIndex)(d >> (sizeof(byte)*8)) || v->z_pos != (byte)d || v->type == VEH_Disaster)
		return NULL;

	VehicleInTheWayErrMsg(v);
	return (void*)1;
}

bool EnsureNoVehicleZ(TileIndex tile, byte z)
{
	TileInfo ti;
	FindLandscapeHeightByTile(&ti, tile);
	// needs z correction for slope-type graphics that have the NORTHERN tile lowered
	// 1, 2, 3, 4, 5, 6 and 7
	if (CORRECT_Z(ti.tileh))
		z += 8;

	return VehicleFromPos(tile, (void*)((TileIndex)tile << (sizeof(byte)*8) | (byte)z), (VehicleFromPosProc*)EnsureNoVehicleProcZ) == NULL;
}

Vehicle *FindVehicleBetween(TileIndex from, TileIndex to, byte z)
{
	int x1 = GET_TILE_X(from);
	int y1 = GET_TILE_Y(from);
	int x2 = GET_TILE_X(to);
	int y2 = GET_TILE_Y(to);
	Vehicle *veh;
	uint count;

	/* Make sure x1 < x2 or y1 < y2 */
	if (x1 > x2 || y1 > y2) {
		intswap(x1,x2);
		intswap(y1,y2);
	}
	for(veh = _vehicles, count=lengthof(_vehicles); count != 0; count--, veh++) {
		if ((veh->type == VEH_Train || veh->type == VEH_Road) && (z==0xFF || veh->z_pos == z)) {
			if ((veh->x_pos>>4) >= x1 && (veh->x_pos>>4) <= x2 &&
					(veh->y_pos>>4) >= y1 && (veh->y_pos>>4) <= y2) {
				return veh;
			}
		}
	}
	return NULL;
}

void VehiclePositionChanged(Vehicle *v)
{
	int img = v->cur_image;
	const SpriteDimension *sd;
	Point pt = RemapCoords(v->x_pos + v->x_offs, v->y_pos + v->y_offs, v->z_pos);

	sd = GetSpriteDimension(img);

	pt.x += sd->xoffs;
	pt.y += sd->yoffs;

	UpdateVehiclePosHash(v, pt.x, pt.y);

	v->left_coord = pt.x;
	v->top_coord = pt.y;
	v->right_coord = pt.x + sd->xsize + 2;
	v->bottom_coord = pt.y + sd->ysize + 2;
}

void UpdateWaypointSign(Waypoint *cp)
{
	Point pt = RemapCoords2(GET_TILE_X(cp->xy)*16, GET_TILE_Y(cp->xy)*16);
	SET_DPARAM16(0, cp - _waypoints);
	UpdateViewportSignPos(&cp->sign, pt.x, pt.y - 0x20, STR_WAYPOINT_VIEWPORT);
}

void RedrawWaypointSign(Waypoint *cp)
{
	MarkAllViewportsDirty(
		cp->sign.left - 6,
		cp->sign.top,
		cp->sign.left + (cp->sign.width_1 << 2) + 12,
		cp->sign.top + 48);
}

// Called after load to update coordinates
void AfterLoadVehicles()
{
	Vehicle *v;
	Waypoint *cp;

	FOR_ALL_VEHICLES(v) {
		if (v->type != 0) {
			v->left_coord = INVALID_COORD;
			VehiclePositionChanged(v);

			if (v->type == VEH_Train) {
				if (v->subtype == 0)
					UpdateTrainAcceleration(v);
			}

#if defined(_DEBUG)
			if (!(v->schedule_ptr == NULL || (v->schedule_ptr >= _order_array && v->schedule_ptr < _ptr_to_next_order)))
				v->schedule_ptr = NULL;
#endif
		}
	}

	// update waypoint signs
	for(cp=_waypoints; cp != endof(_waypoints); cp++) if (cp->xy) UpdateWaypointSign(cp);
}


static Vehicle *InitializeVehicle(Vehicle *v)
{
	VehicleID index = v->index;
	memset(v, 0, sizeof(Vehicle));
	v->index = index;

	assert(v->schedule_ptr == NULL);

	v->left_coord = INVALID_COORD;
	v->next = NULL;
	v->next_hash = 0xffff;
	v->string_id = 0;
	return v;
}

Vehicle *ForceAllocateSpecialVehicle()
{
	Vehicle *v;
	for(v=_vehicles + NUM_NORMAL_VEHICLES;
			v!=&_vehicles[NUM_NORMAL_VEHICLES+NUM_SPECIAL_VEHICLES]; v++) {
		if (v->type == 0)
			return InitializeVehicle(v);
	}
	return NULL;

}

Vehicle *ForceAllocateVehicle()
{
	Vehicle *v;
	for(v=_vehicles; v!=&_vehicles[NUM_NORMAL_VEHICLES]; v++) {
		if (v->type == 0)
			return InitializeVehicle(v);
	}
	return NULL;
}

Vehicle *AllocateVehicle()
{
	Vehicle *v;
	int num;

	if (IS_HUMAN_PLAYER(_current_player)) {
		num = 0;

		for(v=_vehicles; v!=&_vehicles[NUM_NORMAL_VEHICLES]; v++) {
			if (v->type == 0)
				num++;
		}

		if (num <= 90)
			return NULL;
	}

	return ForceAllocateVehicle();
}

void *VehicleFromPos(TileIndex tile, void *data, VehicleFromPosProc *proc)
{
	int x,y,x2,y2;
	VehicleID veh;
	Point pt = RemapCoords(GET_TILE_X(tile) * 16, GET_TILE_Y(tile) * 16, 0);

	x2 = ((pt.x + 104) & 0x1F80) >> 7;
	x  = ((pt.x - 174) & 0x1F80) >> 7;

	y2 = ((pt.y + 56) & 0xFC0);
	y  = ((pt.y - 294) & 0xFC0);

	for(;;) {
		int xb = x;
		for(;;) {
			veh = _vehicle_position_hash[ (x+y)&0xFFFF ];
			while (veh != INVALID_VEHICLE) {
				Vehicle *v = &_vehicles[veh];
				void *a;

				if ((a = proc(v, data)) != NULL)
					return a;
				veh = v->next_hash;
			}

			if (x == x2)
				break;

			x = (x + 1) & 0x3F;
		}
		x = xb;

		if (y == y2)
			break;

		y = (y + 0x40) & ((0x3F) << 6);
	}
	return NULL;
}



void UpdateVehiclePosHash(Vehicle *v, int x, int y)
{
	VehicleID *old_hash, *new_hash;
	int old_x = v->left_coord;
	int old_y = v->top_coord;
	Vehicle *u;

	new_hash = (x == INVALID_COORD) ? NULL : &_vehicle_position_hash[GEN_HASH(x,y)];
	old_hash = (old_x == INVALID_COORD) ? NULL : &_vehicle_position_hash[GEN_HASH(old_x, old_y)];

	if (old_hash == new_hash)
		return;

	/* remove from hash table? */
	if (old_hash != NULL) {
		Vehicle *last = NULL;
		int idx = *old_hash;
		while((u = &_vehicles[idx]) != v) {
			idx = u->next_hash;
			assert(idx != INVALID_VEHICLE);
			last = u;
		}

		if (last == NULL)
			*old_hash = v->next_hash;
		else
			last->next_hash = v->next_hash;
	}

	/* insert into hash table? */
	if (new_hash != NULL) {
		v->next_hash = *new_hash;
		*new_hash = v->index;
	}
}

void InitializeVehicles()
{
	Vehicle *v;
	int i;

	// clear it...
	memset(&_vehicles, 0, sizeof(_vehicles));
	memset(&_waypoints, 0, sizeof(_waypoints));
	memset(&_depots, 0, sizeof(_depots));

	// setup indexes..
	i = 0;
	FOR_ALL_VEHICLES(v)
		v->index = i++;

	memset(_vehicle_position_hash, -1, sizeof(_vehicle_position_hash));

	_ptr_to_next_order = _order_array;
}

Vehicle *GetLastVehicleInChain(Vehicle *v)
{
	while (v->next != NULL) v = v->next;
	return v;
}

Vehicle *GetPrevVehicleInChain(Vehicle *v)
{
	Vehicle *org = v;

	v = _vehicles;
	while (v->type != VEH_Train || org != v->next) {
		if (++v == endof(_vehicles))
			return NULL;
	}

	return v;
}

Vehicle *GetFirstVehicleInChain(Vehicle *v)
{
	for(;;) {
		Vehicle *u = v;
		v = _vehicles;
		while (v->type != VEH_Train || u != v->next) {
			if (++v == endof(_vehicles))
				return u;
		}
	}
}

int CountVehiclesInChain(Vehicle *v)
{
	int count = 0;
	do count++; while ( (v=v->next) != NULL);
	return count;
}


Depot *AllocateDepot()
{
	Depot *dep, *free_dep = NULL;
	int num_free = 0;

	for(dep = _depots; dep != endof(_depots); dep++) {
		if (dep->xy == 0) {
			num_free++;
			if (free_dep==NULL)
				free_dep = dep;
		}
	}

	if (free_dep == NULL ||
			(num_free < 30 && IS_HUMAN_PLAYER(_current_player))) {
		_error_message = STR_1009_TOO_MANY_DEPOTS;
		return NULL;
	}

	return free_dep;
}

Waypoint *AllocateWaypoint()
{
	Waypoint *cp;

	for(cp = _waypoints; cp != endof(_waypoints); cp++) {
		if (cp->xy == 0)
			return cp;
	}

	return NULL;
}

uint GetWaypointByTile(uint tile)
{
	Waypoint *cp;
	int i=0;
	for(cp=_waypoints; cp->xy != (TileIndex)tile; cp++) { i++; }
	return i;
}


Vehicle *IsScheduleShared(Vehicle *u)
{
	uint16 *sched = u->schedule_ptr;
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->schedule_ptr == sched && u != v && v->type != 0)
			return v;
	}
	return NULL;
}

void DeleteVehicleSchedule(Vehicle *v)
{
	uint16 *sched, *cur;
	int num;
	Vehicle *u;

	// if the schedule is shared, don't delete it.
	if ((u = IsScheduleShared(v)) != NULL) {
		v->schedule_ptr = NULL;
		InvalidateWindow(WC_VEHICLE_ORDERS, u->index);
		return;
	}

	sched = v->schedule_ptr;
	v->schedule_ptr = NULL;

	num = v->num_orders + 1;

	_ptr_to_next_order -= num;

	cur = sched;
	while (cur != _ptr_to_next_order) {
		assert(cur < _ptr_to_next_order);
		cur[0] = cur[num];
		cur++;
	}

	FOR_ALL_VEHICLES(v) {
		if (v->schedule_ptr != NULL && sched < v->schedule_ptr) {
			v->schedule_ptr -= num;
		}
	}
}

void DeleteCommandFromVehicleSchedule(uint cmd)
{
	Vehicle *v;
	uint16 *sched;
	uint order;
	bool need_invalidate;

	FOR_ALL_VEHICLES(v) {
		if (v->type != 0 && v->schedule_ptr != NULL) {

			// clear last station visited
			if (v->last_station_visited == (cmd>>8) && (cmd & OT_MASK) == OT_GOTO_STATION)
				v->last_station_visited = 0xFF;

			// check the next order
			if ( (uint)((v->next_order&OT_MASK) | (v->next_order_param<<8)) == cmd) {
				v->next_order = OT_DUMMY;
				InvalidateWindow(WC_VEHICLE_VIEW, v->index);
			}

			// clear the order list
			need_invalidate = false;
			sched = v->schedule_ptr;
			while ((order=*sched++) != 0) {
				if ( (order & (OT_MASK|0xFF00)) == cmd) {
					sched[-1] = OT_DUMMY;
					need_invalidate = true;
				}
			}

			if (need_invalidate)
				InvalidateWindow(WC_VEHICLE_ORDERS, v->index);
		}
	}
}

void DoDeleteDepot(uint tile)
{
	byte dep_index;
	Depot *d;

	// Clear it
	DoClearSquare(tile);

	// Nullify the depot struct
	for(d=_depots,dep_index=0; d->xy != (TileIndex)tile; d++) {dep_index++;}
	d->xy = 0;

	DeleteCommandFromVehicleSchedule((dep_index << 8) + OT_GOTO_DEPOT);

	// Delete the depot
	DeleteWindowById(WC_VEHICLE_DEPOT, tile);
}

void DeleteVehicle(Vehicle *v)
{
	DeleteName(v->string_id);
	v->type = 0;
	UpdateVehiclePosHash(v, INVALID_COORD, 0);
	v->next_hash = 0xffff;

	if (v->schedule_ptr != NULL)
		DeleteVehicleSchedule(v);
}

void DeleteVehicleChain(Vehicle *v)
{
	do {
		Vehicle *u = v;
		v = v->next;
		DeleteVehicle(u);
	} while (v);
}


void Aircraft_Tick(Vehicle *v);
void RoadVeh_Tick(Vehicle *v);
void Ship_Tick(Vehicle *v);
void Train_Tick(Vehicle *v);
void EffectVehicle_Tick(Vehicle *v);
void DisasterVehicle_Tick(Vehicle *v);

VehicleTickProc *_vehicle_tick_procs[] = {
	Train_Tick,
	RoadVeh_Tick,
	Ship_Tick,
	Aircraft_Tick,
	EffectVehicle_Tick,
	DisasterVehicle_Tick,
};

void CallVehicleTicks()
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type != 0)
			_vehicle_tick_procs[v->type - 0x10](v);
	}
}

static bool CanFillVehicle_FullLoadAny(Vehicle *v)
{
	uint32 full = 0, not_full = 0;

	// patch should return "true" to continue loading, i.e. when there is no cargo type that is fully loaded.
	do {
		//Should never happen, but just in case future additions change this
		assert(v->cargo_type<32);

		if (v->cargo_cap != 0) {
			uint32 mask = 1 << v->cargo_type;
			if (v->cargo_cap == v->cargo_count) full |= mask; else not_full |= mask;
		}
	} while ( (v=v->next) != NULL);

	// continue loading if there is a non full cargo type and no cargo type that is full
	return not_full && (full & ~not_full) == 0;
}

bool CanFillVehicle(Vehicle *v)
{
	byte *t = &_map_type_and_height[v->tile];

	if (t[0] >> 4 == MP_STATION ||
			(v->type == VEH_Ship &&
			(t[TILE_XY(1,0)] >> 4 == MP_STATION ||
				t[TILE_XY(-1,0)] >> 4 == MP_STATION ||
				t[TILE_XY(0,1)] >> 4 == MP_STATION ||
				t[TILE_XY(0,-1)] >> 4 == MP_STATION ||
				t[TILE_XY(-2,0)] >> 4 == MP_STATION))) {

		// If patch is active, use alternative CanFillVehicle-function
		if (_patches.full_load_any)
			return CanFillVehicle_FullLoadAny(v);

		do {
			if (v->cargo_count != v->cargo_cap)
				return true;
		} while ( (v=v->next) != NULL);
	}
	return false;
}

static void DoDrawVehicle(Vehicle *v)
{
	uint32 image = v->cur_image;

	if (v->vehstatus & VS_DISASTER) {
		image |= 0x3224000;
	} else if (v->vehstatus & VS_DEFPAL) {
		image |= (v->vehstatus & VS_CRASHED) ? 0x3248000 : SPRITE_PALETTE(PLAYER_SPRITE_COLOR(v->owner));
	}

	AddSortableSpriteToDraw(image, v->x_pos + v->x_offs, v->y_pos + v->y_offs,
		v->sprite_width, v->sprite_height, v->z_height, v->z_pos);
}

void ViewportAddVehicles(DrawPixelInfo *dpi)
{
	int x,xb, y, x2, y2;
	VehicleID veh;
	Vehicle *v;

	x  = ((dpi->left - 70) & 0x1F80) >> 7;
	x2 = ((dpi->left + dpi->width) & 0x1F80) >> 7;

	y  = ((dpi->top - 70) & 0xFC0);
	y2 = ((dpi->top + dpi->height) & 0xFC0);

	for(;;) {
		xb = x;
		for(;;) {
			veh = _vehicle_position_hash[ (x+y)&0xFFFF ];
			while (veh != INVALID_VEHICLE) {
				v = &_vehicles[veh];

				if (!(v->vehstatus & VS_HIDDEN) &&
						dpi->left <= v->right_coord &&
						dpi->top <= v->bottom_coord &&
						dpi->left + dpi->width >= v->left_coord &&
						dpi->top + dpi->height >= v->top_coord) {
					DoDrawVehicle(v);
				}
				veh = v->next_hash;
			}

			if (x == x2)
				break;
			x = (x + 1) & 0x3F;
		}
		x = xb;

		if (y == y2)
			break;
		y = (y + 0x40) & ((0x3F) << 6);
	}
}

static void EffectInit_0(Vehicle *v)
{
	uint32 r = Random();
	v->cur_image = (uint16)((r & 7) + 3701);
	v->progress = (byte)((r >> 16)&7);
}

static void EffectTick_0(Vehicle *v)
{
	uint tile;
	uint img;

	if (--v->progress & 0x80) {
		BeginVehicleMove(v);

		tile = TILE_FROM_XY(v->x_pos, v->y_pos);
		if (!IS_TILETYPE(tile, MP_INDUSTRY)) {
			EndVehicleMove(v);
			DeleteVehicle(v);
			return;
		}

		img = v->cur_image + 1;
		if (img > 3708) img = 3701;
		v->cur_image = img;
		v->progress = 7;
		VehiclePositionChanged(v);
		EndVehicleMove(v);
	}
}

static void EffectInit_1(Vehicle *v)
{
	v->cur_image = 3079;
	v->progress = 12;
}

static void EffectTick_1(Vehicle *v)
{
	bool moved;

	BeginVehicleMove(v);

	moved = false;

	if ((++v->progress & 7) == 0) {
		v->z_pos++;
		moved = true;
	}

	if ((v->progress & 0xF)==4) {
		if (++v->cur_image > 3083) {
			EndVehicleMove(v);
			DeleteVehicle(v);
			return;
		}
		moved = true;
	}

	if (moved) {
		VehiclePositionChanged(v);
		EndVehicleMove(v);
	}
}

static void EffectInit_2(Vehicle *v)
{
	v->cur_image = 3073;
	v->progress = 0;
}

static void EffectTick_2(Vehicle *v)
{
	if ((++v->progress & 3) == 0) {
		BeginVehicleMove(v);
		v->z_pos++;
		VehiclePositionChanged(v);
		EndVehicleMove(v);
	} else if ((v->progress & 7) == 1) {
		BeginVehicleMove(v);
		if (++v->cur_image > 3078) {
			EndVehicleMove(v);
			DeleteVehicle(v);
		} else {
			VehiclePositionChanged(v);
			EndVehicleMove(v);
		}
	}
}

static void EffectInit_3(Vehicle *v)
{
	v->cur_image = 3084;
	v->progress = 1;
}

static void EffectTick_3(Vehicle *v)
{
	if (++v->progress > 2) {
		v->progress = 0;
		BeginVehicleMove(v);
		if (++v->cur_image > 3089) {
			EndVehicleMove(v);
			DeleteVehicle(v);
		} else {
			VehiclePositionChanged(v);
			EndVehicleMove(v);
		}
	}
}

static void EffectInit_4(Vehicle *v)
{
	v->cur_image = 2040;
	v->progress = 12;
}

static void EffectTick_4(Vehicle *v)
{
	bool moved;

	BeginVehicleMove(v);

	moved = false;

	if ((++v->progress & 3) == 0) {
		v->z_pos++;
		moved = true;
	}

	if ((v->progress & 0xF)==4) {
		if (++v->cur_image > 2044) {
			EndVehicleMove(v);
			DeleteVehicle(v);
			return;
		}
		moved = true;
	}

	if (moved) {
		VehiclePositionChanged(v);
		EndVehicleMove(v);
	}
}

static void EffectInit_5(Vehicle *v)
{
	v->cur_image = 3709;
	v->progress = 0;
}

static void EffectTick_5(Vehicle *v)
{
	if (!(++v->progress & 3)) {
		BeginVehicleMove(v);
		if (++v->cur_image > 3724) {
			EndVehicleMove(v);
			DeleteVehicle(v);
		} else {
			VehiclePositionChanged(v);
			EndVehicleMove(v);
		}
	}
}

static void EffectInit_6(Vehicle *v)
{
	v->cur_image = 3737;
	v->progress = 0;
}

static void EffectTick_6(Vehicle *v)
{
	if (!(++v->progress & 7)) {
		BeginVehicleMove(v);
		if (++v->cur_image > 3740) v->cur_image = 3737;
		VehiclePositionChanged(v);
		EndVehicleMove(v);
	}

	if (!--v->u.special.unk0) {
		BeginVehicleMove(v);
		EndVehicleMove(v);
		DeleteVehicle(v);
	}
}

static void EffectInit_7(Vehicle *v)
{
	v->cur_image = 3725;
	v->progress = 0;
}

static void EffectTick_7(Vehicle *v)
{
	if (!(++v->progress & 3)) {
		BeginVehicleMove(v);
		if (++v->cur_image > 3736) {
			EndVehicleMove(v);
			DeleteVehicle(v);
		} else {
			VehiclePositionChanged(v);
			EndVehicleMove(v);
		}
	}
}

static void EffectInit_8(Vehicle *v)
{
	v->cur_image = 1416;
	v->progress = 0;
	v->u.special.unk0 = 0;
	v->u.special.unk2 = 0;
}

#define MK(imag,dir,dur) (imag<<6)+(dir<<4)+dur
static const byte _effecttick8_data[] = {
	MK(0,0,4),
	MK(3,3,4),
	MK(2,2,7),
	MK(0,2,7),
	MK(1,1,3),
	MK(2,2,7),
	MK(0,2,7),
	MK(1,1,3),
	MK(2,2,7),
	MK(0,2,7),
	MK(3,3,6),
	MK(2,2,6),
	MK(1,1,7),
	MK(3,1,7),
	MK(0,0,3),
	MK(1,1,7),
	MK(3,1,7),
	MK(0,0,3),
	MK(1,1,7),
	MK(3,1,7),
	255
};
#undef MK

static const int8 _xy_inc_by_dir[5] = {
	-1, 0, 1, 0, -1,
};

#define GET_X_INC_BY_DIR(d) _xy_inc_by_dir[d]
#define GET_Y_INC_BY_DIR(d) _xy_inc_by_dir[(d)+1]

static void EffectTick_8(Vehicle *v)
{
	byte b;

	if (!(++v->progress & 7)) {
		v->u.special.unk2++;
		BeginVehicleMove(v);

		b = _effecttick8_data[v->u.special.unk0];

		v->cur_image = 0x588 + (b>>6);

		v->x_pos += GET_X_INC_BY_DIR((b>>4)&3);
		v->y_pos += GET_X_INC_BY_DIR((b>>4)&3);

		if (v->u.special.unk2 < (b & 7)) {
			v->u.special.unk2 = 0;
			v->u.special.unk0++;
			if (_effecttick8_data[v->u.special.unk0] == 0xFF) {
				EndVehicleMove(v);
				DeleteVehicle(v);
				return;
			}
		}
		VehiclePositionChanged(v);
		EndVehicleMove(v);
	}
}

static void EffectInit_9(Vehicle *v)
{
	v->cur_image = 4751;
	v->spritenum = 0;
	v->progress = 0;
}

#define MK(x,y,z,i) (x+4)+(y+4)*16,(z+4)+i*16

/* -1,0,1,2 = 2*/
/* -1,0,1   = 2*/
/* */
static const byte _effecttick9_data1[] = {
	MK(0,0,1,0),
	MK(1,0,1,1),
	MK(0,0,1,0),
	MK(1,0,1,2),
	0x81,
};


static const byte _effecttick9_data2[] = {
	MK(0,0,1,0),
	MK(-1,0,1,1),
	MK(0,0,1,0),
	MK(-1,0,1,2),
	0x81,
};

static const byte _effecttick9_data3[] = {
	MK(0,0,1,0),
	MK(0,1,1,1),
	MK(0,0,1,0),
	MK(0,1,1,2),
	0x81,
};

static const byte _effecttick9_data4[] = {
	MK(0,0,1,0),
	MK(0,-1,1,1),
	MK(0,0,1,0),
	MK(0,-1,1,2),
	0x81,
};

static const byte _effecttick9_data5[] = {
	MK(0,0,1,2),
	MK(0,0,1,7),
	MK(0,0,1,8),
	MK(0,0,1,9),
	0x80,
};

static const byte _effecttick9_data6[] = {
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(2,1,3,0),
	MK(1,1,3,1),
	MK(2,1,3,0),
	MK(1,1,3,2),
	MK(2,1,3,0),
	MK(1,1,3,1),
	MK(2,1,3,0),
	MK(1,0,1,2),
	MK(0,0,1,0),
	MK(1,0,1,1),
	MK(0,0,1,0),
	MK(1,0,1,2),
	MK(0,0,1,0),
	MK(1,0,1,1),
	MK(0,0,1,0),
	MK(1,0,1,2),
	0x82,0,
	MK(0,0,0,0xA),
	MK(0,0,0,0xB),
	MK(0,0,0,0xC),
	MK(0,0,0,0xD),
	MK(0,0,0,0xE),
	0x80
};
#undef MK

static const byte * const _effecttick9_data[6] = {
	_effecttick9_data1,
	_effecttick9_data2,
	_effecttick9_data3,
	_effecttick9_data4,
	_effecttick9_data5,
	_effecttick9_data6,
};

static void EffectTick_9(Vehicle *v)
{
	int et;
	const byte *b;

	if (((++v->progress)&3) != 0)
		return;

	BeginVehicleMove(v);

	et = v->engine_type + 1;

	if (v->spritenum == 0) {
		if (++v->cur_image < 4754) {
			VehiclePositionChanged(v);
			EndVehicleMove(v);
			return;
		}
		if (v->u.special.unk2 != 0) {
			v->spritenum = (byte)((Random()&3)+1);
		} else {
			v->spritenum = 6;
		}
		et = 0;
	}

again:
	v->engine_type = et;
	b = &_effecttick9_data[v->spritenum - 1][et*2];

	if (*b == 0x80) {
		EndVehicleMove(v);
		DeleteVehicle(v);
		return;
	}

	if (*b == 0x81) {
		if (v->z_pos > 180 || CHANCE16(1,96)) {
			v->spritenum = 5;
			SndPlayVehicleFx(0x2F, v);
		}
		et = 0;
		goto again;
	}

	if (*b == 0x82) {
		uint tile;

		et++;
		SndPlayVehicleFx(0x31, v);

		tile = TILE_FROM_XY(v->x_pos, v->y_pos);
		if (IS_TILETYPE(tile, MP_INDUSTRY) &&
				_map5[tile]==0xA2) {
			AddAnimatedTile(tile);
		}
		goto again;
	}

	v->x_pos += (b[0]&0xF) - 4;
	v->y_pos += (b[0]>>4)  - 4;
	v->z_pos += (b[1]&0xF) - 4;
	v->cur_image = 4748 + (b[1] >> 4);

	VehiclePositionChanged(v);
	EndVehicleMove(v);
}


typedef void EffectInitProc(Vehicle *v);
typedef void EffectTickProc(Vehicle *v);

static EffectInitProc * const _effect_init_procs[] = {
	EffectInit_0,
	EffectInit_1,
	EffectInit_2,
	EffectInit_3,
	EffectInit_4,
	EffectInit_5,
	EffectInit_6,
	EffectInit_7,
	EffectInit_8,
	EffectInit_9,
};

static EffectTickProc * const _effect_tick_procs[] = {
	EffectTick_0,
	EffectTick_1,
	EffectTick_2,
	EffectTick_3,
	EffectTick_4,
	EffectTick_5,
	EffectTick_6,
	EffectTick_7,
	EffectTick_8,
	EffectTick_9,
};


Vehicle *CreateEffectVehicle(int x, int y, int z, int type)
{
	Vehicle *v;

	v = ForceAllocateSpecialVehicle();
	if (v != NULL) {
		v->type = VEH_Special;
		v->subtype = type;
		v->x_pos = x;
		v->y_pos = y;
		v->z_pos = z;
		v->z_height = v->sprite_width = v->sprite_height = 1;
		v->x_offs = v->y_offs = 0;
		v->tile = 0;
		v->vehstatus = VS_UNCLICKABLE;

		_effect_init_procs[type](v);

		VehiclePositionChanged(v);
		BeginVehicleMove(v);
		EndVehicleMove(v);
	}
	return v;
}

Vehicle *CreateEffectVehicleAbove(int x, int y, int z, int type)
{
	return CreateEffectVehicle(x, y, GetSlopeZ(x, y) + z, type);
}

Vehicle *CreateEffectVehicleRel(Vehicle *v, int x, int y, int z, int type)
{
	return CreateEffectVehicle(v->x_pos + x, v->y_pos + y, v->z_pos + z, type);
}

void EffectVehicle_Tick(Vehicle *v)
{
	_effect_tick_procs[v->subtype](v);
}

Vehicle *CheckClickOnVehicle(ViewPort *vp, int x, int y)
{
	Vehicle *found = NULL, *v;
	uint dist, best_dist = (uint)-1;

	if ( (uint)(x -= vp->left) >= (uint)vp->width ||
			 (uint)(y -= vp->top) >= (uint)vp->height)
				return NULL;

	x = (x << vp->zoom) + vp->virtual_left;
	y = (y << vp->zoom) + vp->virtual_top;

	FOR_ALL_VEHICLES(v) {
		if (v->type != 0 && (v->vehstatus & (VS_HIDDEN|VS_UNCLICKABLE)) == 0 &&
				x >= v->left_coord && x <= v->right_coord &&
				y >= v->top_coord && y <= v->bottom_coord) {

			dist = max(
				myabs( ((v->left_coord + v->right_coord)>>1) - x ),
				myabs( ((v->top_coord + v->bottom_coord)>>1) - y )
			);

			if (dist < best_dist) {
				found = v;
				best_dist = dist;
			}
		}
	}

	return found;
}


void DecreaseVehicleValue(Vehicle *v)
{
	v->value -= v->value >> 8;
	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
}

static const byte _breakdown_chance[64] = {
	3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 5, 5, 6, 6, 7, 7,
	8, 8, 9, 9, 10, 10, 11, 11,
	12, 13, 13, 13, 13, 14, 15, 16,
	17, 19, 21, 25, 28, 31, 34, 37,
	40, 44, 48, 52, 56, 60, 64, 68,
	72, 80, 90, 100, 110, 120, 130, 140,
	150, 170, 190, 210, 230, 250, 250, 250,
};

void CheckVehicleBreakdown(Vehicle *v)
{
	int rel, rel_old;
	uint32 r;
	int chance;

	/* decrease reliability */
	v->reliability = rel = max((rel_old = v->reliability) - v->reliability_spd_dec, 0);
	if ((rel_old >> 8) != (rel >> 8))
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	if (v->breakdown_ctr != 0 || (v->vehstatus & VS_STOPPED) != 0 ||
			v->cur_speed < 5 || _game_mode == GM_MENU)
				return;

	r = Random();

	/* increase chance of failure */
	chance = v->breakdown_chance + 1;
	if (CHANCE16I(1,25,r)) chance += 25;
	v->breakdown_chance = min(255, chance);

	/* calculate reliability value to use in comparison */
	rel = v->reliability;
	if (v->type == VEH_Ship) rel += 0x6666;

	/* disabled breakdowns? */
	if (_opt.diff.vehicle_breakdowns < 1)
		return;

	/* reduced breakdowns? */
	if (_opt.diff.vehicle_breakdowns == 1) rel += 0x6666;

	/* check if to break down */
	if (_breakdown_chance[(uint)min(rel, 0xffff) >> 10] <= v->breakdown_chance) {
		v->breakdown_ctr = (byte)(((r >> 16) & 0x3F) + 0x3F);
		v->breakdown_delay = (byte)(((r >> 24) & 0x7F) | 0x80);
		v->breakdown_chance = 0;
	}
}

static const StringID _vehicle_type_names[4] = {
	STR_019F_TRAIN,
	STR_019C_ROAD_VEHICLE,
	STR_019E_SHIP,
	STR_019D_AIRCRAFT,
};

static void ShowVehicleGettingOld(Vehicle *v, StringID msg)
{
	if (v->owner != _local_player)
		return;
	// Do not show getting-old message if autorenew is active
	if (_patches.autorenew)
		return;

	SET_DPARAM16(0, _vehicle_type_names[v->type - 0x10]);
	SET_DPARAM16(1, v->unitnumber);
	AddNewsItem(msg, NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0), v->index, 0);
}

void AgeVehicle(Vehicle *v)
{
	int age;

	if (v->age < 65535)
		v->age++;

	age = v->age - v->max_age;
	if (age == 366*0 || age == 366*1 || age == 366*2 || age == 366*3 || age == 366*4)
		v->reliability_spd_dec <<= 1;

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	if (age == -366) {
		ShowVehicleGettingOld(v, STR_01A0_IS_GETTING_OLD);
	} else if (age == 0) {
		ShowVehicleGettingOld(v, STR_01A1_IS_GETTING_VERY_OLD);
	} else if (age == 366*1 || age == 366*2 || age == 366*3 || age == 366*4 || age == 366*5) {
		ShowVehicleGettingOld(v, STR_01A2_IS_GETTING_VERY_OLD_AND);
	}
}

void MaybeRenewVehicle(Vehicle *v, int32 build_cost)
{
	Engine *e;

	// A vehicle is autorenewed when it it gets the amount of months
	//  give by _patches.autorenew_months away for his max age.
	//  Standard is -6, meaning 6 months before his max age
	//  It can be any value between -12 and 12.
	if (!_patches.autorenew || v->age - v->max_age < (_patches.autorenew_months * 30))
		return;

	if (DEREF_PLAYER(v->owner)->money64 < _patches.autorenew_money + build_cost - v->value) {
		if (v->owner == _local_player) {
			int message;
			SET_DPARAM16(0, v->unitnumber);
			switch (v->type) {
				case VEH_Train: message = STR_TRAIN_AUTORENEW_FAILED; break;
				case VEH_Road: message = STR_ROADVEHICLE_AUTORENEW_FAILED; break;
				case VEH_Ship: message = STR_SHIP_AUTORENEW_FAILED; break;
				case VEH_Aircraft: message = STR_AIRCRAFT_AUTORENEW_FAILED; break;
				// This should never happen
				default: message = 0; break;
			}

			AddNewsItem(message, NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0), v->index, 0);
		}
		return;
	}

	// Withdraw the money from the right player ;)
	_current_player = v->owner;

	e = &_engines[v->engine_type];
	v->reliability = e->reliability;
	v->reliability_spd_dec = e->reliability_spd_dec;
	v->age = 0;

	v->date_of_last_service = _date;
	v->build_year = _cur_year;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);
	SubtractMoneyFromPlayer(build_cost - v->value);
	v->value = build_cost;

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	_current_player = OWNER_NONE;
}


int32 CmdNameVehicle(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	StringID str;

	v = &_vehicles[p1];

	if (!CheckOwnership(v->owner))
		return CMD_ERROR;

	str = AllocateName((byte*)_decode_parameters, 2);
	if (str == 0)
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		StringID old_str = v->string_id;
		v->string_id = str;
		DeleteName(old_str);
		_train_sort_dirty[v->owner]			= true;
		_aircraft_sort_dirty[v->owner]	= true;
		_ship_sort_dirty[v->owner]			= true;
		_road_sort_dirty[v->owner]			= true;
		MarkWholeScreenDirty();
	} else {
		DeleteName(str);
	}

	return 0;
}



static Rect _old_vehicle_coords;

void BeginVehicleMove(Vehicle *v) {
	_old_vehicle_coords.left = v->left_coord;
	_old_vehicle_coords.top = v->top_coord;
	_old_vehicle_coords.right = v->right_coord;
	_old_vehicle_coords.bottom = v->bottom_coord;
}

void EndVehicleMove(Vehicle *v)
{
	MarkAllViewportsDirty(
		min(_old_vehicle_coords.left,v->left_coord),
		min(_old_vehicle_coords.top,v->top_coord),
		max(_old_vehicle_coords.right,v->right_coord)+1,
		max(_old_vehicle_coords.bottom,v->bottom_coord)+1
	);
}

void InvalidateVehicleOrderWidget(Vehicle *v)
{
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
	InvalidateWindowWidget(WC_VEHICLE_ORDERS, v->index, 2);
}

/* returns true if staying in the same tile */
bool GetNewVehiclePos(Vehicle *v, GetNewVehiclePosResult *gp)
{
	static const int8 _delta_coord[16] = {
		-1,-1,-1, 0, 1, 1, 1, 0, /* x */
		-1, 0, 1, 1, 1, 0,-1,-1, /* y */
	};

	int x = v->x_pos + _delta_coord[v->direction];
	int y = v->y_pos + _delta_coord[v->direction + 8];

	gp->x = x;
	gp->y = y;
	gp->old_tile = v->tile;
	gp->new_tile = TILE_FROM_XY(x,y);
	return gp->old_tile == gp->new_tile;
}

static const byte _new_direction_table[9] = {
	0, 7, 6,
	1, 3, 5,
	2, 3, 4,
};

byte GetDirectionTowards(Vehicle *v, int x, int y)
{
	byte dirdiff, dir;
	int i = 0;

	if (y >= v->y_pos) {
		if (y != v->y_pos) i+=3;
		i+=3;
	}

	if (x >= v->x_pos) {
		if (x != v->x_pos) i++;
		i++;
	}

	dir = v->direction;

	dirdiff = _new_direction_table[i] - dir;
	if (dirdiff == 0)
		return dir;
	return (dir+((dirdiff&7)<5?1:-1)) & 7;
}

/* Return value has bit 0x2 set, when the vehicle enters a station. Then,
 * result << 8 contains the id of the station entered. If the return value has
 * bit 0x8 set, the vehicle could not and did not enter the tile. Are there
 * other bits that can be set? */
uint32 VehicleEnterTile(Vehicle *v, uint tile, int x, int y)
{
	uint old_tile = v->tile;
	uint32 result = _tile_type_procs[GET_TILETYPE(tile)]->vehicle_enter_tile_proc(v, tile, x, y);

	/* When vehicle_enter_tile_proc returns 8, that apparently means that
	 * we cannot enter the tile at all. In that case, don't call
	 * leave_tile. */
	if (!(result & 8) && old_tile != tile) {
		VehicleLeaveTileProc *proc = _tile_type_procs[GET_TILETYPE(old_tile)]->vehicle_leave_tile_proc;
		if (proc != NULL)
			proc(v, old_tile, x, y);
	}
	return result;
}

uint GetFreeUnitNumber(byte type)
{
	uint unit_num = 0;
	Vehicle *u;

restart:
	unit_num++;
	FOR_ALL_VEHICLES(u) {
		if (u->type == type && u->owner == _current_player &&
		    unit_num == u->unitnumber)
					goto restart;
	}
	return unit_num;
}


// Save and load of vehicles
const byte _common_veh_desc[] = {
	SLE_VAR(Vehicle,subtype,					SLE_UINT8),

	SLE_VAR(Vehicle,next_in_chain_old, SLE_UINT16),
	SLE_VAR(Vehicle,string_id,				SLE_STRINGID),
	SLE_VAR(Vehicle,unitnumber,				SLE_UINT8),
	SLE_VAR(Vehicle,owner,						SLE_UINT8),
	SLE_VAR(Vehicle,tile,							SLE_UINT16),
	SLE_VAR(Vehicle,dest_tile,				SLE_UINT16),

	SLE_VAR(Vehicle,x_pos,						SLE_INT16),
	SLE_VAR(Vehicle,y_pos,						SLE_INT16),
	SLE_VAR(Vehicle,z_pos,						SLE_UINT8),
	SLE_VAR(Vehicle,direction,				SLE_UINT8),

	SLE_VAR(Vehicle,cur_image,				SLE_UINT16),
	SLE_VAR(Vehicle,spritenum,				SLE_UINT8),
	SLE_VAR(Vehicle,sprite_width,			SLE_UINT8),
	SLE_VAR(Vehicle,sprite_height,		SLE_UINT8),
	SLE_VAR(Vehicle,z_height,					SLE_UINT8),
	SLE_VAR(Vehicle,x_offs,						SLE_INT8),
	SLE_VAR(Vehicle,y_offs,						SLE_INT8),
	SLE_VAR(Vehicle,engine_type,			SLE_UINT16),
	SLE_VAR(Vehicle,max_speed,				SLE_UINT16),
	SLE_VAR(Vehicle,cur_speed,				SLE_UINT16),
	SLE_VAR(Vehicle,subspeed,					SLE_UINT8),
	SLE_VAR(Vehicle,acceleration,			SLE_UINT8),
	SLE_VAR(Vehicle,progress,					SLE_UINT8),

	SLE_VAR(Vehicle,vehstatus,				SLE_UINT8),
	SLE_VAR(Vehicle,last_station_visited,SLE_UINT8),

	SLE_VAR(Vehicle,cargo_type,				SLE_UINT8),
	SLE_VAR(Vehicle,cargo_days,				SLE_UINT8),
	SLE_VAR(Vehicle,cargo_source,			SLE_UINT8),
	SLE_VAR(Vehicle,cargo_cap,				SLE_UINT16),
	SLE_VAR(Vehicle,cargo_count,			SLE_UINT16),

	SLE_VAR(Vehicle,day_counter,			SLE_UINT8),
	SLE_VAR(Vehicle,tick_counter,			SLE_UINT8),

	SLE_VAR(Vehicle,cur_order_index,	SLE_UINT8),
	SLE_VAR(Vehicle,num_orders,				SLE_UINT8),
	SLE_VAR(Vehicle,next_order,				SLE_UINT8),
	SLE_VAR(Vehicle,next_order_param,	SLE_UINT8),
	SLE_REF(Vehicle,schedule_ptr,			REF_SCHEDULE),

	SLE_VAR(Vehicle,age,							SLE_UINT16),
	SLE_VAR(Vehicle,max_age,					SLE_UINT16),
	SLE_VAR(Vehicle,date_of_last_service,SLE_UINT16),
	SLE_VAR(Vehicle,service_interval,	SLE_UINT16),
	SLE_VAR(Vehicle,reliability,			SLE_UINT16),
	SLE_VAR(Vehicle,reliability_spd_dec,SLE_UINT16),
	SLE_VAR(Vehicle,breakdown_ctr,		SLE_UINT8),
	SLE_VAR(Vehicle,breakdown_delay,	SLE_UINT8),
	SLE_VAR(Vehicle,breakdowns_since_last_service,	SLE_UINT8),
	SLE_VAR(Vehicle,breakdown_chance,	SLE_UINT8),
	SLE_VAR(Vehicle,build_year,				SLE_UINT8),

	SLE_VAR(Vehicle,load_unload_time_rem,	SLE_UINT16),

	SLE_VAR(Vehicle,profit_this_year,	SLE_INT32),
	SLE_VAR(Vehicle,profit_last_year,	SLE_INT32),
	SLE_VAR(Vehicle,value,						SLE_UINT32),

	// reserve extra space in savegame here. (currently 16 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U64 | SLE_VAR_NULL, 2, 2, 255),

	SLE_END()
};


static const byte _train_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_Train, 0), // Train type. VEH_Train in mem, 0 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRail,crash_anim_pos), SLE_UINT16),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRail,force_proceed), SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRail,railtype), SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRail,track), SLE_UINT8),

	SLE_CONDVARX(offsetof(Vehicle,u)+offsetof(VehicleRail,flags), SLE_UINT8, 2, 255),
	SLE_CONDVARX(offsetof(Vehicle,u)+offsetof(VehicleRail,days_since_order_progr), SLE_UINT16, 2, 255),

	// reserve extra space in savegame here. (currently 13 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U8 | SLE_VAR_NULL, 13, 2, 255),

	SLE_END()
};

static const byte _roadveh_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_Road, 1), // Road type. VEH_Road in mem, 1 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,state),					SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,frame),					SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,unk2),					SLE_UINT16),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,overtaking),		SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,overtaking_ctr),SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,crashed_ctr),		SLE_UINT16),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,reverse_ctr),			SLE_UINT8),

	// reserve extra space in savegame here. (currently 16 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U64 | SLE_VAR_NULL, 2, 2, 255),

	SLE_END()
};

static const byte _ship_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_Ship, 2), // Ship type. VEH_Ship in mem, 2 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleShip,state),				SLE_UINT8),

	// reserve extra space in savegame here. (currently 16 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U64 | SLE_VAR_NULL, 2, 2, 255),

	SLE_END()
};

static const byte _aircraft_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_Aircraft, 3), // Aircraft type. VEH_Aircraft in mem, 3 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleAir,crashed_counter),	SLE_UINT16),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleAir,pos),							SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleAir,targetairport),		SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleAir,state),						SLE_UINT8),

	SLE_CONDVARX(offsetof(Vehicle,u)+offsetof(VehicleAir,previous_pos),			SLE_UINT8, 2, 255),

	// reserve extra space in savegame here. (currently 15 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U8 | SLE_VAR_NULL, 15, 2, 255),

	SLE_END()
};

static const byte _special_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_Special, 4),

	SLE_VAR(Vehicle,subtype,					SLE_UINT8),

	SLE_VAR(Vehicle,tile,							SLE_UINT16),

	SLE_VAR(Vehicle,x_pos,						SLE_INT16),
	SLE_VAR(Vehicle,y_pos,						SLE_INT16),
	SLE_VAR(Vehicle,z_pos,						SLE_UINT8),

	SLE_VAR(Vehicle,cur_image,				SLE_UINT16),
	SLE_VAR(Vehicle,sprite_width,			SLE_UINT8),
	SLE_VAR(Vehicle,sprite_height,		SLE_UINT8),
	SLE_VAR(Vehicle,z_height,					SLE_UINT8),
	SLE_VAR(Vehicle,x_offs,						SLE_INT8),
	SLE_VAR(Vehicle,y_offs,						SLE_INT8),
	SLE_VAR(Vehicle,progress,					SLE_UINT8),
	SLE_VAR(Vehicle,vehstatus,				SLE_UINT8),

	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleSpecial,unk0),	SLE_UINT16),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleSpecial,unk2),	SLE_UINT8),

	// reserve extra space in savegame here. (currently 16 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U64 | SLE_VAR_NULL, 2, 2, 255),

	SLE_END()
};

static const byte _disaster_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_Disaster, 5),

	SLE_VAR(Vehicle,next_in_chain_old,SLE_UINT16),

	SLE_VAR(Vehicle,subtype,					SLE_UINT8),
	SLE_VAR(Vehicle,tile,							SLE_UINT16),
	SLE_VAR(Vehicle,dest_tile,				SLE_UINT16),

	SLE_VAR(Vehicle,x_pos,						SLE_INT16),
	SLE_VAR(Vehicle,y_pos,						SLE_INT16),
	SLE_VAR(Vehicle,z_pos,						SLE_UINT8),
	SLE_VAR(Vehicle,direction,				SLE_UINT8),

	SLE_VAR(Vehicle,x_offs,						SLE_INT8),
	SLE_VAR(Vehicle,y_offs,						SLE_INT8),
	SLE_VAR(Vehicle,sprite_width,			SLE_UINT8),
	SLE_VAR(Vehicle,sprite_height,		SLE_UINT8),
	SLE_VAR(Vehicle,z_height,					SLE_UINT8),
	SLE_VAR(Vehicle,owner,						SLE_UINT8),
	SLE_VAR(Vehicle,vehstatus,				SLE_UINT8),
	SLE_VAR(Vehicle,next_order,				SLE_UINT8),

	SLE_VAR(Vehicle,cur_image,				SLE_UINT16),
	SLE_VAR(Vehicle,age,							SLE_UINT16),

	SLE_VAR(Vehicle,tick_counter,			SLE_UINT8),

	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleDisaster,image_override),	SLE_UINT16),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleDisaster,unk2),						SLE_UINT16),

	// reserve extra space in savegame here. (currently 16 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U64 | SLE_VAR_NULL, 2, 2, 255),

	SLE_END()
};


static const void *_veh_descs[] = {
	_train_desc,
	_roadveh_desc,
	_ship_desc,
	_aircraft_desc,
	_special_desc,
	_disaster_desc,
};

// Will be called when the vehicles need to be saved.
static void Save_VEHS()
{
	Vehicle *v;
	// Write the vehicles
	FOR_ALL_VEHICLES(v) {
		if (v->type != 0) {
			SlSetArrayIndex(v->index);
			v->next_in_chain_old = v->next ? v->next->index : INVALID_VEHICLE;
			SlObject(v, _veh_descs[v->type - 0x10]);
		}
	}
}

// Will be called when vehicles need to be loaded.
static void Load_VEHS()
{
	int index;
	Vehicle *v;

	while ((index = SlIterateArray()) != -1) {
		Vehicle *v = &_vehicles[index];
		v->next_in_chain_old = INVALID_VEHICLE;
		SlObject(v, _veh_descs[SlReadByte()]);
		v->next = v->next_in_chain_old == INVALID_VEHICLE ? NULL : &_vehicles[v->next_in_chain_old];
		if (v->type == VEH_Train)
			v->u.rail.first_engine = 0xffff;
	}

	// Iterate through trains and set first_engine appropriately.
	FOR_ALL_VEHICLES(v) {
		Vehicle *w;

		if (v->type != VEH_Train || v->subtype != 0)
			continue;

		for (w = v->next; w; w = w->next)
			w->u.rail.first_engine = v->engine_type;
	}
}

static const byte _depot_desc[] = {
	SLE_VAR(Depot,xy,						SLE_UINT16),
	SLE_VAR(Depot,town_index,		SLE_UINT16),
	SLE_END()
};

static void Save_DEPT()
{
	Depot *d;
	int i;
	for(i=0,d=_depots; i!=lengthof(_depots); i++,d++) {
		if (d->xy != 0) {
			SlSetArrayIndex(i);
			SlObject(d, _depot_desc);
		}
	}
}

static void Load_DEPT()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		SlObject(&_depots[index], _depot_desc);
	}
}

static const byte _waypoint_desc[] = {
	SLE_VAR(Waypoint,xy,								SLE_UINT16),
	SLE_VAR(Waypoint,town_or_string,		SLE_UINT16),
	SLE_VAR(Waypoint,deleted,						SLE_UINT8),

	SLE_CONDVAR(Waypoint, build_date, SLE_UINT16, 3, 255),
	SLE_CONDVAR(Waypoint, stat_id, SLE_UINT8, 3, 255),

	SLE_END()
};

static void Save_CHKP()
{
	Waypoint *cp;
	int i;
	for(i=0,cp=_waypoints; i!=lengthof(_waypoints); i++,cp++) {
		if (cp->xy != 0) {
			SlSetArrayIndex(i);
			SlObject(cp, _waypoint_desc);
		}
	}
}

static void Load_CHKP()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		SlObject(&_waypoints[index], _waypoint_desc);
	}
}


static void Save_ORDR()
{
	uint len = _ptr_to_next_order - _order_array;
	SlArray(_order_array, len, SLE_UINT16);
}

static void Load_ORDR()
{
	uint len = SlGetFieldLength() >> 1;
	_ptr_to_next_order = _order_array + len;
	SlArray(_order_array, len, SLE_UINT16);
}

const ChunkHandler _veh_chunk_handlers[] = {
	{ 'VEHS', Save_VEHS, Load_VEHS, CH_SPARSE_ARRAY},
	{ 'ORDR', Save_ORDR, Load_ORDR, CH_RIFF},
	{ 'DEPT', Save_DEPT, Load_DEPT, CH_ARRAY},
	{ 'CHKP', Save_CHKP, Load_CHKP, CH_ARRAY | CH_LAST},
};


