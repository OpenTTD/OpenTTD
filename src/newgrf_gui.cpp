/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_gui.cpp GUI to change NewGRF settings. */

#include "stdafx.h"
#include "gui.h"
#include "newgrf.h"
#include "strings_func.h"
#include "window_func.h"
#include "gfx_func.h"
#include "gamelog.h"
#include "settings_func.h"
#include "widgets/dropdown_type.h"
#include "network/network.h"
#include "network/network_content.h"
#include "sortlist_type.h"
#include "querystring_gui.h"

#include "table/strings.h"
#include "table/sprites.h"

/** Parse an integerlist string and set each found value
 * @param p the string to be parsed. Each element in the list is seperated by a
 * comma or a space character
 * @param items pointer to the integerlist-array that will be filled with values
 * @param maxitems the maximum number of elements the integerlist-array has
 * @return returns the number of items found, or -1 on an error */
static int parse_intlist(const char *p, int *items, int maxitems)
{
	int n = 0, v;
	char *end;

	for (;;) {
		while (*p == ' ' || *p == ',') p++;
		if (*p == '\0') break;
		v = strtol(p, &end, 0);
		if (p == end || n == maxitems) return -1;
		p = end;
		items[n++] = v;
	}

	return n;
}


static void ShowNewGRFInfo(const GRFConfig *c, uint x, uint y, uint right, uint bottom, bool show_params)
{
	char buff[256];

	if (c->error != NULL) {
		char message[512];
		SetDParamStr(0, c->error->custom_message); // is skipped by built-in messages
		SetDParam   (1, STR_JUST_RAW_STRING);
		SetDParamStr(2, c->filename);
		SetDParam   (3, STR_JUST_RAW_STRING);
		SetDParamStr(4, c->error->data);
		for (uint i = 0; i < c->error->num_params; i++) {
			SetDParam(5 + i, c->error->param_value[i]);
		}
		GetString(message, c->error->custom_message == NULL ? c->error->message : STR_JUST_RAW_STRING, lastof(message));

		SetDParamStr(0, message);
		y = DrawStringMultiLine(x, right, y, bottom, c->error->severity);
	}

	/* Draw filename or not if it is not known (GRF sent over internet) */
	if (c->filename != NULL) {
		SetDParamStr(0, c->filename);
		y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_FILENAME);
	}

	/* Prepare and draw GRF ID */
	snprintf(buff, lengthof(buff), "%08X", BSWAP32(c->grfid));
	SetDParamStr(0, buff);
	y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_GRF_ID);

	/* Prepare and draw MD5 sum */
	md5sumToString(buff, lastof(buff), c->md5sum);
	SetDParamStr(0, buff);
	y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_MD5SUM);

	/* Show GRF parameter list */
	if (show_params) {
		if (c->num_params > 0) {
			GRFBuildParamList(buff, c, lastof(buff));
			SetDParam(0, STR_JUST_RAW_STRING);
			SetDParamStr(1, buff);
		} else {
			SetDParam(0, STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY_NONE);
		}
		y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_PARAMETER);

		/* Draw the palette of the NewGRF */
		SetDParamStr(0, c->windows_paletted ? "Windows" : "DOS");
		y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_PALETTE);
	}

	/* Show flags */
	if (c->status == GCS_NOT_FOUND)       y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_NOT_FOUND);
	if (c->status == GCS_DISABLED)        y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_DISABLED);
	if (HasBit(c->flags, GCF_COMPATIBLE)) y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_COMPATIBLE_LOADED);

	/* Draw GRF info if it exists */
	if (c->info != NULL && !StrEmpty(c->info)) {
		SetDParam(0, STR_JUST_RAW_STRING);
		SetDParamStr(1, c->info);
		y = DrawStringMultiLine(x, right, y, bottom, STR_BLACK_STRING);
	} else {
		y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_NO_INFO);
	}
}


/** Names of the add a newgrf window widgets. */
enum AddNewGRFWindowWidgets {
	ANGRFW_CLOSEBOX = 0,
	ANGRFW_CAPTION,
	ANGRFW_FILTER_PANEL,
	ANGRFW_FILTER_TITLE,
	ANGRFW_FILTER,
	ANGRFW_BACKGROUND,
	ANGRFW_GRF_LIST,
	ANGRFW_SCROLLBAR,
	ANGRFW_GRF_INFO,
	ANGRFW_ADD,
	ANGRFW_RESCAN,
	ANGRFW_RESIZE,
};

/**
 * Window for adding NewGRF files
 */
struct NewGRFAddWindow : public QueryStringBaseWindow {
private:
	typedef GUIList<const GRFConfig *> GUIGRFConfigList;

	GRFConfig **list;

	/** Runtime saved values */
	static Listing last_sorting;
	static Filtering last_filtering;

	static GUIGRFConfigList::SortFunction * const sorter_funcs[];
	static GUIGRFConfigList::FilterFunction * const filter_funcs[];
	GUIGRFConfigList grfs;

