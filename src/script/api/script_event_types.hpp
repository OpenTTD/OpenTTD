/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_event_types.hpp The detailed types of all events. */

#ifndef SCRIPT_EVENT_TYPES_HPP
#define SCRIPT_EVENT_TYPES_HPP

#include "script_event.hpp"
#include "script_goal.hpp"
#include "script_window.hpp"

/**
 * Event Vehicle Crash, indicating a vehicle of yours is crashed.
 *  It contains the crash site, the crashed vehicle and the reason for the crash.
 * @api ai game
 */
class ScriptEventVehicleCrashed : public ScriptEvent {
public:
	/**
	 * The reasons for vehicle crashes
	 */
	enum CrashReason {
		CRASH_TRAIN,                ///< Two trains collided
		CRASH_RV_LEVEL_CROSSING,    ///< Road vehicle got under a train
		CRASH_RV_UFO,               ///< Road vehicle got under a landing ufo
		CRASH_PLANE_LANDING,        ///< Plane crashed on landing
		CRASH_AIRCRAFT_NO_AIRPORT,  ///< Aircraft crashed after it found not a single airport for landing
		CRASH_FLOODED,              ///< Vehicle was flooded
	};

	/**
	 * @param vehicle The vehicle that crashed.
	 * @param crash_site Where the vehicle crashed.
	 * @param crash_reason The reason why the vehicle crashed.
	 */
	ScriptEventVehicleCrashed(VehicleID vehicle, TileIndex crash_site, CrashReason crash_reason) :
		ScriptEvent(ET_VEHICLE_CRASHED),
		crash_site(crash_site),
		vehicle(vehicle),
		crash_reason(crash_reason)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventVehicleCrashed *Convert(ScriptEvent *instance) { return (ScriptEventVehicleCrashed *)instance; }

	/**
	 * Get the VehicleID of the crashed vehicle.
	 * @return The crashed vehicle.
	 */
	VehicleID GetVehicleID() { return this->vehicle; }

	/**
	 * Find the tile the vehicle crashed.
	 * @return The crash site.
	 */
	TileIndex GetCrashSite() { return this->crash_site; }

	/**
	 * Get the reason for crashing
	 * @return The reason for crashing
	 */
	CrashReason GetCrashReason() { return this->crash_reason; }

private:
	TileIndex crash_site;     ///< The location of the crash.
	VehicleID vehicle;        ///< The crashed vehicle.
	CrashReason crash_reason; ///< The reason for crashing.
};

/**
 * Event Subsidy Offered, indicating someone offered a subsidy.
 * @api ai game
 */
class ScriptEventSubsidyOffer : public ScriptEvent {
public:
	/**
	 * @param subsidy_id The index of this subsidy in the _subsidies array.
	 */
	ScriptEventSubsidyOffer(SubsidyID subsidy_id) :
		ScriptEvent(ET_SUBSIDY_OFFER),
		subsidy_id(subsidy_id)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventSubsidyOffer *Convert(ScriptEvent *instance) { return (ScriptEventSubsidyOffer *)instance; }

	/**
	 * Get the SubsidyID of the subsidy.
	 * @return The subsidy id.
	 */
	SubsidyID GetSubsidyID() { return this->subsidy_id; }

private:
	SubsidyID subsidy_id; ///< The subsidy that got offered.
};

/**
 * Event Subsidy Offer Expired, indicating a subsidy will no longer be awarded.
 * @api ai game
 */
class ScriptEventSubsidyOfferExpired : public ScriptEvent {
public:
	/**
	 * @param subsidy_id The index of this subsidy in the _subsidies array.
	 */
	ScriptEventSubsidyOfferExpired(SubsidyID subsidy_id) :
		ScriptEvent(ET_SUBSIDY_OFFER_EXPIRED),
		subsidy_id(subsidy_id)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventSubsidyOfferExpired *Convert(ScriptEvent *instance) { return (ScriptEventSubsidyOfferExpired *)instance; }

	/**
	 * Get the SubsidyID of the subsidy.
	 * @return The subsidy id.
	 */
	SubsidyID GetSubsidyID() { return this->subsidy_id; }

private:
	SubsidyID subsidy_id; ///< The subsidy offer that expired.
};

