/* $Id$ */

#include "stdafx.h"
#include <stdarg.h>
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "map.h"
#include "player.h"
#include "window.h"
#include "gfx.h"
#include "viewport.h"
#include "console.h"
#include "variables.h"
#include "table/sprites.h"
#include "genworld.h"

// delta between mouse cursor and upper left corner of dragged window
static Point _drag_delta;

static Window _windows[25];
Window *_z_windows[lengthof(_windows)];
Window **_last_z_window; ///< always points to the next free space in the z-array

void CDECL SetWindowWidgetsDisabledState(Window *w, bool disab_stat, int widgets, ...)
{
	va_list wdg_list;

	va_start(wdg_list, widgets);

	while (widgets != WIDGET_LIST_END) {
		SetWindowWidgetDisabledState(w, widgets, disab_stat);
		widgets = va_arg(wdg_list, int);
	}

	va_end(wdg_list);
}

void CDECL SetWindowWidgetsHiddenState(Window *w, bool hidden_stat, int widgets, ...)
{
	va_list wdg_list;

	va_start(wdg_list, widgets);

	while (widgets != WIDGET_LIST_END) {
		SetWindowWidgetHiddenState(w, widgets, hidden_stat);
		widgets = va_arg(wdg_list, int);
	}

	va_end(wdg_list);
}

void CDECL SetWindowWidgetsLoweredState(Window *w, bool lowered_stat, int widgets, ...)
{
	va_list wdg_list;

	va_start(wdg_list, widgets);

	while (widgets != WIDGET_LIST_END) {
		SetWindowWidgetLoweredState(w, widgets, lowered_stat);
		widgets = va_arg(wdg_list, int);
	}

	va_end(wdg_list);
}

void RaiseWindowButtons(Window *w)
{
	uint i;

	for (i = 0; i < w->widget_count; i++) {
		if (IsWindowWidgetLowered(w, i)) {
			RaiseWindowWidget(w, i);
			InvalidateWidget(w, i);
		}
	}
}

void HandleButtonClick(Window *w, byte widget)
{
	LowerWindowWidget(w, widget);
	w->flags4 |= 5 << WF_TIMEOUT_SHL;
	InvalidateWidget(w, widget);
}


static void StartWindowDrag(Window *w);
static void StartWindowSizing(Window *w);

static void DispatchLeftClickEvent(Window *w, int x, int y)
{
	WindowEvent e;
	const Widget *wi;

	e.we.click.pt.x = x;
	e.we.click.pt.y = y;
	e.event = WE_CLICK;

	if (w->desc_flags & WDF_DEF_WIDGET) {
		e.we.click.widget = GetWidgetFromPos(w, x, y);
		if (e.we.click.widget < 0) return; /* exit if clicked outside of widgets */

		/* don't allow any interaction if the button has been disabled */
		if (IsWindowWidgetDisabled(w, e.we.click.widget)) return;

		wi = &w->widget[e.we.click.widget];

		if (wi->type & WWB_MASK) {
			/* special widget handling for buttons*/
			switch (wi->type) {
				case WWT_PANEL   | WWB_PUSHBUTTON: /* WWT_PUSHBTN */
				case WWT_IMGBTN  | WWB_PUSHBUTTON: /* WWT_PUSHIMGBTN */
				case WWT_TEXTBTN | WWB_PUSHBUTTON: /* WWT_PUSHTXTBTN */
					HandleButtonClick(w, e.we.click.widget);
					break;
			}
		} else if (wi->type == WWT_SCROLLBAR || wi->type == WWT_SCROLL2BAR || wi->type == WWT_HSCROLLBAR) {
			ScrollbarClickHandler(w, wi, e.we.click.pt.x, e.we.click.pt.y);
		}

		if (w->desc_flags & WDF_STD_BTN) {
			if (e.we.click.widget == 0) { /* 'X' */
				DeleteWindow(w);
				return;
			}

			if (e.we.click.widget == 1) { /* 'Title bar' */
				StartWindowDrag(w);
				return;
			}
		}

		if (w->desc_flags & WDF_RESIZABLE && wi->type == WWT_RESIZEBOX) {
			StartWindowSizing(w);
			InvalidateWidget(w, e.we.click.widget);
			return;
		}

		if (w->desc_flags & WDF_STICKY_BUTTON && wi->type == WWT_STICKYBOX) {
			w->flags4 ^= WF_STICKY;
			InvalidateWidget(w, e.we.click.widget);
			return;
		}
	}

	w->wndproc(w, &e);
}

static void DispatchRightClickEvent(Window *w, int x, int y)
{
	WindowEvent e;

	/* default tooltips handler? */
	if (w->desc_flags & WDF_STD_TOOLTIPS) {
		e.we.click.widget = GetWidgetFromPos(w, x, y);
		if (e.we.click.widget < 0)
			return; /* exit if clicked outside of widgets */

		if (w->widget[e.we.click.widget].tooltips != 0) {
			GuiShowTooltips(w->widget[e.we.click.widget].tooltips);
			return;
		}
	}

	e.event = WE_RCLICK;
	e.we.click.pt.x = x;
	e.we.click.pt.y = y;
	w->wndproc(w, &e);
}

/** Dispatch the mousewheel-action to the window which will scroll any
 * compatible scrollbars if the mouse is pointed over the bar or its contents
 * @param *w Window
 * @param widget the widget where the scrollwheel was used
 * @param wheel scroll up or down
 */
static void DispatchMouseWheelEvent(Window *w, int widget, int wheel)
{
	const Widget *wi1, *wi2;
	Scrollbar *sb;

	if (widget < 0) return;

	wi1 = &w->widget[widget];
	wi2 = &w->widget[widget + 1];

	/* The listbox can only scroll if scrolling was done on the scrollbar itself,
	 * or on the listbox (and the next item is (must be) the scrollbar)
	 * XXX - should be rewritten as a widget-dependent scroller but that's
	 * not happening until someone rewrites the whole widget-code */
	if ((sb = &w->vscroll,  wi1->type == WWT_SCROLLBAR)  || (sb = &w->vscroll2, wi1->type == WWT_SCROLL2BAR)  ||
			(sb = &w->vscroll2, wi2->type == WWT_SCROLL2BAR) || (sb = &w->vscroll, wi2->type == WWT_SCROLLBAR) ) {

		if (sb->count > sb->cap) {
			int pos = clamp(sb->pos + wheel, 0, sb->count - sb->cap);
			if (pos != sb->pos) {
				sb->pos = pos;
				SetWindowDirty(w);
			}
		}
	}
}

static void DrawOverlappedWindow(Window* const *wz, int left, int top, int right, int bottom);

void DrawOverlappedWindowForAll(int left, int top, int right, int bottom)
{
	Window* const *wz;
	DrawPixelInfo bk;
	_cur_dpi = &bk;

	FOR_ALL_WINDOWS(wz) {
		const Window *w = *wz;
		if (right > w->left &&
				bottom > w->top &&
				left < w->left + w->width &&
				top < w->top + w->height) {
			DrawOverlappedWindow(wz, left, top, right, bottom);
		}
	}
}

