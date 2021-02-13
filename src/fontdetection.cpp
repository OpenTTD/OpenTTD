/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fontdetection.cpp Detection of the right font. */

#if defined(WITH_FREETYPE) || defined(_WIN32)

#include "stdafx.h"
#include "debug.h"
#include "fontdetection.h"
#include "string_func.h"
#include "strings_func.h"

#ifdef WITH_FREETYPE
extern FT_Library _library;
#endif /* WITH_FREETYPE */

/**
 * Get the font loaded into a Freetype face by using a font-name.
 * If no appropriate font is found, the function returns an error
 */

#if defined(__APPLE__)
/* ========================================================================================
 * OSX support
 * ======================================================================================== */

#include "os/macosx/macos.h"

#include "safeguards.h"

FT_Error GetFontByFaceName(const char *font_name, FT_Face *face)
{
	FT_Error err = FT_Err_Cannot_Open_Resource;

	/* Get font reference from name. */
	UInt8 file_path[PATH_MAX];
	OSStatus os_err = -1;
	CFAutoRelease<CFStringRef> name(CFStringCreateWithCString(kCFAllocatorDefault, font_name, kCFStringEncodingUTF8));

	/* Simply creating the font using CTFontCreateWithNameAndSize will *always* return
	 * something, no matter the name. As such, we can't use it to check for existence.
	 * We instead query the list of all font descriptors that match the given name which
	 * does not do this stupid name fallback. */
	CFAutoRelease<CTFontDescriptorRef> name_desc(CTFontDescriptorCreateWithNameAndSize(name.get(), 0.0));
	CFAutoRelease<CFSetRef> mandatory_attribs(CFSetCreate(kCFAllocatorDefault, const_cast<const void **>(reinterpret_cast<const void * const *>(&kCTFontNameAttribute)), 1, &kCFTypeSetCallBacks));
	CFAutoRelease<CFArrayRef> descs(CTFontDescriptorCreateMatchingFontDescriptors(name_desc.get(), mandatory_attribs.get()));

	/* Loop over all matches until we can get a path for one of them. */
	for (CFIndex i = 0; descs.get() != nullptr && i < CFArrayGetCount(descs.get()) && os_err != noErr; i++) {
		CFAutoRelease<CTFontRef> font(CTFontCreateWithFontDescriptor((CTFontDescriptorRef)CFArrayGetValueAtIndex(descs.get(), i), 0.0, nullptr));
		CFAutoRelease<CFURLRef> fontURL((CFURLRef)CTFontCopyAttribute(font.get(), kCTFontURLAttribute));
		if (CFURLGetFileSystemRepresentation(fontURL.get(), true, file_path, lengthof(file_path))) os_err = noErr;
	}

	if (os_err == noErr) {
		DEBUG(freetype, 3, "Font path for %s: %s", font_name, file_path);
		err = FT_New_Face(_library, (const char *)file_path, 0, face);
	}

	return err;
}

