/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file signs_cmd.cpp Handling of sign related commands. */

#include "stdafx.h"
#include "landscape.h"
#include "company_func.h"
#include "signs_base.h"
#include "signs_func.h"
#include "command_func.h"
#include "tilehighlight_func.h"
#include "viewport_kdtree.h"
#include "window_func.h"
#include "string_func.h"
#include "signs_cmd.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Place a sign at the given coordinates. Ownership of sign has
 * no effect whatsoever except for the colour the sign gets for easy recognition,
 * but everybody is able to rename/remove it.
 * @param tile tile to place sign at
 * @param flags type of operation
 * @param text contents of the sign
 * @return the cost of this operation + the ID of the new sign or an error
 */
std::tuple<CommandCost, SignID> CmdPlaceSign(DoCommandFlag flags, TileIndex tile, const std::string &text)
{
	/* Try to locate a new sign */
	if (!Sign::CanAllocateItem()) return { CommandCost(STR_ERROR_TOO_MANY_SIGNS), INVALID_SIGN };

	/* Check sign text length if any */
	if (Utf8StringLength(text) >= MAX_LENGTH_SIGN_NAME_CHARS) return { CMD_ERROR, INVALID_SIGN };

	/* When we execute, really make the sign */
	if (flags & DC_EXEC) {
		Sign *si = new Sign(_game_mode == GM_EDITOR ? OWNER_DEITY : _current_company);
		int x = TileX(tile) * TILE_SIZE;
		int y = TileY(tile) * TILE_SIZE;

		si->x = x;
		si->y = y;
		si->z = GetSlopePixelZ(x, y);
		if (!text.empty()) {
			si->name = text;
		}
		si->UpdateVirtCoord();
		InvalidateWindowData(WC_SIGN_LIST, 0, 0);
		return { CommandCost(), si->index };
	}

	return { CommandCost(), INVALID_SIGN };
}

/**
 * Rename a sign. If the new name of the sign is empty, we assume
 * the user wanted to delete it. So delete it. Ownership of signs
 * has no meaning/effect whatsoever except for eyecandy
 * @param flags type of operation
 * @param sign_id index of the sign to be renamed/removed
 * @param text the new name or an empty string when resetting to the default
 * @return the cost of this operation or an error
 */
CommandCost CmdRenameSign(DoCommandFlag flags, SignID sign_id, const std::string &text)
{
	Sign *si = Sign::GetIfValid(sign_id);
	if (si == nullptr) return CMD_ERROR;
	if (!CompanyCanRenameSign(si)) return CMD_ERROR;

	/* Rename the signs when empty, otherwise remove it */
	if (!text.empty()) {
		if (Utf8StringLength(text) >= MAX_LENGTH_SIGN_NAME_CHARS) return CMD_ERROR;

		if (flags & DC_EXEC) {
			/* Assign the new one */
			si->name = text;
			if (_game_mode != GM_EDITOR) si->owner = _current_company;

			si->UpdateVirtCoord();
			InvalidateWindowData(WC_SIGN_LIST, 0, 1);
		}
	} else { // Delete sign
		if (flags & DC_EXEC) {
			si->sign.MarkDirty();
			if (si->sign.kdtree_valid) _viewport_sign_kdtree.Remove(ViewportSignKdtreeItem::MakeSign(si->index));
			delete si;

			InvalidateWindowData(WC_SIGN_LIST, 0, 0);
		}
	}

	return CommandCost();
}

/**
 * Callback function that is called after a sign is placed
 * @param result of the operation
 * @param new_sign ID of the placed sign.
 */
void CcPlaceSign(Commands, const CommandCost &result, SignID new_sign)
{
	if (result.Failed()) return;

	ShowRenameSignWindow(Sign::Get(new_sign));
	ResetObjectToPlace();
}

/**
 *
 * PlaceProc function, called when someone pressed the button if the
 *  sign-tool is selected
 * @param tile on which to place the sign
 */
void PlaceProc_Sign(TileIndex tile)
{
	Command<CMD_PLACE_SIGN>::Post(STR_ERROR_CAN_T_PLACE_SIGN_HERE, CcPlaceSign, tile, {});
}
