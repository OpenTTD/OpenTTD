/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file script_trigger.hpp Functionality to trigger events in AI and game scripts. */

#ifndef SCRIPT_TRIGGER_HPP
#define SCRIPT_TRIGGER_HPP

#include "../ai/ai.hpp"
#include "../game/game.hpp"

/**
 * Main Script class. Contains functions needed to handle Script Events.
 */
class ScriptTrigger {
public:
	/**
	 * Queue two new events, one for an AI, the other for the Game Script.
	 * @param company The company receiving the event.
	 */
	template <class ScriptEventType, typename ... Args>
	static void NewEvent(CompanyID company, Args ... args) {
		AI::NewEvent(company, new ScriptEventType(args...));
		Game::NewEvent(new ScriptEventType(args...));
	}

	/**
	 * Broadcast a new event to all active AIs, and to the Game Script.
	 */
	template <class ScriptEventType, typename ... Args>
	static void BroadcastNewEvent(Args ... args) {
		AI::BroadcastNewEvent(new ScriptEventType(args...));
		Game::NewEvent(new ScriptEventType(args...));
	}

	/**
	 * Broadcast a new event to all active AIs, and to the Game Script, except to one AI.
	 * @param skip_company The company to skip broadcasting for.
	 */
	template <class ScriptEventType, typename ... Args>
	static void BroadcastNewEventExceptForCompany(CompanyID skip_company, Args ... args) {
		AI::BroadcastNewEvent(new ScriptEventType(args...), skip_company);
		Game::NewEvent(new ScriptEventType(args...));
	}
};

#endif /* SCRIPT_TRIGGER_HPP */
