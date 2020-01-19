/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file story.cpp Handling of stories. */

#include "stdafx.h"
#include "story_base.h"
#include "core/pool_func.hpp"
#include "cmd_helper.h"
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "string_func.h"
#include "date_func.h"
#include "tile_map.h"
#include "goal_type.h"
#include "goal_base.h"
#include "window_func.h"
#include "gui.h"
#include "vehicle_base.h"
#include "game/game.hpp"
#include "script/api/script_story_page.hpp"
#include "script/api/script_event_types.hpp"

#include "safeguards.h"


StoryPageElementID _new_story_page_element_id;
StoryPageID _new_story_page_id;
uint32 _story_page_element_next_sort_value;
uint32 _story_page_next_sort_value;

StoryPageElementPool _story_page_element_pool("StoryPageElement");
StoryPagePool _story_page_pool("StoryPage");
INSTANTIATE_POOL_METHODS(StoryPageElement)
INSTANTIATE_POOL_METHODS(StoryPage)

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
static bool VerifyElementContentParameters(StoryPageID page_id, StoryPageElementType type, TileIndex tile, uint32 reference, const char *text)
{
	StoryPageButtonData button_data{ reference };

	switch (type) {
		case SPET_TEXT:
			if (StrEmpty(text)) return false;
			break;
		case SPET_LOCATION:
			if (StrEmpty(text)) return false;
			if (!IsValidTile(tile)) return false;
			break;
		case SPET_GOAL:
			if (!Goal::IsValidID((GoalID)reference)) return false;
			/* Reject company specific goals on global pages */
			if (StoryPage::Get(page_id)->company == INVALID_COMPANY && Goal::Get((GoalID)reference)->company != INVALID_COMPANY) return false;
			break;
		case SPET_BUTTON_PUSH:
			if (!button_data.ValidateColour()) return false;
			return true;
		case SPET_BUTTON_TILE:
			if (!button_data.ValidateColour()) return false;
			if (!button_data.ValidateCursor()) return false;
			return true;
		case SPET_BUTTON_VEHICLE:
			if (!button_data.ValidateColour()) return false;
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
static void UpdateElement(StoryPageElement &pe, TileIndex tile, uint32 reference, const char *text)
{
	switch (pe.type) {
		case SPET_TEXT:
			pe.text = stredup(text);
			break;
		case SPET_LOCATION:
			pe.text = stredup(text);
			pe.referenced_id = tile;
			break;
		case SPET_GOAL:
			pe.referenced_id = (GoalID)reference;
			break;
		case SPET_BUTTON_PUSH:
		case SPET_BUTTON_TILE:
		case SPET_BUTTON_VEHICLE:
			pe.text = stredup(text);
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
	return Extract<Colours, 0, 8>(this->referenced_id);
}

/** Get the mouse cursor used while waiting for input for the button. */
StoryPageButtonCursor StoryPageButtonData::GetCursor() const
{
	return Extract<StoryPageButtonCursor, 8, 8>(this->referenced_id);
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

/** Verify that the data stores a valid StoryPageButtonCursor value */
bool StoryPageButtonData::ValidateCursor() const
{
	return GB(this->referenced_id, 8, 8) < SPBC_END;
}

/** Verity that the data stored a valid VehicleType value */
bool StoryPageButtonData::ValidateVehicleType() const
{
	uint8 vehtype = GB(this->referenced_id, 16, 8);
	return vehtype == VEH_INVALID || vehtype < VEH_COMPANY_END;
}

/**
 * Create a new story page.
 * @param tile unused.
 * @param flags type of operation
 * @param p1 various bitstuffed elements
 * - p1 = (bit  0 -  7) - Company for which this story page belongs to.
 * @param p2 unused.
 * @param text Title of the story page. Null is allowed in which case a generic page title is provided by OpenTTD.
 * @return the cost of this operation or an error
 */
CommandCost CmdCreateStoryPage(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (!StoryPage::CanAllocateItem()) return CMD_ERROR;

	CompanyID company = (CompanyID)GB(p1, 0, 8);

	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (company != INVALID_COMPANY && !Company::IsValidID(company)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (_story_page_pool.items == 0) {
			/* Initialize the next sort value variable. */
			_story_page_next_sort_value = 0;
		}

		StoryPage *s = new StoryPage();
		s->sort_value = _story_page_next_sort_value;
		s->date = _date;
		s->company = company;
		if (StrEmpty(text)) {
			s->title = nullptr;
		} else {
			s->title = stredup(text);
		}

		InvalidateWindowClassesData(WC_STORY_BOOK, -1);
		if (StoryPage::GetNumItems() == 1) InvalidateWindowData(WC_MAIN_TOOLBAR, 0);

		_new_story_page_id = s->index;
		_story_page_next_sort_value++;
	}

	return CommandCost();
}

/**
 * Create a new story page element.
 * @param tile Tile location if it is a location page element, otherwise unused.
 * @param flags type of operation
 * @param p1 various bitstuffed elements
 * - p1 = (bit  0 -  15) - The page which the element belongs to.
 *        (bit  16 -  23) - Page element type
 * @param p2 Id of referenced object
 * @param text Text content in case it is a text or location page element
 * @return the cost of this operation or an error
 */
CommandCost CmdCreateStoryPageElement(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (!StoryPageElement::CanAllocateItem()) return CMD_ERROR;

	StoryPageID page_id = (StoryPageID)GB(p1, 0, 16);
	StoryPageElementType type = Extract<StoryPageElementType, 16, 8>(p1);

	/* Allow at most 128 elements per page. */
	uint16 element_count = 0;
	for (StoryPageElement *iter : StoryPageElement::Iterate()) {
		if (iter->page == page_id) element_count++;
	}
	if (element_count >= 128) return CMD_ERROR;

	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!StoryPage::IsValidID(page_id)) return CMD_ERROR;
	if (!VerifyElementContentParameters(page_id, type, tile, p2, text)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (_story_page_element_pool.items == 0) {
			/* Initialize the next sort value variable. */
			_story_page_element_next_sort_value = 0;
		}

		StoryPageElement *pe = new StoryPageElement();
		pe->sort_value = _story_page_element_next_sort_value;
		pe->type = type;
		pe->page = page_id;
		UpdateElement(*pe, tile, p2, text);

		InvalidateWindowClassesData(WC_STORY_BOOK, page_id);

		_new_story_page_element_id = pe->index;
		_story_page_element_next_sort_value++;
	}

	return CommandCost();
}

/**
 * Update a new story page element.
 * @param tile Tile location if it is a location page element, otherwise unused.
 * @param flags type of operation
 * @param p1 various bitstuffed elements
 * - p1 = (bit  0 -  15) - The page element to update.
 *        (bit  16 -  31) - unused
 * @param p2 Id of referenced object
 * @param text Text content in case it is a text or location page element
 * @return the cost of this operation or an error
 */
CommandCost CmdUpdateStoryPageElement(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	StoryPageElementID page_element_id = (StoryPageElementID)GB(p1, 0, 16);

	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (!StoryPageElement::IsValidID(page_element_id)) return CMD_ERROR;

	StoryPageElement *pe = StoryPageElement::Get(page_element_id);
	StoryPageID page_id = pe->page;
	StoryPageElementType type = pe->type;

	if (!VerifyElementContentParameters(page_id, type, tile, p2, text)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		UpdateElement(*pe, tile, p2, text);
		InvalidateWindowClassesData(WC_STORY_BOOK, pe->page);
	}

	return CommandCost();
}

/**
 * Update title of a story page.
 * @param tile unused.
 * @param flags type of operation
 * @param p1 = (bit 0 - 15) - StoryPageID to update.
 * @param p2 unused
 * @param text title text of the story page.
 * @return the cost of this operation or an error
 */
CommandCost CmdSetStoryPageTitle(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	StoryPageID page_id = (StoryPageID)GB(p1, 0, 16);
	if (!StoryPage::IsValidID(page_id)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		StoryPage *p = StoryPage::Get(page_id);
		free(p->title);
		if (StrEmpty(text)) {
			p->title = nullptr;
		} else {
			p->title = stredup(text);
		}

		InvalidateWindowClassesData(WC_STORY_BOOK, page_id);
	}

	return CommandCost();
}

/**
 * Update date of a story page.
 * @param tile unused.
 * @param flags type of operation
 * @param p1 = (bit 0 - 15) - StoryPageID to update.
 * @param p2 = (bit 0 - 31) - date
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdSetStoryPageDate(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	StoryPageID page_id = (StoryPageID)GB(p1, 0, 16);
	if (!StoryPage::IsValidID(page_id)) return CMD_ERROR;
	Date date = (Date)p2;

	if (flags & DC_EXEC) {
		StoryPage *p = StoryPage::Get(page_id);
		p->date = date;

		InvalidateWindowClassesData(WC_STORY_BOOK, page_id);
	}

	return CommandCost();
}

/**
 * Display a story page for all clients that are allowed to
 * view the story page.
 * @param tile unused.
 * @param flags type of operation
 * @param p1 = (bit 0 - 15) - StoryPageID to show.
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdShowStoryPage(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	StoryPageID page_id = (StoryPageID)GB(p1, 0, 16);
	if (!StoryPage::IsValidID(page_id)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		StoryPage *g = StoryPage::Get(page_id);
		if ((g->company != INVALID_COMPANY && g->company == _local_company) || (g->company == INVALID_COMPANY && Company::IsValidID(_local_company))) ShowStoryBook(_local_company, page_id);
	}

	return CommandCost();
}
/**
 * Remove a story page and associated story page elements.
 * @param tile unused.
 * @param flags type of operation
 * @param p1 = (bit 0 - 15) - StoryPageID to remove.
 * @param p2 unused.
 * @param text unused.
 * @return the cost of this operation or an error
 */
CommandCost CmdRemoveStoryPage(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	StoryPageID page_id = (StoryPageID)p1;
	if (!StoryPage::IsValidID(page_id)) return CMD_ERROR;

	if (flags & DC_EXEC) {
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
 * @param tile unused.
 * @param flags type of operation
 * @param p1 = (bit 0 - 15) - StoryPageElementID to remove.
 * @param p2 unused.
 * @param text unused.
 * @return the cost of this operation or an error
 */
CommandCost CmdRemoveStoryPageElement(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	StoryPageElementID page_element_id = (StoryPageElementID)p1;
	if (!StoryPageElement::IsValidID(page_element_id)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		StoryPageElement *pe = StoryPageElement::Get(page_element_id);
		StoryPageID page_id = pe->page;

		delete pe;

		InvalidateWindowClassesData(WC_STORY_BOOK, page_id);
	}

	return CommandCost();
}

/**
 * Clicked/used a button on a story page.
 * @param tile   Tile selected, for tile selection buttons, otherwise unused.
 * @param flags  Type of operation.
 * @param p1     Bit 0..15 = story page element id of button.
 * @param p2     ID of selected item for buttons that select an item (e.g. vehicle), otherwise unused.
 * @param text   Unused.
 * @return The cost of the operation, or an error.
 */
CommandCost CmdStoryPageButton(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	StoryPageElementID page_element_id = (StoryPageElementID)GB(p1, 0, 16);

	if (!StoryPageElement::IsValidID(page_element_id)) return CMD_ERROR;
	const StoryPageElement *const pe = StoryPageElement::Get(page_element_id);

	/* Check the player belongs to the company that owns the page. */
	const StoryPage *const sp = StoryPage::Get(pe->page);
	if (sp->company != INVALID_COMPANY && sp->company != _current_company) return CMD_ERROR;

	switch (pe->type) {
		case SPET_BUTTON_PUSH:
			/* No validation required */
			if (flags & DC_EXEC) Game::NewEvent(new ScriptEventStoryPageButtonClick(_current_company, pe->page, page_element_id));
			break;
		case SPET_BUTTON_TILE:
			if (!IsValidTile(tile)) return CMD_ERROR;
			if (flags & DC_EXEC) Game::NewEvent(new ScriptEventStoryPageTileSelect(_current_company, pe->page, page_element_id, tile));
			break;
		case SPET_BUTTON_VEHICLE:
			if (!Vehicle::IsValidID(p2)) return CMD_ERROR;
			if (flags & DC_EXEC) Game::NewEvent(new ScriptEventStoryPageVehicleSelect(_current_company, pe->page, page_element_id, (VehicleID)p2));
			break;
		default:
			/* Invalid page element type, not a button. */
			return CMD_ERROR;
	}

	return CommandCost();
}

