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

/**
 * Return name of given Trackdir.
 * @param td The track direction.
 * @return String representation of the track direction.
 */
std::string ValueStr(Trackdir td)
{
	return fmt::format("{} ({})", to_underlying(td), ItemAt(td, trackdir_names, "UNK", INVALID_TRACKDIR, "INV"));
}

/**
 * Return composed name of given TrackdirBits.
 * @param td_bits The track direction bits.
 * @return The string representation.
 */
std::string ValueStr(TrackdirBits td_bits)
{
	return fmt::format("{} ({})", to_underlying(td_bits), ComposeName(td_bits, trackdir_names, "UNK", INVALID_TRACKDIR_BIT, "INV"));
}


/** DiagDirection short names. */
static const std::string_view diagdir_names[] = {
	"NE", "SE", "SW", "NW",
};

/**
 * Return name of given DiagDirection.
 * @param dd The diagonal direction.
 * @return The string representation.
 */
std::string ValueStr(DiagDirection dd)
{
	return fmt::format("{} ({})", to_underlying(dd), ItemAt(dd, diagdir_names, "UNK", INVALID_DIAGDIR, "INV"));
}


/** SignalType short names. */
static const std::string_view signal_type_names[] = {
	"NORMAL", "ENTRY", "EXIT", "COMBO", "PBS", "NOENTRY",
};

/**
 * Return name of given SignalType.
 * @param t The signal type.
 * @return The string representation.
 */
std::string ValueStr(SignalType t)
{
	return fmt::format("{} ({})", to_underlying(t), ItemAt(t, signal_type_names, "UNK"));
}


/**
 * Translate TileIndex into string.
 * @param tile The tile to convert.
 * @return The string representation.
 */
std::string TileStr(TileIndex tile)
{
	return fmt::format("0x{:04X} ({}, {})", tile.base(), TileX(tile), TileY(tile));
}

/**
 * Create a new type_id. Used for anti-recursion.
 * @return The new type_id.
 */
/* static */ size_t DumpTarget::NewTypeId()
{
	static size_t last_type_id = 0;
	return ++last_type_id;
}

/**
 * Return structured name of the current class/structure.
 * @return The name of the current string.
 */
std::string DumpTarget::GetCurrentStructName()
{
	if (this->cur_struct.empty()) return {};

	/* we are inside some named struct, return its name */
	return this->cur_struct.top();
}

/**
 * Find the given instance in our anti-recursion repository.
 * @param type_id Identifier of the type being dumped.
 * @param ptr The content of the struct.
 * @return The name or \c std::nullopt when the type was not found.
 */
std::optional<std::string> DumpTarget::FindKnownAsName(size_t type_id, const void *ptr)
{
	KnownNamesMap::const_iterator it = this->known_names.find(KnownStructKey(type_id, ptr));
	if (it == this->known_names.end()) return std::nullopt;

	return fmt::format("known_as.{}", it->second);
}

/** Write some leading spaces into the output. */
void DumpTarget::WriteIndent()
{
	int num_spaces = 2 * this->indent;
	if (num_spaces > 0) {
		this->output_buffer += std::string(num_spaces, ' ');
	}
}

/**
 * Write name & TileIndex to the output.
 * @param name The name to output.
 * @param tile The tile to output.
 */
void DumpTarget::WriteTile(std::string_view name, TileIndex tile)
{
	this->WriteIndent();
	format_append(this->output_buffer, "{} = {}\n", name, TileStr(tile));
}

/**
 * Open new structure (one level deeper than the current one) 'name = {\<LF\>'.
 * @param type_id Identifier of the type being dumped.
 * @param name The name of the struct.
 * @param ptr The content of the struct.
 */
void DumpTarget::BeginStruct(size_t type_id, std::string_view name, const void *ptr)
{
	/* make composite name */
	std::string cur_name = this->GetCurrentStructName();
	if (!cur_name.empty()) {
		/* add name delimiter (we use structured names) */
		cur_name += ".";
	}
	cur_name += name;

	/* put the name onto stack (as current struct name) */
	this->cur_struct.push(cur_name);

	/* put it also to the map of known structures */
	this->known_names.insert(KnownNamesMap::value_type(KnownStructKey(type_id, ptr), cur_name));

	this->WriteIndent();
	format_append(this->output_buffer, "{} = {{\n", name);
	this->indent++;
}

/**
 * Close structure '}\<LF\>'.
 */
void DumpTarget::EndStruct()
{
	this->indent--;
	this->WriteIndent();
	this->output_buffer += "}\n";

	/* remove current struct name from the stack */
	this->cur_struct.pop();
}
