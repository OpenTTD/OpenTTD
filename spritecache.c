/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "gfx.h"
#include "spritecache.h"
#include "table/sprites.h"
#include "fileio.h"
#include "newgrf.h"
#include "md5.h"
#include "variables.h"
#include <ctype.h>

#define SPRITECACHE_ID 0xF00F0006
#define SPRITE_CACHE_SIZE 1024*1024


//#define WANT_SPRITESIZES
#define WANT_NEW_LRU
//#define WANT_LOCKED


/* These are used in newgrf.c: */

int _skip_sprites = 0;
int _replace_sprites_count[16];
int _replace_sprites_offset[16];

static const char *_cur_grffile;
static int _loading_stage;
static int _skip_specials;
uint16 _custom_sprites_base;
static Sprite _cur_sprite;


static void* _sprite_ptr[NUM_SPRITES];
static uint16 _sprite_size[NUM_SPRITES];
static uint32 _sprite_file_pos[NUM_SPRITES];

// This one is probably not needed.
#if defined(WANT_LOCKED)
static bool _sprite_locked[NUM_SPRITES];
#endif

#if defined(WANT_NEW_LRU)
static int16 _sprite_lru_new[NUM_SPRITES];
#else
static uint16 _sprite_lru[NUM_SPRITES];
static uint16 _sprite_lru_cur[NUM_SPRITES];
#endif

#ifdef WANT_SPRITESIZES
static int8 _sprite_xoffs[NUM_SPRITES];
static int8 _sprite_yoffs[NUM_SPRITES];
static uint16 _sprite_xsize[NUM_SPRITES];
static uint8 _sprite_ysize[NUM_SPRITES];
#endif

bool _cache_sprites;

typedef struct MemBlock {
	uint32 size;
	byte data[VARARRAY_SIZE];
} MemBlock;

static uint _sprite_lru_counter;
static MemBlock *_spritecache_ptr;
static uint32 _spritecache_size;
static int _compact_cache_counter;

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

static const uint16 * const _landscape_spriteindexes[] = {
	_landscape_spriteindexes_1,
	_landscape_spriteindexes_2,
	_landscape_spriteindexes_3,
};

static const uint16 * const _slopes_spriteindexes[] = {
	_slopes_spriteindexes_0,
	_slopes_spriteindexes_1,
	_slopes_spriteindexes_2,
	_slopes_spriteindexes_3,
};

static void CompactSpriteCache(void);

static void ReadSpriteHeaderSkipData(int num, int load_index)
{
	byte type;
	int deaf = 0;

	if (_skip_sprites) {
		if (_skip_sprites > 0)
			_skip_sprites--;
		deaf = 1;
	}

	type = FioReadByte();
	_cur_sprite.info = type;
	if (type == 0xFF) {
		/* We need to really skip only special sprites in the deaf
		 * mode.  It won't hurt to proceed regular sprites as usual
		 * because if no special sprite referencing to them is
		 * processed, they themselves are never referenced and loaded
		 * on their own. */
		if (_skip_specials || deaf) {
			FioSkipBytes(num);
		} else {
			DecodeSpecialSprite(_cur_grffile, num, load_index, _loading_stage);
		}
		return;
	}

#ifdef WANT_SPRITESIZES
	_cur_sprite.height = FioReadByte();
	_cur_sprite.width = FioReadWord();
	_cur_sprite.x_offs = FioReadWord();
	_cur_sprite.y_offs = FioReadWord();
#else
	FioSkipBytes(7);
#endif
	num -= 8;
	if (num == 0)
		return;

	if (type & 2) {
		FioSkipBytes(num);
	} else {
		while (num > 0) {
			int8 i = FioReadByte();
			if (i >= 0) {
				num -= i;
				FioSkipBytes(i);
			} else {
				i = -(i >> 3);
				num -= i;
				FioReadByte();
			}
		}
	}
}

static void* AllocSprite(size_t);

