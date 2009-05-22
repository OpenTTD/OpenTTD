/* $Id$ */

/** @file effectvehicle.cpp Implementation of everything generic to vehicles. */

#include "stdafx.h"
#include "landscape.h"
#include "industry_map.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "animated_tile_func.h"
#include "effectvehicle_base.h"
#include "effectvehicle_func.h"

#include "table/sprites.h"

static void ChimneySmokeInit(EffectVehicle *v)
{
	uint32 r = Random();
	v->cur_image = SPR_CHIMNEY_SMOKE_0 + GB(r, 0, 3);
	v->progress = GB(r, 16, 3);
}

static bool ChimneySmokeTick(EffectVehicle *v)
{
	if (v->progress > 0) {
		v->progress--;
	} else {
		TileIndex tile = TileVirtXY(v->x_pos, v->y_pos);
		if (!IsTileType(tile, MP_INDUSTRY)) {
			delete v;
			return false;
		}

		if (v->cur_image != SPR_CHIMNEY_SMOKE_7) {
			v->cur_image++;
		} else {
			v->cur_image = SPR_CHIMNEY_SMOKE_0;
		}
		v->progress = 7;
		VehicleMove(v, true);
	}

	return true;
}

static void SteamSmokeInit(EffectVehicle *v)
{
	v->cur_image = SPR_STEAM_SMOKE_0;
	v->progress = 12;
}

static bool SteamSmokeTick(EffectVehicle *v)
{
	bool moved = false;

	v->progress++;

	if ((v->progress & 7) == 0) {
		v->z_pos++;
		moved = true;
	}

	if ((v->progress & 0xF) == 4) {
		if (v->cur_image != SPR_STEAM_SMOKE_4) {
			v->cur_image++;
		} else {
			delete v;
			return false;
		}
		moved = true;
	}

	if (moved) VehicleMove(v, true);

	return true;
}

static void DieselSmokeInit(EffectVehicle *v)
{
	v->cur_image = SPR_DIESEL_SMOKE_0;
	v->progress = 0;
}

static bool DieselSmokeTick(EffectVehicle *v)
{
	v->progress++;

	if ((v->progress & 3) == 0) {
		v->z_pos++;
		VehicleMove(v, true);
	} else if ((v->progress & 7) == 1) {
		if (v->cur_image != SPR_DIESEL_SMOKE_5) {
			v->cur_image++;
			VehicleMove(v, true);
		} else {
			delete v;
			return false;
		}
	}

	return true;
}

static void ElectricSparkInit(EffectVehicle *v)
{
	v->cur_image = SPR_ELECTRIC_SPARK_0;
	v->progress = 1;
}

static bool ElectricSparkTick(EffectVehicle *v)
{
	if (v->progress < 2) {
		v->progress++;
	} else {
		v->progress = 0;
		if (v->cur_image != SPR_ELECTRIC_SPARK_5) {
			v->cur_image++;
			VehicleMove(v, true);
		} else {
			delete v;
			return false;
		}
	}

	return true;
}

static void SmokeInit(EffectVehicle *v)
{
	v->cur_image = SPR_SMOKE_0;
	v->progress = 12;
}

static bool SmokeTick(EffectVehicle *v)
{
	bool moved = false;

	v->progress++;

	if ((v->progress & 3) == 0) {
		v->z_pos++;
		moved = true;
	}

	if ((v->progress & 0xF) == 4) {
		if (v->cur_image != SPR_SMOKE_4) {
			v->cur_image++;
		} else {
			delete v;
			return false;
		}
		moved = true;
	}

	if (moved) VehicleMove(v, true);

	return true;
}

static void ExplosionLargeInit(EffectVehicle *v)
{
	v->cur_image = SPR_EXPLOSION_LARGE_0;
	v->progress = 0;
}

static bool ExplosionLargeTick(EffectVehicle *v)
{
	v->progress++;
	if ((v->progress & 3) == 0) {
		if (v->cur_image != SPR_EXPLOSION_LARGE_F) {
			v->cur_image++;
			VehicleMove(v, true);
		} else {
			delete v;
			return false;
		}
	}

	return true;
}

static void BreakdownSmokeInit(EffectVehicle *v)
{
	v->cur_image = SPR_BREAKDOWN_SMOKE_0;
	v->progress = 0;
}

static bool BreakdownSmokeTick(EffectVehicle *v)
{
	v->progress++;
	if ((v->progress & 7) == 0) {
		if (v->cur_image != SPR_BREAKDOWN_SMOKE_3) {
			v->cur_image++;
		} else {
			v->cur_image = SPR_BREAKDOWN_SMOKE_0;
		}
		VehicleMove(v, true);
	}

	v->animation_state--;
	if (v->animation_state == 0) {
		delete v;
		return false;
	}

	return true;
}

