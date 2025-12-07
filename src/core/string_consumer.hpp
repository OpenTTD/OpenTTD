/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file string_consumer.hpp Parse strings.
 */

#ifndef STRING_CONSUMER_HPP
#define STRING_CONSUMER_HPP

#include <charconv>
#include "format.hpp"

/**
 * Parse data from a string / buffer.
 *
 * There are generally four operations for each data type:
 * - Peek: Check and return validity and value. Do not advance read position.
 * - TryRead: Check and return validity and value. Advance reader, if valid.
 * - Read: Check validity, return value or fallback-value. Advance reader, even if value is invalid, to avoid deadlocks/stalling.
 * - Skip: Discard value. Advance reader, even if value is invalid, to avoid deadlocks/stalling.
 */
class StringConsumer {
public:
	using size_type = std::string_view::size_type;

	/**
	 * Special value for "end of data".
	 */
	static constexpr size_type npos = std::string_view::npos;

	/**
	 * ASCII whitespace characters, excluding new-line.
	 * Usable in FindChar(In|NotIn), (Peek|Read|Skip)(If|Until)Char(In|NotIn)
	 */
	static const std::string_view WHITESPACE_NO_NEWLINE;
	/**
	 * ASCII whitespace characters, including new-line.
	 * Usable in FindChar(In|NotIn), (Peek|Read|Skip)(If|Until)Char(In|NotIn)
	 */
	static const std::string_view WHITESPACE_OR_NEWLINE;

private:
	std::string_view src;
	size_type position = 0;

	static void LogError(std::string &&msg);

public:
	/**
	 * Construct parser with data from string.
	 */
	explicit StringConsumer(std::string_view src) : src(src) {}
	/**
	 * Construct parser with data from string.
	 */
	explicit StringConsumer(const std::string &src) : src(src) {}
	/**
	 * Construct parser with data from span.
	 */
	explicit StringConsumer(std::span<const char> src) : src(src.data(), src.size()) {}

	/**
	 * Check whether any bytes left to read.
	 */
	[[nodiscard]] bool AnyBytesLeft() const noexcept { return this->position < this->src.size(); }
	/**
	 * Get number of bytes left to read.
	 */
	[[nodiscard]] size_type GetBytesLeft() const noexcept { return this->src.size() - this->position; }

	/**
	 * Check whether any bytes were already read.
	 */
	[[nodiscard]] bool AnyBytesRead() const noexcept { return this->position > 0; }
	/**
	 * Get number of already read bytes.
	 */
	[[nodiscard]] size_type GetBytesRead() const noexcept { return this->position; }

	/**
	 * Get the original data, as passed to the constructor.
	 */
	[[nodiscard]] std::string_view GetOrigData() const noexcept { return this->src; }
	/**
	 * Get already read data.
	 */
	[[nodiscard]] std::string_view GetReadData() const noexcept { return this->src.substr(0, this->position); }
	/**
	 * Get data left to read.
	 */
	[[nodiscard]] std::string_view GetLeftData() const noexcept { return this->src.substr(this->position); }

	/**
	 * Discard all remaining data.
	 */
	void SkipAll() { this->position = this->src.size(); }

	/**
	 * Peek binary uint8.
	 * @return Read integer, std::nullopt if not enough data.
	 */
	[[nodiscard]] std::optional<uint8_t> PeekUint8() const;
	/**
	 * Try to read binary uint8, and then advance reader.
	 */
	[[nodiscard]] std::optional<uint8_t> TryReadUint8()
	{
		auto value = this->PeekUint8();
		if (value.has_value()) this->SkipUint8();
		return value;
	}
	/**
	 * Read binary uint8, and advance reader.
	 * @param def Default value to return, if not enough data.
	 * @return Read integer, 'def' if not enough data.
	 */
	[[nodiscard]] uint8_t ReadUint8(uint8_t def = 0)
	{
		auto value = this->PeekUint8();
		this->SkipUint8(); // always advance
		return value.value_or(def);
	}
	/**
	 * Skip binary uint8.
	 */
	void SkipUint8() { this->Skip(1); }

