/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_callbacks.h Callbacks that NewGRFs could implement. */

#ifndef NEWGRF_CALLBACKS_H
#define NEWGRF_CALLBACKS_H

#include "core/enum_type.hpp"

/**
 * List of implemented NewGRF callbacks.
 * Most of these callbacks are only triggered when the corresponding
 * bit is set in the callback flags/trigger for a vehicle, house,
 * industry, etc.
 * Names are formatted as CBID_<CLASS>_<CALLBACK>
 */
enum CallbackID : uint16_t {
	/** Set when using the callback resolve system, but not to resolve a callback. */
	CBID_NO_CALLBACK                     = 0x00,

	/** Set when calling a randomizing trigger (almost undocumented). */
	CBID_RANDOM_TRIGGER                  = 0x01,

	/* There are no callbacks 0x02 - 0x0F. */

	/** Visual effects and wagon power. */
	CBID_VEHICLE_VISUAL_EFFECT           = 0x10, // 8 bit callback

	/** Vehicle length, returns the amount of 1/8's the vehicle is shorter for trains and RVs. */
	CBID_VEHICLE_LENGTH                  = 0x11,

	/** Determine the amount of cargo to load per unit of time when using gradual loading. */
	CBID_VEHICLE_LOAD_AMOUNT             = 0x12, // 8 bit callback

	/** Determine whether a newstation should be made available to build. */
	CBID_STATION_AVAILABILITY            = 0x13, // 8 bit callback

	/** Choose a tile layout to draw, instead of the standard range. */
	CBID_STATION_DRAW_TILE_LAYOUT        = 0x14,

	/**
	 * Refit capacity, the passed vehicle needs to have its ->cargo_type set to
	 * the cargo we are refitting to, returns the new cargo capacity.
	 */
	CBID_VEHICLE_REFIT_CAPACITY          = 0x15, // 15 bit callback

	/** Builds articulated engines for trains and RVs. */
	CBID_VEHICLE_ARTIC_ENGINE            = 0x16, // 8 bit callback for grf version < 8

	/** Determine whether the house can be built on the specified tile. */
	CBID_HOUSE_ALLOW_CONSTRUCTION        = 0x17, // 8 bit callback

	/** AI construction/purchase selection */
	CBID_GENERIC_AI_PURCHASE_SELECTION   = 0x18, // 8 bit callback, implemented for stations only

	/** Determine the cargo "suffixes" for each refit possibility of a cargo. */
	CBID_VEHICLE_CARGO_SUFFIX            = 0x19,

	/** Determine the next animation frame for a house. */
	CBID_HOUSE_ANIMATION_NEXT_FRAME      = 0x1A, // 15 bit callback

	/** Called for periodically starting or stopping the animation. */
	CBID_HOUSE_ANIMATION_TRIGGER_TILE_LOOP = 0x1B, // 15 bit callback

	/** Called whenever the construction stage of a house changes. */
	CBID_HOUSE_ANIMATION_TRIGGER_CONSTRUCTION_STAGE_CHANGED = 0x1C, // 15 bit callback

	/** Determine whether a wagon can be attached to an already existing train. */
	CBID_TRAIN_ALLOW_WAGON_ATTACH        = 0x1D,

	/** Called to determine the colour of a town building. */
	CBID_HOUSE_COLOUR                    = 0x1E, // 15 bit callback

	/** Called to decide how much cargo a town building can accept. */
	CBID_HOUSE_CARGO_ACCEPTANCE          = 0x1F, // 15 bit callback

	/** Called to indicate how long the current animation frame should last. */
	CBID_HOUSE_ANIMATION_SPEED           = 0x20, // 8 bit callback

	/** Called periodically to determine if a house should be destroyed. */
	CBID_HOUSE_DESTRUCTION               = 0x21, // 8 bit callback

	/** Called to determine if the given industry type is available. For grf version >= 8 also a probability can be returned. */
	CBID_INDUSTRY_PROBABILITY            = 0x22, // 15 bit callback

	/**
	 * This callback is called from vehicle purchase lists. It returns a value to be
	 * used as a custom string ID in the 0xD000 range.
	 */
	CBID_VEHICLE_ADDITIONAL_TEXT         = 0x23,

