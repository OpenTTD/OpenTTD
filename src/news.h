/* $Id$ */

/** @file news.h */

#ifndef NEWS_H
#define NEWS_H

struct NewsItem {
	StringID string_id; ///< Message text (sometimes also used for storing other info)
	uint16 duration;    ///< Remaining time for showing this news message
	Date date;          ///< Date of the news
	byte flags;         ///< NewsFlags bits @see NewsFlags
	byte display_mode;  ///< Display mode value @see NewsMode
	byte type;          ///< News category @see NewsType
	byte callback;      ///< Call-back function

	TileIndex data_a;   ///< Reference to tile or vehicle
	TileIndex data_b;   ///< Reference to second tile or vehicle

	uint64 params[10];
};

typedef bool ValidationProc ( uint data_a, uint data_b );
typedef void DrawNewsCallbackProc(Window *w);
typedef StringID GetNewsStringCallbackProc(const NewsItem *ni);

/**
 * Macro for creating news flags.
 * @param mode (bits 0 - 7) Display_mode, one of the NewsMode enums (NM_)
 * @param flag (bits 8 - 15) OR-able news flags, any of the NewsFlags enums (NF_)
 * @param type (bits 16-23) News category, one of the NewsType enums (NT_)
 * @param cb (bits 24-31) Call-back function, one of the NewsCallback enums (DNC_) or 0 if no callback
 * @see NewsMode
 * @see NewsFlags
 * @see NewsType
 * @see NewsCallback
 * @see AddNewsItem
 */
#define NEWS_FLAGS(mode, flag, type, cb) ((cb) << 24 | (type) << 16 | (flag) << 8 | (mode))

void AddNewsItem(StringID string, uint32 flags, uint data_a, uint data_b);
void NewsLoop();
void DrawNewsBorder(const Window *w);
void InitNewsItemStructs();

VARDEF NewsItem _statusbar_news_item;

/** Type of news. */
enum NewsType {
	NT_ARRIVAL_PLAYER,  ///< Cargo arrived for player
	NT_ARRIVAL_OTHER,   ///< Cargo arrived for competitor
	NT_ACCIDENT,        ///< An accident or disaster has occurred
	NT_COMPANY_INFO,    ///< Company info (new companies, bankrupcy messages)
	NT_OPENCLOSE,       ///< Opening and closing of industries
	NT_ECONOMY,         ///< Economic changes (recession, industry up/dowm)
	NT_ADVICE,          ///< Bits of news about vehicles of the player
	NT_NEW_VEHICLES,    ///< New vehicle has become available
	NT_ACCEPTANCE,      ///< A type of cargo is (no longer) accepted
	NT_SUBSIDIES,       ///< News about subsidies (announcements, expirations, acceptance)
	NT_GENERAL,         ///< General news (from towns)
	NT_END,             ///< end-of-array marker
};

extern const char *_news_display_name[NT_END];

/**
 * News mode.
 * @see NEWS_FLAGS
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
 * @see NEWS_FLAGS
 */
enum NewsFlags {
	NF_VIEWPORT  = (1 << 1), ///< Does the news message have a viewport? (ingame picture of happening)
	NF_TILE      = (1 << 2), ///< When clicked on the news message scroll to a given tile? Tile is in data_a/data_b
	NF_VEHICLE   = (1 << 3), ///< When clicked on the message scroll to the vehicle? VehicleID is in data_a
	NF_FORCE_BIG = (1 << 4), ///< Force the appearance of a news message if it has already been shown (internal)
	NF_INCOLOR   = (1 << 5), ///< Show the newsmessage in colour, otherwise it defaults to black & white
};

/** Special news items */
enum NewsCallback {
	DNC_VEHICLEAVAIL  = 0, ///< Show new vehicle available message. StringID is EngineID
	DNC_BANKRUPCY     = 1, ///< Show bankrupcy message. StringID is PlayerID (0-3) and NewsBankrupcy (4-7)
};

/** Kinds of bankrupcy */
enum NewsBankrupcy {
	NB_BTROUBLE    = (1 << 4), ///< Company is in trouble (warning)
	NB_BMERGER     = (2 << 4), ///< Company has been bought by another company
	NB_BBANKRUPT   = (3 << 4), ///< Company has gone bankrupt
	NB_BNEWCOMPANY = (4 << 4), ///< A new company has been started
};

/**
 * Delete a news item type about a vehicle
 * if the news item type is INVALID_STRING_ID all news about the vehicle get
 * deleted
 */
void DeleteVehicleNews(VehicleID, StringID news);

#endif /* NEWS_H */
