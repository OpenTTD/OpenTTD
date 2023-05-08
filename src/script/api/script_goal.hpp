/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_goal.hpp Everything to manipulate the current running goal. */

#ifndef SCRIPT_GOAL_HPP
#define SCRIPT_GOAL_HPP

#include "script_client.hpp"
#include "script_company.hpp"
#include "../../goal_type.h"

/**
 * Class that handles some goal related functions.
 *
 * Goals are saved and loaded. Upon bankruptcy or company takeover, all company
 * specific goals are removed for that company. You can also remove individual
 * goals using #Remove.
 *
 * @api game
 */
class ScriptGoal : public ScriptObject {
public:
	/**
	 * The goal IDs.
	 */
	enum GoalID : uint16_t {
		/* Note: these values represent part of the in-game GoalID enum */
		GOAL_INVALID = ::INVALID_GOAL, ///< An invalid goal id.
	};

	/**
	 * Goal types that can be given to a goal.
	 */
	enum GoalType : byte {
		/* Note: these values represent part of the in-game GoalType enum */
		GT_NONE     = ::GT_NONE,     ///< Destination is not linked.
		GT_TILE     = ::GT_TILE,     ///< Destination is a tile.
		GT_INDUSTRY = ::GT_INDUSTRY, ///< Destination is an industry.
		GT_TOWN     = ::GT_TOWN,     ///< Destination is a town.
		GT_COMPANY  = ::GT_COMPANY,  ///< Destination is a company.
		GT_STORY_PAGE = ::GT_STORY_PAGE ///< Destination is a story page.
	};

	/**
	 * Types of queries we could do to the user.
	 * Basically the title of the question window.
	 */
	enum QuestionType {
		QT_QUESTION,    ///< Asking a simple question; title: Question.
		QT_INFORMATION, ///< Showing an informational message; title: Information.
		QT_WARNING,     ///< Showing a warning; title: Warning.
		QT_ERROR,       ///< Showing an error; title: Error.
	};

	/**
	 * Types of buttons that can be in the question window.
	 */
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
	 * Check whether this is a valid goal destination.
	 * @param company The relevant company if a story page is the destination.
	 * @param type The type of the goal.
	 * @param destination The destination of the \a type type.
	 * @return True if and only if this goal destination is valid.
	 */
	static bool IsValidGoalDestination(ScriptCompany::CompanyID company, GoalType type, SQInteger destination);

	/**
	 * Create a new goal.
	 * @param company The company to create the goal for, or ScriptCompany::COMPANY_INVALID for all.
	 * @param goal The goal to add to the GUI (can be either a raw string, or a ScriptText object).
	 * @param type The type of the goal.
	 * @param destination The destination of the \a type type.
	 * @return The new GoalID, or GOAL_INVALID if it failed.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre goal != null && len(goal) != 0.
	 * @pre company == COMPANY_INVALID || ResolveCompanyID(company) != COMPANY_INVALID.
	 * @pre if type is GT_STORY_PAGE, the company of the goal and the company of the story page need to match:
	 *       \li Global goals can only reference global story pages.
	 *       \li Company specific goals can reference global story pages and story pages of the same company.
	 */
	static GoalID New(ScriptCompany::CompanyID company, Text *goal, GoalType type, SQInteger destination);

	/**
	 * Remove a goal from the list.
	 * @param goal_id The goal to remove.
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidGoal(goal_id).
	 */
	static bool Remove(GoalID goal_id);

	/**
	 * Update goal destination of a goal.
	 * @param goal_id The goal to update.
	 * @param type The type of the goal.
	 * @param destination The destination of the \a type type.
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidGoal(goal_id).
	 * @pre IsValidGoalDestination(g->company, type, destination).
	 */
	static bool SetDestination(GoalID goal_id, GoalType type, SQInteger destination);

	/**
	 * Update goal text of a goal.
	 * @param goal_id The goal to update.
	 * @param goal The new goal text (can be either a raw string, or a ScriptText object).
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre goal != null && len(goal) != 0.
	 * @pre IsValidGoal(goal_id).
	 */
	static bool SetText(GoalID goal_id, Text *goal);

