#include "stdafx.h"
#include "ttd.h"
#include "string.h"
#include "table/strings.h"
#include "command.h"
#include "player.h"
#include "gfx.h"
#include "window.h"
#include "gui.h"
#include "saveload.h"
#include "economy.h"
#include "network.h"

/* p1 = player
   p2 = face
 */

int32 CmdSetPlayerFace(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (flags & DC_EXEC) {
		DEREF_PLAYER(p1)->face = p2;
		MarkWholeScreenDirty();
	}
	return 0;
}

/* p1 = player
 * p2 = color
 */
int32 CmdSetPlayerColor(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p,*pp;

//	/* can only set color for itself */
//	if ( (byte)p1 != _current_player)
//		return CMD_ERROR;

	p = DEREF_PLAYER(p1);

	/* ensure no dups */
	FOR_ALL_PLAYERS(pp) {
		if (pp->is_active && pp != p && pp->player_color == (byte)p2)
			return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		_player_colors[p1] = (byte)p2;
		p->player_color = (byte)p2;
		MarkWholeScreenDirty();
	}
	return 0;
}

int32 CmdIncreaseLoan(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p;
	int32 size;

	if ( (byte)p1 != _current_player)
		return CMD_ERROR;

	p = DEREF_PLAYER(p1);

	if (p->current_loan >= _economy.max_loan) {
		SetDParam(0, _economy.max_loan);
		return_cmd_error(STR_702B_MAXIMUM_PERMITTED_LOAN);
	}

	if (flags & DC_EXEC) {
		if (p2)
			size = _economy.max_loan - p->current_loan;
		else
			size = IS_HUMAN_PLAYER((byte)p1) ? 10000 : 50000;

		p->money64 += size;
		p->current_loan += size;
		UpdatePlayerMoney32(p);
		InvalidatePlayerWindows(p);
	}

	return 0;
}

int32 CmdDecreaseLoan(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p;
	int32 size;
	if ( (byte)p1 != _current_player)
		return CMD_ERROR;

	p = DEREF_PLAYER(p1);

	if (p->current_loan == 0)
		return_cmd_error(STR_702D_LOAN_ALREADY_REPAYED);

	size = p->current_loan;

	// p2 is true while CTRL is pressed (repay all possible loan, or max money you have)
	if (!p2) {
	    if (_patches.ainew_active)
 		    size = min(size, 10000);
	    else
		    size = min(size, IS_HUMAN_PLAYER((byte)p1) ? 10000 : 50000);
	} else {	// only repay in chunks of 10K
		size = min(size, p->player_money);
		size = max(size, 10000);
		size -= size % 10000;
	}

	if (p->player_money < size) {
		SetDParam(0, size);
		return_cmd_error(STR_702E_REQUIRED);
	}

	if (flags & DC_EXEC) {
		p->money64 -= size;
		p->current_loan -= size;
		UpdatePlayerMoney32(p);
		InvalidatePlayerWindows(p);
	}
	return 0;
}

int32 CmdChangeCompanyName(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	StringID str,old_str;
	Player *p;

	str = AllocateNameUnique((const char*)_decode_parameters, 4);
	if (str == 0)
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		p = DEREF_PLAYER(p1);
		old_str = p->name_1;
		p->name_1 = str;
		DeleteName(old_str);
		MarkWholeScreenDirty();
	} else {
		DeleteName(str);
	}

	return 0;
}

int32 CmdChangePresidentName(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	StringID str,old_str;
	Player *p;

	str = AllocateNameUnique((const char*)_decode_parameters, 4);
	if (str == 0)
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		p = DEREF_PLAYER(p1);
		old_str = p->president_name_1;
		p->president_name_1 = str;
		DeleteName(old_str);

		if (p->name_1 == STR_SV_UNNAMED) {
			ttd_strlcat(
				(char*)_decode_parameters, " Transport", sizeof(_decode_parameters));
			DoCommandByTile(0, p1, 0, DC_EXEC, CMD_CHANGE_COMPANY_NAME);
		}
		MarkWholeScreenDirty();
	} else {
		DeleteName(str);
	}

	return 0;
}

// p1 = 0   decrease pause counter
// p1 = 1   increase pause counter
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


int32 CmdMoneyCheat(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	SET_EXPENSES_TYPE(EXPENSES_OTHER);
	return (int32)p1;
}

int32 CmdGiveMoney(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	p1 = clamp(p1, 0, 0xFFFFFF); // Clamp between 16 million and 0

	if (p1 == 0)
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		// Add money to player
		byte old_cp = _current_player;
		_current_player = p2;
		SubtractMoneyFromPlayer(-(int32)p1);
		_current_player = old_cp;
	}

	// Subtract money from local-player
	return (int32)p1;
}

int32 CmdChangeDifficultyLevel(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (flags & DC_EXEC) {
		if (p1 != (uint32)-1L) {
			((int*)&_opt_ptr->diff)[p1] = p2;
			_opt_ptr->diff_level = 3;
		} else
			_opt_ptr->diff_level = p2;

		// If we are a network-client, update the difficult setting (if it is open)
		if (_networking && !_network_server && FindWindowById(WC_GAME_OPTIONS, 0) != NULL)
			ShowGameDifficulty();
	}
	return 0;
}
