/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_stringmapping.h NewGRF string mapping definition. */

#ifndef NEWGRF_STRINGMAPPING_H
#define NEWGRF_STRINGMAPPING_H

#include "../strings_type.h"
#include "../newgrf_text_type.h"

void AddStringForMapping(GRFStringID source, std::function<void(StringID)> &&func);
void AddStringForMapping(GRFStringID source, StringID *target);
void FinaliseStringMapping();

#endif /* NEWGRF_STRINGMAPPING_H */
