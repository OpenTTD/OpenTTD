/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file font_win32.cpp Functions related to font handling on Win32. */

#include "../../stdafx.h"
#include "../../debug.h"
#include "../../blitter/factory.hpp"
#include "../../core/alloc_func.hpp"
#include "../../core/math_func.hpp"
#include "../../core/mem_func.hpp"
#include "../../error_func.h"
#include "../../fileio_func.h"
#include "../../fontdetection.h"
#include "../../fontcache.h"
#include "../../fontcache/truetypefontcache.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../zoom_func.h"
#include "font_win32.h"

#include "../../table/control_codes.h"

#include <windows.h>
#include <shlobj.h> /* SHGetFolderPath */
#include "os/windows/win32.h"
#undef small // Say what, Windows?

#include "safeguards.h"

struct EFCParam {
	FontCacheSettings *settings;
	LOCALESIGNATURE  locale;
	MissingGlyphSearcher *callback;
	std::vector<std::wstring> fonts;

	bool Add(const std::wstring_view &font)
	{
		for (const auto &entry : this->fonts) {
			if (font.compare(entry) == 0) return false;
		}

		this->fonts.emplace_back(font);

		return true;
	}
};

static int CALLBACK EnumFontCallback(const ENUMLOGFONTEX *logfont, const NEWTEXTMETRICEX *metric, DWORD type, LPARAM lParam)
{
	EFCParam *info = (EFCParam *)lParam;

	/* Skip duplicates */
	if (!info->Add(logfont->elfFullName)) return 1;
	/* Only use TrueType fonts */
	if (!(type & TRUETYPE_FONTTYPE)) return 1;
	/* Don't use SYMBOL fonts */
	if (logfont->elfLogFont.lfCharSet == SYMBOL_CHARSET) return 1;
	/* Use monospaced fonts when asked for it. */
	if (info->callback->Monospace() && (logfont->elfLogFont.lfPitchAndFamily & (FF_MODERN | FIXED_PITCH)) != (FF_MODERN | FIXED_PITCH)) return 1;

	/* The font has to have at least one of the supported locales to be usable. */
	if ((metric->ntmFontSig.fsCsb[0] & info->locale.lsCsbSupported[0]) == 0 && (metric->ntmFontSig.fsCsb[1] & info->locale.lsCsbSupported[1]) == 0) {
		/* On win9x metric->ntmFontSig seems to contain garbage. */
		FONTSIGNATURE fs;
		memset(&fs, 0, sizeof(fs));
		HFONT font = CreateFontIndirect(&logfont->elfLogFont);
		if (font != nullptr) {
			HDC dc = GetDC(nullptr);
			HGDIOBJ oldfont = SelectObject(dc, font);
			GetTextCharsetInfo(dc, &fs, 0);
			SelectObject(dc, oldfont);
			ReleaseDC(nullptr, dc);
			DeleteObject(font);
		}
		if ((fs.fsCsb[0] & info->locale.lsCsbSupported[0]) == 0 && (fs.fsCsb[1] & info->locale.lsCsbSupported[1]) == 0) return 1;
	}

	char font_name[MAX_PATH];
	convert_from_fs((const wchar_t *)logfont->elfFullName, font_name, lengthof(font_name));

	info->callback->SetFontNames(info->settings, font_name, &logfont->elfLogFont);
	if (info->callback->FindMissingGlyphs()) return 1;
	Debug(fontcache, 1, "Fallback font: {}", font_name);
	return 0; // stop enumerating
}

bool SetFallbackFont(FontCacheSettings *settings, const std::string &, int winlangid, MissingGlyphSearcher *callback)
{
	Debug(fontcache, 1, "Trying fallback fonts");
	EFCParam langInfo;
	if (GetLocaleInfo(MAKELCID(winlangid, SORT_DEFAULT), LOCALE_FONTSIGNATURE, (LPTSTR)&langInfo.locale, sizeof(langInfo.locale) / sizeof(wchar_t)) == 0) {
		/* Invalid langid or some other mysterious error, can't determine fallback font. */
		Debug(fontcache, 1, "Can't get locale info for fallback font (langid=0x{:x})", winlangid);
		return false;
	}
	langInfo.settings = settings;
	langInfo.callback = callback;

	LOGFONT font;
	/* Enumerate all fonts. */
	font.lfCharSet = DEFAULT_CHARSET;
	font.lfFaceName[0] = '\0';
	font.lfPitchAndFamily = 0;

	HDC dc = GetDC(nullptr);
	int ret = EnumFontFamiliesEx(dc, &font, (FONTENUMPROC)&EnumFontCallback, (LPARAM)&langInfo, 0);
	ReleaseDC(nullptr, dc);
	return ret == 0;
}


