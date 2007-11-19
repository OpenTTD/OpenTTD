/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "strings.h"
#include "table/strings.h"
#include "gfx.h"
#include "fileio.h"
#include "viewport.h"
#include "player.h"
#include "screenshot.h"
#include "variables.h"
#include "date.h"
#include "string.h"
#include "helpers.hpp"
#include "blitter/factory.hpp"
#include "fileio.h"

char _screenshot_format_name[8];
uint _num_screenshot_formats;
uint _cur_screenshot_format;
ScreenshotType current_screenshot_type;

/* called by the ScreenShot proc to generate screenshot lines. */
typedef void ScreenshotCallback(void *userdata, void *buf, uint y, uint pitch, uint n);
typedef bool ScreenshotHandlerProc(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const Colour *palette);

struct ScreenshotFormat {
	const char *name;
	const char *extension;
	ScreenshotHandlerProc *proc;
};

//************************************************
//*** SCREENSHOT CODE FOR WINDOWS BITMAP (.BMP)
//************************************************
#if defined(_MSC_VER) || defined(__WATCOMC__)
#pragma pack(push, 1)
#endif

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

struct BitmapInfoHeader {
	uint32 size;
	int32 width, height;
	uint16 planes, bitcount;
	uint32 compression, sizeimage, xpels, ypels, clrused, clrimp;
};
assert_compile(sizeof(BitmapInfoHeader) == 40);

struct RgbQuad {
	byte blue, green, red, reserved;
};
assert_compile(sizeof(RgbQuad) == 4);

/* generic .BMP writer */
static bool MakeBmpImage(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const Colour *palette)
{
	BitmapFileHeader bfh;
	BitmapInfoHeader bih;
	RgbQuad rq[256];
	FILE *f;
	uint i, padw;
	uint n, maxlines;
	uint pal_size = 0;
	uint bpp = pixelformat / 8;

	/* only implemented for 8bit and 32bit images so far. */
	if (pixelformat != 8 && pixelformat != 32) return false;

	f = fopen(name, "wb");
	if (f == NULL) return false;

	/* each scanline must be aligned on a 32bit boundary */
	padw = ALIGN(w, 4);

	if (pixelformat == 8) pal_size = sizeof(RgbQuad) * 256;

	/* setup the file header */
	bfh.type = TO_LE16('MB');
	bfh.size = TO_LE32(sizeof(bfh) + sizeof(bih) + pal_size + padw * h * bpp);
	bfh.reserved = 0;
	bfh.off_bits = TO_LE32(sizeof(bfh) + sizeof(bih) + pal_size);

	/* setup the info header */
	bih.size = TO_LE32(sizeof(BitmapInfoHeader));
	bih.width = TO_LE32(w);
	bih.height = TO_LE32(h);
	bih.planes = TO_LE16(1);
	bih.bitcount = TO_LE16(pixelformat);
	bih.compression = 0;
	bih.sizeimage = 0;
	bih.xpels = 0;
	bih.ypels = 0;
	bih.clrused = 0;
	bih.clrimp = 0;

	if (pixelformat == 8) {
		/* convert the palette to the windows format */
		for (i = 0; i != 256; i++) {
			rq[i].red   = palette[i].r;
			rq[i].green = palette[i].g;
			rq[i].blue  = palette[i].b;
			rq[i].reserved = 0;
		}
	}

	/* write file header and info header and palette */
	if (fwrite(&bfh, sizeof(bfh), 1, f) != 1) return false;
	if (fwrite(&bih, sizeof(bih), 1, f) != 1) return false;
	if (pixelformat == 8) if (fwrite(rq, sizeof(rq), 1, f) != 1) return false;

	/* use by default 64k temp memory */
	maxlines = Clamp(65536 / padw, 16, 128);

	/* now generate the bitmap bits */
	void *buff = MallocT<uint8>(padw * maxlines * bpp); // by default generate 128 lines at a time.
	if (buff == NULL) {
		fclose(f);
		return false;
	}
	memset(buff, 0, padw * maxlines); // zero the buffer to have the padding bytes set to 0

	/* start at the bottom, since bitmaps are stored bottom up. */
	do {
		/* determine # lines */
		n = min(h, maxlines);
		h -= n;

		/* render the pixels */
		callb(userdata, buff, h, padw, n);

		/* write each line */
		while (n)
			if (fwrite((uint8 *)buff + (--n) * padw * bpp, padw * bpp, 1, f) != 1) {
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
	DEBUG(misc, 0, "[libpng] error: %s - %s", message, (char *)png_get_error_ptr(png_ptr));
	longjmp(png_ptr->jmpbuf, 1);
}

static void PNGAPI png_my_warning(png_structp png_ptr, png_const_charp message)
{
	DEBUG(misc, 1, "[libpng] warning: %s - %s", message, (char *)png_get_error_ptr(png_ptr));
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

	png_set_IHDR(png_ptr, info_ptr, w, h, 8, pixelformat == 8 ? PNG_COLOR_TYPE_PALETTE : PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

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

		/* Save exact color/alpha resolution */
		sig_bit.alpha = 0;
		sig_bit.blue  = 8;
		sig_bit.green = 8;
		sig_bit.red   = 8;
		sig_bit.gray  = 8;
		png_set_sBIT(png_ptr, info_ptr, &sig_bit);

#ifdef TTD_LITTLE_ENDIAN
		png_set_bgr(png_ptr);
		png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
#else
		png_set_filler(png_ptr, 0, PNG_FILLER_BEFORE);
#endif
	}

	/* use by default 64k temp memory */
	maxlines = Clamp(65536 / w, 16, 128);

	/* now generate the bitmap bits */
	void *buff = MallocT<uint8>(w * maxlines * bpp); // by default generate 128 lines at a time.
	if (buff == NULL) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(f);
		return false;
	}
	memset(buff, 0, w * maxlines * bpp);

	y = 0;
	do {
		/* determine # lines to write */
		n = min(h - y, maxlines);

		/* render the pixels into the buffer */
		callb(userdata, buff, y, w, n);
		y += n;

		/* write them to png */
		for (i = 0; i != n; i++)
			png_write_row(png_ptr, (png_bytep)buff + i * w * bpp);
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
		DEBUG(misc, 0, "Can't convert a 32bpp screenshot to PCX format. Please pick an other format.");
		return false;
	}
	if (pixelformat != 8 || w == 0)
		return false;

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
	uint8 *buff = MallocT<uint8>(w * maxlines); // by default generate 128 lines at a time.
	if (buff == NULL) {
		fclose(f);
		return false;
	}
	memset(buff, 0, w * maxlines); // zero the buffer to have the padding bytes set to 0

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

			/* write remaining bytes.. */
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

	/* write 8-bit color palette */
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

void InitializeScreenshotFormats()
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

/* screenshot generator that dumps the current video buffer */
static void CurrentScreenCallback(void *userdata, void *buf, uint y, uint pitch, uint n)
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	void *src = blitter->MoveTo(_screen.dst_ptr, 0, y);
	blitter->CopyImageToBuffer(src, buf, _screen.width, n, pitch);
}

/* generate a large piece of the world */
static void LargeWorldCallback(void *userdata, void *buf, uint y, uint pitch, uint n)
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
	dpi.zoom = ZOOM_LVL_WORLD_SCREENSHOT;
	dpi.left = 0;
	dpi.top = y;

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
}

