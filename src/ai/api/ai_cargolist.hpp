/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_cargolist.hpp List all the cargos. */

#ifndef AI_CARGOLIST_HPP
#define AI_CARGOLIST_HPP

#include "ai_list.hpp"

/**
 * Creates a list of cargos that can be produced in the current game.
 * @ingroup AIList
 */
class AICargoList : public AIList {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AICargoList"; }
	AICargoList();
};

/**
 * Creates a list of cargos that the given industry accepts.
 * @note This list also includes cargos that are temporarily not accepted
 *   by this industry, @see AIIndustry::IsCargoAccepted.
 * @ingroup AIList
 */
class AICargoList_IndustryAccepting : public AIList {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AICargoList_IndustryAccepting"; }

	/**
	 * @param industry_id The industry to get the list of cargos it accepts from.
	 */
	AICargoList_IndustryAccepting(IndustryID industry_id);
};

/**
 * Creates a list of cargos that the given industry can produce.
 * @ingroup AIList
 */
class AICargoList_IndustryProducing : public AIList {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AICargoList_IndustryProducing"; }

	/**
	 * @param industry_id The industry to get the list of cargos it produces from.
	 */
	AICargoList_IndustryProducing(IndustryID industry_id);
};

/**
 * Creates a list of cargos that the given station accepts.
 * @ingroup AIList
 */
class AICargoList_StationAccepting : public AIList {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AICargoList_StationAccepting"; }

	/**
	 * @param station_id The station to get the list of cargos it accepts from.
	 */
	AICargoList_StationAccepting(StationID station_id);
};

#endif /* AI_CARGOLIST_HPP */
