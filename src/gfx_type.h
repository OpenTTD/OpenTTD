/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gfx_type.h Types related to the graphics and/or input devices. */

#ifndef GFX_TYPE_H
#define GFX_TYPE_H

#include "core/endian_type.hpp"
#include "core/geometry_type.hpp"
#include "zoom_type.h"

typedef uint32_t SpriteID;  ///< The number of a sprite, without mapping bits and colourtables
typedef uint32_t PaletteID; ///< The number of the palette
typedef uint32_t CursorID;  ///< The number of the cursor (sprite)

/** Combination of a palette sprite and a 'real' sprite */
struct PalSpriteID {
	SpriteID sprite;  ///< The 'real' sprite
	PaletteID pal;    ///< The palette (use \c PAL_NONE) if not needed)
};

enum WindowKeyCodes {
	WKC_SHIFT = 0x8000,
	WKC_CTRL  = 0x4000,
	WKC_ALT   = 0x2000,
	WKC_META  = 0x1000,

	WKC_GLOBAL_HOTKEY = 0x0800, ///< Fake keycode bit to indicate global hotkeys

	WKC_SPECIAL_KEYS = WKC_SHIFT | WKC_CTRL | WKC_ALT | WKC_META | WKC_GLOBAL_HOTKEY,

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

	/* Backquote is the key left of "1"
	 * we only store this key here, no matter what character is really mapped to it
	 * on a particular keyboard. (US keyboard: ` and ~ ; German keyboard: ^ and °) */
	WKC_BACKQUOTE   = 45,
	WKC_PAUSE       = 46,

	/* 0-9 are mapped to 48-57
	 * A-Z are mapped to 65-90
	 * a-z are mapped to 97-122 */

	/* Numerical keyboard */
	WKC_NUM_DIV     = 138,
	WKC_NUM_MUL     = 139,
	WKC_NUM_MINUS   = 140,
	WKC_NUM_PLUS    = 141,
	WKC_NUM_ENTER   = 142,
	WKC_NUM_DECIMAL = 143,

	/* Other keys */
	WKC_SLASH       = 144, ///< / Forward slash
	WKC_SEMICOLON   = 145, ///< ; Semicolon
	WKC_EQUALS      = 146, ///< = Equals
	WKC_L_BRACKET   = 147, ///< [ Left square bracket
	WKC_BACKSLASH   = 148, ///< \ Backslash
	WKC_R_BRACKET   = 149, ///< ] Right square bracket
	WKC_SINGLEQUOTE = 150, ///< ' Single quote
	WKC_COMMA       = 151, ///< , Comma
	WKC_PERIOD      = 152, ///< . Period
	WKC_MINUS       = 153, ///< - Minus
};

/** A single sprite of a list of animated cursors */
struct AnimCursor {
	static const CursorID LAST = MAX_UVALUE(CursorID);
	CursorID sprite;   ///< Must be set to LAST_ANIM when it is the last sprite of the loop
	byte display_time; ///< Amount of ticks this sprite will be shown
};

/** Collection of variables for cursor-display and -animation */
struct CursorVars {
	/* Logical mouse position */
	Point pos;                    ///< logical mouse position
	Point delta;                  ///< relative mouse movement in this tick
	int wheel;                    ///< mouse wheel movement
	bool fix_at;                  ///< mouse is moving, but cursor is not (used for scrolling)

	/* We need two different vars to keep track of how far the scrollwheel moved.
	 * OSX uses this for scrolling around the map. */
	int v_wheel;
	int h_wheel;

	/* Mouse appearance */
	PalSpriteID sprite_seq[16];   ///< current image of cursor
	Point sprite_pos[16];         ///< relative position of individual sprites
	uint sprite_count;            ///< number of sprites to draw
	Point total_offs, total_size; ///< union of sprite properties

	Point draw_pos, draw_size;    ///< position and size bounding-box for drawing

	const AnimCursor *animate_list; ///< in case of animated cursor, list of frames
	const AnimCursor *animate_cur;  ///< in case of animated cursor, current frame
	uint animate_timeout;           ///< in case of animated cursor, number of ticks to show the current cursor

