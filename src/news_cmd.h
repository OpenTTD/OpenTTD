/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file news_cmd.h Command definitions related to news messages. */

#ifndef NEWS_CMD_H
#define NEWS_CMD_H

#include "command_type.h"
#include "company_type.h"
#include "news_type.h"

CommandCost CmdCustomNewsItem(DoCommandFlag flags, NewsType type, NewsReferenceType reftype1, CompanyID company, uint32_t reference, const std::string &text);

DEF_CMD_TRAIT(CMD_CUSTOM_NEWS_ITEM, CmdCustomNewsItem, CMD_STR_CTRL | CMD_DEITY, CMDT_OTHER_MANAGEMENT)

#endif /* NEWS_CMD_H */
