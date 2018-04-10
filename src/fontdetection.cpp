/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fontdetection.cpp Detection of the right font. */

#ifdef WITH_FREETYPE

#include "stdafx.h"
#include "debug.h"
#include "fontdetection.h"
#include "string_func.h"
#include "strings_func.h"

extern FT_Library _library;

/**
 * Get the font loaded into a Freetype face by using a font-name.
 * If no appropriate font is found, the function returns an error
 */

/* ========================================================================================
 * Windows support
 * ======================================================================================== */

#ifdef WIN32
#include "core/alloc_func.hpp"
#include "core/math_func.hpp"
#include <windows.h>
#include <shlobj.h> /* SHGetFolderPath */
#include "os/windows/win32.h"

#include "safeguards.h"

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
const char *GetShortPath(const TCHAR *long_path)
{
	static char short_path[MAX_PATH];
#ifdef UNICODE
	WCHAR short_path_w[MAX_PATH];
	GetShortPathName(long_path, short_path_w, lengthof(short_path_w));
	WideCharToMultiByte(CP_ACP, 0, short_path_w, -1, short_path, lengthof(short_path), NULL, NULL);
#else
	/* Technically not needed, but do it for consistency. */
	GetShortPathName(long_path, short_path, lengthof(short_path));
#endif
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
#define FONT_DIR_NT "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts"
#define FONT_DIR_9X "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Fonts"
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

	/* On windows NT (2000, NT3.5, XP, etc.) the fonts are stored in the
	 * "Windows NT" key, on Windows 9x in the Windows key. To save us having
	 * to retrieve the windows version, we'll just query both */
	ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T(FONT_DIR_NT), 0, KEY_READ, &hKey);
	if (ret != ERROR_SUCCESS) ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T(FONT_DIR_9X), 0, KEY_READ, &hKey);

	if (ret != ERROR_SUCCESS) {
		DEBUG(freetype, 0, "Cannot open registry key HKLM\\SOFTWARE\\Microsoft\\Windows (NT)\\CurrentVersion\\Fonts");
		return err;
	}

	/* Convert font name to file system encoding. */
	TCHAR *font_namep = _tcsdup(OTTD2FS(font_name));

	for (index = 0;; index++) {
		TCHAR *s;
		DWORD vbuflen = lengthof(vbuffer);
		DWORD dbuflen = lengthof(dbuffer);

		ret = RegEnumValue(hKey, index, vbuffer, &vbuflen, NULL, NULL, (byte*)dbuffer, &dbuflen);
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
		if (s != NULL) s[-1] = '\0';

		if (_tcschr(vbuffer, _T('&')) == NULL) {
			if (_tcsicmp(vbuffer, font_namep) == 0) break;
		} else {
			if (_tcsstr(vbuffer, font_namep) != NULL) break;
		}
	}

	if (!SUCCEEDED(OTTDSHGetFolderPath(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, vbuffer))) {
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
	const char *ret_font_name = NULL;
	uint pos = 0;
	HDC dc;
	HGDIOBJ oldfont;
	byte *buf;
	DWORD dw;
	uint16 format, count, stringOffset, platformId, encodingId, languageId, nameId, length, offset;

	HFONT font = CreateFontIndirect(&logfont->elfLogFont);
	if (font == NULL) goto err1;

	dc = GetDC(NULL);
	oldfont = SelectObject(dc, font);
	dw = GetFontData(dc, 'eman', 0, NULL, 0);
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
		length = min(length, MAX_PATH - 1);
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
	ReleaseDC(NULL, dc);
	DeleteObject(font);
err1:
	return ret_font_name == NULL ? WIDE_TO_MB((const TCHAR*)logfont->elfFullName) : ret_font_name;
}

class FontList {
protected:
	TCHAR **fonts;
	uint items;
	uint capacity;

public:
	FontList() : fonts(NULL), items(0), capacity(0) { };

