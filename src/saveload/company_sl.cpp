/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file company_sl.cpp Code handling saving and loading of company data */

#include "../stdafx.h"
#include "../company_func.h"
#include "../company_manager_face.h"
#include "../fios.h"
#include "../tunnelbridge_map.h"
#include "../tunnelbridge.h"
#include "../station_base.h"
#include "../strings_func.h"

#include "saveload.h"

#include "table/strings.h"

#include "../safeguards.h"

/**
 * Converts an old company manager's face format to the new company manager's face format
 *
 * Meaning of the bits in the old face (some bits are used in several times):
 * - 4 and 5: chin
 * - 6 to 9: eyebrows
 * - 10 to 13: nose
 * - 13 to 15: lips (also moustache for males)
 * - 16 to 19: hair
 * - 20 to 22: eye colour
 * - 20 to 27: tie, ear rings etc.
 * - 28 to 30: glasses
 * - 19, 26 and 27: race (bit 27 set and bit 19 equal to bit 26 = black, otherwise white)
 * - 31: gender (0 = male, 1 = female)
 *
 * @param face the face in the old format
 * @return the face in the new format
 */
CompanyManagerFace ConvertFromOldCompanyManagerFace(uint32 face)
{
	CompanyManagerFace cmf = 0;
	GenderEthnicity ge = GE_WM;

	if (HasBit(face, 31)) SetBit(ge, GENDER_FEMALE);
	if (HasBit(face, 27) && (HasBit(face, 26) == HasBit(face, 19))) SetBit(ge, ETHNICITY_BLACK);

	SetCompanyManagerFaceBits(cmf, CMFV_GEN_ETHN,    ge, ge);
	SetCompanyManagerFaceBits(cmf, CMFV_HAS_GLASSES, ge, GB(face, 28, 3) <= 1);
	SetCompanyManagerFaceBits(cmf, CMFV_EYE_COLOUR,  ge, HasBit(ge, ETHNICITY_BLACK) ? 0 : ClampU(GB(face, 20, 3), 5, 7) - 5);
	SetCompanyManagerFaceBits(cmf, CMFV_CHIN,        ge, ScaleCompanyManagerFaceValue(CMFV_CHIN,     ge, GB(face,  4, 2)));
	SetCompanyManagerFaceBits(cmf, CMFV_EYEBROWS,    ge, ScaleCompanyManagerFaceValue(CMFV_EYEBROWS, ge, GB(face,  6, 4)));
	SetCompanyManagerFaceBits(cmf, CMFV_HAIR,        ge, ScaleCompanyManagerFaceValue(CMFV_HAIR,     ge, GB(face, 16, 4)));
	SetCompanyManagerFaceBits(cmf, CMFV_JACKET,      ge, ScaleCompanyManagerFaceValue(CMFV_JACKET,   ge, GB(face, 20, 2)));
	SetCompanyManagerFaceBits(cmf, CMFV_COLLAR,      ge, ScaleCompanyManagerFaceValue(CMFV_COLLAR,   ge, GB(face, 22, 2)));
	SetCompanyManagerFaceBits(cmf, CMFV_GLASSES,     ge, GB(face, 28, 1));

	uint lips = GB(face, 10, 4);
	if (!HasBit(ge, GENDER_FEMALE) && lips < 4) {
		SetCompanyManagerFaceBits(cmf, CMFV_HAS_MOUSTACHE, ge, true);
		SetCompanyManagerFaceBits(cmf, CMFV_MOUSTACHE,     ge, max(lips, 1U) - 1);
	} else {
		if (!HasBit(ge, GENDER_FEMALE)) {
			lips = lips * 15 / 16;
			lips -= 3;
			if (HasBit(ge, ETHNICITY_BLACK) && lips > 8) lips = 0;
		} else {
			lips = ScaleCompanyManagerFaceValue(CMFV_LIPS, ge, lips);
		}
		SetCompanyManagerFaceBits(cmf, CMFV_LIPS, ge, lips);

		uint nose = GB(face, 13, 3);
		if (ge == GE_WF) {
			nose = (nose * 3 >> 3) * 3 >> 2; // There is 'hole' in the nose sprites for females
		} else {
			nose = ScaleCompanyManagerFaceValue(CMFV_NOSE, ge, nose);
		}
		SetCompanyManagerFaceBits(cmf, CMFV_NOSE, ge, nose);
	}

	uint tie_earring = GB(face, 24, 4);
	if (!HasBit(ge, GENDER_FEMALE) || tie_earring < 3) { // Not all females have an earring
		if (HasBit(ge, GENDER_FEMALE)) SetCompanyManagerFaceBits(cmf, CMFV_HAS_TIE_EARRING, ge, true);
		SetCompanyManagerFaceBits(cmf, CMFV_TIE_EARRING, ge, HasBit(ge, GENDER_FEMALE) ? tie_earring : ScaleCompanyManagerFaceValue(CMFV_TIE_EARRING, ge, tie_earring / 2));
	}

	return cmf;
}

