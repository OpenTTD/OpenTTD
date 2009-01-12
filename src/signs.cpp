/* $Id$ */

/** @file signs.cpp Handling of signs. */

#include "stdafx.h"
#include "openttd.h"
#include "landscape.h"
#include "company_func.h"
#include "signs_base.h"
#include "signs_func.h"
#include "command_func.h"
#include "variables.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "tilehighlight_func.h"
#include "zoom_func.h"
#include "functions.h"
#include "window_func.h"
#include "map_func.h"
#include "string_func.h"
#include "oldpool_func.h"

#include "table/strings.h"

SignID _new_sign_id;

/* Initialize the sign-pool */
DEFINE_OLD_POOL_GENERIC(Sign, Sign)

Sign::Sign(Owner owner)
{
	this->owner = owner;
}

Sign::~Sign()
{
	free(this->name);

	if (CleaningPool()) return;

	DeleteRenameSignWindow(this->index);
	this->owner = INVALID_OWNER;
}

/**
 *
 * Update the coordinate of one sign
 * @param si Pointer to the Sign
 *
 */
static void UpdateSignVirtCoords(Sign *si)
{
	Point pt = RemapCoords(si->x, si->y, si->z);
	SetDParam(0, si->index);
	UpdateViewportSignPos(&si->sign, pt.x, pt.y - 6, STR_2806);
}

/** Update the coordinates of all signs */
void UpdateAllSignVirtCoords()
{
	Sign *si;

	FOR_ALL_SIGNS(si) UpdateSignVirtCoords(si);
}

/**
 * Marks the region of a sign as dirty.
 *
 * This function marks the sign in all viewports as dirty for repaint.
 *
 * @param si Pointer to the Sign
 * @ingroup dirty
 */
static void MarkSignDirty(Sign *si)
{
	/* We use ZOOM_LVL_MAX here, as every viewport can have an other zoom,
		*  and there is no way for us to know which is the biggest. So make the
		*  biggest area dirty, and we are safe for sure. */
	MarkAllViewportsDirty(
		si->sign.left - 6,
		si->sign.top  - 3,
		si->sign.left + ScaleByZoom(si->sign.width_1 + 12, ZOOM_LVL_MAX),
		si->sign.top  + ScaleByZoom(12, ZOOM_LVL_MAX));
}

/**
 * Place a sign at the given coordinates. Ownership of sign has
 * no effect whatsoever except for the colour the sign gets for easy recognition,
 * but everybody is able to rename/remove it.
 * @param tile tile to place sign at
 * @param flags type of operation
 * @param p1 unused
 * @param p2 unused
 */
CommandCost CmdPlaceSign(TileIndex tile, uint32 flags, uint32 p1, uint32 p2, const char *text)
{
	/* Try to locate a new sign */
	if (!Sign::CanAllocateItem()) return_cmd_error(STR_2808_TOO_MANY_SIGNS);

	/* Check sign text length if any */
	if (!StrEmpty(text) && strlen(text) >= MAX_LENGTH_SIGN_NAME_BYTES) return CMD_ERROR;

	/* When we execute, really make the sign */
	if (flags & DC_EXEC) {
		Sign *si = new Sign(_current_company);
		int x = TileX(tile) * TILE_SIZE;
		int y = TileY(tile) * TILE_SIZE;

		si->x = x;
		si->y = y;
		si->z = GetSlopeZ(x, y);
		if (!StrEmpty(text)) {
			si->name = strdup(text);
		}
		UpdateSignVirtCoords(si);
		MarkSignDirty(si);
		InvalidateWindowData(WC_SIGN_LIST, 0, 0);
		_new_sign_id = si->index;
	}

	return CommandCost();
}

/** Rename a sign. If the new name of the sign is empty, we assume
 * the user wanted to delete it. So delete it. Ownership of signs
 * has no meaning/effect whatsoever except for eyecandy
 * @param tile unused
 * @param flags type of operation
 * @param p1 index of the sign to be renamed/removed
 * @param p2 unused
 * @return 0 if succesfull, otherwise CMD_ERROR
 */
CommandCost CmdRenameSign(TileIndex tile, uint32 flags, uint32 p1, uint32 p2, const char *text)
{
	if (!IsValidSignID(p1)) return CMD_ERROR;

	/* Rename the signs when empty, otherwise remove it */
	if (!StrEmpty(text)) {
		if (strlen(text) >= MAX_LENGTH_SIGN_NAME_BYTES) return CMD_ERROR;

		if (flags & DC_EXEC) {
			Sign *si = GetSign(p1);

			/* Delete the old name */
			free(si->name);
			/* Assign the new one */
			si->name = strdup(text);
			si->owner = _current_company;

			/* Update; mark sign dirty twice, because it can either becom longer, or shorter */
			MarkSignDirty(si);
			UpdateSignVirtCoords(si);
			MarkSignDirty(si);
			InvalidateWindowData(WC_SIGN_LIST, 0, 1);
		}
	} else { // Delete sign
		if (flags & DC_EXEC) {
			Sign *si = GetSign(p1);

			MarkSignDirty(si);
			delete si;

			InvalidateWindowData(WC_SIGN_LIST, 0, 0);
		}
	}

	return CommandCost();
}

/**
 * Callback function that is called after a sign is placed
 * @param success of the operation
 * @param tile unused
 * @param p1 unused
 * @param p2 unused
 */
void CcPlaceSign(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		ShowRenameSignWindow(GetSign(_new_sign_id));
		ResetObjectToPlace();
	}
}

/**
 *
 * PlaceProc function, called when someone pressed the button if the
 *  sign-tool is selected
 * @param tile on which to place the sign
 */
void PlaceProc_Sign(TileIndex tile)
{
	DoCommandP(tile, 0, 0, CMD_PLACE_SIGN | CMD_MSG(STR_2809_CAN_T_PLACE_SIGN_HERE), CcPlaceSign);
}

/**
 *
 * Initialize the signs
 *
 */
void InitializeSigns()
{
	_Sign_pool.CleanPool();
	_Sign_pool.AddBlockToPool();
}
