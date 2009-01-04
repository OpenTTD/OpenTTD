/* $Id$ */

/** @file cheat_sl.cpp Code handling saving and loading of cheats */

#include "../stdafx.h"
#include "../cheat_type.h"

#include "saveload.h"

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
	size_t count = SlGetFieldLength() / 2;

	for (uint i = 0; i < count; i++) {
		cht[i].been_used = (SlReadByte() != 0);
		cht[i].value     = (SlReadByte() != 0);
	}
}

extern const ChunkHandler _cheat_chunk_handlers[] = {
	{ 'CHTS', Save_CHTS,     Load_CHTS,     CH_RIFF | CH_LAST}
};
