/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file townname_func.h %Town name generator stuff. */

#ifndef TOWNNAME_FUNC_H
#define TOWNNAME_FUNC_H

#include "core/random_func.hpp"
#include "townname_type.h"

std::string GetGeneratorTownName(const TownNameParams *par, uint32_t townnameparts);
std::string GetGeneratorTownName(const Town *t);
bool VerifyTownName(std::string proposed_name);
bool GenerateTownName(Randomizer &randomizer, std::string *town_name);

#endif /* TOWNNAME_FUNC_H */
