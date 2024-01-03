/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strings_sl.cpp Code handling saving and loading of strings */

#include "../stdafx.h"
#include "../core/alloc_func.hpp"
#include "../string_func.h"
#include "../strings_func.h"
#include "saveload_internal.h"
#include <sstream>

#include "table/strings.h"

#include "../safeguards.h"

static const int NUM_OLD_STRINGS     = 512; ///< The number of custom strings stored in old savegames.
static const int LEN_OLD_STRINGS     =  32; ///< The number of characters per string.
static const int LEN_OLD_STRINGS_TTO =  24; ///< The number of characters per string in TTO savegames.

/**
 * Remap a string ID from the old format to the new format
 * @param s StringID that requires remapping
 * @return translated ID
 */
StringID RemapOldStringID(StringID s)
{
	switch (s) {
		case 0x0006: return STR_SV_EMPTY;
		case 0x7000: return STR_SV_UNNAMED;
		case 0x70E4: return SPECSTR_COMPANY_NAME_START;
		case 0x70E9: return SPECSTR_COMPANY_NAME_START;
		case 0x8864: return STR_SV_TRAIN_NAME;
		case 0x902B: return STR_SV_ROAD_VEHICLE_NAME;
		case 0x9830: return STR_SV_SHIP_NAME;
		case 0xA02F: return STR_SV_AIRCRAFT_NAME;

		default:
			if (IsInsideMM(s, 0x300F, 0x3030)) {
				return s - 0x300F + STR_SV_STNAME;
			} else {
				return s;
			}
	}
}

/** Location to load the old names to. */
char *_old_name_array = nullptr;

/**
 * Copy and convert old custom names to UTF-8.
 * They were all stored in a 512 by 32 (200 by 24 for TTO) long string array
 * and are now stored with stations, waypoints and other places with names.
 * @param id the StringID of the custom name to clone.
 * @return the clones custom name.
 */
std::string CopyFromOldName(StringID id)
{
	/* Is this name an (old) custom name? */
	if (GetStringTab(id) != TEXT_TAB_OLD_CUSTOM) return std::string();

	if (IsSavegameVersionBefore(SLV_37)) {
		uint offs = _savegame_type == SGT_TTO ? LEN_OLD_STRINGS_TTO * GB(id, 0, 8) : LEN_OLD_STRINGS * GB(id, 0, 9);
		const char *strfrom = &_old_name_array[offs];

		std::ostringstream tmp;
		std::ostreambuf_iterator<char> strto(tmp);
		for (; *strfrom != '\0'; strfrom++) {
			char32_t c = (byte)*strfrom;

			/* Map from non-ISO8859-15 characters to UTF-8. */
			switch (c) {
				case 0xA4: c = 0x20AC; break; // Euro
				case 0xA6: c = 0x0160; break; // S with caron
				case 0xA8: c = 0x0161; break; // s with caron
				case 0xB4: c = 0x017D; break; // Z with caron
				case 0xB8: c = 0x017E; break; // z with caron
				case 0xBC: c = 0x0152; break; // OE ligature
				case 0xBD: c = 0x0153; break; // oe ligature
				case 0xBE: c = 0x0178; break; // Y with diaeresis
				default: break;
			}

			Utf8Encode(strto, c);
		}

		return tmp.str();
	} else {
		/* Name will already be in UTF-8. */
		return std::string(&_old_name_array[LEN_OLD_STRINGS * GB(id, 0, 9)]);
	}
}

/**
 * Free the memory of the old names array.
 * Should be called once the old names have all been converted.
 */
void ResetOldNames()
{
	free(_old_name_array);
	_old_name_array = nullptr;
}

/**
 * Initialize the old names table memory.
 */
void InitializeOldNames()
{
	free(_old_name_array);
	_old_name_array = CallocT<char>(NUM_OLD_STRINGS * LEN_OLD_STRINGS); // 200 * 24 would be enough for TTO savegames
}

struct NAMEChunkHandler : ChunkHandler {
	NAMEChunkHandler() : ChunkHandler('NAME', CH_READONLY) {}

	void Load() const override
	{
		int index;

		while ((index = SlIterateArray()) != -1) {
			if (index >= NUM_OLD_STRINGS) SlErrorCorrupt("Invalid old name index");
			if (SlGetFieldLength() > (uint)LEN_OLD_STRINGS) SlErrorCorrupt("Invalid old name length");

			SlCopy(&_old_name_array[LEN_OLD_STRINGS * index], SlGetFieldLength(), SLE_UINT8);
			/* Make sure the old name is null terminated */
			_old_name_array[LEN_OLD_STRINGS * index + LEN_OLD_STRINGS - 1] = '\0';
		}
	}
};

static const NAMEChunkHandler NAME;
static const ChunkHandlerRef name_chunk_handlers[] = {
	NAME,
};

extern const ChunkHandlerTable _name_chunk_handlers(name_chunk_handlers);
