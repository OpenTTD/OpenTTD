/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef STRING_BASE_H
#define STRING_BASE_H

#include "string_type.h"

/** Class for iterating over different kind of parts of a string. */
class StringIterator {
public:
	/** Type of the iterator. */
	enum IterType : uint8_t {
		ITER_CHARACTER, ///< Iterate over characters (or more exactly grapheme clusters).
		ITER_WORD,      ///< Iterate over words.
	};

	/** Sentinel to indicate end-of-iteration. */
	static const size_t END = SIZE_MAX;

	/**
	 * Create a new iterator instance.
	 * @return New iterator instance.
	 */
	static std::unique_ptr<StringIterator> Create();

	virtual ~StringIterator() = default;

	/**
	 * Set a new iteration string. Must also be called if the string contents
	 * changed. The cursor is reset to the start of the string.
	 * @param s New string.
	 */
	virtual void SetString(const char *s) = 0;

	/**
	 * Change the current string cursor.
	 * @param pos New cursor position.
	 * @return Actual new cursor position at the next valid character boundary.
	 * @pre pos has to be inside the current string.
	 */
	virtual size_t SetCurPosition(size_t pos) = 0;

	/**
	 * Advance the cursor by one iteration unit.
	 * @return New cursor position (in bytes) or #END if the cursor is already at the end of the string.
	 */
	virtual size_t Next(IterType what = ITER_CHARACTER) = 0;

	/**
	 * Move the cursor back by one iteration unit.
	 * @return New cursor position (in bytes) or #END if the cursor is already at the start of the string.
	 */
	virtual size_t Prev(IterType what = ITER_CHARACTER) = 0;

protected:
	StringIterator() {}
};

/**
 * Input iterator from std::string_view.
 */
class StringConsumer {
	std::string_view string;

public:
	using size_type = std::string_view::size_type;

	explicit StringConsumer(std::string_view string) : string(string) {}

	[[nodiscard]] bool operator==(const StringConsumer &) const noexcept = default;
	[[nodiscard]] bool empty() const noexcept { return this->string.empty(); }
	[[nodiscard]] size_type size() const noexcept { return this->string.size(); }

	StringConsumer &operator++() { *this += 1; return *this; }
	StringConsumer operator++(int) { auto res = *this; *this += 1; return res; }
	StringConsumer &operator+=(size_type count) { assert(count <= this->string.size()); this->string.remove_prefix(count); return *this; }

	[[nodiscard]] char operator*() const { assert(!this->string.empty()); return this->string.front(); }
	[[nodiscard]] const char &operator[](size_type index) const { assert(index < this->string.size()); return this->string[index]; }
	[[nodiscard]] const char *data() const { assert(!this->string.empty()); return this->string.data(); }
	[[nodiscard]] std::string_view str() const noexcept { return this->string; }

	[[nodiscard]] size_type find(char c) const { return this->string.find(c); }
	static constexpr size_type npos = std::string_view::npos;

	[[nodiscard]] char32_t Utf8Consume();
	[[nodiscard]] uint8_t Uint8Consume() { uint8_t res = **this; *this += 1; return res; }
	[[nodiscard]] uint16_t Uint16LEConsume() { uint16_t res = static_cast<uint8_t>((*this)[0]) | static_cast<uint8_t>((*this)[1]) << 8; *this += 2; return res; }
	[[nodiscard]] uint16_t Uint16BEConsume() { uint16_t res = static_cast<uint8_t>((*this)[0]) << 8 | static_cast<uint8_t>((*this)[1]); *this += 2; return res; }
	[[nodiscard]] std::string_view StrConsume(size_type len)
	{
		if (len == npos) {
			std::string_view res = this->string;
			this->string.remove_prefix(this->string.size());
			return res;
		} else {
			assert(len <= this->string.size());
			std::string_view res = this->string.substr(0, len);
			this->string.remove_prefix(len);
			return res;
		}
	}

	[[nodiscard]] uint32_t Uint32Parse(int base);
	[[nodiscard]] uint64_t Uint64Parse(int base);

	void clear() { this->string.remove_prefix(this->string.size()); }
};

#endif /* STRING_BASE_H */
