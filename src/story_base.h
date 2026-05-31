/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file story_base.h %StoryPage base class. */

#ifndef STORY_BASE_H
#define STORY_BASE_H

#include "company_type.h"
#include "story_type.h"
#include "strings_type.h"
#include "timer/timer_game_calendar.h"
#include "gfx_type.h"
#include "vehicle_type.h"
#include "core/pool_type.hpp"

using StoryPageElementPool = Pool<StoryPageElement, StoryPageElementID, 64>;
using StoryPagePool = Pool<StoryPage, StoryPageID, 64>;
extern StoryPageElementPool _story_page_element_pool;
extern StoryPagePool _story_page_pool;
extern uint32_t _story_page_element_next_sort_value;
extern uint32_t _story_page_next_sort_value;

/** Each story page element is one of these types. */
enum class StoryPageElementType : uint8_t {
	Text = 0, ///< A text element.
	Location, ///< An element that references a tile along with a one-line text.
	Goal, ///< An element that references a goal.
	ButtonPush, ///< A push button that triggers an immediate event.
	ButtonTile, ///< A button that allows the player to select a tile, and triggers an event with the tile.
	ButtonVehicle, ///< A button that allows the player to select a vehicle, and triggers an event with the vehicle.
	Invalid = 0xFF, ///< Invalid story page element type.
};

/** Flags available for buttons */
enum class StoryPageButtonFlag : uint8_t {
	FloatLeft, ///< Button will float on the left.
	FloatRight, ///< Button will float on the right.
};

/** Bitset of \c StoryPageButtonFlag elements. */
using StoryPageButtonFlags = EnumBitSet<StoryPageButtonFlag, uint8_t>;

/** Mouse cursors usable by story page buttons. */
enum class StoryPageButtonCursor : uint8_t {
	Mouse, ///< Use the Mouse cursor
	Zzz, ///< Use the Zzz cursor
	Buoy, ///< Use the Buoy cursor
	Query, ///< Use the Query cursor
	HQ, ///< Use the HQ cursor
	ShipDepot, ///< Use the Ship Depot cursor
	Sign, ///< Use the Sign cursor
	Tree, ///< Use the Tree cursor
	BuyLand, ///< Use the Buy Land cursor
	LevelLand, ///< Use the Level Land cursor
	Town, ///< Use the Town cursor
	Industry, ///< Use the Industry cursor
	RockyArea, ///< Use the RockyArea cursor
	Desert, ///< Use the Desert cursor
	Transmitter, ///< Use the Transmitter cursor
	Airport, ///< Use the Airport cursor
	Dock, ///< Use the Dock cursor
	Canal, ///< Use the Canal cursor
	Lock, ///< Use the Lock cursor
	River, ///< Use the River cursor
	Aqueduct, ///< Use the Aqueduct cursor
	Bridge, ///< Use the Bridge cursor
	RailStation, ///< Use the Rail Station cursor
	TunnelRail, ///< Use the Tunnel Rail cursor
	TunnelElrail, ///< Use the Tunnel Elrail cursor
	TunnelMono, ///< Use the Tunnel Mono cursor
	TunnelMaglev, ///< Use the Tunnel Maglev cursor
	AutoRail, ///< Use the Auto Rail cursor
	AutoElrail, ///< Use the Auto Elrail cursor
	AutoMono, ///< Use the Auto Mono cursor
	AutoMaglev, ///< Use the Auto Maglev cursor
	Waypoint, ///< Use the Waypoint cursor
	RailDepot, ///< Use the Rail Depot cursor
	ElrailDepot, ///< Use the Elrail Depot cursor
	MonoDepot, ///< Use the Mono Depot cursor
	MaglevDepot, ///< Use the Maglev Depot cursor
	ConvertRail, ///< Use the Convert Rail cursor
	ConvertElrail, ///< Use the Convert Elrail cursor
	ConvertMono, ///< Use the Convert Mono cursor
	ConvertMaglev, ///< Use the Convert Maglev cursor
	AutoRoad, ///< Use the Auto Road cursor
	AutoTram, ///< Use the Auto Tram cursor
	RoadDepot, ///< Use the Road Depot cursor
	BusStation, ///< Use the Bus Station cursor
	TruckStation, ///< Use the Truck Station cursor
	RoadTunnel, ///< Use the Road Tunnel cursor
	CloneTrain, ///< Use the Clone Train cursor
	CloneRoadVeh, ///< Use the Clone Road Veh cursor
	CloneShip, ///< Use the Clone Ship cursor
	CloneAirplane, ///< Use the Clone Airplane cursor
	Demolish, ///< Use the Demolish cursor
	LowerLand, ///< Use the Lower Land cursor
	RaiseLand, ///< Use the Raise Land cursor
	PickStation, ///< Use the Pick Station cursor
	BuildSignals, ///< Use the Build Signals cursor
	End, ///< End marker.
	Invalid = 0xFF, ///< Invalid story page button cursor.
};

/**
 * Checks if a StoryPageButtonCursor value is valid.
 *
 * @param cursor The value to check.
 * @return true if the given value is a valid StoryPageButtonCursor.
 */
inline bool IsValidStoryPageButtonCursor(StoryPageButtonCursor cursor)
{
	return cursor < StoryPageButtonCursor::End;
}

/** Helper to construct packed "id" values for button-type StoryPageElement */
struct StoryPageButtonData {
	uint32_t referenced_id = 0;

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
	uint32_t sort_value = 0; ///< A number that increases for every created story page element. Used for sorting. The id of a story page element is the pool index.
	StoryPageID page{}; ///< Id of the page which the page element belongs to
	StoryPageElementType type{}; ///< Type of page element

	uint32_t referenced_id = 0; ///< Id of referenced object (location, goal etc.)
	EncodedString text{}; ///< Static content text of page element

	StoryPageElement(StoryPageElementID index) : StoryPageElementPool::PoolItem<&_story_page_element_pool>(index) {}
	StoryPageElement(StoryPageElementID index, uint32_t sort_value, StoryPageElementType type, StoryPageID page) :
		StoryPageElementPool::PoolItem<&_story_page_element_pool>(index), sort_value(sort_value), page(page), type(type) {}

	/**
	 * (Empty) destructor has to be defined else operator delete might be called with nullptr parameter
	 */
	~StoryPageElement() { }
};

/** Struct about stories, current and completed */
struct StoryPage : StoryPagePool::PoolItem<&_story_page_pool> {
	uint32_t sort_value = 0; ///< A number that increases for every created story page. Used for sorting. The id of a story page is the pool index.
	TimerGameCalendar::Date date{}; ///< Date when the page was created.
	CompanyID company = CompanyID::Invalid(); ///< StoryPage is for a specific company; CompanyID::Invalid() if it is global

	EncodedString title; ///< Title of story page

	StoryPage(StoryPageID index) : StoryPagePool::PoolItem<&_story_page_pool>(index) {}
	StoryPage(StoryPageID index, uint32_t sort_value, TimerGameCalendar::Date date, CompanyID company, const EncodedString &title) :
		StoryPagePool::PoolItem<&_story_page_pool>(index), sort_value(sort_value), date(date), company(company), title(title) {}

	~StoryPage();
};

#endif /* STORY_BASE_H */

