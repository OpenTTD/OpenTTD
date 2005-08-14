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

#define SPRITE_CACHE_SIZE 1024*1024

#define WANT_NEW_LRU


/* These are used in newgrf.c: */

int _skip_sprites = 0; // XXX
int _replace_sprites_count[16]; // XXX
int _replace_sprites_offset[16]; // XXX

const char* _cur_grffile; // XXX
int _loading_stage; // XXX
int _skip_specials; // XXX
uint16 _custom_sprites_base; // XXX


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


static int LoadGrfFile(const char *filename, int load_index, int file_index)
{
	int load_index_org = load_index;

	FioOpenFile(file_index, filename);

	/* Thou shalt use LoadNewGrfFile() if thou loadeth a GRF file that
	 * might contain some special sprites. */
	_skip_specials = 1;
	_skip_sprites = 0;

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

static int LoadNewGrfFile(const char *filename, int load_index, int file_index)
{
	int i;

	FioOpenFile(file_index, filename);
	_cur_grffile = filename;
	_skip_specials = 0;
	_skip_sprites = 0;

	DEBUG(spritecache, 2) ("Reading newgrf-file ``%s'' [offset: %u]",
			filename, load_index);

	/* Skip the first sprite; we don't care about how many sprites this
	 * does contain; newest TTDPatches and George's longvehicles don't
	 * neither, apparently. */
	{
		int length;
		byte type;

		length = FioReadWord();
		type = FioReadByte();

		if (length == 4 && type == 0xFF) {
			FioReadDword();
		} else {
			error("Custom .grf has invalid format.");
		}
	}

	for (i = 0; LoadNextSprite(load_index + i, file_index); i++) {
		if (load_index + i >= MAX_SPRITES)
			error("Too many sprites (0x%X). Recompile with higher MAX_SPRITES value or remove some custom GRF files.",
			      load_index + i);
	}

	/* Clean up. */
	_skip_sprites = 0;
	memset(_replace_sprites_count, 0, sizeof(_replace_sprites_count));
	memset(_replace_sprites_offset, 0, sizeof(_replace_sprites_offset));

	return i;
}

static void LoadGrfIndexed(const char *filename, const SpriteID *index_tbl, int file_index)
{
	int start;

	FioOpenFile(file_index, filename);
	_skip_specials = 1;
	_skip_sprites = 0;

	DEBUG(spritecache, 2) ("Reading indexed grf-file ``%s''", filename);

	for (; (start = *index_tbl++) != 0xffff;) {
		int end = *index_tbl++;
		if(start == 0xfffe) { // skip sprites (amount in second var)
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


static byte _sprite_page_to_load = 0xFF;

#define OPENTTD_SPRITES_COUNT 100
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
	SPR_OPENTTD_BASE+62, SPR_OPENTTD_BASE + OPENTTD_SPRITES_COUNT, // more icons
	0xffff,
};

/* FUNCTIONS FOR CHECKING MD5 SUMS OF GRF FILES */

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

static void LoadSpriteTables(void)
{
	int load_index = 0;
	uint i;
	uint j;
	const FileList *files; // list of grf files to be loaded. Either Windows files or DOS files

	_loading_stage = 1;

	/*
	 * TODO:
	 *   I think we can live entirely without Indexed GRFs, but I have to
	 *   invest that further. --octo
	 */

	files = _use_dos_palette? &files_dos : &files_win;

	for (i = 0; files->basic[i].filename != NULL; i++) {
		load_index += LoadGrfFile(files->basic[i].filename, load_index, i);
	}

	LoadGrfIndexed("openttd.grf", _openttd_grf_indexes, i++);

	if (_sprite_page_to_load != 0) {
		LoadGrfIndexed(
			files->landscape[_sprite_page_to_load - 1].filename,
			_landscape_spriteindexes[_sprite_page_to_load - 1],
			i++
		);
	}

	LoadGrfIndexed("trkfoundw.grf", _slopes_spriteindexes[_opt.landscape], i++);

	load_index = SPR_AUTORAIL_BASE;
	load_index += LoadGrfFile("autorail.grf", load_index, i++);

	load_index = SPR_CANALS_BASE;
	load_index += LoadGrfFile("canalsw.grf", load_index, i++);

	load_index = SPR_OPENTTD_BASE + OPENTTD_SPRITES_COUNT + 1;


	/* Load newgrf sprites
	 * in each loading stage, (try to) open each file specified in the config
	 * and load information from it. */
	_custom_sprites_base = load_index;
	for (_loading_stage = 0; _loading_stage < 2; _loading_stage++) {
		load_index = _custom_sprites_base;
		for (j = 0; j != lengthof(_newgrf_files) && _newgrf_files[j]; j++) {
			if (!FiosCheckFileExists(_newgrf_files[j])) {
				// TODO: usrerror()
				error("NewGRF file missing: %s", _newgrf_files[j]);
			}
			if (_loading_stage == 0) InitNewGRFFile(_newgrf_files[j], load_index);
			load_index += LoadNewGrfFile(_newgrf_files[j], load_index, i++);
			DEBUG(spritecache, 2) ("Currently %i sprites are loaded", load_index);
		}
	}
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
