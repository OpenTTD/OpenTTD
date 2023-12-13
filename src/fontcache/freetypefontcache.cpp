/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file freetypefontcache.cpp FreeType font cache implementation. */

#include "../stdafx.h"
#include "../debug.h"
#include "../fontcache.h"
#include "../fontdetection.h"
#include "../blitter/factory.hpp"
#include "../core/math_func.hpp"
#include "../zoom_func.h"
#include "../fileio_func.h"
#include "../error_func.h"
#include "truetypefontcache.h"

#include "../table/control_codes.h"

#include "../safeguards.h"

#ifdef WITH_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_TRUETYPE_TABLES_H

/** Font cache for fonts that are based on a freetype font. */
class FreeTypeFontCache : public TrueTypeFontCache {
private:
	FT_Face face;  ///< The font face associated with this font.

	void SetFontSize(FontSize fs, FT_Face face, int pixels);
	const void *InternalGetFontTable(uint32_t tag, size_t &length) override;
	const Sprite *InternalGetGlyph(GlyphID key, bool aa) override;

public:
	FreeTypeFontCache(FontSize fs, FT_Face face, int pixels);
	~FreeTypeFontCache();
	void ClearFontCache() override;
	GlyphID MapCharToGlyph(char32_t key) override;
	std::string GetFontName() override { return fmt::format("{}, {}", face->family_name, face->style_name); }
	bool IsBuiltInFont() override { return false; }
	const void *GetOSHandle() override { return &face; }
};

FT_Library _library = nullptr;


/**
 * Create a new FreeTypeFontCache.
 * @param fs     The font size that is going to be cached.
 * @param face   The font that has to be loaded.
 * @param pixels The number of pixels this font should be high.
 */
FreeTypeFontCache::FreeTypeFontCache(FontSize fs, FT_Face face, int pixels) : TrueTypeFontCache(fs, pixels), face(face)
{
	assert(face != nullptr);

	this->SetFontSize(fs, face, pixels);
}

void FreeTypeFontCache::SetFontSize(FontSize, FT_Face, int pixels)
{
	if (pixels == 0) {
		/* Try to determine a good height based on the minimal height recommended by the font. */
		int scaled_height = ScaleGUITrad(FontCache::GetDefaultFontHeight(this->fs));
		pixels = scaled_height;

		TT_Header *head = (TT_Header *)FT_Get_Sfnt_Table(this->face, ft_sfnt_head);
		if (head != nullptr) {
			/* Font height is minimum height plus the difference between the default
			 * height for this font size and the small size. */
			int diff = scaled_height - ScaleGUITrad(FontCache::GetDefaultFontHeight(FS_SMALL));
			/* Clamp() is not used as scaled_height could be greater than MAX_FONT_SIZE, which is not permitted in Clamp(). */
			pixels = std::min(std::max(std::min<int>(head->Lowest_Rec_PPEM, MAX_FONT_MIN_REC_SIZE) + diff, scaled_height), MAX_FONT_SIZE);
		}
	} else {
		pixels = ScaleGUITrad(pixels);
	}
	this->used_size = pixels;

	FT_Error err = FT_Set_Pixel_Sizes(this->face, 0, pixels);
	if (err != FT_Err_Ok) {

		/* Find nearest size to that requested */
		FT_Bitmap_Size *bs = this->face->available_sizes;
		int i = this->face->num_fixed_sizes;
		if (i > 0) { // In pathetic cases one might get no fixed sizes at all.
			int n = bs->height;
			FT_Int chosen = 0;
			for (; --i; bs++) {
				if (abs(pixels - bs->height) >= abs(pixels - n)) continue;
				n = bs->height;
				chosen = this->face->num_fixed_sizes - i;
			}

			/* Don't use FT_Set_Pixel_Sizes here - it might give us another
			 * error, even though the size is available (FS#5885). */
			err = FT_Select_Size(this->face, chosen);
		}
	}

	if (err == FT_Err_Ok) {
		this->units_per_em = this->face->units_per_EM;
		this->ascender     = this->face->size->metrics.ascender >> 6;
		this->descender    = this->face->size->metrics.descender >> 6;
		this->height       = this->ascender - this->descender;
	} else {
		/* Both FT_Set_Pixel_Sizes and FT_Select_Size failed. */
		Debug(fontcache, 0, "Font size selection failed. Using FontCache defaults.");
	}
}

