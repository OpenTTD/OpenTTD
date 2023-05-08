/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file news_type.h Types related to news. */

#ifndef NEWS_TYPE_H
#define NEWS_TYPE_H

#include "core/enum_type.hpp"
#include "timer/timer_game_calendar.h"
#include "strings_type.h"
#include "sound_type.h"

/**
 * Type of news.
 */
enum NewsType : byte {
	NT_ARRIVAL_COMPANY, ///< First vehicle arrived for company
	NT_ARRIVAL_OTHER,   ///< First vehicle arrived for competitor
	NT_ACCIDENT,        ///< An accident or disaster has occurred
	NT_ACCIDENT_OTHER,  ///< An accident or disaster has occurred
	NT_COMPANY_INFO,    ///< Company info (new companies, bankruptcy messages)
	NT_INDUSTRY_OPEN,   ///< Opening of industries
	NT_INDUSTRY_CLOSE,  ///< Closing of industries
	NT_ECONOMY,         ///< Economic changes (recession, industry up/dowm)
	NT_INDUSTRY_COMPANY,///< Production changes of industry serviced by local company
	NT_INDUSTRY_OTHER,  ///< Production changes of industry serviced by competitor(s)
	NT_INDUSTRY_NOBODY, ///< Other industry production changes
	NT_ADVICE,          ///< Bits of news about vehicles of the company
	NT_NEW_VEHICLES,    ///< New vehicle has become available
	NT_ACCEPTANCE,      ///< A type of cargo is (no longer) accepted
	NT_SUBSIDIES,       ///< News about subsidies (announcements, expirations, acceptance)
	NT_GENERAL,         ///< General news (from towns)
	NT_END,             ///< end-of-array marker
};

/**
 * References to objects in news.
 *
 * @warning
 * Be careful!
 * Vehicles are a special case, as news are kept when vehicles are autoreplaced/renewed.
 * You have to make sure, #ChangeVehicleNews catches the DParams of your message.
 * This is NOT ensured by the references.
 */
enum NewsReferenceType : byte {
	NR_NONE,      ///< Empty reference
	NR_TILE,      ///< Reference tile.     Scroll to tile when clicking on the news.
	NR_VEHICLE,   ///< Reference vehicle.  Scroll to vehicle when clicking on the news. Delete news when vehicle is deleted.
	NR_STATION,   ///< Reference station.  Scroll to station when clicking on the news. Delete news when station is deleted.
	NR_INDUSTRY,  ///< Reference industry. Scroll to industry when clicking on the news. Delete news when industry is deleted.
	NR_TOWN,      ///< Reference town.     Scroll to town when clicking on the news.
	NR_ENGINE,    ///< Reference engine.
};

/**
 * Various OR-able news-item flags.
 * @note #NF_INCOLOUR is set automatically if needed.
 */
enum NewsFlag {
	NFB_INCOLOUR       = 0,                      ///< News item is shown in colour (otherwise it is shown in black & white).
	NFB_NO_TRANSPARENT = 1,                      ///< News item disables transparency in the viewport.
	NFB_SHADE          = 2,                      ///< News item uses shaded colours.
	NFB_WINDOW_LAYOUT  = 3,                      ///< First bit for window layout.
	NFB_WINDOW_LAYOUT_COUNT = 3,                 ///< Number of bits for window layout.
	NFB_VEHICLE_PARAM0 = 6,                      ///< String param 0 contains a vehicle ID. (special autoreplace behaviour)

	NF_INCOLOUR       = 1 << NFB_INCOLOUR,       ///< Bit value for coloured news.
	NF_NO_TRANSPARENT = 1 << NFB_NO_TRANSPARENT, ///< Bit value for disabling transparency.
	NF_SHADE          = 1 << NFB_SHADE,          ///< Bit value for enabling shading.
	NF_VEHICLE_PARAM0 = 1 << NFB_VEHICLE_PARAM0, ///< Bit value for specifying that string param 0 contains a vehicle ID. (special autoreplace behaviour)

