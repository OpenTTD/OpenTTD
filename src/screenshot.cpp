/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file screenshot.cpp The creation of screenshots! */

#include "stdafx.h"
#include "openttd.h"
#include "fileio_func.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "screenshot.h"
#include "blitter/factory.hpp"
#include "zoom_func.h"
#include "core/endian_func.hpp"
#include "map_func.h"
#include "saveload/saveload.h"
#include "company_func.h"
#include "strings_func.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "tile_map.h"

#include "table/strings.h"


char _screenshot_format_name[8];
uint _num_screenshot_formats;
uint _cur_screenshot_format;
static char _screenshot_name[128];
char _full_screenshot_name[MAX_PATH];

/* called by the ScreenShot proc to generate screenshot lines. */
typedef void ScreenshotCallback(void *userdata, void *buf, uint y, uint pitch, uint n);
typedef bool ScreenshotHandlerProc(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const Colour *palette);

struct ScreenshotFormat {
	const char *name;
	const char *extension;
	ScreenshotHandlerProc *proc;
};

/*************************************************
 **** SCREENSHOT CODE FOR WINDOWS BITMAP (.BMP)
 *************************************************/
#if defined(_MSC_VER) || defined(__WATCOMC__)
#pragma pack(push, 1)
#endif

/** BMP File Header (stored in little endian) */
struct BitmapFileHeader {
	uint16 type;
	uint32 size;
	uint32 reserved;
	uint32 off_bits;
} GCC_PACK;
assert_compile(sizeof(BitmapFileHeader) == 14);

#if defined(_MSC_VER) || defined(__WATCOMC__)
#pragma pack(pop)
#endif

/** BMP Info Header (stored in little endian) */
struct BitmapInfoHeader {
	uint32 size;
	int32 width, height;
	uint16 planes, bitcount;
	uint32 compression, sizeimage, xpels, ypels, clrused, clrimp;
};
assert_compile(sizeof(BitmapInfoHeader) == 40);

/** Format of palette data in BMP header */
struct RgbQuad {
	byte blue, green, red, reserved;
};
assert_compile(sizeof(RgbQuad) == 4);

/**
 * Generic .BMP writer
 * @param name file name including extension
 * @param callb callback used for gathering rendered image
 * @param userdata parameters forwarded to #callb
 * @param w width in pixels
 * @param h height in pixels
 * @param pixelformat bits per pixel
 * @param palette colour palette (for 8bpp mode)
 * @return was everything ok?
 */
