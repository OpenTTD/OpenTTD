/* $Id$ */

/** @file fontcache.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "macros.h"
#include "debug.h"
#include "table/sprites.h"
#include "table/control_codes.h"
#include "spritecache.h"
#include "gfx.h"
#include "string.h"
#include "fontcache.h"
#include "helpers.hpp"

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

FreeTypeSettings _freetype;

enum {
	FACE_COLOUR = 1,
	SHADOW_COLOUR = 2,
};

/** Get the font loaded into a Freetype face by using a font-name.
 * If no appropiate font is found, the function returns an error */
#ifdef WIN32
#include <windows.h>
#include <tchar.h>
#include <shlobj.h> // SHGetFolderPath
#include "win32.h"

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
	font_namep = (char*)font_name; // only cast because in unicode pointer is not const
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
	index = 0;
	do {
		err = FT_New_Face(_library, font_path, index, face);
		if (err != FT_Err_Ok) break;

		if (strncasecmp(font_name, (*face)->family_name, strlen((*face)->family_name)) == 0) break;
		err = FT_Err_Cannot_Open_Resource;

	} while ((FT_Long)++index != (*face)->num_faces);


folder_error:
#if defined(UNICODE)
	free(font_path);
#endif
registry_no_font_found:
	RegCloseKey(hKey);
	return err;
}
#else
# ifdef WITH_FONTCONFIG
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
# else
FT_Error GetFontByFaceName(const char *font_name, FT_Face *face) {return FT_Err_Cannot_Open_Resource;}
# endif /* WITH_FONTCONFIG */

#endif

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
	if (_face_small  != NULL) FT_Set_Pixel_Sizes(_face_small,  0, _freetype.small_size);
	if (_face_medium != NULL) FT_Set_Pixel_Sizes(_face_medium, 0, _freetype.medium_size);
	if (_face_large  != NULL) FT_Set_Pixel_Sizes(_face_large,  0, _freetype.large_size);
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


static GlyphEntry *GetGlyphPtr(FontSize size, WChar key)
{
	if (_glyph_ptr[size] == NULL) return NULL;
	if (_glyph_ptr[size][GB(key, 8, 8)] == NULL) return NULL;
	return &_glyph_ptr[size][GB(key, 8, 8)][GB(key, 0, 8)];
}


static void SetGlyphPtr(FontSize size, WChar key, const GlyphEntry *glyph)
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
	_glyph_ptr[size][GB(key, 8, 8)][GB(key, 0, 8)].sprite = glyph->sprite;
	_glyph_ptr[size][GB(key, 8, 8)][GB(key, 0, 8)].width  = glyph->width;
}


const Sprite *GetGlyph(FontSize size, WChar key)
{
	FT_Face face = GetFontFace(size);
	FT_GlyphSlot slot;
	GlyphEntry new_glyph;
	GlyphEntry *glyph;
	Sprite *sprite;
	int width;
	int height;
	int x;
	int y;
	int y_adj;

	assert(IsPrintable(key));

	/* Bail out if no face loaded, or for our special characters */
	if (face == NULL || (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END)) {
		SpriteID sprite = GetUnicodeGlyph(size, key);
		if (sprite == 0) sprite = GetUnicodeGlyph(size, '?');
		return GetSprite(sprite);
	}

	/* Check for the glyph in our cache */
	glyph = GetGlyphPtr(size, key);
	if (glyph != NULL && glyph->sprite != NULL) return glyph->sprite;

	slot = face->glyph;

	FT_Load_Char(face, key, FT_LOAD_DEFAULT);
	FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO);

	/* Add 1 pixel for the shadow on the medium font. Our sprite must be at least 1x1 pixel */
	width  = max(1, slot->bitmap.width + (size == FS_NORMAL));
	height = max(1, slot->bitmap.rows  + (size == FS_NORMAL));

	/* FreeType has rendered the glyph, now we allocate a sprite and copy the image into it */
	sprite = (Sprite*)calloc(width * height + 8, 1);
	sprite->info   = 1;
	sprite->width  = width;
	sprite->height = height;
	sprite->x_offs = slot->bitmap_left;
	// XXX 2 should be determined somehow... it's right for the normal face
	y_adj = (size == FS_NORMAL) ? 2 : 0;
	sprite->y_offs = GetCharacterHeight(size) - slot->bitmap_top - y_adj;

	/* Draw shadow for medium size */
	if (size == FS_NORMAL) {
		for (y = 0; y < slot->bitmap.rows; y++) {
			for (x = 0; x < slot->bitmap.width; x++) {
				if (HASBIT(slot->bitmap.buffer[(x / 8) + y * slot->bitmap.pitch], 7 - (x % 8))) {
					sprite->data[1 + x + (1 + y) * sprite->width] = SHADOW_COLOUR;
				}
			}
		}
	}

	for (y = 0; y < slot->bitmap.rows; y++) {
		for (x = 0; x < slot->bitmap.width; x++) {
			if (HASBIT(slot->bitmap.buffer[(x / 8) + y * slot->bitmap.pitch], 7 - (x % 8))) {
				sprite->data[x + y * sprite->width] = FACE_COLOUR;
			}
		}
	}

	new_glyph.sprite = sprite;
	new_glyph.width  = (slot->advance.x >> 6) + (size != FS_NORMAL);

	SetGlyphPtr(size, key, &new_glyph);

	return sprite;
}


uint GetGlyphWidth(FontSize size, WChar key)
{
	FT_Face face = GetFontFace(size);
	GlyphEntry *glyph;

	if (face == NULL || (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END)) {
		SpriteID sprite = GetUnicodeGlyph(size, key);
		if (sprite == 0) sprite = GetUnicodeGlyph(size, '?');
		return SpriteExists(sprite) ? GetSprite(sprite)->width + (size != FS_NORMAL) : 0;
	}

	glyph = GetGlyphPtr(size, key);
	if (glyph == NULL || glyph->sprite == NULL) {
		GetGlyph(size, key);
		glyph = GetGlyphPtr(size, key);
	}

	return glyph->width;
}


#endif /* WITH_FREETYPE */

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
	FontSize size;
	SpriteID base;
	SpriteID sprite;
	uint i;

	for (size = FS_NORMAL; size != FS_END; size++) {
		/* Clear out existing glyph map if it exists */
		if (_unicode_glyph_map[size] != NULL) {
			for (i = 0; i < 256; i++) {
				if (_unicode_glyph_map[size][i] != NULL) free(_unicode_glyph_map[size][i]);
			}
			free(_unicode_glyph_map[size]);
			_unicode_glyph_map[size] = NULL;
		}

		base = GetFontBase(size);
		for (i = ASCII_LETTERSTART; i < 256; i++) {
			sprite = base + i - ASCII_LETTERSTART;
			if (!SpriteExists(sprite)) continue;
			SetUnicodeGlyph(size, i, sprite);
			SetUnicodeGlyph(size, i + SCC_SPRITE_START, sprite);
		}
		for (i = 0; i < lengthof(_default_unicode_map); i++) {
			sprite = base + _default_unicode_map[i].key - ASCII_LETTERSTART;
			SetUnicodeGlyph(size, _default_unicode_map[i].code, sprite);
		}
	}
}



