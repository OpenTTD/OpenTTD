/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file train.h Base for the train class. */

#ifndef TRAIN_H
#define TRAIN_H

#include "newgrf_engine.h"
#include "cargotype.h"
#include "rail.h"
#include "engine_base.h"
#include "rail_map.h"
#include "ground_vehicle.hpp"

struct Train;

enum VehicleRailFlags {
	VRF_REVERSING         = 0,

	/* used to store if a wagon is powered or not */
	VRF_POWEREDWAGON      = 3,

	/* used to reverse the visible direction of the vehicle */
	VRF_REVERSE_DIRECTION = 4,

	/* used to mark that electric train engine is allowed to run on normal rail */
	VRF_EL_ENGINE_ALLOWED_NORMAL_RAIL = 6,

	/* used for vehicle var 0xFE bit 8 (toggled each time the train is reversed, accurate for first vehicle only) */
	VRF_TOGGLE_REVERSE = 7,

	/* used to mark a train that can't get a path reservation */
	VRF_TRAIN_STUCK    = 8,

	/* used to mark a train that is just leaving a station */
	VRF_LEAVING_STATION = 9,
};

/** Modes for ignoring signals. */
enum TrainForceProceeding {
	TFP_NONE   = 0,    ///< Normal operation.
	TFP_STUCK  = 1,    ///< Proceed till next signal, but ignore being stuck till then. This includes force leaving depots.
	TFP_SIGNAL = 2,    ///< Ignore next signal, after the signal ignore being stucked.
};
typedef SimpleTinyEnumT<TrainForceProceeding, byte> TrainForceProceedingByte;

byte FreightWagonMult(CargoID cargo);

void CheckTrainsLengths();

void FreeTrainTrackReservation(const Train *v, TileIndex origin = INVALID_TILE, Trackdir orig_td = INVALID_TRACKDIR);
bool TryPathReserve(Train *v, bool mark_as_stuck = false, bool first_tile_okay = false);

int GetTrainStopLocation(StationID station_id, TileIndex tile, const Train *v, int *station_ahead, int *station_length);

/** Variables that are cached to improve performance and such */
struct TrainCache {
	/* Cached wagon override spritegroup */
	const struct SpriteGroup *cached_override;

	uint16 last_speed; // NOSAVE: only used in UI

	/* cached values, recalculated on load and each time a vehicle is added to/removed from the consist. */
	bool cached_tilt;           ///< train can tilt; feature provides a bonus in curves

	byte user_def_data;         ///< Cached property 0x25. Can be set by Callback 0x36.

	/* cached max. speed / acceleration data */
	int cached_max_curve_speed; ///< max consist speed limited by curves
};

/**
 * 'Train' is either a loco or a wagon.
 */
struct Train : public GroundVehicle<Train, VEH_TRAIN> {
	TrainCache tcache;

	/* Link between the two ends of a multiheaded engine */
	Train *other_multiheaded_part;

	uint16 crash_anim_pos;

	uint16 flags;
	TrackBitsByte track;
	TrainForceProceedingByte force_proceed;
	RailTypeByte railtype;
	RailTypes compatible_railtypes;

	/** Ticks waiting in front of a signal, ticks being stuck or a counter for forced proceeding through signals. */
	uint16 wait_counter;

	/** We don't want GCC to zero our struct! It already is zeroed and has an index! */
	Train() : GroundVehicle<Train, VEH_TRAIN>() {}
	/** We want to 'destruct' the right class. */
	virtual ~Train() { this->PreDestructor(); }

	friend struct GroundVehicle<Train, VEH_TRAIN>; // GroundVehicle needs to use the acceleration functions defined at Train.

	const char *GetTypeString() const { return "train"; }
	void MarkDirty();
	void UpdateDeltaXY(Direction direction);
	ExpensesType GetExpenseType(bool income) const { return income ? EXPENSES_TRAIN_INC : EXPENSES_TRAIN_RUN; }
	void PlayLeaveStationSound() const;
	bool IsPrimaryVehicle() const { return this->IsFrontEngine(); }
	SpriteID GetImage(Direction direction) const;
	int GetDisplaySpeed() const { return this->tcache.last_speed; }
	int GetDisplayMaxSpeed() const { return this->vcache.cached_max_speed; }
	Money GetRunningCost() const;
	int GetDisplayImageWidth(Point *offset = NULL) const;
	bool IsInDepot() const;
	bool IsStoppedInDepot() const;
	bool Tick();
	void OnNewDay();
	uint Crash(bool flooded = false);
	Trackdir GetVehicleTrackdir() const;
	TileIndex GetOrderStationLocation(StationID station);
	bool FindClosestDepot(TileIndex *location, DestinationID *destination, bool *reverse);

