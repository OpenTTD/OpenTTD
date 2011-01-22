/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_event_types.hpp The detailed types of all events. */

#ifndef AI_EVENT_TYPES_HPP
#define AI_EVENT_TYPES_HPP

#include "ai_event.hpp"
#include "ai_company.hpp"

/**
 * Event Vehicle Crash, indicating a vehicle of yours is crashed.
 *  It contains the crash site, the crashed vehicle and the reason for the crash.
 */
class AIEventVehicleCrashed : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventVehicleCrashed"; }

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
	AIEventVehicleCrashed(VehicleID vehicle, TileIndex crash_site, CrashReason crash_reason) :
		AIEvent(AI_ET_VEHICLE_CRASHED),
		crash_site(crash_site),
		vehicle(vehicle),
		crash_reason(crash_reason)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventVehicleCrashed *Convert(AIEvent *instance) { return (AIEventVehicleCrashed *)instance; }

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
	TileIndex crash_site;
	VehicleID vehicle;
	CrashReason crash_reason;
};

/**
 * Event Subsidy Offered, indicating someone offered a subsidy.
 */
class AIEventSubsidyOffer : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventSubsidyOffer"; }

	/**
	 * @param subsidy_id The index of this subsidy in the _subsidies array.
	 */
	AIEventSubsidyOffer(SubsidyID subsidy_id) :
		AIEvent(AI_ET_SUBSIDY_OFFER),
		subsidy_id(subsidy_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventSubsidyOffer *Convert(AIEvent *instance) { return (AIEventSubsidyOffer *)instance; }

	/**
	 * Get the SubsidyID of the subsidy.
	 * @return The subsidy id.
	 */
	SubsidyID GetSubsidyID() { return this->subsidy_id; }

private:
	SubsidyID subsidy_id;
};

/**
 * Event Subsidy Offer Expired, indicating a subsidy will no longer be awarded.
 */
class AIEventSubsidyOfferExpired : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventSubsidyOfferExpired"; }

	/**
	 * @param subsidy_id The index of this subsidy in the _subsidies array.
	 */
	AIEventSubsidyOfferExpired(SubsidyID subsidy_id) :
		AIEvent(AI_ET_SUBSIDY_OFFER_EXPIRED),
		subsidy_id(subsidy_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventSubsidyOfferExpired *Convert(AIEvent *instance) { return (AIEventSubsidyOfferExpired *)instance; }

	/**
	 * Get the SubsidyID of the subsidy.
	 * @return The subsidy id.
	 */
	SubsidyID GetSubsidyID() { return this->subsidy_id; }

private:
	SubsidyID subsidy_id;
};

/**
 * Event Subidy Awarded, indicating a subsidy is awarded to some company.
 */
class AIEventSubsidyAwarded : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventSubsidyAwarded"; }

	/**
	 * @param subsidy_id The index of this subsidy in the _subsidies array.
	 */
	AIEventSubsidyAwarded(SubsidyID subsidy_id) :
		AIEvent(AI_ET_SUBSIDY_AWARDED),
		subsidy_id(subsidy_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventSubsidyAwarded *Convert(AIEvent *instance) { return (AIEventSubsidyAwarded *)instance; }

	/**
	 * Get the SubsidyID of the subsidy.
	 * @return The subsidy id.
	 */
	SubsidyID GetSubsidyID() { return this->subsidy_id; }

private:
	SubsidyID subsidy_id;
};

/**
 * Event Subsidy Expired, indicating a route that was once subsidized no longer is.
 */
class AIEventSubsidyExpired : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventSubsidyExpired"; }

	/**
	 * @param subsidy_id The index of this subsidy in the _subsidies array.
	 */
	AIEventSubsidyExpired(SubsidyID subsidy_id) :
		AIEvent(AI_ET_SUBSIDY_EXPIRED),
		subsidy_id(subsidy_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventSubsidyExpired *Convert(AIEvent *instance) { return (AIEventSubsidyExpired *)instance; }

	/**
	 * Get the SubsidyID of the subsidy.
	 * @return The subsidy id.
	 */
	 SubsidyID GetSubsidyID() { return this->subsidy_id; }

private:
	SubsidyID subsidy_id;
};

/**
 * Event Engine Preview, indicating a manufacturer offer you to test a new engine.
 *  You can get the same information about the offered engine as a real user
 *  would see in the offer window. And you can also accept the offer.
 */