	/**
	 * Update the progress text of a goal. The progress text is a text that
	 * is shown adjacent to the goal but in a separate column. Try to keep
	 * the progress string short.
	 * @param goal_id The goal to update.
	 * @param progress The new progress text for the goal (can be either a raw string,
	 * or a ScriptText object). To clear the progress string you can pass null or an
	 * empty string.
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidGoal(goal_id).
	 */
	static bool SetProgress(GoalID goal_id, Text *progress);

	/**
	 * Update completed status of goal
	 * @param goal_id The goal to update.
	 * @param complete The new goal completed status.
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidGoal(goal_id).
	 */
	static bool SetCompleted(GoalID goal_id, bool complete);

	/**
	 * Checks if a given goal have been marked as completed.
	 * @param goal_id The goal to check complete status.
	 * @return True if the goal is completed, otherwise false.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidGoal(goal_id).
	 */
	static bool IsCompleted(GoalID goal_id);

	/**
	 * Ask a question of all players in a company.
	 * @param uniqueid Your unique id to distinguish results of multiple questions in the returning event.
	 * @param company The company to ask the question, or ScriptCompany::COMPANY_INVALID for all.
	 * @param question The question to ask (can be either a raw string, or a ScriptText object).
	 * @param type The type of question that is being asked.
	 * @param buttons Any combinations (at least 1, up to 3) of buttons defined in QuestionButton. Like BUTTON_YES + BUTTON_NO.
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre question != null && len(question) != 0.
	 * @pre company == COMPANY_INVALID || ResolveCompanyID(company) != COMPANY_INVALID.
	 * @pre CountBits(buttons) >= 1 && CountBits(buttons) <= 3.
	 * @pre uniqueid >= 0 && uniqueid <= MAX(uint16_t)
	 * @note Replies to the question are given by you via the event ScriptEventGoalQuestionAnswer.
	 * @note There is no guarantee you ever get a reply on your question.
	 */
	static bool Question(SQInteger uniqueid, ScriptCompany::CompanyID company, Text *question, QuestionType type, SQInteger buttons);

	/**
	 * Ask client a question.
	 * @param uniqueid Your unique id to distinguish results of multiple questions in the returning event.
	 * @param client The client to ask the question.
	 * @param question The question to ask (can be either a raw string, or a ScriptText object).
	 * @param type The type of question that is being asked.
	 * @param buttons Any combinations (at least 1, up to 3) of buttons defined in QuestionButton. Like BUTTON_YES + BUTTON_NO.
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre ScriptGame::IsMultiplayer()
	 * @pre question != null && len(question) != 0.
	 * @pre ResolveClientID(client) != CLIENT_INVALID.
	 * @pre CountBits(buttons) >= 1 && CountBits(buttons) <= 3.
	 * @pre uniqueid >= 0 && uniqueid <= MAX(uint16_t)
	 * @note Replies to the question are given by you via the event ScriptEventGoalQuestionAnswer.
	 * @note There is no guarantee you ever get a reply on your question.
	 */
	static bool QuestionClient(SQInteger uniqueid, ScriptClient::ClientID client, Text *question, QuestionType type, SQInteger buttons);

	/**
	 * Close the question on all clients.
	 * @param uniqueid The uniqueid of the question you want to close.
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre uniqueid >= 0 && uniqueid <= MAX(uint16_t)
	 * @note If you send a question to a single company, and get a reply for them,
	 *   the question is already closed on all clients. Only use this function if
	 *   you want to timeout a question, or if you send the question to all
	 *   companies, but you are only interested in the reply of the first.
	 */
	static bool CloseQuestion(SQInteger uniqueid);

protected:
	/**
	 * Does common checks and asks the question.
	 */
	static bool DoQuestion(SQInteger uniqueid, uint32_t target, bool is_client, Text *question, QuestionType type, SQInteger buttons);
};

#endif /* SCRIPT_GOAL_HPP */
