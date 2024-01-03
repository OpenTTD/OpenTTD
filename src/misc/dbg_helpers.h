/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dbg_helpers.h Functions to be used for debug printings. */

#ifndef DBG_HELPERS_H
#define DBG_HELPERS_H

#include <stack>

#include "../direction_type.h"
#include "../signal_type.h"
#include "../tile_type.h"
#include "../track_type.h"

/** Helper template class that provides C array length and item type */
template <typename T> struct ArrayT;

/** Helper template class that provides C array length and item type */
template <typename T, size_t N> struct ArrayT<T[N]> {
	static const size_t length = N;
	using Item = T;
};


/**
 * Helper template function that returns item of array at given index
 * or t_unk when index is out of bounds.
 */
template <typename E, typename T>
inline typename ArrayT<T>::Item ItemAtT(E idx, const T &t, typename ArrayT<T>::Item t_unk)
{
	if ((size_t)idx >= ArrayT<T>::length) {
		return t_unk;
	}
	return t[idx];
}

/**
 * Helper template function that returns item of array at given index
 * or t_inv when index == idx_inv
 * or t_unk when index is out of bounds.
 */
template <typename E, typename T>
inline typename ArrayT<T>::Item ItemAtT(E idx, const T &t, typename ArrayT<T>::Item t_unk, E idx_inv, typename ArrayT<T>::Item t_inv)
{
	if ((size_t)idx < ArrayT<T>::length) {
		return t[idx];
	}
	if (idx == idx_inv) {
		return t_inv;
	}
	return t_unk;
}

/**
 * Helper template function that returns compound bitfield name that is
 * concatenation of names of each set bit in the given value
 * or t_inv when index == idx_inv
 * or t_unk when index is out of bounds.
 */
template <typename E, typename T>
inline std::string ComposeNameT(E value, T &t, const char *t_unk, E val_inv, const char *name_inv)
{
	std::string out;
	if (value == val_inv) {
		out = name_inv;
	} else if (value == 0) {
		out = "<none>";
	} else {
		for (size_t i = 0; i < ArrayT<T>::length; i++) {
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

	void WriteValue(const std::string &name, int value);
	void WriteValue(const std::string &name, const std::string &value_str);
	void WriteTile(const std::string &name, TileIndex t);

	/** Dump given enum value (as a number and as named value) */
	template <typename E> void WriteEnumT(const std::string &name, E e)
	{
		WriteValue(name, ValueStr(e));
	}

	void BeginStruct(size_t type_id, const std::string &name, const void *ptr);
	void EndStruct();

	/** Dump nested object (or only its name if this instance is already known). */
	template <typename S> void WriteStructT(const std::string &name, const S *s)
	{
		static size_t type_id = ++LastTypeId();

		if (s == nullptr) {
			/* No need to dump nullptr struct. */
			WriteValue(name, "<null>");
			return;
		}
		std::string known_as;
		if (FindKnownName(type_id, s, known_as)) {
			/* We already know this one, no need to dump it. */
			std::string known_as_str = std::string("known_as.") + name;
			WriteValue(name, known_as_str);
		} else {
			/* Still unknown, dump it */
			BeginStruct(type_id, name, s);
			s->Dump(*this);
			EndStruct();
		}
	}
};

#endif /* DBG_HELPERS_H */
