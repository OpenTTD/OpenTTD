/* $Id$ */

/** @file misc_cmd.cpp Some misc functions that are better fitted in other files, but never got moved there... */

#include "stdafx.h"
#include "openttd.h"
#include "command_func.h"
#include "economy_func.h"
#include "window_func.h"
#include "textbuf_gui.h"
#include "network/network.h"
#include "company_manager_face.h"
#include "strings_func.h"
#include "gfx_func.h"
#include "functions.h"
#include "vehicle_func.h"
#include "string_func.h"
#include "company_func.h"
#include "company_gui.h"
#include "settings_type.h"
#include "vehicle_base.h"

#include "table/strings.h"

/** Change the company manager's face.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 unused
 * @param p2 face bitmasked
 */
CommandCost CmdSetCompanyManagerFace(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	CompanyManagerFace cmf = (CompanyManagerFace)p2;

	if (!IsValidCompanyManagerFace(cmf)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		GetCompany(_current_company)->face = cmf;
		MarkWholeScreenDirty();
	}
	return CommandCost();
}

/** Change the company's company-colour
 * @param tile unused
 * @param flags operation to perform
 * @param p1 bitstuffed:
 * p1 bits 0-7 scheme to set
 * p1 bits 8-9 set in use state or first/second colour
 * @param p2 new colour for vehicles, property, etc.
 */
CommandCost CmdSetCompanyColour(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (p2 >= 16) return CMD_ERROR; // max 16 colours

	Colours colour = (Colours)p2;

	LiveryScheme scheme = (LiveryScheme)GB(p1, 0, 8);
	byte state = GB(p1, 8, 2);

	if (scheme >= LS_END || state >= 3) return CMD_ERROR;

	Company *c = GetCompany(_current_company);

	/* Ensure no two companies have the same primary colour */
	if (scheme == LS_DEFAULT && state == 0) {
		const Company *cc;
		FOR_ALL_COMPANIES(cc) {
			if (cc != c && cc->colour == colour) return CMD_ERROR;
		}
	}

	if (flags & DC_EXEC) {
		switch (state) {
			case 0:
				c->livery[scheme].colour1 = colour;

				/* If setting the first colour of the default scheme, adjust the
				 * original and cached company colours too. */
				if (scheme == LS_DEFAULT) {
					_company_colours[_current_company] = colour;
					c->colour = colour;
				}
				break;

			case 1:
				c->livery[scheme].colour2 = colour;
				break;

			case 2:
				c->livery[scheme].in_use = colour != 0;

				/* Now handle setting the default scheme's in_use flag.
				 * This is different to the other schemes, as it signifies if any
				 * scheme is active at all. If this flag is not set, then no
				 * processing of vehicle types occurs at all, and only the default
				 * colours will be used. */

				/* If enabling a scheme, set the default scheme to be in use too */
				if (colour != 0) {
					c->livery[LS_DEFAULT].in_use = true;
					break;
				}

				/* Else loop through all schemes to see if any are left enabled.
				 * If not, disable the default scheme too. */
				c->livery[LS_DEFAULT].in_use = false;
				for (scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
					if (c->livery[scheme].in_use) {
						c->livery[LS_DEFAULT].in_use = true;
						break;
					}
				}
				break;

			default:
				break;
		}
		ResetVehicleColourMap();
		MarkWholeScreenDirty();

		/* Company colour data is indirectly cached. */
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->owner == _current_company) v->cache_valid = 0;
		}
	}
	return CommandCost();
}

/** Increase the loan of your company.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 amount to increase the loan with, multitude of LOAN_INTERVAL. Only used when p2 == 2.
 * @param p2 when 0: loans LOAN_INTERVAL
 *           when 1: loans the maximum loan permitting money (press CTRL),
 *           when 2: loans the amount specified in p1
 */
CommandCost CmdIncreaseLoan(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Company *c = GetCompany(_current_company);

	if (c->current_loan >= _economy.max_loan) {
		SetDParam(0, _economy.max_loan);
		return_cmd_error(STR_702B_MAXIMUM_PERMITTED_LOAN);
	}

	Money loan;
	switch (p2) {
		default: return CMD_ERROR; // Invalid method
		case 0: // Take some extra loan
			loan = LOAN_INTERVAL;
			break;
		case 1: // Take a loan as big as possible
			loan = _economy.max_loan - c->current_loan;
			break;
		case 2: // Take the given amount of loan
			if ((((int32)p1 < LOAN_INTERVAL) || c->current_loan + (int32)p1 > _economy.max_loan || (p1 % LOAN_INTERVAL) != 0)) return CMD_ERROR;
			loan = p1;
			break;
	}

	/* Overflow protection */
	if (c->money + c->current_loan + loan < c->money) return CMD_ERROR;

	if (flags & DC_EXEC) {
		c->money        += loan;
		c->current_loan += loan;
		InvalidateCompanyWindows(c);
	}

	return CommandCost(EXPENSES_OTHER);
}

/** Decrease the loan of your company.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 amount to decrease the loan with, multitude of LOAN_INTERVAL. Only used when p2 == 2.
 * @param p2 when 0: pays back LOAN_INTERVAL
 *           when 1: pays back the maximum loan permitting money (press CTRL),
 *           when 2: pays back the amount specified in p1
 */