static void* ReadSprite(SpriteID id)
{
	uint num = _sprite_size[id];
	byte type;

	DEBUG(spritecache, 9) ("load sprite %d", id);

	if (_sprite_file_pos[id] == 0) {
		error(
			"Tried to load non-existing sprite #%d.\n"
			"Probable cause: Wrong/missing NewGRFs",
			id
		);
	}

	FioSeekToFile(_sprite_file_pos[id]);

	type = FioReadByte();
	if (type == 0xFF) {
		byte* dest = AllocSprite(num);

		_sprite_ptr[id] = dest;
		FioReadBlock(dest, num);

		return dest;
	} else {
		uint height = FioReadByte();
		uint width  = FioReadWord();
		Sprite* sprite;
		byte* dest;

		num = (type & 0x02) ? width * height : num - 8;
		sprite = AllocSprite(sizeof(*sprite) + num);
		_sprite_ptr[id] = sprite;
		sprite->info   = type;
		sprite->height = (id != 142) ? height : 10; // Compensate for a TTD bug
		sprite->width  = width;
		sprite->x_offs = FioReadWord();
		sprite->y_offs = FioReadWord();

		dest = sprite->data;
		while (num > 0) {
			int8 i = FioReadByte();

			if (i >= 0) {
				num -= i;
				for (; i > 0; --i) *dest++ = FioReadByte();
			} else {
				const byte* rel = dest - (((i & 7) << 8) | FioReadByte());

				i = -(i >> 3);
				num -= i;

				for (; i > 0; --i) *dest++ = *rel++;
			}
		}

		return sprite;
	}
}


static bool LoadNextSprite(int load_index, byte file_index)
{
	uint16 size;
	uint32 file_pos;

	size = FioReadWord();
	if (size == 0)
		return false;

	file_pos = FioGetPos() | (file_index << 24);

	ReadSpriteHeaderSkipData(size, load_index);

	if (_replace_sprites_count[0] > 0 && _cur_sprite.info != 0xFF) {
		int count = _replace_sprites_count[0];
		int offset = _replace_sprites_offset[0];

		_replace_sprites_offset[0]++;
		_replace_sprites_count[0]--;

		if ((offset + count) <= NUM_SPRITES) {
			load_index = offset;
		} else {
			DEBUG(spritecache, 1) ("Sprites to be replaced are out of range: %x+%x",
					count, offset);
			_replace_sprites_offset[0] = 0;
			_replace_sprites_count[0] = 0;
		}

		if (_replace_sprites_count[0] == 0) {
			int i;

			for (i = 0; i < 15; i++) {
				_replace_sprites_count[i] = _replace_sprites_count[i + 1];
				_replace_sprites_offset[i] = _replace_sprites_offset[i + 1];
			}
			_replace_sprites_count[i] = 0;
			_replace_sprites_offset[i] = 0;
		}
	}

	_sprite_size[load_index] = size;
	_sprite_file_pos[load_index] = file_pos;

#ifdef WANT_SPRITESIZES
	_sprite_xsize[load_index] = _cur_sprite.width;
	_sprite_ysize[load_index] = _cur_sprite.height;

	_sprite_xoffs[load_index] = _cur_sprite.x_offs;
	_sprite_yoffs[load_index] = _cur_sprite.y_offs;
#endif

	_sprite_ptr[load_index] = NULL;
#if defined(WANT_LOCKED)
	_sprite_locked[load_index] = false;
#endif

#if defined(WANT_NEW_LRU)
	_sprite_lru_new[load_index] = 0;
#else
	_sprite_lru[load_index] = 0xFFFF;
	_sprite_lru_cur[load_index] = 0;
#endif

	return true;
}

static void SkipSprites(uint count)
{
	for (; count > 0; --count)
	{
		uint16 size = FioReadWord();

		if (size == 0)
			return;

		ReadSpriteHeaderSkipData(size, NUM_SPRITES - 1);
	}
}

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
		if (load_index >= NUM_SPRITES) {
			error("Too many sprites. Recompile with higher NUM_SPRITES value or remove some custom GRF files.");
		}
	}

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
		if (load_index + i >= NUM_SPRITES)
			error("Too many sprites (0x%X). Recompile with higher NUM_SPRITES value or remove some custom GRF files.",
			      load_index + i);
	}

	/* Clean up. */
	_skip_sprites = 0;
	memset(_replace_sprites_count, 0, 16 * sizeof(*_replace_sprites_count));
	memset(_replace_sprites_offset, 0, 16 * sizeof(*_replace_sprites_offset));

	return i;
}