	void ReserveTrackUnderConsist() const;

	int GetCurveSpeedLimit() const;

	void ConsistChanged(bool same_length);

	void RailtypeChanged();

	int UpdateSpeed();

	void UpdateAcceleration();

	int GetCurrentMaxSpeed() const;

	/**
	 * enum to handle train subtypes
	 * Do not access it directly unless you have to. Use the access functions below
	 * This is an enum to tell what bit to access as it is a bitmask
	 */
	enum TrainSubtype {
		TS_FRONT             = 0, ///< Leading engine of a train
		TS_ARTICULATED_PART  = 1, ///< Articulated part of an engine
		TS_WAGON             = 2, ///< Wagon
		TS_ENGINE            = 3, ///< Engine that can be front engine, but might be placed behind another engine.
		TS_FREE_WAGON        = 4, ///< First in a wagon chain (in depot)
		TS_MULTIHEADED       = 5, ///< Engine is multiheaded
	};

	/**
	 * Set front engine state
	 */
	FORCEINLINE void SetFrontEngine() { SetBit(this->subtype, TS_FRONT); }

	/**
	 * Remove the front engine state
	 */
	FORCEINLINE void ClearFrontEngine() { ClrBit(this->subtype, TS_FRONT); }

	/**
	 * Set a vehicle to be an articulated part
	 */
	FORCEINLINE void SetArticulatedPart() { SetBit(this->subtype, TS_ARTICULATED_PART); }

	/**
	 * Clear a vehicle from being an articulated part
	 */
	FORCEINLINE void ClearArticulatedPart() { ClrBit(this->subtype, TS_ARTICULATED_PART); }

	/**
	 * Set a vehicle to be a wagon
	 */
	FORCEINLINE void SetWagon() { SetBit(this->subtype, TS_WAGON); }

	/**
	 * Clear wagon property
	 */
	FORCEINLINE void ClearWagon() { ClrBit(this->subtype, TS_WAGON); }

	/**
	 * Set engine status
	 */
	FORCEINLINE void SetEngine() { SetBit(this->subtype, TS_ENGINE); }

	/**
	 * Clear engine status
	 */
	FORCEINLINE void ClearEngine() { ClrBit(this->subtype, TS_ENGINE); }

	/**
	 * Set if a vehicle is a free wagon
	 */
	FORCEINLINE void SetFreeWagon() { SetBit(this->subtype, TS_FREE_WAGON); }

	/**
	 * Clear a vehicle from being a free wagon
	 */
	FORCEINLINE void ClearFreeWagon() { ClrBit(this->subtype, TS_FREE_WAGON); }

	/**
	 * Set if a vehicle is a multiheaded engine
	 */
	FORCEINLINE void SetMultiheaded() { SetBit(this->subtype, TS_MULTIHEADED); }

	/**
	 * Clear multiheaded engine property
	 */
	FORCEINLINE void ClearMultiheaded() { ClrBit(this->subtype, TS_MULTIHEADED); }


	/**
	 * Check if train is a front engine
	 * @return Returns true if train is a front engine
	 */
	FORCEINLINE bool IsFrontEngine() const { return HasBit(this->subtype, TS_FRONT); }

	/**
	 * Check if train is a free wagon (got no engine in front of it)
	 * @return Returns true if train is a free wagon
	 */
	FORCEINLINE bool IsFreeWagon() const { return HasBit(this->subtype, TS_FREE_WAGON); }

	/**
	 * Check if a vehicle is an engine (can be first in a train)
	 * @return Returns true if vehicle is an engine
	 */
	FORCEINLINE bool IsEngine() const { return HasBit(this->subtype, TS_ENGINE); }

	/**
	 * Check if a train is a wagon
	 * @return Returns true if vehicle is a wagon
	 */
	FORCEINLINE bool IsWagon() const { return HasBit(this->subtype, TS_WAGON); }

	/**
	 * Check if train is a multiheaded engine
	 * @return Returns true if vehicle is a multiheaded engine
	 */
	FORCEINLINE bool IsMultiheaded() const { return HasBit(this->subtype, TS_MULTIHEADED); }

