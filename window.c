/* $Id$ */

#include "stdafx.h"
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

// delta between mouse cursor and upper left corner of dragged window
static Point _drag_delta;

void HandleButtonClick(Window *w, byte widget)
{
	w->click_state |= (1 << widget);
	w->flags4 |= 5 << WF_TIMEOUT_SHL;
	InvalidateWidget(w, widget);
}

void DispatchLeftClickEvent(Window *w, int x, int y) {
	WindowEvent e;
	const Widget *wi;

	e.click.pt.x = x;
	e.click.pt.y = y;
	e.event = WE_CLICK;

	if (w->desc_flags & WDF_DEF_WIDGET) {
		e.click.widget = GetWidgetFromPos(w, x, y);
		if (e.click.widget < 0) return; /* exit if clicked outside of widgets */

		wi = &w->widget[e.click.widget];

		/* don't allow any interaction if the button has been disabled */
		if (HASBIT(w->disabled_state, e.click.widget))
			return;

		if (wi->type & 0xE0) {
			/* special widget handling for buttons*/
			switch(wi->type) {
			case WWT_IMGBTN  | WWB_PUSHBUTTON: /* WWT_PUSHIMGBTN */
			case WWT_TEXTBTN | WWB_PUSHBUTTON: /* WWT_PUSHTXTBTN */
				HandleButtonClick(w, e.click.widget);
				break;
			case WWT_NODISTXTBTN:
				break;
			}
		} else if (wi->type == WWT_SCROLLBAR || wi->type == WWT_SCROLL2BAR || wi->type == WWT_HSCROLLBAR) {
			ScrollbarClickHandler(w, wi, e.click.pt.x, e.click.pt.y);
		}

		if (w->desc_flags & WDF_STD_BTN) {
			if (e.click.widget == 0) { /* 'X' */
				DeleteWindow(w);
				return;
			}

			if (e.click.widget == 1) { /* 'Title bar' */
				StartWindowDrag(w); // if not return then w = StartWindowDrag(w); to get correct pointer
				return;
			}
		}

		if (w->desc_flags & WDF_RESIZABLE && wi->type == WWT_RESIZEBOX) {
			StartWindowSizing(w); // if not return then w = StartWindowSizing(w); to get correct pointer
			return;
		}

		if (w->desc_flags & WDF_STICKY_BUTTON && wi->type == WWT_STICKYBOX) {
			TOGGLEBIT(w->click_state, e.click.widget);
			w->flags4 ^= WF_STICKY;
			InvalidateWidget(w, e.click.widget);
			return;
		}
	}

	w->wndproc(w, &e);
}

void DispatchRightClickEvent(Window *w, int x, int y) {
	WindowEvent e;

	/* default tooltips handler? */
	if (w->desc_flags & WDF_STD_TOOLTIPS) {
		e.click.widget = GetWidgetFromPos(w, x, y);
		if (e.click.widget < 0)
			return; /* exit if clicked outside of widgets */

		if (w->widget[e.click.widget].tooltips != 0) {
			GuiShowTooltips(w->widget[e.click.widget].tooltips);
			return;
		}
	}

	e.event = WE_RCLICK;
	e.click.pt.x = x;
	e.click.pt.y = y;
	w->wndproc(w, &e);
}

/** Dispatch the mousewheel-action to the window which will scroll any
 * compatible scrollbars if the mouse is pointed over the bar or its contents
 * @param *w Window
 * @param widget the widget where the scrollwheel was used
 * @param wheel scroll up or down
 */
void DispatchMouseWheelEvent(Window *w, int widget, int wheel)
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


void DrawOverlappedWindowForAll(int left, int top, int right, int bottom)
{
	Window *w;
	DrawPixelInfo bk;
	_cur_dpi = &bk;

	for(w=_windows; w!=_last_window; w++) {
		if (right > w->left &&
				bottom > w->top &&
				left < w->left + w->width &&
				top < w->top + w->height) {
				DrawOverlappedWindow(w, left, top, right, bottom);
			}
	}
}

void DrawOverlappedWindow(Window *w, int left, int top, int right, int bottom)
{
	Window *v = w;
	int x;

	while (++v != _last_window) {
		if (right > v->left &&
				bottom > v->top &&
				left < v->left + v->width &&
				top < v->top + v->height) {

			if (left < (x=v->left)) {
				DrawOverlappedWindow(w, left, top, x, bottom);
				DrawOverlappedWindow(w, x, top, right, bottom);
				return;
			}

			if (right > (x=v->left + v->width)) {
				DrawOverlappedWindow(w, left, top, x, bottom);
				DrawOverlappedWindow(w, x, top, right, bottom);
				return;
			}

			if (top < (x=v->top)) {
				DrawOverlappedWindow(w, left, top, right, x);
				DrawOverlappedWindow(w, left, x, right, bottom);
				return;
			}

			if (bottom > (x=v->top + v->height)) {
				DrawOverlappedWindow(w, left, top, right, x);
				DrawOverlappedWindow(w, left, x, right, bottom);
				return;
			}

			return;
		}
	}

	{
		DrawPixelInfo *dp = _cur_dpi;
		dp->width = right - left;
		dp->height = bottom - top;
		dp->left = left - w->left;
		dp->top = top - w->top;
		dp->pitch = _screen.pitch;
		dp->dst_ptr = _screen.dst_ptr + top * _screen.pitch + left;
		dp->zoom = 0;
		CallWindowEventNP(w, WE_PAINT);
	}
}