/**
 * Event Subsidy Awarded, indicating a subsidy is awarded to some company.
 * @api ai game
 */
class ScriptEventSubsidyAwarded : public ScriptEvent {
public:
	/**
	 * @param subsidy_id The index of this subsidy in the _subsidies array.
	 */
	ScriptEventSubsidyAwarded(SubsidyID subsidy_id) :
		ScriptEvent(ET_SUBSIDY_AWARDED),
		subsidy_id(subsidy_id)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventSubsidyAwarded *Convert(ScriptEvent *instance) { return (ScriptEventSubsidyAwarded *)instance; }

	/**
	 * Get the SubsidyID of the subsidy.
	 * @return The subsidy id.
	 */
	SubsidyID GetSubsidyID() { return this->subsidy_id; }

private:
	SubsidyID subsidy_id; ///< The subsidy that was awarded.
};

/**
 * Event Subsidy Expired, indicating a route that was once subsidized no longer is.
 * @api ai game
 */
class ScriptEventSubsidyExpired : public ScriptEvent {
public:
	/**
	 * @param subsidy_id The index of this subsidy in the _subsidies array.
	 */
	ScriptEventSubsidyExpired(SubsidyID subsidy_id) :
		ScriptEvent(ET_SUBSIDY_EXPIRED),
		subsidy_id(subsidy_id)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventSubsidyExpired *Convert(ScriptEvent *instance) { return (ScriptEventSubsidyExpired *)instance; }

	/**
	 * Get the SubsidyID of the subsidy.
	 * @return The subsidy id.
	 */
	 SubsidyID GetSubsidyID() { return this->subsidy_id; }

private:
	SubsidyID subsidy_id; ///< The subsidy that expired.
};

/**
 * Event Engine Preview, indicating a manufacturer offer you to test a new engine.
 *  You can get the same information about the offered engine as a real user
 *  would see in the offer window. And you can also accept the offer.
 * @api ai
 */
class ScriptEventEnginePreview : public ScriptEvent {
public:
	/**
	 * @param engine The engine offered to test.
	 */
	ScriptEventEnginePreview(EngineID engine) :
		ScriptEvent(ET_ENGINE_PREVIEW),
		engine(engine)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventEnginePreview *Convert(ScriptEvent *instance) { return (ScriptEventEnginePreview *)instance; }

	/**
	 * Get the name of the offered engine.
	 * @return The name the engine has.
	 */
	char *GetName();

	/**
	 * Get the cargo-type of the offered engine. In case it can transport multiple cargoes, it
	 *  returns the first/main.
	 * @return The cargo-type of the engine.
	 */
	CargoID GetCargoType();

	/**
	 * Get the capacity of the offered engine. In case it can transport multiple cargoes, it
	 *  returns the first/main.
	 * @return The capacity of the engine.
	 */
	int32 GetCapacity();

	/**
	 * Get the maximum speed of the offered engine.
	 * @return The maximum speed the engine has.
	 * @note The speed is in OpenTTD's internal speed unit.
	 *       This is mph / 1.6, which is roughly km/h.
	 *       To get km/h multiply this number by 1.00584.
	 */
	int32 GetMaxSpeed();

	/**
	 * Get the new cost of the offered engine.
	 * @return The new cost the engine has.
	 */
	Money GetPrice();

	/**
	 * Get the running cost of the offered engine.
	 * @return The running cost of the vehicle per year.
	 * @note Cost is per year; divide by 365 to get per day.
	 */
	Money GetRunningCost();

	/**
	 * Get the type of the offered engine.
	 * @return The type the engine has.
	 */
#ifdef DOXYGEN_API
	ScriptVehicle::VehicleType GetVehicleType();
#else
	int32 GetVehicleType();
#endif /* DOXYGEN_API */

	/**
	 * Accept the engine preview.
	 * @return True when the accepting succeeded.
	 */
	bool AcceptPreview();

private:
	EngineID engine; ///< The engine the preview is for.

	/**
	 * Check whether the engine from this preview is still valid.
	 * @return True iff the engine is still valid.
	 */
	bool IsEngineValid() const;
};

/**
 * Event Company New, indicating a new company has been created.
 * @api ai game
 */
