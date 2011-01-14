/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fontcache.cpp Cache for characters from fonts. */

#include "stdafx.h"
#include "fontcache.h"
#include "blitter/factory.hpp"
#include "core/math_func.hpp"

#include "table/sprites.h"
#include "table/control_codes.h"

static const int ASCII_LETTERSTART = 32; ///< First printable ASCII letter.

/** Semi-constant for the height of the different sizes of fonts. */
int _font_height[FS_END];

#ifdef WITH_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#ifdef WITH_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

static FT_Library _library = NULL;
static FT_Face _face_small = NULL;
static FT_Face _face_medium = NULL;
static FT_Face _face_large = NULL;
static int _ascender[FS_END];

FreeTypeSettings _freetype;

static const byte FACE_COLOUR   = 1;
static const byte SHADOW_COLOUR = 2;

/**
 * Get the font loaded into a Freetype face by using a font-name.
 * If no appropiate font is found, the function returns an error
 */

/* ========================================================================================
 * Windows support
 * ======================================================================================== */

#ifdef WIN32
#include <windows.h>
#include <shlobj.h> /* SHGetFolderPath */
#include "os/windows/win32.h"

/**
 * Get the short DOS 8.3 format for paths.
 * FreeType doesn't support Unicode filenames and Windows' fopen (as used
 * by FreeType) doesn't support UTF-8 filenames. So we have to convert the
 * filename into something that isn't UTF-8 but represents the Unicode file
 * name. This is the short DOS 8.3 format. This does not contain any
 * characters that fopen doesn't support.
 * @param long_path the path in UTF-8.
 * @return the short path in ANSI (ASCII).
 */
char *GetShortPath(const char *long_path)
{
	static char short_path[MAX_PATH];
#ifdef UNICODE
	/* The non-unicode GetShortPath doesn't support UTF-8...,
	 * so convert the path to wide chars, then get the short
	 * path and convert it back again. */
	wchar_t long_path_w[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, long_path, -1, long_path_w, MAX_PATH);

	wchar_t short_path_w[MAX_PATH];
	GetShortPathNameW(long_path_w, short_path_w, MAX_PATH);

	WideCharToMultiByte(CP_ACP, 0, short_path_w, -1, short_path, MAX_PATH, NULL, NULL);
#else
	/* Technically not needed, but do it for consistency. */
	GetShortPathNameA(long_path, short_path, MAX_PATH);
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
static FT_Error GetFontByFaceName(const char *font_name, FT_Face *face)
{
	FT_Error err = FT_Err_Cannot_Open_Resource;
	HKEY hKey;
	LONG ret;
	TCHAR vbuffer[MAX_PATH], dbuffer[256];
	TCHAR *font_namep;
	char *font_path;
	uint index;

	/* On windows NT (2000, NT3.5, XP, etc.) the fonts are stored in the
	 * "Windows NT" key, on Windows 9x in the Windows key. To save us having
	 * to retrieve the windows version, we'll just query both */
	ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T(FONT_DIR_NT), 0, KEY_READ, &hKey);
	if (ret != ERROR_SUCCESS) ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T(FONT_DIR_9X), 0, KEY_READ, &hKey);

	if (ret != ERROR_SUCCESS) {
		DEBUG(freetype, 0, "Cannot open registry key HKLM\\SOFTWARE\\Microsoft\\Windows (NT)\\CurrentVersion\\Fonts");
		return err;
	}

	/* For Unicode we need some conversion between widechar and
	 * normal char to match the data returned by RegEnumValue,
	 * otherwise just use parameter */
#if defined(UNICODE)
	font_namep = MallocT<TCHAR>(MAX_PATH);
	MB_TO_WIDE_BUFFER(font_name, font_namep, MAX_PATH * sizeof(TCHAR));
#else
	font_namep = const_cast<char *>(font_name); // only cast because in unicode pointer is not const