	/** Called when building a station to customize the tile layout */
	CBID_STATION_BUILD_TILE_LAYOUT       = 0x24, // 15 bit callback

	/** Called for periodically starting or stopping the animation. */
	CBID_INDTILE_ANIMATION_TRIGGER       = 0x25, // 15 bit callback

	/** Called to determine industry tile next animation frame. */
	CBID_INDTILE_ANIMATION_NEXT_FRAME    = 0x26, // 15 bit callback

	/** Called to indicate how long the current animation frame should last. */
	CBID_INDTILE_ANIMATION_SPEED         = 0x27, // 8 bit callback

	/** Called to determine if the given industry can be built on specific area. */
	CBID_INDUSTRY_LOCATION               = 0x28, // 15 bit callback

	/** Called on production changes, so it can be adjusted. */
	CBID_INDUSTRY_PRODUCTION_CHANGE      = 0x29, // 15 bit callback

	/** Called to determine which cargoes a town building should accept. */
	CBID_HOUSE_ACCEPT_CARGO              = 0x2A, // 15 bit callback

	/** Called to query the cargo acceptance of the industry tile */
	CBID_INDTILE_CARGO_ACCEPTANCE        = 0x2B, // 15 bit callback

	/** Called to determine which cargoes an industry should accept. */
	CBID_INDTILE_ACCEPT_CARGO            = 0x2C, // 15 bit callback

	/**
	 * Called to determine if a specific colour map should be used for a vehicle
	 * instead of the default livery.
	 */
	CBID_VEHICLE_COLOUR_MAPPING          = 0x2D, // 15 bit callback

	/** Called to determine how much cargo a town building produces. */
	CBID_HOUSE_PRODUCE_CARGO             = 0x2E, // 15 bit callback

	/** Called to determine if the given industry tile can be built on specific tile. */
	CBID_INDTILE_SHAPE_CHECK             = 0x2F, // 15 bit callback

	/** Called to determine the type (if any) of foundation to draw for industry tile. */
	CBID_INDTILE_DRAW_FOUNDATIONS        = 0x30, // 15 bit callback

	/**
	 * Called when the company (or AI) tries to start or stop a vehicle. Mainly
	 * used for preventing a vehicle from leaving the depot.
	 */
	CBID_VEHICLE_START_STOP_CHECK        = 0x31, // 15 bit callback, but 0xFF test is done with 8 bit

	/** Called for every vehicle every 32 days (not all on same date though). */
	CBID_VEHICLE_32DAY_CALLBACK          = 0x32, // 2 bit callback

	/** Called to play a special sound effect */
	CBID_VEHICLE_SOUND_EFFECT            = 0x33, // 15 bit callback

	/** Return the vehicles this given vehicle can be "upgraded" to. */
	CBID_VEHICLE_AUTOREPLACE_SELECTION   = 0x34, // 15 bit callback, not implemented

	/** Called monthly on production changes, so it can be adjusted more frequently */
	CBID_INDUSTRY_MONTHLYPROD_CHANGE     = 0x35, // 15 bit callback

	/**
	 * Called to modify various vehicle properties. Callback parameter 1
	 * specifies the property index, as used in Action 0, to change.
	 */
	CBID_VEHICLE_MODIFY_PROPERTY         = 0x36, // 8/15 bit depends on queried property

	/** Called to determine text to display after cargo name */
	CBID_INDUSTRY_CARGO_SUFFIX           = 0x37, // 15 bit callback, but 0xFF test is done with 8 bit

	/** Called to determine more text in the fund industry window */
	CBID_INDUSTRY_FUND_MORE_TEXT         = 0x38, // 15 bit callback

	/** Called to calculate the income of delivered cargo */
	CBID_CARGO_PROFIT_CALC               = 0x39, // 15 bit callback

	/** Called to determine more text in the industry window */
	CBID_INDUSTRY_WINDOW_MORE_TEXT       = 0x3A, // 15 bit callback

	/** Called to determine industry special effects */
	CBID_INDUSTRY_SPECIAL_EFFECT         = 0x3B, // 15 bit callback

	/** Called to determine if industry can alter the ground below industry tile */
	CBID_INDTILE_AUTOSLOPE               = 0x3C, // 15 bit callback