	const GRFConfig *sel;
	int sel_pos;

	enum {
		EDITBOX_MAX_SIZE = 50,
		EDITBOX_MAX_LENGTH = 300,
	};

	/**
	 * (Re)build the grf as its amount has changed because
	 * an item has been added or deleted for example
	 */
	void BuildGrfList()
	{
		if (!this->grfs.NeedRebuild()) return;

		/* Create temporary array of grfs to use for listing */
		this->grfs.Clear();

		for (const GRFConfig *c = _all_grfs; c != NULL; c = c->next) {
			*this->grfs.Append() = c;
		}

		this->FilterGrfList();
		this->grfs.Compact();
		this->grfs.RebuildDone();
		this->SortGrfList();

		this->vscroll.SetCount(this->grfs.Length()); // Update the scrollbar
		this->ScrollToSelected();
	}

	/** Sort grfs by name. */
	static int CDECL NameSorter(const GRFConfig * const *a, const GRFConfig * const *b)
	{
		const char *name_a = ((*a)->name != NULL) ? (*a)->name : "";
		const char *name_b = ((*b)->name != NULL) ? (*b)->name : "";
		int result = strcasecmp(name_a, name_b);
		if (result == 0) {
			result = strcasecmp((*a)->filename, (*b)->filename);
		}
		return result;
	}

	/** Sort the grf list */
	void SortGrfList()
	{
		if (!this->grfs.Sort()) return;

		/* update list position */
		if (this->sel != NULL) {
			this->sel_pos = this->grfs.FindIndex(this->sel);
			if (this->sel_pos < 0) {
				this->sel = NULL;
			}
		}
	}

	/** Filter grfs by tags/name */
	static bool CDECL TagNameFilter(const GRFConfig * const *a, const char *filter_string)
	{
		if ((*a)->name     != NULL && strcasestr((*a)->name,     filter_string) != NULL) return true;
		if ((*a)->filename != NULL && strcasestr((*a)->filename, filter_string) != NULL) return true;
		if ((*a)->info     != NULL && strcasestr((*a)->info,     filter_string) != NULL) return true;
		return false;
	}

	/** Filter the grf list */
	void FilterGrfList()
	{
		if (!this->grfs.Filter(this->edit_str_buf)) return;
	}

	/** Make sure that the currently selected grf is within the visible part of the list */
	void ScrollToSelected()
	{
		if (this->sel_pos >= 0) {
			this->vscroll.ScrollTowards(this->sel_pos);
		}
	}

public:
	NewGRFAddWindow(const WindowDesc *desc, Window *parent, GRFConfig **list) : QueryStringBaseWindow(EDITBOX_MAX_SIZE)
	{
		this->parent = parent;
		this->InitNested(desc, 0);

		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, EDITBOX_MAX_LENGTH);
		this->SetFocusedWidget(ANGRFW_FILTER);

		this->list = list;
		this->grfs.SetListing(this->last_sorting);
		this->grfs.SetFiltering(this->last_filtering);
		this->grfs.SetSortFuncs(this->sorter_funcs);
		this->grfs.SetFilterFuncs(this->filter_funcs);

