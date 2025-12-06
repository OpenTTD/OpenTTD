/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dropdown.cpp Implementation of the dropdown widget. */

#include "stdafx.h"
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

static void ShowDropDownSubmenu(struct DropdownWindowBase *parent, const DropDownSubmenuItem &submenu, const Rect wi_rect, int item_index, bool drag_mode);

/** Drop-down menu window base */
struct DropdownWindowBase : Window {
	Window *callback; ///< Window to send events to.
	WidgetID parent_button{}; ///< Parent widget number where the window is dropped from.
	Rect wi_rect{}; ///< Rect of the button that opened the dropdown.
	Colours wi_colour;
	int selected_result = 0; ///< Result value of the selected item in the list.
	int selected_click_result = -1; ///< Click result value, from the OnClick handler of the selected item.
	uint8_t click_delay = 0; ///< Timer to delay selection.
	bool drag_mode = true;
	DropDownOptions options; ///< Options for this drop down menu.
	int scrolling = 0; ///< If non-zero, auto-scroll the item list (one time).
	Point position{}; ///< Position of the topleft corner of the window.
	Scrollbar *vscroll = nullptr;

	int submenu_index = -1;

	Dimension items_dim{}; ///< Calculated cropped and padded dimension for the items widget.

	/**
	 * Create a dropdown menu.
	 * @param parent        Parent window.
	 * @param selected      Initial selected result of the list.
	 * @param button        Widget of the parent window doing the dropdown.
	 * @param wi_rect       Rect of the button that opened the dropdown.
	 * @param wi_colour     Colour of the parent widget.
	 * @param options Drop Down options for this menu.
	 */
	DropdownWindowBase(Window *parent, Window *callback, int selected, WidgetID button, const Rect wi_rect, Colours wi_colour, DropDownOptions options)
			: Window(_dropdown_desc)
			, callback(callback)
			, parent_button(button)
			, wi_rect(wi_rect)
			, wi_colour(wi_colour)
			, selected_result(selected)
			, options(options)
	{
		this->parent = parent;

		this->CreateNestedTree();

		this->GetWidget<NWidgetCore>(WID_DM_ITEMS)->colour = wi_colour;
		this->GetWidget<NWidgetCore>(WID_DM_SCROLL)->colour = wi_colour;
		this->vscroll = this->GetScrollbar(WID_DM_SCROLL);
	}

	void Init(WindowNumber window_number)
	{
		this->UpdateSizeAndPosition();

		this->FinishInitNested(window_number);
		this->flags.Reset(WindowFlag::WhiteBorder);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		/* Finish closing the dropdown, so it doesn't affect new window placement.
		 * Also mark it dirty in case the callback deals with the screen. (e.g. screenshots). */
		this->Window::Close();

		Point pt = _cursor.pos;
		pt.x -= this->callback->left;
		pt.y -= this->callback->top;
		this->callback->OnDropdownClose(pt, this->parent_button, this->selected_result, this->selected_click_result, this->options.Test(DropDownOption::InstantClose));

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
		uint avg_height = list.height / (uint)this->GetList().size();
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
		Dimension list_dim = GetDropDownListDimension(this->GetList());

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

		this->position.x = GetInitalXPosition(button_rect, list_dim, widget_dim);

		this->items_dim = widget_dim;
		this->GetWidget<NWidgetStacked>(WID_DM_SHOW_SCROLL)->SetDisplayedPlane(list_dim.height > widget_dim.height ? 0 : SZSP_NONE);

		/* Capacity is the average number of items visible */
		this->vscroll->SetCapacity((widget_dim.height - WidgetDimensions::scaled.dropdownlist.Vertical()) * this->GetList().size() / list_dim.height);
		this->vscroll->SetCount(this->GetList().size());

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
		int y     = _cursor.pos.y - this->top - r.top;
		int pos   = this->vscroll->GetPosition();

		int item_index = 1;
		for (const auto &item : this->GetList()) {
			/* Skip items that are scrolled up */
			if (--pos >= 0) continue;

			int item_height = item->Height();

			if (y < item_height) {
				if (item->masked || !item->Selectable()) return false;

				auto submenu_item = dynamic_cast<const DropDownSubmenuItem *>(item.get());
				if (submenu_item != nullptr) {
					DropdownWindowBase *sub_window = static_cast<DropdownWindowBase *>(FindWindowById(WC_DROPDOWN_MENU, this->window_number + 1));

					/* Show proper submenu if it isn't already shown. */
					if (sub_window == nullptr || sub_window->submenu_index != item_index) {
						if (sub_window != nullptr) sub_window->Close();

						int item_y = _cursor.pos.y - this->top - y;
						Rect item_rect{r.left, item_y, r.right, item_y};
						ShowDropDownSubmenu(this, *submenu_item, item_rect, item_index, this->drag_mode);
					}

					if (this->selected_result != submenu_item->result) {
						this->selected_result = submenu_item->result;
						this->SetDirty();
					}

					return false;
				} else {
					CloseWindowById(WC_DROPDOWN_MENU, this->window_number + 1);

					result = item->result;
					click_result = item->OnClick(r.WithY(0, item_height - 1), {_cursor.pos.x - this->left, y});
					return true;
				}
			}

			y -= item_height;
			item_index++;
		}

		return false;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_DM_ITEMS) return;