/**
 * Loads the freetype font.
 * First type to load the fontname as if it were a path. If that fails,
 * try to resolve the filename of the font using fontconfig, where the
 * format is 'font family name' or 'font family name, font style'.
 * @param fs The font size to load.
 */
void LoadFreeTypeFont(FontSize fs)
{
	FontCacheSubSetting *settings = GetFontCacheSubSetting(fs);

	if (settings->font.empty()) return;

	if (_library == nullptr) {
		if (FT_Init_FreeType(&_library) != FT_Err_Ok) {
			ShowInfo("Unable to initialize FreeType, using sprite fonts instead");
			return;
		}

		Debug(fontcache, 2, "Initialized");
	}

	const char *font_name = settings->font.c_str();
	FT_Face face = nullptr;

	/* If font is an absolute path to a ttf, try loading that first. */
	int32_t index = 0;
	if (settings->os_handle != nullptr) index = *static_cast<const int32_t *>(settings->os_handle);
	FT_Error error = FT_New_Face(_library, font_name, index, &face);

	if (error != FT_Err_Ok) {
		/* Check if font is a relative filename in one of our search-paths. */
		std::string full_font = FioFindFullPath(BASE_DIR, font_name);
		if (!full_font.empty()) {
			error = FT_New_Face(_library, full_font.c_str(), 0, &face);
		}
	}

	/* Try loading based on font face name (OS-wide fonts). */
	if (error != FT_Err_Ok) error = GetFontByFaceName(font_name, &face);

	if (error == FT_Err_Ok) {
		Debug(fontcache, 2, "Requested '{}', using '{} {}'", font_name, face->family_name, face->style_name);

		/* Attempt to select the unicode character map */
		error = FT_Select_Charmap(face, ft_encoding_unicode);
		if (error == FT_Err_Ok) goto found_face; // Success

		if (error == FT_Err_Invalid_CharMap_Handle) {
			/* Try to pick a different character map instead. We default to
			 * the first map, but platform_id 0 encoding_id 0 should also
			 * be unicode (strange system...) */
			FT_CharMap found = face->charmaps[0];
			int i;

			for (i = 0; i < face->num_charmaps; i++) {
				FT_CharMap charmap = face->charmaps[i];
				if (charmap->platform_id == 0 && charmap->encoding_id == 0) {
					found = charmap;
				}
			}

			if (found != nullptr) {
				error = FT_Set_Charmap(face, found);
				if (error == FT_Err_Ok) goto found_face;
			}
		}
	}

	FT_Done_Face(face);

	ShowInfo("Unable to use '{}' for {} font, FreeType reported error 0x{:X}, using sprite font instead", font_name, FontSizeToName(fs), error);
	return;

found_face:
	new FreeTypeFontCache(fs, face, settings->size);
}


/**
 * Free everything that was allocated for this font cache.
 */
FreeTypeFontCache::~FreeTypeFontCache()
{
	FT_Done_Face(this->face);
	this->face = nullptr;
	this->ClearFontCache();
}

/**
 * Reset cached glyphs.
 */
void FreeTypeFontCache::ClearFontCache()
{
	/* Font scaling might have changed, determine font size anew if it was automatically selected. */
	if (this->face != nullptr) this->SetFontSize(this->fs, this->face, this->req_size);

	this->TrueTypeFontCache::ClearFontCache();
}


