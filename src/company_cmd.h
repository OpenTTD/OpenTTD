/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
CommandCost CmdSetCompanyManagerFace(DoCommandFlags flags, uint style, uint32_t bits);
CommandCost CmdSetCompanyColour(DoCommandFlags flags, LiveryScheme scheme, bool primary, Colours colour);

DEF_CMD_TRAIT(Commands::CompanyControl, CmdCompanyCtrl, CommandFlags({CommandFlag::Spectator, CommandFlag::ClientID, CommandFlag::NoEst}), CommandType::ServerSetting)
DEF_CMD_TRAIT(Commands::CompanyAllowListControl, CmdCompanyAllowListCtrl, CommandFlag::NoEst, CommandType::ServerSetting)
DEF_CMD_TRAIT(Commands::GiveMoney, CmdGiveMoney, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::RenameCompany, CmdRenameCompany, {}, CommandType::CompanySetting)
DEF_CMD_TRAIT(Commands::RenamePresident, CmdRenamePresident, {}, CommandType::CompanySetting)
DEF_CMD_TRAIT(Commands::SetCompanyManagerFace, CmdSetCompanyManagerFace, {}, CommandType::CompanySetting)
DEF_CMD_TRAIT(Commands::SetCompanyColour, CmdSetCompanyColour, {}, CommandType::CompanySetting)

#endif /* COMPANY_CMD_H */
