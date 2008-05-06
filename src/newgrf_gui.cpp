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


/* Dialogue for adding NewGRF files to the selection */
struct newgrf_add_d {
	GRFConfig **list;
	const GRFConfig *sel;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(newgrf_add_d));

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

static void NewGRFAddDlgWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			const GRFConfig *c;
			const Widget *wl = &w->widget[ANGRFW_GRF_LIST];
			int n = 0;

			/* Count the number of GRFs */
			for (c = _all_grfs; c != NULL; c = c->next) n++;

			w->vscroll.cap = (wl->bottom - wl->top) / 10;
			SetVScrollCount(w, n);

			w->SetWidgetDisabledState(ANGRFW_ADD, WP(w, newgrf_add_d).sel == NULL || WP(w, newgrf_add_d).sel->IsOpenTTDBaseGRF());
			DrawWindowWidgets(w);

			GfxFillRect(wl->left + 1, wl->top + 1, wl->right, wl->bottom, 0xD7);

			uint y = wl->top + 1;
			for (c = _all_grfs, n = 0; c != NULL && n < (w->vscroll.pos + w->vscroll.cap); c = c->next, n++) {
				if (n >= w->vscroll.pos) {
					bool h = c == WP(w, newgrf_add_d).sel;
					const char *text = (c->name != NULL && !StrEmpty(c->name)) ? c->name : c->filename;

					/* Draw selection background */
					if (h) GfxFillRect(3, y, w->width - 15, y + 9, 156);
					DoDrawStringTruncated(text, 4, y, h ? TC_WHITE : TC_ORANGE, w->width - 18);
					y += 10;
				}
			}

			if (WP(w, newgrf_add_d).sel != NULL) {
				const Widget *wi = &w->widget[ANGRFW_GRF_INFO];
				ShowNewGRFInfo(WP(w, newgrf_add_d).sel, wi->left + 2, wi->top + 2, wi->right - wi->left - 2, wi->bottom, false);
			}
			break;
		}

		case WE_DOUBLE_CLICK:
			if (e->we.click.widget != ANGRFW_GRF_LIST) break;
			e->we.click.widget = ANGRFW_ADD;
			/* Fall through */

		case WE_CLICK:
			switch (e->we.click.widget) {
				case ANGRFW_GRF_LIST: {
					/* Get row... */
					const GRFConfig *c;
					uint i = (e->we.click.pt.y - w->widget[ANGRFW_GRF_LIST].top) / 10 + w->vscroll.pos;

					for (c = _all_grfs; c != NULL && i > 0; c = c->next, i--) {}
					WP(w, newgrf_add_d).sel = c;
					SetWindowDirty(w);
					break;
				}

				case ANGRFW_ADD: // Add selection to list
					if (WP(w, newgrf_add_d).sel != NULL) {
						const GRFConfig *src = WP(w, newgrf_add_d).sel;
						GRFConfig **list;

						/* Find last entry in the list, checking for duplicate grfid on the way */
						for (list = WP(w, newgrf_add_d).list; *list != NULL; list = &(*list)->next) {
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
					WP(w, newgrf_add_d).sel = NULL;
					ScanNewGRFFiles();
					SetWindowDirty(w);
					break;
			}
			break;
	}
}

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
	NewGRFAddDlgWndProc,
};


/* 'NewGRF Settings' dialogue */
struct newgrf_d {
	GRFConfig **orig_list; ///< grf list the window is shown with
	GRFConfig **list;      ///< temporary grf list to which changes are made
	GRFConfig *sel;        ///< selected grf item
	bool editable;         ///< is the window editable
	bool show_params;      ///< are the grf-parameters shown in the info-panel
	bool execute;          ///< on pressing 'apply changes' are grf changes applied immediately, or only list is updated
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(newgrf_d));


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

static void SetupNewGRFState(Window *w)
{
	bool disable_all = WP(w, newgrf_d).sel == NULL || !WP(w, newgrf_d).editable;

	w->SetWidgetDisabledState(SNGRFS_ADD, !WP(w, newgrf_d).editable);
	w->SetWidgetsDisabledState(disable_all,
		SNGRFS_REMOVE,
		SNGRFS_MOVE_UP,
		SNGRFS_MOVE_DOWN,
		WIDGET_LIST_END
	);
	w->SetWidgetDisabledState(SNGRFS_SET_PARAMETERS, !WP(w, newgrf_d).show_params || disable_all);

	if (!disable_all) {
		/* All widgets are now enabled, so disable widgets we can't use */
		if (WP(w, newgrf_d).sel == *WP(w, newgrf_d).list) w->DisableWidget(SNGRFS_MOVE_UP);
		if (WP(w, newgrf_d).sel->next == NULL) w->DisableWidget(SNGRFS_MOVE_DOWN);
		if (WP(w, newgrf_d).sel->IsOpenTTDBaseGRF()) w->DisableWidget(SNGRFS_REMOVE);
	}
}


