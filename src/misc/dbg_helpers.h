/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file dbg_helpers.h Functions to be used for debug printings. */

#ifndef DBG_HELPERS_H
#define DBG_HELPERS_H

#include <stack>

#include "../direction_type.h"
#include "../signal_type.h"
#include "../tile_type.h"
#include "../track_type.h"
#include "../core/format.hpp"

/**
 * Helper template function that returns item of array at given index
 * or unknown_name when index is out of bounds.
 * @param idx The index into the span.
 * @param names Span of names to index.
 * @param unknown_name The default value when the index does not exist.
 * @return The idx'th string of \c names, or \c unknown_name.
 */
template <typename E>
inline std::string_view ItemAt(E idx, std::span<const std::string_view> names, std::string_view unknown_name)
{
	if (static_cast<size_t>(idx) >= std::size(names)) {
		return unknown_name;
	}
	return names[idx];
}

/**
 * Helper template function that returns item of array at given index
 * or invalid_name when index == invalid_index
 * or unknown_name when index is out of bounds.
 * @param idx The index into the span.
 * @param names Span of names to index.
 * @param unknown_name The default value when the index does not exist.
 * @param invalid_index The invalid value to consider.
 * @param invalid_name The value to output when the invalid value is given.
 * @return The idx'th string of \c names, \c invalid_name, or \c unknown_name.
 */
template <typename E>
inline std::string_view ItemAt(E idx, std::span<const std::string_view> names, std::string_view unknown_name, E invalid_index, std::string_view invalid_name)
{
	if (idx == invalid_index) {
		return invalid_name;
	}
	return ItemAt(idx, names, unknown_name);
}

/**
 * Helper template function that returns compound bitfield name that is
 * concatenation of names of each set bit in the given value
 * or invalid_name when index == invalid_index
 * or unknown_name when index is out of bounds.
 * @param value The bitmask of values.
 * @param names Span of names to index.
 * @param unknown_name The default value when the index does not exist.
 * @param invalid_index The invalid value to consider.
 * @param invalid_name The value to output when the invalid value is given.
 * @return The composed name.
 */
template <typename E>
inline std::string ComposeName(E value, std::span<const std::string_view> names, std::string_view unknown_name, E invalid_index, std::string_view invalid_name)
{
	std::string out;
	if (value == invalid_index) {
		out = invalid_name;
	} else if (value == 0) {
		out = "<none>";
	} else {
		for (size_t i = 0; i < std::size(names); i++) {
			if ((value & (1 << i)) == 0) continue;
			out += (!out.empty() ? "+" : "");
			out += names[i];
			value &= ~(E)(1 << i);
		}
		if (value != 0) {
			out += (!out.empty() ? "+" : "");
			out += unknown_name;
		}
	}
	return out;
}

/**
 * Helper template function that returns compound bitfield name that is
 * concatenation of names of each set bit in the given value
 * or unknown_name when index is out of bounds.
 * @param value The bitmask of values.
 * @param names Span of names to index.
 * @param unknown_name The default value when the index does not exist.
 * @return The composed name.
 */
template <typename E>
inline std::string ComposeName(E value, std::span<const std::string_view> names, std::string_view unknown_name)
{
	std::string out;
	if (value.base() == 0) {
		out = "<none>";
	} else {
		for (size_t i = 0; i < std::size(names); ++i) {
			if (!value.Test(static_cast<E::EnumType>(i))) continue;
			out += (!out.empty() ? "+" : "");
			out += names[i];
			value.Reset(static_cast<E::EnumType>(i));
		}
		if (value.base() != 0) {
			out += (!out.empty() ? "+" : "");
			out += unknown_name;
		}
	}
	return out;
}

std::string ValueStr(Trackdir td);
std::string ValueStr(TrackdirBits td_bits);
std::string ValueStr(DiagDirection dd);
std::string ValueStr(SignalType t);