/** Rebuilding of company statistics after loading a savegame. */
void AfterLoadCompanyStats()
{
	/* Reset infrastructure statistics to zero. */
	Company *c;
	FOR_ALL_COMPANIES(c) MemSetT(&c->infrastructure, 0);

	/* Collect airport count. */
	Station *st;
	FOR_ALL_STATIONS(st) {
		if ((st->facilities & FACIL_AIRPORT) && Company::IsValidID(st->owner)) {
			Company::Get(st->owner)->infrastructure.airport++;
		}
	}

	for (TileIndex tile = 0; tile < MapSize(); tile++) {
		switch (GetTileType(tile)) {
			case MP_RAILWAY:
				c = Company::GetIfValid(GetTileOwner(tile));
				if (c != NULL) {
					uint pieces = 1;
					if (IsPlainRail(tile)) {
						TrackBits bits = GetTrackBits(tile);
						pieces = CountBits(bits);
						if (TracksOverlap(bits)) pieces *= pieces;
					}
					c->infrastructure.rail[GetRailType(tile)] += pieces;

					if (HasSignals(tile)) c->infrastructure.signal += CountBits(GetPresentSignals(tile));
				}
				break;

			case MP_ROAD: {
				if (IsLevelCrossing(tile)) {
					c = Company::GetIfValid(GetTileOwner(tile));
					if (c != NULL) c->infrastructure.rail[GetRailType(tile)] += LEVELCROSSING_TRACKBIT_FACTOR;
				}

				/* Iterate all present road types as each can have a different owner. */
				RoadType rt;
				FOR_EACH_SET_ROADTYPE(rt, GetRoadTypes(tile)) {
					c = Company::GetIfValid(IsRoadDepot(tile) ? GetTileOwner(tile) : GetRoadOwner(tile, rt));
					/* A level crossings and depots have two road bits. */
					if (c != NULL) c->infrastructure.road[rt] += IsNormalRoad(tile) ? CountBits(GetRoadBits(tile, rt)) : 2;
				}
				break;
			}

			case MP_STATION:
				c = Company::GetIfValid(GetTileOwner(tile));
				if (c != NULL && GetStationType(tile) != STATION_AIRPORT && !IsBuoy(tile)) c->infrastructure.station++;

				switch (GetStationType(tile)) {
					case STATION_RAIL:
					case STATION_WAYPOINT:
						if (c != NULL && !IsStationTileBlocked(tile)) c->infrastructure.rail[GetRailType(tile)]++;
						break;

					case STATION_BUS:
					case STATION_TRUCK: {
						/* Iterate all present road types as each can have a different owner. */
						RoadType rt;
						FOR_EACH_SET_ROADTYPE(rt, GetRoadTypes(tile)) {
							c = Company::GetIfValid(GetRoadOwner(tile, rt));
							if (c != NULL) c->infrastructure.road[rt] += 2; // A road stop has two road bits.
						}
						break;
					}

					case STATION_DOCK:
					case STATION_BUOY:
						if (GetWaterClass(tile) == WATER_CLASS_CANAL) {
							if (c != NULL) c->infrastructure.water++;
						}
						break;

					default:
						break;
				}
				break;

			case MP_WATER:
				if (IsShipDepot(tile) || IsLock(tile)) {
					c = Company::GetIfValid(GetTileOwner(tile));
					if (c != NULL) {
						if (IsShipDepot(tile)) c->infrastructure.water += LOCK_DEPOT_TILE_FACTOR;
						if (IsLock(tile) && GetLockPart(tile) == LOCK_PART_MIDDLE) {
							/* The middle tile specifies the owner of the lock. */
							c->infrastructure.water += 3 * LOCK_DEPOT_TILE_FACTOR; // the middle tile specifies the owner of the
							break; // do not count the middle tile as canal
						}
					}
				}
				FALLTHROUGH;

			case MP_OBJECT:
				if (GetWaterClass(tile) == WATER_CLASS_CANAL) {
					c = Company::GetIfValid(GetTileOwner(tile));
					if (c != NULL) c->infrastructure.water++;
				}
				break;

			case MP_TUNNELBRIDGE: {
				/* Only count the tunnel/bridge if we're on the northern end tile. */
				TileIndex other_end = GetOtherTunnelBridgeEnd(tile);
				if (tile < other_end) {
					/* Count each tunnel/bridge TUNNELBRIDGE_TRACKBIT_FACTOR times to simulate
					 * the higher structural maintenance needs, and don't forget the end tiles. */
					uint len = (GetTunnelBridgeLength(tile, other_end) + 2) * TUNNELBRIDGE_TRACKBIT_FACTOR;

					switch (GetTunnelBridgeTransportType(tile)) {
						case TRANSPORT_RAIL:
							c = Company::GetIfValid(GetTileOwner(tile));
							if (c != NULL) c->infrastructure.rail[GetRailType(tile)] += len;
							break;

						case TRANSPORT_ROAD: {
							/* Iterate all present road types as each can have a different owner. */
							RoadType rt;
							FOR_EACH_SET_ROADTYPE(rt, GetRoadTypes(tile)) {
								c = Company::GetIfValid(GetRoadOwner(tile, rt));
								if (c != NULL) c->infrastructure.road[rt] += len * 2; // A full diagonal road has two road bits.
							}
							break;
						}

						case TRANSPORT_WATER:
							c = Company::GetIfValid(GetTileOwner(tile));
							if (c != NULL) c->infrastructure.water += len;
							break;

						default:
							break;
					}
				}
				break;
			}

			default:
				break;
		}
	}
}



