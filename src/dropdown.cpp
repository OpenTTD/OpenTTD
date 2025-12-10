/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file dropdown.cpp Implementation of the dropdown widget. */

#include "stdafx.h"
#include "core/backup_type.hpp"
#include "dropdown_type.h"
#include "dropdown_func.h"
#include "strings_func.h"
#include "sound_func.h"
#include "timer/timer.h"
#include "timer/timer_window.h"
#include "window_gui.h"
#include "window_func.h"
#include "zoom_func.h"

#include "widgets/dropdown_widget.h"

#include "table/strings.h"

#include "dropdown_common_type.h"

#include "safeguards.h"

std::unique_ptr<DropDownListItem> MakeDropDownListDividerItem()
{
	return std::make_unique<DropDownListDividerItem>(-1);
}

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

/** Drop-down menu window */
struct DropdownWindow : Window {
	WidgetID parent_button{}; ///< Parent widget number where the window is dropped from.
	Rect wi_rect{}; ///< Rect of the button that opened the dropdown.
	DropDownList list{}; ///< List with dropdown menu items.
	int selected_result = 0; ///< Result value of the selected item in the list.
	int selected_click_result = -1; ///< Click result value, from the OnClick handler of the selected item.
	uint8_t click_delay = 0; ///< Timer to delay selection.
	bool drag_mode = true;
	DropDownOptions options; ///< Options for this drop down menu.
	int scrolling = 0; ///< If non-zero, auto-scroll the item list (one time).
	Point position{}; ///< Position of the topleft corner of the window.
	Scrollbar *vscroll = nullptr;

	Dimension items_dim{}; ///< Calculated cropped and padded dimension for the items widget.

	/**
	 * Create a dropdown menu.
	 * @param parent        Parent window.
	 * @param list          Dropdown item list.
	 * @param selected      Initial selected result of the list.
	 * @param button        Widget of the parent window doing the dropdown.
	 * @param wi_rect       Rect of the button that opened the dropdown.
	 * @param wi_colour     Colour of the parent widget.
	 * @param options Drop Down options for this menu.
	 */
	DropdownWindow(Window *parent, DropDownList &&list, int selected, WidgetID button, const Rect wi_rect, Colours wi_colour, DropDownOptions options)
			: Window(_dropdown_desc)
			, parent_button(button)
			, wi_rect(wi_rect)
			, list(std::move(list))
			, selected_result(selected)
			, options(options)
	{
		assert(!this->list.empty());

		this->parent = parent;

		this->CreateNestedTree();

		this->GetWidget<NWidgetCore>(WID_DM_ITEMS)->colour = wi_colour;
		this->GetWidget<NWidgetCore>(WID_DM_SCROLL)->colour = wi_colour;
		this->vscroll = this->GetScrollbar(WID_DM_SCROLL);
		this->UpdateSizeAndPosition();

		this->FinishInitNested(0);
		this->flags.Reset(WindowFlag::WhiteBorder);
	}

	void Close([[maybe_unused]] int data = 0) override
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

	void OnFocusLost(bool closing) override
	{
		if (!closing) {
			this->options.Reset(DropDownOption::InstantClose);
			this->Close();
		}
	}

	/**
	 * Fit dropdown list into available height, rounding to average item size. Width is adjusted if scrollbar is present.
	 * @param[in,out] desired Desired dimensions of dropdown list.
	 * @param list Dimensions of the list itself, without padding or cropping.
	 * @param available_height Available height to fit list within.
	 */
	void FitAvailableHeight(Dimension &desired, const Dimension &list, uint available_height)
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
	void UpdateSizeAndPosition()
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
		this->vscroll->SetCapacity(widget_dim.height - WidgetDimensions::scaled.dropdownlist.Vertical());
		this->vscroll->SetStepSize(list_dim.height / this->list.size());
		this->vscroll->SetCount(list_dim.height);

