/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file signs_gui.cpp The GUI for signs. */

#include "stdafx.h"
#include "company_gui.h"
#include "company_func.h"
#include "signs_base.h"
#include "signs_func.h"
#include "debug.h"
#include "command_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "map_func.h"
#include "gfx_func.h"
#include "viewport_func.h"
#include "querystring_gui.h"
#include "sortlist_type.h"
#include "string_func.h"

#include "table/strings.h"
#include "table/sprites.h"

struct SignList {
	typedef GUIList<const Sign *> GUISignList;

	static const Sign *last_sign;
	GUISignList signs;

	void BuildSignsList()
	{
		if (!this->signs.NeedRebuild()) return;

		DEBUG(misc, 3, "Building sign list");

		this->signs.Clear();

		const Sign *si;
		FOR_ALL_SIGNS(si) *this->signs.Append() = si;

		this->signs.Compact();
		this->signs.RebuildDone();
	}

	/** Sort signs by their name */
	static int CDECL SignNameSorter(const Sign * const *a, const Sign * const *b)
	{
		static char buf_cache[64];
		char buf[64];

		SetDParam(0, (*a)->index);
		GetString(buf, STR_SIGN_NAME, lastof(buf));

		if (*b != last_sign) {
			last_sign = *b;
			SetDParam(0, (*b)->index);
			GetString(buf_cache, STR_SIGN_NAME, lastof(buf_cache));
		}

		return strcasecmp(buf, buf_cache);
	}

	void SortSignsList()
	{
		if (!this->signs.Sort(&SignNameSorter)) return;

		/* Reset the name sorter sort cache */
		this->last_sign = NULL;
	}
};

const Sign *SignList::last_sign = NULL;

/** Enum referring to the widgets of the sign list window */
enum SignListWidgets {
	SLW_CAPTION,
	SLW_LIST,
	SLW_SCROLLBAR,
};

struct SignListWindow : Window, SignList {
	int text_offset; // Offset of the sign text relative to the left edge of the SLW_LIST widget.

	SignListWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);

		/* Create initial list. */
		this->signs.ForceRebuild();
		this->signs.ForceResort();
		this->BuildSignsList();
		this->SortSignsList();
		this->vscroll.SetCount(this->signs.Length());
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case SLW_LIST: {
				uint y = r.top + WD_FRAMERECT_TOP; // Offset from top of widget.
				/* No signs? */
				if (this->vscroll.GetCount() == 0) {
					DrawString(r.left + WD_FRAMETEXT_LEFT, r.right, y, STR_STATION_LIST_NONE);
					return;
				}

				bool rtl = _dynlang.text_dir == TD_RTL;
				int sprite_offset_y = (FONT_HEIGHT_NORMAL - 10) / 2 + 1;
				uint icon_left  = 4 + (rtl ? r.right - this->text_offset : r.left);
				uint text_left  = r.left + (rtl ? WD_FRAMERECT_LEFT : this->text_offset);
				uint text_right = r.right - (rtl ? this->text_offset : WD_FRAMERECT_RIGHT);

				/* At least one sign available. */
				for (uint16 i = this->vscroll.GetPosition(); this->vscroll.IsVisible(i) && i < this->vscroll.GetCount(); i++) {
					const Sign *si = this->signs[i];

					if (si->owner != OWNER_NONE) DrawCompanyIcon(si->owner, icon_left, y + sprite_offset_y);

					SetDParam(0, si->index);
					DrawString(text_left, text_right, y, STR_SIGN_NAME, TC_YELLOW);
					y += this->resize.step_height;
				}
				break;
			}
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == SLW_CAPTION) SetDParam(0, this->vscroll.GetCount());
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget == SLW_LIST) {
			uint id_v = (pt.y - this->GetWidget<NWidgetBase>(SLW_LIST)->pos_y - WD_FRAMERECT_TOP) / this->resize.step_height;

			if (id_v >= this->vscroll.GetCapacity()) return;
			id_v += this->vscroll.GetPosition();
			if (id_v >= this->vscroll.GetCount()) return;

			const Sign *si = this->signs[id_v];
			ScrollMainWindowToTile(TileVirtXY(si->x, si->y));
		}
	}

	virtual void OnResize()
	{
		this->vscroll.SetCapacity((this->GetWidget<NWidgetBase>(SLW_LIST)->current_y - WD_FRAMERECT_TOP - WD_FRAMERECT_BOTTOM) / this->resize.step_height);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case SLW_LIST: {
				Dimension spr_dim = GetSpriteSize(SPR_COMPANY_ICON);
				this->text_offset = WD_FRAMETEXT_LEFT + spr_dim.width + 2; // 2 pixels space between icon and the sign text.
				resize->height = max<uint>(FONT_HEIGHT_NORMAL, GetSpriteSize(SPR_COMPANY_ICON).height);
				Dimension d = {this->text_offset + MAX_LENGTH_SIGN_NAME_PIXELS + WD_FRAMETEXT_RIGHT, WD_FRAMERECT_TOP + 5 * resize->height + WD_FRAMERECT_BOTTOM};
				*size = maxdim(*size, d);
			} break;

			case SLW_CAPTION:
				SetDParam(0, max<uint>(1000, Sign::GetPoolSize()));
				*size = GetStringBoundingBox(STR_SIGN_LIST_CAPTION);
				size->height += padding.height;
				size->width  += padding.width;
				break;
		}
	}

	virtual void OnInvalidateData(int data)
	{
		if (data == 0) { // New or deleted sign.
			this->signs.ForceRebuild();
			this->BuildSignsList();
			this->SetWidgetDirty(SLW_CAPTION);
			this->vscroll.SetCount(this->signs.Length());
		} else { // Change of sign contents.
			this->signs.ForceResort();
		}

		this->SortSignsList();
	}
};

