/* $Id$ */

/** @file subsidy_sl.cpp Code handling saving and loading of subsidies */

#include "../stdafx.h"
#include "../subsidy_base.h"

#include "saveload.h"

static const SaveLoad _subsidies_desc[] = {
	    SLE_VAR(Subsidy, cargo_type, SLE_UINT8),
	    SLE_VAR(Subsidy, remaining,  SLE_UINT8),
	SLE_CONDVAR(Subsidy, awarded,    SLE_UINT8,                 125, SL_MAX_VERSION),
	SLE_CONDVAR(Subsidy, src_type,   SLE_UINT8,                 125, SL_MAX_VERSION),
	SLE_CONDVAR(Subsidy, dst_type,   SLE_UINT8,                 125, SL_MAX_VERSION),
	SLE_CONDVAR(Subsidy, src,        SLE_FILE_U8 | SLE_VAR_U16,   0, 4),
	SLE_CONDVAR(Subsidy, src,        SLE_UINT16,                  5, SL_MAX_VERSION),
	SLE_CONDVAR(Subsidy, dst,        SLE_FILE_U8 | SLE_VAR_U16,   0, 4),
	SLE_CONDVAR(Subsidy, dst,        SLE_UINT16,                  5, SL_MAX_VERSION),
	SLE_END()
};

void Save_SUBS()
{
	Subsidy *s;
	FOR_ALL_SUBSIDIES(s) {
		SlSetArrayIndex(s->Index());
		SlObject(s, _subsidies_desc);
	}
}

void Load_SUBS()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		SlObject(&Subsidy::array[index], _subsidies_desc);
	}
}

extern const ChunkHandler _subsidy_chunk_handlers[] = {
	{ 'SUBS', Save_SUBS, Load_SUBS, NULL, CH_ARRAY | CH_LAST},
};
