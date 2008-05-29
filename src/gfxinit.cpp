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

#include "table/sprites.h"

struct MD5File {
	const char * filename;     ///< filename
	uint8 hash[16];            ///< md5 sum of the file
};

struct FileList {
	MD5File basic[2];     ///< GRF files that always have to be loaded
	MD5File landscape[3]; ///< Landscape specific grf files
	MD5File sound;        ///< Sound samples
	MD5File openttd;      ///< GRF File with OTTD specific graphics
};

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
			error("Too many sprites. Recompile with higher MAX_SPRITES value or remove some custom GRF files.");
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
		#ifdef NDEBUG
			LoadNextSprite(start, file_index, *sprite_id);
		#else
			bool b = LoadNextSprite(start, file_index, *sprite_id);
			assert(b);
		#endif
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
 * Determine the palette that has to be used.
 *  - forced DOS palette via command line -> leave it that way
 *  - all Windows files present -> Windows palette
 *  - all DOS files present -> DOS palette
 *  - no Windows files present and any DOS file present -> DOS palette
 *  - otherwise -> Windows palette
 */
static void DeterminePalette()
{
	if (_use_dos_palette) return;

	/* Count of files from the different versions. */
	uint dos = 0;
	uint win = 0;

	for (uint i = 0; i < lengthof(files_dos.basic); i++) if (FioCheckFileExists(files_dos.basic[i].filename)) dos++;
	for (uint i = 0; i < lengthof(files_dos.landscape); i++) if (FioCheckFileExists(files_dos.landscape[i].filename)) dos++;

	for (uint i = 0; i < lengthof(files_win.basic); i++) if (FioCheckFileExists(files_win.basic[i].filename)) win++;
	for (uint i = 0; i < lengthof(files_win.landscape); i++) if (FioCheckFileExists(files_win.landscape[i].filename)) win++;

	if (win == 5) {
		_use_dos_palette = false;
	} else if (dos == 5 || (win == 0 && dos > 0)) {
		_use_dos_palette = true;
	} else {
		_use_dos_palette = false;
	}
}

/**
 * Checks whether the MD5 checksums of the files are correct.
 *
 * @note Also checks sample.cat and other required non-NewGRF GRFs for corruption.
 */
void CheckExternalFiles()
{
	DeterminePalette();

	static const size_t ERROR_MESSAGE_LENGTH = 128;
	const FileList *files = _use_dos_palette ? &files_dos : &files_win;
	char error_msg[ERROR_MESSAGE_LENGTH * (lengthof(files->basic) + lengthof(files->landscape) + 3)];
	error_msg[0] = '\0';
	char *add_pos = error_msg;

	for (uint i = 0; i < lengthof(files->basic); i++) {
		if (!FileMD5(files->basic[i])) {
			add_pos += snprintf(add_pos, ERROR_MESSAGE_LENGTH, "Your '%s' file is corrupted or missing! You can find '%s' on your Transport Tycoon Deluxe CD-ROM.\n", files->basic[i].filename, files->basic[i].filename);
		}
	}

	for (uint i = 0; i < lengthof(files->landscape); i++) {
		if (!FileMD5(files->landscape[i])) {
			add_pos += snprintf(add_pos, ERROR_MESSAGE_LENGTH, "Your '%s' file is corrupted or missing! You can find '%s' on your Transport Tycoon Deluxe CD-ROM.\n", files->landscape[i].filename, files->landscape[i].filename);
		}
	}

	if (!FileMD5(files_win.sound) && !FileMD5(files_dos.sound)) {
		add_pos += snprintf(add_pos, ERROR_MESSAGE_LENGTH, "Your 'sample.cat' file is corrupted or missing! You can find 'sample.cat' on your Transport Tycoon Deluxe CD-ROM.\n");
	}

	if (!FileMD5(files->openttd)) {
		add_pos += snprintf(add_pos, ERROR_MESSAGE_LENGTH, "Your '%s' file is corrupted or missing! The file was part of your installation.\n", files->openttd.filename);
	}

	if (add_pos != error_msg) ShowInfoF(error_msg);
}


static void LoadSpriteTables()
{
	const FileList *files = _use_dos_palette ? &files_dos : &files_win;
	uint i = FIRST_GRF_SLOT;

	LoadGrfFile(files->basic[0].filename, 0, i++);

	/*
	 * The second basic file always starts at the given location and does
	 * contain a different amount of sprites depending on the "type"; DOS
	 * has a few sprites less. However, we do not care about those missing
	 * sprites as they are not shown anyway (logos in intro game).
	 */
	LoadGrfFile(files->basic[1].filename, 4793, i++);

	/*
	 * Load additional sprites for climates other than temperate.
	 * This overwrites some of the temperate sprites, such as foundations
	 * and the ground sprites.
	 */
	if (_settings_game.game_creation.landscape != LT_TEMPERATE) {
		LoadGrfIndexed(
			files->landscape[_settings_game.game_creation.landscape - 1].filename,
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
	master->filename = strdup(files->openttd.filename);
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