		this->OnInvalidateData(0);
	}

	virtual void OnInvalidateData(int data)
	{
		/* data == 0: NewGRFS were rescanned. All pointers to GrfConfigs are invalid.
		 * data == 1: User interaction with this window: Filter or selection change. */
		if (data == 0) {
			this->sel = NULL;
			this->sel_pos = -1;
			this->grfs.ForceRebuild();
		}

		this->BuildGrfList();
		this->SetWidgetDisabledState(ANGRFW_ADD, this->sel == NULL || this->sel->IsOpenTTDBaseGRF());
		this->vscroll.SetCapacity((this->GetWidget<NWidgetBase>(ANGRFW_GRF_LIST)->current_y - WD_FRAMERECT_TOP - WD_FRAMERECT_BOTTOM) / this->resize.step_height);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		switch (widget) {
			case ANGRFW_GRF_LIST:
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = max(size->height, WD_FRAMERECT_TOP + 10 * resize->height + WD_FRAMERECT_BOTTOM + padding.height + 2);
				break;

			case ANGRFW_GRF_INFO:
				size->height = max(size->height, WD_FRAMERECT_TOP + 10 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM + padding.height + 2);
				break;
		}
	}

	virtual void OnResize()
	{
		this->vscroll.SetCapacity(this->GetWidget<NWidgetBase>(ANGRFW_GRF_LIST)->current_y / this->resize.step_height);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
		this->DrawEditBox(ANGRFW_FILTER);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case ANGRFW_GRF_LIST: {
				GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, 0xD7);

				uint y = r.top + WD_FRAMERECT_TOP;
				uint min_index = this->vscroll.GetPosition();
				uint max_index = min(min_index + this->vscroll.GetCapacity(), this->grfs.Length());

				for (uint i = min_index; i < max_index; i++)
				{
					const GRFConfig *c = this->grfs[i];
					bool h = c == this->sel;
					const char *text = (c->name != NULL && !StrEmpty(c->name)) ? c->name : c->filename;

					/* Draw selection background */
					if (h) GfxFillRect(r.left + 1, y, r.right - 1, y + this->resize.step_height - 1, 156);
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, text, h ? TC_WHITE : TC_ORANGE);
					y += this->resize.step_height;
				}
				break;
			}

			case ANGRFW_GRF_INFO:
				if (this->sel != NULL) {
					ShowNewGRFInfo(this->sel, r.left + WD_FRAMERECT_LEFT, r.top + WD_FRAMERECT_TOP, r.right - WD_FRAMERECT_RIGHT, r.bottom - WD_FRAMERECT_BOTTOM, false);
				}
				break;
		}
	}

	virtual void OnDoubleClick(Point pt, int widget)
	{
		if (widget == ANGRFW_GRF_LIST) this->OnClick(pt, ANGRFW_ADD);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case ANGRFW_GRF_LIST: {
				/* Get row... */
				uint i = (pt.y - this->GetWidget<NWidgetBase>(ANGRFW_GRF_LIST)->pos_y - WD_FRAMERECT_TOP) / this->resize.step_height + this->vscroll.GetPosition();

				if (i < this->grfs.Length()) {
					this->sel = this->grfs[i];
					this->sel_pos = i;
				} else {
					this->sel = NULL;
					this->sel_pos = -1;
				}
				this->InvalidateData(1);
				break;
			}

			case ANGRFW_ADD: // Add selection to list
				if (this->sel != NULL) {
					const GRFConfig *src = this->sel;
					GRFConfig **list;

					/* Find last entry in the list, checking for duplicate grfid on the way */
					for (list = this->list; *list != NULL; list = &(*list)->next) {
						if ((*list)->grfid == src->grfid) {
							ShowErrorMessage(INVALID_STRING_ID, STR_NEWGRF_DUPLICATE_GRFID, 0, 0);
							return;
						}
					}

					/* Copy GRF details from scanned list */
					GRFConfig *c = CallocT<GRFConfig>(1);
					*c = *src;
					c->filename = strdup(src->filename);
					if (src->name      != NULL) c->name      = strdup(src->name);
					if (src->info      != NULL) c->info      = strdup(src->info);
					c->next = NULL;

					/* Append GRF config to configuration list */
					*list = c;

					DeleteWindowByClass(WC_SAVELOAD);
					InvalidateWindowData(WC_GAME_OPTIONS, 0, 2);
				}
				break;

			case ANGRFW_RESCAN: // Rescan list
				ScanNewGRFFiles();
				this->InvalidateData();
				break;
		}
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(ANGRFW_FILTER);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		switch (keycode) {
			case WKC_UP:
				/* scroll up by one */
				if (this->sel_pos > 0) this->sel_pos--;
				break;

			case WKC_DOWN:
				/* scroll down by one */
				if (this->sel_pos < (int)this->grfs.Length() - 1) this->sel_pos++;
				break;

			case WKC_PAGEUP:
				/* scroll up a page */
				this->sel_pos = (this->sel_pos < this->vscroll.GetCapacity()) ? 0 : this->sel_pos - this->vscroll.GetCapacity();
				break;

			case WKC_PAGEDOWN:
				/* scroll down a page */
				this->sel_pos = min(this->sel_pos + this->vscroll.GetCapacity(), (int)this->grfs.Length() - 1);
				break;

			case WKC_HOME:
				/* jump to beginning */
				this->sel_pos = 0;
				break;

			case WKC_END:
				/* jump to end */
				this->sel_pos = this->grfs.Length() - 1;
				break;

			default: {
				/* Handle editbox input */
				EventState state = ES_NOT_HANDLED;
				if (this->HandleEditBoxKey(ANGRFW_FILTER, key, keycode, state) == HEBR_EDITING) {
					this->OnOSKInput(ANGRFW_FILTER);
				}
				return state;
			}
		}

		if (this->grfs.Length() == 0) this->sel_pos = -1;
		if (this->sel_pos >= 0) {
			this->sel = this->grfs[this->sel_pos];

			this->ScrollToSelected();
			this->InvalidateData(1);
		}

		return ES_HANDLED;
	}

	virtual void OnOSKInput(int wid)
	{
		this->grfs.SetFilterState(!StrEmpty(this->edit_str_buf));
		this->grfs.ForceRebuild();
		this->InvalidateData(1);
	}
};

Listing NewGRFAddWindow::last_sorting = {false, 0};
Filtering NewGRFAddWindow::last_filtering = {false, 0};

NewGRFAddWindow::GUIGRFConfigList::SortFunction * const NewGRFAddWindow::sorter_funcs[] = {
	&NameSorter,
};

NewGRFAddWindow::GUIGRFConfigList::FilterFunction * const NewGRFAddWindow::filter_funcs[] = {
	&TagNameFilter,
};

