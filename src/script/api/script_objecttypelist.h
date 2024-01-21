/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_objecttypelist.h List all available object types. */

#ifndef SCRIPT_OBJECTTYPELIST_HPP
#define SCRIPT_OBJECTTYPELIST_HPP

#include "script_objecttype.h"

/**
 * Creates a list of valid object types.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptObjectTypeList : public ScriptList {
public:
	ScriptObjectTypeList();
};


#endif /* SCRIPT_OBJECTTYPELIST_HPP */
