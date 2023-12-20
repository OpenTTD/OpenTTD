/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file font_osx.cpp Functions related to font handling on MacOS. */

#include "../../stdafx.h"
#include "../../debug.h"
#include "font_osx.h"
#include "../../core/math_func.hpp"
#include "../../blitter/factory.hpp"
#include "../../error_func.h"
#include "../../fileio_func.h"
#include "../../fontdetection.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../zoom_func.h"
#include "macos.h"

#include "../../table/control_codes.h"

#include "safeguards.h"

bool SetFallbackFont(FontCacheSettings *settings, const std::string &language_isocode, int, MissingGlyphSearcher *callback)
{
	/* Determine fallback font using CoreText. This uses the language isocode
	 * to find a suitable font. CoreText is available from 10.5 onwards. */
	std::string lang;
	if (language_isocode == "zh_TW") {
		/* Traditional Chinese */
		lang = "zh-Hant";
	} else if (language_isocode == "zh_CN") {
		/* Simplified Chinese */
		lang = "zh-Hans";
	} else {
		/* Just copy the first part of the isocode. */
		lang = language_isocode.substr(0, language_isocode.find('_'));
	}

	/* Create a font descriptor matching the wanted language and latin (english) glyphs.
	 * Can't use CFAutoRelease here for everything due to the way the dictionary has to be created. */
	CFStringRef lang_codes[2];
	lang_codes[0] = CFStringCreateWithCString(kCFAllocatorDefault, lang.c_str(), kCFStringEncodingUTF8);
	lang_codes[1] = CFSTR("en");
	CFArrayRef lang_arr = CFArrayCreate(kCFAllocatorDefault, (const void **)lang_codes, lengthof(lang_codes), &kCFTypeArrayCallBacks);
	CFAutoRelease<CFDictionaryRef> lang_attribs(CFDictionaryCreate(kCFAllocatorDefault, const_cast<const void **>(reinterpret_cast<const void *const *>(&kCTFontLanguagesAttribute)), (const void **)&lang_arr, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
	CFAutoRelease<CTFontDescriptorRef> lang_desc(CTFontDescriptorCreateWithAttributes(lang_attribs.get()));
	CFRelease(lang_arr);
	CFRelease(lang_codes[0]);

	/* Get array of all font descriptors for the wanted language. */
	CFAutoRelease<CFSetRef> mandatory_attribs(CFSetCreate(kCFAllocatorDefault, const_cast<const void **>(reinterpret_cast<const void *const *>(&kCTFontLanguagesAttribute)), 1, &kCFTypeSetCallBacks));
	CFAutoRelease<CFArrayRef> descs(CTFontDescriptorCreateMatchingFontDescriptors(lang_desc.get(), mandatory_attribs.get()));

	bool result = false;
	for (int tries = 0; tries < 2; tries++) {
		for (CFIndex i = 0; descs.get() != nullptr && i < CFArrayGetCount(descs.get()); i++) {
			CTFontDescriptorRef font = (CTFontDescriptorRef)CFArrayGetValueAtIndex(descs.get(), i);

			/* Get font traits. */
			CFAutoRelease<CFDictionaryRef> traits((CFDictionaryRef)CTFontDescriptorCopyAttribute(font, kCTFontTraitsAttribute));
			CTFontSymbolicTraits symbolic_traits;
			CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(traits.get(), kCTFontSymbolicTrait), kCFNumberIntType, &symbolic_traits);

			/* Skip symbol fonts and vertical fonts. */
			if ((symbolic_traits & kCTFontClassMaskTrait) == (CTFontStylisticClass)kCTFontSymbolicClass || (symbolic_traits & kCTFontVerticalTrait)) continue;
			/* Skip bold fonts (especially Arial Bold, which looks worse than regular Arial). */
			if (symbolic_traits & kCTFontBoldTrait) continue;
			/* Select monospaced fonts if asked for. */
			if (((symbolic_traits & kCTFontMonoSpaceTrait) == kCTFontMonoSpaceTrait) != callback->Monospace()) continue;

			/* Get font name. */
			char name[128];
			CFAutoRelease<CFStringRef> font_name((CFStringRef)CTFontDescriptorCopyAttribute(font, kCTFontDisplayNameAttribute));
			CFStringGetCString(font_name.get(), name, lengthof(name), kCFStringEncodingUTF8);

			/* Serif fonts usually look worse on-screen with only small
			 * font sizes. As such, we try for a sans-serif font first.
			 * If we can't find one in the first try, try all fonts. */
			if (tries == 0 && (symbolic_traits & kCTFontClassMaskTrait) != (CTFontStylisticClass)kCTFontSansSerifClass) continue;

			/* There are some special fonts starting with an '.' and the last
			 * resort font that aren't usable. Skip them. */
			if (name[0] == '.' || strncmp(name, "LastResort", 10) == 0) continue;

			/* Save result. */
			callback->SetFontNames(settings, name);
			if (!callback->FindMissingGlyphs()) {
				Debug(fontcache, 2, "CT-Font for {}: {}", language_isocode, name);
				result = true;
				break;
			}
		}
	}

	if (!result) {
		/* For some OS versions, the font 'Arial Unicode MS' does not report all languages it
		 * supports. If we didn't find any other font, just try it, maybe we get lucky. */
		callback->SetFontNames(settings, "Arial Unicode MS");
		result = !callback->FindMissingGlyphs();
	}

	callback->FindMissingGlyphs();
	return result;
}


CoreTextFontCache::CoreTextFontCache(FontSize fs, CFAutoRelease<CTFontDescriptorRef> &&font, int pixels) : TrueTypeFontCache(fs, pixels), font_desc(std::move(font))
{
	this->SetFontSize(pixels);
}

/**
 * Reset cached glyphs.
 */
void CoreTextFontCache::ClearFontCache()
{
	/* GUI scaling might have changed, determine font size anew if it was automatically selected. */
	if (this->font) this->SetFontSize(this->req_size);

	this->TrueTypeFontCache::ClearFontCache();
}

void CoreTextFontCache::SetFontSize(int pixels)
{
	if (pixels == 0) {
		/* Try to determine a good height based on the height recommended by the font. */
		int scaled_height = ScaleGUITrad(FontCache::GetDefaultFontHeight(this->fs));
		pixels = scaled_height;

		CFAutoRelease<CTFontRef> font(CTFontCreateWithFontDescriptor(this->font_desc.get(), 0.0f, nullptr));
		if (font) {
			float min_size = 0.0f;

			/* The 'head' TrueType table contains information about the
			 * 'smallest readable size in pixels'. Try to read it, if
			 * that doesn't work, we use the default OS font size instead.
			 *
			 * Reference: https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6head.html */
			CFAutoRelease<CFDataRef> data(CTFontCopyTable(font.get(), kCTFontTableHead, kCTFontTableOptionNoOptions));
			if (data) {
				uint16_t lowestRecPPEM; // At offset 46 of the 'head' TrueType table.
				CFDataGetBytes(data.get(), CFRangeMake(46, sizeof(lowestRecPPEM)), (UInt8 *)&lowestRecPPEM);
				min_size = CFSwapInt16BigToHost(lowestRecPPEM); // TrueType data is always big-endian.
			} else {
				CFAutoRelease<CFNumberRef> size((CFNumberRef)CTFontCopyAttribute(font.get(), kCTFontSizeAttribute));
				CFNumberGetValue(size.get(), kCFNumberFloatType, &min_size);
			}

			/* Font height is minimum height plus the difference between the default
			 * height for this font size and the small size. */
			int diff = scaled_height - ScaleGUITrad(FontCache::GetDefaultFontHeight(FS_SMALL));
			/* Clamp() is not used as scaled_height could be greater than MAX_FONT_SIZE, which is not permitted in Clamp(). */
			pixels = std::min(std::max(std::min<int>(min_size, MAX_FONT_MIN_REC_SIZE) + diff, scaled_height), MAX_FONT_SIZE);
		}
	} else {
		pixels = ScaleGUITrad(pixels);
	}
	this->used_size = pixels;

	this->font.reset(CTFontCreateWithFontDescriptor(this->font_desc.get(), pixels, nullptr));

	/* Query the font metrics we needed. We generally round all values up to
	 * make sure we don't inadvertently cut off a row or column of pixels,
	 * except when determining glyph to glyph advances. */
	this->units_per_em = CTFontGetUnitsPerEm(this->font.get());
	this->ascender = (int)std::ceil(CTFontGetAscent(this->font.get()));
	this->descender = -(int)std::ceil(CTFontGetDescent(this->font.get()));
	this->height = this->ascender - this->descender;

	/* Get real font name. */
	char name[128];
	CFAutoRelease<CFStringRef> font_name((CFStringRef)CTFontCopyAttribute(this->font.get(), kCTFontDisplayNameAttribute));
	CFStringGetCString(font_name.get(), name, lengthof(name), kCFStringEncodingUTF8);
	this->font_name = name;

	Debug(fontcache, 2, "Loaded font '{}' with size {}", this->font_name, pixels);
}

GlyphID CoreTextFontCache::MapCharToGlyph(char32_t key)
{
	assert(IsPrintable(key));

	if (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END) {
		return this->parent->MapCharToGlyph(key);
	}

	/* Convert characters outside of the Basic Multilingual Plane into surrogate pairs. */
	UniChar chars[2];
	if (key >= 0x010000U) {
		chars[0] = (UniChar)(((key - 0x010000U) >> 10) + 0xD800);
		chars[1] = (UniChar)(((key - 0x010000U) & 0x3FF) + 0xDC00);
	} else {
		chars[0] = (UniChar)(key & 0xFFFF);
	}

	CGGlyph glyph[2] = {0, 0};
	if (CTFontGetGlyphsForCharacters(this->font.get(), chars, glyph, key >= 0x010000U ? 2 : 1)) {
		return glyph[0];
	}

	return 0;
}

const void *CoreTextFontCache::InternalGetFontTable(uint32_t tag, size_t &length)
{
	CFAutoRelease<CFDataRef> data(CTFontCopyTable(this->font.get(), (CTFontTableTag)tag, kCTFontTableOptionNoOptions));
	if (!data) return nullptr;

	length = CFDataGetLength(data.get());
	auto buf = MallocT<UInt8>(length);

	CFDataGetBytes(data.get(), CFRangeMake(0, (CFIndex)length), buf);
	return buf;
}

const Sprite *CoreTextFontCache::InternalGetGlyph(GlyphID key, bool use_aa)
{
	/* Get glyph size. */
	CGGlyph glyph = (CGGlyph)key;
	CGRect bounds = CGRectNull;
	if (MacOSVersionIsAtLeast(10, 8, 0)) {
		bounds = CTFontGetOpticalBoundsForGlyphs(this->font.get(), &glyph, nullptr, 1, 0);
	} else {
		bounds = CTFontGetBoundingRectsForGlyphs(this->font.get(), kCTFontOrientationDefault, &glyph, nullptr, 1);
	}
	if (CGRectIsNull(bounds)) UserError("Unable to render font glyph");

	uint bb_width = (uint)std::ceil(bounds.size.width) + 1; // Sometimes the glyph bounds are too tight and cut of the last pixel after rounding.
	uint bb_height = (uint)std::ceil(bounds.size.height);

	/* Add 1 scaled pixel for the shadow on the medium font. Our sprite must be at least 1x1 pixel. */
	uint shadow = (this->fs == FS_NORMAL) ? ScaleGUITrad(1) : 0;
	uint width = std::max(1U, bb_width + shadow);
	uint height = std::max(1U, bb_height + shadow);

	/* Limit glyph size to prevent overflows later on. */
	if (width > MAX_GLYPH_DIM || height > MAX_GLYPH_DIM) UserError("Font glyph is too large");

	SpriteLoader::SpriteCollection spritecollection;
	SpriteLoader::Sprite &sprite = spritecollection[ZOOM_LVL_NORMAL];
	sprite.AllocateData(ZOOM_LVL_NORMAL, width * height);
	sprite.type = SpriteType::Font;
	sprite.colours = (use_aa ? SCC_PAL | SCC_ALPHA : SCC_PAL);
	sprite.width = width;
	sprite.height = height;
	sprite.x_offs = (int16_t)std::round(CGRectGetMinX(bounds));
	sprite.y_offs = this->ascender - (int16_t)std::ceil(CGRectGetMaxY(bounds));

	if (bounds.size.width > 0) {
		/* Glyph is not a white-space glyph. Render it to a bitmap context. */

		/* We only need the alpha channel, as we apply our own colour constants to the sprite. */
		int pitch = Align(bb_width, 16);
		byte *bmp = CallocT<byte>(bb_height * pitch);
		CFAutoRelease<CGContextRef> context(CGBitmapContextCreate(bmp, bb_width, bb_height, 8, pitch, nullptr, kCGImageAlphaOnly));
		/* Set antialias according to requirements. */
		CGContextSetAllowsAntialiasing(context.get(), use_aa);
		CGContextSetAllowsFontSubpixelPositioning(context.get(), use_aa);
		CGContextSetAllowsFontSubpixelQuantization(context.get(), !use_aa);
		CGContextSetShouldSmoothFonts(context.get(), false);

		float offset = 0.5f; // CoreText uses 0.5 as pixel centers. We want pixel alignment.
		CGPoint pos{offset - bounds.origin.x, offset - bounds.origin.y};
		CTFontDrawGlyphs(this->font.get(), &glyph, &pos, 1, context.get());

		/* Draw shadow for medium size. */
		if (this->fs == FS_NORMAL && !use_aa) {
			for (uint y = 0; y < bb_height; y++) {
				for (uint x = 0; x < bb_width; x++) {
					if (bmp[y * pitch + x] > 0) {
						sprite.data[shadow + x + (shadow + y) * sprite.width].m = SHADOW_COLOUR;
						sprite.data[shadow + x + (shadow + y) * sprite.width].a = use_aa ? bmp[x + y * pitch] : 0xFF;
					}
				}
			}
		}

		/* Extract pixel data. */
		for (uint y = 0; y < bb_height; y++) {
			for (uint x = 0; x < bb_width; x++) {
				if (bmp[y * pitch + x] > 0) {
					sprite.data[x + y * sprite.width].m = FACE_COLOUR;
					sprite.data[x + y * sprite.width].a = use_aa ? bmp[x + y * pitch] : 0xFF;
				}
			}
		}
	}

	GlyphEntry new_glyph;
	new_glyph.sprite = BlitterFactory::GetCurrentBlitter()->Encode(spritecollection, SimpleSpriteAlloc);
	new_glyph.width = (byte)std::round(CTFontGetAdvancesForGlyphs(this->font.get(), kCTFontOrientationDefault, &glyph, nullptr, 1));
	this->SetGlyphPtr(key, &new_glyph);

	return new_glyph.sprite;
}

/**
 * Loads the TrueType font.
 * If a CoreText font description is present, e.g. from the automatic font
 * fallback search, use it. Otherwise, try to resolve it by font name.
 * @param fs The font size to load.
 */
void LoadCoreTextFont(FontSize fs)
{
	FontCacheSubSetting *settings = GetFontCacheSubSetting(fs);

	if (settings->font.empty()) return;

	CFAutoRelease<CTFontDescriptorRef> font_ref;

	if (settings->os_handle != nullptr) {
		font_ref.reset(static_cast<CTFontDescriptorRef>(const_cast<void *>(settings->os_handle)));
		CFRetain(font_ref.get()); // Increase ref count to match a later release.
	}

	if (!font_ref && MacOSVersionIsAtLeast(10, 6, 0)) {
		/* Might be a font file name, try load it. Direct font loading is
		 * only supported starting on OSX 10.6. */
		CFAutoRelease<CFStringRef> path;

		/* See if this is an absolute path. */
		if (FileExists(settings->font)) {
			path.reset(CFStringCreateWithCString(kCFAllocatorDefault, settings->font.c_str(), kCFStringEncodingUTF8));
		} else {
			/* Scan the search-paths to see if it can be found. */
			std::string full_font = FioFindFullPath(BASE_DIR, settings->font);
			if (!full_font.empty()) {
				path.reset(CFStringCreateWithCString(kCFAllocatorDefault, full_font.c_str(), kCFStringEncodingUTF8));
			}
		}

		if (path) {
			/* Try getting a font descriptor to see if the system can use it. */
			CFAutoRelease<CFURLRef> url(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path.get(), kCFURLPOSIXPathStyle, false));
			CFAutoRelease<CFArrayRef> descs(CTFontManagerCreateFontDescriptorsFromURL(url.get()));

			if (descs && CFArrayGetCount(descs.get()) > 0) {
				font_ref.reset((CTFontDescriptorRef)CFArrayGetValueAtIndex(descs.get(), 0));
				CFRetain(font_ref.get());
			} else {
				ShowInfo("Unable to load file '{}' for {} font, using default OS font selection instead", settings->font, FontSizeToName(fs));
			}
		}
	}

	if (!font_ref) {
		CFAutoRelease<CFStringRef> name(CFStringCreateWithCString(kCFAllocatorDefault, settings->font.c_str(), kCFStringEncodingUTF8));

		/* Simply creating the font using CTFontCreateWithNameAndSize will *always* return
		 * something, no matter the name. As such, we can't use it to check for existence.
		 * We instead query the list of all font descriptors that match the given name which
		 * does not do this stupid name fallback. */
		CFAutoRelease<CTFontDescriptorRef> name_desc(CTFontDescriptorCreateWithNameAndSize(name.get(), 0.0));
		CFAutoRelease<CFSetRef> mandatory_attribs(CFSetCreate(kCFAllocatorDefault, const_cast<const void **>(reinterpret_cast<const void * const *>(&kCTFontNameAttribute)), 1, &kCFTypeSetCallBacks));
		CFAutoRelease<CFArrayRef> descs(CTFontDescriptorCreateMatchingFontDescriptors(name_desc.get(), mandatory_attribs.get()));

		/* Assume the first result is the one we want. */
		if (descs && CFArrayGetCount(descs.get()) > 0) {
			font_ref.reset((CTFontDescriptorRef)CFArrayGetValueAtIndex(descs.get(), 0));
			CFRetain(font_ref.get());
		}
	}

	if (!font_ref) {
		ShowInfo("Unable to use '{}' for {} font, using sprite font instead", settings->font, FontSizeToName(fs));
		return;
	}

	new CoreTextFontCache(fs, std::move(font_ref), settings->size);
}
