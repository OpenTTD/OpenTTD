/* $Id$ */

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

typedef Pool<Goal, GoalID, 64, 64000> GoalPool;
extern GoalPool _goal_pool;

/** Struct about goals, current and completed */
struct Goal : GoalPool::PoolItem<&_goal_pool> {
	CompanyByte company; ///< Goal is for a specific company; INVALID_COMPANY if it is global
	GoalTypeByte type;   ///< Type of the goal
	GoalTypeID dst;      ///< Index of type
	char *text;          ///< Text of the goal.
	char *progress;      ///< Progress text of the goal.
	bool completed;      ///< Is the goal completed or not?

	/**
	 * We need an (empty) constructor so struct isn't zeroed (as C++ standard states)
	 */
	inline Goal() { }

	/**
	 * (Empty) destructor has to be defined else operator delete might be called with nullptr parameter
	 */
	inline ~Goal() { free(this->text); free(this->progress); }
};

#define FOR_ALL_GOALS_FROM(var, start) FOR_ALL_ITEMS_FROM(Goal, goal_index, var, start)
#define FOR_ALL_GOALS(var) FOR_ALL_GOALS_FROM(var, 0)

#endif /* GOAL_BASE_H */
