/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file story_gui.cpp GUI for stories. */

#include "stdafx.h"
#include "window_gui.h"
#include "strings_func.h"
#include "gui.h"
#include "story_base.h"
#include "core/geometry_func.hpp"
#include "company_func.h"
#include "command_func.h"
#include "widgets/dropdown_type.h"
#include "widgets/dropdown_func.h"
#include "sortlist_type.h"
#include "goal_base.h"
#include "viewport_func.h"
#include "window_func.h"
#include "company_base.h"
#include "tilehighlight_func.h"
#include "vehicle_base.h"
#include "story_cmd.h"

#include "widgets/story_widget.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "safeguards.h"

static CursorID TranslateStoryPageButtonCursor(StoryPageButtonCursor cursor);

typedef GUIList<const StoryPage*> GUIStoryPageList;
typedef GUIList<const StoryPageElement*> GUIStoryPageElementList;

struct StoryBookWindow : Window {
protected:
	struct LayoutCacheElement {
		const StoryPageElement *pe;
		Rect bounds;
	};
	typedef std::vector<LayoutCacheElement> LayoutCache;

	enum class ElementFloat {
		None,
		Left,
		Right,
	};

	Scrollbar *vscroll;                ///< Scrollbar of the page text.
	mutable LayoutCache layout_cache;  ///< Cached element layout.

	GUIStoryPageList story_pages;      ///< Sorted list of pages.
	GUIStoryPageElementList story_page_elements; ///< Sorted list of page elements that belong to the current page.
	StoryPageID selected_page_id;      ///< Pool index of selected page.
	std::string selected_generic_title;  ///< If the selected page doesn't have a custom title, this buffer is used to store a generic page title.

	StoryPageElementID active_button_id; ///< Which button element the player is currently using

	static GUIStoryPageList::SortFunction * const page_sorter_funcs[];
	static GUIStoryPageElementList::SortFunction * const page_element_sorter_funcs[];

	/** (Re)Build story page list. */
	void BuildStoryPageList()
	{
		if (this->story_pages.NeedRebuild()) {
			this->story_pages.clear();

			for (const StoryPage *p : StoryPage::Iterate()) {
				if (this->IsPageAvailable(p)) {
					this->story_pages.push_back(p);
				}
			}

			this->story_pages.shrink_to_fit();
			this->story_pages.RebuildDone();
		}

		this->story_pages.Sort();
	}

	/** Sort story pages by order value. */
	static bool PageOrderSorter(const StoryPage * const &a, const StoryPage * const &b)
	{
		return a->sort_value < b->sort_value;
	}

	/** (Re)Build story page element list. */
	void BuildStoryPageElementList()
	{
		if (this->story_page_elements.NeedRebuild()) {
			this->story_page_elements.clear();

			const StoryPage *p = GetSelPage();
			if (p != nullptr) {
				for (const StoryPageElement *pe : StoryPageElement::Iterate()) {
					if (pe->page == p->index) {
						this->story_page_elements.push_back(pe);
					}
				}
			}

			this->story_page_elements.shrink_to_fit();
			this->story_page_elements.RebuildDone();
		}

		this->story_page_elements.Sort();
		this->InvalidateStoryPageElementLayout();
	}

	/** Sort story page elements by order value. */
	static bool PageElementOrderSorter(const StoryPageElement * const &a, const StoryPageElement * const &b)
	{
		return a->sort_value < b->sort_value;
	}

	/*
	 * Checks if a given page should be visible in the story book.
	 * @param page The page to check.
	 * @return True if the page should be visible, otherwise false.
	 */
	bool IsPageAvailable(const StoryPage *page) const
	{
		return page->company == INVALID_COMPANY || page->company == this->window_number;
	}

	/**
	 * Get instance of selected page.
	 * @return Instance of selected page or nullptr if no page is selected.
	 */
	StoryPage *GetSelPage() const
	{
		if (!_story_page_pool.IsValidID(selected_page_id)) return nullptr;
		return _story_page_pool.Get(selected_page_id);
	}

	/**
	 * Get the page number of selected page.
	 * @return Number of available pages before to the selected one, or -1 if no page is selected.
	 */
	int GetSelPageNum() const
	{
		int page_number = 0;
		for (const StoryPage *p : this->story_pages) {
			if (p->index == this->selected_page_id) {
				return page_number;
			}
			page_number++;
		}
		return -1;
	}

	/**
	 * Check if the selected page is also the first available page.
	 */
	bool IsFirstPageSelected()
	{
		/* Verify that the selected page exist. */
		if (!_story_page_pool.IsValidID(this->selected_page_id)) return false;

		return this->story_pages.front()->index == this->selected_page_id;
	}

	/**
	 * Check if the selected page is also the last available page.
	 */
	bool IsLastPageSelected()
	{
		/* Verify that the selected page exist. */
		if (!_story_page_pool.IsValidID(this->selected_page_id)) return false;

		if (this->story_pages.size() <= 1) return true;
		const StoryPage *last = this->story_pages.back();
		return last->index == this->selected_page_id;
	}

