/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file goal_type.h basic types related to goals */

#ifndef GOAL_TYPE_H
#define GOAL_TYPE_H

#include "core/enum_type.hpp"

static const uint32_t GOAL_QUESTION_BUTTON_COUNT = 18; ///< Amount of buttons available.

enum GoalQuestionType : byte {
	GQT_QUESTION = 0,
	GQT_INFORMATION = 1,
	GQT_WARNING = 2,
	GQT_ERROR = 3,
	GQT_END = 4,
};

/** Types of goal destinations */
enum GoalType : byte {
	GT_NONE,         ///< Destination is not linked
	GT_TILE,         ///< Destination is a tile
	GT_INDUSTRY,     ///< Destination is an industry
	GT_TOWN,         ///< Destination is a town
	GT_COMPANY,      ///< Destination is a company
	GT_STORY_PAGE,   ///< Destination is a story page
};

typedef uint32_t GoalTypeID; ///< Contains either tile, industry ID, town ID, company ID, or story page ID

typedef uint16_t GoalID; ///< ID of a goal
struct Goal;
static const GoalID INVALID_GOAL = 0xFFFF; ///< Constant representing a non-existing goal.

#endif /* GOAL_TYPE_H */
