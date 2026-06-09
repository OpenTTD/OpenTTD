/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file goal_gui.h Goal GUI functions. */

#ifndef GOAL_GUI_H
#define GOAL_GUI_H

#include "company_type.h"
#include "goal_type.h"
#include "strings_type.h"

void ShowGoalsList(CompanyID company);
void ShowGoalQuestion(uint16_t id, GoalQuestionType type, uint32_t button_mask, const EncodedString &question);

#endif /* GOAL_TYPE_H */
