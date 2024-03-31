/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file spritecache.cpp Caching of sprites. */

#include "stdafx.h"
#include "random_access_file_type.h"
#include "spriteloader/grf.hpp"
#include "gfx_func.h"
#include "error.h"
#include "error_func.h"
#include "zoom_func.h"
#include "settings_type.h"
#include "blitter/factory.hpp"
#include "core/math_func.hpp"
#include "core/mem_func.hpp"
#include "video/video_driver.hpp"
#include "spritecache.h"
#include "spritecache_internal.h"

#include "table/sprites.h"
#include "table/strings.h"
#include "table/palette_convert.h"

#include "safeguards.h"

/* Default of 4MB spritecache */
uint _sprite_cache_size = 4;

static std::vector<SpriteCache> _spritecache;
static size_t _spritecache_bytes_used = 0;
static uint32_t _sprite_lru_counter;
static std::vector<std::unique_ptr<SpriteFile>> _sprite_files;

static inline SpriteCache *GetSpriteCache(size_t index)
{
	return &_spritecache[index];
}

SpriteCache *AllocateSpriteCache(size_t index)
{
	if (index >= _spritecache.size()) {
		_spritecache.resize(index + 1);
	}

	return GetSpriteCache(index);
}

/**
 * Get the cached SpriteFile given the name of the file.
 * @param filename The name of the file at the disk.
 * @return The SpriteFile or \c null.
 */
static SpriteFile *GetCachedSpriteFileByName(const std::string &filename)
{
	for (auto &f : _sprite_files) {
		if (f->GetFilename() == filename) {
			return f.get();
		}
	}
	return nullptr;
}

/**
 * Open/get the SpriteFile that is cached for use in the sprite cache.
 * @param filename      Name of the file at the disk.
 * @param subdir        The sub directory to search this file in.
 * @param palette_remap Whether a palette remap needs to be performed for this file.
 * @return The reference to the SpriteCache.
 */
SpriteFile &OpenCachedSpriteFile(const std::string &filename, Subdirectory subdir, bool palette_remap)
{
	SpriteFile *file = GetCachedSpriteFileByName(filename);
	if (file == nullptr) {
		file = _sprite_files.insert(std::end(_sprite_files), std::make_unique<SpriteFile>(filename, subdir, palette_remap))->get();
	} else {
		file->SeekToBegin();
	}
	return *file;
}

/**
 * Skip the given amount of sprite graphics data.
 * @param type the type of sprite (compressed etc)
 * @param num the amount of sprites to skip
 * @return true if the data could be correctly skipped.
 */
bool SkipSpriteData(SpriteFile &file, uint8_t type, uint16_t num)
{
	if (type & 2) {
		file.SkipBytes(num);
	} else {
		while (num > 0) {
			int8_t i = file.ReadByte();
			if (i >= 0) {
				int size = (i == 0) ? 0x80 : i;
				if (size > num) return false;
				num -= size;
				file.SkipBytes(size);
			} else {
				i = -(i >> 3);
				num -= i;
				file.ReadByte();
			}
		}
	}
	return true;
}

/* Check if the given Sprite ID exists */
bool SpriteExists(SpriteID id)
{
	if (static_cast<size_t>(id) >= _spritecache.size()) return false;

	/* Special case for Sprite ID zero -- its position is also 0... */
	if (id == 0) return true;
	return !(GetSpriteCache(id)->file_pos == 0 && GetSpriteCache(id)->file == nullptr);
}

/**
 * Get the sprite type of a given sprite.
 * @param sprite The sprite to look at.
 * @return the type of sprite.
 */
SpriteType GetSpriteType(SpriteID sprite)
{
	if (!SpriteExists(sprite)) return SpriteType::Invalid;
	return GetSpriteCache(sprite)->type;
}

/**
 * Get the SpriteFile of a given sprite.
 * @param sprite The sprite to look at.
 * @return The SpriteFile.
 */
SpriteFile *GetOriginFile(SpriteID sprite)
{
	if (!SpriteExists(sprite)) return nullptr;
	return GetSpriteCache(sprite)->file;
}

/**
 * Get the GRF-local sprite id of a given sprite.
 * @param sprite The sprite to look at.
 * @return The GRF-local sprite id.
 */
uint32_t GetSpriteLocalID(SpriteID sprite)
{
	if (!SpriteExists(sprite)) return 0;
	return GetSpriteCache(sprite)->id;
}

/**
 * Count the sprites which originate from a specific file in a range of SpriteIDs.
 * @param file The loaded SpriteFile.
 * @param begin First sprite in range.
 * @param end First sprite not in range.
 * @return Number of sprites.
 */