void CallWindowEventNP(Window *w, int event)
{
	WindowEvent e;

	e.event = event;
	w->wndproc(w, &e);
}

void SetWindowDirty(const Window* w)
{
	if (w == NULL) return;
	SetDirtyBlocks(w->left, w->top, w->left + w->width, w->top + w->height);
}

void DeleteWindow(Window *w)
{
	WindowClass wc;
	WindowNumber wn;
	ViewPort *vp;
	Window *v;
	int count;

	if (w == NULL)
		return;

	if (_thd.place_mode != 0 && _thd.window_class == w->window_class && _thd.window_number == w->window_number) {
		ResetObjectToPlace();
	}

	wc = w->window_class;
	wn = w->window_number;

	CallWindowEventNP(w, WE_DESTROY);

	w = FindWindowById(wc, wn);

	vp = w->viewport;
	w->viewport = NULL;
	if (vp != NULL) {
		_active_viewports &= ~(1 << (vp - _viewports));
		vp->width = 0;
	}

	SetWindowDirty(w);

	free(w->widget);

	v = --_last_window;
	count = (byte*)v - (byte*)w;
	memmove(w, w + 1, count);
}

Window *FindWindowById(WindowClass cls, WindowNumber number)
{
	Window *w;

	for(w=_windows; w!=_last_window; w++) {
		if (w->window_class == cls &&
			  w->window_number == number) {
			return w;
		}
	}

	return NULL;
}

void DeleteWindowById(WindowClass cls, WindowNumber number)
{
	DeleteWindow(FindWindowById(cls, number));
}

void DeleteWindowByClass(WindowClass cls)
{
	Window *w;
	for (w = _windows; w != _last_window;) {
		if (w->window_class == cls) {
			DeleteWindow(w);
			w = _windows;
		} else
			w++;
	}
}

