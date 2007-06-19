/* $Id$ */

/** @file gfx.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "macros.h"
#include "spritecache.h"
#include "strings.h"
#include "string.h"
#include "gfx.h"
#include "table/palettes.h"
#include "table/sprites.h"
#include "hal.h"
#include "variables.h"
#include "table/control_codes.h"
#include "fontcache.h"
#include "genworld.h"
#include "debug.h"
#include "zoom.hpp"
#include "blitter/factory.hpp"

#ifdef _DEBUG
bool _dbg_screen_rect;
#endif

byte _dirkeys;        ///< 1 = left, 2 = up, 4 = right, 8 = down
bool _fullscreen;
CursorVars _cursor;
bool _ctrl_pressed;   ///< Is Ctrl pressed?
bool _shift_pressed;  ///< Is Shift pressed?
byte _fast_forward;
bool _left_button_down;
bool _left_button_clicked;
bool _right_button_down;
bool _right_button_clicked;
DrawPixelInfo _screen;
bool _exit_game;
bool _networking;         ///< are we in networking mode?
byte _game_mode;
byte _pause_game;
int _pal_first_dirty;
int _pal_count_dirty;

Colour _cur_palette[256];
byte _stringwidth_table[FS_END][224];

static void GfxMainBlitter(const Sprite *sprite, int x, int y, BlitterMode mode);

FontSize _cur_fontsize;
static FontSize _last_fontsize;
static uint8 _cursor_backup[64 * 64 * 4];
static Rect _invalid_rect;
static const byte *_color_remap_ptr;
static byte _string_colorremap[3];

#define DIRTY_BYTES_PER_LINE (MAX_SCREEN_WIDTH / 64)
static byte _dirty_blocks[DIRTY_BYTES_PER_LINE * MAX_SCREEN_HEIGHT / 8];

void GfxScroll(int left, int top, int width, int height, int xo, int yo)
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();

	if (xo == 0 && yo == 0) return;

	if (_cursor.visible) UndrawMouseCursor();
	UndrawTextMessage();

	blitter->ScrollBuffer(_screen.dst_ptr, left, top, width, height, xo, yo);
	/* This part of the screen is now dirty. */
	_video_driver->make_dirty(left, top, width, height);
}


void GfxFillRect(int left, int top, int right, int bottom, int color)
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	const DrawPixelInfo *dpi = _cur_dpi;
	void *dst;
	const int otop = top;
	const int oleft = left;

	if (dpi->zoom != ZOOM_LVL_NORMAL) return;
	if (left > right || top > bottom) return;
	if (right < dpi->left || left >= dpi->left + dpi->width) return;
	if (bottom < dpi->top || top >= dpi->top + dpi->height) return;

	if ( (left -= dpi->left) < 0) left = 0;
	right = right - dpi->left + 1;
	if (right > dpi->width) right = dpi->width;
	right -= left;
	assert(right > 0);

	if ( (top -= dpi->top) < 0) top = 0;
	bottom = bottom - dpi->top + 1;
	if (bottom > dpi->height) bottom = dpi->height;
	bottom -= top;
	assert(bottom > 0);

	dst = blitter->MoveTo(dpi->dst_ptr, left, top);

	if (!HASBIT(color, PALETTE_MODIFIER_GREYOUT)) {
		if (!HASBIT(color, USE_COLORTABLE)) {
			blitter->DrawRect(dst, right, bottom, (uint8)color);
		} else {
			blitter->DrawColorMappingRect(dst, right, bottom, GB(color, 0, PALETTE_WIDTH));
		}
	} else {
		byte bo = (oleft - left + dpi->left + otop - top + dpi->top) & 1;
		do {
			for (int i = (bo ^= 1); i < right; i += 2) blitter->SetPixel(dst, i, 0, (uint8)color);
			dst = blitter->MoveTo(dst, 0, 1);
		} while (--bottom > 0);
	}
}

void GfxDrawLine(int x, int y, int x2, int y2, int color)
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	DrawPixelInfo *dpi = _cur_dpi;

	x -= dpi->left;
	x2 -= dpi->left;
	y -= dpi->top;
	y2 -= dpi->top;

	/* Check clipping */
	if (x < 0 && x2 < 0) return;
	if (y < 0 && y2 < 0) return;
	if (x > dpi->width  && x2 > dpi->width)  return;
	if (y > dpi->height && y2 > dpi->height) return;

	blitter->DrawLine(dpi->dst_ptr, x, y, x2, y2, dpi->width, dpi->height, color);
}


/** Truncate a given string to a maximum width if neccessary.
 * If the string is truncated, add three dots ('...') to show this.
 * @param *str string that is checked and possibly truncated
 * @param maxw maximum width in pixels of the string
 * @return new width of (truncated) string */
