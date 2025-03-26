/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file base_media_sounds.h Generic functions for replacing base sounds data. */

#ifndef BASE_MEDIA_SOUNDS_H
#define BASE_MEDIA_SOUNDS_H

#include "base_media_base.h"

template <> struct BaseSetTraits<struct SoundsSet> {
	static constexpr size_t num_files = 1;
	static constexpr bool search_in_tars = true;
	static constexpr std::string_view set_type = "sounds";
};

/** All data of a sounds set. */
struct SoundsSet : BaseSet<SoundsSet> {};

/** All data/functions related with replacing the base sounds */
class BaseSounds : public BaseMedia<SoundsSet> {
public:
	/** The set as saved in the config file. */
	static inline std::string ini_set;
};

#endif /* BASE_MEDIA_SOUNDS_H */
