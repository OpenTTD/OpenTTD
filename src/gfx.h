/* $Id$ */

#ifndef GFX_H
#define GFX_H

#include "helpers.hpp"

typedef enum FontSizes {
	FS_NORMAL,
	FS_SMALL,
	FS_LARGE,
	FS_END,
} FontSize;

DECLARE_POSTFIX_INCREMENT(FontSize);

void RedrawScreenRect(int left, int top, int right, int bottom);
void GfxScroll(int left, int top, int width, int height, int xo, int yo);


// XXX doesn't really belong here, but the only
// consumers always use it in conjunction with DoDrawString()
#define UPARROW   "\xEE\x8A\x80"
#define DOWNARROW "\xEE\x8A\xAA"


int DrawStringCentered(int x, int y, StringID str, uint16 color);
int DrawStringCenteredTruncated(int xl, int xr, int y, StringID str, uint16 color);
int DoDrawStringCentered(int x, int y, const char *str, uint16 color);

int DrawString(int x, int y, StringID str, uint16 color);
int DrawStringTruncated(int x, int y, StringID str, uint16 color, uint maxw);

int DoDrawString(const char *string, int x, int y, uint16 color);
int DoDrawStringTruncated(const char *str, int x, int y, uint16 color, uint maxw);

void DrawStringCenterUnderline(int x, int y, StringID str, uint16 color);
void DrawStringCenterUnderlineTruncated(int xl, int xr, int y, StringID str, uint16 color);

int DrawStringRightAligned(int x, int y, StringID str, uint16 color);
void DrawStringRightAlignedTruncated(int x, int y, StringID str, uint16 color, uint maxw);
void DrawStringRightAlignedUnderline(int x, int y, StringID str, uint16 color);

void GfxFillRect(int left, int top, int right, int bottom, int color);
void GfxDrawLine(int left, int top, int right, int bottom, int color);

BoundingRect GetStringBoundingBox(const char *str);
uint32 FormatStringLinebreaks(char *str, int maxw);
void LoadStringWidthTable(void);
void DrawStringMultiCenter(int x, int y, StringID str, int maxw);
uint DrawStringMultiLine(int x, int y, StringID str, int maxw);
void DrawDirtyBlocks(void);
void SetDirtyBlocks(int left, int top, int right, int bottom);
void MarkWholeScreenDirty(void);

void GfxInitPalettes(void);

bool FillDrawPixelInfo(DrawPixelInfo* n, int left, int top, int width, int height);

/* window.c */
void DrawOverlappedWindowForAll(int left, int top, int right, int bottom);

void SetMouseCursor(CursorID cursor);
void SetAnimatedMouseCursor(const CursorID *table);
void CursorTick(void);
void DrawMouseCursor(void);
void ScreenSizeChanged(void);
void UndrawMouseCursor(void);
bool ChangeResInGame(int w, int h);
void SortResolutions(int count);
extern "C" void ToggleFullScreen(bool fs);

/* gfx.c */
#define ASCII_LETTERSTART 32
extern FontSize _cur_fontsize;

byte GetCharacterWidth(FontSize size, uint32 key);

static inline byte GetCharacterHeight(FontSize size)
{
	switch (size) {
		default: NOT_REACHED();
		case FS_NORMAL: return 10;
		case FS_SMALL:  return 6;
		case FS_LARGE:  return 18;
	}
}

VARDEF DrawPixelInfo *_cur_dpi;

enum {
	COLOUR_DARK_BLUE,
	COLOUR_PALE_GREEN,
	COLOUR_PINK,
	COLOUR_YELLOW,
	COLOUR_RED,
	COLOUR_LIGHT_BLUE,
	COLOUR_GREEN,
	COLOUR_DARK_GREEN,
	COLOUR_BLUE,
	COLOUR_CREAM,
	COLOUR_MAUVE,
	COLOUR_PURPLE,
	COLOUR_ORANGE,
	COLOUR_BROWN,
	COLOUR_GREY,
	COLOUR_WHITE
};

/**
 * All 16 colour gradients
 * 8 colours per gradient from darkest (0) to lightest (7)
 */
VARDEF byte _colour_gradient[16][8];

VARDEF bool _use_dos_palette;

typedef enum StringColorFlags {
	IS_PALETTE_COLOR = 0x100, // color value is already a real palette color index, not an index of a StringColor
} StringColorFlags;

#ifdef _DEBUG
extern bool _dbg_screen_rect;
#endif

#endif /* GFX_H */
