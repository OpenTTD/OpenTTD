/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gfxinit.cpp Initializing of the (GRF) graphics. */

#include "stdafx.h"
#include "fios.h"
#include "newgrf.h"
#include "3rdparty/md5/md5.h"
#include "fontcache.h"
#include "gfx_func.h"
#include "transparency.h"
#include "blitter/factory.hpp"
#include "video/video_driver.hpp"
#include "window_func.h"
#include "palette_func.h"
#include "base_media_func.h"
#include "base_media_graphics.h"
#include "base_media_sounds.h"

#include "table/sprites.h"

#include "safeguards.h"

#include "table/landscape_sprite.h"

/** Offsets for loading the different "replacement" sprites in the files. */
static constexpr std::span<const std::pair<SpriteID, SpriteID>> _landscape_spriteindexes[] = {
	_landscape_spriteindexes_arctic,
	_landscape_spriteindexes_tropic,
	_landscape_spriteindexes_toyland,
};

/**
 * Load an old fashioned GRF file.
 * @param filename   The name of the file to open.
 * @param load_index The offset of the first sprite.
 * @param needs_palette_remap Whether the colours in the GRF file need a palette remap.
 * @return The number of loaded sprites.
 */
static uint LoadGrfFile(const std::string &filename, SpriteID load_index, bool needs_palette_remap)
{
	SpriteID load_index_org = load_index;
	SpriteID sprite_id = 0;

	SpriteFile &file = OpenCachedSpriteFile(filename, BASESET_DIR, needs_palette_remap);

	Debug(sprite, 2, "Reading grf-file '{}'", filename);

	uint8_t container_ver = file.GetContainerVersion();
	if (container_ver == 0) UserError("Base grf '{}' is corrupt", filename);
	ReadGRFSpriteOffsets(file);
	if (container_ver >= 2) {
		/* Read compression. */
		uint8_t compression = file.ReadByte();
		if (compression != 0) UserError("Unsupported compression format");
	}

	while (LoadNextSprite(load_index, file, sprite_id)) {
		load_index++;
		sprite_id++;
		if (load_index >= MAX_SPRITES) {
			UserError("Too many sprites. Recompile with higher MAX_SPRITES value or remove some custom GRF files.");
		}
	}
	Debug(sprite, 2, "Currently {} sprites are loaded", load_index);

	return load_index - load_index_org;
}

/**
 * Load an old fashioned GRF file to replace already loaded sprites.
 * @param filename   The name of the file to open.
 * @param index_tbl  The offsets of each of the sprites.
 * @param needs_palette_remap Whether the colours in the GRF file need a palette remap.
 * @return The number of loaded sprites.
 */
static void LoadGrfFileIndexed(const std::string &filename, std::span<const std::pair<SpriteID, SpriteID>> index_tbl, bool needs_palette_remap)
{
	uint sprite_id = 0;

	SpriteFile &file = OpenCachedSpriteFile(filename, BASESET_DIR, needs_palette_remap);

	Debug(sprite, 2, "Reading indexed grf-file '{}'", filename);

	uint8_t container_ver = file.GetContainerVersion();
	if (container_ver == 0) UserError("Base grf '{}' is corrupt", filename);
	ReadGRFSpriteOffsets(file);
	if (container_ver >= 2) {
		/* Read compression. */
		uint8_t compression = file.ReadByte();
		if (compression != 0) UserError("Unsupported compression format");
	}

	for (const auto &pair : index_tbl) {
		for (SpriteID load_index = pair.first; load_index <= pair.second; ++load_index) {
			[[maybe_unused]] bool b = LoadNextSprite(load_index, file, sprite_id);
			assert(b);
			sprite_id++;
		}
	}
}

/**
 * Checks whether the MD5 checksums of the files are correct.
 *
 * @note Also checks sample.cat and other required non-NewGRF GRFs for corruption.
 */
