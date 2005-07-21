#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "player.h"
#include "signs.h"
#include "saveload.h"
#include "command.h"
#include "strings.h"
#include "variables.h"

enum {
	/* Max signs: 64000 (4 * 16000) */
	SIGN_POOL_BLOCK_SIZE_BITS = 2,       /* In bits, so (1 << 2) == 4 */
	SIGN_POOL_MAX_BLOCKS      = 16000,
};

/**
 * Called if a new block is added to the sign-pool
 */
static void SignPoolNewBlock(uint start_item)
{
	SignStruct *ss;

	FOR_ALL_SIGNS_FROM(ss, start_item)
		ss->index = start_item++;
}

/* Initialize the sign-pool */
MemoryPool _sign_pool = { "Signs", SIGN_POOL_MAX_BLOCKS, SIGN_POOL_BLOCK_SIZE_BITS, sizeof(SignStruct), &SignPoolNewBlock, 0, 0, NULL };

/**
 *
 * Update the coordinate of one sign
 *
 */
static void UpdateSignVirtCoords(SignStruct *ss)
{
	Point pt = RemapCoords(ss->x, ss->y, ss->z);
	SetDParam(0, ss->str);
	UpdateViewportSignPos(&ss->sign, pt.x, pt.y - 6, STR_2806);
}

/**
 *
 * Update the coordinates of all signs
 *
 */
void UpdateAllSignVirtCoords(void)
{
	SignStruct *ss;

	FOR_ALL_SIGNS(ss)
		if (ss->str != 0)
			UpdateSignVirtCoords(ss);

}

/**
 *
 * Marks the region of a sign as dirty
 *
 * @param ss Pointer to the SignStruct
 */
static void MarkSignDirty(SignStruct *ss)
{
	MarkAllViewportsDirty(
		ss->sign.left - 6,
		ss->sign.top  - 3,
		ss->sign.left + ss->sign.width_1 * 4 + 12,
		ss->sign.top  + 45);
}

/**
 *
 * Allocates a new sign
 *
 * @return The pointer to the new sign, or NULL if there is no more free space
 */
static SignStruct *AllocateSign(void)
{
	SignStruct *ss;
	FOR_ALL_SIGNS(ss) {
		if (ss->str == 0) {
			uint index = ss->index;

			memset(ss, 0, sizeof(SignStruct));
			ss->index = index;

			return ss;
		}
	}

	/* Check if we can add a block to the pool */
	if (AddBlockToPool(&_sign_pool))
		return AllocateSign();

	return NULL;
}

/** Place a sign at the given coordinates. Ownership of sign has
 * no effect whatsoever except for the colour the sign gets for easy recognition,
 * but everybody is able to rename/remove it.
 * @param x,y coordinates to place sign at
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdPlaceSign(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	SignStruct *ss;

	/* Try to locate a new sign */
	ss = AllocateSign();
	if (ss == NULL) return_cmd_error(STR_2808_TOO_MANY_SIGNS);

	/* When we execute, really make the sign */
	if (flags & DC_EXEC) {
		ss->str = STR_280A_SIGN;
		ss->x = x;
		ss->y = y;
		ss->owner = _current_player; // owner of the sign; just eyecandy
		ss->z = GetSlopeZ(x,y);
		UpdateSignVirtCoords(ss);
		MarkSignDirty(ss);
		InvalidateWindow(WC_SIGN_LIST, 0);
		_sign_sort_dirty = true;
		_new_sign_struct = ss;
	}

	return 0;
}

/** Rename a sign. If the new name of the sign is empty, we assume
 * the user wanted to delete it. So delete it. Ownership of signs
 * has no meaning/effect whatsoever except for eyecandy
 * @param x,y unused
 * @param p1 index of the sign to be renamed/removed
 * @param p2 unused
 */
int32 CmdRenameSign(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (!IsSignIndex(p1)) return CMD_ERROR;

	/* If _cmd_text 0 means the new text for the sign is non-empty.
	 * So rename the sign. If it is empty, it has no name, so delete it */
	if (_cmd_text[0] != '\0') {
		/* Create the name */
		StringID str = AllocateName(_cmd_text, 0);
		if (str == 0) return CMD_ERROR;

		if (flags & DC_EXEC) {
			SignStruct *ss = GetSign(p1);

			/* Delete the old name */
			DeleteName(ss->str);
			/* Assign the new one */
			ss->str = str;
			ss->owner = _current_player;

			/* Update; mark sign dirty twice, because it can either becom longer, or shorter */
			MarkSignDirty(ss);
			UpdateSignVirtCoords(ss);
			MarkSignDirty(ss);
			InvalidateWindow(WC_SIGN_LIST, 0);
			_sign_sort_dirty = true;
		} else {
			/* Free the name, because we did not assign it yet */
			DeleteName(str);
		}
	} else { /* Delete sign */
		if (flags & DC_EXEC) {
			SignStruct *ss = GetSign(p1);

			/* Delete the name */
			DeleteName(ss->str);
			ss->str = 0;

			MarkSignDirty(ss);
			InvalidateWindow(WC_SIGN_LIST, 0);
			_sign_sort_dirty = true;
		}
	}

	return 0;
}

/**
 *
 * Callback function that is called after a sign is placed
 *
 */
void CcPlaceSign(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		ShowRenameSignWindow(_new_sign_struct);
		ResetObjectToPlace();
	}
}

/**
 *
 * PlaceProc function, called when someone pressed the button if the
 *  sign-tool is selected
 *
 */
void PlaceProc_Sign(TileIndex tile)
{
	DoCommandP(tile, _current_player, 0, CcPlaceSign, CMD_PLACE_SIGN | CMD_MSG(STR_2809_CAN_T_PLACE_SIGN_HERE));
}

/**
 *
 * Initialize the signs
 *
 */
void InitializeSigns(void)
{
	CleanPool(&_sign_pool);
	AddBlockToPool(&_sign_pool);
}

static const SaveLoad _sign_desc[] = {
	SLE_VAR(SignStruct,str,						SLE_UINT16),
	SLE_CONDVAR(SignStruct,x,					SLE_FILE_I16 | SLE_VAR_I32, 0, 4),
	SLE_CONDVAR(SignStruct,y,					SLE_FILE_I16 | SLE_VAR_I32, 0, 4),
	SLE_CONDVAR(SignStruct,x,					SLE_INT32, 5, 255),
	SLE_CONDVAR(SignStruct,y,					SLE_INT32, 5, 255),
	SLE_CONDVAR(SignStruct,owner,			SLE_UINT8, 6, 255),
	SLE_VAR(SignStruct,z,							SLE_UINT8),
	SLE_END()
};

/**
 *
 * Save all signs
 *
 */
static void Save_SIGN(void)
{
	SignStruct *ss;

	FOR_ALL_SIGNS(ss) {
		/* Don't save empty signs */
		if (ss->str != 0) {
			SlSetArrayIndex(ss->index);
			SlObject(ss, _sign_desc);
		}
	}
}

/**
 *
 * Load all signs
 *
 */
static void Load_SIGN(void)
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		SignStruct *ss;

		if (!AddBlockIfNeeded(&_sign_pool, index))
			error("Signs: failed loading savegame: too many signs");

		ss = GetSign(index);
		SlObject(ss, _sign_desc);
	}

	_sign_sort_dirty = true;
}

const ChunkHandler _sign_chunk_handlers[] = {
	{ 'SIGN', Save_SIGN, Load_SIGN, CH_ARRAY | CH_LAST},
};
