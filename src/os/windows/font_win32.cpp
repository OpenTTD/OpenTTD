/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file font_win32.cpp Functions related to font handling on Win32. */

#include "../../stdafx.h"
#include "../../debug.h"
#include "font_win32.h"
#include "../../blitter/factory.hpp"
#include "../../core/alloc_func.hpp"
#include "../../core/math_func.hpp"
#include "../../fileio_func.h"
#include "../../fontdetection.h"
#include "../../fontcache.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../zoom_func.h"

#include "../../table/control_codes.h"

#include <windows.h>
#include <shlobj.h> /* SHGetFolderPath */
#include "os/windows/win32.h"
#undef small // Say what, Windows?

#include "safeguards.h"

#ifdef WITH_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H

extern FT_Library _library;

/**
 * Get the short DOS 8.3 format for paths.
 * FreeType doesn't support Unicode filenames and Windows' fopen (as used
 * by FreeType) doesn't support UTF-8 filenames. So we have to convert the
 * filename into something that isn't UTF-8 but represents the Unicode file
 * name. This is the short DOS 8.3 format. This does not contain any
 * characters that fopen doesn't support.
 * @param long_path the path in system encoding.
 * @return the short path in ANSI (ASCII).
 */
static const char *GetShortPath(const TCHAR *long_path)
{
	static char short_path[MAX_PATH];
	WCHAR short_path_w[MAX_PATH];
	GetShortPathName(long_path, short_path_w, lengthof(short_path_w));
	WideCharToMultiByte(CP_ACP, 0, short_path_w, -1, short_path, lengthof(short_path), nullptr, nullptr);
	return short_path;
}

/* Get the font file to be loaded into Freetype by looping the registry
 * location where windows lists all installed fonts. Not very nice, will
 * surely break if the registry path changes, but it works. Much better
 * solution would be to use CreateFont, and extract the font data from it
 * by GetFontData. The problem with this is that the font file needs to be
 * kept in memory then until the font is no longer needed. This could mean
 * an additional memory usage of 30MB (just for fonts!) when using an eastern
 * font for all font sizes */
static const wchar_t *FONT_DIR_NT = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
FT_Error GetFontByFaceName(const char *font_name, FT_Face *face)
{
	FT_Error err = FT_Err_Cannot_Open_Resource;
	HKEY hKey;
	LONG ret;
	TCHAR vbuffer[MAX_PATH], dbuffer[256];
	TCHAR *pathbuf;
	const char *font_path;
	uint index;
	size_t path_len;

	ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, FONT_DIR_NT, 0, KEY_READ, &hKey);

	if (ret != ERROR_SUCCESS) {
		DEBUG(freetype, 0, "Cannot open registry key HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts");
		return err;
	}

	/* Convert font name to file system encoding. */
	TCHAR *font_namep = _tcsdup(OTTD2FS(font_name));

	for (index = 0;; index++) {
		TCHAR *s;
		DWORD vbuflen = lengthof(vbuffer);
		DWORD dbuflen = lengthof(dbuffer);

		ret = RegEnumValue(hKey, index, vbuffer, &vbuflen, nullptr, nullptr, (byte *)dbuffer, &dbuflen);
		if (ret != ERROR_SUCCESS) goto registry_no_font_found;

		/* The font names in the registry are of the following 3 forms:
		 * - ADMUI3.fon
		 * - Book Antiqua Bold (TrueType)
		 * - Batang & BatangChe & Gungsuh & GungsuhChe (TrueType)
		 * We will strip the font-type '()' if any and work with the font name
		 * itself, which must match exactly; if...
		 * TTC files, font files which contain more than one font are separated
		 * by '&'. Our best bet will be to do substr match for the fontname
		 * and then let FreeType figure out which index to load */
		s = _tcschr(vbuffer, _T('('));
		if (s != nullptr) s[-1] = '\0';

		if (_tcschr(vbuffer, _T('&')) == nullptr) {
			if (_tcsicmp(vbuffer, font_namep) == 0) break;
		} else {
			if (_tcsstr(vbuffer, font_namep) != nullptr) break;
		}
	}

	if (!SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_FONTS, nullptr, SHGFP_TYPE_CURRENT, vbuffer))) {
		DEBUG(freetype, 0, "SHGetFolderPath cannot return fonts directory");
		goto folder_error;
	}

	/* Some fonts are contained in .ttc files, TrueType Collection fonts. These
	 * contain multiple fonts inside this single file. GetFontData however
	 * returns the whole file, so we need to check each font inside to get the
	 * proper font. */
	path_len = _tcslen(vbuffer) + _tcslen(dbuffer) + 2; // '\' and terminating nul.
	pathbuf = AllocaM(TCHAR, path_len);
	_sntprintf(pathbuf, path_len, _T("%s\\%s"), vbuffer, dbuffer);

	/* Convert the path into something that FreeType understands. */
	font_path = GetShortPath(pathbuf);

	index = 0;
	do {
		err = FT_New_Face(_library, font_path, index, face);
		if (err != FT_Err_Ok) break;

		if (strncasecmp(font_name, (*face)->family_name, strlen((*face)->family_name)) == 0) break;
		/* Try english name if font name failed */
		if (strncasecmp(font_name + strlen(font_name) + 1, (*face)->family_name, strlen((*face)->family_name)) == 0) break;
		err = FT_Err_Cannot_Open_Resource;

	} while ((FT_Long)++index != (*face)->num_faces);