class ScriptEventCompanyNew : public ScriptEvent {
public:
	/**
	 * @param owner The new company.
	 */
	ScriptEventCompanyNew(Owner owner) :
		ScriptEvent(ET_COMPANY_NEW),
		owner((ScriptCompany::CompanyID)owner)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventCompanyNew *Convert(ScriptEvent *instance) { return (ScriptEventCompanyNew *)instance; }

	/**
	 * Get the CompanyID of the company that has been created.
	 * @return The CompanyID of the company.
	 */
	ScriptCompany::CompanyID GetCompanyID() { return this->owner; }

private:
	ScriptCompany::CompanyID owner; ///< The new company.
};

/**
 * Event Company In Trouble, indicating a company is in trouble and might go
 *  bankrupt soon.
 * @api ai game
 */
class ScriptEventCompanyInTrouble : public ScriptEvent {
public:
	/**
	 * @param owner The company that is in trouble.
	 */
	ScriptEventCompanyInTrouble(Owner owner) :
		ScriptEvent(ET_COMPANY_IN_TROUBLE),
		owner((ScriptCompany::CompanyID)owner)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventCompanyInTrouble *Convert(ScriptEvent *instance) { return (ScriptEventCompanyInTrouble *)instance; }

	/**
	 * Get the CompanyID of the company that is in trouble.
	 * @return The CompanyID of the company in trouble.
	 */
	ScriptCompany::CompanyID GetCompanyID() { return this->owner; }

private:
	ScriptCompany::CompanyID owner; ///< The company that is in trouble.
};

/**
 * Event Company Ask Merger, indicating a company can be bought (cheaply) by you.
 * @api ai
 */
class ScriptEventCompanyAskMerger : public ScriptEvent {
public:
	/**
	 * @param owner The company that can be bought.
	 * @param value The value/costs of buying the company.
	 */
	ScriptEventCompanyAskMerger(Owner owner, int32 value) :
		ScriptEvent(ET_COMPANY_ASK_MERGER),
		owner((ScriptCompany::CompanyID)owner),
		value(value)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventCompanyAskMerger *Convert(ScriptEvent *instance) { return (ScriptEventCompanyAskMerger *)instance; }

	/**
	 * Get the CompanyID of the company that can be bought.
	 * @return The CompanyID of the company that can be bought.
	 * @note If the company is bought this will become invalid.
	 */
	ScriptCompany::CompanyID GetCompanyID() { return this->owner; }

	/**
	 * Get the value of the new company.
	 * @return The value of the new company.
	 */
	int32 GetValue() { return this->value; }

	/**
	 * Take over the company for this merger.
	 * @return true if the merger was a success.
	 */
	bool AcceptMerger();

private:
	ScriptCompany::CompanyID owner; ///< The company that is in trouble.
	int32 value;                ///< The value of the company, i.e. the amount you would pay.
};

/**
 * Event Company Merger, indicating a company has been bought by another
 *  company.
 * @api ai game
 */
class ScriptEventCompanyMerger : public ScriptEvent {
public:
	/**
	 * @param old_owner The company bought off.
	 * @param new_owner The company that bought owner.
	 */
	ScriptEventCompanyMerger(Owner old_owner, Owner new_owner) :
		ScriptEvent(ET_COMPANY_MERGER),
		old_owner((ScriptCompany::CompanyID)old_owner),
		new_owner((ScriptCompany::CompanyID)new_owner)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventCompanyMerger *Convert(ScriptEvent *instance) { return (ScriptEventCompanyMerger *)instance; }

	/**
	 * Get the CompanyID of the company that has been bought.
	 * @return The CompanyID of the company that has been bought.
	 * @note: The value below is not valid anymore as CompanyID, and
	 *  ScriptCompany::ResolveCompanyID will return COMPANY_COMPANY. It's
	 *  only useful if you're keeping track of company's yourself.
	 */
	ScriptCompany::CompanyID GetOldCompanyID() { return this->old_owner; }

	/**
	 * Get the CompanyID of the new owner.
	 * @return The CompanyID of the new owner.
	 */
	ScriptCompany::CompanyID GetNewCompanyID() { return this->new_owner; }

private:
	ScriptCompany::CompanyID old_owner; ///< The company that ended to exist.
	ScriptCompany::CompanyID new_owner; ///< The company that's the end result of the merger.
};

