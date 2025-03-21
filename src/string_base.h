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

	/**
	 * Create a consumer for an external string.
	 */
	explicit StringConsumer(std::string_view string) : string(string) {}

	/**
	 * Check whether no bytes are left.
	 * @return true if empty.
	 */
	[[nodiscard]] bool empty() const noexcept { return this->string.empty(); }

	/**
	 * Get number of bytes left.
	 * @return Number of bytes left.
	 */
	[[nodiscard]] size_type size() const noexcept { return this->string.size(); }

	/**
	 * Advance by one byte.
	 * @return this
	 */
	StringConsumer &operator++()
	{
		*this += 1;
		return *this;
	}

	/**
	 * Advance by one byte.
	 * @return Copy of this before advancing.
	 */
	StringConsumer operator++(int)
	{
		auto res = *this;
		*this += 1;
		return res;
	}

	/**
	 * Advance by "count" byte.
	 * @param count Number of bytes to skip.
	 * @return this
	 */
	StringConsumer &operator+=(size_type count)
	{
		if (count <= this->string.size()) {
			this->string.remove_prefix(count);
		} else {
			assert(false);
			this->string.remove_prefix(this->string.size());
		}
		return *this;
	}

	/**
	 * Peek first byte.
	 * @return First byte.
	 */
	[[nodiscard]] char operator*() const
	{
		if (this->string.empty()) {
			assert(false);
			return '?';
		} else {
			return this->string.front();
		}
	}

	/**
	 * Get buffer of all remaining bytes.
	 * @return Buffer start.
	 */
	[[nodiscard]] const char *data() const { return this->string.data(); }

	/**
	 * Get view of all remaining bytes.
	 * @return Remaining bytes.
	 */
	[[nodiscard]] std::string_view str() const noexcept { return this->string; }

	/**
	 * Find position of first occurrence of some byte.
	 * @param c Byte to search for.
	 * @return Offset to first occurrence, or "npos" if no occurrence.
	 */
	[[nodiscard]] size_type find(char c) const { return this->string.find(c); }

	/**
	 * Special value for "find" and "StrConsume".
	 */
	static constexpr size_type npos = std::string_view::npos;

	/**
	 * Read a UTF-8 character and advance the consumer.
	 * @return Read character.
	 */
	[[nodiscard]] char32_t Utf8Consume();

	/**
	 * Read a uint8_t and advance the consumer.
	 * @return Read integer.
	 */
	[[nodiscard]] uint8_t Uint8Consume()
	{
		if (this->string.empty()) {
			assert(false);
			return 0;
		} else {
			uint8_t res = **this;
			*this += 1;
			return res;
		}
	}

	/**
	 * Read a uint16_t in little endian and advance the consumer.
	 * @return Read integer.
	 */
	[[nodiscard]] uint16_t Uint16LEConsume()
	{
		uint16_t res = this->Uint8Consume();
		res |= this->Uint8Consume() << 8;
		return res;
	}

	/**
	 * Read a sequence of bytes and advance the consumer.
	 * @param len Number of bytes to read.
	 * @return Read string.
	 */
	[[nodiscard]] std::string_view StrConsume(size_type len)
	{
		if (len != npos && len > this->string.size()) {
			assert(false);
			len = npos;
		}
		if (len == npos) {
			std::string_view res = this->string;
			this->string.remove_prefix(this->string.size());
			return res;
		} else {
			std::string_view res = this->string.substr(0, len);
			this->string.remove_prefix(len);
			return res;
		}
	}

	/**
	 * Read and parse a uint32_t and advance the consumer.
	 * @param base Number base.
	 * @return Parsed integer.
	 */
	[[nodiscard]] uint32_t Uint32Parse(int base);

	/**
	 * Read and parse a uint64_t and advance the consumer.
	 * @param base Number base.
	 * @return Parsed integer.
	 */
	[[nodiscard]] uint64_t Uint64Parse(int base);

	/**
	 * Discard all remaining bytes.
	 */
	void clear() { this->string.remove_prefix(this->string.size()); }
};

#endif /* STRING_BASE_H */