#endif

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
		 * TTC files, font files which contain more than one font are seperated
		 * byt '&'. Our best bet will be to do substr match for the fontname
		 * and then let FreeType figure out which index to load */
		s = _tcschr(vbuffer, _T('('));
		if (s != NULL) s[-1] = '\0';

		if (_tcschr(vbuffer, _T('&')) == NULL) {
			if (_tcsicmp(vbuffer, font_namep) == 0) break;
		} else {
			if (_tcsstr(vbuffer, font_namep) != NULL) break;
		}
	}

	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, vbuffer))) {
		DEBUG(freetype, 0, "SHGetFolderPath cannot return fonts directory");
		goto folder_error;
	}

	/* Some fonts are contained in .ttc files, TrueType Collection fonts. These
	 * contain multiple fonts inside this single file. GetFontData however
	 * returns the whole file, so we need to check each font inside to get the
	 * proper font.
	 * Also note that FreeType does not support UNICODE filesnames! */
#if defined(UNICODE)
	/* We need a cast here back from wide because FreeType doesn't support
	 * widechar filenames. Just use the buffer we allocated before for the
	 * font_name search */
	font_path = (char*)font_namep;
	WIDE_TO_MB_BUFFER(vbuffer, font_path, MAX_PATH * sizeof(TCHAR));
#else
	font_path = vbuffer;
#endif

	ttd_strlcat(font_path, "\\", MAX_PATH * sizeof(TCHAR));
	ttd_strlcat(font_path, WIDE_TO_MB(dbuffer), MAX_PATH * sizeof(TCHAR));

	/* Convert the path into something that FreeType understands */
	font_path = GetShortPath(font_path);

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
#if defined(UNICODE)
	free(font_namep);
#endif
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

struct EFCParam {
	FreeTypeSettings *settings;
	LOCALESIGNATURE  locale;
	SetFallbackFontCallback *callback;
};

static int CALLBACK EnumFontCallback(const ENUMLOGFONTEX *logfont, const NEWTEXTMETRICEX *metric, DWORD type, LPARAM lParam)
{
	EFCParam *info = (EFCParam *)lParam;

	/* Only use TrueType fonts */
	if (!(type & TRUETYPE_FONTTYPE)) return 1;
	/* Don't use SYMBOL fonts */
	if (logfont->elfLogFont.lfCharSet == SYMBOL_CHARSET) return 1;

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
#if defined(UNICODE)
	WIDE_TO_MB_BUFFER((const TCHAR*)logfont->elfFullName, font_name, lengthof(font_name));
#else
	strecpy(font_name, (const TCHAR*)logfont->elfFullName, lastof(font_name));
#endif

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

	strecpy(info->settings->small_font,  font_name, lastof(info->settings->small_font));
	strecpy(info->settings->medium_font, font_name, lastof(info->settings->medium_font));
	strecpy(info->settings->large_font,  font_name, lastof(info->settings->large_font));
	if (info->callback(NULL)) return 1;
	DEBUG(freetype, 1, "Fallback font: %s (%s)", font_name, english_name);
	return 0; // stop enumerating
}

bool SetFallbackFont(FreeTypeSettings *settings, const char *language_isocode, int winlangid, SetFallbackFontCallback *callback)
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
#include <ApplicationServices/ApplicationServices.h>

FT_Error GetFontByFaceName(const char *font_name, FT_Face *face)
{
	FT_Error err = FT_Err_Cannot_Open_Resource;

	/* Get font reference from name. */
	CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault, font_name, kCFStringEncodingUTF8);
	ATSFontRef font = ATSFontFindFromName(name, kATSOptionFlagsDefault);
	CFRelease(name);
	if (font == kInvalidFont) return err;

	/* Get a file system reference for the font. */
	FSRef ref;
	OSStatus os_err = -1;
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
	if (MacOSVersionIsAtLeast(10, 5, 0)) {
		os_err = ATSFontGetFileReference(font, &ref);
	} else
#endif
	{
#if (MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5) && !__LP64__
		/* This type was introduced with the 10.5 SDK. */
#if (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_5)
	#define ATSFSSpec FSSpec
#endif
		FSSpec spec;
		os_err = ATSFontGetFileSpecification(font, (ATSFSSpec *)&spec);
		if (os_err == noErr) os_err = FSpMakeFSRef(&spec, &ref);
#endif
	}

	if (os_err == noErr) {
		/* Get unix path for file. */
		UInt8 file_path[PATH_MAX];
		if (FSRefMakePath(&ref, file_path, sizeof(file_path)) == noErr) {
			DEBUG(freetype, 3, "Font path for %s: %s", font_name, file_path);
			err = FT_New_Face(_library, (const char *)file_path, 0, face);
		}
	}

	return err;
}

