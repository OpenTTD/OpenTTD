#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "signs.h"
#include "saveload.h"
#include "command.h"

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
	SignStruct *s;
	FOR_ALL_SIGNS(s)
		if (s->str == 0)
			return s;

	/* Check if we can add a block to the pool */
	if (AddBlockToPool(&_sign_pool))
		return AllocateSign();

	return NULL;
}

/**
 *
 * Place a sign at the given x/y
 *
 * @param p1 player number
 * @param p2 not used
 */
int32 CmdPlaceSign(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	SignStruct *ss;

	/* Try to locate a new sign */
	ss = AllocateSign();
	if (ss == NULL)
		return_cmd_error(STR_2808_TOO_MANY_SIGNS);

	/* When we execute, really make the sign */
	if (flags & DC_EXEC) {
		ss->str = STR_280A_SIGN;
		ss->x = x;
		ss->y = y;
		ss->owner = p1;
		ss->z = GetSlopeZ(x,y);
		UpdateSignVirtCoords(ss);
		MarkSignDirty(ss);
		_new_sign_struct = ss;
	}

	return 0;
}

/**
 * Rename a sign
 *
 * @param sign_id Index of the sign
 * @param new owner, if OWNER_NONE, sign will be removed
 */
int32 CmdRenameSign(int x, int y, uint32 flags, uint32 sign_id, uint32 owner)
{
	StringID str;
	SignStruct *ss;

	/* If GetDParam(0) == nothing, we delete the sign */
	if (GetDParam(0) != 0 && owner != OWNER_NONE) {
		/* Create the name */
		str = AllocateName((const char*)_decode_parameters, 0);
		if (str == 0)
			return CMD_ERROR;

		if (flags & DC_EXEC) {
			ss = GetSign(sign_id);

			MarkSignDirty(ss);

			/* Delete the old name */
			DeleteName(ss->str);
			/* Assign the new one */
			ss->str = str;
			ss->owner = owner;

			/* Update */
			UpdateSignVirtCoords(ss);
			MarkSignDirty(ss);
		} else {
			/* Free the name, because we did not assign it yet */
			DeleteName(str);
		}
	} else {
		/* Delete sign */
		if (flags & DC_EXEC) {
			ss = GetSign(sign_id);

			/* Delete the name */
			DeleteName(ss->str);
			ss->str = 0;

			MarkSignDirty(ss);
		}
	}

	return 0;
}

/**
 *
 * Callback function that is called after a sign is placed
 *
 */
void CcPlaceSign(bool success, uint tile, uint32 p1, uint32 p2)
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
void PlaceProc_Sign(uint tile)
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

static const byte _sign_desc[] = {
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
}

const ChunkHandler _sign_chunk_handlers[] = {
	{ 'SIGN', Save_SIGN, Load_SIGN, CH_ARRAY | CH_LAST},
};
