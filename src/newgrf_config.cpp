/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_config.cpp Finding NewGRFs and configuring them. */

#include "stdafx.h"
#include "debug.h"
#include "3rdparty/md5/md5.h"
#include "newgrf.h"
#include "network/network_func.h"
#include "gfx_func.h"
#include "newgrf_text.h"
#include "window_func.h"
#include "progress.h"
#include "video/video_driver.hpp"
#include "string_func.h"
#include "strings_func.h"
#include "textfile_gui.h"
#include "thread.h"
#include "newgrf_config.h"
#include "newgrf_text.h"

#include "fileio_func.h"
#include "fios.h"

#include "safeguards.h"

/**
 * Create a new GRFConfig that is a deep copy of an existing config.
 * @param config The GRFConfig object to make a copy of.
 */
GRFConfig::GRFConfig(const GRFConfig &config) :
	ident(config.ident),
	original_md5sum(config.original_md5sum),
	filename(config.filename),
	name(config.name),
	info(config.info),
	url(config.url),
	errors(config.errors),
	version(config.version),
	min_loadable_version(config.min_loadable_version),
	flags(config.flags),
	status(config.status),
	grf_bugs(config.grf_bugs),
	num_valid_params(config.num_valid_params),
	palette(config.palette),
	has_param_defaults(config.has_param_defaults),
	param_info(config.param_info),
	param(config.param)
{
	this->flags.Reset(GRFConfigFlag::Copy);
}

void GRFConfig::SetParams(std::span<const uint32_t> pars)
{
	this->param.assign(std::begin(pars), std::end(pars));
}

/**
 * Return whether this NewGRF can replace an older version of the same NewGRF.
 */
bool GRFConfig::IsCompatible(uint32_t old_version) const
{
	return this->min_loadable_version <= old_version && old_version <= this->version;
}

/**
 * Copy the parameter information from the \a src config.
 * @param src Source config.
 */
void GRFConfig::CopyParams(const GRFConfig &src)
{
	this->param = src.param;
}

/**
 * Get the name of this grf. In case the name isn't known
 * the filename is returned.
 * @return The name of filename of this grf.
 */
std::string GRFConfig::GetName() const
{
	auto name = GetGRFStringFromGRFText(this->name);
	return name.has_value() && !name->empty() ? std::string(*name) : this->filename;
}

/**
 * Get the grf info.
 * @return A string with a description of this grf.
 */
std::optional<std::string> GRFConfig::GetDescription() const
{
	auto str = GetGRFStringFromGRFText(this->info);
	if (!str.has_value()) return std::nullopt;
	return std::string(*str);
}

/**
 * Get the grf url.
 * @return A string with an url of this grf.
 */
std::optional<std::string> GRFConfig::GetURL() const
{
	auto str = GetGRFStringFromGRFText(this->url);
	if (!str.has_value()) return std::nullopt;
	return std::string(*str);
}

/** Set the default value for all parameters as specified by action14. */
void GRFConfig::SetParameterDefaults()
{
	this->param.clear();

	if (!this->has_param_defaults) return;

	for (const auto &info : this->param_info) {
		if (!info.has_value()) continue;
		this->SetValue(info.value(), info->def_value);
	}
}

/**
 * Set the palette of this GRFConfig to something suitable.
 * That is either the setting coming from the NewGRF or
 * the globally used palette.
 */
void GRFConfig::SetSuitablePalette()
{
	PaletteType pal;
	switch (this->palette & GRFP_GRF_MASK) {
		case GRFP_GRF_DOS:     pal = PAL_DOS;      break;
		case GRFP_GRF_WINDOWS: pal = PAL_WINDOWS;  break;
		default:               pal = _settings_client.gui.newgrf_default_palette == 1 ? PAL_WINDOWS : PAL_DOS; break;
	}
	SB(this->palette, GRFP_USE_BIT, 1, pal == PAL_WINDOWS ? GRFP_USE_WINDOWS : GRFP_USE_DOS);
}

