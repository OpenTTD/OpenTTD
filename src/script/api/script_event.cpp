/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_event.cpp Implementation of ScriptEvent. */

#include "../../stdafx.h"
#include "script_event_types.hpp"
#include "../script_storage.hpp"

#include "../../safeguards.h"

/* static */ bool ScriptEventController::IsEventWaiting()
{
	return !ScriptObject::GetEventQueue().empty();
}

/* static */ ScriptEvent *ScriptEventController::GetNextEvent()
{
	auto &queue = ScriptObject::GetEventQueue();
	if (queue.empty()) return nullptr;

	auto *result = queue.front().release();
	queue.pop();
	return result;
}

/* static */ void ScriptEventController::InsertEvent(ScriptEvent *event)
{
	ScriptObject::GetEventQueue().push(event);
}