bool SetFallbackFont(FreeTypeSettings *settings, const char *language_isocode, int winlangid, SetFallbackFontCallback *callback)
{
	const char *str;
	bool result = false;

	callback(&str);

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
		} else if (strncmp(language_isocode, "ur", 2) == 0) {
			/* The urdu alphabet is variant of persian. As OS X has no default
			 * font that advertises an urdu language code, search for persian
			 * support instead. */
			strecpy(lang, "fa", lastof(lang));
		} else {
			/* Just copy the first part of the isocode. */
			strecpy(lang, language_isocode, lastof(lang));
			char *sep = strchr(lang, '_');
			if (sep != NULL) *sep = '\0';
		}

		CFStringRef lang_code;
		lang_code = CFStringCreateWithCString(kCFAllocatorDefault, lang, kCFStringEncodingUTF8);

		/* Create a font iterator and iterate over all fonts that
		 * are available to the application. */
		ATSFontIterator itr;
		ATSFontRef font;
		ATSFontIteratorCreate(kATSFontContextLocal, NULL, NULL, kATSOptionFlagsUnRestrictedScope, &itr);
		while (!result && ATSFontIteratorNext(itr, &font) == noErr) {
			/* Get CoreText font handle. */
			CTFontRef font_ref = CTFontCreateWithPlatformFont(font, 0.0, NULL, NULL);
			CFArrayRef langs = CTFontCopySupportedLanguages(font_ref);
			if (langs != NULL) {
				/* Font has a list of supported languages. */
				for (CFIndex i = 0; i < CFArrayGetCount(langs); i++) {
					CFStringRef lang = (CFStringRef)CFArrayGetValueAtIndex(langs, i);
					if (CFStringCompare(lang, lang_code, kCFCompareAnchored) == kCFCompareEqualTo) {
						/* Lang code is supported by font, get full font name. */
						CFStringRef font_name = CTFontCopyFullName(font_ref);
						char name[128];
						CFStringGetCString(font_name, name, lengthof(name), kCFStringEncodingUTF8);
						CFRelease(font_name);
						/* Skip some inappropriate or ugly looking fonts that have better alternatives. */
						if (strncmp(name, "Courier", 7) == 0 || strncmp(name, "Apple Symbols", 13) == 0 ||
								strncmp(name, ".Aqua", 5) == 0 || strncmp(name, "LastResort", 10) == 0 ||
								strncmp(name, "GB18030 Bitmap", 14) == 0) continue;

						/* Save result. */
						strecpy(settings->small_font,  name, lastof(settings->small_font));
						strecpy(settings->medium_font, name, lastof(settings->medium_font));
						strecpy(settings->large_font,  name, lastof(settings->large_font));
						DEBUG(freetype, 2, "CT-Font for %s: %s", language_isocode, name);
						result = true;
						break;
					}
				}
				CFRelease(langs);
			}
			CFRelease(font_ref);
		}
		ATSFontIteratorRelease(&itr);
		CFRelease(lang_code);
	} else
