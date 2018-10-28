/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dbg_helpers.cpp Helpers for outputting debug information. */

#include "../stdafx.h"
#include "../rail_map.h"
#include "dbg_helpers.h"

#include "../safeguards.h"

/** Trackdir & TrackdirBits short names. */
static const char * const trackdir_names[] = {
	"NE", "SE", "UE", "LE", "LS", "RS", "rne", "rse",
	"SW", "NW", "UW", "LW", "LN", "RN", "rsw", "rnw",
};

/** Return name of given Trackdir. */
CStrA ValueStr(Trackdir td)
{
	CStrA out;
	out.Format("%d (%s)", td, ItemAtT(td, trackdir_names, "UNK", INVALID_TRACKDIR, "INV"));
	return out.Transfer();
}

/** Return composed name of given TrackdirBits. */
CStrA ValueStr(TrackdirBits td_bits)
{
	CStrA out;
	out.Format("%d (%s)", td_bits, ComposeNameT(td_bits, trackdir_names, "UNK", INVALID_TRACKDIR_BIT, "INV").Data());
	return out.Transfer();
}


/** DiagDirection short names. */
static const char * const diagdir_names[] = {
	"NE", "SE", "SW", "NW",
};

/** Return name of given DiagDirection. */
CStrA ValueStr(DiagDirection dd)
{
	CStrA out;
	out.Format("%d (%s)", dd, ItemAtT(dd, diagdir_names, "UNK", INVALID_DIAGDIR, "INV"));
	return out.Transfer();
}


/** SignalType short names. */
static const char * const signal_type_names[] = {
	"NORMAL", "ENTRY", "EXIT", "COMBO", "PBS", "NOENTRY",
};

/** Return name of given SignalType. */
CStrA ValueStr(SignalType t)
{
	CStrA out;
	out.Format("%d (%s)", t, ItemAtT(t, signal_type_names, "UNK"));
	return out.Transfer();
}


/** Translate TileIndex into string. */
CStrA TileStr(TileIndex tile)
{
	CStrA out;
	out.Format("0x%04X (%d, %d)", tile, TileX(tile), TileY(tile));
	return out.Transfer();
}

/**
 * Keep track of the last assigned type_id. Used for anti-recursion.
 *static*/ size_t& DumpTarget::LastTypeId()
{
	static size_t last_type_id = 0;
	return last_type_id;
}

/** Return structured name of the current class/structure. */
CStrA DumpTarget::GetCurrentStructName()
{
	CStrA out;
	if (!m_cur_struct.empty()) {
		/* we are inside some named struct, return its name */
		out = m_cur_struct.top();
	}
	return out.Transfer();
}

/**
 * Find the given instance in our anti-recursion repository.
 * Return true and set name when object was found.
 */
bool DumpTarget::FindKnownName(size_t type_id, const void *ptr, CStrA &name)
{
	KNOWN_NAMES::const_iterator it = m_known_names.find(KnownStructKey(type_id, ptr));
	if (it != m_known_names.end()) {
		/* we have found it */
		name = (*it).second;
		return true;
	}
	return false;
}

/** Write some leading spaces into the output. */
void DumpTarget::WriteIndent()
{
	int num_spaces = 2 * m_indent;
	if (num_spaces > 0) {
		memset(m_out.GrowSizeNC(num_spaces), ' ', num_spaces);
	}
}

/** Write a line with indent at the beginning and \<LF\> at the end. */
void DumpTarget::WriteLine(const char *format, ...)
{
	WriteIndent();
	va_list args;
	va_start(args, format);
	m_out.AddFormatL(format, args);
	va_end(args);
	m_out.AppendStr("\n");
}

/** Write 'name = value' with indent and new-line. */
void DumpTarget::WriteValue(const char *name, const char *value_str)
{
	WriteIndent();
	m_out.AddFormat("%s = %s\n", name, value_str);
}

/** Write name & TileIndex to the output. */
void DumpTarget::WriteTile(const char *name, TileIndex tile)
{
	WriteIndent();
	m_out.AddFormat("%s = %s\n", name, TileStr(tile).Data());
}

/**
 * Open new structure (one level deeper than the current one) 'name = {\<LF\>'.
 */
void DumpTarget::BeginStruct(size_t type_id, const char *name, const void *ptr)
{
	/* make composite name */
	CStrA cur_name = GetCurrentStructName().Transfer();
	if (cur_name.Size() > 0) {
		/* add name delimiter (we use structured names) */
		cur_name.AppendStr(".");
	}
	cur_name.AppendStr(name);

	/* put the name onto stack (as current struct name) */
	m_cur_struct.push(cur_name);

	/* put it also to the map of known structures */
	m_known_names.insert(KNOWN_NAMES::value_type(KnownStructKey(type_id, ptr), cur_name));

	WriteIndent();
	m_out.AddFormat("%s = {\n", name);
	m_indent++;
}

/**
 * Close structure '}\<LF\>'.
 */
void DumpTarget::EndStruct()
{
	m_indent--;
	WriteIndent();
	m_out.AddFormat("}\n");

	/* remove current struct name from the stack */
	m_cur_struct.pop();
}

/** Just to silence an unsilencable GCC 4.4+ warning */
/* static */ ByteBlob::BlobHeader ByteBlob::hdrEmpty[] = {{0, 0}, {0, 0}};