static bool MakeBMPImage(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const Colour *palette)
{
	uint bpp; // bytes per pixel
	switch (pixelformat) {
		case 8:  bpp = 1; break;
		/* 32bpp mode is saved as 24bpp BMP */
		case 32: bpp = 3; break;
		/* Only implemented for 8bit and 32bit images so far */
		default: return false;
	}

	FILE *f = fopen(name, "wb");
	if (f == NULL) return false;

	/* Each scanline must be aligned on a 32bit boundary */
	uint bytewidth = Align(w * bpp, 4); // bytes per line in file

	/* Size of palette. Only present for 8bpp mode */
	uint pal_size = pixelformat == 8 ? sizeof(RgbQuad) * 256 : 0;

	/* Setup the file header */
	BitmapFileHeader bfh;
	bfh.type = TO_LE16('MB');
	bfh.size = TO_LE32(sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader) + pal_size + bytewidth * h);
	bfh.reserved = 0;
	bfh.off_bits = TO_LE32(sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader) + pal_size);

	/* Setup the info header */
	BitmapInfoHeader bih;
	bih.size = TO_LE32(sizeof(BitmapInfoHeader));
	bih.width = TO_LE32(w);
	bih.height = TO_LE32(h);
	bih.planes = TO_LE16(1);
	bih.bitcount = TO_LE16(bpp * 8);
	bih.compression = 0;
	bih.sizeimage = 0;
	bih.xpels = 0;
	bih.ypels = 0;
	bih.clrused = 0;
	bih.clrimp = 0;

	/* Write file header and info header */
	if (fwrite(&bfh, sizeof(bfh), 1, f) != 1 || fwrite(&bih, sizeof(bih), 1, f) != 1) {
		fclose(f);
		return false;
	}

	if (pixelformat == 8) {
		/* Convert the palette to the windows format */
		RgbQuad rq[256];
		for (uint i = 0; i < 256; i++) {
			rq[i].red   = palette[i].r;
			rq[i].green = palette[i].g;
			rq[i].blue  = palette[i].b;
			rq[i].reserved = 0;
		}
		/* Write the palette */
		if (fwrite(rq, sizeof(rq), 1, f) != 1) {
			fclose(f);
			return false;
		}
	}

	/* Try to use 64k of memory, store between 16 and 128 lines */
	uint maxlines = Clamp(65536 / (w * pixelformat / 8), 16, 128); // number of lines per iteration

	uint8 *buff = MallocT<uint8>(maxlines * w * pixelformat / 8); // buffer which is rendered to
	uint8 *line = AllocaM(uint8, bytewidth); // one line, stored to file
	memset(line, 0, bytewidth);

	/* Start at the bottom, since bitmaps are stored bottom up */
	do {
		uint n = min(h, maxlines);
		h -= n;

		/* Render the pixels */
		callb(userdata, buff, h, w, n);

		/* Write each line */
		while (n-- != 0) {
			if (pixelformat == 8) {
				/* Move to 'line', leave last few pixels in line zeroed */
				memcpy(line, buff + n * w, w);
			} else {
				/* Convert from 'native' 32bpp to BMP-like 24bpp.
				 * Works for both big and little endian machines */
				Colour *src = ((Colour *)buff) + n * w;
				byte *dst = line;
				for (uint i = 0; i < w; i++) {
					dst[i * 3    ] = src[i].b;
					dst[i * 3 + 1] = src[i].g;
					dst[i * 3 + 2] = src[i].r;
				}
			}
			/* Write to file */
			if (fwrite(line, bytewidth, 1, f) != 1) {
				free(buff);
				fclose(f);
				return false;
			}
		}
	} while (h != 0);

	free(buff);
	fclose(f);

	return true;
}

/*********************************************************
 **** SCREENSHOT CODE FOR PORTABLE NETWORK GRAPHICS (.PNG)
 *********************************************************/
#if defined(WITH_PNG)
#include <png.h>

#ifdef PNG_TEXT_SUPPORTED
#include "rev.h"
#include "newgrf_config.h"
#include "ai/ai_info.hpp"
#include "company_base.h"
#include "base_media_base.h"
#endif /* PNG_TEXT_SUPPORTED */

static void PNGAPI png_my_error(png_structp png_ptr, png_const_charp message)
{
	DEBUG(misc, 0, "[libpng] error: %s - %s", message, (const char *)png_get_error_ptr(png_ptr));
	longjmp(png_jmpbuf(png_ptr), 1);
}

static void PNGAPI png_my_warning(png_structp png_ptr, png_const_charp message)
{
	DEBUG(misc, 1, "[libpng] warning: %s - %s", message, (const char *)png_get_error_ptr(png_ptr));
}