static void DrawOverlappedWindow(Window* const *wz, int left, int top, int right, int bottom)
{
	Window* const *vz = wz;
	int x;

	while (++vz != _last_z_window) {
		const Window *v = *vz;

		if (right > v->left &&
				bottom > v->top &&
				left < v->left + v->width &&
				top < v->top + v->height) {
			if (left < (x=v->left)) {
				DrawOverlappedWindow(wz, left, top, x, bottom);
				DrawOverlappedWindow(wz, x, top, right, bottom);
				return;
			}

			if (right > (x=v->left + v->width)) {
				DrawOverlappedWindow(wz, left, top, x, bottom);
				DrawOverlappedWindow(wz, x, top, right, bottom);
				return;
			}

			if (top < (x=v->top)) {
				DrawOverlappedWindow(wz, left, top, right, x);
				DrawOverlappedWindow(wz, left, x, right, bottom);
				return;
			}

			if (bottom > (x=v->top + v->height)) {
				DrawOverlappedWindow(wz, left, top, right, x);
				DrawOverlappedWindow(wz, left, x, right, bottom);
				return;
			}

			return;
		}
	}

	{
		DrawPixelInfo *dp = _cur_dpi;
		dp->width = right - left;
		dp->height = bottom - top;
		dp->left = left - (*wz)->left;
		dp->top = top - (*wz)->top;
		dp->pitch = _screen.pitch;
		dp->dst_ptr = _screen.dst_ptr + top * _screen.pitch + left;
		dp->zoom = 0;
		CallWindowEventNP(*wz, WE_PAINT);
	}
}

void CallWindowEventNP(Window *w, int event)
{
	WindowEvent e;

	e.event = event;
	w->wndproc(w, &e);
}

void SetWindowDirty(const Window *w)
{
	if (w == NULL) return;
	SetDirtyBlocks(w->left, w->top, w->left + w->width, w->top + w->height);
}

/** Find the z-value of a window. A window must already be open
 * or the behaviour is undefined but function should never fail */
Window **FindWindowZPosition(const Window *w)
{
	Window **wz;

	for (wz = _z_windows;; wz++) {
		assert(wz < _last_z_window);
		if (*wz == w) return wz;
	}
}

void DeleteWindow(Window *w)
{
	Window **wz;
	if (w == NULL) return;

	if (_thd.place_mode != VHM_NONE &&
			_thd.window_class == w->window_class &&
			_thd.window_number == w->window_number) {
		ResetObjectToPlace();
	}

	CallWindowEventNP(w, WE_DESTROY);
	if (w->viewport != NULL) DeleteWindowViewport(w);

	SetWindowDirty(w);
	free(w->widget);
	w->widget = NULL;
	w->widget_count = 0;

	/* Find the window in the z-array, and effectively remove it
	 * by moving all windows after it one to the left */
	wz = FindWindowZPosition(w);
	memmove(wz, wz + 1, (byte*)_last_z_window - (byte*)wz);
	_last_z_window--;
}

Window *FindWindowById(WindowClass cls, WindowNumber number)
{
	Window* const *wz;

	FOR_ALL_WINDOWS(wz) {
		Window *w = *wz;
		if (w->window_class == cls && w->window_number == number) return w;
	}

	return NULL;
}

void DeleteWindowById(WindowClass cls, WindowNumber number)
{
	DeleteWindow(FindWindowById(cls, number));
}

void DeleteWindowByClass(WindowClass cls)
{
	Window* const *wz;

restart_search:
	/* When we find the window to delete, we need to restart the search
	 * as deleting this window could cascade in deleting (many) others
	 * anywhere in the z-array */
	FOR_ALL_WINDOWS(wz) {
		Window *w = *wz;
		if (w->window_class == cls) {
			DeleteWindow(w);
			goto restart_search;
		}
	}
}

/** Delete all windows of a player. We identify windows of a player
 * by looking at the caption colour. If it is equal to the player ID
 * then we say the window belongs to the player and should be deleted
 * @param id PlayerID player identifier */
void DeletePlayerWindows(PlayerID id)
{
	Window* const *wz;

restart_search:
	/* When we find the window to delete, we need to restart the search
	 * as deleting this window could cascade in deleting (many) others
	 * anywhere in the z-array */
	FOR_ALL_WINDOWS(wz) {
		Window *w = *wz;
		if (w->caption_color == id) {
			DeleteWindow(w);
			goto restart_search;
		}
	}

	/* Also delete the player specific windows, that don't have a player-colour */
	DeleteWindowById(WC_BUY_COMPANY, id);
}

/** Change the owner of all the windows one player can take over from another
 * player in the case of a company merger. Do not change ownership of windows
 * that need to be deleted once takeover is complete
 * @param old_player PlayerID of original owner of the window
 * @param new_player PlayerID of the new owner of the window */
void ChangeWindowOwner(PlayerID old_player, PlayerID new_player)
{
	Window* const *wz;

	FOR_ALL_WINDOWS(wz) {
		Window *w = *wz;

		if (w->caption_color != old_player)      continue;
		if (w->window_class == WC_PLAYER_COLOR)  continue;
		if (w->window_class == WC_FINANCES)      continue;
		if (w->window_class == WC_STATION_LIST)  continue;
		if (w->window_class == WC_TRAINS_LIST)   continue;
		if (w->window_class == WC_ROADVEH_LIST)  continue;
		if (w->window_class == WC_SHIPS_LIST)    continue;
		if (w->window_class == WC_AIRCRAFT_LIST) continue;
		if (w->window_class == WC_BUY_COMPANY)   continue;
		if (w->window_class == WC_COMPANY)       continue;

		w->caption_color = new_player;
	}
}

static void BringWindowToFront(const Window *w);

/** Find a window and make it the top-window on the screen. The window
 * gets a white border for a brief period of time to visualize its
 * "activation"
 * @return a pointer to the window thus activated */
Window *BringWindowToFrontById(WindowClass cls, WindowNumber number)
{
	Window *w = FindWindowById(cls, number);

	if (w != NULL) {
		w->flags4 |= WF_WHITE_BORDER_MASK;
		BringWindowToFront(w);
		SetWindowDirty(w);
	}

	return w;
}

static inline bool IsVitalWindow(const Window *w)
{
	WindowClass wc = w->window_class;
	return (wc == WC_MAIN_TOOLBAR || wc == WC_STATUS_BAR || wc == WC_NEWS_WINDOW || wc == WC_SEND_NETWORK_MSG);
}

/** On clicking on a window, make it the frontmost window of all. However
 * there are certain windows that always need to be on-top; these include
 * - Toolbar, Statusbar (always on)
 * - New window, Chatbar (only if open)
 * The window is marked dirty for a repaint if the window is actually moved
 * @param w window that is put into the foreground
 * @return pointer to the window, the same as the input pointer
 */
static void BringWindowToFront(const Window *w)
{
	Window *tempz;
	Window **wz = FindWindowZPosition(w);
	Window **vz = _last_z_window;

	/* Bring the window just below the vital windows */
	do {
		if (--vz < _z_windows) return;
	} while (IsVitalWindow(*vz));

	if (wz == vz) return; // window is already in the right position
	assert(wz < vz);

	tempz = *wz;
	memmove(wz, wz + 1, (byte*)vz - (byte*)wz);
	*vz = tempz;

	SetWindowDirty(w);
}

/** We have run out of windows, so find a suitable candidate for replacement.
 * Keep all important windows intact. These are
 * - Main window (gamefield), Toolbar, Statusbar (always on)
 * - News window, Chatbar (when on)
 * - Any sticked windows since we wanted to keep these
 * @return w pointer to the window that is going to be deleted
 */
static Window *FindDeletableWindow(void)
{
	Window* const *wz;

	FOR_ALL_WINDOWS(wz) {
		Window *w = *wz;
		if (w->window_class != WC_MAIN_WINDOW && !IsVitalWindow(w) && !(w->flags4 & WF_STICKY)) {
			return w;
		}
	}
	return NULL;
}

