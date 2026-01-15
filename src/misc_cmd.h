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

/** Different ways to determine the amount to loan/repay. */
enum class LoanCommand : uint8_t {
	Interval, ///< Loan/repay LOAN_INTERVAL.
	Max, ///< Loan/repay the maximum amount permitting money/settings.
	Amount, ///< Loan/repay the given amount.
};

CommandCost CmdMoneyCheat(DoCommandFlags flags, Money amount);
CommandCost CmdChangeBankBalance(DoCommandFlags flags, TileIndex tile, Money delta, CompanyID company, ExpensesType expenses_type);
CommandCost CmdIncreaseLoan(DoCommandFlags flags, LoanCommand cmd, Money amount);
CommandCost CmdDecreaseLoan(DoCommandFlags flags, LoanCommand cmd, Money amount);
CommandCost CmdSetCompanyMaxLoan(DoCommandFlags flags, CompanyID company, Money amount);
CommandCost CmdPause(DoCommandFlags flags, PauseMode mode, bool pause);

DEF_CMD_TRAIT(Commands::MoneyCheat, CmdMoneyCheat, CommandFlags({CommandFlag::Offline, CommandFlag::NoEst}), CommandType::Cheat)
DEF_CMD_TRAIT(Commands::ChangeBankBalance, CmdChangeBankBalance, CommandFlag::Deity, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::IncreaseLoan, CmdIncreaseLoan, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::DecreaseLoan, CmdDecreaseLoan, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::SetCompanyMaxLoan, CmdSetCompanyMaxLoan, CommandFlag::Deity, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::Pause, CmdPause, CommandFlags({CommandFlag::Server, CommandFlag::NoEst}), CommandType::ServerSetting)

#endif /* MISC_CMD_H */
