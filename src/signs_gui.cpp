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
#include "core/geometry_func.hpp"
#include "hotkeys.h"

#include "table/strings.h"
#include "table/sprites.h"

/**
 * Contains the necessary information to decide if a sign should
 * be filtered out or not. This struct is sent as parameter to the
 * sort functions of the GUISignList.
 */
struct FilterInfo {
	const char *string;  ///< String to match sign names against
	bool case_sensitive; ///< Should case sensitive matching be used?
};

struct SignList {
	/**
	 * A GUIList contains signs and uses a custom data structure called #FilterInfo for
	 * passing data to the sort functions.
	 */
	typedef GUIList<const Sign *, FilterInfo> GUISignList;

	static const Sign *last_sign;
	GUISignList signs;

	char filter_string[MAX_LENGTH_SIGN_NAME_CHARS * MAX_CHAR_LENGTH]; ///< The match string to be used when the GUIList is (re)-sorted.
	static bool match_case;                                           ///< Should case sensitive matching be used?

	/**
	 * Creates a SignList with filtering disabled by default.
	 */
	SignList()
	{
		filter_string[0] = '\0';
	}

	void BuildSignsList()
	{
		if (!this->signs.NeedRebuild()) return;

		DEBUG(misc, 3, "Building sign list");

		this->signs.Clear();

		const Sign *si;
		FOR_ALL_SIGNS(si) *this->signs.Append() = si;

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

	/** Filter sign list by sign name (case sensitive setting in FilterInfo) */
	static bool CDECL SignNameFilter(const Sign * const *a, FilterInfo filter_info)
	{
		/* Get sign string */
		char buf1[MAX_LENGTH_SIGN_NAME_CHARS * MAX_CHAR_LENGTH];
		SetDParam(0, (*a)->index);
		GetString(buf1, STR_SIGN_NAME, lastof(buf1));

		return (filter_info.case_sensitive ? strstr(buf1, filter_info.string) : strcasestr(buf1, filter_info.string)) != NULL;
	}

	/** Filter out signs from the sign list that does not match the name filter */
	void FilterSignList()
	{
		FilterInfo filter_info = {this->filter_string, this->match_case};
		this->signs.Filter(&SignNameFilter, filter_info);
	}
};

const Sign *SignList::last_sign = NULL;
bool SignList::match_case = false;

/** Enum referring to the widgets in the sign list window */
enum SignListWidgets {
	SLW_CAPTION,
	SLW_LIST,
	SLW_SCROLLBAR,
	SLW_FILTER_TEXT,           ///< Text box for typing a filter string
	SLW_FILTER_MATCH_CASE_BTN, ///< Button to toggle if case sensitive filtering should be used
	SLW_FILTER_CLEAR_BTN,      ///< Button to clear the filter
};

/** Enum referring to the Hotkeys in the sign list window */
enum SignListHotkeys {
	SLHK_FOCUS_FILTER_BOX, ///< Focus the edit box for editing the filter string
};

struct SignListWindow : QueryStringBaseWindow, SignList {
	int text_offset; ///< Offset of the sign text relative to the left edge of the SLW_LIST widget.
	Scrollbar *vscroll;

	SignListWindow(const WindowDesc *desc, WindowNumber window_number) : QueryStringBaseWindow(MAX_LENGTH_SIGN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_SIGN_NAME_CHARS)
	{
		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(SLW_SCROLLBAR);
		this->FinishInitNested(desc, window_number);
		this->SetWidgetLoweredState(SLW_FILTER_MATCH_CASE_BTN, SignList::match_case);

		/* Initialize the text edit widget */
		this->afilter = CS_ALPHANUMERAL;
		InitializeTextBuffer(&this->text, this->edit_str_buf, MAX_LENGTH_SIGN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_SIGN_NAME_CHARS);
		ClearFilterTextWidget();

		/* Initialize the filtering variables */
		this->SetFilterString("");

		/* Create initial list. */
		this->signs.ForceRebuild();
		this->signs.ForceResort();
		this->BuildSortSignList();
	}

	/**
	 * Empties the string buffer that is edited by the filter text edit widget.
	 * It also triggers the redraw of the widget so it become visible that the string has been made empty.
	 */
	void ClearFilterTextWidget()
	{
		this->edit_str_buf[0] = '\0';
		UpdateTextBufferSize(&this->text);

		this->SetWidgetDirty(SLW_FILTER_TEXT);
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
		if (!StrEmpty(new_filter_string)) {
			/* Copy new filter string */
			strecpy(this->filter_string, new_filter_string, lastof(this->filter_string));

			this->signs.SetFilterState(true);

			this->EnableWidget(SLW_FILTER_CLEAR_BTN);
		} else {
			/* There is no new string -> clear this->filter_string */
			this->filter_string[0] = '\0';

			this->signs.SetFilterState(false);
			this->DisableWidget(SLW_FILTER_CLEAR_BTN);
		}

		/* Repaint the clear button since its disabled state may have changed */
		this->SetWidgetDirty(SLW_FILTER_CLEAR_BTN);

		/* Rebuild the list of signs */
		this->InvalidateData();
	}