/** A window must be freed, and all are marked as important windows. Ease the
 * restriction a bit by allowing to delete sticky windows. Keep important/vital
 * windows intact (Main window, Toolbar, Statusbar, News Window, Chatbar)
 * Start finding an appropiate candidate from the lowest z-values (bottom)
 * @see FindDeletableWindow()
 * @return w Pointer to the window that is being deleted
 */
static Window *ForceFindDeletableWindow(void)
{
	Window* const *wz;

	for (wz = _z_windows;; wz++) {
		Window *w = *wz;
		assert(wz < _last_z_window);
		if (w->window_class != WC_MAIN_WINDOW && !IsVitalWindow(w)) return w;
	}
}

bool IsWindowOfPrototype(const Window *w, const Widget *widget)
{
	return (w->original_widget == widget);
}

/* Copies 'widget' to 'w->widget' to allow for resizable windows */
void AssignWidgetToWindow(Window *w, const Widget *widget)
{
	w->original_widget = widget;

	if (widget != NULL) {
		uint index = 1;
		const Widget *wi;

		for (wi = widget; wi->type != WWT_LAST; wi++) index++;

		w->widget = realloc(w->widget, sizeof(*w->widget) * index);
		memcpy(w->widget, widget, sizeof(*w->widget) * index);
		w->widget_count = index - 1;
	} else {
		w->widget = NULL;
		w->widget_count = 0;
	}
}

static Window *FindFreeWindow(void)
{
	Window *w;

	for (w = _windows; w < endof(_windows); w++) {
		Window* const *wz;
		bool window_in_use = false;

		FOR_ALL_WINDOWS(wz) {
			if (*wz == w) {
				window_in_use = true;
				break;
			}
		}

		if (!window_in_use) return w;
	}

	assert(_last_z_window == endof(_z_windows));
	return NULL;
}

/* Open a new window.
 * This function is called from AllocateWindow() or AllocateWindowDesc()
 * See descriptions for those functions for usage
 * See AllocateWindow() for description of arguments.
 * Only addition here is window_number, which is the window_number being assigned to the new window
 */
static Window *LocalAllocateWindow(
							int x, int y, int width, int height,
							WindowProc *proc, WindowClass cls, const Widget *widget, int window_number)
{
	Window *w = FindFreeWindow();

	/* We have run out of windows, close one and use that as the place for our new one */
	if (w == NULL) {
		w = FindDeletableWindow();
		if (w == NULL) w = ForceFindDeletableWindow();
		DeleteWindow(w);
	}

	// Set up window properties
	memset(w, 0, sizeof(*w));
	w->window_class = cls;
	w->flags4 = WF_WHITE_BORDER_MASK; // just opened windows have a white border
	w->caption_color = 0xFF;
	w->left = x;
	w->top = y;
	w->width = width;
	w->height = height;
	w->wndproc = proc;
	AssignWidgetToWindow(w, widget);
	w->resize.width = width;
	w->resize.height = height;
	w->resize.step_width = 1;
	w->resize.step_height = 1;
	w->window_number = window_number;

	{
		Window **wz = _last_z_window;

		/* Hacky way of specifying always-on-top windows. These windows are
		 * always above other windows because they are moved below them.
		 * status-bar is above news-window because it has been created earlier.
		 * Also, as the chat-window is excluded from this, it will always be
		 * the last window, thus always on top.
		 * XXX - Yes, ugly, probably needs something like w->always_on_top flag
		 * to implement correctly, but even then you need some kind of distinction
		 * between on-top of chat/news and status windows, because these conflict */
		if (wz != _z_windows && w->window_class != WC_SEND_NETWORK_MSG && w->window_class != WC_HIGHSCORE && w->window_class != WC_ENDSCREEN) {
			if (FindWindowById(WC_MAIN_TOOLBAR, 0)     != NULL) wz--;
			if (FindWindowById(WC_STATUS_BAR, 0)       != NULL) wz--;
			if (FindWindowById(WC_NEWS_WINDOW, 0)      != NULL) wz--;
			if (FindWindowById(WC_SEND_NETWORK_MSG, 0) != NULL) wz--;

			assert(wz >= _z_windows);
			if (wz != _last_z_window) memmove(wz + 1, wz, (byte*)_last_z_window - (byte*)wz);
		}

		*wz = w;
		_last_z_window++;
	}

	SetWindowDirty(w);
	CallWindowEventNP(w, WE_CREATE);

	return w;
}

/**
 * Open a new window. If there is no space for a new window, close an open
 * window. Try to avoid stickied windows, but if there is no else, close one of
 * those as well. Then make sure all created windows are below some always-on-top
 * ones. Finally set all variables and call the WE_CREATE event
 * @param x offset in pixels from the left of the screen
 * @param y offset in pixels from the top of the screen
 * @param width width in pixels of the window
 * @param height height in pixels of the window
 * @param *proc @see WindowProc function to call when any messages/updates happen to the window
 * @param cls @see WindowClass class of the window, used for identification and grouping
 * @param *widget @see Widget pointer to the window layout and various elements
 * @return @see Window pointer of the newly created window
 */
Window *AllocateWindow(
							int x, int y, int width, int height,
							WindowProc *proc, WindowClass cls, const Widget *widget)
{
	return LocalAllocateWindow(x, y, width, height, proc, cls, widget, 0);
}

typedef struct SizeRect {
	int left,top,width,height;
} SizeRect;


static SizeRect _awap_r;

static bool IsGoodAutoPlace1(int left, int top)
{
	int right,bottom;
	Window* const *wz;

	_awap_r.left= left;
	_awap_r.top = top;
	right = _awap_r.width + left;
	bottom = _awap_r.height + top;

	if (left < 0 || top < 22 || right > _screen.width || bottom > _screen.height)
		return false;

	// Make sure it is not obscured by any window.
	FOR_ALL_WINDOWS(wz) {
		const Window *w = *wz;
		if (w->window_class == WC_MAIN_WINDOW) continue;

		if (right > w->left &&
				w->left + w->width > left &&
				bottom > w->top &&
				w->top + w->height > top) {
			return false;
		}
	}

	return true;
}

static bool IsGoodAutoPlace2(int left, int top)
{
	int width,height;
	Window* const *wz;

	_awap_r.left= left;
	_awap_r.top = top;
	width = _awap_r.width;
	height = _awap_r.height;

	if (left < -(width>>2) || left > _screen.width - (width>>1)) return false;
	if (top < 22 || top > _screen.height - (height>>2)) return false;

	// Make sure it is not obscured by any window.
	FOR_ALL_WINDOWS(wz) {
		const Window *w = *wz;
		if (w->window_class == WC_MAIN_WINDOW) continue;

		if (left + width > w->left &&
				w->left + w->width > left &&
				top + height > w->top &&
				w->top + w->height > top) {
			return false;
		}
	}

	return true;
}