static int TruncateString(char *str, int maxw)
{
	int w = 0;
	FontSize size = _cur_fontsize;
	int ddd, ddd_w;

	WChar c;
	char *ddd_pos;

	ddd_w = ddd = GetCharacterWidth(size, '.') * 3;

	for (ddd_pos = str; (c = Utf8Consume((const char **)&str)) != '\0'; ) {
		if (IsPrintable(c)) {
			w += GetCharacterWidth(size, c);

			if (w >= maxw) {
				/* string got too big... insert dotdotdot */
				ddd_pos[0] = ddd_pos[1] = ddd_pos[2] = '.';
				ddd_pos[3] = '\0';
				return ddd_w;
			}
		} else {
			if (c == SCC_SETX) str++;
			else if (c == SCC_SETXY) str += 2;
			else if (c == SCC_TINYFONT) {
				size = FS_SMALL;
				ddd = GetCharacterWidth(size, '.') * 3;
			} else if (c == SCC_BIGFONT) {
				size = FS_LARGE;
				ddd = GetCharacterWidth(size, '.') * 3;
			}
		}

		/* Remember the last position where three dots fit. */
		if (w + ddd < maxw) {
			ddd_w = w + ddd;
			ddd_pos = str;
		}
	}

	return w;
}

static inline int TruncateStringID(StringID src, char *dest, int maxw, const char* last)
{
	GetString(dest, src, last);
	return TruncateString(dest, maxw);
}

/* returns right coordinate */
int DrawString(int x, int y, StringID str, uint16 color)
{
	char buffer[512];

	GetString(buffer, str, lastof(buffer));
	return DoDrawString(buffer, x, y, color);
}

int DrawStringTruncated(int x, int y, StringID str, uint16 color, uint maxw)
{
	char buffer[512];
	TruncateStringID(str, buffer, maxw, lastof(buffer));
	return DoDrawString(buffer, x, y, color);
}


int DrawStringRightAligned(int x, int y, StringID str, uint16 color)
{
	char buffer[512];
	int w;

	GetString(buffer, str, lastof(buffer));
	w = GetStringBoundingBox(buffer).width;
	DoDrawString(buffer, x - w, y, color);

	return w;
}

void DrawStringRightAlignedTruncated(int x, int y, StringID str, uint16 color, uint maxw)
{
	char buffer[512];

	TruncateStringID(str, buffer, maxw, lastof(buffer));
	DoDrawString(buffer, x - GetStringBoundingBox(buffer).width, y, color);
}

void DrawStringRightAlignedUnderline(int x, int y, StringID str, uint16 color)
{
	int w = DrawStringRightAligned(x, y, str, color);
	GfxFillRect(x - w, y + 10, x, y + 10, _string_colorremap[1]);
}


int DrawStringCentered(int x, int y, StringID str, uint16 color)
{
	char buffer[512];
	int w;

	GetString(buffer, str, lastof(buffer));

	w = GetStringBoundingBox(buffer).width;
	DoDrawString(buffer, x - w / 2, y, color);

	return w;
}

int DrawStringCenteredTruncated(int xl, int xr, int y, StringID str, uint16 color)
{
	char buffer[512];
	int w = TruncateStringID(str, buffer, xr - xl, lastof(buffer));
	return DoDrawString(buffer, (xl + xr - w) / 2, y, color);
}

int DoDrawStringCentered(int x, int y, const char *str, uint16 color)
{
	int w = GetStringBoundingBox(str).width;
	DoDrawString(str, x - w / 2, y, color);
	return w;
}

void DrawStringCenterUnderline(int x, int y, StringID str, uint16 color)
{
	int w = DrawStringCentered(x, y, str, color);
	GfxFillRect(x - (w >> 1), y + 10, x - (w >> 1) + w, y + 10, _string_colorremap[1]);
}

void DrawStringCenterUnderlineTruncated(int xl, int xr, int y, StringID str, uint16 color)
{
	int w = DrawStringCenteredTruncated(xl, xr, y, str, color);
	GfxFillRect((xl + xr - w) / 2, y + 10, (xl + xr + w) / 2, y + 10, _string_colorremap[1]);
}

/** 'Correct' a string to a maximum length. Longer strings will be cut into
 * additional lines at whitespace characters if possible. The string parameter
 * is modified with terminating characters mid-string which are the
 * placeholders for the newlines.
 * The string WILL be truncated if there was no whitespace for the current
 * line's maximum width.
 *
 * @note To know if the terminating '\0' is the string end or just a
 * newline, the returned 'num' value should be consulted. The num'th '\0',
 * starting with index 0 is the real string end.
 *
 * @param str string to check and correct for length restrictions
 * @param maxw the maximum width the string can have on one line
 * @return return a 32bit wide number consisting of 2 packed values:
 *  0 - 15 the number of lines ADDED to the string
 * 16 - 31 the fontsize in which the length calculation was done at */