void CheckExternalFiles()
{
	if (BaseGraphics::GetUsedSet() == nullptr || BaseSounds::GetUsedSet() == nullptr) return;

	const GraphicsSet *used_set = BaseGraphics::GetUsedSet();

	Debug(grf, 1, "Using the {} base graphics set", used_set->name);

	std::string error_msg;
	auto output_iterator = std::back_inserter(error_msg);
	if (used_set->GetNumInvalid() != 0) {
		/* Not all files were loaded successfully, see which ones */
		fmt::format_to(output_iterator, "Trying to load graphics set '{}', but it is incomplete. The game will probably not run correctly until you properly install this set or select another one. See section 1.4 of README.md.\n\nThe following files are corrupted or missing:\n", used_set->name);
		for (const auto &file : used_set->files) {
			MD5File::ChecksumResult res = GraphicsSet::CheckMD5(&file, BASESET_DIR);
			if (res != MD5File::CR_MATCH) fmt::format_to(output_iterator, "\t{} is {} ({})\n", file.filename, res == MD5File::CR_MISMATCH ? "corrupt" : "missing", file.missing_warning);
		}
		fmt::format_to(output_iterator, "\n");
	}

	const SoundsSet *sounds_set = BaseSounds::GetUsedSet();
	if (sounds_set->GetNumInvalid() != 0) {
		fmt::format_to(output_iterator, "Trying to load sound set '{}', but it is incomplete. The game will probably not run correctly until you properly install this set or select another one. See section 1.4 of README.md.\n\nThe following files are corrupted or missing:\n", sounds_set->name);

		static_assert(SoundsSet::NUM_FILES == 1);
		/* No need to loop each file, as long as there is only a single
		 * sound file. */
		fmt::format_to(output_iterator, "\t{} is {} ({})\n", sounds_set->files[0].filename, SoundsSet::CheckMD5(&sounds_set->files[0], BASESET_DIR) == MD5File::CR_MISMATCH ? "corrupt" : "missing", sounds_set->files[0].missing_warning);
	}

	if (!error_msg.empty()) ShowInfoI(error_msg);
}

/**
 * Get GRFConfig for the default extra graphics.
 * @return Managed pointer to default extra GRFConfig.
 */
static std::unique_ptr<GRFConfig> GetDefaultExtraGRFConfig()
{
	auto gc = std::make_unique<GRFConfig>("OPENTTD.GRF");
	gc->palette |= GRFP_GRF_DOS;
	FillGRFDetails(*gc, false, BASESET_DIR);
	gc->flags.Reset(GRFConfigFlag::InitOnly);
	return gc;
}

/**
 * Get GRFConfig for the baseset extra graphics.
 * @return Managed pointer to baseset extra GRFConfig.
 */
static std::unique_ptr<GRFConfig> GetBasesetExtraGRFConfig()
{
	auto gc = std::make_unique<GRFConfig>(BaseGraphics::GetUsedSet()->GetOrCreateExtraConfig());
	if (gc->param.empty()) gc->SetParameterDefaults();
	gc->flags.Reset(GRFConfigFlag::InitOnly);
	return gc;
}

/** Actually load the sprite tables. */
static void LoadSpriteTables()
{
	const GraphicsSet *used_set = BaseGraphics::GetUsedSet();

	LoadGrfFile(used_set->files[GFT_BASE].filename, 0, PAL_DOS != used_set->palette);

	/*
	 * The second basic file always starts at the given location and does
	 * contain a different amount of sprites depending on the "type"; DOS
	 * has a few sprites less. However, we do not care about those missing
	 * sprites as they are not shown anyway (logos in intro game).
	 */
	LoadGrfFile(used_set->files[GFT_LOGOS].filename, 4793, PAL_DOS != used_set->palette);

	/*
	 * Load additional sprites for climates other than temperate.
	 * This overwrites some of the temperate sprites, such as foundations
	 * and the ground sprites.
	 */
	if (_settings_game.game_creation.landscape != LandscapeType::Temperate) {
		LoadGrfFileIndexed(
			used_set->files[GFT_ARCTIC + to_underlying(_settings_game.game_creation.landscape) - 1].filename,
			_landscape_spriteindexes[to_underlying(_settings_game.game_creation.landscape) - 1],
			PAL_DOS != used_set->palette
		);
	}

	/* Initialize the unicode to sprite mapping table */
	InitializeUnicodeGlyphMap();

	/*
	 * Load the base and extra NewGRF with OTTD required graphics as first NewGRF.
	 * However, we do not want it to show up in the list of used NewGRFs,
	 * so we have to manually add it, and then remove it later.
	 */

	auto default_extra = GetDefaultExtraGRFConfig();
	auto baseset_extra = GetBasesetExtraGRFConfig();
	std::string default_filename = default_extra->filename;

	_grfconfig.insert(std::begin(_grfconfig), std::move(default_extra));
	_grfconfig.insert(std::next(std::begin(_grfconfig)), std::move(baseset_extra));

	LoadNewGRF(SPR_NEWGRFS_BASE, 2);

	uint total_extra_graphics = SPR_NEWGRFS_BASE - SPR_OPENTTD_BASE;
	Debug(sprite, 4, "Checking sprites from fallback grf");
	_missing_extra_graphics = GetSpriteCountForFile(default_filename, SPR_OPENTTD_BASE, SPR_NEWGRFS_BASE);
	Debug(sprite, 1, "{} extra sprites, {} from baseset, {} from fallback", total_extra_graphics, total_extra_graphics - _missing_extra_graphics, _missing_extra_graphics);

	/* The original baseset extra graphics intentionally make use of the fallback graphics.
	 * Let's say everything which provides less than 500 sprites misses the rest intentionally. */
	if (500 + _missing_extra_graphics > total_extra_graphics) _missing_extra_graphics = 0;

	/* Remove the default and baseset extra graphics from the config. */
	_grfconfig.erase(std::begin(_grfconfig), std::next(std::begin(_grfconfig), 2));
}