static Point GetAutoPlacePosition(int width, int height)
{
	Window* const *wz;
	Point pt;

	_awap_r.width = width;
	_awap_r.height = height;

	if (IsGoodAutoPlace1(0, 24)) goto ok_pos;

	FOR_ALL_WINDOWS(wz) {
		const Window *w = *wz;
		if (w->window_class == WC_MAIN_WINDOW) continue;

		if (IsGoodAutoPlace1(w->left+w->width+2,w->top)) goto ok_pos;
		if (IsGoodAutoPlace1(w->left-   width-2,w->top)) goto ok_pos;
		if (IsGoodAutoPlace1(w->left,w->top+w->height+2)) goto ok_pos;
		if (IsGoodAutoPlace1(w->left,w->top-   height-2)) goto ok_pos;
		if (IsGoodAutoPlace1(w->left+w->width+2,w->top+w->height-height)) goto ok_pos;
		if (IsGoodAutoPlace1(w->left-   width-2,w->top+w->height-height)) goto ok_pos;
		if (IsGoodAutoPlace1(w->left+w->width-width,w->top+w->height+2)) goto ok_pos;
		if (IsGoodAutoPlace1(w->left+w->width-width,w->top-   height-2)) goto ok_pos;
	}

	FOR_ALL_WINDOWS(wz) {
		const Window *w = *wz;
		if (w->window_class == WC_MAIN_WINDOW) continue;

		if (IsGoodAutoPlace2(w->left+w->width+2,w->top)) goto ok_pos;
		if (IsGoodAutoPlace2(w->left-   width-2,w->top)) goto ok_pos;
		if (IsGoodAutoPlace2(w->left,w->top+w->height+2)) goto ok_pos;
		if (IsGoodAutoPlace2(w->left,w->top-   height-2)) goto ok_pos;
	}

	{
		int left=0,top=24;

restart:;
		FOR_ALL_WINDOWS(wz) {
			const Window *w = *wz;

			if (w->left == left && w->top == top) {
				left += 5;
				top += 5;
				goto restart;
			}
		}

		pt.x = left;
		pt.y = top;
		return pt;
	}

ok_pos:;
	pt.x = _awap_r.left;
	pt.y = _awap_r.top;
	return pt;
}

static Window *LocalAllocateWindowDesc(const WindowDesc *desc, int window_number)
{
	Point pt;
	Window *w;

	/* By default position a child window at an offset of 10/10 of its parent.
	 * However if it falls too extremely outside window positions, reposition
	 * it to an automatic place */
	if (desc->parent_cls != 0 /* WC_MAIN_WINDOW */ &&
			(w = FindWindowById(desc->parent_cls, window_number)) != NULL &&
			w->left < _screen.width - 20 && w->left > -60 && w->top < _screen.height - 20) {

		pt.x = w->left + 10;
		if (pt.x > _screen.width + 10 - desc->width) {
			pt.x = (_screen.width + 10 - desc->width) - 20;
		}
		pt.y = w->top + 10;
	} else {
		switch (desc->left) {
			case WDP_ALIGN_TBR: { /* Align the right side with the top toolbar */
				w = FindWindowById(WC_MAIN_TOOLBAR, 0);
				pt.x = (w->left + w->width) - desc->width;
			}	break;
			case WDP_ALIGN_TBL: /* Align the left side with the top toolbar */
				pt.x = FindWindowById(WC_MAIN_TOOLBAR, 0)->left;
				break;
			case WDP_AUTO: /* Find a good automatic position for the window */
				pt = GetAutoPlacePosition(desc->width, desc->height);
				goto allocate_window;
			case WDP_CENTER: /* Centre the window horizontally */
				pt.x = (_screen.width - desc->width) / 2;
				break;
			default:
				pt.x = desc->left;
				if (pt.x < 0) pt.x += _screen.width; // negative is from right of the screen
		}

		switch (desc->top) {
			case WDP_CENTER: /* Centre the window vertically */
				pt.y = (_screen.height - desc->height) / 2;
				break;
			/* WDP_AUTO sets the position at once and is controlled by desc->left.
			 * Both left and top must be set to WDP_AUTO */
			case WDP_AUTO:
				NOT_REACHED();
				assert(desc->left == WDP_AUTO && desc->top != WDP_AUTO);
				/* fallthrough */
			default:
				pt.y = desc->top;
				if (pt.y < 0) pt.y += _screen.height; // negative is from bottom of the screen
				break;
		}
	}

allocate_window:
	w = LocalAllocateWindow(pt.x, pt.y, desc->width, desc->height, desc->proc, desc->cls, desc->widgets, window_number);
	w->desc_flags = desc->flags;
	return w;
}

/**
 * Open a new window.
 * @param *desc The pointer to the WindowDesc to be created
 * @return @see Window pointer of the newly created window
 */
Window *AllocateWindowDesc(const WindowDesc *desc)
{
	return LocalAllocateWindowDesc(desc, 0);
}

/**
 * Open a new window.
 * @param *desc The pointer to the WindowDesc to be created
 * @param window_number the window number of the new window
 * @return @see Window pointer of the newly created window
 */
Window *AllocateWindowDescFront(const WindowDesc *desc, int window_number)
{
	Window *w;

	if (BringWindowToFrontById(desc->cls, window_number)) return NULL;
	w = LocalAllocateWindowDesc(desc, window_number);
	return w;
}

/** Do a search for a window at specific coordinates. For this we start
 * at the topmost window, obviously and work our way down to the bottom
 * @return a pointer to the found window if any, NULL otherwise */
Window *FindWindowFromPt(int x, int y)
{
	Window* const *wz;

	for (wz = _last_z_window; wz != _z_windows;) {
		Window *w = *--wz;
		if (IS_INSIDE_1D(x, w->left, w->width) && IS_INSIDE_1D(y, w->top, w->height)) {
			return w;
		}
	}

	return NULL;
}

void InitWindowSystem(void)
{
	IConsoleClose();

	memset(&_windows, 0, sizeof(_windows));
	_last_z_window = _z_windows;
	InitViewports();
	_no_scroll = 0;
}

void UnInitWindowSystem(void)
{
	Window* const *wz;

restart_search:
	/* Delete all windows, reset z-array.
	 *When we find the window to delete, we need to restart the search
	 * as deleting this window could cascade in deleting (many) others
	 * anywhere in the z-array. We call DeleteWindow() so that it can properly
	 * release own alloc'd memory, which otherwise could result in memleaks */
	FOR_ALL_WINDOWS(wz) {
		DeleteWindow(*wz);
		goto restart_search;
	}

	assert(_last_z_window == _z_windows);
}

void ResetWindowSystem(void)
{
	UnInitWindowSystem();
	InitWindowSystem();
	_thd.pos.x = 0;
	_thd.pos.y = 0;
	_thd.new_pos.x = 0;
	_thd.new_pos.y = 0;
}

static void DecreaseWindowCounters(void)
{
	Window *w;
	Window* const *wz;

	for (wz = _last_z_window; wz != _z_windows;) {
		w = *--wz;
		// Unclick scrollbar buttons if they are pressed.
		if (w->flags4 & (WF_SCROLL_DOWN | WF_SCROLL_UP)) {
			w->flags4 &= ~(WF_SCROLL_DOWN | WF_SCROLL_UP);
			SetWindowDirty(w);
		}
		CallWindowEventNP(w, WE_MOUSELOOP);
	}

	for (wz = _last_z_window; wz != _z_windows;) {
		w = *--wz;

		if (w->flags4&WF_TIMEOUT_MASK && !(--w->flags4&WF_TIMEOUT_MASK)) {
			CallWindowEventNP(w, WE_TIMEOUT);
			if (w->desc_flags & WDF_UNCLICK_BUTTONS) RaiseWindowButtons(w);
		}
	}
}

Window *GetCallbackWnd(void)
{
	return FindWindowById(_thd.window_class, _thd.window_number);
}

static void HandlePlacePresize(void)
{
	Window *w;
	WindowEvent e;

	if (_special_mouse_mode != WSM_PRESIZE) return;

	w = GetCallbackWnd();
	if (w == NULL) return;

	e.we.place.pt = GetTileBelowCursor();
	if (e.we.place.pt.x == -1) {
		_thd.selend.x = -1;
		return;
	}
	e.we.place.tile = TileVirtXY(e.we.place.pt.x, e.we.place.pt.y);
	e.event = WE_PLACE_PRESIZE;
	w->wndproc(w, &e);
}