/* Save/load of companies */
static const SaveLoad _company_desc[] = {
	    SLE_VAR(CompanyProperties, name_2,          SLE_UINT32),
	    SLE_VAR(CompanyProperties, name_1,          SLE_STRINGID),
	SLE_CONDSTR(CompanyProperties, name,            SLE_STR | SLF_ALLOW_CONTROL, 0, 84, SL_MAX_VERSION),

	    SLE_VAR(CompanyProperties, president_name_1, SLE_STRINGID),
	    SLE_VAR(CompanyProperties, president_name_2, SLE_UINT32),
	SLE_CONDSTR(CompanyProperties, president_name,  SLE_STR | SLF_ALLOW_CONTROL, 0, 84, SL_MAX_VERSION),

	    SLE_VAR(CompanyProperties, face,            SLE_UINT32),

	/* money was changed to a 64 bit field in savegame version 1. */
	SLE_CONDVAR(CompanyProperties, money,                 SLE_VAR_I64 | SLE_FILE_I32,  0, 0),
	SLE_CONDVAR(CompanyProperties, money,                 SLE_INT64,                   1, SL_MAX_VERSION),

	SLE_CONDVAR(CompanyProperties, current_loan,          SLE_VAR_I64 | SLE_FILE_I32,  0, 64),
	SLE_CONDVAR(CompanyProperties, current_loan,          SLE_INT64,                  65, SL_MAX_VERSION),

	    SLE_VAR(CompanyProperties, colour,                SLE_UINT8),
	    SLE_VAR(CompanyProperties, money_fraction,        SLE_UINT8),
	SLE_CONDNULL(1,  0,  57), ///< avail_railtypes
	    SLE_VAR(CompanyProperties, block_preview,         SLE_UINT8),

	SLE_CONDNULL(2,  0,  93), ///< cargo_types
	SLE_CONDNULL(4, 94, 169), ///< cargo_types
	SLE_CONDVAR(CompanyProperties, location_of_HQ,        SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(CompanyProperties, location_of_HQ,        SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(CompanyProperties, last_build_coordinate, SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(CompanyProperties, last_build_coordinate, SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(CompanyProperties, inaugurated_year,      SLE_FILE_U8  | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(CompanyProperties, inaugurated_year,      SLE_INT32,                  31, SL_MAX_VERSION),

	    SLE_ARR(CompanyProperties, share_owners,          SLE_UINT8, 4),

	    SLE_VAR(CompanyProperties, num_valid_stat_ent,    SLE_UINT8),

	    SLE_VAR(CompanyProperties, months_of_bankruptcy,  SLE_UINT8),
	SLE_CONDVAR(CompanyProperties, bankrupt_asked,        SLE_FILE_U8  | SLE_VAR_U16,  0, 103),
	SLE_CONDVAR(CompanyProperties, bankrupt_asked,        SLE_UINT16,                104, SL_MAX_VERSION),
	    SLE_VAR(CompanyProperties, bankrupt_timeout,      SLE_INT16),
	SLE_CONDVAR(CompanyProperties, bankrupt_value,        SLE_VAR_I64 | SLE_FILE_I32,  0, 64),
	SLE_CONDVAR(CompanyProperties, bankrupt_value,        SLE_INT64,                  65, SL_MAX_VERSION),

	/* yearly expenses was changed to 64-bit in savegame version 2. */
	SLE_CONDARR(CompanyProperties, yearly_expenses,       SLE_FILE_I32 | SLE_VAR_I64, 3 * 13, 0, 1),
	SLE_CONDARR(CompanyProperties, yearly_expenses,       SLE_INT64, 3 * 13,                  2, SL_MAX_VERSION),

	SLE_CONDVAR(CompanyProperties, is_ai,                 SLE_BOOL,                    2, SL_MAX_VERSION),
	SLE_CONDNULL(1, 107, 111), ///< is_noai
	SLE_CONDNULL(1, 4, 99),

	SLE_CONDVAR(CompanyProperties, terraform_limit,       SLE_UINT32,                156, SL_MAX_VERSION),
	SLE_CONDVAR(CompanyProperties, clear_limit,           SLE_UINT32,                156, SL_MAX_VERSION),
	SLE_CONDVAR(CompanyProperties, tree_limit,            SLE_UINT32,                175, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _company_settings_desc[] = {
	/* Engine renewal settings */
	SLE_CONDNULL(512, 16, 18),
	SLE_CONDREF(Company, engine_renew_list,            REF_ENGINE_RENEWS,   19, SL_MAX_VERSION),
	SLE_CONDVAR(Company, settings.engine_renew,        SLE_BOOL,            16, SL_MAX_VERSION),
	SLE_CONDVAR(Company, settings.engine_renew_months, SLE_INT16,           16, SL_MAX_VERSION),
	SLE_CONDVAR(Company, settings.engine_renew_money,  SLE_UINT32,          16, SL_MAX_VERSION),
	SLE_CONDVAR(Company, settings.renew_keep_length,   SLE_BOOL,             2, SL_MAX_VERSION),

	/* Default vehicle settings */
	SLE_CONDVAR(Company, settings.vehicle.servint_ispercent,   SLE_BOOL,     120, SL_MAX_VERSION),
	SLE_CONDVAR(Company, settings.vehicle.servint_trains,    SLE_UINT16,     120, SL_MAX_VERSION),
	SLE_CONDVAR(Company, settings.vehicle.servint_roadveh,   SLE_UINT16,     120, SL_MAX_VERSION),
	SLE_CONDVAR(Company, settings.vehicle.servint_aircraft,  SLE_UINT16,     120, SL_MAX_VERSION),
	SLE_CONDVAR(Company, settings.vehicle.servint_ships,     SLE_UINT16,     120, SL_MAX_VERSION),

	SLE_CONDNULL(63, 2, 143), // old reserved space

	SLE_END()
};

static const SaveLoad _company_settings_skip_desc[] = {
	/* Engine renewal settings */
	SLE_CONDNULL(512, 16, 18),
	SLE_CONDNULL(2, 19, 68),                 // engine_renew_list
	SLE_CONDNULL(4, 69, SL_MAX_VERSION),     // engine_renew_list
	SLE_CONDNULL(1, 16, SL_MAX_VERSION),     // settings.engine_renew
	SLE_CONDNULL(2, 16, SL_MAX_VERSION),     // settings.engine_renew_months
	SLE_CONDNULL(4, 16, SL_MAX_VERSION),     // settings.engine_renew_money
	SLE_CONDNULL(1,  2, SL_MAX_VERSION),     // settings.renew_keep_length

	/* Default vehicle settings */
	SLE_CONDNULL(1, 120, SL_MAX_VERSION),    // settings.vehicle.servint_ispercent
	SLE_CONDNULL(2, 120, SL_MAX_VERSION),    // settings.vehicle.servint_trains
	SLE_CONDNULL(2, 120, SL_MAX_VERSION),    // settings.vehicle.servint_roadveh
	SLE_CONDNULL(2, 120, SL_MAX_VERSION),    // settings.vehicle.servint_aircraft
	SLE_CONDNULL(2, 120, SL_MAX_VERSION),    // settings.vehicle.servint_ships

	SLE_CONDNULL(63, 2, 143), // old reserved space

	SLE_END()
};

static const SaveLoad _company_economy_desc[] = {
	/* these were changed to 64-bit in savegame format 2 */
	SLE_CONDVAR(CompanyEconomyEntry, income,              SLE_FILE_I32 | SLE_VAR_I64, 0, 1),
	SLE_CONDVAR(CompanyEconomyEntry, income,              SLE_INT64,                  2, SL_MAX_VERSION),
	SLE_CONDVAR(CompanyEconomyEntry, expenses,            SLE_FILE_I32 | SLE_VAR_I64, 0, 1),
	SLE_CONDVAR(CompanyEconomyEntry, expenses,            SLE_INT64,                  2, SL_MAX_VERSION),
	SLE_CONDVAR(CompanyEconomyEntry, company_value,       SLE_FILE_I32 | SLE_VAR_I64, 0, 1),
	SLE_CONDVAR(CompanyEconomyEntry, company_value,       SLE_INT64,                  2, SL_MAX_VERSION),

	SLE_CONDVAR(CompanyEconomyEntry, delivered_cargo[NUM_CARGO - 1], SLE_INT32,       0, 169),
	SLE_CONDARR(CompanyEconomyEntry, delivered_cargo,     SLE_UINT32, 32,           170, 198),
	SLE_CONDARR(CompanyEconomyEntry, delivered_cargo,     SLE_UINT32, NUM_CARGO,    199, SL_MAX_VERSION),
	    SLE_VAR(CompanyEconomyEntry, performance_history, SLE_INT32),

	SLE_END()
};

/* We do need to read this single value, as the bigger it gets, the more data is stored */
struct CompanyOldAI {
	uint8 num_build_rec;
};

static const SaveLoad _company_ai_desc[] = {
	SLE_CONDNULL(2,  0, 106),
	SLE_CONDNULL(2,  0, 12),
	SLE_CONDNULL(4, 13, 106),
	SLE_CONDNULL(8,  0, 106),
	 SLE_CONDVAR(CompanyOldAI, num_build_rec, SLE_UINT8, 0, 106),
	SLE_CONDNULL(3,  0, 106),

	SLE_CONDNULL(2,  0,  5),
	SLE_CONDNULL(4,  6, 106),
	SLE_CONDNULL(2,  0,  5),
	SLE_CONDNULL(4,  6, 106),
	SLE_CONDNULL(2,  0, 106),

	SLE_CONDNULL(2,  0,  5),
	SLE_CONDNULL(4,  6, 106),
	SLE_CONDNULL(2,  0,  5),
	SLE_CONDNULL(4,  6, 106),
	SLE_CONDNULL(2,  0, 106),

	SLE_CONDNULL(2,  0, 68),
	SLE_CONDNULL(4,  69, 106),

	SLE_CONDNULL(18, 0, 106),
	SLE_CONDNULL(20, 0, 106),
	SLE_CONDNULL(32, 0, 106),

	SLE_CONDNULL(64, 2, 106),
	SLE_END()
};

static const SaveLoad _company_ai_build_rec_desc[] = {
	SLE_CONDNULL(2, 0, 5),
	SLE_CONDNULL(4, 6, 106),
	SLE_CONDNULL(2, 0, 5),
	SLE_CONDNULL(4, 6, 106),
	SLE_CONDNULL(8, 0, 106),
	SLE_END()
};

static const SaveLoad _company_livery_desc[] = {
	SLE_CONDVAR(Livery, in_use,  SLE_BOOL,  34, SL_MAX_VERSION),
	SLE_CONDVAR(Livery, colour1, SLE_UINT8, 34, SL_MAX_VERSION),
	SLE_CONDVAR(Livery, colour2, SLE_UINT8, 34, SL_MAX_VERSION),
	SLE_END()
};

static void SaveLoad_PLYR_common(Company *c, CompanyProperties *cprops)
{
	int i;

	SlObject(cprops, _company_desc);
	if (c != NULL) {
		SlObject(c, _company_settings_desc);
	} else {
		char nothing;
		SlObject(&nothing, _company_settings_skip_desc);
	}

	/* Keep backwards compatible for savegames, so load the old AI block */
	if (IsSavegameVersionBefore(107) && cprops->is_ai) {
		CompanyOldAI old_ai;
		char nothing;

		SlObject(&old_ai, _company_ai_desc);
		for (i = 0; i != old_ai.num_build_rec; i++) {
			SlObject(&nothing, _company_ai_build_rec_desc);
		}
	}

	/* Write economy */
	SlObject(&cprops->cur_economy, _company_economy_desc);

	/* Write old economy entries. */
	if (cprops->num_valid_stat_ent > lengthof(cprops->old_economy)) SlErrorCorrupt("Too many old economy entries");
	for (i = 0; i < cprops->num_valid_stat_ent; i++) {
		SlObject(&cprops->old_economy[i], _company_economy_desc);
	}

	/* Write each livery entry. */
	int num_liveries = IsSavegameVersionBefore(63) ? LS_END - 4 : (IsSavegameVersionBefore(85) ? LS_END - 2: LS_END);
	if (c != NULL) {
		for (i = 0; i < num_liveries; i++) {
			SlObject(&c->livery[i], _company_livery_desc);
		}

		if (num_liveries < LS_END) {
			/* We want to insert some liveries somewhere in between. This means some have to be moved. */
			memmove(&c->livery[LS_FREIGHT_WAGON], &c->livery[LS_PASSENGER_WAGON_MONORAIL], (LS_END - LS_FREIGHT_WAGON) * sizeof(c->livery[0]));
			c->livery[LS_PASSENGER_WAGON_MONORAIL] = c->livery[LS_MONORAIL];
			c->livery[LS_PASSENGER_WAGON_MAGLEV]   = c->livery[LS_MAGLEV];
		}

		if (num_liveries == LS_END - 4) {
			/* Copy bus/truck liveries over to trams */
			c->livery[LS_PASSENGER_TRAM] = c->livery[LS_BUS];
			c->livery[LS_FREIGHT_TRAM]   = c->livery[LS_TRUCK];
		}
	} else {
		/* Skip liveries */
		Livery dummy_livery;
		for (i = 0; i < num_liveries; i++) {
			SlObject(&dummy_livery, _company_livery_desc);
		}
	}
}

static void SaveLoad_PLYR(Company *c)
{
	SaveLoad_PLYR_common(c, c);
}

static void Save_PLYR()
{
	Company *c;
	FOR_ALL_COMPANIES(c) {
		SlSetArrayIndex(c->index);
		SlAutolength((AutolengthProc*)SaveLoad_PLYR, c);
	}
}

static void Load_PLYR()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		Company *c = new (index) Company();
		SaveLoad_PLYR(c);
		_company_colours[index] = (Colours)c->colour;
	}
}

static void Check_PLYR()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		CompanyProperties *cprops = new CompanyProperties();
		SaveLoad_PLYR_common(NULL, cprops);

		/* We do not load old custom names */
		if (IsSavegameVersionBefore(84)) {
			if (GetStringTab(cprops->name_1) == TEXT_TAB_OLD_CUSTOM) {
				cprops->name_1 = STR_GAME_SAVELOAD_NOT_AVAILABLE;
			}

			if (GetStringTab(cprops->president_name_1) == TEXT_TAB_OLD_CUSTOM) {
				cprops->president_name_1 = STR_GAME_SAVELOAD_NOT_AVAILABLE;
			}
		}

		if (cprops->name == NULL && !IsInsideMM(cprops->name_1, SPECSTR_COMPANY_NAME_START, SPECSTR_COMPANY_NAME_LAST + 1) &&
				cprops->name_1 != STR_GAME_SAVELOAD_NOT_AVAILABLE && cprops->name_1 != STR_SV_UNNAMED &&
				cprops->name_1 != SPECSTR_ANDCO_NAME && cprops->name_1 != SPECSTR_PRESIDENT_NAME &&
				cprops->name_1 != SPECSTR_SILLY_NAME) {
			cprops->name_1 = STR_GAME_SAVELOAD_NOT_AVAILABLE;
		}

		if (!_load_check_data.companies.Insert(index, cprops)) delete cprops;
	}
}

static void Ptrs_PLYR()
{
	Company *c;
	FOR_ALL_COMPANIES(c) {
		SlObject(c, _company_settings_desc);
	}
}


extern const ChunkHandler _company_chunk_handlers[] = {
	{ 'PLYR', Save_PLYR, Load_PLYR, Ptrs_PLYR, Check_PLYR, CH_ARRAY | CH_LAST},
};
