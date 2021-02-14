/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cocoa_keys.h Mappings of Cocoa keys. */

#ifndef COCOA_KEYS_H
#define COCOA_KEYS_H

/* From SDL_QuartzKeys.h
 * These are the Macintosh key scancode constants -- from Inside Macintosh */

#define QZ_ESCAPE       0x35
#define QZ_F1           0x7A
#define QZ_F2           0x78
#define QZ_F3           0x63
#define QZ_F4           0x76
#define QZ_F5           0x60
#define QZ_F6           0x61
#define QZ_F7           0x62
#define QZ_F8           0x64
#define QZ_F9           0x65
#define QZ_F10          0x6D
#define QZ_F11          0x67
#define QZ_F12          0x6F
#define QZ_PRINT        0x69
#define QZ_SCROLLOCK    0x6B
#define QZ_PAUSE        0x71
#define QZ_POWER        0x7F
#define QZ_BACKQUOTE    0x0A
#define QZ_BACKQUOTE2   0x32
#define QZ_1            0x12
#define QZ_2            0x13
#define QZ_3            0x14
#define QZ_4            0x15
#define QZ_5            0x17
#define QZ_6            0x16
#define QZ_7            0x1A
#define QZ_8            0x1C
#define QZ_9            0x19
#define QZ_0            0x1D
#define QZ_MINUS        0x1B
#define QZ_EQUALS       0x18
#define QZ_BACKSPACE    0x33
#define QZ_INSERT       0x72
#define QZ_HOME         0x73
#define QZ_PAGEUP       0x74
#define QZ_NUMLOCK      0x47
#define QZ_KP_EQUALS    0x51
#define QZ_KP_DIVIDE    0x4B
#define QZ_KP_MULTIPLY  0x43
#define QZ_TAB          0x30
#define QZ_q            0x0C
#define QZ_w            0x0D
#define QZ_e            0x0E
#define QZ_r            0x0F
#define QZ_t            0x11
#define QZ_y            0x10
#define QZ_u            0x20
#define QZ_i            0x22
#define QZ_o            0x1F
#define QZ_p            0x23
#define QZ_LEFTBRACKET  0x21
#define QZ_RIGHTBRACKET 0x1E
#define QZ_BACKSLASH    0x2A
#define QZ_DELETE       0x75
#define QZ_END          0x77
#define QZ_PAGEDOWN     0x79
#define QZ_KP7          0x59
#define QZ_KP8          0x5B
#define QZ_KP9          0x5C
#define QZ_KP_MINUS     0x4E
#define QZ_CAPSLOCK     0x39
#define QZ_a            0x00
#define QZ_s            0x01
#define QZ_d            0x02
#define QZ_f            0x03
#define QZ_g            0x05
#define QZ_h            0x04
#define QZ_j            0x26
#define QZ_k            0x28
#define QZ_l            0x25
#define QZ_SEMICOLON    0x29
#define QZ_QUOTE        0x27
#define QZ_RETURN       0x24
#define QZ_KP4          0x56
#define QZ_KP5          0x57
#define QZ_KP6          0x58
#define QZ_KP_PLUS      0x45
#define QZ_LSHIFT       0x38
#define QZ_z            0x06
#define QZ_x            0x07
#define QZ_c            0x08
#define QZ_v            0x09
#define QZ_b            0x0B
#define QZ_n            0x2D
#define QZ_m            0x2E
#define QZ_COMMA        0x2B
#define QZ_PERIOD       0x2F
#define QZ_SLASH        0x2C
#if 1        /* Panther now defines right side keys */
#define QZ_RSHIFT       0x3C
#endif
#define QZ_UP           0x7E
#define QZ_KP1          0x53
#define QZ_KP2          0x54
#define QZ_KP3          0x55
#define QZ_KP_ENTER     0x4C
#define QZ_LCTRL        0x3B
#define QZ_LALT         0x3A
#define QZ_LMETA        0x37
#define QZ_SPACE        0x31
#if 1        /* Panther now defines right side keys */
#define QZ_RMETA        0x36
#define QZ_RALT         0x3D
#define QZ_RCTRL        0x3E
#endif
#define QZ_LEFT         0x7B
#define QZ_DOWN         0x7D
#define QZ_RIGHT        0x7C
#define QZ_KP0          0x52
#define QZ_KP_PERIOD    0x41

/* Weird, these keys are on my iBook under MacOS X */
#define QZ_IBOOK_ENTER  0x34
#define QZ_IBOOK_LEFT   0x3B
#define QZ_IBOOK_RIGHT  0x3C
#define QZ_IBOOK_DOWN   0x3D
#define QZ_IBOOK_UP     0x3E


struct CocoaVkMapping {
	unsigned short vk_from;
	byte map_to;
};

#define AS(x, z) {x, z}

