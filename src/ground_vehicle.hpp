/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ground_vehicle.hpp Base class and functions for all vehicles that move through ground. */

#ifndef GROUND_VEHICLE_HPP
#define GROUND_VEHICLE_HPP

#include "vehicle_base.h"
#include "landscape.h"

/** What is the status of our acceleration? */
enum AccelStatus {
	AS_ACCEL, ///< We want to go faster, if possible of course.
	AS_BRAKE  ///< We want to stop.
};

/**
 * Cached, frequently calculated values.
 * All of these values except cached_slope_resistance are set only for the first part of a vehicle.
 */
struct GroundVehicleCache {
	/* Cached acceleration values, recalculated when the cargo on a vehicle changes (in addition to the conditions below) */
	uint32 cached_weight;           ///< Total weight of the consist (valid only for the first engine).
	uint32 cached_slope_resistance; ///< Resistance caused by weight when this vehicle part is at a slope.
	uint32 cached_max_te;           ///< Maximum tractive effort of consist (valid only for the first engine).
	uint16 cached_axle_resistance;  ///< Resistance caused by the axles of the vehicle (valid only for the first engine).

	/* Cached acceleration values, recalculated on load and each time a vehicle is added to/removed from the consist. */
	uint16 cached_max_track_speed;  ///< Maximum consist speed limited by track type (valid only for the first engine).
	uint32 cached_power;            ///< Total power of the consist (valid only for the first engine).
	uint32 cached_air_drag;         ///< Air drag coefficient of the vehicle (valid only for the first engine).

	/* Cached NewGRF values, recalculated on load and each time a vehicle is added to/removed from the consist. */
	uint16 cached_total_length;     ///< Length of the whole vehicle (valid only for the first engine).
	EngineID first_engine;          ///< Cached EngineID of the front vehicle. INVALID_ENGINE for the front vehicle itself.
	uint8 cached_veh_length;        ///< Length of this vehicle in units of 1/8 of normal length. It is cached because this can be set by a callback.
};

/** Ground vehicle flags. */
enum GroundVehicleFlags {
	GVF_GOINGUP_BIT       = 0,
	GVF_GOINGDOWN_BIT     = 1,
};

/**
 * Base class for all vehicles that move through ground.
 *
 * Child classes must define all of the following functions.
 * These functions are not defined as pure virtual functions at this class to improve performance.
 *
 * virtual uint16      GetPower() const = 0;
 * virtual uint16      GetPoweredPartPower(const T *head) const = 0;
 * virtual uint16      GetWeight() const = 0;
 * virtual byte        GetTractiveEffort() const = 0;
 * virtual byte        GetAirDrag() const = 0;
 * virtual byte        GetAirDragArea() const = 0;
 * virtual AccelStatus GetAccelerationStatus() const = 0;
 * virtual uint16      GetCurrentSpeed() const = 0;
 * virtual uint32      GetRollingFriction() const = 0;
 * virtual int         GetAccelerationType() const = 0;
 * virtual int32       GetSlopeSteepness() const = 0;
 * virtual int         GetDisplayMaxSpeed() const = 0;
 * virtual uint16      GetMaxTrackSpeed() const = 0;
 * virtual bool        TileMayHaveSlopedTrack() const = 0;
 */
template <class T, VehicleType Type>
struct GroundVehicle : public SpecializedVehicle<T, Type> {
	GroundVehicleCache gcache; ///< Cache of often calculated values.
	uint16 gv_flags;           ///< @see GroundVehicleFlags.

	/**
	 * The constructor at SpecializedVehicle must be called.
	 */
	GroundVehicle() : SpecializedVehicle<T, Type>() {}

	void PowerChanged();
	void CargoChanged();
	int GetAcceleration() const;