bool SetFallbackFont(FreeTypeSettings *settings, const char *language_isocode, int winlangid, MissingGlyphSearcher *callback)
{
	/* Determine fallback font using CoreText. This uses the language isocode
	 * to find a suitable font. CoreText is available from 10.5 onwards. */
	char lang[16];
	if (strcmp(language_isocode, "zh_TW") == 0) {
		/* Traditional Chinese */
		strecpy(lang, "zh-Hant", lastof(lang));
	} else if (strcmp(language_isocode, "zh_CN") == 0) {
		/* Simplified Chinese */
		strecpy(lang, "zh-Hans", lastof(lang));
	} else {
		/* Just copy the first part of the isocode. */
		strecpy(lang, language_isocode, lastof(lang));
		char *sep = strchr(lang, '_');
		if (sep != nullptr) *sep = '\0';
	}

	/* Create a font descriptor matching the wanted language and latin (english) glyphs.
	 * Can't use CFAutoRelease here for everything due to the way the dictionary has to be created. */
	CFStringRef lang_codes[2];
	lang_codes[0] = CFStringCreateWithCString(kCFAllocatorDefault, lang, kCFStringEncodingUTF8);
	lang_codes[1] = CFSTR("en");
	CFArrayRef lang_arr = CFArrayCreate(kCFAllocatorDefault, (const void **)lang_codes, lengthof(lang_codes), &kCFTypeArrayCallBacks);
	CFAutoRelease<CFDictionaryRef> lang_attribs(CFDictionaryCreate(kCFAllocatorDefault, const_cast<const void **>(reinterpret_cast<const void * const *>(&kCTFontLanguagesAttribute)), (const void **)&lang_arr, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
	CFAutoRelease<CTFontDescriptorRef> lang_desc(CTFontDescriptorCreateWithAttributes(lang_attribs.get()));
	CFRelease(lang_arr);
	CFRelease(lang_codes[0]);

	/* Get array of all font descriptors for the wanted language. */
	CFAutoRelease<CFSetRef> mandatory_attribs(CFSetCreate(kCFAllocatorDefault, const_cast<const void **>(reinterpret_cast<const void * const *>(&kCTFontLanguagesAttribute)), 1, &kCFTypeSetCallBacks));
	CFAutoRelease<CFArrayRef> descs(CTFontDescriptorCreateMatchingFontDescriptors(lang_desc.get(), mandatory_attribs.get()));

	bool result = false;
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

		/* There are some special fonts starting with an '.' and the last
		 * resort font that aren't usable. Skip them. */
		if (name[0] == '.' || strncmp(name, "LastResort", 10) == 0) continue;

		/* Save result. */
		callback->SetFontNames(settings, name);
		if (!callback->FindMissingGlyphs()) {
			DEBUG(freetype, 2, "CT-Font for %s: %s", language_isocode, name);
			result = true;
			break;
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

#elif defined(WITH_FONTCONFIG) /* end ifdef __APPLE__ */

#include <fontconfig/fontconfig.h>

#include "safeguards.h"

/* ========================================================================================
 * FontConfig (unix) support
 * ======================================================================================== */
FT_Error GetFontByFaceName(const char *font_name, FT_Face *face)
{
	FT_Error err = FT_Err_Cannot_Open_Resource;

	if (!FcInit()) {
		ShowInfoF("Unable to load font configuration");
	} else {
		FcPattern *match;
		FcPattern *pat;
		FcFontSet *fs;
		FcResult  result;
		char *font_style;
		char *font_family;

		/* Split & strip the font's style */
		font_family = stredup(font_name);
		font_style = strchr(font_family, ',');
		if (font_style != nullptr) {
			font_style[0] = '\0';
			font_style++;
			while (*font_style == ' ' || *font_style == '\t') font_style++;
		}

		/* Resolve the name and populate the information structure */
		pat = FcNameParse((FcChar8*)font_family);
		if (font_style != nullptr) FcPatternAddString(pat, FC_STYLE, (FcChar8*)font_style);
		FcConfigSubstitute(0, pat, FcMatchPattern);
		FcDefaultSubstitute(pat);
		fs = FcFontSetCreate();
		match = FcFontMatch(0, pat, &result);

		if (fs != nullptr && match != nullptr) {
			int i;
			FcChar8 *family;
			FcChar8 *style;
			FcChar8 *file;
			FcFontSetAdd(fs, match);

			for (i = 0; err != FT_Err_Ok && i < fs->nfont; i++) {
				/* Try the new filename */
				if (FcPatternGetString(fs->fonts[i], FC_FILE,   0, &file)   == FcResultMatch &&
						FcPatternGetString(fs->fonts[i], FC_FAMILY, 0, &family) == FcResultMatch &&
						FcPatternGetString(fs->fonts[i], FC_STYLE,  0, &style)  == FcResultMatch) {

					/* The correct style? */
					if (font_style != nullptr && strcasecmp(font_style, (char*)style) != 0) continue;

					/* Font config takes the best shot, which, if the family name is spelled
					 * wrongly a 'random' font, so check whether the family name is the
					 * same as the supplied name */
					if (strcasecmp(font_family, (char*)family) == 0) {
						err = FT_New_Face(_library, (char *)file, 0, face);
					}
				}
			}
		}

		free(font_family);
		FcPatternDestroy(pat);
		FcFontSetDestroy(fs);
		FcFini();
	}

	return err;
}

bool SetFallbackFont(FreeTypeSettings *settings, const char *language_isocode, int winlangid, MissingGlyphSearcher *callback)
{
	if (!FcInit()) return false;

	bool ret = false;

	/* Fontconfig doesn't handle full language isocodes, only the part
	 * before the _ of e.g. en_GB is used, so "remove" everything after
	 * the _. */
	char lang[16];
	seprintf(lang, lastof(lang), ":lang=%s", language_isocode);
	char *split = strchr(lang, '_');
	if (split != nullptr) *split = '\0';

	/* First create a pattern to match the wanted language. */
	FcPattern *pat = FcNameParse((FcChar8*)lang);
	/* We only want to know the filename. */
	FcObjectSet *os = FcObjectSetBuild(FC_FILE, FC_SPACING, FC_SLANT, FC_WEIGHT, nullptr);
	/* Get the list of filenames matching the wanted language. */
	FcFontSet *fs = FcFontList(nullptr, pat, os);

	/* We don't need these anymore. */
	FcObjectSetDestroy(os);
	FcPatternDestroy(pat);

	if (fs != nullptr) {
		int best_weight = -1;
		const char *best_font = nullptr;

		for (int i = 0; i < fs->nfont; i++) {
			FcPattern *font = fs->fonts[i];

			FcChar8 *file = nullptr;
			FcResult res = FcPatternGetString(font, FC_FILE, 0, &file);
			if (res != FcResultMatch || file == nullptr) {
				continue;
			}

			/* Get a font with the right spacing .*/
			int value = 0;
			FcPatternGetInteger(font, FC_SPACING, 0, &value);
			if (callback->Monospace() != (value == FC_MONO) && value != FC_DUAL) continue;

			/* Do not use those that explicitly say they're slanted. */
			FcPatternGetInteger(font, FC_SLANT, 0, &value);
			if (value != 0) continue;

			/* We want the fatter font as they look better at small sizes. */
			FcPatternGetInteger(font, FC_WEIGHT, 0, &value);
			if (value <= best_weight) continue;

			callback->SetFontNames(settings, (const char*)file);

			bool missing = callback->FindMissingGlyphs();
			DEBUG(freetype, 1, "Font \"%s\" misses%s glyphs", file, missing ? "" : " no");

			if (!missing) {
				best_weight = value;
				best_font   = (const char *)file;
			}
		}

		if (best_font != nullptr) {
			ret = true;
			callback->SetFontNames(settings, best_font);
			InitFreeType(callback->Monospace());
		}

		/* Clean up the list of filenames. */
		FcFontSetDestroy(fs);
	}

	FcFini();
	return ret;
}
#endif /* end ifdef WITH_FONTCONFIG */

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(WITH_FONTCONFIG)

#ifdef WITH_FREETYPE
FT_Error GetFontByFaceName(const char *font_name, FT_Face *face) {return FT_Err_Cannot_Open_Resource;}
#endif /* WITH_FREETYPE */

bool SetFallbackFont(FreeTypeSettings *settings, const char *language_isocode, int winlangid, MissingGlyphSearcher *callback) { return false; }
#endif /* !defined(_WIN32) && !defined(__APPLE__) && !defined(WITH_FONTCONFIG) */

#endif /* WITH_FREETYPE */
