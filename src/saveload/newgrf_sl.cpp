/* $Id$ */

/** @file newgrf_sl.cpp Code handling saving and loading of newgrf config */

#include "../stdafx.h"
#include "../newgrf_config.h"
#include "../core/bitmath_func.hpp"
#include "../core/alloc_func.hpp"
#include "../gfx_func.h"

#include "saveload.h"

static const SaveLoad _grfconfig_desc[] = {
	    SLE_STR(GRFConfig, filename,         SLE_STR,    0x40),
	    SLE_VAR(GRFConfig, grfid,            SLE_UINT32),
	    SLE_ARR(GRFConfig, md5sum,           SLE_UINT8,  16),
	    SLE_ARR(GRFConfig, param,            SLE_UINT32, 0x80),
	    SLE_VAR(GRFConfig, num_params,       SLE_UINT8),
	SLE_CONDVAR(GRFConfig, windows_paletted, SLE_BOOL,   101, SL_MAX_VERSION),
	SLE_END()
};


static void Save_NGRF()
{
	int index = 0;

	for (GRFConfig *c = _grfconfig; c != NULL; c = c->next) {
		if (HasBit(c->flags, GCF_STATIC)) continue;
		SlSetArrayIndex(index++);
		SlObject(c, _grfconfig_desc);
	}
}


static void Load_NGRF()
{
	ClearGRFConfigList(&_grfconfig);
	while (SlIterateArray() != -1) {
		GRFConfig *c = CallocT<GRFConfig>(1);
		SlObject(c, _grfconfig_desc);
		if (CheckSavegameVersion(101)) c->windows_paletted = (_use_palette == PAL_WINDOWS);
		AppendToGRFConfigList(&_grfconfig, c);
	}

	/* Append static NewGRF configuration */
	AppendStaticGRFConfigs(&_grfconfig);
}

extern const ChunkHandler _newgrf_chunk_handlers[] = {
	{ 'NGRF', Save_NGRF, Load_NGRF, CH_ARRAY | CH_LAST }
};
