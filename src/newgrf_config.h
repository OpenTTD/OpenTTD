/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_config.h Functions to find and configure NewGRFs. */

#ifndef NEWGRF_CONFIG_H
#define NEWGRF_CONFIG_H

#include "strings_type.h"
#include "fileio_type.h"
#include "textfile_type.h"
#include "newgrf_text.h"
#include "3rdparty/md5/md5.h"

/** GRF config bit flags */
enum GRFConfigFlag : uint8_t {
	System,     ///< GRF file is an openttd-internal system grf
	Unsafe,     ///< GRF file is unsafe for static usage
	Static,     ///< GRF file is used statically (can be used in any MP game)
	Compatible, ///< GRF file does not exactly match the requested GRF (different MD5SUM), but grfid matches)
	Copy,       ///< The data is copied from a grf in _all_grfs
	InitOnly,   ///< GRF file is processed up to GLS_INIT
	Reserved,   ///< GRF file passed GLS_RESERVE stage
	Invalid,    ///< GRF is unusable with this version of OpenTTD
};
using GRFConfigFlags = EnumBitSet<GRFConfigFlag, uint8_t>;

/** Status of GRF */
enum GRFStatus : uint8_t {
	GCS_UNKNOWN,      ///< The status of this grf file is unknown
	GCS_DISABLED,     ///< GRF file is disabled
	GCS_NOT_FOUND,    ///< GRF file was not found in the local cache
	GCS_INITIALISED,  ///< GRF file has been initialised
	GCS_ACTIVATED,    ///< GRF file has been activated
};

/** Encountered GRF bugs */
enum class GRFBug : uint8_t {
	VehLength       = 0, ///< Length of rail vehicle changes when not inside a depot
	VehRefit        = 1, ///< Articulated vehicles carry different cargoes resp. are differently refittable than specified in purchase list
	VehPoweredWagon = 2, ///< Powered wagon changed poweredness state when not inside a depot
	UnknownCbResult = 3, ///< A callback returned an unknown/invalid result
	VehCapacity     = 4, ///< Capacity of vehicle changes when not refitting or arranging
};
using GRFBugs = EnumBitSet<GRFBug, uint8_t>;

/** Status of post-gameload GRF compatibility check */
enum GRFListCompatibility : uint8_t {
	GLC_ALL_GOOD,   ///< All GRF needed by game are present
	GLC_COMPATIBLE, ///< Compatible (eg. the same ID, but different checksum) GRF found in at least one case
	GLC_NOT_FOUND,  ///< At least one GRF couldn't be found (higher priority than GLC_COMPATIBLE)
};

/** Information that can/has to be stored about a GRF's palette. */
enum GRFPalette : uint8_t {
	GRFP_USE_BIT     = 0,   ///< The bit used for storing the palette to use.
	GRFP_GRF_OFFSET  = 2,   ///< The offset of the GRFP_GRF data.
	GRFP_GRF_SIZE    = 2,   ///< The size of the GRFP_GRF data.
	GRFP_BLT_OFFSET  = 4,   ///< The offset of the GRFP_BLT data.
	GRFP_BLT_SIZE    = 1,   ///< The size of the GRFP_BLT data.

	GRFP_USE_DOS     = 0x0, ///< The palette state is set to use the DOS palette.
	GRFP_USE_WINDOWS = 0x1, ///< The palette state is set to use the Windows palette.
	GRFP_USE_MASK    = 0x1, ///< Bitmask to get only the use palette use states.

	GRFP_GRF_UNSET   = 0x0 << GRFP_GRF_OFFSET,          ///< The NewGRF provided no information.
	GRFP_GRF_DOS     = 0x1 << GRFP_GRF_OFFSET,          ///< The NewGRF says the DOS palette can be used.
	GRFP_GRF_WINDOWS = 0x2 << GRFP_GRF_OFFSET,          ///< The NewGRF says the Windows palette can be used.
	GRFP_GRF_ANY     = GRFP_GRF_DOS | GRFP_GRF_WINDOWS, ///< The NewGRF says any palette can be used.
	GRFP_GRF_MASK    = GRFP_GRF_ANY,                    ///< Bitmask to get only the NewGRF supplied information.

	GRFP_BLT_UNSET   = 0x0 << GRFP_BLT_OFFSET,          ///< The NewGRF provided no information or doesn't care about a 32 bpp blitter.
	GRFP_BLT_32BPP   = 0x1 << GRFP_BLT_OFFSET,          ///< The NewGRF prefers a 32 bpp blitter.
	GRFP_BLT_MASK    = GRFP_BLT_32BPP,                  ///< Bitmask to only get the blitter information.
};


