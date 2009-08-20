/* $Id$ */

/** @file gfxinit.cpp Initializing of the (GRF) graphics. */

#include "stdafx.h"
#include "debug.h"
#include "spritecache.h"
#include "fileio_func.h"
#include "fios.h"
#include "newgrf.h"
#include "md5.h"
#include "fontcache.h"
#include "gfx_func.h"
#include "settings_type.h"
#include "string_func.h"

/* The type of set we're replacing */
#define SET_TYPE "graphics"
#include "base_media_func.h"

#include "table/sprites.h"
#include "table/palette_convert.h"

/** The currently used palette */
PaletteType _use_palette = PAL_AUTODETECT;
/** Whether the given NewGRFs must get a palette remap or not. */
bool _palette_remap_grf[MAX_FILE_SLOTS];
/** Palette map to go from the !_use_palette to the _use_palette */
const byte *_palette_remap = NULL;
/** Palette map to go from the _use_palette to the !_use_palette */
const byte *_palette_reverse_remap = NULL;

#include "table/landscape_sprite.h"

static const SpriteID * const _landscape_spriteindexes[] = {
	_landscape_spriteindexes_1,
	_landscape_spriteindexes_2,
	_landscape_spriteindexes_3,
};

static uint LoadGrfFile(const char *filename, uint load_index, int file_index)
{
	uint load_index_org = load_index;
	uint sprite_id = 0;

	FioOpenFile(file_index, filename);

	DEBUG(sprite, 2, "Reading grf-file '%s'", filename);

	while (LoadNextSprite(load_index, file_index, sprite_id)) {
		load_index++;
		sprite_id++;
		if (load_index >= MAX_SPRITES) {
			usererror("Too many sprites. Recompile with higher MAX_SPRITES value or remove some custom GRF files.");
		}
	}
	DEBUG(sprite, 2, "Currently %i sprites are loaded", load_index);

	return load_index - load_index_org;
}


void LoadSpritesIndexed(int file_index, uint *sprite_id, const SpriteID *index_tbl)
{
	uint start;
	while ((start = *index_tbl++) != END) {
		uint end = *index_tbl++;

		do {
			bool b = LoadNextSprite(start, file_index, *sprite_id);
			assert(b);
			(*sprite_id)++;
		} while (++start <= end);
	}
}

static void LoadGrfIndexed(const char *filename, const SpriteID *index_tbl, int file_index)
{
	uint sprite_id = 0;

	FioOpenFile(file_index, filename);

	DEBUG(sprite, 2, "Reading indexed grf-file '%s'", filename);

	LoadSpritesIndexed(file_index, &sprite_id, index_tbl);
}

/**
 * Checks whether the MD5 checksums of the files are correct.
 *
 * @note Also checks sample.cat and other required non-NewGRF GRFs for corruption.
 */
