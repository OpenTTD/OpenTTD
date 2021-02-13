/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file font_win32.h Functions related to font handling on Win32. */

#ifndef FONT_WIN32_H
#define FONT_WIN32_H

#include "../../fontcache_internal.h"
#include "win32.h"

/** Font cache for fonts that are based on a Win32 font. */
class Win32FontCache : public TrueTypeFontCache {
private:
	LOGFONT logfont;      ///< Logical font information for selecting the font face.
	HFONT font = nullptr; ///< The font face associated with this font.
	HDC dc = nullptr;     ///< Cached GDI device context.
	HGDIOBJ old_font;     ///< Old font selected into the GDI context.
	SIZE glyph_size;      ///< Maximum size of regular glyphs.

	void SetFontSize(FontSize fs, int pixels);

protected:
	const void *InternalGetFontTable(uint32 tag, size_t &length) override;
	const Sprite *InternalGetGlyph(GlyphID key, bool aa) override;

public:
	Win32FontCache(FontSize fs, const LOGFONT &logfont, int pixels);
	~Win32FontCache();
	void ClearFontCache() override;
	GlyphID MapCharToGlyph(WChar key) override;
	const char *GetFontName() override { return WIDE_TO_MB(this->logfont.lfFaceName); }
	const void *GetOSHandle() override { return &this->logfont; }
};

void LoadWin32Font(FontSize fs);

#endif /* FONT_WIN32_H */
