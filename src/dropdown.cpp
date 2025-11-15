/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dropdown.cpp Implementation of the dropdown widget. */

#include "dropdown_type.h"
#include "stdafx.h"
#include "dropdown_func.h"
#include "strings_func.h"
#include "sound_func.h"
#include "timer/timer.h"
#include "timer/timer_window.h"
#include "window_gui.h"
#include "window_func.h"
#include "zoom_func.h"
#include "newgrf_badge_gui.h"

#include "widgets/dropdown_widget.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "core/geometry_func.hpp"

#include "dropdown_common_type.h"
#include "dropdown_gui.hpp"

#include "safeguards.h"

/**
 * Positive indexes are used to represent selected value.
 * -1 is used by default for all selected.
 * -2, -3, ... are used by other custom items.
 * -0xFF should be safe though.
 */
const int DROPDOWN_SORTER_ITEM_INDEX = -0xFF;

std::unique_ptr<DropDownListItem> MakeDropDownListStringItem(StringID str, int value, bool masked, bool shaded)
{
	return MakeDropDownListStringItem(GetString(str), value, masked, shaded);
}

std::unique_ptr<DropDownListItem> MakeDropDownListStringItem(std::string &&str, int value, bool masked, bool shaded)
{
	return std::make_unique<DropDownListStringItem>(std::move(str), value, masked, shaded);
}

std::unique_ptr<DropDownListItem> MakeDropDownListIconItem(SpriteID sprite, PaletteID palette, StringID str, int value, bool masked, bool shaded)
{
	return std::make_unique<DropDownListIconItem>(sprite, palette, GetString(str), value, masked, shaded);
}

std::unique_ptr<DropDownListItem> MakeDropDownListIconItem(const Dimension &dim, SpriteID sprite, PaletteID palette, StringID str, int value, bool masked, bool shaded)
{
	return std::make_unique<DropDownListIconItem>(dim, sprite, palette, GetString(str), value, masked, shaded);
}

std::unique_ptr<DropDownListItem> MakeDropDownListCheckedItem(bool checked, StringID str, int value, bool masked, bool shaded, uint indent)
{
	return std::make_unique<DropDownListCheckedItem>(indent, checked, GetString(str), value, masked, shaded);
}

static constexpr std::initializer_list<NWidgetPart> _nested_dropdown_menu_widgets = {
	NWidget(NWID_SELECTION, INVALID_COLOUR, WID_DM_SHOW_SORTER),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_END, WID_DM_SORT_ASCENDING_DESCENDING), SetStringTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
				NWidget(WWT_DROPDOWN, COLOUR_END, WID_DM_SORT_SUBDROPDOWN), SetResize(1, 0), SetFill(1, 0), SetToolTip(STR_TOOLTIP_SORT_CRITERIA),
				NWidget(WWT_IMGBTN, COLOUR_END, WID_DM_CONFIGURE), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON), SetResize(0, 0), SetFill(0, 1), SetSpriteTip(SPR_EXTRA_MENU, STR_BADGE_CONFIG_MENU_TOOLTIP),
			EndContainer(),
			NWidget(NWID_VERTICAL, NWidContainerFlag{}, WID_DM_BADGE_FILTER),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_END, WID_DM_ITEMS), SetScrollbar(WID_DM_SCROLL), EndContainer(),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_DM_SHOW_SCROLL),
			NWidget(NWID_VSCROLLBAR, COLOUR_END, WID_DM_SCROLL),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _dropdown_desc(
	WDP_MANUAL, {}, 0, 0,
	WC_DROPDOWN_MENU, WC_NONE,
	WindowDefaultFlag::NoFocus,
	_nested_dropdown_menu_widgets
);

/**
 * Create a dropdown menu.
 * @param window_id Id of the dropdown window.
 * @param parent Parent window.
 * @param list Dropdown item list.
 * @param selected Initial selected result of the list.
 * @param button Widget of the parent window doing the dropdown.
 * @param wi_rect Rect of the button that opened the dropdown.
 * @param wi_colour Colour of the parent widget.
 * @param options Dropdown options for this menu.
 * @param has_sorter Defines if the dropdown has widgets for sorting and filtering.
 */