static const NWidgetPart _nested_newgrf_add_dlg_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, ANGRFW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, ANGRFW_CAPTION), SetDataTip(STR_NEWGRF_ADD_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, ANGRFW_FILTER_PANEL), SetResize(1, 0),
		NWidget(NWID_HORIZONTAL), SetPadding(WD_TEXTPANEL_TOP, 0, WD_TEXTPANEL_BOTTOM, 0), SetPIP(WD_FRAMETEXT_LEFT, WD_FRAMETEXT_RIGHT, WD_FRAMETEXT_RIGHT),
			NWidget(WWT_TEXT, COLOUR_GREY, ANGRFW_FILTER_TITLE), SetFill(false, true), SetDataTip(STR_LIST_FILTER_TITLE, STR_NULL),
			NWidget(WWT_EDITBOX, COLOUR_GREY, ANGRFW_FILTER), SetMinimalSize(100, 12), SetResize(1, 0), SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, ANGRFW_BACKGROUND),
			NWidget(WWT_INSET, COLOUR_GREY, ANGRFW_GRF_LIST), SetMinimalSize(290, 1), SetResize(1, 1), SetPadding(2, 2, 2, 2), EndContainer(),
		EndContainer(),
		NWidget(WWT_SCROLLBAR, COLOUR_GREY, ANGRFW_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, ANGRFW_GRF_INFO), SetMinimalSize(306, 1), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ANGRFW_ADD), SetMinimalSize(147, 12), SetResize(1, 0), SetDataTip(STR_NEWGRF_ADD_FILE, STR_NEWGRF_ADD_FILE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ANGRFW_RESCAN), SetMinimalSize(147, 12), SetResize(1, 0), SetDataTip(STR_NEWGRF_ADD_RESCAN_FILES, STR_NEWGRF_ADD_RESCAN_FILES_TOOLTIP),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY, ANGRFW_RESIZE), SetFill(false, true),
	EndContainer(),
};

/* Window definition for the add a newgrf window */
static const WindowDesc _newgrf_add_dlg_desc(
	WDP_CENTER, WDP_CENTER, 0, 0, 306, 347,
	WC_SAVELOAD, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	NULL, _nested_newgrf_add_dlg_widgets, lengthof(_nested_newgrf_add_dlg_widgets)
);

static GRFPresetList _grf_preset_list;

class DropDownListPresetItem : public DropDownListItem {
public:
	DropDownListPresetItem(int result) : DropDownListItem(result, false) {}

	virtual ~DropDownListPresetItem() {}

	bool Selectable() const
	{
		return true;
	}

	void Draw(int left, int right, int top, int bottom, bool sel, int bg_colour) const
	{
		DrawString(left + 2, right + 2, top, _grf_preset_list[this->result], sel ? TC_WHITE : TC_BLACK);
	}
};

static void NewGRFConfirmationCallback(Window *w, bool confirmed);

/** Names of the manage newgrfs window widgets. */
enum ShowNewGRFStateWidgets {
	SNGRFS_CLOSEBOX = 0,
	SNGRFS_CAPTION,
	SNGRFS_BACKGROUND1,
	SNGRFS_PRESET_LIST,
	SNGRFS_PRESET_SAVE,
	SNGRFS_PRESET_DELETE,
	SNGRFS_BACKGROUND2,
	SNGRFS_ADD,
	SNGRFS_REMOVE,
	SNGRFS_MOVE_UP,
	SNGRFS_MOVE_DOWN,
	SNGRFS_FILE_LIST,
	SNGRFS_SCROLLBAR,
	SNGRFS_NEWGRF_INFO,
	SNGRFS_SET_PARAMETERS,
	SNGRFS_TOGGLE_PALETTE,
	SNGRFS_APPLY_CHANGES,
	SNGRFS_CONTENT_DOWNLOAD,
	SNGRFS_RESIZE,
};

/**
 * Window for showing NewGRF files
 */
struct NewGRFWindow : public Window {
	GRFConfig **orig_list; ///< grf list the window is shown with
	GRFConfig *list;       ///< temporary grf list to which changes are made
	GRFConfig *sel;        ///< selected grf item
	bool editable;         ///< is the window editable
	bool show_params;      ///< are the grf-parameters shown in the info-panel
	bool execute;          ///< on pressing 'apply changes' are grf changes applied immediately, or only list is updated
	int query_widget;      ///< widget that opened a query
	int preset;            ///< selected preset

	NewGRFWindow(const WindowDesc *desc, bool editable, bool show_params, bool exec_changes, GRFConfig **config) : Window()
	{
		this->sel         = NULL;
		this->list        = NULL;
		this->orig_list   = config;
		this->editable    = editable;
		this->execute     = exec_changes;
		this->show_params = show_params;
		this->preset      = -1;

		CopyGRFConfigList(&this->list, *config, false);
		GetGRFPresetList(&_grf_preset_list);

		this->InitNested(desc);
		this->OnInvalidateData(2);
	}

