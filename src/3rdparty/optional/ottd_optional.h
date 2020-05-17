/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ottd_optional.h Header to select between native. */

#ifndef OTTD_OPTIONAL_H
#define OTTD_OPTIONAL_H

#if defined(__has_include)
#	if __has_include(<version>)
#		include <version>
#	endif
#endif

#if (__cplusplus >= 201703L) || (defined(__cpp_lib_optional) && __cpp_lib_optional >= 201606L)

/* Native std::optional. */
#include <optional>
namespace opt = std;

#else

/* No std::optional, use local copy instead. */
#include "optional.hpp"
namespace opt = std::experimental;

#endif

#endif /* OTTD_OPTIONAL_H */