/**
 * Finalize Action 14 info after file scan is finished.
 */
void GRFConfig::FinalizeParameterInfo()
{
	for (auto &info : this->param_info) {
		if (!info.has_value()) continue;
		info->Finalize();
	}
}

GRFConfigList _all_grfs;
GRFConfigList _grfconfig;
GRFConfigList _grfconfig_newgame;
GRFConfigList _grfconfig_static;
uint _missing_extra_graphics = 0;

/**
 * Get the value of the given user-changeable parameter.
 * @param info The grf parameter info to get the value for.
 * @return The value of this parameter.
 */
uint32_t GRFConfig::GetValue(const GRFParameterInfo &info) const
{
	/* If the parameter is not set then it must be 0. */
	if (info.param_nr >= std::size(this->param)) return 0;

	/* GB doesn't work correctly with nbits == 32, so handle that case here. */
	if (info.num_bit == 32) return this->param[info.param_nr];

	return GB(this->param[info.param_nr], info.first_bit, info.num_bit);
}

/**
 * Set the value of the given user-changeable parameter.
 * @param info The grf parameter info to set the value for.
 * @param value The new value.
 */
void GRFConfig::SetValue(const GRFParameterInfo &info, uint32_t value)
{
	value = Clamp(value, info.min_value, info.max_value);

	/* Allocate the new parameter if it's not already present. */
	if (info.param_nr >= std::size(this->param)) this->param.resize(info.param_nr + 1);

	/* SB doesn't work correctly with nbits == 32, so handle that case here. */
	if (info.num_bit == 32) {
		this->param[info.param_nr] = value;
	} else {
		SB(this->param[info.param_nr], info.first_bit, info.num_bit, value);
	}

	SetWindowDirty(WC_GAME_OPTIONS, WN_GAME_OPTIONS_NEWGRF_STATE);
}

/**
 * Finalize Action 14 info after file scan is finished.
 */
void GRFParameterInfo::Finalize()
{
	/* Remove value names outside of the permitted range of values. */
	auto it = std::remove_if(std::begin(this->value_names), std::end(this->value_names),
			[this](const ValueName &vn) { return vn.first < this->min_value || vn.first > this->max_value; });
	this->value_names.erase(it, std::end(this->value_names));

	/* Test if the number of named values matches the full ranges of values. -1 because the range is inclusive. */
	this->complete_labels = (this->max_value - this->min_value) == std::size(this->value_names) - 1;
}

/**
 * Update the palettes of the graphics from the config file.
 * Called when changing the default palette in advanced settings.
 */
void UpdateNewGRFConfigPalette(int32_t)
{
	for (const auto &c : _grfconfig_newgame) c->SetSuitablePalette();
	for (const auto &c : _grfconfig_static ) c->SetSuitablePalette();
	for (const auto &c : _all_grfs         ) c->SetSuitablePalette();
}

/**
 * Get the data section size of a GRF.
 * @param f GRF.
 * @return Size of the data section or SIZE_MAX if the file has no separate data section.
 */
size_t GRFGetSizeOfDataSection(FileHandle &f)
{
	extern const std::array<uint8_t, 8> _grf_cont_v2_sig;
	static const uint header_len = 14;

	uint8_t data[header_len];
	if (fread(data, 1, header_len, f) == header_len) {
		if (data[0] == 0 && data[1] == 0 && std::ranges::equal(std::span(data + 2, _grf_cont_v2_sig.size()), _grf_cont_v2_sig)) {
			/* Valid container version 2, get data section size. */
			size_t offset = (static_cast<size_t>(data[13]) << 24) | (static_cast<size_t>(data[12]) << 16) | (static_cast<size_t>(data[11]) << 8) | static_cast<size_t>(data[10]);
			if (offset >= 1 * 1024 * 1024 * 1024) {
				Debug(grf, 0, "Unexpectedly large offset for NewGRF");
				/* Having more than 1 GiB of data is very implausible. Mostly because then
				 * all pools in OpenTTD are flooded already. Or it's just Action C all over.
				 * In any case, the offsets to graphics will likely not work either. */
				return SIZE_MAX;
			}
			return header_len + offset;
		}
	}

	return SIZE_MAX;
}