void CheckExternalFiles()
{
	if (BaseGraphics::GetUsedSet() == NULL || BaseSounds::GetUsedSet() == NULL) return;

	BaseGraphics::DeterminePalette();
	const GraphicsSet *used_set = BaseGraphics::GetUsedSet();

	DEBUG(grf, 1, "Using the %s base graphics set with the %s palette", used_set->name, _use_palette == PAL_DOS ? "DOS" : "Windows");

	static const size_t ERROR_MESSAGE_LENGTH = 256;
	static const size_t MISSING_FILE_MESSAGE_LENGTH = 128;

	/* Allocate for a message for each missing file and for one error
	 * message per set.
	 */
	char error_msg[MISSING_FILE_MESSAGE_LENGTH * (GraphicsSet::NUM_FILES + SoundsSet::NUM_FILES) + 2 * ERROR_MESSAGE_LENGTH];
	error_msg[0] = '\0';
	char *add_pos = error_msg;
	const char *last = lastof(error_msg);

	if (used_set->GetNumInvalid() != 0) {
		/* Not all files were loaded succesfully, see which ones */
		add_pos += seprintf(add_pos, last, "Trying to load graphics set '%s', but it is incomplete. The game will probably not run correctly until you properly install this set or select another one.\n\nThe following files are corrupted or missing:\n", used_set->name);
		for (uint i = 0; i < GraphicsSet::NUM_FILES; i++) {
			MD5File::ChecksumResult res = used_set->files[i].CheckMD5();
			if (res != MD5File::CR_MATCH) add_pos += seprintf(add_pos, last, "\t%s is %s (%s)\n", used_set->files[i].filename, res == MD5File::CR_MISMATCH ? "corrupt" : "missing", used_set->files[i].missing_warning);
		}
		add_pos += seprintf(add_pos, last, "\n");
	}

	const SoundsSet *sounds_set = BaseSounds::GetUsedSet();
	if (sounds_set->GetNumInvalid() != 0) {
		add_pos += seprintf(add_pos, last, "Trying to load sound set '%s', but it is incomplete. The game will probably not run correctly until you properly install this set or select another one.\n\nThe following files are corrupted or missing:\n", sounds_set->name);

		assert_compile(SoundsSet::NUM_FILES == 1);
		/* No need to loop each file, as long as there is only a single
		 * sound file. */
		add_pos += seprintf(add_pos, last, "\t%s is %s (%s)\n", sounds_set->files->filename, sounds_set->files->CheckMD5() == MD5File::CR_MISMATCH ? "corrupt" : "missing", sounds_set->files->missing_warning);
	}

	if (add_pos != error_msg) ShowInfoF("%s", error_msg);
}


static void LoadSpriteTables()
{
	memset(_palette_remap_grf, 0, sizeof(_palette_remap_grf));
	uint i = FIRST_GRF_SLOT;
	const GraphicsSet *used_set = BaseGraphics::GetUsedSet();

	_palette_remap_grf[i] = (_use_palette != used_set->palette);
	LoadGrfFile(used_set->files[GFT_BASE].filename, 0, i++);

	/*
	 * The second basic file always starts at the given location and does
	 * contain a different amount of sprites depending on the "type"; DOS
	 * has a few sprites less. However, we do not care about those missing
	 * sprites as they are not shown anyway (logos in intro game).
	 */
	_palette_remap_grf[i] = (_use_palette != used_set->palette);
	LoadGrfFile(used_set->files[GFT_LOGOS].filename, 4793, i++);

	/*
	 * Load additional sprites for climates other than temperate.
	 * This overwrites some of the temperate sprites, such as foundations
	 * and the ground sprites.
	 */
	if (_settings_game.game_creation.landscape != LT_TEMPERATE) {
		_palette_remap_grf[i] = (_use_palette != used_set->palette);
		LoadGrfIndexed(
			used_set->files[GFT_ARCTIC + _settings_game.game_creation.landscape - 1].filename,
			_landscape_spriteindexes[_settings_game.game_creation.landscape - 1],
			i++
		);
	}

	/* Initialize the unicode to sprite mapping table */
	InitializeUnicodeGlyphMap();

	/*
	 * Load the base NewGRF with OTTD required graphics as first NewGRF.
	 * However, we do not want it to show up in the list of used NewGRFs,
	 * so we have to manually add it, and then remove it later.
	 */
	GRFConfig *top = _grfconfig;
	GRFConfig *master = CallocT<GRFConfig>(1);
	master->filename = strdup(used_set->files[GFT_EXTRA].filename);
	FillGRFDetails(master, false);
	master->windows_paletted = (used_set->palette == PAL_WINDOWS);
	ClrBit(master->flags, GCF_INIT_ONLY);
	master->next = top;
	_grfconfig = master;

	LoadNewGRF(SPR_NEWGRFS_BASE, i);

	/* Free and remove the top element. */
	ClearGRFConfig(&master);
	_grfconfig = top;
}


