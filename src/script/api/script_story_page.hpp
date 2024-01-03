/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_story_page.hpp Everything to manipulate a story page. */

#ifndef SCRIPT_STORY_HPP
#define SCRIPT_STORY_HPP

#include "script_company.hpp"
#include "script_date.hpp"
#include "script_vehicle.hpp"
#include "../../story_type.h"
#include "../../story_base.h"

/**
 * Class that handles story page related functions.
 *
 * To create a page:
 * 1. Create the page
 * 2. Create page elements that will be appended to the page in the order which they are created.
 *
 * Pages can be either global or company specific. It is possible to mix, but the only mixed solution
 * that will work is to have all global pages first. Once you create the first company specific page,
 * it is not recommended to add additional global pages unless you clear up all pages first.
 *
 * Page elements are stacked vertically on a page. If goal elements are used, the element will
 * become empty if the goal is removed while the page still exist. Instead of removing the goal,
 * you can mark it as complete and the Story Book will show that the goal is completed.
 *
 * Mind that users might want to go back to old pages later on. Thus do not remove pages in
 * the story book unless you really need to.
 *
 * @api game
 */
class ScriptStoryPage : public ScriptObject {
public:
	/**
	 * The story page IDs.
	 */
	enum StoryPageID {
		/* Note: these values represent part of the in-game StoryPageID enum */
		STORY_PAGE_INVALID = ::INVALID_STORY_PAGE, ///< An invalid story page id.
	};

	/**
	 * The story page element IDs.
	 */
	enum StoryPageElementID {
		/* Note: these values represent part of the in-game StoryPageElementID enum */
		STORY_PAGE_ELEMENT_INVALID = ::INVALID_STORY_PAGE_ELEMENT, ///< An invalid story page element id.
	};

	/**
	 * Story page element types.
	 */
	enum StoryPageElementType : byte {
		SPET_TEXT = ::SPET_TEXT,                     ///< An element that displays a block of text.
		SPET_LOCATION = ::SPET_LOCATION,             ///< An element that displays a single line of text along with a button to view the referenced location.
		SPET_GOAL = ::SPET_GOAL,                     ///< An element that displays a goal.
		SPET_BUTTON_PUSH = ::SPET_BUTTON_PUSH,       ///< A push button that triggers an immediate event.
		SPET_BUTTON_TILE = ::SPET_BUTTON_TILE,       ///< A button that allows the player to select a tile, and triggers an event with the tile.
		SPET_BUTTON_VEHICLE = ::SPET_BUTTON_VEHICLE, ///< A button that allows the player to select a vehicle, and triggers an event wih the vehicle.
	};

	/**
	 * Formatting data for button page elements.
	 */
	typedef uint32_t StoryPageButtonFormatting;

	/**
	 * Formatting and layout flags for story page buttons.
	 * The SPBF_FLOAT_LEFT and SPBF_FLOAT_RIGHT flags can not be combined.
	 */
	enum StoryPageButtonFlags : byte {
		SPBF_NONE        = ::SPBF_NONE,        ///< No special formatting for button.
		SPBF_FLOAT_LEFT  = ::SPBF_FLOAT_LEFT,  ///< Button is placed to the left of the following paragraph.
		SPBF_FLOAT_RIGHT = ::SPBF_FLOAT_RIGHT, ///< Button is placed to the right of the following paragraph.
	};

