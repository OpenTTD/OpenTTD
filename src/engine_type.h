/* $Id$ */

/** @file engine_type.h Types related to engines. */

#ifndef ENGINE_TYPE_H
#define ENGINE_TYPE_H

#include "rail_type.h"
#include "cargo_type.h"
#include "vehicle_type.h"
#include "gfx_type.h"
#include "date_type.h"
#include "sound_type.h"
#include "player_type.h"
#include "strings_type.h"

#include <vector>

typedef uint16 EngineID;
typedef uint16 EngineRenewID;
typedef std::vector<EngineID> EngineList; ///< engine list type

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
	byte base_cost;
	RailTypeByte railtype;
	uint16 max_speed;
	uint16 power;
	uint16 weight;
	byte running_cost;
	byte running_cost_class;
	EngineClass engclass;           ///< Class of engine for this vehicle
	byte capacity;
	CargoID cargo_type;
	byte ai_rank;
	byte ai_passenger_only; ///< Bit value to tell AI that this engine is for passenger use only
	uint16 pow_wag_power;
	byte pow_wag_weight;
	byte visual_effect; // NOTE: this is not 100% implemented yet, at the moment it is only used as a 'fallback' value
	                    //       for when the 'powered wagon' callback fails. But it should really also determine what
	                    //       kind of visual effect to generate for a vehicle (default, steam, diesel, electric).
	                    //       Same goes for the callback result, which atm is only used to check if a wagon is powered.
	byte shorten_factor;   ///< length on main map for this type is 8 - shorten_factor
	byte tractive_effort;  ///< Tractive effort coefficient
	byte user_def_data;    ///< Property 0x25: "User-defined bit mask" Used only for (very few) NewGRF vehicles
};

struct ShipVehicleInfo {
	byte image_index;
	byte base_cost;
	uint16 max_speed;
	CargoID cargo_type;
	uint16 capacity;
	byte running_cost;
	SoundFxByte sfx;
	bool refittable;
};

/* AircraftVehicleInfo subtypes, bitmask type.
 * If bit 0 is 0 then it is a helicopter, otherwise it is a plane
 * in which case bit 1 tells us whether it's a big(fast) plane or not */
enum {
	AIR_HELI = 0,
	AIR_CTOL = 1, ///< Conventional Take Off and Landing, i.e. planes
	AIR_FAST = 2
};

struct AircraftVehicleInfo {
	byte image_index;
	byte base_cost;
	byte running_cost;
	byte subtype;
	SoundFxByte sfx;
	byte acceleration;
	uint16 max_speed;
	byte mail_capacity;
	uint16 passenger_capacity;
};

struct RoadVehicleInfo {
	byte image_index;
	byte base_cost;
	byte running_cost;
	byte running_cost_class;
	SoundFxByte sfx;
	byte max_speed;
	byte capacity;
	CargoID cargo_type;
};

/** Information about a vehicle
 *  @see table/engines.h
 */
struct EngineInfo {
	Date base_intro;
	Year lifelength;
	Year base_life;
	byte unk2;         ///< flag for carriage(bit 7) and decay speed(bits0..6)
	byte load_amount;
	byte climates;
	uint32 refit_mask;
	byte refit_cost;
	byte misc_flags;
	byte callbackmask;
	int8 retire_early;  ///< Number of years early to retire vehicle
	StringID string_id; ///< Default name of engine
};

/**
 * EngineInfo.misc_flags is a bitmask, with the following values
 */
enum {
	EF_RAIL_TILTS = 0, ///< Rail vehicle tilts in curves
	EF_ROAD_TRAM  = 0, ///< Road vehicle is a tram/light rail vehicle
	EF_USES_2CC   = 1, ///< Vehicle uses two company colours
	EF_RAIL_IS_MU = 2, ///< Rail vehicle is a multiple-unit (DMU/EMU)
};

/**
 * Engine.flags is a bitmask, with the following values.
 */
enum {
	ENGINE_AVAILABLE         = 1, ///< This vehicle is available to everyone.
	ENGINE_EXCLUSIVE_PREVIEW = 2, ///< This vehicle is in the exclusive preview stage, either being used or being offered to a player.
	ENGINE_OFFER_WINDOW_OPEN = 4, ///< The exclusive offer window is currently open for a player.
};

enum {
	NUM_VEHICLE_TYPES = 6
};

static const EngineID INVALID_ENGINE = 0xFFFF;

#endif /* ENGINE_TYPE_H */
