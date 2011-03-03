/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file endian_type.hpp Definition of various endian-dependant macros. */

#ifndef ENDIAN_TYPE_HPP
#define ENDIAN_TYPE_HPP

#if defined(ARM) || defined(__arm__) || defined(__alpha__)
	#define OTTD_ALIGNMENT 1
#else
	#define OTTD_ALIGNMENT 0
#endif

#define TTD_LITTLE_ENDIAN 0
#define TTD_BIG_ENDIAN 1

/* Windows has always LITTLE_ENDIAN */
#if defined(WIN32) || defined(__OS2__) || defined(WIN64)
	#define TTD_ENDIAN TTD_LITTLE_ENDIAN
#elif !defined(TESTING)
	/* Else include endian[target/host].h, which has the endian-type, autodetected by the Makefile */
	#if defined(STRGEN) || defined(SETTINGSGEN)
		#include "endian_host.h"
	#else
		#include "endian_target.h"
	#endif
#endif /* WIN32 || __OS2__ || WIN64 */

#endif /* ENDIAN_TYPE_HPP */
