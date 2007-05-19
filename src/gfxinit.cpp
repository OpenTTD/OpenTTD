/* $Id$ */

/** @file gfxinit.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "gfx.h"
#include "gfxinit.h"
#include "spritecache.h"
#include "table/sprites.h"
#include "fileio.h"
#include "string.h"
#include "newgrf.h"
#include "md5.h"
#include "variables.h"
#include "fontcache.h"
#include <string.h>

struct MD5File {
	const char * filename;     ///< filename
	md5_byte_t hash[16];       ///< md5 sum of the file
};

struct FileList {
	MD5File basic[4];          ///< grf files that always have to be loaded
	MD5File landscape[3];      ///< landscape specific grf files
};

enum {
	SKIP = 0xFFFE,
	END  = 0xFFFF
};

#include "table/files.h"
#include "table/landscape_sprite.h"

static const SpriteID * const _landscape_spriteindexes[] = {
	_landscape_spriteindexes_1,
	_landscape_spriteindexes_2,
	_landscape_spriteindexes_3,
};

static const SpriteID * const _slopes_spriteindexes[] = {
	_slopes_spriteindexes_0,
	_slopes_spriteindexes_1,
	_slopes_spriteindexes_2,
	_slopes_spriteindexes_3,
};


static uint LoadGrfFile(const char* filename, uint load_index, int file_index)
{
	uint load_index_org = load_index;

	FioOpenFile(file_index, filename);

	DEBUG(sprite, 2, "Reading grf-file '%s'", filename);

	while (LoadNextSprite(load_index, file_index)) {
		load_index++;
		if (load_index >= MAX_SPRITES) {
			error("Too many sprites. Recompile with higher MAX_SPRITES value or remove some custom GRF files.");
		}
	}
	DEBUG(sprite, 2, "Currently %i sprites are loaded", load_index);

	return load_index - load_index_org;
}


static void LoadGrfIndexed(const char* filename, const SpriteID* index_tbl, int file_index)
{
	uint start;

	FioOpenFile(file_index, filename);

	DEBUG(sprite, 2, "Reading indexed grf-file '%s'", filename);

	while ((start = *index_tbl++) != END) {
		uint end = *index_tbl++;

		if (start == SKIP) { // skip sprites (amount in second var)
			SkipSprites(end);
		} else { // load sprites and use indexes from start to end
			do {
			#ifdef NDEBUG
				LoadNextSprite(start, file_index);
			#else
				bool b = LoadNextSprite(start, file_index);
				assert(b);
			#endif
			} while (++start <= end);
		}
	}
}


/* Check that the supplied MD5 hash matches that stored for the supplied filename */
static bool CheckMD5Digest(const MD5File file, md5_byte_t *digest, bool warn)
{
	if (memcmp(file.hash, digest, sizeof(file.hash)) == 0) return true;
	if (warn) fprintf(stderr, "MD5 of %s is ****INCORRECT**** - File Corrupt.\n", file.filename);
	return false;
}

/* Calculate and check the MD5 hash of the supplied filename.
 * returns true if the checksum is correct */
static bool FileMD5(const MD5File file, bool warn)
{
	FILE *f;
	char buf[MAX_PATH];

	/* open file */
	snprintf(buf, lengthof(buf), "%s%s", _paths.data_dir, file.filename);
	f = fopen(buf, "rb");

#if !defined(WIN32)
	if (f == NULL) {
		strtolower(buf + strlen(_paths.data_dir) - 1);
		f = fopen(buf, "rb");
	}
#endif

#if defined SECOND_DATA_DIR
	/* If we failed to find the file in the first data directory, we will try the other one */

	if (f == NULL) {
		snprintf(buf, lengthof(buf), "%s%s", _paths.second_data_dir, file.filename);
		f = fopen(buf, "rb");

		if (f == NULL) {
			strtolower(buf + strlen(_paths.second_data_dir) - 1);
			f = fopen(buf, "rb");
		}
	}
#endif

	if (f != NULL) {
		md5_state_t filemd5state;
		md5_byte_t buffer[1024];
		md5_byte_t digest[16];
		size_t len;

		md5_init(&filemd5state);
		while ((len = fread(buffer, 1, sizeof(buffer), f)) != 0)
			md5_append(&filemd5state, buffer, len);

		if (ferror(f) && warn) ShowInfoF("Error Reading from %s \n", buf);
		fclose(f);

		md5_finish(&filemd5state, digest);
		return CheckMD5Digest(file, digest, warn);
	} else { // file not found
		return false;
	}
}

