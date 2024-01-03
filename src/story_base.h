/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file story_base.h %StoryPage base class. */

#ifndef STORY_BASE_H
#define STORY_BASE_H

#include "company_type.h"
#include "story_type.h"
#include "timer/timer_game_calendar.h"
#include "gfx_type.h"
#include "vehicle_type.h"
#include "core/pool_type.hpp"

typedef Pool<StoryPageElement, StoryPageElementID, 64, 64000> StoryPageElementPool;
typedef Pool<StoryPage, StoryPageID, 64, 64000> StoryPagePool;
extern StoryPageElementPool _story_page_element_pool;
extern StoryPagePool _story_page_pool;
extern uint32_t _story_page_element_next_sort_value;
extern uint32_t _story_page_next_sort_value;

/*
 * Each story page element is one of these types.
 */
enum StoryPageElementType : byte {
	SPET_TEXT = 0,       ///< A text element.
	SPET_LOCATION,       ///< An element that references a tile along with a one-line text.
	SPET_GOAL,           ///< An element that references a goal.
	SPET_BUTTON_PUSH,    ///< A push button that triggers an immediate event.
	SPET_BUTTON_TILE,    ///< A button that allows the player to select a tile, and triggers an event with the tile.
	SPET_BUTTON_VEHICLE, ///< A button that allows the player to select a vehicle, and triggers an event wih the vehicle.
	SPET_END,
	INVALID_SPET = 0xFF,
};

/** Flags available for buttons */
enum StoryPageButtonFlags : byte {
	SPBF_NONE        = 0,
	SPBF_FLOAT_LEFT  = 1 << 0,
	SPBF_FLOAT_RIGHT = 1 << 1,
};
DECLARE_ENUM_AS_BIT_SET(StoryPageButtonFlags)

/** Mouse cursors usable by story page buttons. */
enum StoryPageButtonCursor : byte {
	SPBC_MOUSE,
	SPBC_ZZZ,
	SPBC_BUOY,
	SPBC_QUERY,
	SPBC_HQ,
	SPBC_SHIP_DEPOT,
	SPBC_SIGN,
	SPBC_TREE,
	SPBC_BUY_LAND,
	SPBC_LEVEL_LAND,
	SPBC_TOWN,
	SPBC_INDUSTRY,
	SPBC_ROCKY_AREA,
	SPBC_DESERT,
	SPBC_TRANSMITTER,
	SPBC_AIRPORT,
	SPBC_DOCK,
	SPBC_CANAL,
	SPBC_LOCK,
	SPBC_RIVER,
	SPBC_AQUEDUCT,
	SPBC_BRIDGE,
	SPBC_RAIL_STATION,
	SPBC_TUNNEL_RAIL,
	SPBC_TUNNEL_ELRAIL,
	SPBC_TUNNEL_MONO,
	SPBC_TUNNEL_MAGLEV,
	SPBC_AUTORAIL,
	SPBC_AUTOELRAIL,
	SPBC_AUTOMONO,
	SPBC_AUTOMAGLEV,
	SPBC_WAYPOINT,
	SPBC_RAIL_DEPOT,
	SPBC_ELRAIL_DEPOT,
	SPBC_MONO_DEPOT,
	SPBC_MAGLEV_DEPOT,
	SPBC_CONVERT_RAIL,
	SPBC_CONVERT_ELRAIL,
	SPBC_CONVERT_MONO,
	SPBC_CONVERT_MAGLEV,
	SPBC_AUTOROAD,
	SPBC_AUTOTRAM,
	SPBC_ROAD_DEPOT,
	SPBC_BUS_STATION,
	SPBC_TRUCK_STATION,
	SPBC_ROAD_TUNNEL,
	SPBC_CLONE_TRAIN,
	SPBC_CLONE_ROADVEH,
	SPBC_CLONE_SHIP,
	SPBC_CLONE_AIRPLANE,
	SPBC_DEMOLISH,
	SPBC_LOWERLAND,
	SPBC_RAISELAND,
	SPBC_PICKSTATION,
	SPBC_BUILDSIGNALS,
	SPBC_END,
	INVALID_SPBC = 0xFF
};

/**
 * Checks if a StoryPageButtonCursor value is valid.
 *
 * @param wc The value to check
 * @return true if the given value is a valid StoryPageButtonCursor.
 */
static inline bool IsValidStoryPageButtonCursor(StoryPageButtonCursor cursor)
{
	return cursor < SPBC_END;
}

/** Helper to construct packed "id" values for button-type StoryPageElement */
struct StoryPageButtonData {
	uint32_t referenced_id;

	void SetColour(Colours button_colour);
	void SetFlags(StoryPageButtonFlags flags);
	void SetCursor(StoryPageButtonCursor cursor);
	void SetVehicleType(VehicleType vehtype);
	Colours GetColour() const;
	StoryPageButtonFlags GetFlags() const;
	StoryPageButtonCursor GetCursor() const;
	VehicleType GetVehicleType() const;
	bool ValidateColour() const;
	bool ValidateFlags() const;
	bool ValidateCursor() const;
	bool ValidateVehicleType() const;
};

/**
 * Struct about story page elements.
 * Each StoryPage is composed of one or more page elements that provide
 * page content. Each element only contain one type of content.
 **/
struct StoryPageElement : StoryPageElementPool::PoolItem<&_story_page_element_pool> {
	uint32_t sort_value;         ///< A number that increases for every created story page element. Used for sorting. The id of a story page element is the pool index.
	StoryPageID page;          ///< Id of the page which the page element belongs to
	StoryPageElementType type; ///< Type of page element

	uint32_t referenced_id;      ///< Id of referenced object (location, goal etc.)
	std::string text;          ///< Static content text of page element

	/**
	 * We need an (empty) constructor so struct isn't zeroed (as C++ standard states)
	 */
	inline StoryPageElement() { }

	/**
	 * (Empty) destructor has to be defined else operator delete might be called with nullptr parameter
	 */
	inline ~StoryPageElement() { }
};

/** Struct about stories, current and completed */
struct StoryPage : StoryPagePool::PoolItem<&_story_page_pool> {
	uint32_t sort_value;            ///< A number that increases for every created story page. Used for sorting. The id of a story page is the pool index.
	TimerGameCalendar::Date date; ///< Date when the page was created.
	CompanyID company;            ///< StoryPage is for a specific company; INVALID_COMPANY if it is global

	std::string title;            ///< Title of story page

	/**
	 * We need an (empty) constructor so struct isn't zeroed (as C++ standard states)
	 */
	inline StoryPage() { }

	/**
	 * (Empty) destructor has to be defined else operator delete might be called with nullptr parameter
	 */
	inline ~StoryPage()
	{
		if (!this->CleaningPool()) {
			for (StoryPageElement *spe : StoryPageElement::Iterate()) {
				if (spe->page == this->index) delete spe;
			}
		}
	}
};

#endif /* STORY_BASE_H */

