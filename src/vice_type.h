/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vice_type.h Types related to the City Vice system. */

#ifndef VICE_TYPE_H
#define VICE_TYPE_H

#include "core/enum_type.hpp"

/** Types of vice events that can occur in a town. */
enum class ViceEventType : uint8_t {
	PettyVandalism,   ///< Station rating hit on one station.
	VehicleSabotage,  ///< One vehicle in town radius gets a breakdown.
	Arson,            ///< One house destroyed by fire.
	Riot,             ///< 1-3 houses destroyed + station rating hit + company rating hit.
	CrimeWave,        ///< All stations get rating penalty for 3 months.
	End,              ///< End marker.
};

/** Vice severity thresholds for display and notifications. */
enum class ViceSeverity : uint8_t {
	Peaceful,   ///< Vice level 0-10.
	Low,        ///< Vice level 11-20.
	Moderate,   ///< Vice level 21-35.
	Elevated,   ///< Vice level 36-50.
	High,       ///< Vice level 51-75.
	Critical,   ///< Vice level 76-100.
};

/** Difficulty severity levels for vice events. */
enum class ViceDifficulty : uint8_t {
	Mild   = 0, ///< Only Vandalism and Sabotage; 3-month cooldown.
	Normal = 1, ///< All event types; 2-month cooldown.
	Harsh  = 2, ///< All event types, x1.5 frequency, 4 max riot buildings; 1-month cooldown.
};

/** Data for each crime-reduction campaign tier. */
struct PolicingTierData {
	uint8_t reduction; ///< Vice level reduction.
	uint8_t months;    ///< Duration in months.
};

/** Lookup table for crime-reduction campaign tiers, ordered by TownAction::CrimePatrol..CrimePeacekeeping. */
static constexpr PolicingTierData _policing_tier_data[] = {
	{ 10,  3 }, // CrimePatrol      (low)
	{ 20,  6 }, // CrimeSecurity    (medium)
	{ 30, 12 }, // CrimePeacekeeping (high)
};

/** Lookup table for crime-incitement campaign tiers, ordered by TownAction::CrimeInciteLow..CrimeInciteHigh. */
static constexpr PolicingTierData _inciting_tier_data[] = {
	{ 10,  3 }, // CrimeInciteLow    (low)
	{ 20,  6 }, // CrimeInciteMedium (medium)
	{ 30, 12 }, // CrimeInciteHigh   (high)
};

#endif /* VICE_TYPE_H */