uint32 FormatStringLinebreaks(char *str, int maxw)
{
	FontSize size = _cur_fontsize;
	int num = 0;

	assert(maxw > 0);

	for (;;) {
		char *last_space = NULL;
		int w = 0;

		for (;;) {
			WChar c = Utf8Consume((const char **)&str);
			/* whitespace is where we will insert the line-break */
			if (IsWhitespace(c)) last_space = str;

			if (IsPrintable(c)) {
				w += GetCharacterWidth(size, c);
				/* string is longer than maximum width so we need to decide what to
				 * do. We can do two things:
				 * 1. If no whitespace was found at all up until now (on this line) then
				 *    we will truncate the string and bail out.
				 * 2. In all other cases force a linebreak at the last seen whitespace */
				if (w > maxw) {
					if (last_space == NULL) {
						*Utf8PrevChar(str) = '\0';
						return num + (size << 16);
					}
					str = last_space;
					break;
				}
			} else {
				switch (c) {
					case '\0': return num + (size << 16); break;
					case SCC_SETX:  str++; break;
					case SCC_SETXY: str += 2; break;
					case SCC_TINYFONT: size = FS_SMALL; break;
					case SCC_BIGFONT:  size = FS_LARGE; break;
					case '\n': goto end_of_inner_loop;
				}
			}
		}
end_of_inner_loop:
		/* String didn't fit on line (or a '\n' was encountered), so 'dummy' terminate
		 * and increase linecount. We use Utf8PrevChar() as also non 1 char long
		 * whitespace seperators are supported */
		num++;
		char *s = Utf8PrevChar(str);
		*s++ = '\0';

		/* In which case (see above) we will shift remainder to left and close the gap */
		if (str - s >= 1) {
			for (; str[-1] != '\0';) *s++ = *str++;
		}
	}
}

/** Draw a given string with the centre around the given x coordinates
 * @param x Centre the string around this pixel width
 * @param y Draw the string at this pixel height (first line's bottom)
 * @param str String to draw
 * @param maxw Maximum width the string can have before it is wrapped */
void DrawStringMultiCenter(int x, int y, StringID str, int maxw)
{
	char buffer[512];
	uint32 tmp;
	int num, w, mt;
	const char *src;
	WChar c;

	GetString(buffer, str, lastof(buffer));

	tmp = FormatStringLinebreaks(buffer, maxw);
	num = GB(tmp, 0, 16);

	mt = GetCharacterHeight((FontSize)GB(tmp, 16, 16));

	y -= (mt >> 1) * num;

	src = buffer;

	for (;;) {
		w = GetStringBoundingBox(src).width;
		DoDrawString(src, x - (w >> 1), y, 0xFE);
		_cur_fontsize = _last_fontsize;

		for (;;) {
			c = Utf8Consume(&src);
			if (c == 0) {
				y += mt;
				if (--num < 0) {
					_cur_fontsize = FS_NORMAL;
					return;
				}
				break;
			} else if (c == SCC_SETX) {
				src++;
			} else if (c == SCC_SETXY) {
				src += 2;
			}
		}
	}
}


uint DrawStringMultiLine(int x, int y, StringID str, int maxw, int maxh)
{
	char buffer[512];
	uint32 tmp;
	int num, mt;
	uint total_height;
	const char *src;
	WChar c;

	GetString(buffer, str, lastof(buffer));

	tmp = FormatStringLinebreaks(buffer, maxw);
	num = GB(tmp, 0, 16);

	mt = GetCharacterHeight((FontSize)GB(tmp, 16, 16));
	total_height = (num + 1) * mt;

	if (maxh != -1 && (int)total_height > maxh) {
		/* Check there's room enough for at least one line. */
		if (maxh < mt) return 0;

		num = maxh / mt - 1;
		total_height = (num + 1) * mt;
	}

	src = buffer;

	for (;;) {
		DoDrawString(src, x, y, 0xFE);
		_cur_fontsize = _last_fontsize;

		for (;;) {
			c = Utf8Consume(&src);
			if (c == 0) {
				y += mt;
				if (--num < 0) {
					_cur_fontsize = FS_NORMAL;
					return total_height;
				}
				break;
			} else if (c == SCC_SETX) {
				src++;
			} else if (c == SCC_SETXY) {
				src += 2;
			}
		}
	}
}

/** Return the string dimension in pixels. The height and width are returned
 * in a single BoundingRect value. TINYFONT, BIGFONT modifiers are only
 * supported as the first character of the string. The returned dimensions
 * are therefore a rough estimation correct for all the current strings
 * but not every possible combination
 * @param str string to calculate pixel-width
 * @return string width and height in pixels */
