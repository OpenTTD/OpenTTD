/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "strings.h"
#include "table/strings.h"
#include "gfx.h"
#include "hal.h"
#include "viewport.h"
#include "player.h"
#include "screenshot.h"
#include "variables.h"
#include "date.h"

char _screenshot_format_name[8];
uint _num_screenshot_formats;
uint _cur_screenshot_format;
ScreenshotType current_screenshot_type;

// called by the ScreenShot proc to generate screenshot lines.
typedef void ScreenshotCallback(void *userdata, Pixel *buf, uint y, uint pitch, uint n);
typedef bool ScreenshotHandlerProc(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const Colour *palette);

typedef struct {
	const char *name;
	const char *extension;
	ScreenshotHandlerProc *proc;
} ScreenshotFormat;

//************************************************
//*** SCREENSHOT CODE FOR WINDOWS BITMAP (.BMP)
//************************************************
#if defined(_MSC_VER) || defined(__WATCOMC__)
#pragma pack(push, 1)
#endif

typedef struct BitmapFileHeader {
	uint16 type;
	uint32 size;
	uint32 reserved;
	uint32 off_bits;
} GCC_PACK BitmapFileHeader;
assert_compile(sizeof(BitmapFileHeader) == 14);

#if defined(_MSC_VER) || defined(__WATCOMC__)
#pragma pack(pop)
#endif

typedef struct BitmapInfoHeader {
	uint32 size;
	int32 width, height;
	uint16 planes, bitcount;
	uint32 compression, sizeimage, xpels, ypels, clrused, clrimp;
} BitmapInfoHeader;
assert_compile(sizeof(BitmapInfoHeader) == 40);

typedef struct RgbQuad {
	byte blue, green, red, reserved;
} RgbQuad;
assert_compile(sizeof(RgbQuad) == 4);

// generic .BMP writer
static bool MakeBmpImage(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const Colour *palette)
{
	BitmapFileHeader bfh;
	BitmapInfoHeader bih;
	RgbQuad rq[256];
	Pixel *buff;
	FILE *f;
	uint i, padw;
	uint n, maxlines;

	// only implemented for 8bit images so far.
	if (pixelformat != 8)
		return false;

	f = fopen(name, "wb");
	if (f == NULL) return false;

	// each scanline must be aligned on a 32bit boundary
	padw = ALIGN(w, 4);

	// setup the file header
	bfh.type = TO_LE16('MB');
	bfh.size = TO_LE32(sizeof(bfh) + sizeof(bih) + sizeof(RgbQuad) * 256 + padw * h);
	bfh.reserved = 0;
	bfh.off_bits = TO_LE32(sizeof(bfh) + sizeof(bih) + sizeof(RgbQuad) * 256);

	// setup the info header
	bih.size = TO_LE32(sizeof(BitmapInfoHeader));
	bih.width = TO_LE32(w);
	bih.height = TO_LE32(h);
	bih.planes = TO_LE16(1);
	bih.bitcount = TO_LE16(8);
	bih.compression = 0;
	bih.sizeimage = 0;
	bih.xpels = 0;
	bih.ypels = 0;
	bih.clrused = 0;
	bih.clrimp = 0;

	// convert the palette to the windows format
	for (i = 0; i != 256; i++) {
		rq[i].red   = palette[i].r;
		rq[i].green = palette[i].g;
		rq[i].blue  = palette[i].b;
		rq[i].reserved = 0;
	}

	// write file header and info header and palette
	if (fwrite(&bfh, sizeof(bfh), 1, f) != 1) return false;
	if (fwrite(&bih, sizeof(bih), 1, f) != 1) return false;
	if (fwrite(rq, sizeof(rq), 1, f) != 1) return false;

	// use by default 64k temp memory
	maxlines = clamp(65536 / padw, 16, 128);

	// now generate the bitmap bits
	buff = malloc(padw * maxlines); // by default generate 128 lines at a time.
	if (buff == NULL) {
		fclose(f);
		return false;
	}
	memset(buff, 0, padw * maxlines); // zero the buffer to have the padding bytes set to 0

	// start at the bottom, since bitmaps are stored bottom up.
	do {
		// determine # lines
		n = min(h, maxlines);
		h -= n;

		// render the pixels
		callb(userdata, buff, h, padw, n);

		// write each line
		while (n)
			if (fwrite(buff + (--n) * padw, padw, 1, f) != 1) {
				free(buff);
				fclose(f);
				return false;
			}
	} while (h != 0);

	free(buff);
	fclose(f);

	return true;
}

//********************************************************
//*** SCREENSHOT CODE FOR PORTABLE NETWORK GRAPHICS (.PNG)
//********************************************************
#if defined(WITH_PNG)
#include <png.h>

static void PNGAPI png_my_error(png_structp png_ptr, png_const_charp message)
{
	DEBUG(misc, 0) ("ERROR(libpng): %s - %s", message, (char *)png_get_error_ptr(png_ptr));
	longjmp(png_ptr->jmpbuf, 1);
}

static void PNGAPI png_my_warning(png_structp png_ptr, png_const_charp message)
{
	DEBUG(misc, 0) ("WARNING(libpng): %s - %s", message, (char *)png_get_error_ptr(png_ptr));
}

