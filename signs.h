/* $Id$ */

#ifndef SIGNS_H
#define SIGNS_H

#include "pool.h"

typedef struct Sign {
	StringID     str;
	ViewportSign sign;
	int32        x;
	int32        y;
	byte         z;
	PlayerID     owner; // placed by this player. Anyone can delete them though. OWNER_NONE for gray signs from old games.

	SignID       index;
} Sign;

extern MemoryPool _sign_pool;

/**
 * Get the pointer to the sign with index 'index'
 */
static inline Sign *GetSign(SignID index)
{
	return (Sign *)GetItemFromPool(&_sign_pool, index);
}

/**
 * Get the current size of the SignPool
 */
static inline uint16 GetSignPoolSize(void)
{
	return _sign_pool.total_items;
}

static inline SignID GetSignArraySize(void)
{
	/* TODO - This isn't the real content of the function, but
	 *  with the new pool-system this will be replaced with one that
	 *  _really_ returns the highest index + 1. Now it just returns
	 *  the next safe value we are sure about everything is below.
	 */
	return GetSignPoolSize();
}

/**
 * Check if a Sign really exists.
 */
static inline bool IsValidSign(const Sign *si)
{
	return si->str != STR_NULL;
}

static inline bool IsValidSignID(uint index)
{
	return index < GetSignPoolSize() && IsValidSign(GetSign(index));
}

void DestroySign(Sign *si);

static inline void DeleteSign(Sign *si)
{
	DestroySign(si);
	si->str = STR_NULL;
}

#define FOR_ALL_SIGNS_FROM(ss, start) for (ss = GetSign(start); ss != NULL; ss = (ss->index + 1 < GetSignPoolSize()) ? GetSign(ss->index + 1) : NULL) if (IsValidSign(ss))
#define FOR_ALL_SIGNS(ss) FOR_ALL_SIGNS_FROM(ss, 0)

VARDEF bool _sign_sort_dirty;

void UpdateAllSignVirtCoords(void);
void PlaceProc_Sign(TileIndex tile);

/* misc.c */
void ShowRenameSignWindow(const Sign *si);

#endif /* SIGNS_H */
