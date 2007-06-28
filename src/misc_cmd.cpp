/* $Id$ */

/** @file misc_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "string.h"
#include "table/strings.h"
#include "command.h"
#include "player.h"
#include "gfx.h"
#include "window.h"
#include "gui.h"
#include "economy.h"
#include "network/network.h"
#include "variables.h"
#include "livery.h"
#include "player_face.h"
#include "strings.h"

/** Change the player's face.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 unused
 * @param p2 face bitmasked
 */
CommandCost CmdSetPlayerFace(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	PlayerFace pf = (PlayerFace)p2;

	if (!IsValidPlayerFace(pf)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		GetPlayer(_current_player)->face = pf;
		MarkWholeScreenDirty();
	}
	return CommandCost();
}

/** Change the player's company-colour
 * @param tile unused
 * @param flags operation to perform
 * @param p1 bitstuffed:
 * p1 bits 0-7 scheme to set
 * p1 bits 8-9 set in use state or first/second colour
 * @param p2 new colour for vehicles, property, etc.
 */
CommandCost CmdSetPlayerColor(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p, *pp;
	byte colour;
	LiveryScheme scheme = (LiveryScheme)GB(p1, 0, 8);
	byte state = GB(p1, 8, 2);

	if (p2 >= 16) return CMD_ERROR; // max 16 colours
	colour = p2;

	if (scheme >= LS_END || state >= 3) return CMD_ERROR;

	p = GetPlayer(_current_player);

	/* Ensure no two companies have the same primary colour */
	if (scheme == LS_DEFAULT && state == 0) {
		FOR_ALL_PLAYERS(pp) {
			if (pp->is_active && pp != p && pp->player_color == colour) return CMD_ERROR;
		}
	}

	if (flags & DC_EXEC) {
		switch (state) {
			case 0:
				p->livery[scheme].colour1 = colour;

				/* If setting the first colour of the default scheme, adjust the
				 * original and cached player colours too. */
				if (scheme == LS_DEFAULT) {
					_player_colors[_current_player] = colour;
					p->player_color = colour;
				}
				break;

			case 1:
				p->livery[scheme].colour2 = colour;
				break;

			case 2:
				p->livery[scheme].in_use = colour != 0;

				/* Now handle setting the default scheme's in_use flag.
				 * This is different to the other schemes, as it signifies if any
				 * scheme is active at all. If this flag is not set, then no
				 * processing of vehicle types occurs at all, and only the default
				 * colours will be used. */

				/* If enabling a scheme, set the default scheme to be in use too */
				if (colour != 0) {
					p->livery[LS_DEFAULT].in_use = true;
					break;
				}

				/* Else loop through all schemes to see if any are left enabled.
				 * If not, disable the default scheme too. */
				p->livery[LS_DEFAULT].in_use = false;
				for (scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
					if (p->livery[scheme].in_use) {
						p->livery[LS_DEFAULT].in_use = true;
						break;
					}
				}
				break;

			default:
				break;
		}
		MarkWholeScreenDirty();
	}
	return CommandCost();
}

/** Increase the loan of your company.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 unused
 * @param p2 when 0: loans LOAN_INTERVAL
 *           when 1: loans the maximum loan permitting money (press CTRL),
 */
CommandCost CmdIncreaseLoan(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p = GetPlayer(_current_player);

	if (p->current_loan >= _economy.max_loan) {
		SetDParam(0, _economy.max_loan);
		return_cmd_error(STR_702B_MAXIMUM_PERMITTED_LOAN);
	}

	Money loan;
	switch (p2) {
		default: return CMD_ERROR; // Invalid method
		case 0: // Take some extra loan
			loan = (IsHumanPlayer(_current_player) || _patches.ainew_active) ? LOAN_INTERVAL : LOAN_INTERVAL_OLD_AI;
			break;
		case 1: // Take a loan as big as possible
			loan = _economy.max_loan - p->current_loan;
			break;
	}

	/* Overflow protection */
	if (p->player_money + p->current_loan + loan < p->player_money) return CMD_ERROR;

	if (flags & DC_EXEC) {
		p->player_money += loan;
		p->current_loan += loan;
		InvalidatePlayerWindows(p);
	}

	return CommandCost();
}

/** Decrease the loan of your company.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 unused
 * @param p2 when 0: pays back LOAN_INTERVAL
 *           when 1: pays back the maximum loan permitting money (press CTRL),
 */
CommandCost CmdDecreaseLoan(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p = GetPlayer(_current_player);

	if (p->current_loan == 0) return_cmd_error(STR_702D_LOAN_ALREADY_REPAYED);

	Money loan;
	switch (p2) {
		default: return CMD_ERROR; // Invalid method
		case 0: // Pay back one step
			loan = min(p->current_loan, (Money)(IsHumanPlayer(_current_player) || _patches.ainew_active) ? LOAN_INTERVAL : LOAN_INTERVAL_OLD_AI);
			break;
		case 1: // Pay back as much as possible
			loan = max(min(p->current_loan, p->player_money), (Money)LOAN_INTERVAL);
			loan -= loan % LOAN_INTERVAL;
			break;
	}

	if (p->player_money < loan) {
		SetDParam(0, loan);
		return_cmd_error(STR_702E_REQUIRED);
	}

	if (flags & DC_EXEC) {
		p->player_money -= loan;
		p->current_loan -= loan;
		InvalidatePlayerWindows(p);
	}
	return CommandCost();
}

