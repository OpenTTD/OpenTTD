/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file story_sl.cpp Code handling saving and loading of story pages. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/story_sl_compat.h"

#include "../story_base.h"

#include "../safeguards.h"

/** Called after load to trash broken pages. */
void AfterLoadStoryBook()
{
	if (IsSavegameVersionBefore(SaveLoadVersion::Storybooks)) {
		/* Trash all story pages and page elements because
		 * they were saved with wrong data types.
		 */
		_story_page_element_pool.CleanPool();
		_story_page_pool.CleanPool();
	}
}

static const SaveLoad _story_page_elements_desc[] = {
	SLE_CONDVAR(StoryPageElement, sort_value, VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::Storybooks),
	SLE_CONDVAR(StoryPageElement, sort_value, VarTypes::U32, SaveLoadVersion::Storybooks, SaveLoadVersion::MaxVersion),
	    SLE_VAR(StoryPageElement, page,          VarTypes::U16),
	SLE_CONDVAR(StoryPageElement, type, VarFileType::U16 | VarMemType::U8, SaveLoadVersion::MinVersion, SaveLoadVersion::Storybooks),
	SLE_CONDVAR(StoryPageElement, type, VarTypes::U8, SaveLoadVersion::Storybooks, SaveLoadVersion::MaxVersion),
	    SLE_VAR(StoryPageElement, referenced_id, VarTypes::U32),
	   SLE_SSTR(StoryPageElement, text,          VarTypes::STR | StringValidationSetting::AllowControlCode),
};

struct STPEChunkHandler : ChunkHandler {
	STPEChunkHandler() : ChunkHandler('STPE', ChunkType::Table) {}

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
			StoryPageElement *s = StoryPageElement::CreateAtIndex(StoryPageElementID(index));
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
	SLE_CONDVAR(StoryPage, sort_value, VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::Storybooks),
	SLE_CONDVAR(StoryPage, sort_value, VarTypes::U32, SaveLoadVersion::Storybooks, SaveLoadVersion::MaxVersion),
	    SLE_VAR(StoryPage, date,       VarTypes::U32),
	SLE_CONDVAR(StoryPage, company, VarFileType::U16 | VarMemType::U8, SaveLoadVersion::MinVersion, SaveLoadVersion::Storybooks),
	SLE_CONDVAR(StoryPage, company, VarTypes::U8, SaveLoadVersion::Storybooks, SaveLoadVersion::MaxVersion),
	   SLE_SSTR(StoryPage, title,      VarTypes::STR | StringValidationSetting::AllowControlCode),
};

struct STPAChunkHandler : ChunkHandler {
	STPAChunkHandler() : ChunkHandler('STPA', ChunkType::Table) {}

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
			StoryPage *s = StoryPage::CreateAtIndex(StoryPageID(index));
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
