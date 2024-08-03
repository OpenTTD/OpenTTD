/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_cargo.h Cargo support for NewGRFs. */

#ifndef NEWGRF_CARGO_H
#define NEWGRF_CARGO_H

#include "newgrf_callbacks.h"
#include "cargo_type.h"
#include "gfx_type.h"

/**
 * Sprite Group Cargo types.
 * These special cargo types are used when resolving sprite groups when non-cargo-specific sprites or callbacks are needed,
 * e.g. in purchase lists, or if no specific cargo type sprite group is supplied.
 */
namespace SpriteGroupCargo {
	static constexpr CargoID SG_DEFAULT    = NUM_CARGO;     ///< Default type used when no more-specific cargo matches.
	static constexpr CargoID SG_PURCHASE   = NUM_CARGO + 1; ///< Used in purchase lists before an item exists.
	static constexpr CargoID SG_DEFAULT_NA = NUM_CARGO + 2; ///< Used only by stations and roads when no more-specific cargo matches.
};

/* Forward declarations of structs used */
struct CargoSpec;
struct GRFFile;

SpriteID GetCustomCargoSprite(const CargoSpec *cs);
uint16_t GetCargoCallback(CallbackID callback, uint32_t param1, uint32_t param2, const CargoSpec *cs);
CargoID GetCargoTranslation(uint8_t cargo, const GRFFile *grffile, bool usebit = false);

std::span<const CargoLabel> GetDefaultCargoTranslationTable(uint8_t grf_version);

#endif /* NEWGRF_CARGO_H */
