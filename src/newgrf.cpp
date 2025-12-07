/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf.cpp Base of all NewGRF support. */

#include "stdafx.h"
#include "core/backup_type.hpp"
#include "core/container_func.hpp"
#include "company_manager_face.h"
#include "debug.h"
#include "fileio_func.h"
#include "engine_func.h"
#include "engine_base.h"
#include "bridge.h"
#include "town.h"
#include "newgrf_engine.h"
#include "newgrf_text.h"
#include "spritecache.h"
#include "currency.h"
#include "landscape.h"
#include "newgrf_badge.h"
#include "newgrf_badge_config.h"
#include "newgrf_cargo.h"
#include "newgrf_sound.h"
#include "newgrf_station.h"
#include "industrytype.h"
#include "newgrf_canal.h"
#include "newgrf_townname.h"
#include "newgrf_industries.h"
#include "newgrf_airporttiles.h"
#include "newgrf_airport.h"
#include "newgrf_object.h"
#include "network/core/config.h"
#include "smallmap_gui.h"
#include "genworld.h"
#include "error_func.h"
#include "vehicle_base.h"
#include "road.h"
#include "newgrf_roadstop.h"
#include "newgrf/newgrf_bytereader.h"
#include "newgrf/newgrf_internal_vehicle.h"
#include "newgrf/newgrf_internal.h"
#include "newgrf/newgrf_stringmapping.h"

#include "table/strings.h"

#include "safeguards.h"

/* TTDPatch extended GRF format codec
 * (c) Petr Baudis 2004 (GPL'd)
 * Changes by Florian octo Forster are (c) by the OpenTTD development team.
 *
 * Contains portions of documentation by TTDPatch team.
 * Thanks especially to Josef Drexler for the documentation as well as a lot
 * of help at #tycoon. Also thanks to Michael Blunck for his GRF files which
 * served as subject to the initial testing of this codec. */

/** List of all loaded GRF files */
static std::vector<GRFFile> _grf_files;

std::span<const GRFFile> GetAllGRFFiles()
{
	return _grf_files;
}

/** Miscellaneous GRF features, set by Action 0x0D, parameter 0x9E */
GrfMiscBits _misc_grf_features{};

/** Indicates which are the newgrf features currently loaded ingame */
GRFLoadedFeatures _loaded_newgrf_features;

GrfProcessingState _cur_gps;

TypedIndexContainer<std::vector<GRFTempEngineData>, EngineID> _gted;  ///< Temporary engine data used during NewGRF loading

/**
 * Debug() function dedicated to newGRF debugging messages
 * Function is essentially the same as Debug(grf, severity, ...) with the
 * addition of file:line information when parsing grf files.
 * NOTE: for the above reason(s) GrfMsg() should ONLY be used for
 * loading/parsing grf files, not for runtime debug messages as there
 * is no file information available during that time.
 * @param severity debugging severity level, see debug.h
 * @param msg the message
 */
void GrfMsgI(int severity, const std::string &msg)
{
	if (_cur_gps.grfconfig == nullptr) {
		Debug(grf, severity, "{}", msg);
	} else {
		Debug(grf, severity, "[{}:{}] {}", _cur_gps.grfconfig->filename, _cur_gps.nfo_line, msg);
	}
}

/**
 * Obtain a NewGRF file by its grfID
 * @param grfid The grfID to obtain the file for
 * @return The file.
 */
GRFFile *GetFileByGRFID(uint32_t grfid)
{
	auto it = std::ranges::find(_grf_files, grfid, &GRFFile::grfid);
	if (it != std::end(_grf_files)) return &*it;
	return nullptr;
}

/**
 * Obtain a NewGRF file by its filename
 * @param filename The filename to obtain the file for.
 * @return The file.
 */
static GRFFile *GetFileByFilename(const std::string &filename)
{
	auto it = std::ranges::find(_grf_files, filename, &GRFFile::filename);
	if (it != std::end(_grf_files)) return &*it;
	return nullptr;
}

/** Reset all NewGRFData that was used only while processing data */
static void ClearTemporaryNewGRFData(GRFFile *gf)
{
	gf->labels.clear();
}

/**
 * Disable a GRF
 * @param message Error message or STR_NULL.
 * @param config GRFConfig to disable, nullptr for current.
 * @return Error message of the GRF for further customisation.
 */
GRFError *DisableGrf(StringID message, GRFConfig *config)
{
	GRFFile *file;
	if (config != nullptr) {
		file = GetFileByGRFID(config->ident.grfid);
	} else {
		config = _cur_gps.grfconfig;
		file = _cur_gps.grffile;
	}

	config->status = GCS_DISABLED;
	if (file != nullptr) ClearTemporaryNewGRFData(file);
	if (config == _cur_gps.grfconfig) _cur_gps.skip_sprites = -1;

	if (message == STR_NULL) return nullptr;

	auto it = std::ranges::find(config->errors, _cur_gps.nfo_line, &GRFError::nfo_line);
	if (it == std::end(config->errors)) {
		it = config->errors.emplace(it, STR_NEWGRF_ERROR_MSG_FATAL, _cur_gps.nfo_line, message);
	}
	if (config == _cur_gps.grfconfig) it->param_value[0] = _cur_gps.nfo_line;
	return &*it;
}

/**
 * Disable a static NewGRF when it is influencing another (non-static)
 * NewGRF as this could cause desyncs.
 *
 * We could just tell the NewGRF querying that the file doesn't exist,
 * but that might give unwanted results. Disabling the NewGRF gives the
 * best result as no NewGRF author can complain about that.
 * @param c The NewGRF to disable.
 */
void DisableStaticNewGRFInfluencingNonStaticNewGRFs(GRFConfig &c)
{
	GRFError *error = DisableGrf(STR_NEWGRF_ERROR_STATIC_GRF_CAUSES_DESYNC, &c);
	error->data = _cur_gps.grfconfig->GetName();
}

static std::map<uint32_t, uint32_t> _grf_id_overrides;

/**
 * Set the override for a NewGRF
 * @param source_grfid The grfID which wants to override another NewGRF.
 * @param target_grfid The grfID which is being overridden.
 */
void SetNewGRFOverride(uint32_t source_grfid, uint32_t target_grfid)
{
	if (target_grfid == 0) {
		_grf_id_overrides.erase(source_grfid);
		GrfMsg(5, "SetNewGRFOverride: Removed override of 0x{:X}", std::byteswap(source_grfid));
	} else {
		_grf_id_overrides[source_grfid] = target_grfid;
		GrfMsg(5, "SetNewGRFOverride: Added override of 0x{:X} to 0x{:X}", std::byteswap(source_grfid), std::byteswap(target_grfid));
	}
}

/**
 * Get overridden GRF for current GRF if present.
 * @return Overridden GRFFile if present, or nullptr.
 */
GRFFile *GetCurrentGRFOverride()
{
	auto found = _grf_id_overrides.find(_cur_gps.grffile->grfid);
	if (found != std::end(_grf_id_overrides)) {
		GRFFile *grffile = GetFileByGRFID(found->second);
		if (grffile != nullptr) return grffile;
	}
	return nullptr;
}

/**
 * Returns the engine associated to a certain internal_id, resp. allocates it.
 * @param file NewGRF that wants to change the engine.
 * @param type Vehicle type.
 * @param internal_id Engine ID inside the NewGRF.
 * @param static_access If the engine is not present, return nullptr instead of allocating a new engine. (Used for static Action 0x04).
 * @return The requested engine.
 */
Engine *GetNewEngine(const GRFFile *file, VehicleType type, uint16_t internal_id, bool static_access)
{
	/* Hack for add-on GRFs that need to modify another GRF's engines. This lets
	 * them use the same engine slots. */
	uint32_t scope_grfid = INVALID_GRFID; // If not using dynamic_engines, all newgrfs share their ID range
	if (_settings_game.vehicle.dynamic_engines) {
		/* If dynamic_engies is enabled, there can be multiple independent ID ranges. */
		scope_grfid = file->grfid;
		if (auto it = _grf_id_overrides.find(file->grfid); it != std::end(_grf_id_overrides)) {
			scope_grfid = it->second;
			const GRFFile *grf_match = GetFileByGRFID(scope_grfid);
			if (grf_match == nullptr) {
				GrfMsg(5, "Tried mapping from GRFID {:x} to {:x} but target is not loaded", std::byteswap(file->grfid), std::byteswap(scope_grfid));
			} else {
				GrfMsg(5, "Mapping from GRFID {:x} to {:x}", std::byteswap(file->grfid), std::byteswap(scope_grfid));
			}
		}

		/* Check if the engine is registered in the override manager */
		EngineID engine = _engine_mngr.GetID(type, internal_id, scope_grfid);
		if (engine != EngineID::Invalid()) {
			Engine *e = Engine::Get(engine);
			if (!e->grf_prop.HasGrfFile()) {
				e->grf_prop.SetGRFFile(file);
			}
			return e;
		}
	}

	/* Check if there is an unreserved slot */
	EngineID engine = _engine_mngr.UseUnreservedID(type, internal_id, scope_grfid, static_access);
	if (engine != EngineID::Invalid()) {
		Engine *e = Engine::Get(engine);

		if (!e->grf_prop.HasGrfFile()) {
			e->grf_prop.SetGRFFile(file);
			GrfMsg(5, "Replaced engine at index {} for GRFID {:x}, type {}, index {}", e->index, std::byteswap(file->grfid), type, internal_id);
		}

		return e;
	}

	if (static_access) return nullptr;

	if (!Engine::CanAllocateItem()) {
		GrfMsg(0, "Can't allocate any more engines");
		return nullptr;
	}

	size_t engine_pool_size = Engine::GetPoolSize();

	/* ... it's not, so create a new one based off an existing engine */
	Engine *e = new Engine(type, internal_id);
	e->grf_prop.SetGRFFile(file);

	/* Reserve the engine slot */
	_engine_mngr.SetID(type, internal_id, scope_grfid, std::min<uint8_t>(internal_id, _engine_counts[type]), e->index);

	if (engine_pool_size != Engine::GetPoolSize()) {
		/* Resize temporary engine data ... */
		_gted.resize(Engine::GetPoolSize());
	}
	if (type == VEH_TRAIN) {
		_gted[e->index].railtypelabels.clear();
		for (RailType rt : e->VehInfo<RailVehicleInfo>().railtypes) _gted[e->index].railtypelabels.push_back(GetRailTypeInfo(rt)->label);
	}

	GrfMsg(5, "Created new engine at index {} for GRFID {:x}, type {}, index {}", e->index, std::byteswap(file->grfid), type, internal_id);

	return e;
}

