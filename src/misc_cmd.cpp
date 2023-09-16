/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file misc_cmd.cpp Some misc functions that are better fitted in other files, but never got moved there... */

#include "stdafx.h"
#include "command_func.h"
#include "economy_func.h"
#include "window_func.h"
#include "textbuf_gui.h"
#include "network/network.h"
#include "network/network_func.h"
#include "strings_func.h"
#include "company_func.h"
#include "company_gui.h"
#include "company_base.h"
#include "tile_map.h"
#include "texteff.hpp"
#include "core/backup_type.hpp"
#include "misc_cmd.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Increase the loan of your company.
 * @param flags operation to perform
 * @param cmd when LoanCommand::Interval: loans LOAN_INTERVAL,
 *            when LoanCommand::Max: loans the maximum loan permitting money (press CTRL),
 *            when LoanCommand::Amount: loans the amount specified in \c amount
 * @param amount amount to increase the loan with, multitude of LOAN_INTERVAL. Only used when cmd == LoanCommand::Amount.
 * @return the cost of this operation or an error
 */
CommandCost CmdIncreaseLoan(DoCommandFlag flags, LoanCommand cmd, Money amount)
{
	Company *c = Company::Get(_current_company);

	if (c->current_loan >= _economy.max_loan) {
		SetDParam(0, _economy.max_loan);
		return_cmd_error(STR_ERROR_MAXIMUM_PERMITTED_LOAN);
	}

	Money loan;
	switch (cmd) {
		default: return CMD_ERROR; // Invalid method
		case LoanCommand::Interval: // Take some extra loan
			loan = LOAN_INTERVAL;
			break;
		case LoanCommand::Max: // Take a loan as big as possible
			loan = _economy.max_loan - c->current_loan;
			break;
		case LoanCommand::Amount: // Take the given amount of loan
			loan = amount;
			if (loan < LOAN_INTERVAL || c->current_loan + loan > _economy.max_loan || loan % LOAN_INTERVAL != 0) return CMD_ERROR;
			break;
	}

	/* In case adding the loan triggers the overflow protection of Money,
	 * we would essentially be losing money as taking and repaying the loan
	 * immediately would not get us back to the same bank balance anymore. */
	if (c->money > Money::max() - loan) return CMD_ERROR;

	if (flags & DC_EXEC) {
		c->money        += loan;
		c->current_loan += loan;
		InvalidateCompanyWindows(c);
	}

	return CommandCost(EXPENSES_OTHER);
}

/**
 * Decrease the loan of your company.
 * @param flags operation to perform
 * @param cmd when LoanCommand::Interval: pays back LOAN_INTERVAL,
 *            when LoanCommand::Max: pays back the maximum loan permitting money (press CTRL),
 *            when LoanCommand::Amount: pays back the amount specified in \c amount
 * @param amount amount to decrease the loan with, multitude of LOAN_INTERVAL. Only used when cmd == LoanCommand::Amount.
 * @return the cost of this operation or an error
 */
CommandCost CmdDecreaseLoan(DoCommandFlag flags, LoanCommand cmd, Money amount)
{
	Company *c = Company::Get(_current_company);

	if (c->current_loan == 0) return_cmd_error(STR_ERROR_LOAN_ALREADY_REPAYED);

	Money loan;
	switch (cmd) {
		default: return CMD_ERROR; // Invalid method
		case LoanCommand::Interval: // Pay back one step
			loan = std::min(c->current_loan, (Money)LOAN_INTERVAL);
			break;
		case LoanCommand::Max: // Pay back as much as possible
			loan = std::max(std::min(c->current_loan, c->money), (Money)LOAN_INTERVAL);
			loan -= loan % LOAN_INTERVAL;
			break;
		case LoanCommand::Amount: // Repay the given amount of loan
			loan = amount;
			if (loan % LOAN_INTERVAL != 0 || loan < LOAN_INTERVAL || loan > c->current_loan) return CMD_ERROR; // Invalid amount to loan
			break;
	}

	if (c->money < loan) {
		SetDParam(0, loan);
		return_cmd_error(STR_ERROR_CURRENCY_REQUIRED);
	}

	if (flags & DC_EXEC) {
		c->money        -= loan;
		c->current_loan -= loan;
		InvalidateCompanyWindows(c);
	}
	return CommandCost();
}