	/**
	 * Calculates the total slope resistance for this vehicle.
	 * @return Slope resistance.
	 */
	FORCEINLINE int32 GetSlopeResistance() const
	{
		int32 incl = 0;

		for (const T *u = T::From(this); u != NULL; u = u->Next()) {
			if (HasBit(u->gv_flags, GVF_GOINGUP_BIT)) {
				incl += u->gcache.cached_slope_resistance;
			} else if (HasBit(u->gv_flags, GVF_GOINGDOWN_BIT)) {
				incl -= u->gcache.cached_slope_resistance;
			}
		}

		return incl;
	}

	/**
	 * Checks if the vehicle is in a slope and sets the required flags in that case.
	 * @param new_tile True if the vehicle reached a new tile.
	 * @param turned Indicates if the vehicle has turned.
	 * @return Old height of the vehicle.
	 */
	FORCEINLINE byte UpdateInclination(bool new_tile, bool turned)
	{
		byte old_z = this->z_pos;

		if (new_tile) {
			this->z_pos = GetSlopeZ(this->x_pos, this->y_pos);
			ClrBit(this->gv_flags, GVF_GOINGUP_BIT);
			ClrBit(this->gv_flags, GVF_GOINGDOWN_BIT);

			if (T::From(this)->TileMayHaveSlopedTrack()) {
				/* To check whether the current tile is sloped, and in which
				 * direction it is sloped, we get the 'z' at the center of
				 * the tile (middle_z) and the edge of the tile (old_z),
				 * which we then can compare. */
				static const int HALF_TILE_SIZE = TILE_SIZE / 2;
				static const int INV_TILE_SIZE_MASK = ~(TILE_SIZE - 1);

				byte middle_z = GetSlopeZ((this->x_pos & INV_TILE_SIZE_MASK) | HALF_TILE_SIZE, (this->y_pos & INV_TILE_SIZE_MASK) | HALF_TILE_SIZE);

				if (middle_z != this->z_pos) {
					SetBit(this->gv_flags, (middle_z > old_z) ? GVF_GOINGUP_BIT : GVF_GOINGDOWN_BIT);
				}
			}
		} else {
			/* Flat tile, tile with two opposing corners raised and tile with 3 corners
			 * raised can never have sloped track ... */
			static const uint32 never_sloped = 1 << SLOPE_FLAT | 1 << SLOPE_EW | 1 << SLOPE_NS | 1 << SLOPE_NWS | 1 << SLOPE_WSE | 1 << SLOPE_SEN | 1 << SLOPE_ENW;
			/* ... unless it's a bridge head. */
			if (IsTileType(this->tile, MP_TUNNELBRIDGE) || // the following check would be true for tunnels anyway
					(T::From(this)->TileMayHaveSlopedTrack() && !HasBit(never_sloped, GetTileSlope(this->tile, NULL)))) {
				this->z_pos = GetSlopeZ(this->x_pos, this->y_pos);
			} else {
				/* Verify that assumption. */
				assert(this->z_pos == GetSlopeZ(this->x_pos, this->y_pos));
			}
		}

		this->UpdateViewport(true, turned);
		return old_z;
	}

	/**
	 * Enum to handle ground vehicle subtypes.
	 * Do not access it directly unless you have to. Use the subtype access functions.
	 */
	enum GroundVehicleSubtypeFlags {
		GVSF_FRONT            = 0, ///< Leading engine of a consist.
		GVSF_ARTICULATED_PART = 1, ///< Articulated part of an engine.
		GVSF_WAGON            = 2, ///< Wagon (not used for road vehicles).
		GVSF_ENGINE           = 3, ///< Engine that can be front engine, but might be placed behind another engine (not used for road vehicles).
		GVSF_FREE_WAGON       = 4, ///< First in a wagon chain (in depot) (not used for road vehicles).
		GVSF_MULTIHEADED      = 5, ///< Engine is multiheaded (not used for road vehicles).
	};

	/**
	 * Set front engine state.
	 */
	FORCEINLINE void SetFrontEngine() { SetBit(this->subtype, GVSF_FRONT); }

	/**
	 * Remove the front engine state.
	 */
	FORCEINLINE void ClearFrontEngine() { ClrBit(this->subtype, GVSF_FRONT); }

