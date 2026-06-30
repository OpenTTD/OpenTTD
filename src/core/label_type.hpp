/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file label_type.hpp A type for 4 character labels/tags/ids in files that should be read/shown as is. */

#ifndef LABEL_TYPE_HPP
#define LABEL_TYPE_HPP

/**
 * A four character label/tag/id.
 * @tparam Tag Type to distinguish labels/tags/ids of different types.
 */
template <typename Tag>
struct Label : std::array<uint8_t, 4> {
	/** Create an empty label, i.e. all zeros. */
	constexpr Label()
	{
		std::ranges::fill(*this, 0);
	}

	/**
	 * Create a label with the given 4 letter character string.
	 * @param label The label value.
	 * @note This defines 5 characters, but the 5th character is assumed to null-terminator.
	 */
	constexpr Label(const char (&label)[5])
	{
		std::copy(label, label + this->size(), this->begin());
	}

	/**
	 * Default spaceship operator.
	 * @param other The label to compare to.
	 * @return The comparison ordering.
	 */
	constexpr std::strong_ordering operator<=>(const Label<Tag> &other) const = default;
};

#endif /* LABEL_TYPE_HPP */
