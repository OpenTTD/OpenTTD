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

template <> struct CommandTraits<CMD_COMPANY_CTRL>             : DefaultCommandTraits<CMD_COMPANY_CTRL,             "CmdCompanyCtrl",           CmdCompanyCtrl,           CMD_SPECTATOR | CMD_CLIENT_ID | CMD_NO_EST, CMDT_SERVER_SETTING> {};
template <> struct CommandTraits<CMD_COMPANY_ALLOW_LIST_CTRL>  : DefaultCommandTraits<CMD_COMPANY_ALLOW_LIST_CTRL,  "CmdCompanyAllowListCtrl",  CmdCompanyAllowListCtrl,  CMD_NO_EST,                                 CMDT_SERVER_SETTING> {};
template <> struct CommandTraits<CMD_GIVE_MONEY>               : DefaultCommandTraits<CMD_GIVE_MONEY,               "CmdGiveMoney",             CmdGiveMoney,             {},                                         CMDT_MONEY_MANAGEMENT> {};
template <> struct CommandTraits<CMD_RENAME_COMPANY>           : DefaultCommandTraits<CMD_RENAME_COMPANY,           "CmdRenameCompany",         CmdRenameCompany,         {},                                         CMDT_COMPANY_SETTING> {};
template <> struct CommandTraits<CMD_RENAME_PRESIDENT>         : DefaultCommandTraits<CMD_RENAME_PRESIDENT,         "CmdRenamePresident",       CmdRenamePresident,       {},                                         CMDT_COMPANY_SETTING> {};
template <> struct CommandTraits<CMD_SET_COMPANY_MANAGER_FACE> : DefaultCommandTraits<CMD_SET_COMPANY_MANAGER_FACE, "CmdSetCompanyManagerFace", CmdSetCompanyManagerFace, {},                                         CMDT_COMPANY_SETTING> {};
template <> struct CommandTraits<CMD_SET_COMPANY_COLOUR>       : DefaultCommandTraits<CMD_SET_COMPANY_COLOUR,       "CmdSetCompanyColour",      CmdSetCompanyColour,      {},                                         CMDT_COMPANY_SETTING> {};

#endif /* COMPANY_CMD_H */
