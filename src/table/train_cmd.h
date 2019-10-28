/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file train_cmd.h Sprites to use for trains. */

static const SpriteID _engine_sprite_base[] = {
0x0B59, 0x0B61, 0x0B69, 0x0BE1, 0x0B71, 0x0B75, 0x0B7D, 0x0B7D,
0x0B85, 0x0B85, 0x0B8D, 0x0B8D, 0x0BC9, 0x0BD1, 0x0BD9, 0x0BE9,
0x0BED, 0x0BED, 0x0BF5, 0x0BF9, 0x0B79, 0x0B9D, 0x0B9D, 0x0B95,
0x0B95, 0x0BA5, 0x0BA9, 0x0BA9, 0x0BC1, 0x0BC5, 0x0BB1, 0x0BB9,
0x0BB9, 0x0AAD, 0x0AB1, 0x0AB5, 0x0AB9, 0x0ABD, 0x0AC1, 0x0AC9,
0x0ACD, 0x0AD5, 0x0AD1, 0x0AD9, 0x0AC5, 0x0AD1, 0x0AD5, 0x0AF9,
0x0AFD, 0x0B05, 0x0AB9, 0x0AC1, 0x0AC9, 0x0AD1, 0x0AD9, 0x0AE1,
0x0AE5, 0x0AE9, 0x0AF1, 0x0AF9, 0x0B0D, 0x0B11, 0x0B15, 0x0B19,
0x0B1D, 0x0B21, 0x0B29, 0x0B2D, 0x0B35, 0x0B31, 0x0B39, 0x0B25,
0x0B31, 0x0B35,
};

/* For how many directions do we have sprites? (8 or 4; if 4, the other 4
 * directions are symmetric. */
static const byte _engine_sprite_and[] = {
7, 7, 7, 7, 3, 3, 7, 7,
7, 7, 7, 7, 7, 7, 7, 3,
7, 7, 3, 7, 3, 7, 7, 7,
7, 3, 7, 7, 3, 3, 7, 7,
7, 3, 3, 3, 3, 3, 3, 3,
3, 3, 3, 3, 3, 3, 3, 3,
3, 3, 3, 3, 3, 3, 3, 3,
3, 3, 3, 3, 3, 3, 3, 3,
3, 3, 3, 3, 3, 3, 3, 3,
3, 3,
};

/* Non-zero for multihead trains. */
static const byte _engine_sprite_add[] = {
0, 0, 0, 0, 0, 0, 0, 4,
0, 4, 0, 4, 0, 0, 0, 0,
0, 4, 0, 0, 0, 0, 4, 0,
4, 0, 0, 4, 0, 0, 0, 0,
4, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,
0, 0,
};


static const byte _wagon_full_adder[] = {
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0, 44,  0,  0,  0,  0, 24,
	24, 24, 24,  0,  0, 32, 32,  0,
	 4,  4,  4,  4,  4,  4,  4,  0,
	 0,  4,  4,  4,  0, 44,  0,  0,
	 0,  0, 24, 24, 24, 24,  0,  0,
	32, 32
};

assert_compile(lengthof(_engine_sprite_base) == lengthof(_engine_sprite_and));
assert_compile(lengthof(_engine_sprite_base) == lengthof(_engine_sprite_add));
assert_compile(lengthof(_engine_sprite_base) == lengthof(_wagon_full_adder));
