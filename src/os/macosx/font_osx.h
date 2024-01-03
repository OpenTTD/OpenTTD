/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file font_osx.h Functions related to font handling on MacOS. */

#ifndef FONT_OSX_H
#define FONT_OSX_H

#include "../../fontcache/truetypefontcache.h"
#include "os/macosx/macos.h"

#include <CoreFoundation/CoreFoundation.h>

class CoreTextFontCache : public TrueTypeFontCache {
	CFAutoRelease<CTFontDescriptorRef> font_desc; ///< Font descriptor exlcuding font size.
	CFAutoRelease<CTFontRef> font;                ///< CoreText font handle.

	std::string font_name;                        ///< Cached font name.

	void SetFontSize(int pixels);
	const Sprite *InternalGetGlyph(GlyphID key, bool use_aa) override;
	const void *InternalGetFontTable(uint32_t tag, size_t &length) override;
public:
	CoreTextFontCache(FontSize fs, CFAutoRelease<CTFontDescriptorRef> &&font, int pixels);
	~CoreTextFontCache() {}

	void ClearFontCache() override;
	GlyphID MapCharToGlyph(char32_t key) override;
	std::string GetFontName() override { return font_name; }
	bool IsBuiltInFont() override { return false; }
	const void *GetOSHandle() override { return font.get(); }
};

void LoadCoreTextFont(FontSize fs);

#endif /* FONT_OSX_H */