static void RealChangeBlitter(std::string_view repl_blitter)
{
	std::string_view cur_blitter = BlitterFactory::GetCurrentBlitter()->GetName();
	if (cur_blitter == repl_blitter) return;

	Debug(driver, 1, "Switching blitter from '{}' to '{}'... ", cur_blitter, repl_blitter);
	Blitter *new_blitter = BlitterFactory::SelectBlitter(repl_blitter);
	if (new_blitter == nullptr) NOT_REACHED();
	Debug(driver, 1, "Successfully switched to {}.", repl_blitter);

	if (!VideoDriver::GetInstance()->AfterBlitterChange()) {
		/* Failed to switch blitter, let's hope we can return to the old one. */
		if (BlitterFactory::SelectBlitter(cur_blitter) == nullptr || !VideoDriver::GetInstance()->AfterBlitterChange()) UserError("Failed to reinitialize video driver. Specify a fixed blitter in the config");
	}

	/* Clear caches that might have sprites for another blitter. */
	VideoDriver::GetInstance()->ClearSystemSprites();
	FontCache::ClearFontCaches(FONTSIZES_ALL);
	GfxClearSpriteCache();
	ReInitAllWindows(false);
}

/**
 * Check blitter needed by NewGRF config and switch if needed.
 * @return False when nothing changed, true otherwise.
 */
static bool SwitchNewGRFBlitter()
{
	/* Never switch if the blitter was specified by the user. */
	if (!_blitter_autodetected) return false;

	/* Null driver => dedicated server => do nothing. */
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() == 0) return false;

	/* Get preferred depth.
	 *  - depth_wanted_by_base: Depth required by the baseset, i.e. the majority of the sprites.
	 *  - depth_wanted_by_grf:  Depth required by some NewGRF.
	 * Both can force using a 32bpp blitter. depth_wanted_by_base is used to select
	 * between multiple 32bpp blitters, which perform differently with 8bpp sprites.
	 */
	uint depth_wanted_by_base = BaseGraphics::GetUsedSet()->blitter == BLT_32BPP ? 32 : 8;
	uint depth_wanted_by_grf = _support8bpp != S8BPP_NONE ? 8 : 32;
	for (const auto &c : _grfconfig) {
		if (c->status == GCS_DISABLED || c->status == GCS_NOT_FOUND || c->flags.Test(GRFConfigFlag::InitOnly)) continue;
		if (c->palette & GRFP_BLT_32BPP) depth_wanted_by_grf = 32;
	}
	/* We need a 32bpp blitter for font anti-alias. */
	if (GetFontAAState()) depth_wanted_by_grf = 32;

	/* Search the best blitter. */
	static const struct {
		const std::string_view name;
		uint animation; ///< 0: no support, 1: do support, 2: both
		uint min_base_depth, max_base_depth, min_grf_depth, max_grf_depth;
	} replacement_blitters[] = {
		{ "8bpp-optimized",  2,  8,  8,  8,  8 },
		{ "40bpp-anim",      2,  8, 32,  8, 32 },
#ifdef WITH_SSE
		{ "32bpp-sse4",      0, 32, 32,  8, 32 },
		{ "32bpp-ssse3",     0, 32, 32,  8, 32 },
		{ "32bpp-sse2",      0, 32, 32,  8, 32 },
		{ "32bpp-sse4-anim", 1, 32, 32,  8, 32 },
#endif
		{ "32bpp-optimized", 0,  8, 32,  8, 32 },
#ifdef WITH_SSE
		{ "32bpp-sse2-anim", 1,  8, 32,  8, 32 },
#endif
		{ "32bpp-anim",      1,  8, 32,  8, 32 },
	};

	const bool animation_wanted = HasBit(_display_opt, DO_FULL_ANIMATION);
	std::string_view cur_blitter = BlitterFactory::GetCurrentBlitter()->GetName();

	for (const auto &replacement_blitter : replacement_blitters) {
		if (animation_wanted && (replacement_blitter.animation == 0)) continue;
		if (!animation_wanted && (replacement_blitter.animation == 1)) continue;

		if (!IsInsideMM(depth_wanted_by_base, replacement_blitter.min_base_depth, replacement_blitter.max_base_depth + 1)) continue;
		if (!IsInsideMM(depth_wanted_by_grf, replacement_blitter.min_grf_depth, replacement_blitter.max_grf_depth + 1)) continue;

		if (replacement_blitter.name == cur_blitter) {
			return false;
		}
		if (BlitterFactory::GetBlitterFactory(replacement_blitter.name) == nullptr) continue;

		/* Inform the video driver we want to switch blitter as soon as possible. */
		VideoDriver::GetInstance()->QueueOnMainThread(std::bind(&RealChangeBlitter, replacement_blitter.name));
		break;
	}

	return true;
}

