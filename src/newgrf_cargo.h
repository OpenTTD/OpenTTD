/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_cargo.h Cargo support for NewGRFs. */

#ifndef NEWGRF_CARGO_H
#define NEWGRF_CARGO_H

#include "newgrf_callbacks.h"
#include "cargo_type.h"
#include "gfx_type.h"

/* Forward declarations of structs used */
struct CargoSpec;
struct GRFFile;

SpriteID GetCustomCargoSprite(const CargoSpec *cs);
uint16_t GetCargoCallback(CallbackID callback, uint32_t param1, uint32_t param2, const CargoSpec *cs, std::span<int32_t> regs100 = {});
CargoType GetCargoTranslation(uint8_t cargo, const GRFFile *grffile, bool usebit = false);

std::span<const CargoLabel> GetClimateDependentCargoTranslationTable();
std::span<const CargoLabel> GetClimateIndependentCargoTranslationTable();

#endif /* NEWGRF_CARGO_H */