static bool HandleDragDrop(void)
{
	Window *w;
	WindowEvent e;

	if (_special_mouse_mode != WSM_DRAGDROP) return true;

	if (_left_button_down) return false;

	w = GetCallbackWnd();

	ResetObjectToPlace();

	if (w != NULL) {
		// send an event in client coordinates.
		e.event = WE_DRAGDROP;
		e.we.dragdrop.pt.x = _cursor.pos.x - w->left;
		e.we.dragdrop.pt.y = _cursor.pos.y - w->top;
		e.we.dragdrop.widget = GetWidgetFromPos(w, e.we.dragdrop.pt.x, e.we.dragdrop.pt.y);
		w->wndproc(w, &e);
	}
	return false;
}

static bool HandlePopupMenu(void)
{
	Window *w;
	WindowEvent e;

	if (!_popup_menu_active) return true;

	w = FindWindowById(WC_TOOLBAR_MENU, 0);
	if (w == NULL) {
		_popup_menu_active = false;
		return false;
	}

	if (_left_button_down) {
		e.event = WE_POPUPMENU_OVER;
		e.we.popupmenu.pt = _cursor.pos;
	} else {
		_popup_menu_active = false;
		e.event = WE_POPUPMENU_SELECT;
		e.we.popupmenu.pt = _cursor.pos;
	}

	w->wndproc(w, &e);

	return false;
}

static bool HandleMouseOver(void)
{
	Window *w;
	WindowEvent e;
	static Window *last_w = NULL;

	w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);

	// We changed window, put a MOUSEOVER event to the last window
	if (last_w != NULL && last_w != w) {
		e.event = WE_MOUSEOVER;
		e.we.mouseover.pt.x = -1;
		e.we.mouseover.pt.y = -1;
		if (last_w->wndproc) last_w->wndproc(last_w, &e);
	}
	last_w = w;

	if (w != NULL) {
		// send an event in client coordinates.
		e.event = WE_MOUSEOVER;
		e.we.mouseover.pt.x = _cursor.pos.x - w->left;
		e.we.mouseover.pt.y = _cursor.pos.y - w->top;
		if (w->widget != NULL) {
			e.we.mouseover.widget = GetWidgetFromPos(w, e.we.mouseover.pt.x, e.we.mouseover.pt.y);
		}
		w->wndproc(w, &e);
	}

	// Mouseover never stops execution
	return true;
}

/** Update all the widgets of a window based on their resize flags
 * Both the areas of the old window and the new sized window are set dirty
 * ensuring proper redrawal.
 * @param w Window to resize
 * @param x delta x-size of changed window (positive if larger, etc.(
 * @param y delta y-size of changed window */
void ResizeWindow(Window *w, int x, int y)
{
	Widget *wi;
	bool resize_height = false;
	bool resize_width = false;

	if (x == 0 && y == 0) return;

	SetWindowDirty(w);
	for (wi = w->widget; wi->type != WWT_LAST; wi++) {
		/* Isolate the resizing flags */
		byte rsizeflag = GB(wi->display_flags, 0, 4);

		if (rsizeflag == RESIZE_NONE) continue;

		/* Resize the widget based on its resize-flag */
		if (rsizeflag & RESIZE_LEFT) {
			wi->left += x;
			resize_width = true;
		}

		if (rsizeflag & RESIZE_RIGHT) {
			wi->right += x;
			resize_width = true;
		}

		if (rsizeflag & RESIZE_TOP) {
			wi->top += y;
			resize_height = true;
		}

		if (rsizeflag & RESIZE_BOTTOM) {
			wi->bottom += y;
			resize_height = true;
		}
	}

	/* We resized at least 1 widget, so let's resize the window totally */
	if (resize_width)  w->width  += x;
	if (resize_height) w->height += y;

	SetWindowDirty(w);
}

static bool _dragging_window;

static bool HandleWindowDragging(void)
{
	Window* const *wz;
	// Get out immediately if no window is being dragged at all.
	if (!_dragging_window) return true;

	// Otherwise find the window...
	FOR_ALL_WINDOWS(wz) {
		Window *w = *wz;

		if (w->flags4 & WF_DRAGGING) {
			const Widget *t = &w->widget[1]; // the title bar ... ugh
			const Window *v;
			int x;
			int y;
			int nx;
			int ny;

			// Stop the dragging if the left mouse button was released
			if (!_left_button_down) {
				w->flags4 &= ~WF_DRAGGING;
				break;
			}

			SetWindowDirty(w);

			x = _cursor.pos.x + _drag_delta.x;
			y = _cursor.pos.y + _drag_delta.y;
			nx = x;
			ny = y;

			if (_patches.window_snap_radius != 0) {
				Window* const *vz;

				int hsnap = _patches.window_snap_radius;
				int vsnap = _patches.window_snap_radius;
				int delta;

				FOR_ALL_WINDOWS(vz) {
					const Window *v = *vz;

					if (v == w) continue; // Don't snap at yourself

					if (y + w->height > v->top && y < v->top + v->height) {
						// Your left border <-> other right border
						delta = abs(v->left + v->width - x);
						if (delta <= hsnap) {
							nx = v->left + v->width;
							hsnap = delta;
						}

						// Your right border <-> other left border
						delta = abs(v->left - x - w->width);
						if (delta <= hsnap) {
							nx = v->left - w->width;
							hsnap = delta;
						}
					}

					if (w->top + w->height >= v->top && w->top <= v->top + v->height) {
						// Your left border <-> other left border
						delta = abs(v->left - x);
						if (delta <= hsnap) {
							nx = v->left;
							hsnap = delta;
						}

						// Your right border <-> other right border
						delta = abs(v->left + v->width - x - w->width);
						if (delta <= hsnap) {
							nx = v->left + v->width - w->width;
							hsnap = delta;
						}
					}

					if (x + w->width > v->left && x < v->left + v->width) {
						// Your top border <-> other bottom border
						delta = abs(v->top + v->height - y);
						if (delta <= vsnap) {
							ny = v->top + v->height;
							vsnap = delta;
						}

						// Your bottom border <-> other top border
						delta = abs(v->top - y - w->height);
						if (delta <= vsnap) {
							ny = v->top - w->height;
							vsnap = delta;
						}
					}

					if (w->left + w->width >= v->left && w->left <= v->left + v->width) {
						// Your top border <-> other top border
						delta = abs(v->top - y);
						if (delta <= vsnap) {
							ny = v->top;
							vsnap = delta;
						}

						// Your bottom border <-> other bottom border
						delta = abs(v->top + v->height - y - w->height);
						if (delta <= vsnap) {
							ny = v->top + v->height - w->height;
							vsnap = delta;
						}
					}
				}
			}

			// Make sure the window doesn't leave the screen
			// 13 is the height of the title bar
			nx = clamp(nx, 13 - t->right, _screen.width - 13 - t->left);
			ny = clamp(ny, 0, _screen.height - 13);

			// Make sure the title bar isn't hidden by behind the main tool bar
			v = FindWindowById(WC_MAIN_TOOLBAR, 0);
			if (v != NULL) {
				int v_bottom = v->top + v->height;
				int v_right = v->left + v->width;
				if (ny + t->top >= v->top && ny + t->top < v_bottom) {
					if ((v->left < 13 && nx + t->left < v->left) ||
							(v_right > _screen.width - 13 && nx + t->right > v_right)) {
						ny = v_bottom;
					} else {
						if (nx + t->left > v->left - 13 &&
								nx + t->right < v_right + 13) {
							if (w->top >= v_bottom) {
								ny = v_bottom;
							} else if (w->left < nx) {
								nx = v->left - 13 - t->left;
							} else {
								nx = v_right + 13 - t->right;
							}
						}
					}
				}
			}

			if (w->viewport != NULL) {
				w->viewport->left += nx - w->left;
				w->viewport->top  += ny - w->top;
			}
			w->left = nx;
			w->top  = ny;

			SetWindowDirty(w);
			return false;
		} else if (w->flags4 & WF_SIZING) {
			WindowEvent e;
			int x, y;

			/* Stop the sizing if the left mouse button was released */
			if (!_left_button_down) {
				w->flags4 &= ~WF_SIZING;
				SetWindowDirty(w);
				break;
			}

			x = _cursor.pos.x - _drag_delta.x;
			y = _cursor.pos.y - _drag_delta.y;

			/* X and Y has to go by step.. calculate it.
			 * The cast to int is necessary else x/y are implicitly casted to
			 * unsigned int, which won't work. */
			if (w->resize.step_width > 1) x -= x % (int)w->resize.step_width;

			if (w->resize.step_height > 1) y -= y % (int)w->resize.step_height;

			/* Check if we don't go below the minimum set size */
			if ((int)w->width + x < (int)w->resize.width)
				x = w->resize.width - w->width;
			if ((int)w->height + y < (int)w->resize.height)
				y = w->resize.height - w->height;

			/* Window already on size */
			if (x == 0 && y == 0) return false;

			/* Now find the new cursor pos.. this is NOT _cursor, because
			    we move in steps. */
			_drag_delta.x += x;
			_drag_delta.y += y;

			/* ResizeWindow sets both pre- and after-size to dirty for redrawal */
			ResizeWindow(w, x, y);

			e.event = WE_RESIZE;
			e.we.sizing.size.x = x + w->width;
			e.we.sizing.size.y = y + w->height;
			e.we.sizing.diff.x = x;
			e.we.sizing.diff.y = y;
			w->wndproc(w, &e);
			return false;
		}
	}

	_dragging_window = false;
	return false;
}