/** Check whether we still use the right blitter, or use another (better) one. */
void CheckBlitter()
{
	if (!SwitchNewGRFBlitter()) return;

	FontCache::ClearFontCaches(FONTSIZES_ALL);
	GfxClearSpriteCache();
	ReInitAllWindows(false);
}

/** Initialise and load all the sprites. */
void GfxLoadSprites()
{
	Debug(sprite, 2, "Loading sprite set {}", _settings_game.game_creation.landscape);

	SwitchNewGRFBlitter();
	VideoDriver::GetInstance()->ClearSystemSprites();
	FontCache::ClearFontCaches(FONTSIZES_ALL);
	GfxInitSpriteMem();
	LoadSpriteTables();
	GfxInitPalettes();

	UpdateCursorSize();
}

/* instantiate here, because unique_ptr needs a complete type */
GraphicsSet::GraphicsSet() = default;

/* instantiate here, because unique_ptr needs a complete type */
GraphicsSet::~GraphicsSet() = default;

bool GraphicsSet::FillSetDetails(const IniFile &ini, const std::string &path, const std::string &full_filename)
{
	if (!this->BaseSet<GraphicsSet>::FillSetDetails(ini, path, full_filename, false)) return false;

	const IniGroup *metadata = ini.GetGroup("metadata");
	assert(metadata != nullptr); /* already checked by the inherited FillSetDetails. */
	const IniItem *item;

	item = this->GetMandatoryItem(full_filename, *metadata, "palette");
	if (item == nullptr) return false;
	this->palette = ((*item->value)[0] == 'D' || (*item->value)[0] == 'd') ? PAL_DOS : PAL_WINDOWS;

	/* Get optional blitter information. */
	item = metadata->GetItem("blitter");
	this->blitter = (item != nullptr && (*item->value)[0] == '3') ? BLT_32BPP : BLT_8BPP;

	return true;
}

/**
 * Return configuration for the extra GRF, or lazily create it.
 * @return NewGRF configuration
 */
GRFConfig &GraphicsSet::GetOrCreateExtraConfig() const
{
	if (!this->extra_cfg) {
		this->extra_cfg = std::make_unique<GRFConfig>(this->files[GFT_EXTRA].filename);

		/* We know the palette of the base set, so if the base NewGRF is not
		 * setting one, use the palette of the base set and not the global
		 * one which might be the wrong palette for this base NewGRF.
		 * The value set here might be overridden via action14 later. */
		switch (this->palette) {
			case PAL_DOS:     this->extra_cfg->palette |= GRFP_GRF_DOS;     break;
			case PAL_WINDOWS: this->extra_cfg->palette |= GRFP_GRF_WINDOWS; break;
			default: break;
		}
		FillGRFDetails(*this->extra_cfg, false, BASESET_DIR);
	}
	return *this->extra_cfg;
}

