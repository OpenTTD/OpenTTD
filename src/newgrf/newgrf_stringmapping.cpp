/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_stringmapping.cpp NewGRF string mapping implementation. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf.h"
#include "../newgrf_text.h"
#include "../newgrf_text_type.h"
#include "../strings_type.h"
#include "newgrf_internal.h"
#include "newgrf_stringmapping.h"

#include "table/strings.h"

#include "../safeguards.h"

/**
 * Information for mapping static StringIDs.
 */
struct StringIDMapping {
	uint32_t grfid; ///< Source NewGRF.
	GRFStringID source; ///< Source grf-local GRFStringID.
	std::function<void(StringID)> func; ///< Function for mapping result.

	StringIDMapping(uint32_t grfid, GRFStringID source, std::function<void(StringID)> &&func) : grfid(grfid), source(source), func(std::move(func)) { }
};

/** Strings to be mapped during load. */
static std::vector<StringIDMapping> _string_to_grf_mapping;

/**
 * Record a static StringID for getting translated later.
 * @param source Source grf-local GRFStringID.
 * @param func Function to call to set the mapping result.
 */
void AddStringForMapping(GRFStringID source, std::function<void(StringID)> &&func)
{
	func(STR_UNDEFINED);
	_string_to_grf_mapping.emplace_back(_cur_gps.grffile->grfid, source, std::move(func));
}

/**
 * Record a static StringID for getting translated later.
 * @param source Source grf-local GRFStringID.
 * @param target Destination for the mapping result.
 */
void AddStringForMapping(GRFStringID source, StringID *target)
{
	AddStringForMapping(source, [target](StringID str) { *target = str; });
}

/**
 * Perform a mapping from TTDPatch's string IDs to OpenTTD's
 * string IDs, but only for the ones we are aware off; the rest
 * like likely unused and will show a warning.
 * @param str Grf-local GRFStringID to convert.
 * @return the converted string ID
 */