	/**
	 * Mouse cursors usable by story page buttons.
	 */
	enum StoryPageButtonCursor : byte {
		SPBC_MOUSE          = ::SPBC_MOUSE,
		SPBC_ZZZ            = ::SPBC_ZZZ,
		SPBC_BUOY           = ::SPBC_BUOY,
		SPBC_QUERY          = ::SPBC_QUERY,
		SPBC_HQ             = ::SPBC_HQ,
		SPBC_SHIP_DEPOT     = ::SPBC_SHIP_DEPOT,
		SPBC_SIGN           = ::SPBC_SIGN,
		SPBC_TREE           = ::SPBC_TREE,
		SPBC_BUY_LAND       = ::SPBC_BUY_LAND,
		SPBC_LEVEL_LAND     = ::SPBC_LEVEL_LAND,
		SPBC_TOWN           = ::SPBC_TOWN,
		SPBC_INDUSTRY       = ::SPBC_INDUSTRY,
		SPBC_ROCKY_AREA     = ::SPBC_ROCKY_AREA,
		SPBC_DESERT         = ::SPBC_DESERT,
		SPBC_TRANSMITTER    = ::SPBC_TRANSMITTER,
		SPBC_AIRPORT        = ::SPBC_AIRPORT,
		SPBC_DOCK           = ::SPBC_DOCK,
		SPBC_CANAL          = ::SPBC_CANAL,
		SPBC_LOCK           = ::SPBC_LOCK,
		SPBC_RIVER          = ::SPBC_RIVER,
		SPBC_AQUEDUCT       = ::SPBC_AQUEDUCT,
		SPBC_BRIDGE         = ::SPBC_BRIDGE,
		SPBC_RAIL_STATION   = ::SPBC_RAIL_STATION,
		SPBC_TUNNEL_RAIL    = ::SPBC_TUNNEL_RAIL,
		SPBC_TUNNEL_ELRAIL  = ::SPBC_TUNNEL_ELRAIL,
		SPBC_TUNNEL_MONO    = ::SPBC_TUNNEL_MONO,
		SPBC_TUNNEL_MAGLEV  = ::SPBC_TUNNEL_MAGLEV,
		SPBC_AUTORAIL       = ::SPBC_AUTORAIL,
		SPBC_AUTOELRAIL     = ::SPBC_AUTOELRAIL,
		SPBC_AUTOMONO       = ::SPBC_AUTOMONO,
		SPBC_AUTOMAGLEV     = ::SPBC_AUTOMAGLEV,
		SPBC_WAYPOINT       = ::SPBC_WAYPOINT,
		SPBC_RAIL_DEPOT     = ::SPBC_RAIL_DEPOT,
		SPBC_ELRAIL_DEPOT   = ::SPBC_ELRAIL_DEPOT,
		SPBC_MONO_DEPOT     = ::SPBC_MONO_DEPOT,
		SPBC_MAGLEV_DEPOT   = ::SPBC_MAGLEV_DEPOT,
		SPBC_CONVERT_RAIL   = ::SPBC_CONVERT_RAIL,
		SPBC_CONVERT_ELRAIL = ::SPBC_CONVERT_ELRAIL,
		SPBC_CONVERT_MONO   = ::SPBC_CONVERT_MONO,
		SPBC_CONVERT_MAGLEV = ::SPBC_CONVERT_MAGLEV,
		SPBC_AUTOROAD       = ::SPBC_AUTOROAD,
		SPBC_AUTOTRAM       = ::SPBC_AUTOTRAM,
		SPBC_ROAD_DEPOT     = ::SPBC_ROAD_DEPOT,
		SPBC_BUS_STATION    = ::SPBC_BUS_STATION,
		SPBC_TRUCK_STATION  = ::SPBC_TRUCK_STATION,
		SPBC_ROAD_TUNNEL    = ::SPBC_ROAD_TUNNEL,
		SPBC_CLONE_TRAIN    = ::SPBC_CLONE_TRAIN,
		SPBC_CLONE_ROADVEH  = ::SPBC_CLONE_ROADVEH,
		SPBC_CLONE_SHIP     = ::SPBC_CLONE_SHIP,
		SPBC_CLONE_AIRPLANE = ::SPBC_CLONE_AIRPLANE,
		SPBC_DEMOLISH       = ::SPBC_DEMOLISH,
		SPBC_LOWERLAND      = ::SPBC_LOWERLAND,
		SPBC_RAISELAND      = ::SPBC_RAISELAND,
		SPBC_PICKSTATION    = ::SPBC_PICKSTATION,
		SPBC_BUILDSIGNALS   = ::SPBC_BUILDSIGNALS,
	};

