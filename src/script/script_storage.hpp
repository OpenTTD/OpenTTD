/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_storage.hpp Defines ScriptStorage and includes all files required for it. */

#ifndef SCRIPT_STORAGE_HPP
#define SCRIPT_STORAGE_HPP

#include "../signs_func.h"
#include "../vehicle_func.h"
#include "../road_type.h"
#include "../group.h"
#include "../goal_type.h"
#include "../story_type.h"

#include "table/strings.h"
#include <vector>

/**
 * The callback function for Mode-classes.
 */
typedef bool (ScriptModeProc)();

/**
 * The storage for each script. It keeps track of important information.
 */
class ScriptStorage {
friend class ScriptObject;
private:
	ScriptModeProc *mode;             ///< The current build mode we are int.
	class ScriptObject *mode_instance; ///< The instance belonging to the current build mode.
	CompanyID root_company;          ///< The root company, the company that the script really belongs to.
	CompanyID company;               ///< The current company.

	uint delay;                      ///< The ticks of delay each DoCommand has.
	bool allow_do_command;           ///< Is the usage of DoCommands restricted?

	CommandCost costs;               ///< The costs the script is tracking.
	Money last_cost;                 ///< The last cost of the command.
	uint last_error;                 ///< The last error of the command.
	bool last_command_res;           ///< The last result of the command.

	VehicleID new_vehicle_id;        ///< The ID of the new Vehicle.
	SignID new_sign_id;              ///< The ID of the new Sign.
	GroupID new_group_id;            ///< The ID of the new Group.
	GoalID new_goal_id;              ///< The ID of the new Goal.
	StoryPageID new_story_page_id;   ///< The ID of the new StoryPage.
	StoryPageID new_story_page_element_id; ///< The ID of the new StoryPageElement.

	std::vector<int> callback_value; ///< The values which need to survive a callback.

	RoadTypeIdentifier rtid;         ///< The current roadtype we build.
	RailType rail_type;              ///< The current railtype we build.

	void *event_data;                ///< Pointer to the event data storage.
	void *log_data;                  ///< Pointer to the log data storage.

public:
	ScriptStorage() :
		mode              (NULL),
		mode_instance     (NULL),
		root_company      (INVALID_OWNER),
		company           (INVALID_OWNER),
		delay             (1),
		allow_do_command  (true),
		/* costs (can't be set) */
		last_cost         (0),
		last_error        (STR_NULL),
		last_command_res  (true),
		new_vehicle_id    (0),
		new_sign_id       (0),
		new_group_id      (0),
		new_goal_id       (0),
		new_story_page_id (0),
		new_story_page_element_id(0),
		/* calback_value (can't be set) */
		rtid              (),
		rail_type         (INVALID_RAILTYPE),
		event_data        (NULL),
		log_data          (NULL)
	{ }

	~ScriptStorage();
};

#endif /* SCRIPT_STORAGE_HPP */