void GfxLoadSprites()
{
	DEBUG(sprite, 2, "Loading sprite set %d", _settings_game.game_creation.landscape);

	GfxInitSpriteMem();
	LoadSpriteTables();
	GfxInitPalettes();
}

bool GraphicsSet::FillSetDetails(IniFile *ini, const char *path)
{
	bool ret = this->BaseSet<GraphicsSet, MAX_GFT>::FillSetDetails(ini, path);
	if (ret) {
		IniGroup *metadata = ini->GetGroup("metadata");
		IniItem *item;

		fetch_metadata("palette");
		this->palette = (*item->value == 'D' || *item->value == 'd') ? PAL_DOS : PAL_WINDOWS;
	}
	return true;
}


/**
 * Calculate and check the MD5 hash of the supplied filename.
 * @return
 *  CR_MATCH if the MD5 hash matches
 *  CR_MISMATCH if the MD5 does not match
 *  CR_NO_FILE if the file misses
 */
MD5File::ChecksumResult MD5File::CheckMD5() const
{
	size_t size;
	FILE *f = FioFOpenFile(this->filename, "rb", DATA_DIR, &size);

	if (f == NULL) return CR_NO_FILE;

	Md5 checksum;
	uint8 buffer[1024];
	uint8 digest[16];
	size_t len;

	while ((len = fread(buffer, 1, (size > sizeof(buffer)) ? sizeof(buffer) : size, f)) != 0 && size != 0) {
		size -= len;
		checksum.Append(buffer, len);
	}

	FioFCloseFile(f);

	checksum.Finish(digest);
	return memcmp(this->hash, digest, sizeof(this->hash)) == 0 ? CR_MATCH : CR_MISMATCH;
}

/** Names corresponding to the GraphicsFileType */
const char *_graphics_file_names[] = { "base", "logos", "arctic", "tropical", "toyland", "extra" };

/** Implementation */
template <class T, size_t Tnum_files>
/* static */ const char **BaseSet<T, Tnum_files>::file_names = _graphics_file_names;

extern void UpdateNewGRFConfigPalette();

/**
 * Determine the palette that has to be used.
 *  - forced palette via command line -> leave it that way
 *  - otherwise -> palette based on the graphics pack
 */
/* static */ void BaseGraphics::DeterminePalette()
{
	assert(BaseGraphics::used_set != NULL);
	if (_use_palette >= MAX_PAL) _use_palette = BaseGraphics::used_set->palette;

	switch (_use_palette) {
		case PAL_DOS:
			_palette_remap = _palmap_w2d;
			_palette_reverse_remap = _palmap_d2w;
			break;

		case PAL_WINDOWS:
			_palette_remap = _palmap_d2w;
			_palette_reverse_remap = _palmap_w2d;
			break;

		default:
			NOT_REACHED();
	}

	UpdateNewGRFConfigPalette();
}

template <class Tbase_set>
/* static */ bool BaseMedia<Tbase_set>::DetermineBestSet()
{
	if (BaseMedia<Tbase_set>::used_set != NULL) return true;

	const Tbase_set *best = NULL;
	for (const Tbase_set *c = BaseMedia<Tbase_set>::available_sets; c != NULL; c = c->next) {
		/* Skip unuseable sets */
		if (c->GetNumMissing() != 0) continue;

		if (best == NULL ||
				best->valid_files < c->valid_files ||
				(best->valid_files == c->valid_files && (
					(best->shortname == c->shortname && best->version < c->version) ||
					(best->palette != _use_palette && c->palette == _use_palette)))) {
			best = c;
		}
	}

	BaseMedia<Tbase_set>::used_set = best;
	return BaseMedia<Tbase_set>::used_set != NULL;
}

template <class Tbase_set>
/* static */ const char *BaseMedia<Tbase_set>::GetExtension()
{
	return ".obg"; // OpenTTD Base Graphics
}

INSTANTIATE_BASE_MEDIA_METHODS(BaseMedia<GraphicsSet>, GraphicsSet)