const Sprite *FreeTypeFontCache::InternalGetGlyph(GlyphID key, bool aa)
{
	FT_GlyphSlot slot = this->face->glyph;

	FT_Load_Glyph(this->face, key, aa ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO);
	FT_Render_Glyph(this->face->glyph, aa ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO);

	/* Despite requesting a normal glyph, FreeType may have returned a bitmap */
	aa = (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);

	/* Add 1 scaled pixel for the shadow on the medium font. Our sprite must be at least 1x1 pixel */
	uint shadow = (this->fs == FS_NORMAL) ? ScaleGUITrad(1) : 0;
	uint width  = std::max(1U, (uint)slot->bitmap.width + shadow);
	uint height = std::max(1U, (uint)slot->bitmap.rows  + shadow);

	/* Limit glyph size to prevent overflows later on. */
	if (width > MAX_GLYPH_DIM || height > MAX_GLYPH_DIM) UserError("Font glyph is too large");

	/* FreeType has rendered the glyph, now we allocate a sprite and copy the image into it */
	SpriteLoader::SpriteCollection spritecollection;
	SpriteLoader::Sprite &sprite = spritecollection[ZOOM_LVL_NORMAL];
	sprite.AllocateData(ZOOM_LVL_NORMAL, static_cast<size_t>(width) * height);
	sprite.type = SpriteType::Font;
	sprite.colours = (aa ? SCC_PAL | SCC_ALPHA : SCC_PAL);
	sprite.width = width;
	sprite.height = height;
	sprite.x_offs = slot->bitmap_left;
	sprite.y_offs = this->ascender - slot->bitmap_top;

	/* Draw shadow for medium size */
	if (this->fs == FS_NORMAL && !aa) {
		for (uint y = 0; y < (uint)slot->bitmap.rows; y++) {
			for (uint x = 0; x < (uint)slot->bitmap.width; x++) {
				if (HasBit(slot->bitmap.buffer[(x / 8) + y * slot->bitmap.pitch], 7 - (x % 8))) {
					sprite.data[shadow + x + (shadow + y) * sprite.width].m = SHADOW_COLOUR;
					sprite.data[shadow + x + (shadow + y) * sprite.width].a = 0xFF;
				}
			}
		}
	}

	for (uint y = 0; y < (uint)slot->bitmap.rows; y++) {
		for (uint x = 0; x < (uint)slot->bitmap.width; x++) {
			if (aa ? (slot->bitmap.buffer[x + y * slot->bitmap.pitch] > 0) : HasBit(slot->bitmap.buffer[(x / 8) + y * slot->bitmap.pitch], 7 - (x % 8))) {
				sprite.data[x + y * sprite.width].m = FACE_COLOUR;
				sprite.data[x + y * sprite.width].a = aa ? slot->bitmap.buffer[x + y * slot->bitmap.pitch] : 0xFF;
			}
		}
	}

	GlyphEntry new_glyph;
	new_glyph.sprite = BlitterFactory::GetCurrentBlitter()->Encode(spritecollection, SimpleSpriteAlloc);
	new_glyph.width  = slot->advance.x >> 6;

	this->SetGlyphPtr(key, &new_glyph);

	return new_glyph.sprite;
}


GlyphID FreeTypeFontCache::MapCharToGlyph(char32_t key)
{
	assert(IsPrintable(key));

	if (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END) {
		return this->parent->MapCharToGlyph(key);
	}

	return FT_Get_Char_Index(this->face, key);
}

const void *FreeTypeFontCache::InternalGetFontTable(uint32_t tag, size_t &length)
{
	FT_ULong len = 0;
	FT_Byte *result = nullptr;

	FT_Load_Sfnt_Table(this->face, tag, 0, nullptr, &len);

	if (len > 0) {
		result = MallocT<FT_Byte>(len);
		FT_Load_Sfnt_Table(this->face, tag, 0, result, &len);
	}

	length = len;
	return result;
}

/**
 * Free everything allocated w.r.t. freetype.
 */
void UninitFreeType()
{
	FT_Done_FreeType(_library);
	_library = nullptr;
}

#if !defined(WITH_FONTCONFIG)

FT_Error GetFontByFaceName(const char *font_name, FT_Face *face) { return FT_Err_Cannot_Open_Resource; }

#endif /* !defined(WITH_FONTCONFIG) */

#endif /* WITH_FREETYPE */
