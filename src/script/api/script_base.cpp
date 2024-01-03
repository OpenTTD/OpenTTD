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

/* static */ SQInteger ScriptBase::Rand()
{
	return ScriptObject::GetRandomizer().Next();
}

/* static */ SQInteger ScriptBase::RandItem(SQInteger)
{
	return ScriptBase::Rand();
}

/* static */ SQInteger ScriptBase::RandRange(SQInteger max)
{
	max = Clamp<SQInteger>(max, 0, UINT32_MAX);
	return ScriptObject::GetRandomizer().Next(max);
}

/* static */ SQInteger ScriptBase::RandRangeItem(SQInteger, SQInteger max)
{
	return ScriptBase::RandRange(max);
}

/* static */ bool ScriptBase::Chance(SQInteger out, SQInteger max)
{
	out = Clamp<SQInteger>(out, 0, UINT32_MAX);
	max = Clamp<SQInteger>(max, 0, UINT32_MAX);
	EnforcePrecondition(false, out <= max);
	return ScriptBase::RandRange(max) < out;
}

/* static */ bool ScriptBase::ChanceItem(SQInteger, SQInteger out, SQInteger max)
{
	return ScriptBase::Chance(out, max);
}