#ifndef ANTIALIASED_QUALITY
#define ANTIALIASED_QUALITY     4
#endif

/**
 * Create a new Win32FontCache.
 * @param fs      The font size that is going to be cached.
 * @param logfont The font that has to be loaded.
 * @param pixels  The number of pixels this font should be high.
 */
Win32FontCache::Win32FontCache(FontSize fs, const LOGFONT &logfont, int pixels) : TrueTypeFontCache(fs, pixels), logfont(logfont)
{
	this->dc = CreateCompatibleDC(nullptr);
	this->SetFontSize(pixels);
}

Win32FontCache::~Win32FontCache()
{
	this->ClearFontCache();
	DeleteDC(this->dc);
	DeleteObject(this->font);
}

void Win32FontCache::SetFontSize(int pixels)
{
	if (pixels == 0) {
		/* Try to determine a good height based on the minimal height recommended by the font. */
		int scaled_height = ScaleGUITrad(FontCache::GetDefaultFontHeight(this->fs));
		pixels = scaled_height;

		HFONT temp = CreateFontIndirect(&this->logfont);
		if (temp != nullptr) {
			HGDIOBJ old = SelectObject(this->dc, temp);

			UINT size = GetOutlineTextMetrics(this->dc, 0, nullptr);
			LPOUTLINETEXTMETRIC otm = (LPOUTLINETEXTMETRIC)new BYTE[size];
			GetOutlineTextMetrics(this->dc, size, otm);

			/* Font height is minimum height plus the difference between the default
			 * height for this font size and the small size. */
			int diff = scaled_height - ScaleGUITrad(FontCache::GetDefaultFontHeight(FS_SMALL));
			/* Clamp() is not used as scaled_height could be greater than MAX_FONT_SIZE, which is not permitted in Clamp(). */
			pixels = std::min(std::max(std::min<int>(otm->otmusMinimumPPEM, MAX_FONT_MIN_REC_SIZE) + diff, scaled_height), MAX_FONT_SIZE);

			delete[] (BYTE*)otm;
			SelectObject(dc, old);
			DeleteObject(temp);
		}
	} else {
		pixels = ScaleGUITrad(pixels);
	}
	this->used_size = pixels;

	/* Create GDI font handle. */
	this->logfont.lfHeight = -pixels;
	this->logfont.lfWidth = 0;
	this->logfont.lfOutPrecision = ANTIALIASED_QUALITY;

	if (this->font != nullptr) {
		SelectObject(dc, this->old_font);
		DeleteObject(this->font);
	}
	this->font = CreateFontIndirect(&this->logfont);
	this->old_font = SelectObject(this->dc, this->font);

	/* Query the font metrics we needed. */
	UINT otmSize = GetOutlineTextMetrics(this->dc, 0, nullptr);
	POUTLINETEXTMETRIC otm = (POUTLINETEXTMETRIC)new BYTE[otmSize];
	GetOutlineTextMetrics(this->dc, otmSize, otm);

	this->units_per_em = otm->otmEMSquare;
	this->ascender = otm->otmTextMetrics.tmAscent;
	this->descender = otm->otmTextMetrics.tmDescent;
	this->height = this->ascender + this->descender;
	this->glyph_size.cx = otm->otmTextMetrics.tmMaxCharWidth;
	this->glyph_size.cy = otm->otmTextMetrics.tmHeight;

	this->fontname = FS2OTTD((LPWSTR)((BYTE *)otm + (ptrdiff_t)otm->otmpFaceName));

	Debug(fontcache, 2, "Loaded font '{}' with size {}", this->fontname, pixels);
	delete[] (BYTE*)otm;
}

/**
 * Reset cached glyphs.
 */
void Win32FontCache::ClearFontCache()
{
	/* GUI scaling might have changed, determine font size anew if it was automatically selected. */
	if (this->font != nullptr) this->SetFontSize(this->req_size);

	this->TrueTypeFontCache::ClearFontCache();
}