#endif
	{
#if (MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5) && !__LP64__
		/* Determine fallback font using ATSUI. This uses a string sample with
		 * missing characters. This is not failure-proof, but a better way like
		 * using the isocode as in the CoreText code path is not available.
		 * ATSUI was deprecated with 10.6 and is only partially available in
		 * 64-bit mode. */

		/* Remove all control characters in the range from SCC_CONTROL_START to
		 * SCC_CONTROL_END as well as all ASCII < 0x20 from the string as it will
		 * mess with the automatic font detection */
		char buff[256]; // This length is enough to find a suitable replacement font
		strecpy(buff, str, lastof(buff));
		str_validate(buff, lastof(buff), true, false);

		/* Extract a UniChar represenation of the sample string. */
		CFStringRef cf_str = CFStringCreateWithCString(kCFAllocatorDefault, buff, kCFStringEncodingUTF8);
		if (cf_str == NULL) {
			/* Something went wrong. Corrupt/invalid sample string? */
			return false;
		}
		CFIndex str_len = CFStringGetLength(cf_str);
		UniChar string[str_len];
		CFStringGetCharacters(cf_str, CFRangeMake(0, str_len), string);

		/* Create a default text style with the default font. */
		ATSUStyle style;
		ATSUCreateStyle(&style);

		/* Create a text layout object from the sample string using the text style. */
		UniCharCount run_len = kATSUToTextEnd;
		ATSUTextLayout text_layout;
		ATSUCreateTextLayoutWithTextPtr(string, kATSUFromTextBeginning, kATSUToTextEnd, str_len, 1, &run_len, &style, &text_layout);

		/* Try to match a font for the sample text. ATSUMatchFontsToText stops after
		 * it finds the first continous character run not renderable with the currently
		 * selected font starting at offset. The matching needs to be repeated until
		 * the end of the string is reached to make sure the fallback font matches for
		 * all characters in the string and not only the first run. */
		UniCharArrayOffset offset = kATSUFromTextBeginning;
		OSStatus os_err;
		do {
			ATSUFontID font;
			UniCharCount run_len;
			os_err = ATSUMatchFontsToText(text_layout, offset, kATSUToTextEnd, &font, &offset, &run_len);
			if (os_err == kATSUFontsMatched) {
				/* Found a better fallback font. Update the text layout
				 * object with the new font. */
				ATSUAttributeTag tag = kATSUFontTag;
				ByteCount size = sizeof(font);
				ATSUAttributeValuePtr val = &font;
				ATSUSetAttributes(style, 1, &tag, &size, &val);
				offset += run_len;
			}
			/* Exit if the end of the string is reached or some other error occurred. */
		} while (os_err == kATSUFontsMatched && offset < (UniCharArrayOffset)str_len);

		if (os_err == noErr || os_err == kATSUFontsMatched) {
			/* ATSUMatchFontsToText exited normally. Extract font
			 * out of the text layout object. */
			ATSUFontID font;
			ByteCount act_len;
			ATSUGetAttribute(style, kATSUFontTag, sizeof(font), &font, &act_len);

			/* Get unique font name. The result is not a c-string, we have
			 * to leave space for a \0 and terminate it ourselves. */
			char name[128];
			ATSUFindFontName(font, kFontUniqueName, kFontNoPlatformCode, kFontNoScriptCode, kFontNoLanguageCode, 127, name, &act_len, NULL);
			name[act_len > 127 ? 127 : act_len] = '\0';

			/* Save Result. */
			strecpy(settings->small_font,  name, lastof(settings->small_font));
			strecpy(settings->medium_font, name, lastof(settings->medium_font));
			strecpy(settings->large_font,  name, lastof(settings->large_font));
			DEBUG(freetype, 2, "ATSUI-Font for %s: %s", language_isocode, name);
			result = true;
		}

		ATSUDisposeTextLayout(text_layout);
		ATSUDisposeStyle(style);
		CFRelease(cf_str);
#endif
	}

	if (result && strncmp(settings->medium_font, "Geeza Pro", 9) == 0) {
		/* The font 'Geeza Pro' is often found for arabic characters, but
		 * it has the 'tiny' problem of not having any latin characters.
		 * 'Arial Unicode MS' on the other hand has arabic and latin glyphs,
		 * but seems to 'forget' to inform the OS about this fact. Manually
		 * substitute the latter for the former if it is loadable. */
		bool ft_init = _library != NULL;
		FT_Face face;
		/* Init FreeType if needed. */
		if ((ft_init || FT_Init_FreeType(&_library) == FT_Err_Ok) && GetFontByFaceName("Arial Unicode MS", &face) == FT_Err_Ok) {
			FT_Done_Face(face);
			strecpy(settings->small_font,  "Arial Unicode MS", lastof(settings->small_font));
			strecpy(settings->medium_font, "Arial Unicode MS", lastof(settings->medium_font));
			strecpy(settings->large_font,  "Arial Unicode MS", lastof(settings->large_font));
			DEBUG(freetype, 1, "Replacing font 'Geeza Pro' with 'Arial Unicode MS'");
		}
		if (!ft_init) {
			/* Uninit FreeType if we did the init. */
			FT_Done_FreeType(_library);
			_library = NULL;
		}
	 }

	callback(NULL);
	return result;
}

#elif defined(WITH_FONTCONFIG) /* end ifdef __APPLE__ */
/* ========================================================================================
 * FontConfig (unix) support
 * ======================================================================================== */
static FT_Error GetFontByFaceName(const char *font_name, FT_Face *face)
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
		font_family = strdup(font_name);
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

