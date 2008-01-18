/* $Id$ */

/** @file gfx_func.h Functions related to the gfx engine. */

/**
 * @defgroup dirty Dirty
 *
 * Handles the repaint of some part of the screen.
 *
 * Some places in the code are called functions which makes something "dirty".
 * This has nothing to do with making a Tile or Window darker or less visible.
 * This term comes from memory caching and is used to define an object must
 * be repaint. If some data of an object (like a Tile, Window, Vehicle, whatever)
 * are changed which are so extensive the object must be repaint its marked
 * as "dirty". The video driver repaint this object instead of the whole screen
 * (this is btw. also possible if needed). This is used to avoid a
 * flickering of the screen by the video driver constantly repainting it.
 *
 * This whole mechanism is controlled by an rectangle defined in #_invalid_rect. This
 * rectangle defines the area on the screen which must be repaint. If a new object
 * needs to be repainted this rectangle is extended to 'catch' the object on the
 * screen. At some point (which is normaly uninteressted for patch writers) this
 * rectangle is send to the video drivers method
 * VideoDriver::MakeDirty and it is truncated back to an empty rectangle. At some
 * later point (which is uninteressted, too) the video driver
 * repaints all these saved rectangle instead of the whole screen and drop the
 * rectangle informations. Then a new round begins by marking objects "dirty".
 *
 * @see VideoDriver::MakeDirty
 * @see _invalid_rect
 * @see _screen
 */


#ifndef GFX_FUNC_H
#define GFX_FUNC_H

#include "gfx_type.h"
#include "strings_type.h"

void GameLoop();

void CreateConsole();

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
extern bool _screen_disable_anim;   ///< Disable palette animation (important for 32bpp-anim blitter during giant screenshot)

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
void DrawChatMessage();
void DrawMouseCursor();
void ScreenSizeChanged();
void HandleExitGameRequest();
void GameSizeChanged();
void UndrawMouseCursor();

void RedrawScreenRect(int left, int top, int right, int bottom);
void GfxScroll(int left, int top, int width, int height, int xo, int yo);

void DrawSprite(SpriteID img, SpriteID pal, int x, int y, const SubSprite *sub = NULL);

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
void DrawBox(int x, int y, int dx1, int dy1, int dx2, int dy2, int dx3, int dy3);

Dimension GetStringBoundingBox(const char *str);
uint32 FormatStringLinebreaks(char *str, int maxw);
void LoadStringWidthTable();
void DrawStringMultiCenter(int x, int y, StringID str, int maxw);
uint DrawStringMultiLine(int x, int y, StringID str, int maxw, int maxh = -1);

/**
 * Let the dirty blocks repainting by the video driver.
 *
 * @ingroup dirty
 */
void DrawDirtyBlocks();

/**
 * Set a new dirty block.
 *
 * @ingroup dirty
 */
void SetDirtyBlocks(int left, int top, int right, int bottom);

/**
 * Marks the whole screen as dirty.
 *
 * @ingroup dirty
 */
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
bool ToggleFullScreen(bool fs);

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

extern DrawPixelInfo *_cur_dpi;

/**
 * All 16 colour gradients
 * 8 colours per gradient from darkest (0) to lightest (7)
 */
extern byte _colour_gradient[16][8];

extern bool _use_dos_palette;

#endif /* GFX_FUNC_H */