folder_error:
registry_no_font_found:
	free(font_namep);
	RegCloseKey(hKey);
	return err;
}

/**
 * Fonts can have localised names and when the system locale is the same as
 * one of those localised names Windows will always return that localised name
 * instead of allowing to get the non-localised (English US) name of the font.
 * This will later on give problems as freetype uses the non-localised name of
 * the font and we need to compare based on that name.
 * Windows furthermore DOES NOT have an API to get the non-localised name nor
 * can we override the system locale. This means that we have to actually read
 * the font itself to gather the font name we want.
 * Based on: http://blogs.msdn.com/michkap/archive/2006/02/13/530814.aspx
 * @param logfont the font information to get the english name of.
 * @return the English name (if it could be found).
 */
static const char *GetEnglishFontName(const ENUMLOGFONTEX *logfont)
{
	static char font_name[MAX_PATH];
	const char *ret_font_name = nullptr;
	uint pos = 0;
	HDC dc;
	HGDIOBJ oldfont;
	byte *buf;
	DWORD dw;
	uint16 format, count, stringOffset, platformId, encodingId, languageId, nameId, length, offset;

	HFONT font = CreateFontIndirect(&logfont->elfLogFont);
	if (font == nullptr) goto err1;

	dc = GetDC(nullptr);
	oldfont = SelectObject(dc, font);
	dw = GetFontData(dc, 'eman', 0, nullptr, 0);
	if (dw == GDI_ERROR) goto err2;

	buf = MallocT<byte>(dw);
	dw = GetFontData(dc, 'eman', 0, buf, dw);
	if (dw == GDI_ERROR) goto err3;

	format = buf[pos++] << 8;
	format += buf[pos++];
	assert(format == 0);
	count = buf[pos++] << 8;
	count += buf[pos++];
	stringOffset = buf[pos++] << 8;
	stringOffset += buf[pos++];
	for (uint i = 0; i < count; i++) {
		platformId = buf[pos++] << 8;
		platformId += buf[pos++];
		encodingId = buf[pos++] << 8;
		encodingId += buf[pos++];
		languageId = buf[pos++] << 8;
		languageId += buf[pos++];
		nameId = buf[pos++] << 8;
		nameId += buf[pos++];
		if (nameId != 1) {
			pos += 4; // skip length and offset
			continue;
		}
		length = buf[pos++] << 8;
		length += buf[pos++];
		offset = buf[pos++] << 8;
		offset += buf[pos++];

		/* Don't buffer overflow */
		length = std::min<uint16>(length, MAX_PATH - 1);
		for (uint j = 0; j < length; j++) font_name[j] = buf[stringOffset + offset + j];
		font_name[length] = '\0';

		if ((platformId == 1 && languageId == 0) ||      // Macintosh English
			(platformId == 3 && languageId == 0x0409)) { // Microsoft English (US)
			ret_font_name = font_name;
			break;
		}
	}

err3:
	free(buf);
err2:
	SelectObject(dc, oldfont);
	ReleaseDC(nullptr, dc);
	DeleteObject(font);
err1:
	return ret_font_name == nullptr ? WIDE_TO_MB((const TCHAR *)logfont->elfFullName) : ret_font_name;
}
#endif /* WITH_FREETYPE */

