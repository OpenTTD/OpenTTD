/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file disaster_vehicle.cpp
 *
 * All disaster/easter egg vehicles are handled here.
 * The general flow of control for the disaster vehicles is as follows:
 * <ol>
 * <li>Initialize the disaster in a disaster specific way (eg start position,
 *     possible target, etc.) Disaster_XXX_Init() function
 * <li>Add a subtype to a disaster, which is an index into the function array
 *     that handles the vehicle's ticks.
 * <li>Run the disaster vehicles each tick until their target has been reached,
 *     this happens in the DisasterTick_XXX() functions. In here, a vehicle's
 *     state is kept by v->state variable. Each achieved sub-target
 *     will increase this value, and the last one will remove the disaster itself
 * </ol>
 */


#include "stdafx.h"

#include "aircraft.h"
#include "disaster_vehicle.h"
#include "industry.h"
#include "station_base.h"
#include "command_func.h"
#include "news_func.h"
#include "town.h"
#include "company_func.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "effectvehicle_func.h"
#include "roadveh.h"
#include "train.h"
#include "ai/ai.hpp"
#include "game/game.hpp"
#include "company_base.h"
#include "core/random_func.hpp"
#include "core/backup_type.hpp"
#include "landscape_cmd.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"

#include "table/strings.h"

#include "safeguards.h"

/** Delay counter for considering the next disaster. */
uint16_t _disaster_delay;