static void StartWindowDrag(Window *w)
{
	w->flags4 |= WF_DRAGGING;
	_dragging_window = true;

	_drag_delta.x = w->left - _cursor.pos.x;
	_drag_delta.y = w->top  - _cursor.pos.y;

	BringWindowToFront(w);
	DeleteWindowById(WC_DROPDOWN_MENU, 0);
}

static void StartWindowSizing(Window *w)
{
	w->flags4 |= WF_SIZING;
	_dragging_window = true;

	_drag_delta.x = _cursor.pos.x;
	_drag_delta.y = _cursor.pos.y;

	BringWindowToFront(w);
	DeleteWindowById(WC_DROPDOWN_MENU, 0);
}


static bool HandleScrollbarScrolling(void)
{
	Window* const *wz;
	int i;
	int pos;
	Scrollbar *sb;

	// Get out quickly if no item is being scrolled
	if (!_scrolling_scrollbar) return true;

	// Find the scrolling window
	FOR_ALL_WINDOWS(wz) {
		Window *w = *wz;

		if (w->flags4 & WF_SCROLL_MIDDLE) {
			// Abort if no button is clicked any more.
			if (!_left_button_down) {
				w->flags4 &= ~WF_SCROLL_MIDDLE;
				SetWindowDirty(w);
				break;
			}

			if (w->flags4 & WF_HSCROLL) {
				sb = &w->hscroll;
				i = _cursor.pos.x - _cursorpos_drag_start.x;
			} else if (w->flags4 & WF_SCROLL2){
				sb = &w->vscroll2;
				i = _cursor.pos.y - _cursorpos_drag_start.y;
			} else {
				sb = &w->vscroll;
				i = _cursor.pos.y - _cursorpos_drag_start.y;
			}

			// Find the item we want to move to and make sure it's inside bounds.
			pos = min(max(0, i + _scrollbar_start_pos) * sb->count / _scrollbar_size, max(0, sb->count - sb->cap));
			if (pos != sb->pos) {
				sb->pos = pos;
				SetWindowDirty(w);
			}
			return false;
		}
	}

	_scrolling_scrollbar = false;
	return false;
}

static bool HandleViewportScroll(void)
{
	WindowEvent e;
	Window *w;

	if (!_scrolling_viewport) return true;

	w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);

	if (!_right_button_down || w == NULL) {
		_cursor.fix_at = false;
		_scrolling_viewport = false;
		return true;
	}

	if (_patches.reverse_scroll) {
		e.we.scroll.delta.x = -_cursor.delta.x;
		e.we.scroll.delta.y = -_cursor.delta.y;
	} else {
		e.we.scroll.delta.x = _cursor.delta.x;
		e.we.scroll.delta.y = _cursor.delta.y;
	}

	/* Create a scroll-event and send it to the window */
	e.event = WE_SCROLL;
	w->wndproc(w, &e);

	_cursor.delta.x = 0;
	_cursor.delta.y = 0;
	return false;
}

static void MaybeBringWindowToFront(const Window *w)
{
	Window* const *wz;
	Window* const *uz;

	if (w->window_class == WC_MAIN_WINDOW ||
			IsVitalWindow(w) ||
			w->window_class == WC_TOOLTIPS ||
			w->window_class == WC_DROPDOWN_MENU) {
		return;
	}

	wz = FindWindowZPosition(w);
	for (uz = wz; ++uz != _last_z_window;) {
		const Window *u = *uz;

		if (u->window_class == WC_MAIN_WINDOW ||
				IsVitalWindow(u) ||
				u->window_class == WC_TOOLTIPS ||
				u->window_class == WC_DROPDOWN_MENU) {
			continue;
		}

		if (w->left + w->width <= u->left ||
				u->left + u->width <= w->left ||
				w->top  + w->height <= u->top ||
				u->top + u->height <= w->top) {
			continue;
		}

		BringWindowToFront(w);
		return;
	}
}

/** Send a message from one window to another. The receiving window is found by
 * @param w @see Window pointer pointing to the other window
 * @param msg Specifies the message to be sent
 * @param wparam Specifies additional message-specific information
 * @param lparam Specifies additional message-specific information
 */
static void SendWindowMessageW(Window *w, uint msg, uint wparam, uint lparam)
{
	WindowEvent e;

	e.event             = WE_MESSAGE;
	e.we.message.msg    = msg;
	e.we.message.wparam = wparam;
	e.we.message.lparam = lparam;

	w->wndproc(w, &e);
}

/** Send a message from one window to another. The receiving window is found by
 * @param wnd_class @see WindowClass class AND
 * @param wnd_num @see WindowNumber number, mostly 0
 * @param msg Specifies the message to be sent
 * @param wparam Specifies additional message-specific information
 * @param lparam Specifies additional message-specific information
 */
void SendWindowMessage(WindowClass wnd_class, WindowNumber wnd_num, uint msg, uint wparam, uint lparam)
{
	Window *w = FindWindowById(wnd_class, wnd_num);
	if (w != NULL) SendWindowMessageW(w, msg, wparam, lparam);
}

/** Send a message from one window to another. The message will be sent
 * to ALL windows of the windowclass specified in the first parameter
 * @param wnd_class @see WindowClass class
 * @param msg Specifies the message to be sent
 * @param wparam Specifies additional message-specific information
 * @param lparam Specifies additional message-specific information
 */
void SendWindowMessageClass(WindowClass wnd_class, uint msg, uint wparam, uint lparam)
{
	Window* const *wz;

	FOR_ALL_WINDOWS(wz) {
		if ((*wz)->window_class == wnd_class) SendWindowMessageW(*wz, msg, wparam, lparam);
	}
}

/** Handle keyboard input.
 * @param key Lower 8 bits contain the ASCII character, the higher
 * 16 bits the keycode */