static void LoadGrfIndexed(const char *filename, const uint16 *index_tbl, int file_index)
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

typedef size_t CDECL fread_t(void*, size_t, size_t, FILE*);

static bool HandleCachedSpriteHeaders(const char *filename, bool read)
{
	FILE *f;
	fread_t *proc;
	uint32 hdr;

	if (!_cache_sprites)
		return false;

	if (read) {
		f = fopen(filename, "rb");
		proc = fread;

		if (f == NULL)
			return false;

		proc(&hdr, sizeof(hdr), 1, f);
		if (hdr != SPRITECACHE_ID) {
			fclose(f);
			return false;
		}
	} else {
		f = fopen(filename, "wb");
		proc = (fread_t*) fwrite;

		if (f == NULL)
			return false;

		hdr = SPRITECACHE_ID;
		proc(&hdr, sizeof(hdr), 1, f);
	}

	proc(_sprite_size, 1, sizeof(_sprite_size), f);
	proc(_sprite_file_pos, 1, sizeof(_sprite_file_pos), f);

#if 0
	proc(_sprite_xsize, 1, sizeof(_sprite_xsize), f);
	proc(_sprite_ysize, 1, sizeof(_sprite_ysize), f);
	proc(_sprite_xoffs, 1, sizeof(_sprite_xoffs), f);
	proc(_sprite_yoffs, 1, sizeof(_sprite_yoffs), f);
#endif

#if !defined(WANT_NEW_LRU)
	if (read)
		memset(_sprite_lru, 0xFF, sizeof(_sprite_lru));
#endif

	fclose(f);
	return true;
}

#define S_FREE_MASK 1

static inline MemBlock* NextBlock(MemBlock* block)
{
	return (MemBlock*)((byte*)block + (block->size & ~S_FREE_MASK));
}

static uint32 GetSpriteCacheUsage(void)
{
	size_t tot_size = 0;
	MemBlock* s;

	for (s = _spritecache_ptr; s->size != 0; s = NextBlock(s))
		if (!(s->size & S_FREE_MASK)) tot_size += s->size;

	return tot_size;
}


void IncreaseSpriteLRU(void)
{
	int i;

	// Increase all LRU values
#if defined(WANT_NEW_LRU)
	if (_sprite_lru_counter > 16384) {
		DEBUG(spritecache, 2) ("fixing lru %d, inuse=%d", _sprite_lru_counter, GetSpriteCacheUsage());

		for (i = 0; i != NUM_SPRITES; i++)
			if (_sprite_ptr[i] != NULL) {
				if (_sprite_lru_new[i] >= 0) {
					_sprite_lru_new[i] = -1;
				} else if (_sprite_lru_new[i] != -32768) {
					_sprite_lru_new[i]--;
				}
			}
		_sprite_lru_counter = 0;
	}
#else
	for (i = 0; i != NUM_SPRITES; i++)
		if (_sprite_ptr[i] != NULL && _sprite_lru[i] != 65535)
			_sprite_lru[i]++;
	// Reset the lru counter.
	_sprite_lru_counter = 0;
#endif

	// Compact sprite cache every now and then.
	if (++_compact_cache_counter >= 740) {
		CompactSpriteCache();
		_compact_cache_counter = 0;
	}
}

