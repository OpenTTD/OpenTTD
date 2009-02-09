/* $Id$ */

/** @file dropdown.cpp Implementation of the dropdown widget. */

#include "../stdafx.h"
#include "../window_gui.h"
#include "../strings_func.h"
#include "../gfx_func.h"
#include "../window_func.h"
#include "../core/math_func.hpp"
#include "dropdown_type.h"

#include "table/strings.h"

void DropDownListItem::Draw(int x, int y, uint width, uint height, bool sel, int bg_colour) const
{
	int c1 = _colour_gradient[bg_colour][3];
	int c2 = _colour_gradient[bg_colour][7];

	GfxFillRect(x + 1, y + 3, x + width - 2, y + 3, c1);
	GfxFillRect(x + 1, y + 4, x + width - 2, y + 4, c2);
}

uint DropDownListStringItem::Width() const
{
	char buffer[512];
	GetString(buffer, this->String(), lastof(buffer));
	return GetStringBoundingBox(buffer).width;
}

void DropDownListStringItem::Draw(int x, int y, uint width, uint height, bool sel, int bg_colour) const
{
	DrawStringTruncated(x + 2, y, this->String(), sel ? TC_WHITE : TC_BLACK, width);
}

StringID DropDownListParamStringItem::String() const
{
	for (uint i = 0; i < lengthof(this->decode_params); i++) SetDParam(i, this->decode_params[i]);
	return this->string;
}

uint DropDownListCharStringItem::Width() const
{
	return GetStringBoundingBox(this->string).width;
}

void DropDownListCharStringItem::Draw(int x, int y, uint width, uint height, bool sel, int bg_colour) const
{
	DoDrawStringTruncated(this->string, x + 2, y, sel ? TC_WHITE : TC_BLACK, width);
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

static const Widget _dropdown_menu_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_END,     0, 0,     0, 0, 0x0, STR_NULL},
{  WWT_SCROLLBAR,   RESIZE_NONE,  COLOUR_END,     0, 0,     0, 0, 0x0, STR_0190_SCROLL_BAR_SCROLLS_LIST},
{   WIDGETS_END},
};

struct DropdownWindow : Window {
	WindowClass parent_wnd_class;
	WindowNumber parent_wnd_num;
	byte parent_button;
	DropDownList *list;
	int selected_index;
	byte click_delay;
	bool drag_mode;
	bool instant_close;
	int scrolling;

	DropdownWindow(int x, int y, int width, int height, const Widget *widget) : Window(x, y, width, height, WC_DROPDOWN_MENU, widget)
	{
		this->FindWindowPlacementAndResize(width, height);
	}

	~DropdownWindow()
	{
		Window *w2 = FindWindowById(this->parent_wnd_class, this->parent_wnd_num);
		if (w2 != NULL) {
			w2->RaiseWidget(this->parent_button);
			w2->InvalidateWidget(this->parent_button);
		}

		DeleteDropDownList(this->list);
	}