	/**
	 * Peek binary int8.
	 * @return Read integer, std::nullopt if not enough data.
	 */
	[[nodiscard]] std::optional<int8_t> PeekSint8() const
	{
		auto result = PeekUint8();
		if (!result.has_value()) return std::nullopt;
		return static_cast<int8_t>(*result);
	}
	/**
	 * Try to read binary int8, and then advance reader.
	 */
	[[nodiscard]] std::optional<int8_t> TryReadSint8()
	{
		auto value = this->PeekSint8();
		if (value.has_value()) this->SkipSint8();
		return value;
	}
	/**
	 * Read binary int8, and advance reader.
	 * @param def Default value to return, if not enough data.
	 * @return Read integer, 'def' if not enough data.
	 */
	[[nodiscard]] int8_t ReadSint8(int8_t def = 0)
	{
		auto value = this->PeekSint8();
		this->SkipSint8(); // always advance
		return value.value_or(def);
	}
	/**
	 * Skip binary int8.
	 */
	void SkipSint8() { this->Skip(1); }

	/**
	 * Peek binary uint16 using little endian.
	 * @return Read integer, std::nullopt if not enough data.
	 */
	[[nodiscard]] std::optional<uint16_t> PeekUint16LE() const;
	/**
	 * Try to read binary uint16, and then advance reader.
	 */
	[[nodiscard]] std::optional<uint16_t> TryReadUint16LE()
	{
		auto value = this->PeekUint16LE();
		if (value.has_value()) this->SkipUint16LE();
		return value;
	}
	/**
	 * Read binary uint16 using little endian, and advance reader.
	 * @param def Default value to return, if not enough data.
	 * @return Read integer, 'def' if not enough data.
	 * @note The reader is advanced, even if not enough data was present.
	 */
	[[nodiscard]] uint16_t ReadUint16LE(uint16_t def = 0)
	{
		auto value = this->PeekUint16LE();
		this->SkipUint16LE(); // always advance
		return value.value_or(def);
	}
	/**
	 * Skip binary uint16, and advance reader.
	 * @note The reader is advanced, even if not enough data was present.
	 */
	void SkipUint16LE() { this->Skip(2); }

	/**
	 * Peek binary int16 using little endian.
	 * @return Read integer, std::nullopt if not enough data.
	 */
	[[nodiscard]] std::optional<int16_t> PeekSint16LE() const
	{
		auto result = PeekUint16LE();
		if (!result.has_value()) return std::nullopt;
		return static_cast<int16_t>(*result);
	}
	/**
	 * Try to read binary int16, and then advance reader.
	 */
	[[nodiscard]] std::optional<int16_t> TryReadSint16LE()
	{
		auto value = this->PeekSint16LE();
		if (value.has_value()) this->SkipSint16LE();
		return value;
	}
	/**
	 * Read binary int16 using little endian, and advance reader.
	 * @param def Default value to return, if not enough data.
	 * @return Read integer, 'def' if not enough data.
	 * @note The reader is advanced, even if not enough data was present.
	 */
	[[nodiscard]] int16_t ReadSint16LE(int16_t def = 0)
	{
		auto value = this->PeekSint16LE();
		this->SkipSint16LE(); // always advance
		return value.value_or(def);
	}
	/**
	 * Skip binary int16, and advance reader.
	 * @note The reader is advanced, even if not enough data was present.
	 */
	void SkipSint16LE() { this->Skip(2); }