	/**
	 * Updates the content of selected page.
	 */
	void RefreshSelectedPage()
	{
		/* Generate generic title if selected page have no custom title. */
		StoryPage *page = this->GetSelPage();
		if (page != nullptr && page->title.empty()) {
			SetDParam(0, GetSelPageNum() + 1);
			selected_generic_title = GetString(STR_STORY_BOOK_GENERIC_PAGE_ITEM);
		}

		this->story_page_elements.ForceRebuild();
		this->BuildStoryPageElementList();

		if (this->active_button_id != INVALID_STORY_PAGE_ELEMENT) ResetObjectToPlace();

		this->vscroll->SetCount(this->GetContentHeight());
		this->SetWidgetDirty(WID_SB_SCROLLBAR);
		this->SetWidgetDirty(WID_SB_SEL_PAGE);
		this->SetWidgetDirty(WID_SB_PAGE_PANEL);
	}

	/**
	 * Selects the previous available page before the currently selected page.
	 */
	void SelectPrevPage()
	{
		if (!_story_page_pool.IsValidID(this->selected_page_id)) return;

		/* Find the last available page which is previous to the current selected page. */
		const StoryPage *last_available;
		last_available = nullptr;
		for (const StoryPage *p : this->story_pages) {
			if (p->index == this->selected_page_id) {
				if (last_available == nullptr) return; // No previous page available.
				this->SetSelectedPage(last_available->index);
				return;
			}
			last_available = p;
		}
	}

	/**
	 * Selects the next available page after the currently selected page.
	 */
	void SelectNextPage()
	{
		if (!_story_page_pool.IsValidID(this->selected_page_id)) return;

		/* Find selected page. */
		for (auto iter = this->story_pages.begin(); iter != this->story_pages.end(); iter++) {
			const StoryPage *p = *iter;
			if (p->index == this->selected_page_id) {
				/* Select the page after selected page. */
				iter++;
				if (iter != this->story_pages.end()) {
					this->SetSelectedPage((*iter)->index);
				}
				return;
			}
		}
	}

	/**
	 * Builds the page selector drop down list.
	 */
	DropDownList BuildDropDownList() const
	{
		DropDownList list;
		uint16_t page_num = 1;
		for (const StoryPage *p : this->story_pages) {
			bool current_page = p->index == this->selected_page_id;
			if (!p->title.empty()) {
				list.push_back(std::make_unique<DropDownListStringItem>(p->title, p->index, current_page));
			} else {
				/* No custom title => use a generic page title with page number. */
				SetDParam(0, page_num);
				list.push_back(std::make_unique<DropDownListStringItem>(STR_STORY_BOOK_GENERIC_PAGE_ITEM, p->index, current_page));
			}
			page_num++;
		}

		return list;
	}

	/**
	 * Get the width available for displaying content on the page panel.
	 */
	uint GetAvailablePageContentWidth() const
	{
		return this->GetWidget<NWidgetCore>(WID_SB_PAGE_PANEL)->current_x - WidgetDimensions::scaled.frametext.Horizontal() - 1;
	}

	/**
	 * Counts how many pixels of height that are used by Date and Title
	 * (excluding marginal after Title, as each body element has
	 * an empty row before the element).
	 * @param max_width Available width to display content.
	 * @return the height in pixels.
	 */
	uint GetHeadHeight(int max_width) const
	{
		StoryPage *page = this->GetSelPage();
		if (page == nullptr) return 0;
		int height = 0;

		/* Title lines */
		height += GetCharacterHeight(FS_NORMAL); // Date always use exactly one line.
		SetDParamStr(0, !page->title.empty() ? page->title : this->selected_generic_title);
		height += GetStringHeight(STR_STORY_BOOK_TITLE, max_width);

		return height;
	}

	/**
	 * Decides which sprite to display for a given page element.
	 * @param pe The page element.
	 * @return The SpriteID of the sprite to display.
	 * @pre pe.type must be SPET_GOAL or SPET_LOCATION.
	 */
	SpriteID GetPageElementSprite(const StoryPageElement &pe) const
	{
		switch (pe.type) {
			case SPET_GOAL: {
				Goal *g = Goal::Get((GoalID) pe.referenced_id);
				if (g == nullptr) return SPR_IMG_GOAL_BROKEN_REF;
				return g->completed ? SPR_IMG_GOAL_COMPLETED : SPR_IMG_GOAL;
			}
			case SPET_LOCATION:
				return SPR_IMG_VIEW_LOCATION;
			default:
				NOT_REACHED();
		}
	}

