/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file label.cpp Implementation of label functions. */

#include "../stdafx.h"
#include "label_type.hpp"
#include "../string_func.h"

#include "../safeguards.h"

/**
 * Get the label as a \c std::string.
 * If the label is all \c std::isgraph characters, it will return these characters as string,
 * otherwise it will format it as a 8-digit hexadecimal.
 * @return The label as string.
 */
std::string BaseLabel::AsString() const
{
	if (std::ranges::all_of(*this, [](uint8_t c) { return std::isgraph(c); })) {
		return std::string{reinterpret_cast<const char *>(this->data()), this->size()};
	}

	return FormatArrayAsHex(*this);
}