	~FontList() {
		if (this->fonts == NULL) return;

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
	if (!info->fonts.Add((const TCHAR*)logfont->elfFullName)) return 1;
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
		if (font != NULL) {
			HDC dc = GetDC(NULL);
			HGDIOBJ oldfont = SelectObject(dc, font);
			GetTextCharsetInfo(dc, &fs, 0);
			SelectObject(dc, oldfont);
			ReleaseDC(NULL, dc);
			DeleteObject(font);
		}
		if ((fs.fsCsb[0] & info->locale.lsCsbSupported[0]) == 0 && (fs.fsCsb[1] & info->locale.lsCsbSupported[1]) == 0) return 1;
	}

	char font_name[MAX_PATH];
	convert_from_fs((const TCHAR *)logfont->elfFullName, font_name, lengthof(font_name));

	/* Add english name after font name */
	const char *english_name = GetEnglishFontName(logfont);
	strecpy(font_name + strlen(font_name) + 1, english_name, lastof(font_name));

	/* Check whether we can actually load the font. */
	bool ft_init = _library != NULL;
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
		_library = NULL;
	}

	if (!found) return 1;

	info->callback->SetFontNames(info->settings, font_name);
	if (info->callback->FindMissingGlyphs(NULL)) return 1;
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

	HDC dc = GetDC(NULL);
	int ret = EnumFontFamiliesEx(dc, &font, (FONTENUMPROC)&EnumFontCallback, (LPARAM)&langInfo, 0);
	ReleaseDC(NULL, dc);
	return ret == 0;
}

#elif defined(__APPLE__) /* end ifdef Win32 */
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
	CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault, font_name, kCFStringEncodingUTF8);

#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
	if (MacOSVersionIsAtLeast(10, 6, 0)) {
		/* Simply creating the font using CTFontCreateWithNameAndSize will *always* return
		 * something, no matter the name. As such, we can't use it to check for existance.
		 * We instead query the list of all font descriptors that match the given name which
		 * does not do this stupid name fallback. */
		CTFontDescriptorRef name_desc = CTFontDescriptorCreateWithNameAndSize(name, 0.0);
		CFSetRef mandatory_attribs = CFSetCreate(kCFAllocatorDefault, (const void **)&kCTFontNameAttribute, 1, &kCFTypeSetCallBacks);
		CFArrayRef descs = CTFontDescriptorCreateMatchingFontDescriptors(name_desc, mandatory_attribs);
		CFRelease(mandatory_attribs);
		CFRelease(name_desc);
		CFRelease(name);

		/* Loop over all matches until we can get a path for one of them. */
		for (CFIndex i = 0; descs != NULL && i < CFArrayGetCount(descs) && os_err != noErr; i++) {
			CTFontRef font = CTFontCreateWithFontDescriptor((CTFontDescriptorRef)CFArrayGetValueAtIndex(descs, i), 0.0, NULL);
			CFURLRef fontURL = (CFURLRef)CTFontCopyAttribute(font, kCTFontURLAttribute);
			if (CFURLGetFileSystemRepresentation(fontURL, true, file_path, lengthof(file_path))) os_err = noErr;
			CFRelease(font);
			CFRelease(fontURL);
		}
		if (descs != NULL) CFRelease(descs);
	} else
#endif
	{
#if (MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_6)
		ATSFontRef font = ATSFontFindFromName(name, kATSOptionFlagsDefault);
		CFRelease(name);
		if (font == kInvalidFont) return err;

		/* Get a file system reference for the font. */
		FSRef ref;
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
		if (MacOSVersionIsAtLeast(10, 5, 0)) {
			os_err = ATSFontGetFileReference(font, &ref);
		} else
#endif
		{
#if (MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5) && !defined(__LP64__)
			/* This type was introduced with the 10.5 SDK. */
#if (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_5)
#define ATSFSSpec FSSpec
#endif
			FSSpec spec;
			os_err = ATSFontGetFileSpecification(font, (ATSFSSpec *)&spec);
			if (os_err == noErr) os_err = FSpMakeFSRef(&spec, &ref);
#endif
		}

		/* Get unix path for file. */
		if (os_err == noErr) os_err = FSRefMakePath(&ref, file_path, sizeof(file_path));
#endif
	}

	if (os_err == noErr) {
		DEBUG(freetype, 3, "Font path for %s: %s", font_name, file_path);
		err = FT_New_Face(_library, (const char *)file_path, 0, face);
	}

	return err;
}