	/**
	 * Get the height in pixels used by a page element.
	 * @param pe The story page element.
	 * @param max_width Available width to display content.
	 * @return the height in pixels.
	 */
	uint GetPageElementHeight(const StoryPageElement &pe, int max_width) const
	{
		switch (pe.type) {
			case SPET_TEXT:
				SetDParamStr(0, pe.text);
				return GetStringHeight(STR_JUST_RAW_STRING, max_width);

			case SPET_GOAL:
			case SPET_LOCATION: {
				Dimension sprite_dim = GetSpriteSize(GetPageElementSprite(pe));
				return sprite_dim.height;
			}

			case SPET_BUTTON_PUSH:
			case SPET_BUTTON_TILE:
			case SPET_BUTTON_VEHICLE: {
				Dimension dim = GetStringBoundingBox(pe.text, FS_NORMAL);
				return dim.height + WidgetDimensions::scaled.framerect.Vertical() + WidgetDimensions::scaled.frametext.Vertical();
			}

			default:
				NOT_REACHED();
		}
		return 0;
	}

	/**
	 * Get the float style of a page element.
	 * @param pe The story page element.
	 * @return The float style.
	 */
	ElementFloat GetPageElementFloat(const StoryPageElement &pe) const
	{
		switch (pe.type) {
			case SPET_BUTTON_PUSH:
			case SPET_BUTTON_TILE:
			case SPET_BUTTON_VEHICLE: {
				StoryPageButtonFlags flags = StoryPageButtonData{ pe.referenced_id }.GetFlags();
				if (flags & SPBF_FLOAT_LEFT) return ElementFloat::Left;
				if (flags & SPBF_FLOAT_RIGHT) return ElementFloat::Right;
				return ElementFloat::None;
			}

			default:
				return ElementFloat::None;
		}
	}

	/**
	 * Get the width a page element would use if it was floating left or right.
	 * @param pe The story page element.
	 * @return The calculated width of the element.
	 */
	int GetPageElementFloatWidth(const StoryPageElement &pe) const
	{
		switch (pe.type) {
			case SPET_BUTTON_PUSH:
			case SPET_BUTTON_TILE:
			case SPET_BUTTON_VEHICLE: {
				Dimension dim = GetStringBoundingBox(pe.text, FS_NORMAL);
				return dim.width + WidgetDimensions::scaled.framerect.Vertical() + WidgetDimensions::scaled.frametext.Vertical();
			}

			default:
				NOT_REACHED(); // only buttons can float
		}
	}

	/** Invalidate the current page layout */
	void InvalidateStoryPageElementLayout()
	{
		this->layout_cache.clear();
	}

	/** Create the page layout if it is missing */
	void EnsureStoryPageElementLayout() const
	{
		/* Assume if the layout cache has contents it is valid */
		if (!this->layout_cache.empty()) return;

		StoryPage *page = this->GetSelPage();
		if (page == nullptr) return;
		int max_width = GetAvailablePageContentWidth();
		int element_dist = GetCharacterHeight(FS_NORMAL);

		/* Make space for the header */
		int main_y = GetHeadHeight(max_width) + element_dist;

		/* Current bottom of left/right column */
		int left_y = main_y;
		int right_y = main_y;
		/* Current width of left/right column, 0 indicates no content in column */
		int left_width = 0;
		int right_width = 0;
		/* Indexes into element cache for yet unresolved floats */
		std::vector<size_t> left_floats;
		std::vector<size_t> right_floats;

		/* Build layout */
		for (const StoryPageElement *pe : this->story_page_elements) {
			ElementFloat fl = this->GetPageElementFloat(*pe);

			if (fl == ElementFloat::None) {
				/* Verify available width */
				const int min_required_width = 10 * GetCharacterHeight(FS_NORMAL);
				int left_offset = (left_width == 0) ? 0 : (left_width + element_dist);
				int right_offset = (right_width == 0) ? 0 : (right_width + element_dist);
				if (left_offset + right_offset + min_required_width >= max_width) {
					/* Width of floats leave too little for main content, push down */
					main_y = std::max(main_y, left_y);
					main_y = std::max(main_y, right_y);
					left_width = right_width = 0;
					left_offset = right_offset = 0;
					/* Do not add element_dist here, to keep together elements which were supposed to float besides each other. */
				}
				/* Determine height */
				const int available_width = max_width - left_offset - right_offset;
				const int height = GetPageElementHeight(*pe, available_width);
				/* Check for button that needs extra margin */
				if (left_offset == 0 && right_offset == 0) {
					switch (pe->type) {
						case SPET_BUTTON_PUSH:
						case SPET_BUTTON_TILE:
						case SPET_BUTTON_VEHICLE:
							left_offset = right_offset = available_width / 5;
							break;
						default:
							break;
					}
				}
				/* Position element in main column */
				LayoutCacheElement ce{ pe, {} };
				ce.bounds.left = left_offset;
				ce.bounds.right = max_width - right_offset;
				ce.bounds.top = main_y;
				main_y += height;
				ce.bounds.bottom = main_y;
				this->layout_cache.push_back(ce);
				main_y += element_dist;
				/* Clear all floats */
				left_width = right_width = 0;
				left_y = right_y = main_y = std::max({main_y, left_y, right_y});
				left_floats.clear();
				right_floats.clear();
			} else {
				/* Prepare references to correct column */
				int &cur_width = (fl == ElementFloat::Left) ? left_width : right_width;
				int &cur_y = (fl == ElementFloat::Left) ? left_y : right_y;
				std::vector<size_t> &cur_floats = (fl == ElementFloat::Left) ? left_floats : right_floats;
				/* Position element */
				cur_width = std::max(cur_width, this->GetPageElementFloatWidth(*pe));
				LayoutCacheElement ce{ pe, {} };
				ce.bounds.left = (fl == ElementFloat::Left) ? 0 : (max_width - cur_width);
				ce.bounds.right = (fl == ElementFloat::Left) ? cur_width : max_width;
				ce.bounds.top = cur_y;
				cur_y += GetPageElementHeight(*pe, cur_width);
				ce.bounds.bottom = cur_y;
				cur_floats.push_back(this->layout_cache.size());
				this->layout_cache.push_back(ce);
				cur_y += element_dist;
				/* Update floats in column to all have the same width */
				for (size_t index : cur_floats) {
					LayoutCacheElement &ce = this->layout_cache[index];
					ce.bounds.left = (fl == ElementFloat::Left) ? 0 : (max_width - cur_width);
					ce.bounds.right = (fl == ElementFloat::Left) ? cur_width : max_width;
				}
			}
		}
	}