static void SetupNewGRFWindow(Window *w)
{
	const GRFConfig *c;
	int i;

	for (c = *WP(w, newgrf_d).list, i = 0; c != NULL; c = c->next, i++) {}

	w->vscroll.cap = (w->widget[SNGRFS_FILE_LIST].bottom - w->widget[SNGRFS_FILE_LIST].top) / 14 + 1;
	SetVScrollCount(w, i);
	w->SetWidgetDisabledState(SNGRFS_APPLY_CHANGES, !WP(w, newgrf_d).editable);
}


/** Callback function for the newgrf 'apply changes' confirmation window
 * @param w Window which is calling this callback
 * @param confirmed boolean value, true when yes was clicked, false otherwise
 */
static void NewGRFConfirmationCallback(Window *w, bool confirmed)
{
	if (confirmed) {
		newgrf_d *nd = &WP(w, newgrf_d);
		GRFConfig *c;
		int i = 0;

		CopyGRFConfigList(nd->orig_list, *nd->list, false);
		ReloadNewGRFData();

		/* Show new, updated list */
		for (c = *nd->list; c != NULL && c != nd->sel; c = c->next, i++) {}
		CopyGRFConfigList(nd->list, *nd->orig_list, false);
		for (c = *nd->list; c != NULL && i > 0; c = c->next, i--) {}
		nd->sel = c;

		SetWindowDirty(w);
	}
}


