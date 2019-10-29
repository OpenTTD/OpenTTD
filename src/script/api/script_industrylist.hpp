/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_industrylist.hpp List all the industries. */

#ifndef SCRIPT_INDUSTRYLIST_HPP
#define SCRIPT_INDUSTRYLIST_HPP

#include "script_list.hpp"

/**
 * Creates a list of industries that are currently on the map.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptIndustryList : public ScriptList {
public:
	ScriptIndustryList();
};

/**
 * Creates a list of industries that accepts a given cargo.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptIndustryList_CargoAccepting : public ScriptList {
public:
	/**
	 * @param cargo_id The cargo this industry should accept.
	 */
	ScriptIndustryList_CargoAccepting(CargoID cargo_id);
};

/**
 * Creates a list of industries that can produce a given cargo.
 * @note It also contains industries that currently produces 0 units of the cargo.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptIndustryList_CargoProducing : public ScriptList {
public:
	/**
	 * @param cargo_id The cargo this industry should produce.
	 */
	ScriptIndustryList_CargoProducing(CargoID cargo_id);
};

#endif /* SCRIPT_INDUSTRYLIST_HPP */
