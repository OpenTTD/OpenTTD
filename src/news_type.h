/* $Id$ */

/** @file news_type.h Types related to news. */

#ifndef NEWS_TYPE_H
#define NEWS_TYPE_H

#include "window_type.h"
#include "date_type.h"
#include "strings_type.h"
#include "sound_type.h"

/**
 * Type of news.
 */
enum NewsType {
	NT_ARRIVAL_PLAYER,  ///< Cargo arrived for player
	NT_ARRIVAL_OTHER,   ///< Cargo arrived for competitor
	NT_ACCIDENT,        ///< An accident or disaster has occurred
	NT_COMPANY_INFO,    ///< Company info (new companies, bankrupcy messages)
	NT_OPENCLOSE,       ///< Opening and closing of industries
	NT_ECONOMY,         ///< Economic changes (recession, industry up/dowm)
	NT_INDUSTRY_PLAYER, ///< Production changes of industry serviced by local player
	NT_INDUSTRY_OTHER,  ///< Production changes of industry serviced by competitor(s)
	NT_INDUSTRY_NOBODY, ///< Other industry production changes
	NT_ADVICE,          ///< Bits of news about vehicles of the player
	NT_NEW_VEHICLES,    ///< New vehicle has become available
	NT_ACCEPTANCE,      ///< A type of cargo is (no longer) accepted
	NT_SUBSIDIES,       ///< News about subsidies (announcements, expirations, acceptance)
	NT_GENERAL,         ///< General news (from towns)
	NT_END,             ///< end-of-array marker
};

/**
 * News mode.
 */
enum NewsMode {
	NM_SMALL    = 0, ///< Show only a small popup informing us about vehicle age for example
	NM_NORMAL   = 1, ///< Show a simple news message (height 170 pixels)
	NM_THIN     = 2, ///< Show a simple news message (height 130 pixels)
	NM_CALLBACK = 3, ///< Do some special processing before displaying news message. Which callback to call is in NewsCallback
};

/**
 * Various OR-able news-item flags.
 * note: NF_INCOLOR is set automatically if needed
 */
enum NewsFlag {
	NF_NONE      = 0,        ///< No flag is set.
	NF_VIEWPORT  = (1 << 1), ///< Does the news message have a viewport? (ingame picture of happening)
	NF_TILE      = (1 << 2), ///< When clicked on the news message scroll to a given tile? Tile is in data_a/data_b
	NF_VEHICLE   = (1 << 3), ///< When clicked on the message scroll to the vehicle? VehicleID is in data_a
	NF_FORCE_BIG = (1 << 4), ///< Force the appearance of a news message if it has already been shown (internal)
	NF_INCOLOR   = (1 << 5), ///< Show the newsmessage in colour, otherwise it defaults to black & white
};
DECLARE_ENUM_AS_BIT_SET(NewsFlag);


/**
 * Special news items
 */
enum NewsCallback {
	DNC_VEHICLEAVAIL  = 0,    ///< Show new vehicle available message. StringID is EngineID
	DNC_BANKRUPCY     = 1,    ///< Show bankrupcy message. StringID is PlayerID (0-3) and NewsBankrupcy (4-7)
	DNC_NONE          = 0xFF, ///< No news callback.
};

/**
 * Kinds of bankrupcy
 * @note These flags are or'd with player index
 */
enum NewsBankrupcy {
	NB_BTROUBLE    = (1 << 4), ///< Company is in trouble (warning)
	NB_BMERGER     = (2 << 4), ///< Company has been bought by another company
	NB_BBANKRUPT   = (3 << 4), ///< Company has gone bankrupt
	NB_BNEWCOMPANY = (4 << 4), ///< A new company has been started
};

/**
 * Per-NewsType data
 */
struct NewsTypeData {
	const char *const name; ///< Name
	const byte age;         ///< Maximum age of news items (in days)
	const SoundFx sound;    ///< Sound
};

struct NewsItem {
	StringID string_id;    ///< Message text (sometimes also used for storing other info)
	uint16 duration;       ///< Remaining time for showing this news message
	Date date;             ///< Date of the news
	NewsFlag flags;        ///< NewsFlags bits @see NewsFlag
	NewsMode display_mode; ///< Display mode value @see NewsMode
	NewsType type;         ///< News category @see NewsType
	NewsCallback callback; ///< Call-back function

	uint data_a;           ///< Reference to tile or vehicle
	uint data_b;           ///< Reference to second tile or vehicle

	uint64 params[10];
};

typedef bool ValidationProc(uint data_a, uint data_b);
typedef void DrawNewsCallbackProc(Window *w, const NewsItem *ni);
typedef StringID GetNewsStringCallbackProc(const NewsItem *ni);

#endif /* NEWS_TYPE_H */