	/**
	 * Tell if we are dealing with the rear end of a multiheaded engine.
	 * @return True if the engine is the rear part of a dualheaded engine.
	 */
	FORCEINLINE bool IsRearDualheaded() const { return this->IsMultiheaded() && !this->IsEngine(); }

	/**
	 * Check if train is an articulated part of an engine
	 * @return Returns true if train is an articulated part
	 */
	FORCEINLINE bool IsArticulatedPart() const { return HasBit(this->subtype, TS_ARTICULATED_PART); }

	/**
	 * Check if an engine has an articulated part.
	 * @return True if the engine has an articulated part.
	 */
	FORCEINLINE bool HasArticulatedPart() const { return this->Next() != NULL && this->Next()->IsArticulatedPart(); }


	/**
	 * Get the next part of a multi-part engine.
	 * Will only work on a multi-part engine (this->EngineHasArticPart() == true),
	 * Result is undefined for normal engine.
	 * @return next part of articulated engine
	 */
	FORCEINLINE Train *GetNextArticPart() const
	{
		assert(this->HasArticulatedPart());
		return this->Next();
	}

	/**
	 * Get the first part of a multi-part engine.
	 * @return First part of the engine.
	 */
	FORCEINLINE Train *GetFirstEnginePart()
	{
		Train *v = this;
		while (v->IsArticulatedPart()) v = v->Previous();
		return v;
	}

	/**
	 * Get the first part of a multi-part engine.
	 * @return First part of the engine.
	 */
	FORCEINLINE const Train *GetFirstEnginePart() const
	{
		const Train *v = this;
		while (v->IsArticulatedPart()) v = v->Previous();
		return v;
	}

	/**
	 * Get the last part of a multi-part engine.
	 * @return Last part of the engine.
	 */
	FORCEINLINE Train *GetLastEnginePart()
	{
		Train *v = this;
		while (v->HasArticulatedPart()) v = v->GetNextArticPart();
		return v;
	}

	/**
	 * Get the next real (non-articulated part) vehicle in the consist.
	 * @return Next vehicle in the consist.
	 */
	FORCEINLINE Train *GetNextVehicle() const
	{
		const Train *v = this;
		while (v->HasArticulatedPart()) v = v->GetNextArticPart();

		/* v now contains the last artic part in the engine */
		return v->Next();
	}

	/**
	 * Get the previous real (non-articulated part) vehicle in the consist.
	 * @return Previous vehicle in the consist.
	 */
	FORCEINLINE Train *GetPrevVehicle() const
	{
		Train *v = this->Previous();
		while (v != NULL && v->IsArticulatedPart()) v = v->Previous();

		return v;
	}

	/**
	 * Get the next real (non-articulated part and non rear part of dualheaded engine) vehicle in the consist.
	 * @return Next vehicle in the consist.
	 */
	FORCEINLINE Train *GetNextUnit() const
	{
		Train *v = this->GetNextVehicle();
		if (v != NULL && v->IsRearDualheaded()) v = v->GetNextVehicle();

		return v;
	}

	/**
	 * Get the previous real (non-articulated part and non rear part of dualheaded engine) vehicle in the consist.
	 * @return Previous vehicle in the consist.
	 */
	FORCEINLINE Train *GetPrevUnit()
	{
		Train *v = this->GetPrevVehicle();
		if (v != NULL && v->IsRearDualheaded()) v = v->GetPrevVehicle();

		return v;
	}


protected: // These functions should not be called outside acceleration code.

	/**
	 * Allows to know the power value that this vehicle will use.
	 * @return Power value from the engine in HP, or zero if the vehicle is not powered.
	 */
	FORCEINLINE uint16 GetPower() const
	{
		/* Power is not added for articulated parts */
		if (!this->IsArticulatedPart() && HasPowerOnRail(this->railtype, GetRailType(this->tile))) {
			uint16 power = GetVehicleProperty(this, PROP_TRAIN_POWER, RailVehInfo(this->engine_type)->power);
			/* Halve power for multiheaded parts */
			if (this->IsMultiheaded()) power /= 2;
			return power;
		}

		return 0;
	}

	/**
	 * Returns a value if this articulated part is powered.
	 * @return Power value from the articulated part in HP, or zero if it is not powered.
	 */
	FORCEINLINE uint16 GetPoweredPartPower(const Train *head) const
	{
		/* For powered wagons the engine defines the type of engine (i.e. railtype) */
		if (HasBit(this->flags, VRF_POWEREDWAGON) && HasPowerOnRail(head->railtype, GetRailType(this->tile))) {
			return RailVehInfo(this->gcache.first_engine)->pow_wag_power;
		}

		return 0;
	}