/**
 * Event Company Bankrupt, indicating a company has gone bankrupt.
 * @api ai game
 */
class ScriptEventCompanyBankrupt : public ScriptEvent {
public:
	/**
	 * @param owner The company that has gone bankrupt.
	 */
	ScriptEventCompanyBankrupt(Owner owner) :
		ScriptEvent(ET_COMPANY_BANKRUPT),
		owner((ScriptCompany::CompanyID)owner)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventCompanyBankrupt *Convert(ScriptEvent *instance) { return (ScriptEventCompanyBankrupt *)instance; }

	/**
	 * Get the CompanyID of the company that has gone bankrupt.
	 * @return The CompanyID of the company that has gone bankrupt.
	 */
	ScriptCompany::CompanyID GetCompanyID() { return this->owner; }

private:
	ScriptCompany::CompanyID owner; ///< The company that has gone bankrupt.
};

/**
 * Event Vehicle Lost, indicating a vehicle can't find its way to its destination.
 * @api ai
 */
class ScriptEventVehicleLost : public ScriptEvent {
public:
	/**
	 * @param vehicle_id The vehicle that is lost.
	 */
	ScriptEventVehicleLost(VehicleID vehicle_id) :
		ScriptEvent(ET_VEHICLE_LOST),
		vehicle_id(vehicle_id)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventVehicleLost *Convert(ScriptEvent *instance) { return (ScriptEventVehicleLost *)instance; }

	/**
	 * Get the VehicleID of the vehicle that is lost.
	 * @return The VehicleID of the vehicle that is lost.
	 */
	VehicleID GetVehicleID() { return this->vehicle_id; }

private:
	VehicleID vehicle_id; ///< The vehicle that is lost.
};

/**
 * Event VehicleWaitingInDepot, indicating a vehicle has arrived a depot and is now waiting there.
 * @api ai
 */
class ScriptEventVehicleWaitingInDepot : public ScriptEvent {
public:
	/**
	 * @param vehicle_id The vehicle that is waiting in a depot.
	 */
	ScriptEventVehicleWaitingInDepot(VehicleID vehicle_id) :
		ScriptEvent(ET_VEHICLE_WAITING_IN_DEPOT),
		vehicle_id(vehicle_id)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventVehicleWaitingInDepot *Convert(ScriptEvent *instance) { return (ScriptEventVehicleWaitingInDepot *)instance; }

	/**
	 * Get the VehicleID of the vehicle that is waiting in a depot.
	 * @return The VehicleID of the vehicle that is waiting in a depot.
	 */
	VehicleID GetVehicleID() { return this->vehicle_id; }

private:
	VehicleID vehicle_id; ///< The vehicle that is waiting in the depot.
};

/**
 * Event Vehicle Unprofitable, indicating a vehicle lost money last year.
 * @api ai
 */
class ScriptEventVehicleUnprofitable : public ScriptEvent {
public:
	/**
	 * @param vehicle_id The vehicle that was unprofitable.
	 */
	ScriptEventVehicleUnprofitable(VehicleID vehicle_id) :
		ScriptEvent(ET_VEHICLE_UNPROFITABLE),
		vehicle_id(vehicle_id)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventVehicleUnprofitable *Convert(ScriptEvent *instance) { return (ScriptEventVehicleUnprofitable *)instance; }

	/**
	 * Get the VehicleID of the vehicle that lost money.
	 * @return The VehicleID of the vehicle that lost money.
	 */
	VehicleID GetVehicleID() { return this->vehicle_id; }

private:
	VehicleID vehicle_id; ///< The vehicle that is unprofitable.
};

/**
 * Event Industry Open, indicating a new industry has been created.
 * @api ai game
 */
class ScriptEventIndustryOpen : public ScriptEvent {
public:
	/**
	 * @param industry_id The new industry.
	 */
	ScriptEventIndustryOpen(IndustryID industry_id) :
		ScriptEvent(ET_INDUSTRY_OPEN),
		industry_id(industry_id)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventIndustryOpen *Convert(ScriptEvent *instance) { return (ScriptEventIndustryOpen *)instance; }

