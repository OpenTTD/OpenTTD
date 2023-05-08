/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file effectvehicle.cpp Implementation of everything generic to vehicles. */

#include "stdafx.h"
#include "landscape.h"
#include "core/random_func.hpp"
#include "industry_map.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "animated_tile_func.h"
#include "effectvehicle_func.h"
#include "effectvehicle_base.h"

#include "safeguards.h"


/**
 * Increment the sprite unless it has reached the end of the animation.
 * @param v Vehicle to increment sprite of.
 * @param last Last sprite of animation.
 * @return true if the sprite was incremented, false if the end was reached.
 */
static bool IncrementSprite(EffectVehicle *v, SpriteID last)
{
	if (v->sprite_cache.sprite_seq.seq[0].sprite != last) {
		v->sprite_cache.sprite_seq.seq[0].sprite++;
		return true;
	} else {
		return false;
	}
}

static void ChimneySmokeInit(EffectVehicle *v)
{
	uint32_t r = Random();
	v->sprite_cache.sprite_seq.Set(SPR_CHIMNEY_SMOKE_0 + GB(r, 0, 3));
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

		if (!IncrementSprite(v, SPR_CHIMNEY_SMOKE_7)) {
			v->sprite_cache.sprite_seq.Set(SPR_CHIMNEY_SMOKE_0);
		}
		v->progress = 7;
		v->UpdatePositionAndViewport();
	}

	return true;
}

static void SteamSmokeInit(EffectVehicle *v)
{
	v->sprite_cache.sprite_seq.Set(SPR_STEAM_SMOKE_0);
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
		if (!IncrementSprite(v, SPR_STEAM_SMOKE_4)) {
			delete v;
			return false;
		}
		moved = true;
	}

	if (moved) v->UpdatePositionAndViewport();

	return true;
}

static void DieselSmokeInit(EffectVehicle *v)
{
	v->sprite_cache.sprite_seq.Set(SPR_DIESEL_SMOKE_0);
	v->progress = 0;
}

static bool DieselSmokeTick(EffectVehicle *v)
{
	v->progress++;

	if ((v->progress & 3) == 0) {
		v->z_pos++;
		v->UpdatePositionAndViewport();
	} else if ((v->progress & 7) == 1) {
		if (!IncrementSprite(v, SPR_DIESEL_SMOKE_5)) {
			delete v;
			return false;
		}
		v->UpdatePositionAndViewport();
	}

	return true;
}

static void ElectricSparkInit(EffectVehicle *v)
{
	v->sprite_cache.sprite_seq.Set(SPR_ELECTRIC_SPARK_0);
	v->progress = 1;
}

static bool ElectricSparkTick(EffectVehicle *v)
{
	if (v->progress < 2) {
		v->progress++;
	} else {
		v->progress = 0;

		if (!IncrementSprite(v, SPR_ELECTRIC_SPARK_5)) {
			delete v;
			return false;
		}
		v->UpdatePositionAndViewport();
	}

	return true;
}

static void SmokeInit(EffectVehicle *v)
{
	v->sprite_cache.sprite_seq.Set(SPR_SMOKE_0);
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
		if (!IncrementSprite(v, SPR_SMOKE_4)) {
			delete v;
			return false;
		}
		moved = true;
	}

	if (moved) v->UpdatePositionAndViewport();

	return true;
}

static void ExplosionLargeInit(EffectVehicle *v)
{
	v->sprite_cache.sprite_seq.Set(SPR_EXPLOSION_LARGE_0);
	v->progress = 0;
}

static bool ExplosionLargeTick(EffectVehicle *v)
{
	v->progress++;
	if ((v->progress & 3) == 0) {
		if (!IncrementSprite(v, SPR_EXPLOSION_LARGE_F)) {
			delete v;
			return false;
		}
		v->UpdatePositionAndViewport();
	}

	return true;
}

static void BreakdownSmokeInit(EffectVehicle *v)
{
	v->sprite_cache.sprite_seq.Set(SPR_BREAKDOWN_SMOKE_0);
	v->progress = 0;
}

