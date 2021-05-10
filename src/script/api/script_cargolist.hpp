/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_cargolist.hpp List all the cargoes. */

#ifndef SCRIPT_CARGOLIST_HPP
#define SCRIPT_CARGOLIST_HPP

#include "script_list.hpp"

/**
 * Creates a list of cargoes that can be produced in the current game.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptCargoList : public ScriptList {
public:
	ScriptCargoList();
};

/**
 * Creates a list of cargoes that the given industry accepts.
 * @note This list also includes cargoes that are temporarily not accepted
 *   by this industry, @see ScriptIndustry::IsCargoAccepted.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptCargoList_IndustryAccepting : public ScriptList {
public:
	/**
	 * @param industry_id The industry to get the list of cargoes it accepts from.
	 */
	ScriptCargoList_IndustryAccepting(IndustryID industry_id);
};

/**
 * Creates a list of cargoes that the given industry can produce.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptCargoList_IndustryProducing : public ScriptList {
public:
	/**
	 * @param industry_id The industry to get the list of cargoes it produces from.
	 */
	ScriptCargoList_IndustryProducing(IndustryID industry_id);
};

/**
 * Creates a list of cargoes that the given station accepts.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptCargoList_StationAccepting : public ScriptList {
public:
	/**
	 * @param station_id The station to get the list of cargoes it accepts from.
	 */
	ScriptCargoList_StationAccepting(StationID station_id);
};

#endif /* SCRIPT_CARGOLIST_HPP */