/**
 * Return the ID of a new engine
 * @param file The NewGRF file providing the engine.
 * @param type The Vehicle type.
 * @param internal_id NewGRF-internal ID of the engine.
 * @return The new EngineID.
 * @note depending on the dynamic_engine setting and a possible override
 *       property the grfID may be unique or overwriting or partially re-defining
 *       properties of an existing engine.
 */
EngineID GetNewEngineID(const GRFFile *file, VehicleType type, uint16_t internal_id)
{
	uint32_t scope_grfid = INVALID_GRFID; // If not using dynamic_engines, all newgrfs share their ID range
	if (_settings_game.vehicle.dynamic_engines) {
		scope_grfid = file->grfid;
		if (auto it = _grf_id_overrides.find(file->grfid); it != std::end(_grf_id_overrides)) {
			scope_grfid = it->second;
		}
	}

	return _engine_mngr.GetID(type, internal_id, scope_grfid);
}




/**
 * Translate the refit mask. refit_mask is uint32_t as it has not been mapped to CargoTypes.
 */
CargoTypes TranslateRefitMask(uint32_t refit_mask)
{
	CargoTypes result = 0;
	for (uint8_t bit : SetBitIterator(refit_mask)) {
		CargoType cargo = GetCargoTranslation(bit, _cur_gps.grffile, true);
		if (IsValidCargoType(cargo)) SetBit(result, cargo);
	}
	return result;
}

/**
 * Converts TTD(P) Base Price pointers into the enum used by OTTD
 * See http://wiki.ttdpatch.net/tiki-index.php?page=BaseCosts
 * @param base_pointer TTD(P) Base Price Pointer
 * @param error_location Function name for grf error messages
 * @param[out] index If \a base_pointer is valid, \a index is assigned to the matching price; else it is left unchanged
 */
void ConvertTTDBasePrice(uint32_t base_pointer, std::string_view error_location, Price *index)
{
	/* Special value for 'none' */
	if (base_pointer == 0) {
		*index = INVALID_PRICE;
		return;
	}

	static const uint32_t start = 0x4B34; ///< Position of first base price
	static const uint32_t size  = 6;      ///< Size of each base price record

	if (base_pointer < start || (base_pointer - start) % size != 0 || (base_pointer - start) / size >= PR_END) {
		GrfMsg(1, "{}: Unsupported running cost base 0x{:04X}, ignoring", error_location, base_pointer);
		return;
	}

	*index = (Price)((base_pointer - start) / size);
}

/**
 * Get the language map associated with a given NewGRF and language.
 * @param grfid       The NewGRF to get the map for.
 * @param language_id The (NewGRF) language ID to get the map for.
 * @return The LanguageMap, or nullptr if it couldn't be found.
 */
/* static */ const LanguageMap *LanguageMap::GetLanguageMap(uint32_t grfid, uint8_t language_id)
{
	/* LanguageID "MAX_LANG", i.e. 7F is any. This language can't have a gender/case mapping, but has to be handled gracefully. */
	const GRFFile *grffile = GetFileByGRFID(grfid);
	if (grffile == nullptr) return nullptr;

	auto it = grffile->language_map.find(language_id);
	if (it == std::end(grffile->language_map)) return nullptr;

	return &it->second;
}

/**
 * Set the current NewGRF as unsafe for static use
 * @note Used during safety scan on unsafe actions.
 */
void GRFUnsafe(ByteReader &)
{
	_cur_gps.grfconfig->flags.Set(GRFConfigFlag::Unsafe);

	/* Skip remainder of GRF */
	_cur_gps.skip_sprites = -1;
}

/** Reset and clear all NewGRFs */
static void ResetNewGRF()
{
	_cur_gps.grfconfig = nullptr;
	_cur_gps.grffile = nullptr;
	_grf_files.clear();

	/* We store pointers to GRFFiles in many places, so need to ensure that the pointers do not become invalid
	 * due to vector reallocation. Should not happen due to loading taking place in multiple stages, but
	 * reserving when the size is known is good practice anyway. */
	_grf_files.reserve(_grfconfig.size());
}

/** Clear all NewGRF errors */
static void ResetNewGRFErrors()
{
	for (const auto &c : _grfconfig) {
		c->errors.clear();
	}
}

extern void ResetCallbacks(bool final);
extern void ResetGRM();

/**
 * Reset all NewGRF loaded data
 */
void ResetNewGRFData()
{
	CleanUpStrings();
	CleanUpGRFTownNames();

	ResetBadges();

	/* Copy/reset original engine info data */
	SetupEngines();

	/* Copy/reset original bridge info data */
	ResetBridges();

	/* Reset rail type information */
	ResetRailTypes();

	/* Copy/reset original road type info data */
	ResetRoadTypes();

	/* Allocate temporary refit/cargo class data */
	_gted.resize(Engine::GetPoolSize());

	/* Fill rail type label temporary data for default trains */
	for (const Engine *e : Engine::IterateType(VEH_TRAIN)) {
		_gted[e->index].railtypelabels.clear();
		for (RailType rt : e->VehInfo<RailVehicleInfo>().railtypes) _gted[e->index].railtypelabels.push_back(GetRailTypeInfo(rt)->label);
	}

	/* Reset GRM reservations */
	ResetGRM();

	/* Reset generic feature callback lists */
	ResetGenericCallbacks();

	/* Reset price base data */
	ResetPriceBaseMultipliers();

	/* Reset the curencies array */
	ResetCurrencies();

	/* Reset the house array */
	ResetHouses();

	/* Reset the industries structures*/
	ResetIndustries();

	/* Reset the objects. */
	ObjectClass::Reset();
	ResetObjects();

	/* Reset station classes */
	StationClass::Reset();

	/* Reset airport-related structures */
	AirportClass::Reset();
	AirportSpec::ResetAirports();
	AirportTileSpec::ResetAirportTiles();

	/* Reset road stop classes */
	RoadStopClass::Reset();

	/* Reset canal sprite groups and flags */
	_water_feature.fill({});

	ResetFaces();

	/* Reset the snowline table. */
	ClearSnowLine();

	/* Reset NewGRF files */
	ResetNewGRF();

	/* Reset NewGRF errors. */
	ResetNewGRFErrors();

	/* Set up the default cargo types */
	SetupCargoForClimate(_settings_game.game_creation.landscape);

	/* Reset misc GRF features and train list display variables */
	_misc_grf_features = {};

	_loaded_newgrf_features.has_2CC           = false;
	_loaded_newgrf_features.used_liveries     = 1 << LS_DEFAULT;
	_loaded_newgrf_features.shore             = SHORE_REPLACE_NONE;
	_loaded_newgrf_features.tram              = TRAMWAY_REPLACE_DEPOT_NONE;

	/* Clear all GRF overrides */
	_grf_id_overrides.clear();

	InitializeSoundPool();
	_spritegroup_pool.CleanPool();
	ResetCallbacks(false);
}

/**
 * Reset NewGRF data which is stored persistently in savegames.
 */
void ResetPersistentNewGRFData()
{
	/* Reset override managers */
	_engine_mngr.ResetToDefaultMapping();
	_house_mngr.ResetMapping();
	_industry_mngr.ResetMapping();
	_industile_mngr.ResetMapping();
	_airport_mngr.ResetMapping();
	_airporttile_mngr.ResetMapping();
}

/**
 * Get the cargo translation table to use for the given GRF file.
 * @param grffile GRF file.
 * @returns Readonly cargo translation table to use.
 */
 std::span<const CargoLabel> GetCargoTranslationTable(const GRFFile &grffile)
 {
	 /* Always use the translation table if it's installed. */
	 if (!grffile.cargo_list.empty()) return grffile.cargo_list;

	 /* Pre-v7 use climate-dependent "slot" table. */
	 if (grffile.grf_version < 7) return GetClimateDependentCargoTranslationTable();

	 /* Otherwise use climate-independent "bitnum" table. */
	 return GetClimateIndependentCargoTranslationTable();
 }

/**
 * Construct the Cargo Mapping
 * @note This is the reverse of a cargo translation table
 */
static void BuildCargoTranslationMap()
{
	_cur_gps.grffile->cargo_map.fill(UINT8_MAX);

	auto cargo_list = GetCargoTranslationTable(*_cur_gps.grffile);

	for (const CargoSpec *cs : CargoSpec::Iterate()) {
		/* Check the translation table for this cargo's label */
		int idx = find_index(cargo_list, cs->label);
		if (idx >= 0) _cur_gps.grffile->cargo_map[cs->Index()] = idx;
	}
}

/**
 * Prepare loading a NewGRF file with its config
 * @param config The NewGRF configuration struct with name, id, parameters and alike.
 */
static void InitNewGRFFile(const GRFConfig &config)
{
	GRFFile *newfile = GetFileByFilename(config.filename);
	if (newfile != nullptr) {
		/* We already loaded it once. */
		_cur_gps.grffile = newfile;
		return;
	}

	assert(_grf_files.size() < _grf_files.capacity()); // We must not invalidate pointers.
	_cur_gps.grffile = &_grf_files.emplace_back(config);
}

/**
 * Constructor for GRFFile
 * @param config GRFConfig to copy name, grfid and parameters from.
 */
GRFFile::GRFFile(const GRFConfig &config)
{
	this->filename = config.filename;
	this->grfid = config.ident.grfid;

	/* Initialise local settings to defaults */
	this->traininfo_vehicle_pitch = 0;
	this->traininfo_vehicle_width = TRAININFO_DEFAULT_VEHICLE_WIDTH;

	/* Mark price_base_multipliers as 'not set' */
	this->price_base_multipliers.fill(INVALID_PRICE_MODIFIER);

	/* Initialise rail type map with default rail types */
	this->railtype_map.fill(INVALID_RAILTYPE);
	this->railtype_map[0] = RAILTYPE_RAIL;
	this->railtype_map[1] = RAILTYPE_ELECTRIC;
	this->railtype_map[2] = RAILTYPE_MONO;
	this->railtype_map[3] = RAILTYPE_MAGLEV;

	/* Initialise road type map with default road types */
	this->roadtype_map.fill(INVALID_ROADTYPE);
	this->roadtype_map[0] = ROADTYPE_ROAD;

	/* Initialise tram type map with default tram types */
	this->tramtype_map.fill(INVALID_ROADTYPE);
	this->tramtype_map[0] = ROADTYPE_TRAM;

	/* Copy the initial parameter list */
	this->param = config.param;
}