uint GetSpriteCountForFile(const std::string &filename, SpriteID begin, SpriteID end)
{
	SpriteFile *file = GetCachedSpriteFileByName(filename);
	if (file == nullptr) return 0;

	uint count = 0;
	for (SpriteID i = begin; i != end; i++) {
		if (SpriteExists(i)) {
			SpriteCache *sc = GetSpriteCache(i);
			if (sc->file == file) {
				count++;
				Debug(sprite, 4, "Sprite: {}", i);
			}
		}
	}
	return count;
}

/**
 * Get a reasonable (upper bound) estimate of the maximum
 * SpriteID used in OpenTTD; there will be no sprites with
 * a higher SpriteID, although there might be up to roughly
 * a thousand unused SpriteIDs below this number.
 * @note It's actually the number of spritecache items.
 * @return maximum SpriteID
 */
uint GetMaxSpriteID()
{
	return static_cast<uint>(_spritecache.size());
}

static bool ResizeSpriteIn(SpriteLoader::SpriteCollection &sprite, ZoomLevel src, ZoomLevel tgt)
{
	uint8_t scaled_1 = ScaleByZoom(1, (ZoomLevel)(src - tgt));

	/* Check for possible memory overflow. */
	if (sprite[src].width * scaled_1 > UINT16_MAX || sprite[src].height * scaled_1 > UINT16_MAX) return false;

	sprite[tgt].width  = sprite[src].width  * scaled_1;
	sprite[tgt].height = sprite[src].height * scaled_1;
	sprite[tgt].x_offs = sprite[src].x_offs * scaled_1;
	sprite[tgt].y_offs = sprite[src].y_offs * scaled_1;
	sprite[tgt].colours = sprite[src].colours;

	sprite[tgt].AllocateData(tgt, static_cast<size_t>(sprite[tgt].width) * sprite[tgt].height);

	SpriteLoader::CommonPixel *dst = sprite[tgt].data;
	for (int y = 0; y < sprite[tgt].height; y++) {
		const SpriteLoader::CommonPixel *src_ln = &sprite[src].data[y / scaled_1 * sprite[src].width];
		for (int x = 0; x < sprite[tgt].width; x++) {
			*dst = src_ln[x / scaled_1];
			dst++;
		}
	}

	return true;
}

static void ResizeSpriteOut(SpriteLoader::SpriteCollection &sprite, ZoomLevel zoom)
{
	/* Algorithm based on 32bpp_Optimized::ResizeSprite() */
	sprite[zoom].width  = UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].width,  zoom);
	sprite[zoom].height = UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].height, zoom);
	sprite[zoom].x_offs = UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].x_offs, zoom);
	sprite[zoom].y_offs = UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].y_offs, zoom);
	sprite[zoom].colours = sprite[ZOOM_LVL_NORMAL].colours;

	sprite[zoom].AllocateData(zoom, static_cast<size_t>(sprite[zoom].height) * sprite[zoom].width);

	SpriteLoader::CommonPixel *dst = sprite[zoom].data;
	const SpriteLoader::CommonPixel *src = sprite[zoom - 1].data;
	[[maybe_unused]] const SpriteLoader::CommonPixel *src_end = src + sprite[zoom - 1].height * sprite[zoom - 1].width;

	for (uint y = 0; y < sprite[zoom].height; y++) {
		const SpriteLoader::CommonPixel *src_ln = src + sprite[zoom - 1].width;
		assert(src_ln <= src_end);
		for (uint x = 0; x < sprite[zoom].width; x++) {
			assert(src < src_ln);
			if (src + 1 != src_ln && (src + 1)->a != 0) {
				*dst = *(src + 1);
			} else {
				*dst = *src;
			}
			dst++;
			src += 2;
		}
		src = src_ln + sprite[zoom - 1].width;
	}
}