/* virtual */ const Sprite *Win32FontCache::InternalGetGlyph(GlyphID key, bool aa)
{
	GLYPHMETRICS gm;
	MAT2 mat = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };

	/* Call GetGlyphOutline with zero size initially to get required memory size. */
	DWORD size = GetGlyphOutline(this->dc, key, GGO_GLYPH_INDEX | (aa ? GGO_GRAY8_BITMAP : GGO_BITMAP), &gm, 0, nullptr, &mat);
	if (size == GDI_ERROR) UserError("Unable to render font glyph");

	/* Add 1 scaled pixel for the shadow on the medium font. Our sprite must be at least 1x1 pixel. */
	uint shadow = (this->fs == FS_NORMAL) ? ScaleGUITrad(1) : 0;
	uint width = std::max(1U, (uint)gm.gmBlackBoxX + shadow);
	uint height = std::max(1U, (uint)gm.gmBlackBoxY + shadow);

	/* Limit glyph size to prevent overflows later on. */
	if (width > MAX_GLYPH_DIM || height > MAX_GLYPH_DIM) UserError("Font glyph is too large");

	/* Call GetGlyphOutline again with size to actually render the glyph. */
	byte *bmp = new byte[size];
	GetGlyphOutline(this->dc, key, GGO_GLYPH_INDEX | (aa ? GGO_GRAY8_BITMAP : GGO_BITMAP), &gm, size, bmp, &mat);

	/* GDI has rendered the glyph, now we allocate a sprite and copy the image into it. */
	SpriteLoader::SpriteCollection spritecollection;
	SpriteLoader::Sprite &sprite = spritecollection[ZOOM_LVL_NORMAL];
	sprite.AllocateData(ZOOM_LVL_NORMAL, width * height);
	sprite.type = SpriteType::Font;
	sprite.colours = (aa ? SCC_PAL | SCC_ALPHA : SCC_PAL);
	sprite.width = width;
	sprite.height = height;
	sprite.x_offs = gm.gmptGlyphOrigin.x;
	sprite.y_offs = this->ascender - gm.gmptGlyphOrigin.y;

	if (size > 0) {
		/* All pixel data returned by GDI is in the form of DWORD-aligned rows.
		 * For a non anti-aliased glyph, the returned bitmap has one bit per pixel.
		 * For anti-aliased rendering, GDI uses the strange value range of 0 to 64,
		 * inclusively. To map this to 0 to 255, we shift left by two and then
		 * subtract one. */
		uint pitch = Align(aa ? gm.gmBlackBoxX : std::max((gm.gmBlackBoxX + 7u) / 8u, 1u), 4);

		/* Draw shadow for medium size. */
		if (this->fs == FS_NORMAL && !aa) {
			for (uint y = 0; y < gm.gmBlackBoxY; y++) {
				for (uint x = 0; x < gm.gmBlackBoxX; x++) {
					if (aa ? (bmp[x + y * pitch] > 0) : HasBit(bmp[(x / 8) + y * pitch], 7 - (x % 8))) {
						sprite.data[shadow + x + (shadow + y) * sprite.width].m = SHADOW_COLOUR;
						sprite.data[shadow + x + (shadow + y) * sprite.width].a = aa ? (bmp[x + y * pitch] << 2) - 1 : 0xFF;
					}
				}
			}
		}

		for (uint y = 0; y < gm.gmBlackBoxY; y++) {
			for (uint x = 0; x < gm.gmBlackBoxX; x++) {
				if (aa ? (bmp[x + y * pitch] > 0) : HasBit(bmp[(x / 8) + y * pitch], 7 - (x % 8))) {
					sprite.data[x + y * sprite.width].m = FACE_COLOUR;
					sprite.data[x + y * sprite.width].a = aa ? (bmp[x + y * pitch] << 2) - 1 : 0xFF;
				}
			}
		}
	}

	GlyphEntry new_glyph;
	new_glyph.sprite = BlitterFactory::GetCurrentBlitter()->Encode(spritecollection, SimpleSpriteAlloc);
	new_glyph.width = gm.gmCellIncX;

	this->SetGlyphPtr(key, &new_glyph);

	delete[] bmp;

	return new_glyph.sprite;
}

/* virtual */ GlyphID Win32FontCache::MapCharToGlyph(char32_t key)
{
	assert(IsPrintable(key));

	if (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END) {
		return this->parent->MapCharToGlyph(key);
	}

	/* Convert characters outside of the BMP into surrogate pairs. */
	WCHAR chars[2];
	if (key >= 0x010000U) {
		chars[0] = (wchar_t)(((key - 0x010000U) >> 10) + 0xD800);
		chars[1] = (wchar_t)(((key - 0x010000U) & 0x3FF) + 0xDC00);
	} else {
		chars[0] = (wchar_t)(key & 0xFFFF);
	}

	WORD glyphs[2] = { 0, 0 };
	GetGlyphIndicesW(this->dc, chars, key >= 0x010000U ? 2 : 1, glyphs, GGI_MARK_NONEXISTING_GLYPHS);

	return glyphs[0] != 0xFFFF ? glyphs[0] : 0;
}

