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
#include "viewport_func.h"
#include "querystring_gui.h"
#include "sortlist_type.h"
#include "stringfilter_type.h"
#include "string_func.h"
#include "core/geometry_func.hpp"
#include "hotkeys.h"
#include "transparency.h"

#include "widgets/sign_widget.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "safeguards.h"

struct SignList {
	/**
	 * A GUIList contains signs and uses a StringFilter for filtering.
	 */
	typedef GUIList<const Sign *, StringFilter &> GUISignList;

	static const Sign *last_sign;
	GUISignList signs;

	StringFilter string_filter;                                       ///< The match string to be used when the GUIList is (re)-sorted.
	static bool match_case;                                           ///< Should case sensitive matching be used?

	/**
	 * Creates a SignList with filtering disabled by default.
	 */
	SignList() : string_filter(&match_case)
	{
	}

	void BuildSignsList()
	{
		if (!this->signs.NeedRebuild()) return;

		DEBUG(misc, 3, "Building sign list");

		this->signs.Clear();

		const Sign *si;
		FOR_ALL_SIGNS(si) *this->signs.Append() = si;

		this->signs.SetFilterState(true);
		this->FilterSignList();
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

		int r = strnatcmp(buf, buf_cache); // Sort by name (natural sorting).

		return r != 0 ? r : ((*a)->index - (*b)->index);
	}

	void SortSignsList()
	{
		if (!this->signs.Sort(&SignNameSorter)) return;

		/* Reset the name sorter sort cache */
		this->last_sign = NULL;
	}

	/** Filter sign list by sign name */
	static bool CDECL SignNameFilter(const Sign * const *a, StringFilter &filter)
	{
		/* Get sign string */
		char buf1[MAX_LENGTH_SIGN_NAME_CHARS * MAX_CHAR_LENGTH];
		SetDParam(0, (*a)->index);
		GetString(buf1, STR_SIGN_NAME, lastof(buf1));

		filter.ResetState();
		filter.AddLine(buf1);
		return filter.GetState();
	}

	/** Filter sign list excluding OWNER_DEITY */
	static bool CDECL OwnerDeityFilter(const Sign * const *a, StringFilter &filter)
	{
		/* You should never be able to edit signs of owner DEITY */
		return (*a)->owner != OWNER_DEITY;
	}

	/** Filter sign list by owner */
	static bool CDECL OwnerVisibilityFilter(const Sign * const *a, StringFilter &filter)
	{
		assert(!HasBit(_display_opt, DO_SHOW_COMPETITOR_SIGNS));
		/* Hide sign if non-own signs are hidden in the viewport */
		return (*a)->owner == _local_company || (*a)->owner == OWNER_DEITY;
	}

	/** Filter out signs from the sign list that does not match the name filter */
	void FilterSignList()
	{
		this->signs.Filter(&SignNameFilter, this->string_filter);
		if (_game_mode != GM_EDITOR) this->signs.Filter(&OwnerDeityFilter, this->string_filter);
		if (!HasBit(_display_opt, DO_SHOW_COMPETITOR_SIGNS)) {
			this->signs.Filter(&OwnerVisibilityFilter, this->string_filter);
		}
	}
};

const Sign *SignList::last_sign = NULL;
bool SignList::match_case = false;

/** Enum referring to the Hotkeys in the sign list window */
enum SignListHotkeys {
	SLHK_FOCUS_FILTER_BOX, ///< Focus the edit box for editing the filter string
};

struct SignListWindow : Window, SignList {
	QueryString filter_editbox; ///< Filter editbox;
	int text_offset; ///< Offset of the sign text relative to the left edge of the WID_SIL_LIST widget.
	Scrollbar *vscroll;

	SignListWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc), filter_editbox(MAX_LENGTH_SIGN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_SIGN_NAME_CHARS)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_SIL_SCROLLBAR);
		this->FinishInitNested(window_number);
		this->SetWidgetLoweredState(WID_SIL_FILTER_MATCH_CASE_BTN, SignList::match_case);

		/* Initialize the text edit widget */
		this->querystrings[WID_SIL_FILTER_TEXT] = &this->filter_editbox;
		this->filter_editbox.cancel_button = QueryString::ACTION_CLEAR;

		/* Initialize the filtering variables */
		this->SetFilterString("");

		/* Create initial list. */
		this->signs.ForceRebuild();
		this->signs.ForceResort();
		this->BuildSortSignList();
	}

	/**
	 * This function sets the filter string of the sign list. The contents of
	 * the edit widget is not updated by this function. Depending on if the
	 * new string is zero-length or not the clear button is made
	 * disabled/enabled. The sign list is updated according to the new filter.
	 */
	void SetFilterString(const char *new_filter_string)
	{
		/* check if there is a new filter string */
		this->string_filter.SetFilterTerm(new_filter_string);

		/* Rebuild the list of signs */
		this->InvalidateData();
	}

	virtual void OnPaint()
	{
		if (this->signs.NeedRebuild()) this->BuildSortSignList();
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_SIL_LIST: {
				uint y = r.top + WD_FRAMERECT_TOP; // Offset from top of widget.
				/* No signs? */
				if (this->vscroll->GetCount() == 0) {
					DrawString(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, y, STR_STATION_LIST_NONE);
					return;
				}

				bool rtl = _current_text_dir == TD_RTL;
				int sprite_offset_y = (FONT_HEIGHT_NORMAL - 10) / 2 + 1;
				uint icon_left  = 4 + (rtl ? r.right - this->text_offset : r.left);
				uint text_left  = r.left + (rtl ? WD_FRAMERECT_LEFT : this->text_offset);
				uint text_right = r.right - (rtl ? this->text_offset : WD_FRAMERECT_RIGHT);

				/* At least one sign available. */
				for (uint16 i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && i < this->vscroll->GetCount(); i++) {
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
		if (widget == WID_SIL_CAPTION) SetDParam(0, this->vscroll->GetCount());
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_SIL_LIST: {
				uint id_v = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_SIL_LIST, WD_FRAMERECT_TOP);
				if (id_v == INT_MAX) return;

				const Sign *si = this->signs[id_v];
				ScrollMainWindowToTile(TileVirtXY(si->x, si->y));
				break;
			}

			case WID_SIL_FILTER_ENTER_BTN:
				if (this->signs.Length() >= 1) {
					const Sign *si = this->signs[0];
					ScrollMainWindowToTile(TileVirtXY(si->x, si->y));
				}
				break;

			case WID_SIL_FILTER_MATCH_CASE_BTN:
				SignList::match_case = !SignList::match_case; // Toggle match case
				this->SetWidgetLoweredState(WID_SIL_FILTER_MATCH_CASE_BTN, SignList::match_case); // Toggle button pushed state
				this->InvalidateData(); // Rebuild the list of signs
				break;
		}
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, WID_SIL_LIST, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_SIL_LIST: {
				Dimension spr_dim = GetSpriteSize(SPR_COMPANY_ICON);
				this->text_offset = WD_FRAMETEXT_LEFT + spr_dim.width + 2; // 2 pixels space between icon and the sign text.
				resize->height = max<uint>(FONT_HEIGHT_NORMAL, spr_dim.height);
				Dimension d = {(uint)(this->text_offset + WD_FRAMETEXT_RIGHT), WD_FRAMERECT_TOP + 5 * resize->height + WD_FRAMERECT_BOTTOM};
				*size = maxdim(*size, d);
				break;
			}

			case WID_SIL_CAPTION:
				SetDParamMaxValue(0, Sign::GetPoolSize(), 3);
				*size = GetStringBoundingBox(STR_SIGN_LIST_CAPTION);
				size->height += padding.height;
				size->width  += padding.width;
				break;
		}
	}

	virtual EventState OnHotkey(int hotkey)
	{
		switch (hotkey) {
			case SLHK_FOCUS_FILTER_BOX:
				this->SetFocusedWidget(WID_SIL_FILTER_TEXT);
				SetFocusedWindow(this); // The user has asked to give focus to the text box, so make sure this window is focused.
				break;

			default:
				return ES_NOT_HANDLED;
		}

		return ES_HANDLED;
	}

	virtual void OnEditboxChanged(int widget)
	{
		if (widget == WID_SIL_FILTER_TEXT) this->SetFilterString(this->filter_editbox.text.buf);
	}

	void BuildSortSignList()
	{
		if (this->signs.NeedRebuild()) {
			this->BuildSignsList();
			this->vscroll->SetCount(this->signs.Length());
			this->SetWidgetDirty(WID_SIL_CAPTION);
		}
		this->SortSignsList();
	}

	virtual void OnHundredthTick()
	{
		this->BuildSortSignList();
		this->SetDirty();
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		/* When there is a filter string, we always need to rebuild the list even if
		 * the amount of signs in total is unchanged, as the subset of signs that is
		 * accepted by the filter might has changed. */
		if (data == 0 || data == -1 || !this->string_filter.IsEmpty()) { // New or deleted sign, changed visibility setting or there is a filter string
			/* This needs to be done in command-scope to enforce rebuilding before resorting invalid data */
			this->signs.ForceRebuild();
		} else { // Change of sign contents while there is no filter string
			this->signs.ForceResort();
		}
	}

	static HotkeyList hotkeys;
};