class FontList {
protected:
	TCHAR **fonts;
	uint items;
	uint capacity;

public:
	FontList() : fonts(nullptr), items(0), capacity(0) { };

	~FontList() {
		if (this->fonts == nullptr) return;

		for (uint i = 0; i < this->items; i++) {
			free(this->fonts[i]);
		}

		free(this->fonts);
	}

	bool Add(const TCHAR *font) {
		for (uint i = 0; i < this->items; i++) {
			if (_tcscmp(this->fonts[i], font) == 0) return false;
		}

		if (this->items == this->capacity) {
			this->capacity += 10;
			this->fonts = ReallocT(this->fonts, this->capacity);
		}

		this->fonts[this->items++] = _tcsdup(font);

		return true;
	}
};

struct EFCParam {
	FreeTypeSettings *settings;
	LOCALESIGNATURE  locale;
	MissingGlyphSearcher *callback;
	FontList fonts;
};

static int CALLBACK EnumFontCallback(const ENUMLOGFONTEX *logfont, const NEWTEXTMETRICEX *metric, DWORD type, LPARAM lParam)
{
	EFCParam *info = (EFCParam *)lParam;

	/* Skip duplicates */
	if (!info->fonts.Add((const TCHAR *)logfont->elfFullName)) return 1;
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
	convert_from_fs((const TCHAR *)logfont->elfFullName, font_name, lengthof(font_name));

#ifdef WITH_FREETYPE
	/* Add english name after font name */
	const char *english_name = GetEnglishFontName(logfont);
	strecpy(font_name + strlen(font_name) + 1, english_name, lastof(font_name));

	/* Check whether we can actually load the font. */
	bool ft_init = _library != nullptr;
	bool found = false;
	FT_Face face;
	/* Init FreeType if needed. */
	if ((ft_init || FT_Init_FreeType(&_library) == FT_Err_Ok) && GetFontByFaceName(font_name, &face) == FT_Err_Ok) {
		FT_Done_Face(face);
		found = true;
	}
	if (!ft_init) {
		/* Uninit FreeType if we did the init. */
		FT_Done_FreeType(_library);
		_library = nullptr;
	}

	if (!found) return 1;
#else
	const char *english_name = font_name;
#endif /* WITH_FREETYPE */

	info->callback->SetFontNames(info->settings, font_name, &logfont->elfLogFont);
	if (info->callback->FindMissingGlyphs()) return 1;
	DEBUG(freetype, 1, "Fallback font: %s (%s)", font_name, english_name);
	return 0; // stop enumerating
}