	/**
	 * Get the IndustryID of the new industry.
	 * @return The IndustryID of the industry.
	 */
	IndustryID GetIndustryID() { return this->industry_id; }

private:
	IndustryID industry_id; ///< The industry that opened.
};

/**
 * Event Industry Close, indicating an industry is going to be closed.
 * @api ai game
 */
class ScriptEventIndustryClose : public ScriptEvent {
public:
	/**
	 * @param industry_id The new industry.
	 */
	ScriptEventIndustryClose(IndustryID industry_id) :
		ScriptEvent(ET_INDUSTRY_CLOSE),
		industry_id(industry_id)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventIndustryClose *Convert(ScriptEvent *instance) { return (ScriptEventIndustryClose *)instance; }

	/**
	 * Get the IndustryID of the closing industry.
	 * @return The IndustryID of the industry.
	 */
	IndustryID GetIndustryID() { return this->industry_id; }

private:
	IndustryID industry_id; ///< The industry that closed.
};

/**
 * Event Engine Available, indicating a new engine is available.
 * @api ai
 */
class ScriptEventEngineAvailable : public ScriptEvent {
public:
	/**
	 * @param engine The engine that is available.
	 */
	ScriptEventEngineAvailable(EngineID engine) :
		ScriptEvent(ET_ENGINE_AVAILABLE),
		engine(engine)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventEngineAvailable *Convert(ScriptEvent *instance) { return (ScriptEventEngineAvailable *)instance; }

	/**
	 * Get the EngineID of the new engine.
	 * @return The EngineID of the new engine.
	 */
	EngineID GetEngineID() { return this->engine; }

private:
	EngineID engine; ///< The engine that became available.
};

/**
 * Event Station First Vehicle, indicating a station has been visited by a vehicle for the first time.
 * @api ai game
 */
class ScriptEventStationFirstVehicle : public ScriptEvent {
public:
	/**
	 * @param station The station visited for the first time.
	 * @param vehicle The vehicle visiting the station.
	 */
	ScriptEventStationFirstVehicle(StationID station, VehicleID vehicle) :
		ScriptEvent(ET_STATION_FIRST_VEHICLE),
		station(station),
		vehicle(vehicle)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventStationFirstVehicle *Convert(ScriptEvent *instance) { return (ScriptEventStationFirstVehicle *)instance; }

	/**
	 * Get the StationID of the visited station.
	 * @return The StationID of the visited station.
	 */
	StationID GetStationID() { return this->station; }

	/**
	 * Get the VehicleID of the first vehicle.
	 * @return The VehicleID of the first vehicle.
	 */
	VehicleID GetVehicleID() { return this->vehicle; }

private:
	StationID station; ///< The station the vehicle arrived at.
	VehicleID vehicle; ///< The vehicle that arrived at the station.
};

/**
 * Event Disaster Zeppeliner Crashed, indicating a zeppeliner has crashed on an airport and is blocking the runway.
 * @api ai
 */
class ScriptEventDisasterZeppelinerCrashed : public ScriptEvent {
public:
	/**
	 * @param station The station containing the affected airport
	 */
	ScriptEventDisasterZeppelinerCrashed(StationID station) :
		ScriptEvent(ET_DISASTER_ZEPPELINER_CRASHED),
		station(station)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventDisasterZeppelinerCrashed *Convert(ScriptEvent *instance) { return (ScriptEventDisasterZeppelinerCrashed *)instance; }

	/**
	 * Get the StationID of the station containing the affected airport.
	 * @return The StationID of the station containing the affected airport.
	 */
	StationID GetStationID() { return this->station; }

private:
	StationID station; ///< The station the zeppeliner crashed.
};

/**
 * Event Disaster Zeppeliner Cleared, indicating a previously crashed zeppeliner has been removed, and the airport is operating again.
 * @api ai
 */
class ScriptEventDisasterZeppelinerCleared : public ScriptEvent {
public:
	/**
	 * @param station The station containing the affected airport
	 */
	ScriptEventDisasterZeppelinerCleared(StationID station) :
		ScriptEvent(ET_DISASTER_ZEPPELINER_CLEARED),
		station(station)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventDisasterZeppelinerCleared *Convert(ScriptEvent *instance) { return (ScriptEventDisasterZeppelinerCleared *)instance; }

