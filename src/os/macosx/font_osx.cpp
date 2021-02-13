/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file font_osx.cpp Functions related to font handling on MacOS. */

#include "../../stdafx.h"
#include "../../debug.h"
#include "../../fontdetection.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "macos.h"

#include "safeguards.h"

#ifdef WITH_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H

extern FT_Library _library;


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
	CFAutoRelease<CFSetRef> mandatory_attribs(CFSetCreate(kCFAllocatorDefault, const_cast<const void **>(reinterpret_cast<const void *const *>(&kCTFontNameAttribute)), 1, &kCFTypeSetCallBacks));
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

#endif /* WITH_FREETYPE */


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
	CFAutoRelease<CFDictionaryRef> lang_attribs(CFDictionaryCreate(kCFAllocatorDefault, const_cast<const void **>(reinterpret_cast<const void *const *>(&kCTFontLanguagesAttribute)), (const void **)&lang_arr, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
	CFAutoRelease<CTFontDescriptorRef> lang_desc(CTFontDescriptorCreateWithAttributes(lang_attribs.get()));
	CFRelease(lang_arr);
	CFRelease(lang_codes[0]);

	/* Get array of all font descriptors for the wanted language. */
	CFAutoRelease<CFSetRef> mandatory_attribs(CFSetCreate(kCFAllocatorDefault, const_cast<const void **>(reinterpret_cast<const void *const *>(&kCTFontLanguagesAttribute)), 1, &kCFTypeSetCallBacks));
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
