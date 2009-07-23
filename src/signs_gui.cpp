/* $Id$ */

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
	SLW_CLOSEBOX = 0,
	SLW_CAPTION,
	SLW_STICKY,
	SLW_LIST,
	SLW_SCROLLBAR,
	SLW_RESIZE,
};

struct SignListWindow : Window, SignList {
	SignListWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->vscroll.cap = 12;
		this->resize.step_height = 10;
		this->resize.height = this->height - 10 * 7; // minimum if 5 in the list

		this->signs.ForceRebuild();
		this->signs.NeedResort();

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		BuildSignsList();
		SortSignsList();

		SetVScrollCount(this, this->signs.Length());

		SetDParam(0, this->vscroll.count);
		this->DrawWidgets();

		/* No signs? */
		int y = this->widget[SLW_LIST].top + 2; // offset from top of widget
		if (this->vscroll.count == 0) {
			DrawString(this->widget[SLW_LIST].left + 2, this->widget[SLW_LIST].right, y, STR_STATION_LIST_NONE);
			return;
		}

		/* Start drawing the signs */
		for (uint16 i = this->vscroll.pos; i < this->vscroll.cap + this->vscroll.pos && i < this->vscroll.count; i++) {
			const Sign *si = this->signs[i];

			if (si->owner != OWNER_NONE) DrawCompanyIcon(si->owner, this->widget[SLW_LIST].left + 4, y + 1);

			SetDParam(0, si->index);
			DrawString(this->widget[SLW_LIST].left + 22, this->widget[SLW_LIST].right, y, STR_SIGN_NAME, TC_YELLOW);
			y += 10;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget == SLW_LIST) {
			uint32 id_v = (pt.y - this->widget[SLW_LIST].top - 1) / 10;

			if (id_v >= this->vscroll.cap) return;
			id_v += this->vscroll.pos;
			if (id_v >= this->vscroll.count) return;

			const Sign *si = this->signs[id_v];
			ScrollMainWindowToTile(TileVirtXY(si->x, si->y));
		}
	}

	virtual void OnResize(Point delta)
	{
		this->vscroll.cap += delta.y / 10;
	}

	virtual void OnInvalidateData(int data)
	{
		if (data == 0) {
			this->signs.ForceRebuild();
		} else {
			this->signs.ForceResort();
		}
	}
};

static const Widget _sign_list_widget[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_BLACK_CROSS,       STR_TOOLTIP_CLOSE_WINDOW},
{    WWT_CAPTION,  RESIZE_RIGHT,  COLOUR_GREY,    11,   345,     0,    13, STR_SIGN_LIST_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,     RESIZE_LR,  COLOUR_GREY,   346,   357,     0,    13, 0x0,                   STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_RB,  COLOUR_GREY,     0,   345,    14,   137, 0x0,                   STR_NULL},
{  WWT_SCROLLBAR,    RESIZE_LRB,  COLOUR_GREY,   346,   357,    14,   125, 0x0,                   STR_TOOLTIP_VSCROLL_BAR_SCROLLS_LIST},
{  WWT_RESIZEBOX,   RESIZE_LRTB,  COLOUR_GREY,   346,   357,   126,   137, 0x0,                   STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const NWidgetPart _nested_sign_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, SLW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, SLW_CAPTION), SetDataTip(STR_SIGN_LIST_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_GREY, SLW_STICKY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, SLW_LIST), SetMinimalSize(346, 124), SetResize(1, 10), EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_SCROLLBAR, COLOUR_GREY, SLW_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY, SLW_RESIZE),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _sign_list_desc(
	WDP_AUTO, WDP_AUTO, 358, 138, 358, 138,
	WC_SIGN_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_sign_list_widget, _nested_sign_list_widgets, lengthof(_nested_sign_list_widgets)
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
	QUERY_EDIT_SIGN_WIDGET_CLOSEBOX,
	QUERY_EDIT_SIGN_WIDGET_CAPTION,
	QUERY_EDIT_SIGN_WIDGET_PANEL,
	QUERY_EDIT_SIGN_WIDGET_TEXT,
	QUERY_EDIT_SIGN_WIDGET_OK,
	QUERY_EDIT_SIGN_WIDGET_CANCEL,
	QUERY_EDIT_SIGN_WIDGET_DELETE,
	QUERY_EDIT_SIGN_WIDGET_FILL,
	QUERY_EDIT_SIGN_WIDGET_PREVIOUS,
	QUERY_EDIT_SIGN_WIDGET_NEXT,
};

struct SignWindow : QueryStringBaseWindow, SignList {
	SignID cur_sign;