bool SetFallbackFont(FreeTypeSettings *settings, const char *language_isocode, int winlangid, MissingGlyphSearcher *callback)
{
	bool result = false;

#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
	if (MacOSVersionIsAtLeast(10, 5, 0)) {
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
			if (sep != NULL) *sep = '\0';
		}

		/* Create a font descriptor matching the wanted language and latin (english) glyphs. */
		CFStringRef lang_codes[2];
		lang_codes[0] = CFStringCreateWithCString(kCFAllocatorDefault, lang, kCFStringEncodingUTF8);
		lang_codes[1] = CFSTR("en");
		CFArrayRef lang_arr = CFArrayCreate(kCFAllocatorDefault, (const void **)lang_codes, lengthof(lang_codes), &kCFTypeArrayCallBacks);
		CFDictionaryRef lang_attribs = CFDictionaryCreate(kCFAllocatorDefault, (const void**)&kCTFontLanguagesAttribute, (const void **)&lang_arr, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CTFontDescriptorRef lang_desc = CTFontDescriptorCreateWithAttributes(lang_attribs);
		CFRelease(lang_arr);
		CFRelease(lang_attribs);
		CFRelease(lang_codes[0]);

		/* Get array of all font descriptors for the wanted language. */
		CFSetRef mandatory_attribs = CFSetCreate(kCFAllocatorDefault, (const void **)&kCTFontLanguagesAttribute, 1, &kCFTypeSetCallBacks);
		CFArrayRef descs = CTFontDescriptorCreateMatchingFontDescriptors(lang_desc, mandatory_attribs);
		CFRelease(mandatory_attribs);
		CFRelease(lang_desc);

		for (CFIndex i = 0; descs != NULL && i < CFArrayGetCount(descs); i++) {
			CTFontDescriptorRef font = (CTFontDescriptorRef)CFArrayGetValueAtIndex(descs, i);

			/* Get font traits. */
			CFDictionaryRef traits = (CFDictionaryRef)CTFontDescriptorCopyAttribute(font, kCTFontTraitsAttribute);
			CTFontSymbolicTraits symbolic_traits;
			CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(traits, kCTFontSymbolicTrait), kCFNumberIntType, &symbolic_traits);
			CFRelease(traits);

			/* Skip symbol fonts and vertical fonts. */
			if ((symbolic_traits & kCTFontClassMaskTrait) == (CTFontStylisticClass)kCTFontSymbolicClass || (symbolic_traits & kCTFontVerticalTrait)) continue;
			/* Skip bold fonts (especially Arial Bold, which looks worse than regular Arial). */
			if (symbolic_traits & kCTFontBoldTrait) continue;
			/* Select monospaced fonts if asked for. */
			if (((symbolic_traits & kCTFontMonoSpaceTrait) == kCTFontMonoSpaceTrait) != callback->Monospace()) continue;

			/* Get font name. */
			char name[128];
			CFStringRef font_name = (CFStringRef)CTFontDescriptorCopyAttribute(font, kCTFontDisplayNameAttribute);
			CFStringGetCString(font_name, name, lengthof(name), kCFStringEncodingUTF8);
			CFRelease(font_name);

			/* There are some special fonts starting with an '.' and the last
			 * resort font that aren't usable. Skip them. */
			if (name[0] == '.' || strncmp(name, "LastResort", 10) == 0) continue;

			/* Save result. */
			callback->SetFontNames(settings, name);
			if (!callback->FindMissingGlyphs(NULL)) {
				DEBUG(freetype, 2, "CT-Font for %s: %s", language_isocode, name);
				result = true;
				break;
			}
		}
		if (descs != NULL) CFRelease(descs);
	} else
