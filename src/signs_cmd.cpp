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

#include "table/strings.h"

#include "safeguards.h"

/** The last built sign. */
SignID _new_sign_id;

/**
 * Place a sign at the given coordinates. Ownership of sign has
 * no effect whatsoever except for the colour the sign gets for easy recognition,
 * but everybody is able to rename/remove it.
 * @param tile tile to place sign at
 * @param flags type of operation
 * @param p1 unused
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdPlaceSign(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	/* Try to locate a new sign */
	if (!Sign::CanAllocateItem()) return_cmd_error(STR_ERROR_TOO_MANY_SIGNS);

	/* Check sign text length if any */
	if (!StrEmpty(text) && Utf8StringLength(text) >= MAX_LENGTH_SIGN_NAME_CHARS) return CMD_ERROR;

	/* When we execute, really make the sign */
	if (flags & DC_EXEC) {
		Sign *si = new Sign(_game_mode == GM_EDITOR ? OWNER_DEITY : _current_company);
		int x = TileX(tile) * TILE_SIZE;
		int y = TileY(tile) * TILE_SIZE;

		si->x = x;
		si->y = y;
		si->z = GetSlopePixelZ(x, y);
		if (!StrEmpty(text)) {
			si->name = stredup(text);
		}
		si->UpdateVirtCoord();
		_viewport_sign_kdtree.Insert(ViewportSignKdtreeItem::MakeSign(si->index));
		InvalidateWindowData(WC_SIGN_LIST, 0, 0);
		_new_sign_id = si->index;
	}

	return CommandCost();
}

/**
 * Rename a sign. If the new name of the sign is empty, we assume
 * the user wanted to delete it. So delete it. Ownership of signs
 * has no meaning/effect whatsoever except for eyecandy
 * @param tile unused
 * @param flags type of operation
 * @param p1 index of the sign to be renamed/removed
 * @param p2 unused
 * @param text the new name or an empty string when resetting to the default
 * @return the cost of this operation or an error
 */
CommandCost CmdRenameSign(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Sign *si = Sign::GetIfValid(p1);
	if (si == nullptr) return CMD_ERROR;
	if (si->owner == OWNER_DEITY && _current_company != OWNER_DEITY && _game_mode != GM_EDITOR) return CMD_ERROR;

	/* Rename the signs when empty, otherwise remove it */
	if (!StrEmpty(text)) {
		if (Utf8StringLength(text) >= MAX_LENGTH_SIGN_NAME_CHARS) return CMD_ERROR;

		if (flags & DC_EXEC) {
			/* Delete the old name */
			free(si->name);
			/* Assign the new one */
			si->name = stredup(text);
			if (_game_mode != GM_EDITOR) si->owner = _current_company;

			si->UpdateVirtCoord();
			InvalidateWindowData(WC_SIGN_LIST, 0, 1);
		}
	} else { // Delete sign
		if (flags & DC_EXEC) {
			si->sign.MarkDirty();
			_viewport_sign_kdtree.Remove(ViewportSignKdtreeItem::MakeSign(si->index));
			delete si;

			InvalidateWindowData(WC_SIGN_LIST, 0, 0);
		}
	}

	return CommandCost();
}

/**
 * Callback function that is called after a sign is placed
 * @param result of the operation
 * @param tile unused
 * @param p1 unused
 * @param p2 unused
 * @param cmd unused
 */
void CcPlaceSign(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2, uint32 cmd)
{
	if (result.Failed()) return;

	ShowRenameSignWindow(Sign::Get(_new_sign_id));
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
	DoCommandP(tile, 0, 0, CMD_PLACE_SIGN | CMD_MSG(STR_ERROR_CAN_T_PLACE_SIGN_HERE), CcPlaceSign);
}
