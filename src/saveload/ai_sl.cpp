/* $Id$ */

/** @file ai_sl.cpp Code handling saving and loading of old AI + new AI initialisation after game load */

#include "../stdafx.h"
#include "../ai/ai.h"
#include "../ai/default/default.h"

#include "saveload.h"

static const SaveLoad _company_ai_desc[] = {
	    SLE_VAR(CompanyAI, state,             SLE_UINT8),
	    SLE_VAR(CompanyAI, tick,              SLE_UINT8),
	SLE_CONDVAR(CompanyAI, state_counter,     SLE_FILE_U16 | SLE_VAR_U32,  0, 12),
	SLE_CONDVAR(CompanyAI, state_counter,     SLE_UINT32,                 13, SL_MAX_VERSION),
	    SLE_VAR(CompanyAI, timeout_counter,   SLE_UINT16),

	    SLE_VAR(CompanyAI, state_mode,        SLE_UINT8),
	    SLE_VAR(CompanyAI, banned_tile_count, SLE_UINT8),
	    SLE_VAR(CompanyAI, railtype_to_use,   SLE_UINT8),

	    SLE_VAR(CompanyAI, cargo_type,        SLE_UINT8),
	    SLE_VAR(CompanyAI, num_wagons,        SLE_UINT8),
	    SLE_VAR(CompanyAI, build_kind,        SLE_UINT8),
	    SLE_VAR(CompanyAI, num_build_rec,     SLE_UINT8),
	    SLE_VAR(CompanyAI, num_loco_to_build, SLE_UINT8),
	    SLE_VAR(CompanyAI, num_want_fullload, SLE_UINT8),

	    SLE_VAR(CompanyAI, route_type_mask,   SLE_UINT8),

	SLE_CONDVAR(CompanyAI, start_tile_a,      SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(CompanyAI, start_tile_a,      SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(CompanyAI, cur_tile_a,        SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(CompanyAI, cur_tile_a,        SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLE_VAR(CompanyAI, start_dir_a,       SLE_UINT8),
	    SLE_VAR(CompanyAI, cur_dir_a,         SLE_UINT8),

	SLE_CONDVAR(CompanyAI, start_tile_b,      SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(CompanyAI, start_tile_b,      SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(CompanyAI, cur_tile_b,        SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(CompanyAI, cur_tile_b,        SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLE_VAR(CompanyAI, start_dir_b,       SLE_UINT8),
	    SLE_VAR(CompanyAI, cur_dir_b,         SLE_UINT8),

	    SLE_REF(CompanyAI, cur_veh,           REF_VEHICLE),

	    SLE_ARR(CompanyAI, wagon_list,        SLE_UINT16, 9),
	    SLE_ARR(CompanyAI, order_list_blocks, SLE_UINT8, 20),
	    SLE_ARR(CompanyAI, banned_tiles,      SLE_UINT16, 16),

	SLE_CONDNULL(64, 2, SL_MAX_VERSION),
	SLE_END()
};

static const SaveLoad _company_ai_build_rec_desc[] = {
	SLE_CONDVAR(AiBuildRec, spec_tile,         SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(AiBuildRec, spec_tile,         SLE_UINT32,                 6, SL_MAX_VERSION),
	SLE_CONDVAR(AiBuildRec, use_tile,          SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(AiBuildRec, use_tile,          SLE_UINT32,                 6, SL_MAX_VERSION),
	    SLE_VAR(AiBuildRec, rand_rng,          SLE_UINT8),
	    SLE_VAR(AiBuildRec, cur_building_rule, SLE_UINT8),
	    SLE_VAR(AiBuildRec, unk6,              SLE_UINT8),
	    SLE_VAR(AiBuildRec, unk7,              SLE_UINT8),
	    SLE_VAR(AiBuildRec, buildcmd_a,        SLE_UINT8),
	    SLE_VAR(AiBuildRec, buildcmd_b,        SLE_UINT8),
	    SLE_VAR(AiBuildRec, direction,         SLE_UINT8),
	    SLE_VAR(AiBuildRec, cargo,             SLE_UINT8),
	SLE_END()
};


void SaveLoad_AI(CompanyID company)
{
	CompanyAI *cai = &_companies_ai[company];
	SlObject(cai, _company_ai_desc);
	for (int i = 0; i != cai->num_build_rec; i++) {
		SlObject(&cai->src + i, _company_ai_build_rec_desc);
	}
}