/* Checks, if either the Windows files exist (TRG1R.GRF) or the DOS files (TRG1.GRF)
 * by comparing the MD5 checksums of the files. _use_dos_palette is set accordingly.
 * If neither are found, Windows palette is assumed.
 *
 * (Note: Also checks sample.cat for corruption) */
void CheckExternalFiles()
{
	uint i;
	/* count of files from this version */
	uint dos = 0;
	uint win = 0;

	for (i = 0; i < 2; i++) if (FileMD5(files_dos.basic[i], true)) dos++;
	for (i = 0; i < 3; i++) if (FileMD5(files_dos.landscape[i], true)) dos++;

	for (i = 0; i < 2; i++) if (FileMD5(files_win.basic[i], true)) win++;
	for (i = 0; i < 3; i++) if (FileMD5(files_win.landscape[i], true)) win++;

	if (!FileMD5(sample_cat_win, false) && !FileMD5(sample_cat_dos, false))
		ShowInfo("Your 'sample.cat' file is corrupted or missing!");

	for (i = 0; i < lengthof(files_openttd); i++) {
		if (!FileMD5(files_openttd[i], false)) {
			ShowInfoF("Your '%s' file is corrupted or missing!", files_openttd[i].filename);
		}
	}

	/*
	 * forced DOS palette via command line -> leave it that way
	 * all Windows files present -> Windows palette
	 * all DOS files present -> DOS palette
	 * no Windows files present and any DOS file present -> DOS palette
	 * otherwise -> Windows palette
	 */
	if (_use_dos_palette) {
		return;
	} else if (win == 5) {
		_use_dos_palette = false;
	} else if (dos == 5 || (win == 0 && dos > 0)) {
		_use_dos_palette = true;
	} else {
		_use_dos_palette = false;
	}
}