static bool PadSingleSprite(SpriteLoader::Sprite *sprite, ZoomLevel zoom, uint pad_left, uint pad_top, uint pad_right, uint pad_bottom)
{
	uint width  = sprite->width + pad_left + pad_right;
	uint height = sprite->height + pad_top + pad_bottom;

	if (width > UINT16_MAX || height > UINT16_MAX) return false;

	/* Copy source data and reallocate sprite memory. */
	size_t sprite_size = static_cast<size_t>(sprite->width) * sprite->height;
	SpriteLoader::CommonPixel *src_data = MallocT<SpriteLoader::CommonPixel>(sprite_size);
	MemCpyT(src_data, sprite->data, sprite_size);
	sprite->AllocateData(zoom, static_cast<size_t>(width) * height);

	/* Copy with padding to destination. */
	SpriteLoader::CommonPixel *src = src_data;
	SpriteLoader::CommonPixel *data = sprite->data;
	for (uint y = 0; y < height; y++) {
		if (y < pad_top || pad_bottom + y >= height) {
			/* Top/bottom padding. */
			MemSetT(data, 0, width);
			data += width;
		} else {
			if (pad_left > 0) {
				/* Pad left. */
				MemSetT(data, 0, pad_left);
				data += pad_left;
			}

			/* Copy pixels. */
			MemCpyT(data, src, sprite->width);
			src += sprite->width;
			data += sprite->width;

			if (pad_right > 0) {
				/* Pad right. */
				MemSetT(data, 0, pad_right);
				data += pad_right;
			}
		}
	}
	free(src_data);

	/* Update sprite size. */
	sprite->width   = width;
	sprite->height  = height;
	sprite->x_offs -= pad_left;
	sprite->y_offs -= pad_top;

	return true;
}

static bool PadSprites(SpriteLoader::SpriteCollection &sprite, uint8_t sprite_avail, SpriteEncoder *encoder)
{
	/* Get minimum top left corner coordinates. */
	int min_xoffs = INT32_MAX;
	int min_yoffs = INT32_MAX;
	for (ZoomLevel zoom = ZOOM_LVL_BEGIN; zoom != ZOOM_LVL_END; zoom++) {
		if (HasBit(sprite_avail, zoom)) {
			min_xoffs = std::min(min_xoffs, ScaleByZoom(sprite[zoom].x_offs, zoom));
			min_yoffs = std::min(min_yoffs, ScaleByZoom(sprite[zoom].y_offs, zoom));
		}
	}

	/* Get maximum dimensions taking necessary padding at the top left into account. */
	int max_width  = INT32_MIN;
	int max_height = INT32_MIN;
	for (ZoomLevel zoom = ZOOM_LVL_BEGIN; zoom != ZOOM_LVL_END; zoom++) {
		if (HasBit(sprite_avail, zoom)) {
			max_width  = std::max(max_width, ScaleByZoom(sprite[zoom].width + sprite[zoom].x_offs - UnScaleByZoom(min_xoffs, zoom), zoom));
			max_height = std::max(max_height, ScaleByZoom(sprite[zoom].height + sprite[zoom].y_offs - UnScaleByZoom(min_yoffs, zoom), zoom));
		}
	}

	/* Align height and width if required to match the needs of the sprite encoder. */
	uint align = encoder->GetSpriteAlignment();
	if (align != 0) {
		max_width  = Align(max_width,  align);
		max_height = Align(max_height, align);
	}

	/* Pad sprites where needed. */
	for (ZoomLevel zoom = ZOOM_LVL_BEGIN; zoom != ZOOM_LVL_END; zoom++) {
		if (HasBit(sprite_avail, zoom)) {
			/* Scaling the sprite dimensions in the blitter is done with rounding up,
			 * so a negative padding here is not an error. */
			int pad_left   = std::max(0, sprite[zoom].x_offs - UnScaleByZoom(min_xoffs, zoom));
			int pad_top    = std::max(0, sprite[zoom].y_offs - UnScaleByZoom(min_yoffs, zoom));
			int pad_right  = std::max(0, UnScaleByZoom(max_width, zoom) - sprite[zoom].width - pad_left);
			int pad_bottom = std::max(0, UnScaleByZoom(max_height, zoom) - sprite[zoom].height - pad_top);

			if (pad_left > 0 || pad_right > 0 || pad_top > 0 || pad_bottom > 0) {
				if (!PadSingleSprite(&sprite[zoom], zoom, pad_left, pad_top, pad_right, pad_bottom)) return false;
			}
		}
	}

	return true;
}