static bool MakePNGImage(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const Colour *palette)
{
	png_color rq[256];
	FILE *f;
	uint i, y, n;
	uint maxlines;
	uint bpp = pixelformat / 8;
	png_structp png_ptr;
	png_infop info_ptr;

	/* only implemented for 8bit and 32bit images so far. */
	if (pixelformat != 8 && pixelformat != 32) return false;

	f = fopen(name, "wb");
	if (f == NULL) return false;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (void *)name, png_my_error, png_my_warning);

	if (png_ptr == NULL) {
		fclose(f);
		return false;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		fclose(f);
		return false;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(f);
		return false;
	}

	png_init_io(png_ptr, f);

	png_set_filter(png_ptr, 0, PNG_FILTER_NONE);

	png_set_IHDR(png_ptr, info_ptr, w, h, 8, pixelformat == 8 ? PNG_COLOR_TYPE_PALETTE : PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

#ifdef PNG_TEXT_SUPPORTED
	/* Try to add some game metadata to the PNG screenshot so
	 * it's more useful for debugging and archival purposes. */
	png_text_struct text[2];
	memset(text, 0, sizeof(text));
	text[0].key = const_cast<char *>("Software");
	text[0].text = const_cast<char *>(_openttd_revision);
	text[0].text_length = strlen(_openttd_revision);
	text[0].compression = PNG_TEXT_COMPRESSION_NONE;

	char buf[2048];
	char *p = buf;
	p += seprintf(p, lastof(buf), "Graphics set: %s (%u)\n", BaseGraphics::GetUsedSet()->name, BaseGraphics::GetUsedSet()->version);
	p = strecpy(p, "NewGRFs:\n", lastof(buf));
	for (const GRFConfig *c = _grfconfig; c != NULL; c = c->next) {
		p += seprintf(p, lastof(buf), "%08X ", BSWAP32(c->ident.grfid));
		p = md5sumToString(p, lastof(buf), c->ident.md5sum);
		p += seprintf(p, lastof(buf), " %s\n", c->filename);
	}
	p = strecpy(p, "\nCompanies:\n", lastof(buf));
	const Company *c;
	FOR_ALL_COMPANIES(c) {
		if (c->ai_info == NULL) {
			p += seprintf(p, lastof(buf), "%2i: Human\n", (int)c->index);
		} else {
#ifdef ENABLE_AI
			p += seprintf(p, lastof(buf), "%2i: %s (v%d)\n", (int)c->index, c->ai_info->GetName(), c->ai_info->GetVersion());
#endif /* ENABLE_AI */
		}
	}
	text[1].key = const_cast<char *>("Description");
	text[1].text = buf;
	text[1].text_length = p - buf;
	text[1].compression = PNG_TEXT_COMPRESSION_zTXt;
	png_set_text(png_ptr, info_ptr, text, 2);
#endif /* PNG_TEXT_SUPPORTED */

	if (pixelformat == 8) {
		/* convert the palette to the .PNG format. */
		for (i = 0; i != 256; i++) {
			rq[i].red   = palette[i].r;
			rq[i].green = palette[i].g;
			rq[i].blue  = palette[i].b;
		}

		png_set_PLTE(png_ptr, info_ptr, rq, 256);
	}

	png_write_info(png_ptr, info_ptr);
	png_set_flush(png_ptr, 512);

	if (pixelformat == 32) {
		png_color_8 sig_bit;

		/* Save exact colour/alpha resolution */
		sig_bit.alpha = 0;
		sig_bit.blue  = 8;
		sig_bit.green = 8;
		sig_bit.red   = 8;
		sig_bit.gray  = 8;
		png_set_sBIT(png_ptr, info_ptr, &sig_bit);

#if TTD_ENDIAN == TTD_LITTLE_ENDIAN
		png_set_bgr(png_ptr);
		png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
#else
		png_set_filler(png_ptr, 0, PNG_FILLER_BEFORE);
#endif /* TTD_ENDIAN == TTD_LITTLE_ENDIAN */
	}

	/* use by default 64k temp memory */
	maxlines = Clamp(65536 / w, 16, 128);

	/* now generate the bitmap bits */
	void *buff = CallocT<uint8>(w * maxlines * bpp); // by default generate 128 lines at a time.

	y = 0;
	do {
		/* determine # lines to write */
		n = min(h - y, maxlines);

		/* render the pixels into the buffer */
		callb(userdata, buff, y, w, n);
		y += n;

		/* write them to png */
		for (i = 0; i != n; i++) {
			png_write_row(png_ptr, (png_bytep)buff + i * w * bpp);
		}
	} while (y != h);

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	free(buff);
	fclose(f);
	return true;
}
#endif /* WITH_PNG */


/*************************************************
 **** SCREENSHOT CODE FOR ZSOFT PAINTBRUSH (.PCX)
 *************************************************/

struct PcxHeader {
	byte manufacturer;
	byte version;
	byte rle;
	byte bpp;
	uint32 unused;
	uint16 xmax, ymax;
	uint16 hdpi, vdpi;
	byte pal_small[16 * 3];
	byte reserved;
	byte planes;
	uint16 pitch;
	uint16 cpal;
	uint16 width;
	uint16 height;
	byte filler[54];
};
assert_compile(sizeof(PcxHeader) == 128);