static void NewGRFWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			const GRFConfig *c;
			int i, y;

			SetupNewGRFState(w);

			DrawWindowWidgets(w);

			/* Draw NewGRF list */
			y = w->widget[SNGRFS_FILE_LIST].top;
			for (c = *WP(w, newgrf_d).list, i = 0; c != NULL; c = c->next, i++) {
				if (i >= w->vscroll.pos && i < w->vscroll.pos + w->vscroll.cap) {
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
					DoDrawStringTruncated(text, txtoffset, y + 3, WP(w, newgrf_d).sel == c ? TC_WHITE : TC_BLACK, w->width - txtoffset - 10);
					y += 14;
				}
			}

			if (WP(w, newgrf_d).sel != NULL) {
				/* Draw NewGRF file info */
				const Widget *wi = &w->widget[SNGRFS_NEWGRF_INFO];
				ShowNewGRFInfo(WP(w, newgrf_d).sel, wi->left + 2, wi->top + 2, wi->right - wi->left - 2, wi->bottom, WP(w, newgrf_d).show_params);
			}

			break;
		}

		case WE_INVALIDATE_DATA:
			SetupNewGRFWindow(w);
			break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case SNGRFS_ADD: { // Add GRF
					GRFConfig **list = WP(w, newgrf_d).list;
					Window *w;

					DeleteWindowByClass(WC_SAVELOAD);
					w = AllocateWindowDesc(&_newgrf_add_dlg_desc);
					w->resize.step_height = 10;

					WP(w, newgrf_add_d).list = list;
					break;
				}

				case SNGRFS_REMOVE: { // Remove GRF
					GRFConfig **pc, *c, *newsel;

					/* Choose the next GRF file to be the selected file */
					newsel = WP(w, newgrf_d).sel->next;

					for (pc = WP(w, newgrf_d).list; (c = *pc) != NULL; pc = &c->next) {
						/* If the new selection is empty (i.e. we're deleting the last item
						 * in the list, pick the file just before the selected file */
						if (newsel == NULL && c->next == WP(w, newgrf_d).sel) newsel = c;

						if (c == WP(w, newgrf_d).sel) {
							*pc = c->next;
							free(c);
							break;
						}
					}

					WP(w, newgrf_d).sel = newsel;
					SetupNewGRFWindow(w);
					SetWindowDirty(w);
					break;
				}

				case SNGRFS_MOVE_UP: { // Move GRF up
					GRFConfig **pc, *c;
					if (WP(w, newgrf_d).sel == NULL) break;

					for (pc = WP(w, newgrf_d).list; (c = *pc) != NULL; pc = &c->next) {
						if (c->next == WP(w, newgrf_d).sel) {
							c->next = WP(w, newgrf_d).sel->next;
							WP(w, newgrf_d).sel->next = c;
							*pc = WP(w, newgrf_d).sel;
							break;
						}
					}
					SetWindowDirty(w);
					break;
				}

				case SNGRFS_MOVE_DOWN: { // Move GRF down
					GRFConfig **pc, *c;
					if (WP(w, newgrf_d).sel == NULL) break;

					for (pc = WP(w, newgrf_d).list; (c = *pc) != NULL; pc = &c->next) {
						if (c == WP(w, newgrf_d).sel) {
							*pc = c->next;
							c->next = c->next->next;
							(*pc)->next = c;
							break;
						}
					}
					SetWindowDirty(w);
					break;
				}

				case SNGRFS_FILE_LIST: { // Select a GRF
					GRFConfig *c;
					uint i = (e->we.click.pt.y - w->widget[SNGRFS_FILE_LIST].top) / 14 + w->vscroll.pos;

					for (c = *WP(w, newgrf_d).list; c != NULL && i > 0; c = c->next, i--) {}
					WP(w, newgrf_d).sel = c;

					SetWindowDirty(w);
					break;
				}

				case SNGRFS_APPLY_CHANGES: // Apply changes made to GRF list
					if (WP(w, newgrf_d).execute) {
						ShowQuery(
							STR_POPUP_CAUTION_CAPTION,
							STR_NEWGRF_CONFIRMATION_TEXT,
							w,
							NewGRFConfirmationCallback
						);
					} else {
						CopyGRFConfigList(WP(w, newgrf_d).orig_list, *WP(w, newgrf_d).list, true);
						ResetGRFConfig(false);
						ReloadNewGRFData();
					}
					break;

				case SNGRFS_SET_PARAMETERS: { // Edit parameters
					char buff[512];
					if (WP(w, newgrf_d).sel == NULL) break;

					GRFBuildParamList(buff, WP(w, newgrf_d).sel, lastof(buff));
					ShowQueryString(BindCString(buff), STR_NEWGRF_PARAMETER_QUERY, 63, 250, w, CS_ALPHANUMERAL);
					break;
				}
			}
			break;

		case WE_ON_EDIT_TEXT:
			if (e->we.edittext.str != NULL) {
				/* Parse our new "int list" */
				GRFConfig *c = WP(w, newgrf_d).sel;
				c->num_params = parse_intlist(e->we.edittext.str, (int*)c->param, lengthof(c->param));

				/* parse_intlist returns -1 on error */
				if (c->num_params == (byte)-1) c->num_params = 0;
			}
			SetWindowDirty(w);
			break;

		case WE_DESTROY:
			if (!WP(w, newgrf_d).execute) {
				CopyGRFConfigList(WP(w, newgrf_d).orig_list, *WP(w, newgrf_d).list, true);
				ResetGRFConfig(false);
				ReloadNewGRFData();
			}
			/* Remove the temporary copy of grf-list used in window */
			ClearGRFConfigList(WP(w, newgrf_d).list);
			break;

		case WE_RESIZE:
			if (e->we.sizing.diff.x != 0) {
				ResizeButtons(w, SNGRFS_ADD, SNGRFS_MOVE_DOWN);
				ResizeButtons(w, SNGRFS_SET_PARAMETERS, SNGRFS_APPLY_CHANGES);
			}
			w->vscroll.cap += e->we.sizing.diff.y / 14;
			w->widget[SNGRFS_FILE_LIST].data = (w->vscroll.cap << 8) + 1;
			SetupNewGRFWindow(w);
			break;
	}
}

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
	NewGRFWndProc,
};


/** Setup the NewGRF gui
 * @param editable allow the user to make changes to the grfconfig in the window
 * @param show_params show information about what parameters are set for the grf files
 * @param exec_changes if changes are made to the list (editable is true), apply these
 *        changes immediately or only update the list
 * @param config pointer to a linked-list of grfconfig's that will be shown */
void ShowNewGRFSettings(bool editable, bool show_params, bool exec_changes, GRFConfig **config)
{
	static GRFConfig *local = NULL;
	Window *w;

	DeleteWindowByClass(WC_GAME_OPTIONS);
	w = AllocateWindowDesc(&_newgrf_desc);
	if (w == NULL) return;

	w->resize.step_height = 14;
	CopyGRFConfigList(&local, *config, false);

	/* Clear selections */
	WP(w, newgrf_d).sel         = NULL;
	WP(w, newgrf_d).list        = &local;
	WP(w, newgrf_d).orig_list   = config;
	WP(w, newgrf_d).editable    = editable;
	WP(w, newgrf_d).execute     = exec_changes;
	WP(w, newgrf_d).show_params = show_params;

	SetupNewGRFWindow(w);
}
