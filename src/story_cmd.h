/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file story_cmd.h Command definitions related to stories. */

#ifndef STORY_CMD_H
#define STORY_CMD_H

#include "command_type.h"
#include "company_type.h"
#include "story_type.h"
#include "vehicle_type.h"

std::tuple<CommandCost, StoryPageID> CmdCreateStoryPage(DoCommandFlag flags, CompanyID company, const std::string &text);
std::tuple<CommandCost, StoryPageElementID> CmdCreateStoryPageElement(DoCommandFlag flags, TileIndex tile, StoryPageID page_id, StoryPageElementType type, uint32_t reference, const std::string &text);
CommandCost CmdUpdateStoryPageElement(DoCommandFlag flags, TileIndex tile, StoryPageElementID page_element_id, uint32_t reference, const std::string &text);
CommandCost CmdSetStoryPageTitle(DoCommandFlag flags, StoryPageID page_id, const std::string &text);
CommandCost CmdSetStoryPageDate(DoCommandFlag flags, StoryPageID page_id, TimerGameCalendar::Date date);
CommandCost CmdShowStoryPage(DoCommandFlag flags, StoryPageID page_id);
CommandCost CmdRemoveStoryPage(DoCommandFlag flags, StoryPageID page_id);
CommandCost CmdRemoveStoryPageElement(DoCommandFlag flags, StoryPageElementID page_element_id);
CommandCost CmdStoryPageButton(DoCommandFlag flags, TileIndex tile, StoryPageElementID page_element_id, VehicleID reference);

DEF_CMD_TRAIT(CMD_CREATE_STORY_PAGE,         CmdCreateStoryPage,        CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_CREATE_STORY_PAGE_ELEMENT, CmdCreateStoryPageElement, CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_UPDATE_STORY_PAGE_ELEMENT, CmdUpdateStoryPageElement, CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SET_STORY_PAGE_TITLE,      CmdSetStoryPageTitle,      CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SET_STORY_PAGE_DATE,       CmdSetStoryPageDate,       CMD_DEITY,                CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SHOW_STORY_PAGE,           CmdShowStoryPage,          CMD_DEITY,                CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_REMOVE_STORY_PAGE,         CmdRemoveStoryPage,        CMD_DEITY,                CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_REMOVE_STORY_PAGE_ELEMENT, CmdRemoveStoryPageElement, CMD_DEITY,                CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_STORY_PAGE_BUTTON,         CmdStoryPageButton,        CMD_DEITY,                CMDT_OTHER_MANAGEMENT)

#endif /* STORY_CMD_H */