	bool GetDropDownItem(int &value)
	{
		if (GetWidgetFromPos(this, _cursor.pos.x - this->left, _cursor.pos.y - this->top) < 0) return false;

		int y     = _cursor.pos.y - this->top - 2;
		int width = this->widget[0].right - 3;
		int pos   = this->vscroll.pos;

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

	virtual void OnPaint()
	{
		this->DrawWidgets();

		int x = 1;
		int y = 2;

		int sel    = this->selected_index;
		int width  = this->widget[0].right - 2;
		int height = this->widget[0].bottom;
		int pos    = this->vscroll.pos;

		DropDownList *list = this->list;

		for (DropDownList::const_iterator it = list->begin(); it != list->end(); ++it) {
			const DropDownListItem *item = *it;
			int item_height = item->Height(width);

			/* Skip items that are scrolled up */
			if (--pos >= 0) continue;

			if (y + item_height < height) {
				if (sel == item->result) GfxFillRect(x + 1, y, x + width - 1, y + item_height - 1, 0);

				item->Draw(x, y, width, height, sel == item->result, (TextColour)this->widget[0].colour);

				if (item->masked) {
					GfxFillRect(x, y, x + width - 1, y + item_height - 1,
						_colour_gradient[this->widget[0].colour][5], FILLRECT_CHECKER
					);
				}
			}
			y += item_height;
		}
	};

	virtual void OnClick(Point pt, int widget)
	{
		if (widget != 0) return;
		int item;
		if (this->GetDropDownItem(item)) {
			this->click_delay = 4;
			this->selected_index = item;
			this->SetDirty();
		}
	}

	virtual void OnTick()
	{
		if (this->scrolling == -1) {
			this->vscroll.pos = max(0, this->vscroll.pos - 1);
			this->SetDirty();
		} else if (this->scrolling == 1) {
			this->vscroll.pos = min(this->vscroll.count - this->vscroll.cap, this->vscroll.pos + 1);
			this->SetDirty();
		}
		this->scrolling = 0;
	}

	virtual void OnMouseLoop()
	{
		Window *w2 = FindWindowById(this->parent_wnd_class, this->parent_wnd_num);
		if (w2 == NULL) {
			delete this;
			return;
		}

		if (this->click_delay != 0 && --this->click_delay == 0) {
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

			this->selected_index = item;
			this->SetDirty();
		}
	}
};

void ShowDropDownList(Window *w, DropDownList *list, int selected, int button, uint width, bool auto_width, bool instant_close)
{
	DeleteWindowById(WC_DROPDOWN_MENU, 0);

	w->LowerWidget(button);
	w->InvalidateWidget(button);

	/* Our parent's button widget is used to determine where to place the drop
	 * down list window. */
	const Widget *wi = &w->widget[button];

	/* The preferred position is just below the dropdown calling widget */
	int top = w->top + wi->bottom + 1;

	if (width == 0) width = wi->right - wi->left + 1;

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
	Window *w3 = FindWindowById(WC_STATUS_BAR, 0);
	int screen_bottom = w3 == NULL ? _screen.height : w3->top;

	bool scroll = false;

	/* Check if the dropdown will fully fit below the widget */
	if (top + height + 4 >= screen_bottom) {
		w3 = FindWindowById(WC_MAIN_TOOLBAR, 0);
		int screen_top = w3 == NULL ? 0 : w3->top + w3->height;

		/* If not, check if it will fit above the widget */
		if (w->top + wi->top - height > screen_top) {
			top = w->top + wi->top - height - 4;
		} else {
			/* ... and lastly if it won't, enable the scroll bar and fit the
			 * list in below the widget */
			int avg_height = list_height / (int)list->size();
			int rows = (screen_bottom - 4 - top) / avg_height;
			height = rows * avg_height;
			scroll = true;
			/* Add space for the scroll bar if we automatically determined
			 * the width of the list. */
			max_item_width += 12;
		}
	}

	if (auto_width) width = max(width, max_item_width);

	DropdownWindow *dw = new DropdownWindow(
		w->left + wi->left,
		top,
		width,
		height + 4,
		_dropdown_menu_widgets);

	dw->widget[0].colour = wi->colour;
	dw->widget[0].right = width - 1;
	dw->widget[0].bottom = height + 3;

	dw->SetWidgetHiddenState(1, !scroll);

	if (scroll) {
		/* We're scrolling, so enable the scroll bar and shrink the list by
		 * the scrollbar's width */
		dw->widget[1].colour = wi->colour;
		dw->widget[1].right  = dw->widget[0].right;
		dw->widget[1].left   = dw->widget[1].right - 11;
		dw->widget[1].bottom = dw->widget[0].bottom;
		dw->widget[0].right -= 12;

		/* Capacity is the average number of items visible */
		dw->vscroll.cap   = height * (uint16)list->size() / list_height;
		dw->vscroll.count = (uint16)list->size();
	}

	dw->desc_flags = WDF_DEF_WIDGET;
	dw->flags4 &= ~WF_WHITE_BORDER_MASK;

	dw->parent_wnd_class = w->window_class;
	dw->parent_wnd_num   = w->window_number;
	dw->parent_button    = button;
	dw->list             = list;
	dw->selected_index   = selected;
	dw->click_delay      = 0;
	dw->drag_mode        = true;
	dw->instant_close    = instant_close;
}

void ShowDropDownMenu(Window *w, const StringID *strings, int selected, int button, uint32 disabled_mask, uint32 hidden_mask, uint width)
{
	uint result = 0;
	DropDownList *list = new DropDownList();

	for (uint i = 0; strings[i] != INVALID_STRING_ID; i++) {
		if (!HasBit(hidden_mask, i)) {
			list->push_back(new DropDownListStringItem(strings[i], result, HasBit(disabled_mask, i)));
		}
		result++;
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