/**
 * Calculate the MD5 sum for a GRF, and store it in the config.
 * @param config GRF to compute.
 * @param subdir The subdirectory to look in.
 * @return MD5 sum was successfully computed
 */
static bool CalcGRFMD5Sum(GRFConfig &config, Subdirectory subdir)
{
	Md5 checksum;
	uint8_t buffer[1024];
	size_t len, size;

	/* open the file */
	auto f = FioFOpenFile(config.filename, "rb", subdir, &size);
	if (!f.has_value()) return false;

	long start = ftell(*f);
	size = std::min(size, GRFGetSizeOfDataSection(*f));

	if (start < 0 || fseek(*f, start, SEEK_SET) < 0) {
		return false;
	}

	/* calculate md5sum */
	while ((len = fread(buffer, 1, (size > sizeof(buffer)) ? sizeof(buffer) : size, *f)) != 0 && size != 0) {
		size -= len;
		checksum.Append(buffer, len);
	}
	checksum.Finish(config.ident.md5sum);

	return true;
}


/**
 * Find the GRFID of a given grf, and calculate its md5sum.
 * @param config    grf to fill.
 * @param is_static grf is static.
 * @param subdir    the subdirectory to search in.
 * @return Operation was successfully completed.
 */
bool FillGRFDetails(GRFConfig &config, bool is_static, Subdirectory subdir)
{
	if (!FioCheckFileExists(config.filename, subdir)) {
		config.status = GCS_NOT_FOUND;
		return false;
	}

	/* Find and load the Action 8 information */
	LoadNewGRFFile(config, GLS_FILESCAN, subdir, true);
	config.SetSuitablePalette();
	config.FinalizeParameterInfo();

	/* Skip if the grfid is 0 (not read) or if it is an internal GRF */
	if (config.ident.grfid == 0 || config.flags.Test(GRFConfigFlag::System)) return false;

	if (is_static) {
		/* Perform a 'safety scan' for static GRFs */
		LoadNewGRFFile(config, GLS_SAFETYSCAN, subdir, true);

		/* GRFConfigFlag::Unsafe is set if GLS_SAFETYSCAN finds unsafe actions */
		if (config.flags.Test(GRFConfigFlag::Unsafe)) return false;
	}

	return CalcGRFMD5Sum(config, subdir);
}


/**
 * Clear a GRF Config list, freeing all nodes.
 * @param config Start of the list.
 * @post \a config is set to \c nullptr.
 */
void ClearGRFConfigList(GRFConfigList &config)
{
	config.clear();
}

/**
 * Append a GRF Config list onto another list.
 * @param dst The destination list
 * @param src The source list
 * @param init_only the copied GRF will be processed up to GLS_INIT
 */
static void AppendGRFConfigList(GRFConfigList &dst, const GRFConfigList &src, bool init_only)
{
	for (const auto &s : src) {
		auto &c = dst.emplace_back(std::make_unique<GRFConfig>(*s));
		if (init_only) {
			c->flags.Set(GRFConfigFlag::InitOnly);
		} else {
			c->flags.Reset(GRFConfigFlag::InitOnly);
		}
	}
}

/**
 * Copy a GRF Config list.
 * @param dst The destination list
 * @param src The source list
 * @param init_only the copied GRF will be processed up to GLS_INIT
 */
void CopyGRFConfigList(GRFConfigList &dst, const GRFConfigList &src, bool init_only)
{
	/* Clear destination as it will be overwritten */
	ClearGRFConfigList(dst);
	AppendGRFConfigList(dst, src, init_only);
}

