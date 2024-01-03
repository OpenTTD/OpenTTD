/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file endian_buffer.hpp Endian-aware buffer. */

#ifndef ENDIAN_BUFFER_HPP
#define ENDIAN_BUFFER_HPP

#include <string_view>
#include "../core/span_type.hpp"
#include "../core/bitmath_func.hpp"
#include "../core/overflowsafe_type.hpp"

struct StrongTypedefBase;

/**
 * Endian-aware buffer adapter that always writes values in little endian order.
 * @note This class uses operator overloading (<<, just like streams) for writing
 * as this allows providing custom operator overloads for more complex types
 * like e.g. structs without needing to modify this class.
 */
template <typename Tcont = typename std::vector<byte>, typename Titer = typename std::back_insert_iterator<Tcont>>
class EndianBufferWriter {
	/** Output iterator for the destination buffer. */
	Titer buffer;

public:
	EndianBufferWriter(Titer buffer) : buffer(buffer) {}
	EndianBufferWriter(typename Titer::container_type &container) : buffer(std::back_inserter(container)) {}

	EndianBufferWriter &operator <<(const std::string &data) { return *this << std::string_view{ data }; }
	EndianBufferWriter &operator <<(const char *data) { return *this << std::string_view{ data }; }
	EndianBufferWriter &operator <<(std::string_view data) { this->Write(data); return *this; }
	EndianBufferWriter &operator <<(bool data) { return *this << static_cast<byte>(data ? 1 : 0); }

	template <typename T>
	EndianBufferWriter &operator <<(const OverflowSafeInt<T> &data) { return *this << static_cast<T>(data); };

	template <typename... Targs>
	EndianBufferWriter &operator <<(const std::tuple<Targs...> &data)
	{
		this->WriteTuple(data, std::index_sequence_for<Targs...>{});
		return *this;
	}

	template <class T, std::enable_if_t<std::disjunction_v<std::negation<std::is_class<T>>, std::is_base_of<StrongTypedefBase, T>>, int> = 0>
	EndianBufferWriter &operator <<(const T data)
	{
		if constexpr (std::is_enum_v<T>) {
			this->Write(static_cast<std::underlying_type_t<const T>>(data));
		} else if constexpr (std::is_base_of_v<StrongTypedefBase, T>) {
			this->Write(data.base());
		} else {
			this->Write(data);
		}
		return *this;
	}

	template <typename Tvalue, typename Tbuf = std::vector<byte>>
	static Tbuf FromValue(const Tvalue &data)
	{
		Tbuf buffer;
		EndianBufferWriter writer{ buffer };
		writer << data;
		return buffer;
	}

private:
	/** Helper function to write a tuple to the buffer. */
	template<class Ttuple, size_t... Tindices>
	void WriteTuple(const Ttuple &values, std::index_sequence<Tindices...>)
	{
		((*this << std::get<Tindices>(values)), ...);
	}

	/** Write overload for string values. */
	void Write(std::string_view value)
	{
		for (auto c : value) {
			this->buffer++ = c;
		}
		this->buffer++ = '\0';
	}

	/** Fundamental write function. */
	template <class T>
	void Write(T value)
	{
		static_assert(sizeof(T) <= 8, "Value can't be larger than 8 bytes");

		if constexpr (sizeof(T) > 1) {
			this->buffer++ = GB(value, 0, 8);
			this->buffer++ = GB(value, 8, 8);
			if constexpr (sizeof(T) > 2) {
				this->buffer++ = GB(value, 16, 8);
				this->buffer++ = GB(value, 24, 8);
			}
			if constexpr (sizeof(T) > 4) {
				this->buffer++ = GB(value, 32, 8);
				this->buffer++ = GB(value, 40, 8);
				this->buffer++ = GB(value, 48, 8);
				this->buffer++ = GB(value, 56, 8);
			}
		} else {
			this->buffer++ = value;
		}
	}
};

/**
 * Endian-aware buffer adapter that always reads values in little endian order.
 * @note This class uses operator overloading (>>, just like streams) for reading
 * as this allows providing custom operator overloads for more complex types
 * like e.g. structs without needing to modify this class.
 */
class EndianBufferReader {
	/** Reference to storage buffer. */
	span<const byte> buffer;
	/** Current read position. */
	size_t read_pos = 0;

public:
	EndianBufferReader(span<const byte> buffer) : buffer(buffer) {}

	void rewind() { this->read_pos = 0; }

	EndianBufferReader &operator >>(std::string &data) { data = this->ReadStr(); return *this; }
	EndianBufferReader &operator >>(bool &data) { data = this->Read<byte>() != 0; return *this; }

	template <typename T>
	EndianBufferReader &operator >>(OverflowSafeInt<T> &data) { data = this->Read<T>(); return *this; };

	template <typename... Targs>
	EndianBufferReader &operator >>(std::tuple<Targs...> &data)
	{
		this->ReadTuple(data, std::index_sequence_for<Targs...>{});
		return *this;
	}

	template <class T, std::enable_if_t<std::disjunction_v<std::negation<std::is_class<T>>, std::is_base_of<StrongTypedefBase, T>>, int> = 0>
	EndianBufferReader &operator >>(T &data)
	{
		if constexpr (std::is_enum_v<T>) {
			data = static_cast<T>(this->Read<std::underlying_type_t<T>>());
		} else if constexpr (std::is_base_of_v<StrongTypedefBase, T>) {
			data = this->Read<typename T::BaseType>();
		} else {
			data = this->Read<T>();
		}
		return *this;
	}

	template <typename Tvalue>
	static Tvalue ToValue(span<const byte> buffer)
	{
		Tvalue result{};
		EndianBufferReader reader{ buffer };
		reader >> result;
		return result;
	}

private:
	/** Helper function to read a tuple from the buffer. */
	template<class Ttuple, size_t... Tindices>
	void ReadTuple(Ttuple &values, std::index_sequence<Tindices...>)
	{
		((*this >> std::get<Tindices>(values)), ...);
	}

	/** Read overload for string data. */
	std::string ReadStr()
	{
		std::string str;
		while (this->read_pos < this->buffer.size()) {
			char ch = this->Read<char>();
			if (ch == '\0') break;
			str.push_back(ch);
		}
		return str;
	}

	/** Fundamental read function. */
	template <class T>
	T Read()
	{
		static_assert(!std::is_const_v<T>, "Can't read into const variables");
		static_assert(sizeof(T) <= 8, "Value can't be larger than 8 bytes");

		if (read_pos + sizeof(T) > this->buffer.size()) return {};

		T value = static_cast<T>(this->buffer[this->read_pos++]);
		if constexpr (sizeof(T) > 1) {
			value += static_cast<T>(this->buffer[this->read_pos++]) << 8;
		}
		if constexpr (sizeof(T) > 2) {
			value += static_cast<T>(this->buffer[this->read_pos++]) << 16;
			value += static_cast<T>(this->buffer[this->read_pos++]) << 24;
		}
		if constexpr (sizeof(T) > 4) {
			value += static_cast<T>(this->buffer[this->read_pos++]) << 32;
			value += static_cast<T>(this->buffer[this->read_pos++]) << 40;
			value += static_cast<T>(this->buffer[this->read_pos++]) << 48;
			value += static_cast<T>(this->buffer[this->read_pos++]) << 56;
		}

		return value;
	}
};

#endif /* ENDIAN_BUFFER_HPP */
