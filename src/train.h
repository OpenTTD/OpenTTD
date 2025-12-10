/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file train.h Base for the train class. */

#ifndef TRAIN_H
#define TRAIN_H

#include "core/enum_type.hpp"

#include "newgrf_engine.h"
#include "cargotype.h"
#include "rail.h"
#include "engine_base.h"
#include "rail_map.h"
#include "ground_vehicle.hpp"

struct Train;

/** Rail vehicle flags. */
enum VehicleRailFlag : uint8_t {
	Reversing = 0, ///< Train is slowing down to reverse.
	PoweredWagon = 3, ///< Wagon is powered.
	Flipped = 4, ///< Reverse the visible direction of the vehicle.

	AllowedOnNormalRail = 6, ///< Electric train engine is allowed to run on normal rail. */
	Reversed = 7, ///< Used for vehicle var 0xFE bit 8 (toggled each time the train is reversed, accurate for first vehicle only).
	Stuck = 8, ///< Train can't get a path reservation.
	LeavingStation = 9, ///< Train is just leaving a station.
};
using VehicleRailFlags = EnumBitSet<VehicleRailFlag, uint16_t>;

/** Modes for ignoring signals. */
enum TrainForceProceeding : uint8_t {
	TFP_NONE   = 0,    ///< Normal operation.
	TFP_STUCK  = 1,    ///< Proceed till next signal, but ignore being stuck till then. This includes force leaving depots.
	TFP_SIGNAL = 2,    ///< Ignore next signal, after the signal ignore being stuck.
};

/** Flags for Train::ConsistChanged */
enum class ConsistChangeFlag : uint8_t {
	Length, ///< Allow vehicles to change length.
	Capacity, ///< Allow vehicles to change capacity.
};

using ConsistChangeFlags = EnumBitSet<ConsistChangeFlag, uint8_t>;

static constexpr ConsistChangeFlags CCF_TRACK{}; ///< Valid changes while vehicle is driving, and possibly changing tracks.
static constexpr ConsistChangeFlags CCF_LOADUNLOAD{}; ///< Valid changes while vehicle is loading/unloading.
static constexpr ConsistChangeFlags CCF_AUTOREFIT{ConsistChangeFlag::Capacity}; ///< Valid changes for autorefitting in stations.
static constexpr ConsistChangeFlags CCF_REFIT{ConsistChangeFlag::Length, ConsistChangeFlag::Capacity}; ///< Valid changes for refitting in a depot.
static constexpr ConsistChangeFlags CCF_ARRANGE{ConsistChangeFlag::Length, ConsistChangeFlag::Capacity}; ///< Valid changes for arranging the consist in a depot.
static constexpr ConsistChangeFlags CCF_SAVELOAD{ConsistChangeFlag::Length}; ///< Valid changes when loading a savegame. (Everything that is not stored in the save.)

uint8_t FreightWagonMult(CargoType cargo);

void CheckTrainsLengths();

void FreeTrainTrackReservation(const Train *v);
bool TryPathReserve(Train *v, bool mark_as_stuck = false, bool first_tile_okay = false);

int GetTrainStopLocation(StationID station_id, TileIndex tile, const Train *v, int *station_ahead, int *station_length);

void GetTrainSpriteSize(EngineID engine, uint &width, uint &height, int &xoffs, int &yoffs, EngineImageType image_type);

bool TrainOnCrossing(TileIndex tile);
void NormalizeTrainVehInDepot(const Train *u);

/** Variables that are cached to improve performance and such */
struct TrainCache {
	/* Cached wagon override spritegroup */
	const struct SpriteGroup *cached_override = nullptr;

	/* cached values, recalculated on load and each time a vehicle is added to/removed from the consist. */
	bool cached_tilt = false; ///< train can tilt; feature provides a bonus in curves
	uint8_t user_def_data = 0; ///< Cached property 0x25. Can be set by Callback 0x36.

	int16_t cached_curve_speed_mod = 0; ///< curve speed modifier of the entire train
	uint16_t cached_max_curve_speed = 0; ///< max consist speed limited by curves

	auto operator<=>(const TrainCache &) const = default;
};

/**
 * 'Train' is either a loco or a wagon.
 */
struct Train final : public GroundVehicle<Train, VEH_TRAIN> {
	VehicleRailFlags flags{};
	uint16_t crash_anim_pos = 0; ///< Crash animation counter.
	uint16_t wait_counter = 0; ///< Ticks waiting in front of a signal, ticks being stuck or a counter for forced proceeding through signals.

	TrainCache tcache{};