/** Basic data to distinguish a GRF. Used in the server list window */
struct GRFIdentifier {
	uint32_t grfid;     ///< GRF ID (defined by Action 0x08)
	MD5Hash md5sum;   ///< MD5 checksum of file to distinguish files with the same GRF ID (eg. newer version of GRF)

	GRFIdentifier() = default;
	GRFIdentifier(const GRFIdentifier &other) = default;
	GRFIdentifier(GRFIdentifier &&other) = default;
	GRFIdentifier(uint32_t grfid, const MD5Hash &md5sum) : grfid(grfid), md5sum(md5sum) {}

	GRFIdentifier& operator =(const GRFIdentifier &other) = default;

	/**
	 * Does the identification match the provided values?
	 * @param grfid  Expected grfid.
	 * @param md5sum Expected md5sum, may be \c nullptr (in which case, do not check it).
	 * @return the object has the provided grfid and md5sum.
	 */
	inline bool HasGrfIdentifier(uint32_t grfid, const MD5Hash *md5sum) const
	{
		if (this->grfid != grfid) return false;
		if (md5sum == nullptr) return true;
		return *md5sum == this->md5sum;
	}
};

/** Information about why GRF had problems during initialisation */
struct GRFError {
	GRFError(StringID severity, StringID message = {});

	std::string custom_message{}; ///< Custom message (if present)
	std::string data{}; ///< Additional data for message and custom_message
	StringID message{}; ///< Default message
	StringID severity{}; ///< Info / Warning / Error / Fatal
	std::array<uint32_t, 2> param_value{}; ///< Values of GRF parameters to show for message and custom_message
};

/** The possible types of a newgrf parameter. */
enum GRFParameterType : uint8_t {
	PTYPE_UINT_ENUM, ///< The parameter allows a range of numbers, each of which can have a special name
	PTYPE_BOOL,      ///< The parameter is either 0 or 1
	PTYPE_END,       ///< Invalid parameter type
};

/** Information about one grf parameter. */
struct GRFParameterInfo {
	/**
	 * Create a new empty GRFParameterInfo object.
	 * @param nr The newgrf parameter that is changed.
	 */
	explicit GRFParameterInfo(uint nr) : param_nr(nr) {}

	GRFTextList name = {}; ///< The name of this parameter
	GRFTextList desc = {}; ///< The description of this parameter

	uint32_t min_value = 0; ///< The minimal value this parameter can have
	uint32_t max_value = UINT32_MAX; ///< The maximal value of this parameter
	uint32_t def_value = 0; ///< Default value of this parameter

	GRFParameterType type = PTYPE_UINT_ENUM; ///< The type of this parameter

	uint8_t param_nr; ///< GRF parameter to store content in
	uint8_t first_bit = 0; ///< First bit to use in the GRF parameter
	uint8_t num_bit = 32; ///< Number of bits to use for this parameter

	bool complete_labels = false; ///< True if all values have a label.

	using ValueName = std::pair<uint32_t, GRFTextList>;
	std::vector<ValueName> value_names; ///< Names for each value.

	void Finalize();
};

/** Information about GRF, used in the game and (part of it) in savegames */
struct GRFConfig {
	static constexpr uint8_t MAX_NUM_PARAMS = 0x80;

	GRFConfig() = default;
	GRFConfig(const std::string &filename) : filename(filename) {}
	GRFConfig(const GRFConfig &config);

	/* Remove the copy assignment, as the default implementation will not do the right thing. */
	GRFConfig &operator=(GRFConfig &rhs) = delete;

	GRFIdentifier ident{}; ///< grfid and md5sum to uniquely identify newgrfs
	MD5Hash original_md5sum{}; ///< MD5 checksum of original file if only a 'compatible' file was loaded
	std::string filename{}; ///< Filename - either with or without full path
	GRFTextWrapper name{}; ///< NOSAVE: GRF name (Action 0x08)
	GRFTextWrapper info{}; ///< NOSAVE: GRF info (author, copyright, ...) (Action 0x08)
	GRFTextWrapper url{}; ///< NOSAVE: URL belonging to this GRF.
	std::optional<GRFError> error = std::nullopt; ///< NOSAVE: Error/Warning during GRF loading (Action 0x0B)

