/* $Id$ */

/** @file gfxinit.cpp Initializing of the (GRF) graphics. */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "gfxinit.h"
#include "spritecache.h"
#include "fileio.h"
#include "fios.h"
#include "newgrf.h"
#include "md5.h"
#include "variables.h"
#include "fontcache.h"
#include "gfx_func.h"
#include "core/alloc_func.hpp"
#include "core/bitmath_func.hpp"
#include <string.h>
#include "settings_type.h"
#include "string_func.h"

#include "table/sprites.h"

Palette _use_palette = PAL_AUTODETECT;

struct MD5File {
	const char * filename;     ///< filename
	uint8 hash[16];            ///< md5 sum of the file
};

/**
 * Information about a single graphics set.
 */
struct GraphicsSet {
	const char *name;          ///< The name of the graphics set
	const char *description;   ///< Description of the graphics set
	Palette palette;           ///< Palette of this graphics set
	MD5File basic[2];          ///< GRF files that always have to be loaded
	MD5File landscape[3];      ///< Landscape specific grf files
	const char *base_missing;  ///< Warning when one of the base GRF files is missing
	MD5File extra;             ///< NewGRF File with extra graphics loaded using Action 05
	const char *extra_missing; ///< Warning when the extra (NewGRF) file is missing
	uint found_grfs;           ///< Number of the GRFs that could be found
};

static const uint GRAPHICS_SET_GRF_COUNT = 6;
static int _use_graphics_set = -1;

#include "table/files.h"
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

static void LoadGrfIndexed(const char* filename, const SpriteID* index_tbl, int file_index)
{
	uint sprite_id = 0;

	FioOpenFile(file_index, filename);

	DEBUG(sprite, 2, "Reading indexed grf-file '%s'", filename);

	LoadSpritesIndexed(file_index, &sprite_id, index_tbl);
}


/**
 * Calculate and check the MD5 hash of the supplied filename.
 * @param file filename and expected MD5 hash for the given filename.
 * @return true if the checksum is correct.
 */
static bool FileMD5(const MD5File file)
{
	size_t size;
	FILE *f = FioFOpenFile(file.filename, "rb", DATA_DIR, &size);

	if (f != NULL) {
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
		return memcmp(file.hash, digest, sizeof(file.hash)) == 0;
	} else { // file not found
		return false;
	}
}

/**
 * Determine the graphics pack that has to be used.
 * The one with the most correct files wins.
 */
static void DetermineGraphicsPack()
{
	if (_use_graphics_set >= 0) return;

	uint max_index = 0;
	for (uint j = 1; j < lengthof(_graphics_sets); j++) {
		if (_graphics_sets[max_index].found_grfs < _graphics_sets[j].found_grfs) max_index = j;
	}

	_use_graphics_set = max_index;
}

/**
 * Determine the palette that has to be used.
 *  - forced palette via command line -> leave it that way
 *  - otherwise -> palette based on the graphics pack
 */
static void DeterminePalette()
{
	if (_use_palette < MAX_PAL) return;

	_use_palette = _graphics_sets[_use_graphics_set].palette;
}

/**
 * Checks whether the MD5 checksums of the files are correct.
 *
 * @note Also checks sample.cat and other required non-NewGRF GRFs for corruption.
 */
void CheckExternalFiles()
{
	DetermineGraphicsPack();
	DeterminePalette();

	static const size_t ERROR_MESSAGE_LENGTH = 128;
	const GraphicsSet *graphics = &_graphics_sets[_use_graphics_set];
	char error_msg[ERROR_MESSAGE_LENGTH * (GRAPHICS_SET_GRF_COUNT + 1)];
	error_msg[0] = '\0';
	char *add_pos = error_msg;

	for (uint i = 0; i < lengthof(graphics->basic); i++) {
		if (!FileMD5(graphics->basic[i])) {
			add_pos += snprintf(add_pos, ERROR_MESSAGE_LENGTH, "Your '%s' file is corrupted or missing! %s.\n", graphics->basic[i].filename, graphics->base_missing);
		}
	}

	for (uint i = 0; i < lengthof(graphics->landscape); i++) {
		if (!FileMD5(graphics->landscape[i])) {
			add_pos += snprintf(add_pos, ERROR_MESSAGE_LENGTH, "Your '%s' file is corrupted or missing! %s\n", graphics->landscape[i].filename, graphics->base_missing);
		}
	}

	bool sound = false;
	for (uint i = 0; !sound && i < lengthof(_sound_sets); i++) {
		sound = FileMD5(_sound_sets[i]);
	}

	if (!sound) {
		add_pos += snprintf(add_pos, ERROR_MESSAGE_LENGTH, "Your 'sample.cat' file is corrupted or missing! You can find 'sample.cat' on your Transport Tycoon Deluxe CD-ROM.\n");
	}

	if (!FileMD5(graphics->extra)) {
		add_pos += snprintf(add_pos, ERROR_MESSAGE_LENGTH, "Your '%s' file is corrupted or missing! %s\n", graphics->extra.filename, graphics->extra_missing);
	}

	if (add_pos != error_msg) ShowInfoF(error_msg);
}