	~NewGRFWindow()
	{
		if (this->editable && !this->execute) {
			CopyGRFConfigList(this->orig_list, this->list, true);
			ResetGRFConfig(false);
			ReloadNewGRFData();
		}

		/* Remove the temporary copy of grf-list used in window */
		ClearGRFConfigList(&this->list);
		_grf_preset_list.Clear();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		switch (widget) {
			case SNGRFS_FILE_LIST:
				resize->height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;
				size->height = max(size->height, 7 * resize->height);
				break;

			case SNGRFS_NEWGRF_INFO:
				size->height = max(size->height, WD_FRAMERECT_TOP + 10 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM + padding.height + 2);
				break;

			case SNGRFS_PRESET_LIST: {
				Dimension d = GetStringBoundingBox(STR_NUM_CUSTOM);
				for (uint i = 0; i < _grf_preset_list.Length(); i++) {
					if (_grf_preset_list[i] != NULL) {
						SetDParamStr(0, _grf_preset_list[i]);
						d = maxdim(d, GetStringBoundingBox(STR_JUST_RAW_STRING));
					}
				}
				d.width += padding.width;
				*size = maxdim(d, *size);
				break;
			}
		}
	}

	virtual void OnResize()
	{
		this->vscroll.SetCapacity(this->GetWidget<NWidgetCore>(SNGRFS_FILE_LIST)->current_y / this->resize.step_height);
		this->GetWidget<NWidgetCore>(SNGRFS_FILE_LIST)->widget_data = (this->vscroll.GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case SNGRFS_PRESET_LIST:
				if (this->preset == -1) {
					SetDParam(0, STR_NUM_CUSTOM);
				} else {
					SetDParam(0, STR_JUST_RAW_STRING);
					SetDParamStr(1, _grf_preset_list[this->preset]);
				}
				break;
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case SNGRFS_FILE_LIST: {
				uint y = r.top + WD_MATRIX_TOP;

				int i = 0;
				for (const GRFConfig *c = this->list; c != NULL; c = c->next, i++) {
					if (this->vscroll.IsVisible(i)) {
						const char *text = (c->name != NULL && !StrEmpty(c->name)) ? c->name : c->filename;
						SpriteID pal;

						/* Pick a colour */
						switch (c->status) {
							case GCS_NOT_FOUND:
							case GCS_DISABLED:
								pal = PALETTE_TO_RED;
								break;
							case GCS_ACTIVATED:
								pal = PALETTE_TO_GREEN;
								break;
							default:
								pal = PALETTE_TO_BLUE;
								break;
						}

						/* Do not show a "not-failure" colour when it actually failed to load */
						if (pal != PALETTE_TO_RED) {
							if (HasBit(c->flags, GCF_STATIC)) {
								pal = PALETTE_TO_GREY;
							} else if (HasBit(c->flags, GCF_COMPATIBLE)) {
								pal = PALETTE_TO_ORANGE;
							}
						}

						DrawSprite(SPR_SQUARE, pal, r.left + 5, y - 1);
						if (c->error != NULL) DrawSprite(SPR_WARNING_SIGN, 0, r.left + 20, y - 1);
						byte txtoffset = c->error != NULL ? 35 : 25;
						DrawString(r.left + txtoffset, r.right - WD_FRAMERECT_RIGHT, y, text, this->sel == c ? TC_WHITE : TC_BLACK);
						y += this->resize.step_height;
					}
				}
			} break;

			case SNGRFS_NEWGRF_INFO:
				if (this->sel != NULL) {
					ShowNewGRFInfo(this->sel, r.left + WD_FRAMERECT_LEFT, r.top + WD_FRAMERECT_TOP, r.right - WD_FRAMERECT_RIGHT, r.bottom - WD_FRAMERECT_BOTTOM, false);
				}
				break;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case SNGRFS_PRESET_LIST: {
				DropDownList *list = new DropDownList();

				/* Add 'None' option for clearing list */
				list->push_back(new DropDownListStringItem(STR_NONE, -1, false));

				for (uint i = 0; i < _grf_preset_list.Length(); i++) {
					if (_grf_preset_list[i] != NULL) {
						list->push_back(new DropDownListPresetItem(i));
					}
				}

				ShowDropDownList(this, list, this->preset, SNGRFS_PRESET_LIST);
				break;
			}

			case SNGRFS_PRESET_SAVE:
				this->query_widget = widget;
				ShowQueryString(STR_EMPTY, STR_NEWGRF_SETTINGS_PRESET_SAVE_QUERY, 32, 100, this, CS_ALPHANUMERAL, QSF_NONE);
				break;

			case SNGRFS_PRESET_DELETE:
				if (this->preset == -1) return;

				DeleteGRFPresetFromConfig(_grf_preset_list[this->preset]);
				GetGRFPresetList(&_grf_preset_list);
				this->preset = -1;
				this->InvalidateData();
				break;

			case SNGRFS_ADD: // Add GRF
				DeleteWindowByClass(WC_SAVELOAD);
				new NewGRFAddWindow(&_newgrf_add_dlg_desc, this, &this->list);
				break;

			case SNGRFS_REMOVE: { // Remove GRF
				GRFConfig **pc, *c, *newsel;

				/* Choose the next GRF file to be the selected file */
				newsel = this->sel->next;

				for (pc = &this->list; (c = *pc) != NULL; pc = &c->next) {
					/* If the new selection is empty (i.e. we're deleting the last item
					 * in the list, pick the file just before the selected file */
					if (newsel == NULL && c->next == this->sel) newsel = c;

					if (c == this->sel) {
						*pc = c->next;
						free(c);
						break;
					}
				}

				this->sel = newsel;
				this->preset = -1;
				this->InvalidateData();
				break;
			}

			case SNGRFS_MOVE_UP: { // Move GRF up
				GRFConfig **pc, *c;
				if (this->sel == NULL) break;

				for (pc = &this->list; (c = *pc) != NULL; pc = &c->next) {
					if (c->next == this->sel) {
						c->next = this->sel->next;
						this->sel->next = c;
						*pc = this->sel;
						break;
					}
				}
				this->preset = -1;
				this->InvalidateData();
				break;
			}

			case SNGRFS_MOVE_DOWN: { // Move GRF down
				GRFConfig **pc, *c;
				if (this->sel == NULL) break;

				for (pc = &this->list; (c = *pc) != NULL; pc = &c->next) {
					if (c == this->sel) {
						*pc = c->next;
						c->next = c->next->next;
						(*pc)->next = c;
						break;
					}
				}
				this->preset = -1;
				this->InvalidateData();
				break;
			}

			case SNGRFS_FILE_LIST: { // Select a GRF
				GRFConfig *c;
				uint i = (pt.y - this->GetWidget<NWidgetBase>(SNGRFS_FILE_LIST)->pos_y) / this->resize.step_height + this->vscroll.GetPosition();

				for (c = this->list; c != NULL && i > 0; c = c->next, i--) {}
				this->sel = c;

				this->InvalidateData();
				break;
			}

			case SNGRFS_APPLY_CHANGES: // Apply changes made to GRF list
				if (this->execute) {
					ShowQuery(
						STR_NEWGRF_POPUP_CAUTION_CAPTION,
						STR_NEWGRF_CONFIRMATION_TEXT,
						this,
						NewGRFConfirmationCallback
					);
				} else {
					CopyGRFConfigList(this->orig_list, this->list, true);
					ResetGRFConfig(false);
					ReloadNewGRFData();
				}
				break;

			case SNGRFS_SET_PARAMETERS: { // Edit parameters
				if (this->sel == NULL) break;

				this->query_widget = widget;
				static char buff[512];
				GRFBuildParamList(buff, this->sel, lastof(buff));
				SetDParamStr(0, buff);
				ShowQueryString(STR_JUST_RAW_STRING, STR_NEWGRF_SETTINGS_PARAMETER_QUERY, 63, 250, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
			}

			case SNGRFS_TOGGLE_PALETTE:
				if (this->sel != NULL) {
					this->sel->windows_paletted ^= true;
					this->SetDirty();
				}
				break;

			case SNGRFS_CONTENT_DOWNLOAD:
				if (!_network_available) {
					ShowErrorMessage(INVALID_STRING_ID, STR_NETWORK_ERROR_NOTAVAILABLE, 0, 0);
				} else {
#if defined(ENABLE_NETWORK)
				/* Only show the things in the current list, or everything when nothing's selected */
					ContentVector cv;
					for (const GRFConfig *c = this->list; c != NULL; c = c->next) {
						if (c->status != GCS_NOT_FOUND && !HasBit(c->flags, GCF_COMPATIBLE)) continue;

						ContentInfo *ci = new ContentInfo();
						ci->type = CONTENT_TYPE_NEWGRF;
						ci->state = ContentInfo::DOES_NOT_EXIST;
						ttd_strlcpy(ci->name, c->name != NULL ? c->name : c->filename, lengthof(ci->name));
						ci->unique_id = BSWAP32(c->grfid);
						memcpy(ci->md5sum, c->md5sum, sizeof(ci->md5sum));
						if (HasBit(c->flags, GCF_COMPATIBLE)) GamelogGetOriginalGRFMD5Checksum(c->grfid, ci->md5sum);
						*cv.Append() = ci;
					}
					ShowNetworkContentListWindow(cv.Length() == 0 ? NULL : &cv, CONTENT_TYPE_NEWGRF);
#endif
				}
				break;
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		if (index == -1) {
			ClearGRFConfigList(&this->list);
			this->preset = -1;
		} else {
			GRFConfig *c = LoadGRFPresetFromConfig(_grf_preset_list[index]);

			if (c != NULL) {
				this->sel = NULL;
				ClearGRFConfigList(&this->list);
				this->list = c;
				this->preset = index;
			}
		}

		this->sel = NULL;
		this->InvalidateData(3);
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		switch (this->query_widget) {
			default: NOT_REACHED();

			case SNGRFS_PRESET_SAVE:
				SaveGRFPresetToConfig(str, this->list);
				GetGRFPresetList(&_grf_preset_list);

				/* Switch to this preset */
				for (uint i = 0; i < _grf_preset_list.Length(); i++) {
					if (_grf_preset_list[i] != NULL && strcmp(_grf_preset_list[i], str) == 0) {
						this->preset = i;
						break;
					}
				}
				break;

			case SNGRFS_SET_PARAMETERS: {
				/* Parse our new "int list" */
				GRFConfig *c = this->sel;
				c->num_params = parse_intlist(str, (int*)c->param, lengthof(c->param));

				/* parse_intlist returns -1 on error */
				if (c->num_params == (byte)-1) c->num_params = 0;

				this->preset = -1;
				break;
			}
		}

		this->InvalidateData();
	}

	virtual void OnInvalidateData(int data = 0)
	{
		switch (data) {
			default: NOT_REACHED();
			case 0:
				/* Nothing important to do */
				break;

			case 1:
				/* Search the list for items that are now found and mark them as such. */
				for (GRFConfig *c = this->list; c != NULL; c = c->next) {
					if (c->status != GCS_NOT_FOUND) continue;

					const GRFConfig *f = FindGRFConfig(c->grfid, c->md5sum);
					if (f == NULL) continue;

					free(c->filename);
					free(c->name);
					free(c->info);

					c->filename  = f->filename == NULL ? NULL : strdup(f->filename);
					c->name      = f->name == NULL ? NULL : strdup(f->name);;
					c->info      = f->info == NULL ? NULL : strdup(f->info);;
					c->status    = GCS_UNKNOWN;
				}
				break;

			case 2:
				this->preset = -1;
				/* Fall through */
			case 3:
				const GRFConfig *c;
				int i;

				for (c = this->list, i = 0; c != NULL; c = c->next, i++) {}

				this->vscroll.SetCapacity((this->GetWidget<NWidgetBase>(SNGRFS_FILE_LIST)->current_y) / this->resize.step_height);
				this->GetWidget<NWidgetCore>(SNGRFS_FILE_LIST)->widget_data = (this->vscroll.GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
				this->vscroll.SetCount(i);
				break;
		}

		this->SetWidgetsDisabledState(!this->editable,
			SNGRFS_PRESET_LIST,
			SNGRFS_ADD,
			SNGRFS_APPLY_CHANGES,
			SNGRFS_TOGGLE_PALETTE,
			WIDGET_LIST_END
		);

		bool disable_all = this->sel == NULL || !this->editable;
		this->SetWidgetsDisabledState(disable_all,
			SNGRFS_REMOVE,
			SNGRFS_MOVE_UP,
			SNGRFS_MOVE_DOWN,
			WIDGET_LIST_END
		);
		this->SetWidgetDisabledState(SNGRFS_SET_PARAMETERS, !this->show_params || disable_all);
		this->SetWidgetDisabledState(SNGRFS_TOGGLE_PALETTE, disable_all);

		if (!disable_all) {
			/* All widgets are now enabled, so disable widgets we can't use */
			if (this->sel == this->list)       this->DisableWidget(SNGRFS_MOVE_UP);
			if (this->sel->next == NULL)       this->DisableWidget(SNGRFS_MOVE_DOWN);
			if (this->sel->IsOpenTTDBaseGRF()) this->DisableWidget(SNGRFS_REMOVE);
		}

		this->SetWidgetDisabledState(SNGRFS_PRESET_DELETE, this->preset == -1);

		bool has_missing = false;
		bool has_compatible = false;
		for (const GRFConfig *c = this->list; !has_missing && c != NULL; c = c->next) {
			has_missing    |= c->status == GCS_NOT_FOUND;
			has_compatible |= HasBit(c->flags, GCF_COMPATIBLE);
		}
		if (has_missing || has_compatible) {
			this->GetWidget<NWidgetCore>(SNGRFS_CONTENT_DOWNLOAD)->widget_data = STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_BUTTON;
			this->GetWidget<NWidgetCore>(SNGRFS_CONTENT_DOWNLOAD)->tool_tip    = STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_TOOLTIP;
		} else {
			this->GetWidget<NWidgetCore>(SNGRFS_CONTENT_DOWNLOAD)->widget_data = STR_INTRO_ONLINE_CONTENT;
			this->GetWidget<NWidgetCore>(SNGRFS_CONTENT_DOWNLOAD)->tool_tip    = STR_INTRO_TOOLTIP_ONLINE_CONTENT;
		}
		this->SetWidgetDisabledState(SNGRFS_PRESET_SAVE, has_missing);
	}
};

/* Widget definition of the manage newgrfs window */
static const NWidgetPart _nested_newgrf_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE, SNGRFS_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_MAUVE, SNGRFS_CAPTION), SetDataTip(STR_NEWGRF_SETTINGS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE, SNGRFS_BACKGROUND1),
		NWidget(NWID_HORIZONTAL), SetPadding(2, 10, 2, 10),
			NWidget(WWT_DROPDOWN, COLOUR_YELLOW, SNGRFS_PRESET_LIST), SetFill(true, false), SetResize(1, 0), SetDataTip(STR_JUST_STRING, STR_NEWGRF_SETTINGS_PRESET_LIST_TOOLTIP),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_PRESET_SAVE), SetFill(true, false), SetDataTip(STR_NEWGRF_SETTINGS_PRESET_SAVE, STR_NEWGRF_SETTINGS_PRESET_SAVE_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_PRESET_DELETE), SetFill(true, false), SetDataTip(STR_NEWGRF_SETTINGS_PRESET_DELETE, STR_NEWGRF_SETTINGS_PRESET_DELETE_TOOLTIP),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE, SNGRFS_BACKGROUND2),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPadding(2, 10, 2, 10),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_ADD), SetFill(true, false), SetResize(1, 0), SetDataTip(STR_NEWGRF_SETTINGS_ADD, STR_NEWGRF_SETTINGS_ADD_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_REMOVE), SetFill(true, false), SetResize(1, 0), SetDataTip(STR_NEWGRF_SETTINGS_REMOVE, STR_NEWGRF_SETTINGS_REMOVE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_MOVE_UP), SetFill(true, false), SetResize(1, 0), SetDataTip(STR_NEWGRF_SETTINGS_MOVEUP, STR_NEWGRF_SETTINGS_MOVEUP_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_MOVE_DOWN), SetFill(true, false), SetResize(1, 0), SetDataTip(STR_NEWGRF_SETTINGS_MOVEDOWN, STR_NEWGRF_SETTINGS_MOVEDOWN_TOOLTIP),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_MAUVE, SNGRFS_FILE_LIST), SetFill(true, false), SetDataTip(0x501, STR_NEWGRF_SETTINGS_FILE_TOOLTIP), SetResize(1, 0),
		NWidget(WWT_SCROLLBAR, COLOUR_MAUVE, SNGRFS_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE, SNGRFS_NEWGRF_INFO), SetFill(true, false), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, SNGRFS_SET_PARAMETERS), SetFill(true, false), SetResize(1, 0), SetDataTip(STR_NEWGRF_SETTINGS_SET_PARAMETERS, STR_NULL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, SNGRFS_TOGGLE_PALETTE), SetFill(true, false), SetResize(1, 0), SetDataTip(STR_NEWGRF_SETTINGS_TOGGLE_PALETTE, STR_NEWGRF_SETTINGS_TOGGLE_PALETTE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, SNGRFS_APPLY_CHANGES), SetFill(true, false), SetResize(1, 0), SetDataTip(STR_NEWGRF_SETTINGS_APPLY_CHANGES, STR_NULL),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, SNGRFS_CONTENT_DOWNLOAD), SetFill(true, false), SetResize(1, 0), SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
		NWidget(WWT_RESIZEBOX, COLOUR_MAUVE, SNGRFS_RESIZE),
	EndContainer(),
};