/** Class that represents the dump-into-string target. */
struct DumpTarget {
	/** Used as a key into map of known object instances. */
	struct KnownStructKey {
		size_t type_id; ///< Unique identifier of the type.
		const void *ptr; ///< Pointer to the structure.

		/**
		 * Create the key.
		 * @param type_id Unique type identifier.
		 * @param ptr The structure.
		 */
		KnownStructKey(size_t type_id, const void *ptr) : type_id(type_id), ptr(ptr) {}

		bool operator<(const KnownStructKey &other) const
		{
			if (reinterpret_cast<size_t>(this->ptr) < reinterpret_cast<size_t>(other.ptr)) return true;
			if (reinterpret_cast<size_t>(this->ptr) > reinterpret_cast<size_t>(other.ptr)) return false;
			if (this->type_id < other.type_id) return true;
			return false;
		}
	};

	/** Mapping of the KnownStructKey to the name for that structure. */
	using KnownNamesMap = std::map<KnownStructKey, std::string>;

	std::string output_buffer; ///< The output string.
	int indent = 0; ///< Current indent/nesting level.
	std::stack<std::string> cur_struct; ///< Tracker of the current structure name.
	KnownNamesMap known_names; ///< Map of known object instances and their structured names.

	static size_t NewTypeId();
	std::string GetCurrentStructName();
	std::optional<std::string> FindKnownAsName(size_t type_id, const void *ptr);

	void WriteIndent();

	/**
	 * Write 'name = value' with indent and new-line.
	 * @param name The name.
	 * @param value The actual value to write.
	 */
	void WriteValue(std::string_view name, const auto &value)
	{
		this->WriteIndent();
		format_append(this->output_buffer, "{} = {}\n", name, value);
	}

	void WriteTile(std::string_view name, TileIndex t);

	/**
	 * Dump given enum value (as a number and as named value).
	 * @param name The name of the enumeration.
	 * @param e The value of the enumeration.
	 */
	template <typename E> void WriteEnumT(std::string_view name, E e)
	{
		this->WriteValue(name, ValueStr(e));
	}

	void BeginStruct(size_t type_id, std::string_view name, const void *ptr);
	void EndStruct();

	/**
	 * Dump nested object (or only its name if this instance is already known).
	 * @param name The name of the struct.
	 * @param s Pointer to the struct.
	 */
	template <typename S> void WriteStructT(std::string_view name, const S *s)
	{
		static const size_t type_id = this->NewTypeId();

		if (s == nullptr) {
			/* No need to dump nullptr struct. */
			this->WriteValue(name, "<null>");
			return;
		}
		if (auto known_as = this->FindKnownAsName(type_id, s); known_as.has_value()) {
			/* We already know this one, no need to dump it. */
			this->WriteValue(name, *known_as);
		} else {
			/* Still unknown, dump it */
			this->BeginStruct(type_id, name, s);
			s->Dump(*this);
			this->EndStruct();
		}
	}

	/**
	 * Dump nested object (or only its name if this instance is already known).
	 * @param name The name of the struct.
	 * @param s Pointer to the std::deque of structs.
	 */
	template <typename S> void WriteStructT(std::string_view name, const std::deque<S> *s)
	{
		static const size_t type_id = this->NewTypeId();

		if (s == nullptr) {
			/* No need to dump nullptr struct. */
			this->WriteValue(name, "<null>");
			return;
		}
		if (auto known_as = this->FindKnownAsName(type_id, s); known_as.has_value()) {
			/* We already know this one, no need to dump it. */
			this->WriteValue(name, *known_as);
		} else {
			/* Still unknown, dump it */
			this->BeginStruct(type_id, name, s);
			size_t num_items = s->size();
			this->WriteValue("num_items", num_items);
			for (size_t i = 0; i < num_items; i++) {
				const auto &item = (*s)[i];
				this->WriteStructT(fmt::format("item[{}]", i), &item);
			}
			this->EndStruct();
		}
	}
};

#endif /* DBG_HELPERS_H */