static bool MakePNGImage(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const Colour *palette)
{
	png_color rq[256];
	Pixel *buff;
	FILE *f;
	uint i, y, n;
	uint maxlines;
	png_structp png_ptr;
	png_infop info_ptr;

	// only implemented for 8bit images so far.
	if (pixelformat != 8)
		return false;

	f = fopen(name, "wb");
	if (f == NULL) return false;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (char *)name, png_my_error, png_my_warning);

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

	png_set_IHDR(png_ptr, info_ptr, w, h, pixelformat, PNG_COLOR_TYPE_PALETTE,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	// convert the palette to the .PNG format.
	for (i = 0; i != 256; i++) {
		rq[i].red   = palette[i].r;
		rq[i].green = palette[i].g;
		rq[i].blue  = palette[i].b;
	}

	png_set_PLTE(png_ptr, info_ptr, rq, 256);
	png_write_info(png_ptr, info_ptr);
	png_set_flush(png_ptr, 512);

	// use by default 64k temp memory
	maxlines = clamp(65536 / w, 16, 128);

	// now generate the bitmap bits
	buff = malloc(w * maxlines); // by default generate 128 lines at a time.
	if (buff == NULL) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(f);
		return false;
	}
	memset(buff, 0, w * maxlines); // zero the buffer to have the padding bytes set to 0

	y = 0;
	do {
		// determine # lines to write
		n = min(h - y, maxlines);

		// render the pixels into the buffer
		callb(userdata, buff, y, w, n);
		y += n;

		// write them to png
		for (i = 0; i != n; i++)
			png_write_row(png_ptr, buff + i * w);
	} while (y != h);

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	free(buff);
	fclose(f);
	return true;
}
#endif /* WITH_PNG */


//************************************************
//*** SCREENSHOT CODE FOR ZSOFT PAINTBRUSH (.PCX)
//************************************************

typedef struct {
	byte manufacturer;
	byte version;
	byte rle;
	byte bpp;
	uint32 unused;
	uint16 xmax, ymax;
	uint16 hdpi, vdpi;
	byte pal_small[16*3];
	byte reserved;
	byte planes;
	uint16 pitch;
	uint16 cpal;
	uint16 width;
	uint16 height;
	byte filler[54];
} PcxHeader;
assert_compile(sizeof(PcxHeader) == 128);