static void ExplosionSmallInit(EffectVehicle *v)
{
	v->cur_image = SPR_EXPLOSION_SMALL_0;
	v->progress = 0;
}

static bool ExplosionSmallTick(EffectVehicle *v)
{
	v->progress++;
	if ((v->progress & 3) == 0) {
		if (v->cur_image != SPR_EXPLOSION_SMALL_B) {
			v->cur_image++;
			VehicleMove(v, true);
		} else {
			delete v;
			return false;
		}
	}

	return true;
}

static void BulldozerInit(EffectVehicle *v)
{
	v->cur_image = SPR_BULLDOZER_NE;
	v->progress = 0;
	v->animation_state = 0;
	v->animation_substate = 0;
}

struct BulldozerMovement {
	byte direction:2;
	byte image:2;
	byte duration:3;
};

static const BulldozerMovement _bulldozer_movement[] = {
	{ 0, 0, 4 },
	{ 3, 3, 4 },
	{ 2, 2, 7 },
	{ 0, 2, 7 },
	{ 1, 1, 3 },
	{ 2, 2, 7 },
	{ 0, 2, 7 },
	{ 1, 1, 3 },
	{ 2, 2, 7 },
	{ 0, 2, 7 },
	{ 3, 3, 6 },
	{ 2, 2, 6 },
	{ 1, 1, 7 },
	{ 3, 1, 7 },
	{ 0, 0, 3 },
	{ 1, 1, 7 },
	{ 3, 1, 7 },
	{ 0, 0, 3 },
	{ 1, 1, 7 },
	{ 3, 1, 7 }
};

static const struct {
	int8 x;
	int8 y;
} _inc_by_dir[] = {
	{ -1,  0 },
	{  0,  1 },
	{  1,  0 },
	{  0, -1 }
};

static bool BulldozerTick(EffectVehicle *v)
{
	v->progress++;
	if ((v->progress & 7) == 0) {
		const BulldozerMovement *b = &_bulldozer_movement[v->animation_state];

		v->cur_image = SPR_BULLDOZER_NE + b->image;

		v->x_pos += _inc_by_dir[b->direction].x;
		v->y_pos += _inc_by_dir[b->direction].y;

		v->animation_substate++;
		if (v->animation_substate >= b->duration) {
			v->animation_substate = 0;
			v->animation_state++;
			if (v->animation_state == lengthof(_bulldozer_movement)) {
				delete v;
				return false;
			}
		}
		VehicleMove(v, true);
	}

	return true;
}

static void BubbleInit(EffectVehicle *v)
{
	v->cur_image = SPR_BUBBLE_GENERATE_0;
	v->spritenum = 0;
	v->progress = 0;
}

struct BubbleMovement {
	int8 x:4;
	int8 y:4;
	int8 z:4;
	byte image:4;
};

#define MK(x, y, z, i) { x, y, z, i }
#define ME(i) { i, 4, 0, 0 }

static const BubbleMovement _bubble_float_sw[] = {
	MK(0, 0, 1, 0),
	MK(1, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(1, 0, 1, 2),
	ME(1)
};


static const BubbleMovement _bubble_float_ne[] = {
	MK( 0, 0, 1, 0),
	MK(-1, 0, 1, 1),
	MK( 0, 0, 1, 0),
	MK(-1, 0, 1, 2),
	ME(1)
};

static const BubbleMovement _bubble_float_se[] = {
	MK(0, 0, 1, 0),
	MK(0, 1, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 1, 1, 2),
	ME(1)
};

static const BubbleMovement _bubble_float_nw[] = {
	MK(0,  0, 1, 0),
	MK(0, -1, 1, 1),
	MK(0,  0, 1, 0),
	MK(0, -1, 1, 2),
	ME(1)
};

static const BubbleMovement _bubble_burst[] = {
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 7),
	MK(0, 0, 1, 8),
	MK(0, 0, 1, 9),
	ME(0)
};

