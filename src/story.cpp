/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file story.cpp Handling of stories. */

#include "stdafx.h"
#include "story_base.h"
#include "core/pool_func.hpp"
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "string_func.h"
#include "timer/timer_game_calendar.h"
#include "tile_map.h"
#include "goal_type.h"
#include "goal_base.h"
#include "window_func.h"
#include "gui.h"
#include "vehicle_base.h"
#include "game/game.hpp"
#include "script/api/script_story_page.hpp"
#include "script/api/script_event_types.hpp"
#include "story_cmd.h"

#include "safeguards.h"


uint32_t _story_page_element_next_sort_value;
uint32_t _story_page_next_sort_value;

StoryPageElementPool _story_page_element_pool("StoryPageElement");
StoryPagePool _story_page_pool("StoryPage");
INSTANTIATE_POOL_METHODS(StoryPageElement)
INSTANTIATE_POOL_METHODS(StoryPage)

StoryPage::~StoryPage()
{
	if (CleaningPool()) return;

	for (StoryPageElement *spe : StoryPageElement::Iterate()) {
		if (spe->page == this->index) delete spe;
	}
}

/**
 * This helper for Create/Update PageElement Cmd procedure verifies if the page
 * element parameters are correct for the given page element type.
 * @param page_id The page id of the page which the page element (will) belong to
 * @param type The type of the page element to create/update
 * @param tile The tile parameter of the DoCommand proc
 * @param reference The reference parameter of the DoCommand proc (p2)
 * @param text The text parameter of the DoCommand proc
 * @return true, if and only if the given parameters are valid for the given page element type and page id.
 */
static bool VerifyElementContentParameters(StoryPageID page_id, StoryPageElementType type, TileIndex tile, uint32_t reference, const EncodedString &text)
{
	StoryPageButtonData button_data{ reference };

	switch (type) {
		case SPET_TEXT:
			if (text.empty()) return false;
			break;
		case SPET_LOCATION:
			if (text.empty()) return false;
			if (!IsValidTile(tile)) return false;
			break;
		case SPET_GOAL:
			if (!Goal::IsValidID((GoalID)reference)) return false;
			/* Reject company specific goals on global pages */
			if (StoryPage::Get(page_id)->company == CompanyID::Invalid() && Goal::Get((GoalID)reference)->company != CompanyID::Invalid()) return false;
			break;
		case SPET_BUTTON_PUSH:
			if (!button_data.ValidateColour()) return false;
			if (!button_data.ValidateFlags()) return false;
			return true;
		case SPET_BUTTON_TILE:
			if (!button_data.ValidateColour()) return false;
			if (!button_data.ValidateFlags()) return false;
			if (!button_data.ValidateCursor()) return false;
			return true;
		case SPET_BUTTON_VEHICLE:
			if (!button_data.ValidateColour()) return false;
			if (!button_data.ValidateFlags()) return false;
			if (!button_data.ValidateCursor()) return false;
			if (!button_data.ValidateVehicleType()) return false;
			return true;
		default:
			return false;
	}

	return true;
}

/**
 * This helper for Create/Update PageElement Cmd procedure updates a page
 * element with new content data.
 * @param pe The page element to update
 * @param tile The tile parameter of the DoCommand proc
 * @param reference The reference parameter of the DoCommand proc (p2)
 * @param text The text parameter of the DoCommand proc
 */
static void UpdateElement(StoryPageElement &pe, TileIndex tile, uint32_t reference, const EncodedString &text)
{
	switch (pe.type) {
		case SPET_TEXT:
			pe.text = text;
			break;
		case SPET_LOCATION:
			pe.text = text;
			pe.referenced_id = tile.base();
			break;
		case SPET_GOAL:
			pe.referenced_id = reference;
			break;
		case SPET_BUTTON_PUSH:
		case SPET_BUTTON_TILE:
		case SPET_BUTTON_VEHICLE:
			pe.text = text;
			pe.referenced_id = reference;
			break;
		default: NOT_REACHED();
	}
}