static bool ResizeSprites(SpriteLoader::SpriteCollection &sprite, uint8_t sprite_avail, SpriteEncoder *encoder)
{
	/* Create a fully zoomed image if it does not exist */
	ZoomLevel first_avail = static_cast<ZoomLevel>(FindFirstBit(sprite_avail));
	if (first_avail != ZOOM_LVL_NORMAL) {
		if (!ResizeSpriteIn(sprite, first_avail, ZOOM_LVL_NORMAL)) return false;
		SetBit(sprite_avail, ZOOM_LVL_NORMAL);
	}

	/* Pad sprites to make sizes match. */
	if (!PadSprites(sprite, sprite_avail, encoder)) return false;

	/* Create other missing zoom levels */
	for (ZoomLevel zoom = ZOOM_LVL_OUT_2X; zoom != ZOOM_LVL_END; zoom++) {
		if (HasBit(sprite_avail, zoom)) {
			/* Check that size and offsets match the fully zoomed image. */
			assert(sprite[zoom].width  == UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].width,  zoom));
			assert(sprite[zoom].height == UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].height, zoom));
			assert(sprite[zoom].x_offs == UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].x_offs, zoom));
			assert(sprite[zoom].y_offs == UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].y_offs, zoom));
		}

		/* Zoom level is not available, or unusable, so create it */
		if (!HasBit(sprite_avail, zoom)) ResizeSpriteOut(sprite, zoom);
	}

	/* Upscale to desired sprite_min_zoom if provided sprite only had zoomed in versions. */
	if (first_avail < _settings_client.gui.sprite_zoom_min) {
		if (_settings_client.gui.sprite_zoom_min >= ZOOM_LVL_OUT_4X) ResizeSpriteIn(sprite, ZOOM_LVL_OUT_4X, ZOOM_LVL_OUT_2X);
		if (_settings_client.gui.sprite_zoom_min >= ZOOM_LVL_OUT_2X) ResizeSpriteIn(sprite, ZOOM_LVL_OUT_2X, ZOOM_LVL_NORMAL);
	}

	return  true;
}

/**
 * Load a recolour sprite into memory.
 * @param file GRF we're reading from.
 * @param num Size of the sprite in the GRF.
 * @return Sprite data.
 */
static void *ReadRecolourSprite(SpriteFile &file, uint num, SpriteAllocator &allocator)
{
	/* "Normal" recolour sprites are ALWAYS 257 bytes. Then there is a small
	 * number of recolour sprites that are 17 bytes that only exist in DOS
	 * GRFs which are the same as 257 byte recolour sprites, but with the last
	 * 240 bytes zeroed.  */
	static const uint RECOLOUR_SPRITE_SIZE = 257;
	uint8_t *dest = (uint8_t *)allocator.Allocate(std::max(RECOLOUR_SPRITE_SIZE, num));

	if (file.NeedsPaletteRemap()) {
		uint8_t *dest_tmp = new uint8_t[std::max(RECOLOUR_SPRITE_SIZE, num)];

		/* Only a few recolour sprites are less than 257 bytes */
		if (num < RECOLOUR_SPRITE_SIZE) memset(dest_tmp, 0, RECOLOUR_SPRITE_SIZE);
		file.ReadBlock(dest_tmp, num);

		/* The data of index 0 is never used; "literal 00" according to the (New)GRF specs. */
		for (uint i = 1; i < RECOLOUR_SPRITE_SIZE; i++) {
			dest[i] = _palmap_w2d[dest_tmp[_palmap_d2w[i - 1] + 1]];
		}
		delete[] dest_tmp;
	} else {
		file.ReadBlock(dest, num);
	}

	return dest;
}

/**
 * Read a sprite from disk.
 * @param sc          Location of sprite.
 * @param id          Sprite number.
 * @param sprite_type Type of sprite.
 * @param allocator   Allocator function to use.
 * @param encoder     Sprite encoder to use.
 * @return Read sprite data.
 */
