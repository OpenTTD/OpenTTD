/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_sl.cpp Code handling saving and loading of newgrf config */

#include "../stdafx.h"
#include "../newgrf_config.h"
#include "../core/bitmath_func.hpp"
#include "../core/alloc_func.hpp"
#include "../gfx_func.h"
#include "../fios.h"

#include "saveload.h"

static const SaveLoad _grfconfig_desc[] = {
	    SLE_STR(GRFConfig, filename,         SLE_STR,    0x40),
	    SLE_VAR(GRFConfig, ident.grfid,      SLE_UINT32),
	    SLE_ARR(GRFConfig, ident.md5sum,     SLE_UINT8,  16),
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


static void Load_NGRF_common(GRFConfig *&grfconfig)
{
	ClearGRFConfigList(&grfconfig);
	while (SlIterateArray() != -1) {
		GRFConfig *c = new GRFConfig();
		SlObject(c, _grfconfig_desc);
		if (CheckSavegameVersion(101)) c->windows_paletted = (_use_palette == PAL_WINDOWS);
		AppendToGRFConfigList(&grfconfig, c);
	}
}

static void Load_NGRF()
{
	Load_NGRF_common(_grfconfig);

	/* Append static NewGRF configuration */
	AppendStaticGRFConfigs(&_grfconfig);
}

static void Check_NGRF()
{
	Load_NGRF_common(_load_check_data.grfconfig);
}

extern const ChunkHandler _newgrf_chunk_handlers[] = {
	{ 'NGRF', Save_NGRF, Load_NGRF, NULL, Check_NGRF, CH_ARRAY | CH_LAST }
};
