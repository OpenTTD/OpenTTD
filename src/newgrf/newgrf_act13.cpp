/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_act13.cpp NewGRF Action 0x13 handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_text.h"
#include "../strings_func.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "table/strings.h"

#include "../safeguards.h"

/** Action 0x13 */
static void TranslateGRFStrings(ByteReader &buf)
{
	/* <13> <grfid> <num-ent> <offset> <text...>
	 *
	 * 4*B grfid     The GRFID of the file whose texts are to be translated
	 * B   num-ent   Number of strings
	 * W   offset    First text ID
	 * S   text...   Zero-terminated strings */

	uint32_t grfid = buf.ReadDWord();
	const GRFConfig *c = GetGRFConfig(grfid);
	if (c == nullptr || (c->status != GCS_INITIALISED && c->status != GCS_ACTIVATED)) {
		GrfMsg(7, "TranslateGRFStrings: GRFID 0x{:08X} unknown, skipping action 13", std::byteswap(grfid));
		return;
	}

	if (c->status == GCS_INITIALISED) {
		/* If the file is not active but will be activated later, give an error
		 * and disable this file. */
		GRFError *error = DisableGrf(STR_NEWGRF_ERROR_LOAD_AFTER);

		error->data = GetString(STR_NEWGRF_ERROR_AFTER_TRANSLATED_FILE);

		return;
	}

	/* Since no language id is supplied for with version 7 and lower NewGRFs, this string has
	 * to be added as a generic string, thus the language id of 0x7F. For this to work
	 * new_scheme has to be true as well, which will also be implicitly the case for version 8
	 * and higher. A language id of 0x7F will be overridden by a non-generic id, so this will
	 * not change anything if a string has been provided specifically for this language. */
	uint8_t language = _cur_gps.grffile->grf_version >= 8 ? buf.ReadByte() : 0x7F;
	uint8_t num_strings = buf.ReadByte();
	uint16_t first_id  = buf.ReadWord();

	if (!((first_id >= 0xD000 && first_id + num_strings <= 0xD400) || (first_id >= 0xD800 && first_id + num_strings <= 0xE000))) {
		GrfMsg(7, "TranslateGRFStrings: Attempting to set out-of-range string IDs in action 13 (first: 0x{:04X}, number: 0x{:02X})", first_id, num_strings);
		return;
	}

	for (uint i = 0; i < num_strings && buf.HasData(); i++) {
		std::string_view string = buf.ReadString();

		if (string.empty()) {
			GrfMsg(7, "TranslateGRFString: Ignoring empty string.");
			continue;
		}

		AddGRFString(grfid, GRFStringID(first_id + i), language, true, true, string, STR_UNDEFINED);
	}
}

template <> void GrfActionHandler<0x13>::FileScan(ByteReader &) { }
template <> void GrfActionHandler<0x13>::SafetyScan(ByteReader &) { }
template <> void GrfActionHandler<0x13>::LabelScan(ByteReader &) { }
template <> void GrfActionHandler<0x13>::Init(ByteReader &) { }
template <> void GrfActionHandler<0x13>::Reserve(ByteReader &) { }
template <> void GrfActionHandler<0x13>::Activation(ByteReader &buf) { TranslateGRFStrings(buf); }
