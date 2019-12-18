/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_signlist.cpp Implementation of ScriptSignList and friends. */

#include "../../stdafx.h"
#include "script_signlist.hpp"
#include "script_sign.hpp"
#include "../../signs_base.h"

#include "../../safeguards.h"

ScriptSignList::ScriptSignList()
{
	for (const Sign *s : Sign::Iterate()) {
		if (ScriptSign::IsValidSign(s->index)) this->AddItem(s->index);
	}
}
