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
	SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(StoryPageElement, sort_value), SaveLoadVersion::MinVersion, SaveLoadVersion::Storybooks),
	SaveLoad::Variable<VarFileType::U32>(SLE_NAME_AND_OBJECT_ADDRESS(StoryPageElement, sort_value), SaveLoadVersion::Storybooks),
	SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(StoryPageElement, page)),
	SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(StoryPageElement, type), SaveLoadVersion::MinVersion, SaveLoadVersion::Storybooks),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(StoryPageElement, type), SaveLoadVersion::Storybooks),
	SaveLoad::Variable<VarFileType::U32>(SLE_NAME_AND_OBJECT_ADDRESS(StoryPageElement, referenced_id)),
	SaveLoad::String(SLE_NAME_AND_OBJECT_ADDRESS(StoryPageElement, text), StringValidationSetting::AllowControlCode),
};

struct STPEChunkHandler : ChunkHandler {
	STPEChunkHandler() : ChunkHandler("STPE", ChunkType::Table) {}

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
	SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(StoryPage, sort_value), SaveLoadVersion::MinVersion, SaveLoadVersion::Storybooks),
	SaveLoad::Variable<VarFileType::U32>(SLE_NAME_AND_OBJECT_ADDRESS(StoryPage, sort_value), SaveLoadVersion::Storybooks),
	SaveLoad::Variable<VarFileType::U32>(SLE_NAME_AND_OBJECT_ADDRESS(StoryPage, date)),
	SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(StoryPage, company), SaveLoadVersion::MinVersion, SaveLoadVersion::Storybooks),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(StoryPage, company), SaveLoadVersion::Storybooks),
	SaveLoad::String(SLE_NAME_AND_OBJECT_ADDRESS(StoryPage, title), StringValidationSetting::AllowControlCode),
};

struct STPAChunkHandler : ChunkHandler {
	STPAChunkHandler() : ChunkHandler("STPA", ChunkType::Table) {}

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
