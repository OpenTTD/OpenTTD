/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_base.cpp Implementation of ScriptBase. */

#include "../../stdafx.h"
#include "script_base.hpp"
#include "script_error.hpp"

#include "../../safeguards.h"

/* static */ uint32 ScriptBase::Rand()
{
	return ScriptObject::GetRandomizer().Next();
}

/* static */ uint32 ScriptBase::RandItem(int unused_param)
{
	return ScriptBase::Rand();
}

/* static */ uint ScriptBase::RandRange(uint max)
{
	return ScriptObject::GetRandomizer().Next(max);
}

/* static */ uint32 ScriptBase::RandRangeItem(int unused_param, uint max)
{
	return ScriptBase::RandRange(max);
}

/* static */ bool ScriptBase::Chance(uint out, uint max)
{
	EnforcePrecondition(false, out <= max);
	return ScriptBase::RandRange(max) < out;
}

/* static */ bool ScriptBase::ChanceItem(int unused_param, uint out, uint max)
{
	return ScriptBase::Chance(out, max);
}