static bool BreakdownSmokeTick(EffectVehicle *v)
{
	v->progress++;
	if ((v->progress & 7) == 0) {
		if (!IncrementSprite(v, SPR_BREAKDOWN_SMOKE_3)) {
			v->sprite_cache.sprite_seq.Set(SPR_BREAKDOWN_SMOKE_0);
		}
		v->UpdatePositionAndViewport();
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
	v->sprite_cache.sprite_seq.Set(SPR_EXPLOSION_SMALL_0);
	v->progress = 0;
}

static bool ExplosionSmallTick(EffectVehicle *v)
{
	v->progress++;
	if ((v->progress & 3) == 0) {
		if (!IncrementSprite(v, SPR_EXPLOSION_SMALL_B)) {
			delete v;
			return false;
		}
		v->UpdatePositionAndViewport();
	}

	return true;
}

static void BulldozerInit(EffectVehicle *v)
{
	v->sprite_cache.sprite_seq.Set(SPR_BULLDOZER_NE);
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
	int8_t x;
	int8_t y;
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

		v->sprite_cache.sprite_seq.Set(SPR_BULLDOZER_NE + b->image);

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
		v->UpdatePositionAndViewport();
	}

	return true;
}

static void BubbleInit(EffectVehicle *v)
{
	v->sprite_cache.sprite_seq.Set(SPR_BUBBLE_GENERATE_0);
	v->spritenum = 0;
	v->progress = 0;
}

struct BubbleMovement {
	int8_t x:4;
	int8_t y:4;
	int8_t z:4;
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
		v->sprite_cache.sprite_seq.seq[0].sprite++;
		if (v->sprite_cache.sprite_seq.seq[0].sprite < SPR_BUBBLE_GENERATE_3) {
			v->UpdatePositionAndViewport();
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
			if (_settings_client.sound.ambient) SndPlayVehicleFx(SND_2F_BUBBLE_GENERATOR_FAIL, v);
		}
		anim_state = 0;
	}

	if (b->y == 4 && b->x == 2) {
		TileIndex tile;

		anim_state++;
		if (_settings_client.sound.ambient) SndPlayVehicleFx(SND_31_BUBBLE_GENERATOR_SUCCESS, v);

		tile = TileVirtXY(v->x_pos, v->y_pos);
		if (IsTileType(tile, MP_INDUSTRY) && GetIndustryGfx(tile) == GFX_BUBBLE_CATCHER) AddAnimatedTile(tile);
	}

	v->animation_state = anim_state;
	b = &_bubble_movement[v->spritenum - 1][anim_state];

	v->x_pos += b->x;
	v->y_pos += b->y;
	v->z_pos += b->z;
	v->sprite_cache.sprite_seq.Set(SPR_BUBBLE_0 + b->image);

	v->UpdatePositionAndViewport();

	return true;
}


typedef void EffectInitProc(EffectVehicle *v);
typedef bool EffectTickProc(EffectVehicle *v);

/** Functions to initialise an effect vehicle after construction. */
static EffectInitProc * const _effect_init_procs[] = {
	ChimneySmokeInit,   // EV_CHIMNEY_SMOKE
	SteamSmokeInit,     // EV_STEAM_SMOKE
	DieselSmokeInit,    // EV_DIESEL_SMOKE
	ElectricSparkInit,  // EV_ELECTRIC_SPARK
	SmokeInit,          // EV_CRASH_SMOKE
	ExplosionLargeInit, // EV_EXPLOSION_LARGE
	BreakdownSmokeInit, // EV_BREAKDOWN_SMOKE
	ExplosionSmallInit, // EV_EXPLOSION_SMALL
	BulldozerInit,      // EV_BULLDOZER
	BubbleInit,         // EV_BUBBLE
	SmokeInit,          // EV_BREAKDOWN_SMOKE_AIRCRAFT
	SmokeInit,          // EV_COPPER_MINE_SMOKE
};
static_assert(lengthof(_effect_init_procs) == EV_END);

