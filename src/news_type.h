/* $Id$ */

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
#include "date_type.h"
#include "strings_type.h"
#include "sound_type.h"

/**
 * Type of news.
 */
enum NewsType {
	NT_ARRIVAL_COMPANY, ///< Cargo arrived for company
	NT_ARRIVAL_OTHER,   ///< Cargo arrived for competitor
	NT_ACCIDENT,        ///< An accident or disaster has occurred
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
 * News subtypes.
 */
enum NewsSubtype {
	NS_ARRIVAL_COMPANY,  ///< NT_ARRIVAL_COMPANY
	NS_ARRIVAL_OTHER,    ///< NT_ARRIVAL_OTHER
	NS_ACCIDENT,         ///< NT_ACCIDENT
	NS_COMPANY_TROUBLE,  ///< NT_COMPANY_INFO (trouble)
	NS_COMPANY_MERGER,   ///< NT_COMPANY_INFO (merger)
	NS_COMPANY_BANKRUPT, ///< NT_COMPANY_INFO (bankrupt)
	NS_COMPANY_NEW,      ///< NT_COMPANY_INFO (new company)
	NS_INDUSTRY_OPEN,    ///< NT_INDUSTRY_OPEN
	NS_INDUSTRY_CLOSE,   ///< NT_INDUSTRY_CLOSE
	NS_ECONOMY,          ///< NT_ECONOMY
	NS_INDUSTRY_COMPANY, ///< NT_INDUSTRY_COMPANY
	NS_INDUSTRY_OTHER,   ///< NT_INDUSTRY_OTHER
	NS_INDUSTRY_NOBODY,  ///< NT_INDUSTRY_NOBODY
	NS_ADVICE,           ///< NT_ADVICE
	NS_NEW_VEHICLES,     ///< NT_NEW_VEHICLES
	NS_ACCEPTANCE,       ///< NT_ACCEPTANCE
	NS_SUBSIDIES,        ///< NT_SUBSIDIES
	NS_GENERAL,          ///< NT_GENERAL
	NS_END,              ///< end-of-array marker
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
enum NewsReferenceType {
	NR_NONE,      ///< Empty reference
	NR_TILE,      ///< Reference tile.     Scroll to tile when clicking on the news.
	NR_VEHICLE,   ///< Reference vehicle.  Scroll to vehicle when clicking on the news. Delete news when vehicle is deleted.
	NR_STATION,   ///< Reference station.  Scroll to station when clicking on the news. Delete news when station is deleted.
	NR_INDUSTRY,  ///< Reference industry. Scroll to industry when clicking on the news. Delete news when industry is deleted.
	NR_TOWN,      ///< Reference town.     Scroll to town when clicking on the news.
	NR_ENGINE     ///< Reference engine.
};

/**
 * Various OR-able news-item flags.
 * @note #NF_INCOLOUR is set automatically if needed.
 */
enum NewsFlag {
	NFB_INCOLOUR       = 0, ///< News item is shown in colour (otherwise it is shown in black & white).
	NFB_NO_TRANSPARENT = 1, ///< News item disables transparency in the viewport.
	NFB_SHADE          = 2, ///< News item uses shaded colours.

	NF_NONE           = 0,      ///< No flag is set.
	NF_INCOLOUR       = 1 << 0, ///< Bit value for coloured news.
	NF_NO_TRANSPARENT = 1 << 1, ///< Bit value for disabling transparency.
	NF_SHADE          = 1 << 2, ///< Bit value for enabling shading.
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
	NewsDisplay display;        ///< Display mode (off, summary, full)
	const StringID description; ///< Description of the news type in news settings window

	/**
	 * Construct this entry.
	 * @param name The name of the type.
	 * @param age The maximum age for these messages.
	 * @param sound The sound to play.
	 * @param description The description for this type of messages.
	 */
	NewsTypeData(const char *name, byte age, SoundFx sound, StringID description) :
		name(name),
		age(age),
		sound(sound),
		display(ND_FULL),
		description(description)
	{
	}
};

/** Information about a single item of news. */
struct NewsItem {
	NewsItem *prev;              ///< Previous news item
	NewsItem *next;              ///< Next news item
	StringID string_id;          ///< Message text
	Date date;                   ///< Date of the news
	NewsSubtype subtype;         ///< News subtype @see NewsSubtype
	NewsFlag flags;              ///< NewsFlags bits @see NewsFlag

	NewsReferenceType reftype1;  ///< Type of ref1
	NewsReferenceType reftype2;  ///< Type of ref2
	uint32 ref1;                 ///< Reference 1 to some object: Used for a possible viewport, scrolling after clicking on the news, and for deleteing the news when the object is deleted.
	uint32 ref2;                 ///< Reference 2 to some object: Used for scrolling after clicking on the news, and for deleteing the news when the object is deleted.

	void *free_data;             ///< Data to be freed when the news item has reached its end.

	~NewsItem()
	{
		free(this->free_data);
	}

	uint64 params[10]; ///< Parameters for string resolving.
};

/**
 * Data that needs to be stored for company news messages.
 * The problem with company news messages are the custom name
 * of the companies and the fact that the company data is reset,
 * resulting in wrong names and such.
 */
struct CompanyNewsInformation {
	char company_name[64];       ///< The name of the company
	char president_name[64];     ///< The name of the president
	char other_company_name[64]; ///< The name of the company taking over this one

	uint32 face; ///< The face of the president
	byte colour; ///< The colour related to the company

	void FillData(const struct Company *c, const struct Company *other = NULL);
};

#endif /* NEWS_TYPE_H */