/**
 * Removes duplicates from lists of GRFConfigs. These duplicates
 * are introduced when the _grfconfig_static GRFs are appended
 * to the _grfconfig on a newgame or savegame. As the parameters
 * of the static GRFs could be different that the parameters of
 * the ones used non-statically. This can result in desyncs in
 * multiplayers, so the duplicate static GRFs have to be removed.
 *
 * This function _assumes_ that all static GRFs are placed after
 * the non-static GRFs.
 *
 * @param list the list to remove the duplicates from
 */
static void RemoveDuplicatesFromGRFConfigList(GRFConfigList &list)
{
	if (list.empty()) return;

	auto last = std::end(list);
	for (auto it = std::begin(list); it != last; ++it) {
		auto remove = std::remove_if(std::next(it), last, [&grfid = (*it)->ident.grfid](const auto &c) { return grfid == c->ident.grfid; });
		last = list.erase(remove, last);
	}
}

/**
 * Appends the static GRFs to a list of GRFs
 * @param dst the head of the list to add to
 */
void AppendStaticGRFConfigs(GRFConfigList &dst)
{
	AppendGRFConfigList(dst, _grfconfig_static, false);
	RemoveDuplicatesFromGRFConfigList(dst);
}

/**
 * Appends an element to a list of GRFs
 * @param dst the head of the list to add to
 * @param el the new tail to be
 */
void AppendToGRFConfigList(GRFConfigList &dst, std::unique_ptr<GRFConfig> &&el)
{
	dst.push_back(std::move(el));
	RemoveDuplicatesFromGRFConfigList(dst);
}


/** Reset the current GRF Config to either blank or newgame settings. */
void ResetGRFConfig(bool defaults)
{
	CopyGRFConfigList(_grfconfig, _grfconfig_newgame, !defaults);
	AppendStaticGRFConfigs(_grfconfig);
}


/**
 * Check if all GRFs in the GRF config from a savegame can be loaded.
 * @param grfconfig GrfConfig to check
 * @return will return any of the following 3 values:<br>
 * <ul>
 * <li> GLC_ALL_GOOD: No problems occurred, all GRF files were found and loaded
 * <li> GLC_COMPATIBLE: For one or more GRF's no exact match was found, but a
 *     compatible GRF with the same grfid was found and used instead
 * <li> GLC_NOT_FOUND: For one or more GRF's no match was found at all
 * </ul>
 */
GRFListCompatibility IsGoodGRFConfigList(GRFConfigList &grfconfig)
{
	GRFListCompatibility res = GLC_ALL_GOOD;

	for (auto &c : grfconfig) {
		const GRFConfig *f = FindGRFConfig(c->ident.grfid, FGCM_EXACT, &c->ident.md5sum);
		if (f == nullptr || f->flags.Test(GRFConfigFlag::Invalid)) {
			/* If we have not found the exactly matching GRF try to find one with the
			 * same grfid, as it most likely is compatible */
			f = FindGRFConfig(c->ident.grfid, FGCM_COMPATIBLE, nullptr, c->version);
			if (f != nullptr) {
				Debug(grf, 1, "NewGRF {:08X} ({}) not found; checksum {}. Compatibility mode on", std::byteswap(c->ident.grfid), c->filename, FormatArrayAsHex(c->ident.md5sum));
				if (!c->flags.Test(GRFConfigFlag::Compatible)) {
					/* Preserve original_md5sum after it has been assigned */
					c->flags.Set(GRFConfigFlag::Compatible);
					c->original_md5sum = c->ident.md5sum;
				}

				/* Non-found has precedence over compatibility load */
				if (res != GLC_NOT_FOUND) res = GLC_COMPATIBLE;
				goto compatible_grf;
			}

			/* No compatible grf was found, mark it as disabled */
			Debug(grf, 0, "NewGRF {:08X} ({}) not found; checksum {}", std::byteswap(c->ident.grfid), c->filename, FormatArrayAsHex(c->ident.md5sum));

			c->status = GCS_NOT_FOUND;
			res = GLC_NOT_FOUND;
		} else {
compatible_grf:
			Debug(grf, 1, "Loading GRF {:08X} from {}", std::byteswap(f->ident.grfid), f->filename);
			/* The filename could be the filename as in the savegame. As we need
			 * to load the GRF here, we need the correct filename, so overwrite that
			 * in any case and set the name and info when it is not set already.
			 * When the GRFConfigFlag::Copy flag is set, it is certain that the filename is
			 * already a local one, so there is no need to replace it. */
			if (!c->flags.Test(GRFConfigFlag::Copy)) {
				c->filename = f->filename;
				c->ident.md5sum = f->ident.md5sum;
				c->name = f->name;
				c->info = f->name;
				c->errors.clear();
				c->version = f->version;
				c->min_loadable_version = f->min_loadable_version;
				c->num_valid_params = f->num_valid_params;
				c->param_info = f->param_info;
				c->has_param_defaults = f->has_param_defaults;
			}
		}
	}

	return res;
}


