/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file livery.h Functions/types related to livery colours. */

#ifndef LIVERY_H
#define LIVERY_H

#include "core/enum_type.hpp"
#include "company_type.h"
#include "gfx_type.h"

static const uint8_t LIT_NONE    = 0; ///< Don't show the liveries at all
static const uint8_t LIT_COMPANY = 1; ///< Show the liveries of your own company
static const uint8_t LIT_ALL     = 2; ///< Show the liveries of all companies

/** List of different livery schemes. */
enum class LiveryScheme : uint8_t {
	Begin = 0, ///< Begin marker.
	Default = 0, ///< Default scheme.

	/* Rail vehicles */
	Steam, ///< Steam engines.
	Diesel, ///< Diesel engines.
	Electric, ///< Electric engines.
	Monorail, ///< Monorail engines.
	Maglev, ///< Maglev engines.
	DMU, ///< DMUs and their passenger wagons.
	EMU, ///< EMUs and their passenger wagons.
	PassengerWagonSteam, ///< Passenger wagons attached to steam engines.
	PassengerWagonDiesel, ///< Passenger wagons attached to diesel engines.
	PassengerWagonElectric, ///< Passenger wagons attached to electric engines.
	PassengerWagonMonorail, ///< Passenger wagons attached to monorail engines.
	PassengerWagonMaglev, ///< Passenger wagons attached to maglev engines.
	FreightWagon, ///< Freight wagons.

	/* Road vehicles */
	Bus, ///< Buses.
	Truck, ///< Trucks.

	/* Ships */
	PassengerShip, ///< Passenger ships.
	FreightShip, ///< Freight ships.

	/* Aircraft */
	Helicopter, ///< Helicopters.
	SmallPlane, ///< Small aeroplanes.
	LargePlane, ///< Large aeroplanes.

	/* Trams (appear on Road Vehicles tab) */
	PassengerTram, ///< Passenger trams.
	FreightTram, ///< Freight trams.

	End, ///< End marker.
};

DECLARE_INCREMENT_DECREMENT_OPERATORS(LiveryScheme)
DECLARE_ENUM_AS_ADDABLE(LiveryScheme)

/** Bitset of \c LiveryScheme elements. */
using LiverySchemes = EnumBitSet<LiveryScheme, uint32_t, LiveryScheme::End>;

/** List of different livery classes, used only by the livery GUI. */
enum class LiveryClass : uint8_t {
	Other, ///< General livery schemes.
	Rail, ///< Rail livery schemes.
	Road, ///< Road livery schemes.
	Ship, ///< Ship livery schemes.
	Aircraft, ///< Aircraft livery schemes.
	GroupRail, ///< Rail group.
	GroupRoad, ///< Road group.
	GroupShip, ///< Ship group.
	GroupAircraft, ///< Aircraft group.
};

DECLARE_ENUM_AS_ADDABLE(LiveryClass)

/** Information about a particular livery. */
struct Livery {
	/** Flags for bitmask to declare which of the colours are set. */
	enum class Flag : uint8_t {
		Primary = 0, ///< Primary colour is set.
		Secondary = 1, ///< Secondary colour is set.
	};

	/** Bitset of \c Flag elements. */
	using Flags = EnumBitSet<Flag, uint8_t>;

	Flags in_use{}; ///< Livery flags.
	Colours colour1 = Colours::Begin; ///< First colour, for all vehicles.
	Colours colour2 = Colours::Begin; ///< Second colour, for vehicles with 2CC support.

	/**
	 * Get offset for recolour palette.
	 * @param use_secondary Specify whether to add secondary colour offset to the result.
	 * @return The palette offset.
	 */
	inline uint8_t GetRecolourOffset(bool use_secondary = true) const
	{
		return use_secondary ? to_underlying(this->colour1) + to_underlying(this->colour2) * 16 : to_underlying(this->colour1);
	}
};

void ResetCompanyLivery(Company *c);

#endif /* LIVERY_H */