	/** Called to determine if the industry can still accept or refuse more cargo arrival */
	CBID_INDUSTRY_REFUSE_CARGO           = 0x3D, // 15 bit callback

	/* There are no callbacks 0x3E - 0x13F */

	/** Called for periodically starting or stopping the animation. */
	CBID_STATION_ANIMATION_TRIGGER       = 0x140, // 15 bit callback

	/** Called to determine station tile next animation frame. */
	CBID_STATION_ANIMATION_NEXT_FRAME    = 0x141, // 15 bit callback

	/** Called to indicate how long the current animation frame should last. */
	CBID_STATION_ANIMATION_SPEED         = 0x142, // 8 bit callback

	/** Called to determine whether a town building can be destroyed. */
	CBID_HOUSE_DENY_DESTRUCTION          = 0x143, // 15 bit callback

	/** Select an ambient sound to play for a given type of tile. */
	CBID_SOUNDS_AMBIENT_EFFECT           = 0x144, // 15 bit callback

	/** Called to calculate part of a station rating. */
	CBID_CARGO_STATION_RATING_CALC       = 0x145, // 15 bit callback

	/** Allow signal sprites to be replaced dynamically. */
	CBID_NEW_SIGNALS_SPRITE_DRAW         = 0x146, // 15 bit callback, not implemented

	/** Add an offset to the default sprite numbers to show another sprite. */
	CBID_CANALS_SPRITE_OFFSET            = 0x147, // 15 bit callback

	/** Called when a cargo type specified in property 20 is accepted. */
	CBID_HOUSE_ANIMATION_TRIGGER_WATCHED_CARGO_ACCEPTED = 0x148, // 15 bit callback

	/** Callback done for each tile of a station to check the slope. */
	CBID_STATION_LAND_SLOPE_CHECK        = 0x149, // 15 bit callback

	/** Called to determine the colour of an industry. */
	CBID_INDUSTRY_DECIDE_COLOUR          = 0x14A, // 4 bit callback

	/** Customize the input cargo types of a newly build industry. */
	CBID_INDUSTRY_INPUT_CARGO_TYPES      = 0x14B, // 8 bit callback

	/** Customize the output cargo types of a newly build industry. */
	CBID_INDUSTRY_OUTPUT_CARGO_TYPES     = 0x14C, // 8 bit callback

	/** Called on the Get Tile Description for an house tile. */
	CBID_HOUSE_CUSTOM_NAME               = 0x14D, // 15 bit callback

	/** Called to determine the type (if any) of foundation to draw for house tile. */
	CBID_HOUSE_DRAW_FOUNDATIONS          = 0x14E, // 15 bit callback

	/** Called to determine if one can alter the ground below a house tile */
	CBID_HOUSE_AUTOSLOPE                 = 0x14F, // 15 bit callback

	/** Called to determine the type (if any) of foundation to draw for an airport tile. */
	CBID_AIRPTILE_DRAW_FOUNDATIONS       = 0x150, // 15 bit callback

	/** Called for periodically starting or stopping the animation. */
	CBID_AIRPTILE_ANIMATION_TRIGGER      = 0x152, // 15 bit callback

	/** Called to determine airport tile next animation frame. */
	CBID_AIRPTILE_ANIMATION_NEXT_FRAME   = 0x153, // 15 bit callback

	/** Called to indicate how long the current animation frame should last. */
	CBID_AIRPTILE_ANIMATION_SPEED        = 0x154, // 8 bit callback

	/**
	 * This callback is called from airport list. It returns a value to be
	 * used as a custom string ID in the 0xD000 range.
	 */
	CBID_AIRPORT_ADDITIONAL_TEXT         = 0x155, // 15 bit callback

	/** Called to determine text to show as airport layout name. */
	CBID_AIRPORT_LAYOUT_NAME             = 0x156, // 15 bit callback

	/** Callback done for each tile of an object to check the slope. */
	CBID_OBJECT_LAND_SLOPE_CHECK         = 0x157, // 15 bit callback

	/** Determine the next animation frame for a house. */
	CBID_OBJECT_ANIMATION_NEXT_FRAME     = 0x158, // 15 bit callback

	/** Called for periodically starting or stopping the animation. */
	CBID_OBJECT_ANIMATION_TRIGGER        = 0x159, // 15 bit callback