static const CocoaVkMapping _vk_mapping[] = {
	AS(QZ_BACKQUOTE,  WKC_BACKQUOTE), // key left of '1'
	AS(QZ_BACKQUOTE2, WKC_BACKQUOTE), // some keyboards have it on another scancode

	/* Pageup stuff + up/down */
	AS(QZ_PAGEUP,   WKC_PAGEUP),
	AS(QZ_PAGEDOWN, WKC_PAGEDOWN),

	AS(QZ_UP,    WKC_UP),
	AS(QZ_DOWN,  WKC_DOWN),
	AS(QZ_LEFT,  WKC_LEFT),
	AS(QZ_RIGHT, WKC_RIGHT),

	AS(QZ_HOME, WKC_HOME),
	AS(QZ_END,  WKC_END),

	AS(QZ_INSERT, WKC_INSERT),
	AS(QZ_DELETE, WKC_DELETE),

	/* Letters. QZ_[a-z] is not in numerical order so we can't use AM(...) */
	AS(QZ_a, 'A'),
	AS(QZ_b, 'B'),
	AS(QZ_c, 'C'),
	AS(QZ_d, 'D'),
	AS(QZ_e, 'E'),
	AS(QZ_f, 'F'),
	AS(QZ_g, 'G'),
	AS(QZ_h, 'H'),
	AS(QZ_i, 'I'),
	AS(QZ_j, 'J'),
	AS(QZ_k, 'K'),
	AS(QZ_l, 'L'),
	AS(QZ_m, 'M'),
	AS(QZ_n, 'N'),
	AS(QZ_o, 'O'),
	AS(QZ_p, 'P'),
	AS(QZ_q, 'Q'),
	AS(QZ_r, 'R'),
	AS(QZ_s, 'S'),
	AS(QZ_t, 'T'),
	AS(QZ_u, 'U'),
	AS(QZ_v, 'V'),
	AS(QZ_w, 'W'),
	AS(QZ_x, 'X'),
	AS(QZ_y, 'Y'),
	AS(QZ_z, 'Z'),
	/* Same thing for digits */
	AS(QZ_0, '0'),
	AS(QZ_1, '1'),
	AS(QZ_2, '2'),
	AS(QZ_3, '3'),
	AS(QZ_4, '4'),
	AS(QZ_5, '5'),
	AS(QZ_6, '6'),
	AS(QZ_7, '7'),
	AS(QZ_8, '8'),
	AS(QZ_9, '9'),

	AS(QZ_ESCAPE,    WKC_ESC),
	AS(QZ_PAUSE,     WKC_PAUSE),
	AS(QZ_BACKSPACE, WKC_BACKSPACE),

	AS(QZ_SPACE,  WKC_SPACE),
	AS(QZ_RETURN, WKC_RETURN),
	AS(QZ_TAB,    WKC_TAB),

	/* Function keys */
	AS(QZ_F1,  WKC_F1),
	AS(QZ_F2,  WKC_F2),
	AS(QZ_F3,  WKC_F3),
	AS(QZ_F4,  WKC_F4),
	AS(QZ_F5,  WKC_F5),
	AS(QZ_F6,  WKC_F6),
	AS(QZ_F7,  WKC_F7),
	AS(QZ_F8,  WKC_F8),
	AS(QZ_F9,  WKC_F9),
	AS(QZ_F10, WKC_F10),
	AS(QZ_F11, WKC_F11),
	AS(QZ_F12, WKC_F12),

	/* Numeric part */
	AS(QZ_KP0,         '0'),
	AS(QZ_KP1,         '1'),
	AS(QZ_KP2,         '2'),
	AS(QZ_KP3,         '3'),
	AS(QZ_KP4,         '4'),
	AS(QZ_KP5,         '5'),
	AS(QZ_KP6,         '6'),
	AS(QZ_KP7,         '7'),
	AS(QZ_KP8,         '8'),
	AS(QZ_KP9,         '9'),
	AS(QZ_KP_DIVIDE,   WKC_NUM_DIV),
	AS(QZ_KP_MULTIPLY, WKC_NUM_MUL),
	AS(QZ_KP_MINUS,    WKC_NUM_MINUS),
	AS(QZ_KP_PLUS,     WKC_NUM_PLUS),
	AS(QZ_KP_ENTER,    WKC_NUM_ENTER),
	AS(QZ_KP_PERIOD,   WKC_NUM_DECIMAL),

	/* Other non-letter keys */
	AS(QZ_SLASH,        WKC_SLASH),
	AS(QZ_SEMICOLON,    WKC_SEMICOLON),
	AS(QZ_EQUALS,       WKC_EQUALS),
	AS(QZ_LEFTBRACKET,  WKC_L_BRACKET),
	AS(QZ_BACKSLASH,    WKC_BACKSLASH),
	AS(QZ_RIGHTBRACKET, WKC_R_BRACKET),

	AS(QZ_QUOTE,   WKC_SINGLEQUOTE),
	AS(QZ_COMMA,   WKC_COMMA),
	AS(QZ_MINUS,   WKC_MINUS),
	AS(QZ_PERIOD,  WKC_PERIOD)
};

#undef AS

#endif
