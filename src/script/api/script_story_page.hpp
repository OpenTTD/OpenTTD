/* $Id$ */

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
	enum StoryPageElementType {
		SPET_TEXT = ::SPET_TEXT,         ///< An element that displays a block of text.
		SPET_LOCATION = ::SPET_LOCATION, ///< An element that displays a single line of text along with a button to view the referenced location.
		SPET_GOAL = ::SPET_GOAL,         ///< An element that displays a goal.
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
	 * @pre No ScriptCompanyMode may be in scope.
	 * @pre company == COMPANY_INVALID || ResolveCompanyID(company) != COMPANY_INVALID.
	 */
	static StoryPageID New(ScriptCompany::CompanyID company, Text *title);

	/**
	 * Create a new story page element.
	 * @param story_page_id The page id of the story page which the page element should be appended to.
	 * @param type Which page element type to create.
	 * @param reference A reference value to the object that is referred to by some page element types. When type is SPET_GOAL, this is the goal ID. When type is SPET_LOCATION, this is the TileIndex.
	 * @param text The body text of page elements that allow custom text. (SPET_TEXT and SPET_LOCATION)
	 * @return The new StoryPageElementID, or STORY_PAGE_ELEMENT_INVALID if it failed.
	 * @pre No ScriptCompanyMode may be in scope.
	 * @pre IsValidStoryPage(story_page).
	 * @pre (type != SPET_TEXT && type != SPET_LOCATION) || (text != nullptr && len(text) != 0).
	 * @pre type != SPET_LOCATION || ScriptMap::IsValidTile(reference).
	 * @pre type != SPET_GOAL || ScriptGoal::IsValidGoal(reference).
	 * @pre if type is SPET_GOAL and story_page is a global page, then referenced goal must be global.
	 */
	static StoryPageElementID NewElement(StoryPageID story_page_id, StoryPageElementType type, uint32 reference, Text *text);

	/**
	 * Update the content of a page element
	 * @param story_page_element_id The page id of the story page which the page element should be appended to.
	 * @param reference A reference value to the object that is referred to by some page element types. See also NewElement.
	 * @param text The body text of page elements that allow custom text. See also NewElement.
	 * @return True if the action succeeded.
	 * @pre No ScriptCompanyMode may be in scope.
	 * @pre IsValidStoryPage(story_page).
	 * @pre (type != SPET_TEXT && type != SPET_LOCATION) || (text != nullptr && len(text) != 0).
	 * @pre type != SPET_LOCATION || ScriptMap::IsValidTile(reference).
	 * @pre type != SPET_GOAL || ScriptGoal::IsValidGoal(reference).
	 * @pre if type is SPET_GOAL and story_page is a global page, then referenced goal must be global.
	 */
	static bool UpdateElement(StoryPageElementID story_page_element_id, uint32 reference, Text *text);

	/**
	 * Get story page sort value. Each page has a sort value that is internally assigned and used
	 * to sort the pages in the story book. OpenTTD maintains this number so that the sort order
	 * is perceived. This API exist only so that you can sort ScriptStoryPageList the same order
	 * as in GUI. You should not use this number for anything else.
	 * @param story_page_id The story page to get the sort value of.
	 * @return Page sort value.
	 */
	static uint32 GetPageSortValue(StoryPageID story_page_id);

	/**
	 * Get story page element sort value. Each page element has a sort value that is internally
	 * assigned and used to sort the page elements within a page of the story book. OpenTTD
	 * maintains this number so that the sort order is perceived. This API exist only so that
	 * you can sort ScriptStoryPageList the same order as in GUI. You should not use this number
	 * for anything else.
	 * @param story_page_element_id The story page element to get the sort value of.
	 * @return Page element sort value.
	 */
	static uint32 GetPageElementSortValue(StoryPageElementID story_page_element_id);

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
	 * @pre No ScriptCompanyMode may be in scope.
	 * @pre IsValidStoryPage(story_page_id).
	 */
	static bool SetDate(StoryPageID story_page_id, ScriptDate::Date date);

	/**
	 * Update title of a story page. The title is shown in the page selector drop down.
	 * @param story_page_id The story page to update.
	 * @param title Page title (can be either a raw string, a ScriptText object, or null).
	 * @return True if the action succeeded.
	 * @pre No ScriptCompanyMode may be in scope.
	 * @pre IsValidStoryPage(story_page_id).
	 */
	static bool SetTitle(StoryPageID story_page_id, Text *title);

	/**
	 * Opens the Story Book if not yet open and selects the given page.
	 * @param story_page_id The story page to update. If it is a global page, clients of all
	 * companies are affecetd. Otherwise only the clients of the company which the page belongs
	 * to are affected.
	 * @return True if the action succeeded.
	 * @pre No ScriptCompanyMode may be in scope.
	 * @pre IsValidStoryPage(story_page_id).
	 */
	static bool Show(StoryPageID story_page_id);

	/**
	 * Remove a story page and all the page elements
	 * associated with it.
	 * @param story_page_id The story page to remove.
	 * @return True if the action succeeded.
	 * @pre No ScriptCompanyMode may be in scope.
	 * @pre IsValidStoryPage(story_page_id).
	 */
	static bool Remove(StoryPageID story_page_id);

	/**
	 * Removes a story page element.
	 * @param story_page_element_id The story page element to remove.
	 * @return True if the action succeeded.
	 * @pre No ScriptCompanyMode may be in scope.
	 * @pre IsValidStoryPageElement(story_page_element_id).
	 */
	static bool RemoveElement(StoryPageElementID story_page_element_id);
};

#endif /* SCRIPT_STORY_HPP */