	/** Called to indicate how long the current animation frame should last. */
	CBID_OBJECT_ANIMATION_SPEED          = 0x15A, // 8 bit callback

	/** Called to determine the colour of a town building. */
	CBID_OBJECT_COLOUR                   = 0x15B, // 15 bit callback

	/** Called to determine more text in the fund object window */
	CBID_OBJECT_FUND_MORE_TEXT           = 0x15C, // 15 bit callback

	/** Called to determine if one can alter the ground below an object tile */
	CBID_OBJECT_AUTOSLOPE                = 0x15D, // 15 bit callback

	/** Called to determine the cost factor for refitting a vehicle. */
	CBID_VEHICLE_REFIT_COST              = 0x15E, // 15 bit callback

	/** Called when industry is built to set initial production level. */
	CBID_INDUSTRY_PROD_CHANGE_BUILD      = 0x15F, // 15 bit callback

	/** Called to spawn visual effects for vehicles. */
	CBID_VEHICLE_SPAWN_VISUAL_EFFECT     = 0x160, // 15 bit callback

	/** Called to determine the engine name to show. */
	CBID_VEHICLE_NAME                    = 0x161, // 15 bit callback

	/** Called to determine probability during build. */
	CBID_VEHICLE_BUILD_PROBABILITY       = 0x162, // 15 bit callback

	/**
	 * Called to get custom engine refit mask. Called once
	 * for each defined cargo after all NewGRFs are loaded.
	 */
	CBID_VEHICLE_CUSTOM_REFIT            = 0x0163, // 15 bit callback
};

/**
 * Callback masks for vehicles, indicates which callbacks are used by a vehicle.
 * Some callbacks are always used and don't have a mask.
 */
enum class VehicleCallbackMask : uint8_t {
	VisualEffect  = 0, ///< Visual effects and wagon power (trains, road vehicles and ships)
	Length        = 1, ///< Vehicle length (trains and road vehicles)
	LoadAmount    = 2, ///< Load amount
	RefitCapacity = 3, ///< Cargo capacity after refit
	ArticEngine   = 4, ///< Add articulated engines (trains and road vehicles)
	CargoSuffix   = 5, ///< Show suffix after cargo name
	ColourRemap   = 6, ///< Change colour mapping of vehicle
	SoundEffect   = 7, ///< Vehicle uses custom sound effects
	Name          = 8, ///< Engine name
	CustomRefit   = 9, ///< Custom refit mask
};
using VehicleCallbackMasks = EnumBitSet<VehicleCallbackMask, uint16_t>;

/**
 * Callback masks for stations.
 */
enum class StationCallbackMask : uint8_t {
	Avail              = 0, ///< Availability of station in construction window
	DrawTileLayout     = 1, ///< Use callback to select a tile layout to use when drawing.
	AnimationNextFrame = 2, ///< Use a custom next frame callback
	AnimationSpeed     = 3, ///< Customize the animation speed of the station
	SlopeCheck         = 4, ///< Check slope of new station tiles
};
using StationCallbackMasks = EnumBitSet<StationCallbackMask, uint16_t>;

/**
 * Callback masks for road stops.
 */
enum class RoadStopCallbackMask : uint8_t {
	Avail              = 0, ///< Availability of road stop in construction window
	AnimationNextFrame = 1, ///< Use a custom next frame callback
	AnimationSpeed     = 2, ///< Customize the animation speed of the road stop
};
using RoadStopCallbackMasks = EnumBitSet<RoadStopCallbackMask, uint8_t>;

/**
 * Callback masks for houses.
 */
enum class HouseCallbackMask : uint8_t {
	AllowConstruction       =  0, ///< decide whether the house can be built on a given tile
	AnimationNextFrame      =  1, ///< decides next animation frame
	AnimationTriggerTileLoop = 2, ///< periodically start/stop the animation
	AnimationTriggerConstructionStageChanged = 3, ///< change animation when construction stage changes
	Colour                  =  4, ///< decide the colour of the building
	CargoAcceptance         =  5, ///< decides amount of cargo acceptance
	AnimationSpeed          =  6, ///< decides animation speed
	Destruction             =  7, ///< trigger destruction of building
	AcceptCargo             =  8, ///< decides accepted types
	ProduceCargo            =  9, ///< custom cargo production
	DenyDestruction         = 10, ///< conditional protection
	DrawFoundations         = 11, ///< decides if default foundations need to be drawn
	Autoslope               = 12, ///< decides allowance of autosloping
};
using HouseCallbackMasks = EnumBitSet<HouseCallbackMask, uint16_t>;

