/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file story_page_sl.cpp Code handling saving and loading of story pages */

#include "../stdafx.h"
#include "../story_base.h"

#include "saveload.h"

static const SaveLoad _story_page_elements_desc[] = {
	    SLE_VAR(StoryPageElement, sort_value,    SLE_UINT16),
	    SLE_VAR(StoryPageElement, page,          SLE_UINT16),
	    SLE_VAR(StoryPageElement, type,          SLE_UINT16),
	    SLE_VAR(StoryPageElement, referenced_id, SLE_UINT32),
	    SLE_STR(StoryPageElement, text,          SLE_STR | SLF_ALLOW_CONTROL, 0),
	    SLE_END()
};

static void Save_STORY_PAGE_ELEMENT()
{
	StoryPageElement *s;
	FOR_ALL_STORY_PAGE_ELEMENTS(s) {
		SlSetArrayIndex(s->index);
		SlObject(s, _story_page_elements_desc);
	}
}

static void Load_STORY_PAGE_ELEMENT()
{
	int index;
	uint32 max_sort_value = 0;
	while ((index = SlIterateArray()) != -1) {
		StoryPageElement *s = new (index) StoryPageElement();
		SlObject(s, _story_page_elements_desc);
		if (s->sort_value > max_sort_value) {
			max_sort_value = s->sort_value;
		}
	}
	/* Update the next sort value, so that the next
	 * created page is shown after all existing pages.
	 */
	_story_page_element_next_sort_value = max_sort_value + 1;
}

static const SaveLoad _story_pages_desc[] = {
	    SLE_VAR(StoryPage, sort_value, SLE_UINT16),
	    SLE_VAR(StoryPage, date,       SLE_UINT32),
	    SLE_VAR(StoryPage, company,    SLE_UINT16),
	    SLE_STR(StoryPage, title,      SLE_STR | SLF_ALLOW_CONTROL, 0),
	    SLE_END()
};

static void Save_STORY_PAGE()
{
	StoryPage *s;
	FOR_ALL_STORY_PAGES(s) {
		SlSetArrayIndex(s->index);
		SlObject(s, _story_pages_desc);
	}
}

static void Load_STORY_PAGE()
{
	int index;
	uint32 max_sort_value = 0;
	while ((index = SlIterateArray()) != -1) {
		StoryPage *s = new (index) StoryPage();
		SlObject(s, _story_pages_desc);
		if (s->sort_value > max_sort_value) {
			max_sort_value = s->sort_value;
		}
	}
	/* Update the next sort value, so that the next
	 * created page is shown after all existing pages.
	 */
	_story_page_next_sort_value = max_sort_value + 1;
}

extern const ChunkHandler _story_page_chunk_handlers[] = {
	{ 'STPE', Save_STORY_PAGE_ELEMENT, Load_STORY_PAGE_ELEMENT, NULL, NULL, CH_ARRAY},
	{ 'STPA', Save_STORY_PAGE, Load_STORY_PAGE, NULL, NULL, CH_ARRAY | CH_LAST},
};