	/* Link between the two ends of a multiheaded engine */
	Train *other_multiheaded_part = nullptr;

	RailTypes compatible_railtypes{};
	RailTypes railtypes{};

	TrackBits track{};
	TrainForceProceeding force_proceed{};

	/** We don't want GCC to zero our struct! It already is zeroed and has an index! */
	Train() : GroundVehicleBase() {}
	/** We want to 'destruct' the right class. */
	virtual ~Train() { this->PreDestructor(); }

	friend struct GroundVehicle<Train, VEH_TRAIN>; // GroundVehicle needs to use the acceleration functions defined at Train.

	void MarkDirty() override;
	void UpdateDeltaXY() override;
	ExpensesType GetExpenseType(bool income) const override { return income ? EXPENSES_TRAIN_REVENUE : EXPENSES_TRAIN_RUN; }
	void PlayLeaveStationSound(bool force = false) const override;
	bool IsPrimaryVehicle() const override { return this->IsFrontEngine(); }
	void GetImage(Direction direction, EngineImageType image_type, VehicleSpriteSeq *result) const override;
	int GetDisplaySpeed() const override { return this->gcache.last_speed; }
	int GetDisplayMaxSpeed() const override { return this->vcache.cached_max_speed; }
	Money GetRunningCost() const override;
	int GetCursorImageOffset() const;
	int GetDisplayImageWidth(Point *offset = nullptr) const;
	bool IsInDepot() const override { return this->track == TRACK_BIT_DEPOT; }
	bool Tick() override;
	void OnNewCalendarDay() override;
	void OnNewEconomyDay() override;
	uint Crash(bool flooded = false) override;
	Trackdir GetVehicleTrackdir() const override;
	TileIndex GetOrderStationLocation(StationID station) override;
	ClosestDepot FindClosestDepot() override;

	void ReserveTrackUnderConsist() const;

	uint16_t GetCurveSpeedLimit() const;

	void ConsistChanged(ConsistChangeFlags allowed_changes);

	int UpdateSpeed();

	void UpdateAcceleration();

	int GetCurrentMaxSpeed() const override;

	/**
	 * Get the next real (non-articulated part and non rear part of dualheaded engine) vehicle in the consist.
	 * @return Next vehicle in the consist.
	 */
	inline Train *GetNextUnit() const
	{
		Train *v = this->GetNextVehicle();
		if (v != nullptr && v->IsRearDualheaded()) v = v->GetNextVehicle();

		return v;
	}

	/**
	 * Get the previous real (non-articulated part and non rear part of dualheaded engine) vehicle in the consist.
	 * @return Previous vehicle in the consist.
	 */
	inline Train *GetPrevUnit()
	{
		Train *v = this->GetPrevVehicle();
		if (v != nullptr && v->IsRearDualheaded()) v = v->GetPrevVehicle();

		return v;
	}

	/**
	 * Calculate the offset from this vehicle's center to the following center taking the vehicle lengths into account.
	 * @return Offset from center to center.
	 */
	int CalcNextVehicleOffset() const
	{
		/* For vehicles with odd lengths the part before the center will be one unit
		 * longer than the part after the center. This means we have to round up the
		 * length of the next vehicle but may not round the length of the current
		 * vehicle. */
		return this->gcache.cached_veh_length / 2 + (this->Next() != nullptr ? this->Next()->gcache.cached_veh_length + 1 : 0) / 2;
	}

	/**
	 * Allows to know the acceleration type of a vehicle.
	 * @return Acceleration type of the vehicle.
	 */
	inline VehicleAccelerationModel GetAccelerationType() const
	{
		return GetRailTypeInfo(GetRailType(this->tile))->acceleration_type;
	}

protected: // These functions should not be called outside acceleration code.