/** Set this flag to prevent any NewGRF scanning from being done. */
int _skip_all_newgrf_scanning = 0;

/** Helper for scanning for files with GRF as extension */
class GRFFileScanner : FileScanner {
	std::chrono::steady_clock::time_point next_update; ///< The next moment we do update the screen.
	uint num_scanned; ///< The number of GRFs we have scanned.

public:
	GRFFileScanner() : num_scanned(0)
	{
		this->next_update = std::chrono::steady_clock::now();
	}

	bool AddFile(const std::string &filename, size_t basepath_length, const std::string &tar_filename) override;

	/** Do the scan for GRFs. */
	static uint DoScan()
	{
		if (_skip_all_newgrf_scanning > 0) {
			if (_skip_all_newgrf_scanning == 1) _skip_all_newgrf_scanning = 0;
			return 0;
		}

		GRFFileScanner fs;
		int ret = fs.Scan(".grf", NEWGRF_DIR);
		/* The number scanned and the number returned may not be the same;
		 * duplicate NewGRFs and base sets are ignored in the return value. */
		_settings_client.gui.last_newgrf_count = fs.num_scanned;
		return ret;
	}
};

bool GRFFileScanner::AddFile(const std::string &filename, size_t basepath_length, const std::string &)
{
	/* Abort if the user stopped the game during a scan. */
	if (_exit_game) return false;

	bool added = false;
	auto c = std::make_unique<GRFConfig>(filename.substr(basepath_length));
	GRFConfig *grfconfig = c.get();
	if (FillGRFDetails(*c, false)) {
		if (std::ranges::none_of(_all_grfs, [&c](const auto &gc) { return c->ident.grfid == gc->ident.grfid && c->ident.md5sum == gc->ident.md5sum; })) {
			_all_grfs.push_back(std::move(c));
			added = true;
		}
	}

	this->num_scanned++;

	std::string name = grfconfig->GetName();
	UpdateNewGRFScanStatus(this->num_scanned, std::move(name));
	VideoDriver::GetInstance()->GameLoopPause();

	return added;
}

/**
 * Simple sorter for GRFS
 * @param c1 the first GRFConfig *
 * @param c2 the second GRFConfig *
 * @return true if the name of first NewGRF is before the name of the second.
 */
static bool GRFSorter(std::unique_ptr<GRFConfig> const &c1, std::unique_ptr<GRFConfig> const &c2)
{
	return StrNaturalCompare(c1->GetName(), c2->GetName()) < 0;
}

/**
 * Really perform the scan for all NewGRFs.
 * @param callback The callback to call after the scanning is complete.
 */