bool SetFallbackFont(FreeTypeSettings *settings, const char *language_isocode, int winlangid, SetFallbackFontCallback *callback)
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
	FcObjectSet *os = FcObjectSetBuild(FC_FILE, NULL);
	/* Get the list of filenames matching the wanted language. */
	FcFontSet *fs = FcFontList(NULL, pat, os);

	/* We don't need these anymore. */
	FcObjectSetDestroy(os);
	FcPatternDestroy(pat);

	if (fs != NULL) {
		for (int i = 0; i < fs->nfont; i++) {
			FcPattern *font = fs->fonts[i];

			FcChar8 *file = NULL;
			FcResult res = FcPatternGetString(font, FC_FILE, 0, &file);
			if (res != FcResultMatch || file == NULL) {
				continue;
			}

			strecpy(settings->small_font,  (const char*)file, lastof(settings->small_font));
			strecpy(settings->medium_font, (const char*)file, lastof(settings->medium_font));
			strecpy(settings->large_font,  (const char*)file, lastof(settings->large_font));

			bool missing = callback(NULL);
			DEBUG(freetype, 1, "Font \"%s\" misses%s glyphs", file, missing ? "" : " no");

			if (!missing) {
				ret = true;
				break;
			}
		}

		/* Clean up the list of filenames. */
		FcFontSetDestroy(fs);
	}

	FcFini();
	return ret;
}

#else /* without WITH_FONTCONFIG */
FT_Error GetFontByFaceName(const char *font_name, FT_Face *face) {return FT_Err_Cannot_Open_Resource;}
bool SetFallbackFont(FreeTypeSettings *settings, const char *language_isocode, int winlangid, SetFallbackFontCallback *callback) { return false; }
#endif /* WITH_FONTCONFIG */

static void SetFontGeometry(FT_Face face, FontSize size, int pixels)
{
	FT_Set_Pixel_Sizes(face, 0, pixels);

	if (FT_IS_SCALABLE(face)) {
		int asc = face->ascender * pixels / face->units_per_EM;
		int dec = face->descender * pixels / face->units_per_EM;

		_ascender[size] = asc;
		_font_height[size] = asc - dec;
	} else {
		_ascender[size] = pixels;
		_font_height[size] = pixels;
	}
}

/**
 * Loads the freetype font.
 * First type to load the fontname as if it were a path. If that fails,
 * try to resolve the filename of the font using fontconfig, where the
 * format is 'font family name' or 'font family name, font style'.
 */
static void LoadFreeTypeFont(const char *font_name, FT_Face *face, const char *type)
{
	FT_Error error;

	if (StrEmpty(font_name)) return;

	error = FT_New_Face(_library, font_name, 0, face);

	if (error != FT_Err_Ok) error = GetFontByFaceName(font_name, face);

	if (error == FT_Err_Ok) {
		DEBUG(freetype, 2, "Requested '%s', using '%s %s'", font_name, (*face)->family_name, (*face)->style_name);

		/* Attempt to select the unicode character map */
		error = FT_Select_Charmap(*face, ft_encoding_unicode);
		if (error == FT_Err_Ok) return; // Success

		if (error == FT_Err_Invalid_CharMap_Handle) {
			/* Try to pick a different character map instead. We default to
			 * the first map, but platform_id 0 encoding_id 0 should also
			 * be unicode (strange system...) */
			FT_CharMap found = (*face)->charmaps[0];
			int i;

			for (i = 0; i < (*face)->num_charmaps; i++) {
				FT_CharMap charmap = (*face)->charmaps[i];
				if (charmap->platform_id == 0 && charmap->encoding_id == 0) {
					found = charmap;
				}
			}

			if (found != NULL) {
				error = FT_Set_Charmap(*face, found);
				if (error == FT_Err_Ok) return;
			}
		}
	}

	FT_Done_Face(*face);
	*face = NULL;

	ShowInfoF("Unable to use '%s' for %s font, FreeType reported error 0x%X, using sprite font instead", font_name, type, error);
}


