/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file goal_cmd.h Command definitions related to goals. */

#ifndef GOAL_CMD_H
#define GOAL_CMD_H

#include "command_type.h"
#include "goal_type.h"

std::tuple<CommandCost, GoalID> CmdCreateGoal(DoCommandFlags flags, CompanyID company, GoalType type, GoalTypeID dest, const EncodedString &text);
CommandCost CmdRemoveGoal(DoCommandFlags flags, GoalID goal);
CommandCost CmdSetGoalDestination(DoCommandFlags flags, GoalID goal, GoalType type, GoalTypeID dest);
CommandCost CmdSetGoalText(DoCommandFlags flags, GoalID goal, const EncodedString &text);
CommandCost CmdSetGoalProgress(DoCommandFlags flags, GoalID goal, const EncodedString &text);
CommandCost CmdSetGoalCompleted(DoCommandFlags flags, GoalID goal, bool completed);
CommandCost CmdGoalQuestion(DoCommandFlags flags, uint16_t uniqueid, uint32_t target, bool is_client, uint32_t button_mask, GoalQuestionType type, const EncodedString &text);
CommandCost CmdGoalQuestionAnswer(DoCommandFlags flags, uint16_t uniqueid, uint8_t button);

DEF_CMD_TRAIT(CMD_CREATE_GOAL,          CmdCreateGoal,         CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CommandType::OtherManagement)
DEF_CMD_TRAIT(CMD_REMOVE_GOAL,          CmdRemoveGoal,         CommandFlag::Deity, CommandType::OtherManagement)
DEF_CMD_TRAIT(CMD_SET_GOAL_DESTINATION, CmdSetGoalDestination, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CommandType::OtherManagement)
DEF_CMD_TRAIT(CMD_SET_GOAL_TEXT,        CmdSetGoalText,        CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CommandType::OtherManagement)
DEF_CMD_TRAIT(CMD_SET_GOAL_PROGRESS,    CmdSetGoalProgress,    CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CommandType::OtherManagement)
DEF_CMD_TRAIT(CMD_SET_GOAL_COMPLETED,   CmdSetGoalCompleted,   CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CommandType::OtherManagement)
DEF_CMD_TRAIT(CMD_GOAL_QUESTION,        CmdGoalQuestion,       CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CommandType::OtherManagement)
DEF_CMD_TRAIT(CMD_GOAL_QUESTION_ANSWER, CmdGoalQuestionAnswer, CommandFlag::Deity, CommandType::OtherManagement)

#endif /* GOAL_CMD_H */