/** Functions for controlling effect vehicles at each tick. */
static EffectTickProc * const _effect_tick_procs[] = {
	ChimneySmokeTick,   // EV_CHIMNEY_SMOKE
	SteamSmokeTick,     // EV_STEAM_SMOKE
	DieselSmokeTick,    // EV_DIESEL_SMOKE
	ElectricSparkTick,  // EV_ELECTRIC_SPARK
	SmokeTick,          // EV_CRASH_SMOKE
	ExplosionLargeTick, // EV_EXPLOSION_LARGE
	BreakdownSmokeTick, // EV_BREAKDOWN_SMOKE
	ExplosionSmallTick, // EV_EXPLOSION_SMALL
	BulldozerTick,      // EV_BULLDOZER
	BubbleTick,         // EV_BUBBLE
	SmokeTick,          // EV_BREAKDOWN_SMOKE_AIRCRAFT
	SmokeTick,          // EV_COPPER_MINE_SMOKE
};
static_assert(lengthof(_effect_tick_procs) == EV_END);

/** Transparency options affecting the effects. */
static const TransparencyOption _effect_transparency_options[] = {
	TO_INDUSTRIES,      // EV_CHIMNEY_SMOKE
	TO_INVALID,         // EV_STEAM_SMOKE
	TO_INVALID,         // EV_DIESEL_SMOKE
	TO_INVALID,         // EV_ELECTRIC_SPARK
	TO_INVALID,         // EV_CRASH_SMOKE
	TO_INVALID,         // EV_EXPLOSION_LARGE
	TO_INVALID,         // EV_BREAKDOWN_SMOKE
	TO_INVALID,         // EV_EXPLOSION_SMALL
	TO_INVALID,         // EV_BULLDOZER
	TO_INDUSTRIES,      // EV_BUBBLE
	TO_INVALID,         // EV_BREAKDOWN_SMOKE_AIRCRAFT
	TO_INDUSTRIES,      // EV_COPPER_MINE_SMOKE
};
static_assert(lengthof(_effect_transparency_options) == EV_END);


/**
 * Create an effect vehicle at a particular location.
 * @param x The x location on the map.
 * @param y The y location on the map.
 * @param z The z location on the map.
 * @param type The type of effect vehicle.
 * @return The effect vehicle.
 */
EffectVehicle *CreateEffectVehicle(int x, int y, int z, EffectVehicleType type)
{
	if (!Vehicle::CanAllocateItem()) return nullptr;

	EffectVehicle *v = new EffectVehicle();
	v->subtype = type;
	v->x_pos = x;
	v->y_pos = y;
	v->z_pos = z;
	v->tile = 0;
	v->UpdateDeltaXY();
	v->vehstatus = VS_UNCLICKABLE;

	_effect_init_procs[type](v);

	v->UpdatePositionAndViewport();

	return v;
}

/**
 * Create an effect vehicle above a particular location.
 * @param x The x location on the map.
 * @param y The y location on the map.
 * @param z The offset from the ground.
 * @param type The type of effect vehicle.
 * @return The effect vehicle.
 */
EffectVehicle *CreateEffectVehicleAbove(int x, int y, int z, EffectVehicleType type)
{
	int safe_x = Clamp(x, 0, Map::MaxX() * TILE_SIZE);
	int safe_y = Clamp(y, 0, Map::MaxY() * TILE_SIZE);
	return CreateEffectVehicle(x, y, GetSlopePixelZ(safe_x, safe_y) + z, type);
}

/**
 * Create an effect vehicle above a particular vehicle.
 * @param v The vehicle to base the position on.
 * @param x The x offset to the vehicle.
 * @param y The y offset to the vehicle.
 * @param z The z offset to the vehicle.
 * @param type The type of effect vehicle.
 * @return The effect vehicle.
 */
EffectVehicle *CreateEffectVehicleRel(const Vehicle *v, int x, int y, int z, EffectVehicleType type)
{
	return CreateEffectVehicle(v->x_pos + x, v->y_pos + y, v->z_pos + z, type);
}

bool EffectVehicle::Tick()
{
	return _effect_tick_procs[this->subtype](this);
}

void EffectVehicle::UpdateDeltaXY()
{
	this->x_offs        = 0;
	this->y_offs        = 0;
	this->x_extent      = 1;
	this->y_extent      = 1;
	this->z_extent      = 1;
}

/**
 * Determines the transparency option affecting the effect.
 * @return Transparency option, or TO_INVALID if none.
 */
TransparencyOption EffectVehicle::GetTransparencyOption() const
{
	return _effect_transparency_options[this->subtype];
}
