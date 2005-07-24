/* $Id$ */

#ifndef SIGNS_H
#define SIGNS_H

#include "pool.h"

typedef struct SignStruct {
	StringID     str;
	ViewportSign sign;
	int32        x;
	int32        y;
	byte         z;
	byte owner; // placed by this player. Anyone can delete them though.
							// OWNER_NONE for gray signs from old games.

	uint16       index;
} SignStruct;

extern MemoryPool _sign_pool;

/**
 * Check if a Sign really exists.
 */
static inline bool IsValidSign(SignStruct* ss)
{
	return ss->str != 0;
}

/**
 * Get the pointer to the sign with index 'index'
 */
static inline SignStruct *GetSign(uint index)
{
	return (SignStruct*)GetItemFromPool(&_sign_pool, index);
}

/**
 * Get the current size of the SignPool
 */
static inline uint16 GetSignPoolSize(void)
{
	return _sign_pool.total_items;
}

static inline bool IsSignIndex(uint index)
{
	return index < GetSignPoolSize();
}

#define FOR_ALL_SIGNS_FROM(ss, start) for (ss = GetSign(start); ss != NULL; ss = (ss->index + 1 < GetSignPoolSize()) ? GetSign(ss->index + 1) : NULL)
#define FOR_ALL_SIGNS(ss) FOR_ALL_SIGNS_FROM(ss, 0)

VARDEF SignStruct *_new_sign_struct;

VARDEF bool _sign_sort_dirty;
VARDEF uint16 *_sign_sort;

void UpdateAllSignVirtCoords(void);
void PlaceProc_Sign(TileIndex tile);

/* misc.c */
void ShowRenameSignWindow(const SignStruct *ss);

#endif /* SIGNS_H */