/* Some compilers get confused about vectors of unique_ptrs. */
GRFFile::GRFFile() = default;
GRFFile::GRFFile(GRFFile &&other) = default;
GRFFile::~GRFFile() = default;

/**
 * Find first cargo label that exists and is active from a list of cargo labels.
 * @param labels List of cargo labels.
 * @returns First cargo label in list that exists, or CT_INVALID if none exist.
 */
static CargoLabel GetActiveCargoLabel(const std::initializer_list<CargoLabel> &labels)
{
	for (const CargoLabel &label : labels) {
		CargoType cargo_type = GetCargoTypeByLabel(label);
		if (cargo_type != INVALID_CARGO) return label;
	}
	return CT_INVALID;
}

/**
 * Get active cargo label from either a cargo label or climate-dependent mixed cargo type.
 * @param label Cargo label or climate-dependent mixed cargo type.
 * @returns Active cargo label, or CT_INVALID if cargo label is not active.
 */
static CargoLabel GetActiveCargoLabel(const std::variant<CargoLabel, MixedCargoType> &label)
{
	struct visitor {
		CargoLabel operator()(const CargoLabel &label) { return label; }
		CargoLabel operator()(const MixedCargoType &mixed)
		{
			switch (mixed) {
				case MCT_LIVESTOCK_FRUIT: return GetActiveCargoLabel({CT_LIVESTOCK, CT_FRUIT});
				case MCT_GRAIN_WHEAT_MAIZE: return GetActiveCargoLabel({CT_GRAIN, CT_WHEAT, CT_MAIZE});
				case MCT_VALUABLES_GOLD_DIAMONDS: return GetActiveCargoLabel({CT_VALUABLES, CT_GOLD, CT_DIAMONDS});
				default: NOT_REACHED();
			}
		}
	};

	return std::visit(visitor{}, label);
}

/**
 * Precalculate refit masks from cargo classes for all vehicles.
 */
static void CalculateRefitMasks()
{
	CargoTypes original_known_cargoes = 0;
	for (CargoType cargo_type = 0; cargo_type != NUM_CARGO; ++cargo_type) {
		if (IsDefaultCargo(cargo_type)) SetBit(original_known_cargoes, cargo_type);
	}

	for (Engine *e : Engine::Iterate()) {
		EngineID engine = e->index;
		EngineInfo *ei = &e->info;
		bool only_defaultcargo; ///< Set if the vehicle shall carry only the default cargo

		/* Apply default cargo translation map if cargo type hasn't been set, either explicitly or by aircraft cargo handling. */
		if (!IsValidCargoType(e->info.cargo_type)) {
			e->info.cargo_type = GetCargoTypeByLabel(GetActiveCargoLabel(e->info.cargo_label));
		}

		/* If the NewGRF did not set any cargo properties, we apply default values. */
		if (_gted[engine].defaultcargo_grf == nullptr) {
			/* If the vehicle has any capacity, apply the default refit masks */
			if (e->type != VEH_TRAIN || e->VehInfo<RailVehicleInfo>().capacity != 0) {
				static constexpr LandscapeType T = LandscapeType::Temperate;
				static constexpr LandscapeType A = LandscapeType::Arctic;
				static constexpr LandscapeType S = LandscapeType::Tropic;
				static constexpr LandscapeType Y = LandscapeType::Toyland;
				static const struct DefaultRefitMasks {
					LandscapeTypes climate;
					CargoLabel cargo_label;
					CargoClasses cargo_allowed;
					CargoClasses cargo_disallowed;
				} _default_refit_masks[] = {
					{{T, A, S, Y}, CT_PASSENGERS, {CargoClass::Passengers},                      {}},
					{{T, A, S   }, CT_MAIL,       {CargoClass::Mail},                            {}},
					{{T, A, S   }, CT_VALUABLES,  {CargoClass::Armoured},                        {CargoClass::Liquid}},
					{{         Y}, CT_MAIL,       {CargoClass::Mail, CargoClass::Armoured},      {CargoClass::Liquid}},
					{{T, A      }, CT_COAL,       {CargoClass::Bulk},                            {}},
					{{      S   }, CT_COPPER_ORE, {CargoClass::Bulk},                            {}},
					{{         Y}, CT_SUGAR,      {CargoClass::Bulk},                            {}},
					{{T, A, S   }, CT_OIL,        {CargoClass::Liquid},                          {}},
					{{         Y}, CT_COLA,       {CargoClass::Liquid},                          {}},
					{{T         }, CT_GOODS,      {CargoClass::PieceGoods, CargoClass::Express}, {CargoClass::Liquid, CargoClass::Passengers}},
					{{   A, S   }, CT_GOODS,      {CargoClass::PieceGoods, CargoClass::Express}, {CargoClass::Liquid, CargoClass::Passengers, CargoClass::Refrigerated}},
					{{   A, S   }, CT_FOOD,       {CargoClass::Refrigerated},                    {}},
					{{         Y}, CT_CANDY,      {CargoClass::PieceGoods, CargoClass::Express}, {CargoClass::Liquid, CargoClass::Passengers}},
				};

				if (e->type == VEH_AIRCRAFT) {
					/* Aircraft default to "light" cargoes */
					_gted[engine].cargo_allowed = {CargoClass::Passengers, CargoClass::Mail, CargoClass::Armoured, CargoClass::Express};
					_gted[engine].cargo_disallowed = {CargoClass::Liquid};
				} else if (e->type == VEH_SHIP) {
					CargoLabel label = GetActiveCargoLabel(ei->cargo_label);
					switch (label.base()) {
						case CT_PASSENGERS.base():
							/* Ferries */
							_gted[engine].cargo_allowed = {CargoClass::Passengers};
							_gted[engine].cargo_disallowed = {};
							break;
						case CT_OIL.base():
							/* Tankers */
							_gted[engine].cargo_allowed = {CargoClass::Liquid};
							_gted[engine].cargo_disallowed = {};
							break;
						default:
							/* Cargo ships */
							if (_settings_game.game_creation.landscape == LandscapeType::Toyland) {
								/* No tanker in toyland :( */
								_gted[engine].cargo_allowed = {CargoClass::Mail, CargoClass::Armoured, CargoClass::Express, CargoClass::Bulk, CargoClass::PieceGoods, CargoClass::Liquid};
								_gted[engine].cargo_disallowed = {CargoClass::Passengers};
							} else {
								_gted[engine].cargo_allowed = {CargoClass::Mail, CargoClass::Armoured, CargoClass::Express, CargoClass::Bulk, CargoClass::PieceGoods};
								_gted[engine].cargo_disallowed = {CargoClass::Liquid, CargoClass::Passengers};
							}
							break;
					}
					e->VehInfo<ShipVehicleInfo>().old_refittable = true;
				} else if (e->type == VEH_TRAIN && e->VehInfo<RailVehicleInfo>().railveh_type != RAILVEH_WAGON) {
					/* Train engines default to all cargoes, so you can build single-cargo consists with fast engines.
					 * Trains loading multiple cargoes may start stations accepting unwanted cargoes. */
					_gted[engine].cargo_allowed = {CargoClass::Passengers, CargoClass::Mail, CargoClass::Armoured, CargoClass::Express, CargoClass::Bulk, CargoClass::PieceGoods, CargoClass::Liquid};
					_gted[engine].cargo_disallowed = {};
				} else {
					/* Train wagons and road vehicles are classified by their default cargo type */
					CargoLabel label = GetActiveCargoLabel(ei->cargo_label);
					for (const auto &drm : _default_refit_masks) {
						if (!drm.climate.Test(_settings_game.game_creation.landscape)) continue;
						if (drm.cargo_label != label) continue;

						_gted[engine].cargo_allowed = drm.cargo_allowed;
						_gted[engine].cargo_disallowed = drm.cargo_disallowed;
						break;
					}

					/* All original cargoes have specialised vehicles, so exclude them */
					_gted[engine].ctt_exclude_mask = original_known_cargoes;
				}
			}
			_gted[engine].UpdateRefittability(_gted[engine].cargo_allowed.Any());

			if (IsValidCargoType(ei->cargo_type)) ClrBit(_gted[engine].ctt_exclude_mask, ei->cargo_type);
		}

		/* Compute refittability */
		{
			CargoTypes mask = 0;
			CargoTypes not_mask = 0;
			CargoTypes xor_mask = ei->refit_mask;

			/* If the original masks set by the grf are zero, the vehicle shall only carry the default cargo.
			 * Note: After applying the translations, the vehicle may end up carrying no defined cargo. It becomes unavailable in that case. */
			only_defaultcargo = _gted[engine].refittability != GRFTempEngineData::NONEMPTY;

			if (_gted[engine].cargo_allowed.Any()) {
				/* Build up the list of cargo types from the set cargo classes. */
				for (const CargoSpec *cs : CargoSpec::Iterate()) {
					if (cs->classes.Any(_gted[engine].cargo_allowed) && cs->classes.All(_gted[engine].cargo_allowed_required)) SetBit(mask, cs->Index());
					if (cs->classes.Any(_gted[engine].cargo_disallowed)) SetBit(not_mask, cs->Index());
				}
			}

			ei->refit_mask = ((mask & ~not_mask) ^ xor_mask) & _cargo_mask;

			/* Apply explicit refit includes/excludes. */
			ei->refit_mask |= _gted[engine].ctt_include_mask;
			ei->refit_mask &= ~_gted[engine].ctt_exclude_mask;

			/* Custom refit mask callback. */
			const GRFFile *file = _gted[e->index].defaultcargo_grf;
			if (file == nullptr) file = e->GetGRF();
			if (file != nullptr && e->info.callback_mask.Test(VehicleCallbackMask::CustomRefit)) {
				for (const CargoSpec *cs : CargoSpec::Iterate()) {
					uint8_t local_slot = file->cargo_map[cs->Index()];
					uint16_t callback = GetVehicleCallback(CBID_VEHICLE_CUSTOM_REFIT, cs->classes.base(), local_slot, engine, nullptr);
					switch (callback) {
						case CALLBACK_FAILED:
						case 0:
							break; // Do nothing.
						case 1: SetBit(ei->refit_mask, cs->Index()); break;
						case 2: ClrBit(ei->refit_mask, cs->Index()); break;

						default: ErrorUnknownCallbackResult(file->grfid, CBID_VEHICLE_CUSTOM_REFIT, callback);
					}
				}
			}
		}

		/* Clear invalid cargoslots (from default vehicles or pre-NewCargo GRFs) */
		if (IsValidCargoType(ei->cargo_type) && !HasBit(_cargo_mask, ei->cargo_type)) ei->cargo_type = INVALID_CARGO;

		/* Ensure that the vehicle is either not refittable, or that the default cargo is one of the refittable cargoes.
		 * Note: Vehicles refittable to no cargo are handle differently to vehicle refittable to a single cargo. The latter might have subtypes. */
		if (!only_defaultcargo && (e->type != VEH_SHIP || e->VehInfo<ShipVehicleInfo>().old_refittable) && IsValidCargoType(ei->cargo_type) && !HasBit(ei->refit_mask, ei->cargo_type)) {
			ei->cargo_type = INVALID_CARGO;
		}

		/* Check if this engine's cargo type is valid. If not, set to the first refittable
		 * cargo type. Finally disable the vehicle, if there is still no cargo. */
		if (!IsValidCargoType(ei->cargo_type) && ei->refit_mask != 0) {
			/* Figure out which CTT to use for the default cargo, if it is 'first refittable'. */
			const GRFFile *file = _gted[engine].defaultcargo_grf;
			if (file == nullptr) file = e->GetGRF();
			if (file != nullptr && file->grf_version >= 8 && !file->cargo_list.empty()) {
				/* Use first refittable cargo from cargo translation table */
				uint8_t best_local_slot = UINT8_MAX;
				for (CargoType cargo_type : SetCargoBitIterator(ei->refit_mask)) {
					uint8_t local_slot = file->cargo_map[cargo_type];
					if (local_slot < best_local_slot) {
						best_local_slot = local_slot;
						ei->cargo_type = cargo_type;
					}
				}
			}

			if (!IsValidCargoType(ei->cargo_type)) {
				/* Use first refittable cargo slot */
				ei->cargo_type = (CargoType)FindFirstBit(ei->refit_mask);
			}
		}
		if (!IsValidCargoType(ei->cargo_type) && e->type == VEH_TRAIN && e->VehInfo<RailVehicleInfo>().railveh_type != RAILVEH_WAGON && e->VehInfo<RailVehicleInfo>().capacity == 0) {
			/* For train engines which do not carry cargo it does not matter if their cargo type is invalid.
			 * Fallback to the first available instead, if the cargo type has not been changed (as indicated by
			 * cargo_label not being CT_INVALID). */
			if (GetActiveCargoLabel(ei->cargo_label) != CT_INVALID) {
				ei->cargo_type = static_cast<CargoType>(FindFirstBit(_standard_cargo_mask));
			}
		}
		if (!IsValidCargoType(ei->cargo_type)) ei->climates = {};

		/* Clear refit_mask for not refittable ships */
		if (e->type == VEH_SHIP && !e->VehInfo<ShipVehicleInfo>().old_refittable) {
			ei->refit_mask = 0;
		}
	}
}