class AIEventEnginePreview : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventEnginePreview"; }

	/**
	 * @param engine The engine offered to test.
	 */
	AIEventEnginePreview(EngineID engine) :
		AIEvent(AI_ET_ENGINE_PREVIEW),
		engine(engine)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventEnginePreview *Convert(AIEvent *instance) { return (AIEventEnginePreview *)instance; }

	/**
	 * Get the name of the offered engine.
	 * @return The name the engine has.
	 */
	char *GetName();

	/**
	 * Get the cargo-type of the offered engine. In case it can transport multiple cargos, it
	 *  returns the first/main.
	 * @return The cargo-type of the engine.
	 */
	CargoID GetCargoType();

	/**
	 * Get the capacity of the offered engine. In case it can transport multiple cargos, it
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

#ifdef DOXYGEN_SKIP
	/**
	 * Get the type of the offered engine.
	 * @return The type the engine has.
	 */
	AIVehicle::VehicleType GetVehicleType();
#else
	int32 GetVehicleType();
#endif

	/**
	 * Accept the engine preview.
	 * @return True when the accepting succeeded.
	 */
	bool AcceptPreview();

private:
	EngineID engine;
};

/**
 * Event Company New, indicating a new company has been created.
 */
class AIEventCompanyNew : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventCompanyNew"; }

	/**
	 * @param owner The new company.
	 */
	AIEventCompanyNew(Owner owner) :
		AIEvent(AI_ET_COMPANY_NEW),
		owner((AICompany::CompanyID)owner)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventCompanyNew *Convert(AIEvent *instance) { return (AIEventCompanyNew *)instance; }

	/**
	 * Get the CompanyID of the company that has been created.
	 * @return The CompanyID of the company.
	 */
	AICompany::CompanyID GetCompanyID() { return this->owner; }

private:
	AICompany::CompanyID owner;
};

/**
 * Event Company In Trouble, indicating a company is in trouble and might go
 *  bankrupt soon.
 */
class AIEventCompanyInTrouble : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventCompanyInTrouble"; }

	/**
	 * @param owner The company that is in trouble.
	 */
	AIEventCompanyInTrouble(Owner owner) :
		AIEvent(AI_ET_COMPANY_IN_TROUBLE),
		owner((AICompany::CompanyID)owner)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventCompanyInTrouble *Convert(AIEvent *instance) { return (AIEventCompanyInTrouble *)instance; }

	/**
	 * Get the CompanyID of the company that is in trouble.
	 * @return The CompanyID of the company in trouble.
	 */
	AICompany::CompanyID GetCompanyID() { return this->owner; }

private:
	AICompany::CompanyID owner;
};

/**
 * Event Company Ask Merger, indicating a company can be bought (cheaply) by you.
 */
class AIEventCompanyAskMerger : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventCompanyAskMerger"; }

	/**
	 * @param owner The company that can be bough.
	 * @param value The value/costs of buying the company.
	 */
	AIEventCompanyAskMerger(Owner owner, int32 value) :
		AIEvent(AI_ET_COMPANY_ASK_MERGER),
		owner((AICompany::CompanyID)owner),
		value(value)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventCompanyAskMerger *Convert(AIEvent *instance) { return (AIEventCompanyAskMerger *)instance; }

	/**
	 * Get the CompanyID of the company that can be bought.
	 * @return The CompanyID of the company that can be bought.
	 * @note If the company is bought this will become invalid.
	 */
	AICompany::CompanyID GetCompanyID() { return this->owner; }

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
	AICompany::CompanyID owner;
	int32 value;
};

/**
 * Event Company Merger, indicating a company has been bought by another
 *  company.
 */
class AIEventCompanyMerger : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventCompanyMerger"; }

	/**
	 * @param old_owner The company bought off.
	 * @param new_owner The company that bougth owner.
	 */
	AIEventCompanyMerger(Owner old_owner, Owner new_owner) :
		AIEvent(AI_ET_COMPANY_MERGER),
		old_owner((AICompany::CompanyID)old_owner),
		new_owner((AICompany::CompanyID)new_owner)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventCompanyMerger *Convert(AIEvent *instance) { return (AIEventCompanyMerger *)instance; }

	/**
	 * Get the CompanyID of the company that has been bought.
	 * @return The CompanyID of the company that has been bought.
	 * @note: The value below is not valid anymore as CompanyID, and
	 *  AICompany::ResolveCompanyID will return COMPANY_COMPANY. It's
	 *  only usefull if you're keeping track of company's yourself.
	 */
	AICompany::CompanyID GetOldCompanyID() { return this->old_owner; }

	/**
	 * Get the CompanyID of the new owner.
	 * @return The CompanyID of the new owner.
	 */
	AICompany::CompanyID GetNewCompanyID() { return this->new_owner; }