static void *ReadSprite(const SpriteCache *sc, SpriteID id, SpriteType sprite_type, SpriteAllocator &allocator, SpriteEncoder *encoder)
{
	/* Use current blitter if no other sprite encoder is given. */
	if (encoder == nullptr) encoder = BlitterFactory::GetCurrentBlitter();

	SpriteFile &file = *sc->file;
	size_t file_pos = sc->file_pos;

	assert(sprite_type != SpriteType::Recolour);
	assert(IsMapgenSpriteID(id) == (sprite_type == SpriteType::MapGen));
	assert(sc->type == sprite_type);

	Debug(sprite, 9, "Load sprite {}", id);

	SpriteLoader::SpriteCollection sprite;
	uint8_t sprite_avail = 0;
	sprite[ZOOM_LVL_NORMAL].type = sprite_type;

	SpriteLoaderGrf sprite_loader(file.GetContainerVersion());
	if (sprite_type != SpriteType::MapGen && encoder->Is32BppSupported()) {
		/* Try for 32bpp sprites first. */
		sprite_avail = sprite_loader.LoadSprite(sprite, file, file_pos, sprite_type, true, sc->control_flags);
	}
	if (sprite_avail == 0) {
		sprite_avail = sprite_loader.LoadSprite(sprite, file, file_pos, sprite_type, false, sc->control_flags);
	}

	if (sprite_avail == 0) {
		if (sprite_type == SpriteType::MapGen) return nullptr;
		if (id == SPR_IMG_QUERY) UserError("Okay... something went horribly wrong. I couldn't load the fallback sprite. What should I do?");
		return (void*)GetRawSprite(SPR_IMG_QUERY, SpriteType::Normal, &allocator, encoder);
	}

	if (sprite_type == SpriteType::MapGen) {
		/* Ugly hack to work around the problem that the old landscape
		 *  generator assumes that those sprites are stored uncompressed in
		 *  the memory, and they are only read directly by the code, never
		 *  send to the blitter. So do not send it to the blitter (which will
		 *  result in a data array in the format the blitter likes most), but
		 *  extract the data directly and store that as sprite.
		 * Ugly: yes. Other solution: no. Blame the original author or
		 *  something ;) The image should really have been a data-stream
		 *  (so type = 0xFF basically). */
		uint num = sprite[ZOOM_LVL_NORMAL].width * sprite[ZOOM_LVL_NORMAL].height;

		Sprite *s = (Sprite *)allocator.Allocate(sizeof(*s) + num);
		s->width  = sprite[ZOOM_LVL_NORMAL].width;
		s->height = sprite[ZOOM_LVL_NORMAL].height;
		s->x_offs = sprite[ZOOM_LVL_NORMAL].x_offs;
		s->y_offs = sprite[ZOOM_LVL_NORMAL].y_offs;

		SpriteLoader::CommonPixel *src = sprite[ZOOM_LVL_NORMAL].data;
		uint8_t *dest = s->data;
		while (num-- > 0) {
			*dest++ = src->m;
			src++;
		}

		return s;
	}

	if (!ResizeSprites(sprite, sprite_avail, encoder)) {
		if (id == SPR_IMG_QUERY) UserError("Okay... something went horribly wrong. I couldn't resize the fallback sprite. What should I do?");
		return (void*)GetRawSprite(SPR_IMG_QUERY, SpriteType::Normal, &allocator, encoder);
	}

	if (sprite[ZOOM_LVL_NORMAL].type == SpriteType::Font && _font_zoom != ZOOM_LVL_NORMAL) {
		/* Make ZOOM_LVL_NORMAL be ZOOM_LVL_GUI */
		sprite[ZOOM_LVL_NORMAL].width  = sprite[_font_zoom].width;
		sprite[ZOOM_LVL_NORMAL].height = sprite[_font_zoom].height;
		sprite[ZOOM_LVL_NORMAL].x_offs = sprite[_font_zoom].x_offs;
		sprite[ZOOM_LVL_NORMAL].y_offs = sprite[_font_zoom].y_offs;
		sprite[ZOOM_LVL_NORMAL].data   = sprite[_font_zoom].data;
		sprite[ZOOM_LVL_NORMAL].colours = sprite[_font_zoom].colours;
	}

	return encoder->Encode(sprite, allocator);
}

struct GrfSpriteOffset {
	size_t file_pos;
	uint8_t control_flags;
};

/** Map from sprite numbers to position in the GRF file. */
static std::map<uint32_t, GrfSpriteOffset> _grf_sprite_offsets;

/**
 * Get the file offset for a specific sprite in the sprite section of a GRF.
 * @param id ID of the sprite to look up.
 * @return Position of the sprite in the sprite section or SIZE_MAX if no such sprite is present.
 */
size_t GetGRFSpriteOffset(uint32_t id)
{
	return _grf_sprite_offsets.find(id) != _grf_sprite_offsets.end() ? _grf_sprite_offsets[id].file_pos : SIZE_MAX;
}

/**
 * Parse the sprite section of GRFs.
 * @param container_version Container version of the GRF we're currently processing.
 */
