/* $Id$ */

/** @file newgrf_gui.cpp GUI to change NewGRF settings. */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "newgrf.h"
#include "newgrf_config.h"
#include "strings_func.h"
#include "window_func.h"
#include "core/alloc_func.hpp"
#include "string_func.h"
#include "gfx_func.h"

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
		v = strtol(p, &end, 0);
		if (p == end || n == maxitems) return -1;
		p = end;
		items[n++] = v;
		if (*p == '\0') break;
		if (*p != ',' && *p != ' ') return -1;
		p++;
	}

	return n;
}


static void ShowNewGRFInfo(const GRFConfig *c, uint x, uint y, uint w, uint bottom, bool show_params)
{
	char buff[256];

	if (c->error != NULL) {
		SetDParamStr(0, c->filename);
		SetDParamStr(1, c->error->data);
		for (uint i = 0; i < c->error->num_params; i++) {
			uint32 param = 0;
			byte param_number = c->error->param_number[i];

			if (param_number < c->num_params) param = c->param[param_number];

			SetDParam(2 + i, param);
		}

		char message[512];
		GetString(message, c->error->custom_message != NULL ? BindCString(c->error->custom_message) : c->error->message, lastof(message));

		SetDParamStr(0, message);
		y += DrawStringMultiLine(x, y, c->error->severity, w, bottom - y);
	}

	/* Draw filename or not if it is not known (GRF sent over internet) */
	if (c->filename != NULL) {
		SetDParamStr(0, c->filename);
		y += DrawStringMultiLine(x, y, STR_NEWGRF_FILENAME, w, bottom - y);
	}

	/* Prepare and draw GRF ID */
	snprintf(buff, lengthof(buff), "%08X", BSWAP32(c->grfid));
	SetDParamStr(0, buff);
	y += DrawStringMultiLine(x, y, STR_NEWGRF_GRF_ID, w, bottom - y);

	/* Prepare and draw MD5 sum */
	md5sumToString(buff, lastof(buff), c->md5sum);
	SetDParamStr(0, buff);
	y += DrawStringMultiLine(x, y, STR_NEWGRF_MD5SUM, w, bottom - y);

	/* Show GRF parameter list */
	if (show_params) {
		if (c->num_params > 0) {
			GRFBuildParamList(buff, c, lastof(buff));
			SetDParamStr(0, buff);
		} else {
			SetDParam(0, STR_01A9_NONE);
		}
		y += DrawStringMultiLine(x, y, STR_NEWGRF_PARAMETER, w, bottom - y);
	}

	/* Show flags */
	if (c->status == GCS_NOT_FOUND)        y += DrawStringMultiLine(x, y, STR_NEWGRF_NOT_FOUND, w, bottom - y);
	if (c->status == GCS_DISABLED)         y += DrawStringMultiLine(x, y, STR_NEWGRF_DISABLED, w, bottom - y);
	if (HasBit(c->flags, GCF_COMPATIBLE)) y += DrawStringMultiLine(x, y, STR_NEWGRF_COMPATIBLE_LOADED, w, bottom - y);

	/* Draw GRF info if it exists */
	if (c->info != NULL && !StrEmpty(c->info)) {
		SetDParamStr(0, c->info);
		y += DrawStringMultiLine(x, y, STR_02BD, w, bottom - y);
	} else {
		y += DrawStringMultiLine(x, y, STR_NEWGRF_NO_INFO, w, bottom - y);
	}
}


/**
 * Window for adding NewGRF files
 */
struct NewGRFAddWindow : public Window {
	/* Names of the add a newgrf window widgets */
	enum AddNewGRFWindowWidgets {
		ANGRFW_CLOSEBOX = 0,
		ANGRFW_CAPTION,
		ANGRFW_BACKGROUND,
		ANGRFW_GRF_LIST,
		ANGRFW_SCROLLBAR,
		ANGRFW_GRF_INFO,
		ANGRFW_ADD,
		ANGRFW_RESCAN,
		ANGRFW_RESIZE,
	};

	GRFConfig **list;
	const GRFConfig *sel;