private:
	AICompany::CompanyID old_owner;
	AICompany::CompanyID new_owner;
};

/**
 * Event Company Bankrupt, indicating a company has gone bankrupt.
 */
class AIEventCompanyBankrupt : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventCompanyBankrupt"; }

	/**
	 * @param owner The company that has gone bankrupt.
	 */
	AIEventCompanyBankrupt(Owner owner) :
		AIEvent(AI_ET_COMPANY_BANKRUPT),
		owner((AICompany::CompanyID)owner)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventCompanyBankrupt *Convert(AIEvent *instance) { return (AIEventCompanyBankrupt *)instance; }

	/**
	 * Get the CompanyID of the company that has gone bankrupt.
	 * @return The CompanyID of the company that has gone bankrupt.
	 */
	AICompany::CompanyID GetCompanyID() { return this->owner; }

private:
	AICompany::CompanyID owner;
};

/**
 * Event Vehicle Lost, indicating a vehicle can't find its way to its destination.
 */
class AIEventVehicleLost : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventVehicleLost"; }

	/**
	 * @param vehicle_id The vehicle that is lost.
	 */
	AIEventVehicleLost(VehicleID vehicle_id) :
		AIEvent(AI_ET_VEHICLE_LOST),
		vehicle_id(vehicle_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventVehicleLost *Convert(AIEvent *instance) { return (AIEventVehicleLost *)instance; }

	/**
	 * Get the VehicleID of the vehicle that is lost.
	 * @return The VehicleID of the vehicle that is lost.
	 */
	VehicleID GetVehicleID() { return this->vehicle_id; }

private:
	VehicleID vehicle_id;
};

/**
 * Event VehicleWaitingInDepot, indicating a vehicle has arrived a depot and is now waiting there.
 */
class AIEventVehicleWaitingInDepot : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventVehicleWaitingInDepot"; }

	/**
	 * @param vehicle_id The vehicle that is waiting in a depot.
	 */
	AIEventVehicleWaitingInDepot(VehicleID vehicle_id) :
		AIEvent(AI_ET_VEHICLE_WAITING_IN_DEPOT),
		vehicle_id(vehicle_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventVehicleWaitingInDepot *Convert(AIEvent *instance) { return (AIEventVehicleWaitingInDepot *)instance; }

	/**
	 * Get the VehicleID of the vehicle that is waiting in a depot.
	 * @return The VehicleID of the vehicle that is waiting in a depot.
	 */
	VehicleID GetVehicleID() { return this->vehicle_id; }

private:
	VehicleID vehicle_id;
};

/**
 * Event Vehicle Unprofitable, indicating a vehicle lost money last year.
 */
class AIEventVehicleUnprofitable : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventVehicleUnprofitable"; }

	/**
	 * @param vehicle_id The vehicle that was unprofitable.
	 */
	AIEventVehicleUnprofitable(VehicleID vehicle_id) :
		AIEvent(AI_ET_VEHICLE_UNPROFITABLE),
		vehicle_id(vehicle_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventVehicleUnprofitable *Convert(AIEvent *instance) { return (AIEventVehicleUnprofitable *)instance; }

	/**
	 * Get the VehicleID of the vehicle that lost money.
	 * @return The VehicleID of the vehicle that lost money.
	 */
	VehicleID GetVehicleID() { return this->vehicle_id; }

private:
	VehicleID vehicle_id;
};

/**
 * Event Industry Open, indicating a new industry has been created.
 */
class AIEventIndustryOpen : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventIndustryOpen"; }

	/**
	 * @param industry_id The new industry.
	 */
	AIEventIndustryOpen(IndustryID industry_id) :
		AIEvent(AI_ET_INDUSTRY_OPEN),
		industry_id(industry_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventIndustryOpen *Convert(AIEvent *instance) { return (AIEventIndustryOpen *)instance; }

	/**
	 * Get the IndustryID of the new industry.
	 * @return The IndustryID of the industry.
	 */
	IndustryID GetIndustryID() { return this->industry_id; }

private:
	IndustryID industry_id;
};