	/**
	 * Colour codes usable for story page button elements.
	 * Place a colour value in the lowest 8 bits of the \c reference parameter to the button.
	 */
	enum StoryPageButtonColour {
		SPBC_DARK_BLUE  = ::COLOUR_DARK_BLUE,
		SPBC_PALE_GREEN = ::COLOUR_PALE_GREEN,
		SPBC_PINK       = ::COLOUR_PINK,
		SPBC_YELLOW     = ::COLOUR_YELLOW,
		SPBC_RED        = ::COLOUR_RED,
		SPBC_LIGHT_BLUE = ::COLOUR_LIGHT_BLUE,
		SPBC_GREEN      = ::COLOUR_GREEN,
		SPBC_DARK_GREEN = ::COLOUR_DARK_GREEN,
		SPBC_BLUE       = ::COLOUR_BLUE,
		SPBC_CREAM      = ::COLOUR_CREAM,
		SPBC_MAUVE      = ::COLOUR_MAUVE,
		SPBC_PURPLE     = ::COLOUR_PURPLE,
		SPBC_ORANGE     = ::COLOUR_ORANGE,
		SPBC_BROWN      = ::COLOUR_BROWN,
		SPBC_GREY       = ::COLOUR_GREY,
		SPBC_WHITE      = ::COLOUR_WHITE,
	};

	/**
	 * Check whether this is a valid story page ID.
	 * @param story_page_id The StoryPageID to check.
	 * @return True if and only if this story page is valid.
	 */
	static bool IsValidStoryPage(StoryPageID story_page_id);

	/**
	 * Check whether this is a valid story page element ID.
	 * @param story_page_element_id The StoryPageElementID to check.
	 * @return True if and only if this story page element is valid.
	 */
	static bool IsValidStoryPageElement(StoryPageElementID story_page_element_id);

	/**
	 * Create a new story page.
	 * @param company The company to create the story page for, or ScriptCompany::COMPANY_INVALID for all.
	 * @param title Page title (can be either a raw string, a ScriptText object, or null).
	 * @return The new StoryPageID, or STORY_PAGE_INVALID if it failed.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre company == COMPANY_INVALID || ResolveCompanyID(company) != COMPANY_INVALID.
	 */
	static StoryPageID New(ScriptCompany::CompanyID company, Text *title);

	/**
	 * Create a new story page element.
	 * @param story_page_id The page id of the story page which the page element should be appended to.
	 * @param type Which page element type to create.
	 * @param reference A reference value to the object that is referred to by some page element types.
	 *                  When type is SPET_GOAL, this is the goal ID.
	 *                  When type is SPET_LOCATION, this is the TileIndex.
	 *                  When type is a button, this is additional parameters for the button,
	 *                  use the #BuildPushButtonReference, #BuildTileButtonReference, or #BuildVehicleButtonReference functions to make the values.
	 * @param text The body text of page elements that allow custom text. (SPET_TEXT and SPET_LOCATION)
	 * @return The new StoryPageElementID, or STORY_PAGE_ELEMENT_INVALID if it failed.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidStoryPage(story_page).
	 * @pre (type != SPET_TEXT && type != SPET_LOCATION) || (text != null && len(text) != 0).
	 * @pre type != SPET_LOCATION || ScriptMap::IsValidTile(reference).
	 * @pre type != SPET_GOAL || ScriptGoal::IsValidGoal(reference).
	 * @pre if type is SPET_GOAL and story_page is a global page, then referenced goal must be global.
	 */
	static StoryPageElementID NewElement(StoryPageID story_page_id, StoryPageElementType type, SQInteger reference, Text *text);

	/**
	 * Update the content of a page element
	 * @param story_page_element_id The page id of the story page which the page element should be appended to.
	 * @param reference A reference value to the object that is referred to by some page element types. See also NewElement.
	 * @param text The body text of page elements that allow custom text. See also NewElement.
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidStoryPage(story_page).
	 * @pre (type != SPET_TEXT && type != SPET_LOCATION) || (text != null && len(text) != 0).
	 * @pre type != SPET_LOCATION || ScriptMap::IsValidTile(reference).
	 * @pre type != SPET_GOAL || ScriptGoal::IsValidGoal(reference).
	 * @pre if type is SPET_GOAL and story_page is a global page, then referenced goal must be global.
	 */
	static bool UpdateElement(StoryPageElementID story_page_element_id, SQInteger reference, Text *text);

	/**
	 * Get story page sort value. Each page has a sort value that is internally assigned and used
	 * to sort the pages in the story book. OpenTTD maintains this number so that the sort order
	 * is perceived. This API exist only so that you can sort ScriptStoryPageList the same order
	 * as in GUI. You should not use this number for anything else.
	 * @param story_page_id The story page to get the sort value of.
	 * @return Page sort value.
	 */
	static SQInteger GetPageSortValue(StoryPageID story_page_id);

