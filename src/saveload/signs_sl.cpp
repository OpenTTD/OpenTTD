/* $Id$ */

/** @file signs_sl.cpp Code handling saving and loading of economy data */

#include "../stdafx.h"
#include "../strings_func.h"
#include "../company_func.h"
#include "../signs_base.h"
#include "../signs_func.h"

#include "saveload_internal.h"
#include "saveload.h"

static const SaveLoad _sign_desc[] = {
  SLE_CONDVAR(Sign, name,  SLE_NAME,                   0, 83),
  SLE_CONDSTR(Sign, name,  SLE_STR, 0,                84, SL_MAX_VERSION),
  SLE_CONDVAR(Sign, x,     SLE_FILE_I16 | SLE_VAR_I32, 0, 4),
  SLE_CONDVAR(Sign, y,     SLE_FILE_I16 | SLE_VAR_I32, 0, 4),
  SLE_CONDVAR(Sign, x,     SLE_INT32,                  5, SL_MAX_VERSION),
  SLE_CONDVAR(Sign, y,     SLE_INT32,                  5, SL_MAX_VERSION),
  SLE_CONDVAR(Sign, owner, SLE_UINT8,                  6, SL_MAX_VERSION),
      SLE_VAR(Sign, z,     SLE_UINT8),
	SLE_END()
};

/** Save all signs */
static void Save_SIGN()
{
	Sign *si;

	FOR_ALL_SIGNS(si) {
		SlSetArrayIndex(si->index);
		SlObject(si, _sign_desc);
	}
}

/** Load all signs */
static void Load_SIGN()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		Sign *si = new (index) Sign();
		SlObject(si, _sign_desc);
	}
}

extern const ChunkHandler _sign_chunk_handlers[] = {
	{ 'SIGN', Save_SIGN, Load_SIGN, CH_ARRAY | CH_LAST},
};
