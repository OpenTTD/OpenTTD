/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file engine_type.h Types related to engines. */

#ifndef ENGINE_TYPE_H
#define ENGINE_TYPE_H

#include "economy_type.h"
#include "rail_type.h"
#include "cargo_type.h"
#include "date_type.h"
#include "sound_type.h"
#include "strings_type.h"

typedef uint16 EngineID;

struct Engine;

enum RailVehicleTypes {
	RAILVEH_SINGLEHEAD,  ///< indicates a "standalone" locomotive
	RAILVEH_MULTIHEAD,   ///< indicates a combination of two locomotives
	RAILVEH_WAGON,       ///< simple wagon, not motorized
};

enum EngineClass {
	EC_STEAM,
	EC_DIESEL,
	EC_ELECTRIC,
	EC_MONORAIL,
	EC_MAGLEV,
};

struct RailVehicleInfo {
	byte image_index;
	RailVehicleTypes railveh_type;
	byte cost_factor;               ///< Purchase cost factor;      For multiheaded engines the sum of both engine prices.
	RailTypeByte railtype;
	uint16 max_speed;               ///< Maximum speed (1 unit = 1/1.6 mph = 1 km-ish/h)
	uint16 power;                   ///< Power of engine (hp);      For multiheaded engines the sum of both engine powers.
	uint16 weight;                  ///< Weight of vehicle (tons);  For multiheaded engines the weight of each single engine.
	byte running_cost;              ///< Running cost of engine;    For multiheaded engines the sum of both running costs.
	Price running_cost_class;
	EngineClass engclass;           ///< Class of engine for this vehicle
	byte capacity;                  ///< Cargo capacity of vehicle; For multiheaded engines the capacity of each single engine.
	byte ai_passenger_only;         ///< Bit value to tell AI that this engine is for passenger use only
	uint16 pow_wag_power;           ///< Extra power applied to consist if wagon should be powered
	byte pow_wag_weight;            ///< Extra weight applied to consist if wagon should be powered
	byte visual_effect;             ///< Bitstuffed NewGRF visual effect data
	byte shorten_factor;            ///< length on main map for this type is 8 - shorten_factor
	byte tractive_effort;           ///< Tractive effort coefficient
	byte air_drag;                  ///< Coefficient of air drag
	byte user_def_data;             ///< Property 0x25: "User-defined bit mask" Used only for (very few) NewGRF vehicles
};

struct ShipVehicleInfo {
	byte image_index;
	byte cost_factor;
	uint16 max_speed;      ///< Maximum speed (1 unit = 1/3.2 mph = 0.5 km-ish/h)
	uint16 capacity;
	byte running_cost;
	SoundID sfx;
	bool old_refittable;   ///< Is ship refittable; only used during initialisation. Later use EngineInfo::refit_mask.
	byte visual_effect;    ///< Bitstuffed NewGRF visual effect data
};

/* AircraftVehicleInfo subtypes, bitmask type.
 * If bit 0 is 0 then it is a helicopter, otherwise it is a plane
 * in which case bit 1 tells us whether it's a big(fast) plane or not */
enum AircraftSubTypeBits {
	AIR_HELI = 0,
	AIR_CTOL = 1, ///< Conventional Take Off and Landing, i.e. planes
	AIR_FAST = 2
};

struct AircraftVehicleInfo {
	byte image_index;
	byte cost_factor;
	byte running_cost;
	byte subtype;
	SoundID sfx;
	byte acceleration;
	uint16 max_speed;           ///< Maximum speed (1 unit = 8 mph = 12.8 km-ish/h)
	byte mail_capacity;
	uint16 passenger_capacity;
};

struct RoadVehicleInfo {
	byte image_index;
	byte cost_factor;
	byte running_cost;
	Price running_cost_class;
	SoundID sfx;
	uint16 max_speed;        ///< Maximum speed (1 unit = 1/3.2 mph = 0.5 km-ish/h)
	byte capacity;
	uint8 weight;            ///< Weight in 1/4t units
	uint8 power;             ///< Power in 10hp units
	uint8 tractive_effort;   ///< Coefficient of tractive effort
	uint8 air_drag;          ///< Coefficient of air drag
	byte visual_effect;      ///< Bitstuffed NewGRF visual effect data
};

/**
 * Information about a vehicle
 *  @see table/engines.h
 */
struct EngineInfo {
	Date base_intro;
	Year lifelength;    ///< Lifetime of a single vehicle
	Year base_life;     ///< Basic duration of engine availability (without random parts)
	byte decay_speed;
	byte load_amount;
	byte climates;
	CargoID cargo_type;
	uint32 refit_mask;
	byte refit_cost;
	byte misc_flags;
	byte callback_mask; ///< Bitmask of vehicle callbacks that have to be called
	int8 retire_early;  ///< Number of years early to retire vehicle
	StringID string_id; ///< Default name of engine
};

/**
 * EngineInfo.misc_flags is a bitmask, with the following values
 */
enum EngineMiscFlags {
	EF_RAIL_TILTS = 0, ///< Rail vehicle tilts in curves
	EF_ROAD_TRAM  = 0, ///< Road vehicle is a tram/light rail vehicle
	EF_USES_2CC   = 1, ///< Vehicle uses two company colours
	EF_RAIL_IS_MU = 2, ///< Rail vehicle is a multiple-unit (DMU/EMU)
	EF_RAIL_FLIPS = 3, ///< Rail vehicle can be flipped in the depot
};

/**
 * Engine.flags is a bitmask, with the following values.
 */
enum EngineFlags {
	ENGINE_AVAILABLE         = 1, ///< This vehicle is available to everyone.
	ENGINE_EXCLUSIVE_PREVIEW = 2, ///< This vehicle is in the exclusive preview stage, either being used or being offered to a company.
	ENGINE_OFFER_WINDOW_OPEN = 4, ///< The exclusive offer window is currently open for a company.
};

static const uint NUM_VEHICLE_TYPES             =   6;
static const uint MAX_LENGTH_ENGINE_NAME_CHARS  =  32; ///< The maximum length of an engine name in characters including '\0'
static const uint MAX_LENGTH_ENGINE_NAME_PIXELS = 160; ///< The maximum length of an engine name in pixels

static const EngineID INVALID_ENGINE = 0xFFFF;

#endif /* ENGINE_TYPE_H */