	/**
	 * Allows to know the power value that this vehicle will use.
	 * @return Power value from the engine in HP, or zero if the vehicle is not powered.
	 */
	inline uint16_t GetPower() const
	{
		/* Power is not added for articulated parts */
		if (!this->IsArticulatedPart() && HasPowerOnRail(this->railtypes, GetRailType(this->tile))) {
			uint16_t power = GetVehicleProperty(this, PROP_TRAIN_POWER, RailVehInfo(this->engine_type)->power);
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
	inline uint16_t GetPoweredPartPower(const Train *head) const
	{
		/* For powered wagons the engine defines the type of engine (i.e. railtype) */
		if (this->flags.Test(VehicleRailFlag::PoweredWagon) && HasPowerOnRail(head->railtypes, GetRailType(this->tile))) {
			return RailVehInfo(this->gcache.first_engine)->pow_wag_power;
		}

		return 0;
	}

	/**
	 * Allows to know the weight value that this vehicle will use.
	 * @return Weight value from the engine in tonnes.
	 */
	inline uint16_t GetWeight() const
	{
		uint16_t weight = CargoSpec::Get(this->cargo_type)->WeightOfNUnitsInTrain(this->cargo.StoredCount());

		/* Vehicle weight is not added for articulated parts. */
		if (!this->IsArticulatedPart()) {
			weight += GetVehicleProperty(this, PROP_TRAIN_WEIGHT, RailVehInfo(this->engine_type)->weight);
		}

		/* Powered wagons have extra weight added. */
		if (this->flags.Test(VehicleRailFlag::PoweredWagon)) {
			weight += RailVehInfo(this->gcache.first_engine)->pow_wag_weight;
		}

		return weight;
	}

	/**
	 * Calculates the weight value that this vehicle will have when fully loaded with its current cargo.
	 * @return Weight value in tonnes.
	 */
	uint16_t GetMaxWeight() const override;

	/**
	 * Allows to know the tractive effort value that this vehicle will use.
	 * @return Tractive effort value from the engine.
	 */
	inline uint8_t GetTractiveEffort() const
	{
		return GetVehicleProperty(this, PROP_TRAIN_TRACTIVE_EFFORT, RailVehInfo(this->engine_type)->tractive_effort);
	}

	/**
	 * Gets the area used for calculating air drag.
	 * @return Area of the engine in m^2.
	 */
	inline uint8_t GetAirDragArea() const
	{
		/* Air drag is higher in tunnels due to the limited cross-section. */
		return (this->track == TRACK_BIT_WORMHOLE && this->vehstatus.Test(VehState::Hidden)) ? 28 : 14;
	}

	/**
	 * Gets the air drag coefficient of this vehicle.
	 * @return Air drag value from the engine.
	 */
	inline uint8_t GetAirDrag() const
	{
		return RailVehInfo(this->engine_type)->air_drag;
	}

	/**
	 * Checks the current acceleration status of this vehicle.
	 * @return Acceleration status.
	 */
	inline AccelStatus GetAccelerationStatus() const
	{
		return this->vehstatus.Test(VehState::Stopped) || this->flags.Any({VehicleRailFlag::Reversing, VehicleRailFlag::Stuck}) ? AS_BRAKE : AS_ACCEL;
	}

	/**
	 * Calculates the current speed of this vehicle.
	 * @return Current speed in km/h-ish.
	 */
	inline uint16_t GetCurrentSpeed() const
	{
		return this->cur_speed;
	}

	/**
	 * Returns the rolling friction coefficient of this vehicle.
	 * @return Rolling friction coefficient in [1e-4].
	 */
	inline uint32_t GetRollingFriction() const
	{
		/* Rolling friction for steel on steel is between 0.1% and 0.2%.
		 * The friction coefficient increases with speed in a way that
		 * it doubles at 512 km/h, triples at 1024 km/h and so on. */
		return 15 * (512 + this->GetCurrentSpeed()) / 512;
	}

	/**
	 * Returns the slope steepness used by this vehicle.
	 * @return Slope steepness used by the vehicle.
	 */
	inline uint32_t GetSlopeSteepness() const
	{
		return _settings_game.vehicle.train_slope_steepness;
	}

	/**
	 * Gets the maximum speed allowed by the track for this vehicle.
	 * @return Maximum speed allowed.
	 */
	inline uint16_t GetMaxTrackSpeed() const
	{
		return GetRailTypeInfo(GetRailType(this->tile))->max_speed;
	}

	/**
	 * Returns the curve speed modifier of this vehicle.
	 * @return Current curve speed modifier, in fixed-point binary representation with 8 fractional bits.
	 */
	inline int16_t GetCurveSpeedModifier() const
	{
		return GetVehicleProperty(this, PROP_TRAIN_CURVE_SPEED_MOD, RailVehInfo(this->engine_type)->curve_speed_mod, true);
	}

	/**
	 * Checks if the vehicle is at a tile that can be sloped.
	 * @return True if the tile can be sloped.
	 */
	inline bool TileMayHaveSlopedTrack() const
	{
		/* Any track that isn't TRACK_BIT_X or TRACK_BIT_Y cannot be sloped. */
		return this->track == TRACK_BIT_X || this->track == TRACK_BIT_Y;
	}

	/**
	 * Trains can always use the faster algorithm because they
	 * have always the same direction as the track under them.
	 * @return false
	 */
	inline bool HasToUseGetSlopePixelZ()
	{
		return false;
	}
};

#endif /* TRAIN_H */
