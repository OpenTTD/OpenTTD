/* $Id$ */

#ifndef NEWS_H
#define NEWS_H

struct NewsItem {
	StringID string_id;
	uint16 duration;
	Date date;
	byte flags;
	byte display_mode;
	byte type;
	byte callback;

	TileIndex data_a;
	TileIndex data_b;

	uint32 params[10];
};

typedef bool ValidationProc ( uint data_a, uint data_b );
typedef void DrawNewsCallbackProc(Window *w);
typedef StringID GetNewsStringCallbackProc(const NewsItem *ni);

#define NEWS_FLAGS(mode,flag,type,cb) ((cb)<<24 | (type)<<16 | (flag)<<8 | (mode))
void AddNewsItem(StringID string, uint32 flags, uint data_a, uint data_b);
void NewsLoop(void);
void DrawNewsBorder(const Window *w);
void InitNewsItemStructs(void);

VARDEF NewsItem _statusbar_news_item;

enum NewsType {
	NT_ARRIVAL_PLAYER = 0,
	NT_ARRIVAL_OTHER  = 1,
	NT_ACCIDENT       = 2,
	NT_COMPANY_INFO   = 3,
	NT_ECONOMY        = 4,
	NT_ADVICE         = 5,
	NT_NEW_VEHICLES   = 6,
	NT_ACCEPTANCE     = 7,
	NT_SUBSIDIES      = 8,
	NT_GENERAL        = 9,
};

enum NewsMode {
	NM_SMALL    = 0, ///< Show only a small popup informing us about vehicle age for example
	NM_NORMAL   = 1, ///< Show a simple news message (height 170 pixels)
	NM_THIN     = 2, ///< Show a simple news message (height 130 pixels)
	NM_CALLBACK = 3, ///< Do some special processing before displaying news message. Which callback to call is in NewsCallback
};

enum NewsFlags {
	NF_VIEWPORT  = (1 << 1), ///< Does the news message have a viewport? (ingame picture of happening)
	NF_TILE      = (1 << 2), ///< When clicked on the news message scroll to a given tile? Tile is in data_a/data_b
	NF_VEHICLE   = (1 << 3), ///< When clicked on the message scroll to the vehicle? VehicleID is in data_a
	NF_FORCE_BIG = (1 << 4), ///< Force the appearance of a news message if it has already been shown (internal)
	NF_NOEXPIRE  = (1 << 5), ///< Some flag that I think is already deprecated
	NF_INCOLOR   = (1 << 6), ///< Show the newsmessage in colour, otherwise it defaults to black & white
};

enum NewsCallback {
	DNC_TRAINAVAIL    = 0, ///< Show new train available message. StringID is EngineID
	DNC_ROADAVAIL     = 1, ///< Show new road vehicle available message. StringID is EngineID
	DNC_SHIPAVAIL     = 2, ///< Show new ship available message. StringID is EngineID
	DNC_AIRCRAFTAVAIL = 3, ///< Show new aircraft available message. StringID is EngineID
	DNC_BANKRUPCY     = 4, ///< Show bankrupcy message. StringID is PlayerID (0-3) and NewsBankrupcy (4-7)
};

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