	/**
	 * Allows to know the weight value that this vehicle will use.
	 * @return Weight value from the engine in tonnes.
	 */
	FORCEINLINE uint16 GetWeight() const
	{
		uint16 weight = (CargoSpec::Get(this->cargo_type)->weight * this->cargo.Count() * FreightWagonMult(this->cargo_type)) / 16;

		/* Vehicle weight is not added for articulated parts. */
		if (!this->IsArticulatedPart()) {
			weight += GetVehicleProperty(this, PROP_TRAIN_WEIGHT, RailVehInfo(this->engine_type)->weight);
		}

		/* Powered wagons have extra weight added. */
		if (HasBit(this->flags, VRF_POWEREDWAGON)) {
			weight += RailVehInfo(this->gcache.first_engine)->pow_wag_weight;
		}

		return weight;
	}

	/**
	 * Allows to know the tractive effort value that this vehicle will use.
	 * @return Tractive effort value from the engine.
	 */
	FORCEINLINE byte GetTractiveEffort() const
	{
		return GetVehicleProperty(this, PROP_TRAIN_TRACTIVE_EFFORT, RailVehInfo(this->engine_type)->tractive_effort);
	}

	/**
	 * Gets the area used for calculating air drag.
	 * @return Area of the engine in m^2.
	 */
	FORCEINLINE byte GetAirDragArea() const
	{
		/* Air drag is higher in tunnels due to the limited cross-section. */
		return (this->track == TRACK_BIT_WORMHOLE && this->vehstatus & VS_HIDDEN) ? 28 : 14;
	}

	/**
	 * Gets the air drag coefficient of this vehicle.
	 * @return Air drag value from the engine.
	 */
	FORCEINLINE byte GetAirDrag() const
	{
		return RailVehInfo(this->engine_type)->air_drag;
	}

	/**
	 * Checks the current acceleration status of this vehicle.
	 * @return Acceleration status.
	 */
	FORCEINLINE AccelStatus GetAccelerationStatus() const
	{
		return (this->vehstatus & VS_STOPPED) || HasBit(this->flags, VRF_REVERSING) || HasBit(this->flags, VRF_TRAIN_STUCK) ? AS_BRAKE : AS_ACCEL;
	}

	/**
	 * Calculates the current speed of this vehicle.
	 * @return Current speed in km/h-ish.
	 */
	FORCEINLINE uint16 GetCurrentSpeed() const
	{
		return this->cur_speed;
	}

	/**
	 * Returns the rolling friction coefficient of this vehicle.
	 * @return Rolling friction coefficient in [1e-4].
	 */
	FORCEINLINE uint32 GetRollingFriction() const
	{
		/* Rolling friction for steel on steel is between 0.1% and 0.2%.
		 * The friction coefficient increases with speed in a way that
		 * it doubles at 512 km/h, triples at 1024 km/h and so on. */
		return 15 * (512 + this->GetCurrentSpeed()) / 512;
	}

	/**
	 * Allows to know the acceleration type of a vehicle.
	 * @return Acceleration type of the vehicle.
	 */
	FORCEINLINE int GetAccelerationType() const
	{
		return GetRailTypeInfo(this->railtype)->acceleration_type;
	}

	/**
	 * Returns the slope steepness used by this vehicle.
	 * @return Slope steepness used by the vehicle.
	 */
	FORCEINLINE uint32 GetSlopeSteepness() const
	{
		return _settings_game.vehicle.train_slope_steepness;
	}

	/**
	 * Gets the maximum speed allowed by the track for this vehicle.
	 * @return Maximum speed allowed.
	 */
	FORCEINLINE uint16 GetMaxTrackSpeed() const
	{
		return GetRailTypeInfo(GetRailType(this->tile))->max_speed;
	}

	/**
	 * Checks if the vehicle is at a tile that can be sloped.
	 * @return True if the tile can be sloped.
	 */
	FORCEINLINE bool TileMayHaveSlopedTrack() const
	{
		/* Any track that isn't TRACK_BIT_X or TRACK_BIT_Y cannot be sloped. */
		return this->track == TRACK_BIT_X || this->track == TRACK_BIT_Y;
	}
};

#define FOR_ALL_TRAINS(var) FOR_ALL_VEHICLES_OF_TYPE(Train, var)

#endif /* TRAIN_H */