void ReadGRFSpriteOffsets(SpriteFile &file)
{
	_grf_sprite_offsets.clear();

	if (file.GetContainerVersion() >= 2) {
		/* Seek to sprite section of the GRF. */
		size_t data_offset = file.ReadDword();
		size_t old_pos = file.GetPos();
		file.SeekTo(data_offset, SEEK_CUR);

		GrfSpriteOffset offset = { 0, 0 };

		/* Loop over all sprite section entries and store the file
		 * offset for each newly encountered ID. */
		uint32_t id, prev_id = 0;
		while ((id = file.ReadDword()) != 0) {
			if (id != prev_id) {
				_grf_sprite_offsets[prev_id] = offset;
				offset.file_pos = file.GetPos() - 4;
				offset.control_flags = 0;
			}
			prev_id = id;
			uint length = file.ReadDword();
			if (length > 0) {
				uint8_t colour = file.ReadByte() & SCC_MASK;
				length--;
				if (length > 0) {
					uint8_t zoom = file.ReadByte();
					length--;
					if (colour != 0 && zoom == 0) { // ZOOM_LVL_OUT_4X (normal zoom)
						SetBit(offset.control_flags, (colour != SCC_PAL) ? SCCF_ALLOW_ZOOM_MIN_1X_32BPP : SCCF_ALLOW_ZOOM_MIN_1X_PAL);
						SetBit(offset.control_flags, (colour != SCC_PAL) ? SCCF_ALLOW_ZOOM_MIN_2X_32BPP : SCCF_ALLOW_ZOOM_MIN_2X_PAL);
					}
					if (colour != 0 && zoom == 2) { // ZOOM_LVL_OUT_2X (2x zoomed in)
						SetBit(offset.control_flags, (colour != SCC_PAL) ? SCCF_ALLOW_ZOOM_MIN_2X_32BPP : SCCF_ALLOW_ZOOM_MIN_2X_PAL);
					}
				}
			}
			file.SkipBytes(length);
		}
		if (prev_id != 0) _grf_sprite_offsets[prev_id] = offset;

		/* Continue processing the data section. */
		file.SeekTo(old_pos, SEEK_SET);
	}
}


/**
 * Load a real or recolour sprite.
 * @param load_index Global sprite index.
 * @param file GRF to load from.
 * @param file_sprite_id Sprite number in the GRF.
 * @param container_version Container version of the GRF.
 * @return True if a valid sprite was loaded, false on any error.
 */
bool LoadNextSprite(uint load_index, SpriteFile &file, uint file_sprite_id)
{
	size_t file_pos = file.GetPos();

	/* Read sprite header. */
	uint32_t num = file.GetContainerVersion() >= 2 ? file.ReadDword() : file.ReadWord();
	if (num == 0) return false;
	uint8_t grf_type = file.ReadByte();

	SpriteType type;
	uint8_t control_flags = 0;
	if (grf_type == 0xFF) {
		/* Some NewGRF files have "empty" pseudo-sprites which are 1
		 * byte long. Catch these so the sprites won't be displayed. */
		if (num == 1) {
			file.ReadByte();
			return false;
		}
		type = SpriteType::Recolour;
	} else if (file.GetContainerVersion() >= 2 && grf_type == 0xFD) {
		if (num != 4) {
			/* Invalid sprite section include, ignore. */
			file.SkipBytes(num);
			return false;
		}
		/* It is not an error if no sprite with the provided ID is found in the sprite section. */
		auto iter = _grf_sprite_offsets.find(file.ReadDword());
		if (iter != _grf_sprite_offsets.end()) {
			file_pos = iter->second.file_pos;
			control_flags = iter->second.control_flags;
		} else {
			file_pos = SIZE_MAX;
		}
		type = SpriteType::Normal;
	} else {
		file.SkipBytes(7);
		type = SkipSpriteData(file, grf_type, num - 8) ? SpriteType::Normal : SpriteType::Invalid;
		/* Inline sprites are not supported for container version >= 2. */
		if (file.GetContainerVersion() >= 2) return false;
	}

	if (type == SpriteType::Invalid) return false;

	if (load_index >= MAX_SPRITES) {
		UserError("Tried to load too many sprites (#{}; max {})", load_index, MAX_SPRITES);
	}

	bool is_mapgen = IsMapgenSpriteID(load_index);

	if (is_mapgen) {
		if (type != SpriteType::Normal) UserError("Uhm, would you be so kind not to load a NewGRF that changes the type of the map generator sprites?");
		type = SpriteType::MapGen;
	}

	SpriteCache *sc = AllocateSpriteCache(load_index);
	sc->file = &file;
	sc->file_pos = file_pos;
	CacheSpriteAllocator allocator(sc->data);
	if (type == SpriteType::Recolour) ReadRecolourSprite(file, num, allocator);
	sc->lru = 0;
	sc->id = file_sprite_id;
	sc->type = type;
	sc->warned = false;
	sc->control_flags = control_flags;

	return true;
}

void DupSprite(SpriteID old_spr, SpriteID new_spr)
{
	SpriteCache *scnew = AllocateSpriteCache(new_spr); // may reallocate: so put it first
	SpriteCache *scold = GetSpriteCache(old_spr);

	scnew->file = scold->file;
	scnew->file_pos = scold->file_pos;
	scnew->ClearSpriteData();
	scnew->id = scold->id;
	scnew->type = scold->type;
	scnew->warned = false;
	scnew->control_flags = scold->control_flags;
}

