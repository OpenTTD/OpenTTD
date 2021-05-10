/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_ssse3.cpp Implementation of the SSSE3 32 bpp blitter. */

#ifdef WITH_SSE

#include "../stdafx.h"
#include "../zoom_func.h"
#include "../settings_type.h"
#include "32bpp_ssse3.hpp"
#include "32bpp_sse_func.hpp"

#include "../safeguards.h"

/** Instantiation of the SSSE3 32bpp blitter factory. */
static FBlitter_32bppSSSE3 iFBlitter_32bppSSSE3;

#endif /* WITH_SSE */