		/* If the dropdown is positioned above the parent widget, start selection at the bottom. */
		if (this->position.y < button_rect.top && list_dim.height > widget_dim.height) this->vscroll->UpdatePosition(INT_MAX);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget == WID_DM_ITEMS) size = this->items_dim;
	}

	Point OnInitialPosition([[maybe_unused]] int16_t sm_width, [[maybe_unused]] int16_t sm_height, [[maybe_unused]] int window_number) override
	{
		return this->position;
	}

	/**
	 * Find the dropdown item under the cursor.
	 * @param[out] result Selected item, if function returns \c true.
	 * @param[out] click_result Click result from OnClick of Selected item, if function returns \c true.
	 * @return Cursor points to a dropdown item.
	 */
	bool GetDropDownItem(int &result, int &click_result)
	{
		if (GetWidgetFromPos(this, _cursor.pos.x - this->left, _cursor.pos.y - this->top) < 0) return false;

		const Rect &r = this->GetWidget<NWidgetBase>(WID_DM_ITEMS)->GetCurrentRect().Shrink(WidgetDimensions::scaled.dropdownlist).Shrink(WidgetDimensions::scaled.dropdowntext, RectPadding::zero);
		int click_y = _cursor.pos.y - this->top - r.top;
		int y = -this->vscroll->GetPosition();
		int y_end = r.Height();

		for (const auto &item : this->list) {
			int item_height = item->Height();

			/* Skip items that are scrolled up */
			if (y > y_end) break;
			if (click_y >= y && click_y < y + item_height) {
				if (item->masked || !item->Selectable()) return false;
				result = item->result;
				click_result = item->OnClick(r.WithY(0, item_height - 1), {_cursor.pos.x - this->left, click_y - y});
				return true;
			}
			y += item_height;
		}

		return false;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_DM_ITEMS) return;

		Colours colour = this->GetWidget<NWidgetCore>(widget)->colour;

		Rect ir = r.Shrink(WidgetDimensions::scaled.dropdownlist);

		/* Setup a clipping rectangle... */
		DrawPixelInfo tmp_dpi;
		if (!FillDrawPixelInfo(&tmp_dpi, ir)) return;
		/* ...but keep coordinates relative to the window. */
		tmp_dpi.left += ir.left;
		tmp_dpi.top += ir.top;
		AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);

		int y = -this->vscroll->GetPosition();
		int y_end = ir.Height();

		for (const auto &item : this->list) {
			int item_height = item->Height();

			/* Skip items that are scrolled up */
			if (y > y_end) break;
			if (y > -item_height) {
				Rect full = ir.Translate(0, y).WithHeight(item_height);

				bool selected = (this->selected_result == item->result) && item->Selectable();
				if (selected) GfxFillRect(full, PC_BLACK);

				item->Draw(full, full.Shrink(WidgetDimensions::scaled.dropdowntext, RectPadding::zero), selected, selected ? this->selected_click_result : -1, colour);
			}
			y += item_height;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget != WID_DM_ITEMS) return;
		int result, click_result;
		if (this->GetDropDownItem(result, click_result)) {
			this->click_delay = 4;
			this->selected_result = result;
			this->selected_click_result = click_result;
			this->SetDirty();
		}
	}

	/** Rate limit how fast scrolling happens. */
	const IntervalTimer<TimerWindow> scroll_interval = {std::chrono::milliseconds(30), [this](auto) {
		if (this->scrolling == 0) return;

		if (this->vscroll->UpdatePosition(this->scrolling)) this->SetDirty();

		this->scrolling = 0;
	}};

	void OnMouseLoop() override
	{
		if (this->click_delay != 0 && --this->click_delay == 0) {
			/* Close the dropdown, so it doesn't affect new window placement.
			 * Also mark it dirty in case the callback deals with the screen. (e.g. screenshots). */
			if (!this->options.Test(DropDownOption::Persist)) this->Close();
			this->parent->OnDropdownSelect(this->parent_button, this->selected_result, this->selected_click_result);
			return;
		}

		if (this->drag_mode) {
			int result, click_result;

			if (!_left_button_clicked) {
				this->drag_mode = false;
				if (!this->GetDropDownItem(result, click_result)) {
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

				if (!this->GetDropDownItem(result, click_result)) return;
			}

			if (this->selected_result != result || this->selected_click_result != click_result) {
				this->selected_result = result;
				this->selected_click_result = click_result;
				this->SetDirty();
			}
		}
	}

	void ReplaceList(DropDownList &&list, std::optional<int> selected_result)
	{
		this->list = std::move(list);
		if (selected_result.has_value()) this->selected_result = *selected_result;
		this->UpdateSizeAndPosition();
		this->ReInit(0, 0);
		this->InitializePositionSize(this->position.x, this->position.y, this->nested_root->smallest_x, this->nested_root->smallest_y);
		this->FindWindowPlacementAndResize(this->window_desc.GetDefaultWidth(), this->window_desc.GetDefaultHeight(), true);
		this->SetDirty();
	}
};

void ReplaceDropDownList(Window *parent, DropDownList &&list, std::optional<int> selected_result)
{
	DropdownWindow *ddw = dynamic_cast<DropdownWindow *>(parent->FindChildWindow(WC_DROPDOWN_MENU));
	if (ddw != nullptr) ddw->ReplaceList(std::move(list), selected_result);
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
	new DropdownWindow(w, std::move(list), selected, button, wi_rect, wi_colour, options);
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
	/* Handle the beep of the player's click. */
	SndClickBeep();

	/* Our parent's button widget is used to determine where to place the drop
	 * down list window. */
	NWidgetCore *nwi = w->GetWidget<NWidgetCore>(button);
	Rect wi_rect      = nwi->GetCurrentRect();
	Colours wi_colour = nwi->colour;

	if ((nwi->type & WWT_MASK) == NWID_BUTTON_DROPDOWN) {
		nwi->disp_flags.Set(NWidgetDisplayFlag::DropdownActive);
	} else {
		nwi->SetLowered(true);
	}
	nwi->SetDirty(w);

	if (width != 0) {
		if (_current_text_dir == TD_RTL) {
			wi_rect.left = wi_rect.right + 1 - ScaleGUITrad(width);
		} else {
			wi_rect.right = wi_rect.left + ScaleGUITrad(width) - 1;
		}
	}

	ShowDropDownListAt(w, std::move(list), selected, button, wi_rect, wi_colour, options);
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