// Called when holes in the sprite cache should be removed.
// That is accomplished by moving the cached data.
static void CompactSpriteCache(void)
{
	MemBlock *s;

	DEBUG(spritecache, 2) (
		"compacting sprite cache, inuse=%d", GetSpriteCacheUsage()
	);

	for (s = _spritecache_ptr; s->size != 0;) {
		if (s->size & S_FREE_MASK) {
			MemBlock* next = NextBlock(s);
			MemBlock temp;
			void** i;

			// Since free blocks are automatically coalesced, this should hold true.
			assert(!(next->size & S_FREE_MASK));

			// If the next block is the sentinel block, we can safely return
			if (next->size == 0)
				break;

			// Locate the sprite belonging to the next pointer.
			for (i = _sprite_ptr; *i != next->data; ++i) {
				assert(i != endof(_sprite_ptr));
			}

			#ifdef WANT_LOCKED
			if (_sprite_locked[i]) {
				s = next;
				continue;
			}
			#endif

			*i = s->data; // Adjust sprite array entry
			// Swap this and the next block
			temp = *s;
			memmove(s, next, next->size);
			s = NextBlock(s);
			*s = temp;

			// Coalesce free blocks
			while (NextBlock(s)->size & S_FREE_MASK) {
				s->size += NextBlock(s)->size & ~S_FREE_MASK;
			}
		} else {
			s = NextBlock(s);
		}
	}
}

static void DeleteEntryFromSpriteCache(void)
{
	int i;
	int best = -1;
	MemBlock* s;
	int cur_lru;

	DEBUG(spritecache, 2) ("DeleteEntryFromSpriteCache, inuse=%d", GetSpriteCacheUsage());

#if defined(WANT_NEW_LRU)
	cur_lru = 0xffff;
	for (i = 0; i != NUM_SPRITES; i++) {
		if (_sprite_ptr[i] != 0 &&
				_sprite_lru_new[i] < cur_lru
#if defined(WANT_LOCKED)
				 && !_sprite_locked[i]
#endif
				) {
			cur_lru = _sprite_lru_new[i];
			best = i;
		}
	}
#else
	{
	uint16 cur_lru = 0, cur_lru_cur = 0xffff;
	for (i = 0; i != NUM_SPRITES; i++) {
		if (_sprite_ptr[i] == 0 ||
#if defined(WANT_LOCKED)
				_sprite_locked[i] ||
#endif
				_sprite_lru[i] < cur_lru)
					continue;

		// Found a sprite with a higher LRU value, then remember it.
		if (_sprite_lru[i] != cur_lru) {
			cur_lru = _sprite_lru[i];
			best = i;

		// Else if both sprites were very recently referenced, compare by the cur value instead.
		} else if (cur_lru == 0 && _sprite_lru_cur[i] <= cur_lru_cur) {
			cur_lru_cur = _sprite_lru_cur[i];
			cur_lru = _sprite_lru[i];
			best = i;
		}
	}
	}
#endif

	// Display an error message and die, in case we found no sprite at all.
	// This shouldn't really happen, unless all sprites are locked.
	if (best == -1)
		error("Out of sprite memory");

	// Mark the block as free (the block must be in use)
	s = (MemBlock*)_sprite_ptr[best] - 1;
	assert(!(s->size & S_FREE_MASK));
	s->size |= S_FREE_MASK;
	_sprite_ptr[best] = NULL;

	// And coalesce adjacent free blocks
	for (s = _spritecache_ptr; s->size != 0; s = NextBlock(s)) {
		if (s->size & S_FREE_MASK) {
			while (NextBlock(s)->size & S_FREE_MASK) {
				s->size += NextBlock(s)->size & ~S_FREE_MASK;
			}
		}
	}
}

static void* AllocSprite(size_t mem_req)
{
	mem_req += sizeof(MemBlock);

	/* Align this to an uint32 boundary. This also makes sure that the 2 least
	 * bits are not used, so we could use those for other things. */
	mem_req = (mem_req + sizeof(uint32) - 1) & ~(sizeof(uint32) - 1);

	for (;;) {
		MemBlock* s;

		for (s = _spritecache_ptr; s->size != 0; s = NextBlock(s)) {
			if (s->size & S_FREE_MASK) {
				size_t cur_size = s->size & ~S_FREE_MASK;

				/* Is the block exactly the size we need or
				 * big enough for an additional free block? */
				if (cur_size == mem_req ||
						cur_size >= mem_req + sizeof(MemBlock)) {
					// Set size and in use
					s->size = mem_req;

					// Do we need to inject a free block too?
					if (cur_size != mem_req) {
						NextBlock(s)->size = (cur_size - mem_req) | S_FREE_MASK;
					}

					return s->data;
				}
			}
		}

		// Reached sentinel, but no block found yet. Delete some old entry.
		DeleteEntryFromSpriteCache();
	}
}