static bool IsUniqueCompanyName(const char *name)
{
	const Player *p;
	char buf[512];

	FOR_ALL_PLAYERS(p) {
		if (!p->is_active) continue;
		SetDParam(0, p->index);
		GetString(buf, STR_COMPANY_NAME, lastof(buf));
		if (strcmp(buf, name) == 0) return false;
	}

	return true;
}

/** Change the name of the company.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 unused
 * @param p2 unused
 */
CommandCost CmdChangeCompanyName(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	StringID str;
	Player *p;

	if (StrEmpty(_cmd_text)) return CMD_ERROR;

	if (!IsUniqueCompanyName(_cmd_text)) return_cmd_error(STR_NAME_MUST_BE_UNIQUE);

	str = AllocateName(_cmd_text, 4);
	if (str == 0) return CMD_ERROR;

	if (flags & DC_EXEC) {
		p = GetPlayer(_current_player);
		DeleteName(p->name_1);
		p->name_1 = str;
		MarkWholeScreenDirty();
	} else {
		DeleteName(str);
	}

	return CommandCost();
}

static bool IsUniquePresidentName(const char *name)
{
	const Player *p;
	char buf[512];

	FOR_ALL_PLAYERS(p) {
		if (!p->is_active) continue;
		SetDParam(0, p->index);
		GetString(buf, STR_PLAYER_NAME, lastof(buf));
		if (strcmp(buf, name) == 0) return false;
	}

	return true;
}

/** Change the name of the president.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 unused
 * @param p2 unused
 */
CommandCost CmdChangePresidentName(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	StringID str;
	Player *p;

	if (StrEmpty(_cmd_text)) return CMD_ERROR;

	if (!IsUniquePresidentName(_cmd_text)) return_cmd_error(STR_NAME_MUST_BE_UNIQUE);

	str = AllocateName(_cmd_text, 4);
	if (str == 0) return CMD_ERROR;

	if (flags & DC_EXEC) {
		p = GetPlayer(_current_player);
		DeleteName(p->president_name_1);
		p->president_name_1 = str;

		if (p->name_1 == STR_SV_UNNAMED) {
			char buf[80];

			snprintf(buf, lengthof(buf), "%s Transport", _cmd_text);
			_cmd_text = buf;
			DoCommand(0, 0, 0, DC_EXEC, CMD_CHANGE_COMPANY_NAME);
		}
		MarkWholeScreenDirty();
	} else {
		DeleteName(str);
	}

	return CommandCost();
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
CommandCost CmdPause(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (flags & DC_EXEC) {
		_pause_game += (p1 == 1) ? 1 : -1;
		if (_pause_game == (byte)-1) _pause_game = 0;
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
CommandCost CmdMoneyCheat(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
#ifndef _DEBUG
	if (_networking) return CMD_ERROR;
#endif
	SET_EXPENSES_TYPE(EXPENSES_OTHER);
	return CommandCost(-(Money)p1);
}

/** Transfer funds (money) from one player to another.
 * To prevent abuse in multiplayer games you can only send money to other
 * players if you have paid off your loan (either explicitely, or implicitely
 * given the fact that you have more money than loan).
 * @param tile unused
 * @param flags operation to perform
 * @param p1 the amount of money to transfer; max 20.000.000
 * @param p2 the player to transfer the money to
 */
CommandCost CmdGiveMoney(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	const Player *p = GetPlayer(_current_player);
	CommandCost amount(min((Money)p1, (Money)20000000LL));

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	/* You can only transfer funds that is in excess of your loan */
	if (p->player_money - p->current_loan < amount.GetCost() || amount.GetCost() <= 0) return CMD_ERROR;
	if (!_networking || !IsValidPlayer((PlayerID)p2)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		/* Add money to player */
		PlayerID old_cp = _current_player;
		_current_player = (PlayerID)p2;
		SubtractMoneyFromPlayer(CommandCost(-amount.GetCost()));
		_current_player = old_cp;
	}

	/* Subtract money from local-player */
	return amount;
}

/** Change difficulty level/settings (server-only).
 * We cannot really check for valid values of p2 (too much work mostly); stored
 * in file 'settings_gui.c' _game_setting_info[]; we'll just trust the server it knows
 * what to do and does this correctly
 * @param tile unused
 * @param flags operation to perform
 * @param p1 the difficulty setting being changed. If it is -1, the difficulty level
 *           itself is changed. The new value is inside p2
 * @param p2 new value for a difficulty setting or difficulty level
 */
CommandCost CmdChangeDifficultyLevel(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (p1 != (uint32)-1L && ((int32)p1 >= GAME_DIFFICULTY_NUM || (int32)p1 < 0)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (p1 != (uint32)-1L) {
			((int*)&_opt_ptr->diff)[p1] = p2;
			_opt_ptr->diff_level = 3; // custom difficulty level
		} else {
			_opt_ptr->diff_level = p2;
		}

		/* If we are a network-client, update the difficult setting (if it is open).
		 * Use this instead of just dirtying the window because we need to load in
		 * the new difficulty settings */
		if (_networking && !_network_server && FindWindowById(WC_GAME_OPTIONS, 0) != NULL)
			ShowGameDifficulty();
	}
	return CommandCost();
}
