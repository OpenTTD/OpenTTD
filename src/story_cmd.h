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

std::tuple<CommandCost, StoryPageID> CmdCreateStoryPage(DoCommandFlags flags, CompanyID company, const std::string &text);
std::tuple<CommandCost, StoryPageElementID> CmdCreateStoryPageElement(DoCommandFlags flags, TileIndex tile, StoryPageID page_id, StoryPageElementType type, uint32_t reference, const std::string &text);
CommandCost CmdUpdateStoryPageElement(DoCommandFlags flags, TileIndex tile, StoryPageElementID page_element_id, uint32_t reference, const std::string &text);
CommandCost CmdSetStoryPageTitle(DoCommandFlags flags, StoryPageID page_id, const std::string &text);
CommandCost CmdSetStoryPageDate(DoCommandFlags flags, StoryPageID page_id, TimerGameCalendar::Date date);
CommandCost CmdShowStoryPage(DoCommandFlags flags, StoryPageID page_id);
CommandCost CmdRemoveStoryPage(DoCommandFlags flags, StoryPageID page_id);
CommandCost CmdRemoveStoryPageElement(DoCommandFlags flags, StoryPageElementID page_element_id);
CommandCost CmdStoryPageButton(DoCommandFlags flags, TileIndex tile, StoryPageElementID page_element_id, VehicleID reference);

template <> struct CommandTraits<CMD_CREATE_STORY_PAGE>         : DefaultCommandTraits<CMD_CREATE_STORY_PAGE,         "CmdCreateStoryPage",        CmdCreateStoryPage,        CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_CREATE_STORY_PAGE_ELEMENT> : DefaultCommandTraits<CMD_CREATE_STORY_PAGE_ELEMENT, "CmdCreateStoryPageElement", CmdCreateStoryPageElement, CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_UPDATE_STORY_PAGE_ELEMENT> : DefaultCommandTraits<CMD_UPDATE_STORY_PAGE_ELEMENT, "CmdUpdateStoryPageElement", CmdUpdateStoryPageElement, CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_SET_STORY_PAGE_TITLE>      : DefaultCommandTraits<CMD_SET_STORY_PAGE_TITLE,      "CmdSetStoryPageTitle",      CmdSetStoryPageTitle,      CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_SET_STORY_PAGE_DATE>       : DefaultCommandTraits<CMD_SET_STORY_PAGE_DATE,       "CmdSetStoryPageDate",       CmdSetStoryPageDate,       CMD_DEITY,                CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_SHOW_STORY_PAGE>           : DefaultCommandTraits<CMD_SHOW_STORY_PAGE,           "CmdShowStoryPage",          CmdShowStoryPage,          CMD_DEITY,                CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_REMOVE_STORY_PAGE>         : DefaultCommandTraits<CMD_REMOVE_STORY_PAGE,         "CmdRemoveStoryPage",        CmdRemoveStoryPage,        CMD_DEITY,                CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_REMOVE_STORY_PAGE_ELEMENT> : DefaultCommandTraits<CMD_REMOVE_STORY_PAGE_ELEMENT, "CmdRemoveStoryPageElement", CmdRemoveStoryPageElement, CMD_DEITY,                CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_STORY_PAGE_BUTTON>         : DefaultCommandTraits<CMD_STORY_PAGE_BUTTON,         "CmdStoryPageButton",        CmdStoryPageButton,        CMD_DEITY,                CMDT_OTHER_MANAGEMENT> {};

#endif /* STORY_CMD_H */
