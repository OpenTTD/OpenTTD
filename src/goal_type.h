/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file goal_type.h Basic types related to goals. */

#ifndef GOAL_TYPE_H
#define GOAL_TYPE_H

#include "core/pool_type.hpp"

static const uint32_t GOAL_QUESTION_BUTTON_COUNT = 18; ///< Amount of buttons available.

/** Types of goal questions. */
enum class GoalQuestionType : uint8_t {
	Question = 0, ///< Asking a simple question; title: Question.
	Information = 1, ///< Showing an informational message; title: Information.
	Warning = 2, ///< Showing a warning; title: Warning.
	Error = 3, ///< Showing an error; title: Error.
	End, ///< End marker.
};

/** Types of goal destinations */
enum GoalType : uint8_t {
	GT_NONE,         ///< Destination is not linked
	GT_TILE,         ///< Destination is a tile
	GT_INDUSTRY,     ///< Destination is an industry
	GT_TOWN,         ///< Destination is a town
	GT_COMPANY,      ///< Destination is a company
	GT_STORY_PAGE,   ///< Destination is a story page
};

typedef uint32_t GoalTypeID; ///< Contains either tile, industry ID, town ID, company ID, or story page ID

/** ID of a goal */
using GoalID = PoolID<uint16_t, struct GoalIDTag, 64000, 0xFFFF>;

struct Goal;

#endif /* GOAL_TYPE_H */