DropdownWindow::DropdownWindow(int window_id, Window *parent, DropDownList &&list, int selected, WidgetID button, const Rect wi_rect, Colours wi_colour, DropDownOptions options, bool has_sorter)
	: Window(_dropdown_desc)
	, scroll_interval{std::chrono::milliseconds(30), [this](auto) {
		if (this->scrolling == 0) return;

		if (this->vscroll->UpdatePosition(this->scrolling)) this->SetDirty();

		this->scrolling = 0;
	}}
	, parent_button(button)
	, wi_rect(wi_rect)
	, list(std::move(list))
	, selected_result(selected)
	, options(options)
	, last_shift_state(_shift_pressed)
	, last_ctrl_state(_ctrl_pressed)
	, window_colour(wi_colour)
{
	this->parent = parent;
	this->window_number = window_id;

	this->CreateNestedTree();

	for (auto widget : {WID_DM_ITEMS, WID_DM_SCROLL, WID_DM_SORT_ASCENDING_DESCENDING, WID_DM_SORT_SUBDROPDOWN, WID_DM_CONFIGURE}) {
		this->GetWidget<NWidgetCore>(widget)->colour = wi_colour;
	}

	this->vscroll = this->GetScrollbar(WID_DM_SCROLL);

	if (!has_sorter) this->GetWidget<NWidgetStacked>(WID_DM_SHOW_SORTER)->SetDisplayedPlane(SZSP_HORIZONTAL);
}

/**
 * Create a dropdown menu.
 * @param window_id Id of the dropdown window.
 * @param parent Parent window.
 * @param list Dropdown item list.
 * @param selected Initial selected result of the list.
 * @param button Widget of the parent window doing the dropdown.
 * @param wi_rect Rect of the button that opened the dropdown.
 * @param wi_colour Colour of the parent widget.
 * @param options Dropdown options for this menu.
 */
DropdownWindow::DropdownWindow(int window_id, Window *parent, DropDownList &&list, int selected, WidgetID button, const Rect wi_rect, Colours wi_colour, DropDownOptions options)
	: DropdownWindow(window_id, parent, std::move(list), selected, button, wi_rect, wi_colour, options, false)
{
	this->FinishInitNested(window_id);
	this->flags.Reset(WindowFlag::WhiteBorder);
}

void DropdownWindow::OnInit()
{
	if (this->list.empty()) {
		this->list = this->GetDropdownList(this->badge_filter_choices);
	}

	assert(!this->list.empty());
	this->UpdateSizeAndPosition();

	this->badge_classes = GUIBadgeClasses(this->GetGrfSpecFeature());

	auto container = this->GetWidget<NWidgetContainer>(WID_DM_BADGE_FILTER);
	container->UnFocusWidgets(this);
	this->badge_filters = AddBadgeDropdownFilters(*container, WID_DM_BADGE_FILTER, window_colour, this->GetGrfSpecFeature());

	this->widget_lookup.clear();
	this->nested_root->FillWidgetLookup(this->widget_lookup);
}

void DropdownWindow::Close([[maybe_unused]] int data)
{
	/* Finish closing the dropdown, so it doesn't affect new window placement.
	 * Also mark it dirty in case the callback deals with the screen. (e.g. screenshots). */
	this->Window::Close();

	Point pt = _cursor.pos;
	pt.x -= this->parent->left;
	pt.y -= this->parent->top;
	this->parent->OnDropdownClose(pt, this->parent_button, this->selected_result, this->selected_click_result, this->options.Test(DropDownOption::InstantClose));

	/* Set flag on parent widget to indicate that we have just closed. */
	NWidgetCore *nwc = this->parent->GetWidget<NWidgetCore>(this->parent_button);
	if (nwc != nullptr) nwc->disp_flags.Set(NWidgetDisplayFlag::DropdownClosed);
}

