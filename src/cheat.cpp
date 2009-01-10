/* $Id$ */

/** @file cheat.cpp Handling (loading/saving/initializing) of cheats. */

#include "stdafx.h"
#include "cheat_type.h"

Cheats _cheats;

void InitializeCheats()
{
	memset(&_cheats, 0, sizeof(Cheats));
}

bool CheatHasBeenUsed()
{
	/* Cannot use lengthof because _cheats is of type Cheats, not Cheat */
	const Cheat *cht = (Cheat*)&_cheats;
	const Cheat *cht_last = &cht[sizeof(_cheats) / sizeof(Cheat)];

	for (; cht != cht_last; cht++) {
		if (cht->been_used) return true;
	}

	return false;
}