void HandleKeypress(uint32 key)
{
	Window* const *wz;
	WindowEvent e;
	/* Stores if a window with a textfield for typing is open
	 * If this is the case, keypress events are only passed to windows with text fields and
	 * to thein this main toolbar. */
	bool query_open = false;

	/*
	* During the generation of the world, there might be
	* another thread that is currently building for example
	* a road. To not interfere with those tasks, we should
	* NOT change the _current_player here.
	*
	* This is not necessary either, as the only events that
	* can be handled are the 'close application' events
	*/
	if (!IsGeneratingWorld()) _current_player = _local_player;

	// Setup event
	e.event = WE_KEYPRESS;
	e.we.keypress.key     = GB(key,  0, 16);
	e.we.keypress.keycode = GB(key, 16, 16);
	e.we.keypress.cont = true;

	// check if we have a query string window open before allowing hotkeys
	if (FindWindowById(WC_QUERY_STRING,       0) != NULL ||
			FindWindowById(WC_SEND_NETWORK_MSG,   0) != NULL ||
			FindWindowById(WC_GENERATE_LANDSCAPE, 0) != NULL ||
			FindWindowById(WC_CONSOLE,            0) != NULL ||
			FindWindowById(WC_SAVELOAD,           0) != NULL) {
		query_open = true;
	}

	// Call the event, start with the uppermost window.
	for (wz = _last_z_window; wz != _z_windows;) {
		Window *w = *--wz;

		// if a query window is open, only call the event for certain window types
		if (query_open &&
				w->window_class != WC_QUERY_STRING &&
				w->window_class != WC_SEND_NETWORK_MSG &&
				w->window_class != WC_GENERATE_LANDSCAPE &&
				w->window_class != WC_CONSOLE &&
				w->window_class != WC_SAVELOAD) {
			continue;
		}
		w->wndproc(w, &e);
		if (!e.we.keypress.cont) break;
	}

	if (e.we.keypress.cont) {
		Window *w = FindWindowById(WC_MAIN_TOOLBAR, 0);
		// When there is no toolbar w is null, check for that
		if (w != NULL) w->wndproc(w, &e);
	}
}

extern void UpdateTileSelection(void);
extern bool VpHandlePlaceSizingDrag(void);

static int _input_events_this_tick = 0;

static void HandleAutoscroll(void)
{
	Window *w;
	ViewPort *vp;
	int x = _cursor.pos.x;
	int y = _cursor.pos.y;

	if (_input_events_this_tick != 0) {
		/* HandleAutoscroll is called only once per GameLoop() - so we can clear the counter here */
		_input_events_this_tick = 0;
		/* there were some inputs this tick, don't scroll ??? */
		return;
	}

	if (_patches.autoscroll && _game_mode != GM_MENU && !IsGeneratingWorld()) {
		w = FindWindowFromPt(x, y);
		if (w == NULL || w->flags4 & WF_DISABLE_VP_SCROLL) return;
		vp = IsPtInWindowViewport(w, x, y);
		if (vp != NULL) {
			x -= vp->left;
			y -= vp->top;
			//here allows scrolling in both x and y axis
#define scrollspeed 3
			if (x - 15 < 0) {
				WP(w, vp_d).scrollpos_x += (x - 15) * scrollspeed << vp->zoom;
			} else if (15 - (vp->width - x) > 0) {
				WP(w, vp_d).scrollpos_x += (15 - (vp->width - x)) * scrollspeed << vp->zoom;
			}
			if (y - 15 < 0) {
				WP(w, vp_d).scrollpos_y += (y - 15) * scrollspeed << vp->zoom;
			} else if (15 - (vp->height - y) > 0) {
				WP(w,vp_d).scrollpos_y += (15 - (vp->height - y)) * scrollspeed << vp->zoom;
			}
#undef scrollspeed
		}
	}
}

void MouseLoop(int click, int mousewheel)
{
	int x,y;
	Window *w;
	ViewPort *vp;

	DecreaseWindowCounters();
	HandlePlacePresize();
	UpdateTileSelection();
	if (!VpHandlePlaceSizingDrag())  return;
	if (!HandleDragDrop())           return;
	if (!HandlePopupMenu())          return;
	if (!HandleWindowDragging())     return;
	if (!HandleScrollbarScrolling()) return;
	if (!HandleViewportScroll())     return;
	if (!HandleMouseOver())          return;

	x = _cursor.pos.x;
	y = _cursor.pos.y;

	if (click == 0 && mousewheel == 0) return;

	w = FindWindowFromPt(x, y);
	if (w == NULL) return;
	MaybeBringWindowToFront(w);
	vp = IsPtInWindowViewport(w, x, y);

	/* Don't allow any action in a viewport if either in menu of in generating world */
	if (vp != NULL && (_game_mode == GM_MENU || IsGeneratingWorld())) return;

	if (mousewheel != 0) {
		WindowEvent e;

		/* Send WE_MOUSEWHEEL event to window */
		e.event = WE_MOUSEWHEEL;
		e.we.wheel.wheel = mousewheel;
		w->wndproc(w, &e);

		/* Dispatch a MouseWheelEvent for widgets if it is not a viewport */
		if (vp == NULL) DispatchMouseWheelEvent(w, GetWidgetFromPos(w, x - w->left, y - w->top), mousewheel);
	}

	if (vp != NULL) {
		switch (click) {
			case 1:
				DEBUG(misc, 2) ("cursor: 0x%X (%d)", _cursor.sprite, _cursor.sprite);
				if (_thd.place_mode != 0 &&
						// query button and place sign button work in pause mode
						_cursor.sprite != SPR_CURSOR_QUERY &&
						_cursor.sprite != SPR_CURSOR_SIGN &&
						_pause != 0 &&
						!_cheats.build_in_pause.value) {
					return;
				}

				if (_thd.place_mode == 0) {
					HandleViewportClicked(vp, x, y);
				} else {
					PlaceObject();
				}
				break;

			case 2:
				if (!(w->flags4 & WF_DISABLE_VP_SCROLL)) {
					_scrolling_viewport = true;
					_cursor.fix_at = true;
				}
				break;
		}
	} else {
		switch (click) {
			case 1: DispatchLeftClickEvent(w, x - w->left, y - w->top);  break;
			case 2: DispatchRightClickEvent(w, x - w->left, y - w->top); break;
		}
	}
}

void HandleMouseEvents(void)
{
	int click;
	int mousewheel;

	/*
	 * During the generation of the world, there might be
	 * another thread that is currently building for example
	 * a road. To not interfere with those tasks, we should
	 * NOT change the _current_player here.
	 *
	 * This is not necessary either, as the only events that
	 * can be handled are the 'close application' events
	 */
	if (!IsGeneratingWorld()) _current_player = _local_player;

	// Mouse event?
	click = 0;
	if (_left_button_down && !_left_button_clicked) {
		_left_button_clicked = true;
		click = 1;
		_input_events_this_tick++;
	} else if (_right_button_clicked) {
		_right_button_clicked = false;
		click = 2;
		_input_events_this_tick++;
	}

	mousewheel = 0;
	if (_cursor.wheel) {
		mousewheel = _cursor.wheel;
		_cursor.wheel = 0;
		_input_events_this_tick++;
	}

	MouseLoop(click, mousewheel);
}

void InputLoop(void)
{
	HandleMouseEvents();
	HandleAutoscroll();
}