bool GraphicsSet::IsConfigurable() const
{
	const GRFConfig &cfg = this->GetOrCreateExtraConfig();
	/* This check is more strict than the one for NewGRF Settings.
	 * There are no legacy basesets with parameters, but without Action14 */
	return !cfg.param_info.empty();
}

void GraphicsSet::CopyCompatibleConfig(const GraphicsSet &src)
{
	const GRFConfig *src_cfg = src.GetExtraConfig();
	if (src_cfg == nullptr || src_cfg->param.empty()) return;
	GRFConfig &dest_cfg = this->GetOrCreateExtraConfig();
	if (dest_cfg.IsCompatible(src_cfg->version)) return;
	dest_cfg.CopyParams(*src_cfg);
}

/**
 * Calculate and check the MD5 hash of the supplied GRF.
 * @param file The file get the hash of.
 * @param subdir The sub directory to get the files from.
 * @return
 * - #CR_MATCH if the MD5 hash matches
 * - #CR_MISMATCH if the MD5 does not match
 * - #CR_NO_FILE if the file misses
 */
/* static */ MD5File::ChecksumResult GraphicsSet::CheckMD5(const MD5File *file, Subdirectory subdir)
{
	size_t size = 0;
	auto f = FioFOpenFile(file->filename, "rb", subdir, &size);
	if (!f.has_value()) return MD5File::CR_NO_FILE;

	size_t max = GRFGetSizeOfDataSection(*f);

	return file->CheckMD5(subdir, max);
}


/**
 * Calculate and check the MD5 hash of the supplied filename.
 * @param subdir The sub directory to get the files from
 * @param max_size Only calculate the hash for this many bytes from the file start.
 * @return
 * - #CR_MATCH if the MD5 hash matches
 * - #CR_MISMATCH if the MD5 does not match
 * - #CR_NO_FILE if the file misses
 */
MD5File::ChecksumResult MD5File::CheckMD5(Subdirectory subdir, size_t max_size) const
{
	size_t size;
	auto f = FioFOpenFile(this->filename, "rb", subdir, &size);
	if (!f.has_value()) return CR_NO_FILE;

	size = std::min(size, max_size);

	Md5 checksum;
	uint8_t buffer[1024];
	MD5Hash digest;
	size_t len;

	while ((len = fread(buffer, 1, (size > sizeof(buffer)) ? sizeof(buffer) : size, *f)) != 0 && size != 0) {
		size -= len;
		checksum.Append(buffer, len);
	}

	checksum.Finish(digest);
	return this->hash == digest ? CR_MATCH : CR_MISMATCH;
}

/** Names corresponding to the GraphicsFileType */
static const std::string_view _graphics_file_names[] = { "base", "logos", "arctic", "tropical", "toyland", "extra" };

/** Implementation */
template <>
/* static */ std::span<const std::string_view> BaseSet<GraphicsSet>::GetFilenames()
{
	return _graphics_file_names;
}

template <>
/* static */ bool BaseMedia<GraphicsSet>::DetermineBestSet()
{
	if (BaseMedia<GraphicsSet>::used_set != nullptr) return true;

	const GraphicsSet *best = nullptr;

	auto IsBetter = [&best] (const GraphicsSet *current) {
		/* Nothing chosen yet. */
		if (best == nullptr) return true;
		/* Not being a fallback is better. */
		if (best->fallback && !current->fallback) return true;
		/* Having more valid files is better. */
		if (best->valid_files < current->valid_files) return true;
		/* Having (essentially) fewer valid files is worse. */
		if (best->valid_files != current->valid_files) return false;
		/* Having a later version of the same base set is better. */
		if (best->shortname == current->shortname && best->version < current->version) return true;
		/* The DOS palette is the better palette. */
		return best->palette != PAL_DOS && current->palette == PAL_DOS;
	};

	for (const auto &c : BaseMedia<GraphicsSet>::available_sets) {
		/* Skip unusable sets */
		if (c->GetNumMissing() != 0) continue;

		if (IsBetter(c.get())) best = c.get();
	}

	BaseMedia<GraphicsSet>::used_set = best;
	return BaseMedia<GraphicsSet>::used_set != nullptr;
}

template <>
/* static */ std::string_view BaseMedia<GraphicsSet>::GetExtension()
{
	return ".obg"; // OpenTTD Base Graphics
}

template class BaseMedia<GraphicsSet>;
