/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_cargomonitor.hpp Everything to monitor cargo pickup and deliveries by companies. */

#ifndef SCRIPT_CARGO_MONITOR_HPP
#define SCRIPT_CARGO_MONITOR_HPP

#include "script_list.hpp"
#include "script_object.hpp"
#include "script_company.hpp"
#include "../../cargomonitor.h"

/**
 * Class that handles all cargo movement monitoring related functions.
 *
 * To get an understanding of what users are transporting, this class provides cargo pick-up and delivery monitoring functions.
 * It works as follows:
 * - Select a company, a cargo-type, and an industry that gets the cargo triplet.
 * - Perform a call to #GetIndustryDeliveryAmount, setting 'keep_monitoring' to \c true.
 *   The return value is not important, but from this moment the program accumulates all deliveries by
 *   the given company to the given industry of the given cargo type.
 * - Some time later, perform another call to #GetIndustryDeliveryAmount. It returns the accumulated
 *   amount of cargo that the company has delivered.
 *   The call causes the collected amount to be reset. On the next call you will thus always get the
 *   delivered amount since the previous call.
 * - When monitoring the deliveries is not interesting any more, set 'keep_monitoring' to \c false.
 *   The collecting process that happens between calls is stopped.
 *
 * In the same way you can monitor town deliveries, and you can monitor pick-up from towns and industries.
 * The latter get added at the moment the cargo is delivered. This prevents users from getting credit for
 * picking up cargo without delivering it.
 *
 * The active monitors are saved and loaded. Upon bankruptcy or company takeover, the cargo monitors are
 * automatically stopped for that company. You can reset to the empty state with #StopAllMonitoring.
 *
 * @api game
 */
class ScriptCargoMonitor : public ScriptObject {
public:
	/**
	 * Get the amount of cargo delivered to a town by a company since the last query, and update the monitoring state.
	 * @param company %Company to query.
	 * @param cargo Cargo type to query.
	 * @param town_id %Town to query.
	 * @param keep_monitoring If \c true, the given combination continues to be monitored for the next call. If \c false, monitoring ends.
	 * @return Amount of delivered cargo of the given cargo type to the given town by the given company since the last call, or
	 * \c -1 if a parameter is out-of-bound.
	 */
	static SQInteger GetTownDeliveryAmount(ScriptCompany::CompanyID company, CargoID cargo, TownID town_id, bool keep_monitoring);

	/**
	 * Get the amount of cargo delivered to an industry by a company since the last query, and update the monitoring state.
	 * @param company %Company to query.
	 * @param cargo Cargo type to query.
	 * @param industry_id %Industry to query.
	 * @param keep_monitoring If \c true, the given combination continues to be monitored for the next call. If \c false, monitoring ends.
	 * @return Amount of delivered cargo of the given cargo type to the given industry by the given company since the last call, or
	 * \c -1 if a parameter is out-of-bound.
	 */
	static SQInteger GetIndustryDeliveryAmount(ScriptCompany::CompanyID company, CargoID cargo, IndustryID industry_id, bool keep_monitoring);

	/**
	 * Get the amount of cargo picked up (and delivered) from a town by a company since the last query, and update the monitoring state.
	 * @param company %Company to query.
	 * @param cargo Cargo type to query.
	 * @param town_id %Town to query.
	 * @param keep_monitoring If \c true, the given combination continues to be monitored for the next call. If \c false, monitoring ends.
	 * @return Amount of picked up cargo of the given cargo type to the given town by the given company since the last call, or
	 * \c -1 if a parameter is out-of-bound.
	 * @note Amounts of picked-up cargo are added during final delivery of it, to prevent users from getting credit for picking up without delivering it.
	 */
	static SQInteger GetTownPickupAmount(ScriptCompany::CompanyID company, CargoID cargo, TownID town_id, bool keep_monitoring);

	/**
	 * Get the amount of cargo picked up (and delivered) from an industry by a company since the last query, and update the monitoring state.
	 * @param company %Company to query.
	 * @param cargo Cargo type to query.
	 * @param industry_id %Industry to query.
	 * @param keep_monitoring If \c true, the given combination continues to be monitored for the next call. If \c false, monitoring ends.
	 * @return Amount of picked up cargo of the given cargo type to the given industry by the given company since the last call, or
	 * \c -1 if a parameter is out-of-bound.
	 * @note Amounts of picked-up cargo are added during final delivery of it, to prevent users from getting credit for picking up without delivering it.
	 */
	static SQInteger GetIndustryPickupAmount(ScriptCompany::CompanyID company, CargoID cargo, IndustryID industry_id, bool keep_monitoring);

	/** Stop monitoring everything. */
	static void StopAllMonitoring();
};

#endif /* SCRIPT_CARGO_MONITOR_HPP */
