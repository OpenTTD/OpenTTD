#include "stdafx.h"
#include "ttd.h"
#include "gfx.h"
#include "viewport.h"
#include "player.h"
#include "gui.h"

// called by the ScreenShot proc to generate screenshot lines.
typedef void ScreenshotCallback(void *userdata, byte *buf, uint y, uint pitch, uint n);
typedef bool ScreenshotHandlerProc(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const byte *palette);

typedef struct {
	const char *name;
	const char *extension;
	ScreenshotHandlerProc *proc;
	byte id;
} ScreenshotFormat;

//************************************************
//*** SCREENSHOT CODE FOR WINDOWS BITMAP (.BMP)
//************************************************
#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

typedef struct BitmapFileHeader { 
	uint16 type;
	uint32 size;
	uint32 reserved;
	uint32 off_bits;
} GCC_PACK BitmapFileHeader;
assert_compile(sizeof(BitmapFileHeader) == 14);

#if defined(_MSC_VER)
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
static bool MakeBmpImage(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const byte *palette)
{
	BitmapFileHeader bfh;
	BitmapInfoHeader bih;
	RgbQuad *rq = alloca(sizeof(RgbQuad) * 256);
	byte *buff;
	FILE *f;
	uint i, padw;
	uint n, maxlines;

	// only implemented for 8bit images so far.
	if (pixelformat != 8)
		return false;

	f = fopen(name, "wb");
	if (f == NULL) return false;

	// each scanline must be aligned on a 32bit boundary
	padw = (w + 3) & ~3;

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
	for(i=0; i!=256; i++) {
		rq[i].red = *palette++;
		rq[i].green = *palette++;
		rq[i].blue = *palette++;
		rq[i].reserved = 0;
	}

	// write file header and info header and palette
	fwrite(&bfh, 1, sizeof(bfh), f);
	fwrite(&bih, 1, sizeof(bih), f);
	fwrite(rq, 1, sizeof(RgbQuad) * 256, f);

	// use by default 64k temp memory
	maxlines = clamp(65536 / padw, 16, 128);

	// now generate the bitmap bits
	buff = (byte*)alloca(padw * maxlines); // by default generate 128 lines at a time.
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
			fwrite(buff + (--n) * padw, 1, padw, f);
	} while (h);

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
	DEBUG(misc, 0) ("ERROR(libpng): %s - %s\n", message,(char *)png_get_error_ptr(png_ptr));
	longjmp(png_ptr->jmpbuf, 1);
}

static void PNGAPI png_my_warning(png_structp png_ptr, png_const_charp message)
{
	DEBUG(misc, 0) ("WARNING(libpng): %s - %s\n", message, (char *)png_get_error_ptr(png_ptr));
}

