/* $Id$ */

/** @file signs.h */

#ifndef SIGNS_H
#define SIGNS_H

#include "oldpool.h"

struct Sign;
DECLARE_OLD_POOL(Sign, Sign, 2, 16000)

struct Sign : PoolItem<Sign, SignID, &_Sign_pool> {
	char *name;
	ViewportSign sign;
	int32        x;
	int32        y;
	byte         z;
	PlayerByte   owner; // placed by this player. Anyone can delete them though. OWNER_NONE for gray signs from old games.

	/**
	 * Creates a new sign
	 */
	Sign(PlayerID owner = INVALID_PLAYER);

	/** Destroy the sign */
	~Sign();

	inline bool IsValid() const { return this->owner != INVALID_PLAYER; }
};

enum {
	INVALID_SIGN = 0xFFFF,
};

extern SignID _new_sign_id;


static inline SignID GetMaxSignIndex()
{
	/* TODO - This isn't the real content of the function, but
	 *  with the new pool-system this will be replaced with one that
	 *  _really_ returns the highest index. Now it just returns
	 *  the next safe value we are sure about everything is below.
	 */
	return GetSignPoolSize() - 1;
}

static inline uint GetNumSigns()
{
	extern uint _total_signs;
	return _total_signs;
}

static inline bool IsValidSignID(uint index)
{
	return index < GetSignPoolSize() && GetSign(index)->IsValid();
}

#define FOR_ALL_SIGNS_FROM(ss, start) for (ss = GetSign(start); ss != NULL; ss = (ss->index + 1U < GetSignPoolSize()) ? GetSign(ss->index + 1U) : NULL) if (ss->IsValid())
#define FOR_ALL_SIGNS(ss) FOR_ALL_SIGNS_FROM(ss, 0)

extern bool _sign_sort_dirty;

void UpdateAllSignVirtCoords();
void PlaceProc_Sign(TileIndex tile);

/* signs_gui.cpp */
void ShowRenameSignWindow(const Sign *si);

void ShowSignList();

#endif /* SIGNS_H */