/** Set the button background colour. */
void StoryPageButtonData::SetColour(Colours button_colour)
{
	assert(button_colour < COLOUR_END);
	SB(this->referenced_id, 0, 8, button_colour);
}

void StoryPageButtonData::SetFlags(StoryPageButtonFlags flags)
{
	SB(this->referenced_id, 24, 8, flags);
}

/** Set the mouse cursor used while waiting for input for the button. */
void StoryPageButtonData::SetCursor(StoryPageButtonCursor cursor)
{
	assert(cursor < SPBC_END);
	SB(this->referenced_id, 8, 8, cursor);
}

/** Set the type of vehicles that are accepted by the button */
void StoryPageButtonData::SetVehicleType(VehicleType vehtype)
{
	assert(vehtype == VEH_INVALID || vehtype < VEH_COMPANY_END);
	SB(this->referenced_id, 16, 8, vehtype);
}

/** Get the button background colour. */
Colours StoryPageButtonData::GetColour() const
{
	Colours colour = static_cast<Colours>(GB(this->referenced_id, 0, 8));
	if (!IsValidColours(colour)) return INVALID_COLOUR;
	return colour;
}

StoryPageButtonFlags StoryPageButtonData::GetFlags() const
{
	return (StoryPageButtonFlags)GB(this->referenced_id, 24, 8);
}

/** Get the mouse cursor used while waiting for input for the button. */
StoryPageButtonCursor StoryPageButtonData::GetCursor() const
{
	StoryPageButtonCursor cursor = (StoryPageButtonCursor)GB(this->referenced_id, 8, 8);
	if (!IsValidStoryPageButtonCursor(cursor)) return INVALID_SPBC;
	return cursor;
}

/** Get the type of vehicles that are accepted by the button */
VehicleType StoryPageButtonData::GetVehicleType() const
{
	return (VehicleType)GB(this->referenced_id, 16, 8);
}

/** Verify that the data stored a valid Colour value */
bool StoryPageButtonData::ValidateColour() const
{
	return GB(this->referenced_id, 0, 8) < COLOUR_END;
}

bool StoryPageButtonData::ValidateFlags() const
{
	uint8_t flags = GB(this->referenced_id, 24, 8);
	/* Don't allow float left and right together */
	if ((flags & SPBF_FLOAT_LEFT) && (flags & SPBF_FLOAT_RIGHT)) return false;
	/* Don't allow undefined flags */
	if (flags & ~(SPBF_FLOAT_LEFT | SPBF_FLOAT_RIGHT)) return false;
	return true;
}

/** Verify that the data stores a valid StoryPageButtonCursor value */
bool StoryPageButtonData::ValidateCursor() const
{
	return GB(this->referenced_id, 8, 8) < SPBC_END;
}

/** Verity that the data stored a valid VehicleType value */
bool StoryPageButtonData::ValidateVehicleType() const
{
	uint8_t vehtype = GB(this->referenced_id, 16, 8);
	return vehtype == VEH_INVALID || vehtype < VEH_COMPANY_END;
}

/**
 * Create a new story page.
 * @param flags type of operation
 * @param company Company for which this story page belongs to.
 * @param text Title of the story page. Null is allowed in which case a generic page title is provided by OpenTTD.
 * @return the cost of this operation or an error
 */
std::tuple<CommandCost, StoryPageID> CmdCreateStoryPage(DoCommandFlags flags, CompanyID company, const EncodedString &text)
{
	if (!StoryPage::CanAllocateItem()) return { CMD_ERROR, StoryPageID::Invalid() };

	if (_current_company != OWNER_DEITY) return { CMD_ERROR, StoryPageID::Invalid() };
	if (company != CompanyID::Invalid() && !Company::IsValidID(company)) return { CMD_ERROR, StoryPageID::Invalid() };

	if (flags.Test(DoCommandFlag::Execute)) {
		if (StoryPage::GetNumItems() == 0) {
			/* Initialize the next sort value variable. */
			_story_page_next_sort_value = 0;
		}

		StoryPage *s = new StoryPage(_story_page_next_sort_value, TimerGameCalendar::date, company, text);

		InvalidateWindowClassesData(WC_STORY_BOOK, -1);
		if (StoryPage::GetNumItems() == 1) InvalidateWindowData(WC_MAIN_TOOLBAR, 0);

		_story_page_next_sort_value++;
		return { CommandCost(), s->index };
	}

	return { CommandCost(), StoryPageID::Invalid() };
}

