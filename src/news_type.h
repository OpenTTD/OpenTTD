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
#include "engine_type.h"
#include "industry_type.h"
#include "gfx_type.h"
#include "sound_type.h"
#include "station_type.h"
#include "strings_type.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_economy.h"
#include "town_type.h"
#include "vehicle_type.h"

/**
 * Type of news.
 */
enum class NewsType : uint8_t {
	ArrivalCompany, ///< First vehicle arrived for company
	ArrivalOther, ///< First vehicle arrived for competitor
	Accident, ///< An accident or disaster has occurred
	AccidentOther, ///< An accident or disaster has occurred
	CompanyInfo, ///< Company info (new companies, bankruptcy messages)
	IndustryOpen, ///< Opening of industries
	IndustryClose, ///< Closing of industries
	Economy, ///< Economic changes (recession, industry up/dowm)
	IndustryCompany, ///< Production changes of industry serviced by local company
	IndustryOther, ///< Production changes of industry serviced by competitor(s)
	IndustryNobody, ///< Other industry production changes
	Advice, ///< Bits of news about vehicles of the company
	NewVehicles, ///< New vehicle has become available
	Acceptance, ///< A type of cargo is (no longer) accepted
	Subsidies, ///< News about subsidies (announcements, expirations, acceptance)
	General, ///< General news (from towns)

	End, ///< end-of-array marker
};

/** Sub type of the #NewsType::Advice to be able to remove specific news items. */
enum class AdviceType : uint8_t {
	AircraftDestinationTooFar, ///< Next (order) destination is too far for the aircraft type.
	AutorenewFailed, ///< Autorenew or autoreplace failed.
	Order, ///< Something wrong with the order, e.g. invalid or duplicate entries, too few entries
	RefitFailed, ///< The refit order failed to execute.
	TrainStuck, ///< The train got stuck and needs to be unstuck manually.
	VehicleLost, ///< The vehicle has become lost.
	VehicleOld, ///< The vehicle is starting to get old.
	VehicleUnprofitable, ///< The vehicle is costing you money.
	VehicleWaiting, ///< The vehicle is waiting in the depot.

	Invalid
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
using NewsReference = std::variant<std::monostate, TileIndex, VehicleID, StationID, IndustryID, TownID, EngineID>;

/** News Window Styles. */
enum class NewsStyle : uint8_t {
	Thin, ///< Thin news item. (Newspaper with headline and viewport)
	Small, ///< Small news item. (Information window with text and viewport)
	Normal, ///< Normal news item. (Newspaper with text only)
	Vehicle, ///< Vehicle news item. (new engine available)
	Company, ///< Company news item. (Newspaper with face)
};

/**
 * Various OR-able news-item flags.
 * @note #NewsFlag::InColour is set automatically if needed.
 */
enum class NewsFlag : uint8_t {
	InColour, ///< News item is shown in colour (otherwise it is shown in black & white).
	NoTransparency, ///< News item disables transparency in the viewport.
	Shaded, ///< News item uses shaded colours.
	VehicleParam0, ///< String param 0 contains a vehicle ID. (special autoreplace behaviour)
};
using NewsFlags = EnumBitSet<NewsFlag, uint8_t>;

/**
 * News display options
 */
enum class NewsDisplay : uint8_t {
	Off, ///< Only show a reminder in the status bar
	Summary, ///< Show ticker
	Full, ///< Show newspaper
};

/**
 * Per-NewsType data
 */
struct NewsTypeData {
	const char * const name;    ///< Name
	const uint8_t age;             ///< Maximum age of news items (in days)
	const SoundFx sound;        ///< Sound

	/**
	 * Construct this entry.
	 * @param name The name of the type.
	 * @param age The maximum age for these messages.
	 * @param sound The sound to play.
	 */
	NewsTypeData(const char *name, uint8_t age, SoundFx sound) :
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
	EncodedString headline; ///< Headline of news.
	TimerGameCalendar::Date date; ///< Calendar date to show for the news
	TimerGameEconomy::Date economy_date; ///< Economy date of the news item, never shown but used to calculate age
	NewsType type;                ///< Type of the news
	AdviceType advice_type; ///< The type of advice, to be able to remove specific advices later on.
	NewsStyle style; /// Window style for the news.
	NewsFlags flags;               ///< NewsFlags bits @see NewsFlag

	NewsReference ref1; ///< Reference 1 to some object: Used for a possible viewport, scrolling after clicking on the news, and for deleting the news when the object is deleted.
	NewsReference ref2; ///< Reference 2 to some object: Used for scrolling after clicking on the news, and for deleting the news when the object is deleted.

	std::unique_ptr<NewsAllocatedData> data; ///< Custom data for the news item that will be deallocated (deleted) when the news item has reached its end.

	NewsItem(EncodedString &&headline, NewsType type, NewsStyle style, NewsFlags flags, NewsReference ref1, NewsReference ref2, std::unique_ptr<NewsAllocatedData> &&data, AdviceType advice_type);

	std::string GetStatusText() const;
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

	StringID title;
	uint32_t face; ///< The face of the president
	Colours colour; ///< The colour related to the company

	CompanyNewsInformation(StringID title, const struct Company *c, const struct Company *other = nullptr);
};

using NewsContainer = std::list<NewsItem>; ///< Container type for storing news items.
using NewsIterator = NewsContainer::const_iterator; ///< Iterator type for news items.

#endif /* NEWS_TYPE_H */