static bool MakePCXImage(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const Colour *palette)
{
	FILE *f;
	uint maxlines;
	uint y;
	PcxHeader pcx;
	bool success;

	if (pixelformat == 32) {
		DEBUG(misc, 0, "Can't convert a 32bpp screenshot to PCX format. Please pick another format.");
		return false;
	}
	if (pixelformat != 8 || w == 0) return false;

	f = fopen(name, "wb");
	if (f == NULL) return false;

	memset(&pcx, 0, sizeof(pcx));

	/* setup pcx header */
	pcx.manufacturer = 10;
	pcx.version = 5;
	pcx.rle = 1;
	pcx.bpp = 8;
	pcx.xmax = TO_LE16(w - 1);
	pcx.ymax = TO_LE16(h - 1);
	pcx.hdpi = TO_LE16(320);
	pcx.vdpi = TO_LE16(320);

	pcx.planes = 1;
	pcx.cpal = TO_LE16(1);
	pcx.width = pcx.pitch = TO_LE16(w);
	pcx.height = TO_LE16(h);

	/* write pcx header */
	if (fwrite(&pcx, sizeof(pcx), 1, f) != 1) {
		fclose(f);
		return false;
	}

	/* use by default 64k temp memory */
	maxlines = Clamp(65536 / w, 16, 128);

	/* now generate the bitmap bits */
	uint8 *buff = CallocT<uint8>(w * maxlines); // by default generate 128 lines at a time.

	y = 0;
	do {
		/* determine # lines to write */
		uint n = min(h - y, maxlines);
		uint i;

		/* render the pixels into the buffer */
		callb(userdata, buff, y, w, n);
		y += n;

		/* write them to pcx */
		for (i = 0; i != n; i++) {
			const uint8 *bufp = buff + i * w;
			byte runchar = bufp[0];
			uint runcount = 1;
			uint j;

			/* for each pixel... */
			for (j = 1; j < w; j++) {
				uint8 ch = bufp[j];

				if (ch != runchar || runcount >= 0x3f) {
					if (runcount > 1 || (runchar & 0xC0) == 0xC0) {
						if (fputc(0xC0 | runcount, f) == EOF) {
							free(buff);
							fclose(f);
							return false;
						}
					}
					if (fputc(runchar, f) == EOF) {
						free(buff);
						fclose(f);
						return false;
					}
					runcount = 0;
					runchar = ch;
				}
				runcount++;
			}

			/* write remaining bytes.. */
			if (runcount > 1 || (runchar & 0xC0) == 0xC0) {
				if (fputc(0xC0 | runcount, f) == EOF) {
					free(buff);
					fclose(f);
					return false;
				}
			}
			if (fputc(runchar, f) == EOF) {
				free(buff);
				fclose(f);
				return false;
			}
		}
	} while (y != h);

	free(buff);

	/* write 8-bit colour palette */
	if (fputc(12, f) == EOF) {
		fclose(f);
		return false;
	}

	/* Palette is word-aligned, copy it to a temporary byte array */
	byte tmp[256 * 3];

	for (uint i = 0; i < 256; i++) {
		tmp[i * 3 + 0] = palette[i].r;
		tmp[i * 3 + 1] = palette[i].g;
		tmp[i * 3 + 2] = palette[i].b;
	}
	success = fwrite(tmp, sizeof(tmp), 1, f) == 1;

	fclose(f);

	return success;
}

/*************************************************
 **** GENERIC SCREENSHOT CODE
 *************************************************/

static const ScreenshotFormat _screenshot_formats[] = {
#if defined(WITH_PNG)
	{"PNG", "png", &MakePNGImage},
#endif
	{"BMP", "bmp", &MakeBMPImage},
	{"PCX", "pcx", &MakePCXImage},
};