	NF_THIN           = 0 << NFB_WINDOW_LAYOUT,  ///< Thin news item. (Newspaper with headline and viewport)
	NF_SMALL          = 1 << NFB_WINDOW_LAYOUT,  ///< Small news item. (Information window with text and viewport)
	NF_NORMAL         = 2 << NFB_WINDOW_LAYOUT,  ///< Normal news item. (Newspaper with text only)
	NF_VEHICLE        = 3 << NFB_WINDOW_LAYOUT,  ///< Vehicle news item. (new engine available)
	NF_COMPANY        = 4 << NFB_WINDOW_LAYOUT,  ///< Company news item. (Newspaper with face)
};
DECLARE_ENUM_AS_BIT_SET(NewsFlag)


/**
 * News display options
 */
enum NewsDisplay {
	ND_OFF,        ///< Only show a reminder in the status bar
	ND_SUMMARY,    ///< Show ticker
	ND_FULL,       ///< Show newspaper
};

/**
 * Per-NewsType data
 */
struct NewsTypeData {
	const char * const name;    ///< Name
	const byte age;             ///< Maximum age of news items (in days)
	const SoundFx sound;        ///< Sound

	/**
	 * Construct this entry.
	 * @param name The name of the type.
	 * @param age The maximum age for these messages.
	 * @param sound The sound to play.
	 */
	NewsTypeData(const char *name, byte age, SoundFx sound) :
		name(name),
		age(age),
		sound(sound)
	{
	}

	NewsDisplay GetDisplay() const;
};

/** Container for any custom data that must be deleted after the news item has reached end-of-life. */
struct NewsAllocatedData {
	virtual ~NewsAllocatedData() = default;
};


/** Information about a single item of news. */
struct NewsItem {
	NewsItem *prev;               ///< Previous news item
	NewsItem *next;               ///< Next news item
	StringID string_id;           ///< Message text
	TimerGameCalendar::Date date; ///< Date of the news
	NewsType type;                ///< Type of the news
	NewsFlag flags;               ///< NewsFlags bits @see NewsFlag

	NewsReferenceType reftype1;   ///< Type of ref1
	NewsReferenceType reftype2;   ///< Type of ref2
	uint32_t ref1;                  ///< Reference 1 to some object: Used for a possible viewport, scrolling after clicking on the news, and for deleting the news when the object is deleted.
	uint32_t ref2;                  ///< Reference 2 to some object: Used for scrolling after clicking on the news, and for deleting the news when the object is deleted.

	std::unique_ptr<const NewsAllocatedData> data; ///< Custom data for the news item that will be deallocated (deleted) when the news item has reached its end.

	std::vector<StringParameterBackup> params; ///< Parameters for string resolving.

	NewsItem(StringID string_id, NewsType type, NewsFlag flags, NewsReferenceType reftype1, uint32_t ref1, NewsReferenceType reftype2, uint32_t ref2, const NewsAllocatedData *data);
};

/** Container for a single string to be passed as NewsAllocatedData. */
struct NewsStringData : NewsAllocatedData {
	std::string string; ///< The string to retain.
	NewsStringData(const std::string &str) : string(str) {}
};

/**
 * Data that needs to be stored for company news messages.
 * The problem with company news messages are the custom name
 * of the companies and the fact that the company data is reset,
 * resulting in wrong names and such.
 */
struct CompanyNewsInformation : NewsAllocatedData {
	std::string company_name;       ///< The name of the company
	std::string president_name;     ///< The name of the president
	std::string other_company_name; ///< The name of the company taking over this one

	uint32_t face; ///< The face of the president
	byte colour; ///< The colour related to the company

	CompanyNewsInformation(const struct Company *c, const struct Company *other = nullptr);
};

#endif /* NEWS_TYPE_H */