void DropdownWindow::OnFocusLost(bool closing)
{
	if (closing) return;
	if (this->has_subdropdown_open) return;
	this->options.Reset(DropDownOption::InstantClose);
	this->Close();
}

/**
 * Fit dropdown list into available height, rounding to average item size. Width is adjusted if scrollbar is present.
 * @param[in,out] desired Desired dimensions of dropdown list.
 * @param list Dimensions of the list itself, without padding or cropping.
 * @param available_height Available height to fit list within.
 */
void DropdownWindow::FitAvailableHeight(Dimension &desired, const Dimension &list, uint available_height)
{
	if (desired.height < available_height) return;

	/* If the dropdown doesn't fully fit, we a need a dropdown. */
	uint avg_height = list.height / (uint)this->list.size();
	uint rows = std::max((available_height - WidgetDimensions::scaled.dropdownlist.Vertical()) / avg_height, 1U);

	desired.width = std::max(list.width, desired.width - NWidgetScrollbar::GetVerticalDimension().width);
	desired.height = rows * avg_height + WidgetDimensions::scaled.dropdownlist.Vertical();
}

/**
 * Update size and position of window to fit dropdown list into available space.
 */
void DropdownWindow::UpdateSizeAndPosition()
{
	Rect button_rect = this->wi_rect.Translate(this->parent->left, this->parent->top);

	/* Get the dimensions required for the list. */
	Dimension list_dim = GetDropDownListDimension(this->list);

	/* Set up dimensions for the items widget. */
	Dimension widget_dim = list_dim;
	widget_dim.width += WidgetDimensions::scaled.dropdownlist.Horizontal();
	widget_dim.height += WidgetDimensions::scaled.dropdownlist.Vertical();

	/* Width should match at least the width of the parent widget. */
	widget_dim.width = std::max<uint>(widget_dim.width, button_rect.Width());

	/* Available height below (or above, if the dropdown is placed above the widget). */
	uint available_height_below = std::max(GetMainViewBottom() - button_rect.bottom - 1, 0);
	uint available_height_above = std::max(button_rect.top - 1 - GetMainViewTop(), 0);

	/* Is it better to place the dropdown above the widget? */
	if (widget_dim.height > available_height_below && available_height_above > available_height_below) {
		FitAvailableHeight(widget_dim, list_dim, available_height_above);
		this->position.y = button_rect.top - widget_dim.height;
	} else {
		FitAvailableHeight(widget_dim, list_dim, available_height_below);
		this->position.y = button_rect.bottom + 1;
	}

	if (_current_text_dir == TD_RTL) {
		/* In case the list is wider than the parent button, the list should be right aligned to the button and overflow to the left. */
		this->position.x = button_rect.right + 1 - (int)(widget_dim.width + (list_dim.height > widget_dim.height ? NWidgetScrollbar::GetVerticalDimension().width : 0));
	} else {
		this->position.x = button_rect.left;
	}

	this->items_dim = widget_dim;
	this->GetWidget<NWidgetStacked>(WID_DM_SHOW_SCROLL)->SetDisplayedPlane(list_dim.height > widget_dim.height ? 0 : SZSP_NONE);

	/* Capacity is the average number of items visible */
	this->vscroll->SetCapacity((widget_dim.height - WidgetDimensions::scaled.dropdownlist.Vertical()) * this->list.size() / list_dim.height);
	this->vscroll->SetCount(this->list.size());

	/* If the dropdown is positioned above the parent widget, start selection at the bottom. */
	if (this->position.y < button_rect.top && list_dim.height > widget_dim.height) this->vscroll->UpdatePosition(INT_MAX);
}

void DropdownWindow::UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize)
{
	if (widget == WID_DM_ITEMS) size = this->items_dim;
	else if (widget == WID_DM_SORT_ASCENDING_DESCENDING) {
		Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->GetString());
		d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
		d.height += padding.height;
		size = maxdim(size, d);
	} else if (widget == WID_DM_CONFIGURE) {
		/* Hide the configuration button if no configurable badges are present. */
		if (this->badge_classes.GetClasses().empty()) size = {0, 0};
	}
}

