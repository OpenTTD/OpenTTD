/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file endian_type.hpp Definition of various endian-dependant macros. */

#ifndef ENDIAN_TYPE_HPP
#define ENDIAN_TYPE_HPP

/** Little endian builds use this for TTD_ENDIAN. */
#define TTD_LITTLE_ENDIAN 0
/** Big endian builds use this for TTD_ENDIAN. */
#define TTD_BIG_ENDIAN 1

#if !defined(TTD_ENDIAN)
#	error "TTD_ENDIAN is not defined; please set it to either TTD_LITTLE_ENDIAN or TTD_BIG_ENDIAN"
#endif /* !TTD_ENDIAN */

#endif /* ENDIAN_TYPE_HPP */