#if defined(NEW_ROTATION)
#define X15(x) else if (s >= x && s < (x+15)) { s = _rotate_tile_sprite[s - x] + x; }
#define X19(x) else if (s >= x && s < (x+19)) { s = _rotate_tile_sprite[s - x] + x; }
#define MAP(from,to,map) else if (s >= from && s <= to) { s = map[s - from] + from; }


static uint RotateSprite(uint s)
{
	static const byte _rotate_tile_sprite[19] = { 0,2,4,6,8,10,12,14,1,3,5,7,9,11,13,17,18,16,15 };
	static const byte _coast_map[9] = {0, 4, 3, 1, 2, 6, 8, 5, 7};
	static const byte _fence_map[6] = {1, 0, 5, 4, 3, 2};

	if (0);
	X19(752)
	X15(990-1)
	X19(3924)
	X19(3943)
	X19(3962)
	X19(3981)
	X19(4000)
	X19(4023)
	X19(4042)
	MAP(4061,4069,_coast_map)
	X19(4126)
	X19(4145)
	X19(4164)
	X19(4183)
	X19(4202)
	X19(4221)
	X19(4240)
	X19(4259)
	X19(4259)
	X19(4278)
	MAP(4090, 4095, _fence_map)
	MAP(4096, 4101, _fence_map)
	MAP(4102, 4107, _fence_map)
	MAP(4108, 4113, _fence_map)
	MAP(4114, 4119, _fence_map)
	MAP(4120, 4125, _fence_map)
	return s;
}
#endif

const void *GetRawSprite(SpriteID sprite)
{
	void* p;

	assert(sprite < NUM_SPRITES);

#if defined(NEW_ROTATION)
	sprite = RotateSprite(sprite);
#endif

	// Update LRU
#if defined(WANT_NEW_LRU)
	_sprite_lru_new[sprite] = ++_sprite_lru_counter;
#else
	_sprite_lru_cur[sprite] = ++_sprite_lru_counter;
	_sprite_lru[sprite] = 0;
#endif

	p = _sprite_ptr[sprite];
	// Load the sprite, if it is not loaded, yet
	if (p == NULL) p = ReadSprite(sprite);
	return p;
}

byte _sprite_page_to_load = 0xFF;

static const char * const _cached_filenames[4] = {
	"cached_sprites.xxx",
	"cached_sprites.xx1",
	"cached_sprites.xx2",
	"cached_sprites.xx3",
};