void UpdateWindows(void)
{
	Window* const *wz;
	static int we4_timer = 0;
	int t = we4_timer + 1;

	if (t >= 100) {
		for (wz = _last_z_window; wz != _z_windows;) {
			CallWindowEventNP(*--wz, WE_4);
		}
		t = 0;
	}
	we4_timer = t;

	for (wz = _last_z_window; wz != _z_windows;) {
		Window *w = *--wz;
		if (w->flags4 & WF_WHITE_BORDER_MASK) {
			w->flags4 -= WF_WHITE_BORDER_ONE;

			if (!(w->flags4 & WF_WHITE_BORDER_MASK)) SetWindowDirty(w);
		}
	}

	DrawDirtyBlocks();

	FOR_ALL_WINDOWS(wz) {
		if ((*wz)->viewport != NULL) UpdateViewportPosition(*wz);
	}
	DrawTextMessage();
	// Redraw mouse cursor in case it was hidden
	DrawMouseCursor();
}


int GetMenuItemIndex(const Window *w, int x, int y)
{
	if ((x -= w->left) >= 0 && x < w->width && (y -= w->top + 1) >= 0) {
		y /= 10;

		if (y < WP(w, const menu_d).item_count &&
				!HASBIT(WP(w, const menu_d).disabled_items, y)) {
			return y;
		}
	}
	return -1;
}

void InvalidateWindow(WindowClass cls, WindowNumber number)
{
	Window* const *wz;

	FOR_ALL_WINDOWS(wz) {
		const Window *w = *wz;
		if (w->window_class == cls && w->window_number == number) SetWindowDirty(w);
	}
}

void InvalidateWidget(const Window *w, byte widget_index)
{
	const Widget *wi = &w->widget[widget_index];

	/* Don't redraw the window if the widget is invisible or of no-type */
	if (wi->type == WWT_EMPTY || IsWindowWidgetHidden(w, widget_index)) return;

	SetDirtyBlocks(w->left + wi->left, w->top + wi->top, w->left + wi->right + 1, w->top + wi->bottom + 1);
}

void InvalidateWindowWidget(WindowClass cls, WindowNumber number, byte widget_index)
{
	Window* const *wz;

	FOR_ALL_WINDOWS(wz) {
		const Window *w = *wz;
		if (w->window_class == cls && w->window_number == number) {
			InvalidateWidget(w, widget_index);
		}
	}
}

void InvalidateWindowClasses(WindowClass cls)
{
	Window* const *wz;

	FOR_ALL_WINDOWS(wz) {
		if ((*wz)->window_class == cls) SetWindowDirty(*wz);
	}
}

void InvalidateThisWindowData(Window *w)
{
	CallWindowEventNP(w, WE_INVALIDATE_DATA);
	SetWindowDirty(w);
}

void InvalidateWindowData(WindowClass cls, WindowNumber number)
{
	Window* const *wz;

	FOR_ALL_WINDOWS(wz) {
		Window *w = *wz;
		if (w->window_class == cls && w->window_number == number) InvalidateThisWindowData(w);
	}
}

void InvalidateWindowClassesData(WindowClass cls)
{
	Window* const *wz;

	FOR_ALL_WINDOWS(wz) {
		if ((*wz)->window_class == cls) InvalidateThisWindowData(*wz);
	}
}

void CallWindowTickEvent(void)
{
	Window* const *wz;

	for (wz = _last_z_window; wz != _z_windows;) {
		CallWindowEventNP(*--wz, WE_TICK);
	}
}

void DeleteNonVitalWindows(void)
{
	Window* const *wz;

restart_search:
	/* When we find the window to delete, we need to restart the search
	 * as deleting this window could cascade in deleting (many) others
	 * anywhere in the z-array */
	FOR_ALL_WINDOWS(wz) {
		Window *w = *wz;
		if (w->window_class != WC_MAIN_WINDOW &&
				w->window_class != WC_SELECT_GAME &&
				w->window_class != WC_MAIN_TOOLBAR &&
				w->window_class != WC_STATUS_BAR &&
				w->window_class != WC_TOOLBAR_MENU &&
				w->window_class != WC_TOOLTIPS &&
				(w->flags4 & WF_STICKY) == 0) { // do not delete windows which are 'pinned'

			DeleteWindow(w);
			goto restart_search;
		}
	}
}

/* It is possible that a stickied window gets to a position where the
 * 'close' button is outside the gaming area. You cannot close it then; except
 * with this function. It closes all windows calling the standard function,
 * then, does a little hacked loop of closing all stickied windows. Note
 * that standard windows (status bar, etc.) are not stickied, so these aren't affected */
void DeleteAllNonVitalWindows(void)
{
	Window* const *wz;

	/* Delete every window except for stickied ones, then sticky ones as well */
	DeleteNonVitalWindows();

restart_search:
	/* When we find the window to delete, we need to restart the search
	 * as deleting this window could cascade in deleting (many) others
	 * anywhere in the z-array */
	FOR_ALL_WINDOWS(wz) {
		if ((*wz)->flags4 & WF_STICKY) {
			DeleteWindow(*wz);
			goto restart_search;
		}
	}
}

/* Delete all always on-top windows to get an empty screen */
void HideVitalWindows(void)
{
	DeleteWindowById(WC_TOOLBAR_MENU, 0);
	DeleteWindowById(WC_MAIN_TOOLBAR, 0);
	DeleteWindowById(WC_STATUS_BAR, 0);
}

int PositionMainToolbar(Window *w)
{
	DEBUG(misc, 1) ("Repositioning Main Toolbar...");

	if (w == NULL || w->window_class != WC_MAIN_TOOLBAR) {
		w = FindWindowById(WC_MAIN_TOOLBAR, 0);
	}

	switch (_patches.toolbar_pos) {
		case 1:  w->left = (_screen.width - w->width) / 2; break;
		case 2:  w->left = _screen.width - w->width; break;
		default: w->left = 0;
	}
	SetDirtyBlocks(0, 0, _screen.width, w->height); // invalidate the whole top part
	return w->left;
}

void RelocateAllWindows(int neww, int newh)
{
	Window* const *wz;

	FOR_ALL_WINDOWS(wz) {
		Window *w = *wz;
		int left, top;

		if (w->window_class == WC_MAIN_WINDOW) {
			ViewPort *vp = w->viewport;
			vp->width = w->width = neww;
			vp->height = w->height = newh;
			vp->virtual_width = neww << vp->zoom;
			vp->virtual_height = newh << vp->zoom;
			continue; // don't modify top,left
		}

		/* XXX - this probably needs something more sane. For example specying
		 * in a 'backup'-desc that the window should always be centred. */
		switch (w->window_class) {
			case WC_MAIN_TOOLBAR:
				top = w->top;
				left = PositionMainToolbar(w); // changes toolbar orientation
				break;

			case WC_SELECT_GAME:
			case WC_GAME_OPTIONS:
			case WC_NETWORK_WINDOW:
				top = (newh - w->height) >> 1;
				left = (neww - w->width) >> 1;
				break;

			case WC_NEWS_WINDOW:
				top = newh - w->height;
				left = (neww - w->width) >> 1;
				break;

			case WC_STATUS_BAR:
				top = newh - w->height;
				left = (neww - w->width) >> 1;
				break;

			case WC_SEND_NETWORK_MSG:
				top = (newh - 26); // 26 = height of status bar + height of chat bar
				left = (neww - w->width) >> 1;
				break;

			case WC_CONSOLE:
				IConsoleResize(w);
				continue;

			default:
				left = w->left;
				if (left + (w->width >> 1) >= neww) left = neww - w->width;
				top = w->top;
				if (top + (w->height >> 1) >= newh) top = newh - w->height;
				break;
		}

		if (w->viewport != NULL) {
			w->viewport->left += left - w->left;
			w->viewport->top += top - w->top;
		}

		w->left = left;
		w->top = top;
	}
}