BoundingRect GetStringBoundingBox(const char *str)
{
	FontSize size = _cur_fontsize;
	BoundingRect br;
	int max_width;
	WChar c;

	br.width = br.height = max_width = 0;
	for (;;) {
		c = Utf8Consume(&str);
		if (c == 0) break;
		if (IsPrintable(c)) {
			br.width += GetCharacterWidth(size, c);
		} else {
			switch (c) {
				case SCC_SETX: br.width += (byte)*str++; break;
				case SCC_SETXY:
					br.width += (byte)*str++;
					br.height += (byte)*str++;
					break;
				case SCC_TINYFONT: size = FS_SMALL; break;
				case SCC_BIGFONT:  size = FS_LARGE; break;
				case '\n':
					br.height += GetCharacterHeight(size);
					if (br.width > max_width) max_width = br.width;
					br.width = 0;
					break;
			}
		}
	}
	br.height += GetCharacterHeight(size);

	br.width  = max(br.width, max_width);
	return br;
}

/** Draw a string at the given coordinates with the given colour
 * @param string the string to draw
 * @param x offset from left side of the screen, if negative offset from the right side
 * @param y offset from top side of the screen, if negative offset from the bottom
 * @param real_color colour of the string, see _string_colormap in
 * table/palettes.h or docs/ottd-colourtext-palette.png
 * @return the x-coordinates where the drawing has finished. If nothing is drawn
 * the originally passed x-coordinate is returned */
int DoDrawString(const char *string, int x, int y, uint16 real_color)
{
	DrawPixelInfo *dpi = _cur_dpi;
	FontSize size = _cur_fontsize;
	WChar c;
	byte color;
	int xo = x, yo = y;

	color = real_color & 0xFF;

	if (color != 0xFE) {
		if (x >= dpi->left + dpi->width ||
				x + _screen.width * 2 <= dpi->left ||
				y >= dpi->top + dpi->height ||
				y + _screen.height <= dpi->top)
					return x;

		if (color != 0xFF) {
switch_color:;
			if (real_color & IS_PALETTE_COLOR) {
				_string_colorremap[1] = color;
				_string_colorremap[2] = _use_dos_palette ? 1 : 215;
			} else {
				uint palette = _use_dos_palette ? 1 : 0;
				_string_colorremap[1] = _string_colormap[palette][color].text;
				_string_colorremap[2] = _string_colormap[palette][color].shadow;
			}
			_color_remap_ptr = _string_colorremap;
		}
	}

check_bounds:
	if (y + 19 <= dpi->top || dpi->top + dpi->height <= y) {
skip_char:;
		for (;;) {
			c = Utf8Consume(&string);
			if (!IsPrintable(c)) goto skip_cont;
		}
	}

	for (;;) {
		c = Utf8Consume(&string);
skip_cont:;
		if (c == 0) {
			_last_fontsize = size;
			return x;
		}
		if (IsPrintable(c)) {
			if (x >= dpi->left + dpi->width) goto skip_char;
			if (x + 26 >= dpi->left) {
				GfxMainBlitter(GetGlyph(size, c), x, y, BM_COLOUR_REMAP);
			}
			x += GetCharacterWidth(size, c);
		} else if (c == '\n') { // newline = {}
			x = xo;
			y += GetCharacterHeight(size);
			goto check_bounds;
		} else if (c >= SCC_BLUE && c <= SCC_BLACK) { // change color?
			color = (byte)(c - SCC_BLUE);
			goto switch_color;
		} else if (c == SCC_SETX) { // {SETX}
			x = xo + (byte)*string++;
		} else if (c == SCC_SETXY) {// {SETXY}
			x = xo + (byte)*string++;
			y = yo + (byte)*string++;
		} else if (c == SCC_TINYFONT) { // {TINYFONT}
			size = FS_SMALL;
		} else if (c == SCC_BIGFONT) { // {BIGFONT}
			size = FS_LARGE;
		} else {
			DEBUG(misc, 0, "[utf8] unknown string command character %d", c);
		}
	}
}

int DoDrawStringTruncated(const char *str, int x, int y, uint16 color, uint maxw)
{
	char buffer[512];
	ttd_strlcpy(buffer, str, sizeof(buffer));
	TruncateString(buffer, maxw);
	return DoDrawString(buffer, x, y, color);
}

void DrawSprite(SpriteID img, SpriteID pal, int x, int y)
{
	if (HASBIT(img, PALETTE_MODIFIER_TRANSPARENT)) {
		_color_remap_ptr = GetNonSprite(GB(pal, 0, PALETTE_WIDTH)) + 1;
		GfxMainBlitter(GetSprite(GB(img, 0, SPRITE_WIDTH)), x, y, BM_TRANSPARENT);
	} else if (pal != PAL_NONE) {
		_color_remap_ptr = GetNonSprite(GB(pal, 0, PALETTE_WIDTH)) + 1;
		GfxMainBlitter(GetSprite(GB(img, 0, SPRITE_WIDTH)), x, y, BM_COLOUR_REMAP);
	} else {
		GfxMainBlitter(GetSprite(GB(img, 0, SPRITE_WIDTH)), x, y, BM_NORMAL);
	}
}