void InitializeScreenshotFormats()
{
	uint j = 0;
	for (uint i = 0; i < lengthof(_screenshot_formats); i++) {
		if (!strcmp(_screenshot_format_name, _screenshot_formats[i].extension)) {
			j = i;
			break;
		}
	}
	_cur_screenshot_format = j;
	_num_screenshot_formats = lengthof(_screenshot_formats);
}

const char *GetScreenshotFormatDesc(int i)
{
	return _screenshot_formats[i].name;
}

void SetScreenshotFormat(uint i)
{
	assert(i < _num_screenshot_formats);
	_cur_screenshot_format = i;
	strecpy(_screenshot_format_name, _screenshot_formats[i].extension, lastof(_screenshot_format_name));
}

/* screenshot generator that dumps the current video buffer */
static void CurrentScreenCallback(void *userdata, void *buf, uint y, uint pitch, uint n)
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	void *src = blitter->MoveTo(_screen.dst_ptr, 0, y);
	blitter->CopyImageToBuffer(src, buf, _screen.width, n, pitch);
}

/**
 * generate a large piece of the world
 * @param userdata Viewport area to draw
 * @param buf Videobuffer with same bitdepth as current blitter
 * @param y First line to render
 * @param pitch Pitch of the videobuffer
 * @param n Number of lines to render
 */
static void LargeWorldCallback(void *userdata, void *buf, uint y, uint pitch, uint n)
{
	ViewPort *vp = (ViewPort *)userdata;
	DrawPixelInfo dpi, *old_dpi;
	int wx, left;

	/* We are no longer rendering to the screen */
	DrawPixelInfo old_screen = _screen;
	bool old_disable_anim = _screen_disable_anim;

	_screen.dst_ptr = buf;
	_screen.width = pitch;
	_screen.height = n;
	_screen.pitch = pitch;
	_screen_disable_anim = true;

	old_dpi = _cur_dpi;
	_cur_dpi = &dpi;

	dpi.dst_ptr = buf;
	dpi.height = n;
	dpi.width = vp->width;
	dpi.pitch = pitch;
	dpi.zoom = ZOOM_LVL_WORLD_SCREENSHOT;
	dpi.left = 0;
	dpi.top = y;

	/* Render viewport in blocks of 1600 pixels width */
	left = 0;
	while (vp->width - left != 0) {
		wx = min(vp->width - left, 1600);
		left += wx;

		ViewportDoDraw(vp,
			ScaleByZoom(left - wx - vp->left, vp->zoom) + vp->virtual_left,
			ScaleByZoom(y - vp->top, vp->zoom) + vp->virtual_top,
			ScaleByZoom(left - vp->left, vp->zoom) + vp->virtual_left,
			ScaleByZoom((y + n) - vp->top, vp->zoom) + vp->virtual_top
		);
	}

	_cur_dpi = old_dpi;

	/* Switch back to rendering to the screen */
	_screen = old_screen;
	_screen_disable_anim = old_disable_anim;
}

static const char *MakeScreenshotName(const char *ext)
{
	bool generate = StrEmpty(_screenshot_name);

	if (generate) {
		if (_game_mode == GM_EDITOR || _game_mode == GM_MENU || _local_company == COMPANY_SPECTATOR) {
			strecpy(_screenshot_name, "screenshot", lastof(_screenshot_name));
		} else {
			GenerateDefaultSaveName(_screenshot_name, lastof(_screenshot_name));
		}
	}

	/* Add extension to screenshot file */
	size_t len = strlen(_screenshot_name);
	snprintf(&_screenshot_name[len], lengthof(_screenshot_name) - len, ".%s", ext);

	for (uint serial = 1;; serial++) {
		if (snprintf(_full_screenshot_name, lengthof(_full_screenshot_name), "%s%s", _personal_dir, _screenshot_name) >= (int)lengthof(_full_screenshot_name)) {
			/* We need more characters than MAX_PATH -> end with error */
			_full_screenshot_name[0] = '\0';
			break;
		}
		if (!generate) break; // allow overwriting of non-automatic filenames
		if (!FileExists(_full_screenshot_name)) break;
		/* If file exists try another one with same name, but just with a higher index */
		snprintf(&_screenshot_name[len], lengthof(_screenshot_name) - len, "#%u.%s", serial, ext);
	}

	return _full_screenshot_name;
}