void DoScanNewGRFFiles(NewGRFScanCallback *callback)
{
	ClearGRFConfigList(_all_grfs);
	TarScanner::DoScan(TarScanner::Mode::NewGRF);

	Debug(grf, 1, "Scanning for NewGRFs");
	uint num = GRFFileScanner::DoScan();

	Debug(grf, 1, "Scan complete, found {} files", num);
	std::ranges::sort(_all_grfs, GRFSorter);
	NetworkAfterNewGRFScan();

	/* Yes... these are the NewGRF windows */
	InvalidateWindowClassesData(WC_SAVELOAD, 0, true);
	InvalidateWindowData(WC_GAME_OPTIONS, WN_GAME_OPTIONS_NEWGRF_STATE, GOID_NEWGRF_RESCANNED, true);
	if (!_exit_game && callback != nullptr) callback->OnNewGRFsScanned();

	CloseWindowByClass(WC_MODAL_PROGRESS);
	SetModalProgress(false);
	MarkWholeScreenDirty();
}

/**
 * Scan for all NewGRFs.
 * @param callback The callback to call after the scanning is complete.
 */
void ScanNewGRFFiles(NewGRFScanCallback *callback)
{
	/* First set the modal progress. This ensures that it will eventually let go of the paint mutex. */
	SetModalProgress(true);
	/* Only then can we really start, especially by marking the whole screen dirty. Get those other windows hidden!. */
	MarkWholeScreenDirty();

	DoScanNewGRFFiles(callback);
}

/**
 * Find a NewGRF in the scanned list.
 * @param grfid GRFID to look for,
 * @param mode Restrictions for matching grfs
 * @param md5sum Expected MD5 sum
 * @param desired_version Requested version
 * @return The matching grf, if it exists in #_all_grfs, else \c nullptr.
 */
const GRFConfig *FindGRFConfig(uint32_t grfid, FindGRFConfigMode mode, const MD5Hash *md5sum, uint32_t desired_version)
{
	assert((mode == FGCM_EXACT) != (md5sum == nullptr));
	const GRFConfig *best = nullptr;
	for (const auto &c : _all_grfs) {
		/* if md5sum is set, we look for an exact match and continue if not found */
		if (!c->ident.HasGrfIdentifier(grfid, md5sum)) continue;
		/* return it, if the exact same newgrf is found, or if we do not care about finding "the best" */
		if (md5sum != nullptr || mode == FGCM_ANY) return c.get();
		/* Skip incompatible stuff, unless explicitly allowed */
		if (mode != FGCM_NEWEST && c->flags.Test(GRFConfigFlag::Invalid)) continue;
		/* check version compatibility */
		if (mode == FGCM_COMPATIBLE && !c->IsCompatible(desired_version)) continue;
		/* remember the newest one as "the best" */
		if (best == nullptr || c->version > best->version) best = c.get();
	}

	return best;
}

/**
 * Retrieve a NewGRF from the current config by its grfid.
 * @param grfid grf to look for.
 * @param mask  GRFID mask to allow for partial matching.
 * @return The grf config, if it exists, else \c nullptr.
 */
GRFConfig *GetGRFConfig(uint32_t grfid, uint32_t mask)
{
	auto it = std::ranges::find_if(_grfconfig, [grfid, mask](const auto &c) { return (c->ident.grfid & mask) == (grfid & mask); });
	if (it != std::end(_grfconfig)) return it->get();

	return nullptr;
}


/** Build a string containing space separated parameter values, and terminate */
std::string GRFBuildParamList(const GRFConfig &c)
{
	std::string result;
	for (const uint32_t &value : c.param) {
		if (!result.empty()) result += ' ';
		format_append(result, "{}", value);
	}
	return result;
}

/**
 * Search a textfile file next to this NewGRF.
 * @param type The type of the textfile to search for.
 * @return The filename for the textfile.
 */
std::optional<std::string> GRFConfig::GetTextfile(TextfileType type) const
{
	return ::GetTextfile(type, NEWGRF_DIR, this->filename);
}