		Colours colour = this->GetWidget<NWidgetCore>(widget)->colour;

		Rect ir = r.Shrink(WidgetDimensions::scaled.dropdownlist);
		int y = ir.top;
		int pos = this->vscroll->GetPosition();
		for (const auto &item : this->GetList()) {
			int item_height = item->Height();

			/* Skip items that are scrolled up */
			if (--pos >= 0) continue;

			if (y + item_height - 1 <= ir.bottom) {
				Rect full = ir.WithY(y, y + item_height - 1);

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
			if (!this->options.Test(DropDownOption::Persist)) {
				this->Close();
				/* If we are a sub-menu window, also close our parent window. */
				if (this->window_number > 0) CloseWindowById(WC_DROPDOWN_MENU, this->window_number - 1);
			}
			this->callback->OnDropdownSelect(this->parent_button, this->selected_result, this->selected_click_result);
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
				/* If some other window is above us, don't handle mouse movement. */
				if (FindWindowFromPt(_cursor.pos.x, _cursor.pos.y) != this) return;

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

protected:
	virtual const DropDownList &GetList() const = 0;

	virtual int GetInitalXPosition(const Rect &button_rect, const Dimension &list_dim, const Dimension &widget_dim) = 0;
};

/** Drop-down menu window */
struct DropdownWindow : public DropdownWindowBase {
	DropDownList list{}; ///< List with dropdown menu items.

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
			: DropdownWindowBase(parent, parent, selected, button, wi_rect, wi_colour, options)
			, list(std::move(list))
	{
		assert(!this->list.empty());

		this->Init(0);
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

protected:
	const DropDownList &GetList() const override { return this->list; }

	int GetInitalXPosition(const Rect &button_rect, const Dimension &list_dim, const Dimension &widget_dim) override
	{
		if (_current_text_dir == TD_RTL) {
			/* In case the list is wider than the parent button, the list should be right aligned to the button and overflow to the left. */
			return button_rect.right + 1 - (int)(widget_dim.width + (list_dim.height > widget_dim.height ? NWidgetScrollbar::GetVerticalDimension().width : 0));
		} else {
			return button_rect.left;
		}
	}
};

/** Drop-down sub-menu window */
struct DropdownSubmenuWindow : public DropdownWindowBase {
	const DropDownSubmenuItem &submenu;

	DropdownSubmenuWindow(DropdownWindowBase *parent, const DropDownSubmenuItem &submenu, const Rect wi_rect, int item_index, bool drag_mode)
			: DropdownWindowBase(parent, parent->callback, -1, parent->parent_button, wi_rect, parent->wi_colour, parent->options)
			, submenu(submenu)
	{
		this->submenu_index = item_index;
		this->drag_mode = drag_mode;

		this->Init(parent->window_number + 1);
	}

protected:
	const DropDownList &GetList() const override { return this->submenu.list; }

	int GetInitalXPosition(const Rect &button_rect, const Dimension &, const Dimension &widget_dim) override
	{
		if (_current_text_dir == TD_RTL) {
			/* Prefer to position the menu on the left. If this would cover more than a third of the parent button, position it to the right instead. */
			if (button_rect.left - (int)widget_dim.width >= 0 - button_rect.Width() / 3) {
				return std::max(0, button_rect.left - (int)widget_dim.width);
			} else {
				return std::min(button_rect.right, _screen.width - (int)widget_dim.width);
			}
		} else {
			/* Prefer to position the menu on the right. If this would cover more than a third of the parent button, position it to the left instead. */
			if (button_rect.right + (int)widget_dim.width <= _screen.width + button_rect.Width() / 3) {
				return std::min(button_rect.right, _screen.width - (int)widget_dim.width);
			} else {
				return std::max(0, button_rect.left - (int)widget_dim.width);
			}
		}
	}
};

void ShowDropDownSubmenu(DropdownWindowBase *parent, const DropDownSubmenuItem &submenu, const Rect wi_rect, int item_index, bool drag_mode)
{
	new DropdownSubmenuWindow(parent, submenu, wi_rect, item_index, drag_mode);
}

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
