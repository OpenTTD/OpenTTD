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

CommandProc CmdCreateGoal;
CommandProc CmdRemoveGoal;
CommandProc CmdSetGoalText;
CommandProc CmdSetGoalProgress;
CommandProc CmdSetGoalCompleted;
CommandProc CmdGoalQuestion;
CommandProc CmdGoalQuestionAnswer;

DEF_CMD_TRAIT(CMD_CREATE_GOAL,          CmdCreateGoal,         CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_REMOVE_GOAL,          CmdRemoveGoal,         CMD_DEITY,                CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SET_GOAL_TEXT,        CmdSetGoalText,        CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SET_GOAL_PROGRESS,    CmdSetGoalProgress,    CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SET_GOAL_COMPLETED,   CmdSetGoalCompleted,   CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_GOAL_QUESTION,        CmdGoalQuestion,       CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_GOAL_QUESTION_ANSWER, CmdGoalQuestionAnswer, CMD_DEITY,                CMDT_OTHER_MANAGEMENT)

#endif /* GOAL_CMD_H */