static const NWidgetPart _nested_sign_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, SLW_CAPTION), SetDataTip(STR_SIGN_LIST_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, SLW_LIST), SetMinimalSize(WD_FRAMETEXT_LEFT + 16 + MAX_LENGTH_SIGN_NAME_PIXELS + WD_FRAMETEXT_RIGHT, 50),
							SetResize(1, 10), SetFill(1, 0), EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_SCROLLBAR, COLOUR_GREY, SLW_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _sign_list_desc(
	WDP_AUTO, WDP_AUTO, 358, 138,
	WC_SIGN_LIST, WC_NONE,
	0,
	_nested_sign_list_widgets, lengthof(_nested_sign_list_widgets)
);


void ShowSignList()
{
	AllocateWindowDescFront<SignListWindow>(&_sign_list_desc, 0);
}

/**
 * Actually rename the sign.
 * @param index the sign to rename.
 * @param text  the new name.
 * @return true if the window will already be removed after returning.
 */
static bool RenameSign(SignID index, const char *text)
{
	bool remove = StrEmpty(text);
	DoCommandP(0, index, 0, CMD_RENAME_SIGN | (StrEmpty(text) ? CMD_MSG(STR_ERROR_CAN_T_DELETE_SIGN) : CMD_MSG(STR_ERROR_CAN_T_CHANGE_SIGN_NAME)), NULL, text);
	return remove;
}

/** Widget numbers of the query sign edit window. */
enum QueryEditSignWidgets {
	QUERY_EDIT_SIGN_WIDGET_CAPTION,
	QUERY_EDIT_SIGN_WIDGET_TEXT,
	QUERY_EDIT_SIGN_WIDGET_OK,
	QUERY_EDIT_SIGN_WIDGET_CANCEL,
	QUERY_EDIT_SIGN_WIDGET_DELETE,
	QUERY_EDIT_SIGN_WIDGET_PREVIOUS,
	QUERY_EDIT_SIGN_WIDGET_NEXT,
};

struct SignWindow : QueryStringBaseWindow, SignList {
	SignID cur_sign;

	SignWindow(const WindowDesc *desc, const Sign *si) : QueryStringBaseWindow(MAX_LENGTH_SIGN_NAME_BYTES)
	{
		this->caption = STR_EDIT_SIGN_CAPTION;
		this->afilter = CS_ALPHANUMERAL;

		this->InitNested(desc);

		this->LowerWidget(QUERY_EDIT_SIGN_WIDGET_TEXT);
		UpdateSignEditWindow(si);
		this->SetFocusedWidget(QUERY_EDIT_SIGN_WIDGET_TEXT);
	}

	void UpdateSignEditWindow(const Sign *si)
	{
		char *last_of = &this->edit_str_buf[this->edit_str_size - 1]; // points to terminating '\0'

		/* Display an empty string when the sign hasnt been edited yet */
		if (si->name != NULL) {
			SetDParam(0, si->index);
			GetString(this->edit_str_buf, STR_SIGN_NAME, last_of);
		} else {
			GetString(this->edit_str_buf, STR_EMPTY, last_of);
		}
		*last_of = '\0';

		this->cur_sign = si->index;
		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, MAX_LENGTH_SIGN_NAME_PIXELS);