Point DropdownWindow::OnInitialPosition([[maybe_unused]] int16_t sm_width, [[maybe_unused]] int16_t sm_height, [[maybe_unused]] int window_number)
{
	return this->position;
}

/**
 * Find the dropdown item under the cursor.
 * @param[out] result Selected item, if function returns \c true.
 * @param[out] click_result Click result from OnClick of Selected item, if function returns \c true.
 * @return Whether the cursor points to a dropdown item.
 */
bool DropdownWindow::GetDropdownItem(int &result, int &click_result)
{
	WidgetID widget = GetWidgetFromPos(this, _cursor.pos.x - this->left, _cursor.pos.y - this->top);
	if (widget < 0) return false;

	if (widget != WID_DM_ITEMS ) {
		if (widget == WID_DM_SORT_ASCENDING_DESCENDING || widget == WID_DM_SORT_SUBDROPDOWN || widget == WID_DM_CONFIGURE
			|| IsInsideMM(widget, this->badge_filters.first, this->badge_filters.second)) {
			result = DROPDOWN_SORTER_ITEM_INDEX;
			click_result = widget;
			return true;
		}
		return false;
	}

	const Rect &r = this->GetWidget<NWidgetBase>(WID_DM_ITEMS)->GetCurrentRect().Shrink(WidgetDimensions::scaled.dropdownlist).Shrink(WidgetDimensions::scaled.dropdowntext, RectPadding::zero);
	int y = _cursor.pos.y - this->top - r.top;
	int pos = this->vscroll->GetPosition();

	for (const auto &item : this->list) {
		/* Skip items that are scrolled up */
		if (--pos >= 0) continue;

		int item_height = item->Height();

		if (y < item_height) {
			if (item->masked || !item->Selectable()) return false;
			result = item->result;
			click_result = item->OnClick(r.WithY(0, item_height - 1), {_cursor.pos.x - this->left, y});
			return true;
		}

		y -= item_height;
	}

	return false;
}

void DropdownWindow::DrawWidget(const Rect &r, WidgetID widget) const
{
	if (widget == WID_DM_SORT_ASCENDING_DESCENDING) {
		this->DrawSortButtonState(WID_DM_SORT_ASCENDING_DESCENDING, this->IsSortOrderInverted() ? SBS_UP : SBS_DOWN);
		return;
	}

	if (widget != WID_DM_ITEMS) return;

	Colours colour = this->GetWidget<NWidgetCore>(widget)->colour;

	Rect ir = r.Shrink(WidgetDimensions::scaled.dropdownlist);
	int y = ir.top;
	int pos = this->vscroll->GetPosition();
	for (const auto &item : this->list) {
		int item_height = item->Height();

		/* Skip items that are scrolled up */
		if (--pos >= 0) continue;

		if (y + item_height - 1 <= ir.bottom) {
			Rect full = ir.WithY(y, y + item_height - 1);

			bool selected = (this->selected_result == item->result) && item->Selectable();
			if (selected) GfxFillRect(full, item->GetSelectedBGColour(colour));

			item->Draw(full, full.Shrink(WidgetDimensions::scaled.dropdowntext, RectPadding::zero), selected, selected ? this->selected_click_result : -1, colour);
		}
		y += item_height;
	}
}

void DropdownWindow::OnClick([[maybe_unused]] Point pt, [[maybe_unused]] WidgetID widget, [[maybe_unused]] int click_count)
{
	int result, click_result;
	if (this->GetDropdownItem(result, click_result)) {
		this->click_delay = 4;
		this->selected_result = result;
		this->selected_click_result = click_result;
		this->SetDirty();
	}
}

std::string DropdownWindow::GetWidgetString(WidgetID widget, StringID stringid) const
{
	if (widget == WID_DM_SORT_SUBDROPDOWN) {
		return GetString(this->GetSortCriteriaString());
	} else if (IsInsideMM(widget, this->badge_filters.first, this->badge_filters.second)) {
		return this->GetWidget<NWidgetBadgeFilter>(widget)->GetStringParameter(this->badge_filter_choices);
	}
	return this->Window::GetWidgetString(widget, stringid);
}