	bool visible;                 ///< cursor is visible
	bool dirty;                   ///< the rect occupied by the mouse is dirty (redraw)
	bool in_window;               ///< mouse inside this window, determines drawing logic

	/* Drag data */
	bool vehchain;                ///< vehicle chain is dragged

	void UpdateCursorPositionRelative(int delta_x, int delta_y);
	bool UpdateCursorPosition(int x, int y);
};

/** Data about how and where to blit pixels. */
struct DrawPixelInfo {
	void *dst_ptr;
	int left, top, width, height;
	int pitch;
	ZoomLevel zoom;
};

/** Structure to access the alpha, red, green, and blue channels from a 32 bit number. */
union Colour {
	uint32_t data; ///< Conversion of the channel information to a 32 bit number.
	struct {
#if defined(__EMSCRIPTEN__)
		uint8_t r, g, b, a;  ///< colour channels as used in browsers
#elif TTD_ENDIAN == TTD_BIG_ENDIAN
		uint8_t a, r, g, b; ///< colour channels in BE order
#else
		uint8_t b, g, r, a; ///< colour channels in LE order
#endif /* TTD_ENDIAN == TTD_BIG_ENDIAN */
	};

	/**
	 * Create a new colour.
	 * @param r The channel for the red colour.
	 * @param g The channel for the green colour.
	 * @param b The channel for the blue colour.
	 * @param a The channel for the alpha/transparency.
	 */
	Colour(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF) :
#if defined(__EMSCRIPTEN__)
		r(r), g(g), b(b), a(a)
#elif TTD_ENDIAN == TTD_BIG_ENDIAN
		a(a), r(r), g(g), b(b)
#else
		b(b), g(g), r(r), a(a)
#endif /* TTD_ENDIAN == TTD_BIG_ENDIAN */
	{
	}

	/**
	 * Create a new colour.
	 * @param data The colour in the correct packed format.
	 */
	Colour(uint data = 0) : data(data)
	{
	}
};

static_assert(sizeof(Colour) == sizeof(uint32_t));


/** Available font sizes */
enum FontSize {
	FS_NORMAL, ///< Index of the normal font in the font tables.
	FS_SMALL,  ///< Index of the small font in the font tables.
	FS_LARGE,  ///< Index of the large font in the font tables.
	FS_MONO,   ///< Index of the monospaced font in the font tables.
	FS_END,

	FS_BEGIN = FS_NORMAL, ///< First font.
};
DECLARE_POSTFIX_INCREMENT(FontSize)

static inline const char *FontSizeToName(FontSize fs)
{
	static const char *SIZE_TO_NAME[] = { "medium", "small", "large", "mono" };
	assert(fs < FS_END);
	return SIZE_TO_NAME[fs];
}

/**
 * Used to only draw a part of the sprite.
 * Draw the subsprite in the rect (sprite_x_offset + left, sprite_y_offset + top) to (sprite_x_offset + right, sprite_y_offset + bottom).
 * Both corners are included in the drawing area.
 */
struct SubSprite {
	int left, top, right, bottom;
};

enum Colours : byte {
	COLOUR_BEGIN,
	COLOUR_DARK_BLUE = COLOUR_BEGIN,
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
	COLOUR_WHITE,
	COLOUR_END,
	INVALID_COLOUR = 0xFF,
};

/** Colour of the strings, see _string_colourmap in table/string_colours.h or docs/ottd-colourtext-palette.png */
enum TextColour {
	TC_BEGIN       = 0x00,
	TC_FROMSTRING  = 0x00,
	TC_BLUE        = 0x00,
	TC_SILVER      = 0x01,
	TC_GOLD        = 0x02,
	TC_RED         = 0x03,
	TC_PURPLE      = 0x04,
	TC_LIGHT_BROWN = 0x05,
	TC_ORANGE      = 0x06,
	TC_GREEN       = 0x07,
	TC_YELLOW      = 0x08,
	TC_DARK_GREEN  = 0x09,
	TC_CREAM       = 0x0A,
	TC_BROWN       = 0x0B,
	TC_WHITE       = 0x0C,
	TC_LIGHT_BLUE  = 0x0D,
	TC_GREY        = 0x0E,
	TC_DARK_BLUE   = 0x0F,
	TC_BLACK       = 0x10,
	TC_END,
	TC_INVALID     = 0xFF,