void InitFreeType()
{
	ResetFontSizes();

	if (StrEmpty(_freetype.small_font) && StrEmpty(_freetype.medium_font) && StrEmpty(_freetype.large_font)) {
		DEBUG(freetype, 1, "No font faces specified, using sprite fonts instead");
		return;
	}

	if (FT_Init_FreeType(&_library) != FT_Err_Ok) {
		ShowInfoF("Unable to initialize FreeType, using sprite fonts instead");
		return;
	}

	DEBUG(freetype, 2, "Initialized");

	/* Load each font */
	LoadFreeTypeFont(_freetype.small_font,  &_face_small,  "small");
	LoadFreeTypeFont(_freetype.medium_font, &_face_medium, "medium");
	LoadFreeTypeFont(_freetype.large_font,  &_face_large,  "large");

	/* Set each font size */
	if (_face_small != NULL) {
		SetFontGeometry(_face_small, FS_SMALL, _freetype.small_size);
	}
	if (_face_medium != NULL) {
		SetFontGeometry(_face_medium, FS_NORMAL, _freetype.medium_size);
	}
	if (_face_large != NULL) {
		SetFontGeometry(_face_large, FS_LARGE, _freetype.large_size);
	}
}

static void ResetGlyphCache();

/**
 * Unload a face and set it to NULL.
 * @param face the face to unload
 */
static void UnloadFace(FT_Face *face)
{
	if (*face == NULL) return;

	FT_Done_Face(*face);
	*face = NULL;
}

/**
 * Free everything allocated w.r.t. fonts.
 */
void UninitFreeType()
{
	ResetFontSizes();
	ResetGlyphCache();

	UnloadFace(&_face_small);
	UnloadFace(&_face_medium);
	UnloadFace(&_face_large);

	FT_Done_FreeType(_library);
	_library = NULL;
}


static FT_Face GetFontFace(FontSize size)
{
	switch (size) {
		default: NOT_REACHED();
		case FS_NORMAL: return _face_medium;
		case FS_SMALL:  return _face_small;
		case FS_LARGE:  return _face_large;
	}
}


struct GlyphEntry {
	Sprite *sprite;
	byte width;
	bool duplicate;
};


/* The glyph cache. This is structured to reduce memory consumption.
 * 1) There is a 'segment' table for each font size.
 * 2) Each segment table is a discrete block of characters.
 * 3) Each block contains 256 (aligned) characters sequential characters.
 *
 * The cache is accessed in the following way:
 * For character 0x0041  ('A'): _glyph_ptr[FS_NORMAL][0x00][0x41]
 * For character 0x20AC (Euro): _glyph_ptr[FS_NORMAL][0x20][0xAC]
 *
 * Currently only 256 segments are allocated, "limiting" us to 65536 characters.
 * This can be simply changed in the two functions Get & SetGlyphPtr.
 */
static GlyphEntry **_glyph_ptr[FS_END];

/** Clear the complete cache */
static void ResetGlyphCache()
{
	for (FontSize i = FS_BEGIN; i < FS_END; i++) {
		if (_glyph_ptr[i] == NULL) continue;

		for (int j = 0; j < 256; j++) {
			if (_glyph_ptr[i][j] == NULL) continue;

			for (int k = 0; k < 256; k++) {
				if (_glyph_ptr[i][j][k].duplicate) continue;
				free(_glyph_ptr[i][j][k].sprite);
			}

			free(_glyph_ptr[i][j]);
		}

		free(_glyph_ptr[i]);
		_glyph_ptr[i] = NULL;
	}
}

static GlyphEntry *GetGlyphPtr(FontSize size, WChar key)
{
	if (_glyph_ptr[size] == NULL) return NULL;
	if (_glyph_ptr[size][GB(key, 8, 8)] == NULL) return NULL;
	return &_glyph_ptr[size][GB(key, 8, 8)][GB(key, 0, 8)];
}


static void SetGlyphPtr(FontSize size, WChar key, const GlyphEntry *glyph, bool duplicate = false)
{
	if (_glyph_ptr[size] == NULL) {
		DEBUG(freetype, 3, "Allocating root glyph cache for size %u", size);
		_glyph_ptr[size] = CallocT<GlyphEntry*>(256);
	}

	if (_glyph_ptr[size][GB(key, 8, 8)] == NULL) {
		DEBUG(freetype, 3, "Allocating glyph cache for range 0x%02X00, size %u", GB(key, 8, 8), size);
		_glyph_ptr[size][GB(key, 8, 8)] = CallocT<GlyphEntry>(256);
	}

	DEBUG(freetype, 4, "Set glyph for unicode character 0x%04X, size %u", key, size);
	_glyph_ptr[size][GB(key, 8, 8)][GB(key, 0, 8)].sprite    = glyph->sprite;
	_glyph_ptr[size][GB(key, 8, 8)][GB(key, 0, 8)].width     = glyph->width;
	_glyph_ptr[size][GB(key, 8, 8)][GB(key, 0, 8)].duplicate = duplicate;
}