static const BubbleMovement _bubble_absorb[] = {
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(2, 1, 3, 0),
	MK(1, 1, 3, 1),
	MK(2, 1, 3, 0),
	MK(1, 1, 3, 2),
	MK(2, 1, 3, 0),
	MK(1, 1, 3, 1),
	MK(2, 1, 3, 0),
	MK(1, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(1, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(1, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(1, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(1, 0, 1, 2),
	ME(2),
	MK(0, 0, 0, 0xA),
	MK(0, 0, 0, 0xB),
	MK(0, 0, 0, 0xC),
	MK(0, 0, 0, 0xD),
	MK(0, 0, 0, 0xE),
	ME(0)
};
#undef ME
#undef MK

static const BubbleMovement * const _bubble_movement[] = {
	_bubble_float_sw,
	_bubble_float_ne,
	_bubble_float_se,
	_bubble_float_nw,
	_bubble_burst,
	_bubble_absorb,
};

static bool BubbleTick(EffectVehicle *v)
{
	uint anim_state;

	v->progress++;
	if ((v->progress & 3) != 0) return true;

	if (v->spritenum == 0) {
		v->cur_image++;
		if (v->cur_image < SPR_BUBBLE_GENERATE_3) {
			VehicleMove(v, true);
			return true;
		}
		if (v->animation_substate != 0) {
			v->spritenum = GB(Random(), 0, 2) + 1;
		} else {
			v->spritenum = 6;
		}
		anim_state = 0;
	} else {
		anim_state = v->animation_state + 1;
	}

	const BubbleMovement *b = &_bubble_movement[v->spritenum - 1][anim_state];

	if (b->y == 4 && b->x == 0) {
		delete v;
		return false;
	}

	if (b->y == 4 && b->x == 1) {
		if (v->z_pos > 180 || Chance16I(1, 96, Random())) {
			v->spritenum = 5;
			SndPlayVehicleFx(SND_2F_POP, v);
		}
		anim_state = 0;
	}

	if (b->y == 4 && b->x == 2) {
		TileIndex tile;

		anim_state++;
		SndPlayVehicleFx(SND_31_EXTRACT, v);

		tile = TileVirtXY(v->x_pos, v->y_pos);
		if (IsTileType(tile, MP_INDUSTRY) && GetIndustryGfx(tile) == GFX_BUBBLE_CATCHER) AddAnimatedTile(tile);
	}

	v->animation_state = anim_state;
	b = &_bubble_movement[v->spritenum - 1][anim_state];

	v->x_pos += b->x;
	v->y_pos += b->y;
	v->z_pos += b->z;
	v->cur_image = SPR_BUBBLE_0 + b->image;

	VehicleMove(v, true);

	return true;
}


typedef void EffectInitProc(EffectVehicle *v);
typedef bool EffectTickProc(EffectVehicle *v);

static EffectInitProc * const _effect_init_procs[] = {
	ChimneySmokeInit,
	SteamSmokeInit,
	DieselSmokeInit,
	ElectricSparkInit,
	SmokeInit,
	ExplosionLargeInit,
	BreakdownSmokeInit,
	ExplosionSmallInit,
	BulldozerInit,
	BubbleInit,
};

static EffectTickProc * const _effect_tick_procs[] = {
	ChimneySmokeTick,
	SteamSmokeTick,
	DieselSmokeTick,
	ElectricSparkTick,
	SmokeTick,
	ExplosionLargeTick,
	BreakdownSmokeTick,
	ExplosionSmallTick,
	BulldozerTick,
	BubbleTick,
};


EffectVehicle *CreateEffectVehicle(int x, int y, int z, EffectVehicleType type)
{
	if (!Vehicle::CanAllocateItem()) return NULL;

	EffectVehicle *v = new EffectVehicle();
	v->subtype = type;
	v->x_pos = x;
	v->y_pos = y;
	v->z_pos = z;
	v->tile = 0;
	v->UpdateDeltaXY(INVALID_DIR);
	v->vehstatus = VS_UNCLICKABLE;

	_effect_init_procs[type](v);

	VehicleMove(v, false);
	MarkSingleVehicleDirty(v);

	return v;
}

EffectVehicle *CreateEffectVehicleAbove(int x, int y, int z, EffectVehicleType type)
{
	int safe_x = Clamp(x, 0, MapMaxX() * TILE_SIZE);
	int safe_y = Clamp(y, 0, MapMaxY() * TILE_SIZE);
	return CreateEffectVehicle(x, y, GetSlopeZ(safe_x, safe_y) + z, type);
}

EffectVehicle *CreateEffectVehicleRel(const Vehicle *v, int x, int y, int z, EffectVehicleType type)
{
	return CreateEffectVehicle(v->x_pos + x, v->y_pos + y, v->z_pos + z, type);
}

bool EffectVehicle::Tick()
{
	return _effect_tick_procs[this->subtype](this);
}

void EffectVehicle::UpdateDeltaXY(Direction direction)
{
	this->x_offs        = 0;
	this->y_offs        = 0;
	this->x_extent      = 1;
	this->y_extent      = 1;
	this->z_extent      = 1;
}
