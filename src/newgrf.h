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
#include "livery.h"
#include "rail_type.h"
#include "road_type.h"
#include "fileio_type.h"
#include "newgrf_badge_type.h"
#include "newgrf_callbacks.h"
#include "newgrf_text_type.h"
#include "vehicle_type.h"

/**
 * List of different canal 'features'.
 * Each feature gets an entry in the canal spritegroup table
 */
enum class CanalFeature : uint8_t {
	LockWaterSlope, ///< The sloped water tiles in locks.
	Locks, ///< The sides of the lock.
	Dikes, ///< The canal dikes/embankment.
	Icon, ///< Unused: the TTDP UI icon for canals.
	FlatDocks, ///< Unused: the graphics for TTDP flat docks.
	RiverSlope, ///< The sloped water tiles for rivers.
	RiverEdge, ///< The river banks.
	RiverIcon, ///< Unused: the TTDP UI icons for rivers.
	Buoy, ///< Buoy without underlying water.
	End, ///< End marker.
};

/** Flags controlling the display of canals. */
enum class CanalFeatureFlag : uint8_t {
	HasFlatSprite = 0, ///< Additional flat ground sprite in the beginning.
};
/** CanalFeatureFlag bitmask. */
using CanalFeatureFlags = EnumBitSet<CanalFeatureFlag, uint8_t>;

/** Canal properties local to the NewGRF */
struct CanalProperties {
	CanalCallbackMasks callback_mask; ///< Bitmask of canal callbacks that have to be called.
	CanalFeatureFlags flags; ///< Flags controlling display.
};

/** Stages of loading all NewGRFs. */
enum class GrfLoadingStage : uint8_t {
	FileScan, ///< Load the Action 8 metadata (GRF ID, name).
	SafetyScan, ///< Checks whether the NewGRF can be used in a static context.
	LabelScan, ///< First step of NewGRF loading; find the 'goto' labels in the NewGRF.
	Init, ///< Second step of NewGRF loading; load all actions into memory.
	Reserve, ///< Third step of NewGRF loading; reserve features and GRMs.
	Activation, ///< Forth step of NewGRF loading; activate the features.
	End, ///< End marker.
};

/** Bits of NewGRF's GlobalVariable 1E/9E. */
enum class GrfMiscBit : uint8_t {
	DesertTreesFields = 0, ///< Unsupported: allow trees and fields in desert climate.
	DesertPavedRoads = 1, ///< Show pavement and lights in desert towns
	FieldBoundingBox = 2, ///< Unsupported: fields have a height.
	TrainWidth32Pixels = 3, ///< Use 32 pixels per train vehicle in depot gui and vehicle details. Never set in the global variable; @see GRFFile::traininfo_vehicle_width
	AmbientSoundCallback = 4, ///< Enable ambient sound effect callback 144.
	CatenaryOn3rdTrack = 5, ///< Unsupported: enable catenaries over third track type.
	SecondRockyTileSet = 6, ///< Enable using the second rocky tile set.
};

/** Bitset of \c GrfMiscBit elements. */
using GrfMiscBits = EnumBitSet<GrfMiscBit, uint8_t>;

enum class GrfSpecFeature : uint8_t {
	Trains, ///< Trains feature
	RoadVehicles, ///< Road vehicles feature
	Ships, ///< Ships feature
	Aircraft, ///< Aircraft feature
	Stations, ///< Stations feature
	Canals, ///< Canals feature
	Bridges, ///< Bridges feature
	Houses, ///< Houses feature
	GlobalVar, ///< Global variables feature
	IndustryTiles, ///< Industry tiles feature
	Industries, ///< Industries feature
	Cargoes, ///< Cargoes feature
	SoundEffects, ///< Sound effects feature
	Airports, ///< Airports feature
	Signals, ///< Signals feature
	Objects, ///< Objects feature
	RailTypes, ///< Rail types feature
	AirportTiles, ///< Airport tiles feature
	RoadTypes, ///< Road types feature
	TramTypes, ///< Tram types feature
	RoadStops, ///< Road stops feature
	Badges, ///< Badges feature
	End, ///< End marker

	Default = End, ///< Unspecified feature, default badge
	FakeTowns = End, ///< Fake town GrfSpecFeature for NewGRF debugging (parent scope)
	FakeEnd, ///< End of the fake features

	OriginalStrings = 0x48, ///< Pseudo unsupported 'feature' for replacing original strings

	Invalid = 0xFF, ///< An invalid spec feature
};

/** Bitset of \c GrfSpecFeature elements. */
using GrfSpecFeatures = EnumBitSet<GrfSpecFeature, uint32_t, GrfSpecFeature::End>;

