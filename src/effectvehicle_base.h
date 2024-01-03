/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file effectvehicle_base.h Base class for all effect vehicles. */

#ifndef EFFECTVEHICLE_BASE_H
#define EFFECTVEHICLE_BASE_H

#include "vehicle_base.h"
#include "transparency.h"

/**
 * A special vehicle is one of the following:
 *  - smoke
 *  - electric sparks for trains
 *  - explosions
 *  - bulldozer (road works)
 *  - bubbles (industry)
 */
struct EffectVehicle FINAL : public SpecializedVehicle<EffectVehicle, VEH_EFFECT> {
	uint16_t animation_state;  ///< State primarily used to change the graphics/behaviour.
	byte animation_substate; ///< Sub state to time the change of the graphics/behaviour.

	/** We don't want GCC to zero our struct! It already is zeroed and has an index! */
	EffectVehicle() : SpecializedVehicleBase() {}
	/** We want to 'destruct' the right class. */
	virtual ~EffectVehicle() = default;

	void UpdateDeltaXY() override;
	bool Tick() override;
	TransparencyOption GetTransparencyOption() const;
};

#endif /* EFFECTVEHICLE_BASE_H */
