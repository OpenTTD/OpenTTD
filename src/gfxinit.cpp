/* $Id$ */

/** @file gfxinit.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "gfxinit.h"
#include "spritecache.h"
#include "table/sprites.h"
#include "fileio.h"
#include "fios.h"
#include "string.h"
#include "newgrf.h"
#include "md5.h"
#include "variables.h"
#include "fontcache.h"
#include "gfx_func.h"
#include <string.h>

struct MD5File {
	const char * filename;     ///< filename
	uint8 hash[16];            ///< md5 sum of the file
};

struct FileList {
	MD5File basic[2];     ///< GRF files that always have to be loaded
	MD5File landscape[3]; ///< Landscape specific grf files
	MD5File sound;        ///< Sound samples
	MD5File chars;        ///< GRF File with character replacements
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

		if (start == SKIP) { // skip sprites (amount in second var)
			SkipSprites(end);
			(*sprite_id) += end;
		} else { // load sprites and use indexes from start to end
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

	if (!FileMD5(files->chars)) {
		add_pos += snprintf(add_pos, ERROR_MESSAGE_LENGTH, "Your '%s' file is corrupted or missing! The file was part of your installation.\n", files->chars.filename);
	}

	if (!FileMD5(files->openttd)) {
		add_pos += snprintf(add_pos, ERROR_MESSAGE_LENGTH, "Your '%s' file is corrupted or missing! The file was part of your installation.\n", files->openttd.filename);
	}

	if (add_pos != error_msg) ShowInfoF(error_msg);
}


static const SpriteID trg1idx[] = {
	   0,    1, ///< Mouse cursor, ZZZ
/* Medium font */
	   2,   92, ///< ' ' till 'z'
	SKIP,   36,
	 160,  160, ///< Move Ÿ to the correct position
	  98,   98, ///< Up arrow
	 131,  133,
	SKIP,    1, ///< skip currency sign
	 135,  135,
	SKIP,    1,
	 137,  137,
	SKIP,    1,
	 139,  139,
	 140,  140, ///< @todo Down arrow
	 141,  141,
	 142,  142, ///< @todo Check mark
	 143,  143, ///< @todo Cross
	 144,  144,
	 145,  145, ///< @todo Right arrow
	 146,  149,
	 118,  122, ///< Transport markers
	SKIP,    2,
	 157,  157,
	 114,  115, ///< Small up/down arrows
	SKIP,    1,
	 161,  225,
/* Small font */
	 226,  316, ///< ' ' till 'z'
	SKIP,   36,
	 384,  384, ///< Move Ÿ to the correct position
	 322,  322, ///< Up arrow
	 355,  357,
	SKIP,    1, ///< skip currency sign
	 359,  359,
	SKIP,    1,
	 361,  361,
	SKIP,    1,
	 363,  363,
	 364,  364, ////< @todo Down arrow
	 365,  366,
	SKIP,    1,
	 368,  368,
	 369,  369, ///< @todo Right arrow
	 370,  373,
	SKIP,    7,
	 381,  381,
	SKIP,    3,
	 385,  449,
/* Big font */
	 450,  540, ///< ' ' till 'z'
	SKIP,   36,
	 608,  608, ///< Move Ÿ to the correct position
	SKIP,    1,
	 579,  581,
	SKIP,    1,
	 583,  583,
	SKIP,    5,
	 589,  589,
	SKIP,   15,
	 605,  605,
	SKIP,    3,
	 609,  625,
	SKIP,    1,
	 627,  632,
	SKIP,    1,
	 634,  639,
	SKIP,    1,
	 641,  657,
	SKIP,    1,
	 659,  664,
	SKIP,    2,
	 667,  671,
	SKIP,    1,
	 673,  673,
/* Graphics */
	 674, 4792,
	END
};

/** Replace some letter sprites with some other letters */
static const SpriteID _chars_grf_indexes[] = {
	134, 134, ///<  euro symbol medium size
	582, 582, ///<  euro symbol large size
	358, 358, ///<  euro symbol tiny
	648, 648, ///<  nordic char: æ
	616, 616, ///<  nordic char: Æ
	666, 666, ///<  nordic char: ø
	634, 634, ///<  nordic char: Ø
	382, 383, ///<  Œ œ tiny
	158, 159, ///<  Œ œ medium
	606, 607, ///<  Œ œ large
	360, 360, ///<  Š tiny
	362, 362, ///<  š tiny
	136, 136, ///<  Š medium
	138, 138, ///<  š medium
	584, 584, ///<  Š large
	586, 586, ///<  š large
	626, 626, ///<  Ð large
	658, 658, ///<  ð large
	374, 374, ///<  Ž tiny
	378, 378, ///<  ž tiny
	150, 150, ///<  Ž medium
	154, 154, ///<  ž medium
	598, 598, ///<  Ž large
	602, 602, ///<  ž large
	640, 640, ///<  Þ large
	672, 672, ///<  þ large
	380, 380, ///<  º tiny
	156, 156, ///<  º medium
	604, 604, ///<  º large
	317, 320, ///<  { | } ~ tiny
	 93,  96, ///<  { | } ~ medium
	541, 544, ///<  { | } ~ large
	585, 585, ///<  § large
	587, 587, ///<  © large
	592, 592, ///<  ® large
	594, 597, ///<  ° ± ² ³ large
	633, 633, ///<  × large
	665, 665, ///<  ÷ large
	377, 377, ///<  · small
	153, 153, ///<  · medium
	601, 601, ///<  · large
	END
};


static void LoadSpriteTables()
{
	const FileList *files = _use_dos_palette ? &files_dos : &files_win;
	uint i = FIRST_GRF_SLOT;

	LoadGrfIndexed(files->basic[0].filename, trg1idx, i++);
	DupSprite(  2, 130); // non-breaking space medium
	DupSprite(226, 354); // non-breaking space tiny
	DupSprite(450, 578); // non-breaking space large

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
	if (_opt.landscape != LT_TEMPERATE) {
		LoadGrfIndexed(
			files->landscape[_opt.landscape - 1].filename,
			_landscape_spriteindexes[_opt.landscape - 1],
			i++
		);
	}

	LoadGrfIndexed(files->chars.filename, _chars_grf_indexes, i++);

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
	DEBUG(sprite, 2, "Loading sprite set %d", _opt.landscape);

	GfxInitSpriteMem();
	LoadSpriteTables();
	GfxInitPalettes();
}
