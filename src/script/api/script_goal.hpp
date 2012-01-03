/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_goal.hpp Everything to manipulate the current running goal. */

#ifndef SCRIPT_GOAL_HPP
#define SCRIPT_GOAL_HPP

#include "script_object.hpp"
#include "script_company.hpp"
#include "../../goal_type.h"

/**
 * Class that handles some goal related functions.
 * @api game
 */
class ScriptGoal : public ScriptObject {
public:
	/**
	 * The goal IDs.
	 */
	enum GoalID {
		/* Note: these values represent part of the in-game GoalID enum */
		GOAL_INVALID = ::INVALID_GOALTYPE, ///< An invalid goal id.
	};

	/**
	 * Goal types that can be given to a goal.
	 */
	enum GoalType {
		/* Note: these values represent part of the in-game GoalType enum */
		GT_NONE     = ::GT_NONE,     ///< Destination is not linked.
		GT_TILE     = ::GT_TILE,     ///< Destination is a tile.
		GT_INDUSTRY = ::GT_INDUSTRY, ///< Destination is an industry.
		GT_TOWN     = ::GT_TOWN,     ///< Destination is a town.
		GT_COMPANY  = ::GT_COMPANY,  ///< Destination is a company.
	};

	enum QuestionButton {
		/* Note: these values represent part of the string list starting with STR_GOAL_QUESTION_BUTTON_CANCEL */
		BUTTON_CANCEL    = (1 << 0),  ///< Cancel button.
		BUTTON_OK        = (1 << 1),  ///< OK button.
		BUTTON_NO        = (1 << 2),  ///< No button.
		BUTTON_YES       = (1 << 3),  ///< Yes button.
		BUTTON_DECLINE   = (1 << 4),  ///< Decline button.
		BUTTON_ACCEPT    = (1 << 5),  ///< Accept button.
		BUTTON_IGNORE    = (1 << 6),  ///< Ignore button.
		BUTTON_RETRY     = (1 << 7),  ///< Retry button.
		BUTTON_PREVIOUS  = (1 << 8),  ///< Previous button.
		BUTTON_NEXT      = (1 << 9),  ///< Next button.
		BUTTON_STOP      = (1 << 10), ///< Stop button.
		BUTTON_START     = (1 << 11), ///< Start button.
		BUTTON_GO        = (1 << 12), ///< Go button.
		BUTTON_CONTINUE  = (1 << 13), ///< Continue button.
		BUTTON_RESTART   = (1 << 14), ///< Restart button.
		BUTTON_POSTPONE  = (1 << 15), ///< Postpone button.
		BUTTON_SURRENDER = (1 << 16), ///< Surrender button.
		BUTTON_CLOSE     = (1 << 17), ///< Close button.
	};

	/**
	 * Check whether this is a valid goalID.
	 * @param goal_id The GoalID to check.
	 * @return True if and only if this goal is valid.
	 */
	static bool IsValidGoal(GoalID goal_id);

	/**
	 * Create a new goal.
	 * @param company The company to create the goal for, or ScriptCompany::COMPANY_INVALID for all.
	 * @param goal The goal to add to the GUI (can be either a raw string, or a ScriptText object).
	 * @param type The type of the goal.
	 * @param destination The destination of the #type type.
	 * @return The new GoalID, or GOAL_INVALID if it failed.
	 * @pre No ScriptCompanyMode may be in scope.
	 * @pre goal != NULL && len(goal) != 0.
	 * @pre company == COMPANY_INVALID || ResolveCompanyID(company) != COMPANY_INVALID.
	 */
	static GoalID New(ScriptCompany::CompanyID company, Text *goal, GoalType type, uint32 destination);

	/**
	 * Remove a goal from the list.
	 * @param goal_id The goal to remove.
	 * @return True if the action succeeded.
	 * @pre No ScriptCompanyMode may be in scope.
	 * @pre IsValidGoal(goal_id).
	 */
	static bool Remove(GoalID goal_id);

	/**
	 * Ask a question.
	 * @param uniqueid Your unique id to distinguish results of multiple questions in the returning event.
	 * @param company The company to ask the question, or ScriptCompany::COMPANY_INVALID for all.
	 * @param question The question to ask (can be either a raw string, or a ScriptText object).
	 * @param buttons Any combinations (at least 1, up to 3) of buttons defined in QuestionButton. Like BUTTON_YES + BUTTON_NO.
	 * @return True if the action succeeded.
	 * @pre No ScriptCompanyMode may be in scope.
	 * @pre question != NULL && len(question) != 0.
	 * @pre company == COMPANY_INVALID || ResolveCompanyID(company) != COMPANY_INVALID.
	 * @pre CountBits(buttons) >= 1 && CountBits(buttons) <= 3.
	 * @note Replies to the question are given by you via the event ScriptEvent_GoalQuestionAnswer.
	 * @note There is no guarantee you ever get a reply on your question.
	 */
	static bool Question(uint16 uniqueid, ScriptCompany::CompanyID company, Text *question, int buttons);

	/**
	 * Close the question on all clients.
	 * @param uniqueid The uniqueid of the question you want to close.
	 * @return True if the action succeeded.
	 * @pre No ScriptCompanyMode may be in scope.
	 * @note If you send a question to a single company, and get a reply for them,
	 *   the question is already closed on all clients. Only use this function if
	 *   you want to timeout a question, or if you send the question to all
	 *   companies, but you are only interested in the reply of the first.
	 */
	static bool CloseQuestion(uint16 uniqueid);
};

#endif /* SCRIPT_GOAL_HPP */