static const SpriteID trg1idx[] = {
	   0,    1, ///< Mouse cursor, ZZZ
/* Medium font */
	   2,   92, ///< ' ' till 'z'
	SKIP,   36,
	 160,  160, ///< Move ¾ to the correct position
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
	 384,  384, ///< Move ¾ to the correct position
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
	 608,  608, ///< Move ¾ to the correct position
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

/* NOTE: When adding a normal sprite, increase OPENTTD_SPRITES_COUNT with the
 * amount of sprites and add them to the end of the list, with the index of
 * the old sprite-count offset from SPR_OPENTTD_BASE. With this there is no
 * correspondence of any kind with the ID's in the grf file, but results in
 * a maximum use of sprite slots. */
static const SpriteID _openttd_grf_indexes[] = {
	SPR_IMG_AUTORAIL, SPR_CURSOR_WAYPOINT, // icons etc
	134, 134,  ///< euro symbol medium size
	582, 582,  ///<  euro symbol large size
	358, 358,  ///<  euro symbol tiny
	SPR_CURSOR_CANAL, SPR_IMG_FASTFORWARD, // more icons
	648, 648, ///<  nordic char: æ
	616, 616, ///<  nordic char: Æ
	666, 666, ///<  nordic char: ø
	634, 634, ///<  nordic char: Ø
	SPR_PIN_UP, SPR_CURSOR_CLONE_TRAIN, // more icons
	382, 383, ///<  ¼ ½ tiny
	158, 159, ///<  ¼ ½ medium
	606, 607, ///<  ¼ ½ large
	360, 360, ///<  ¦ tiny
	362, 362, ///<  ¨ tiny
	136, 136, ///<  ¦ medium
	138, 138, ///<  ¨ medium
	584, 584, ///<  ¦ large
	586, 586, ///<  ¨ large
	626, 626, ///<  Ð large
	658, 658, ///<  ð large
	374, 374, ///<  ´ tiny
	378, 378, ///<  ¸ tiny
	150, 150, ///<  ´ medium
	154, 154, ///<  ¸ medium
	598, 598, ///<  ´ large
	602, 602, ///<  ¸ large
	640, 640, ///<  Þ large
	672, 672, ///<  þ large
	380, 380, ///<  º tiny
	156, 156, ///<  º medium
	604, 604, ///<  º large
	317, 320, ///<  { | } ~ tiny
	 93,  96, ///<  { | } ~ medium
	541, 544, ///<  { | } ~ large
	SPR_HOUSE_ICON, SPR_HOUSE_ICON,
	585, 585, ///<  § large
	587, 587, ///<  © large
	592, 592, ///<  ® large
	594, 597, ///<  ° ± ² ³ large
	633, 633, ///<  × large
	665, 665, ///<  ÷ large
	SPR_SELL_TRAIN, SPR_SHARED_ORDERS_ICON,
	377, 377, ///<  · small
	153, 153, ///<  · medium
	601, 601, ///<  · large
	SPR_WARNING_SIGN, SPR_WARNING_SIGN,
	END
};


static void LoadSpriteTables()
{
	const FileList* files = _use_dos_palette ? &files_dos : &files_win;
	uint load_index;
	uint i;

	LoadGrfIndexed(files->basic[0].filename, trg1idx, 0);
	DupSprite(  2, 130); // non-breaking space medium
	DupSprite(226, 354); // non-breaking space tiny
	DupSprite(450, 578); // non-breaking space large
	load_index = 4793;

	for (i = 1; files->basic[i].filename != NULL; i++) {
		load_index += LoadGrfFile(files->basic[i].filename, load_index, i);
	}

	/* Load additional sprites for climates other than temperate */
	if (_opt.landscape != LT_TEMPERATE) {
		LoadGrfIndexed(
			files->landscape[_opt.landscape - 1].filename,
			_landscape_spriteindexes[_opt.landscape - 1],
			i++
		);
	}

	assert(load_index == SPR_SIGNALS_BASE);
	load_index += LoadGrfFile("nsignalsw.grf", load_index, i++);

	assert(load_index == SPR_CANALS_BASE);
	load_index += LoadGrfFile("canalsw.grf", load_index, i++);

	assert(load_index == SPR_SLOPES_BASE);
	LoadGrfIndexed("trkfoundw.grf", _slopes_spriteindexes[_opt.landscape], i++);

	load_index = SPR_AUTORAIL_BASE;
	load_index += LoadGrfFile("autorail.grf", load_index, i++);

	assert(load_index == SPR_ELRAIL_BASE);
	load_index += LoadGrfFile("elrailsw.grf", load_index, i++);

	assert(load_index == SPR_2CCMAP_BASE);
	load_index += LoadGrfFile("2ccmap.grf", load_index, i++);

	assert(load_index == SPR_OPENTTD_BASE);
	LoadGrfIndexed("openttd.grf", _openttd_grf_indexes, i++);
	load_index = SPR_OPENTTD_BASE + OPENTTD_SPRITES_COUNT;

	assert(load_index == SPR_AIRPORTX_BASE);
	load_index += LoadGrfFile("airports.grf", load_index, i++);

	assert(load_index == SPR_ROADSTOP_BASE);
	load_index += LoadGrfFile("roadstops.grf", load_index, i++);

	assert(load_index == SPR_GROUP_BASE);
	load_index += LoadGrfFile("group.grf", load_index, i++);

	/* Initialize the unicode to sprite mapping table */
	InitializeUnicodeGlyphMap();

	LoadNewGRF(load_index, i);
}


void GfxLoadSprites()
{
	DEBUG(sprite, 2, "Loading sprite set %d", _opt.landscape);

	GfxInitSpriteMem();
	LoadSpriteTables();
	GfxInitPalettes();
}