/**
 * Create a new story page element.
 * @param flags type of operation
 * @param tile Tile location if it is a location page element, otherwise unused.
 * @param page_id The page which the element belongs to.
 * @param type Page element type
 * @param reference Id of referenced object
 * @param text Text content in case it is a text or location page element
 * @return the cost of this operation or an error
 */
std::tuple<CommandCost, StoryPageElementID> CmdCreateStoryPageElement(DoCommandFlags flags, TileIndex tile, StoryPageID page_id, StoryPageElementType type, uint32_t reference, const EncodedString &text)
{
	if (!StoryPageElement::CanAllocateItem()) return { CMD_ERROR, StoryPageElementID::Invalid() };

	/* Allow at most 128 elements per page. */
	uint16_t element_count = 0;
	for (StoryPageElement *iter : StoryPageElement::Iterate()) {
		if (iter->page == page_id) element_count++;
	}
	if (element_count >= 128) return { CMD_ERROR, StoryPageElementID::Invalid() };

	if (_current_company != OWNER_DEITY) return { CMD_ERROR, StoryPageElementID::Invalid() };
	if (!StoryPage::IsValidID(page_id)) return { CMD_ERROR, StoryPageElementID::Invalid() };
	if (!VerifyElementContentParameters(page_id, type, tile, reference, text)) return { CMD_ERROR, StoryPageElementID::Invalid() };


	if (flags.Test(DoCommandFlag::Execute)) {
		if (StoryPageElement::GetNumItems() == 0) {
			/* Initialize the next sort value variable. */
			_story_page_element_next_sort_value = 0;
		}

		StoryPageElement *pe = new StoryPageElement(_story_page_element_next_sort_value, type, page_id);
		UpdateElement(*pe, tile, reference, text);

		InvalidateWindowClassesData(WC_STORY_BOOK, page_id);

		_story_page_element_next_sort_value++;
		return { CommandCost(), pe->index };
	}

	return { CommandCost(), StoryPageElementID::Invalid() };
}

/**
 * Update a new story page element.
 * @param flags type of operation
 * @param tile Tile location if it is a location page element, otherwise unused.
 * @param page_element_id The page element to update.
 * @param reference Id of referenced object
 * @param text Text content in case it is a text or location page element
 * @return the cost of this operation or an error
 */
CommandCost CmdUpdateStoryPageElement(DoCommandFlags flags, TileIndex tile, StoryPageElementID page_element_id, uint32_t reference, const EncodedString &text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!StoryPageElement::IsValidID(page_element_id)) return CMD_ERROR;

	StoryPageElement *pe = StoryPageElement::Get(page_element_id);
	StoryPageID page_id = pe->page;
	StoryPageElementType type = pe->type;

	if (!VerifyElementContentParameters(page_id, type, tile, reference, text)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		UpdateElement(*pe, tile, reference, text);
		InvalidateWindowClassesData(WC_STORY_BOOK, pe->page);
	}

	return CommandCost();
}

/**
 * Update title of a story page.
 * @param flags type of operation
 * @param page_id StoryPageID to update.
 * @param text title text of the story page.
 * @return the cost of this operation or an error
 */
CommandCost CmdSetStoryPageTitle(DoCommandFlags flags, StoryPageID page_id, const EncodedString &text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!StoryPage::IsValidID(page_id)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		StoryPage *p = StoryPage::Get(page_id);
		p->title = text;

		InvalidateWindowClassesData(WC_STORY_BOOK, page_id);
	}

	return CommandCost();
}

/**
 * Update date of a story page.
 * @param flags type of operation
 * @param page_id StoryPageID to update.
 * @param date date
 * @return the cost of this operation or an error
 */
