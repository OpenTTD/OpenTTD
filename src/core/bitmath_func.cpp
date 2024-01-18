/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bitmath_func.cpp Functions related to bit mathematics. */

#include "../stdafx.h"
#include "bitmath_func.hpp"

#include "../safeguards.h"

const uint8_t _ffb_64[64] = {
	0,  0,  1,  0,  2,  0,  1,  0,
	3,  0,  1,  0,  2,  0,  1,  0,
	4,  0,  1,  0,  2,  0,  1,  0,
	3,  0,  1,  0,  2,  0,  1,  0,
	5,  0,  1,  0,  2,  0,  1,  0,
	3,  0,  1,  0,  2,  0,  1,  0,
	4,  0,  1,  0,  2,  0,  1,  0,
	3,  0,  1,  0,  2,  0,  1,  0,
};
