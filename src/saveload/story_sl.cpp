/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file story_sl.cpp Code handling saving and loading of story pages */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/story_sl_compat.h"

#include "../story_base.h"

#include "../safeguards.h"

/** Called after load to trash broken pages. */
void AfterLoadStoryBook()
{
	if (IsSavegameVersionBefore(SLV_185)) {
		/* Trash all story pages and page elements because
		 * they were saved with wrong data types.
		 */
		_story_page_element_pool.CleanPool();
		_story_page_pool.CleanPool();
	}
}

static const SaveLoad _story_page_elements_desc[] = {
	SLE_CONDVAR(StoryPageElement, sort_value,    SLE_FILE_U16 | SLE_VAR_U32, SL_MIN_VERSION,   SLV_185),
	SLE_CONDVAR(StoryPageElement, sort_value,    SLE_UINT32,                 SLV_185, SL_MAX_VERSION),
	    SLE_VAR(StoryPageElement, page,          SLE_UINT16),
	SLE_CONDVAR(StoryPageElement, type,          SLE_FILE_U16 | SLE_VAR_U8,  SL_MIN_VERSION,   SLV_185),
	SLE_CONDVAR(StoryPageElement, type,          SLE_UINT8,                  SLV_185, SL_MAX_VERSION),
	    SLE_VAR(StoryPageElement, referenced_id, SLE_UINT32),
	   SLE_SSTR(StoryPageElement, text,          SLE_STR | SLF_ALLOW_CONTROL),
};

struct STPEChunkHandler : ChunkHandler {
	STPEChunkHandler() : ChunkHandler('STPE', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_story_page_elements_desc);

		for (StoryPageElement *s : StoryPageElement::Iterate()) {
			SlSetArrayIndex(s->index);
			SlObject(s, _story_page_elements_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_story_page_elements_desc, _story_page_elements_sl_compat);

		int index;
		uint32_t max_sort_value = 0;
		while ((index = SlIterateArray()) != -1) {
			StoryPageElement *s = new (index) StoryPageElement();
			SlObject(s, slt);
			if (s->sort_value > max_sort_value) {
				max_sort_value = s->sort_value;
			}
		}
		/* Update the next sort value, so that the next
		 * created page is shown after all existing pages.
		 */
		_story_page_element_next_sort_value = max_sort_value + 1;
	}
};

static const SaveLoad _story_pages_desc[] = {
	SLE_CONDVAR(StoryPage, sort_value, SLE_FILE_U16 | SLE_VAR_U32, SL_MIN_VERSION,   SLV_185),
	SLE_CONDVAR(StoryPage, sort_value, SLE_UINT32,                 SLV_185, SL_MAX_VERSION),
	    SLE_VAR(StoryPage, date,       SLE_UINT32),
	SLE_CONDVAR(StoryPage, company,    SLE_FILE_U16 | SLE_VAR_U8,  SL_MIN_VERSION,   SLV_185),
	SLE_CONDVAR(StoryPage, company,    SLE_UINT8,                  SLV_185, SL_MAX_VERSION),
	   SLE_SSTR(StoryPage, title,      SLE_STR | SLF_ALLOW_CONTROL),
};

struct STPAChunkHandler : ChunkHandler {
	STPAChunkHandler() : ChunkHandler('STPA', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_story_pages_desc);

		for (StoryPage *s : StoryPage::Iterate()) {
			SlSetArrayIndex(s->index);
			SlObject(s, _story_pages_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_story_pages_desc, _story_pages_sl_compat);

		int index;
		uint32_t max_sort_value = 0;
		while ((index = SlIterateArray()) != -1) {
			StoryPage *s = new (index) StoryPage();
			SlObject(s, slt);
			if (s->sort_value > max_sort_value) {
				max_sort_value = s->sort_value;
			}
		}
		/* Update the next sort value, so that the next
		 * created page is shown after all existing pages.
		 */
		_story_page_next_sort_value = max_sort_value + 1;
	}
};

static const STPEChunkHandler STPE;
static const STPAChunkHandler STPA;
static const ChunkHandlerRef story_page_chunk_handlers[] = {
	STPE,
	STPA,
};

extern const ChunkHandlerTable _story_page_chunk_handlers(story_page_chunk_handlers);