static StringID TTDPStringIDToOTTDStringIDMapping(GRFStringID str)
{
	/* StringID table for TextIDs 0x4E->0x6D */
	static const StringID units_volume[] = {
		STR_ITEMS,      STR_PASSENGERS, STR_TONS,       STR_BAGS,
		STR_LITERS,     STR_ITEMS,      STR_CRATES,     STR_TONS,
		STR_TONS,       STR_TONS,       STR_TONS,       STR_BAGS,
		STR_TONS,       STR_TONS,       STR_TONS,       STR_BAGS,
		STR_TONS,       STR_TONS,       STR_BAGS,       STR_LITERS,
		STR_TONS,       STR_LITERS,     STR_TONS,       STR_ITEMS,
		STR_BAGS,       STR_LITERS,     STR_TONS,       STR_ITEMS,
		STR_TONS,       STR_ITEMS,      STR_LITERS,     STR_ITEMS
	};

	/* A string straight from a NewGRF; this was already translated by MapGRFStringID(). */
	assert(!IsInsideMM(str.base(), 0xD000, 0xD7FF));

#define TEXTID_TO_STRINGID(begin, end, stringid, stringend) \
	static_assert(stringend - stringid == end - begin); \
	if (str.base() >= begin && str.base() <= end) return StringID{str.base() + (stringid - begin)}

	/* We have some changes in our cargo strings, resulting in some missing. */
	TEXTID_TO_STRINGID(0x000E, 0x002D, STR_CARGO_PLURAL_NOTHING,                      STR_CARGO_PLURAL_FIZZY_DRINKS);
	TEXTID_TO_STRINGID(0x002E, 0x004D, STR_CARGO_SINGULAR_NOTHING,                    STR_CARGO_SINGULAR_FIZZY_DRINK);
	if (str.base() >= 0x004E && str.base() <= 0x006D) return units_volume[str.base() - 0x004E];
	TEXTID_TO_STRINGID(0x006E, 0x008D, STR_QUANTITY_NOTHING,                          STR_QUANTITY_FIZZY_DRINKS);
	TEXTID_TO_STRINGID(0x008E, 0x00AD, STR_ABBREV_NOTHING,                            STR_ABBREV_FIZZY_DRINKS);
	TEXTID_TO_STRINGID(0x00D1, 0x00E0, STR_COLOUR_DARK_BLUE,                          STR_COLOUR_WHITE);

	/* Map building names according to our lang file changes. There are several
	 * ranges of house ids, all of which need to be remapped to allow newgrfs
	 * to use original house names. */
	TEXTID_TO_STRINGID(0x200F, 0x201F, STR_TOWN_BUILDING_NAME_TALL_OFFICE_BLOCK_1,    STR_TOWN_BUILDING_NAME_OLD_HOUSES_1);
	TEXTID_TO_STRINGID(0x2036, 0x2041, STR_TOWN_BUILDING_NAME_COTTAGES_1,             STR_TOWN_BUILDING_NAME_SHOPPING_MALL_1);
	TEXTID_TO_STRINGID(0x2059, 0x205C, STR_TOWN_BUILDING_NAME_IGLOO_1,                STR_TOWN_BUILDING_NAME_PIGGY_BANK_1);

	/* Same thing for industries */
	TEXTID_TO_STRINGID(0x4802, 0x4826, STR_INDUSTRY_NAME_COAL_MINE,                   STR_INDUSTRY_NAME_SUGAR_MINE);
	TEXTID_TO_STRINGID(0x482D, 0x482E, STR_NEWS_INDUSTRY_CONSTRUCTION,                STR_NEWS_INDUSTRY_PLANTED);
	TEXTID_TO_STRINGID(0x4832, 0x4834, STR_NEWS_INDUSTRY_CLOSURE_GENERAL,             STR_NEWS_INDUSTRY_CLOSURE_LACK_OF_TREES);
	TEXTID_TO_STRINGID(0x4835, 0x4838, STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL, STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_FARM);
	TEXTID_TO_STRINGID(0x4839, 0x483A, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_FARM);

	switch (str.base()) {
		case 0x4830: return STR_ERROR_CAN_T_CONSTRUCT_THIS_INDUSTRY;
		case 0x4831: return STR_ERROR_FOREST_CAN_ONLY_BE_PLANTED;
		case 0x483B: return STR_ERROR_CAN_ONLY_BE_POSITIONED;
	}
#undef TEXTID_TO_STRINGID

	if (str.base() == 0) return STR_EMPTY;

	Debug(grf, 0, "Unknown StringID 0x{:04X} remapped to STR_EMPTY. Please open a Feature Request if you need it", str);

	return STR_EMPTY;
}

/**
 * Used when setting an object's property to map to the GRF's strings
 * while taking in consideration the "drift" between TTDPatch string system and OpenTTD's one
 * @param grfid Id of the grf file.
 * @param str GRF-local GRFStringID that we want to have the equivalent in OpenTTD.
 * @return The properly adjusted StringID.
 */
StringID MapGRFStringID(uint32_t grfid, GRFStringID str)
{
	if (IsInsideMM(str.base(), 0xD800, 0x10000)) {
		/* General text provided by NewGRF.
		 * In the specs this is called the 0xDCxx range (misc persistent texts),
		 * but we meanwhile extended the range to 0xD800-0xFFFF.
		 * Note: We are not involved in the "persistent" business, since we do not store
		 * any NewGRF strings in savegames. */
		return GetGRFStringID(grfid, str);
	} else if (IsInsideMM(str.base(), 0xD000, 0xD800)) {
		/* Callback text provided by NewGRF.
		 * In the specs this is called the 0xD0xx range (misc graphics texts).
		 * These texts can be returned by various callbacks.
		 *
		 * Due to how TTDP implements the GRF-local- to global-textid translation
		 * texts included via 0x80 or 0x81 control codes have to add 0x400 to the textid.
		 * We do not care about that difference and just mask out the 0x400 bit.
		 */
		str = GRFStringID(str.base() & ~0x400);
		return GetGRFStringID(grfid, str);
	} else {
		/* The NewGRF wants to include/reference an original TTD string.
		 * Try our best to find an equivalent one. */
		return TTDPStringIDToOTTDStringIDMapping(str);
	}
}

/**
 * Finalise all string mappings.
 */
void FinaliseStringMapping()
{
	for (StringIDMapping &it : _string_to_grf_mapping) {
		it.func(MapGRFStringID(it.grfid, it.source));
	}
	_string_to_grf_mapping.clear();
}