#endif
	{
#if (MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_6)
		/* Create a font iterator and iterate over all fonts that
		 * are available to the application. */
		ATSFontIterator itr;
		ATSFontRef font;
		ATSFontIteratorCreate(kATSFontContextLocal, NULL, NULL, kATSOptionFlagsDefaultScope, &itr);
		while (!result && ATSFontIteratorNext(itr, &font) == noErr) {
			/* Get font name. */
			char name[128];
			CFStringRef font_name;
			ATSFontGetName(font, kATSOptionFlagsDefault, &font_name);
			CFStringGetCString(font_name, name, lengthof(name), kCFStringEncodingUTF8);

			bool monospace = IsMonospaceFont(font_name);
			CFRelease(font_name);

			/* Select monospaced fonts if asked for. */
			if (monospace != callback->Monospace()) continue;

			/* We only want the base font and not bold or italic variants. */
			if (strstr(name, "Italic") != NULL || strstr(name, "Bold")) continue;

			/* Skip some inappropriate or ugly looking fonts that have better alternatives. */
			if (name[0] == '.' || strncmp(name, "Apple Symbols", 13) == 0 || strncmp(name, "LastResort", 10) == 0) continue;

			/* Save result. */
			callback->SetFontNames(settings, name);
			if (!callback->FindMissingGlyphs(NULL)) {
				DEBUG(freetype, 2, "ATS-Font for %s: %s", language_isocode, name);
				result = true;
				break;
			}
		}
		ATSFontIteratorRelease(&itr);
#endif
	}

	if (!result) {
		/* For some OS versions, the font 'Arial Unicode MS' does not report all languages it
		 * supports. If we didn't find any other font, just try it, maybe we get lucky. */
		callback->SetFontNames(settings, "Arial Unicode MS");
		result = !callback->FindMissingGlyphs(NULL);
	}

	callback->FindMissingGlyphs(NULL);
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
		if (font_style != NULL) {
			font_style[0] = '\0';
			font_style++;
			while (*font_style == ' ' || *font_style == '\t') font_style++;
		}

		/* Resolve the name and populate the information structure */
		pat = FcNameParse((FcChar8*)font_family);
		if (font_style != NULL) FcPatternAddString(pat, FC_STYLE, (FcChar8*)font_style);
		FcConfigSubstitute(0, pat, FcMatchPattern);
		FcDefaultSubstitute(pat);
		fs = FcFontSetCreate();
		match = FcFontMatch(0, pat, &result);

		if (fs != NULL && match != NULL) {
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
					if (font_style != NULL && strcasecmp(font_style, (char*)style) != 0) continue;

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
	if (split != NULL) *split = '\0';

	/* First create a pattern to match the wanted language. */
	FcPattern *pat = FcNameParse((FcChar8*)lang);
	/* We only want to know the filename. */
	FcObjectSet *os = FcObjectSetBuild(FC_FILE, FC_SPACING, FC_SLANT, FC_WEIGHT, NULL);
	/* Get the list of filenames matching the wanted language. */
	FcFontSet *fs = FcFontList(NULL, pat, os);

	/* We don't need these anymore. */
	FcObjectSetDestroy(os);
	FcPatternDestroy(pat);

	if (fs != NULL) {
		int best_weight = -1;
		const char *best_font = NULL;

		for (int i = 0; i < fs->nfont; i++) {
			FcPattern *font = fs->fonts[i];

			FcChar8 *file = NULL;
			FcResult res = FcPatternGetString(font, FC_FILE, 0, &file);
			if (res != FcResultMatch || file == NULL) {
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

			bool missing = callback->FindMissingGlyphs(NULL);
			DEBUG(freetype, 1, "Font \"%s\" misses%s glyphs", file, missing ? "" : " no");

			if (!missing) {
				best_weight = value;
				best_font   = (const char *)file;
			}
		}

		if (best_font != NULL) {
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

#else /* without WITH_FONTCONFIG */
FT_Error GetFontByFaceName(const char *font_name, FT_Face *face) {return FT_Err_Cannot_Open_Resource;}
bool SetFallbackFont(FreeTypeSettings *settings, const char *language_isocode, int winlangid, MissingGlyphSearcher *callback) { return false; }
#endif /* WITH_FONTCONFIG */

#endif /* WITH_FREETYPE */