static bool MakePCXImage(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const Colour *palette)
{
	Pixel *buff;
	FILE *f;
	uint maxlines;
	uint y;
	PcxHeader pcx;
	bool success;

	if (pixelformat != 8 || w == 0)
		return false;

	f = fopen(name, "wb");
	if (f == NULL) return false;

	memset(&pcx, 0, sizeof(pcx));

	// setup pcx header
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

	// write pcx header
	if (fwrite(&pcx, sizeof(pcx), 1, f) != 1) {
		fclose(f);
		return false;
	}

	// use by default 64k temp memory
	maxlines = clamp(65536 / w, 16, 128);

	// now generate the bitmap bits
	buff = malloc(w * maxlines); // by default generate 128 lines at a time.
	if (buff == NULL) {
		fclose(f);
		return false;
	}
	memset(buff, 0, w * maxlines); // zero the buffer to have the padding bytes set to 0

	y = 0;
	do {
		// determine # lines to write
		uint n = min(h - y, maxlines);
		uint i;

		// render the pixels into the buffer
		callb(userdata, buff, y, w, n);
		y += n;

		// write them to pcx
		for (i = 0; i != n; i++) {
			const Pixel* bufp = buff + i * w;
			byte runchar = bufp[0];
			uint runcount = 1;
			uint j;

			// for each pixel...
			for (j = 1; j < w; j++) {
				Pixel ch = bufp[j];

				if (ch != runchar || runcount >= 0x3f) {
					if (runcount > 1 || (runchar & 0xC0) == 0xC0)
						if (fputc(0xC0 | runcount, f) == EOF) {
							free(buff);
							fclose(f);
							return false;
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

			// write remaining bytes..
			if (runcount > 1 || (runchar & 0xC0) == 0xC0)
				if (fputc(0xC0 | runcount, f) == EOF) {
					free(buff);
					fclose(f);
					return false;
				}
			if (fputc(runchar, f) == EOF) {
				free(buff);
				fclose(f);
				return false;
			}
		}
	} while (y != h);

	free(buff);

	// write 8-bit color palette
	if (fputc(12, f) == EOF) {
		fclose(f);
		return false;
	}

	if (sizeof(*palette) == 3) {
		success = fwrite(palette, 256 * sizeof(*palette), 1, f) == 1;
	} else {
		/* If the palette is word-aligned, copy it to a temporary byte array */
		byte tmp[256 * 3];
		uint i;

		for (i = 0; i < 256; i++) {
			tmp[i * 3 + 0] = palette[i].r;
			tmp[i * 3 + 1] = palette[i].g;
			tmp[i * 3 + 2] = palette[i].b;
		}
		success = fwrite(tmp, sizeof(tmp), 1, f) == 1;
	}

	fclose(f);

	return success;
}

//************************************************
//*** GENERIC SCREENSHOT CODE
//************************************************

static const ScreenshotFormat _screenshot_formats[] = {
#if defined(WITH_PNG)
	{"PNG", "png", &MakePNGImage},
#endif
	{"BMP", "bmp", &MakeBmpImage},
	{"PCX", "pcx", &MakePCXImage},
};

void InitializeScreenshotFormats(void)
{
	int i, j;
	for (i = 0, j = 0; i != lengthof(_screenshot_formats); i++)
		if (!strcmp(_screenshot_format_name, _screenshot_formats[i].extension)) {
			j = i;
			break;
		}
	_cur_screenshot_format = j;
	_num_screenshot_formats = lengthof(_screenshot_formats);
	current_screenshot_type = SC_NONE;
}

const char *GetScreenshotFormatDesc(int i)
{
	return _screenshot_formats[i].name;
}

void SetScreenshotFormat(int i)
{
	_cur_screenshot_format = i;
	strcpy(_screenshot_format_name, _screenshot_formats[i].extension);
}

// screenshot generator that dumps the current video buffer
static void CurrentScreenCallback(void *userdata, Pixel *buf, uint y, uint pitch, uint n)
{
	for (; n > 0; --n) {
		memcpy(buf, _screen.dst_ptr + y * _screen.pitch, _screen.width);
		++y;
		buf += pitch;
	}
}

// generate a large piece of the world
static void LargeWorldCallback(void *userdata, Pixel *buf, uint y, uint pitch, uint n)
{
	ViewPort *vp = (ViewPort *)userdata;
	DrawPixelInfo dpi, *old_dpi;
	int wx, left;

	old_dpi = _cur_dpi;
	_cur_dpi = &dpi;

	dpi.dst_ptr = buf;
	dpi.height = n;
	dpi.width = vp->width;
	dpi.pitch = pitch;
	dpi.zoom = 0;
	dpi.left = 0;
	dpi.top = y;

	left = 0;
	while (vp->width - left != 0) {
		wx = min(vp->width - left, 1600);
		left += wx;

		ViewportDoDraw(vp,
			((left - wx - vp->left) << vp->zoom) + vp->virtual_left,
			((y - vp->top) << vp->zoom) + vp->virtual_top,
			((left - vp->left) << vp->zoom) + vp->virtual_left,
			(((y + n) - vp->top) << vp->zoom) + vp->virtual_top
		);
	}

	_cur_dpi = old_dpi;
}

static char *MakeScreenshotName(const char *ext)
{
	static char filename[256];
	char *base;
	int serial;

	if (_game_mode == GM_EDITOR || _game_mode == GM_MENU || _local_player == PLAYER_SPECTATOR) {
		sprintf(_screenshot_name, "screenshot");
	} else {
		const Player* p = GetPlayer(_local_player);
		SetDParam(0, p->name_1);
		SetDParam(1, p->name_2);
		SetDParam(2, _date);
		GetString(_screenshot_name, STR_4004, lastof(_screenshot_name));
	}

	base = strchr(_screenshot_name, 0);
	base[0] = '.'; strcpy(base + 1, ext);

	serial = 0;
	for (;;) {
		snprintf(filename, sizeof(filename), "%s%s", _paths.personal_dir, _screenshot_name);
		if (!FileExists(filename))
			break;
		sprintf(base, " #%d.%s", ++serial, ext);
	}

	return filename;
}

void SetScreenshotType(ScreenshotType t)
{
	current_screenshot_type = t;
}

bool IsScreenshotRequested(void)
{
	return (current_screenshot_type != SC_NONE);
}

static bool MakeSmallScreenshot(void)
{
	const ScreenshotFormat *sf = _screenshot_formats + _cur_screenshot_format;
	return sf->proc(MakeScreenshotName(sf->extension), CurrentScreenCallback, NULL, _screen.width, _screen.height, 8, _cur_palette);
}

static bool MakeWorldScreenshot(void)
{
	ViewPort vp;
	const ScreenshotFormat *sf;

	vp.zoom = 0;
	vp.left = 0;
	vp.top = 0;
	vp.virtual_left = -(int)MapMaxX() * TILE_PIXELS;
	vp.virtual_top = 0;
	vp.virtual_width = (MapMaxX() + MapMaxY()) * TILE_PIXELS;
	vp.width = vp.virtual_width;
	vp.virtual_height = (MapMaxX() + MapMaxY()) * TILE_PIXELS >> 1;
	vp.height = vp.virtual_height;

	sf = _screenshot_formats + _cur_screenshot_format;
	return sf->proc(MakeScreenshotName(sf->extension), LargeWorldCallback, &vp, vp.width, vp.height, 8, _cur_palette);
}

bool MakeScreenshot(void)
{
	switch (current_screenshot_type) {
		case SC_VIEWPORT:
			UndrawMouseCursor();
			DrawDirtyBlocks();
			current_screenshot_type = SC_NONE;
			return MakeSmallScreenshot();
		case SC_WORLD:
			current_screenshot_type = SC_NONE;
			return MakeWorldScreenshot();
		default: return false;
	}
}

