/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file misc_cmd.h Miscellaneous command definitions. */

#ifndef MISC_CMD_H
#define MISC_CMD_H

#include "command_type.h"
#include "economy_type.h"
#include "openttd.h"

enum class LoanCommand : uint8_t {
	Interval,
	Max,
	Amount,
};

CommandCost CmdMoneyCheat(DoCommandFlags flags, Money amount);
CommandCost CmdChangeBankBalance(DoCommandFlags flags, TileIndex tile, Money delta, CompanyID company, ExpensesType expenses_type);
CommandCost CmdIncreaseLoan(DoCommandFlags flags, LoanCommand cmd, Money amount);
CommandCost CmdDecreaseLoan(DoCommandFlags flags, LoanCommand cmd, Money amount);
CommandCost CmdSetCompanyMaxLoan(DoCommandFlags flags, CompanyID company, Money amount);
CommandCost CmdPause(DoCommandFlags flags, PauseMode mode, bool pause);

DEF_CMD_TRAIT(CMD_MONEY_CHEAT,          CmdMoneyCheat,        CommandFlags({CommandFlag::Offline, CommandFlag::NoEst}), CommandType::Cheat)
DEF_CMD_TRAIT(CMD_CHANGE_BANK_BALANCE,  CmdChangeBankBalance, CommandFlag::Deity,               CommandType::MoneyManagement)
DEF_CMD_TRAIT(CMD_INCREASE_LOAN,        CmdIncreaseLoan,      {},                       CommandType::MoneyManagement)
DEF_CMD_TRAIT(CMD_DECREASE_LOAN,        CmdDecreaseLoan,      {},                       CommandType::MoneyManagement)
DEF_CMD_TRAIT(CMD_SET_COMPANY_MAX_LOAN, CmdSetCompanyMaxLoan, CommandFlag::Deity,               CommandType::MoneyManagement)
DEF_CMD_TRAIT(CMD_PAUSE,                CmdPause,             CommandFlags({CommandFlag::Server, CommandFlag::NoEst}), CommandType::ServerSetting)

#endif /* MISC_CMD_H */