static inline void GfxMainBlitter(const Sprite *sprite, int x, int y, BlitterMode mode)
{
	const DrawPixelInfo *dpi = _cur_dpi;
	Blitter::BlitterParams bp;

	/* Move to the correct offset */
	x += sprite->x_offs;
	y += sprite->y_offs;

	/* Copy the main data directly from the sprite */
	bp.sprite = sprite->data;
	bp.sprite_width = sprite->width;
	bp.sprite_height = sprite->height;
	bp.width = UnScaleByZoom(sprite->width, dpi->zoom);
	bp.height = UnScaleByZoom(sprite->height, dpi->zoom);
	bp.top = 0;
	bp.left = 0;
	bp.skip_left = 0;
	bp.skip_top = 0;
	bp.dst = dpi->dst_ptr;
	bp.pitch = dpi->pitch;
	bp.remap = _color_remap_ptr;

	assert(sprite->width > 0);
	assert(sprite->height > 0);

	if (bp.width <= 0) return;
	if (bp.height <= 0) return;

	y -= dpi->top;
	/* Check for top overflow */
	if (y < 0) {
		bp.height -= -UnScaleByZoom(y, dpi->zoom);
		if (bp.height <= 0) return;
		bp.skip_top += -UnScaleByZoom(y, dpi->zoom);
		y = 0;
	} else {
		bp.top = UnScaleByZoom(y, dpi->zoom);
	}

	/* Check for bottom overflow */
	y += ScaleByZoom(bp.height, dpi->zoom) - dpi->height;
	if (y > 0) {
		bp.height -= UnScaleByZoom(y, dpi->zoom);
		if (bp.height <= 0) return;
	}

	x -= dpi->left;
	/* Check for left overflow */
	if (x < 0) {
		bp.width -= -UnScaleByZoom(x, dpi->zoom);
		if (bp.width <= 0) return;
		bp.skip_left += -UnScaleByZoom(x, dpi->zoom);
		x = 0;
	} else {
		bp.left = UnScaleByZoom(x, dpi->zoom);
	}

	/* Check for right overflow */
	x += ScaleByZoom(bp.width, dpi->zoom) - dpi->width;
	if (x > 0) {
		bp.width -= UnScaleByZoom(x, dpi->zoom);
		if (bp.width <= 0) return;
	}

	BlitterFactoryBase::GetCurrentBlitter()->Draw(&bp, mode, dpi->zoom);
}

void DoPaletteAnimations();

void GfxInitPalettes()
{
	memcpy(_cur_palette, _palettes[_use_dos_palette ? 1 : 0], sizeof(_cur_palette));

	DoPaletteAnimations();
	_pal_first_dirty = 0;
	_pal_count_dirty = 255;
}

#define EXTR(p, q) (((uint16)(_timer_counter * (p)) * (q)) >> 16)
#define EXTR2(p, q) (((uint16)(~_timer_counter * (p)) * (q)) >> 16)