/**
 * Handler for global hotkeys of the SignListWindow.
 * @param hotkey Hotkey
 * @return ES_HANDLED if hotkey was accepted.
 */
static EventState SignListGlobalHotkeys(int hotkey)
{
	if (_game_mode == GM_MENU) return ES_NOT_HANDLED;
	Window *w = ShowSignList();
	if (w == NULL) return ES_NOT_HANDLED;
	return w->OnHotkey(hotkey);
}

static Hotkey signlist_hotkeys[] = {
	Hotkey('F', "focus_filter_box", SLHK_FOCUS_FILTER_BOX),
	HOTKEY_LIST_END
};
HotkeyList SignListWindow::hotkeys("signlist", signlist_hotkeys, SignListGlobalHotkeys);

static const NWidgetPart _nested_sign_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_SIL_CAPTION), SetDataTip(STR_SIGN_LIST_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_SIL_LIST), SetMinimalSize(WD_FRAMETEXT_LEFT + 16 + 255 + WD_FRAMETEXT_RIGHT, 50),
								SetResize(1, 10), SetFill(1, 0), SetScrollbar(WID_SIL_SCROLLBAR), EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1),
					NWidget(WWT_EDITBOX, COLOUR_GREY, WID_SIL_FILTER_TEXT), SetMinimalSize(80, 12), SetResize(1, 0), SetFill(1, 0), SetPadding(2, 2, 2, 2),
							SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
				EndContainer(),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_SIL_FILTER_MATCH_CASE_BTN), SetDataTip(STR_SIGN_LIST_MATCH_CASE, STR_SIGN_LIST_MATCH_CASE_TOOLTIP),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VERTICAL), SetFill(0, 1),
				NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_SIL_SCROLLBAR),
			EndContainer(),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _sign_list_desc(
	WDP_AUTO, "list_signs", 358, 138,
	WC_SIGN_LIST, WC_NONE,
	0,
	_nested_sign_list_widgets, lengthof(_nested_sign_list_widgets),
	&SignListWindow::hotkeys
);

/**
 * Open the sign list window
 *
 * @return newly opened sign list window, or NULL if the window could not be opened.
 */
