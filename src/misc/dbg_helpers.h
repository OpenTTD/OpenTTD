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
 * or t_unk when index is out of bounds.
 */
template <typename E>
inline std::string_view ItemAt(E idx, std::span<const std::string_view> t, std::string_view t_unk)
{
	if (static_cast<size_t>(idx) >= std::size(t)) {
		return t_unk;
	}
	return t[idx];
}

/**
 * Helper template function that returns item of array at given index
 * or t_inv when index == idx_inv
 * or t_unk when index is out of bounds.
 */
template <typename E>
inline std::string_view ItemAt(E idx, std::span<const std::string_view> t, std::string_view t_unk, E idx_inv, std::string_view t_inv)
{
	if (idx == idx_inv) {
		return t_inv;
	}
	return ItemAt(idx, t, t_unk);
}

/**
 * Helper template function that returns compound bitfield name that is
 * concatenation of names of each set bit in the given value
 * or t_inv when index == idx_inv
 * or t_unk when index is out of bounds.
 */
template <typename E>
inline std::string ComposeName(E value, std::span<const std::string_view> t, std::string_view t_unk, E val_inv, std::string_view name_inv)
{
	std::string out;
	if (value == val_inv) {
		out = name_inv;
	} else if (value == 0) {
		out = "<none>";
	} else {
		for (size_t i = 0; i < std::size(t); i++) {
			if ((value & (1 << i)) == 0) continue;
			out += (!out.empty() ? "+" : "");
			out += t[i];
			value &= ~(E)(1 << i);
		}
		if (value != 0) {
			out += (!out.empty() ? "+" : "");
			out += t_unk;
		}
	}
	return out;
}

/**
 * Helper template function that returns compound bitfield name that is
 * concatenation of names of each set bit in the given value
 * or unknown_name when index is out of bounds.
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
		size_t      m_type_id;
		const void *m_ptr;

		KnownStructKey(size_t type_id, const void *ptr)
			: m_type_id(type_id)
			, m_ptr(ptr)
		{}

		bool operator<(const KnownStructKey &other) const
		{
			if ((size_t)m_ptr < (size_t)other.m_ptr) return true;
			if ((size_t)m_ptr > (size_t)other.m_ptr) return false;
			if (m_type_id < other.m_type_id) return true;
			return false;
		}
	};

	typedef std::map<KnownStructKey, std::string> KNOWN_NAMES;

	std::string m_out;                    ///< the output string
	int m_indent;                         ///< current indent/nesting level
	std::stack<std::string> m_cur_struct; ///< here we will track the current structure name
	KNOWN_NAMES m_known_names;            ///< map of known object instances and their structured names

	DumpTarget()
		: m_indent(0)
	{}

	static size_t &LastTypeId();
	std::string GetCurrentStructName();
	bool FindKnownName(size_t type_id, const void *ptr, std::string &name);

	void WriteIndent();

	/** Write 'name = value' with indent and new-line. */
	void WriteValue(std::string_view name, const auto &value)
	{
		WriteIndent();
		format_append(m_out, "{} = {}\n", name, value);
	}

	void WriteTile(std::string_view name, TileIndex t);

	/** Dump given enum value (as a number and as named value) */
	template <typename E> void WriteEnumT(std::string_view name, E e)
	{
		WriteValue(name, ValueStr(e));
	}

	void BeginStruct(size_t type_id, std::string_view name, const void *ptr);
	void EndStruct();

	/** Dump nested object (or only its name if this instance is already known). */
	template <typename S> void WriteStructT(std::string_view name, const S *s)
	{
		static const size_t type_id = ++LastTypeId();

		if (s == nullptr) {
			/* No need to dump nullptr struct. */
			WriteValue(name, "<null>");
			return;
		}
		std::string known_as;
		if (FindKnownName(type_id, s, known_as)) {
			/* We already know this one, no need to dump it. */
			std::string known_as_str = std::string("known_as.") + known_as;
			WriteValue(name, known_as_str);
		} else {
			/* Still unknown, dump it */
			BeginStruct(type_id, name, s);
			s->Dump(*this);
			EndStruct();
		}
	}

	/** Dump nested object (or only its name if this instance is already known). */
	template <typename S> void WriteStructT(std::string_view name, const std::deque<S> *s)
	{
		static const size_t type_id = ++LastTypeId();

		if (s == nullptr) {
			/* No need to dump nullptr struct. */
			WriteValue(name, "<null>");
			return;
		}
		std::string known_as;
		if (FindKnownName(type_id, s, known_as)) {
			/* We already know this one, no need to dump it. */
			std::string known_as_str = std::string("known_as.") + known_as;
			WriteValue(name, known_as_str);
		} else {
			/* Still unknown, dump it */
			BeginStruct(type_id, name, s);
			size_t num_items = s->size();
			this->WriteValue("num_items", num_items);
			for (size_t i = 0; i < num_items; i++) {
				const auto &item = (*s)[i];
				this->WriteStructT(fmt::format("item[{}]", i), &item);
			}
			EndStruct();
		}
	}
};

#endif /* DBG_HELPERS_H */