Window *BringWindowToFrontById(WindowClass cls, WindowNumber number)
{
	Window *w = FindWindowById(cls, number);

	if (w != NULL) {
		w->flags4 |= WF_WHITE_BORDER_MASK;
		SetWindowDirty(w);
		w = BringWindowToFront(w);
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
 * @param w window that is put into the foreground
 */
Window *BringWindowToFront(Window *w)
{
	Window *v;
	Window temp;

	v = _last_window;
	do {
		if (--v < _windows)
			return w;
	} while (IsVitalWindow(v));

	if (w == v)
		return w;

	assert(w < v);

	temp = *w;
	memmove(w, w + 1, (v - w) * sizeof(Window));
	*v = temp;

	SetWindowDirty(v);

	return v;
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
	Window *w;
	for (w = _windows; w < endof(_windows); w++) {
		if (w->window_class != WC_MAIN_WINDOW && !IsVitalWindow(w) && !(w->flags4 & WF_STICKY) )
				return w;
	}
	return NULL;
}

/** A window must be freed, and all are marked as important windows. Ease the
 * restriction a bit by allowing to delete sticky windows. Keep important/vital
 * windows intact (Main window, Toolbar, Statusbar, News Window, Chatbar)
 * @see FindDeletableWindow()
 * @return w Pointer to the window that is being deleted
 */
static Window *ForceFindDeletableWindow(void)
{
	Window *w;
	for (w = _windows;; w++) {
		assert(w < _last_window);

		if (w->window_class != WC_MAIN_WINDOW && !IsVitalWindow(w))
				return w;
	}
}

bool IsWindowOfPrototype(Window *w, const Widget *widget)
{
	return (w->original_widget == widget);
}

/* Copies 'widget' to 'w->widget' to allow for resizable windows */
void AssignWidgetToWindow(Window *w, const Widget *widget)
{
	w->original_widget = widget;

	if (widget != NULL) {
		const Widget *wi = widget;
		uint index = 1;
		while (wi->type != WWT_LAST) {
			wi++;
			index++;
		}

		w->widget = realloc(w->widget, sizeof(Widget) * index);
		memcpy(w->widget, widget, sizeof(Widget) * index);
	} else
		w->widget = NULL;
}

/** Open a new window. If there is no space for a new window, close an open
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
	Window *w = _last_window; // last window keeps track of the highest open window

	// We have run out of windows, close one and use that as the place for our new one
	if (w >= endof(_windows)) {
		w = FindDeletableWindow();

		if (w == NULL) // no window found, force it!
			w = ForceFindDeletableWindow();

		DeleteWindow(w);
		w = _last_window;
	}

	/* XXX - This very strange construction makes sure that the chatbar is always
	 * on top of other windows. Why? It is created as last_window (so, on top).
	 * Any other window will go below toolbar/statusbar/news window, which implicitely
	 * also means it is below the chatbar. Very likely needs heavy improvement
	 * to de-braindeadize */
	if (w != _windows && cls != WC_SEND_NETWORK_MSG) {
		Window *v;

		/* XXX - if not this order (toolbar/statusbar and then news), game would
		 * crash because it will try to copy a negative size for the news-window.
		 * Eg. window was already moved BELOW news (which is below toolbar/statusbar)
		 * and now needs to move below those too. That is a negative move. */
		v = FindWindowById(WC_MAIN_TOOLBAR, 0);
		if (v != NULL) {
			memmove(v+1, v, (byte*)w - (byte*)v);
			w = v;
		}

		v = FindWindowById(WC_STATUS_BAR, 0);
		if (v != NULL) {
			memmove(v+1, v, (byte*)w - (byte*)v);
			w = v;
		}

		v = FindWindowById(WC_NEWS_WINDOW, 0);
		if (v != NULL) {
			memmove(v+1, v, (byte*)w - (byte*)v);
			w = v;
		}
	}

	// Set up window properties
	memset(w, 0, sizeof(Window));
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

	_last_window++;

	SetWindowDirty(w);

	CallWindowEventNP(w, WE_CREATE);

	return w;
}

Window *AllocateWindowAutoPlace2(
	WindowClass exist_class,
	WindowNumber exist_num,
	int width,
	int height,
	WindowProc *proc,
	WindowClass cls,
	const Widget *widget)
{
	Window *w;
	int x;

	w = FindWindowById(exist_class, exist_num);
	if (w == NULL || w->left >= (_screen.width-20) || w->left <= -60 || w->top >= (_screen.height-20)) {
		return AllocateWindowAutoPlace(width,height,proc,cls,widget);
	}

	x = w->left;
	if (x > _screen.width - width)
		x = (_screen.width - width) - 20;

	return AllocateWindow(x+10,w->top+10,width,height,proc,cls,widget);
}


typedef struct SizeRect {
	int left,top,width,height;
} SizeRect;


static SizeRect _awap_r;

static bool IsGoodAutoPlace1(int left, int top)
{
	int right,bottom;
	Window *w;

	_awap_r.left= left;
	_awap_r.top = top;
	right = _awap_r.width + left;
	bottom = _awap_r.height + top;

	if (left < 0 || top < 22 || right > _screen.width || bottom > _screen.height)
		return false;

	// Make sure it is not obscured by any window.
	for(w=_windows; w!=_last_window; w++) {
		if (w->window_class == WC_MAIN_WINDOW)
			continue;

		if (right > w->left &&
		    w->left + w->width > left &&
				bottom > w->top &&
				w->top + w->height > top)
					return false;
	}

	return true;
}

static bool IsGoodAutoPlace2(int left, int top)
{
	int width,height;
	Window *w;

	_awap_r.left= left;
	_awap_r.top = top;
	width = _awap_r.width;
	height = _awap_r.height;

	if (left < -(width>>2) || left > _screen.width - (width>>1))
		return false;
	if (top < 22 || top > _screen.height - (height>>2))
		return false;

	// Make sure it is not obscured by any window.
	for(w=_windows; w!=_last_window; w++) {
		if (w->window_class == WC_MAIN_WINDOW)
			continue;

		if (left + width > w->left &&
		    w->left + w->width > left &&
				top + height > w->top &&
				w->top + w->height > top)
					return false;
	}

	return true;
}

static Point GetAutoPlacePosition(int width, int height)
{
	Window *w;
	Point pt;

	_awap_r.width = width;
	_awap_r.height = height;

	if (IsGoodAutoPlace1(0, 24)) goto ok_pos;

	for(w=_windows; w!=_last_window; w++) {
		if (w->window_class == WC_MAIN_WINDOW)
			continue;

		if (IsGoodAutoPlace1(w->left+w->width+2,w->top)) goto ok_pos;
		if (IsGoodAutoPlace1(w->left-   width-2,w->top)) goto ok_pos;
		if (IsGoodAutoPlace1(w->left,w->top+w->height+2)) goto ok_pos;
		if (IsGoodAutoPlace1(w->left,w->top-   height-2)) goto ok_pos;
		if (IsGoodAutoPlace1(w->left+w->width+2,w->top+w->height-height)) goto ok_pos;
		if (IsGoodAutoPlace1(w->left-   width-2,w->top+w->height-height)) goto ok_pos;
		if (IsGoodAutoPlace1(w->left+w->width-width,w->top+w->height+2)) goto ok_pos;
		if (IsGoodAutoPlace1(w->left+w->width-width,w->top-   height-2)) goto ok_pos;
	}

	for(w=_windows; w!=_last_window; w++) {
		if (w->window_class == WC_MAIN_WINDOW)
			continue;

		if (IsGoodAutoPlace2(w->left+w->width+2,w->top)) goto ok_pos;
		if (IsGoodAutoPlace2(w->left-   width-2,w->top)) goto ok_pos;
		if (IsGoodAutoPlace2(w->left,w->top+w->height+2)) goto ok_pos;
		if (IsGoodAutoPlace2(w->left,w->top-   height-2)) goto ok_pos;
	}

	{
		int left=0,top=24;

restart:;
		for(w=_windows; w!=_last_window; w++) {
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

Window *AllocateWindowAutoPlace(
	int width,
	int height,
	WindowProc *proc,
	WindowClass cls,
	const Widget *widget) {

	Point pt = GetAutoPlacePosition(width, height);
	return AllocateWindow(pt.x, pt.y, width, height, proc, cls, widget);
}

Window *AllocateWindowDescFront(const WindowDesc *desc, int value)
{
	Window *w;

	if (BringWindowToFrontById(desc->cls, value))
		return NULL;
	w = AllocateWindowDesc(desc);
	w->window_number = value;
	return w;
}

Window *AllocateWindowDesc(const WindowDesc *desc)
{
	Point pt;
	Window *w;

	if (desc->parent_cls != WC_MAIN_WINDOW &&
			(w = FindWindowById(desc->parent_cls, _alloc_wnd_parent_num), _alloc_wnd_parent_num=0, w) != NULL &&
			w->left < _screen.width-20 && w->left > -60 && w->top < _screen.height-20) {
		pt.x = w->left + 10;
		if (pt.x > _screen.width + 10 - desc->width)
			pt.x = (_screen.width + 10 - desc->width) - 20;
		pt.y = w->top + 10;
	} else if (desc->cls == WC_BUILD_TOOLBAR) { // open Build Toolbars aligned
		/* Override the position if a toolbar is opened according to the place of the maintoolbar
		 * The main toolbar (WC_MAIN_TOOLBAR) is 640px in width */
		switch (_patches.toolbar_pos) {
			case 1:  pt.x = ((_screen.width + 640) >> 1) - desc->width; break;
			case 2:  pt.x = _screen.width - desc->width; break;
			default: pt.x = 640 - desc->width;
		}
		pt.y = desc->top;
	} else {
		pt.x = desc->left;
		pt.y = desc->top;
		if (pt.x == WDP_AUTO) {
			pt = GetAutoPlacePosition(desc->width, desc->height);
		} else {
			if (pt.x == WDP_CENTER) pt.x = (_screen.width - desc->width) >> 1;
			if (pt.y == WDP_CENTER) pt.y = (_screen.height - desc->height) >> 1;
			else if(pt.y < 0) pt.y = _screen.height + pt.y; // if y is negative, it's from the bottom of the screen
		}
	}

	w = AllocateWindow(pt.x, pt.y, desc->width, desc->height, desc->proc, desc->cls, desc->widgets);
	w->desc_flags = desc->flags;
	return w;
}

Window *FindWindowFromPt(int x, int y)
{
	Window *w;

	for(w=_last_window; w != _windows;) {
		--w;
		if (IS_INSIDE_1D(x, w->left, w->width) &&
		    IS_INSIDE_1D(y, w->top, w->height))
					return w;
	}

	return NULL;
}

void InitWindowSystem(void)
{
	IConsoleClose();

	memset(&_windows, 0, sizeof(_windows));
	_last_window = _windows;
	memset(_viewports, 0, sizeof(_viewports));
	_active_viewports = 0;
	_no_scroll = 0;
}

void UnInitWindowSystem(void)
{
	Window *w;
	// delete all malloced widgets
	for (w = _windows; w != _last_window; w++) {
		free(w->widget);
		w->widget = NULL;
	}
}

void ResetWindowSystem(void)
{
	UnInitWindowSystem();
	InitWindowSystem();
	_thd.pos.x = 0;
	_thd.pos.y = 0;
}

static void DecreaseWindowCounters(void)
{
	Window *w;


	for (w = _last_window; w != _windows;) {
		--w;
		// Unclick scrollbar buttons if they are pressed.
		if (w->flags4 & (WF_SCROLL_DOWN | WF_SCROLL_UP)) {
			w->flags4 &= ~(WF_SCROLL_DOWN | WF_SCROLL_UP);
			SetWindowDirty(w);
		}
		CallWindowEventNP(w, WE_MOUSELOOP);
	}

	for (w = _last_window; w != _windows;) {
		--w;

		if (w->flags4&WF_TIMEOUT_MASK && !(--w->flags4&WF_TIMEOUT_MASK)) {
			CallWindowEventNP(w, WE_TIMEOUT);
			if (w->desc_flags & WDF_UNCLICK_BUTTONS)
				UnclickWindowButtons(w);
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

	if (_special_mouse_mode != WSM_PRESIZE)
		return;

	if ((w = GetCallbackWnd()) == NULL)
		return;

	e.place.pt = GetTileBelowCursor();
	if (e.place.pt.x == -1) {
		_thd.selend.x = -1;
		return;
	}
	e.place.tile = TileVirtXY(e.place.pt.x, e.place.pt.y);
	e.event = WE_PLACE_PRESIZE;
	w->wndproc(w, &e);
}

static bool HandleDragDrop(void)
{
	Window *w;
	WindowEvent e;

	if (_special_mouse_mode != WSM_DRAGDROP)
		return true;

	if (_left_button_down)
		return false;

	w = GetCallbackWnd();

	ResetObjectToPlace();

	if (w) {
		// send an event in client coordinates.
		e.event = WE_DRAGDROP;
		e.dragdrop.pt.x = _cursor.pos.x - w->left;
		e.dragdrop.pt.y = _cursor.pos.y - w->top;
		e.dragdrop.widget = GetWidgetFromPos(w, e.dragdrop.pt.x, e.dragdrop.pt.y);
		w->wndproc(w, &e);
	}
	return false;
}

static bool HandlePopupMenu(void)
{
	Window *w;
	WindowEvent e;

	if (!_popup_menu_active)
		return true;

	w = FindWindowById(WC_TOOLBAR_MENU, 0);
	if (w == NULL) {
		_popup_menu_active = false;
		return false;
	}

	if (_left_button_down) {
		e.event = WE_POPUPMENU_OVER;
		e.popupmenu.pt = _cursor.pos;
	} else {
		_popup_menu_active = false;
		e.event = WE_POPUPMENU_SELECT;
		e.popupmenu.pt = _cursor.pos;
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
	if (last_w && last_w != w) {
		e.event = WE_MOUSEOVER;
		e.mouseover.pt.x = -1;
		e.mouseover.pt.y = -1;
		if (last_w->wndproc)
			last_w->wndproc(last_w, &e);
	}
	last_w = w;

	if (w) {
		// send an event in client coordinates.
		e.event = WE_MOUSEOVER;
		e.mouseover.pt.x = _cursor.pos.x - w->left;
		e.mouseover.pt.y = _cursor.pos.y - w->top;
		if (w->widget != NULL) {
			e.mouseover.widget = GetWidgetFromPos(w, e.mouseover.pt.x, e.mouseover.pt.y);
		}
		w->wndproc(w, &e);
	}

	// Mouseover never stops execution
	return true;
}

static bool HandleWindowDragging(void)
{
	Window *w;
	// Get out immediately if no window is being dragged at all.
	if (!_dragging_window)
		return true;

	// Otherwise find the window...
	for (w = _windows; w != _last_window; w++) {
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
				int hsnap = _patches.window_snap_radius;
				int vsnap = _patches.window_snap_radius;
				int delta;

				for (v = _windows; v != _last_window; ++v) {
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
							if (w->top >= v_bottom)
								ny = v_bottom;
							else if (w->left < nx)
								nx = v->left - 13 - t->left;
							else
								nx = v_right + 13 - t->right;
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

			/* X and Y has to go by step.. calculate it */
			if (w->resize.step_width > 1)
				x = x - (x % (int)w->resize.step_width);

			if (w->resize.step_height > 1)
				y = y - (y % (int)w->resize.step_height);

			/* Check if we don't go below the minimum set size */
			if ((int)w->width + x < (int)w->resize.width)
				x = w->resize.width - w->width;
			if ((int)w->height + y < (int)w->resize.height)
				y = w->resize.height - w->height;

			/* Window already on size */
			if (x == 0 && y == 0)
				return false;

			/* Now find the new cursor pos.. this is NOT _cursor, because
			    we move in steps. */
			_drag_delta.x += x;
			_drag_delta.y += y;

			SetWindowDirty(w);

			/* Scroll through all the windows and update the widgets if needed */
			{
				Widget *wi = w->widget;
				bool resize_height = false;
				bool resize_width = false;

				while (wi->type != WWT_LAST) {
					if (wi->resize_flag != RESIZE_NONE) {
						/* Resize this widget */
						if (wi->resize_flag & RESIZE_LEFT) {
							wi->left += x;
							resize_width = true;
						}
						if (wi->resize_flag & RESIZE_RIGHT) {
							wi->right += x;
							resize_width = true;
						}

						if (wi->resize_flag & RESIZE_TOP) {
							wi->top += y;
							resize_height = true;
						}
						if (wi->resize_flag & RESIZE_BOTTOM) {
							wi->bottom += y;
							resize_height = true;
						}
					}
					wi++;
				}

				/* We resized at least 1 widget, so let's rezise the window totally */
				if (resize_width)
					w->width  = x + w->width;
				if (resize_height)
					w->height = y + w->height;
			}

			e.event = WE_RESIZE;
			e.sizing.size.x = x + w->width;
			e.sizing.size.y = y + w->height;
			e.sizing.diff.x = x;
			e.sizing.diff.y = y;
			w->wndproc(w, &e);

			SetWindowDirty(w);
			return false;
		}
	}

	_dragging_window = false;
	return false;
}

Window *StartWindowDrag(Window *w)
{
	w->flags4 |= WF_DRAGGING;
	_dragging_window = true;

	_drag_delta.x = w->left - _cursor.pos.x;
	_drag_delta.y = w->top  - _cursor.pos.y;

	w = BringWindowToFront(w);
	DeleteWindowById(WC_DROPDOWN_MENU, 0);
	return w;
}

Window *StartWindowSizing(Window *w)
{
	w->flags4 |= WF_SIZING;
	_dragging_window = true;

	_drag_delta.x = _cursor.pos.x;
	_drag_delta.y = _cursor.pos.y;

	w = BringWindowToFront(w);
	DeleteWindowById(WC_DROPDOWN_MENU, 0);
	SetWindowDirty(w);
	return w;
}


static bool HandleScrollbarScrolling(void)
{
	Window *w;
	int i;
	int pos;
	Scrollbar *sb;

	// Get out quickly if no item is being scrolled
	if (!_scrolling_scrollbar)
		return true;

	// Find the scrolling window
	for(w=_windows; w != _last_window; w++) {
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
	Window *w;
	ViewPort *vp;
	int dx,dy, x, y, sub;

	if (!_scrolling_viewport)
		return true;

	if (!_right_button_down) {
stop_capt:;
		_cursor.fix_at = false;
		_scrolling_viewport = false;
		return true;
	}

	w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);
	if (w == NULL) goto stop_capt;

	if (w->window_class != WC_SMALLMAP) {
		vp = IsPtInWindowViewport(w, _cursor.pos.x, _cursor.pos.y);
		if (vp == NULL)
			goto stop_capt;

		WP(w,vp_d).scrollpos_x += _cursor.delta.x << vp->zoom;
		WP(w,vp_d).scrollpos_y += _cursor.delta.y << vp->zoom;
		_cursor.delta.x = _cursor.delta.y = 0;
		return false;
	} else {
		// scroll the smallmap ?
		int hx;
		int hy;
		int hvx;
		int hvy;

		_cursor.fix_at = true;

		dx = _cursor.delta.x;
		dy = _cursor.delta.y;

		x = WP(w,smallmap_d).scroll_x;
		y = WP(w,smallmap_d).scroll_y;

		sub = WP(w,smallmap_d).subscroll + dx;

		x -= (sub >> 2) << 4;
		y += (sub >> 2) << 4;
		sub &= 3;

		x += (dy >> 1) << 4;
		y += (dy >> 1) << 4;

		if (dy & 1) {
			x += 16;
			sub += 2;
			if (sub > 3) {
				sub -= 4;
				x -= 16;
				y += 16;
			}
		}

		hx = (w->widget[4].right  - w->widget[4].left) / 2;
		hy = (w->widget[4].bottom - w->widget[4].top ) / 2;
		hvx = hx * -4 + hy * 8;
		hvy = hx *  4 + hy * 8;
		if (x < -hvx) { x = -hvx; sub = 0; }
		if (x > (int)MapMaxX() * 16 - hvx) { x = MapMaxX() * 16 - hvx; sub = 0; }
		if (y < -hvy) { y = -hvy; sub = 0; }
		if (y > (int)MapMaxY() * 16 - hvy) { y = MapMaxY() * 16 - hvy; sub = 0; }

		WP(w,smallmap_d).scroll_x = x;
		WP(w,smallmap_d).scroll_y = y;
		WP(w,smallmap_d).subscroll = sub;

		_cursor.delta.x = _cursor.delta.y = 0;

		SetWindowDirty(w);
		return false;
	}
}

static Window *MaybeBringWindowToFront(Window *w)
{
	Window *u;

	if (w->window_class == WC_MAIN_WINDOW || IsVitalWindow(w) ||
			w->window_class == WC_TOOLTIPS    || w->window_class == WC_DROPDOWN_MENU)
				return w;

	for (u = w; ++u != _last_window;) {
		if (u->window_class == WC_MAIN_WINDOW || IsVitalWindow(u) ||
			  u->window_class == WC_TOOLTIPS    || u->window_class == WC_DROPDOWN_MENU)
				continue;

		if (w->left + w->width <= u->left ||
				u->left + u->width <= w->left ||
				w->top  + w->height <= u->top ||
				u->top + u->height <= w->top)
					continue;

		return BringWindowToFront(w);
	}

	return w;
}

/** Send a message from one window to another. The receiving window is found by
 * @param w @see Window pointer pointing to the other window
 * @param msg Specifies the message to be sent
 * @param wparam Specifies additional message-specific information
 * @param lparam Specifies additional message-specific information
 */
void SendWindowMessageW(Window *w, uint msg, uint wparam, uint lparam)
{
	WindowEvent e;

	e.message.event  = WE_MESSAGE;
	e.message.msg    = msg;
	e.message.wparam = wparam;
	e.message.lparam = lparam;

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

static void HandleKeypress(uint32 key)
{
	Window *w;
	WindowEvent we;
 /* Stores if a window with a textfield for typing is open
  * If this is the case, keypress events are only passed to windows with text fields and
	* to thein this main toolbar. */
	bool query_open = false;

	// Setup event
	we.keypress.event = WE_KEYPRESS;
	we.keypress.ascii = key & 0xFF;
	we.keypress.keycode = key >> 16;
	we.keypress.cont = true;

	// check if we have a query string window open before allowing hotkeys
	if(FindWindowById(WC_QUERY_STRING, 0)!=NULL || FindWindowById(WC_SEND_NETWORK_MSG, 0)!=NULL || FindWindowById(WC_CONSOLE, 0)!=NULL || FindWindowById(WC_SAVELOAD, 0)!=NULL)
		query_open = true;

	// Call the event, start with the uppermost window.
	for(w=_last_window; w != _windows;) {
		--w;
		// if a query window is open, only call the event for certain window types
		if(query_open && w->window_class!=WC_QUERY_STRING && w->window_class!=WC_SEND_NETWORK_MSG && w->window_class!=WC_CONSOLE && w->window_class!=WC_SAVELOAD)
			continue;
		w->wndproc(w, &we);
		if (!we.keypress.cont)
			break;
	}

	if (we.keypress.cont) {
		w = FindWindowById(WC_MAIN_TOOLBAR, 0);
		// When there is no toolbar w is null, check for that
		if (w != NULL) w->wndproc(w, &we);
	}
}

extern void UpdateTileSelection(void);
extern bool VpHandlePlaceSizingDrag(void);

static void MouseLoop(int click, int mousewheel)
{
	int x,y;
	Window *w;
	ViewPort *vp;

	DecreaseWindowCounters();
	HandlePlacePresize();
	UpdateTileSelection();
	if (!VpHandlePlaceSizingDrag())
		return;

	if (!HandleDragDrop())
		return;

	if (!HandlePopupMenu())
		return;

	if (!HandleWindowDragging())
		return;

	if (!HandleScrollbarScrolling())
		return;

	if (!HandleViewportScroll())
		return;

	if (!HandleMouseOver())
		return;

	x = _cursor.pos.x;
	y = _cursor.pos.y;


	if (click == 0 && mousewheel == 0) {
		if (_patches.autoscroll && _game_mode != GM_MENU) {
			w = FindWindowFromPt(x, y);
			if (w == NULL || w->flags4 & WF_DISABLE_VP_SCROLL ) return;
			vp = IsPtInWindowViewport(w, x, y);
			if (vp) {
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
		return;
	}

	w = FindWindowFromPt(x, y);
	if (w == NULL)
		return;
	w = MaybeBringWindowToFront(w);
	vp = IsPtInWindowViewport(w, x, y);
	if (vp != NULL) {
		if (_game_mode == GM_MENU)
			return;

		// only allow zooming in-out in main window, or in viewports
		if ( mousewheel && !(w->flags4 & WF_DISABLE_VP_SCROLL) &&
			   (w->window_class == WC_MAIN_WINDOW || w->window_class == WC_EXTRA_VIEW_PORT) ) {
			ZoomInOrOutToCursorWindow(mousewheel < 0,w);
		}

		if (click == 1) {
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
		} else if (click == 2) {
			if (!(w->flags4 & WF_DISABLE_VP_SCROLL)) {
				_scrolling_viewport = true;
				_cursor.fix_at = true;
			}
		}
	} else {
		if (mousewheel)
			DispatchMouseWheelEvent(w, GetWidgetFromPos(w, x - w->left, y - w->top), mousewheel);

		if (click == 1)
			DispatchLeftClickEvent(w, x - w->left, y - w->top);
		else if (click == 2)
			DispatchRightClickEvent(w, x - w->left, y - w->top);
	}
}

void InputLoop(void)
{
	int click;
	int mousewheel;

	_current_player = _local_player;

	// Handle pressed keys
	if (_pressed_key) {
		uint32 key = _pressed_key; _pressed_key = 0;
		HandleKeypress(key);
	}

	// Mouse event?
	click = 0;
	if (_left_button_down && !_left_button_clicked) {
		_left_button_clicked = true;
		click = 1;
	} else if (_right_button_clicked) {
		_right_button_clicked = false;
		click = 2;
	}

	mousewheel = 0;
	if (_cursor.wheel) {
		mousewheel = _cursor.wheel;
		_cursor.wheel = 0;
	}

	MouseLoop(click, mousewheel);
}


static int _we4_timer;

void UpdateWindows(void)
{
	Window *w;
	int t;


	if ((t=_we4_timer+1) >= 100) {
		for(w = _last_window; w != _windows;) {
			w--;
			CallWindowEventNP(w, WE_4);
		}
		t = 0;
	}
	_we4_timer = t;

	for(w = _last_window; w != _windows;) {
		w--;
		if (w->flags4 & WF_WHITE_BORDER_MASK) {
			w->flags4 -= WF_WHITE_BORDER_ONE;
			if (!(w->flags4 & WF_WHITE_BORDER_MASK)) {
				SetWindowDirty(w);
			}
		}
	}

	DrawDirtyBlocks();

	for(w = _windows; w!=_last_window; w++) {
		if (w->viewport != NULL)
			UpdateViewportPosition(w);
	}
	DrawTextMessage();
	// Redraw mouse cursor in case it was hidden
	DrawMouseCursor();
}


int GetMenuItemIndex(const Window *w, int x, int y)
{
	if ((x -= w->left) >= 0 && x < w->width && (y -= w->top + 1) >= 0) {
		y /= 10;

		if (y < WP(w,menu_d).item_count && !HASBIT(WP(w,menu_d).disabled_items, y))
			return y;
	}
	return -1;
}

void InvalidateWindow(byte cls, WindowNumber number)
{
	Window *w;

	for(w=_windows; w!=_last_window; w++) {
		if (w->window_class==cls && w->window_number==number)
			SetWindowDirty(w);
	}
}

void InvalidateWidget(Window *w, byte widget_index)
{
	const Widget *wi = &w->widget[widget_index];

	/* Don't redraw the window if the widget is invisible or of no-type */
	if (wi->type == WWT_EMPTY || HASBIT(w->hidden_state, widget_index)) return;

	SetDirtyBlocks(w->left + wi->left, w->top + wi->top, w->left + wi->right + 1, w->top + wi->bottom + 1);
}

void InvalidateWindowWidget(byte cls, WindowNumber number, byte widget_index)
{
	Window *w;

	for(w=_windows; w!=_last_window; w++) {
		if (w->window_class==cls && w->window_number==number) {
			InvalidateWidget(w, widget_index);
		}
	}
}

void InvalidateWindowClasses(byte cls)
{
	Window *w;
	for(w=_windows; w!=_last_window; w++) {
		if (w->window_class==cls)
			SetWindowDirty(w);
	}
}


void CallWindowTickEvent(void)
{
	Window *w;
	for(w=_last_window; w != _windows;) {
		--w;
		CallWindowEventNP(w, WE_TICK);
	}
}

void DeleteNonVitalWindows(void)
{
	Window *w;
	for(w=_windows; w!=_last_window;) {
		if (w->window_class != WC_MAIN_WINDOW &&
				w->window_class != WC_SELECT_GAME &&
				w->window_class != WC_MAIN_TOOLBAR &&
				w->window_class != WC_STATUS_BAR &&
				w->window_class != WC_TOOLBAR_MENU &&
				w->window_class != WC_TOOLTIPS &&
				(w->flags4 & WF_STICKY) == 0) { // do not delete windows which are 'pinned'
			DeleteWindow(w);
			w = _windows;
		} else {
			w++;
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
	Window *w;
	// Delete every window except for stickied ones
	DeleteNonVitalWindows();
	// Delete all sticked windows
	for (w = _windows; w != _last_window;) {
		if (w->flags4 & WF_STICKY) {
			DeleteWindow(w);
			w = _windows;
		} else
			w++;
	}
}

/* Delete all always on-top windows to get an empty screen */
void HideVitalWindows(void)
{
	DeleteWindowById(WC_MAIN_TOOLBAR, 0);
	DeleteWindowById(WC_STATUS_BAR, 0);
}

int PositionMainToolbar(Window *w)
{
	DEBUG(misc, 1) ("Repositioning Main Toolbar...");

	if (w == NULL || w->window_class != WC_MAIN_TOOLBAR)
		w = FindWindowById(WC_MAIN_TOOLBAR, 0);

	switch (_patches.toolbar_pos) {
		case 1:  w->left = (_screen.width - w->width) >> 1; break;
		case 2:  w->left = _screen.width - w->width; break;
		default: w->left = 0;
	}
	SetDirtyBlocks(0, 0, _screen.width, w->height); // invalidate the whole top part
	return w->left;
}

void RelocateAllWindows(int neww, int newh)
{
	Window *w;

	for(w=_windows; w!= _last_window ;w++) {
		int left, top;

		if (w->window_class == WC_MAIN_WINDOW) {
			ViewPort *vp = w->viewport;
			vp->width = w->width = neww;
			vp->height = w->height = newh;
			vp->virtual_width = neww << vp->zoom;
			vp->virtual_height = newh << vp->zoom;
			continue; // don't modify top,left
		}

		IConsoleResize();

		if (w->window_class == WC_MAIN_TOOLBAR) {
			top = w->top;
			left = PositionMainToolbar(w); // changes toolbar orientation
		} else if (w->window_class == WC_SELECT_GAME || w->window_class == WC_GAME_OPTIONS || w->window_class == WC_NETWORK_WINDOW){
			top = (newh - w->height) >> 1;
			left = (neww - w->width) >> 1;
		} else if (w->window_class == WC_NEWS_WINDOW) {
			top = newh - w->height;
			left = (neww - w->width) >> 1;
		} else if (w->window_class == WC_STATUS_BAR) {
			top = newh - w->height;
			left = (neww - w->width) >> 1;
		} else if (w->window_class == WC_SEND_NETWORK_MSG) {
			top = (newh - 26); // 26 = height of status bar + height of chat bar
			left = (neww - w->width) >> 1;
		} else {
			left = w->left;
			if (left + (w->width>>1) >= neww) left = neww - w->width;
			top = w->top;
			if (top + (w->height>>1) >= newh) top = newh - w->height;
		}

		if (w->viewport) {
			w->viewport->left += left - w->left;
			w->viewport->top += top - w->top;
		}

		w->left = left;
		w->top = top;
	}
}
