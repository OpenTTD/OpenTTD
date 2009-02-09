/* $Id$ */

/** @file industry_sl.cpp Code handling saving and loading of industries */

#include "../stdafx.h"
#include "../tile_type.h"
#include "../strings_type.h"
#include "../company_type.h"
#include "../industry.h"
#include "../newgrf_commons.h"

#include "saveload.h"

static const SaveLoad _industry_desc[] = {
	SLE_CONDVAR(Industry, xy,                         SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Industry, xy,                         SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLE_VAR(Industry, width,                      SLE_UINT8),
	    SLE_VAR(Industry, height,                     SLE_UINT8),
	    SLE_REF(Industry, town,                       REF_TOWN),
	SLE_CONDNULL( 2, 0, 60),       ///< used to be industry's produced_cargo
	SLE_CONDARR(Industry, produced_cargo,             SLE_UINT8,  2,              78, SL_MAX_VERSION),
	SLE_CONDARR(Industry, incoming_cargo_waiting,     SLE_UINT16, 3,              70, SL_MAX_VERSION),
	    SLE_ARR(Industry, produced_cargo_waiting,     SLE_UINT16, 2),
	    SLE_ARR(Industry, production_rate,            SLE_UINT8,  2),
	SLE_CONDNULL( 3, 0, 60),       ///< used to be industry's accepts_cargo
	SLE_CONDARR(Industry, accepts_cargo,              SLE_UINT8,  3,              78, SL_MAX_VERSION),
	    SLE_VAR(Industry, prod_level,                 SLE_UINT8),
	    SLE_ARR(Industry, this_month_production,      SLE_UINT16, 2),
	    SLE_ARR(Industry, this_month_transported,     SLE_UINT16, 2),
	    SLE_ARR(Industry, last_month_pct_transported, SLE_UINT8,  2),
	    SLE_ARR(Industry, last_month_production,      SLE_UINT16, 2),
	    SLE_ARR(Industry, last_month_transported,     SLE_UINT16, 2),

	    SLE_VAR(Industry, counter,                    SLE_UINT16),

	    SLE_VAR(Industry, type,                       SLE_UINT8),
	    SLE_VAR(Industry, owner,                      SLE_UINT8),
	    SLE_VAR(Industry, random_colour,              SLE_UINT8),
	SLE_CONDVAR(Industry, last_prod_year,             SLE_FILE_U8 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Industry, last_prod_year,             SLE_INT32,                 31, SL_MAX_VERSION),
	    SLE_VAR(Industry, was_cargo_delivered,        SLE_UINT8),

	SLE_CONDVAR(Industry, founder,                    SLE_UINT8,                 70, SL_MAX_VERSION),
	SLE_CONDVAR(Industry, construction_date,          SLE_INT32,                 70, SL_MAX_VERSION),
	SLE_CONDVAR(Industry, construction_type,          SLE_UINT8,                 70, SL_MAX_VERSION),
	SLE_CONDVAR(Industry, last_cargo_accepted_at,     SLE_INT32,                 70, SL_MAX_VERSION),
	SLE_CONDVAR(Industry, selected_layout,            SLE_UINT8,                 73, SL_MAX_VERSION),

	SLE_CONDARRX(cpp_offsetof(Industry, psa) + cpp_offsetof(Industry::PersistentStorage, storage), SLE_UINT32, 16, 76, SL_MAX_VERSION),

	SLE_CONDVAR(Industry, random_triggers,            SLE_UINT8,                 82, SL_MAX_VERSION),
	SLE_CONDVAR(Industry, random,                     SLE_UINT16,                82, SL_MAX_VERSION),

	/* reserve extra space in savegame here. (currently 32 bytes) */
	SLE_CONDNULL(32, 2, SL_MAX_VERSION),

	SLE_END()
};

static void Save_INDY()
{
	Industry *ind;

	/* Write the industries */
	FOR_ALL_INDUSTRIES(ind) {
		SlSetArrayIndex(ind->index);
		SlObject(ind, _industry_desc);
	}
}

/* Save and load the mapping between the industry/tile id on the map, and the grf file
 * it came from. */
static const SaveLoad _industries_id_mapping_desc[] = {
	SLE_VAR(EntityIDMapping, grfid,         SLE_UINT32),
	SLE_VAR(EntityIDMapping, entity_id,     SLE_UINT8),
	SLE_VAR(EntityIDMapping, substitute_id, SLE_UINT8),
	SLE_END()
};

static void Save_IIDS()
{
	uint i;
	uint j = _industry_mngr.GetMaxMapping();

	for (i = 0; i < j; i++) {
		SlSetArrayIndex(i);
		SlObject(&_industry_mngr.mapping_ID[i], _industries_id_mapping_desc);
	}
}

static void Save_TIDS()
{
	uint i;
	uint j = _industile_mngr.GetMaxMapping();

	for (i = 0; i < j; i++) {
		SlSetArrayIndex(i);
		SlObject(&_industile_mngr.mapping_ID[i], _industries_id_mapping_desc);
	}
}

static void Load_INDY()
{
	int index;

	ResetIndustryCounts();

	while ((index = SlIterateArray()) != -1) {
		Industry *i = new (index) Industry();
		SlObject(i, _industry_desc);
		IncIndustryTypeCount(i->type);
	}
}

static void Load_IIDS()
{
	int index;
	uint max_id;

	/* clear the current mapping stored.
	 * This will create the manager if ever it is not yet done */
	_industry_mngr.ResetMapping();

	/* get boundary for the temporary map loader NUM_INDUSTRYTYPES? */
	max_id = _industry_mngr.GetMaxMapping();

	while ((index = SlIterateArray()) != -1) {
		if ((uint)index >= max_id) break;
		SlObject(&_industry_mngr.mapping_ID[index], _industries_id_mapping_desc);
	}
}

static void Load_TIDS()
{
	int index;
	uint max_id;

	/* clear the current mapping stored.
	 * This will create the manager if ever it is not yet done */
	_industile_mngr.ResetMapping();

	/* get boundary for the temporary map loader NUM_INDUSTILES? */
	max_id = _industile_mngr.GetMaxMapping();

	while ((index = SlIterateArray()) != -1) {
		if ((uint)index >= max_id) break;
		SlObject(&_industile_mngr.mapping_ID[index], _industries_id_mapping_desc);
	}
}

extern const ChunkHandler _industry_chunk_handlers[] = {
	{ 'INDY', Save_INDY, Load_INDY, CH_ARRAY},
	{ 'IIDS', Save_IIDS, Load_IIDS, CH_ARRAY},
	{ 'TIDS', Save_TIDS, Load_TIDS, CH_ARRAY | CH_LAST},
};