void DropdownWindow::OnDropdownSelect(WidgetID widget, int index, int click_result)
{
	switch (widget) {
		case WID_DM_SORT_SUBDROPDOWN: this->SetSortCriteria(index); break;
		case WID_DM_CONFIGURE:
			if (HandleBadgeConfigurationDropDownClick(this->GetGrfSpecFeature(), BADGE_COLUMNS, index, click_result, this->badge_filter_choices)) {
				ReplaceDropDownList(this, BuildBadgeClassConfigurationList(this->badge_classes, BADGE_COLUMNS, {}), -1);
			} else {
				this->CloseChildWindows(WC_DROPDOWN_MENU);
			}
			break;

		default:
			if (IsInsideMM(widget, this->badge_filters.first, this->badge_filters.second)) {
				if (index < 0) {
					ResetBadgeFilter(this->badge_filter_choices, this->GetWidget<NWidgetBadgeFilter>(widget)->GetBadgeClassID());
				} else {
					SetBadgeFilter(this->badge_filter_choices, BadgeID(index));
				}
			}
	}
	this->ReplaceList(this->GetDropdownList(this->badge_filter_choices), std::nullopt);
}

void DropdownWindow::OnDropdownClose(Point pt, WidgetID widget, int index, int click_result, bool instant_close)
{
	this->has_subdropdown_open = false;
	this->Window::OnDropdownClose(pt, widget, index, click_result, instant_close);
	SetFocusedWindow(this);
}

/**
 * Shows sub dropdown window bound to this dropdown.
 * @param widget Button for showing hiding the sub dropdown.
 * @param list Elements of the sub dropdown.
 * @param sub_dropdown_id Window id for the sub dropdown.
 * @param options Dropdown options for this menu.
 * @param selected_result Result when no item has been chosen.
 */
void DropdownWindow::ShowSubDropdownList(WidgetID widget, DropDownList &&list, int sub_dropdown_id, DropDownOptions options, int selected_result)
{
	NWidgetCore *nwi = this->GetWidget<NWidgetCore>(widget);
	nwi->SetLowered(true);
	this->has_subdropdown_open = true;
	SetFocusedWindow(ShowSubDropDownListAt(sub_dropdown_id, this, std::move(list), selected_result, widget, nwi->GetCurrentRect(), nwi->colour, options));
}

