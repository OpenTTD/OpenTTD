#ifndef GFX_H
#define GFX_H


typedef struct ColorList {
	byte unk0, unk1, unk2;
	byte window_color_1a, window_color_1b;
	byte window_color_bga, window_color_bgb;
	byte window_color_2;
} ColorList;

struct DrawPixelInfo {
	byte *dst_ptr;
	int left, top, width, height;
	int pitch;
	uint16 zoom;
};


typedef struct SpriteHdr {
	byte info;
	byte height;
	uint16 width;
	int16 x_offs, y_offs;
} SpriteHdr;
assert_compile(sizeof(SpriteHdr) == 8);

typedef struct CursorVars {
	Point pos, size, offs, delta;
	Point draw_pos, draw_size;
	uint32 sprite;

	int wheel; // mouse wheel movement
	const uint16 *animate_list, *animate_cur;
	uint animate_timeout;

	bool visible;
	bool dirty;
	bool fix_at;
} CursorVars;


void RedrawScreenRect(int left, int top, int right, int bottom);
void GfxScroll(int left, int top, int width, int height, int xo, int yo);
int DrawStringCentered(int x, int y, uint16 str, byte color);
int DrawString(int x, int y, uint16 str, byte color);
void DrawStringCenterUnderline(int x, int y, uint16 str, byte color);
int DoDrawString(const byte *string, int x, int y, byte color);
void DrawStringRightAligned(int x, int y, uint16 str, byte color);
void GfxFillRect(int left, int top, int right, int bottom, int color);
void GfxDrawLine(int left, int top, int right, int bottom, int color);
void DrawFrameRect(int left, int top, int right, int bottom, int color, int flags);

int GetStringWidth(const byte *str);
void LoadStringWidthTable();
void DrawStringMultiCenter(int x, int y, uint16 str, int maxw);
void DrawStringMultiLine(int x, int y, uint16 str, int maxw);
void DrawDirtyBlocks();
void SetDirtyBlocks(int left, int top, int right, int bottom);
void MarkWholeScreenDirty();

void GfxInitPalettes();

bool FillDrawPixelInfo(DrawPixelInfo *n, DrawPixelInfo *o, int left, int top, int width, int height);

/* window.c */
void DrawOverlappedWindowForAll(int left, int top, int right, int bottom);

/* spritecache.c */
byte *GetSpritePtr(uint sprite);
void GfxInitSpriteMem(byte *ptr, uint32 size);
void GfxLoadSprites();

void SetMouseCursor(uint cursor);
void SetAnimatedMouseCursor(const uint16 *table);
void CursorTick();
void DrawMouseCursor();
void ScreenSizeChanged();
void UndrawMouseCursor();
bool ChangeResInGame(int w, int h);
void ToggleFullScreen(const bool full_screen);

typedef struct {
	int xoffs, yoffs;
	int xsize, ysize;
} SpriteDimension;

const SpriteDimension *GetSpriteDimension(uint sprite);

/* gfx.c */
VARDEF int _stringwidth_base;
VARDEF byte _stringwidth_table[0x2A0];

VARDEF DrawPixelInfo _screen;
VARDEF DrawPixelInfo *_cur_dpi;
VARDEF ColorList _color_list[16];
VARDEF CursorVars _cursor;

VARDEF int _pal_first_dirty;
VARDEF int _pal_last_dirty;

/* spritecache.c */
//enum { NUM_SPRITES = 0x1320 };
//enum { NUM_SPRITES = 0x1500 };
enum { NUM_SPRITES = 0x3500 }; // 1500 + space for custom GRF sets

/* tables.h */
extern byte _palettes[4][256 * 3];
VARDEF byte _cur_palette[768];

#endif