static void *AllocateFont(size_t size)
{
	return MallocT<byte>(size);
}


/* Check if a glyph should be rendered with antialiasing */
static bool GetFontAAState(FontSize size)
{
	/* AA is only supported for 32 bpp */
	if (BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth() != 32) return false;

	switch (size) {
		default: NOT_REACHED();
		case FS_NORMAL: return _freetype.medium_aa;
		case FS_SMALL:  return _freetype.small_aa;
		case FS_LARGE:  return _freetype.large_aa;
	}
}


const Sprite *GetGlyph(FontSize size, WChar key)
{
	FT_Face face = GetFontFace(size);
	FT_GlyphSlot slot;
	GlyphEntry new_glyph;
	GlyphEntry *glyph;
	SpriteLoader::Sprite sprite;
	int width;
	int height;
	int x;
	int y;

	assert(IsPrintable(key));

	/* Bail out if no face loaded, or for our special characters */
	if (face == NULL || (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END)) {
		SpriteID sprite = GetUnicodeGlyph(size, key);
		if (sprite == 0) sprite = GetUnicodeGlyph(size, '?');
		return GetSprite(sprite, ST_FONT);
	}

	/* Check for the glyph in our cache */
	glyph = GetGlyphPtr(size, key);
	if (glyph != NULL && glyph->sprite != NULL) return glyph->sprite;

	slot = face->glyph;

	bool aa = GetFontAAState(size);

	FT_UInt glyph_index = FT_Get_Char_Index(face, key);
	if (glyph_index == 0) {
		if (key == '?') {
			/* The font misses the '?' character. Use sprite font. */
			SpriteID sprite = GetUnicodeGlyph(size, key);
			Sprite *spr = (Sprite*)GetRawSprite(sprite, ST_FONT, AllocateFont);
			assert(spr != NULL);
			new_glyph.sprite = spr;
			new_glyph.width  = spr->width + (size != FS_NORMAL);
			SetGlyphPtr(size, key, &new_glyph, false);
			return new_glyph.sprite;
		} else {
			/* Use '?' for missing characters. */
			GetGlyph(size, '?');
			glyph = GetGlyphPtr(size, '?');
			SetGlyphPtr(size, key, glyph, true);
			return glyph->sprite;
		}
	}
	FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
	FT_Render_Glyph(face->glyph, aa ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO);

	/* Despite requesting a normal glyph, FreeType may have returned a bitmap */
	aa = (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);

	/* Add 1 pixel for the shadow on the medium font. Our sprite must be at least 1x1 pixel */
	width  = max(1, slot->bitmap.width + (size == FS_NORMAL));
	height = max(1, slot->bitmap.rows  + (size == FS_NORMAL));

	/* FreeType has rendered the glyph, now we allocate a sprite and copy the image into it */
	sprite.AllocateData(width * height);
	sprite.width = width;
	sprite.height = height;
	sprite.x_offs = slot->bitmap_left;
	sprite.y_offs = _ascender[size] - slot->bitmap_top;

	/* Draw shadow for medium size */
	if (size == FS_NORMAL) {
		for (y = 0; y < slot->bitmap.rows; y++) {
			for (x = 0; x < slot->bitmap.width; x++) {
				if (aa ? (slot->bitmap.buffer[x + y * slot->bitmap.pitch] > 0) : HasBit(slot->bitmap.buffer[(x / 8) + y * slot->bitmap.pitch], 7 - (x % 8))) {
					sprite.data[1 + x + (1 + y) * sprite.width].m = SHADOW_COLOUR;
					sprite.data[1 + x + (1 + y) * sprite.width].a = aa ? slot->bitmap.buffer[x + y * slot->bitmap.pitch] : 0xFF;
				}
			}
		}
	}

	for (y = 0; y < slot->bitmap.rows; y++) {
		for (x = 0; x < slot->bitmap.width; x++) {
			if (aa ? (slot->bitmap.buffer[x + y * slot->bitmap.pitch] > 0) : HasBit(slot->bitmap.buffer[(x / 8) + y * slot->bitmap.pitch], 7 - (x % 8))) {
				sprite.data[x + y * sprite.width].m = FACE_COLOUR;
				sprite.data[x + y * sprite.width].a = aa ? slot->bitmap.buffer[x + y * slot->bitmap.pitch] : 0xFF;
			}
		}
	}

	new_glyph.sprite = BlitterFactoryBase::GetCurrentBlitter()->Encode(&sprite, AllocateFont);
	new_glyph.width  = slot->advance.x >> 6;

	SetGlyphPtr(size, key, &new_glyph);

	return new_glyph.sprite;
}