void DropdownWindow::OnMouseLoop()
{
	if (this->last_ctrl_state != _ctrl_pressed || this->last_shift_state != _shift_pressed) {
		/* Dropdown might contain an item with specified custom bg colours, allow it to update. */
		this->SetDirty();

		/* Also handle hiddable items. */
		this->ReInit(0, 0);
		this->InitializePositionSize(this->position.x, this->position.y, this->nested_root->smallest_x, this->nested_root->smallest_y);
		this->FindWindowPlacementAndResize(this->window_desc.GetDefaultWidth(), this->window_desc.GetDefaultHeight(), true);

		this->last_ctrl_state = _ctrl_pressed;
		this->last_shift_state = _shift_pressed;
	}

	if (this->click_delay != 0 && --this->click_delay == 0) {
		/* Close the dropdown, so it doesn't affect new window placement.
		 * Also mark it dirty in case the callback deals with the screen. (e.g. screenshots). */
		if (!this->options.Test(DropDownOption::Persist)) this->Close();
		if (this->selected_result == DROPDOWN_SORTER_ITEM_INDEX) {
			this->RaiseWidget(this->selected_click_result);
			switch (this->selected_click_result) {
				case WID_DM_SORT_ASCENDING_DESCENDING:
					this->SetSortOrderInverted(!this->IsSortOrderInverted());
					this->ReplaceList(this->GetDropdownList(this->badge_filter_choices), std::nullopt);
					break;

				case WID_DM_SORT_SUBDROPDOWN:
					this->ShowSubDropdownList(WID_DM_SORT_SUBDROPDOWN, this->GetSortDropdownList());
					break;

				case WID_DM_CONFIGURE:
					if (this->badge_classes.GetClasses().empty()) break;
					this->ShowSubDropdownList(WID_DM_CONFIGURE, BuildBadgeClassConfigurationList(this->badge_classes, BADGE_COLUMNS, {}), 1, {DropDownOption::Persist});
					break;

				default:
					this->ShowSubDropdownList(this->selected_click_result, this->GetWidget<NWidgetBadgeFilter>(this->selected_click_result)->GetDropDownList());
					break;
			}
			return;
		}
		this->parent->OnDropdownSelect(this->parent_button, this->selected_result, this->selected_click_result);
		return;
	}

	if (this->drag_mode) {
		int result, click_result;

		if (!_left_button_clicked) {
			this->drag_mode = false;
			if (!this->GetDropdownItem(result, click_result)) {
				if (this->options.Test(DropDownOption::InstantClose)) this->Close();
				return;
			}
			this->click_delay = 2;
		} else {
			if (_cursor.pos.y <= this->top + WidgetDimensions::scaled.dropdownlist.top) {
				/* Cursor is above the list, set scroll up */
				this->scrolling = -1;
				return;
			} else if (_cursor.pos.y >= this->top + this->height - WidgetDimensions::scaled.dropdownlist.bottom) {
				/* Cursor is below list, set scroll down */
				this->scrolling = 1;
				return;
			}

			if (!this->GetDropdownItem(result, click_result)) return;
		}

		if (this->selected_result != result || this->selected_click_result != click_result) {
			if (this->selected_result == DROPDOWN_SORTER_ITEM_INDEX) this->RaiseWidget(this->selected_click_result);
			if (result == DROPDOWN_SORTER_ITEM_INDEX) this->LowerWidget(click_result);
			this->selected_result = result;
			this->selected_click_result = click_result;
			this->SetDirty();
		}
	}
}

/**
 * Replaces the content with provided one.
 * If new constent is empty, uses return value of @see GetDropDownList instead.
 * @param list The new content.
 * @param selected_result Id of the selected item.
 */
void DropdownWindow::ReplaceList(DropDownList &&list, std::optional<int> selected_result)
{
	this->list = std::move(list);
	if (selected_result.has_value()) this->selected_result = *selected_result;
	this->ReInit(0, 0);
	this->InitializePositionSize(this->position.x, this->position.y, this->nested_root->smallest_x, this->nested_root->smallest_y);
	this->FindWindowPlacementAndResize(this->window_desc.GetDefaultWidth(), this->window_desc.GetDefaultHeight(), true);
	this->SetDirty();
}

/**
 * Determines string for currently selected sorting criteria.
 * @return StringID for sort criteria subdropdown.
 */
StringID DropdownWindow::GetSortCriteriaString() const
{
	return STR_EMPTY;
}

/**
 * Checks if the sort order is inverted.
 * @return true iff sort order is inverted.
 */
bool DropdownWindow::IsSortOrderInverted() const
{
	return false;
}

/**
 * Gets the new content for itself.
 * @return New dropdown list that should replace the current one.
 */
DropDownList DropdownWindow::GetDropdownList([[maybe_unused]] const BadgeFilterChoices &badge_filter_choices) const
{
	NOT_REACHED();
}

/**
 * Get the grf feature of the content of the dropdown, mainly used for badges.
 * @return grf feature of dropdown's content.
 */
GrfSpecFeature DropdownWindow::GetGrfSpecFeature() const
{
	return GSF_INVALID;
}

/**
 * Get the content for the sort criteria subdropdown.
 * @return New dropdown list that represents possible sort criteria.
 */
DropDownList DropdownWindow::GetSortDropdownList() const
{
	NOT_REACHED();
}