/** Set to use the correct action0 properties for each canal feature */
static void FinaliseCanals()
{
	for (uint i = 0; i < CF_END; i++) {
		if (_water_feature[i].grffile != nullptr) {
			_water_feature[i].callback_mask = _water_feature[i].grffile->canal_local_properties[i].callback_mask;
			_water_feature[i].flags = _water_feature[i].grffile->canal_local_properties[i].flags;
		}
	}
}

/** Check for invalid engines */
static void FinaliseEngineArray()
{
	for (Engine *e : Engine::Iterate()) {
		if (e->GetGRF() == nullptr) {
			auto found = std::ranges::find(_engine_mngr.mappings[e->type], e->index, &EngineIDMapping::engine);
			if (found == std::end(_engine_mngr.mappings[e->type]) || found->grfid != INVALID_GRFID || found->internal_id != found->substitute_id) {
				e->info.string_id = STR_NEWGRF_INVALID_ENGINE;
			}
		}

		/* Do final mapping on variant engine ID. */
		if (e->info.variant_id != EngineID::Invalid()) {
			e->info.variant_id = GetNewEngineID(e->grf_prop.grffile, e->type, e->info.variant_id.base());
		}

		if (!e->info.climates.Test(_settings_game.game_creation.landscape)) continue;

		switch (e->type) {
			case VEH_TRAIN:
				for (RailType rt : e->VehInfo<RailVehicleInfo>().railtypes) {
					AppendCopyableBadgeList(e->badges, GetRailTypeInfo(rt)->badges, GSF_TRAINS);
				}
				break;
			case VEH_ROAD: AppendCopyableBadgeList(e->badges, GetRoadTypeInfo(e->VehInfo<RoadVehicleInfo>().roadtype)->badges, GSF_ROADVEHICLES); break;
			default: break;
		}

		/* Skip wagons, there livery is defined via the engine */
		if (e->type != VEH_TRAIN || e->VehInfo<RailVehicleInfo>().railveh_type != RAILVEH_WAGON) {
			LiveryScheme ls = GetEngineLiveryScheme(e->index, EngineID::Invalid(), nullptr);
			SetBit(_loaded_newgrf_features.used_liveries, ls);
			/* Note: For ships and roadvehicles we assume that they cannot be refitted between passenger and freight */

			if (e->type == VEH_TRAIN) {
				SetBit(_loaded_newgrf_features.used_liveries, LS_FREIGHT_WAGON);
				switch (ls) {
					case LS_STEAM:
					case LS_DIESEL:
					case LS_ELECTRIC:
					case LS_MONORAIL:
					case LS_MAGLEV:
						SetBit(_loaded_newgrf_features.used_liveries, LS_PASSENGER_WAGON_STEAM + ls - LS_STEAM);
						break;

					case LS_DMU:
					case LS_EMU:
						SetBit(_loaded_newgrf_features.used_liveries, LS_PASSENGER_WAGON_DIESEL + ls - LS_DMU);
						break;

					default: NOT_REACHED();
				}
			}
		}
	}

	/* Check engine variants don't point back on themselves (either directly or via a loop) then set appropriate flags
	 * on variant engine. This is performed separately as all variant engines need to have been resolved.
	 * Use Floyd's cycle-detection algorithm to handle the case where a cycle is present but does
	 * not include the starting engine ID. */
	for (Engine *e : Engine::Iterate()) {
		EngineID parent = e->info.variant_id;
		EngineID parent_slow = parent;
		bool update_slow = false;
		while (parent != EngineID::Invalid()) {
			parent = Engine::Get(parent)->info.variant_id;
			if (update_slow) parent_slow = Engine::Get(parent_slow)->info.variant_id;
			update_slow = !update_slow;
			if (parent != e->index && parent != parent_slow) continue;

			/* Engine looped back on itself, so clear the variant. */
			e->info.variant_id = EngineID::Invalid();

			GrfMsg(1, "FinaliseEngineArray: Variant of engine {:x} in '{}' loops back on itself", e->grf_prop.local_id, e->GetGRF()->filename);
			break;
		}

		if (e->info.variant_id != EngineID::Invalid()) {
			Engine::Get(e->info.variant_id)->display_flags.Set({EngineDisplayFlag::HasVariants, EngineDisplayFlag::IsFolded});
		}
	}
}

/** Check for invalid cargoes */
void FinaliseCargoArray()
{
	for (CargoSpec &cs : CargoSpec::array) {
		if (cs.town_production_effect == INVALID_TPE) {
			/* Set default town production effect by cargo label. */
			switch (cs.label.base()) {
				case CT_PASSENGERS.base(): cs.town_production_effect = TPE_PASSENGERS; break;
				case CT_MAIL.base():       cs.town_production_effect = TPE_MAIL; break;
				default:                   cs.town_production_effect = TPE_NONE; break;
			}
		}
		if (!cs.IsValid()) {
			cs.name = cs.name_single = cs.units_volume = STR_NEWGRF_INVALID_CARGO;
			cs.quantifier = STR_NEWGRF_INVALID_CARGO_QUANTITY;
			cs.abbrev = STR_NEWGRF_INVALID_CARGO_ABBREV;
		}
	}
}

/**
 * Check if a given housespec is valid and disable it if it's not.
 * The housespecs that follow it are used to check the validity of
 * multitile houses.
 * @param hs The housespec to check.
 * @param next1 The housespec that follows \c hs.
 * @param next2 The housespec that follows \c next1.
 * @param next3 The housespec that follows \c next2.
 * @param filename The filename of the newgrf this house was defined in.
 * @return Whether the given housespec is valid.
 */
static bool IsHouseSpecValid(HouseSpec &hs, const HouseSpec *next1, const HouseSpec *next2, const HouseSpec *next3, const std::string &filename)
{
	if ((hs.building_flags.Any(BUILDING_HAS_2_TILES) &&
				(next1 == nullptr || !next1->enabled || next1->building_flags.Any(BUILDING_HAS_1_TILE))) ||
			(hs.building_flags.Any(BUILDING_HAS_4_TILES) &&
				(next2 == nullptr || !next2->enabled || next2->building_flags.Any(BUILDING_HAS_1_TILE) ||
				next3 == nullptr || !next3->enabled || next3->building_flags.Any(BUILDING_HAS_1_TILE)))) {
		hs.enabled = false;
		if (!filename.empty()) Debug(grf, 1, "FinaliseHouseArray: {} defines house {} as multitile, but no suitable tiles follow. Disabling house.", filename, hs.grf_prop.local_id);
		return false;
	}

	/* Some places sum population by only counting north tiles. Other places use all tiles causing desyncs.
	 * As the newgrf specs define population to be zero for non-north tiles, we just disable the offending house.
	 * If you want to allow non-zero populations somewhen, make sure to sum the population of all tiles in all places. */
	if ((hs.building_flags.Any(BUILDING_HAS_2_TILES) && next1->population != 0) ||
			(hs.building_flags.Any(BUILDING_HAS_4_TILES) && (next2->population != 0 || next3->population != 0))) {
		hs.enabled = false;
		if (!filename.empty()) Debug(grf, 1, "FinaliseHouseArray: {} defines multitile house {} with non-zero population on additional tiles. Disabling house.", filename, hs.grf_prop.local_id);
		return false;
	}

	/* Substitute type is also used for override, and having an override with a different size causes crashes.
	 * This check should only be done for NewGRF houses because grf_prop.subst_id is not set for original houses.*/
	if (!filename.empty() && (hs.building_flags & BUILDING_HAS_1_TILE) != (HouseSpec::Get(hs.grf_prop.subst_id)->building_flags & BUILDING_HAS_1_TILE)) {
		hs.enabled = false;
		Debug(grf, 1, "FinaliseHouseArray: {} defines house {} with different house size then it's substitute type. Disabling house.", filename, hs.grf_prop.local_id);
		return false;
	}

	/* Make sure that additional parts of multitile houses are not available. */
	if (!hs.building_flags.Any(BUILDING_HAS_1_TILE) && hs.building_availability.Any(HZ_ZONE_ALL) && hs.building_availability.Any(HZ_CLIMATE_ALL)) {
		hs.enabled = false;
		if (!filename.empty()) Debug(grf, 1, "FinaliseHouseArray: {} defines house {} without a size but marked it as available. Disabling house.", filename, hs.grf_prop.local_id);
		return false;
	}

	return true;
}

