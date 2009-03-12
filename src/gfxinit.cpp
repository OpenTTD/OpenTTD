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
#include "ini_type.h"

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

char *_ini_graphics_set;

/** Structure holding filename and MD5 information about a single file */
struct MD5File {
	const char *filename;        ///< filename
	uint8 hash[16];              ///< md5 sum of the file
	const char *missing_warning; ///< warning when this file is missing
};

/** Types of graphics in the base graphics set */
enum GraphicsFileType {
	GFT_BASE,     ///< Base sprites for all climates
	GFT_LOGOS,    ///< Logos, landscape icons and original terrain generator sprites
	GFT_ARCTIC,   ///< Landscape replacement sprites for arctic
	GFT_TROPICAL, ///< Landscape replacement sprites for tropical
	GFT_TOYLAND,  ///< Landscape replacement sprites for toyland
	GFT_EXTRA,    ///< Extra sprites that were not part of the original sprites
	MAX_GFT       ///< We are looking for this amount of GRFs
};

/** Information about a single graphics set. */
struct GraphicsSet {
	const char *name;          ///< The name of the graphics set
	const char *description;   ///< Description of the graphics set
	uint32 shortname;          ///< Four letter short variant of the name
	uint32 version;            ///< The version of this graphics set
	PaletteType palette;       ///< Palette of this graphics set

	MD5File files[MAX_GFT];    ///< All GRF files part of this set
	uint found_grfs;           ///< Number of the GRFs that could be found

	GraphicsSet *next;         ///< The next graphics set in this list

	/** Free everything we allocated */
	~GraphicsSet()
	{
		free((void*)this->name);
		free((void*)this->description);
		for (uint i = 0; i < MAX_GFT; i++) {
			free((void*)this->files[i].filename);
			free((void*)this->files[i].missing_warning);
		}

		delete this->next;
	}
};

/** All graphics sets currently available */
static GraphicsSet *_available_graphics_sets = NULL;
/** The one and only graphics set that is currently being used. */
static const GraphicsSet *_used_graphics_set = NULL;

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

static void LoadGrfIndexed(const char *filename, const SpriteID *index_tbl, int file_index)
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
static bool DetermineGraphicsPack()
{
	if (_used_graphics_set != NULL) return true;

	const GraphicsSet *best = _available_graphics_sets;
	for (const GraphicsSet *c = _available_graphics_sets; c != NULL; c = c->next) {
		if (best->found_grfs < c->found_grfs ||
				(best->found_grfs == c->found_grfs && (
					(best->shortname == c->shortname && best->version < c->version) ||
					(best->palette != _use_palette && c->palette == _use_palette)))) {
			best = c;
		}
	}

	_used_graphics_set = best;
	return _used_graphics_set != NULL;
}

extern void UpdateNewGRFConfigPalette();

/**
 * Determine the palette that has to be used.
 *  - forced palette via command line -> leave it that way
 *  - otherwise -> palette based on the graphics pack
 */
