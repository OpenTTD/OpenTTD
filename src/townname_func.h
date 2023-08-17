/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file townname_func.h %Town name generator stuff. */

#ifndef TOWNNAME_FUNC_H
#define TOWNNAME_FUNC_H

#include "core/random_func.hpp"
#include "townname_type.h"

std::string GetTownName(const TownNameParams *par, uint32_t townnameparts);
std::string GetTownName(const Town *t);
bool VerifyTownName(uint32_t r, const TownNameParams *par, TownNames *town_names = nullptr);
bool GenerateTownName(Randomizer &randomizer, uint32_t *townnameparts, TownNames *town_names = nullptr);

#endif /* TOWNNAME_FUNC_H */