struct GRFLabel {
	uint8_t label;
	uint32_t nfo_line;
	size_t pos;

	GRFLabel(uint8_t label, uint32_t nfo_line, size_t pos) : label(label), nfo_line(nfo_line), pos(pos) {}
};

/** Dynamic data of a loaded NewGRF */
struct GRFFile {
	std::string filename{};
	GrfID grfid{};
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

	EnumIndexArray<CanalProperties, CanalFeature, CanalFeature::End> canal_local_properties{}; ///< Canal properties as set by this NewGRF

	std::unordered_map<GRFLanguage, LanguageMap> language_map{}; ///< Mappings related to the languages.

	int traininfo_vehicle_pitch = 0; ///< Vertical offset for drawing train images in depot GUI and vehicle details
	uint traininfo_vehicle_width = 0; ///< Width (in pixels) of a 8/8 train vehicle in depot GUI and vehicle details
	bool cargo_list_is_fallback = false; ///< Set if cargo types have been created but a cargo list has not been installed

	GrfSpecFeatures grf_features{}; ///< Bitset of GrfSpecFeature the grf uses
	PriceMultipliers price_base_multipliers{}; ///< Price base multipliers as set by the grf.

	GRFFile(const GRFConfig &config);
	GRFFile();
	GRFFile(GRFFile &&other);
	~GRFFile();

	/**
	 * Get GRF Parameter with range checking.
	 * @param number The parameter number/index.
	 * @return The parameter, or \c 0 when the number is out of bounds.
	 */
	uint32_t GetParam(uint number) const
	{
		/* Note: We implicitly test for number < this->param.size() and return 0 for invalid parameters.
		 *       In fact this is the more important test, as param is zeroed anyway. */
		return (number < std::size(this->param)) ? this->param[number] : 0;
	}
};

/** Type of shore replacement loaded by NewGRFs. */
enum class ShoreReplacement : uint8_t {
	None, ///< No shore sprites were replaced.
	Action5, ///< Shore sprites were replaced by Action5.
	ActionA, ///< Shore sprites were replaced by ActionA (using grass tiles for the corner-shores).
	OnlyNew, ///< Only corner-shores were loaded by Action5 (openttd(w/d).grf only).
};

/** Type of tram depot replacement loaded by NewGRFs. */
enum class TramDepotReplacement : uint8_t {
	None, ///< No tram depot graphics were loaded.
	WithTrack, ///< Electrified depot graphics with tram track were loaded.
	WithoutTrack, ///< Electrified depot graphics without tram track were loaded.
};

/** State of features loaded by NewGRFs. */
struct GRFLoadedFeatures {
	bool has_2CC; ///< Set if any vehicle is loaded which uses 2cc (two company colours).
	LiverySchemes used_liveries; ///< Bitmask of #LiveryScheme used by the defined engines.
	ShoreReplacement shore; ///< In which way shore sprites were replaced.
	TramDepotReplacement tram; ///< In which way tram depots were replaced.
};

/**
 * Describes properties of price bases.
 */
struct PriceBaseSpec {
	Money start_price; ///< Default value at game start, before adding multipliers.
	PriceCategory category; ///< Price is affected by certain difficulty settings.
	GrfSpecFeature grf_feature; ///< GRF Feature that decides whether price multipliers apply locally or globally, #GrfSpecFeature::End if none.
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

StringID MapGRFStringID(GrfID grfid, GRFStringID str);
void ShowNewGRFError();

GrfSpecFeature GetGrfSpecFeature(VehicleType type);
VehicleType GetVehicleType(GrfSpecFeature feature);

/**
 * Flatten a NewGRF related label to a 32 bits integer.
 * @param label The label to flatten.
 * @return The flattened label.
 */
template <typename T> requires std::is_base_of_v<BaseLabel, T>
constexpr uint32_t FlattenNewGRFLabel(T label) { return label[0] << 24 | label[1] << 16 | label[2] << 8 | label[3]; }

/**
 * Unflatten a NewGRF related label from a 32 bits integer.
 * @param value The value to decode.
 * @return The unflattened label.
 */
template <typename T> requires std::is_base_of_v<BaseLabel, T>
constexpr T UnflattenNewGRFLabel(uint32_t value)
{
	uint8_t buf[]{
		static_cast<uint8_t>(GB(value, 24, 8)),
		static_cast<uint8_t>(GB(value, 16, 8)),
		static_cast<uint8_t>(GB(value, 8, 8)),
		static_cast<uint8_t>(GB(value, 0, 8))
	};
	return buf;
}

#endif /* NEWGRF_H */