/**
 * Event Industry Close, indicating an industry is going to be closed.
 */
class AIEventIndustryClose : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventIndustryClose"; }

	/**
	 * @param industry_id The new industry.
	 */
	AIEventIndustryClose(IndustryID industry_id) :
		AIEvent(AI_ET_INDUSTRY_CLOSE),
		industry_id(industry_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventIndustryClose *Convert(AIEvent *instance) { return (AIEventIndustryClose *)instance; }

	/**
	 * Get the IndustryID of the closing industry.
	 * @return The IndustryID of the industry.
	 */
	IndustryID GetIndustryID() { return this->industry_id; }

private:
	IndustryID industry_id;
};

/**
 * Event Engine Available, indicating a new engine is available.
 */
class AIEventEngineAvailable : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventEngineAvailable"; }

	/**
	 * @param engine The engine that is available.
	 */
	AIEventEngineAvailable(EngineID engine) :
		AIEvent(AI_ET_ENGINE_AVAILABLE),
		engine(engine)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventEngineAvailable *Convert(AIEvent *instance) { return (AIEventEngineAvailable *)instance; }

	/**
	 * Get the EngineID of the new engine.
	 * @return The EngineID of the new engine.
	 */
	EngineID GetEngineID() { return this->engine; }

private:
	EngineID engine;
};

/**
 * Event Station First Vehicle, indicating a station has been visited by a vehicle for the first time.
 */
class AIEventStationFirstVehicle : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventStationFirstVehicle"; }

	/**
	 * @param station The station visited for the first time.
	 * @param vehicle The vehicle visiting the station.
	 */
	AIEventStationFirstVehicle(StationID station, VehicleID vehicle) :
		AIEvent(AI_ET_STATION_FIRST_VEHICLE),
		station(station),
		vehicle(vehicle)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventStationFirstVehicle *Convert(AIEvent *instance) { return (AIEventStationFirstVehicle *)instance; }

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
	StationID station;
	VehicleID vehicle;
};

/**
 * Event Disaster Zeppeliner Crashed, indicating a zeppeliner has crashed on an airport and is blocking the runway.
 */
class AIEventDisasterZeppelinerCrashed : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventDisasterZeppelinerCrashed"; }

	/**
	 * @param station The station containing the affected airport
	 */
	AIEventDisasterZeppelinerCrashed(StationID station) :
		AIEvent(AI_ET_DISASTER_ZEPPELINER_CRASHED),
		station(station)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventDisasterZeppelinerCrashed *Convert(AIEvent *instance) { return (AIEventDisasterZeppelinerCrashed *)instance; }

	/**
	 * Get the StationID of the station containing the affected airport.
	 * @return The StationID of the station containing the affected airport.
	 */
	StationID GetStationID() { return this->station; }

private:
	StationID station;
};

/**
 * Event Disaster Zeppeliner Cleared, indicating a previously crashed zeppeliner has been removed, and the airport is operating again.
 */
class AIEventDisasterZeppelinerCleared : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventDisasterZeppelinerCleared"; }

	/**
	 * @param station The station containing the affected airport
	 */
	AIEventDisasterZeppelinerCleared(StationID station) :
		AIEvent(AI_ET_DISASTER_ZEPPELINER_CLEARED),
		station(station)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventDisasterZeppelinerCleared *Convert(AIEvent *instance) { return (AIEventDisasterZeppelinerCleared *)instance; }

	/**
	 * Get the StationID of the station containing the affected airport.
	 * @return The StationID of the station containing the affected airport.
	 */
	StationID GetStationID() { return this->station; }

private:
	StationID station;
};

/**
 * Event Town Founded, indicating a new town has been created.
 */
class AIEventTownFounded : public AIEvent {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventTownFounded"; }

	/**
	 * @param town The town that was created.
	 */
	AIEventTownFounded(TownID town) :
		AIEvent(AI_ET_TOWN_FOUNDED),
		town(town)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventTownFounded *Convert(AIEvent *instance) { return (AIEventTownFounded *)instance; }

	/**
	 * Get the TownID of the town.
	 * @return The TownID of the town that was created.
	 */
	TownID GetTownID() { return this->town; }

private:
	TownID town;
};

#endif /* AI_EVENT_TYPES_HPP */
