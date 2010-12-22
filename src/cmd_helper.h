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

#include "core/enum_type.hpp"

template<typename T, uint S, uint N, typename U> static inline T Extract(U v)
{
	/* Check if there are enough bits in v */
	assert_tcompile(N == EnumPropsT<T>::num_bits);
	assert_tcompile(S + N <= sizeof(U) * 8);
	assert_tcompile(EnumPropsT<T>::end <= (1 << N));
	U masked = GB(v, S, N);
	return IsInsideMM(masked, EnumPropsT<T>::begin, EnumPropsT<T>::end) ? (T)masked : EnumPropsT<T>::invalid;
}

#endif /* CMD_HELPER_H */
