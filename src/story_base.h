/* $Id$ */

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
#include "date_type.h"
#include "core/pool_type.hpp"

typedef Pool<StoryPageElement, StoryPageElementID, 64, 64000> StoryPageElementPool;
typedef Pool<StoryPage, StoryPageID, 64, 64000> StoryPagePool;
extern StoryPageElementPool _story_page_element_pool;
extern StoryPagePool _story_page_pool;
extern uint32 _story_page_element_next_sort_value;
extern uint32 _story_page_next_sort_value;

/*
 * Each story page element is one of these types.
 */
enum StoryPageElementType : byte {
	SPET_TEXT = 0, ///< A text element.
	SPET_LOCATION, ///< An element that references a tile along with a one-line text.
	SPET_GOAL,     ///< An element that references a goal.
	SPET_END,
	INVALID_SPET = 0xFF,
};

/** Define basic enum properties */
template <> struct EnumPropsT<StoryPageElementType> : MakeEnumPropsT<StoryPageElementType, byte, SPET_TEXT, SPET_END, INVALID_SPET, 8> {};

/**
 * Struct about story page elements.
 * Each StoryPage is composed of one or more page elements that provide
 * page content. Each element only contain one type of content.
 **/
struct StoryPageElement : StoryPageElementPool::PoolItem<&_story_page_element_pool> {
	uint32 sort_value;   ///< A number that increases for every created story page element. Used for sorting. The id of a story page element is the pool index.
	StoryPageID page; ///< Id of the page which the page element belongs to
	StoryPageElementType type; ///< Type of page element

	uint32 referenced_id; ///< Id of referenced object (location, goal etc.)
	char *text;           ///< Static content text of page element

	/**
	 * We need an (empty) constructor so struct isn't zeroed (as C++ standard states)
	 */
	inline StoryPageElement() { }

	/**
	 * (Empty) destructor has to be defined else operator delete might be called with nullptr parameter
	 */
	inline ~StoryPageElement() { free(this->text); }
};

#define FOR_ALL_STORY_PAGE_ELEMENTS_FROM(var, start) FOR_ALL_ITEMS_FROM(StoryPageElement, story_page_element_index, var, start)
#define FOR_ALL_STORY_PAGE_ELEMENTS(var) FOR_ALL_STORY_PAGE_ELEMENTS_FROM(var, 0)

/** Struct about stories, current and completed */
struct StoryPage : StoryPagePool::PoolItem<&_story_page_pool> {
	uint32 sort_value;   ///< A number that increases for every created story page. Used for sorting. The id of a story page is the pool index.
	Date date;           ///< Date when the page was created.
	CompanyByte company; ///< StoryPage is for a specific company; INVALID_COMPANY if it is global

	char *title;         ///< Title of story page

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
			StoryPageElement *spe;
			FOR_ALL_STORY_PAGE_ELEMENTS(spe) {
				if (spe->page == this->index) delete spe;
			}
		}
		free(this->title);
	}
};

#define FOR_ALL_STORY_PAGES_FROM(var, start) FOR_ALL_ITEMS_FROM(StoryPage, story_page_index, var, start)
#define FOR_ALL_STORY_PAGES(var) FOR_ALL_STORY_PAGES_FROM(var, 0)

#endif /* STORY_BASE_H */