	/**
	 * Peek binary uint32 using little endian.
	 * @return Read integer, std::nullopt if not enough data.
	 */
	[[nodiscard]] std::optional<uint32_t> PeekUint32LE() const;
	/**
	 * Try to read binary uint32, and then advance reader.
	 */
	[[nodiscard]] std::optional<uint32_t> TryReadUint32LE()
	{
		auto value = this->PeekUint32LE();
		if (value.has_value()) this->SkipUint32LE();
		return value;
	}
	/**
	 * Read binary uint32 using little endian, and advance reader.
	 * @param def Default value to return, if not enough data.
	 * @return Read integer, 'def' if not enough data.
	 * @note The reader is advanced, even if not enough data was present.
	 */
	[[nodiscard]] uint32_t ReadUint32LE(uint32_t def = 0)
	{
		auto value = this->PeekUint32LE();
		this->SkipUint32LE(); // always advance
		return value.value_or(def);
	}
	/**
	 * Skip binary uint32, and advance reader.
	 * @note The reader is advanced, even if not enough data was present.
	 */
	void SkipUint32LE() { this->Skip(4); }

	/**
	 * Peek binary int32 using little endian.
	 * @return Read integer, std::nullopt if not enough data.
	 */
	[[nodiscard]] std::optional<int32_t> PeekSint32LE() const
	{
		auto result = PeekUint32LE();
		if (!result.has_value()) return std::nullopt;
		return static_cast<int32_t>(*result);
	}
	/**
	 * Try to read binary int32, and then advance reader.
	 */
	[[nodiscard]] std::optional<int32_t> TryReadSint32LE()
	{
		auto value = this->PeekSint32LE();
		if (value.has_value()) this->SkipSint32LE();
		return value;
	}
	/**
	 * Read binary int32 using little endian, and advance reader.
	 * @param def Default value to return, if not enough data.
	 * @return Read integer, 'def' if not enough data.
	 * @note The reader is advanced, even if not enough data was present.
	 */
	[[nodiscard]] int32_t ReadSint32LE(int32_t def = 0)
	{
		auto value = this->PeekSint32LE();
		this->SkipSint32LE(); // always advance
		return value.value_or(def);
	}
	/**
	 * Skip binary int32, and advance reader.
	 * @note The reader is advanced, even if not enough data was present.
	 */
	void SkipSint32LE() { this->Skip(4); }

	/**
	 * Peek binary uint64 using little endian.
	 * @return Read integer, std::nullopt if not enough data.
	 */
	[[nodiscard]] std::optional<uint64_t> PeekUint64LE() const;
	/**
	 * Try to read binary uint64, and then advance reader.
	 */
	[[nodiscard]] std::optional<uint64_t> TryReadUint64LE()
	{
		auto value = this->PeekUint64LE();
		if (value.has_value()) this->SkipUint64LE();
		return value;
	}
	/**
	 * Read binary uint64 using little endian, and advance reader.
	 * @param def Default value to return, if not enough data.
	 * @return Read integer, 'def' if not enough data.
	 * @note The reader is advanced, even if not enough data was present.
	 */
	[[nodiscard]] uint64_t ReadUint64LE(uint64_t def = 0)
	{
		auto value = this->PeekUint64LE();
		this->SkipUint64LE(); // always advance
		return value.value_or(def);
	}
	/**
	 * Skip binary uint64, and advance reader.
	 * @note The reader is advanced, even if not enough data was present.
	 */
	void SkipUint64LE() { this->Skip(8); }

	/**
	 * Peek binary int64 using little endian.
	 * @return Read integer, std::nullopt if not enough data.
	 */
	[[nodiscard]] std::optional<int64_t> PeekSint64LE() const
	{
		auto result = PeekUint64LE();
		if (!result.has_value()) return std::nullopt;
		return static_cast<int64_t>(*result);
	}
	/**
	 * Try to read binary int64, and then advance reader.
	 */
	[[nodiscard]] std::optional<int64_t> TryReadSint64LE()
	{
		auto value = this->PeekSint64LE();
		if (value.has_value()) this->SkipSint64LE();
		return value;
	}
	/**
	 * Read binary int64 using little endian, and advance reader.
	 * @param def Default value to return, if not enough data.
	 * @return Read integer, 'def' if not enough data.
	 * @note The reader is advanced, even if not enough data was present.
	 */
	[[nodiscard]] int64_t ReadSint64LE(int64_t def = 0)
	{
		auto value = this->PeekSint64LE();
		this->SkipSint64LE(); // always advance
		return value.value_or(def);
	}
	/**
	 * Skip binary int64, and advance reader.
	 * @note The reader is advanced, even if not enough data was present.
	 */
	void SkipSint64LE() { this->Skip(8); }

