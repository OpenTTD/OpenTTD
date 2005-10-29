/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "gfx.h"
#include "gfxinit.h"
#include "spritecache.h"
#include "table/sprites.h"
#include "fileio.h"
#include "newgrf.h"
#include "md5.h"
#include "variables.h"
#include <ctype.h>

typedef struct MD5File {
	const char * const filename;     // filename
	const md5_byte_t hash[16]; // md5 sum of the file
} MD5File;

typedef struct FileList {
	const MD5File basic[5];     // grf files that always have to be loaded
	const MD5File landscape[3]; // landscape specific grf files
} FileList;

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

	DEBUG(spritecache, 2) ("Reading grf-file ``%s''", filename);

	while (LoadNextSprite(load_index, file_index)) {
		load_index++;
		if (load_index >= MAX_SPRITES) {
			error("Too many sprites. Recompile with higher MAX_SPRITES value or remove some custom GRF files.");
		}
	}
	DEBUG(spritecache, 2) ("Currently %i sprites are loaded", load_index);

	return load_index - load_index_org;
}


static void LoadGrfIndexed(const char* filename, const SpriteID* index_tbl, int file_index)
{
	uint start;

	FioOpenFile(file_index, filename);

	DEBUG(spritecache, 2) ("Reading indexed grf-file ``%s''", filename);

	for (; (start = *index_tbl++) != 0xffff;) {
		uint end = *index_tbl++;

		if (start == 0xfffe) { // skip sprites (amount in second var)
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
	uint i;

	/* Loop through each byte of the file MD5 and the stored MD5... */
	for (i = 0; i < 16; i++) if (file.hash[i] != digest[i]) break;

		/* If all bytes of the MD5's match (i.e. the MD5's match)... */
	if (i == 16) {
		return true;
	} else {
		if (warn) fprintf(stderr, "MD5 of %s is ****INCORRECT**** - File Corrupt.\n", file.filename);
		return false;
	};
}

/* Calculate and check the MD5 hash of the supplied filename.
 * returns true if the checksum is correct */
static bool FileMD5(const MD5File file, bool warn)
{
	FILE *f;
	char buf[MAX_PATH];

	// open file
	sprintf(buf, "%s%s", _path.data_dir, file.filename);
	f = fopen(buf, "rb");

#if !defined(WIN32)
	if (f == NULL) {
		char *s;
	// make lower case and check again
		for (s = buf + strlen(_path.data_dir) - 1; *s != '\0'; s++)
			*s = tolower(*s);
		f = fopen(buf, "rb");
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

		if (ferror(f) && warn) fprintf(stderr, "Error Reading from %s \n", buf);
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
void CheckExternalFiles(void)
{
	uint i;
	// count of files from this version
	uint dos = 0;
	uint win = 0;

	for (i = 0; i < 2; i++) if (FileMD5(files_dos.basic[i], true)) dos++;
	for (i = 0; i < 3; i++) if (FileMD5(files_dos.landscape[i], true)) dos++;

	for (i = 0; i < 2; i++) if (FileMD5(files_win.basic[i], true)) win++;
	for (i = 0; i < 3; i++) if (FileMD5(files_win.landscape[i], true)) win++;

	if (!FileMD5(sample_cat_win, false) && !FileMD5(sample_cat_dos, false))
		fprintf(stderr, "Your sample.cat file is corrupted or missing!\n");

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
	     0,    1, // Mouse cursor, ZZZ
/* Medium font */
	     2,   92, // ' ' till 'z'
	0xFFFE,   36,
	   160,  160, // Move ¾ to the correct position
	    98,   98, // Up arrow
	   131,  133,
	0xFFFE,    1, // skip currency sign
	   135,  135,
	0xFFFE,    1,
	   137,  137,
	0xFFFE,    1,
	   139,  139,
	   140,  140, // TODO Down arrow
	   141,  141,
	   142,  142, // TODO Check mark
	   143,  143, // TODO Cross
	   144,  144,
	   145,  145, // TODO Right arrow
	   146,  149,
	   118,  122, // Transport markers
	0xFFFE,    2,
	   157,  157,
	   114,  115, // Small up/down arrows
	0xFFFE,    1,
	   161,  225,
/* Small font */
	   226,  316, // ' ' till 'z'
	0xFFFE,   36,
	   384,  384, // Move ¾ to the correct position
	   322,  322, // Up arrow
	   355,  357,
	0xFFFE,    1, // skip currency sign
	   359,  359,
	0xFFFE,    1,
	   361,  361,
	0xFFFE,    1,
	   363,  363,
	   364,  364, // TODO Down arrow
	   365,  366,
	0xFFFE,    1,
	   368,  368,
	   369,  369, // TODO Right arrow
	   370,  373,
	0xFFFE,    7,
	   381,  381,
	0xFFFE,    3,
	   385,  449,
/* Big font */
	   450,  540, // ' ' till 'z'
	0xFFFE,   36,
	   608,  608, // Move ¾ to the correct position
	0xFFFE,    1,
	   579,  581,
	0xFFFE,    1,
	   583,  583,
	0xFFFE,    5,
	   589,  589,
	0xFFFE,   15,
	   605,  605,
	0xFFFE,    3,
	   609,  625,
	0xFFFE,    1,
	   627,  632,
	0xFFFE,    1,
	   634,  639,
	0xFFFE,    1,
	   641,  657,
	0xFFFE,    1,
	   659,  664,
	0xFFFE,    2,
	   667,  671,
	0xFFFE,    1,
	   673,  673,
/* Graphics */
	   674, 4792,
	0xFFFF
};

/* NOTE: When adding a normal sprite, increase OPENTTD_SPRITES_COUNT with the
 * amount of sprites and add them to the end of the list, with the index of
 * the old sprite-count offset from SPR_OPENTTD_BASE. With this there is no
 * correspondence of any kind with the ID's in the grf file, but results in
 * a maximum use of sprite slots. */
#define OPENTTD_SPRITES_COUNT 101
static const SpriteID _openttd_grf_indexes[] = {
	SPR_OPENTTD_BASE + 0, SPR_OPENTTD_BASE + 7, // icons etc
	134, 134,  // euro symbol medium size
	582, 582,  // euro symbol large size
	358, 358,  // euro symbol tiny
	SPR_OPENTTD_BASE+11, SPR_OPENTTD_BASE+57, // more icons
	648, 648, // nordic char: æ
	616, 616, // nordic char: Æ
	666, 666, // nordic char: ø
	634, 634, // nordic char: Ø
	SPR_OPENTTD_BASE+62, SPR_OPENTTD_BASE + 100, // more icons
	382, 383, // ¼ ½ tiny
	158, 159, // ¼ ½ medium
	606, 607, // ¼ ½ large
	360, 360, // ¦ tiny
	362, 362, // ¨ tiny
	136, 136, // ¦ medium
	138, 138, // ¨ medium
	584, 584, // ¦ large
	586, 586, // ¨ large
	626, 626, // Ð large
	658, 658, // ð large
	374, 374, // ´ tiny
	378, 378, // ¸ tiny
	150, 150, // ´ medium
	154, 154, // ¸ medium
	598, 598, // ´ large
	602, 602, // ¸ large
	640, 640, // Þ large
	672, 672, // þ large
	380, 380, // º tiny
	156, 156, // º medium
	604, 604, // º large
	317, 320, // { | } ~ tiny
	 93,  96, // { | } ~ medium
	541, 544, // { | } ~ large
	SPR_OPENTTD_BASE + 101, SPR_OPENTTD_BASE + OPENTTD_SPRITES_COUNT, // icon (HOUSE)
	0xffff,
};

static byte _sprite_page_to_load = 0xFF;

static void LoadSpriteTables(void)
{
	uint load_index = 0;
	uint i;
	const FileList *files; // list of grf files to be loaded. Either Windows files or DOS files

	files = _use_dos_palette? &files_dos : &files_win;

	LoadGrfIndexed(files->basic[0].filename, trg1idx, 0);
	DupSprite(  2, 130); // non-breaking space medium
	DupSprite(226, 354); // non-breaking space tiny
	DupSprite(450, 578); // non-breaking space large
	load_index = 4793;

	for (i = 1; files->basic[i].filename != NULL; i++) {
		load_index += LoadGrfFile(files->basic[i].filename, load_index, i);
	}

	if (_sprite_page_to_load != 0) {
		LoadGrfIndexed(
			files->landscape[_sprite_page_to_load - 1].filename,
			_landscape_spriteindexes[_sprite_page_to_load - 1],
			i++
		);
	}

	assert(load_index == SPR_CANALS_BASE);
	load_index += LoadGrfFile("canalsw.grf", load_index, i++);

	assert(load_index == SPR_SLOPES_BASE);
	LoadGrfIndexed("trkfoundw.grf", _slopes_spriteindexes[_opt.landscape], i++);

	load_index = SPR_AUTORAIL_BASE;
	load_index += LoadGrfFile("autorail.grf", load_index, i++);

	assert(load_index == SPR_OPENTTD_BASE);
	LoadGrfIndexed("openttd.grf", _openttd_grf_indexes, i++);
	load_index = SPR_OPENTTD_BASE + OPENTTD_SPRITES_COUNT + 1;

	LoadNewGRF(load_index, i);
}


void GfxLoadSprites(void)
{
	// Need to reload the sprites only if the landscape changed
	if (_sprite_page_to_load != _opt.landscape) {
		_sprite_page_to_load = _opt.landscape;

		// Sprite cache
		DEBUG(spritecache, 1) ("Loading sprite set %d.", _sprite_page_to_load);

		GfxInitSpriteMem();
		LoadSpriteTables();
		GfxInitPalettes();
	}
}