	/**
	 * Get the total height of the content displayed in this window.
	 * @return the height in pixels
	 */
	uint GetContentHeight()
	{
		this->EnsureStoryPageElementLayout();

		/* The largest bottom coordinate of any element is the height of the content */
		uint max_y = std::accumulate(this->layout_cache.begin(), this->layout_cache.end(), 0, [](uint max_y, const LayoutCacheElement &ce) -> uint { return std::max<uint>(max_y, ce.bounds.bottom); });

		return max_y;
	}

	/**
	 * Draws a page element that is composed of a sprite to the left and a single line of
	 * text after that. These page elements are generally clickable and are thus called
	 * action elements.
	 * @param y_offset Current y_offset which will get updated when this method has completed its drawing.
	 * @param width Width of the region available for drawing.
	 * @param line_height Height of one line of text.
	 * @param action_sprite The sprite to draw.
	 * @param string_id The string id to draw.
	 * @return the number of lines.
	 */
	void DrawActionElement(int &y_offset, int width, int line_height, SpriteID action_sprite, StringID string_id = STR_JUST_RAW_STRING) const
	{
		Dimension sprite_dim = GetSpriteSize(action_sprite);
		uint element_height = std::max(sprite_dim.height, (uint)line_height);

		uint sprite_top = y_offset + (element_height - sprite_dim.height) / 2;
		uint text_top = y_offset + (element_height - line_height) / 2;

		DrawSprite(action_sprite, PAL_NONE, 0, sprite_top);
		DrawString(sprite_dim.width + WidgetDimensions::scaled.frametext.left, width, text_top, string_id, TC_BLACK);

		y_offset += element_height;
	}