	/**
	 * Peek 8-bit character.
	 * @return Read char, std::nullopt if not enough data.
	 */
	[[nodiscard]] std::optional<char> PeekChar() const;
	/**
	 * Try to read a 8-bit character, and then advance reader.
	 */
	[[nodiscard]] std::optional<char> TryReadChar()
	{
		auto value = this->PeekChar();
		if (value.has_value()) this->SkipChar();
		return value;
	}
	/**
	 * Read 8-bit character, and advance reader.
	 * @param def Default value to return, if not enough data.
	 * @return Read character, 'def' if not enough data.
	 */
	[[nodiscard]] char ReadChar(char def = '?') {
		auto value = this->PeekChar();
		this->SkipChar(); // always advance
		return value.value_or(def);
	}
	/**
	 * Skip 8-bit character, and advance reader.
	 */
	void SkipChar() { this->Skip(1); }

	/**
	 * Peek UTF-8 character.
	 * @return Length and read char, {0, 0} if no valid data.
	 */
	[[nodiscard]] std::pair<size_type, char32_t> PeekUtf8() const;
	/**
	 * Try to read a UTF-8 character, and then advance reader.
	 */
	[[nodiscard]] std::optional<char32_t> TryReadUtf8()
	{
		auto [len, value] = this->PeekUtf8();
		if (len == 0) return std::nullopt;
		this->Skip(len);
		return value;
	}
	/**
	 * Read UTF-8 character, and advance reader.
	 * @param def Default value to return, if no valid data.
	 * @return Read char, 'def' if no valid data.
	 * @note The reader is advanced, even if no valid data was present.
	 */
	[[nodiscard]] char32_t ReadUtf8(char32_t def = '?')
	{
		auto [len, value] = this->PeekUtf8();
		this->Skip(len > 0 ? len : 1); // advance at least one byte
		return len > 0 ? value : def;
	}
	/**
	 * Skip UTF-8 character, and advance reader.
	 * @note The reader is advanced, even if no valid data was present.
	 * @note This behaves different to Utf8View::iterator.
	 *       Here we do not skip overlong encodings, because we want to
	 *       allow binary data to follow UTF-8 data.
	 */
	void SkipUtf8()
	{
		auto len = this->PeekUtf8().first;
		this->Skip(len > 0 ? len : 1); // advance at least one byte
	}

	/**
	 * Check whether the next data matches 'str'.
	 */
	[[nodiscard]] bool PeekIf(std::string_view str) const
	{
		return this->src.compare(this->position, str.size(), str) == 0;
	}
	/**
	 * Check whether the next data matches 'str', and skip it.
	 */
	[[nodiscard]] bool ReadIf(std::string_view str)
	{
		bool result = this->PeekIf(str);
		if (result) this->Skip(str.size());
		return result;
	}
	/**
	 * If the next data matches 'str', then skip it.
	 */
	void SkipIf(std::string_view str)
	{
		if (this->PeekIf(str)) this->Skip(str.size());
	}

	/**
	 * Check whether the next 8-bit char matches 'c'.
	 */
	[[nodiscard]] bool PeekCharIf(char c) const
	{
		return this->PeekIf({&c, 1});
	}
	/**
	 * Check whether the next 8-bit char matches 'c', and skip it.
	 */
	[[nodiscard]] bool ReadCharIf(char c)
	{
		return this->ReadIf({&c, 1});
	}
	/**
	 * If the next data matches the 8-bit char 'c', then skip it.
	 */
	void SkipCharIf(char c)
	{
		return this->SkipIf({&c, 1});
	}

