/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file label_type.hpp A type for 4 character labels/tags/ids in files that should be read/shown as is. */

#ifndef LABEL_TYPE_HPP
#define LABEL_TYPE_HPP

/** Base for a four character label/tag/id. */
struct BaseLabel : std::array<uint8_t, 4> {
	/**
	 * Check whether the label is empty.
	 * @return \c true iff the label is empty, i.e. all zeros.
	 */
	constexpr inline bool Empty() const
	{
		return std::ranges::all_of(*this, [](uint8_t b) { return b == 0; });
	};

	/**
	 * Get the label as a \c std::string.
	 * If the label is all \c std::isgraph characters, it will return these characters as string,
	 * otherwise it will format it as a 8-digit hexadecimal.
	 * @return The label as string.
	 */
	std::string AsString() const;
};

/**
 * A four character label/tag/id.
 * @tparam Tag Type to distinguish labels/tags/ids of different types.
 */
template <typename Tag>
struct Label : BaseLabel {
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
	 * Create a label with the given 4 bytes.
	 * @param label The label value.
	 */
	constexpr Label(const uint8_t (&label)[4])
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
