/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dropdown.cpp Implementation of the dropdown widget. */

#include "../stdafx.h"
#include "../window_gui.h"
#include "../string_func.h"
#include "../strings_func.h"
#include "../window_func.h"
#include "dropdown_type.h"


void DropDownListItem::Draw(int left, int right, int top, int bottom, bool sel, int bg_colour) const
{
	int c1 = _colour_gradient[bg_colour][3];
	int c2 = _colour_gradient[bg_colour][7];

	int mid = top + this->Height(0) / 2;
	GfxFillRect(left + 1, mid - 2, right - 1, mid - 2, c1);
	GfxFillRect(left + 1, mid - 1, right - 1, mid - 1, c2);
}

uint DropDownListStringItem::Width() const
{
	char buffer[512];
	GetString(buffer, this->String(), lastof(buffer));
	return GetStringBoundingBox(buffer).width;
}

void DropDownListStringItem::Draw(int left, int right, int top, int bottom, bool sel, int bg_colour) const
{
	DrawString(left + WD_FRAMERECT_LEFT, right - WD_FRAMERECT_RIGHT, top, this->String(), sel ? TC_WHITE : TC_BLACK);
}

/**
 * Natural sorting comparator function for DropDownList::sort().
 * @param first Left side of comparison.
 * @param second Right side of comparison.
 * @return true if \a first precedes \a second.
 * @warning All items in the list need to be derivates of DropDownListStringItem.
 */
/* static */ bool DropDownListStringItem::NatSortFunc(const DropDownListItem *first, const DropDownListItem *second)
{
	char buffer1[512], buffer2[512];
	GetString(buffer1, static_cast<const DropDownListStringItem*>(first)->String(), lastof(buffer1));
	GetString(buffer2, static_cast<const DropDownListStringItem*>(second)->String(), lastof(buffer2));
	return strnatcmp(buffer1, buffer2) < 0;
}

StringID DropDownListParamStringItem::String() const
{
	for (uint i = 0; i < lengthof(this->decode_params); i++) SetDParam(i, this->decode_params[i]);
	return this->string;
}

StringID DropDownListCharStringItem::String() const
{
	SetDParamStr(0, this->raw_string);
	return this->string;
}

/**
 * Delete all items of a drop down list and the list itself
 * @param list List to delete.
 */
static void DeleteDropDownList(DropDownList *list)
{
	for (DropDownList::iterator it = list->begin(); it != list->end(); ++it) {
		DropDownListItem *item = *it;
		delete item;
	}
	delete list;
}

/** Widget numbers of the dropdown menu. */
enum DropdownMenuWidgets {
	DDM_ITEMS,        ///< Panel showing the dropdown items.
	DDM_SHOW_SCROLL,  ///< Hide scrollbar if too few items.
	DDM_SCROLL,       ///< Scrollbar.
};

static const NWidgetPart _nested_dropdown_menu_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_END, DDM_ITEMS), SetMinimalSize(1, 1), SetScrollbar(DDM_SCROLL), EndContainer(),
		NWidget(NWID_SELECTION, INVALID_COLOUR, DDM_SHOW_SCROLL),
			NWidget(NWID_VSCROLLBAR, COLOUR_END, DDM_SCROLL),
		EndContainer(),
	EndContainer(),
};

const WindowDesc _dropdown_desc(
	WDP_MANUAL, 0, 0,
	WC_DROPDOWN_MENU, WC_NONE,
	0,
	_nested_dropdown_menu_widgets, lengthof(_nested_dropdown_menu_widgets)
);

/** Drop-down menu window */
struct DropdownWindow : Window {
	WindowClass parent_wnd_class; ///< Parent window class.
	WindowNumber parent_wnd_num;  ///< Parent window number.
	byte parent_button;           ///< Parent widget number where the window is dropped from.
	DropDownList *list;           ///< List with dropdown menu items.
	int selected_index;           ///< Index of the selected item in the list.
	byte click_delay;             ///< Timer to delay selection.
	bool drag_mode;
	bool instant_close;           ///< Close the window when the mouse button is raised.
	int scrolling;                ///< If non-zero, auto-scroll the item list (one time).
	Point position;               ///< Position of the topleft corner of the window.
	Scrollbar *vscroll;

