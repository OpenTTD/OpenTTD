/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_industrylist.hpp List all the industries. */

#ifndef AI_INDUSTRYLIST_HPP
#define AI_INDUSTRYLIST_HPP

#include "ai_list.hpp"

/**
 * Creates a list of industries that are currently on the map.
 * @ingroup AIList
 */
class AIIndustryList : public AIList {
public:
	AIIndustryList();
};

/**
 * Creates a list of industries that accepts a given cargo.
 * @ingroup AIList
 */
class AIIndustryList_CargoAccepting : public AIList {
public:
	/**
	 * @param cargo_id The cargo this industry should accept.
	 */
	AIIndustryList_CargoAccepting(CargoID cargo_id);
};

/**
 * Creates a list of industries that can produce a given cargo.
 * @note It also contains industries that currently produces 0 units of the cargo.
 * @ingroup AIList
 */
class AIIndustryList_CargoProducing : public AIList {
public:
	/**
	 * @param cargo_id The cargo this industry should produce.
	 */
	AIIndustryList_CargoProducing(CargoID cargo_id);
};

#endif /* AI_INDUSTRYLIST_HPP */