void DoPaletteAnimations()
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	const Colour *s;
	Colour *d;
	/* Amount of colors to be rotated.
	 * A few more for the DOS palette, because the water colors are
	 * 245-254 for DOS and 217-226 for Windows.  */
	const ExtraPaletteValues *ev = &_extra_palette_values;
	int c = _use_dos_palette ? 38 : 28;
	Colour old_val[38];
	uint i;
	uint j;
	uint old_tc = _timer_counter;

	if (blitter != NULL && blitter->UsePaletteAnimation() == Blitter::PALETTE_ANIMATION_NONE) {
		_timer_counter = 0;
	}

	d = &_cur_palette[217];
	memcpy(old_val, d, c * sizeof(*old_val));

	/* Dark blue water */
	s = (_opt.landscape == LT_TOYLAND) ? ev->ac : ev->a;
	j = EXTR(320, 5);
	for (i = 0; i != 5; i++) {
		*d++ = s[j];
		j++;
		if (j == 5) j = 0;
	}

	/* Glittery water */
	s = (_opt.landscape == LT_TOYLAND) ? ev->bc : ev->b;
	j = EXTR(128, 15);
	for (i = 0; i != 5; i++) {
		*d++ = s[j];
		j += 3;
		if (j >= 15) j -= 15;
	}

	s = ev->e;
	j = EXTR2(512, 5);
	for (i = 0; i != 5; i++) {
		*d++ = s[j];
		j++;
		if (j == 5) j = 0;
	}

	/* Oil refinery fire animation */
	s = ev->oil_ref;
	j = EXTR2(512, 7);
	for (i = 0; i != 7; i++) {
		*d++ = s[j];
		j++;
		if (j == 7) j = 0;
	}

	/* Radio tower blinking */
	{
		byte i = (_timer_counter >> 1) & 0x7F;
		byte v;

		(v = 255, i < 0x3f) ||
		(v = 128, i < 0x4A || i >= 0x75) ||
		(v = 20);
		d->r = v;
		d->g = 0;
		d->b = 0;
		d++;

		i ^= 0x40;
		(v = 255, i < 0x3f) ||
		(v = 128, i < 0x4A || i >= 0x75) ||
		(v = 20);
		d->r = v;
		d->g = 0;
		d->b = 0;
		d++;
	}

	/* Handle lighthouse and stadium animation */
	s = ev->lighthouse;
	j = EXTR(256, 4);
	for (i = 0; i != 4; i++) {
		*d++ = s[j];
		j++;
		if (j == 4) j = 0;
	}

	/* Animate water for old DOS graphics */
	if (_use_dos_palette) {
		/* Dark blue water DOS */
		s = (_opt.landscape == LT_TOYLAND) ? ev->ac : ev->a;
		j = EXTR(320, 5);
		for (i = 0; i != 5; i++) {
			*d++ = s[j];
			j++;
			if (j == 5) j = 0;
		}

		/* Glittery water DOS */
		s = (_opt.landscape == LT_TOYLAND) ? ev->bc : ev->b;
		j = EXTR(128, 15);
		for (i = 0; i != 5; i++) {
			*d++ = s[j];
			j += 3;
			if (j >= 15) j -= 15;
		}
	}

	if (blitter != NULL && blitter->UsePaletteAnimation() == Blitter::PALETTE_ANIMATION_NONE) {
		_timer_counter = old_tc;
	} else {
		if (memcmp(old_val, &_cur_palette[217], c * sizeof(*old_val)) != 0) {
			_pal_first_dirty = 217;
			_pal_count_dirty = c;
		}
	}
}


void LoadStringWidthTable()
{
	uint i;

	/* Normal font */
	for (i = 0; i != 224; i++) {
		_stringwidth_table[FS_NORMAL][i] = GetGlyphWidth(FS_NORMAL, i + 32);
	}

	/* Small font */
	for (i = 0; i != 224; i++) {
		_stringwidth_table[FS_SMALL][i] = GetGlyphWidth(FS_SMALL, i + 32);
	}

	/* Large font */
	for (i = 0; i != 224; i++) {
		_stringwidth_table[FS_LARGE][i] = GetGlyphWidth(FS_LARGE, i + 32);
	}
}


byte GetCharacterWidth(FontSize size, WChar key)
{
	if (key >= 32 && key < 256) return _stringwidth_table[size][key - 32];

	return GetGlyphWidth(size, key);
}


void ScreenSizeChanged()
{
	/* check the dirty rect */
	if (_invalid_rect.right >= _screen.width) _invalid_rect.right = _screen.width;
	if (_invalid_rect.bottom >= _screen.height) _invalid_rect.bottom = _screen.height;

	/* screen size changed and the old bitmap is invalid now, so we don't want to undraw it */
	_cursor.visible = false;
}

void UndrawMouseCursor()
{
	if (_cursor.visible) {
		Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
		_cursor.visible = false;
		blitter->CopyFromBuffer(blitter->MoveTo(_screen.dst_ptr, _cursor.draw_pos.x, _cursor.draw_pos.y), _cursor_backup, _cursor.draw_size.x, _cursor.draw_size.y, _cursor.draw_size.x);
		_video_driver->make_dirty(_cursor.draw_pos.x, _cursor.draw_pos.y, _cursor.draw_size.x, _cursor.draw_size.y);
	}
}

void DrawMouseCursor()
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	int x;
	int y;
	int w;
	int h;

	/* Redraw mouse cursor but only when it's inside the window */
	if (!_cursor.in_window) return;

	/* Don't draw the mouse cursor if it's already drawn */
	if (_cursor.visible) {
		if (!_cursor.dirty) return;
		UndrawMouseCursor();
	}

	w = _cursor.size.x;
	x = _cursor.pos.x + _cursor.offs.x;
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (w > _screen.width - x) w = _screen.width - x;
	if (w <= 0) return;
	_cursor.draw_pos.x = x;
	_cursor.draw_size.x = w;

	h = _cursor.size.y;
	y = _cursor.pos.y + _cursor.offs.y;
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (h > _screen.height - y) h = _screen.height - y;
	if (h <= 0) return;
	_cursor.draw_pos.y = y;
	_cursor.draw_size.y = h;

	assert(blitter->BufferSize(w, h) < (int)sizeof(_cursor_backup));

	/* Make backup of stuff below cursor */
	blitter->CopyToBuffer(blitter->MoveTo(_screen.dst_ptr, _cursor.draw_pos.x, _cursor.draw_pos.y), _cursor_backup, _cursor.draw_size.x, _cursor.draw_size.y, _cursor.draw_size.x);

	/* Draw cursor on screen */
	_cur_dpi = &_screen;
	DrawSprite(_cursor.sprite, _cursor.pal, _cursor.pos.x, _cursor.pos.y);

	_video_driver->make_dirty(_cursor.draw_pos.x, _cursor.draw_pos.y, _cursor.draw_size.x, _cursor.draw_size.y);

	_cursor.visible = true;
	_cursor.dirty = false;
}