/**
 * Make sure there is at least one house available in the year 0 for the given
 * climate / housezone combination.
 * @param bitmask The climate and housezone to check for. Exactly one climate
 *   bit and one housezone bit should be set.
 */
static void EnsureEarlyHouse(HouseZones bitmask)
{
	TimerGameCalendar::Year min_year = CalendarTime::MAX_YEAR;

	for (const auto &hs : HouseSpec::Specs()) {
		if (!hs.enabled) continue;
		if (!hs.building_availability.All(bitmask)) continue;
		if (hs.min_year < min_year) min_year = hs.min_year;
	}

	if (min_year == 0) return;

	for (auto &hs : HouseSpec::Specs()) {
		if (!hs.enabled) continue;
		if (!hs.building_availability.All(bitmask)) continue;
		if (hs.min_year == min_year) hs.min_year = CalendarTime::MIN_YEAR;
	}
}

/**
 * Add all new houses to the house array. House properties can be set at any
 * time in the GRF file, so we can only add a house spec to the house array
 * after the file has finished loading. We also need to check the dates, due to
 * the TTDPatch behaviour described below that we need to emulate.
 */
static void FinaliseHouseArray()
{
	/* If there are no houses with start dates before 1930, then all houses
	 * with start dates of 1930 have them reset to 0. This is in order to be
	 * compatible with TTDPatch, where if no houses have start dates before
	 * 1930 and the date is before 1930, the game pretends that this is 1930.
	 * If there have been any houses defined with start dates before 1930 then
	 * the dates are left alone.
	 * On the other hand, why 1930? Just 'fix' the houses with the lowest
	 * minimum introduction date to 0.
	 */
	for (auto &file : _grf_files) {
		if (file.housespec.empty()) continue;

		size_t num_houses = file.housespec.size();
		for (size_t i = 0; i < num_houses; i++) {
			auto &hs = file.housespec[i];

			if (hs == nullptr) continue;

			const HouseSpec *next1 = (i + 1 < num_houses ? file.housespec[i + 1].get() : nullptr);
			const HouseSpec *next2 = (i + 2 < num_houses ? file.housespec[i + 2].get() : nullptr);
			const HouseSpec *next3 = (i + 3 < num_houses ? file.housespec[i + 3].get() : nullptr);

			if (!IsHouseSpecValid(*hs, next1, next2, next3, file.filename)) continue;

			_house_mngr.SetEntitySpec(std::move(*hs));
		}

		/* Won't be used again */
		file.housespec.clear();
		file.housespec.shrink_to_fit();
	}

	for (size_t i = 0; i < HouseSpec::Specs().size(); i++) {
		HouseSpec *hs = HouseSpec::Get(i);
		const HouseSpec *next1 = (i + 1 < NUM_HOUSES ? HouseSpec::Get(i + 1) : nullptr);
		const HouseSpec *next2 = (i + 2 < NUM_HOUSES ? HouseSpec::Get(i + 2) : nullptr);
		const HouseSpec *next3 = (i + 3 < NUM_HOUSES ? HouseSpec::Get(i + 3) : nullptr);

		/* We need to check all houses again to we are sure that multitile houses
		 * did get consecutive IDs and none of the parts are missing. */
		if (!IsHouseSpecValid(*hs, next1, next2, next3, std::string{})) {
			/* GetHouseNorthPart checks 3 houses that are directly before
			 * it in the house pool. If any of those houses have multi-tile
			 * flags set it assumes it's part of a multitile house. Since
			 * we can have invalid houses in the pool marked as disabled, we
			 * don't want to have them influencing valid tiles. As such set
			 * building_flags to zero here to make sure any house following
			 * this one in the pool is properly handled as 1x1 house. */
			hs->building_flags = {};
		}

		/* Apply default cargo translation map for unset cargo slots */
		for (uint i = 0; i < lengthof(hs->accepts_cargo_label); ++i) {
			if (!IsValidCargoType(hs->accepts_cargo[i])) hs->accepts_cargo[i] = GetCargoTypeByLabel(hs->accepts_cargo_label[i]);
			/* Disable acceptance if cargo type is invalid. */
			if (!IsValidCargoType(hs->accepts_cargo[i])) hs->cargo_acceptance[i] = 0;
		}
	}

	HouseZones climate_mask = GetClimateMaskForLandscape();
	for (HouseZone climate : climate_mask) {
		for (HouseZone zone : HZ_ZONE_ALL) {
			EnsureEarlyHouse({climate, zone});
		}
	}
}

/**
 * Add all new industries to the industry array. Industry properties can be set at any
 * time in the GRF file, so we can only add a industry spec to the industry array
 * after the file has finished loading.
 */
static void FinaliseIndustriesArray()
{
	for (auto &file : _grf_files) {
		for (auto &indsp : file.industryspec) {
			if (indsp == nullptr || !indsp->enabled) continue;

			_industry_mngr.SetEntitySpec(std::move(*indsp));
		}

		for (auto &indtsp : file.indtspec) {
			if (indtsp != nullptr) {
				_industile_mngr.SetEntitySpec(std::move(*indtsp));
			}
		}

		/* Won't be used again */
		file.industryspec.clear();
		file.industryspec.shrink_to_fit();
		file.indtspec.clear();
		file.indtspec.shrink_to_fit();
	}

	for (auto &indsp : _industry_specs) {
		if (indsp.enabled && indsp.grf_prop.HasGrfFile()) {
			for (auto &conflicting : indsp.conflicting) {
				conflicting = MapNewGRFIndustryType(conflicting, indsp.grf_prop.grfid);
			}
		}
		if (!indsp.enabled) {
			indsp.name = STR_NEWGRF_INVALID_INDUSTRYTYPE;
		}

		/* Apply default cargo translation map for unset cargo slots */
		for (size_t i = 0; i < std::size(indsp.produced_cargo_label); ++i) {
			if (!IsValidCargoType(indsp.produced_cargo[i])) indsp.produced_cargo[i] = GetCargoTypeByLabel(GetActiveCargoLabel(indsp.produced_cargo_label[i]));
		}
		for (size_t i = 0; i < std::size(indsp.accepts_cargo_label); ++i) {
			if (!IsValidCargoType(indsp.accepts_cargo[i])) indsp.accepts_cargo[i] = GetCargoTypeByLabel(GetActiveCargoLabel(indsp.accepts_cargo_label[i]));
		}
	}

	for (auto &indtsp : _industry_tile_specs) {
		/* Apply default cargo translation map for unset cargo slots */
		for (size_t i = 0; i < std::size(indtsp.accepts_cargo_label); ++i) {
			if (!IsValidCargoType(indtsp.accepts_cargo[i])) indtsp.accepts_cargo[i] = GetCargoTypeByLabel(GetActiveCargoLabel(indtsp.accepts_cargo_label[i]));
		}
	}
}

/**
 * Add all new objects to the object array. Object properties can be set at any
 * time in the GRF file, so we can only add an object spec to the object array
 * after the file has finished loading.
 */
static void FinaliseObjectsArray()
{
	for (auto &file : _grf_files) {
		for (auto &objectspec : file.objectspec) {
			if (objectspec != nullptr && objectspec->grf_prop.HasGrfFile() && objectspec->IsEnabled()) {
				_object_mngr.SetEntitySpec(std::move(*objectspec));
			}
		}

		/* Won't be used again */
		file.objectspec.clear();
		file.objectspec.shrink_to_fit();
	}

	ObjectSpec::BindToClasses();
}

/**
 * Add all new airports to the airport array. Airport properties can be set at any
 * time in the GRF file, so we can only add a airport spec to the airport array
 * after the file has finished loading.
 */
static void FinaliseAirportsArray()
{
	for (auto &file : _grf_files) {
		for (auto &as : file.airportspec) {
			if (as != nullptr && as->enabled) {
				_airport_mngr.SetEntitySpec(std::move(*as));
			}
		}

		for (auto &ats : file.airtspec) {
			if (ats != nullptr && ats->enabled) {
				_airporttile_mngr.SetEntitySpec(std::move(*ats));
			}
		}

		/* Won't be used again */
		file.airportspec.clear();
		file.airportspec.shrink_to_fit();
		file.airtspec.clear();
		file.airtspec.shrink_to_fit();
	}
}

/** Helper class to invoke a GrfActionHandler. */
struct InvokeGrfActionHandler {
	template <uint8_t TAction>
	static void Invoke(ByteReader &buf, GrfLoadingStage stage)
	{
		switch (stage) {
			case GLS_FILESCAN: GrfActionHandler<TAction>::FileScan(buf); break;
			case GLS_SAFETYSCAN: GrfActionHandler<TAction>::SafetyScan(buf); break;
			case GLS_LABELSCAN: GrfActionHandler<TAction>::LabelScan(buf); break;
			case GLS_INIT: GrfActionHandler<TAction>::Init(buf); break;
			case GLS_RESERVE: GrfActionHandler<TAction>::Reserve(buf); break;
			case GLS_ACTIVATION: GrfActionHandler<TAction>::Activation(buf); break;
			default: NOT_REACHED();
		}
	}