/**
 * Delete entries from the sprite cache to remove the requested number of bytes.
 * Sprite data is removed in order of LRU values.
 * The total number of bytes removed may be larger than the number requested.
 * @param to_remove Requested number of bytes to remove.
 */
static void DeleteEntriesFromSpriteCache(size_t to_remove)
{
	const size_t initial_in_use = _spritecache_bytes_used;

	struct SpriteInfo {
		uint32_t lru;
		SpriteID id;
		size_t size;

		bool operator<(const SpriteInfo &other) const
		{
			return this->lru < other.lru;
		}
	};

	std::vector<SpriteInfo> candidates; // max heap, ordered by LRU
	size_t candidate_bytes = 0;         // total bytes that would be released when clearing all sprites in candidates

	auto push = [&](SpriteInfo info) {
		candidates.push_back(info);
		std::push_heap(candidates.begin(), candidates.end());
		candidate_bytes += info.size;
	};

	auto pop = [&]() {
		candidate_bytes -= candidates.front().size;
		std::pop_heap(candidates.begin(), candidates.end());
		candidates.pop_back();
	};

	SpriteID i = 0;
	for (; i != static_cast<SpriteID>(_spritecache.size()) && candidate_bytes < to_remove; i++) {
		const SpriteCache *sc = GetSpriteCache(i);
		if (sc->type != SpriteType::Recolour && !sc->data.empty()) {
			push({ sc->lru, i, sc->data.size() });
			if (candidate_bytes >= to_remove) break;
		}
	}
	/* candidates now contains enough bytes to meet to_remove.
	 * only sprites with LRU values <= the maximum (i.e. the top of the heap) need to be considered */
	for (; i != static_cast<SpriteID>(_spritecache.size()); i++) {
		const SpriteCache *sc = GetSpriteCache(i);
		if (sc->type != SpriteType::Recolour && !sc->data.empty() && sc->lru <= candidates.front().lru) {
			push({ sc->lru, i, sc->data.size() });
			while (!candidates.empty() && candidate_bytes - candidates.front().size >= to_remove) {
				pop();
			}
		}
	}

	for (const auto &it : candidates) {
		GetSpriteCache(it.id)->ClearSpriteData();
	}

	Debug(sprite, 3, "DeleteEntriesFromSpriteCache, deleted: {}, freed: {}, in use: {} --> {}, requested: {}",
			candidates.size(), candidate_bytes, initial_in_use, _spritecache_bytes_used, to_remove);
}

void IncreaseSpriteLRU()
{
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	uint target_size = (bpp > 0 ? _sprite_cache_size * bpp / 8 : 1) * 1024 * 1024;
	if (_spritecache_bytes_used > target_size) {
		DeleteEntriesFromSpriteCache(_spritecache_bytes_used - target_size + 512 * 1024);
	}

	if (_sprite_lru_counter >= 0xC0000000) {
		Debug(sprite, 3, "Fixing lru {}, inuse={}", _sprite_lru_counter, _spritecache_bytes_used);

		for (SpriteCache &sc : _spritecache) {
			if (!sc.data.empty()) {
				if (sc.lru > 0x80000000) {
					sc.lru -= 0x80000000;
				} else {
					sc.lru = 0;
				}
			}
		}
		_sprite_lru_counter -= 0x80000000;
	}
}

void SpriteCache::ClearSpriteData()
{
	_spritecache_bytes_used -= this->data.size();
	this->data.clear();
	this->data.shrink_to_fit();
}

void *CacheSpriteAllocator::Allocate(size_t size)
{
	_spritecache_bytes_used -= this->data.size();
	this->data.resize(size);
	_spritecache_bytes_used += this->data.size();
	return this->data.data();
}

/**
 * Sprite allocator simply using malloc.
 */
void *SimpleSpriteAllocator::Allocate(size_t size)
{
	return MallocT<uint8_t>(size);
}

/**
 * Handles the case when a sprite of different type is requested than is present in the SpriteCache.
 * For SpriteType::Font sprites, it is normal. In other cases, default sprite is loaded instead.
 * @param sprite ID of loaded sprite
 * @param requested requested sprite type
 * @param sc the currently known sprite cache for the requested sprite
 * @return fallback sprite
 * @note this function will do UserError() in the case the fallback sprite isn't available
 */
