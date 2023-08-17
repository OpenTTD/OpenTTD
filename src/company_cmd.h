/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file company_cmd.h Command definitions related to companies. */

#ifndef COMPANY_CMD_H
#define COMPANY_CMD_H

#include "command_type.h"
#include "company_type.h"
#include "livery.h"

enum ClientID : uint32_t;
enum Colours : byte;

CommandCost CmdCompanyCtrl(DoCommandFlag flags, CompanyCtrlAction cca, CompanyID company_id, CompanyRemoveReason reason, ClientID client_id);
CommandCost CmdGiveMoney(DoCommandFlag flags, Money money, CompanyID dest_company);
CommandCost CmdRenameCompany(DoCommandFlag flags, const std::string &text);
CommandCost CmdRenamePresident(DoCommandFlag flags, const std::string &text);
CommandCost CmdSetCompanyManagerFace(DoCommandFlag flags, CompanyManagerFace cmf);
CommandCost CmdSetCompanyColour(DoCommandFlag flags, LiveryScheme scheme, bool primary, Colours colour);

DEF_CMD_TRAIT(CMD_COMPANY_CTRL,             CmdCompanyCtrl,           CMD_SPECTATOR | CMD_CLIENT_ID | CMD_NO_EST, CMDT_SERVER_SETTING)
DEF_CMD_TRAIT(CMD_GIVE_MONEY,               CmdGiveMoney,             0,                                          CMDT_MONEY_MANAGEMENT)
DEF_CMD_TRAIT(CMD_RENAME_COMPANY,           CmdRenameCompany,         0,                                          CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_RENAME_PRESIDENT,         CmdRenamePresident,       0,                                          CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SET_COMPANY_MANAGER_FACE, CmdSetCompanyManagerFace, 0,                                          CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SET_COMPANY_COLOUR,       CmdSetCompanyColour,      0,                                          CMDT_OTHER_MANAGEMENT)

#endif /* COMPANY_CMD_H */
