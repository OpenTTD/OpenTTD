/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file dbg_helpers.cpp Helpers for outputting debug information. */

#include "../stdafx.h"
#include "../rail_map.h"
#include "../core/enum_type.hpp"
#include "dbg_helpers.h"

#include "../safeguards.h"

/** Trackdir & TrackdirBits short names. */
static const std::string_view trackdir_names[] = {
	"NE", "SE", "UE", "LE", "LS", "RS", "rne", "rse",
	"SW", "NW", "UW", "LW", "LN", "RN", "rsw", "rnw",
};

/** Return name of given Trackdir. */
std::string ValueStr(Trackdir td)
{
	return fmt::format("{} ({})", to_underlying(td), ItemAt(td, trackdir_names, "UNK", INVALID_TRACKDIR, "INV"));
}

/** Return composed name of given TrackdirBits. */
std::string ValueStr(TrackdirBits td_bits)
{
	return fmt::format("{} ({})", to_underlying(td_bits), ComposeName(td_bits, trackdir_names, "UNK", INVALID_TRACKDIR_BIT, "INV"));
}


/** DiagDirection short names. */
static const std::string_view diagdir_names[] = {
	"NE", "SE", "SW", "NW",
};

/** Return name of given DiagDirection. */
std::string ValueStr(DiagDirection dd)
{
	return fmt::format("{} ({})", to_underlying(dd), ItemAt(dd, diagdir_names, "UNK", INVALID_DIAGDIR, "INV"));
}


/** SignalType short names. */
static const std::string_view signal_type_names[] = {
	"NORMAL", "ENTRY", "EXIT", "COMBO", "PBS", "NOENTRY",
};

/** Return name of given SignalType. */
std::string ValueStr(SignalType t)
{
	return fmt::format("{} ({})", to_underlying(t), ItemAt(t, signal_type_names, "UNK"));
}


/** Translate TileIndex into string. */
std::string TileStr(TileIndex tile)
{
	return fmt::format("0x{:04X} ({}, {})", tile.base(), TileX(tile), TileY(tile));
}

/**
 * Keep track of the last assigned type_id. Used for anti-recursion.
 *static*/ size_t& DumpTarget::LastTypeId()
{
	static size_t last_type_id = 0;
	return last_type_id;
}

/** Return structured name of the current class/structure. */
std::string DumpTarget::GetCurrentStructName()
{
	std::string out;
	if (!m_cur_struct.empty()) {
		/* we are inside some named struct, return its name */
		out = m_cur_struct.top();
	}
	return out;
}

/**
 * Find the given instance in our anti-recursion repository.
 * Return true and set name when object was found.
 */
bool DumpTarget::FindKnownName(size_t type_id, const void *ptr, std::string &name)
{
	KNOWN_NAMES::const_iterator it = m_known_names.find(KnownStructKey(type_id, ptr));
	if (it != m_known_names.end()) {
		/* we have found it */
		name = it->second;
		return true;
	}
	return false;
}

/** Write some leading spaces into the output. */
void DumpTarget::WriteIndent()
{
	int num_spaces = 2 * m_indent;
	if (num_spaces > 0) {
		m_out += std::string(num_spaces, ' ');
	}
}

/** Write name & TileIndex to the output. */
void DumpTarget::WriteTile(std::string_view name, TileIndex tile)
{
	WriteIndent();
	format_append(m_out, "{} = {}\n", name, TileStr(tile));
}

/**
 * Open new structure (one level deeper than the current one) 'name = {\<LF\>'.
 */
void DumpTarget::BeginStruct(size_t type_id, std::string_view name, const void *ptr)
{
	/* make composite name */
	std::string cur_name = GetCurrentStructName();
	if (!cur_name.empty()) {
		/* add name delimiter (we use structured names) */
		cur_name += ".";
	}
	cur_name += name;

	/* put the name onto stack (as current struct name) */
	m_cur_struct.push(cur_name);

	/* put it also to the map of known structures */
	m_known_names.insert(KNOWN_NAMES::value_type(KnownStructKey(type_id, ptr), cur_name));

	WriteIndent();
	format_append(m_out, "{} = {{\n", name);
	m_indent++;
}

/**
 * Close structure '}\<LF\>'.
 */
void DumpTarget::EndStruct()
{
	m_indent--;
	WriteIndent();
	m_out += "}\n";

	/* remove current struct name from the stack */
	m_cur_struct.pop();
}
