/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_event.cpp Implementation of ScriptEvent. */

#include "../../stdafx.h"
#include "script_event_types.hpp"

#include <queue>

#include "../../safeguards.h"

/** The queue of events for a script. */
struct ScriptEventData {
	std::queue<ScriptEvent *> stack; ///< The actual queue.
};

/* static */ void ScriptEventController::CreateEventPointer()
{
	assert(ScriptObject::GetEventPointer() == nullptr);

	ScriptObject::GetEventPointer() = new ScriptEventData();
}

/* static */ void ScriptEventController::FreeEventPointer()
{
	ScriptEventData *data = (ScriptEventData *)ScriptObject::GetEventPointer();

	/* Free all waiting events (if any) */
	while (!data->stack.empty()) {
		ScriptEvent *e = data->stack.front();
		data->stack.pop();
		e->Release();
	}

	/* Now kill our data pointer */
	delete data;
}

/* static */ bool ScriptEventController::IsEventWaiting()
{
	if (ScriptObject::GetEventPointer() == nullptr) ScriptEventController::CreateEventPointer();
	ScriptEventData *data = (ScriptEventData *)ScriptObject::GetEventPointer();

	return !data->stack.empty();
}

/* static */ ScriptEvent *ScriptEventController::GetNextEvent()
{
	if (ScriptObject::GetEventPointer() == nullptr) ScriptEventController::CreateEventPointer();
	ScriptEventData *data = (ScriptEventData *)ScriptObject::GetEventPointer();

	if (data->stack.empty()) return nullptr;

	ScriptEvent *e = data->stack.front();
	data->stack.pop();
	return e;
}

/* static */ void ScriptEventController::InsertEvent(ScriptEvent *event)
{
	if (ScriptObject::GetEventPointer() == nullptr) ScriptEventController::CreateEventPointer();
	ScriptEventData *data = (ScriptEventData *)ScriptObject::GetEventPointer();

	event->AddRef();
	data->stack.push(event);
}

