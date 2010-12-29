/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_event.hpp Everything to handle events from the game. */

#ifndef AI_EVENT_HPP
#define AI_EVENT_HPP

#include "ai_object.hpp"

/**
 * Class that handles all event related functions.
 * You can lookup the type, and than convert it to the real event-class.
 * That way you can request more detailed information about the event.
 */
class AIEvent : public AIObject {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEvent"; }

	/**
	 * The type of event. Needed to lookup the detailed class.
	 */
	enum AIEventType {
		AI_ET_INVALID = 0,
		AI_ET_TEST,
		AI_ET_SUBSIDY_OFFER,
		AI_ET_SUBSIDY_OFFER_EXPIRED,
		AI_ET_SUBSIDY_AWARDED,
		AI_ET_SUBSIDY_EXPIRED,
		AI_ET_ENGINE_PREVIEW,
		AI_ET_COMPANY_NEW,
		AI_ET_COMPANY_IN_TROUBLE,
		AI_ET_COMPANY_ASK_MERGER,
		AI_ET_COMPANY_MERGER,
		AI_ET_COMPANY_BANKRUPT,
		AI_ET_VEHICLE_CRASHED,
		AI_ET_VEHICLE_LOST,
		AI_ET_VEHICLE_WAITING_IN_DEPOT,
		AI_ET_VEHICLE_UNPROFITABLE,
		AI_ET_INDUSTRY_OPEN,
		AI_ET_INDUSTRY_CLOSE,
		AI_ET_ENGINE_AVAILABLE,
		AI_ET_STATION_FIRST_VEHICLE,
		AI_ET_DISASTER_ZEPPELINER_CRASHED,
		AI_ET_DISASTER_ZEPPELINER_CLEARED,
		AI_ET_TOWN_FOUNDED,
	};

	/**
	 * Constructor of AIEvent, to get the type of event.
	 */
	AIEvent(AIEvent::AIEventType type) :
		type(type)
	{}

	/**
	 * Get the event-type.
	 * @return The @c AIEventType.
	 */
	AIEventType GetEventType() { return this->type; }

protected:
	/**
	 * The type of this event.
	 */
	AIEventType type;
};

/**
 * Class that handles all event related functions.
 * @note it is not needed to create an instance of AIEvent to access it, as
 *  all members are static, and all data is stored AI-wide.
 */
class AIEventController : public AIObject {
public:
	/**
	 * The name of the class, needed by several sub-processes.
	 */
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEventController"; }

	/**
	 * Check if there is an event waiting.
	 * @return true if there is an event on the stack.
	 */
	static bool IsEventWaiting();

	/**
	 * Get the next event.
	 * @return a class of the event-child issues.
	 */
	static AIEvent *GetNextEvent();

#ifndef EXPORT_SKIP
	/**
	 * Insert an event to the queue for the company.
	 * @param event The event to insert.
	 * @note DO NOT CALL YOURSELF; leave it to the internal AI programming.
	 */
	static void InsertEvent(AIEvent *event);

	/**
	 * Free the event pointer.
	 * @note DO NOT CALL YOURSELF; leave it to the internal AI programming.
	 */
	static void FreeEventPointer();
#endif /* EXPORT_SKIP */

private:
	/**
	 * Create the event pointer.
	 */
	static void CreateEventPointer();
};

#endif /* AI_EVENT_HPP */