uint GetGlyphWidth(FontSize size, WChar key)
{
	FT_Face face = GetFontFace(size);
	GlyphEntry *glyph;

	if (face == NULL || (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END)) {
		SpriteID sprite = GetUnicodeGlyph(size, key);
		if (sprite == 0) sprite = GetUnicodeGlyph(size, '?');
		return SpriteExists(sprite) ? GetSprite(sprite, ST_FONT)->width + (size != FS_NORMAL) : 0;
	}

	glyph = GetGlyphPtr(size, key);
	if (glyph == NULL || glyph->sprite == NULL) {
		GetGlyph(size, key);
		glyph = GetGlyphPtr(size, key);
	}

	return glyph->width;
}


#endif /* WITH_FREETYPE */

/** Reset the font sizes to the defaults of the sprite based fonts. */
void ResetFontSizes()
{
	_font_height[FS_SMALL]  =  6;
	_font_height[FS_NORMAL] = 10;
	_font_height[FS_LARGE]  = 18;
}

/* Sprite based glyph mapping */

#include "table/unicode.h"

static SpriteID **_unicode_glyph_map[FS_END];


/** Get the SpriteID of the first glyph for the given font size */
static SpriteID GetFontBase(FontSize size)
{
	switch (size) {
		default: NOT_REACHED();
		case FS_NORMAL: return SPR_ASCII_SPACE;
		case FS_SMALL:  return SPR_ASCII_SPACE_SMALL;
		case FS_LARGE:  return SPR_ASCII_SPACE_BIG;
	}
}


SpriteID GetUnicodeGlyph(FontSize size, uint32 key)
{
	if (_unicode_glyph_map[size][GB(key, 8, 8)] == NULL) return 0;
	return _unicode_glyph_map[size][GB(key, 8, 8)][GB(key, 0, 8)];
}


void SetUnicodeGlyph(FontSize size, uint32 key, SpriteID sprite)
{
	if (_unicode_glyph_map[size] == NULL) _unicode_glyph_map[size] = CallocT<SpriteID*>(256);
	if (_unicode_glyph_map[size][GB(key, 8, 8)] == NULL) _unicode_glyph_map[size][GB(key, 8, 8)] = CallocT<SpriteID>(256);
	_unicode_glyph_map[size][GB(key, 8, 8)][GB(key, 0, 8)] = sprite;
}


void InitializeUnicodeGlyphMap()
{
	for (FontSize size = FS_BEGIN; size != FS_END; size++) {
		/* Clear out existing glyph map if it exists */
		if (_unicode_glyph_map[size] != NULL) {
			for (uint i = 0; i < 256; i++) {
				free(_unicode_glyph_map[size][i]);
			}
			free(_unicode_glyph_map[size]);
			_unicode_glyph_map[size] = NULL;
		}

		SpriteID base = GetFontBase(size);

		for (uint i = ASCII_LETTERSTART; i < 256; i++) {
			SpriteID sprite = base + i - ASCII_LETTERSTART;
			if (!SpriteExists(sprite)) continue;
			SetUnicodeGlyph(size, i, sprite);
			SetUnicodeGlyph(size, i + SCC_SPRITE_START, sprite);
		}

		for (uint i = 0; i < lengthof(_default_unicode_map); i++) {
			byte key = _default_unicode_map[i].key;
			if (key == CLRA || key == CLRL) {
				/* Clear the glyph. This happens if the glyph at this code point
				 * is non-standard and should be accessed by an SCC_xxx enum
				 * entry only. */
				if (key == CLRA || size == FS_LARGE) {
					SetUnicodeGlyph(size, _default_unicode_map[i].code, 0);
				}
			} else {
				SpriteID sprite = base + key - ASCII_LETTERSTART;
				SetUnicodeGlyph(size, _default_unicode_map[i].code, sprite);
			}
		}
	}
}