/** Make a screenshot of the current screen. */
static bool MakeSmallScreenshot()
{
	const ScreenshotFormat *sf = _screenshot_formats + _cur_screenshot_format;
	return sf->proc(MakeScreenshotName(sf->extension), CurrentScreenCallback, NULL, _screen.width, _screen.height, BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth(), _cur_palette);
}

/** Make a zoomed-in screenshot of the currently visible area. */
static bool MakeZoomedInScreenshot()
{
	Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
	ViewPort vp;

	vp.zoom = ZOOM_LVL_WORLD_SCREENSHOT;
	vp.left = w->viewport->left;
	vp.top = w->viewport->top;
	vp.virtual_left = w->viewport->virtual_left;
	vp.virtual_top = w->viewport->virtual_top;
	vp.virtual_width = w->viewport->virtual_width;
	vp.width = vp.virtual_width;
	vp.virtual_height = w->viewport->virtual_height;
	vp.height = vp.virtual_height;

	const ScreenshotFormat *sf = _screenshot_formats + _cur_screenshot_format;
	return sf->proc(MakeScreenshotName(sf->extension), LargeWorldCallback, &vp, vp.width, vp.height, BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth(), _cur_palette);
}

/** Make a screenshot of the whole map. */
static bool MakeWorldScreenshot()
{
	ViewPort vp;
	const ScreenshotFormat *sf;

	/* We need to account for a hill or high building at tile 0,0. */
	int extra_height_top = TileHeight(0) * TILE_HEIGHT + 150;
	/* If there is a hill at the bottom don't create a large black area. */
	int reclaim_height_bottom = TileHeight(MapSize() - 1) * TILE_HEIGHT;

	vp.zoom = ZOOM_LVL_WORLD_SCREENSHOT;
	vp.left = 0;
	vp.top = 0;
	vp.virtual_left = -(int)MapMaxX() * TILE_PIXELS;
	vp.virtual_top = -extra_height_top;
	vp.virtual_width = (MapMaxX() + MapMaxY()) * TILE_PIXELS;
	vp.width = vp.virtual_width;
	vp.virtual_height = ((MapMaxX() + MapMaxY()) * TILE_PIXELS >> 1) + extra_height_top - reclaim_height_bottom;
	vp.height = vp.virtual_height;

	sf = _screenshot_formats + _cur_screenshot_format;
	return sf->proc(MakeScreenshotName(sf->extension), LargeWorldCallback, &vp, vp.width, vp.height, BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth(), _cur_palette);
}

/**
 * Make an actual screenshot.
 * @param t    the type of screenshot to make.
 * @param name the name to give to the screenshot.
 * @return true iff the screenshow was made successfully
 */
bool MakeScreenshot(ScreenshotType t, const char *name)
{
	if (t == SC_VIEWPORT) {
		/* First draw the dirty parts of the screen and only then change the name
		 * of the screenshot. This way the screenshot will always show the name
		 * of the previous screenshot in the 'successful' message instead of the
		 * name of the new screenshot (or an empty name). */
		UndrawMouseCursor();
		DrawDirtyBlocks();
	}

	_screenshot_name[0] = '\0';
	if (name != NULL) strecpy(_screenshot_name, name, lastof(_screenshot_name));

	bool ret;
	switch (t) {
		case SC_VIEWPORT:
		case SC_RAW:
			ret = MakeSmallScreenshot();
			break;

		case SC_ZOOMEDIN:
			ret = MakeZoomedInScreenshot();
			break;

		case SC_WORLD:
			ret = MakeWorldScreenshot();
			break;

		default:
			NOT_REACHED();
	}

	if (ret) {
		SetDParamStr(0, _screenshot_name);
		ShowErrorMessage(STR_MESSAGE_SCREENSHOT_SUCCESSFULLY, INVALID_STRING_ID, WL_WARNING);
	} else {
		ShowErrorMessage(STR_ERROR_SCREENSHOT_FAILED, INVALID_STRING_ID, WL_ERROR);
	}

	return ret;
}
