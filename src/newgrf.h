/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf.h Base for the NewGRF implementation. */

#ifndef NEWGRF_H
#define NEWGRF_H

#include "cargotype.h"
#include "rail_type.h"
#include "road_type.h"
#include "fileio_type.h"
#include "newgrf_badge_type.h"
#include "newgrf_callbacks.h"
#include "newgrf_text_type.h"

/**
 * List of different canal 'features'.
 * Each feature gets an entry in the canal spritegroup table
 */
enum CanalFeature : uint8_t {
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
	CanalCallbackMasks callback_mask;  ///< Bitmask of canal callbacks that have to be called.
	uint8_t flags;          ///< Flags controlling display.
};

enum GrfLoadingStage : uint8_t {
	GLS_FILESCAN,
	GLS_SAFETYSCAN,
	GLS_LABELSCAN,
	GLS_INIT,
	GLS_RESERVE,
	GLS_ACTIVATION,
	GLS_END,
};

DECLARE_INCREMENT_DECREMENT_OPERATORS(GrfLoadingStage)

enum class GrfMiscBit : uint8_t {
	DesertTreesFields = 0, // Unsupported.
	DesertPavedRoads = 1,
	FieldBoundingBox = 2, // Unsupported.
	TrainWidth32Pixels = 3, ///< Use 32 pixels per train vehicle in depot gui and vehicle details. Never set in the global variable; @see GRFFile::traininfo_vehicle_width
	AmbientSoundCallback = 4,
	CatenaryOn3rdTrack = 5, // Unsupported.
	SecondRockyTileSet = 6,
};

using GrfMiscBits = EnumBitSet<GrfMiscBit, uint8_t>;

enum GrfSpecFeature : uint8_t {
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
	GSF_BADGES,
	GSF_END,

	GSF_DEFAULT = GSF_END, ///< Unspecified feature, default badge
	GSF_FAKE_TOWNS = GSF_END, ///< Fake town GrfSpecFeature for NewGRF debugging (parent scope)
	GSF_FAKE_END,             ///< End of the fake features

	GSF_ORIGINAL_STRINGS = 0x48,

	GSF_INVALID = 0xFF,       ///< An invalid spec feature
};
using GrfSpecFeatures = EnumBitSet<GrfSpecFeature, uint32_t, GrfSpecFeature::GSF_END>;

static const uint32_t INVALID_GRFID = 0xFFFFFFFF;

struct GRFLabel {
	uint8_t label;
	uint32_t nfo_line;
	size_t pos;

	GRFLabel(uint8_t label, uint32_t nfo_line, size_t pos) : label(label), nfo_line(nfo_line), pos(pos) {}
};

/** Dynamic data of a loaded NewGRF */
struct GRFFile {
	std::string filename{};
	uint32_t grfid = 0;
	uint8_t grf_version = 0;

	uint sound_offset = 0;
	uint16_t num_sounds = 0;

	std::vector<std::unique_ptr<struct StationSpec>> stations;
	std::vector<std::unique_ptr<struct HouseSpec>> housespec;
	std::vector<std::unique_ptr<struct IndustrySpec>> industryspec;
	std::vector<std::unique_ptr<struct IndustryTileSpec>> indtspec;
	std::vector<std::unique_ptr<struct ObjectSpec>> objectspec;
	std::vector<std::unique_ptr<struct AirportSpec>> airportspec;
	std::vector<std::unique_ptr<struct AirportTileSpec>> airtspec;
	std::vector<std::unique_ptr<struct RoadStopSpec>> roadstops;

	std::vector<uint32_t> param{};

	std::vector<GRFLabel> labels{}; ///< List of labels

	std::vector<CargoLabel> cargo_list{}; ///< Cargo translation table (local ID -> label)
	std::array<uint8_t, NUM_CARGO> cargo_map{}; ///< Inverse cargo translation table (CargoType -> local ID)

	std::vector<BadgeID> badge_list{}; ///< Badge translation table (local index -> global index)
	std::unordered_map<uint16_t, BadgeID> badge_map{};