static bool MakePNGImage(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const byte *palette)
{
	png_colorp rq = alloca(sizeof(png_color) * 256);
	byte *buff;
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

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (char *) name, png_my_error, png_my_warning);

	if (!png_ptr) {
		fclose(f);
		return false;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
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

	png_set_IHDR(png_ptr, info_ptr, w, h, pixelformat, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE, 
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	// convert the palette to the .PNG format.
	{
    // avoids "might be clobbered" warning of argument "palette"
		const byte *pal = palette;

		for(i=0; i!=256; i++) {
			rq[i].red = *pal++;
			rq[i].green = *pal++;
			rq[i].blue = *pal++;
		}
	}

	png_set_PLTE(png_ptr, info_ptr, rq, 256);
	png_write_info(png_ptr, info_ptr);
	png_set_flush(png_ptr, 512);

	// use by default 64k temp memory
	maxlines = clamp(65536 / w, 16, 128);

	// now generate the bitmap bits
	buff = (byte*)alloca(w * maxlines); // by default generate 128 lines at a time.
	memset(buff, 0, w * maxlines); // zero the buffer to have the padding bytes set to 0

	y = 0;
	do {
		// determine # lines to write
		n = min(h - y, maxlines);
		
		// render the pixels into the buffer
		callb(userdata, buff, y, w, n);
		y += n;

		// write them to png
		for(i=0; i!=n; i++)
			png_write_row(png_ptr, buff + i * w);
	} while (y != h);

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(f);
	return true;
}
#endif // WITH_PNG


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

static bool MakePCXImage(const char *name, ScreenshotCallback *callb, void *userdata, uint w, uint h, int pixelformat, const byte *palette)
{
	byte *buff;
	FILE *f;
	uint maxlines;
	uint y;
	PcxHeader pcx;

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
	pcx.xmax = TO_LE16(w-1);
	pcx.ymax = TO_LE16(h-1);
	pcx.hdpi = TO_LE16(320);
	pcx.vdpi = TO_LE16(320);

	pcx.planes = 1;
	pcx.cpal = TO_LE16(1);
	pcx.width = pcx.pitch = TO_LE16(w);
	pcx.height = TO_LE16(h);
	
	// write pcx header
	fwrite(&pcx, sizeof(pcx), 1, f);

	// use by default 64k temp memory
	maxlines = clamp(65536 / w, 16, 128);

	// now generate the bitmap bits
	buff = (byte*)alloca(w * maxlines);				// by default generate 128 lines at a time.
	memset(buff, 0, w * maxlines);			// zero the buffer to have the padding bytes set to 0

	y = 0;
	do {
		// determine # lines to write
		uint n = min(h - y, maxlines), i;
		
		// render the pixels into the buffer
		callb(userdata, buff, y, w, n);
		y += n;

		// write them to pcx
		for(i=0; i!=n; i++) {
			int runcount = 1;
			byte *bufp = buff + i * w;
			byte runchar = buff[0];
			uint left = w - 1;

			// for each pixel... 
			while (left) {
				byte ch = *bufp++;
				if (ch != runchar || runcount >= 0x3f) {
					if (runcount > 1 || (runchar & 0xC0) == 0xC0) fputc(0xC0 | runcount, f);
					fputc(runchar,f);
					runcount = 0;
					runchar = ch;
				}
				runcount++;
				left--;
			}

			// write remaining bytes..
			if (runcount > 1 || (runchar & 0xC0) == 0xC0) fputc(0xC0 | runcount, f);
			fputc(runchar,f);
		}
	} while (y != h);

	// write 8-bit color palette
	fputc(12, f); 
	fwrite(palette, 256*3, 1, f);
	fclose(f);

	return true;
}

//************************************************
//*** GENERIC SCREENSHOT CODE
//************************************************

static const ScreenshotFormat _screenshot_formats[] = {
	{"BMP", "bmp", &MakeBmpImage},
#if defined(WITH_PNG)
	{"PNG", "png", &MakePNGImage},
#endif
	{"PCX", "pcx", &MakePCXImage},
};

void InitializeScreenshotFormats()
{
	int i,j;
	for (i=0,j=0; i!=lengthof(_screenshot_formats); i++)
		if (!strcmp(_screenshot_format_name, _screenshot_formats[i].extension)) { j=i; break; }
	_cur_screenshot_format = j;
	_num_screenshot_formats = lengthof(_screenshot_formats);
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
void CurrentScreenCallback(void *userdata, byte *buf, uint y, uint pitch, uint n)
{
	assert(_screen.pitch == (int)pitch);
	memcpy(buf, _screen.dst_ptr + y * _screen.pitch, n * _screen.pitch);
}

extern void ViewportDoDraw(ViewPort *vp, int left, int top, int right, int bottom);

// generate a large piece of the world
void LargeWorldCallback(void *userdata, byte *buf, uint y, uint pitch, uint n)
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
			(((y+n) - vp->top) << vp->zoom) + vp->virtual_top
		);
	}

	_cur_dpi = old_dpi;
}

static char *MakeScreenshotName(const char *ext)
{
	static char filename[256];
	char *base;
	int serial;

	if (_game_mode == GM_EDITOR || _local_player == 0xff) {
		sprintf(_screenshot_name, "screenshot");
	} else {
		Player *p = &_players[_local_player];
		SET_DPARAM16(0, p->name_1);
		SET_DPARAM32(1, p->name_2);
		SET_DPARAM16(2, _date);
		GetString(_screenshot_name, STR_4004);
	}

	base = strchr(_screenshot_name, 0);
	base[0] = '.'; strcpy(base + 1, ext);

	serial = 0;
	for(;;) {
		snprintf(filename, sizeof(filename), "%s%s", _path.personal_dir, _screenshot_name);	
		if (!FileExists(filename))
			break;
		sprintf(base, " #%d.%s", ++serial, ext);
	}

	return filename;
}

extern byte _cur_palette[768];

bool MakeScreenshot()
{
	const ScreenshotFormat *sf = _screenshot_formats + _cur_screenshot_format;
	return sf->proc(MakeScreenshotName(sf->extension), CurrentScreenCallback, NULL, _screen.width, _screen.height, 8, _cur_palette); 
}

bool MakeWorldScreenshot(int left, int top, int width, int height, int zoom)
{
	ViewPort vp;
	const ScreenshotFormat *sf;

	vp.zoom = zoom;
	vp.left = 0;
	vp.top = 0;
	vp.virtual_width = width;
	vp.width = width >> zoom;
	vp.virtual_height = height;
	vp.height = height >> zoom;
	vp.virtual_left = left;
	vp.virtual_top = top;

	sf = _screenshot_formats + _cur_screenshot_format;
	return sf->proc(MakeScreenshotName(sf->extension), LargeWorldCallback, &vp, vp.width, vp.height, 8, _cur_palette);
}
