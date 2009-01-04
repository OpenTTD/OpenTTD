/* $Id$ */

/** @file subsidy_sl.cpp Code handling saving and loading of subsidies */

#include "../stdafx.h"
#include "../economy_func.h"

#include "saveload.h"

static const SaveLoad _subsidies_desc[] = {
	    SLE_VAR(Subsidy, cargo_type, SLE_UINT8),
	    SLE_VAR(Subsidy, age,        SLE_UINT8),
	SLE_CONDVAR(Subsidy, from,       SLE_FILE_U8 | SLE_VAR_U16, 0, 4),
	SLE_CONDVAR(Subsidy, from,       SLE_UINT16,                5, SL_MAX_VERSION),
	SLE_CONDVAR(Subsidy, to,         SLE_FILE_U8 | SLE_VAR_U16, 0, 4),
	SLE_CONDVAR(Subsidy, to,         SLE_UINT16,                5, SL_MAX_VERSION),
	SLE_END()
};

void Save_SUBS()
{
	int i;
	Subsidy *s;

	for (i = 0; i != lengthof(_subsidies); i++) {
		s = &_subsidies[i];
		if (s->cargo_type != CT_INVALID) {
			SlSetArrayIndex(i);
			SlObject(s, _subsidies_desc);
		}
	}
}

void Load_SUBS()
{
	int index;
	while ((index = SlIterateArray()) != -1)
		SlObject(&_subsidies[index], _subsidies_desc);
}

extern const ChunkHandler _subsidy_chunk_handlers[] = {
	{ 'SUBS', Save_SUBS,     Load_SUBS,     CH_ARRAY | CH_LAST},
};
