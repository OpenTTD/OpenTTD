/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file news_cmd.h Command definitions related to news messages. */

#ifndef NEWS_CMD_H
#define NEWS_CMD_H

#include "command_func.h"
#include "company_type.h"
#include "news_func.h"

CommandCost CmdCustomNewsItem(DoCommandFlags flags, NewsType type, CompanyID company, NewsReference reference, const EncodedString &text);

DEF_CMD_TRAIT(CMD_CUSTOM_NEWS_ITEM, CmdCustomNewsItem, CommandFlags({CommandFlag::StrCtrl, CommandFlag::Deity}), CommandType::OtherManagement)

#endif /* NEWS_CMD_H */
