/* $Id$ */

/** @file gfx.h */

#ifndef GFX_H
#define GFX_H

#include "openttd.h"
#include "zoom.hpp"

enum WindowKeyCodes {
	WKC_SHIFT = 0x8000,
	WKC_CTRL  = 0x4000,
	WKC_ALT   = 0x2000,
	WKC_META  = 0x1000,

	/* Special ones */
	WKC_NONE        =  0,
	WKC_ESC         =  1,
	WKC_BACKSPACE   =  2,
	WKC_INSERT      =  3,
	WKC_DELETE      =  4,

	WKC_PAGEUP      =  5,
	WKC_PAGEDOWN    =  6,
	WKC_END         =  7,
	WKC_HOME        =  8,

	/* Arrow keys */
	WKC_LEFT        =  9,
	WKC_UP          = 10,
	WKC_RIGHT       = 11,
	WKC_DOWN        = 12,

	/* Return & tab */
	WKC_RETURN      = 13,
	WKC_TAB         = 14,

	/* Numerical keyboard */
	WKC_NUM_0       = 16,
	WKC_NUM_1       = 17,
	WKC_NUM_2       = 18,
	WKC_NUM_3       = 19,
	WKC_NUM_4       = 20,
	WKC_NUM_5       = 21,
	WKC_NUM_6       = 22,
	WKC_NUM_7       = 23,
	WKC_NUM_8       = 24,
	WKC_NUM_9       = 25,
	WKC_NUM_DIV     = 26,
	WKC_NUM_MUL     = 27,
	WKC_NUM_MINUS   = 28,
	WKC_NUM_PLUS    = 29,
	WKC_NUM_ENTER   = 30,
	WKC_NUM_DECIMAL = 31,

	/* Space */
	WKC_SPACE       = 32,

	/* Function keys */
	WKC_F1          = 33,
	WKC_F2          = 34,
	WKC_F3          = 35,
	WKC_F4          = 36,
	WKC_F5          = 37,
	WKC_F6          = 38,
	WKC_F7          = 39,
	WKC_F8          = 40,
	WKC_F9          = 41,
	WKC_F10         = 42,
	WKC_F11         = 43,
	WKC_F12         = 44,

	/* backquote is the key left of "1"
	 * we only store this key here, no matter what character is really mapped to it
	 * on a particular keyboard. (US keyboard: ` and ~ ; German keyboard: ^ and °) */
	WKC_BACKQUOTE   = 45,
	WKC_PAUSE       = 46,

	/* 0-9 are mapped to 48-57
	 * A-Z are mapped to 65-90
	 * a-z are mapped to 97-122 */
};

enum GameModes {
	GM_MENU,
	GM_NORMAL,
	GM_EDITOR
};

void GameLoop();

void CreateConsole();

typedef int32 CursorID;

struct Point {
	int x,y;
};

struct Rect {
	int left,top,right,bottom;
};

/** A single sprite of a list of animated cursors */
struct AnimCursor {
	static const CursorID LAST = MAX_UVALUE(CursorID);
	CursorID sprite;   ///< Must be set to LAST_ANIM when it is the last sprite of the loop
	byte display_time; ///< Amount of ticks this sprite will be shown
};

struct CursorVars {
	Point pos, size, offs, delta; ///< position, size, offset from top-left, and movement
	Point draw_pos, draw_size;    ///< position and size bounding-box for drawing
	SpriteID sprite; ///< current image of cursor
	SpriteID pal;

	int wheel;       ///< mouse wheel movement

	/* We need two different vars to keep track of how far the scrollwheel moved.
	 * OSX uses this for scrolling around the map. */
	int v_wheel;
	int h_wheel;

	const AnimCursor *animate_list; ///< in case of animated cursor, list of frames
	const AnimCursor *animate_cur;  ///< in case of animated cursor, current frame
	uint animate_timeout;           ///< in case of animated cursor, number of ticks to show the current cursor

	bool visible;    ///< cursor is visible
	bool dirty;      ///< the rect occupied by the mouse is dirty (redraw)
	bool fix_at;     ///< mouse is moving, but cursor is not (used for scrolling)
	bool in_window;  ///< mouse inside this window, determines drawing logic
};

struct DrawPixelInfo {
	void *dst_ptr;
	int left, top, width, height;
	int pitch;
	ZoomLevel zoom;
};

struct Colour {
	byte r;
	byte g;
	byte b;
};



extern byte _dirkeys;        ///< 1 = left, 2 = up, 4 = right, 8 = down
extern bool _fullscreen;
extern CursorVars _cursor;
extern bool _ctrl_pressed;   ///< Is Ctrl pressed?
extern bool _shift_pressed;  ///< Is Shift pressed?
extern byte _fast_forward;

extern bool _left_button_down;
extern bool _left_button_clicked;
extern bool _right_button_down;
extern bool _right_button_clicked;

extern DrawPixelInfo _screen;
extern bool _exit_game;
extern bool _networking;         ///< are we in networking mode?
extern byte _game_mode;
extern byte _pause_game;

extern int _pal_first_dirty;
extern int _pal_count_dirty;
extern int _num_resolutions;
extern uint16 _resolutions[32][2];
extern uint16 _cur_resolution[2];
extern Colour _cur_palette[256];

void HandleKeypress(uint32 key);
void HandleMouseEvents();
void CSleep(int milliseconds);
void UpdateWindows();

uint32 InteractiveRandom(); //< Used for random sequences that are not the same on the other end of the multiplayer link
uint InteractiveRandomRange(uint max);
void DrawTextMessage();
void DrawMouseCursor();
void ScreenSizeChanged();
void HandleExitGameRequest();
void GameSizeChanged();
void UndrawMouseCursor();

#include "helpers.hpp"

enum FontSize {
	FS_NORMAL,
	FS_SMALL,
	FS_LARGE,
	FS_END,
};

DECLARE_POSTFIX_INCREMENT(FontSize);

void RedrawScreenRect(int left, int top, int right, int bottom);
void GfxScroll(int left, int top, int width, int height, int xo, int yo);


/* XXX doesn't really belong here, but the only
 * consumers always use it in conjunction with DoDrawString() */
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
void LoadStringWidthTable();
void DrawStringMultiCenter(int x, int y, StringID str, int maxw);
uint DrawStringMultiLine(int x, int y, StringID str, int maxw, int maxh = -1);
void DrawDirtyBlocks();
void SetDirtyBlocks(int left, int top, int right, int bottom);
void MarkWholeScreenDirty();

void GfxInitPalettes();

bool FillDrawPixelInfo(DrawPixelInfo* n, int left, int top, int width, int height);

/* window.cpp */
void DrawOverlappedWindowForAll(int left, int top, int right, int bottom);

void SetMouseCursor(SpriteID sprite, SpriteID pal);
void SetAnimatedMouseCursor(const AnimCursor *table);
void CursorTick();
void DrawMouseCursor();
void ScreenSizeChanged();
void UndrawMouseCursor();
bool ChangeResInGame(int w, int h);
void SortResolutions(int count);
void ToggleFullScreen(bool fs);

/* gfx.cpp */
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

enum StringColorFlags {
	IS_PALETTE_COLOR = 0x100, ///< color value is already a real palette color index, not an index of a StringColor
};

#endif /* GFX_H */