	using Invoker = void(*)(ByteReader &buf, GrfLoadingStage stage);
	static constexpr Invoker funcs[] = { // Must be listed in action order.
		Invoke<0x00>, Invoke<0x01>, Invoke<0x02>, Invoke<0x03>, Invoke<0x04>, Invoke<0x05>, Invoke<0x06>, Invoke<0x07>,
		Invoke<0x08>, Invoke<0x09>, Invoke<0x0A>, Invoke<0x0B>, Invoke<0x0C>, Invoke<0x0D>, Invoke<0x0E>, Invoke<0x0F>,
		Invoke<0x10>, Invoke<0x11>, Invoke<0x12>, Invoke<0x13>, Invoke<0x14>,
	};

	static void Invoke(uint8_t action, GrfLoadingStage stage, ByteReader &buf)
	{
		Invoker func = action < std::size(funcs) ? funcs[action] : nullptr;
		if (func == nullptr) {
			GrfMsg(7, "DecodeSpecialSprite: Skipping unknown action 0x{:02X}", action);
		} else {
			GrfMsg(7, "DecodeSpecialSprite: Handling action 0x{:02X} in stage {}", action, stage);
			func(buf, stage);
		}
	}
};

/* Here we perform initial decoding of some special sprites (as are they
 * described at http://www.ttdpatch.net/src/newgrf.txt, but this is only a very
 * partial implementation yet).
 * XXX: We consider GRF files trusted. It would be trivial to exploit OTTD by
 * a crafted invalid GRF file. We should tell that to the user somehow, or
 * better make this more robust in the future. */
static void DecodeSpecialSprite(ReusableBuffer<uint8_t> &allocator, uint num, GrfLoadingStage stage)
{
	uint8_t *buf;
	auto it = _grf_line_to_action6_sprite_override.find({_cur_gps.grfconfig->ident.grfid, _cur_gps.nfo_line});
	if (it == _grf_line_to_action6_sprite_override.end()) {
		/* No preloaded sprite to work with; read the
		 * pseudo sprite content. */
		buf = allocator.Allocate(num);
		_cur_gps.file->ReadBlock(buf, num);
	} else {
		/* Use the preloaded sprite data. */
		buf = it->second.data();
		assert(it->second.size() == num);
		GrfMsg(7, "DecodeSpecialSprite: Using preloaded pseudo sprite data");

		/* Skip the real (original) content of this action. */
		_cur_gps.file->SeekTo(num, SEEK_CUR);
	}

	ByteReader br(buf, num);

	try {
		uint8_t action = br.ReadByte();

		if (action == 0xFF) {
			GrfMsg(2, "DecodeSpecialSprite: Unexpected data block, skipping");
		} else if (action == 0xFE) {
			GrfMsg(2, "DecodeSpecialSprite: Unexpected import block, skipping");
		} else {
			InvokeGrfActionHandler::Invoke(action, stage, br);
		}
	} catch (...) {
		GrfMsg(1, "DecodeSpecialSprite: Tried to read past end of pseudo-sprite data");
		DisableGrf(STR_NEWGRF_ERROR_READ_BOUNDS);
	}
}

/**
 * Load a particular NewGRF from a SpriteFile.
 * @param config The configuration of the to be loaded NewGRF.
 * @param stage  The loading stage of the NewGRF.
 * @param file   The file to load the GRF data from.
 */
static void LoadNewGRFFileFromFile(GRFConfig &config, GrfLoadingStage stage, SpriteFile &file)
{
	AutoRestoreBackup cur_file(_cur_gps.file, &file);
	AutoRestoreBackup cur_config(_cur_gps.grfconfig, &config);

	Debug(grf, 2, "LoadNewGRFFile: Reading NewGRF-file '{}'", config.filename);

	uint8_t grf_container_version = file.GetContainerVersion();
	if (grf_container_version == 0) {
		Debug(grf, 7, "LoadNewGRFFile: Custom .grf has invalid format");
		return;
	}

	if (stage == GLS_INIT || stage == GLS_ACTIVATION) {
		/* We need the sprite offsets in the init stage for NewGRF sounds
		 * and in the activation stage for real sprites. */
		ReadGRFSpriteOffsets(file);
	} else {
		/* Skip sprite section offset if present. */
		if (grf_container_version >= 2) file.ReadDword();
	}

	if (grf_container_version >= 2) {
		/* Read compression value. */
		uint8_t compression = file.ReadByte();
		if (compression != 0) {
			Debug(grf, 7, "LoadNewGRFFile: Unsupported compression format");
			return;
		}
	}

	/* Skip the first sprite; we don't care about how many sprites this
	 * does contain; newest TTDPatches and George's longvehicles don't
	 * neither, apparently. */
	uint32_t num = grf_container_version >= 2 ? file.ReadDword() : file.ReadWord();
	if (num == 4 && file.ReadByte() == 0xFF) {
		file.ReadDword();
	} else {
		Debug(grf, 7, "LoadNewGRFFile: Custom .grf has invalid format");
		return;
	}

	_cur_gps.ClearDataForNextFile();

	ReusableBuffer<uint8_t> allocator;

	while ((num = (grf_container_version >= 2 ? file.ReadDword() : file.ReadWord())) != 0) {
		uint8_t type = file.ReadByte();
		_cur_gps.nfo_line++;

		if (type == 0xFF) {
			if (_cur_gps.skip_sprites == 0) {
				/* Limit the special sprites to 1 MiB. */
				if (num > 1024 * 1024) {
					GrfMsg(0, "LoadNewGRFFile: Unexpectedly large sprite, disabling");
					DisableGrf(STR_NEWGRF_ERROR_UNEXPECTED_SPRITE);
					break;
				}

				DecodeSpecialSprite(allocator, num, stage);

				/* Stop all processing if we are to skip the remaining sprites */
				if (_cur_gps.skip_sprites == -1) break;

				continue;
			} else {
				file.SkipBytes(num);
			}
		} else {
			if (_cur_gps.skip_sprites == 0) {
				GrfMsg(0, "LoadNewGRFFile: Unexpected sprite, disabling");
				DisableGrf(STR_NEWGRF_ERROR_UNEXPECTED_SPRITE);
				break;
			}

			if (grf_container_version >= 2 && type == 0xFD) {
				/* Reference to data section. Container version >= 2 only. */
				file.SkipBytes(num);
			} else {
				file.SkipBytes(7);
				SkipSpriteData(file, type, num - 8);
			}
		}

		if (_cur_gps.skip_sprites > 0) _cur_gps.skip_sprites--;
	}
}

/**
 * Load a particular NewGRF.
 * @param config     The configuration of the to be loaded NewGRF.
 * @param stage      The loading stage of the NewGRF.
 * @param subdir     The sub directory to find the NewGRF in.
 * @param temporary  The NewGRF/sprite file is to be loaded temporarily and should be closed immediately,
 *                   contrary to loading the SpriteFile and having it cached by the SpriteCache.
 */
void LoadNewGRFFile(GRFConfig &config, GrfLoadingStage stage, Subdirectory subdir, bool temporary)
{
	const std::string &filename = config.filename;

	/* A .grf file is activated only if it was active when the game was
	 * started.  If a game is loaded, only its active .grfs will be
	 * reactivated, unless "loadallgraphics on" is used.  A .grf file is
	 * considered active if its action 8 has been processed, i.e. its
	 * action 8 hasn't been skipped using an action 7.
	 *
	 * During activation, only actions 0, 1, 2, 3, 4, 5, 7, 8, 9, 0A and 0B are
	 * carried out.  All others are ignored, because they only need to be
	 * processed once at initialization.  */
	if (stage != GLS_FILESCAN && stage != GLS_SAFETYSCAN && stage != GLS_LABELSCAN) {
		_cur_gps.grffile = GetFileByFilename(filename);
		if (_cur_gps.grffile == nullptr) UserError("File '{}' lost in cache.\n", filename);
		if (stage == GLS_RESERVE && config.status != GCS_INITIALISED) return;
		if (stage == GLS_ACTIVATION && !config.flags.Test(GRFConfigFlag::Reserved)) return;
	}

	bool needs_palette_remap = config.palette & GRFP_USE_MASK;
	if (temporary) {
		SpriteFile temporarySpriteFile(filename, subdir, needs_palette_remap);
		LoadNewGRFFileFromFile(config, stage, temporarySpriteFile);
	} else {
		LoadNewGRFFileFromFile(config, stage, OpenCachedSpriteFile(filename, subdir, needs_palette_remap));
	}
}

/**
 * Relocates the old shore sprites at new positions.
 *
 * 1. If shore sprites are neither loaded by Action5 nor ActionA, the extra sprites from openttd(w/d).grf are used. (SHORE_REPLACE_ONLY_NEW)
 * 2. If a newgrf replaces some shore sprites by ActionA. The (maybe also replaced) grass tiles are used for corner shores. (SHORE_REPLACE_ACTION_A)
 * 3. If a newgrf replaces shore sprites by Action5 any shore replacement by ActionA has no effect. (SHORE_REPLACE_ACTION_5)
 */