	std::vector<RailTypeLabel> railtype_list{}; ///< Railtype translation table
	std::array<RailType, RAILTYPE_END> railtype_map{};

	std::vector<RoadTypeLabel> roadtype_list{}; ///< Roadtype translation table (road)
	std::array<RoadType, ROADTYPE_END> roadtype_map{};

	std::vector<RoadTypeLabel> tramtype_list{}; ///< Roadtype translation table (tram)
	std::array<RoadType, ROADTYPE_END> tramtype_map{};

	std::array<CanalProperties, CF_END> canal_local_properties{}; ///< Canal properties as set by this NewGRF

	std::unordered_map<uint8_t, LanguageMap> language_map{}; ///< Mappings related to the languages.

	int traininfo_vehicle_pitch = 0; ///< Vertical offset for drawing train images in depot GUI and vehicle details
	uint traininfo_vehicle_width = 0; ///< Width (in pixels) of a 8/8 train vehicle in depot GUI and vehicle details

	GrfSpecFeatures grf_features{}; ///< Bitset of GrfSpecFeature the grf uses
	PriceMultipliers price_base_multipliers{}; ///< Price base multipliers as set by the grf.

	GRFFile(const struct GRFConfig &config);
	GRFFile();
	GRFFile(GRFFile &&other);
	~GRFFile();

	/** Get GRF Parameter with range checking */
	uint32_t GetParam(uint number) const
	{
		/* Note: We implicitly test for number < this->param.size() and return 0 for invalid parameters.
		 *       In fact this is the more important test, as param is zeroed anyway. */
		return (number < std::size(this->param)) ? this->param[number] : 0;
	}
};

enum ShoreReplacement : uint8_t {
	SHORE_REPLACE_NONE,       ///< No shore sprites were replaced.
	SHORE_REPLACE_ACTION_5,   ///< Shore sprites were replaced by Action5.
	SHORE_REPLACE_ACTION_A,   ///< Shore sprites were replaced by ActionA (using grass tiles for the corner-shores).
	SHORE_REPLACE_ONLY_NEW,   ///< Only corner-shores were loaded by Action5 (openttd(w/d).grf only).
};

enum TramReplacement : uint8_t {
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
 * Describes properties of price bases.
 */
struct PriceBaseSpec {
	Money start_price; ///< Default value at game start, before adding multipliers.
	PriceCategory category; ///< Price is affected by certain difficulty settings.
	GrfSpecFeature grf_feature; ///< GRF Feature that decides whether price multipliers apply locally or globally, #GSF_END if none.
	Price fallback_price; ///< Fallback price multiplier for new prices but old grfs.
};

/**
 * Check for grf miscellaneous bits
 * @param bit The bit to check.
 * @return Whether the bit is set.
 */
inline bool HasGrfMiscBit(GrfMiscBit bit)
{
	extern GrfMiscBits _misc_grf_features;
	return _misc_grf_features.Test(bit);
}

/* Indicates which are the newgrf features currently loaded ingame */
extern GRFLoadedFeatures _loaded_newgrf_features;

void LoadNewGRFFile(GRFConfig &config, GrfLoadingStage stage, Subdirectory subdir, bool temporary);
void LoadNewGRF(SpriteID load_index, uint num_baseset);
void ReloadNewGRFData(); // in saveload/afterload.cpp
void ResetNewGRFData();
void ResetPersistentNewGRFData();

void GrfMsgI(int severity, const std::string &msg);
#define GrfMsg(severity, format_string, ...) do { if ((severity) == 0 || _debug_grf_level >= (severity)) GrfMsgI(severity, fmt::format(FMT_STRING(format_string) __VA_OPT__(,) __VA_ARGS__)); } while (false)

bool GetGlobalVariable(uint8_t param, uint32_t *value, const GRFFile *grffile);

StringID MapGRFStringID(uint32_t grfid, GRFStringID str);
void ShowNewGRFError();

#endif /* NEWGRF_H */