	/**
	 * Check whether the next UTF-8 char matches 'c'.
	 */
	[[nodiscard]] bool PeekUtf8If(char32_t c) const
	{
		auto [len, result] = this->PeekUtf8();
		return len > 0 && result == c;
	}
	/**
	 * Check whether the next UTF-8 char matches 'c', and skip it.
	 */
	[[nodiscard]] bool ReadUtf8If(char32_t c)
	{
		auto [len, result] = this->PeekUtf8();
		if (len == 0 || result != c) return false;
		this->Skip(len);
		return true;
	}
	/**
	 * If the next data matches the UTF-8 char 'c', then skip it.
	 */
	void SkipUtf8If(char32_t c)
	{
		auto [len, result] = this->PeekUtf8();
		if (len > 0 && result == c) {
			this->Skip(len);
		}
	}

	/**
	 * Peek the next 'len' bytes.
	 * @param len Bytes to read, 'npos' to read all.
	 * @return Up to 'len' bytes.
	 */
	[[nodiscard]] std::string_view Peek(size_type len) const;
	/**
	 * Read the next 'len' bytes, and advance reader.
	 * @param len Bytes to read, 'npos' to read all.
	 * @return Up to 'len' bytes.
	 */
	[[nodiscard]] std::string_view Read(size_type len)
	{
		auto result = this->Peek(len);
		if (len != npos && len != result.size()) {
			LogError(fmt::format("Source buffer too short: {} > {}", len, result.size()));
		}
		this->Skip(result.size());
		return result;
	}
	/**
	 * Discard some bytes.
	 * @param len Number of bytes to skip, 'npos' to skip all.
	 */
	void Skip(size_type len);

	/**
	 * Find first occurrence of 'str'.
	 * @return Offset from current reader position. 'npos' if no match found.
	 */
	[[nodiscard]] size_type Find(std::string_view str) const;
	/**
	 * Find first occurrence of 8-bit char 'c'.
	 * @return Offset from current reader position. 'npos' if no match found.
	 */
	[[nodiscard]] size_type FindChar(char c) const
	{
		return this->Find({&c, 1});
	}
	/**
	 * Find first occurrence of UTF-8 char 'c'.
	 * @return Offset from current reader position. 'npos' if no match found.
	 */
	[[nodiscard]] size_type FindUtf8(char32_t c) const;

	/**
	 * Find first occurrence of any 8-bit char in 'chars'.
	 * @return Offset from current reader position. 'npos' if no match found.
	 */
	[[nodiscard]] size_type FindCharIn(std::string_view chars) const;
	/**
	 * Find first occurrence of any 8-bit char not in 'chars'.
	 * @return Offset from current reader position. 'npos' if no match found.
	 */
	[[nodiscard]] size_type FindCharNotIn(std::string_view chars) const;

	/**
	 * Check whether the next 8-bit char is in 'chars'.
	 * @return Matching char, std::nullopt if no match.
	 */
	[[nodiscard]] std::optional<char> PeekCharIfIn(std::string_view chars) const
	{
		assert(!chars.empty());
		std::optional<char> c = this->PeekChar();
		if (c.has_value() && chars.find(*c) != std::string_view::npos) return c;
		return std::nullopt;
	}
	/**
	 * Read next 8-bit char, check whether it is in 'chars', and advance reader.
	 * @return Matching char, std::nullopt if no match.
	 */
	[[nodiscard]] std::optional<char> ReadCharIfIn(std::string_view chars)
	{
		auto result = this->PeekCharIfIn(chars);
		if (result.has_value()) this->Skip(1);
		return result;
	}
	/**
	 * If the next 8-bit char is in 'chars', skip it.
	 */
	void SkipCharIfIn(std::string_view chars)
	{
		auto result = this->PeekCharIfIn(chars);
		if (result.has_value()) this->Skip(1);
	}