static void DisasterClearSquare(TileIndex tile)
{
	if (EnsureNoVehicleOnGround(tile).Failed()) return;

	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			if (Company::IsHumanID(GetTileOwner(tile)) && !IsRailDepot(tile)) {
				Backup<CompanyID> cur_company(_current_company, OWNER_WATER, FILE_LINE);
				Command<CMD_LANDSCAPE_CLEAR>::Do(DC_EXEC, tile);
				cur_company.Restore();

				/* update signals in buffer */
				UpdateSignalsInBuffer();
			}
			break;

		case MP_HOUSE: {
			Backup<CompanyID> cur_company(_current_company, OWNER_NONE, FILE_LINE);
			Command<CMD_LANDSCAPE_CLEAR>::Do(DC_EXEC, tile);
			cur_company.Restore();
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

void DisasterVehicle::UpdateImage()
{
	SpriteID img = this->image_override;
	if (img == 0) img = _disaster_images[this->subtype][this->direction];
	this->sprite_cache.sprite_seq.Set(img);
}

/**
 * Construct the disaster vehicle.
 * @param x         The X coordinate.
 * @param y         The Y coordinate.
 * @param direction The direction the vehicle is facing.
 * @param subtype   The sub type of vehicle.
 * @param big_ufo_destroyer_target The target for the UFO destroyer.
 */
DisasterVehicle::DisasterVehicle(int x, int y, Direction direction, DisasterSubType subtype, VehicleID big_ufo_destroyer_target) :
		SpecializedVehicleBase(), big_ufo_destroyer_target(big_ufo_destroyer_target)
{
	this->vehstatus = VS_UNCLICKABLE;

	this->x_pos = x;
	this->y_pos = y;
	switch (subtype) {
		case ST_ZEPPELINER:
		case ST_SMALL_UFO:
		case ST_AIRPLANE:
		case ST_HELICOPTER:
		case ST_BIG_UFO:
		case ST_BIG_UFO_DESTROYER:
			GetAircraftFlightLevelBounds(this, &this->z_pos, nullptr);
			break;

		case ST_HELICOPTER_ROTORS:
			GetAircraftFlightLevelBounds(this, &this->z_pos, nullptr);
			this->z_pos += ROTOR_Z_OFFSET;
			break;

		case ST_SMALL_SUBMARINE:
		case ST_BIG_SUBMARINE:
			this->z_pos = 0;
			break;

		case ST_ZEPPELINER_SHADOW:
		case ST_SMALL_UFO_SHADOW:
		case ST_AIRPLANE_SHADOW:
		case ST_HELICOPTER_SHADOW:
		case ST_BIG_UFO_SHADOW:
		case ST_BIG_UFO_DESTROYER_SHADOW:
			this->z_pos = 0;
			this->vehstatus |= VS_SHADOW;
			break;
	}

	this->direction = direction;
	this->tile = TileVirtXY(x, y);
	this->subtype = subtype;
	this->UpdateDeltaXY();
	this->owner = OWNER_NONE;
	this->image_override = 0;
	this->state = 0;

	this->UpdateImage();
	this->UpdatePositionAndViewport();
}

/**
 * Update the position of the vehicle.
 * @param x The new X-coordinate.
 * @param y The new Y-coordinate.
 * @param z The new Z-coordinate.
 */
void DisasterVehicle::UpdatePosition(int x, int y, int z)
{
	this->x_pos = x;
	this->y_pos = y;
	this->z_pos = z;
	this->tile = TileVirtXY(x, y);

	this->UpdateImage();
	this->UpdatePositionAndViewport();

	DisasterVehicle *u = this->Next();
	if (u != nullptr) {
		int safe_x = Clamp(x, 0, Map::MaxX() * TILE_SIZE);
		int safe_y = Clamp(y - 1, 0, Map::MaxY() * TILE_SIZE);

		u->x_pos = x;
		u->y_pos = y - 1 - (std::max(z - GetSlopePixelZ(safe_x, safe_y), 0) >> 3);
		safe_y = Clamp(u->y_pos, 0, Map::MaxY() * TILE_SIZE);
		u->z_pos = GetSlopePixelZ(safe_x, safe_y);
		u->direction = this->direction;

		u->UpdateImage();
		u->UpdatePositionAndViewport();

		if ((u = u->Next()) != nullptr) {
			u->x_pos = x;
			u->y_pos = y;
			u->z_pos = z + ROTOR_Z_OFFSET;
			u->UpdatePositionAndViewport();
		}
	}
}

/**
 * Zeppeliner handling, v->state states:
 * 0: Zeppeliner initialization has found a small airport, go there and crash
 * 1: Create crash and animate falling down for extra dramatic effect
 * 2: Create more smoke and leave debris on ground
 * 2: Clear the runway after some time and remove crashed zeppeliner
 * If not airport was found, only state 0 is reached until zeppeliner leaves map
 */
static bool DisasterTick_Zeppeliner(DisasterVehicle *v)
{
	v->tick_counter++;

	if (v->state < 2) {
		if (HasBit(v->tick_counter, 0)) return true;

		GetNewVehiclePosResult gp = GetNewVehiclePos(v);

		v->UpdatePosition(gp.x, gp.y, GetAircraftFlightLevel(v));

		if (v->state == 1) {
			if (++v->age == 38) {
				v->state = 2;
				v->age = 0;
			}

			if (GB(v->tick_counter, 0, 3) == 0) CreateEffectVehicleRel(v, 0, -17, 2, EV_CRASH_SMOKE);

		} else if (v->state == 0) {
			if (IsValidTile(v->tile) && IsAirportTile(v->tile)) {
				v->state = 1;
				v->age = 0;

				SetDParam(0, GetStationIndex(v->tile));
				AddTileNewsItem(STR_NEWS_DISASTER_ZEPPELIN, NT_ACCIDENT, v->tile);
				AI::NewEvent(GetTileOwner(v->tile), new ScriptEventDisasterZeppelinerCrashed(GetStationIndex(v->tile)));
			}
		}

		if (v->y_pos >= (int)((Map::SizeY() + 9) * TILE_SIZE - 1)) {
			delete v;
			return false;
		}

		return true;
	}

	if (v->state > 2) {
		if (++v->age <= 13320) return true;

		if (IsValidTile(v->tile) && IsAirportTile(v->tile)) {
			Station *st = Station::GetByTile(v->tile);
			CLRBITS(st->airport.flags, RUNWAY_IN_block);
			AI::NewEvent(GetTileOwner(v->tile), new ScriptEventDisasterZeppelinerCleared(st->index));
		}

		v->UpdatePosition(v->x_pos, v->y_pos, GetAircraftFlightLevel(v));
		delete v;
		return false;
	}

	int x = v->x_pos;
	int y = v->y_pos;
	int z = GetSlopePixelZ(x, y);
	if (z < v->z_pos) z = v->z_pos - 1;
	v->UpdatePosition(x, y, z);

	if (++v->age == 1) {
		CreateEffectVehicleRel(v, 0, 7, 8, EV_EXPLOSION_LARGE);
		if (_settings_client.sound.disaster) SndPlayVehicleFx(SND_12_EXPLOSION, v);
		v->image_override = SPR_BLIMP_CRASHING;
	} else if (v->age == 70) {
		v->image_override = SPR_BLIMP_CRASHED;
	} else if (v->age <= 300) {
		if (GB(v->tick_counter, 0, 3) == 0) {
			uint32_t r = Random();

			CreateEffectVehicleRel(v,
				GB(r, 0, 4) - 7,
				GB(r, 4, 4) - 7,
				GB(r, 8, 3) + 5,
				EV_EXPLOSION_SMALL);
		}
	} else if (v->age == 350) {
		v->state = 3;
		v->age = 0;
	}

	if (IsValidTile(v->tile) && IsAirportTile(v->tile)) {
		SETBITS(Station::GetByTile(v->tile)->airport.flags, RUNWAY_IN_block);
	}

	return true;
}

/**
 * (Small) Ufo handling, v->state states:
 * 0: Fly around to the middle of the map, then randomly, after a while target a road vehicle
 * 1: Home in on a road vehicle and crash it >:)
 * If not road vehicle was found, only state 0 is used and Ufo disappears after a while
 */
static bool DisasterTick_Ufo(DisasterVehicle *v)
{
	v->image_override = (HasBit(++v->tick_counter, 3)) ? SPR_UFO_SMALL_SCOUT_DARKER : SPR_UFO_SMALL_SCOUT;

	if (v->state == 0) {
		/* Fly around randomly */
		int x = TileX(v->dest_tile) * TILE_SIZE;
		int y = TileY(v->dest_tile) * TILE_SIZE;
		if (Delta(x, v->x_pos) + Delta(y, v->y_pos) >= (int)TILE_SIZE) {
			v->direction = GetDirectionTowards(v, x, y);
			GetNewVehiclePosResult gp = GetNewVehiclePos(v);
			v->UpdatePosition(gp.x, gp.y, GetAircraftFlightLevel(v));
			return true;
		}
		if (++v->age < 6) {
			v->dest_tile = RandomTile();
			return true;
		}
		v->state = 1;

		uint n = 0; // Total number of targetable road vehicles.
		for (const RoadVehicle *u : RoadVehicle::Iterate()) {
			if (u->IsFrontEngine()) n++;
		}

		if (n == 0) {
			/* If there are no targetable road vehicles, destroy the UFO. */
			delete v;
			return false;
		}

		n = RandomRange(n); // Choose one of them.
		for (const RoadVehicle *u : RoadVehicle::Iterate()) {
			/* Find (n+1)-th road vehicle. */
			if (u->IsFrontEngine() && (n-- == 0)) {
				/* Target it. */
				v->dest_tile = u->index;
				v->age = 0;
				break;
			}
		}

		return true;
	} else {
		/* Target a vehicle */
		RoadVehicle *u = RoadVehicle::Get(v->dest_tile.base());
		assert(u != nullptr && u->type == VEH_ROAD && u->IsFrontEngine());

		uint dist = Delta(v->x_pos, u->x_pos) + Delta(v->y_pos, u->y_pos);

		if (dist < TILE_SIZE && !(u->vehstatus & VS_HIDDEN) && u->breakdown_ctr == 0) {
			u->breakdown_ctr = 3;
			u->breakdown_delay = 140;
		}

		v->direction = GetDirectionTowards(v, u->x_pos, u->y_pos);
		GetNewVehiclePosResult gp = GetNewVehiclePos(v);

		int z = v->z_pos;
		if (dist <= TILE_SIZE && z > u->z_pos) z--;
		v->UpdatePosition(gp.x, gp.y, z);

		if (z <= u->z_pos && (u->vehstatus & VS_HIDDEN) == 0) {
			v->age++;
			if (u->crashed_ctr == 0) {
				u->Crash();

				AddTileNewsItem(STR_NEWS_DISASTER_SMALL_UFO, NT_ACCIDENT, u->tile);

				AI::NewEvent(u->owner, new ScriptEventVehicleCrashed(u->index, u->tile, ScriptEventVehicleCrashed::CRASH_RV_UFO));
				Game::NewEvent(new ScriptEventVehicleCrashed(u->index, u->tile, ScriptEventVehicleCrashed::CRASH_RV_UFO));
			}
		}

		/* Destroy? */
		if (v->age > 50) {
			CreateEffectVehicleRel(v, 0, 7, 8, EV_EXPLOSION_LARGE);
			if (_settings_client.sound.disaster) SndPlayVehicleFx(SND_12_EXPLOSION, v);
			delete v;
			return false;
		}
	}

	return true;
}

static void DestructIndustry(Industry *i)
{
	for (TileIndex tile = 0; tile != Map::Size(); tile++) {
		if (i->TileBelongsToIndustry(tile)) {
			ResetIndustryConstructionStage(tile);
			MarkTileDirtyByTile(tile);
		}
	}
}

/**
 * Aircraft handling, v->state states:
 * 0: Fly towards the targeted industry
 * 1: If within 15 tiles, fire away rockets and destroy industry
 * 2: Industry explosions
 * 3: Fly out of the map
 * If the industry was removed in the meantime just fly to the end of the map.
 * @param v The disaster vehicle.
 * @param image_override The image at the time the aircraft is firing.
 * @param leave_at_top True iff the vehicle leaves the map at the north side.
 * @param news_message The string that's used as news message.
 * @param industry_flag Only attack industries that have this flag set.
 */
static bool DisasterTick_Aircraft(DisasterVehicle *v, uint16_t image_override, bool leave_at_top, StringID news_message, IndustryBehaviour industry_flag)
{
	v->tick_counter++;
	v->image_override = (v->state == 1 && HasBit(v->tick_counter, 2)) ? image_override : 0;

	GetNewVehiclePosResult gp = GetNewVehiclePos(v);
	v->UpdatePosition(gp.x, gp.y, GetAircraftFlightLevel(v));

	if ((leave_at_top && gp.x < (-10 * (int)TILE_SIZE)) || (!leave_at_top && gp.x > (int)(Map::SizeX() * TILE_SIZE + 9 * TILE_SIZE) - 1)) {
		delete v;
		return false;
	}

	if (v->state == 2) {
		if (GB(v->tick_counter, 0, 2) == 0) {
			Industry *i = Industry::Get(v->dest_tile.base()); // Industry destructor calls ReleaseDisastersTargetingIndustry, so this is valid
			int x = TileX(i->location.tile) * TILE_SIZE;
			int y = TileY(i->location.tile) * TILE_SIZE;
			uint32_t r = Random();

			CreateEffectVehicleAbove(
				GB(r,  0, 6) + x,
				GB(r,  6, 6) + y,
				GB(r, 12, 4),
				EV_EXPLOSION_SMALL);

			if (++v->age >= 55) v->state = 3;
		}
	} else if (v->state == 1) {
		if (++v->age == 112) {
			v->state = 2;
			v->age = 0;

			Industry *i = Industry::Get(v->dest_tile.base()); // Industry destructor calls ReleaseDisastersTargetingIndustry, so this is valid
			DestructIndustry(i);

			SetDParam(0, i->town->index);
			AddIndustryNewsItem(news_message, NT_ACCIDENT, i->index);
			if (_settings_client.sound.disaster) SndPlayTileFx(SND_12_EXPLOSION, i->location.tile);
		}
	} else if (v->state == 0) {
		int x = v->x_pos + ((leave_at_top ? -15 : 15) * TILE_SIZE);
		int y = v->y_pos;

		if ((uint)x > Map::MaxX() * TILE_SIZE - 1) return true;

		TileIndex tile = TileVirtXY(x, y);
		if (!IsTileType(tile, MP_INDUSTRY)) return true;

		IndustryID ind = GetIndustryIndex(tile);
		v->dest_tile = ind;

		if (GetIndustrySpec(Industry::Get(ind)->type)->behaviour & industry_flag) {
			v->state = 1;
			v->age = 0;
		}
	}

	return true;
}

/** Airplane handling. */
static bool DisasterTick_Airplane(DisasterVehicle *v)
{
	return DisasterTick_Aircraft(v, SPR_F_15_FIRING, true, STR_NEWS_DISASTER_AIRPLANE_OIL_REFINERY, INDUSTRYBEH_AIRPLANE_ATTACKS);
}

/** Helicopter handling. */
static bool DisasterTick_Helicopter(DisasterVehicle *v)
{
	return DisasterTick_Aircraft(v, SPR_AH_64A_FIRING, false, STR_NEWS_DISASTER_HELICOPTER_FACTORY, INDUSTRYBEH_CHOPPER_ATTACKS);
}

/** Helicopter rotor blades; keep these spinning */
static bool DisasterTick_Helicopter_Rotors(DisasterVehicle *v)
{
	v->tick_counter++;
	if (HasBit(v->tick_counter, 0)) return true;

	SpriteID &cur_image = v->sprite_cache.sprite_seq.seq[0].sprite;
	if (++cur_image > SPR_ROTOR_MOVING_3) cur_image = SPR_ROTOR_MOVING_1;

	v->UpdatePositionAndViewport();

	return true;
}

/**
 * (Big) Ufo handling, v->state states:
 * 0: Fly around to the middle of the map, then randomly for a while and home in on a piece of rail
 * 1: Land there and breakdown all trains in a radius of 12 tiles; and now we wait...
 *    because as soon as the Ufo lands, a fighter jet, a Skyranger, is called to clear up the mess
 */
static bool DisasterTick_Big_Ufo(DisasterVehicle *v)
{
	v->tick_counter++;

	if (v->state == 1) {
		int x = TileX(v->dest_tile) * TILE_SIZE + TILE_SIZE / 2;
		int y = TileY(v->dest_tile) * TILE_SIZE + TILE_SIZE / 2;
		if (Delta(v->x_pos, x) + Delta(v->y_pos, y) >= 8) {
			v->direction = GetDirectionTowards(v, x, y);

			GetNewVehiclePosResult gp = GetNewVehiclePos(v);
			v->UpdatePosition(gp.x, gp.y, GetAircraftFlightLevel(v));
			return true;
		}

		if (!IsValidTile(v->dest_tile)) {
			/* Make sure we don't land outside the map. */
			delete v;
			return false;
		}

		int z = GetSlopePixelZ(v->x_pos, v->y_pos);
		if (z < v->z_pos) {
			v->UpdatePosition(v->x_pos, v->y_pos, v->z_pos - 1);
			return true;
		}

		v->state = 2;

		for (Vehicle *target : Vehicle::Iterate()) {
			if (target->IsGroundVehicle()) {
				if (Delta(target->x_pos, v->x_pos) + Delta(target->y_pos, v->y_pos) <= 12 * (int)TILE_SIZE) {
					target->breakdown_ctr = 5;
					target->breakdown_delay = 0xF0;
				}
			}
		}

		Town *t = ClosestTownFromTile(v->dest_tile, UINT_MAX);
		SetDParam(0, t->index);
		AddTileNewsItem(STR_NEWS_DISASTER_BIG_UFO, NT_ACCIDENT, v->tile);

		if (!Vehicle::CanAllocateItem(2)) {
			delete v;
			return false;
		}
		DisasterVehicle *u = new DisasterVehicle(-6 * (int)TILE_SIZE, v->y_pos, DIR_SW, ST_BIG_UFO_DESTROYER, v->index);
		DisasterVehicle *w = new DisasterVehicle(-6 * (int)TILE_SIZE, v->y_pos, DIR_SW, ST_BIG_UFO_DESTROYER_SHADOW);
		u->SetNext(w);
	} else if (v->state == 0) {
		int x = TileX(v->dest_tile) * TILE_SIZE;
		int y = TileY(v->dest_tile) * TILE_SIZE;
		if (Delta(x, v->x_pos) + Delta(y, v->y_pos) >= (int)TILE_SIZE) {
			v->direction = GetDirectionTowards(v, x, y);
			GetNewVehiclePosResult gp = GetNewVehiclePos(v);
			v->UpdatePosition(gp.x, gp.y, GetAircraftFlightLevel(v));
			return true;
		}

		if (++v->age < 6) {
			v->dest_tile = RandomTile();
			return true;
		}
		v->state = 1;

		const auto is_valid_target = [](const Train *t) {
			return t->IsFrontEngine() // Only the engines
				&& Company::IsHumanID(t->owner) // Don't break AIs
				&& IsPlainRailTile(t->tile) // No tunnels
				&& (t->vehstatus & VS_CRASHED) == 0; // Not crashed
		};

		uint n = 0; // Total number of targetable trains.
		for (const Train *t : Train::Iterate()) {
			if (is_valid_target(t)) n++;
		}

		if (n == 0) {
			/* If there are no targetable trains, destroy the UFO. */
			delete v;
			return false;
		}

		n = RandomRange(n); // Choose one of them.
		for (const Train *t : Train::Iterate()) {
			/* Find (n+1)-th train. */
			if (is_valid_target(t) && (n-- == 0)) {
				/* Target it. */
				v->dest_tile = t->tile;
				v->age = 0;
				break;
			}
		}
	}

	return true;
}

/**
 * Skyranger destroying (Big) Ufo handling, v->state states:
 * 0: Home in on landed Ufo and shoot it down
 */
static bool DisasterTick_Big_Ufo_Destroyer(DisasterVehicle *v)
{
	v->tick_counter++;

	GetNewVehiclePosResult gp = GetNewVehiclePos(v);
	v->UpdatePosition(gp.x, gp.y, GetAircraftFlightLevel(v));

	if (gp.x > (int)(Map::SizeX() * TILE_SIZE + 9 * TILE_SIZE) - 1) {
		delete v;
		return false;
	}

	if (v->state == 0) {
		Vehicle *u = Vehicle::Get(v->big_ufo_destroyer_target);
		if (Delta(v->x_pos, u->x_pos) > (int)TILE_SIZE) return true;
		v->state = 1;

		CreateEffectVehicleRel(u, 0, 7, 8, EV_EXPLOSION_LARGE);
		if (_settings_client.sound.disaster) SndPlayVehicleFx(SND_12_EXPLOSION, u);

		delete u;

		for (int i = 0; i != 80; i++) {
			uint32_t r = Random();
			CreateEffectVehicleAbove(
				GB(r, 0, 6) + v->x_pos - 32,
				GB(r, 5, 6) + v->y_pos - 32,
				0,
				EV_EXPLOSION_SMALL);
		}

		for (int dy = -3; dy < 3; dy++) {
			for (int dx = -3; dx < 3; dx++) {
				TileIndex tile = TileAddWrap(v->tile, dx, dy);
				if (tile != INVALID_TILE) DisasterClearSquare(tile);
			}
		}
	}

	return true;
}

/**
 * Submarine, v->state states:
 * Unused, just float around aimlessly and pop up at different places, turning around
 */
static bool DisasterTick_Submarine(DisasterVehicle *v)
{
	v->tick_counter++;

	if (++v->age > 8880) {
		delete v;
		return false;
	}

	if (!HasBit(v->tick_counter, 0)) return true;

	TileIndex tile = v->tile + TileOffsByDiagDir(DirToDiagDir(v->direction));
	if (IsValidTile(tile)) {
		TrackBits trackbits = TrackStatusToTrackBits(GetTileTrackStatus(tile, TRANSPORT_WATER, 0));
		if (trackbits == TRACK_BIT_ALL && !Chance16(1, 90)) {
			GetNewVehiclePosResult gp = GetNewVehiclePos(v);
			v->UpdatePosition(gp.x, gp.y, v->z_pos);
			return true;
		}
	}

	v->direction = ChangeDir(v->direction, GB(Random(), 0, 1) ? DIRDIFF_90RIGHT : DIRDIFF_90LEFT);

	return true;
}


static bool DisasterTick_NULL(DisasterVehicle *)
{
	return true;
}

typedef bool DisasterVehicleTickProc(DisasterVehicle *v);

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


bool DisasterVehicle::Tick()
{
	return _disastervehicle_tick_procs[this->subtype](this);
}

typedef void DisasterInitProc();


/**
 * Zeppeliner which crashes on a small airport if one found,
 * otherwise crashes on a random tile
 */
static void Disaster_Zeppeliner_Init()
{
	if (!Vehicle::CanAllocateItem(2)) return;

	/* Pick a random place, unless we find a small airport */
	int x = TileX(Random()) * TILE_SIZE + TILE_SIZE / 2;

	for (const Station *st : Station::Iterate()) {
		if (st->airport.tile != INVALID_TILE && (st->airport.type == AT_SMALL || st->airport.type == AT_LARGE)) {
			x = (TileX(st->airport.tile) + 2) * TILE_SIZE;
			break;
		}
	}

	DisasterVehicle *v = new DisasterVehicle(x, 0, DIR_SE, ST_ZEPPELINER);
	/* Allocate shadow */
	DisasterVehicle *u = new DisasterVehicle(x, 0, DIR_SE, ST_ZEPPELINER_SHADOW);
	v->SetNext(u);
}


/**
 * Ufo which flies around aimlessly from the middle of the map a bit
 * until it locates a road vehicle which it targets and then destroys
 */
static void Disaster_Small_Ufo_Init()
{
	if (!Vehicle::CanAllocateItem(2)) return;

	int x = TileX(Random()) * TILE_SIZE + TILE_SIZE / 2;
	DisasterVehicle *v = new DisasterVehicle(x, 0, DIR_SE, ST_SMALL_UFO);
	v->dest_tile = TileXY(Map::SizeX() / 2, Map::SizeY() / 2);

	/* Allocate shadow */
	DisasterVehicle *u = new DisasterVehicle(x, 0, DIR_SE, ST_SMALL_UFO_SHADOW);
	v->SetNext(u);
}


/* Combat airplane which destroys an oil refinery */
static void Disaster_Airplane_Init()
{
	if (!Vehicle::CanAllocateItem(2)) return;

	Industry *found = nullptr;

	for (Industry *i : Industry::Iterate()) {
		if ((GetIndustrySpec(i->type)->behaviour & INDUSTRYBEH_AIRPLANE_ATTACKS) &&
				(found == nullptr || Chance16(1, 2))) {
			found = i;
		}
	}

	if (found == nullptr) return;

	/* Start from the bottom (south side) of the map */
	int x = (Map::SizeX() + 9) * TILE_SIZE - 1;
	int y = TileY(found->location.tile) * TILE_SIZE + 37;

	DisasterVehicle *v = new DisasterVehicle(x, y, DIR_NE, ST_AIRPLANE);
	DisasterVehicle *u = new DisasterVehicle(x, y, DIR_NE, ST_AIRPLANE_SHADOW);
	v->SetNext(u);
}


/** Combat helicopter that destroys a factory */
static void Disaster_Helicopter_Init()
{
	if (!Vehicle::CanAllocateItem(3)) return;

	Industry *found = nullptr;

	for (Industry *i : Industry::Iterate()) {
		if ((GetIndustrySpec(i->type)->behaviour & INDUSTRYBEH_CHOPPER_ATTACKS) &&
				(found == nullptr || Chance16(1, 2))) {
			found = i;
		}
	}

	if (found == nullptr) return;

	int x = -16 * (int)TILE_SIZE;
	int y = TileY(found->location.tile) * TILE_SIZE + 37;

	DisasterVehicle *v = new DisasterVehicle(x, y, DIR_SW, ST_HELICOPTER);
	DisasterVehicle *u = new DisasterVehicle(x, y, DIR_SW, ST_HELICOPTER_SHADOW);
	v->SetNext(u);

	DisasterVehicle *w = new DisasterVehicle(x, y, DIR_SW, ST_HELICOPTER_ROTORS);
	u->SetNext(w);
}


/* Big Ufo which lands on a piece of rail and will consequently be shot
 * down by a combat airplane, destroying the surroundings */
static void Disaster_Big_Ufo_Init()
{
	if (!Vehicle::CanAllocateItem(2)) return;

	int x = TileX(Random()) * TILE_SIZE + TILE_SIZE / 2;
	int y = Map::MaxX() * TILE_SIZE - 1;

	DisasterVehicle *v = new DisasterVehicle(x, y, DIR_NW, ST_BIG_UFO);
	v->dest_tile = TileXY(Map::SizeX() / 2, Map::SizeY() / 2);

	/* Allocate shadow */
	DisasterVehicle *u = new DisasterVehicle(x, y, DIR_NW, ST_BIG_UFO_SHADOW);
	v->SetNext(u);
}


static void Disaster_Submarine_Init(DisasterSubType subtype)
{
	if (!Vehicle::CanAllocateItem()) return;

	int y;
	Direction dir;
	uint32_t r = Random();
	int x = TileX(r) * TILE_SIZE + TILE_SIZE / 2;

	if (HasBit(r, 31)) {
		y = Map::MaxY() * TILE_SIZE - TILE_SIZE / 2 - 1;
		dir = DIR_NW;
	} else {
		y = TILE_SIZE / 2;
		if (_settings_game.construction.freeform_edges) y += TILE_SIZE;
		dir = DIR_SE;
	}
	if (!IsWaterTile(TileVirtXY(x, y))) return;

	new DisasterVehicle(x, y, dir, subtype);
}

/* Curious submarine #1, just floats around */
static void Disaster_Small_Submarine_Init()
{
	Disaster_Submarine_Init(ST_SMALL_SUBMARINE);
}


/* Curious submarine #2, just floats around */
static void Disaster_Big_Submarine_Init()
{
	Disaster_Submarine_Init(ST_BIG_SUBMARINE);
}


/**
 * Coal mine catastrophe, destroys a stretch of 30 tiles of
 * land in a certain direction
 */
static void Disaster_CoalMine_Init()
{
	int index = GB(Random(), 0, 4);
	uint m;

	for (m = 0; m < 15; m++) {
		for (const Industry *i : Industry::Iterate()) {
			if ((GetIndustrySpec(i->type)->behaviour & INDUSTRYBEH_CAN_SUBSIDENCE) && --index < 0) {
				SetDParam(0, i->town->index);
				AddTileNewsItem(STR_NEWS_DISASTER_COAL_MINE_SUBSIDENCE, NT_ACCIDENT, i->location.tile + TileDiffXY(1, 1)); // keep the news, even when the mine closes

				{
					TileIndex tile = i->location.tile;
					TileIndexDiff step = TileOffsByDiagDir((DiagDirection)GB(Random(), 0, 2));

					for (uint n = 0; n < 30; n++) {
						DisasterClearSquare(tile);
						tile += step;
						if (!IsValidTile(tile)) break;
					}
				}
				return;
			}
		}
	}
}

struct Disaster {
	DisasterInitProc *init_proc;      ///< The init function for this disaster.
	TimerGameCalendar::Year min_year; ///< The first year this disaster will occur.
	TimerGameCalendar::Year max_year; ///< The last year this disaster will occur.
};

static const Disaster _disasters[] = {
	{Disaster_Zeppeliner_Init,      1930, 1955}, // zeppeliner
	{Disaster_Small_Ufo_Init,       1940, 1970}, // ufo (small)
	{Disaster_Airplane_Init,        1960, 1990}, // airplane
	{Disaster_Helicopter_Init,      1970, 2000}, // helicopter
	{Disaster_Big_Ufo_Init,         2000, 2100}, // ufo (big)
	{Disaster_Small_Submarine_Init, 1940, 1965}, // submarine (small)
	{Disaster_Big_Submarine_Init,   1975, 2010}, // submarine (big)
	{Disaster_CoalMine_Init,        1950, 1985}, // coalmine
};

static void DoDisaster()
{
	byte buf[lengthof(_disasters)];

	byte j = 0;
	for (size_t i = 0; i != lengthof(_disasters); i++) {
		if (TimerGameCalendar::year >= _disasters[i].min_year && TimerGameCalendar::year < _disasters[i].max_year) buf[j++] = (byte)i;
	}

	if (j == 0) return;

	_disasters[buf[RandomRange(j)]].init_proc();
}


static void ResetDisasterDelay()
{
	_disaster_delay = GB(Random(), 0, 9) + 730;
}

static IntervalTimer<TimerGameCalendar> _disaster_daily({TimerGameCalendar::DAY, TimerGameCalendar::Priority::DISASTER}, [](auto)
{
	if (--_disaster_delay != 0) return;

	ResetDisasterDelay();

	if (_settings_game.difficulty.disasters != 0) DoDisaster();
});

void StartupDisasters()
{
	ResetDisasterDelay();
}

/**
 * Marks all disasters targeting this industry in such a way
 * they won't call Industry::Get(v->dest_tile) on invalid industry anymore.
 * @param i deleted industry
 */
void ReleaseDisastersTargetingIndustry(IndustryID i)
{
	for (DisasterVehicle *v : DisasterVehicle::Iterate()) {
		/* primary disaster vehicles that have chosen target */
		if (v->subtype == ST_AIRPLANE || v->subtype == ST_HELICOPTER) {
			/* if it has chosen target, and it is this industry (yes, dest_tile is IndustryID here), set order to "leaving map peacefully" */
			if (v->state > 0 && v->dest_tile == (uint32_t)i) v->state = 3;
		}
	}
}

/**
 * Notify disasters that we are about to delete a vehicle. So make them head elsewhere.
 * @param vehicle deleted vehicle
 */
void ReleaseDisastersTargetingVehicle(VehicleID vehicle)
{
	for (DisasterVehicle *v : DisasterVehicle::Iterate()) {
		/* primary disaster vehicles that have chosen target */
		if (v->subtype == ST_SMALL_UFO) {
			if (v->state != 0 && v->dest_tile == vehicle) {
				/* Revert to target-searching */
				v->state = 0;
				v->dest_tile = RandomTile();
				GetAircraftFlightLevelBounds(v, &v->z_pos, nullptr);
				v->age = 0;
			}
		}
	}
}

void DisasterVehicle::UpdateDeltaXY()
{
	this->x_offs        = -1;
	this->y_offs        = -1;
	this->x_extent      =  2;
	this->y_extent      =  2;
	this->z_extent      =  5;
}