		this->SetWidgetDirty(QUERY_EDIT_SIGN_WIDGET_TEXT);
		this->SetFocusedWidget(QUERY_EDIT_SIGN_WIDGET_TEXT);
	}

	/**
	 * Returns a pointer to the (alphabetically) previous or next sign of the current sign.
	 * @param next false if the previous sign is wanted, true if the next sign is wanted
	 * @return pointer to the previous/next sign
	 */
	const Sign *PrevNextSign(bool next)
	{
		/* Rebuild the sign list */
		this->signs.ForceRebuild();
		this->signs.NeedResort();
		this->BuildSignsList();
		this->SortSignsList();

		/* Search through the list for the current sign, excluding
		 * - the first sign if we want the previous sign or
		 * - the last sign if we want the next sign */
		uint end = this->signs.Length() - (next ? 1 : 0);
		for (uint i = next ? 0 : 1; i < end; i++) {
			if (this->cur_sign == this->signs[i]->index) {
				/* We've found the current sign, so return the sign before/after it */
				return this->signs[i + (next ? 1 : -1)];
			}
		}
		/* If we haven't found the current sign by now, return the last/first sign */
		return this->signs[next ? 0 : this->signs.Length() - 1];
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case QUERY_EDIT_SIGN_WIDGET_CAPTION:
				SetDParam(0, this->caption);
				break;
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
		this->DrawEditBox(QUERY_EDIT_SIGN_WIDGET_TEXT);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case QUERY_EDIT_SIGN_WIDGET_PREVIOUS:
			case QUERY_EDIT_SIGN_WIDGET_NEXT: {
				const Sign *si = this->PrevNextSign(widget == QUERY_EDIT_SIGN_WIDGET_NEXT);

				/* Rebuild the sign list */
				this->signs.ForceRebuild();
				this->signs.NeedResort();
				this->BuildSignsList();
				this->SortSignsList();

				/* Scroll to sign and reopen window */
				ScrollMainWindowToTile(TileVirtXY(si->x, si->y));
				UpdateSignEditWindow(si);
				break;
			}

			case QUERY_EDIT_SIGN_WIDGET_DELETE:
				/* Only need to set the buffer to null, the rest is handled as the OK button */
				RenameSign(this->cur_sign, "");
				/* don't delete this, we are deleted in Sign::~Sign() -> DeleteRenameSignWindow() */
				break;

			case QUERY_EDIT_SIGN_WIDGET_OK:
				if (RenameSign(this->cur_sign, this->text.buf)) break;
				/* FALL THROUGH */

			case QUERY_EDIT_SIGN_WIDGET_CANCEL:
				delete this;
				break;
		}
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		EventState state = ES_NOT_HANDLED;
		switch (this->HandleEditBoxKey(QUERY_EDIT_SIGN_WIDGET_TEXT, key, keycode, state)) {
			default: break;

			case HEBR_CONFIRM:
				if (RenameSign(this->cur_sign, this->text.buf)) break;
				/* FALL THROUGH */

			case HEBR_CANCEL: // close window, abandon changes
				delete this;
				break;
		}
		return state;
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(QUERY_EDIT_SIGN_WIDGET_TEXT);
	}

	virtual void OnOpenOSKWindow(int wid)
	{
		ShowOnScreenKeyboard(this, wid, QUERY_EDIT_SIGN_WIDGET_CANCEL, QUERY_EDIT_SIGN_WIDGET_OK);
	}
};

static const NWidgetPart _nested_query_sign_edit_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_CAPTION), SetDataTip(STR_WHITE_STRING, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(WWT_EDITBOX, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_TEXT), SetMinimalSize(256, 12), SetDataTip(STR_EDIT_SIGN_SIGN_OSKTITLE, STR_NULL), SetPadding(2, 2, 2, 2),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_OK), SetMinimalSize(61, 12), SetDataTip(STR_BUTTON_OK, STR_NULL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_CANCEL), SetMinimalSize(60, 12), SetDataTip(STR_BUTTON_CANCEL, STR_NULL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_DELETE), SetMinimalSize(60, 12), SetDataTip(STR_TOWN_VIEW_DELETE_BUTTON, STR_NULL),
		NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1), EndContainer(),
		NWidget(NWID_BUTTON_ARROW, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_PREVIOUS), SetMinimalSize(11, 12), SetDataTip(AWV_DECREASE, STR_EDIT_SIGN_PREVIOUS_SIGN_TOOLTIP),
		NWidget(NWID_BUTTON_ARROW, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_NEXT), SetMinimalSize(11, 12), SetDataTip(AWV_INCREASE, STR_EDIT_SIGN_NEXT_SIGN_TOOLTIP),
	EndContainer(),
};

static const WindowDesc _query_sign_edit_desc(
	WDP_AUTO, WDP_AUTO, 260, 42,
	WC_QUERY_STRING, WC_NONE,
	WDF_CONSTRUCTION | WDF_UNCLICK_BUTTONS,
	_nested_query_sign_edit_widgets, lengthof(_nested_query_sign_edit_widgets)
);

void HandleClickOnSign(const Sign *si)
{
	if (_ctrl_pressed && si->owner == _local_company) {
		RenameSign(si->index, NULL);
		return;
	}
	ShowRenameSignWindow(si);
}

void ShowRenameSignWindow(const Sign *si)
{
	/* Delete all other edit windows */
	DeleteWindowById(WC_QUERY_STRING, 0);

	new SignWindow(&_query_sign_edit_desc, si);
}

void DeleteRenameSignWindow(SignID sign)
{
	SignWindow *w = dynamic_cast<SignWindow *>(FindWindowById(WC_QUERY_STRING, 0));

	if (w != NULL && w->cur_sign == sign) delete w;
}
