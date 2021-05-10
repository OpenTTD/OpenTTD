/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_subsidylist.hpp List all the subsidies. */

#ifndef SCRIPT_SUBSIDYLIST_HPP
#define SCRIPT_SUBSIDYLIST_HPP

#include "script_list.hpp"

/**
 * Creates a list of all current subsidies.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptSubsidyList : public ScriptList {
public:
	ScriptSubsidyList();
};

#endif /* SCRIPT_SUBSIDYLIST_HPP */