static void ActivateOldShore()
{
	/* Use default graphics, if no shore sprites were loaded.
	 * Should not happen, as the base set's extra grf should include some. */
	if (_loaded_newgrf_features.shore == SHORE_REPLACE_NONE) _loaded_newgrf_features.shore = SHORE_REPLACE_ACTION_A;

	if (_loaded_newgrf_features.shore != SHORE_REPLACE_ACTION_5) {
		DupSprite(SPR_ORIGINALSHORE_START +  1, SPR_SHORE_BASE +  1); // SLOPE_W
		DupSprite(SPR_ORIGINALSHORE_START +  2, SPR_SHORE_BASE +  2); // SLOPE_S
		DupSprite(SPR_ORIGINALSHORE_START +  6, SPR_SHORE_BASE +  3); // SLOPE_SW
		DupSprite(SPR_ORIGINALSHORE_START +  0, SPR_SHORE_BASE +  4); // SLOPE_E
		DupSprite(SPR_ORIGINALSHORE_START +  4, SPR_SHORE_BASE +  6); // SLOPE_SE
		DupSprite(SPR_ORIGINALSHORE_START +  3, SPR_SHORE_BASE +  8); // SLOPE_N
		DupSprite(SPR_ORIGINALSHORE_START +  7, SPR_SHORE_BASE +  9); // SLOPE_NW
		DupSprite(SPR_ORIGINALSHORE_START +  5, SPR_SHORE_BASE + 12); // SLOPE_NE
	}

	if (_loaded_newgrf_features.shore == SHORE_REPLACE_ACTION_A) {
		DupSprite(SPR_FLAT_GRASS_TILE + 16, SPR_SHORE_BASE +  0); // SLOPE_STEEP_S
		DupSprite(SPR_FLAT_GRASS_TILE + 17, SPR_SHORE_BASE +  5); // SLOPE_STEEP_W
		DupSprite(SPR_FLAT_GRASS_TILE +  7, SPR_SHORE_BASE +  7); // SLOPE_WSE
		DupSprite(SPR_FLAT_GRASS_TILE + 15, SPR_SHORE_BASE + 10); // SLOPE_STEEP_N
		DupSprite(SPR_FLAT_GRASS_TILE + 11, SPR_SHORE_BASE + 11); // SLOPE_NWS
		DupSprite(SPR_FLAT_GRASS_TILE + 13, SPR_SHORE_BASE + 13); // SLOPE_ENW
		DupSprite(SPR_FLAT_GRASS_TILE + 14, SPR_SHORE_BASE + 14); // SLOPE_SEN
		DupSprite(SPR_FLAT_GRASS_TILE + 18, SPR_SHORE_BASE + 15); // SLOPE_STEEP_E

		/* XXX - SLOPE_EW, SLOPE_NS are currently not used.
		 *       If they would be used somewhen, then these grass tiles will most like not look as needed */
		DupSprite(SPR_FLAT_GRASS_TILE +  5, SPR_SHORE_BASE + 16); // SLOPE_EW
		DupSprite(SPR_FLAT_GRASS_TILE + 10, SPR_SHORE_BASE + 17); // SLOPE_NS
	}
}

/**
 * Relocate the old tram depot sprites to the new position, if no new ones were loaded.
 */
static void ActivateOldTramDepot()
{
	if (_loaded_newgrf_features.tram == TRAMWAY_REPLACE_DEPOT_WITH_TRACK) {
		DupSprite(SPR_ROAD_DEPOT               + 0, SPR_TRAMWAY_DEPOT_NO_TRACK + 0); // use road depot graphics for "no tracks"
		DupSprite(SPR_TRAMWAY_DEPOT_WITH_TRACK + 1, SPR_TRAMWAY_DEPOT_NO_TRACK + 1);
		DupSprite(SPR_ROAD_DEPOT               + 2, SPR_TRAMWAY_DEPOT_NO_TRACK + 2); // use road depot graphics for "no tracks"
		DupSprite(SPR_TRAMWAY_DEPOT_WITH_TRACK + 3, SPR_TRAMWAY_DEPOT_NO_TRACK + 3);
		DupSprite(SPR_TRAMWAY_DEPOT_WITH_TRACK + 4, SPR_TRAMWAY_DEPOT_NO_TRACK + 4);
		DupSprite(SPR_TRAMWAY_DEPOT_WITH_TRACK + 5, SPR_TRAMWAY_DEPOT_NO_TRACK + 5);
	}
}

/**
 * Decide whether price base multipliers of grfs shall apply globally or only to the grf specifying them
 */
static void FinalisePriceBaseMultipliers()
{
	extern const PriceBaseSpec _price_base_specs[];
	/** Features, to which '_grf_id_overrides' applies. Currently vehicle features only. */
	static constexpr GrfSpecFeatures override_features{GSF_TRAINS, GSF_ROADVEHICLES, GSF_SHIPS, GSF_AIRCRAFT};

	/* Evaluate grf overrides */
	int num_grfs = (uint)_grf_files.size();
	std::vector<int> grf_overrides(num_grfs, -1);
	for (int i = 0; i < num_grfs; i++) {
		GRFFile &source = _grf_files[i];
		auto it = _grf_id_overrides.find(source.grfid);
		if (it == std::end(_grf_id_overrides)) continue;
		uint32_t override_grfid = it->second;

		auto dest = std::ranges::find(_grf_files, override_grfid, &GRFFile::grfid);
		if (dest == std::end(_grf_files)) continue;

		grf_overrides[i] = static_cast<int>(std::ranges::distance(std::begin(_grf_files), dest));
		assert(grf_overrides[i] >= 0);
	}

	/* Override features and price base multipliers of earlier loaded grfs */
	for (int i = 0; i < num_grfs; i++) {
		if (grf_overrides[i] < 0 || grf_overrides[i] >= i) continue;
		GRFFile &source = _grf_files[i];
		GRFFile &dest = _grf_files[grf_overrides[i]];

		GrfSpecFeatures features = (source.grf_features | dest.grf_features) & override_features;
		source.grf_features.Set(features);
		dest.grf_features.Set(features);

		for (Price p = PR_BEGIN; p < PR_END; p++) {
			/* No price defined -> nothing to do */
			if (!features.Test(_price_base_specs[p].grf_feature) || source.price_base_multipliers[p] == INVALID_PRICE_MODIFIER) continue;
			Debug(grf, 3, "'{}' overrides price base multiplier {} of '{}'", source.filename, p, dest.filename);
			dest.price_base_multipliers[p] = source.price_base_multipliers[p];
		}
	}

	/* Propagate features and price base multipliers of afterwards loaded grfs, if none is present yet */
	for (int i = num_grfs - 1; i >= 0; i--) {
		if (grf_overrides[i] < 0 || grf_overrides[i] <= i) continue;
		GRFFile &source = _grf_files[i];
		GRFFile &dest = _grf_files[grf_overrides[i]];

		GrfSpecFeatures features = (source.grf_features | dest.grf_features) & override_features;
		source.grf_features.Set(features);
		dest.grf_features.Set(features);

		for (Price p = PR_BEGIN; p < PR_END; p++) {
			/* Already a price defined -> nothing to do */
			if (!features.Test(_price_base_specs[p].grf_feature) || dest.price_base_multipliers[p] != INVALID_PRICE_MODIFIER) continue;
			Debug(grf, 3, "Price base multiplier {} from '{}' propagated to '{}'", p, source.filename, dest.filename);
			dest.price_base_multipliers[p] = source.price_base_multipliers[p];
		}
	}

	/* The 'master grf' now have the correct multipliers. Assign them to the 'addon grfs' to make everything consistent. */
	for (int i = 0; i < num_grfs; i++) {
		if (grf_overrides[i] < 0) continue;
		GRFFile &source = _grf_files[i];
		GRFFile &dest = _grf_files[grf_overrides[i]];

		GrfSpecFeatures features = (source.grf_features | dest.grf_features) & override_features;
		source.grf_features.Set(features);
		dest.grf_features.Set(features);

		for (Price p = PR_BEGIN; p < PR_END; p++) {
			if (!features.Test(_price_base_specs[p].grf_feature)) continue;
			if (source.price_base_multipliers[p] != dest.price_base_multipliers[p]) {
				Debug(grf, 3, "Price base multiplier {} from '{}' propagated to '{}'", p, dest.filename, source.filename);
			}
			source.price_base_multipliers[p] = dest.price_base_multipliers[p];
		}
	}

	/* Apply fallback prices for grf version < 8 */
	for (auto &file : _grf_files) {
		if (file.grf_version >= 8) continue;
		PriceMultipliers &price_base_multipliers = file.price_base_multipliers;
		for (Price p = PR_BEGIN; p < PR_END; p++) {
			Price fallback_price = _price_base_specs[p].fallback_price;
			if (fallback_price != INVALID_PRICE && price_base_multipliers[p] == INVALID_PRICE_MODIFIER) {
				/* No price multiplier has been set.
				 * So copy the multiplier from the fallback price, maybe a multiplier was set there. */
				price_base_multipliers[p] = price_base_multipliers[fallback_price];
			}
		}
	}

	/* Decide local/global scope of price base multipliers */
	for (auto &file : _grf_files) {
		PriceMultipliers &price_base_multipliers = file.price_base_multipliers;
		for (Price p = PR_BEGIN; p < PR_END; p++) {
			if (price_base_multipliers[p] == INVALID_PRICE_MODIFIER) {
				/* No multiplier was set; set it to a neutral value */
				price_base_multipliers[p] = 0;
			} else {
				if (!file.grf_features.Test(_price_base_specs[p].grf_feature)) {
					/* The grf does not define any objects of the feature,
					 * so it must be a difficulty setting. Apply it globally */
					Debug(grf, 3, "'{}' sets global price base multiplier {}", file.filename, p);
					SetPriceBaseMultiplier(p, price_base_multipliers[p]);
					price_base_multipliers[p] = 0;
				} else {
					Debug(grf, 3, "'{}' sets local price base multiplier {}", file.filename, p);
				}
			}
		}
	}
}

template <typename T>
void AddBadgeToSpecs(T &specs, GrfSpecFeature feature, Badge &badge)
{
	for (auto &spec : specs) {
		if (spec == nullptr) continue;
		spec->badges.push_back(badge.index);
		badge.features.Set(feature);
	}
}

/** Finish up applying badges to things */
static void FinaliseBadges()
{
	for (const auto &file : _grf_files) {
		Badge *badge = GetBadgeByLabel(fmt::format("newgrf/{:08x}", std::byteswap(file.grfid)));
		if (badge == nullptr) continue;

		for (Engine *e : Engine::Iterate()) {
			if (e->grf_prop.grffile != &file) continue;
			e->badges.push_back(badge->index);
			badge->features.Set(static_cast<GrfSpecFeature>(GSF_TRAINS + e->type));
		}

		AddBadgeToSpecs(file.stations, GSF_STATIONS, *badge);
		AddBadgeToSpecs(file.housespec, GSF_HOUSES, *badge);
		AddBadgeToSpecs(file.industryspec, GSF_INDUSTRIES, *badge);
		AddBadgeToSpecs(file.indtspec, GSF_INDUSTRYTILES, *badge);
		AddBadgeToSpecs(file.objectspec, GSF_OBJECTS, *badge);
		AddBadgeToSpecs(file.airportspec, GSF_AIRPORTS, *badge);
		AddBadgeToSpecs(file.airtspec, GSF_AIRPORTTILES, *badge);
		AddBadgeToSpecs(file.roadstops, GSF_ROADSTOPS, *badge);
	}

	ApplyBadgeFeaturesToClassBadges();
	AddBadgeClassesToConfiguration();
}

extern void InitGRFTownGeneratorNames();