	virtual void OnPaint()
	{
		if (this->signs.NeedRebuild()) this->BuildSortSignList();
		this->DrawWidgets();
		if (!this->IsShaded()) this->DrawEditBox(SLW_FILTER_TEXT);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case SLW_LIST: {
				uint y = r.top + WD_FRAMERECT_TOP; // Offset from top of widget.
				/* No signs? */
				if (this->vscroll->GetCount() == 0) {
					DrawString(r.left + WD_FRAMETEXT_LEFT, r.right, y, STR_STATION_LIST_NONE);
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
		if (widget == SLW_CAPTION) SetDParam(0, this->vscroll->GetCount());
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case SLW_LIST: {
				uint id_v = this->vscroll->GetScrolledRowFromWidget(pt.y, this, SLW_LIST, WD_FRAMERECT_TOP);
				if (id_v == INT_MAX) return;

				const Sign *si = this->signs[id_v];
				ScrollMainWindowToTile(TileVirtXY(si->x, si->y));
				break;
			}
			case SLW_FILTER_CLEAR_BTN:
				this->ClearFilterTextWidget(); // Empty the text in the EditBox widget
				this->SetFilterString("");     // Use empty text as filter text (= view all signs)
				break;

			case SLW_FILTER_MATCH_CASE_BTN:
				SignList::match_case = !SignList::match_case; // Toggle match case
				this->SetWidgetLoweredState(SLW_FILTER_MATCH_CASE_BTN, SignList::match_case); // Toggle button pushed state
				this->InvalidateData(); // Rebuild the list of signs
				break;
		}
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, SLW_LIST, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case SLW_LIST: {
				Dimension spr_dim = GetSpriteSize(SPR_COMPANY_ICON);
				this->text_offset = WD_FRAMETEXT_LEFT + spr_dim.width + 2; // 2 pixels space between icon and the sign text.
				resize->height = max<uint>(FONT_HEIGHT_NORMAL, spr_dim.height);
				Dimension d = {this->text_offset + WD_FRAMETEXT_RIGHT, WD_FRAMERECT_TOP + 5 * resize->height + WD_FRAMERECT_BOTTOM};
				*size = maxdim(*size, d);
				break;
			}

			case SLW_CAPTION:
				SetDParam(0, max<size_t>(1000, Sign::GetPoolSize()));
				*size = GetStringBoundingBox(STR_SIGN_LIST_CAPTION);
				size->height += padding.height;
				size->width  += padding.width;
				break;
		}
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		EventState state = ES_NOT_HANDLED;
		switch (this->HandleEditBoxKey(SLW_FILTER_TEXT, key, keycode, state)) {
			case HEBR_EDITING:
				this->SetFilterString(this->text.buf);
				break;

			case HEBR_CONFIRM: // Enter pressed -> goto first sign in list
				if (this->signs.Length() >= 1) {
					const Sign *si = this->signs[0];
					ScrollMainWindowToTile(TileVirtXY(si->x, si->y));
				}
				return state;

			case HEBR_CANCEL: // ESC pressed, clear filter.
				this->OnClick(Point(), SLW_FILTER_CLEAR_BTN, 1); // Simulate click on clear button.
				this->UnfocusFocusedWidget();                    // Unfocus the text box.
				return state;

			case HEBR_NOT_FOCUSED: // The filter text box is not globaly focused.
				if (CheckHotkeyMatch(signlist_hotkeys, keycode, this) == SLHK_FOCUS_FILTER_BOX) {
					this->SetFocusedWidget(SLW_FILTER_TEXT);
					SetFocusedWindow(this); // The user has asked to give focus to the text box, so make sure this window is focused.
					state = ES_HANDLED;
				}
				break;

			default:
				NOT_REACHED();
		}

		if (state == ES_HANDLED) OnOSKInput(SLW_FILTER_TEXT);

		return state;
	}

	virtual void OnOSKInput(int widget)
	{
		if (widget == SLW_FILTER_TEXT) this->SetFilterString(this->text.buf);
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(SLW_FILTER_TEXT);
	}