	NewGRFAddWindow(const WindowDesc *desc, GRFConfig **list) : Window(desc, 0)
	{
		this->list = list;
		this->resize.step_height = 10;

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		const GRFConfig *c;
		const Widget *wl = &this->widget[ANGRFW_GRF_LIST];
		int n = 0;

		/* Count the number of GRFs */
		for (c = _all_grfs; c != NULL; c = c->next) n++;

		this->vscroll.cap = (wl->bottom - wl->top) / 10;
		SetVScrollCount(this, n);

		this->SetWidgetDisabledState(ANGRFW_ADD, this->sel == NULL || this->sel->IsOpenTTDBaseGRF());
		this->DrawWidgets();

		GfxFillRect(wl->left + 1, wl->top + 1, wl->right, wl->bottom, 0xD7);

		uint y = wl->top + 1;
		for (c = _all_grfs, n = 0; c != NULL && n < (this->vscroll.pos + this->vscroll.cap); c = c->next, n++) {
			if (n >= this->vscroll.pos) {
				bool h = c == this->sel;
				const char *text = (c->name != NULL && !StrEmpty(c->name)) ? c->name : c->filename;

				/* Draw selection background */
				if (h) GfxFillRect(3, y, this->width - 15, y + 9, 156);
				DoDrawStringTruncated(text, 4, y, h ? TC_WHITE : TC_ORANGE, this->width - 18);
				y += 10;
			}
		}

		if (this->sel != NULL) {
			const Widget *wi = &this->widget[ANGRFW_GRF_INFO];
			ShowNewGRFInfo(this->sel, wi->left + 2, wi->top + 2, wi->right - wi->left - 2, wi->bottom, false);
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
				const GRFConfig *c;
				uint i = (pt.y - this->widget[ANGRFW_GRF_LIST].top) / 10 + this->vscroll.pos;

				for (c = _all_grfs; c != NULL && i > 0; c = c->next, i--) {}
				this->sel = c;
				this->SetDirty();
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
					InvalidateWindowData(WC_GAME_OPTIONS, 0);
				}
				break;

			case ANGRFW_RESCAN: // Rescan list
				this->sel = NULL;
				ScanNewGRFFiles();
				this->SetDirty();
				break;
		}
	}
};

/* Widget definition for the add a newgrf window */
static const Widget _newgrf_add_dlg_widgets[] = {
{   WWT_CLOSEBOX,    RESIZE_NONE, 14,   0,  10,   0,  13, STR_00C5,                STR_018B_CLOSE_WINDOW },           // ANGRFW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_RIGHT, 14,  11, 306,   0,  13, STR_NEWGRF_ADD_CAPTION,  STR_018C_WINDOW_TITLE_DRAG_THIS }, // ANGRFW_CAPTION
{      WWT_PANEL,      RESIZE_RB, 14,   0, 294,  14, 121, 0x0,                     STR_NULL },                        // ANGRFW_BACKGROUND
{      WWT_INSET,      RESIZE_RB, 14,   2, 292,  16, 119, 0x0,                     STR_NULL },                        // ANGRFW_GRF_LIST
{  WWT_SCROLLBAR,     RESIZE_LRB, 14, 295, 306,  14, 121, 0x0,                     STR_NULL },                        // ANGRFW_SCROLLBAR
{      WWT_PANEL,     RESIZE_RTB, 14,   0, 306, 122, 224, 0x0,                     STR_NULL },                        // ANGRFW_GRF_INFO
{ WWT_PUSHTXTBTN,     RESIZE_RTB, 14,   0, 146, 225, 236, STR_NEWGRF_ADD_FILE,     STR_NEWGRF_ADD_FILE_TIP },         // ANGRFW_ADD
{ WWT_PUSHTXTBTN,    RESIZE_LRTB, 14, 147, 294, 225, 236, STR_NEWGRF_RESCAN_FILES, STR_NEWGRF_RESCAN_FILES_TIP },     // ANGRFW_RESCAN
{  WWT_RESIZEBOX,    RESIZE_LRTB, 14, 295, 306, 225, 236, 0x0,                     STR_RESIZE_BUTTON },               // ANGRFW_RESIZE
{   WIDGETS_END },
};

/* Window definition for the add a newgrf window */
static const WindowDesc _newgrf_add_dlg_desc = {
	WDP_CENTER, WDP_CENTER, 307, 237, 307, 337,
	WC_SAVELOAD, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_newgrf_add_dlg_widgets,
	NULL,
};


static void NewGRFConfirmationCallback(Window *w, bool confirmed);

/**
 * Window for showing NewGRF files
 */
struct NewGRFWindow : public Window {
	/* Names of the manage newgrfs window widgets */
	enum ShowNewGRFStateWidgets {
		SNGRFS_CLOSEBOX = 0,
		SNGRFS_CAPTION,
		SNGRFS_BACKGROUND,
		SNGRFS_ADD,
		SNGRFS_REMOVE,
		SNGRFS_MOVE_UP,
		SNGRFS_MOVE_DOWN,
		SNGRFS_FILE_LIST,
		SNGRFS_SCROLLBAR,
		SNGRFS_NEWGRF_INFO,
		SNGRFS_SET_PARAMETERS,
		SNGRFS_APPLY_CHANGES,
		SNGRFS_RESIZE,
	};

