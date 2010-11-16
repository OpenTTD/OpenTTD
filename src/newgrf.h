/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf.h Base for the NewGRF implementation. */

#ifndef NEWGRF_H
#define NEWGRF_H

#include "cargotype.h"
#include "rail_type.h"

enum GrfLoadingStage {
	GLS_FILESCAN,
	GLS_SAFETYSCAN,
	GLS_LABELSCAN,
	GLS_INIT,
	GLS_RESERVE,
	GLS_ACTIVATION,
	GLS_END,
};

DECLARE_POSTFIX_INCREMENT(GrfLoadingStage)

enum GrfMiscBit {
	GMB_DESERT_TREES_FIELDS    = 0, // Unsupported.
	GMB_DESERT_PAVED_ROADS     = 1,
	GMB_FIELD_BOUNDING_BOX     = 2, // Unsupported.
	GMB_TRAIN_WIDTH_32_PIXELS  = 3, ///< Use 32 pixels per train vehicle in depot gui and vehicle details. Never set in the global variable; @see GRFFile::traininfo_vehicle_width
	GMB_AMBIENT_SOUND_CALLBACK = 4, // Unsupported.
	GMB_CATENARY_ON_3RD_TRACK  = 5, // Unsupported.
};

enum GrfSpecFeature {
	GSF_TRAINS,
	GSF_ROADVEHICLES,
	GSF_SHIPS,
	GSF_AIRCRAFT,
	GSF_STATIONS,
	GSF_CANALS,
	GSF_BRIDGES,
	GSF_HOUSES,
	GSF_GLOBALVAR,
	GSF_INDUSTRYTILES,
	GSF_INDUSTRIES,
	GSF_CARGOS,
	GSF_SOUNDFX,
	GSF_AIRPORTS,
	GSF_SIGNALS,
	GSF_OBJECTS,
	GSF_RAILTYPES,
	GSF_AIRPORTTILES,
	GSF_END,

	GSF_FAKE_TOWNS = GSF_END, ///< Fake town GrfSpecFeature for NewGRF debugging (parent scope)
	GSF_FAKE_END,             ///< End of the fake features

	GSF_INVALID = 0xFF        ///< An invalid spec feature
};

static const uint32 INVALID_GRFID = 0xFFFFFFFF;

struct GRFLabel {
	byte label;
	uint32 nfo_line;
	size_t pos;
	struct GRFLabel *next;
};

/** Dynamic data of a loaded NewGRF */
struct GRFFile {
	char *filename;
	bool is_ottdfile;
	uint32 grfid;
	uint16 sprite_offset;
	byte grf_version;

	/* A sprite group contains all sprites of a given vehicle (or multiple
	 * vehicles) when carrying given cargo. It consists of several sprite
	 * sets.  Group ids are refered as "cargo id"s by TTDPatch
	 * documentation, contributing to the global confusion.
	 *
	 * A sprite set contains all sprites of a given vehicle carrying given
	 * cargo at a given *stage* - that is usually its load stage. Ie. you
	 * can have a spriteset for an empty wagon, wagon full of coal,
	 * half-filled wagon etc.  Each spriteset contains eight sprites (one
	 * per direction) or four sprites if the vehicle is symmetric. */

	SpriteID spriteset_start;
	int spriteset_numsets;
	int spriteset_numents;
	int spriteset_feature;

	int spritegroups_count;
	struct SpriteGroup **spritegroups;

	uint sound_offset;
	uint16 num_sounds;

	struct StationSpec **stations;
	struct HouseSpec **housespec;
	struct IndustrySpec **industryspec;
	struct IndustryTileSpec **indtspec;
	struct ObjectSpec **objectspec;
	struct AirportSpec **airportspec;
	struct AirportTileSpec **airtspec;

	uint32 param[0x80];
	uint param_end;  ///< one more than the highest set parameter

	GRFLabel *label; ///< Pointer to the first label. This is a linked list, not an array.

	uint8 cargo_max;
	CargoLabel *cargo_list;
	uint8 cargo_map[NUM_CARGO];

	uint8 railtype_max;
	RailTypeLabel *railtype_list;
	RailType railtype_map[RAILTYPE_END];

	struct LanguageMap *language_map; ///< Mappings related to the languages.

	int traininfo_vehicle_pitch;  ///< Vertical offset for draing train images in depot GUI and vehicle details
	uint traininfo_vehicle_width; ///< Width (in pixels) of a 8/8 train vehicle in depot GUI and vehicle details

	uint32 grf_features;                     ///< Bitset of GrfSpecFeature the grf uses
	PriceMultipliers price_base_multipliers; ///< Price base multipliers as set by the grf.

	/** Get GRF Parameter with range checking */
	uint32 GetParam(uint number) const
	{
		/* Note: We implicitly test for number < lengthof(this->param) and return 0 for invalid parameters.
		 *       In fact this is the more important test, as param is zeroed anyway. */
		assert(this->param_end <= lengthof(this->param));
		return (number < this->param_end) ? this->param[number] : 0;
	}
};

enum ShoreReplacement {
	SHORE_REPLACE_NONE,       ///< No shore sprites were replaced.
	SHORE_REPLACE_ACTION_5,   ///< Shore sprites were replaced by Action5.
	SHORE_REPLACE_ACTION_A,   ///< Shore sprites were replaced by ActionA (using grass tiles for the corner-shores).
	SHORE_REPLACE_ONLY_NEW,   ///< Only corner-shores were loaded by Action5 (openttd(w/d).grf only).
};

struct GRFLoadedFeatures {
	bool has_2CC;             ///< Set if any vehicle is loaded which uses 2cc (two company colours).
	uint64 used_liveries;     ///< Bitmask of #LiveryScheme used by the defined engines.
	bool has_newhouses;       ///< Set if there are any newhouses loaded.
	bool has_newindustries;   ///< Set if there are any newindustries loaded.
	ShoreReplacement shore;   ///< It which way shore sprites were replaced.
};

/* Indicates which are the newgrf features currently loaded ingame */
extern GRFLoadedFeatures _loaded_newgrf_features;

void LoadNewGRFFile(struct GRFConfig *config, uint file_index, GrfLoadingStage stage);
void LoadNewGRF(uint load_index, uint file_index);
void ReloadNewGRFData(); // in saveload/afterload.cpp
void ResetNewGRFData();

void CDECL grfmsg(int severity, const char *str, ...) WARN_FORMAT(2, 3);

bool HasGrfMiscBit(GrfMiscBit bit);
bool GetGlobalVariable(byte param, uint32 *value);

StringID MapGRFStringID(uint32 grfid, StringID str);
void ShowNewGRFError();

#endif /* NEWGRF_H */