/* virtual */ const void *Win32FontCache::InternalGetFontTable(uint32_t tag, size_t &length)
{
	DWORD len = GetFontData(this->dc, tag, 0, nullptr, 0);

	void *result = nullptr;
	if (len != GDI_ERROR && len > 0) {
		result = MallocT<BYTE>(len);
		GetFontData(this->dc, tag, 0, result, len);
	}

	length = len;
	return result;
}


/**
 * Loads the GDI font.
 * If a GDI font description is present, e.g. from the automatic font
 * fallback search, use it. Otherwise, try to resolve it by font name.
 * @param fs The font size to load.
 */
void LoadWin32Font(FontSize fs)
{
	FontCacheSubSetting *settings = GetFontCacheSubSetting(fs);

	if (settings->font.empty()) return;

	const char *font_name = settings->font.c_str();
	LOGFONT logfont;
	MemSetT(&logfont, 0);
	logfont.lfPitchAndFamily = fs == FS_MONO ? FIXED_PITCH : VARIABLE_PITCH;
	logfont.lfCharSet = DEFAULT_CHARSET;
	logfont.lfOutPrecision = OUT_OUTLINE_PRECIS;
	logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;

	if (settings->os_handle != nullptr) {
		logfont = *(const LOGFONT *)settings->os_handle;
	} else if (strchr(font_name, '.') != nullptr) {
		/* Might be a font file name, try load it. */

		wchar_t fontPath[MAX_PATH] = {};

		/* See if this is an absolute path. */
		if (FileExists(settings->font)) {
			convert_to_fs(font_name, fontPath, lengthof(fontPath));
		} else {
			/* Scan the search-paths to see if it can be found. */
			std::string full_font = FioFindFullPath(BASE_DIR, font_name);
			if (!full_font.empty()) {
				convert_to_fs(font_name, fontPath, lengthof(fontPath));
			}
		}

		if (fontPath[0] != 0) {
			if (AddFontResourceEx(fontPath, FR_PRIVATE, 0) != 0) {
				/* Try a nice little undocumented function first for getting the internal font name.
				 * Some documentation is found at: http://www.undocprint.org/winspool/getfontresourceinfo */
				static DllLoader _gdi32(L"gdi32.dll");
				typedef BOOL(WINAPI *PFNGETFONTRESOURCEINFO)(LPCTSTR, LPDWORD, LPVOID, DWORD);
				static PFNGETFONTRESOURCEINFO GetFontResourceInfo = _gdi32.GetProcAddress("GetFontResourceInfoW");

				if (GetFontResourceInfo != nullptr) {
					/* Try to query an array of LOGFONTs that describe the file. */
					DWORD len = 0;
					if (GetFontResourceInfo(fontPath, &len, nullptr, 2) && len >= sizeof(LOGFONT)) {
						LOGFONT *buf = (LOGFONT *)new byte[len];
						if (GetFontResourceInfo(fontPath, &len, buf, 2)) {
							logfont = *buf; // Just use first entry.
						}
						delete[] (byte *)buf;
					}
				}

				/* No dice yet. Use the file name as the font face name, hoping it matches. */
				if (logfont.lfFaceName[0] == 0) {
					wchar_t fname[_MAX_FNAME];
					_wsplitpath(fontPath, nullptr, nullptr, fname, nullptr);

					wcsncpy_s(logfont.lfFaceName, lengthof(logfont.lfFaceName), fname, _TRUNCATE);
					logfont.lfWeight = strcasestr(font_name, " bold") != nullptr || strcasestr(font_name, "-bold") != nullptr ? FW_BOLD : FW_NORMAL; // Poor man's way to allow selecting bold fonts.
				}
			} else {
				ShowInfo("Unable to load file '{}' for {} font, using default windows font selection instead", font_name, FontSizeToName(fs));
			}
		}
	}

	if (logfont.lfFaceName[0] == 0) {
		logfont.lfWeight = strcasestr(font_name, " bold") != nullptr ? FW_BOLD : FW_NORMAL; // Poor man's way to allow selecting bold fonts.
		convert_to_fs(font_name, logfont.lfFaceName, lengthof(logfont.lfFaceName));
	}

	HFONT font = CreateFontIndirect(&logfont);
	if (font == nullptr) {
		ShowInfo("Unable to use '{}' for {} font, Win32 reported error 0x{:X}, using sprite font instead", font_name, FontSizeToName(fs), GetLastError());
		return;
	}
	DeleteObject(font);

	new Win32FontCache(fs, logfont, settings->size);
}