static void DeterminePalette()
{
	assert(_used_graphics_set != NULL);
	if (_use_palette >= MAX_PAL) _use_palette = _used_graphics_set->palette;

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

/**
 * Checks whether the MD5 checksums of the files are correct.
 *
 * @note Also checks sample.cat and other required non-NewGRF GRFs for corruption.
 */
void CheckExternalFiles()
{
	DeterminePalette();

	DEBUG(grf, 1, "Using the %s base graphics set with the %s palette", _used_graphics_set->name, _use_palette == PAL_DOS ? "DOS" : "Windows");

	static const size_t ERROR_MESSAGE_LENGTH = 128;
	char error_msg[ERROR_MESSAGE_LENGTH * (MAX_GFT + 1)];
	error_msg[0] = '\0';
	char *add_pos = error_msg;
	const char *last = lastof(error_msg);

	for (uint i = 0; i < lengthof(_used_graphics_set->files); i++) {
		if (!FileMD5(_used_graphics_set->files[i])) {
			add_pos += seprintf(add_pos, last, "Your '%s' file is corrupted or missing! %s\n", _used_graphics_set->files[i].filename, _used_graphics_set->files[i].missing_warning);
		}
	}

	bool sound = false;
	for (uint i = 0; !sound && i < lengthof(_sound_sets); i++) {
		sound = FileMD5(_sound_sets[i]);
	}

	if (!sound) {
		add_pos += seprintf(add_pos, last, "Your 'sample.cat' file is corrupted or missing! You can find 'sample.cat' on your Transport Tycoon Deluxe CD-ROM.\n");
	}

	if (add_pos != error_msg) ShowInfoF(error_msg);
}


static void LoadSpriteTables()
{
	memset(_palette_remap_grf, 0, sizeof(_palette_remap_grf));
	uint i = FIRST_GRF_SLOT;

	_palette_remap_grf[i] = (_use_palette != _used_graphics_set->palette);
	LoadGrfFile(_used_graphics_set->files[GFT_BASE].filename, 0, i++);

	/*
	 * The second basic file always starts at the given location and does
	 * contain a different amount of sprites depending on the "type"; DOS
	 * has a few sprites less. However, we do not care about those missing
	 * sprites as they are not shown anyway (logos in intro game).
	 */
	_palette_remap_grf[i] = (_use_palette != _used_graphics_set->palette);
	LoadGrfFile(_used_graphics_set->files[GFT_LOGOS].filename, 4793, i++);

	/*
	 * Load additional sprites for climates other than temperate.
	 * This overwrites some of the temperate sprites, such as foundations
	 * and the ground sprites.
	 */
	if (_settings_game.game_creation.landscape != LT_TEMPERATE) {
		_palette_remap_grf[i] = (_use_palette != _used_graphics_set->palette);
		LoadGrfIndexed(
			_used_graphics_set->files[GFT_ARCTIC + _settings_game.game_creation.landscape - 1].filename,
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
	master->filename = strdup(_used_graphics_set->files[GFT_EXTRA].filename);
	FillGRFDetails(master, false);
	master->windows_paletted = (_used_graphics_set->palette == PAL_WINDOWS);
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
 * Try to read a single piece of metadata and return false if it doesn't exist.
 * @param name the name of the item to fetch.
 */
#define fetch_metadata(name) \
	item = metadata->GetItem(name, false); \
	if (item == NULL || strlen(item->value) == 0) { \
		DEBUG(grf, 0, "Base graphics set detail loading: %s field missing", name); \
		return false; \
	}

/** Names corresponding to the GraphicsFileType */
static const char *_gft_names[MAX_GFT] = { "base", "logos", "arctic", "tropical", "toyland", "extra" };

/**
 * Read the graphics set information from a loaded ini.
 * @param graphics the graphics set to write to
 * @param ini      the ini to read from
 * @param path     the path to this ini file (for filenames)
 * @return true if loading was successful.
 */
static bool FillGraphicsSetDetails(GraphicsSet *graphics, IniFile *ini, const char *path)
{
	memset(graphics, 0, sizeof(*graphics));

	IniGroup *metadata = ini->GetGroup("metadata");
	IniItem *item;

	fetch_metadata("name");
	graphics->name = strdup(item->value);

	fetch_metadata("description");
	graphics->description = strdup(item->value);

	fetch_metadata("shortname");
	for (uint i = 0; item->value[i] != '\0' && i < 4; i++) {
		graphics->shortname |= ((uint8)item->value[i]) << (i * 8);
	}

	fetch_metadata("version");
	graphics->version = atoi(item->value);

	fetch_metadata("palette");
	graphics->palette = (*item->value == 'D' || *item->value == 'd') ? PAL_DOS : PAL_WINDOWS;

	/* For each of the graphics file types we want to find the file, MD5 checksums and warning messages. */
	IniGroup *files  = ini->GetGroup("files");
	IniGroup *md5s   = ini->GetGroup("md5s");
	IniGroup *origin = ini->GetGroup("origin");
	for (uint i = 0; i < MAX_GFT; i++) {
		MD5File *file = &graphics->files[i];
		/* Find the filename first. */
		item = files->GetItem(_gft_names[i], false);
		if (item == NULL) {
			DEBUG(grf, 0, "No graphics file for: %s", _gft_names[i]);
			return false;
		}

		const char *filename = item->value;
		file->filename = MallocT<char>(strlen(filename) + strlen(path) + 1);
		sprintf((char*)file->filename, "%s%s", path, filename);

		/* Then find the MD5 checksum */
		item = md5s->GetItem(filename, false);
		if (item == NULL) {
			DEBUG(grf, 0, "No MD5 checksum specified for: %s", filename);
			return false;
		}
		char *c = item->value;
		for (uint i = 0; i < sizeof(file->hash) * 2; i++, c++) {
			uint j;
			if ('0' <= *c && *c <= '9') {
				j = *c - '0';
			} else if ('a' <= *c && *c <= 'f') {
				j = *c - 'a' + 10;
			} else if ('A' <= *c && *c <= 'F') {
				j = *c - 'A' + 10;
			} else {
				DEBUG(grf, 0, "Malformed MD5 checksum specified for: %s", filename);
				return false;
			}
			if (i % 2 == 0) {
				file->hash[i / 2] = j << 4;
			} else {
				file->hash[i / 2] |= j;
			}
		}

		/* Then find the warning message when the file's missing */
		item = origin->GetItem(filename, false);
		if (item == NULL) item = origin->GetItem("default", false);
		if (item == NULL) {
			DEBUG(grf, 1, "No origin warning message specified for: %s", filename);
			file->missing_warning = strdup("");
		} else {
			file->missing_warning = strdup(item->value);
		}

		if (FileMD5(*file)) graphics->found_grfs++;
	}

	return true;
}

/** Helper for scanning for files with GRF as extension */
class OBGFileScanner : FileScanner {
public:
	/* virtual */ bool AddFile(const char *filename, size_t basepath_length);

	/** Do the scan for OBGs. */
	static uint DoScan()
	{
		OBGFileScanner fs;
		return fs.Scan(".obg", DATA_DIR);
	}
};

/**
 * Try to add a graphics set with the given filename.
 * @param filename        the full path to the file to read
 * @param basepath_length amount of characters to chop of before to get a relative DATA_DIR filename
 * @return true if the file is added.
 */
bool OBGFileScanner::AddFile(const char *filename, size_t basepath_length)
{
	bool ret = false;
	DEBUG(grf, 1, "Found %s as base graphics set", filename);

	GraphicsSet *graphics = new GraphicsSet();;
	IniFile *ini = new IniFile();
	ini->LoadFromDisk(filename);

	char *path = strdup(filename + basepath_length);
	char *psep = strrchr(path, PATHSEPCHAR);
	if (psep != NULL) {
		psep[1] = '\0';
	} else {
		*path = '\0';
	}

	if (FillGraphicsSetDetails(graphics, ini, path)) {
		bool duplicate = false;
		for (const GraphicsSet *c = _available_graphics_sets; !duplicate && c != NULL; c = c->next) {
			duplicate = (strcmp(c->name, graphics->name) == 0 || c->shortname == graphics->shortname) && c->version == graphics->version;
		}
		if (duplicate) {
			delete graphics;
		} else {
			GraphicsSet **last = &_available_graphics_sets;
			while (*last != NULL) last = &(*last)->next;

			*last = graphics;
			ret = true;
		}
	} else {
		delete graphics;
	}
	free(path);

	delete ini;
	return ret;
}



/** Scan for all Grahpics sets */
void FindGraphicsSets()
{
	DEBUG(grf, 1, "Scanning for Graphics sets");
	OBGFileScanner::DoScan();
}

/**
 * Set the graphics set to be used.
 * @param name of the graphics set to use
 * @return true if it could be loaded
 */
bool SetGraphicsSet(const char *name)
{
	if (StrEmpty(name)) {
		if (!DetermineGraphicsPack()) return false;
		CheckExternalFiles();
		return true;
	}

	for (const GraphicsSet *g = _available_graphics_sets; g != NULL; g = g->next) {
		if (strcmp(name, g->name) == 0) {
			_used_graphics_set = g;
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
	p += seprintf(p, last, "List of graphics sets:\n");
	for (const GraphicsSet *g = _available_graphics_sets; g != NULL; g = g->next) {
		if (g->found_grfs <= 1) continue;

		p += seprintf(p, last, "%18s: %s", g->name, g->description);
		int difference = MAX_GFT - g->found_grfs;
		if (difference != 0) {
			p += seprintf(p, last, " (missing %i file%s)\n", difference, difference == 1 ? "" : "s");
		} else {
			p += seprintf(p, last, "\n");
		}
	}
	p += seprintf(p, last, "\n");

	return p;
}

#if defined(ENABLE_NETWORK)
#include "network/network_content.h"

/**
 * Check whether we have an graphics with the exact characteristics as ci.
 * @param ci the characteristics to search on (shortname and md5sum)
 * @param md5sum whether to check the MD5 checksum
 * @return true iff we have an graphics set matching.
 */
bool HasGraphicsSet(const ContentInfo *ci, bool md5sum)
{
	assert(ci->type == CONTENT_TYPE_BASE_GRAPHICS);
	for (const GraphicsSet *g = _available_graphics_sets; g != NULL; g = g->next) {
		if (g->found_grfs <= 1) continue;

		if (g->shortname != ci->unique_id) continue;
		if (!md5sum) return true;

		byte md5[16];
		memset(md5, 0, sizeof(md5));
		for (uint i = 0; i < MAX_GFT; i++) {
			for (uint j = 0; j < sizeof(md5); j++) {
				md5[j] ^= g->files[i].hash[j];
			}
		}
		if (memcmp(md5, ci->md5sum, sizeof(md5)) == 0) return true;
	}

	return false;
}

#endif /* ENABLE_NETWORK */

/**
 * Count the number of available graphics sets.
 */
int GetNumGraphicsSets()
{
	int n = 0;
	for (const GraphicsSet *g = _available_graphics_sets; g != NULL; g = g->next) {
		if (g != _used_graphics_set && g->found_grfs <= 1) continue;
		n++;
	}
	return n;
}

/**
 * Get the index of the currently active graphics set
 */
int GetIndexOfCurrentGraphicsSet()
{
	int n = 0;
	for (const GraphicsSet *g = _available_graphics_sets; g != NULL; g = g->next) {
		if (g == _used_graphics_set) return n;
		if (g->found_grfs <= 1) continue;
		n++;
	}
	return -1;
}

/**
 * Get the name of the graphics set at the specified index
 */
const char *GetGraphicsSetName(int index)
{
	for (const GraphicsSet *g = _available_graphics_sets; g != NULL; g = g->next) {
		if (g != _used_graphics_set && g->found_grfs <= 1) continue;
		if (index == 0) return g->name;
		index--;
	}
	error("GetGraphicsSetName: index %d out of range", index);
}