	/**
	 * Get the StationID of the station containing the affected airport.
	 * @return The StationID of the station containing the affected airport.
	 */
	StationID GetStationID() { return this->station; }

private:
	StationID station; ///< The station the zeppeliner crashed.
};

/**
 * Event Town Founded, indicating a new town has been created.
 * @api ai game
 */
class ScriptEventTownFounded : public ScriptEvent {
public:
	/**
	 * @param town The town that was created.
	 */
	ScriptEventTownFounded(TownID town) :
		ScriptEvent(ET_TOWN_FOUNDED),
		town(town)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventTownFounded *Convert(ScriptEvent *instance) { return (ScriptEventTownFounded *)instance; }

	/**
	 * Get the TownID of the town.
	 * @return The TownID of the town that was created.
	 */
	TownID GetTownID() { return this->town; }

private:
	TownID town; ///< The town that got founded.
};

/**
 * Event AircraftDestTooFar, indicating the next destination of an aircraft is too far away.
 * This event can be trigger when the current oder of an aircraft changes, usually either when
 * loading is done or when switch manually.
 * @api ai
 */
class ScriptEventAircraftDestTooFar : public ScriptEvent {
public:
	/**
	 * @param vehicle_id The aircraft whose destination is too far away.
	 */
	ScriptEventAircraftDestTooFar(VehicleID vehicle_id) :
		ScriptEvent(ET_AIRCRAFT_DEST_TOO_FAR),
		vehicle_id(vehicle_id)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventAircraftDestTooFar *Convert(ScriptEvent *instance) { return (ScriptEventAircraftDestTooFar *)instance; }

	/**
	 * Get the VehicleID of the aircraft whose destination is too far away.
	 * @return The VehicleID of the aircraft whose destination is too far away.
	 */
	VehicleID GetVehicleID() { return this->vehicle_id; }

private:
	VehicleID vehicle_id; ///< The vehicle aircraft whose destination is too far away.
};

/**
 * Event Admin Port, indicating the admin port is sending you information.
 * @api game
 */
class ScriptEventAdminPort : public ScriptEvent {
public:
	/**
	 * @param json The JSON string which got sent.
	 */
	ScriptEventAdminPort(const char *json);
	~ScriptEventAdminPort();

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventAdminPort *Convert(ScriptEvent *instance) { return (ScriptEventAdminPort *)instance; }

	/**
	 * Get the information that was sent to you back as Squirrel object.
	 */
	SQInteger GetObject(HSQUIRRELVM vm);

private:
	char *json; ///< The JSON string.

	/**
	 * Read a table from a JSON string.
	 * @param vm The VM used.
	 * @param p The (part of the) JSON string reading.
	 */
	char *ReadTable(HSQUIRRELVM vm, char *p);

	/**
	 * Read a value from a JSON string.
	 * @param vm The VM used.
	 * @param p The (part of the) JSON string reading.
	 */
	char *ReadValue(HSQUIRRELVM vm, char *p);

	/**
	 * Read a string from a JSON string.
	 * @param vm The VM used.
	 * @param p The (part of the) JSON string reading.
	 */
	char *ReadString(HSQUIRRELVM vm, char *p);
};

/**
 * Event Window Widget Click, when a user clicks on a highlighted widget.
 * @api game
 */
class ScriptEventWindowWidgetClick : public ScriptEvent {
public:
	/**
	 * @param window The windowclass that was clicked.
	 * @param number The windownumber that was clicked.
	 * @param widget The widget in the window that was clicked.
	 */
	ScriptEventWindowWidgetClick(ScriptWindow::WindowClass window, uint32 number, uint8 widget) :
		ScriptEvent(ET_WINDOW_WIDGET_CLICK),
		window(window),
		number(number),
		widget(widget)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventWindowWidgetClick *Convert(ScriptEvent *instance) { return (ScriptEventWindowWidgetClick *)instance; }

	/**
	 * Get the class of the window that was clicked.
	 */
	ScriptWindow::WindowClass GetWindowClass() { return this->window; }

	/**
	 * Get the number of the window that was clicked.
	 */
	uint32 GetWindowNumber() { return this->number; }