	GRFConfig **orig_list; ///< grf list the window is shown with
	GRFConfig *list;       ///< temporary grf list to which changes are made
	GRFConfig *sel;        ///< selected grf item
	bool editable;         ///< is the window editable
	bool show_params;      ///< are the grf-parameters shown in the info-panel
	bool execute;          ///< on pressing 'apply changes' are grf changes applied immediately, or only list is updated

	NewGRFWindow(const WindowDesc *desc, bool editable, bool show_params, bool exec_changes, GRFConfig **config) : Window(desc, 0)
	{
		this->resize.step_height = 14;
		this->sel         = NULL;
		this->list        = NULL;
		this->orig_list   = config;
		this->editable    = editable;
		this->execute     = exec_changes;
		this->show_params = show_params;

		CopyGRFConfigList(&this->list, *config, false);

		this->FindWindowPlacementAndResize(desc);
		this->SetupNewGRFWindow();
	}

	~NewGRFWindow()
	{
		if (!this->execute) {
			CopyGRFConfigList(this->orig_list, this->list, true);
			ResetGRFConfig(false);
			ReloadNewGRFData();
		}

		/* Remove the temporary copy of grf-list used in window */
		ClearGRFConfigList(&this->list);
	}

	void SetupNewGRFWindow()
	{
		const GRFConfig *c;
		int i;

		for (c = this->list, i = 0; c != NULL; c = c->next, i++) {}

		this->vscroll.cap = (this->widget[SNGRFS_FILE_LIST].bottom - this->widget[SNGRFS_FILE_LIST].top) / 14 + 1;
		SetVScrollCount(this, i);

		this->SetWidgetDisabledState(SNGRFS_ADD, !this->editable);
		this->SetWidgetDisabledState(SNGRFS_APPLY_CHANGES, !this->editable);
	}

