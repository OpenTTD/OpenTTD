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
#include "road_type.h"
#include "fileio_type.h"
#include "core/bitmath_func.hpp"
#include "core/alloc_type.hpp"
#include "core/mem_func.hpp"

/**
 * List of different canal 'features'.
 * Each feature gets an entry in the canal spritegroup table
 */
enum CanalFeature {
	CF_WATERSLOPE,
	CF_LOCKS,
	CF_DIKES,
	CF_ICON,
	CF_DOCKS,
	CF_RIVER_SLOPE,
	CF_RIVER_EDGE,
	CF_RIVER_GUI,
	CF_BUOY,
	CF_END,
};

/** Canal properties local to the NewGRF */
struct CanalProperties {
	uint8_t callback_mask;  ///< Bitmask of canal callbacks that have to be called.
	uint8_t flags;          ///< Flags controlling display.
};

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
	GMB_AMBIENT_SOUND_CALLBACK = 4,
	GMB_CATENARY_ON_3RD_TRACK  = 5, // Unsupported.
	GMB_SECOND_ROCKY_TILE_SET  = 6,
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
	GSF_CARGOES,
	GSF_SOUNDFX,
	GSF_AIRPORTS,
	GSF_SIGNALS,
	GSF_OBJECTS,
	GSF_RAILTYPES,
	GSF_AIRPORTTILES,
	GSF_ROADTYPES,
	GSF_TRAMTYPES,
	GSF_ROADSTOPS,
	GSF_END,

	GSF_FAKE_TOWNS = GSF_END, ///< Fake town GrfSpecFeature for NewGRF debugging (parent scope)
	GSF_FAKE_END,             ///< End of the fake features

	GSF_INVALID = 0xFF,       ///< An invalid spec feature
};

static const uint32_t INVALID_GRFID = 0xFFFFFFFF;

struct GRFLabel {
	byte label;
	uint32_t nfo_line;
	size_t pos;

	GRFLabel(byte label, uint32_t nfo_line, size_t pos) : label(label), nfo_line(nfo_line), pos(pos) {}
};

/** Dynamic data of a loaded NewGRF */
struct GRFFile : ZeroedMemoryAllocator {
	std::string filename;
	uint32_t grfid;
	byte grf_version;

	uint sound_offset;
	uint16_t num_sounds;

	std::vector<std::unique_ptr<struct StationSpec>> stations;
	std::vector<std::unique_ptr<struct HouseSpec>> housespec;
	std::vector<std::unique_ptr<struct IndustrySpec>> industryspec;
	std::vector<std::unique_ptr<struct IndustryTileSpec>> indtspec;
	std::vector<std::unique_ptr<struct ObjectSpec>> objectspec;
	std::vector<std::unique_ptr<struct AirportSpec>> airportspec;
	std::vector<std::unique_ptr<struct AirportTileSpec>> airtspec;
	std::vector<std::unique_ptr<struct RoadStopSpec>> roadstops;

	std::array<uint32_t, 0x80> param;
	uint param_end;  ///< one more than the highest set parameter

	std::vector<GRFLabel> labels;                   ///< List of labels

	std::vector<CargoLabel> cargo_list;             ///< Cargo translation table (local ID -> label)
	std::array<uint8_t, NUM_CARGO> cargo_map{}; ///< Inverse cargo translation table (CargoID -> local ID)

	std::vector<RailTypeLabel> railtype_list;       ///< Railtype translation table
	RailType railtype_map[RAILTYPE_END];

	std::vector<RoadTypeLabel> roadtype_list;       ///< Roadtype translation table (road)
	RoadType roadtype_map[ROADTYPE_END];

	std::vector<RoadTypeLabel> tramtype_list;       ///< Roadtype translation table (tram)
	RoadType tramtype_map[ROADTYPE_END];

	CanalProperties canal_local_properties[CF_END]; ///< Canal properties as set by this NewGRF

	struct LanguageMap *language_map; ///< Mappings related to the languages.

	int traininfo_vehicle_pitch;  ///< Vertical offset for drawing train images in depot GUI and vehicle details
	uint traininfo_vehicle_width; ///< Width (in pixels) of a 8/8 train vehicle in depot GUI and vehicle details

	uint32_t grf_features;                     ///< Bitset of GrfSpecFeature the grf uses
	PriceMultipliers price_base_multipliers; ///< Price base multipliers as set by the grf.

	GRFFile(const struct GRFConfig *config);
	~GRFFile();

	/** Get GRF Parameter with range checking */
	uint32_t GetParam(uint number) const
	{
		/* Note: We implicitly test for number < this->param.size() and return 0 for invalid parameters.
		 *       In fact this is the more important test, as param is zeroed anyway. */
		assert(this->param_end <= this->param.size());
		return (number < this->param_end) ? this->param[number] : 0;
	}
};

enum ShoreReplacement {
	SHORE_REPLACE_NONE,       ///< No shore sprites were replaced.
	SHORE_REPLACE_ACTION_5,   ///< Shore sprites were replaced by Action5.
	SHORE_REPLACE_ACTION_A,   ///< Shore sprites were replaced by ActionA (using grass tiles for the corner-shores).
	SHORE_REPLACE_ONLY_NEW,   ///< Only corner-shores were loaded by Action5 (openttd(w/d).grf only).
};

enum TramReplacement {
	TRAMWAY_REPLACE_DEPOT_NONE,       ///< No tram depot graphics were loaded.
	TRAMWAY_REPLACE_DEPOT_WITH_TRACK, ///< Electrified depot graphics with tram track were loaded.
	TRAMWAY_REPLACE_DEPOT_NO_TRACK,   ///< Electrified depot graphics without tram track were loaded.
};

struct GRFLoadedFeatures {
	bool has_2CC;             ///< Set if any vehicle is loaded which uses 2cc (two company colours).
	uint64_t used_liveries;     ///< Bitmask of #LiveryScheme used by the defined engines.
	ShoreReplacement shore;   ///< In which way shore sprites were replaced.
	TramReplacement tram;     ///< In which way tram depots were replaced.
};

/**
 * Check for grf miscellaneous bits
 * @param bit The bit to check.
 * @return Whether the bit is set.
 */
static inline bool HasGrfMiscBit(GrfMiscBit bit)
{
	extern byte _misc_grf_features;
	return HasBit(_misc_grf_features, bit);
}

/* Indicates which are the newgrf features currently loaded ingame */
extern GRFLoadedFeatures _loaded_newgrf_features;

void LoadNewGRFFile(struct GRFConfig *config, GrfLoadingStage stage, Subdirectory subdir, bool temporary);
void LoadNewGRF(uint load_index, uint num_baseset);
void ReloadNewGRFData(); // in saveload/afterload.cpp
void ResetNewGRFData();
void ResetPersistentNewGRFData();

void GrfMsgI(int severity, const std::string &msg);
#define GrfMsg(severity, format_string, ...) GrfMsgI(severity, fmt::format(FMT_STRING(format_string), ## __VA_ARGS__))

bool GetGlobalVariable(byte param, uint32_t *value, const GRFFile *grffile);

StringID MapGRFStringID(uint32_t grfid, StringID str);
void ShowNewGRFError();

#endif /* NEWGRF_H */