	/**
	 * Get the number of the widget that was clicked.
	 */
	uint8 GetWidgetNumber() { return this->widget; }

private:
	ScriptWindow::WindowClass window; ///< Window of the click.
	uint32 number;                    ///< Number of the click.
	uint8 widget;                     ///< Widget of the click.
};

/**
 * Event Goal Question Answer, where you receive the answer given to your questions.
 * @note It is possible that you get more than 1 answer from the same company
 *  (due to lag). Please keep this in mind while handling this event.
 * @api game
 */
class ScriptEventGoalQuestionAnswer : public ScriptEvent {
public:
	/**
	 * @param uniqueid The uniqueID you have given this question.
	 * @param company The company that is replying.
	 * @param button The button the company pressed.
	 */
	ScriptEventGoalQuestionAnswer(uint16 uniqueid, ScriptCompany::CompanyID company, ScriptGoal::QuestionButton button) :
		ScriptEvent(ET_GOAL_QUESTION_ANSWER),
		uniqueid(uniqueid),
		company(company),
		button(button)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventGoalQuestionAnswer *Convert(ScriptEvent *instance) { return (ScriptEventGoalQuestionAnswer *)instance; }

	/**
	 * Get the unique id of the question.
	 */
	uint16 GetUniqueID() { return this->uniqueid; }

	/**
	 * Get the company that pressed a button.
	 */
	ScriptCompany::CompanyID GetCompany() { return this->company; }

	/**
	 * Get the button that got pressed.
	 */
	ScriptGoal::QuestionButton GetButton() { return this->button; }

private:
	uint16 uniqueid;                   ///< The uniqueid of the question.
	ScriptCompany::CompanyID company;  ///< The company given the answer.
	ScriptGoal::QuestionButton button; ///< The button he pressed.
};

/**
 * Base class for events involving a town and a company.
 * @api ai game
 */
class ScriptEventCompanyTown : public ScriptEvent {
public:
	/**
	 * @param event The eventtype.
	 * @param company The company.
	 * @param town The town.
	 */
	ScriptEventCompanyTown(ScriptEventType event, ScriptCompany::CompanyID company, TownID town) :
		ScriptEvent(event),
		company(company),
		town(town)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventCompanyTown *Convert(ScriptEvent *instance) { return (ScriptEventCompanyTown *)instance; }

	/**
	 * Get the CompanyID of the company.
	 * @return The CompanyID of the company involved into the event.
	 */
	ScriptCompany::CompanyID GetCompanyID() { return this->company; }

	/**
	 * Get the TownID of the town.
	 * @return The TownID of the town involved into the event.
	 */
	TownID GetTownID() { return this->town; }

private:
	ScriptCompany::CompanyID company; ///< The company involved into the event.
	TownID town;                      ///< The town involved into the event.
};

/**
 * Event Exclusive Transport Rights, indicating that company bought
 * exclusive transport rights in a town.
 * @api ai game
 */
class ScriptEventExclusiveTransportRights : public ScriptEventCompanyTown {
public:
	/**
	 * @param company The company.
	 * @param town The town.
	 */
	ScriptEventExclusiveTransportRights(ScriptCompany::CompanyID company, TownID town) :
		ScriptEventCompanyTown(ET_EXCLUSIVE_TRANSPORT_RIGHTS, company, town)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventExclusiveTransportRights *Convert(ScriptEventCompanyTown *instance) { return (ScriptEventExclusiveTransportRights *)instance; }
};

/**
 * Event Road Reconstruction, indicating that company triggered
 * road reconstructions in a town.
 * @api ai game
 */
class ScriptEventRoadReconstruction : public ScriptEventCompanyTown {
public:
	/**
	 * @param company The company.
	 * @param town The town.
	 */
	ScriptEventRoadReconstruction(ScriptCompany::CompanyID company, TownID town) :
		ScriptEventCompanyTown(ET_ROAD_RECONSTRUCTION, company, town)
	{}

	/**
	 * Convert an ScriptEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static ScriptEventRoadReconstruction *Convert(ScriptEventCompanyTown *instance) { return (ScriptEventRoadReconstruction *)instance; }
};

#endif /* SCRIPT_EVENT_TYPES_HPP */