/** Finish loading NewGRFs and execute needed post-processing */
static void AfterLoadGRFs()
{
	/* Cached callback groups are no longer needed. */
	ResetCallbacks(true);

	FinaliseStringMapping();

	/* Clear the action 6 override sprites. */
	_grf_line_to_action6_sprite_override.clear();

	FinaliseBadges();

	/* Polish cargoes */
	FinaliseCargoArray();

	/* Pre-calculate all refit masks after loading GRF files. */
	CalculateRefitMasks();

	/* Polish engines */
	FinaliseEngineArray();

	/* Set the actually used Canal properties */
	FinaliseCanals();

	/* Add all new houses to the house array. */
	FinaliseHouseArray();

	/* Add all new industries to the industry array. */
	FinaliseIndustriesArray();

	/* Add all new objects to the object array. */
	FinaliseObjectsArray();

	InitializeSortedCargoSpecs();

	/* Sort the list of industry types. */
	SortIndustryTypes();

	/* Create dynamic list of industry legends for smallmap_gui.cpp */
	BuildIndustriesLegend();

	/* Build the routemap legend, based on the available cargos */
	BuildLinkStatsLegend();

	/* Add all new airports to the airports array. */
	FinaliseAirportsArray();
	BindAirportSpecs();

	/* Update the townname generators list */
	InitGRFTownGeneratorNames();

	/* Run all queued vehicle list order changes */
	CommitVehicleListOrderChanges();

	/* Load old shore sprites in new position, if they were replaced by ActionA */
	ActivateOldShore();

	/* Load old tram depot sprites in new position, if no new ones are present */
	ActivateOldTramDepot();

	/* Set up custom rail types */
	InitRailTypes();
	InitRoadTypes();

	for (Engine *e : Engine::IterateType(VEH_ROAD)) {
		if (_gted[e->index].rv_max_speed != 0) {
			/* Set RV maximum speed from the mph/0.8 unit value */
			e->VehInfo<RoadVehicleInfo>().max_speed = _gted[e->index].rv_max_speed * 4;
		}

		RoadTramType rtt = e->info.misc_flags.Test(EngineMiscFlag::RoadIsTram) ? RTT_TRAM : RTT_ROAD;

		const GRFFile *file = e->GetGRF();
		if (file == nullptr || _gted[e->index].roadtramtype == 0) {
			e->VehInfo<RoadVehicleInfo>().roadtype = (rtt == RTT_TRAM) ? ROADTYPE_TRAM : ROADTYPE_ROAD;
			continue;
		}

		/* Remove +1 offset. */
		_gted[e->index].roadtramtype--;

		const std::vector<RoadTypeLabel> *list = (rtt == RTT_TRAM) ? &file->tramtype_list : &file->roadtype_list;
		if (_gted[e->index].roadtramtype < list->size())
		{
			RoadTypeLabel rtl = (*list)[_gted[e->index].roadtramtype];
			RoadType rt = GetRoadTypeByLabel(rtl);
			if (rt != INVALID_ROADTYPE && GetRoadTramType(rt) == rtt) {
				e->VehInfo<RoadVehicleInfo>().roadtype = rt;
				continue;
			}
		}

		/* Road type is not available, so disable this engine */
		e->info.climates = {};
	}

	for (Engine *e : Engine::IterateType(VEH_TRAIN)) {
		RailTypes railtypes{};
		for (RailTypeLabel label : _gted[e->index].railtypelabels) {
			auto rt = GetRailTypeByLabel(label);
			if (rt != INVALID_RAILTYPE) railtypes.Set(rt);
		}

		if (railtypes.Any()) {
			e->VehInfo<RailVehicleInfo>().railtypes = railtypes;
			e->VehInfo<RailVehicleInfo>().intended_railtypes = railtypes;
		} else {
			/* Rail type is not available, so disable this engine */
			e->info.climates = {};
		}
	}

	SetYearEngineAgingStops();

	FinalisePriceBaseMultipliers();

	/* Deallocate temporary loading data */
	_gted.clear();
	_grm_sprites.clear();
}

/**
 * Load all the NewGRFs.
 * @param load_index The offset for the first sprite to add.
 * @param num_baseset Number of NewGRFs at the front of the list to look up in the baseset dir instead of the newgrf dir.
 */
void LoadNewGRF(SpriteID load_index, uint num_baseset)
{
	/* In case of networking we need to "sync" the start values
	 * so all NewGRFs are loaded equally. For this we use the
	 * start date of the game and we set the counters, etc. to
	 * 0 so they're the same too. */
	TimerGameCalendar::Date date            = TimerGameCalendar::date;
	TimerGameCalendar::Year year            = TimerGameCalendar::year;
	TimerGameCalendar::DateFract date_fract = TimerGameCalendar::date_fract;

	TimerGameEconomy::Date economy_date = TimerGameEconomy::date;
	TimerGameEconomy::Year economy_year = TimerGameEconomy::year;
	TimerGameEconomy::DateFract economy_date_fract = TimerGameEconomy::date_fract;

	uint64_t tick_counter  = TimerGameTick::counter;
	uint8_t display_opt     = _display_opt;

	if (_networking) {
		TimerGameCalendar::year = _settings_game.game_creation.starting_year;
		TimerGameCalendar::date = TimerGameCalendar::ConvertYMDToDate(TimerGameCalendar::year, 0, 1);
		TimerGameCalendar::date_fract = 0;

		TimerGameEconomy::year = TimerGameEconomy::Year{_settings_game.game_creation.starting_year.base()};
		TimerGameEconomy::date = TimerGameEconomy::ConvertYMDToDate(TimerGameEconomy::year, 0, 1);
		TimerGameEconomy::date_fract = 0;

		TimerGameTick::counter = 0;
		_display_opt  = 0;
	}

	InitializePatchFlags();

	ResetNewGRFData();

	/*
	 * Reset the status of all files, so we can 'retry' to load them.
	 * This is needed when one for example rearranges the NewGRFs in-game
	 * and a previously disabled NewGRF becomes usable. If it would not
	 * be reset, the NewGRF would remain disabled even though it should
	 * have been enabled.
	 */
	for (const auto &c : _grfconfig) {
		if (c->status != GCS_NOT_FOUND) c->status = GCS_UNKNOWN;
	}

	_cur_gps.spriteid = load_index;

	/* Load newgrf sprites
	 * in each loading stage, (try to) open each file specified in the config
	 * and load information from it. */
	for (GrfLoadingStage stage = GLS_LABELSCAN; stage <= GLS_ACTIVATION; stage++) {
		/* Set activated grfs back to will-be-activated between reservation- and activation-stage.
		 * This ensures that action7/9 conditions 0x06 - 0x0A work correctly. */
		for (const auto &c : _grfconfig) {
			if (c->status == GCS_ACTIVATED) c->status = GCS_INITIALISED;
		}

		if (stage == GLS_RESERVE) {
			static const std::pair<uint32_t, uint32_t> default_grf_overrides[] = {
				{ std::byteswap(0x44442202), std::byteswap(0x44440111) }, // UKRS addons modifies UKRS
				{ std::byteswap(0x6D620402), std::byteswap(0x6D620401) }, // DBSetXL ECS extension modifies DBSetXL
				{ std::byteswap(0x4D656f20), std::byteswap(0x4D656F17) }, // LV4cut modifies LV4
			};
			for (const auto &grf_override : default_grf_overrides) {
				SetNewGRFOverride(grf_override.first, grf_override.second);
			}
		}

		uint num_grfs = 0;
		uint num_non_static = 0;

		_cur_gps.stage = stage;
		for (const auto &c : _grfconfig) {
			if (c->status == GCS_DISABLED || c->status == GCS_NOT_FOUND) continue;
			if (stage > GLS_INIT && c->flags.Test(GRFConfigFlag::InitOnly)) continue;

			Subdirectory subdir = num_grfs < num_baseset ? BASESET_DIR : NEWGRF_DIR;
			if (!FioCheckFileExists(c->filename, subdir)) {
				Debug(grf, 0, "NewGRF file is missing '{}'; disabling", c->filename);
				c->status = GCS_NOT_FOUND;
				continue;
			}

			if (stage == GLS_LABELSCAN) InitNewGRFFile(*c);

			if (!c->flags.Test(GRFConfigFlag::Static) && !c->flags.Test(GRFConfigFlag::System)) {
				if (num_non_static == NETWORK_MAX_GRF_COUNT) {
					Debug(grf, 0, "'{}' is not loaded as the maximum number of non-static GRFs has been reached", c->filename);
					c->status = GCS_DISABLED;
					c->errors.emplace_back(STR_NEWGRF_ERROR_MSG_FATAL, 0, STR_NEWGRF_ERROR_TOO_MANY_NEWGRFS_LOADED);
					continue;
				}
				num_non_static++;
			}

			num_grfs++;

			LoadNewGRFFile(*c, stage, subdir, false);
			if (stage == GLS_RESERVE) {
				c->flags.Set(GRFConfigFlag::Reserved);
			} else if (stage == GLS_ACTIVATION) {
				c->flags.Reset(GRFConfigFlag::Reserved);
				assert(GetFileByGRFID(c->ident.grfid) == _cur_gps.grffile);
				ClearTemporaryNewGRFData(_cur_gps.grffile);
				BuildCargoTranslationMap();
				Debug(sprite, 2, "LoadNewGRF: Currently {} sprites are loaded", _cur_gps.spriteid);
			} else if (stage == GLS_INIT && c->flags.Test(GRFConfigFlag::InitOnly)) {
				/* We're not going to activate this, so free whatever data we allocated */
				ClearTemporaryNewGRFData(_cur_gps.grffile);
			}
		}
	}

	/* We've finished reading files. */
	_cur_gps.grfconfig = nullptr;
	_cur_gps.grffile = nullptr;

	/* Pseudo sprite processing is finished; free temporary stuff */
	_cur_gps.ClearDataForNextFile();

	/* Call any functions that should be run after GRFs have been loaded. */
	AfterLoadGRFs();

	/* Now revert back to the original situation */
	TimerGameCalendar::year = year;
	TimerGameCalendar::date = date;
	TimerGameCalendar::date_fract = date_fract;

	TimerGameEconomy::year = economy_year;
	TimerGameEconomy::date = economy_date;
	TimerGameEconomy::date_fract = economy_date_fract;

	TimerGameTick::counter = tick_counter;
	_display_opt  = display_opt;
}