#if defined(_DEBUG)
static void DbgScreenRect(int left, int top, int right, int bottom)
{
	DrawPixelInfo dp;
	DrawPixelInfo *old;

	old = _cur_dpi;
	_cur_dpi = &dp;
	dp = _screen;
	GfxFillRect(left, top, right - 1, bottom - 1, rand() & 255);
	_cur_dpi = old;
}
#endif

void RedrawScreenRect(int left, int top, int right, int bottom)
{
	assert(right <= _screen.width && bottom <= _screen.height);
	if (_cursor.visible) {
		if (right > _cursor.draw_pos.x &&
				left < _cursor.draw_pos.x + _cursor.draw_size.x &&
				bottom > _cursor.draw_pos.y &&
				top < _cursor.draw_pos.y + _cursor.draw_size.y) {
			UndrawMouseCursor();
		}
	}
	UndrawTextMessage();

#if defined(_DEBUG)
	if (_dbg_screen_rect)
		DbgScreenRect(left, top, right, bottom);
	else
#endif
		DrawOverlappedWindowForAll(left, top, right, bottom);

	_video_driver->make_dirty(left, top, right - left, bottom - top);
}

void DrawDirtyBlocks()
{
	byte *b = _dirty_blocks;
	const int w = ALIGN(_screen.width, 64);
	const int h = ALIGN(_screen.height, 8);
	int x;
	int y;

	if (IsGeneratingWorld() && !IsGeneratingWorldReadyForPaint()) return;

	y = 0;
	do {
		x = 0;
		do {
			if (*b != 0) {
				int left;
				int top;
				int right = x + 64;
				int bottom = y;
				byte *p = b;
				int h2;

				/* First try coalescing downwards */
				do {
					*p = 0;
					p += DIRTY_BYTES_PER_LINE;
					bottom += 8;
				} while (bottom != h && *p != 0);

				/* Try coalescing to the right too. */
				h2 = (bottom - y) >> 3;
				assert(h2 > 0);
				p = b;

				while (right != w) {
					byte *p2 = ++p;
					int h = h2;
					/* Check if a full line of dirty flags is set. */
					do {
						if (!*p2) goto no_more_coalesc;
						p2 += DIRTY_BYTES_PER_LINE;
					} while (--h != 0);

					/* Wohoo, can combine it one step to the right!
					 * Do that, and clear the bits. */
					right += 64;

					h = h2;
					p2 = p;
					do {
						*p2 = 0;
						p2 += DIRTY_BYTES_PER_LINE;
					} while (--h != 0);
				}
				no_more_coalesc:

				left = x;
				top = y;

				if (left   < _invalid_rect.left  ) left   = _invalid_rect.left;
				if (top    < _invalid_rect.top   ) top    = _invalid_rect.top;
				if (right  > _invalid_rect.right ) right  = _invalid_rect.right;
				if (bottom > _invalid_rect.bottom) bottom = _invalid_rect.bottom;

				if (left < right && top < bottom) {
					RedrawScreenRect(left, top, right, bottom);
				}

			}
		} while (b++, (x += 64) != w);
	} while (b += -(w >> 6) + DIRTY_BYTES_PER_LINE, (y += 8) != h);

	_invalid_rect.left = w;
	_invalid_rect.top = h;
	_invalid_rect.right = 0;
	_invalid_rect.bottom = 0;

	/* If we are generating a world, and waiting for a paint run, mark it here
	 *  as done painting, so we can continue generating. */
	if (IsGeneratingWorld() && IsGeneratingWorldReadyForPaint()) {
		SetGeneratingWorldPaintStatus(false);
	}
}


