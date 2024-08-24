/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file goal_cmd.h Command definitions related to goals. */

#ifndef GOAL_CMD_H
#define GOAL_CMD_H

#include "command_type.h"
#include "goal_type.h"

std::tuple<CommandCost, GoalID> CmdCreateGoal(DoCommandFlags flags, CompanyID company, GoalType type, GoalTypeID dest, const std::string &text);
CommandCost CmdRemoveGoal(DoCommandFlags flags, GoalID goal);
CommandCost CmdSetGoalDestination(DoCommandFlags flags, GoalID goal, GoalType type, GoalTypeID dest);
CommandCost CmdSetGoalText(DoCommandFlags flags, GoalID goal, const std::string &text);
CommandCost CmdSetGoalProgress(DoCommandFlags flags, GoalID goal, const std::string &text);
CommandCost CmdSetGoalCompleted(DoCommandFlags flags, GoalID goal, bool completed);
CommandCost CmdGoalQuestion(DoCommandFlags flags, uint16_t uniqueid, uint32_t target, bool is_client, uint32_t button_mask, GoalQuestionType type, const std::string &text);
CommandCost CmdGoalQuestionAnswer(DoCommandFlags flags, uint16_t uniqueid, uint8_t button);

template <> struct CommandTraits<CMD_CREATE_GOAL>          : DefaultCommandTraits<CMD_CREATE_GOAL,          "CmdCreateGoal",         CmdCreateGoal,         CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_REMOVE_GOAL>          : DefaultCommandTraits<CMD_REMOVE_GOAL,          "CmdRemoveGoal",         CmdRemoveGoal,         CMD_DEITY,                CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_SET_GOAL_DESTINATION> : DefaultCommandTraits<CMD_SET_GOAL_DESTINATION, "CmdSetGoalDestination", CmdSetGoalDestination, CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_SET_GOAL_TEXT>        : DefaultCommandTraits<CMD_SET_GOAL_TEXT,        "CmdSetGoalText",        CmdSetGoalText,        CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_SET_GOAL_PROGRESS>    : DefaultCommandTraits<CMD_SET_GOAL_PROGRESS,    "CmdSetGoalProgress",    CmdSetGoalProgress,    CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_SET_GOAL_COMPLETED>   : DefaultCommandTraits<CMD_SET_GOAL_COMPLETED,   "CmdSetGoalCompleted",   CmdSetGoalCompleted,   CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_GOAL_QUESTION>        : DefaultCommandTraits<CMD_GOAL_QUESTION,        "CmdGoalQuestion",       CmdGoalQuestion,       CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_GOAL_QUESTION_ANSWER> : DefaultCommandTraits<CMD_GOAL_QUESTION_ANSWER, "CmdGoalQuestionAnswer", CmdGoalQuestionAnswer, CMD_DEITY,                CMDT_OTHER_MANAGEMENT> {};

#endif /* GOAL_CMD_H */