	/**
	 * Create a dropdown menu.
	 * @param parent        Parent window.
	 * @param list          Dropdown item list.
	 * @param selected      Index of the selected item in the list.
	 * @param button        Widget of the parent window doing the dropdown.
	 * @param instant_close Close the window when the mouse button is raised.
	 * @param position      Topleft position of the dropdown menu window.
	 * @param size          Size of the dropdown menu window.
	 * @param wi_colour     Colour of the parent widget.
	 * @param scroll        Dropdown menu has a scrollbar.
	 * @param widget        Widgets of the dropdown menu window.
	 */
	DropdownWindow(Window *parent, DropDownList *list, int selected, int button, bool instant_close, const Point &position, const Dimension &size, Colours wi_colour, bool scroll) : Window()
	{
		this->position = position;

		this->CreateNestedTree(&_dropdown_desc);

		this->vscroll = this->GetScrollbar(DDM_SCROLL);

		uint items_width = size.width - (scroll ? WD_VSCROLLBAR_WIDTH : 0);
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(DDM_ITEMS);
		nwi->SetMinimalSize(items_width, size.height + 4);
		nwi->colour = wi_colour;

		nwi = this->GetWidget<NWidgetCore>(DDM_SCROLL);
		nwi->colour = wi_colour;

		this->GetWidget<NWidgetStacked>(DDM_SHOW_SCROLL)->SetDisplayedPlane(scroll ? 0 : SZSP_NONE);

		this->FinishInitNested(&_dropdown_desc, 0);
		this->flags4 &= ~WF_WHITE_BORDER_MASK;

		/* Total length of list */
		int list_height = 0;
		for (DropDownList::const_iterator it = list->begin(); it != list->end(); ++it) {
			DropDownListItem *item = *it;
			list_height += item->Height(items_width);
		}

		/* Capacity is the average number of items visible */
		this->vscroll->SetCapacity(size.height * (uint16)list->size() / list_height);
		this->vscroll->SetCount((uint16)list->size());

		this->parent_wnd_class = parent->window_class;
		this->parent_wnd_num   = parent->window_number;
		this->parent_button    = button;
		this->list             = list;
		this->selected_index   = selected;
		this->click_delay      = 0;
		this->drag_mode        = true;
		this->instant_close    = instant_close;
	}

	~DropdownWindow()
	{
		Window *w2 = FindWindowById(this->parent_wnd_class, this->parent_wnd_num);
		if (w2 != NULL) {
			if (w2->nested_array != NULL) {
				NWidgetCore *nwi2 = w2->GetWidget<NWidgetCore>(this->parent_button);
				if (nwi2->type == NWID_BUTTON_DROPDOWN) {
					nwi2->disp_flags &= ~ND_DROPDOWN_ACTIVE;
				} else {
					w2->RaiseWidget(this->parent_button);
				}
			} else {
				w2->RaiseWidget(this->parent_button);
			}
			w2->SetWidgetDirty(this->parent_button);
		}

		DeleteDropDownList(this->list);
	}

	virtual Point OnInitialPosition(const WindowDesc *desc, int16 sm_width, int16 sm_height, int window_number)
	{
		return this->position;
	}

	/**
	 * Find the dropdown item under the cursor.
	 * @param value [out] Selected item, if function returns \c true.
	 * @return Cursor points to a dropdown item.
	 */
	bool GetDropDownItem(int &value)
	{
		if (GetWidgetFromPos(this, _cursor.pos.x - this->left, _cursor.pos.y - this->top) < 0) return false;

		NWidgetBase *nwi = this->GetWidget<NWidgetBase>(DDM_ITEMS);
		int y     = _cursor.pos.y - this->top - nwi->pos_y - 2;
		int width = nwi->current_x - 4;
		int pos   = this->vscroll->GetPosition();

		const DropDownList *list = this->list;

		for (DropDownList::const_iterator it = list->begin(); it != list->end(); ++it) {
			/* Skip items that are scrolled up */
			if (--pos >= 0) continue;

			const DropDownListItem *item = *it;
			int item_height = item->Height(width);

			if (y < item_height) {
				if (item->masked || !item->Selectable()) return false;
				value = item->result;
				return true;
			}

			y -= item_height;
		}

		return false;
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != DDM_ITEMS) return;