#define OPENTTD_SPRITES_COUNT 98
static const uint16 _openttd_grf_indexes[] = {
	SPR_OPENTTD_BASE + 0, SPR_OPENTTD_BASE + 7, // icons etc
	134, 134,  // euro symbol medium size
	582, 582,  // euro symbol large size
	358, 358,  // euro symbol tiny
	SPR_OPENTTD_BASE+11, SPR_OPENTTD_BASE+57, // more icons
	648, 648, // nordic char: æ
	616, 616, // nordic char: Æ
	666, 666, // nordic char: Ø
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
	uint i;
	uint j;
	const FileList *files; // list of grf files to be loaded. Either Windows files or DOS files

	_loading_stage = 1;

	/*
	 * Note for developers:
	 *   Keep in mind that when you add a LoadGrfIndexed in the 'if'-section below
	 *   that you should also add the corresponding FioOpenFile to the 'else'-section
	 *   below.
	 *
	 * TODO:
	 *   I think we can live entirely without Indexed GRFs, but I have to
	 *   invest that further. --octo
	 */

	files = _use_dos_palette? &files_dos : &files_win;

	// Try to load the sprites from cache
	if (!HandleCachedSpriteHeaders(_cached_filenames[_opt.landscape], true)) {
		// We do not have the sprites in cache yet, or cache is disabled
		// So just load all files from disk..

		int load_index = 0;

		for (i = 0; files->basic[i].filename != NULL; i++) {
			load_index += LoadGrfFile(files->basic[i].filename, load_index, (byte)i);
		}

		LoadGrfIndexed("openttd.grf", _openttd_grf_indexes, i++);

		if (_sprite_page_to_load != 0)
			LoadGrfIndexed(
				files->landscape[_sprite_page_to_load - 1].filename,
				_landscape_spriteindexes[_sprite_page_to_load - 1],
				i++
			);

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
				if (!FiosCheckFileExists(_newgrf_files[j]))
					// TODO: usrerror()
					error("NewGRF file missing: %s", _newgrf_files[j]);
				if (_loading_stage == 0)
					InitNewGRFFile(_newgrf_files[j], load_index);
				load_index += LoadNewGrfFile(_newgrf_files[j], load_index, i++);
			}
		}

		// If needed, save the cache to file
		HandleCachedSpriteHeaders(_cached_filenames[_opt.landscape], false);
	} else {
		// We have sprites cached. We just loaded the cached files
		//  now we only have to open a file-pointer to all the original grf-files
		// This is very important. Not all sprites are in the cache. So sometimes
		//  the game needs to load the sprite from disk. When the file is not
		//  open it can not read. So all files that are in the 'if'-section
		//  above should also be in this 'else'-section.
		//
		// NOTE: the order of the files must be identical as in the section above!!

		for (i = 0; files->basic[i].filename != NULL; i++)
			FioOpenFile(i,files->basic[i].filename);

		FioOpenFile(i++, "openttd.grf");

		if (_sprite_page_to_load != 0)
			FioOpenFile(i++, files->landscape[_sprite_page_to_load - 1].filename);

		FioOpenFile(i++, "trkfoundw.grf");
		FioOpenFile(i++, "canalsw.grf");

		// FIXME: if a user changes his newgrf's, the cached-sprites gets
		//  invalid. We should have some kind of check for this.
		// The best solution for this is to delete the cached-sprites.. but how
		//  do we detect it?
		for (j = 0; j != lengthof(_newgrf_files) && _newgrf_files[j] != NULL; j++)
			FioOpenFile(i++, _newgrf_files[j]);
	}

	_compact_cache_counter = 0;
}

static void GfxInitSpriteMem(void *ptr, uint32 size)
{
	// initialize sprite cache heap
	_spritecache_ptr = ptr;
	_spritecache_size = size;

	// A big free block
	_spritecache_ptr->size = (size - sizeof(MemBlock)) | S_FREE_MASK;
	// Sentinel block (identified by size == 0)
	NextBlock(_spritecache_ptr)->size = 0;

	memset(_sprite_ptr, 0, sizeof(_sprite_ptr));
}


void GfxLoadSprites(void)
{
	static byte *_sprite_mem;

	// Need to reload the sprites only if the landscape changed
	if (_sprite_page_to_load != _opt.landscape) {
		_sprite_page_to_load = _opt.landscape;

		// Sprite cache
		DEBUG(spritecache, 1) ("Loading sprite set %d.", _sprite_page_to_load);

		// Reuse existing memory?
		if (_sprite_mem == NULL) _sprite_mem = malloc(SPRITE_CACHE_SIZE);
		GfxInitSpriteMem(_sprite_mem, SPRITE_CACHE_SIZE);
		LoadSpriteTables();
		GfxInitPalettes();
	}
}


const SpriteDimension *GetSpriteDimension(SpriteID sprite)
{
	static SpriteDimension sd_static;
	SpriteDimension *sd = &sd_static;

#ifdef WANT_SPRITESIZES
	sd->xoffs = _sprite_xoffs[sprite];
	sd->yoffs = _sprite_yoffs[sprite];
	sd->xsize = _sprite_xsize[sprite];
	sd->ysize = _sprite_ysize[sprite];
#else
	const Sprite* p = GetSprite(sprite);

	/* decode sprite header */
	sd->xoffs = p->x_offs;
	sd->yoffs = p->y_offs;
	sd->xsize = p->width;
	sd->ysize = p->height;
#endif

	return sd;
}