static void *HandleInvalidSpriteRequest(SpriteID sprite, SpriteType requested, SpriteCache *sc, SpriteAllocator *allocator)
{
	static const char * const sprite_types[] = {
		"normal",        // SpriteType::Normal
		"map generator", // SpriteType::MapGen
		"character",     // SpriteType::Font
		"recolour",      // SpriteType::Recolour
	};

	SpriteType available = sc->type;
	if (requested == SpriteType::Font && available == SpriteType::Normal) {
		if (sc->data.empty()) sc->type = SpriteType::Font;
		return GetRawSprite(sprite, sc->type, allocator);
	}

	uint8_t warning_level = sc->warned ? 6 : 0;
	sc->warned = true;
	Debug(sprite, warning_level, "Tried to load {} sprite #{} as a {} sprite. Probable cause: NewGRF interference", sprite_types[static_cast<uint8_t>(available)], sprite, sprite_types[static_cast<uint8_t>(requested)]);

	switch (requested) {
		case SpriteType::Normal:
			if (sprite == SPR_IMG_QUERY) UserError("Uhm, would you be so kind not to load a NewGRF that makes the 'query' sprite a non-normal sprite?");
			[[fallthrough]];
		case SpriteType::Font:
			return GetRawSprite(SPR_IMG_QUERY, SpriteType::Normal, allocator);
		case SpriteType::Recolour:
			if (sprite == PALETTE_TO_DARK_BLUE) UserError("Uhm, would you be so kind not to load a NewGRF that makes the 'PALETTE_TO_DARK_BLUE' sprite a non-remap sprite?");
			return GetRawSprite(PALETTE_TO_DARK_BLUE, SpriteType::Recolour, allocator);
		case SpriteType::MapGen:
			/* this shouldn't happen, overriding of SpriteType::MapGen sprites is checked in LoadNextSprite()
			 * (the only case the check fails is when these sprites weren't even loaded...) */
		default:
			NOT_REACHED();
	}
}

/**
 * Reads a sprite (from disk or sprite cache).
 * If the sprite is not available or of wrong type, a fallback sprite is returned.
 * @param sprite Sprite to read.
 * @param type Expected sprite type.
 * @param allocator Allocator function to use. Set to nullptr to use the usual sprite cache.
 * @param encoder Sprite encoder to use. Set to nullptr to use the currently active blitter.
 * @return Sprite raw data
 */
void *GetRawSprite(SpriteID sprite, SpriteType type, SpriteAllocator *allocator, SpriteEncoder *encoder)
{
	assert(type != SpriteType::MapGen || IsMapgenSpriteID(sprite));
	assert(type < SpriteType::Invalid);

	if (!SpriteExists(sprite)) {
		Debug(sprite, 1, "Tried to load non-existing sprite #{}. Probable cause: Wrong/missing NewGRFs", sprite);

		/* SPR_IMG_QUERY is a BIG FAT RED ? */
		sprite = SPR_IMG_QUERY;
	}

	SpriteCache *sc = GetSpriteCache(sprite);

	if (sc->type != type) return HandleInvalidSpriteRequest(sprite, type, sc, allocator);

	if (allocator == nullptr && encoder == nullptr) {
		/* Load sprite into/from spritecache */

		/* Update LRU */
		sc->lru = ++_sprite_lru_counter;

		/* Load the sprite, if it is not loaded, yet */
		if (sc->data.empty()) {
			CacheSpriteAllocator cache_allocator(sc->data);
			ReadSprite(sc, sprite, type, cache_allocator, nullptr);
		}

		return static_cast<void *>(sc->data.data());
	} else {
		/* Do not use the spritecache, but a different allocator. */
		return ReadSprite(sc, sprite, type, *allocator, encoder);
	}
}

void GfxInitSpriteMem()
{
	/* Reset the spritecache 'pool' */
	_spritecache.clear();
	_sprite_files.clear();
	_spritecache_bytes_used = 0;
}

/**
 * Remove all encoded sprites from the sprite cache without
 * discarding sprite location information.
 */
void GfxClearSpriteCache()
{
	/* Clear sprite ptr for all cached items */
	for (SpriteCache &sc : _spritecache) {
		if (sc.type != SpriteType::Recolour && !sc.data.empty()) sc.ClearSpriteData();
	}

	VideoDriver::GetInstance()->ClearSystemSprites();
}

/**
 * Remove all encoded font sprites from the sprite cache without
 * discarding sprite location information.
 */
void GfxClearFontSpriteCache()
{
	/* Clear sprite ptr for all cached font items */
	for (SpriteCache &sc : _spritecache) {
		if (sc.type == SpriteType::Font && !sc.data.empty()) sc.ClearSpriteData();
	}
}

/**
 * Shrink to fit the sprite cache index.
 */
void GfxShrinkToFitSpriteCacheIndex()
{
	_spritecache.shrink_to_fit();
}

/* static */ ReusableBuffer<SpriteLoader::CommonPixel> SpriteLoader::Sprite::buffer[ZOOM_LVL_END];
