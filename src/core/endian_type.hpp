/* $Id$ */

/** @file endian_type.hpp Definition of various endian-dependant macros. */

#ifndef ENDIAN_TYPE_H
#define ENDIAN_TYPE_H

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
	#if defined(STRGEN)
		#include "endian_host.h"
	#else
		#include "endian_target.h"
	#endif
#endif /* WIN32 || __OS2__ || WIN64 */

#endif /* ENDIAN_TYPE_HPP */
