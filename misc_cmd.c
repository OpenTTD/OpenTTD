#include "stdafx.h"
#include "ttd.h"
#include "string.h"
#include "table/strings.h"
#include "command.h"
#include "player.h"
#include "gfx.h"
#include "window.h"
#include "gui.h"
#include "economy.h"
#include "network.h"

/** Change the player's face.
 * @param x,y unused
 * @param p1 unused
 * @param p2 face bitmasked
 */
int32 CmdSetPlayerFace(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (flags & DC_EXEC) {
		GetPlayer(_current_player)->face = p2;
		MarkWholeScreenDirty();
	}
	return 0;
}

/** Change the player's company-colour
 * @param x,y unused
 * @param p1 unused
 * @param p2 new colour for vehicles, property, etc.
 */
int32 CmdSetPlayerColor(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p, *pp;
	byte colour = (byte)p2;

	if (p2 >= 16) return CMD_ERROR; // max 16 colours

	p = GetPlayer(_current_player);

	/* Ensure no two companies have the same colour */
	FOR_ALL_PLAYERS(pp) {
		if (pp->is_active && pp != p && pp->player_color == colour)
			return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		_player_colors[_current_player] = colour;
		p->player_color = colour;
		MarkWholeScreenDirty();
	}
	return 0;
}

/** Increase the loan of your company.
 * @param x,y unused
 * @param p1 unused
 * @param p2 when set, loans the maximum amount in one go (press CTRL)
 */
int32 CmdIncreaseLoan(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p;

	p = GetPlayer(_current_player);

	if (p->current_loan >= _economy.max_loan) {
		SetDParam(0, _economy.max_loan);
		return_cmd_error(STR_702B_MAXIMUM_PERMITTED_LOAN);
	}

	if (flags & DC_EXEC) {
		/* Loan the maximum amount or not? */
		int32 loan = (p2) ? _economy.max_loan - p->current_loan : IS_HUMAN_PLAYER(_current_player) ? 10000 : 50000;

		p->money64 += loan;
		p->current_loan += loan;
		UpdatePlayerMoney32(p);
		InvalidatePlayerWindows(p);
	}

	return 0;
}

/** Decrease the loan of your company.
 * @param x,y unused
 * @param p1 unused
 * @param p2 when set, pays back the maximum loan permitting money (press CTRL)
 */
int32 CmdDecreaseLoan(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p;
	int32 loan;

	p = GetPlayer(_current_player);

	if (p->current_loan == 0) return_cmd_error(STR_702D_LOAN_ALREADY_REPAYED);

	loan = p->current_loan;

	/* p2 is true while CTRL is pressed (repay all possible loan, or max money you have)
	 * Repay any loan in chunks of 10.000 pounds */
	if (p2) {
		loan = min(loan, p->player_money);
		loan = max(loan, 10000);
		loan -= loan % 10000;
	} else {
		loan = (_patches.ainew_active) ? min(loan, 10000) : min(loan, IS_HUMAN_PLAYER(_current_player) ? 10000 : 50000);
	}

	if (p->player_money < loan) {
		SetDParam(0, loan);
		return_cmd_error(STR_702E_REQUIRED);
	}

	if (flags & DC_EXEC) {
		p->money64 -= loan;
		p->current_loan -= loan;
		UpdatePlayerMoney32(p);
		InvalidatePlayerWindows(p);
	}
	return 0;
}