/**
 * Callback masks for canals.
 */
enum class CanalCallbackMask : uint8_t {
	SpriteOffset       = 0, ///< Enable add sprite offset callback
};
using CanalCallbackMasks = EnumBitSet<CanalCallbackMask, uint8_t>;

/**
 * Callback masks for cargoes.
 */
enum class CargoCallbackMask : uint8_t {
	ProfitCalc        = 0, ///< custom profit calculation
	StationRatingCalc = 1, ///< custom station rating for this cargo type
};
using CargoCallbackMasks = EnumBitSet<CargoCallbackMask, uint8_t>;

/**
 * Callback masks for Industries
 */
enum class IndustryCallbackMask : uint8_t {
	Probability            =  0, ///< industry availability/probability callback
	ProductionCargoArrival =  1, ///< call production callback when cargo arrives at the industry
	Production256Ticks     =  2, ///< call production callback every 256 ticks
	Location               =  3, ///< check industry construction on given area
	ProductionChange       =  4, ///< controls random production change
	MonthlyProdChange      =  5, ///< controls monthly random production change
	CargoSuffix            =  6, ///< cargo sub-type display
	FundMoreText           =  7, ///< additional text in fund window
	WindowMoreText         =  8, ///< additional text in industry window
	SpecialEffect          =  9, ///< control special effects
	RefuseCargo            = 10, ///< option out of accepting cargo
	DecideColour           = 11, ///< give a custom colour to newly build industries
	InputCargoTypes        = 12, ///< customize the cargoes the industry requires
	OutputCargoTypes       = 13, ///< customize the cargoes the industry produces
	ProdChangeBuild        = 14, ///< initialise production level on construction
};
using IndustryCallbackMasks = EnumBitSet<IndustryCallbackMask, uint16_t>;

/**
 * Callback masks for industry tiles
 */
enum class IndustryTileCallbackMask : uint8_t {
	AnimationNextFrame = 0, ///< decides next animation frame
	AnimationSpeed     = 1, ///< decides animation speed
	CargoAcceptance    = 2, ///< decides amount of cargo acceptance
	AcceptCargo        = 3, ///< decides accepted types
	ShapeCheck         = 4, ///< decides slope suitability
	DrawFoundations    = 5, ///< decides if default foundations need to be drawn
	Autoslope          = 6, ///< decides allowance of autosloping
};
using IndustryTileCallbackMasks = EnumBitSet<IndustryTileCallbackMask, uint8_t>;

/**
 * Callback masks for objects
 */
enum class ObjectCallbackMask : uint8_t {
	SlopeCheck         =  0, ///< decides slope suitability
	AnimationNextFrame =  1, ///< decides next animation frame
	AnimationSpeed     =  2, ///< decides animation speed
	Colour             =  3, ///< decide the colour of the building
	FundMoreText       =  4, ///< additional text in fund window
	Autoslope          =  5, ///< decides allowance of autosloping
};
using ObjectCallbackMasks = EnumBitSet<ObjectCallbackMask, uint8_t>;

/**
 * Callback masks for airport tiles
 */
enum class AirportTileCallbackMask : uint8_t {
	AnimationNextFrame = 0, ///< decides next animation frame
	AnimationSpeed     = 1, ///< decides animation speed
	ShapeCheck         = 4, ///< decides slope suitability
	DrawFoundations    = 5, ///< decides if default foundations need to be drawn
	Autoslope          = 6, ///< decides allowance of autosloping
};
using AirportTileCallbackMasks = EnumBitSet<AirportTileCallbackMask, uint8_t>;

/**
 * Different values for Callback result evaluations
 */
static const uint CALLBACK_FAILED              = 0xFFFF; ///< Result of a failed callback.
static const uint CALLBACK_HOUSEPRODCARGO_END  = 0x20FF; ///< Sentinel indicating that the loop for CBID_HOUSE_PRODUCE_CARGO has ended

#endif /* NEWGRF_CALLBACKS_H */