/* Window definition of the manage newgrfs window */
static const WindowDesc _newgrf_desc(
	WDP_CENTER, WDP_CENTER, 300, 263, 300, 263,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	NULL, _nested_newgrf_widgets, lengthof(_nested_newgrf_widgets)
);

/** Callback function for the newgrf 'apply changes' confirmation window
 * @param w Window which is calling this callback
 * @param confirmed boolean value, true when yes was clicked, false otherwise
 */
static void NewGRFConfirmationCallback(Window *w, bool confirmed)
{
	if (confirmed) {
		NewGRFWindow *nw = dynamic_cast<NewGRFWindow*>(w);
		GRFConfig *c;
		int i = 0;

		GamelogStartAction(GLAT_GRF);
		GamelogGRFUpdate(_grfconfig, nw->list); // log GRF changes
		CopyGRFConfigList(nw->orig_list, nw->list, false);
		ReloadNewGRFData();
		GamelogStopAction();

		/* Show new, updated list */
		for (c = nw->list; c != NULL && c != nw->sel; c = c->next, i++) {}
		CopyGRFConfigList(&nw->list, *nw->orig_list, false);
		for (c = nw->list; c != NULL && i > 0; c = c->next, i--) {}
		nw->sel = c;

		w->SetDirty();
	}
}



/** Setup the NewGRF gui
 * @param editable allow the user to make changes to the grfconfig in the window
 * @param show_params show information about what parameters are set for the grf files
 * @param exec_changes if changes are made to the list (editable is true), apply these
 *        changes immediately or only update the list
 * @param config pointer to a linked-list of grfconfig's that will be shown */
void ShowNewGRFSettings(bool editable, bool show_params, bool exec_changes, GRFConfig **config)
{
	DeleteWindowByClass(WC_GAME_OPTIONS);
	new NewGRFWindow(&_newgrf_desc, editable, show_params, exec_changes, config);
}