static char *MakeScreenshotName(const char *ext)
{
	static char filename[MAX_PATH];
	int serial;
	size_t len;

	if (_game_mode == GM_EDITOR || _game_mode == GM_MENU || _local_player == PLAYER_SPECTATOR) {
		ttd_strlcpy(_screenshot_name, "screenshot", lengthof(_screenshot_name));
	} else {
		SetDParam(0, _local_player);
		SetDParam(1, _date);
		GetString(_screenshot_name, STR_4004, lastof(_screenshot_name));
	}

	/* Add extension to screenshot file */
	SanitizeFilename(_screenshot_name);
	len = strlen(_screenshot_name);
	snprintf(&_screenshot_name[len], lengthof(_screenshot_name) - len, ".%s", ext);

	for (serial = 1;; serial++) {
		snprintf(filename, lengthof(filename), "%s%s", _personal_dir, _screenshot_name);
		if (!FileExists(filename)) break;
		/* If file exists try another one with same name, but just with a higher index */
		snprintf(&_screenshot_name[len], lengthof(_screenshot_name) - len, "#%d.%s", serial, ext);
	}

	return filename;
}

void SetScreenshotType(ScreenshotType t)
{
	current_screenshot_type = t;
}

bool IsScreenshotRequested()
{
	return (current_screenshot_type != SC_NONE);
}

static bool MakeSmallScreenshot()
{
	const ScreenshotFormat *sf = _screenshot_formats + _cur_screenshot_format;
	return sf->proc(MakeScreenshotName(sf->extension), CurrentScreenCallback, NULL, _screen.width, _screen.height, BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth(), _cur_palette);
}

static bool MakeWorldScreenshot()
{
	ViewPort vp;
	const ScreenshotFormat *sf;

	vp.zoom = ZOOM_LVL_WORLD_SCREENSHOT;
	vp.left = 0;
	vp.top = 0;
	vp.virtual_left = -(int)MapMaxX() * TILE_PIXELS;
	vp.virtual_top = 0;
	vp.virtual_width = (MapMaxX() + MapMaxY()) * TILE_PIXELS;
	vp.width = vp.virtual_width;
	vp.virtual_height = (MapMaxX() + MapMaxY()) * TILE_PIXELS >> 1;
	vp.height = vp.virtual_height;

	sf = _screenshot_formats + _cur_screenshot_format;
	return sf->proc(MakeScreenshotName(sf->extension), LargeWorldCallback, &vp, vp.width, vp.height, BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth(), _cur_palette);
}

bool MakeScreenshot()
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