	virtual void OnPaint()
	{
		bool disable_all = this->sel == NULL || !this->editable;

		this->SetWidgetsDisabledState(disable_all,
			SNGRFS_REMOVE,
			SNGRFS_MOVE_UP,
			SNGRFS_MOVE_DOWN,
			WIDGET_LIST_END
		);
		this->SetWidgetDisabledState(SNGRFS_SET_PARAMETERS, !this->show_params || disable_all);

		if (!disable_all) {
			/* All widgets are now enabled, so disable widgets we can't use */
			if (this->sel == this->list)       this->DisableWidget(SNGRFS_MOVE_UP);
			if (this->sel->next == NULL)       this->DisableWidget(SNGRFS_MOVE_DOWN);
			if (this->sel->IsOpenTTDBaseGRF()) this->DisableWidget(SNGRFS_REMOVE);
		}

		this->DrawWidgets();

		/* Draw NewGRF list */
		int y = this->widget[SNGRFS_FILE_LIST].top;
		int i = 0;
		for (const GRFConfig *c = this->list; c != NULL; c = c->next, i++) {
			if (i >= this->vscroll.pos && i < this->vscroll.pos + this->vscroll.cap) {
				const char *text = (c->name != NULL && !StrEmpty(c->name)) ? c->name : c->filename;
				SpriteID pal;
				byte txtoffset;

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

				DrawSprite(SPR_SQUARE, pal, 5, y + 2);
				if (c->error != NULL) DrawSprite(SPR_WARNING_SIGN, 0, 20, y + 2);
				txtoffset = c->error != NULL ? 35 : 25;
				DoDrawStringTruncated(text, txtoffset, y + 3, this->sel == c ? TC_WHITE : TC_BLACK, this->width - txtoffset - 10);
				y += 14;
			}
		}

		if (this->sel != NULL) {
			/* Draw NewGRF file info */
			const Widget *wi = &this->widget[SNGRFS_NEWGRF_INFO];
			ShowNewGRFInfo(this->sel, wi->left + 2, wi->top + 2, wi->right - wi->left - 2, wi->bottom, this->show_params);
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case SNGRFS_ADD: // Add GRF
				DeleteWindowByClass(WC_SAVELOAD);
				new NewGRFAddWindow(&_newgrf_add_dlg_desc, &this->list);
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
				this->SetupNewGRFWindow();
				this->SetDirty();
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
				this->SetDirty();
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
				this->SetDirty();
				break;
			}

			case SNGRFS_FILE_LIST: { // Select a GRF
				GRFConfig *c;
				uint i = (pt.y - this->widget[SNGRFS_FILE_LIST].top) / 14 + this->vscroll.pos;

				for (c = this->list; c != NULL && i > 0; c = c->next, i--) {}
				this->sel = c;

				this->SetDirty();
				break;
			}

			case SNGRFS_APPLY_CHANGES: // Apply changes made to GRF list
				if (this->execute) {
					ShowQuery(
						STR_POPUP_CAUTION_CAPTION,
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

				char buff[512];
				GRFBuildParamList(buff, this->sel, lastof(buff));
				ShowQueryString(BindCString(buff), STR_NEWGRF_PARAMETER_QUERY, 63, 250, this, CS_ALPHANUMERAL);
				break;
			}
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		/* Parse our new "int list" */
		GRFConfig *c = this->sel;
		c->num_params = parse_intlist(str, (int*)c->param, lengthof(c->param));

		/* parse_intlist returns -1 on error */
		if (c->num_params == (byte)-1) c->num_params = 0;

		this->SetDirty();
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		if (delta.x != 0) {
			ResizeButtons(this, SNGRFS_ADD, SNGRFS_MOVE_DOWN);
			ResizeButtons(this, SNGRFS_SET_PARAMETERS, SNGRFS_APPLY_CHANGES);
		}

		this->vscroll.cap += delta.y / 14;
		this->widget[SNGRFS_FILE_LIST].data = (this->vscroll.cap << 8) + 1;

		this->SetupNewGRFWindow();
	}

	virtual void OnInvalidateData(int data = 0)
	{
		this->SetupNewGRFWindow();
	}
};

/* Widget definition of the manage newgrfs window */
static const Widget _newgrf_widgets[] = {
{   WWT_CLOSEBOX,  RESIZE_NONE, 10,   0,  10,   0,  13, STR_00C5,                    STR_018B_CLOSE_WINDOW },            // SNGRFS_CLOSEBOX
{    WWT_CAPTION, RESIZE_RIGHT, 10,  11, 299,   0,  13, STR_NEWGRF_SETTINGS_CAPTION, STR_018C_WINDOW_TITLE_DRAG_THIS },  // SNGRFS_CAPTION
{      WWT_PANEL, RESIZE_RIGHT, 10,   0, 299,  14,  29, STR_NULL,                    STR_NULL },                         // SNGRFS_BACKGROUND
{ WWT_PUSHTXTBTN,  RESIZE_NONE,  3,  10,  79,  16,  27, STR_NEWGRF_ADD,              STR_NEWGRF_ADD_TIP },               // SNGRFS_ADD
{ WWT_PUSHTXTBTN,  RESIZE_NONE,  3,  80, 149,  16,  27, STR_NEWGRF_REMOVE,           STR_NEWGRF_REMOVE_TIP },            // SNGRFS_REMOVE
{ WWT_PUSHTXTBTN,  RESIZE_NONE,  3, 150, 219,  16,  27, STR_NEWGRF_MOVEUP,           STR_NEWGRF_MOVEUP_TIP },            // SNGRFS_MOVE_UP
{ WWT_PUSHTXTBTN, RESIZE_RIGHT,  3, 220, 289,  16,  27, STR_NEWGRF_MOVEDOWN,         STR_NEWGRF_MOVEDOWN_TIP },          // SNGRFS_MOVE_DOWN
{     WWT_MATRIX,    RESIZE_RB, 10,   0, 287,  30,  99, 0x501,                       STR_NEWGRF_FILE_TIP },              // SNGRFS_FILE_LIST
{  WWT_SCROLLBAR,   RESIZE_LRB, 10, 288, 299,  30,  99, 0x0,                         STR_0190_SCROLL_BAR_SCROLLS_LIST }, // SNGRFS_SCROLLBAR
{      WWT_PANEL,   RESIZE_RTB, 10,   0, 299, 100, 212, STR_NULL,                    STR_NULL },                         // SNGRFS_NEWGRF_INFO
{ WWT_PUSHTXTBTN,    RESIZE_TB, 10,   0, 143, 213, 224, STR_NEWGRF_SET_PARAMETERS,   STR_NULL },                         // SNGRFS_SET_PARAMETERS
{ WWT_PUSHTXTBTN,   RESIZE_RTB, 10, 144, 287, 213, 224, STR_NEWGRF_APPLY_CHANGES,    STR_NULL },                         // SNGRFS_APPLY_CHANGES
{  WWT_RESIZEBOX,  RESIZE_LRTB, 10, 288, 299, 213, 224, 0x0,                         STR_RESIZE_BUTTON },                // SNGRFS_RESIZE
{ WIDGETS_END },
};

/* Window definition of the manage newgrfs window */
static const WindowDesc _newgrf_desc = {
	WDP_CENTER, WDP_CENTER, 300, 225, 300, 225,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_newgrf_widgets,
	NULL,
};

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

		CopyGRFConfigList(nw->orig_list, nw->list, false);
		ReloadNewGRFData();

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