	/**
	 * Internal event handler for when a page element is clicked.
	 * @param pe The clicked page element.
	 */
	void OnPageElementClick(const StoryPageElement &pe)
	{
		switch (pe.type) {
			case SPET_TEXT:
				/* Do nothing. */
				break;

			case SPET_LOCATION:
				if (_ctrl_pressed) {
					ShowExtraViewportWindow((TileIndex)pe.referenced_id);
				} else {
					ScrollMainWindowToTile((TileIndex)pe.referenced_id);
				}
				break;

			case SPET_GOAL:
				ShowGoalsList((CompanyID)this->window_number);
				break;

			case SPET_BUTTON_PUSH:
				if (this->active_button_id != INVALID_STORY_PAGE_ELEMENT) ResetObjectToPlace();
				this->active_button_id = pe.index;
				this->SetTimeout();
				this->SetWidgetDirty(WID_SB_PAGE_PANEL);

				Command<CMD_STORY_PAGE_BUTTON>::Post(0, pe.index, 0);
				break;

			case SPET_BUTTON_TILE:
				if (this->active_button_id == pe.index) {
					ResetObjectToPlace();
					this->active_button_id = INVALID_STORY_PAGE_ELEMENT;
				} else {
					CursorID cursor = TranslateStoryPageButtonCursor(StoryPageButtonData{ pe.referenced_id }.GetCursor());
					SetObjectToPlaceWnd(cursor, PAL_NONE, HT_RECT, this);
					this->active_button_id = pe.index;
				}
				this->SetWidgetDirty(WID_SB_PAGE_PANEL);
				break;

			case SPET_BUTTON_VEHICLE:
				if (this->active_button_id == pe.index) {
					ResetObjectToPlace();
					this->active_button_id = INVALID_STORY_PAGE_ELEMENT;
				} else {
					CursorID cursor = TranslateStoryPageButtonCursor(StoryPageButtonData{ pe.referenced_id }.GetCursor());
					SetObjectToPlaceWnd(cursor, PAL_NONE, HT_VEHICLE, this);
					this->active_button_id = pe.index;
				}
				this->SetWidgetDirty(WID_SB_PAGE_PANEL);
				break;

			default:
				NOT_REACHED();
		}
	}

public:
	StoryBookWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_SB_SCROLLBAR);
		this->vscroll->SetStepSize(GetCharacterHeight(FS_NORMAL));

		/* Initialize page sort. */
		this->story_pages.SetSortFuncs(StoryBookWindow::page_sorter_funcs);
		this->story_pages.ForceRebuild();
		this->BuildStoryPageList();
		this->story_page_elements.SetSortFuncs(StoryBookWindow::page_element_sorter_funcs);
		/* story_page_elements will get built by SetSelectedPage */

		this->FinishInitNested(window_number);
		this->owner = (Owner)this->window_number;

		/* Initialize selected vars. */
		this->selected_generic_title.clear();
		this->selected_page_id = INVALID_STORY_PAGE;

		this->active_button_id = INVALID_STORY_PAGE_ELEMENT;

		this->OnInvalidateData(-1);
	}

	/**
	 * Updates the disabled state of the prev/next buttons.
	 */
	void UpdatePrevNextDisabledState()
	{
		this->SetWidgetDisabledState(WID_SB_PREV_PAGE, story_pages.empty() || this->IsFirstPageSelected());
		this->SetWidgetDisabledState(WID_SB_NEXT_PAGE, story_pages.empty() || this->IsLastPageSelected());
		this->SetWidgetDirty(WID_SB_PREV_PAGE);
		this->SetWidgetDirty(WID_SB_NEXT_PAGE);
	}

	/**
	 * Sets the selected page.
	 * @param page_index pool index of the page to select.
	 */
	void SetSelectedPage(uint16_t page_index)
	{
		if (this->selected_page_id != page_index) {
			if (this->active_button_id) ResetObjectToPlace();
			this->active_button_id = INVALID_STORY_PAGE_ELEMENT;
			this->selected_page_id = page_index;
			this->RefreshSelectedPage();
			this->UpdatePrevNextDisabledState();
		}
	}

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_SB_SEL_PAGE: {
				StoryPage *page = this->GetSelPage();
				SetDParamStr(0, page != nullptr && !page->title.empty() ? page->title : this->selected_generic_title);
				break;
			}
			case WID_SB_CAPTION:
				if (this->window_number == INVALID_COMPANY) {
					SetDParam(0, STR_STORY_BOOK_SPECTATOR_CAPTION);
				} else {
					SetDParam(0, STR_STORY_BOOK_CAPTION);
					SetDParam(1, this->window_number);
				}
				break;
		}
	}

	void OnPaint() override
	{
		/* Detect if content has changed height. This can happen if a
		 * multi-line text contains eg. {COMPANY} and that company is
		 * renamed.
		 */
		if (this->vscroll->GetCount() != this->GetContentHeight()) {
			this->vscroll->SetCount(this->GetContentHeight());
			this->SetWidgetDirty(WID_SB_SCROLLBAR);
			this->SetWidgetDirty(WID_SB_PAGE_PANEL);
		}

		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_SB_PAGE_PANEL) return;

		StoryPage *page = this->GetSelPage();
		if (page == nullptr) return;

		Rect fr = r.Shrink(WidgetDimensions::scaled.frametext);

		/* Set up a clipping region for the panel. */
		DrawPixelInfo tmp_dpi;
		if (!FillDrawPixelInfo(&tmp_dpi, fr)) return;

		AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);

		/* Draw content (now coordinates given to Draw** are local to the new clipping region). */
		fr = fr.Translate(-fr.left, -fr.top);
		int line_height = GetCharacterHeight(FS_NORMAL);
		const int scrollpos = this->vscroll->GetPosition();
		int y_offset = -scrollpos;

		/* Date */
		if (page->date != CalendarTime::INVALID_DATE) {
			SetDParam(0, page->date);
			DrawString(0, fr.right, y_offset, STR_JUST_DATE_LONG, TC_BLACK);
		}
		y_offset += line_height;

		/* Title */
		SetDParamStr(0, !page->title.empty() ? page->title : this->selected_generic_title);
		y_offset = DrawStringMultiLine(0, fr.right, y_offset, fr.bottom, STR_STORY_BOOK_TITLE, TC_BLACK, SA_TOP | SA_HOR_CENTER);

		/* Page elements */
		this->EnsureStoryPageElementLayout();
		for (const LayoutCacheElement &ce : this->layout_cache) {
			y_offset = ce.bounds.top - scrollpos;
			switch (ce.pe->type) {
				case SPET_TEXT:
					SetDParamStr(0, ce.pe->text);
					y_offset = DrawStringMultiLine(ce.bounds.left, ce.bounds.right, ce.bounds.top - scrollpos, ce.bounds.bottom - scrollpos, STR_JUST_RAW_STRING, TC_BLACK, SA_TOP | SA_LEFT);
					break;

				case SPET_GOAL: {
					Goal *g = Goal::Get((GoalID) ce.pe->referenced_id);
					StringID string_id = g == nullptr ? STR_STORY_BOOK_INVALID_GOAL_REF : STR_JUST_RAW_STRING;
					if (g != nullptr) SetDParamStr(0, g->text);
					DrawActionElement(y_offset, ce.bounds.right - ce.bounds.left, line_height, GetPageElementSprite(*ce.pe), string_id);
					break;
				}

				case SPET_LOCATION:
					SetDParamStr(0, ce.pe->text);
					DrawActionElement(y_offset, ce.bounds.right - ce.bounds.left, line_height, GetPageElementSprite(*ce.pe));
					break;

				case SPET_BUTTON_PUSH:
				case SPET_BUTTON_TILE:
				case SPET_BUTTON_VEHICLE: {
					const int tmargin = WidgetDimensions::scaled.bevel.top + WidgetDimensions::scaled.frametext.top;
					const FrameFlags frame = this->active_button_id == ce.pe->index ? FR_LOWERED : FR_NONE;
					const Colours bgcolour = StoryPageButtonData{ ce.pe->referenced_id }.GetColour();

					DrawFrameRect(ce.bounds.left, ce.bounds.top - scrollpos, ce.bounds.right, ce.bounds.bottom - scrollpos - 1, bgcolour, frame);

					SetDParamStr(0, ce.pe->text);
					DrawString(ce.bounds.left + WidgetDimensions::scaled.bevel.left, ce.bounds.right - WidgetDimensions::scaled.bevel.right, ce.bounds.top + tmargin - scrollpos, STR_JUST_RAW_STRING, TC_WHITE, SA_CENTER);
					break;
				}

				default: NOT_REACHED();
			}
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != WID_SB_SEL_PAGE && widget != WID_SB_PAGE_PANEL) return;

		Dimension d;
		d.height = GetCharacterHeight(FS_NORMAL);
		d.width = 0;

		switch (widget) {
			case WID_SB_SEL_PAGE: {

				/* Get max title width. */
				for (size_t i = 0; i < this->story_pages.size(); i++) {
					const StoryPage *s = this->story_pages[i];

					if (!s->title.empty()) {
						SetDParamStr(0, s->title);
					} else {
						SetDParamStr(0, this->selected_generic_title);
					}
					Dimension title_d = GetStringBoundingBox(STR_JUST_RAW_STRING);

					if (title_d.width > d.width) {
						d.width = title_d.width;
					}
				}

				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case WID_SB_PAGE_PANEL: {
				d.height *= 5;
				d.height += padding.height + WidgetDimensions::scaled.frametext.Vertical();
				*size = maxdim(*size, d);
				break;
			}
		}

	}

	void OnResize() override
	{
		this->InvalidateStoryPageElementLayout();
		this->vscroll->SetCapacityFromWidget(this, WID_SB_PAGE_PANEL, WidgetDimensions::scaled.frametext.Vertical());
		this->vscroll->SetCount(this->GetContentHeight());
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_SB_SEL_PAGE: {
				DropDownList list = this->BuildDropDownList();
				if (!list.empty()) {
					/* Get the index of selected page. */
					int selected = 0;
					for (size_t i = 0; i < this->story_pages.size(); i++) {
						const StoryPage *p = this->story_pages[i];
						if (p->index == this->selected_page_id) break;
						selected++;
					}

					ShowDropDownList(this, std::move(list), selected, widget);
				}
				break;
			}

			case WID_SB_PREV_PAGE:
				this->SelectPrevPage();
				break;

			case WID_SB_NEXT_PAGE:
				this->SelectNextPage();
				break;

			case WID_SB_PAGE_PANEL: {
				int clicked_y = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_SB_PAGE_PANEL, WidgetDimensions::scaled.frametext.top);
				this->EnsureStoryPageElementLayout();

				for (const LayoutCacheElement &ce : this->layout_cache) {
					if (clicked_y >= ce.bounds.top && clicked_y < ce.bounds.bottom && pt.x >= ce.bounds.left && pt.x < ce.bounds.right) {
						this->OnPageElementClick(*ce.pe);
						return;
					}
				}
			}
		}
	}

	void OnDropdownSelect(WidgetID widget, int index) override
	{
		if (widget != WID_SB_SEL_PAGE) return;

		/* index (which is set in BuildDropDownList) is the page id. */
		this->SetSelectedPage(index);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 *   -1     Rebuild page list and refresh current page;
	 *   >= 0   Id of the page that needs to be refreshed. If it is not the current page, nothing happens.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;

		/* If added/removed page, force rebuild. Sort order never change so just a
		 * re-sort is never needed.
		 */
		if (data == -1) {
			this->story_pages.ForceRebuild();
			this->BuildStoryPageList();

			/* Was the last page removed? */
			if (this->story_pages.empty()) {
				this->selected_generic_title.clear();
			}

			/* Verify page selection. */
			if (!_story_page_pool.IsValidID(this->selected_page_id)) {
				this->selected_page_id = INVALID_STORY_PAGE;
			}
			if (this->selected_page_id == INVALID_STORY_PAGE && !this->story_pages.empty()) {
				/* No page is selected, but there exist at least one available.
				 * => Select first page.
				 */
				this->SetSelectedPage(this->story_pages[0]->index);
			}

			this->SetWidgetDisabledState(WID_SB_SEL_PAGE, this->story_pages.empty());
			this->SetWidgetDirty(WID_SB_SEL_PAGE);
			this->UpdatePrevNextDisabledState();
		} else if (data >= 0 && this->selected_page_id == data) {
			this->RefreshSelectedPage();
		}
	}

	void OnTimeout() override
	{
		this->active_button_id = INVALID_STORY_PAGE_ELEMENT;
		this->SetWidgetDirty(WID_SB_PAGE_PANEL);
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		const StoryPageElement *const pe = StoryPageElement::GetIfValid(this->active_button_id);
		if (pe == nullptr || pe->type != SPET_BUTTON_TILE) {
			ResetObjectToPlace();
			this->active_button_id = INVALID_STORY_PAGE_ELEMENT;
			this->SetWidgetDirty(WID_SB_PAGE_PANEL);
			return;
		}

		Command<CMD_STORY_PAGE_BUTTON>::Post(tile, pe->index, 0);
		ResetObjectToPlace();
	}

	bool OnVehicleSelect(const Vehicle *v) override
	{
		const StoryPageElement *const pe = StoryPageElement::GetIfValid(this->active_button_id);
		if (pe == nullptr || pe->type != SPET_BUTTON_VEHICLE) {
			ResetObjectToPlace();
			this->active_button_id = INVALID_STORY_PAGE_ELEMENT;
			this->SetWidgetDirty(WID_SB_PAGE_PANEL);
			return false;
		}

		/* Check that the vehicle matches the requested type */
		StoryPageButtonData data{ pe->referenced_id };
		VehicleType wanted_vehtype = data.GetVehicleType();
		if (wanted_vehtype != VEH_INVALID && wanted_vehtype != v->type) return false;

		Command<CMD_STORY_PAGE_BUTTON>::Post(0, pe->index, v->index);
		ResetObjectToPlace();
		return true;
	}

	void OnPlaceObjectAbort() override
	{
		this->active_button_id = INVALID_STORY_PAGE_ELEMENT;
		this->SetWidgetDirty(WID_SB_PAGE_PANEL);
	}
};