		TextColour colour = (TextColour)this->GetWidget<NWidgetCore>(widget)->colour;

		int y = r.top + 2;
		int pos = this->vscroll->GetPosition();
		for (DropDownList::const_iterator it = this->list->begin(); it != this->list->end(); ++it) {
			const DropDownListItem *item = *it;
			int item_height = item->Height(r.right - r.left + 1);

			/* Skip items that are scrolled up */
			if (--pos >= 0) continue;

			if (y + item_height < r.bottom) {
				bool selected = (this->selected_index == item->result);
				if (selected) GfxFillRect(r.left + 2, y, r.right - 1, y + item_height - 1, PC_BLACK);

				item->Draw(r.left, r.right, y, r.bottom, selected, colour);

				if (item->masked) {
					GfxFillRect(r.left + 1, y, r.right - 1, y + item_height - 1, _colour_gradient[colour][5], FILLRECT_CHECKER);
				}
			}
			y += item_height;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		if (widget != DDM_ITEMS) return;
		int item;
		if (this->GetDropDownItem(item)) {
			this->click_delay = 4;
			this->selected_index = item;
			this->SetDirty();
		}
	}

	virtual void OnTick()
	{
		if (this->scrolling != 0) {
			int pos = this->vscroll->GetPosition();

			this->vscroll->UpdatePosition(this->scrolling);
			this->scrolling = 0;

			if (pos != this->vscroll->GetPosition()) {
				this->SetDirty();
			}
		}
	}

	virtual void OnMouseLoop()
	{
		Window *w2 = FindWindowById(this->parent_wnd_class, this->parent_wnd_num);
		if (w2 == NULL) {
			delete this;
			return;
		}

		if (this->click_delay != 0 && --this->click_delay == 0) {
			/* Make the dropdown "invisible", so it doesn't affect new window placement.
			 * Also mark it dirty in case the callback deals with the screen. (e.g. screenshots). */
			this->window_class = WC_INVALID;
			this->SetDirty();

			w2->OnDropdownSelect(this->parent_button, this->selected_index);
			delete this;
			return;
		}

		if (this->drag_mode) {
			int item;

			if (!_left_button_clicked) {
				this->drag_mode = false;
				if (!this->GetDropDownItem(item)) {
					if (this->instant_close) {
						/* Make the dropdown "invisible", so it doesn't affect new window placement.
						 * Also mark it dirty in case the callback deals with the screen. (e.g. screenshots). */
						this->window_class = WC_INVALID;
						this->SetDirty();

						if (GetWidgetFromPos(w2, _cursor.pos.x - w2->left, _cursor.pos.y - w2->top) == this->parent_button) {
							/* Send event for selected option if we're still
							 * on the parent button of the list. */
							w2->OnDropdownSelect(this->parent_button, this->selected_index);
						}
						delete this;
					}
					return;
				}
				this->click_delay = 2;
			} else {
				if (_cursor.pos.y <= this->top + 2) {
					/* Cursor is above the list, set scroll up */
					this->scrolling = -1;
					return;
				} else if (_cursor.pos.y >= this->top + this->height - 2) {
					/* Cursor is below list, set scroll down */
					this->scrolling = 1;
					return;
				}

				if (!this->GetDropDownItem(item)) return;
			}

			if (this->selected_index != item) {
				this->selected_index = item;
				this->SetDirty();
			}
		}
	}
};

