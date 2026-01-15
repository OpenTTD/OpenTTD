/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file story_cmd.h Command definitions related to stories. */

#ifndef STORY_CMD_H
#define STORY_CMD_H

#include "command_type.h"
#include "company_type.h"
#include "story_type.h"
#include "vehicle_type.h"

std::tuple<CommandCost, StoryPageID> CmdCreateStoryPage(DoCommandFlags flags, CompanyID company, const EncodedString &text);
std::tuple<CommandCost, StoryPageElementID> CmdCreateStoryPageElement(DoCommandFlags flags, TileIndex tile, StoryPageID page_id, StoryPageElementType type, uint32_t reference, const EncodedString &text);
CommandCost CmdUpdateStoryPageElement(DoCommandFlags flags, TileIndex tile, StoryPageElementID page_element_id, uint32_t reference, const EncodedString &text);
CommandCost CmdSetStoryPageTitle(DoCommandFlags flags, StoryPageID page_id, const EncodedString &text);
CommandCost CmdSetStoryPageDate(DoCommandFlags flags, StoryPageID page_id, TimerGameCalendar::Date date);
CommandCost CmdShowStoryPage(DoCommandFlags flags, StoryPageID page_id);
CommandCost CmdRemoveStoryPage(DoCommandFlags flags, StoryPageID page_id);
CommandCost CmdRemoveStoryPageElement(DoCommandFlags flags, StoryPageElementID page_element_id);
CommandCost CmdStoryPageButton(DoCommandFlags flags, TileIndex tile, StoryPageElementID page_element_id, VehicleID reference);

DEF_CMD_TRAIT(Commands::CreateStoryPage, CmdCreateStoryPage, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::CreateStoryPageElement, CmdCreateStoryPageElement, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::UpdateStoryPageElement, CmdUpdateStoryPageElement, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::SetStoryPageTitle, CmdSetStoryPageTitle, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::SetStoryPageDate, CmdSetStoryPageDate, CommandFlag::Deity, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::ShowStoryPage, CmdShowStoryPage, CommandFlag::Deity, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::RemoveStoryPage, CmdRemoveStoryPage, CommandFlag::Deity, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::RemoveStoryPageElement, CmdRemoveStoryPageElement, CommandFlag::Deity, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::StoryPageButton, CmdStoryPageButton, CommandFlag::Deity, CommandType::OtherManagement)

#endif /* STORY_CMD_H */
