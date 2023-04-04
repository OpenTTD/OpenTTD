/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_objecttypelist.cpp Implementation of ScriptObjectTypeList. */

#include "../../stdafx.h"
#include "script_objecttypelist.hpp"
#include "../../newgrf_object.h"

#include "../../safeguards.h"

ScriptObjectTypeList::ScriptObjectTypeList()
{
	for (const auto &spec : ObjectSpec::Specs()) {
		if (!spec.IsEverAvailable()) continue;
		this->AddItem(spec.Index());
	}
}
