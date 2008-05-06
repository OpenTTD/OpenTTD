/* $Id$ */

/** @file cheat.cpp Handling (loading/saving/initializing) of cheats. */

#include "stdafx.h"
#include "saveload.h"
#include "cheat_type.h"

Cheats _cheats;

void InitializeCheats()
{
	memset(&_cheats, 0, sizeof(Cheats));
}

static void Save_CHTS()
{
	/* Cannot use lengthof because _cheats is of type Cheats, not Cheat */
	byte count = sizeof(_cheats) / sizeof(Cheat);
	Cheat *cht = (Cheat*) &_cheats;
	Cheat *cht_last = &cht[count];

	SlSetLength(count * 2);
	for (; cht != cht_last; cht++) {
		SlWriteByte(cht->been_used);
		SlWriteByte(cht->value);
	}
}

static void Load_CHTS()
{
	Cheat *cht = (Cheat*)&_cheats;
	uint count = SlGetFieldLength() / 2;

	for (uint i = 0; i < count; i++) {
		cht[i].been_used = (SlReadByte() != 0);
		cht[i].value     = (SlReadByte() != 0);
	}
}

bool CheatHasBeenUsed()
{
	/* Cannot use lengthof because _cheats is of type Cheats, not Cheat */
	const Cheat* cht = (Cheat*)&_cheats;
	const Cheat* cht_last = &cht[sizeof(_cheats) / sizeof(Cheat)];

	for (; cht != cht_last; cht++) {
		if (cht->been_used) return true;
	}

	return false;
}


extern const ChunkHandler _cheat_chunk_handlers[] = {
	{ 'CHTS', Save_CHTS,     Load_CHTS,     CH_RIFF | CH_LAST}
};
