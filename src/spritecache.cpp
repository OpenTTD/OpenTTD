/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file spritecache.cpp Caching of sprites. */

#include "stdafx.h"
#include "spriteloader/grf.hpp"
#include "spriteloader/makeindexed.h"
#include "error_func.h"
#include "strings_func.h"
#include "zoom_func.h"
#include "settings_type.h"
#include "blitter/factory.hpp"
#include "core/math_func.hpp"
#include "video/video_driver.hpp"
#include "spritecache.h"
#include "spritecache_internal.h"

#include "table/sprites.h"
#include "table/palette_convert.h"

#include "safeguards.h"

/* Default of 4MB spritecache */
uint _sprite_cache_size = 4;


static std::vector<SpriteCache> _spritecache;
static size_t _spritecache_bytes_used = 0;
static uint32_t _sprite_lru_counter;
static std::vector<std::unique_ptr<SpriteFile>> _sprite_files;

static inline SpriteCache *GetSpriteCache(uint index)
{
	return &_spritecache[index];
}

SpriteCache *AllocateSpriteCache(uint index)
{
	if (index >= _spritecache.size()) {
		/* Add another 1024 items to the 'pool' */
		uint items = Align(index + 1, 1024);

		Debug(sprite, 4, "Increasing sprite cache to {} items ({} bytes)", items, items * sizeof(SpriteCache));

		_spritecache.resize(items);
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
 * Get the list of cached SpriteFiles.
 * @return Read-only list of cache SpriteFiles.
 */
std::span<const std::unique_ptr<SpriteFile>> GetCachedSpriteFiles()
{
	return _sprite_files;
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
	if (id >= _spritecache.size()) return false;

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
SpriteID GetMaxSpriteID()
{
	return static_cast<SpriteID>(_spritecache.size());
}

static bool ResizeSpriteIn(SpriteLoader::SpriteCollection &sprite, ZoomLevel src, ZoomLevel tgt)
{
	uint8_t scaled_1 = AdjustByZoom(1, src - tgt);
	const auto &src_sprite = sprite[src];
	auto &dest_sprite = sprite[tgt];

	/* Check for possible memory overflow. */
	if (src_sprite.width * scaled_1 > UINT16_MAX || src_sprite.height * scaled_1 > UINT16_MAX) return false;

	dest_sprite.width = src_sprite.width * scaled_1;
	dest_sprite.height = src_sprite.height * scaled_1;
	dest_sprite.x_offs = src_sprite.x_offs * scaled_1;
	dest_sprite.y_offs = src_sprite.y_offs * scaled_1;
	dest_sprite.colours = src_sprite.colours;

	dest_sprite.AllocateData(tgt, static_cast<size_t>(dest_sprite.width) * dest_sprite.height);

	SpriteLoader::CommonPixel *dst = dest_sprite.data;
	for (int y = 0; y < dest_sprite.height; y++) {
		const SpriteLoader::CommonPixel *src_ln = &src_sprite.data[y / scaled_1 * src_sprite.width];
		for (int x = 0; x < dest_sprite.width; x++) {
			*dst = src_ln[x / scaled_1];
			dst++;
		}
	}

	return true;
}

static void ResizeSpriteOut(SpriteLoader::SpriteCollection &sprite, ZoomLevel zoom)
{
	const auto &root_sprite = sprite.Root();
	const auto &src_sprite = sprite[zoom - 1];
	auto &dest_sprite = sprite[zoom];

	/* Algorithm based on 32bpp_Optimized::ResizeSprite() */
	dest_sprite.width = UnScaleByZoom(root_sprite.width, zoom);
	dest_sprite.height = UnScaleByZoom(root_sprite.height, zoom);
	dest_sprite.x_offs = UnScaleByZoom(root_sprite.x_offs, zoom);
	dest_sprite.y_offs = UnScaleByZoom(root_sprite.y_offs, zoom);
	dest_sprite.colours = root_sprite.colours;

	dest_sprite.AllocateData(zoom, static_cast<size_t>(dest_sprite.height) * dest_sprite.width);

	SpriteLoader::CommonPixel *dst = dest_sprite.data;
	const SpriteLoader::CommonPixel *src = src_sprite.data;
	[[maybe_unused]] const SpriteLoader::CommonPixel *src_end = src + src_sprite.height * src_sprite.width;

	for (uint y = 0; y < dest_sprite.height; y++) {
		const SpriteLoader::CommonPixel *src_ln = src + src_sprite.width;
		assert(src_ln <= src_end);
		for (uint x = 0; x < dest_sprite.width; x++) {
			assert(src < src_ln);
			if (src + 1 != src_ln && (src + 1)->a != 0) {
				*dst = *(src + 1);
			} else {
				*dst = *src;
			}
			dst++;
			src += 2;
		}
		src = src_ln + src_sprite.width;
	}
}

static bool PadSingleSprite(SpriteLoader::Sprite *sprite, ZoomLevel zoom, uint pad_left, uint pad_top, uint pad_right, uint pad_bottom)
{
	uint width  = sprite->width + pad_left + pad_right;
	uint height = sprite->height + pad_top + pad_bottom;

	if (width > UINT16_MAX || height > UINT16_MAX) return false;

	/* Copy source data and reallocate sprite memory. */
	size_t sprite_size = static_cast<size_t>(sprite->width) * sprite->height;
	std::vector<SpriteLoader::CommonPixel> src_data(sprite->data, sprite->data + sprite_size);
	sprite->AllocateData(zoom, static_cast<size_t>(width) * height);

	/* Copy with padding to destination. */
	SpriteLoader::CommonPixel *src = src_data.data();
	SpriteLoader::CommonPixel *data = sprite->data;
	for (uint y = 0; y < height; y++) {
		if (y < pad_top || pad_bottom + y >= height) {
			/* Top/bottom padding. */
			std::fill_n(data, width, SpriteLoader::CommonPixel{});
			data += width;
		} else {
			if (pad_left > 0) {
				/* Pad left. */
				std::fill_n(data, pad_left, SpriteLoader::CommonPixel{});
				data += pad_left;
			}

			/* Copy pixels. */
			std::copy_n(src, sprite->width, data);
			src += sprite->width;
			data += sprite->width;

			if (pad_right > 0) {
				/* Pad right. */
				std::fill_n(data, pad_right, SpriteLoader::CommonPixel{});
				data += pad_right;
			}
		}
	}

	/* Update sprite size. */
	sprite->width   = width;
	sprite->height  = height;
	sprite->x_offs -= pad_left;
	sprite->y_offs -= pad_top;

	return true;
}

static bool PadSprites(SpriteLoader::SpriteCollection &sprite, ZoomLevels sprite_avail, SpriteEncoder *encoder)
{
	/* Get minimum top left corner coordinates. */
	int min_xoffs = INT32_MAX;
	int min_yoffs = INT32_MAX;
	for (ZoomLevel zoom : sprite_avail) {
		min_xoffs = std::min(min_xoffs, ScaleByZoom(sprite[zoom].x_offs, zoom));
		min_yoffs = std::min(min_yoffs, ScaleByZoom(sprite[zoom].y_offs, zoom));
	}

	/* Get maximum dimensions taking necessary padding at the top left into account. */
	int max_width  = INT32_MIN;
	int max_height = INT32_MIN;
	for (ZoomLevel zoom : sprite_avail) {
		max_width  = std::max(max_width, ScaleByZoom(sprite[zoom].width + sprite[zoom].x_offs - UnScaleByZoom(min_xoffs, zoom), zoom));
		max_height = std::max(max_height, ScaleByZoom(sprite[zoom].height + sprite[zoom].y_offs - UnScaleByZoom(min_yoffs, zoom), zoom));
	}

	/* Align height and width if required to match the needs of the sprite encoder. */
	uint align = encoder->GetSpriteAlignment();
	if (align != 0) {
		max_width  = Align(max_width,  align);
		max_height = Align(max_height, align);
	}

	/* Pad sprites where needed. */
	for (ZoomLevel zoom : sprite_avail) {
		auto &cur_sprite = sprite[zoom];
		/* Scaling the sprite dimensions in the blitter is done with rounding up,
		 * so a negative padding here is not an error. */
		int pad_left = std::max(0, cur_sprite.x_offs - UnScaleByZoom(min_xoffs, zoom));
		int pad_top = std::max(0, cur_sprite.y_offs - UnScaleByZoom(min_yoffs, zoom));
		int pad_right = std::max(0, UnScaleByZoom(max_width, zoom) - cur_sprite.width - pad_left);
		int pad_bottom = std::max(0, UnScaleByZoom(max_height, zoom) - cur_sprite.height - pad_top);

		if (pad_left > 0 || pad_right > 0 || pad_top > 0 || pad_bottom > 0) {
			if (!PadSingleSprite(&cur_sprite, zoom, pad_left, pad_top, pad_right, pad_bottom)) return false;
		}
	}

	return true;
}

static bool ResizeSprites(SpriteLoader::SpriteCollection &sprite, ZoomLevels sprite_avail, SpriteEncoder *encoder)
{
	/* Create a fully zoomed image if it does not exist */
	ZoomLevel first_avail = ZoomLevel::End;
	for (ZoomLevel zoom = ZoomLevel::Min; zoom <= ZoomLevel::Max; ++zoom) {
		if (!sprite_avail.Test(zoom)) continue;
		first_avail = zoom;
		if (zoom != ZoomLevel::Min) {
			if (!ResizeSpriteIn(sprite, zoom, ZoomLevel::Min)) return false;
			sprite_avail.Set(ZoomLevel::Min);
		}
		break;
	}

	/* Pad sprites to make sizes match. */
	if (!PadSprites(sprite, sprite_avail, encoder)) return false;

	/* Create other missing zoom levels */
	for (ZoomLevel zoom = ZoomLevel::Begin; zoom != ZoomLevel::End; zoom++) {
		if (zoom == ZoomLevel::Min) continue;

		if (sprite_avail.Test(zoom)) {
			/* Check that size and offsets match the fully zoomed image. */
			[[maybe_unused]] const auto &root_sprite = sprite[ZoomLevel::Min];
			[[maybe_unused]] const auto &dest_sprite = sprite[zoom];
			assert(dest_sprite.width == UnScaleByZoom(root_sprite.width, zoom));
			assert(dest_sprite.height == UnScaleByZoom(root_sprite.height, zoom));
			assert(dest_sprite.x_offs == UnScaleByZoom(root_sprite.x_offs, zoom));
			assert(dest_sprite.y_offs == UnScaleByZoom(root_sprite.y_offs, zoom));
		} else {
			/* Zoom level is not available, or unusable, so create it */
			ResizeSpriteOut(sprite, zoom);
		}
	}

	/* Replace sprites with higher resolution than the desired maximum source resolution with scaled up sprites, if not already done. */
	if (first_avail < _settings_client.gui.sprite_zoom_min) {
		for (ZoomLevel zoom = std::min(ZoomLevel::Normal, _settings_client.gui.sprite_zoom_min); zoom > ZoomLevel::Min; --zoom) {
			ResizeSpriteIn(sprite, zoom, zoom - 1);
		}
	}

	return  true;
}

/**
 * Load a recolour sprite into memory.
 * @param file GRF we're reading from.
 * @param file_pos Position within file.
 * @param num Size of the sprite in the GRF.
 * @param allocator Sprite allocator to use.
 * @return Sprite data.
 */
static void *ReadRecolourSprite(SpriteFile &file, size_t file_pos, uint num, SpriteAllocator &allocator)
{
	/* "Normal" recolour sprites are ALWAYS 257 bytes. Then there is a small
	 * number of recolour sprites that are 17 bytes that only exist in DOS
	 * GRFs which are the same as 257 byte recolour sprites, but with the last
	 * 240 bytes zeroed.  */
	static const uint RECOLOUR_SPRITE_SIZE = 257;
	uint8_t *dest = allocator.Allocate<uint8_t>(std::max(RECOLOUR_SPRITE_SIZE, num));

	file.SeekTo(file_pos, SEEK_SET);
	if (file.NeedsPaletteRemap()) {
		uint8_t *dest_tmp = new uint8_t[std::max(RECOLOUR_SPRITE_SIZE, num)];

		/* Only a few recolour sprites are less than 257 bytes */
		if (num < RECOLOUR_SPRITE_SIZE) std::fill_n(dest_tmp, RECOLOUR_SPRITE_SIZE, 0);
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
	ZoomLevels sprite_avail;
	ZoomLevels avail_8bpp;
	ZoomLevels avail_32bpp;

	SpriteLoaderGrf sprite_loader(file.GetContainerVersion());
	if (sprite_type != SpriteType::MapGen && encoder->Is32BppSupported()) {
		/* Try for 32bpp sprites first. */
		sprite_avail = sprite_loader.LoadSprite(sprite, file, file_pos, sprite_type, true, sc->control_flags, avail_8bpp, avail_32bpp);
	}
	if (sprite_avail.None()) {
		sprite_avail = sprite_loader.LoadSprite(sprite, file, file_pos, sprite_type, false, sc->control_flags, avail_8bpp, avail_32bpp);
		if (sprite_type == SpriteType::Normal && avail_32bpp.Any() && !encoder->Is32BppSupported() && sprite_avail.None()) {
			/* No 8bpp available, try converting from 32bpp. */
			SpriteLoaderMakeIndexed make_indexed(sprite_loader);
			sprite_avail = make_indexed.LoadSprite(sprite, file, file_pos, sprite_type, true, sc->control_flags, sprite_avail, avail_32bpp);
		}
	}

	if (sprite_avail.None()) {
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
		const auto &root_sprite = sprite.Root();
		uint num = root_sprite.width * root_sprite.height;

		Sprite *s = allocator.Allocate<Sprite>(sizeof(*s) + num);
		s->width = root_sprite.width;
		s->height = root_sprite.height;
		s->x_offs = root_sprite.x_offs;
		s->y_offs = root_sprite.y_offs;

		SpriteLoader::CommonPixel *src = root_sprite.data;
		uint8_t *dest = reinterpret_cast<uint8_t *>(s->data);
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

	if (sprite_type == SpriteType::Font && _font_zoom != ZoomLevel::Min) {
		/* Make ZoomLevel::Min the desired font zoom level. */
		sprite[ZoomLevel::Min] = sprite[_font_zoom];
	}

	return encoder->Encode(sprite_type, sprite, allocator);
}

struct GrfSpriteOffset {
	size_t file_pos;
	SpriteCacheCtrlFlags control_flags{};
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

		GrfSpriteOffset offset{0};

		/* Loop over all sprite section entries and store the file
		 * offset for each newly encountered ID. */
		SpriteID id, prev_id = 0;
		while ((id = file.ReadDword()) != 0) {
			if (id != prev_id) {
				_grf_sprite_offsets[prev_id] = offset;
				offset.file_pos = file.GetPos() - 4;
			}
			prev_id = id;
			uint length = file.ReadDword();
			if (length > 0) {
				SpriteComponents colour{file.ReadByte()};
				length--;
				if (length > 0) {
					uint8_t zoom = file.ReadByte();
					length--;
					if (colour.Any() && zoom == 0) { // ZoomLevel::Normal (normal zoom)
						offset.control_flags.Set((colour != SpriteComponent::Palette) ? SpriteCacheCtrlFlag::AllowZoomMin1x32bpp : SpriteCacheCtrlFlag::AllowZoomMin1xPal);
						offset.control_flags.Set((colour != SpriteComponent::Palette) ? SpriteCacheCtrlFlag::AllowZoomMin2x32bpp : SpriteCacheCtrlFlag::AllowZoomMin2xPal);
					}
					if (colour.Any() && zoom == 2) { // ZoomLevel::In2x (2x zoomed in)
						offset.control_flags.Set((colour != SpriteComponent::Palette) ? SpriteCacheCtrlFlag::AllowZoomMin2x32bpp : SpriteCacheCtrlFlag::AllowZoomMin2xPal);
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
bool LoadNextSprite(SpriteID load_index, SpriteFile &file, uint file_sprite_id)
{
	size_t file_pos = file.GetPos();

	/* Read sprite header. */
	uint32_t num = file.GetContainerVersion() >= 2 ? file.ReadDword() : file.ReadWord();
	if (num == 0) return false;
	uint8_t grf_type = file.ReadByte();

	SpriteType type;
	SpriteCacheCtrlFlags control_flags;
	if (grf_type == 0xFF) {
		/* Some NewGRF files have "empty" pseudo-sprites which are 1
		 * byte long. Catch these so the sprites won't be displayed. */
		if (num == 1) {
			file.ReadByte();
			return false;
		}
		file_pos = file.GetPos();
		type = SpriteType::Recolour;
		file.SkipBytes(num);
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
	sc->length = num;
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
		if (sc->ptr != nullptr) {
			push({ sc->lru, i, sc->length });
			if (candidate_bytes >= to_remove) break;
		}
	}
	/* candidates now contains enough bytes to meet to_remove.
	 * only sprites with LRU values <= the maximum (i.e. the top of the heap) need to be considered */
	for (; i != static_cast<SpriteID>(_spritecache.size()); i++) {
		const SpriteCache *sc = GetSpriteCache(i);
		if (sc->ptr != nullptr && sc->lru <= candidates.front().lru) {
			push({ sc->lru, i, sc->length });
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
			if (sc.ptr != nullptr) {
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
	_spritecache_bytes_used -= this->length;
	this->ptr.reset();
}

void *UniquePtrSpriteAllocator::AllocatePtr(size_t size)
{
	this->data = std::make_unique<std::byte[]>(size);
	this->size = size;
	return this->data.get();
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
	static const std::string_view sprite_types[] = {
		"normal",        // SpriteType::Normal
		"map generator", // SpriteType::MapGen
		"character",     // SpriteType::Font
		"recolour",      // SpriteType::Recolour
	};

	SpriteType available = sc->type;
	if (requested == SpriteType::Font && available == SpriteType::Normal) {
		if (sc->ptr == nullptr) sc->type = SpriteType::Font;
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
		if (sc->ptr == nullptr) {
			UniquePtrSpriteAllocator cache_allocator;
			if (sc->type == SpriteType::Recolour) {
				ReadRecolourSprite(*sc->file, sc->file_pos, sc->length, cache_allocator);
			} else {
				ReadSprite(sc, sprite, type, cache_allocator, nullptr);
			}
			sc->ptr = std::move(cache_allocator.data);
			sc->length = static_cast<uint32_t>(cache_allocator.size);
			_spritecache_bytes_used += sc->length;
		}

		return static_cast<void *>(sc->ptr.get());
	} else {
		/* Do not use the spritecache, but a different allocator. */
		return ReadSprite(sc, sprite, type, *allocator, encoder);
	}
}

void GfxInitSpriteMem()
{
	/* Reset the spritecache 'pool' */
	_spritecache.clear();
	_spritecache.shrink_to_fit();

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
		if (sc.ptr != nullptr) sc.ClearSpriteData();
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
		if (sc.type == SpriteType::Font && sc.ptr != nullptr) sc.ClearSpriteData();
	}
}

/* static */ SpriteCollMap<ReusableBuffer<SpriteLoader::CommonPixel>> SpriteLoader::Sprite::buffer;