	/**
	 * Check whether the next 8-bit char is not in 'chars'.
	 * @return Non-matching char, std::nullopt if match.
	 */
	[[nodiscard]] std::optional<char> PeekCharIfNotIn(std::string_view chars) const
	{
		assert(!chars.empty());
		std::optional<char> c = this->PeekChar();
		if (c.has_value() && chars.find(*c) == std::string_view::npos) return c;
		return std::nullopt;
	}
	/**
	 * Read next 8-bit char, check whether it is not in 'chars', and advance reader.
	 * @return Non-matching char, std::nullopt if match.
	 */
	[[nodiscard]] std::optional<char> ReadCharIfNotIn(std::string_view chars)
	{
		auto result = this->PeekCharIfNotIn(chars);
		if (result.has_value()) this->Skip(1);
		return result;
	}
	/**
	 * If the next 8-bit char is not in 'chars', skip it.
	 */
	void SkipCharIfNotIn(std::string_view chars)
	{
		auto result = this->PeekCharIfNotIn(chars);
		if (result.has_value()) this->Skip(1);
	}

	/**
	 * Peek 8-bit chars, while they are not in 'chars', until they are.
	 * @return Non-matching chars.
	 */
	[[nodiscard]] std::string_view PeekUntilCharIn(std::string_view chars) const
	{
		size_type len = this->FindCharIn(chars);
		return this->Peek(len);
	}
	/**
	 * Read 8-bit chars, while they are not in 'chars', until they are; and advance reader.
	 * @return Non-matching chars.
	 */
	[[nodiscard]] std::string_view ReadUntilCharIn(std::string_view chars)
	{
		size_type len = this->FindCharIn(chars);
		return this->Read(len);
	}
	/**
	 * Skip 8-bit chars, while they are not in 'chars', until they are.
	 */
	void SkipUntilCharIn(std::string_view chars)
	{
		size_type len = this->FindCharIn(chars);
		this->Skip(len);
	}

	/**
	 * Peek 8-bit chars, while they are in 'chars', until they are not.
	 * @return Matching chars.
	 */
	[[nodiscard]] std::string_view PeekUntilCharNotIn(std::string_view chars) const
	{
		size_type len = this->FindCharNotIn(chars);
		return this->Peek(len);
	}
	/**
	 * Read 8-bit chars, while they are in 'chars', until they are not; and advance reader.
	 * @return Matching chars.
	 */
	[[nodiscard]] std::string_view ReadUntilCharNotIn(std::string_view chars)
	{
		size_type len = this->FindCharNotIn(chars);
		return this->Read(len);
	}
	/**
	 * Skip 8-bit chars, while they are in 'chars', until they are not.
	 */
	void SkipUntilCharNotIn(std::string_view chars)
	{
		size_type len = this->FindCharNotIn(chars);
		this->Skip(len);
	}

	/**
	 * Treatment of separator characters.
	 */
	enum SeparatorUsage {
		READ_ALL_SEPARATORS, ///< Read all consecutive separators, and include them all in the result
		READ_ONE_SEPARATOR,  ///< Read one separator, and include it in the result
		KEEP_SEPARATOR,      ///< Keep the separator in the data as next value to be read.
		SKIP_ONE_SEPARATOR,  ///< Read and discard one separator, do not include it in the result.
		SKIP_ALL_SEPARATORS, ///< Read and discard all consecutive separators, do not include any in the result.
	};

	/**
	 * Peek data until the first occurrence of 'str'.
	 * @param str Separator string.
	 * @param sep Whether to include/exclude 'str' from the result.
	 */
	[[nodiscard]] std::string_view PeekUntil(std::string_view str, SeparatorUsage sep) const;
	/**
	 * Read data until the first occurrence of 'str', and advance reader.
	 * @param str Separator string.
	 * @param sep Whether to include/exclude 'str' from the result, and/or skip it.
	 */
	[[nodiscard]] std::string_view ReadUntil(std::string_view str, SeparatorUsage sep)
	{
		assert(!str.empty());
		auto result = this->PeekUntil(str, sep);
		this->Skip(result.size());
		switch (sep) {
			default:
				break;
			case SKIP_ONE_SEPARATOR:
				this->SkipIf(str);
				break;
			case SKIP_ALL_SEPARATORS:
				while (this->ReadIf(str)) {}
				break;
		}
		return result;
	}
	/**
	 * Skip data until the first occurrence of 'str'.
	 * @param str Separator string.
	 * @param sep Whether to also skip 'str'.
	 */
	void SkipUntil(std::string_view str, SeparatorUsage sep)
	{
		assert(!str.empty());
		this->Skip(this->Find(str));
		switch (sep) {
			default:
				break;
			case READ_ONE_SEPARATOR:
			case SKIP_ONE_SEPARATOR:
				this->SkipIf(str);
				break;
			case READ_ALL_SEPARATORS:
			case SKIP_ALL_SEPARATORS:
				while (this->ReadIf(str)) {}
				break;
		}
	}

