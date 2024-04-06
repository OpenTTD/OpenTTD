/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_act5.h Information about NewGRF Action 5. */

#ifndef NEWGRF_ACT5_H
#define NEWGRF_ACT5_H

/** The type of action 5 type. */
enum Action5BlockType {
	A5BLOCK_FIXED,                ///< Only allow replacing a whole block of sprites. (TTDP compatible)
	A5BLOCK_ALLOW_OFFSET,         ///< Allow replacing any subset by specifiing an offset.
	A5BLOCK_INVALID,              ///< unknown/not-implemented type
};

/** Information about a single action 5 type. */
struct Action5Type {
	Action5BlockType block_type; ///< How is this Action5 type processed?
	SpriteID sprite_base;        ///< Load the sprites starting from this sprite.
	uint16_t min_sprites;        ///< If the Action5 contains less sprites, the whole block will be ignored.
	uint16_t max_sprites;        ///< If the Action5 contains more sprites, only the first max_sprites sprites will be used.
	const std::string_view name; ///< Name for error messages.
};

std::span<const Action5Type> GetAction5Types();

#endif /* NEWGRF_ACT5_H */
