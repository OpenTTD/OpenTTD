#include "stdafx.h"
#include "ttd.h"
#include "window.h"
#include "gfx.h"
#include "viewport.h"
#include "console.h"

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
		} else if (wi->type == WWT_SCROLLBAR || wi->type == WWT_HSCROLLBAR) {
			ScrollbarClickHandler(w, wi, e.click.pt.x, e.click.pt.y);
		}

		w->wndproc(w, &e);

		if (w->desc_flags & WDF_STD_BTN) {
			if (e.click.widget == 0) DeleteWindow(w);
			else {
				if (e.click.widget == 1) {
					if (_ctrl_pressed)
						StartWindowSizing(w);
					else
						StartWindowDrag(w);
				}
			}
		}
	} else {
		w->wndproc(w, &e);
	}
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


void DispatchMouseWheelEvent(Window *w, int wheel)
{
	if (w->vscroll.count > w->vscroll.cap) {
		int pos = clamp(w->vscroll.pos + wheel, 0, w->vscroll.count - w->vscroll.cap);
		if (pos != w->vscroll.pos) {
			w->vscroll.pos = pos;
			SetWindowDirty(w);
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

void SetWindowDirty(Window *w)
{
	if (w == NULL)
		return;

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

	v = --_last_window;
	count = (byte*)v - (byte*)w;
	memcpy(w, w + 1, count);
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

Window *BringWindowToFrontById(WindowClass cls, WindowNumber number)
{
	Window *w = FindWindowById(cls, number);

	if (w != NULL) {
		w->flags4 |= WF_WHITE_BORDER_MASK;
		SetWindowDirty(w);
		BringWindowToFront(w);
	}

	return w;
}

Window *BringWindowToFront(Window *w)
{
	Window *v;

	v = _last_window;
	do {
		if (--v < _windows)
			return w;
	} while (v->window_class == WC_MAIN_TOOLBAR || v->window_class == WC_STATUS_BAR || v->window_class == WC_NEWS_WINDOW);

	if (w == v)
		return w;

	assert(w < v);

	do {
		memswap(w, w+1, sizeof(Window));
		w++;
	} while (v != w);

	SetWindowDirty(w);

	return w;
}

Window *AllocateWindow(
							int x,
							int y,
							int width,
							int height,
							WindowProc *proc,
							WindowClass cls,
							const Widget *widget)
{
	Window *w;

restart:;
	w = _last_window;

	if (w >= endof(_windows)) {
		for(w=_windows; ;w++) {
			assert(w < _last_window);

			if (w->window_class != WC_MAIN_WINDOW && w->window_class != WC_MAIN_TOOLBAR &&
			    w->window_class != WC_STATUS_BAR && w->window_class != WC_NEWS_WINDOW) {

					DeleteWindow(w);
					goto restart;
			}
		}
	}

	if (w != _windows && cls != WC_NEWS_WINDOW) {
		Window *v;

		v = FindWindowById(WC_MAIN_TOOLBAR, 0);
		if (v) {
			memmove(v+1, v, (byte*)w - (byte*)v);
			w = v;
		}

		v = FindWindowById(WC_STATUS_BAR, 0);
		if (v) {
			memmove(v+1, v, (byte*)w - (byte*)v);
			w = v;
		}
	}

	/* XXX: some more code here */
	w->window_class = cls;
	w->flags4 = WF_WHITE_BORDER_MASK;
	w->caption_color = 0xFF;
	w->window_number = 0;
	w->left = x;
	w->top = y;
	w->width = width;
	w->height = height;
	w->viewport = NULL;
	w->desc_flags = 0;
//	w->landscape_assoc = 0xFFFF;
	w->wndproc = proc;
	w->click_state = 0;
	w->disabled_state = 0;
	w->hidden_state = 0;
//	w->unk22 = 0xFFFF;
	w->vscroll.pos = 0;
	w->vscroll.count = 0;
	w->hscroll.pos = 0;
	w->hscroll.count = 0;
	w->widget = widget;

	{
		int i;
		for (i=0;i<lengthof(w->custom);i++)
			w->custom[i] = 0;
	}

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

Point GetAutoPlacePosition(int width, int height) {
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
	} else if (desc->cls == WC_BUILD_TOOLBAR) {	// open Build Toolbars aligned
		/* Override the position if a toolbar is opened according to the place of the maintoolbar
		 * The main toolbar (WC_MAIN_TOOLBAR) is 640px in width */
		switch (_patches.toolbar_pos) {
		case 1:		pt.x = ((_screen.width + 640) >> 1) - desc->width; break;
		case 2:		pt.x = _screen.width - desc->width; break;
		default:	pt.x = 640 - desc->width;
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


void InitWindowSystem()
{
	IConsoleClose();
	memset(&_windows, 0, sizeof(_windows));
	_last_window = _windows;
	memset(_viewports, 0, sizeof(_viewports));
	_active_viewports = 0;
}

void DecreaseWindowCounters()
{
	Window *w;


	for(w=_last_window; w != _windows;) {
		--w;
		// Unclick scrollbar buttons if they are pressed.
		if (w->flags4 & (WF_SCROLL_DOWN | WF_SCROLL_UP)) {
			w->flags4 &= ~(WF_SCROLL_DOWN | WF_SCROLL_UP);
			SetWindowDirty(w);
		}
		CallWindowEventNP(w, WE_MOUSELOOP);
	}

	for(w=_last_window; w != _windows;) {
		--w;

		if (w->flags4&WF_TIMEOUT_MASK && !(--w->flags4&WF_TIMEOUT_MASK)) {
			CallWindowEventNP(w, WE_TIMEOUT);
			if (w->desc_flags & WDF_UNCLICK_BUTTONS)
				UnclickWindowButtons(w);
		}
	}
}

Window *GetCallbackWnd()
{
	return FindWindowById(_thd.window_class, _thd.window_number);
}

void HandlePlacePresize()
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
	e.place.tile = TILE_FROM_XY(e.place.pt.x, e.place.pt.y);
	e.event = WE_PLACE_PRESIZE;
	w->wndproc(w, &e);
}

bool HandleDragDrop()
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

bool HandlePopupMenu()
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
		w->wndproc(w, &e);
	} else {
		_popup_menu_active = false;
		e.event = WE_POPUPMENU_SELECT;
		e.popupmenu.pt = _cursor.pos;
		w->wndproc(w, &e);
	}

	return false;
}

bool HandleWindowDragging()
{
	Window *w;
	// Get out immediately if no window is being dragged at all.
	if (!_dragging_window)
		return true;

	// Otherwise find the window...
	for (w = _windows; w != _last_window; w++) {
		if (w->flags4 & WF_DRAGGING) {
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
				uint hsnap = _patches.window_snap_radius;
				uint vsnap = _patches.window_snap_radius;
				uint delta;

				// Snap at screen borders
				// Left screen border
				delta = abs(x);
				if (delta <= hsnap) {
					nx = 0;
					hsnap = delta;
				}

				// Right screen border
				delta = abs(_screen.width - x - w->width);
				if (delta <= hsnap) {
					nx = _screen.width - w->width;
					hsnap = delta;
				}

				// Top of screen
				delta = abs(y);
				if (delta <= vsnap) {
					ny = 0;
					vsnap = delta;
				}

				// Bottom of screen
				delta = abs(_screen.height - y - w->height);
				if (delta <= vsnap) {
					ny = _screen.height - w->height;
					vsnap = delta;
				}

				// Snap at other windows
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
				}
			}

			// Make sure the window doesn't leave the screen
			// 13 is the height of the title bar
			nx = clamp(nx, 13 - w->width, _screen.width - 13);
			ny = clamp(ny, 0, _screen.height - 13);

			if (w->viewport != NULL) {
				w->viewport->left += nx - w->left;
				w->viewport->top  += ny - w->top;
			}
			w->left = nx;
			w->top  = ny;

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
	_cursorpos_drag_start = _cursor.pos;
	w = BringWindowToFront(w);
	DeleteWindowById(WC_DROPDOWN_MENU, 0);
	return w;
}


bool HandleScrollbarScrolling()
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

bool HandleViewportScroll()
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

		if (x < 16) { x = 16; sub = 0; }
		if (x > (TILES_X-2)*16) { x = (TILES_X-2)*16; sub = 0; }
		if (y < -1120) { y = -1120; sub = 0; }
		if (y > (TILE_X_MAX-40) * 16) { y = (TILE_X_MAX-40) * 16; sub = 0; }

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

	if (w->window_class == WC_MAIN_WINDOW ||
			w->window_class == WC_MAIN_TOOLBAR ||
			w->window_class == WC_STATUS_BAR ||
			w->window_class == WC_NEWS_WINDOW ||
			w->window_class == WC_TOOLTIPS ||
			w->window_class == WC_DROPDOWN_MENU)
				return w;

	for(u=w; ++u != _last_window;) {
		if (u->window_class == WC_MAIN_WINDOW || u->window_class==WC_MAIN_TOOLBAR || u->window_class==WC_STATUS_BAR ||
				u->window_class == WC_NEWS_WINDOW || u->window_class == WC_TOOLTIPS || u->window_class == WC_DROPDOWN_MENU)
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

static void HandleKeypress(uint32 key)
{
	Window *w;
	WindowEvent we;

	// Setup event
	we.keypress.event = WE_KEYPRESS;
	we.keypress.ascii = key & 0xFF;
	we.keypress.keycode = key >> 16;
	we.keypress.cont = true;

	// Call the event, start with the uppermost window.
	for(w=_last_window; w != _windows;) {
		--w;
		w->wndproc(w, &we);
		if (!we.keypress.cont)
			break;
	}
}

extern void UpdateTileSelection();
extern bool VpHandlePlaceSizingDrag();

void MouseLoop()
{
	int x,y;
	Window *w;
	ViewPort *vp;
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
				if (x-15<0) { WP(w,vp_d).scrollpos_x += (x-15) * scrollspeed << vp->zoom; }
				else if (15-(vp->width-x) > 0) { WP(w,vp_d).scrollpos_x += (15-(vp->width-x))*scrollspeed << vp->zoom; }
				if (y-15<0) { WP(w,vp_d).scrollpos_y += (y-15)*scrollspeed << vp->zoom; }
				else if (15-(vp->height-y) > 0) { WP(w,vp_d).scrollpos_y += (15-(vp->height-y))*scrollspeed << vp->zoom; }
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
					!(_cursor.sprite == 0x2CF || _cursor.sprite == 0x2D2) &&
					_pause != 0 &&
					!_cheats.build_in_pause.value)
						return;

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
			DispatchMouseWheelEvent(w, mousewheel);

		if (click == 1)
			DispatchLeftClickEvent(w, x - w->left, y - w->top);
		else if (click == 2)
			DispatchRightClickEvent(w, x - w->left, y - w->top);
	}
}

static int _we4_timer;

extern uint32 _pixels_redrawn;

void UpdateWindows()
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
	// Redraw mouse cursor in case it was hidden
	DrawMouseCursor();
}


int GetMenuItemIndex(Window *w, int x, int y)
{
	if ((x -= w->left) >= 0 && x < w->width && (y -= w->top + 1) >= 0) {
		y /= 10;

		if (y < WP(w,menu_d).item_count)
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
//	if (wi->left != -2) {
		SetDirtyBlocks(
			w->left + wi->left,
			w->top + wi->top,
			w->left + wi->right + 1,
			w->top + wi->bottom + 1);
//	}
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


void CallWindowTickEvent()
{
	Window *w;
	for(w=_last_window; w != _windows;) {
		--w;
		CallWindowEventNP(w, WE_TICK);
	}
}

void DeleteNonVitalWindows()
{
	Window *w;
	for(w=_windows; w!=_last_window;) {
		if (w->window_class != WC_MAIN_WINDOW &&
				w->window_class != WC_SELECT_GAME &&
				w->window_class != WC_MAIN_TOOLBAR &&
				w->window_class != WC_STATUS_BAR &&
				w->window_class != WC_TOOLBAR_MENU &&
				w->window_class != WC_TOOLTIPS) {
			DeleteWindow(w);
			w = _windows;
		} else {
			w++;
		}
	}
}

int PositionMainToolbar(Window *w)
{
	DEBUG(misc, 1) ("Repositioning Main Toolbar...");

	if (w == NULL || w->window_class != WC_MAIN_TOOLBAR)
		w = FindWindowById(WC_MAIN_TOOLBAR, 0);

	switch (_patches.toolbar_pos) {
		case 1:		w->left = (_screen.width - w->width) >> 1; break;
		case 2:		w->left = _screen.width - w->width; break;
		default:	w->left = 0;
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