bool SetFallbackFont(FreeTypeSettings *settings, const char *language_isocode, int winlangid, MissingGlyphSearcher *callback)
{
	DEBUG(freetype, 1, "Trying fallback fonts");
	EFCParam langInfo;
	if (GetLocaleInfo(MAKELCID(winlangid, SORT_DEFAULT), LOCALE_FONTSIGNATURE, (LPTSTR)&langInfo.locale, sizeof(langInfo.locale) / sizeof(TCHAR)) == 0) {
		/* Invalid langid or some other mysterious error, can't determine fallback font. */
		DEBUG(freetype, 1, "Can't get locale info for fallback font (langid=0x%x)", winlangid);
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
	this->SetFontSize(fs, pixels);
}

Win32FontCache::~Win32FontCache()
{
	this->ClearFontCache();
	DeleteDC(this->dc);
	DeleteObject(this->font);
}

void Win32FontCache::SetFontSize(FontSize fs, int pixels)
{
	if (pixels == 0) {
		/* Try to determine a good height based on the minimal height recommended by the font. */
		int scaled_height = ScaleFontTrad(this->GetDefaultFontHeight(this->fs));
		pixels = scaled_height;

		HFONT temp = CreateFontIndirect(&this->logfont);
		if (temp != nullptr) {
			HGDIOBJ old = SelectObject(this->dc, temp);

			UINT size = GetOutlineTextMetrics(this->dc, 0, nullptr);
			LPOUTLINETEXTMETRIC otm = (LPOUTLINETEXTMETRIC)AllocaM(BYTE, size);
			GetOutlineTextMetrics(this->dc, size, otm);

			/* Font height is minimum height plus the difference between the default
			 * height for this font size and the small size. */
			int diff = scaled_height - ScaleFontTrad(this->GetDefaultFontHeight(FS_SMALL));
			pixels = Clamp(std::min(otm->otmusMinimumPPEM, MAX_FONT_MIN_REC_SIZE) + diff, scaled_height, MAX_FONT_SIZE);

			SelectObject(dc, old);
			DeleteObject(temp);
		}
	} else {
		pixels = ScaleFontTrad(pixels);
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
	POUTLINETEXTMETRIC otm = (POUTLINETEXTMETRIC)AllocaM(BYTE, otmSize);
	GetOutlineTextMetrics(this->dc, otmSize, otm);

	this->units_per_em = otm->otmEMSquare;
	this->ascender = otm->otmTextMetrics.tmAscent;
	this->descender = otm->otmTextMetrics.tmDescent;
	this->height = this->ascender + this->descender;
	this->glyph_size.cx = otm->otmTextMetrics.tmMaxCharWidth;
	this->glyph_size.cy = otm->otmTextMetrics.tmHeight;

	DEBUG(freetype, 2, "Loaded font '%s' with size %d", FS2OTTD((LPTSTR)((BYTE *)otm + (ptrdiff_t)otm->otmpFullName)), pixels);
}

/**
 * Reset cached glyphs.
 */
void Win32FontCache::ClearFontCache()
{
	/* GUI scaling might have changed, determine font size anew if it was automatically selected. */
	if (this->font != nullptr) this->SetFontSize(this->fs, this->req_size);

	this->TrueTypeFontCache::ClearFontCache();
}

/* virtual */ const Sprite *Win32FontCache::InternalGetGlyph(GlyphID key, bool aa)
{
	GLYPHMETRICS gm;
	MAT2 mat = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };

	/* Make a guess for the needed memory size. */
	DWORD size = this->glyph_size.cy * Align(aa ? this->glyph_size.cx : std::max(this->glyph_size.cx / 8l, 1l), 4); // Bitmap data is DWORD-aligned rows.
	byte *bmp = AllocaM(byte, size);
	size = GetGlyphOutline(this->dc, key, GGO_GLYPH_INDEX | (aa ? GGO_GRAY8_BITMAP : GGO_BITMAP), &gm, size, bmp, &mat);

	if (size == GDI_ERROR) {
		/* No dice with the guess. First query size of needed glyph memory, then allocate the
		 * memory and query again. This dance is necessary as some glyphs will only render with
		 * the exact matching size; e.g. the space glyph has no pixels and must be requested
		 * with size == 0, anything else fails. Unfortunately, a failed call doesn't return any
		 * info about the size and thus the triple GetGlyphOutline()-call. */
		size = GetGlyphOutline(this->dc, key, GGO_GLYPH_INDEX | (aa ? GGO_GRAY8_BITMAP : GGO_BITMAP), &gm, 0, nullptr, &mat);
		if (size == GDI_ERROR) usererror("Unable to render font glyph");
		bmp = AllocaM(byte, size);
		GetGlyphOutline(this->dc, key, GGO_GLYPH_INDEX | (aa ? GGO_GRAY8_BITMAP : GGO_BITMAP), &gm, size, bmp, &mat);
	}

	/* Add 1 pixel for the shadow on the medium font. Our sprite must be at least 1x1 pixel. */
	uint width = std::max(1U, (uint)gm.gmBlackBoxX + (this->fs == FS_NORMAL));
	uint height = std::max(1U, (uint)gm.gmBlackBoxY + (this->fs == FS_NORMAL));

	/* Limit glyph size to prevent overflows later on. */
	if (width > MAX_GLYPH_DIM || height > MAX_GLYPH_DIM) usererror("Font glyph is too large");

	/* GDI has rendered the glyph, now we allocate a sprite and copy the image into it. */
	SpriteLoader::Sprite sprite;
	sprite.AllocateData(ZOOM_LVL_NORMAL, width * height);
	sprite.type = ST_FONT;
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
		uint pitch = Align(aa ? gm.gmBlackBoxX : std::max(gm.gmBlackBoxX / 8u, 1u), 4);

		/* Draw shadow for medium size. */
		if (this->fs == FS_NORMAL && !aa) {
			for (uint y = 0; y < gm.gmBlackBoxY; y++) {
				for (uint x = 0; x < gm.gmBlackBoxX; x++) {
					if (aa ? (bmp[x + y * pitch] > 0) : HasBit(bmp[(x / 8) + y * pitch], 7 - (x % 8))) {
						sprite.data[1 + x + (1 + y) * sprite.width].m = SHADOW_COLOUR;
						sprite.data[1 + x + (1 + y) * sprite.width].a = aa ? (bmp[x + y * pitch] << 2) - 1 : 0xFF;
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
	new_glyph.sprite = BlitterFactory::GetCurrentBlitter()->Encode(&sprite, SimpleSpriteAlloc);
	new_glyph.width = gm.gmCellIncX;

	this->SetGlyphPtr(key, &new_glyph);

	return new_glyph.sprite;
}

/* virtual */ GlyphID Win32FontCache::MapCharToGlyph(WChar key)
{
	assert(IsPrintable(key));

	if (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END) {
		return this->parent->MapCharToGlyph(key);
	}

	/* Convert characters outside of the BMP into surrogate pairs. */
	WCHAR chars[2];
	if (key >= 0x010000U) {
		chars[0] = (WCHAR)(((key - 0x010000U) >> 10) + 0xD800);
		chars[1] = (WCHAR)(((key - 0x010000U) & 0x3FF) + 0xDC00);
	} else {
		chars[0] = (WCHAR)(key & 0xFFFF);
	}

	WORD glyphs[2] = { 0, 0 };
	GetGlyphIndicesW(this->dc, chars, key >= 0x010000U ? 2 : 1, glyphs, GGI_MARK_NONEXISTING_GLYPHS);

	return glyphs[0] != 0xFFFF ? glyphs[0] : 0;
}

/* virtual */ const void *Win32FontCache::InternalGetFontTable(uint32 tag, size_t &length)
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
	static const char *SIZE_TO_NAME[] = { "medium", "small", "large", "mono" };

	FreeTypeSubSetting *settings = nullptr;
	switch (fs) {
		case FS_SMALL:  settings = &_freetype.small;  break;
		case FS_NORMAL: settings = &_freetype.medium; break;
		case FS_LARGE:  settings = &_freetype.large;  break;
		case FS_MONO:   settings = &_freetype.mono;   break;
		default: NOT_REACHED();
	}

	if (StrEmpty(settings->font)) return;

	LOGFONT logfont;
	MemSetT(&logfont, 0);
	logfont.lfPitchAndFamily = fs == FS_MONO ? FIXED_PITCH : VARIABLE_PITCH;
	logfont.lfCharSet = DEFAULT_CHARSET;
	logfont.lfOutPrecision = OUT_OUTLINE_PRECIS;
	logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;

	if (settings->os_handle != nullptr) {
		logfont = *(const LOGFONT *)settings->os_handle;
	} else if (strchr(settings->font, '.') != nullptr) {
		/* Might be a font file name, try load it. */

		TCHAR fontPath[MAX_PATH] = {};

		/* See if this is an absolute path. */
		if (FileExists(settings->font)) {
			convert_to_fs(settings->font, fontPath, lengthof(fontPath), false);
		} else {
			/* Scan the search-paths to see if it can be found. */
			std::string full_font = FioFindFullPath(BASE_DIR, settings->font);
			if (!full_font.empty()) {
				convert_to_fs(full_font.c_str(), fontPath, lengthof(fontPath), false);
			}
		}

		if (fontPath[0] != 0) {
			if (AddFontResourceEx(fontPath, FR_PRIVATE, 0) != 0) {
				/* Try a nice little undocumented function first for getting the internal font name.
				 * Some documentation is found at: http://www.undocprint.org/winspool/getfontresourceinfo */
				typedef BOOL(WINAPI *PFNGETFONTRESOURCEINFO)(LPCTSTR, LPDWORD, LPVOID, DWORD);
				static PFNGETFONTRESOURCEINFO GetFontResourceInfo = (PFNGETFONTRESOURCEINFO)GetProcAddress(GetModuleHandle(_T("Gdi32")), "GetFontResourceInfoW");

				if (GetFontResourceInfo != nullptr) {
					/* Try to query an array of LOGFONTs that describe the file. */
					DWORD len = 0;
					if (GetFontResourceInfo(fontPath, &len, nullptr, 2) && len >= sizeof(LOGFONT)) {
						LOGFONT *buf = (LOGFONT *)AllocaM(byte, len);
						if (GetFontResourceInfo(fontPath, &len, buf, 2)) {
							logfont = *buf; // Just use first entry.
						}
					}
				}

				/* No dice yet. Use the file name as the font face name, hoping it matches. */
				if (logfont.lfFaceName[0] == 0) {
					TCHAR fname[_MAX_FNAME];
					_tsplitpath(fontPath, nullptr, nullptr, fname, nullptr);

					_tcsncpy_s(logfont.lfFaceName, lengthof(logfont.lfFaceName), fname, _TRUNCATE);
					logfont.lfWeight = strcasestr(settings->font, " bold") != nullptr || strcasestr(settings->font, "-bold") != nullptr ? FW_BOLD : FW_NORMAL; // Poor man's way to allow selecting bold fonts.
				}
			} else {
				ShowInfoF("Unable to load file '%s' for %s font, using default windows font selection instead", settings->font, SIZE_TO_NAME[fs]);
			}
		}
	}

	if (logfont.lfFaceName[0] == 0) {
		logfont.lfWeight = strcasestr(settings->font, " bold") != nullptr ? FW_BOLD : FW_NORMAL; // Poor man's way to allow selecting bold fonts.
		convert_to_fs(settings->font, logfont.lfFaceName, lengthof(logfont.lfFaceName), false);
	}

	HFONT font = CreateFontIndirect(&logfont);
	if (font == nullptr) {
		ShowInfoF("Unable to use '%s' for %s font, Win32 reported error 0x%lX, using sprite font instead", settings->font, SIZE_TO_NAME[fs], GetLastError());
		return;
	}
	DeleteObject(font);

	new Win32FontCache(fs, logfont, settings->size);
}