void ShowDropDownList(Window *w, DropDownList *list, int selected, int button, uint width, bool auto_width, bool instant_close)
{
	DeleteWindowById(WC_DROPDOWN_MENU, 0);

	/* Our parent's button widget is used to determine where to place the drop
	 * down list window. */
	Rect wi_rect;
	Colours wi_colour;
	NWidgetCore *nwi = w->GetWidget<NWidgetCore>(button);
	wi_rect.left   = nwi->pos_x;
	wi_rect.right  = nwi->pos_x + nwi->current_x - 1;
	wi_rect.top    = nwi->pos_y;
	wi_rect.bottom = nwi->pos_y + nwi->current_y - 1;
	wi_colour = nwi->colour;

	if (nwi->type == NWID_BUTTON_DROPDOWN) {
		nwi->disp_flags |= ND_DROPDOWN_ACTIVE;
	} else {
		w->LowerWidget(button);
	}
	w->SetWidgetDirty(button);

	/* The preferred position is just below the dropdown calling widget */
	int top = w->top + wi_rect.bottom + 1;

	if (width == 0) width = wi_rect.right - wi_rect.left + 1;

	uint max_item_width = 0;

	if (auto_width) {
		/* Find the longest item in the list */
		for (DropDownList::const_iterator it = list->begin(); it != list->end(); ++it) {
			const DropDownListItem *item = *it;
			max_item_width = max(max_item_width, item->Width() + 5);
		}
	}

	/* Total length of list */
	int list_height = 0;

	for (DropDownList::const_iterator it = list->begin(); it != list->end(); ++it) {
		DropDownListItem *item = *it;
		list_height += item->Height(width);
	}

	/* Height of window visible */
	int height = list_height;

	/* Check if the status bar is visible, as we don't want to draw over it */
	int screen_bottom = GetMainViewBottom();
	bool scroll = false;

	/* Check if the dropdown will fully fit below the widget */
	if (top + height + 4 >= screen_bottom) {
		/* If not, check if it will fit above the widget */
		if (w->top + wi_rect.top - height > GetMainViewTop()) {
			top = w->top + wi_rect.top - height - 4;
		} else {
			/* ... and lastly if it won't, enable the scroll bar and fit the
			 * list in below the widget */
			int avg_height = list_height / (int)list->size();
			int rows = (screen_bottom - 4 - top) / avg_height;
			height = rows * avg_height;
			scroll = true;
			/* Add space for the scroll bar if we automatically determined
			 * the width of the list. */
			max_item_width += WD_VSCROLLBAR_WIDTH;
		}
	}

	if (auto_width) width = max(width, max_item_width);

	Point dw_pos = { w->left + (_current_text_dir == TD_RTL ? wi_rect.right + 1 - width : wi_rect.left), top};
	Dimension dw_size = {width, height};
	new DropdownWindow(w, list, selected, button, instant_close, dw_pos, dw_size, wi_colour, scroll);
}

/**
 * Show a dropdown menu window near a widget of the parent window.
 * The result code of the items is their index in the #strings list.
 * @param w             Parent window that wants the dropdown menu.
 * @param strings       Menu list, end with #INVALID_STRING_ID
 * @param selected      Index of initial selected item.
 * @param button        Button widget number of the parent window #w that wants the dropdown menu.
 * @param disabled_mask Bitmask for diabled items (items with their bit set are not copied to the dropdown list).
 * @param hidden_mask   Bitmask for hidden items (items with their bit set are displayed, but not selectable in the dropdown list).
 * @param width         Width of the dropdown menu. If \c 0, use the width of parent widget #button.
 */
void ShowDropDownMenu(Window *w, const StringID *strings, int selected, int button, uint32 disabled_mask, uint32 hidden_mask, uint width)
{
	DropDownList *list = new DropDownList();

	for (uint i = 0; strings[i] != INVALID_STRING_ID; i++) {
		if (!HasBit(hidden_mask, i)) {
			list->push_back(new DropDownListStringItem(strings[i], i, HasBit(disabled_mask, i)));
		}
	}

	/* No entries in the list? */
	if (list->size() == 0) {
		DeleteDropDownList(list);
		return;
	}

	ShowDropDownList(w, list, selected, button, width);
}

/**
 * Delete the drop-down menu from window \a pw
 * @param pw Parent window of the drop-down menu window
 * @return Parent widget number if the drop-down was found and closed, \c -1 if the window was not found.
 */
int HideDropDownMenu(Window *pw)
{
	Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class != WC_DROPDOWN_MENU) continue;

		DropdownWindow *dw = dynamic_cast<DropdownWindow*>(w);
		if (pw->window_class == dw->parent_wnd_class &&
				pw->window_number == dw->parent_wnd_num) {
			int parent_button = dw->parent_button;
			delete dw;
			return parent_button;
		}
	}

	return -1;
}

