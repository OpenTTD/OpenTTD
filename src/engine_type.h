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
#include "road_type.h"
#include "cargo_type.h"
#include "timer/timer_game_calendar.h"
#include "sound_type.h"
#include "strings_type.h"

typedef uint16_t EngineID; ///< Unique identification number of an engine.

struct Engine;

/** Available types of rail vehicles. */
enum RailVehicleTypes {
	RAILVEH_SINGLEHEAD,  ///< indicates a "standalone" locomotive
	RAILVEH_MULTIHEAD,   ///< indicates a combination of two locomotives
	RAILVEH_WAGON,       ///< simple wagon, not motorized
};

/** Type of rail engine. */
enum EngineClass {
	EC_STEAM,    ///< Steam rail engine.
	EC_DIESEL,   ///< Diesel rail engine.
	EC_ELECTRIC, ///< Electric rail engine.
	EC_MONORAIL, ///< Mono rail engine.
	EC_MAGLEV,   ///< Maglev engine.
};

/** Information about a rail vehicle. */
struct RailVehicleInfo {
	byte image_index;
	RailVehicleTypes railveh_type;
	byte cost_factor;               ///< Purchase cost factor;      For multiheaded engines the sum of both engine prices.
	RailType railtype;              ///< Railtype, mangled if elrail is disabled.
	RailType intended_railtype;     ///< Intended railtype, regardless of elrail being enabled or disabled.
	uint16_t max_speed;               ///< Maximum speed (1 unit = 1/1.6 mph = 1 km-ish/h)
	uint16_t power;                   ///< Power of engine (hp);      For multiheaded engines the sum of both engine powers.
	uint16_t weight;                  ///< Weight of vehicle (tons);  For multiheaded engines the weight of each single engine.
	byte running_cost;              ///< Running cost of engine;    For multiheaded engines the sum of both running costs.
	Price running_cost_class;
	EngineClass engclass;           ///< Class of engine for this vehicle
	byte capacity;                  ///< Cargo capacity of vehicle; For multiheaded engines the capacity of each single engine.
	byte ai_passenger_only;         ///< Bit value to tell AI that this engine is for passenger use only
	uint16_t pow_wag_power;           ///< Extra power applied to consist if wagon should be powered
	byte pow_wag_weight;            ///< Extra weight applied to consist if wagon should be powered
	byte visual_effect;             ///< Bitstuffed NewGRF visual effect data
	byte shorten_factor;            ///< length on main map for this type is 8 - shorten_factor
	byte tractive_effort;           ///< Tractive effort coefficient
	byte air_drag;                  ///< Coefficient of air drag
	byte user_def_data;             ///< Property 0x25: "User-defined bit mask" Used only for (very few) NewGRF vehicles
	int16_t curve_speed_mod;          ///< Modifier to maximum speed in curves (fixed-point binary with 8 fractional bits)
};

/** Information about a ship vehicle. */
struct ShipVehicleInfo {
	byte image_index;
	byte cost_factor;
	uint16_t max_speed;      ///< Maximum speed (1 unit = 1/3.2 mph = 0.5 km-ish/h)
	uint16_t capacity;
	byte running_cost;
	SoundID sfx;
	bool old_refittable;   ///< Is ship refittable; only used during initialisation. Later use EngineInfo::refit_mask.
	byte visual_effect;    ///< Bitstuffed NewGRF visual effect data
	byte ocean_speed_frac; ///< Fraction of maximum speed for ocean tiles.
	byte canal_speed_frac; ///< Fraction of maximum speed for canal/river tiles.

	/** Apply ocean/canal speed fraction to a velocity */
	uint ApplyWaterClassSpeedFrac(uint raw_speed, bool is_ocean) const
	{
		/* speed_frac == 0 means no reduction while 0xFF means reduction to 1/256. */
		return raw_speed * (256 - (is_ocean ? this->ocean_speed_frac : this->canal_speed_frac)) / 256;
	}
};

/**
 * AircraftVehicleInfo subtypes, bitmask type.
 * If bit 0 is 0 then it is a helicopter, otherwise it is a plane
 * in which case bit 1 tells us whether it's a big(fast) plane or not.
 */
enum AircraftSubTypeBits {
	AIR_HELI = 0,
	AIR_CTOL = 1, ///< Conventional Take Off and Landing, i.e. planes
	AIR_FAST = 2
};

/** Information about a aircraft vehicle. */
struct AircraftVehicleInfo {
	byte image_index;
	byte cost_factor;
	byte running_cost;
	byte subtype;               ///< Type of aircraft. @see AircraftSubTypeBits
	SoundID sfx;
	byte acceleration;
	uint16_t max_speed;           ///< Maximum speed (1 unit = 8 mph = 12.8 km-ish/h)
	byte mail_capacity;         ///< Mail capacity (bags).
	uint16_t passenger_capacity;  ///< Passenger capacity (persons).
	uint16_t max_range;           ///< Maximum range of this aircraft.
};

