#include "stdafx.h"
#include "ttd.h"
#include "gfx.h"
#include "fileio.h"
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
static SpriteHdr _cur_sprite;


static byte *_sprite_ptr[NUM_SPRITES];
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

static uint _sprite_lru_counter;
static byte *_spritecache_ptr;
static uint32 _spritecache_size;
static int _compact_cache_counter;


static const char * const _filename_list[] = {
	"TRG1R.GRF",
	"TRGIR.GRF",
	"signalsw.grf", //0x1320 - 0x1405 inclusive
//	"openttd.grf",	//0x1406 -
	NULL
};

static const char * const _landscape_filenames[] = {
	"TRGCR.GRF",
	"TRGHR.GRF",
	"TRGTR.GRF"
};


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

static void CompactSpriteCache();

void InitNewGRFFile(const char *filename, int sprite_offset);
void DecodeSpecialSprite(const char *filename, int num, int load_index, int stage);

static void ReadSpriteHeaderSkipData(int num, int load_index)
{
	byte type;
	int8 i;
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
		return;
	}

	while (num) {
		i = FioReadByte();
		if (i>=0) {
			num -= i;
			FioSkipBytes(i);
		} else {
			i = -(i >> 3);
			num -= i;
			FioReadByte();
		}
	}
}

static void ReadSprite(int num, byte *dest)
{
	byte type;
	byte *rel;
	int8 i;
	int j, dist;

	type = FioReadByte();
	/* We've decoded special sprites when reading headers. */
	if (type != 0xFF) {
		/* read sprite hdr */
		*dest++ = type;
		for(j=0; j!=7; j++)
			*dest++ = FioReadByte();
		num -= 8;
	}

	if (type & 2) {
		while (num--)
			*dest++ = FioReadByte();
		return;
	}

	while (num) {
		i = FioReadByte();
		if (i>=0) {
			num -= i;
			while (i--)
				*dest++ = FioReadByte();
		} else {
			dist = -(((i&7)<<8)|FioReadByte());
			i = -(i >> 3);
			num -= i;

			rel = &dest[dist];
			while (i--)
				*dest++ = *rel++;
		}
	}
}


