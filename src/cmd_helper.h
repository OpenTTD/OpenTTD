/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cmd_helper.h Helper functions to extract data from command parameters. */

#ifndef CMD_HELPER_H
#define CMD_HELPER_H

#include "direction_type.h"
#include "road_type.h"


template<uint N> static inline void ExtractValid();
template<> inline void ExtractValid<1>() {}


template<typename T> struct ExtractBits;
template<> struct ExtractBits<Axis>          { static const uint Count =  1; };
template<> struct ExtractBits<DiagDirection> { static const uint Count =  2; };
template<> struct ExtractBits<RoadBits>      { static const uint Count =  4; };


template<typename T, uint N, typename U> static inline T Extract(U v)
{
	/* Check if there are enough bits in v */
	ExtractValid<N + ExtractBits<T>::Count <= sizeof(U) * 8>();
	return (T)GB(v, N, ExtractBits<T>::Count);
}

#endif