void ReplaceDropDownList(Window *parent, DropDownList &&list, std::optional<int> selected_result)
{
	ReplaceDropDownList<DropdownWindow>(parent, std::move(list), selected_result);
}

/**
 * Determine width and height required to fully display a DropDownList
 * @param list The list.
 * @return Dimension required to display the list.
 */
Dimension GetDropDownListDimension(const DropDownList &list)
{
	Dimension dim{};
	for (const auto &item : list) {
		dim.height += item->Height();
		dim.width = std::max(dim.width, item->Width());
	}
	dim.width += WidgetDimensions::scaled.dropdowntext.Horizontal();
	return dim;
}

/**
 * Show a drop down list.
 * @param w        Parent window for the list.
 * @param list     Prepopulated DropDownList.
 * @param selected The initially selected list item.
 * @param button   The widget which is passed to Window::OnDropdownSelect and OnDropdownClose.
 *                 Unless you override those functions, this should be then widget index of the dropdown button.
 * @param wi_rect  Coord of the parent drop down button, used to position the dropdown menu.
 * @param options Drop Down options for this menu.
 */
void ShowDropDownListAt(Window *w, DropDownList &&list, int selected, WidgetID button, Rect wi_rect, Colours wi_colour, DropDownOptions options)
{
	CloseWindowByClass(WC_DROPDOWN_MENU);
	new DropdownWindow(0, w, std::move(list), selected, button, wi_rect, wi_colour, options);
}

/**
 * Show a sub dropdown list.
 * @param sub_dropdown_id Id of the sub dropdown window.
 * @param w Parent window for the list.
 * @param list Prepopulated DropDownList.
 * @param selected The initially selected list item.
 * @param button The widget which is passed to Window::OnDropdownSelect and OnDropdownClose.
 * @param wi_rect Coord of the parent drop down button, used to position the dropdown menu.
 * @param options Dropdown options for this menu.
 * @return Pointer to the newly created window.
 */
Window *ShowSubDropDownListAt(int sub_dropdown_id, Window *w, DropDownList &&list, int selected, WidgetID button, Rect wi_rect, Colours wi_colour, DropDownOptions options)
{
	CloseWindowById(WC_DROPDOWN_MENU, sub_dropdown_id);
	return new DropdownWindow(sub_dropdown_id, w, std::move(list), selected, button, wi_rect, wi_colour, options);
}

/**
 * Show a drop down list.
 * @param w        Parent window for the list.
 * @param list     Prepopulated DropDownList.
 * @param selected The initially selected list item.
 * @param button   The widget within the parent window that is used to determine
 *                 the list's location.
 * @param width    Override the minimum width determined by the selected widget and list contents.
 * @param options Drop Down options for this menu.
 */
void ShowDropDownList(Window *w, DropDownList &&list, int selected, WidgetID button, uint width, DropDownOptions options)
{
	ShowCustomDropdownList<DropdownWindow>(w, std::move(list), selected, button, width, options);
}

/**
 * Show a dropdown menu window near a widget of the parent window.
 * The result code of the items is their index in the \a strings list.
 * @param w             Parent window that wants the dropdown menu.
 * @param strings       Menu list.
 * @param selected      Index of initial selected item.
 * @param button        Button widget number of the parent window \a w that wants the dropdown menu.
 * @param disabled_mask Bitmask for disabled items (items with their bit set are displayed, but not selectable in the dropdown list).
 * @param hidden_mask   Bitmask for hidden items (items with their bit set are not copied to the dropdown list).
 * @param width         Minimum width of the dropdown menu.
 */
void ShowDropDownMenu(Window *w, std::span<const StringID> strings, int selected, WidgetID button, uint32_t disabled_mask, uint32_t hidden_mask, uint width)
{
	DropDownList list;

	uint i = 0;
	for (auto string : strings) {
		if (!HasBit(hidden_mask, i)) {
			list.push_back(MakeDropDownListStringItem(string, i, HasBit(disabled_mask, i)));
		}
		++i;
	}

	if (!list.empty()) ShowDropDownList(w, std::move(list), selected, button, width, {});
}