Window *ShowSignList()
{
	return AllocateWindowDescFront<SignListWindow>(&_sign_list_desc, 0);
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

struct SignWindow : Window, SignList {
	QueryString name_editbox;
	SignID cur_sign;

	SignWindow(WindowDesc *desc, const Sign *si) : Window(desc), name_editbox(MAX_LENGTH_SIGN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_SIGN_NAME_CHARS)
	{
		this->querystrings[WID_QES_TEXT] = &this->name_editbox;
		this->name_editbox.caption = STR_EDIT_SIGN_CAPTION;
		this->name_editbox.cancel_button = WID_QES_CANCEL;
		this->name_editbox.ok_button = WID_QES_OK;

		this->InitNested(WN_QUERY_STRING_SIGN);

		UpdateSignEditWindow(si);
		this->SetFocusedWidget(WID_QES_TEXT);
	}

	void UpdateSignEditWindow(const Sign *si)
	{
		/* Display an empty string when the sign hasn't been edited yet */
		if (si->name != NULL) {
			SetDParam(0, si->index);
			this->name_editbox.text.Assign(STR_SIGN_NAME);
		} else {
			this->name_editbox.text.DeleteAll();
		}

		this->cur_sign = si->index;

		this->SetWidgetDirty(WID_QES_TEXT);
		this->SetFocusedWidget(WID_QES_TEXT);
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
			case WID_QES_CAPTION:
				SetDParam(0, this->name_editbox.caption);
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_QES_PREVIOUS:
			case WID_QES_NEXT: {
				const Sign *si = this->PrevNextSign(widget == WID_QES_NEXT);

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

			case WID_QES_DELETE:
				/* Only need to set the buffer to null, the rest is handled as the OK button */
				RenameSign(this->cur_sign, "");
				/* don't delete this, we are deleted in Sign::~Sign() -> DeleteRenameSignWindow() */
				break;

			case WID_QES_OK:
				if (RenameSign(this->cur_sign, this->name_editbox.text.buf)) break;
				FALLTHROUGH;

			case WID_QES_CANCEL:
				delete this;
				break;
		}
	}
};

static const NWidgetPart _nested_query_sign_edit_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_QES_CAPTION), SetDataTip(STR_WHITE_STRING, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(WWT_EDITBOX, COLOUR_GREY, WID_QES_TEXT), SetMinimalSize(256, 12), SetDataTip(STR_EDIT_SIGN_SIGN_OSKTITLE, STR_NULL), SetPadding(2, 2, 2, 2),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_QES_OK), SetMinimalSize(61, 12), SetDataTip(STR_BUTTON_OK, STR_NULL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_QES_CANCEL), SetMinimalSize(60, 12), SetDataTip(STR_BUTTON_CANCEL, STR_NULL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_QES_DELETE), SetMinimalSize(60, 12), SetDataTip(STR_TOWN_VIEW_DELETE_BUTTON, STR_NULL),
		NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1), EndContainer(),
		NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, WID_QES_PREVIOUS), SetMinimalSize(11, 12), SetDataTip(AWV_DECREASE, STR_EDIT_SIGN_PREVIOUS_SIGN_TOOLTIP),
		NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, WID_QES_NEXT), SetMinimalSize(11, 12), SetDataTip(AWV_INCREASE, STR_EDIT_SIGN_NEXT_SIGN_TOOLTIP),
	EndContainer(),
};

static WindowDesc _query_sign_edit_desc(
	WDP_CENTER, "query_sign", 0, 0,
	WC_QUERY_STRING, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_query_sign_edit_widgets, lengthof(_nested_query_sign_edit_widgets)
);

/**
 * Handle clicking on a sign.
 * @param si The sign that was clicked on.
 */
void HandleClickOnSign(const Sign *si)
{
	if (_ctrl_pressed && (si->owner == _local_company || (si->owner == OWNER_DEITY && _game_mode == GM_EDITOR))) {
		RenameSign(si->index, NULL);
		return;
	}
	ShowRenameSignWindow(si);
}

/**
 * Show the window to change the text of a sign.
 * @param si The sign to show the window for.
 */
void ShowRenameSignWindow(const Sign *si)
{
	/* Delete all other edit windows */
	DeleteWindowByClass(WC_QUERY_STRING);

	new SignWindow(&_query_sign_edit_desc, si);
}

/**
 * Close the sign window associated with the given sign.
 * @param sign The sign to close the window for.
 */
void DeleteRenameSignWindow(SignID sign)
{
	SignWindow *w = dynamic_cast<SignWindow *>(FindWindowById(WC_QUERY_STRING, WN_QUERY_STRING_SIGN));

	if (w != NULL && w->cur_sign == sign) delete w;
}