	TC_IS_PALETTE_COLOUR = 0x100, ///< Colour value is already a real palette colour index, not an index of a StringColour.
	TC_NO_SHADE          = 0x200, ///< Do not add shading to this text colour.
	TC_FORCED            = 0x400, ///< Ignore colour changes from strings.

	TC_COLOUR_MASK = 0xFF, ///< Mask to test if TextColour (without flags) is within limits.
	TC_FLAGS_MASK = 0x700, ///< Mask to test if TextColour (with flags) is within limits.
};
DECLARE_ENUM_AS_BIT_SET(TextColour)

/** Defines a few values that are related to animations using palette changes */
enum PaletteAnimationSizes {
	PALETTE_ANIM_SIZE  = 28,   ///< number of animated colours
	PALETTE_ANIM_START = 227,  ///< Index in  the _palettes array from which all animations are taking places (table/palettes.h)
};

/** Define the operation GfxFillRect performs */
enum FillRectMode {
	FILLRECT_OPAQUE,  ///< Fill rectangle with a single colour
	FILLRECT_CHECKER, ///< Draw only every second pixel, used for greying-out
	FILLRECT_RECOLOUR, ///< Apply a recolour sprite to the screen content
};

/** Palettes OpenTTD supports. */
enum PaletteType {
	PAL_DOS,        ///< Use the DOS palette.
	PAL_WINDOWS,    ///< Use the Windows palette.
	PAL_AUTODETECT, ///< Automatically detect the palette based on the graphics pack.
	MAX_PAL = 2,    ///< The number of palettes.
};

/** Types of sprites that might be loaded */
enum class SpriteType : byte {
	Normal   = 0,      ///< The most basic (normal) sprite
	MapGen   = 1,      ///< Special sprite for the map generator
	Font     = 2,      ///< A sprite used for fonts
	Recolour = 3,      ///< Recolour sprite
	Invalid  = 4,      ///< Pseudosprite or other unusable sprite, used only internally
};

/**
 * The number of milliseconds per game tick.
 * The value 27 together with a day length of 74 ticks makes one day 1998 milliseconds, almost exactly 2 seconds.
 * With a 2 second day, one standard month is 1 minute, and one standard year is slightly over 12 minutes.
 */
static const uint MILLISECONDS_PER_TICK = 27;

/** Information about the currently used palette. */
struct Palette {
	Colour palette[256]; ///< Current palette. Entry 0 has to be always fully transparent!
	int first_dirty;     ///< The first dirty element.
	int count_dirty;     ///< The number of dirty elements.
};

/** Modes for 8bpp support */
enum Support8bpp {
	S8BPP_NONE = 0, ///< No support for 8bpp by OS or hardware, force 32bpp blitters.
	S8BPP_SYSTEM,   ///< No 8bpp support by hardware, do not try to use 8bpp video modes or hardware palettes.
	S8BPP_HARDWARE, ///< Full 8bpp support by OS and hardware.
};

	/** How to align the to-be drawn text. */
enum StringAlignment {
	SA_LEFT        = 0 << 0, ///< Left align the text.
	SA_HOR_CENTER  = 1 << 0, ///< Horizontally center the text.
	SA_RIGHT       = 2 << 0, ///< Right align the text (must be a single bit).
	SA_HOR_MASK    = 3 << 0, ///< Mask for horizontal alignment.

	SA_TOP         = 0 << 2, ///< Top align the text.
	SA_VERT_CENTER = 1 << 2, ///< Vertically center the text.
	SA_BOTTOM      = 2 << 2, ///< Bottom align the text.
	SA_VERT_MASK   = 3 << 2, ///< Mask for vertical alignment.

	SA_CENTER      = SA_HOR_CENTER | SA_VERT_CENTER, ///< Center both horizontally and vertically.

	SA_FORCE       = 1 << 4, ///< Force the alignment, i.e. don't swap for RTL languages.
};
DECLARE_ENUM_AS_BIT_SET(StringAlignment)

#endif /* GFX_TYPE_H */