CommandCost CmdDecreaseLoan(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Company *c = GetCompany(_current_company);

	if (c->current_loan == 0) return_cmd_error(STR_702D_LOAN_ALREADY_REPAYED);

	Money loan;
	switch (p2) {
		default: return CMD_ERROR; // Invalid method
		case 0: // Pay back one step
			loan = min(c->current_loan, (Money)LOAN_INTERVAL);
			break;
		case 1: // Pay back as much as possible
			loan = max(min(c->current_loan, c->money), (Money)LOAN_INTERVAL);
			loan -= loan % LOAN_INTERVAL;
			break;
		case 2: // Repay the given amount of loan
			if ((p1 % LOAN_INTERVAL != 0) || ((int32)p1 < LOAN_INTERVAL)) return CMD_ERROR; // Invalid amount to loan
			loan = p1;
			break;
	}

	if (c->money < loan) {
		SetDParam(0, loan);
		return_cmd_error(STR_702E_REQUIRED);
	}

	if (flags & DC_EXEC) {
		c->money        -= loan;
		c->current_loan -= loan;
		InvalidateCompanyWindows(c);
	}
	return CommandCost();
}

static bool IsUniqueCompanyName(const char *name)
{
	const Company *c;

	FOR_ALL_COMPANIES(c) {
		if (c->name != NULL && strcmp(c->name, name) == 0) return false;
	}

	return true;
}

/** Change the name of the company.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 unused
 * @param p2 unused
 */
CommandCost CmdRenameCompany(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	bool reset = StrEmpty(text);

	if (!reset) {
		if (strlen(text) >= MAX_LENGTH_COMPANY_NAME_BYTES) return CMD_ERROR;
		if (!IsUniqueCompanyName(text)) return_cmd_error(STR_NAME_MUST_BE_UNIQUE);
	}

	if (flags & DC_EXEC) {
		Company *c = GetCompany(_current_company);
		free(c->name);
		c->name = reset ? NULL : strdup(text);
		MarkWholeScreenDirty();
	}

	return CommandCost();
}

static bool IsUniquePresidentName(const char *name)
{
	const Company *c;

	FOR_ALL_COMPANIES(c) {
		if (c->president_name != NULL && strcmp(c->president_name, name) == 0) return false;
	}

	return true;
}

/** Change the name of the president.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 unused
 * @param p2 unused
 */
CommandCost CmdRenamePresident(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	bool reset = StrEmpty(text);

	if (!reset) {
		if (strlen(text) >= MAX_LENGTH_PRESIDENT_NAME_BYTES) return CMD_ERROR;
		if (!IsUniquePresidentName(text)) return_cmd_error(STR_NAME_MUST_BE_UNIQUE);
	}

	if (flags & DC_EXEC) {
		Company *c = GetCompany(_current_company);
		free(c->president_name);

		if (reset) {
			c->president_name = NULL;
		} else {
			c->president_name = strdup(text);

			if (c->name_1 == STR_SV_UNNAMED && c->name == NULL) {
				char buf[80];

				snprintf(buf, lengthof(buf), "%s Transport", text);
				DoCommand(0, 0, 0, DC_EXEC, CMD_RENAME_COMPANY, buf);
			}
		}

		MarkWholeScreenDirty();
	}

	return CommandCost();
}

/**
 * In case of an unsafe unpause, we want the
 * user to confirm that it might crash.
 * @param w         unused
 * @param confirmed whether the user confirms his/her action
 */
static void AskUnsafeUnpauseCallback(Window *w, bool confirmed)
{
	DoCommandP(0, confirmed ? 0 : 1, 0, CMD_PAUSE);
}

/** Pause/Unpause the game (server-only).
 * Increase or decrease the pause counter. If the counter is zero,
 * the game is unpaused. A counter is used instead of a boolean value
 * to have more control over the game when saving/loading, etc.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 0 = decrease pause counter; 1 = increase pause counter
 * @param p2 unused
 */
CommandCost CmdPause(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (flags & DC_EXEC) {
		_pause_game += (p1 == 0) ? -1 : 1;

		switch (_pause_game) {
			case -4:
			case -1:
				_pause_game = 0;
				break;
			case -3:
				ShowQuery(
					STR_NEWGRF_UNPAUSE_WARNING_TITLE,
					STR_NEWGRF_UNPAUSE_WARNING,
					NULL,
					AskUnsafeUnpauseCallback
				);
				break;

			default: break;
		}

		InvalidateWindow(WC_STATUS_BAR, 0);
		InvalidateWindow(WC_MAIN_TOOLBAR, 0);
	}
	return CommandCost();
}

/** Change the financial flow of your company.
 * This is normally only enabled in offline mode, but if there is a debug
 * build, you can cheat (to test).
 * @param tile unused
 * @param flags operation to perform
 * @param p1 the amount of money to receive (if negative), or spend (if positive)
 * @param p2 unused
 */
CommandCost CmdMoneyCheat(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
#ifndef _DEBUG
	if (_networking) return CMD_ERROR;
#endif
	return CommandCost(EXPENSES_OTHER, -(int32)p1);
}

/** Transfer funds (money) from one company to another.
 * To prevent abuse in multiplayer games you can only send money to other
 * companies if you have paid off your loan (either explicitely, or implicitely
 * given the fact that you have more money than loan).
 * @param tile unused
 * @param flags operation to perform
 * @param p1 the amount of money to transfer; max 20.000.000
 * @param p2 the company to transfer the money to
 */
CommandCost CmdGiveMoney(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (!_settings_game.economy.give_money) return CMD_ERROR;

	const Company *c = GetCompany(_current_company);
	CommandCost amount(EXPENSES_OTHER, min((Money)p1, (Money)20000000LL));

	/* You can only transfer funds that is in excess of your loan */
	if (c->money - c->current_loan < amount.GetCost() || amount.GetCost() <= 0) return CMD_ERROR;
	if (!_networking || !IsValidCompanyID((CompanyID)p2)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		/* Add money to company */
		CompanyID old_company = _current_company;
		_current_company = (CompanyID)p2;
		SubtractMoneyFromCompany(CommandCost(EXPENSES_OTHER, -amount.GetCost()));
		_current_company = old_company;
	}

	/* Subtract money from local-company */
	return amount;
}
