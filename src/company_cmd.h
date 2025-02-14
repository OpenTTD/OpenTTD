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
enum Colours : uint8_t;

CommandCost CmdCompanyCtrl(DoCommandFlags flags, CompanyCtrlAction cca, CompanyID company_id, CompanyRemoveReason reason, ClientID client_id);
CommandCost CmdCompanyAllowListCtrl(DoCommandFlags flags, CompanyAllowListCtrlAction action, const std::string &public_key);
CommandCost CmdGiveMoney(DoCommandFlags flags, Money money, CompanyID dest_company);
CommandCost CmdRenameCompany(DoCommandFlags flags, const std::string &text);
CommandCost CmdRenamePresident(DoCommandFlags flags, const std::string &text);
CommandCost CmdSetCompanyManagerFace(DoCommandFlags flags, CompanyManagerFace cmf);
CommandCost CmdSetCompanyColour(DoCommandFlags flags, LiveryScheme scheme, bool primary, Colours colour);

DEF_CMD_TRAIT(CMD_COMPANY_CTRL,             CmdCompanyCtrl,           CommandFlags({CommandFlag::Spectator, CommandFlag::ClientID, CommandFlag::NoEst}), CMDT_SERVER_SETTING)
DEF_CMD_TRAIT(CMD_COMPANY_ALLOW_LIST_CTRL,  CmdCompanyAllowListCtrl,  CommandFlag::NoEst,                    CMDT_SERVER_SETTING)
DEF_CMD_TRAIT(CMD_GIVE_MONEY,               CmdGiveMoney,             {},                                          CMDT_MONEY_MANAGEMENT)
DEF_CMD_TRAIT(CMD_RENAME_COMPANY,           CmdRenameCompany,         {},                                          CMDT_COMPANY_SETTING)
DEF_CMD_TRAIT(CMD_RENAME_PRESIDENT,         CmdRenamePresident,       {},                                          CMDT_COMPANY_SETTING)
DEF_CMD_TRAIT(CMD_SET_COMPANY_MANAGER_FACE, CmdSetCompanyManagerFace, {},                                          CMDT_COMPANY_SETTING)
DEF_CMD_TRAIT(CMD_SET_COMPANY_COLOUR,       CmdSetCompanyColour,      {},                                          CMDT_COMPANY_SETTING)

#endif /* COMPANY_CMD_H */