static bool LoadNextSprite(int load_index, byte file_index)
{
	uint16 size;
	uint32 file_pos;

	if ((size = FioReadWord()) == 0)
		return false;

	file_pos = FioGetPos() | (file_index << 24);

	ReadSpriteHeaderSkipData(size, load_index);

	if ((_replace_sprites_count[0] > 0) && (_cur_sprite.info != 0xFF)) {
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

static void SkipSprites(int count)
{
	while(count>0)
	{
		uint16 size;
		if ( (size = FioReadWord()) == 0)
			return;

		ReadSpriteHeaderSkipData(size, NUM_SPRITES-1);
		count--;
	}
}

// Checks, if trg1r.grf is the Windows version
static bool CheckGrfFile()
{
	byte check;
	FioSeekToFile(38); // Byte 38 has the value 0x21 in Windows version, 0x07 in DOS
	check = FioReadWord();
	FioSeekToFile(0);
	return (check==0x21);
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

 	if(file_index==0 && !_ignore_wrong_grf)
 		if(!CheckGrfFile())
 			error("Wrong version of grf files!\nThe Windows 95 edition of Transport Tycoon Deluxe is required to play OTTD!\n(you can disable this message by starting with the \"-i\" switch.");

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

		if ((length == 4) && (type == 0xFF)) {
			FioReadDword();
		} else {
			error("Custom .grf has invalid format.");
		}
	}

	for (i = 0; LoadNextSprite(load_index + i, file_index); i++) {
		if (load_index + i >= NUM_SPRITES)
			error("Too many sprites (%x). Recompile with higher NUM_SPRITES value or remove some custom GRF files.",
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

	for(;(start=*index_tbl++) != 0xffff;) {
		int end = *index_tbl++;
		if(start==0xfffe) { // skip sprites (amount in second var)
			SkipSprites(end);
		} else { // load sprites and use indexes from start to end
			do {
				bool b = LoadNextSprite(start, file_index);
				assert(b);
			} while (++start <= end);
		}
	}
}

typedef size_t CDECL fread_t(void*,size_t,size_t,FILE*);

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

#define S_DATA(x) (*(uint32*)(x))
#define S_FREE_MASK 1
#define S_HDRSIZE sizeof(uint32)

static uint32 GetSpriteCacheUsage()
{
	byte *s = _spritecache_ptr;
	size_t cur_size, tot_size = 0;
	for(; (cur_size=S_DATA(s)) != 0; s+=cur_size) {
		if ( cur_size & S_FREE_MASK ) {
			cur_size--;
		} else {
			tot_size += cur_size;
		}
	}

	return tot_size;
}


void IncreaseSpriteLRU()
{
	int i;

	// Increase all LRU values
#if defined(WANT_NEW_LRU)
	if (_sprite_lru_counter > 16384) {
		DEBUG(spritecache, 2) ("fixing lru %d, inuse=%d", _sprite_lru_counter, GetSpriteCacheUsage());

		for(i=0; i!=NUM_SPRITES; i++)
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
	for(i=0; i!=NUM_SPRITES; i++)
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
static void CompactSpriteCache()
{
	byte *s, *t;
	size_t size, sizeb, cur_size;
	int i;

	DEBUG(spritecache, 2) ("compacting sprite cache, inuse=%d", GetSpriteCacheUsage());

	s = _spritecache_ptr;

	while (true) {
		size = S_DATA(s);

		// Only look for free blocks.
		if (size & S_FREE_MASK) {
			size -= S_FREE_MASK;
			// Since free blocks are automatically coalesced, this should hold true.
			assert(!(S_DATA(s+size) & S_FREE_MASK));

			// If the next block is the sentinel block, we can safely return
			if ( (sizeb=S_DATA(s + size)) == 0)
				break;

			// Locate the sprite number belonging to the next pointer.
			for(i=0,t=s+size+S_HDRSIZE; _sprite_ptr[i] != t; i++) {assert(i < NUM_SPRITES);}

			// If it's locked, we must not move it.
#if defined(WANT_LOCKED)
			if (!_sprite_locked[i]) {
#endif

				// Offset the sprite pointer by the size of the free block
				_sprite_ptr[i] -= size;

				// Move the memory
				memmove(s + S_HDRSIZE, s + S_HDRSIZE + size, sizeb - S_HDRSIZE);

				// What we just did had the effect of swapping the allocated block with the free block, so we need to update
				//  the block pointers. First update the allocated one. It is in use.
				S_DATA(s) = sizeb;

				// Then coalesce the free ones that follow.
				s += sizeb;
				while ((cur_size = S_DATA(s+size)) & S_FREE_MASK)
					size += cur_size - S_FREE_MASK;
				S_DATA(s) = size + S_FREE_MASK;
				continue;
#if defined(WANT_LOCKED)
			}
#endif
		}
		// Continue with next block until the sentinel is reached.
		s += size;
		if (size == 0)
			break;
	}
}

static void DeleteEntryFromSpriteCache()
{
	int i;
	int best = -1;
	byte *s;
	size_t cur_size, cur;
	int cur_lru;

	DEBUG(spritecache, 2) ("DeleteEntryFromSpriteCache, inuse=%d", GetSpriteCacheUsage());

#if defined(WANT_NEW_LRU)
	cur_lru = 0xffff;
	for(i=0; i!=NUM_SPRITES; i++) {
		if (_sprite_ptr[i] != 0 &&
				_sprite_lru_new[i] < cur_lru
#if defined(WANT_LOCKED)
				 && !_sprite_locked[i]) {
#else
				) {
#endif
			cur_lru = _sprite_lru_new[i];
			best = i;
		}
	}
#else
	{
	uint16 cur_lru = 0, cur_lru_cur = 0xffff;
	for(i=0; i!=NUM_SPRITES; i++) {
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
	s = _sprite_ptr[best];
	assert(!(S_DATA(s - S_HDRSIZE) & S_FREE_MASK));
	S_DATA(s - S_HDRSIZE) += S_FREE_MASK;
	_sprite_ptr[best] = NULL;

	// And coalesce adjacent free blocks
	s = _spritecache_ptr;
	for(; (cur_size=S_DATA(s)) != 0; s+=cur_size) {
		if ( cur_size & S_FREE_MASK ) {
			while ((cur=S_DATA(s+cur_size-S_FREE_MASK)) & S_FREE_MASK) {
				cur_size += cur - S_FREE_MASK;
				S_DATA(s) = cur_size;
			}
			cur_size--;
		}
	}
}

static byte *LoadSpriteToMem(int sprite)
{
	byte *s;
	size_t mem_req, cur_size;

	DEBUG(spritecache, 9) ("load sprite %d", sprite);

restart:
	// Number of needed bytes
	mem_req = _sprite_size[sprite] + S_HDRSIZE;

	// Align this to an uint32 boundary. This also makes sure that the 2 least bit are not used,
	//  so we could use those for other things.
	mem_req = (mem_req + sizeof(uint32) - 1) & ~(sizeof(uint32) - 1);

	s = _spritecache_ptr;
	for(;;) {
		for(;;) {
			cur_size = S_DATA(s);
			if (! (cur_size & S_FREE_MASK) ) break;

			cur_size -= S_FREE_MASK;

			// Now s points at a free block.
			// The block is exactly the size we need?
			if (cur_size != mem_req) {

				// No.. is it too small?
				if (cur_size < mem_req + S_HDRSIZE)
					break;

				// Block was big enough, and we need to inject a free block too.
				S_DATA(s + mem_req) = cur_size - mem_req + S_FREE_MASK;

			}
			// Set size and in use
			S_DATA(s) = mem_req;

			_sprite_ptr[sprite] = (s += S_HDRSIZE);

			FioSeekToFile(_sprite_file_pos[sprite]);
			ReadSprite(_sprite_size[sprite], s);

			// Patch the height to compensate for a TTD bug?
			if (sprite == 142) { s[1] = 10; }

			// Return sprite ptr
			return s;
		}

		// Reached sentinel, but no block found yet. Need to delete some old entries.
		if (cur_size == 0) {
			DeleteEntryFromSpriteCache();
			goto restart;
		}

		s += cur_size;
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

byte *GetSpritePtr(uint sprite)
{
	byte *p;

	assert(sprite < NUM_SPRITES);

#if defined(NEW_ROTATION)
	sprite = RotateSprite(sprite);
#endif

	// Update LRU
#if defined(WANT_NEW_LRU)
	_sprite_lru_new[sprite] = ++_sprite_lru_counter;
#else
	_sprite_lru_cur[sprite]	= ++_sprite_lru_counter;
	_sprite_lru[sprite] = 0;
#endif

	// Check if the sprite is loaded already?
	p = _sprite_ptr[sprite];
	if (p == NULL)
		p = LoadSpriteToMem(sprite);  // No, need to load it.
	return p;
}

byte _sprite_page_to_load = 0xFF;

static const char * const _cached_filenames[4] = {
	"cached_sprites.xxx",
	"cached_sprites.xx1",
	"cached_sprites.xx2",
	"cached_sprites.xx3",
};

#define OPENTTD_SPRITES_COUNT 70
static const uint16 _openttd_grf_indexes[] = {
	SPR_OPENTTD_BASE+0,	SPR_OPENTTD_BASE+7, // icons etc
	98,98,     // euro symbol medium size
	546,546,   // euro symbol large size
	SPR_OPENTTD_BASE+10, SPR_OPENTTD_BASE+57, // more icons
	648, 648, // nordic char: æ
	616, 616, // nordic char: Æ
	666, 666, // nordic char: Ø
	634, 634, // nordic char: Ø
	SPR_OPENTTD_BASE+62, SPR_OPENTTD_BASE + OPENTTD_SPRITES_COUNT, // more icons
	0xffff,
};

static void LoadSpriteTables()
{
	int i,j;

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

	// Try to load the sprites from cache
	if (!HandleCachedSpriteHeaders(_cached_filenames[_opt.landscape], true)) {
		// We do not have the sprites in cache yet, or cache is disabled
		// So just load all files from disk..

		int load_index = 0;

		for(i=0; _filename_list[i] != NULL; i++) {
			load_index += LoadGrfFile(_filename_list[i], load_index, (byte)i);
		}

		LoadGrfIndexed("openttd.grf", _openttd_grf_indexes, i++);

		if (_sprite_page_to_load != 0)
			LoadGrfIndexed(_landscape_filenames[_sprite_page_to_load-1], _landscape_spriteindexes[_sprite_page_to_load-1], i++);

		LoadGrfIndexed("trkfoundw.grf", _slopes_spriteindexes[_opt.landscape], i++);

		load_index = SPR_CANALS_BASE;
		load_index += LoadGrfFile("canalsw.grf", load_index, i++);

		load_index = SPR_OPENTTD_BASE + OPENTTD_SPRITES_COUNT + 1;


		/* Load newgrf sprites */

		_custom_sprites_base = load_index;

		_loading_stage = 0;
		for (j = 0; j != lengthof(_newgrf_files) && _newgrf_files[j]; j++) {
			InitNewGRFFile(_newgrf_files[j], load_index);
			load_index += LoadNewGrfFile(_newgrf_files[j], load_index, i++);
		}

		_loading_stage = 1;
		load_index = _custom_sprites_base;
		for (j = 0; j != lengthof(_newgrf_files) && _newgrf_files[j]; j++)
			load_index += LoadNewGrfFile(_newgrf_files[j], load_index, i++);


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

		for(i = 0; _filename_list[i] != NULL; i++)
			FioOpenFile(i,_filename_list[i]);

		FioOpenFile(i++, "openttd.grf");

		if (_sprite_page_to_load != 0)
			FioOpenFile(i++, _landscape_filenames[_sprite_page_to_load-1]);

		FioOpenFile(i++, "trkfoundw.grf");
		FioOpenFile(i++, "canalsw.grf");

		// FIXME: if a user changes his newgrf's, the cached-sprites gets
		//  invalid. We should have some kind of check for this.
		// The best solution for this is to delete the cached-sprites.. but how
		//  do we detect it?
		for(j=0; j!=lengthof(_newgrf_files) && _newgrf_files[j]; j++)
			FioOpenFile(i++, _newgrf_files[j]);
	}

	_compact_cache_counter = 0;
}

void GfxInitSpriteMem(byte *ptr, uint32 size)
{
	// initialize sprite cache heap
	_spritecache_ptr = ptr;
	_spritecache_size = size;

	// Sentinel block (identified by size=0)
	S_DATA(ptr + size - S_HDRSIZE) = 0;
	// A big free block
	S_DATA(ptr) = size - S_HDRSIZE + S_FREE_MASK;

	memset(_sprite_ptr, 0, sizeof(_sprite_ptr));
}


void GfxLoadSprites() {
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


const SpriteDimension *GetSpriteDimension(uint sprite)
{
	static SpriteDimension sd_static;
	SpriteDimension *sd;

#ifndef WANT_SPRITESIZES
	byte *p;

	p = _sprite_ptr[sprite];
	if (p == NULL)
		p = GetSpritePtr(sprite);

	/* decode sprite header */
	sd = &sd_static;
	sd->xoffs = (int16)READ_LE_UINT16(&((SpriteHdr*)p)->x_offs);
	sd->yoffs = (int16)READ_LE_UINT16(&((SpriteHdr*)p)->y_offs);
	sd->xsize = READ_LE_UINT16(&((SpriteHdr*)p)->width);
	sd->ysize = ((SpriteHdr*)p)->height;
#else
	sd = &sd_static;
	sd->xoffs = _sprite_xoffs[sprite];
	sd->yoffs = _sprite_yoffs[sprite];
	sd->xsize = _sprite_xsize[sprite];
	sd->ysize = _sprite_ysize[sprite];
#endif
/*	sd->xoffs = _sprite_xoffs[sprite];
	sd->yoffs = _sprite_yoffs[sprite];
	sd->xsize = _sprite_xsize[sprite];
	sd->ysize = _sprite_ysize[sprite];
*/
	return sd;
}