GUIStoryPageList::SortFunction * const StoryBookWindow::page_sorter_funcs[] = {
	&PageOrderSorter,
};

GUIStoryPageElementList::SortFunction * const StoryBookWindow::page_element_sorter_funcs[] = {
	&PageElementOrderSorter,
};

static const NWidgetPart _nested_story_book_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_SB_CAPTION), SetDataTip(STR_JUST_STRING1, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, WID_SB_PAGE_PANEL), SetResize(1, 1), SetScrollbar(WID_SB_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_SB_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_SB_PREV_PAGE), SetMinimalSize(100, 0), SetFill(0, 0), SetDataTip(STR_STORY_BOOK_PREV_PAGE, STR_STORY_BOOK_PREV_PAGE_TOOLTIP),
		NWidget(NWID_BUTTON_DROPDOWN, COLOUR_BROWN, WID_SB_SEL_PAGE), SetMinimalSize(93, 12), SetFill(1, 0),
												SetDataTip(STR_JUST_RAW_STRING, STR_STORY_BOOK_SEL_PAGE_TOOLTIP), SetResize(1, 0),
		NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_SB_NEXT_PAGE), SetMinimalSize(100, 0), SetFill(0, 0), SetDataTip(STR_STORY_BOOK_NEXT_PAGE, STR_STORY_BOOK_NEXT_PAGE_TOOLTIP),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static WindowDesc _story_book_desc(__FILE__, __LINE__,
	WDP_CENTER, "view_story", 400, 300,
	WC_STORY_BOOK, WC_NONE,
	0,
	std::begin(_nested_story_book_widgets), std::end(_nested_story_book_widgets)
);

