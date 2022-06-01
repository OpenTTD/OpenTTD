/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file economy_cmd.h Command definitions related to the economy. */

#ifndef ECONOMY_CMD_H
#define ECONOMY_CMD_H

#include "command_type.h"
#include "company_type.h"

CommandCost CmdBuyShareInCompany(DoCommandFlag flags, CompanyID target_company);
CommandCost CmdSellShareInCompany(DoCommandFlag flags, CompanyID target_company);
CommandCost CmdBuyCompany(DoCommandFlag flags, CompanyID target_company);

DEF_CMD_TRAIT(CMD_BUY_SHARE_IN_COMPANY,  CmdBuyShareInCompany,  0, CMDT_MONEY_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SELL_SHARE_IN_COMPANY, CmdSellShareInCompany, 0, CMDT_MONEY_MANAGEMENT)
DEF_CMD_TRAIT(CMD_BUY_COMPANY,           CmdBuyCompany,         0, CMDT_MONEY_MANAGEMENT)

#endif /* ECONOMY_CMD_H */
