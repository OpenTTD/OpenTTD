/* $Id$ */

/** @file news_type.h Types related to news. */

#ifndef NEWS_TYPE_H
#define NEWS_TYPE_H

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
	NT_COMPANY_INFO,    ///< Company info (new companies, bankrupcy messages)
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
	NS_ACCIDENT_TILE,    ///< NT_ACCIDENT (tile)
	NS_ACCIDENT_VEHICLE, ///< NT_ACCIDENT (vehicle)
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
 * News mode.
 */
enum NewsMode {
	NM_SMALL    = 0, ///< Show only a small popup informing us about vehicle age for example
	NM_NORMAL   = 1, ///< Show a simple news message (height 170 pixels)
	NM_THIN     = 2, ///< Show a simple news message (height 130 pixels)
};

/**
 * Various OR-able news-item flags.
 * note: NF_INCOLOUR is set automatically if needed
 */
enum NewsFlag {
	NF_NONE      = 0,        ///< No flag is set.
	NF_VIEWPORT  = (1 << 1), ///< Does the news message have a viewport? (ingame picture of happening)
	NF_TILE      = (1 << 2), ///< When clicked on the news message scroll to a given tile? Tile is in data_a
	NF_VEHICLE   = (1 << 3), ///< When clicked on the message scroll to the vehicle? VehicleID is in data_a
	NF_INCOLOUR  = (1 << 5), ///< Show the newsmessage in colour, otherwise it defaults to black & white
	NF_TILE2     = (1 << 6), ///< There is a second tile to scroll to; tile is in data_b
};
DECLARE_ENUM_AS_BIT_SET(NewsFlag);


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
	const char * const name; ///< Name
	const byte age;         ///< Maximum age of news items (in days)
	const SoundFx sound;    ///< Sound
	NewsDisplay display;    ///< Display mode (off, summary, full)
};

struct NewsItem {
	NewsItem *prev;        ///< Previous news item
	NewsItem *next;        ///< Next news item
	StringID string_id;    ///< Message text
	Date date;             ///< Date of the news
	NewsSubtype subtype;   ///< News subtype @see NewsSubtype
	NewsFlag flags;        ///< NewsFlags bits @see NewsFlag

	uint data_a;           ///< Custom data 1 (usually tile or vehicle)
	uint data_b;           ///< Custom data 2

	void *free_data;       ///< Data to be freed when the news item has reached it's end.

	uint64 params[10];
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