CommandCost CmdSetStoryPageDate(DoCommandFlags flags, StoryPageID page_id, TimerGameCalendar::Date date)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!StoryPage::IsValidID(page_id)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		StoryPage *p = StoryPage::Get(page_id);
		p->date = date;

		InvalidateWindowClassesData(WC_STORY_BOOK, page_id);
	}

	return CommandCost();
}

/**
 * Display a story page for all clients that are allowed to
 * view the story page.
 * @param flags type of operation
 * @param page_id StoryPageID to show.
 * @return the cost of this operation or an error
 */
CommandCost CmdShowStoryPage(DoCommandFlags flags, StoryPageID page_id)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!StoryPage::IsValidID(page_id)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		StoryPage *g = StoryPage::Get(page_id);
		if ((g->company != CompanyID::Invalid() && g->company == _local_company) || (g->company == CompanyID::Invalid() && Company::IsValidID(_local_company))) ShowStoryBook(_local_company, page_id, true);
	}

	return CommandCost();
}
/**
 * Remove a story page and associated story page elements.
 * @param flags type of operation
 * @param page_id StoryPageID to remove.
 * @return the cost of this operation or an error
 */
CommandCost CmdRemoveStoryPage(DoCommandFlags flags, StoryPageID page_id)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!StoryPage::IsValidID(page_id)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		StoryPage *p = StoryPage::Get(page_id);

		for (StoryPageElement *pe : StoryPageElement::Iterate()) {
			if (pe->page == p->index) {
				delete pe;
			}
		}

		delete p;

		InvalidateWindowClassesData(WC_STORY_BOOK, -1);
		if (StoryPage::GetNumItems() == 0) InvalidateWindowData(WC_MAIN_TOOLBAR, 0);
	}

	return CommandCost();
}

/**
 * Remove a story page element
 * @param flags type of operation
 * @param page_element_id StoryPageElementID to remove.
 * @return the cost of this operation or an error
 */
CommandCost CmdRemoveStoryPageElement(DoCommandFlags flags, StoryPageElementID page_element_id)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!StoryPageElement::IsValidID(page_element_id)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		StoryPageElement *pe = StoryPageElement::Get(page_element_id);
		StoryPageID page_id = pe->page;

		delete pe;

		InvalidateWindowClassesData(WC_STORY_BOOK, page_id);
	}

	return CommandCost();
}

/**
 * Clicked/used a button on a story page.
 * @param flags  Type of operation.
 * @param tile   Tile selected, for tile selection buttons, otherwise unused.
 * @param page_element_id story page element id of button.
 * @param reference ID of selected item for buttons that select an item (e.g. vehicle), otherwise unused.
 * @return The cost of the operation, or an error.
 */
CommandCost CmdStoryPageButton(DoCommandFlags flags, TileIndex tile, StoryPageElementID page_element_id, VehicleID reference)
{
	if (!StoryPageElement::IsValidID(page_element_id)) return CMD_ERROR;
	const StoryPageElement *const pe = StoryPageElement::Get(page_element_id);

	/* Check the player belongs to the company that owns the page. */
	const StoryPage *const sp = StoryPage::Get(pe->page);
	if (sp->company != CompanyID::Invalid() && sp->company != _current_company) return CMD_ERROR;

	switch (pe->type) {
		case SPET_BUTTON_PUSH:
			/* No validation required */
			if (flags.Test(DoCommandFlag::Execute)) Game::NewEvent(new ScriptEventStoryPageButtonClick(_current_company, pe->page, page_element_id));
			break;
		case SPET_BUTTON_TILE:
			if (!IsValidTile(tile)) return CMD_ERROR;
			if (flags.Test(DoCommandFlag::Execute)) Game::NewEvent(new ScriptEventStoryPageTileSelect(_current_company, pe->page, page_element_id, tile));
			break;
		case SPET_BUTTON_VEHICLE:
			if (!Vehicle::IsValidID(reference)) return CMD_ERROR;
			if (flags.Test(DoCommandFlag::Execute)) Game::NewEvent(new ScriptEventStoryPageVehicleSelect(_current_company, pe->page, page_element_id, reference));
			break;
		default:
			/* Invalid page element type, not a button. */
			return CMD_ERROR;
	}

	return CommandCost();
}