	uint32_t version = 0; ///< NOSAVE: Version a NewGRF can set so only the newest NewGRF is shown
	uint32_t min_loadable_version = 0; ///< NOSAVE: Minimum compatible version a NewGRF can define
	GRFConfigFlags flags = {}; ///< NOSAVE: GCF_Flags, bitset
	GRFStatus status = GCS_UNKNOWN; ///< NOSAVE: GRFStatus, enum
	GRFBugs grf_bugs = {}; ///< NOSAVE: bugs in this GRF in this run, @see enum GRFBugs
	uint8_t num_valid_params = MAX_NUM_PARAMS; ///< NOSAVE: Number of valid parameters (action 0x14)
	uint8_t palette = 0; ///< GRFPalette, bitset
	bool has_param_defaults = false; ///< NOSAVE: did this newgrf specify any defaults for it's parameters
	std::vector<std::optional<GRFParameterInfo>> param_info; ///< NOSAVE: extra information about the parameters
	std::vector<uint32_t> param; ///< GRF parameters

	bool IsCompatible(uint32_t old_version) const;
	void SetParams(std::span<const uint32_t> pars);
	void CopyParams(const GRFConfig &src);

	uint32_t GetValue(const GRFParameterInfo &info) const;
	void SetValue(const GRFParameterInfo &info, uint32_t value);

	std::optional<std::string> GetTextfile(TextfileType type) const;
	std::string GetName() const;
	std::optional<std::string> GetDescription() const;
	std::optional<std::string> GetURL() const;

	void SetParameterDefaults();
	void SetSuitablePalette();
	void FinalizeParameterInfo();
};

using GRFConfigList = std::vector<std::unique_ptr<GRFConfig>>;

/** Method to find GRFs using FindGRFConfig */
enum FindGRFConfigMode : uint8_t {
	FGCM_EXACT,       ///< Only find Grfs matching md5sum
	FGCM_COMPATIBLE,  ///< Find best compatible Grf wrt. desired_version
	FGCM_NEWEST,      ///< Find newest Grf
	FGCM_NEWEST_VALID,///< Find newest Grf, ignoring Grfs with GRFConfigFlag::Invalid set
	FGCM_ANY,         ///< Use first found
};

extern GRFConfigList _all_grfs;          ///< First item in list of all scanned NewGRFs
extern GRFConfigList _grfconfig;         ///< First item in list of current GRF set up
extern GRFConfigList _grfconfig_newgame; ///< First item in list of default GRF set up
extern GRFConfigList _grfconfig_static;  ///< First item in list of static GRF set up
extern uint _missing_extra_graphics;  ///< Number of sprites provided by the fallback extra GRF, i.e. missing in the baseset.

/** Callback for NewGRF scanning. */
struct NewGRFScanCallback {
	/** Make sure the right destructor gets called. */
	virtual ~NewGRFScanCallback() = default;
	/** Called whenever the NewGRF scan completed. */
	virtual void OnNewGRFsScanned() = 0;
};

size_t GRFGetSizeOfDataSection(FileHandle &f);

void ScanNewGRFFiles(NewGRFScanCallback *callback);
const GRFConfig *FindGRFConfig(uint32_t grfid, FindGRFConfigMode mode, const MD5Hash *md5sum = nullptr, uint32_t desired_version = 0);
GRFConfig *GetGRFConfig(uint32_t grfid, uint32_t mask = 0xFFFFFFFF);
void CopyGRFConfigList(GRFConfigList &dst, const GRFConfigList &src, bool init_only);
void AppendStaticGRFConfigs(GRFConfigList &dst);
void AppendToGRFConfigList(GRFConfigList &dst, std::unique_ptr<GRFConfig> &&el);
void ClearGRFConfigList(GRFConfigList &config);
void ResetGRFConfig(bool defaults);
GRFListCompatibility IsGoodGRFConfigList(GRFConfigList &grfconfig);
bool FillGRFDetails(GRFConfig &config, bool is_static, Subdirectory subdir = NEWGRF_DIR);
std::string GRFBuildParamList(const GRFConfig &c);

/* In newgrf_gui.cpp */
void ShowNewGRFSettings(bool editable, bool show_params, bool exec_changes, GRFConfigList &config);
void OpenGRFParameterWindow(bool is_baseset, GRFConfig &c, bool editable);

void UpdateNewGRFScanStatus(uint num, std::string &&name);
void UpdateNewGRFConfigPalette(int32_t new_value = 0);

#endif /* NEWGRF_CONFIG_H */