	SignWindow(const WindowDesc *desc, const Sign *si) : QueryStringBaseWindow(MAX_LENGTH_SIGN_NAME_BYTES, desc)
	{
		this->caption = STR_EDIT_SIGN_CAPTION;
		this->afilter = CS_ALPHANUMERAL;
		this->LowerWidget(QUERY_EDIT_SIGN_WIDGET_TEXT);

		UpdateSignEditWindow(si);
		this->SetFocusedWidget(QUERY_EDIT_SIGN_WIDGET_TEXT);
		this->FindWindowPlacementAndResize(desc);
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

		this->InvalidateWidget(QUERY_EDIT_SIGN_WIDGET_TEXT);
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

	virtual void OnPaint()
	{
		SetDParam(0, this->caption);
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

static const Widget _query_sign_edit_widgets[] = {
{ WWT_CLOSEBOX, RESIZE_NONE,  COLOUR_GREY,   0,  10,   0,  13, STR_BLACK_CROSS,             STR_TOOLTIP_CLOSE_WINDOW},   // QUERY_EDIT_SIGN_WIDGET_CLOSEBOX
{  WWT_CAPTION, RESIZE_NONE,  COLOUR_GREY,  11, 259,   0,  13, STR_QUERY_CAPTION,           STR_NULL },                  // QUERY_EDIT_SIGN_WIDGET_CAPTION
{    WWT_PANEL, RESIZE_NONE,  COLOUR_GREY,   0, 259,  14,  29, STR_NULL,                    STR_NULL },                  // QUERY_EDIT_SIGN_WIDGET_PANEL
{  WWT_EDITBOX, RESIZE_NONE,  COLOUR_GREY,   2, 257,  16,  27, STR_SIGN_OSKTITLE,           STR_NULL },                  // QUERY_EDIT_SIGN_WIDGET_TEXT (Text field)
{  WWT_TEXTBTN, RESIZE_NONE,  COLOUR_GREY,   0,  60,  30,  41, STR_QUERY_OK,                STR_NULL },                  // QUERY_EDIT_SIGN_WIDGET_OK
{  WWT_TEXTBTN, RESIZE_NONE,  COLOUR_GREY,  61, 120,  30,  41, STR_QUERY_CANCEL,            STR_NULL },                  // QUERY_EDIT_SIGN_WIDGET_CANCEL
{  WWT_TEXTBTN, RESIZE_NONE,  COLOUR_GREY, 121, 180,  30,  41, STR_TOWN_VIEW_DELETE_BUTTON, STR_NULL },                  // QUERY_EDIT_SIGN_WIDGET_DELETE
{    WWT_PANEL, RESIZE_NONE,  COLOUR_GREY, 181, 237,  30,  41, STR_NULL,                    STR_NULL },                  // QUERY_EDIT_SIGN_WIDGET_FILL
{  WWT_TEXTBTN, RESIZE_NONE,  COLOUR_GREY, 238, 248,  30,  41, STR_BLACK_SMALL_ARROW_LEFT,  STR_PREVIOUS_SIGN_TOOLTIP }, // QUERY_EDIT_SIGN_WIDGET_PREVIOUS
{  WWT_TEXTBTN, RESIZE_NONE,  COLOUR_GREY, 249, 259,  30,  41, STR_BLACK_SMALL_ARROW_RIGHT, STR_NEXT_SIGN_TOOLTIP },     // QUERY_EDIT_SIGN_WIDGET_NEXT
{ WIDGETS_END },
};

static const NWidgetPart _nested_query_sign_edit_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_CAPTION), SetDataTip(STR_QUERY_CAPTION, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_PANEL),
		NWidget(WWT_EDITBOX, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_TEXT), SetMinimalSize(256, 12), SetDataTip(STR_SIGN_OSKTITLE, STR_NULL), SetPadding(2, 2, 2, 2),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_OK), SetMinimalSize(61, 12), SetDataTip(STR_QUERY_OK, STR_NULL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_CANCEL), SetMinimalSize(60, 12), SetDataTip(STR_QUERY_CANCEL, STR_NULL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_DELETE), SetMinimalSize(60, 12), SetDataTip(STR_TOWN_VIEW_DELETE_BUTTON, STR_NULL),
		NWidget(WWT_PANEL, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_FILL), SetFill(1, 1), EndContainer(),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_PREVIOUS), SetMinimalSize(11, 12), SetDataTip(STR_BLACK_SMALL_ARROW_LEFT, STR_PREVIOUS_SIGN_TOOLTIP),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, QUERY_EDIT_SIGN_WIDGET_NEXT), SetMinimalSize(11, 12), SetDataTip(STR_BLACK_SMALL_ARROW_RIGHT, STR_NEXT_SIGN_TOOLTIP),
	EndContainer(),
};

static const WindowDesc _query_sign_edit_desc(
	190, 170, 260, 42, 260, 42,
	WC_QUERY_STRING, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_CONSTRUCTION,
	_query_sign_edit_widgets, _nested_query_sign_edit_widgets, lengthof(_nested_query_sign_edit_widgets)
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
