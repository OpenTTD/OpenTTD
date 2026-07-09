/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_type.h Commonly used types for the NewGRF implementation. */

#ifndef NEWGRF_TYPE_H
#define NEWGRF_TYPE_H

#include "core/label_type.hpp"

struct GRFConfig;

using GrfID = Label<struct GrfIDTag>; ///< The unique identifier of a NewGRF.

static const GrfID INVALID_GRFID{"\xFF\xFF\xFF\xFF"}; ///< An invalid NewGRF.

#endif /* NEWGRF_TYPE_H */
