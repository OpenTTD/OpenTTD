/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file screensaver.cpp Code for automatically following vehicles for a screensaver effect. */
#include <iostream>
#include <memory>

#include "stdafx.h"
#include "core/random_func.hpp"
#include "timer/timer.h"
#include "timer/timer_game_tick.h"
#include "vehicle_base.h"
#include "vehicle_func.h"
#include "vehicle_type.h"
#include "window_func.h"
#include "safeguards.h"


/**
 * Selects a random vehicle from all vehicles.
 *
 * This function can return nullptr (aka no vehicle was picked) if:
 * - There are no vehicles
 *
 * @return A pointer to the selected vehicle or nullptr if not found
 */
static Vehicle *PickRandomVehicle()
{
	// first pass, add up vehicles of all buildable types for all companies
	uint n = 0;
	for (const Company *c : Company::Iterate()) {
		for (VehicleType v = VEH_BEGIN; v < VEH_COMPANY_END; ++v) {
			n += c->group_all[v].num_vehicle;
		}
	}

	// If we have no vehicles, we can't pick a vehicle so exit
	if (n == 0) {
		return nullptr;
	}

	// otherwise, pick a random vehicle index between 0 and n
	n = InteractiveRandomRange(n);

	// loop over all the vehicles until we get to that vehicle
	for (Vehicle *v : Vehicle::Iterate()) {
		// skip count anything companies can't build
		if (!IsCompanyBuildableVehicleType(v)) {
			continue;
		}

		// skip vehicles if they're not "front vehicles" so we don't count train cars (or similar) as targets to follow
		if (!v->IsPrimaryVehicle()) {
			continue;
		}

		// otherwise, it is a vehicle we should count, so count down until zero
		if (n-- == 0) {
			// return it
			return v;
		}
	}

	// In case we go over all the vehicles, return nullptr in that case
	return nullptr;
}

/**
 * switches the current followed vehicle to a new random one.
 *
 * This is intended to be used as the callback of an IntervalTimer object to periodically
 * update the main window's followed vehicle so that it rotates between randomly selected
 * vehicles in a "screensaver" effect.
 *
 * @see PickRandomVehicle
 *
 * @todo Improve the screensaver effect or make it customizable?
 */
static void switchVehicle()
{
	const Window *main_win = GetMainWindow();

	// We start with a high chance of keeping with a moving vehicle
	static int movement_bias = 48;

	// if we're targeting a vehicle, we have a chance to return early and not pick a new one
	auto curV = main_win->viewport->follow_vehicle;
	auto v = Vehicle::GetIfValid(curV);

	if (v != nullptr) {
		// Every time we stick on a moving vehicle, we make it less likely that
		// we stick around next cycle to prevent staying on one vehicle for forever.
		if (v->cur_speed > 0 && !Chance16I(1, movement_bias, InteractiveRandom())) {
			movement_bias -= 1;
			return;
		}

		// If we're not moving, we have a flat 25% chance to stay until the call
		// to encourage switching to a moving vehicle.
		if (v->cur_speed == 0 && Chance16I(3, 4, InteractiveRandom())) {
			return;
		}
	}

	// reset our bias so that the next vehicle gets its full time
	movement_bias = 48;

	// try to get a vehicle (this fails if the vehicle picked isn't valid to follow)
	Vehicle *new_vehicle = PickRandomVehicle();
	// if we didn't find vehicle to follow, try again next cycle
	if (new_vehicle == nullptr) {
		return;
	}

	// swap the main window over to following the new vehicle
	main_win->viewport->CancelFollow(*main_win);
	main_win->viewport->follow_vehicle = new_vehicle->index;
}


// The screensaver is based off a timer on a fixed interval which rotates between vehicles

/**
 * We use a fixed period for updating the screensaver's followed vehicle, stored in this variable
 *
 * @see ToggleScreensaverMode
 */
const static auto _screensaver_check_interval = TimerGameTick::TPeriod(TimerGameTick::Priority::None, 128);

/**
 * The screensaver runs on an interval timer, stored in this pointer when it is running and freed when
 * it is no longer in use.
 *
 * @see ToggleScreensaverMode
 */
static std::unique_ptr<IntervalTimer<TimerGameTick>> _switchTimer;

/**
 * Enters or exits "Screensaver Mode"
 *
 * Screensaver mode is simply rotating the main window's followed vehicle every so often to show off
 * a world full of moving trains, planes, boats, and automobiles.
 *
 * If not activated, the function allocates a new IntervalTimer which fires every so often to pick
 * new vehicles to follow.  There is some randomness in this to keep things interesting.
 *
 * If "Screensaver Mode" was previously activated, this instead deletes the aforementioned timer and
 * stops following vehicles, returning the camera to the original position.
 *
 * @see switchVehicle
 */
void ToggleScreensaverMode()
{
	const Window *main_win = GetMainWindow();

	// If we are in screensaver mode, exit it
	if (_switchTimer != nullptr) {
		_switchTimer.reset();
		main_win->viewport->CancelFollow(*main_win);
		return;
	}

	// If we aren't, start it with a new timer
	_switchTimer = std::make_unique<IntervalTimer<TimerGameTick>>(
		_screensaver_check_interval,
		[]([[maybe_unused]] uint _unused) { switchVehicle(); }
	);

	// fire our callback once to start us following a first vehicle;
	// otherwise we'd have to wait for the first time the timer fired
	switchVehicle();
}