static void LoadSpriteTables()
{
	const GraphicsSet *graphics = &_graphics_sets[_use_graphics_set];
	uint i = FIRST_GRF_SLOT;

	LoadGrfFile(graphics->basic[0].filename, 0, i++);

	/*
	 * The second basic file always starts at the given location and does
	 * contain a different amount of sprites depending on the "type"; DOS
	 * has a few sprites less. However, we do not care about those missing
	 * sprites as they are not shown anyway (logos in intro game).
	 */
	LoadGrfFile(graphics->basic[1].filename, 4793, i++);

	/*
	 * Load additional sprites for climates other than temperate.
	 * This overwrites some of the temperate sprites, such as foundations
	 * and the ground sprites.
	 */
	if (_settings_game.game_creation.landscape != LT_TEMPERATE) {
		LoadGrfIndexed(
			graphics->landscape[_settings_game.game_creation.landscape - 1].filename,
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
	master->filename = strdup(graphics->extra.filename);
	FillGRFDetails(master, false);
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

/**
 * Find all graphics sets and populate their data.
 */
void FindGraphicsSets()
{
	for (uint j = 0; j < lengthof(_graphics_sets); j++) {
		_graphics_sets[j].found_grfs = 0;
		for (uint i = 0; i < lengthof(_graphics_sets[j].basic); i++) {
			if (FioCheckFileExists(_graphics_sets[j].basic[i].filename)) _graphics_sets[j].found_grfs++;
		}
		for (uint i = 0; i < lengthof(_graphics_sets[j].landscape); i++) {
			if (FioCheckFileExists(_graphics_sets[j].landscape[i].filename)) _graphics_sets[j].found_grfs++;
		}
		if (FioCheckFileExists(_graphics_sets[j].extra.filename)) _graphics_sets[j].found_grfs++;
	}
}

/**
 * Set the graphics set to be used.
 * @param name of the graphics set to use
 * @return true if it could be loaded
 */
bool SetGraphicsSet(const char *name)
{
	if (StrEmpty(name)) {
		DetermineGraphicsPack();
		CheckExternalFiles();
		return true;
	}

	for (uint i = 0; i < lengthof(_graphics_sets); i++) {
		if (strcmp(name, _graphics_sets[i].name) == 0) {
			_use_graphics_set = i;
			CheckExternalFiles();
			return true;
		}
	}
	return false;
}

/**
 * Returns a list with the graphics sets.
 * @param p    where to print to
 * @param last the last character to print to
 * @return the last printed character
 */
char *GetGraphicsSetsList(char *p, const char *last)
{
	p += snprintf(p, last - p, "List of graphics sets:\n");
	for (uint i = 0; i < lengthof(_graphics_sets); i++) {
		if (_graphics_sets[i].found_grfs <= 1) continue;

		p += snprintf(p, last - p, "%18s: %s", _graphics_sets[i].name, _graphics_sets[i].description);
		int difference = GRAPHICS_SET_GRF_COUNT - _graphics_sets[i].found_grfs;
		if (difference != 0) {
			p += snprintf(p, last - p, " (missing %i file%s)\n", difference, difference == 1 ? "" : "s");
		} else {
			p += snprintf(p, last - p, "\n");
		}
	}
	p += snprintf(p, last - p, "\n");

	return p;
}
