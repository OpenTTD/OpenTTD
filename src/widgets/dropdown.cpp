/* $Id$ */

/** @file dropdown.cpp Implementation of the dropdown widget. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../strings_type.h"
#include "../window_gui.h"
#include "../strings_func.h"
#include "../strings_type.h"
#include "../gfx_func.h"
#include "../window_func.h"
#include "../core/math_func.hpp"
#include "dropdown_type.h"
#include "dropdown_func.h"

#include "../table/sprites.h"
#include "table/strings.h"

StringID DropDownListItem::String() const
{
	return STR_NULL;
}

uint DropDownListItem::Height(uint width) const
{
	return 10;
}

StringID DropDownListStringItem::String() const
{
	return this->string;
}

StringID DropDownListParamStringItem::String() const
{
	for (uint i = 0; i < lengthof(this->decode_params); i++) SetDParam(i, this->decode_params[i]);
	return this->string;
}

void DropDownListItem::Draw(int x, int y, uint width, uint height, bool sel) const
{
	DrawStringTruncated(x + 2, y, this->String(), sel ? TC_WHITE : TC_BLACK, x + width);
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
{      WWT_PANEL,   RESIZE_NONE,     0,     0, 0,     0, 0, 0x0, STR_NULL},
{  WWT_SCROLLBAR,   RESIZE_NONE,     0,     0, 0,     0, 0, 0x0, STR_0190_SCROLL_BAR_SCROLLS_LIST},
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
	int scrolling;

	DropdownWindow(int x, int y, int width, int height, const Widget *widget) : Window(x, y, width, height, NULL, WC_DROPDOWN_MENU, widget)
	{
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

	int GetDropDownItem()
	{
		if (GetWidgetFromPos(this, _cursor.pos.x - this->left, _cursor.pos.y - this->top) < 0) return -1;

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
				if (item->masked || item->String() == STR_NULL) return -1;
				return item->result;
			}

			y -= item_height;
		}

		return -1;
	}

	virtual void OnPaint()
	{
		DrawWindowWidgets(this);

		int x = 1;
		int y = 2;

		int sel    = this->selected_index;
		int width  = this->widget[0].right - 3;
		int height = this->widget[0].bottom;
		int pos    = this->vscroll.pos;

		DropDownList *list = this->list;

		for (DropDownList::const_iterator it = list->begin(); it != list->end(); ++it) {
			const DropDownListItem *item = *it;
			int item_height = item->Height(width);

			/* Skip items that are scrolled up */
			if (--pos >= 0) continue;

			if (y + item_height < height) {
				if (item->String() != STR_NULL) {
					if (sel == item->result) GfxFillRect(x + 1, y, x + width, y + item_height - 1, 0);

					item->Draw(x, y, width, 10, sel == item->result);

					if (item->masked) {
						GfxFillRect(x, y, x + width, y + item_height - 1,
							(1 << PALETTE_MODIFIER_GREYOUT) | _colour_gradient[this->widget[0].color][5]
						);
					}
				} else {
					int c1 = _colour_gradient[this->widget[0].color][3];
					int c2 = _colour_gradient[this->widget[0].color][7];

					GfxFillRect(x + 1, y + 3, x + this->width - 5, y + 3, c1);
					GfxFillRect(x + 1, y + 4, x + this->width - 5, y + 4, c2);
				}
			}
			y += item_height;
		}
	};

	virtual void OnClick(Point pt, int widget)
	{
		if (widget != 0) return;
		int item = GetDropDownItem();
		if (item >= 0) {
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
			int item = GetDropDownItem();

			if (!_left_button_clicked) {
				this->drag_mode = false;
				if (item < 0) return;
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

				if (item < 0) return;
			}

			this->selected_index = item;
			this->SetDirty();
		}
	}
};

void ShowDropDownList(Window *w, DropDownList *list, int selected, int button, uint width)
{
	bool is_dropdown_menu_shown = w->IsWidgetLowered(button);

	DeleteWindowById(WC_DROPDOWN_MENU, 0);

	if (is_dropdown_menu_shown) {
		DeleteDropDownList(list);
		return;
	}

	w->LowerWidget(button);
	w->InvalidateWidget(button);

	/* Our parent's button widget is used to determine where to place the drop
	 * down list window. */
	const Widget *wi = &w->widget[button];

	/* The preferred position is just below the dropdown calling widget */
	int top = w->top + wi->bottom + 1;

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
			int avg_height = list_height / list->size();
			int rows = (screen_bottom - 4 - top) / avg_height;
			height = rows * avg_height;
			scroll = true;
		}
	}

	if (width == 0) width = wi->right - wi->left + 1;

	DropdownWindow *dw = new DropdownWindow(
		w->left + wi->left,
		top,
		width,
		height + 4,
		_dropdown_menu_widgets);

	dw->widget[0].color = wi->color;
	dw->widget[0].right = width - 1;
	dw->widget[0].bottom = height + 3;

	dw->SetWidgetHiddenState(1, !scroll);

	if (scroll) {
		/* We're scrolling, so enable the scroll bar and shrink the list by
		 * the scrollbar's width */
		dw->widget[1].color  = wi->color;
		dw->widget[1].right  = dw->widget[0].right;
		dw->widget[1].left   = dw->widget[1].right - 11;
		dw->widget[1].bottom = dw->widget[0].bottom;
		dw->widget[0].right -= 12;

		/* Capacity is the average number of items visible */
		dw->vscroll.cap   = height * list->size() / list_height;
		dw->vscroll.count = list->size();
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
}

void ShowDropDownMenu(Window *w, const StringID *strings, int selected, int button, uint32 disabled_mask, uint32 hidden_mask, uint width)
{
	/* Don't create a new list if we're just closing an existing menu */
	if (w->IsWidgetLowered(button)) {
		DeleteWindowById(WC_DROPDOWN_MENU, 0);
		return;
	}

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
void HideDropDownMenu(Window *pw)
{
	Window **wz;
	FOR_ALL_WINDOWS(wz) {
		if ((*wz)->window_class != WC_DROPDOWN_MENU) continue;

		DropdownWindow *dw = dynamic_cast<DropdownWindow*>(*wz);
		if (pw->window_class == dw->parent_wnd_class &&
				pw->window_number == dw->parent_wnd_num) {
			delete dw;
			break;
		}
	}
}

