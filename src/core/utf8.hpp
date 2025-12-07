/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file utf8.hpp Handling of UTF-8 encoded data.
 */

#ifndef UTF8_HPP
#define UTF8_HPP

#include <iterator>
#include "bitmath_func.hpp"

[[nodiscard]] std::pair<char[4], size_t> EncodeUtf8(char32_t c);
[[nodiscard]] std::pair<size_t, char32_t> DecodeUtf8(std::string_view buf);

/* Check if the given character is part of a UTF8 sequence */
inline bool IsUtf8Part(char c)
{
	return GB(c, 6, 2) == 2;
}

/**
 * Constant span of UTF-8 encoded data.
 */
class Utf8View {
	std::string_view src;
public:
	Utf8View() = default;
	Utf8View(std::string_view src) : src(src) {}

	/**
	 * Bidirectional input iterator over codepoints.
	 *
	 * If invalid encodings are present:
	 * - the iterator will skip overlong encodings, and
	 * - dereferencing returns a placeholder char '?'.
	 */
	class iterator {
		std::string_view src;
		size_t position = 0;
	public:
		using value_type = char32_t;
		using difference_type = std::ptrdiff_t;
		using iterator_category = std::bidirectional_iterator_tag;
		using pointer = void;
		using reference = void;

		iterator() = default;
		iterator(std::string_view src, size_t position) : src(src), position(position) {}

		size_t GetByteOffset() const
		{
			return this->position;
		}

		bool operator==(const iterator &rhs) const
		{
			assert(this->src.data() == rhs.src.data());
			return this->position == rhs.position;
		}

		std::strong_ordering operator<=>(const iterator &rhs) const
		{
			assert(this->src.data() == rhs.src.data());
			return this->position <=> rhs.position;
		}

		char32_t operator*() const
		{
			assert(this->position < this->src.size());
			auto [len, c] = DecodeUtf8(this->src.substr(this->position));
			return len > 0 ? c : '?';
		}

		iterator& operator++()
		{
			auto size = this->src.size();
			assert(this->position < size);
			do {
				++this->position;
			} while (this->position < size && IsUtf8Part(this->src[this->position]));
			return *this;
		}

		iterator operator++(int)
		{
			iterator result = *this;
			++*this;
			return result;
		}

		iterator& operator--()
		{
			assert(this->position > 0);
			do {
				--this->position;
			} while (this->position > 0 && IsUtf8Part(this->src[this->position]));
			return *this;
		}

		iterator operator--(int)
		{
			iterator result = *this;
			--*this;
			return result;
		}
	};

	iterator begin() const
	{
		return iterator(this->src, 0);
	}

	iterator end() const
	{
		return iterator(this->src, this->src.size());
	}

	iterator GetIterAtByte(size_t offset) const;
};

#endif /* UTF8_HPP */