	/**
	 * Set a vehicle to be an articulated part.
	 */
	FORCEINLINE void SetArticulatedPart() { SetBit(this->subtype, GVSF_ARTICULATED_PART); }

	/**
	 * Clear a vehicle from being an articulated part.
	 */
	FORCEINLINE void ClearArticulatedPart() { ClrBit(this->subtype, GVSF_ARTICULATED_PART); }

	/**
	 * Set a vehicle to be a wagon.
	 */
	FORCEINLINE void SetWagon() { SetBit(this->subtype, GVSF_WAGON); }

	/**
	 * Clear wagon property.
	 */
	FORCEINLINE void ClearWagon() { ClrBit(this->subtype, GVSF_WAGON); }

	/**
	 * Set engine status.
	 */
	FORCEINLINE void SetEngine() { SetBit(this->subtype, GVSF_ENGINE); }

	/**
	 * Clear engine status.
	 */
	FORCEINLINE void ClearEngine() { ClrBit(this->subtype, GVSF_ENGINE); }

	/**
	 * Set a vehicle as a free wagon.
	 */
	FORCEINLINE void SetFreeWagon() { SetBit(this->subtype, GVSF_FREE_WAGON); }

	/**
	 * Clear a vehicle from being a free wagon.
	 */
	FORCEINLINE void ClearFreeWagon() { ClrBit(this->subtype, GVSF_FREE_WAGON); }

	/**
	 * Set a vehicle as a multiheaded engine.
	 */
	FORCEINLINE void SetMultiheaded() { SetBit(this->subtype, GVSF_MULTIHEADED); }

	/**
	 * Clear multiheaded engine property.
	 */
	FORCEINLINE void ClearMultiheaded() { ClrBit(this->subtype, GVSF_MULTIHEADED); }

	/**
	 * Check if the vehicle is a front engine.
	 * @return Returns true if the vehicle is a front engine.
	 */
	FORCEINLINE bool IsFrontEngine() const { return HasBit(this->subtype, GVSF_FRONT); }

	/**
	 * Check if the vehicle is a free wagon (got no engine in front of it).
	 * @return Returns true if the vehicle is a free wagon.
	 */
	FORCEINLINE bool IsFreeWagon() const { return HasBit(this->subtype, GVSF_FREE_WAGON); }

	/**
	 * Check if a vehicle is an engine (can be first in a consist).
	 * @return Returns true if vehicle is an engine.
	 */
	FORCEINLINE bool IsEngine() const { return HasBit(this->subtype, GVSF_ENGINE); }

	/**
	 * Check if a vehicle is a wagon.
	 * @return Returns true if vehicle is a wagon.
	 */
	FORCEINLINE bool IsWagon() const { return HasBit(this->subtype, GVSF_WAGON); }

	/**
	 * Check if the vehicle is a multiheaded engine.
	 * @return Returns true if the vehicle is a multiheaded engine.
	 */
	FORCEINLINE bool IsMultiheaded() const { return HasBit(this->subtype, GVSF_MULTIHEADED); }

	/**
	 * Tell if we are dealing with the rear end of a multiheaded engine.
	 * @return True if the engine is the rear part of a dualheaded engine.
	 */
	FORCEINLINE bool IsRearDualheaded() const { return this->IsMultiheaded() && !this->IsEngine(); }

	/**
	 * Check if the vehicle is an articulated part of an engine.
	 * @return Returns true if the vehicle is an articulated part.
	 */
	FORCEINLINE bool IsArticulatedPart() const { return HasBit(this->subtype, GVSF_ARTICULATED_PART); }

	/**
	 * Check if an engine has an articulated part.
	 * @return True if the engine has an articulated part.
	 */
	FORCEINLINE bool HasArticulatedPart() const { return this->Next() != NULL && this->Next()->IsArticulatedPart(); }
};

#endif /* GROUND_VEHICLE_HPP */