static CursorID TranslateStoryPageButtonCursor(StoryPageButtonCursor cursor)
{
	switch (cursor) {
		case SPBC_MOUSE:          return SPR_CURSOR_MOUSE;
		case SPBC_ZZZ:            return SPR_CURSOR_ZZZ;
		case SPBC_BUOY:           return SPR_CURSOR_BUOY;
		case SPBC_QUERY:          return SPR_CURSOR_QUERY;
		case SPBC_HQ:             return SPR_CURSOR_HQ;
		case SPBC_SHIP_DEPOT:     return SPR_CURSOR_SHIP_DEPOT;
		case SPBC_SIGN:           return SPR_CURSOR_SIGN;
		case SPBC_TREE:           return SPR_CURSOR_TREE;
		case SPBC_BUY_LAND:       return SPR_CURSOR_BUY_LAND;
		case SPBC_LEVEL_LAND:     return SPR_CURSOR_LEVEL_LAND;
		case SPBC_TOWN:           return SPR_CURSOR_TOWN;
		case SPBC_INDUSTRY:       return SPR_CURSOR_INDUSTRY;
		case SPBC_ROCKY_AREA:     return SPR_CURSOR_ROCKY_AREA;
		case SPBC_DESERT:         return SPR_CURSOR_DESERT;
		case SPBC_TRANSMITTER:    return SPR_CURSOR_TRANSMITTER;
		case SPBC_AIRPORT:        return SPR_CURSOR_AIRPORT;
		case SPBC_DOCK:           return SPR_CURSOR_DOCK;
		case SPBC_CANAL:          return SPR_CURSOR_CANAL;
		case SPBC_LOCK:           return SPR_CURSOR_LOCK;
		case SPBC_RIVER:          return SPR_CURSOR_RIVER;
		case SPBC_AQUEDUCT:       return SPR_CURSOR_AQUEDUCT;
		case SPBC_BRIDGE:         return SPR_CURSOR_BRIDGE;
		case SPBC_RAIL_STATION:   return SPR_CURSOR_RAIL_STATION;
		case SPBC_TUNNEL_RAIL:    return SPR_CURSOR_TUNNEL_RAIL;
		case SPBC_TUNNEL_ELRAIL:  return SPR_CURSOR_TUNNEL_ELRAIL;
		case SPBC_TUNNEL_MONO:    return SPR_CURSOR_TUNNEL_MONO;
		case SPBC_TUNNEL_MAGLEV:  return SPR_CURSOR_TUNNEL_MAGLEV;
		case SPBC_AUTORAIL:       return SPR_CURSOR_AUTORAIL;
		case SPBC_AUTOELRAIL:     return SPR_CURSOR_AUTOELRAIL;
		case SPBC_AUTOMONO:       return SPR_CURSOR_AUTOMONO;
		case SPBC_AUTOMAGLEV:     return SPR_CURSOR_AUTOMAGLEV;
		case SPBC_WAYPOINT:       return SPR_CURSOR_WAYPOINT;
		case SPBC_RAIL_DEPOT:     return SPR_CURSOR_RAIL_DEPOT;
		case SPBC_ELRAIL_DEPOT:   return SPR_CURSOR_ELRAIL_DEPOT;
		case SPBC_MONO_DEPOT:     return SPR_CURSOR_MONO_DEPOT;
		case SPBC_MAGLEV_DEPOT:   return SPR_CURSOR_MAGLEV_DEPOT;
		case SPBC_CONVERT_RAIL:   return SPR_CURSOR_CONVERT_RAIL;
		case SPBC_CONVERT_ELRAIL: return SPR_CURSOR_CONVERT_ELRAIL;
		case SPBC_CONVERT_MONO:   return SPR_CURSOR_CONVERT_MONO;
		case SPBC_CONVERT_MAGLEV: return SPR_CURSOR_CONVERT_MAGLEV;
		case SPBC_AUTOROAD:       return SPR_CURSOR_AUTOROAD;
		case SPBC_AUTOTRAM:       return SPR_CURSOR_AUTOTRAM;
		case SPBC_ROAD_DEPOT:     return SPR_CURSOR_ROAD_DEPOT;
		case SPBC_BUS_STATION:    return SPR_CURSOR_BUS_STATION;
		case SPBC_TRUCK_STATION:  return SPR_CURSOR_TRUCK_STATION;
		case SPBC_ROAD_TUNNEL:    return SPR_CURSOR_ROAD_TUNNEL;
		case SPBC_CLONE_TRAIN:    return SPR_CURSOR_CLONE_TRAIN;
		case SPBC_CLONE_ROADVEH:  return SPR_CURSOR_CLONE_ROADVEH;
		case SPBC_CLONE_SHIP:     return SPR_CURSOR_CLONE_SHIP;
		case SPBC_CLONE_AIRPLANE: return SPR_CURSOR_CLONE_AIRPLANE;
		case SPBC_DEMOLISH:       return ANIMCURSOR_DEMOLISH;
		case SPBC_LOWERLAND:      return ANIMCURSOR_LOWERLAND;
		case SPBC_RAISELAND:      return ANIMCURSOR_RAISELAND;
		case SPBC_PICKSTATION:    return ANIMCURSOR_PICKSTATION;
		case SPBC_BUILDSIGNALS:   return ANIMCURSOR_BUILDSIGNALS;
		default: return SPR_CURSOR_QUERY;
	}
}

/**
 * Raise or create the story book window for \a company, at page \a page_id.
 * @param company 'Owner' of the story book, may be #INVALID_COMPANY.
 * @param page_id Page to open, may be #INVALID_STORY_PAGE.
 */
void ShowStoryBook(CompanyID company, uint16_t page_id)
{
	if (!Company::IsValidID(company)) company = (CompanyID)INVALID_COMPANY;

	StoryBookWindow *w = AllocateWindowDescFront<StoryBookWindow>(&_story_book_desc, company, true);
	if (page_id != INVALID_STORY_PAGE) w->SetSelectedPage(page_id);
}
