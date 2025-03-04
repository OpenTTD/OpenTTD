/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file goal_base.h %Goal base class. */

#ifndef GOAL_BASE_H
#define GOAL_BASE_H

#include "company_type.h"
#include "goal_type.h"
#include "core/pool_type.hpp"
#include "strings_type.h"

using GoalPool = Pool<Goal, GoalID, 64>;
extern GoalPool _goal_pool;

/** Struct about goals, current and completed */
struct Goal : GoalPool::PoolItem<&_goal_pool> {
	CompanyID company = CompanyID::Invalid(); ///< Goal is for a specific company; CompanyID::Invalid() if it is global
	GoalType type = GT_NONE; ///< Type of the goal
	GoalTypeID dst = 0; ///< Index of type
	EncodedString text{}; ///< Text of the goal.
	EncodedString progress{}; ///< Progress text of the goal.
	bool completed = false; ///< Is the goal completed or not?

	/**
	 * We need an (empty) constructor so struct isn't zeroed (as C++ standard states)
	 */
	Goal() { }
	Goal(GoalType type, GoalTypeID dst, CompanyID company, const EncodedString &text) : company(company), type(type), dst(dst), text(text) {}

	/**
	 * (Empty) destructor has to be defined else operator delete might be called with nullptr parameter
	 */
	~Goal() { }

	static bool IsValidGoalDestination(CompanyID company, GoalType type, GoalTypeID dest);
};

#endif /* GOAL_BASE_H */