	/**
	 * Get story page element sort value. Each page element has a sort value that is internally
	 * assigned and used to sort the page elements within a page of the story book. OpenTTD
	 * maintains this number so that the sort order is perceived. This API exist only so that
	 * you can sort ScriptStoryPageList the same order as in GUI. You should not use this number
	 * for anything else.
	 * @param story_page_element_id The story page element to get the sort value of.
	 * @return Page element sort value.
	 */
	static SQInteger GetPageElementSortValue(StoryPageElementID story_page_element_id);

	/**
	 * Get the company which the page belongs to. If the page is global,
	 * ScriptCompany::COMPANY_INVALID is returned.
	 * @param story_page_id The story page to get the company for.
	 * @return owner company or ScriptCompany::COMPANY_INVALID
	 * @pre IsValidStoryPage(story_page_id).
	 */
	static ScriptCompany::CompanyID GetCompany(StoryPageID story_page_id);

	/**
	 * Get the page date which is displayed at the top of each page.
	 * @param story_page_id The story page to get the date of.
	 * @return The date
	 * @pre IsValidStoryPage(story_page_id).
	 */
	static ScriptDate::Date GetDate(StoryPageID story_page_id);

	/**
	 * Update date of a story page. The date is shown in the top left of the page
	 * @param story_page_id The story page to set the date for.
	 * @param date Date to display at the top of story page or ScriptDate::DATE_INVALID to disable showing date on this page. (also, @see ScriptDate)
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidStoryPage(story_page_id).
	 */
	static bool SetDate(StoryPageID story_page_id, ScriptDate::Date date);

	/**
	 * Update title of a story page. The title is shown in the page selector drop down.
	 * @param story_page_id The story page to update.
	 * @param title Page title (can be either a raw string, a ScriptText object, or null).
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidStoryPage(story_page_id).
	 */
	static bool SetTitle(StoryPageID story_page_id, Text *title);

	/**
	 * Opens the Story Book if not yet open and selects the given page.
	 * @param story_page_id The story page to update. If it is a global page, clients of all
	 * companies are affecetd. Otherwise only the clients of the company which the page belongs
	 * to are affected.
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidStoryPage(story_page_id).
	 */
	static bool Show(StoryPageID story_page_id);

	/**
	 * Remove a story page and all the page elements
	 * associated with it.
	 * @param story_page_id The story page to remove.
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidStoryPage(story_page_id).
	 */
	static bool Remove(StoryPageID story_page_id);

	/**
	 * Removes a story page element.
	 * @param story_page_element_id The story page element to remove.
	 * @return True if the action succeeded.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidStoryPageElement(story_page_element_id).
	 */
	static bool RemoveElement(StoryPageElementID story_page_element_id);

	/**
	 * Create a reference value for SPET_BUTTON_PUSH element parameters.
	 * @param colour The colour for the face of the button.
	 * @param flags The formatting and layout flags for the button.
	 * @return A reference value usable with the #NewElement and #UpdateElement functions.
	 */
	static StoryPageButtonFormatting MakePushButtonReference(StoryPageButtonColour colour, StoryPageButtonFlags flags);

	/**
	 * Create a reference value for SPET_BUTTON_TILE element parameters.
	 * @param colour The colour for the face of the button.
	 * @param flags The formatting and layout flags for the button.
	 * @param cursor The mouse cursor to use when the player clicks the button and the game is ready for the player to select a tile.
	 * @return A reference value usable with the #NewElement and #UpdateElement functions.
	 */
	static StoryPageButtonFormatting MakeTileButtonReference(StoryPageButtonColour colour, StoryPageButtonFlags flags, StoryPageButtonCursor cursor);

	/**
	 * Create a reference value for SPET_BUTTON_VEHICLE element parameters.
	 * @param colour  The colour for the face of the button.
	 * @param flags The formatting and layout flags for the button.
	 * @param cursor  The mouse cursor to use when the player clicks the button and the game is ready for the player to select a vehicle.
	 * @param vehtype The type of vehicle that will be selectable, or \c VT_INVALID to allow all types.
	 * @return A reference value usable with the #NewElement and #UpdateElement functions.
	 */
	static StoryPageButtonFormatting MakeVehicleButtonReference(StoryPageButtonColour colour, StoryPageButtonFlags flags, StoryPageButtonCursor cursor, ScriptVehicle::VehicleType vehtype);
};

#endif /* SCRIPT_STORY_HPP */