/** Change the name of the company.
 * @param x,y unused
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdChangeCompanyName(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	StringID str;
	Player *p;

	if (_cmd_text[0] == '\0') return CMD_ERROR;

	str = AllocateNameUnique(_cmd_text, 4);
	if (str == 0) return CMD_ERROR;

	if (flags & DC_EXEC) {
		p = DEREF_PLAYER(_current_player);
		DeleteName(p->name_1);
		p->name_1 = str;
		MarkWholeScreenDirty();
	} else
		DeleteName(str);

	return 0;
}

/** Change the name of the president.
 * @param x,y unused
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdChangePresidentName(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	StringID str;
	Player *p;

	if (_cmd_text[0] == '\0') return CMD_ERROR;

	str = AllocateNameUnique(_cmd_text, 4);
	if (str == 0) return CMD_ERROR;

	if (flags & DC_EXEC) {
		p = DEREF_PLAYER(_current_player);
		DeleteName(p->president_name_1);
		p->president_name_1 = str;

		if (p->name_1 == STR_SV_UNNAMED) {
			char buf[80];

			snprintf(buf, lengthof(buf), "%s Transport", _cmd_text);
			_cmd_text = buf;
			DoCommandByTile(0, 0, 0, DC_EXEC, CMD_CHANGE_COMPANY_NAME);
		}
		MarkWholeScreenDirty();
	} else
		DeleteName(str);

	return 0;
}

/** Pause/Unpause the game (server-only).
 * Increase or decrease the pause counter. If the counter is zero,
 * the game is unpaused. A counter is used instead of a boolean value
 * to have more control over the game when saving/loading, etc.
 * @param x,y unused
 * @param p1 0 = decrease pause counter; 1 = increase pause counter
 * @param p2 unused
 */
int32 CmdPause(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (flags & DC_EXEC) {
		_pause += (p1 == 1) ? 1 : -1;
		if (_pause == (byte)-1) _pause = 0;
		InvalidateWindow(WC_STATUS_BAR, 0);
		InvalidateWindow(WC_MAIN_TOOLBAR, 0);
	}
	return 0;
}

/** Change the financial flow of your company.
 * This is normally only enabled in offline mode, but if there is a debug
 * build, you can cheat (to test).
 * @param x,y unused
 * @param p1 the amount of money to receive (if negative), or spend (if positive)
 * @param p2 unused
 */
int32 CmdMoneyCheat(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
#ifndef _DEBUG
	if (_networking) return CMD_ERROR;
#endif
	SET_EXPENSES_TYPE(EXPENSES_OTHER);
	return (int32)p1;
}

/** Transfer funds (money) from one player to another.
 * To prevent abuse	in multiplayer games you can only send money to other
 * players if you have paid off your loan (either explicitely, or implicitely
 * given the fact that you have more money than loan).
 * @param x,y unused
 * @param p1 the amount of money to transfer; max 20.000.000
 * @param p2 the player to transfer the money to
 */
int32 CmdGiveMoney(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	const Player *p = DEREF_PLAYER(_current_player);
	int32 amount = min((int32)p1, 20000000);

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	/* You can only transfer funds that is in excess of your loan */
	if (p->money64 - p->current_loan < amount || amount <= 0) return CMD_ERROR;
	if (!_networking || p2 >= MAX_PLAYERS) return CMD_ERROR;

	if (flags & DC_EXEC) {
		/* Add money to player */
		PlayerID old_cp = _current_player;
		_current_player = p2;
		SubtractMoneyFromPlayer(-amount);
		_current_player = old_cp;
	}

	/* Subtract money from local-player */
	return amount;
}

/** Change difficulty level/settings (server-only).
 * We cannot really check for valid values of p2 (too much work mostly); stored
 * in file 'settings_gui.c' _game_setting_info[]; we'll just trust the server it knows
 * what to do and does this correctly
 * @param x,y unused
 * @param p1 the difficulty setting being changed. If it is -1, the difficulty level
 *           itself is changed. The new value is inside p2
 * @param p2 new value for a difficulty setting or difficulty level
 */
int32 CmdChangeDifficultyLevel(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (p1 != (uint32)-1L && ((int32)p1 >= GAME_DIFFICULTY_NUM || (int32)p1 < 0)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (p1 != (uint32)-1L) {
			((int*)&_opt_ptr->diff)[p1] = p2;
			_opt_ptr->diff_level = 3; // custom difficulty level
		} else
			_opt_ptr->diff_level = p2;

		/* If we are a network-client, update the difficult setting (if it is open).
		 * Use this instead of just dirtying the window because we need to load in
		 * the new difficulty settings */
		if (_networking && !_network_server && FindWindowById(WC_GAME_OPTIONS, 0) != NULL)
			ShowGameDifficulty();
	}
	return 0;
}