/** Information about a road vehicle. */
struct RoadVehicleInfo {
	byte image_index;
	byte cost_factor;
	byte running_cost;
	Price running_cost_class;
	SoundID sfx;
	uint16_t max_speed;        ///< Maximum speed (1 unit = 1/3.2 mph = 0.5 km-ish/h)
	byte capacity;
	uint8_t weight;            ///< Weight in 1/4t units
	uint8_t power;             ///< Power in 10hp units
	uint8_t tractive_effort;   ///< Coefficient of tractive effort
	uint8_t air_drag;          ///< Coefficient of air drag
	byte visual_effect;      ///< Bitstuffed NewGRF visual effect data
	byte shorten_factor;     ///< length on main map for this type is 8 - shorten_factor
	RoadType roadtype;       ///< Road type
};

enum class ExtraEngineFlags : uint32_t {
	None = 0,
	NoNews          = (1U << 0), ///< No 'new vehicle' news will be generated.
	NoPreview       = (1U << 1), ///< No exclusive preview will be offered.
	JoinPreview     = (1U << 2), ///< Engine will join exclusive preview with variant parent.
	SyncReliability = (1U << 3), ///< Engine reliability will be synced with variant parent.
};
DECLARE_ENUM_AS_BIT_SET(ExtraEngineFlags);

/**
 * Information about a vehicle
 *  @see table/engines.h
 */
struct EngineInfo {
	TimerGameCalendar::Date base_intro;    ///< Basic date of engine introduction (without random parts).
	TimerGameCalendar::Year lifelength;    ///< Lifetime of a single vehicle
	TimerGameCalendar::Year base_life;     ///< Basic duration of engine availability (without random parts). \c 0xFF means infinite life.
	byte decay_speed;
	byte load_amount;
	byte climates;      ///< Climates supported by the engine.
	CargoID cargo_type;
	CargoTypes refit_mask;
	byte refit_cost;
	byte misc_flags;         ///< Miscellaneous flags. @see EngineMiscFlags
	uint16_t callback_mask;    ///< Bitmask of vehicle callbacks that have to be called
	int8_t retire_early;       ///< Number of years early to retire vehicle
	StringID string_id;      ///< Default name of engine
	uint16_t cargo_age_period; ///< Number of ticks before carried cargo is aged.
	EngineID variant_id;     ///< Engine variant ID. If set, will be treated specially in purchase lists.
	ExtraEngineFlags extra_flags;
};

/**
 * EngineInfo.misc_flags is a bitmask, with the following values
 */
enum EngineMiscFlags {
	EF_RAIL_TILTS = 0, ///< Rail vehicle tilts in curves
	EF_ROAD_TRAM  = 0, ///< Road vehicle is a tram/light rail vehicle
	EF_USES_2CC   = 1, ///< Vehicle uses two company colours
	EF_RAIL_IS_MU = 2, ///< Rail vehicle is a multiple-unit (DMU/EMU)
	EF_RAIL_FLIPS = 3, ///< Rail vehicle has old depot-flip handling
	EF_AUTO_REFIT = 4, ///< Automatic refitting is allowed
	EF_NO_DEFAULT_CARGO_MULTIPLIER = 5, ///< Use the new capacity algorithm. The default cargotype of the vehicle does not affect capacity multipliers. CB 15 is also called in purchase list.
	EF_NO_BREAKDOWN_SMOKE          = 6, ///< Do not show black smoke during a breakdown.
	EF_SPRITE_STACK                = 7, ///< Draw vehicle by stacking multiple sprites.
};

/**
 * Engine.flags is a bitmask, with the following values.
 */
enum EngineFlags {
	ENGINE_AVAILABLE         = 1, ///< This vehicle is available to everyone.
	ENGINE_EXCLUSIVE_PREVIEW = 2, ///< This vehicle is in the exclusive preview stage, either being used or being offered to a company.
};

/**
 * Contexts an engine name can be shown in.
 */
enum EngineNameContext : uint8_t {
	Generic                 = 0x00, ///< No specific context available.
	VehicleDetails          = 0x11, ///< Name is shown in the vehicle details GUI.
	PurchaseList            = 0x20, ///< Name is shown in the purchase list (including autoreplace window 'Available vehicles' panel).
	PreviewNews             = 0x21, ///< Name is shown in exclusive preview or newspaper.
	AutoreplaceVehicleInUse = 0x22, ///< Name is show in the autoreplace window 'Vehicles in use' panel.
};

/** Combine an engine ID and a name context to an engine name dparam. */
inline uint64_t PackEngineNameDParam(EngineID engine_id, EngineNameContext context, uint32_t extra_data = 0)
{
	return engine_id | (static_cast<uint64_t>(context) << 32) | (static_cast<uint64_t>(extra_data) << 40);
}

static const uint MAX_LENGTH_ENGINE_NAME_CHARS = 32; ///< The maximum length of an engine name in characters including '\0'

static const EngineID INVALID_ENGINE = 0xFFFF; ///< Constant denoting an invalid engine.

#endif /* ENGINE_TYPE_H */