void SetDirtyBlocks(int left, int top, int right, int bottom)
{
	byte *b;
	int width;
	int height;

	if (left < 0) left = 0;
	if (top < 0) top = 0;
	if (right > _screen.width) right = _screen.width;
	if (bottom > _screen.height) bottom = _screen.height;

	if (left >= right || top >= bottom) return;

	if (left   < _invalid_rect.left  ) _invalid_rect.left   = left;
	if (top    < _invalid_rect.top   ) _invalid_rect.top    = top;
	if (right  > _invalid_rect.right ) _invalid_rect.right  = right;
	if (bottom > _invalid_rect.bottom) _invalid_rect.bottom = bottom;

	left >>= 6;
	top  >>= 3;

	b = _dirty_blocks + top * DIRTY_BYTES_PER_LINE + left;

	width  = ((right  - 1) >> 6) - left + 1;
	height = ((bottom - 1) >> 3) - top  + 1;

	assert(width > 0 && height > 0);

	do {
		int i = width;

		do b[--i] = 0xFF; while (i);

		b += DIRTY_BYTES_PER_LINE;
	} while (--height != 0);
}

void MarkWholeScreenDirty()
{
	SetDirtyBlocks(0, 0, _screen.width, _screen.height);
}

/** Set up a clipping area for only drawing into a certain area. To do this,
 * Fill a DrawPixelInfo object with the supplied relative rectangle, backup
 * the original (calling) _cur_dpi and assign the just returned DrawPixelInfo
 * _cur_dpi. When you are done, give restore _cur_dpi's original value
 * @param *n the DrawPixelInfo that will be the clipping rectangle box allowed
 * for drawing
 * @param left,top,width,height the relative coordinates of the clipping
 * rectangle relative to the current _cur_dpi. This will most likely be the
 * offset from the calling window coordinates
 * @return return false if the requested rectangle is not possible with the
 * current dpi pointer. Only continue of the return value is true, or you'll
 * get some nasty results */
bool FillDrawPixelInfo(DrawPixelInfo *n, int left, int top, int width, int height)
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	const DrawPixelInfo *o = _cur_dpi;

	n->zoom = ZOOM_LVL_NORMAL;

	assert(width > 0);
	assert(height > 0);

	if ((left -= o->left) < 0) {
		width += left;
		if (width <= 0) return false;
		n->left = -left;
		left = 0;
	} else {
		n->left = 0;
	}

	if (width > o->width - left) {
		width = o->width - left;
		if (width <= 0) return false;
	}
	n->width = width;

	if ((top -= o->top) < 0) {
		height += top;
		if (height <= 0) return false;
		n->top = -top;
		top = 0;
	} else {
		n->top = 0;
	}

	n->dst_ptr = blitter->MoveTo(o->dst_ptr, left, top);
	n->pitch = o->pitch;

	if (height > o->height - top) {
		height = o->height - top;
		if (height <= 0) return false;
	}
	n->height = height;

	return true;
}

static void SetCursorSprite(SpriteID cursor, SpriteID pal)
{
	CursorVars *cv = &_cursor;
	const Sprite *p;

	if (cv->sprite == cursor) return;

	p = GetSprite(GB(cursor, 0, SPRITE_WIDTH));
	cv->sprite = cursor;
	cv->pal    = pal;
	cv->size.y = p->height;
	cv->size.x = p->width;
	cv->offs.x = p->x_offs;
	cv->offs.y = p->y_offs;

	cv->dirty = true;
}

static void SwitchAnimatedCursor()
{
	const AnimCursor *cur = _cursor.animate_cur;

	if (cur == NULL || cur->sprite == AnimCursor::LAST) cur = _cursor.animate_list;

	SetCursorSprite(cur->sprite, _cursor.pal);

	_cursor.animate_timeout = cur->display_time;
	_cursor.animate_cur     = cur + 1;
}

void CursorTick()
{
	if (_cursor.animate_timeout != 0 && --_cursor.animate_timeout == 0)
		SwitchAnimatedCursor();
}

void SetMouseCursor(SpriteID sprite, SpriteID pal)
{
	/* Turn off animation */
	_cursor.animate_timeout = 0;
	/* Set cursor */
	SetCursorSprite(sprite, pal);
}

void SetAnimatedMouseCursor(const AnimCursor *table)
{
	_cursor.animate_list = table;
	_cursor.animate_cur = NULL;
	_cursor.pal = PAL_NONE;
	SwitchAnimatedCursor();
}

bool ChangeResInGame(int w, int h)
{
	return
		(_screen.width == w && _screen.height == h) ||
		_video_driver->change_resolution(w, h);
}

void ToggleFullScreen(bool fs)
{
	_video_driver->toggle_fullscreen(fs);
	if (_fullscreen != fs && _num_resolutions == 0) {
		DEBUG(driver, 0, "Could not find a suitable fullscreen resolution");
	}
}

static int CDECL compare_res(const void *pa, const void *pb)
{
	int x = ((const uint16*)pa)[0] - ((const uint16*)pb)[0];
	if (x != 0) return x;
	return ((const uint16*)pa)[1] - ((const uint16*)pb)[1];
}

void SortResolutions(int count)
{
	qsort(_resolutions, count, sizeof(_resolutions[0]), compare_res);
}