/**
 * In case of an unsafe unpause, we want the
 * user to confirm that it might crash.
 * @param confirmed whether the user confirmed their action
 */
static void AskUnsafeUnpauseCallback(Window *, bool confirmed)
{
	if (confirmed) {
		Command<CMD_PAUSE>::Post(PM_PAUSED_ERROR, false);
	}
}

/**
 * Pause/Unpause the game (server-only).
 * Set or unset a bit in the pause mode. If pause mode is zero the game is
 * unpaused. A bitset is used instead of a boolean value/counter to have
 * more control over the game when saving/loading, etc.
 * @param flags operation to perform
 * @param mode the pause mode to change
 * @param pause true pauses, false unpauses this mode
 * @return the cost of this operation or an error
 */
CommandCost CmdPause(DoCommandFlag flags, PauseMode mode, bool pause)
{
	switch (mode) {
		case PM_PAUSED_SAVELOAD:
		case PM_PAUSED_ERROR:
		case PM_PAUSED_NORMAL:
		case PM_PAUSED_GAME_SCRIPT:
		case PM_PAUSED_LINK_GRAPH:
			break;

		case PM_PAUSED_JOIN:
		case PM_PAUSED_ACTIVE_CLIENTS:
			if (!_networking) return CMD_ERROR;
			break;

		default: return CMD_ERROR;
	}
	if (flags & DC_EXEC) {
		if (mode == PM_PAUSED_NORMAL && _pause_mode & PM_PAUSED_ERROR) {
			ShowQuery(
				STR_NEWGRF_UNPAUSE_WARNING_TITLE,
				STR_NEWGRF_UNPAUSE_WARNING,
				nullptr,
				AskUnsafeUnpauseCallback
			);
		} else {
			PauseMode prev_mode = _pause_mode;

			if (pause) {
				_pause_mode |= mode;
			} else {
				_pause_mode &= ~mode;

				/* If the only remaining reason to be paused is that we saw a command during pause, unpause. */
				if (_pause_mode == PM_COMMAND_DURING_PAUSE) {
					_pause_mode = PM_UNPAUSED;
				}
			}

			NetworkHandlePauseChange(prev_mode, mode);
		}

		SetWindowDirty(WC_STATUS_BAR, 0);
		SetWindowDirty(WC_MAIN_TOOLBAR, 0);
	}
	return CommandCost();
}

/**
 * Change the financial flow of your company.
 * @param amount the amount of money to receive (if positive), or spend (if negative)
 * @return the cost of this operation or an error
 */
CommandCost CmdMoneyCheat(DoCommandFlag, Money amount)
{
	return CommandCost(EXPENSES_OTHER, -amount);
}

/**
 * Change the bank bank balance of a company by inserting or removing money without affecting the loan.
 * @param flags operation to perform
 * @param tile tile to show text effect on (if not 0)
 * @param delta the amount of money to receive (if positive), or spend (if negative)
 * @param company the company ID.
 * @param expenses_type the expenses type which should register the cost/income @see ExpensesType.
 * @return zero cost or an error
 */
CommandCost CmdChangeBankBalance(DoCommandFlag flags, TileIndex tile, Money delta, CompanyID company, ExpensesType expenses_type)
{
	if (!Company::IsValidID(company)) return CMD_ERROR;
	if (expenses_type >= EXPENSES_END) return CMD_ERROR;
	if (_current_company != OWNER_DEITY) return CMD_ERROR;

	if (flags & DC_EXEC) {
		/* Change company bank balance of company. */
		Backup<CompanyID> cur_company(_current_company, company, FILE_LINE);
		SubtractMoneyFromCompany(CommandCost(expenses_type, -delta));
		cur_company.Restore();

		if (tile != 0) {
			ShowCostOrIncomeAnimation(TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE, GetTilePixelZ(tile), -delta);
		}
	}

	/* This command doesn't cost anything for deity. */
	CommandCost zero_cost(expenses_type, (Money)0);
	return zero_cost;
}