	/**
	 * Peek data until the first occurrence of 8-bit char 'c'.
	 * @param c Separator char.
	 * @param sep Whether to include/exclude 'c' from the result.
	 */
	[[nodiscard]] std::string_view PeekUntilChar(char c, SeparatorUsage sep) const
	{
		return this->PeekUntil({&c, 1}, sep);
	}
	/**
	 * Read data until the first occurrence of 8-bit char 'c', and advance reader.
	 * @param c Separator char.
	 * @param sep Whether to include/exclude 'c' from the result, and/or skip it.
	 */
	[[nodiscard]] std::string_view ReadUntilChar(char c, SeparatorUsage sep)
	{
		return this->ReadUntil({&c, 1}, sep);
	}
	/**
	 * Skip data until the first occurrence of 8-bit char 'c'.
	 * @param c Separator char.
	 * @param sep Whether to also skip 'c'.
	 */
	void SkipUntilChar(char c, SeparatorUsage sep)
	{
		this->SkipUntil({&c, 1}, sep);
	}

	/**
	 * Peek data until the first occurrence of UTF-8 char 'c'.
	 * @param c Separator char.
	 * @param sep Whether to include/exclude 'c' from the result.
	 */
	[[nodiscard]] std::string_view PeekUntilUtf8(char32_t c, SeparatorUsage sep) const;
	/**
	 * Read data until the first occurrence of UTF-8 char 'c', and advance reader.
	 * @param c Separator char.
	 * @param sep Whether to include/exclude 'c' from the result, and/or skip it.
	 */
	[[nodiscard]] std::string_view ReadUntilUtf8(char32_t c, SeparatorUsage sep);
	/**
	 * Skip data until the first occurrence of UTF-8 char 'c'.
	 * @param c Separator char.
	 * @param sep Whether to also skip 'c'.
	 */
	void SkipUntilUtf8(char32_t c, SeparatorUsage sep);

private:
	template <class T>
	[[nodiscard]] static std::pair<size_type, T> ParseIntegerBase(std::string_view src, int base, bool clamp, bool log_errors)
	{
		if (base == 0) {
			/* Try positive hex */
			if (src.starts_with("0x") || src.starts_with("0X")) {
				auto [len, value] = ParseIntegerBase<T>(src.substr(2), 16, clamp, log_errors);
				if (len == 0) return {};
				return {len + 2, value};
			}

			/* Try negative hex */
			if (std::is_signed_v<T> && (src.starts_with("-0x") || src.starts_with("-0X"))) {
				using Unsigned = std::make_unsigned_t<T>;
				auto [len, uvalue] = ParseIntegerBase<Unsigned>(src.substr(3), 16, clamp, log_errors);
				if (len == 0) return {};
				T value = static_cast<T>(0 - uvalue);
				if (value > 0) {
					if (!clamp) {
						if (log_errors) LogError(fmt::format("Integer out of range: '{}'", src.substr(0, len + 3)));
						return {};
					}
					value = std::numeric_limits<T>::lowest();
				}
				return {len + 3, value};
			}

			/* Try decimal */
			return ParseIntegerBase<T>(src, 10, clamp, log_errors);
		}

		T value{};
		assert(base == 8 || base == 10 || base == 16); // we only support these bases when skipping
		auto result = std::from_chars(src.data(), src.data() + src.size(), value, base);
		auto len = result.ptr - src.data();
		if (result.ec == std::errc::result_out_of_range) {
			if (!clamp) {
				if (log_errors) LogError(fmt::format("Integer out of range: '{}'+'{}'", src.substr(0, len), src.substr(len, 4)));
				return {};
			}
			if (src.starts_with("-")) {
				value = std::numeric_limits<T>::lowest();
			} else {
				value = std::numeric_limits<T>::max();
			}
		} else if (result.ec != std::errc{}) {
			if (log_errors) LogError(fmt::format("Cannot parse integer: '{}'+'{}'", src.substr(0, len), src.substr(len, 4)));
			return {};
		}
		return {len, value};
	}

public:
	/**
	 * Peek and parse an integer in number 'base'.
	 * If 'base == 0', then a prefix '0x' decides between base 16 or base 10.
	 * @param clamp If the value is a valid number, but out of range for T, return the maximum representable value.
	 *              Negative values for unsigned results are still treated as invalid.
	 * @return Length of string match, and parsed value.
	 * @note The parser rejects leading whitespace and unary plus.
	 */
	template <class T>
	[[nodiscard]] std::pair<size_type, T> PeekIntegerBase(int base, bool clamp = false) const
	{
		return ParseIntegerBase<T>(this->src.substr(this->position), base, clamp, false);
	}
	/**
	 * Try to read and parse an integer in number 'base', and then advance the reader.
	 * If 'base == 0', then a prefix '0x' decides between base 16 or base 10.
	 * @param clamp If the value is a valid number, but out of range for T, return the maximum representable value.
	 *              Negative values for unsigned results are still treated as invalid.
	 * @return Parsed value, if valid.
	 * @note The parser rejects leading whitespace and unary plus.
	 */
	template <class T>
	[[nodiscard]] std::optional<T> TryReadIntegerBase(int base, bool clamp = false)
	{
		auto [len, value] = this->PeekIntegerBase<T>(base, clamp);
		if (len == 0) return std::nullopt;
		this->SkipIntegerBase(base);
		return value;
	}
	/**
	 * Read and parse an integer in number 'base', and advance the reader.
	 * If 'base == 0', then a prefix '0x' decides between base 16 or base 10.
	 * @param clamp If the value is a valid number, but out of range for T, return the maximum representable value.
	 *              Negative values for unsigned results are still treated as invalid.
	 * @return Parsed value, or 'def' if invalid.
	 * @note The reader is advanced, even if no valid data was present.
	 * @note The parser rejects leading whitespace and unary plus.
	 */
	template <class T>
	[[nodiscard]] T ReadIntegerBase(int base, T def = 0, bool clamp = false)
	{
		auto [len, value] = ParseIntegerBase<T>(this->src.substr(this->position), base, clamp, true);
		this->SkipIntegerBase(base); // always advance
		return len > 0 ? value : def;
	}
	/**
	 * Skip an integer in number 'base'.
	 * If 'base == 0', then a prefix '0x' decides between base 16 or base 10.
	 * @note The reader is advanced, even if no valid data was present.
	 * @note The parser rejects leading whitespace and unary plus.
	 */
	void SkipIntegerBase(int base);
};

/**
 * Change a string into its number representation.
 * Supports decimal and hexadecimal numbers.
 * Accepts leading and trailing whitespace. Trailing junk is an error.
 * @param arg The string to be converted.
 * @param base The base for parsing the number, defaults to only decimal numbers. Use 0 to also allow hexadecimal.
 * @param clamp If the value is a valid number, but out of range for T, return the maximum representable value.
 *              Negative values for unsigned results are still treated as invalid.
 * @return The number, or std::nullopt if it could not be parsed.
 */
template <class T = uint32_t>
static inline std::optional<T> ParseInteger(std::string_view arg, int base = 10, bool clamp = false)
{
	StringConsumer consumer{arg};
	consumer.SkipUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE);
	auto result = consumer.TryReadIntegerBase<T>(base, clamp);
	consumer.SkipUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE);
	if (consumer.AnyBytesLeft()) return std::nullopt;
	return result;
}

#endif /* STRING_CONSUMER_HPP */