	void BuildSortSignList()
	{
		if (this->signs.NeedRebuild()) {
			this->BuildSignsList();
			this->vscroll->SetCount(this->signs.Length());
			this->SetWidgetDirty(SLW_CAPTION);
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
		if (data == 0 || !StrEmpty(this->filter_string)) { // New or deleted sign, or there is a filter string
			/* This needs to be done in command-scope to enforce rebuilding before resorting invalid data */
			this->signs.ForceRebuild();
		} else { // Change of sign contents while there is no filter string
			this->signs.ForceResort();
		}
	}

	static Hotkey<SignListWindow> signlist_hotkeys[];
};

Hotkey<SignListWindow> SignListWindow::signlist_hotkeys[] = {
	Hotkey<SignListWindow>('F', "focus_filter_box", SLHK_FOCUS_FILTER_BOX),
	HOTKEY_LIST_END(SignListWindow)
};
Hotkey<SignListWindow> *_signlist_hotkeys = SignListWindow::signlist_hotkeys;

static const NWidgetPart _nested_sign_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, SLW_CAPTION), SetDataTip(STR_SIGN_LIST_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_PANEL, COLOUR_GREY, SLW_LIST), SetMinimalSize(WD_FRAMETEXT_LEFT + 16 + 255 + WD_FRAMETEXT_RIGHT, 50),
								SetResize(1, 10), SetFill(1, 0), SetScrollbar(SLW_SCROLLBAR), EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1),
					NWidget(WWT_EDITBOX, COLOUR_GREY, SLW_FILTER_TEXT), SetMinimalSize(80, 12), SetResize(1, 0), SetFill(1, 0), SetPadding(2, 2, 2, 2),
							SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
				EndContainer(),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, SLW_FILTER_MATCH_CASE_BTN), SetDataTip(STR_SIGN_LIST_MATCH_CASE, STR_SIGN_LIST_MATCH_CASE_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLW_FILTER_CLEAR_BTN), SetDataTip(STR_SIGN_LIST_CLEAR, STR_SIGN_LIST_CLEAR_TOOLTIP),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VERTICAL), SetFill(0, 1),
				NWidget(NWID_VSCROLLBAR, COLOUR_GREY, SLW_SCROLLBAR),
			EndContainer(),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _sign_list_desc(
	WDP_AUTO, 358, 138,
	WC_SIGN_LIST, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_sign_list_widgets, lengthof(_nested_sign_list_widgets)
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

EventState SignListGlobalHotkeys(uint16 key, uint16 keycode)
{
	int num = CheckHotkeyMatch<SignListWindow>(_signlist_hotkeys, keycode, NULL, true);
	if (num == -1) return ES_NOT_HANDLED;
	Window *w = ShowSignList();
	if (w == NULL) return ES_NOT_HANDLED;
	return w->OnKeyPress(key, keycode);
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

	SignWindow(const WindowDesc *desc, const Sign *si) : QueryStringBaseWindow(MAX_LENGTH_SIGN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_SIGN_NAME_CHARS)
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
		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, this->max_chars);

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
		if (!this->IsShaded()) this->DrawEditBox(QUERY_EDIT_SIGN_WIDGET_TEXT);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
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
		NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_PREVIOUS), SetMinimalSize(11, 12), SetDataTip(AWV_DECREASE, STR_EDIT_SIGN_PREVIOUS_SIGN_TOOLTIP),
		NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_NEXT), SetMinimalSize(11, 12), SetDataTip(AWV_INCREASE, STR_EDIT_SIGN_NEXT_SIGN_TOOLTIP),
	EndContainer(),
};

static const WindowDesc _query_sign_edit_desc(
	WDP_AUTO, 0, 0,
	WC_QUERY_STRING, WC_NONE,
	WDF_CONSTRUCTION | WDF_UNCLICK_BUTTONS,
	_nested_query_sign_edit_widgets, lengthof(_nested_query_sign_edit_widgets)
);

/**
 * Handle clicking on a sign.
 * @param si The sign that was clicked on.
 */
void HandleClickOnSign(const Sign *si)
{
	if (_ctrl_pressed && si->owner == _local_company) {
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
	DeleteWindowById(WC_QUERY_STRING, 0);

	new SignWindow(&_query_sign_edit_desc, si);
}

/**
 * Close the sign window associated with the given sign.
 * @param sign The sign to close the window for.
 */
void DeleteRenameSignWindow(SignID sign)
{
	SignWindow *w = dynamic_cast<SignWindow *>(FindWindowById(WC_QUERY_STRING, 0));

	if (w != NULL && w->cur_sign == sign) delete w;
}
