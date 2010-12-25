/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file window.cpp Windowing system, widgets and events */

#include "stdafx.h"
#include <stdarg.h>
#include "openttd.h"
#include "company_func.h"
#include "gfx_func.h"
#include "console_func.h"
#include "console_gui.h"
#include "viewport_func.h"
#include "genworld.h"
#include "blitter/factory.hpp"
#include "zoom_func.h"
#include "map_func.h"
#include "vehicle_base.h"
#include "cheat_type.h"
#include "window_func.h"
#include "tilehighlight_func.h"
#include "network/network.h"
#include "querystring_gui.h"
#include "widgets/dropdown_func.h"
#include "strings_func.h"
#include "settings_type.h"
#include "newgrf_debug.h"
#include "hotkeys.h"
#include "toolbar_gui.h"
#include "statusbar_gui.h"

#include "table/sprites.h"

static Point _drag_delta; ///< delta between mouse cursor and upper left corner of dragged window
static Window *_mouseover_last_w = NULL; ///< Window of the last #MOUSEOVER event.

/** List of windows opened at the screen sorted from the front. */
Window *_z_front_window = NULL;
/** List of windows opened at the screen sorted from the back. */
Window *_z_back_window  = NULL;

/*
 * Window that currently has focus. - The main purpose is to generate
 * #FocusLost events, not to give next window in z-order focus when a
 * window is closed.
 */
Window *_focused_window;

Point _cursorpos_drag_start;

int _scrollbar_start_pos;
int _scrollbar_size;
byte _scroller_click_timeout = 0;

bool _scrolling_viewport;  ///< A viewport is being scrolled with the mouse.
bool _mouse_hovering;      ///< The mouse is hovering over the same point.

SpecialMouseMode _special_mouse_mode; ///< Mode of the mouse.

/** Window description constructor. */
WindowDesc::WindowDesc(WindowPosition def_pos, int16 def_width, int16 def_height,
			WindowClass window_class, WindowClass parent_class, uint32 flags,
			const NWidgetPart *nwid_parts, int16 nwid_length) :
	default_pos(def_pos),
	default_width(def_width),
	default_height(def_height),
	cls(window_class),
	parent_cls(parent_class),
	flags(flags),
	nwid_parts(nwid_parts),
	nwid_length(nwid_length)
{
}

WindowDesc::~WindowDesc()
{
}

/**
 * Compute the row of a widget that a user clicked in.
 * @param clickpos    Vertical position of the mouse click.
 * @param widget      Widget number of the widget clicked in.
 * @param padding     Amount of empty space between the widget edge and the top of the first row.
 * @param line_height Height of a single row. A negative value means using the vertical resize step of the widget.
 * @return Row number clicked at. If clicked at a wrong position, #INT_MAX is returned.
 * @note The widget does not know where a list printed at the widget ends, so below a list is not a wrong position.
 */
int Window::GetRowFromWidget(int clickpos, int widget, int padding, int line_height) const
{
	const NWidgetBase *wid = this->GetWidget<NWidgetBase>(widget);
	if (line_height < 0) line_height = wid->resize_y;
	if (clickpos < (int)wid->pos_y + padding) return INT_MAX;
	return (clickpos - (int)wid->pos_y - padding) / line_height;
}

/**
 * Return the Scrollbar to a widget index.
 * @param widnum Scrollbar widget index
 * @return Scrollbar to the widget
 */
const Scrollbar *Window::GetScrollbar(uint widnum) const
{
	return this->GetWidget<NWidgetScrollbar>(widnum);
}

/**
 * Return the Scrollbar to a widget index.
 * @param widnum Scrollbar widget index
 * @return Scrollbar to the widget
 */
Scrollbar *Window::GetScrollbar(uint widnum)
{
	return this->GetWidget<NWidgetScrollbar>(widnum);
}


/**
 * Set the window that has the focus
 * @param w The window to set the focus on
 */
void SetFocusedWindow(Window *w)
{
	if (_focused_window == w) return;

	/* Invalidate focused widget */
	if (_focused_window != NULL) {
		if (_focused_window->nested_focus != NULL) _focused_window->nested_focus->SetDirty(_focused_window);
	}

	/* Remember which window was previously focused */
	Window *old_focused = _focused_window;
	_focused_window = w;

	/* So we can inform it that it lost focus */
	if (old_focused != NULL) old_focused->OnFocusLost();
	if (_focused_window != NULL) _focused_window->OnFocus();
}

/**
 * Check if an edit box is in global focus. That is if focused window
 * has a edit box as focused widget, or if a console is focused.
 * @return returns true if an edit box is in global focus or if the focused window is a console, else false
 */
static bool EditBoxInGlobalFocus()
{
	if (_focused_window == NULL) return false;

	/* The console does not have an edit box so a special case is needed. */
	if (_focused_window->window_class == WC_CONSOLE) return true;

	return _focused_window->nested_focus != NULL && _focused_window->nested_focus->type == WWT_EDITBOX;
}

/**
 * Makes no widget on this window have focus. The function however doesn't change which window has focus.
 */
void Window::UnfocusFocusedWidget()
{
	if (this->nested_focus != NULL) {
		/* Repaint the widget that lost focus. A focused edit box may else leave the caret on the screen. */
		this->nested_focus->SetDirty(this);
		this->nested_focus = NULL;
	}
}

/**
 * Set focus within this window to the given widget. The function however doesn't change which window has focus.
 * @param widget_index Index of the widget in the window to set the focus to.
 * @return Focus has changed.
 */
bool Window::SetFocusedWidget(byte widget_index)
{
	/* Do nothing if widget_index is already focused, or if it wasn't a valid widget. */
	if (widget_index >= this->nested_array_size) return false;

	assert(this->nested_array[widget_index] != NULL); // Setting focus to a non-existing widget is a bad idea.
	if (this->nested_focus != NULL) {
		if (this->GetWidget<NWidgetCore>(widget_index) == this->nested_focus) return false;

		/* Repaint the widget that lost focus. A focused edit box may else leave the caret on the screen. */
		this->nested_focus->SetDirty(this);
	}
	this->nested_focus = this->GetWidget<NWidgetCore>(widget_index);
	return true;
}

/**
 * Sets the enabled/disabled status of a list of widgets.
 * By default, widgets are enabled.
 * On certain conditions, they have to be disabled.
 * @param disab_stat status to use ie: disabled = true, enabled = false
 * @param widgets list of widgets ended by WIDGET_LIST_END
 */
void CDECL Window::SetWidgetsDisabledState(bool disab_stat, int widgets, ...)
{
	va_list wdg_list;

	va_start(wdg_list, widgets);

	while (widgets != WIDGET_LIST_END) {
		SetWidgetDisabledState(widgets, disab_stat);
		widgets = va_arg(wdg_list, int);
	}

	va_end(wdg_list);
}

/**
 * Sets the lowered/raised status of a list of widgets.
 * @param lowered_stat status to use ie: lowered = true, raised = false
 * @param widgets list of widgets ended by WIDGET_LIST_END
 */
void CDECL Window::SetWidgetsLoweredState(bool lowered_stat, int widgets, ...)
{
	va_list wdg_list;

	va_start(wdg_list, widgets);

	while (widgets != WIDGET_LIST_END) {
		SetWidgetLoweredState(widgets, lowered_stat);
		widgets = va_arg(wdg_list, int);
	}

	va_end(wdg_list);
}

/**
 * Raise the buttons of the window.
 * @param autoraise Raise only the push buttons of the window.
 */
void Window::RaiseButtons(bool autoraise)
{
	for (uint i = 0; i < this->nested_array_size; i++) {
		if (this->nested_array[i] != NULL && (this->nested_array[i]->type & ~WWB_PUSHBUTTON) < WWT_LAST &&
				(!autoraise || (this->nested_array[i]->type & WWB_PUSHBUTTON)) && this->IsWidgetLowered(i)) {
			this->RaiseWidget(i);
			this->SetWidgetDirty(i);
		}
	}
}

/**
 * Invalidate a widget, i.e. mark it as being changed and in need of redraw.
 * @param widget_index the widget to redraw.
 */
void Window::SetWidgetDirty(byte widget_index) const
{
	/* Sometimes this function is called before the window is even fully initialized */
	if (this->nested_array == NULL) return;

	this->nested_array[widget_index]->SetDirty(this);
}

/**
 * Do all things to make a button look clicked and mark it to be
 * unclicked in a few ticks.
 * @param widget the widget to "click"
 */
void Window::HandleButtonClick(byte widget)
{
	this->LowerWidget(widget);
	this->flags4 |= WF_TIMEOUT_BEGIN;
	this->SetWidgetDirty(widget);
}

static void StartWindowDrag(Window *w);
static void StartWindowSizing(Window *w, bool to_left);

/**
 * Dispatch left mouse-button (possibly double) click in window.
 * @param w Window to dispatch event in
 * @param x X coordinate of the click
 * @param y Y coordinate of the click
 * @param click_count Number of fast consecutive clicks at same position
 */
static void DispatchLeftClickEvent(Window *w, int x, int y, int click_count)
{
	NWidgetCore *nw = w->nested_root->GetWidgetFromPos(x, y);
	WidgetType widget_type = (nw != NULL) ? nw->type : WWT_EMPTY;

	bool focused_widget_changed = false;
	/* If clicked on a window that previously did dot have focus */
	if (_focused_window != w &&                 // We already have focus, right?
			(w->desc_flags & WDF_NO_FOCUS) == 0 &&  // Don't lose focus to toolbars
			widget_type != WWT_CLOSEBOX) {          // Don't change focused window if 'X' (close button) was clicked
		focused_widget_changed = true;
		if (_focused_window != NULL) {
			_focused_window->OnFocusLost();

			/* The window that lost focus may have had opened a OSK, window so close it, unless the user has clicked on the OSK window. */
			if (w->window_class != WC_OSK) DeleteWindowById(WC_OSK, 0);
		}
		SetFocusedWindow(w);
		w->OnFocus();
	}

	if (nw == NULL) return; // exit if clicked outside of widgets

	/* don't allow any interaction if the button has been disabled */
	if (nw->IsDisabled()) return;

	int widget_index = nw->index; ///< Index of the widget

	/* Clicked on a widget that is not disabled.
	 * So unless the clicked widget is the caption bar, change focus to this widget */
	if (widget_type != WWT_CAPTION) {
		/* Close the OSK window if a edit box loses focus */
		if (w->nested_focus != NULL &&  w->nested_focus->type == WWT_EDITBOX && w->nested_focus != nw && w->window_class != WC_OSK) {
			DeleteWindowById(WC_OSK, 0);
		}

		/* focused_widget_changed is 'now' only true if the window this widget
		 * is in gained focus. In that case it must remain true, also if the
		 * local widget focus did not change. As such it's the logical-or of
		 * both changed states.
		 *
		 * If this is not preserved, then the OSK window would be opened when
		 * a user has the edit box focused and then click on another window and
		 * then back again on the edit box (to type some text).
		 */
		focused_widget_changed |= w->SetFocusedWidget(widget_index);
	}

	/* Close any child drop down menus. If the button pressed was the drop down
	 * list's own button, then we should not process the click any further. */
	if (HideDropDownMenu(w) == widget_index && widget_index >= 0) return;

	if ((widget_type & ~WWB_PUSHBUTTON) < WWT_LAST && (widget_type & WWB_PUSHBUTTON)) w->HandleButtonClick(widget_index);

	switch (widget_type) {
		case NWID_VSCROLLBAR:
		case NWID_HSCROLLBAR:
			ScrollbarClickHandler(w, nw, x, y);
			break;

		case WWT_EDITBOX:
			if (!focused_widget_changed) { // Only open the OSK window if clicking on an already focused edit box
				/* Open the OSK window if clicked on an edit box */
				QueryStringBaseWindow *qs = dynamic_cast<QueryStringBaseWindow *>(w);
				if (qs != NULL) {
					qs->OnOpenOSKWindow(widget_index);
				}
			}
			break;

		case WWT_CLOSEBOX: // 'X'
			delete w;
			return;

		case WWT_CAPTION: // 'Title bar'
			StartWindowDrag(w);
			return;

		case WWT_RESIZEBOX:
			/* When the resize widget is on the left size of the window
			 * we assume that that button is used to resize to the left. */
			StartWindowSizing(w, (int)nw->pos_x < (w->width / 2));
			nw->SetDirty(w);
			return;

		case WWT_DEBUGBOX:
			w->ShowNewGRFInspectWindow();
			break;

		case WWT_SHADEBOX:
			nw->SetDirty(w);
			w->SetShaded(!w->IsShaded());
			return;

		case WWT_STICKYBOX:
			w->flags4 ^= WF_STICKY;
			nw->SetDirty(w);
			return;

		default:
			break;
	}

	/* Widget has no index, so the window is not interested in it. */
	if (widget_index < 0) return;

	Point pt = { x, y };
	w->OnClick(pt, widget_index, click_count);
}

/**
 * Dispatch right mouse-button click in window.
 * @param w Window to dispatch event in
 * @param x X coordinate of the click
 * @param y Y coordinate of the click
 */
static void DispatchRightClickEvent(Window *w, int x, int y)
{
	NWidgetCore *wid = w->nested_root->GetWidgetFromPos(x, y);
	if (wid == NULL) return;

	/* No widget to handle, or the window is not interested in it. */
	if (wid->index >= 0) {
		Point pt = { x, y };
		if (w->OnRightClick(pt, wid->index)) return;
	}

	if (_settings_client.gui.hover_delay == 0 && wid->tool_tip != 0) GuiShowTooltips(w, wid->tool_tip, 0, NULL, TCC_RIGHT_CLICK);
}

/**
 * Dispatch hover of the mouse over a window.
 * @param w Window to dispatch event in.
 * @param x X coordinate of the click.
 * @param y Y coordinate of the click.
 */
static void DispatchHoverEvent(Window *w, int x, int y)
{
	NWidgetCore *wid = w->nested_root->GetWidgetFromPos(x, y);

	/* No widget to handle */
	if (wid == NULL) return;

	/* Show the tooltip if there is any */
	if (wid->tool_tip != 0) {
		GuiShowTooltips(w, wid->tool_tip);
		return;
	}

	/* Widget has no index, so the window is not interested in it. */
	if (wid->index < 0) return;

	Point pt = { x, y };
	w->OnHover(pt, wid->index);
}

/**
 * Dispatch the mousewheel-action to the window.
 * The window will scroll any compatible scrollbars if the mouse is pointed over the bar or its contents
 * @param w Window
 * @param nwid the widget where the scrollwheel was used
 * @param wheel scroll up or down
 */
static void DispatchMouseWheelEvent(Window *w, const NWidgetCore *nwid, int wheel)
{
	if (nwid == NULL) return;

	/* Using wheel on caption/shade-box shades or unshades the window. */
	if (nwid->type == WWT_CAPTION || nwid->type == WWT_SHADEBOX) {
		w->SetShaded(wheel < 0);
		return;
	}

	/* Scroll the widget attached to the scrollbar. */
	Scrollbar *sb = (nwid->scrollbar_index >= 0 ? w->GetScrollbar(nwid->scrollbar_index) : NULL);
	if (sb != NULL && sb->GetCount() > sb->GetCapacity()) {
		sb->UpdatePosition(wheel);
		w->SetDirty();
	}
}

/**
 * Generate repaint events for the visible part of window w within the rectangle.
 *
 * The function goes recursively upwards in the window stack, and splits the rectangle
 * into multiple pieces at the window edges, so obscured parts are not redrawn.
 *
 * @param w Window that needs to be repainted
 * @param left Left edge of the rectangle that should be repainted
 * @param top Top edge of the rectangle that should be repainted
 * @param right Right edge of the rectangle that should be repainted
 * @param bottom Bottom edge of the rectangle that should be repainted
 */
static void DrawOverlappedWindow(Window *w, int left, int top, int right, int bottom)
{
	const Window *v;
	FOR_ALL_WINDOWS_FROM_BACK_FROM(v, w->z_front) {
		if (right > v->left &&
				bottom > v->top &&
				left < v->left + v->width &&
				top < v->top + v->height) {
			/* v and rectangle intersect with eeach other */
			int x;

			if (left < (x = v->left)) {
				DrawOverlappedWindow(w, left, top, x, bottom);
				DrawOverlappedWindow(w, x, top, right, bottom);
				return;
			}

			if (right > (x = v->left + v->width)) {
				DrawOverlappedWindow(w, left, top, x, bottom);
				DrawOverlappedWindow(w, x, top, right, bottom);
				return;
			}

			if (top < (x = v->top)) {
				DrawOverlappedWindow(w, left, top, right, x);
				DrawOverlappedWindow(w, left, x, right, bottom);
				return;
			}

			if (bottom > (x = v->top + v->height)) {
				DrawOverlappedWindow(w, left, top, right, x);
				DrawOverlappedWindow(w, left, x, right, bottom);
				return;
			}

			return;
		}
	}

	/* Setup blitter, and dispatch a repaint event to window *wz */
	DrawPixelInfo *dp = _cur_dpi;
	dp->width = right - left;
	dp->height = bottom - top;
	dp->left = left - w->left;
	dp->top = top - w->top;
	dp->pitch = _screen.pitch;
	dp->dst_ptr = BlitterFactoryBase::GetCurrentBlitter()->MoveTo(_screen.dst_ptr, left, top);
	dp->zoom = ZOOM_LVL_NORMAL;
	w->OnPaint();
}

/**
 * From a rectangle that needs redrawing, find the windows that intersect with the rectangle.
 * These windows should be re-painted.
 * @param left Left edge of the rectangle that should be repainted
 * @param top Top edge of the rectangle that should be repainted
 * @param right Right edge of the rectangle that should be repainted
 * @param bottom Bottom edge of the rectangle that should be repainted
 */
void DrawOverlappedWindowForAll(int left, int top, int right, int bottom)
{
	Window *w;
	DrawPixelInfo bk;
	_cur_dpi = &bk;

	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (right > w->left &&
				bottom > w->top &&
				left < w->left + w->width &&
				top < w->top + w->height) {
			/* Window w intersects with the rectangle => needs repaint */
			DrawOverlappedWindow(w, left, top, right, bottom);
		}
	}
}

/**
 * Mark entire window as dirty (in need of re-paint)
 * @ingroup dirty
 */
void Window::SetDirty() const
{
	SetDirtyBlocks(this->left, this->top, this->left + this->width, this->top + this->height);
}

/**
 * Re-initialize a window, and optionally change its size.
 * @param rx Horizontal resize of the window.
 * @param ry Vertical resize of the window.
 * @note For just resizing the window, use #ResizeWindow instead.
 */
void Window::ReInit(int rx, int ry)
{
	this->SetDirty(); // Mark whole current window as dirty.

	/* Save current size. */
	int window_width  = this->width;
	int window_height = this->height;

	this->OnInit();
	/* Re-initialize the window from the ground up. No need to change the nested_array, as all widgets stay where they are. */
	this->nested_root->SetupSmallestSize(this, false);
	this->nested_root->AssignSizePosition(ST_SMALLEST, 0, 0, this->nested_root->smallest_x, this->nested_root->smallest_y, _current_text_dir == TD_RTL);
	this->width  = this->nested_root->smallest_x;
	this->height = this->nested_root->smallest_y;
	this->resize.step_width  = this->nested_root->resize_x;
	this->resize.step_height = this->nested_root->resize_y;

	/* Resize as close to the original size + requested resize as possible. */
	window_width  = max(window_width  + rx, this->width);
	window_height = max(window_height + ry, this->height);
	int dx = (this->resize.step_width  == 0) ? 0 : window_width  - this->width;
	int dy = (this->resize.step_height == 0) ? 0 : window_height - this->height;
	/* dx and dy has to go by step.. calculate it.
	 * The cast to int is necessary else dx/dy are implicitly casted to unsigned int, which won't work. */
	if (this->resize.step_width  > 1) dx -= dx % (int)this->resize.step_width;
	if (this->resize.step_height > 1) dy -= dy % (int)this->resize.step_height;

	ResizeWindow(this, dx, dy);
	/* ResizeWindow() does this->SetDirty() already, no need to do it again here. */
}

/**
 * Set the shaded state of the window to \a make_shaded.
 * @param make_shaded If \c true, shade the window (roll up until just the title bar is visible), else unshade/unroll the window to its original size.
 * @note The method uses #Window::ReInit(), thus after the call, the whole window should be considered changed.
 */
void Window::SetShaded(bool make_shaded)
{
	if (this->shade_select == NULL) return;

	int desired = make_shaded ? SZSP_HORIZONTAL : 0;
	if (this->shade_select->shown_plane != desired) {
		if (make_shaded) {
			this->unshaded_size.width  = this->width;
			this->unshaded_size.height = this->height;
			this->shade_select->SetDisplayedPlane(desired);
			this->ReInit(0, -this->height);
		} else {
			this->shade_select->SetDisplayedPlane(desired);
			int dx = ((int)this->unshaded_size.width  > this->width)  ? (int)this->unshaded_size.width  - this->width  : 0;
			int dy = ((int)this->unshaded_size.height > this->height) ? (int)this->unshaded_size.height - this->height : 0;
			this->ReInit(dx, dy);
		}
	}
}

/**
 * Find the Window whose parent pointer points to this window
 * @param w parent Window to find child of
 * @param wc Window class of the window to remove; WC_INVALID if class does not matter
 * @return a Window pointer that is the child of w, or NULL otherwise
 */
static Window *FindChildWindow(const Window *w, WindowClass wc)
{
	Window *v;
	FOR_ALL_WINDOWS_FROM_BACK(v) {
		if ((wc == WC_INVALID || wc == v->window_class) && v->parent == w) return v;
	}

	return NULL;
}

/**
 * Delete all children a window might have in a head-recursive manner
 * @param wc Window class of the window to remove; WC_INVALID if class does not matter
 */
void Window::DeleteChildWindows(WindowClass wc) const
{
	Window *child = FindChildWindow(this, wc);
	while (child != NULL) {
		delete child;
		child = FindChildWindow(this, wc);
	}
}

/**
 * Remove window and all its child windows from the window stack.
 */
Window::~Window()
{
	if (_thd.window_class == this->window_class &&
			_thd.window_number == this->window_number) {
		ResetObjectToPlace();
	}

	/* Prevent Mouseover() from resetting mouse-over coordinates on a non-existing window */
	if (_mouseover_last_w == this) _mouseover_last_w = NULL;

	/* Make sure we don't try to access this window as the focused window when it doesn't exist anymore. */
	if (_focused_window == this) _focused_window = NULL;

	this->DeleteChildWindows();

	if (this->viewport != NULL) DeleteWindowViewport(this);

	this->SetDirty();

	free(this->nested_array); // Contents is released through deletion of #nested_root.
	delete this->nested_root;

	this->window_class = WC_INVALID;
}

/**
 * Find a window by its class and window number
 * @param cls Window class
 * @param number Number of the window within the window class
 * @return Pointer to the found window, or \c NULL if not available
 */
Window *FindWindowById(WindowClass cls, WindowNumber number)
{
	Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class == cls && w->window_number == number) return w;
	}

	return NULL;
}

/**
 * Find any window by its class. Useful when searching for a window that uses
 * the window number as a WindowType, like WC_SEND_NETWORK_MSG.
 * @param cls Window class
 * @return Pointer to the found window, or \c NULL if not available
 */
Window *FindWindowByClass(WindowClass cls)
{
	Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class == cls) return w;
	}

	return NULL;
}

/**
 * Delete a window by its class and window number (if it is open).
 * @param cls Window class
 * @param number Number of the window within the window class
 * @param force force deletion; if false don't delete when stickied
 */
void DeleteWindowById(WindowClass cls, WindowNumber number, bool force)
{
	Window *w = FindWindowById(cls, number);
	if (force || w == NULL ||
			(w->flags4 & WF_STICKY) == 0) {
		delete w;
	}
}

/**
 * Delete all windows of a given class
 * @param cls Window class of windows to delete
 */
void DeleteWindowByClass(WindowClass cls)
{
	Window *w;

restart_search:
	/* When we find the window to delete, we need to restart the search
	 * as deleting this window could cascade in deleting (many) others
	 * anywhere in the z-array */
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class == cls) {
			delete w;
			goto restart_search;
		}
	}
}

/**
 * Delete all windows of a company. We identify windows of a company
 * by looking at the caption colour. If it is equal to the company ID
 * then we say the window belongs to the company and should be deleted
 * @param id company identifier
 */
void DeleteCompanyWindows(CompanyID id)
{
	Window *w;

restart_search:
	/* When we find the window to delete, we need to restart the search
	 * as deleting this window could cascade in deleting (many) others
	 * anywhere in the z-array */
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->owner == id) {
			delete w;
			goto restart_search;
		}
	}

	/* Also delete the company specific windows that don't have a company-colour. */
	DeleteWindowById(WC_BUY_COMPANY, id);
}

/**
 * Change the owner of all the windows one company can take over from another
 * company in the case of a company merger. Do not change ownership of windows
 * that need to be deleted once takeover is complete
 * @param old_owner original owner of the window
 * @param new_owner the new owner of the window
 */
void ChangeWindowOwner(Owner old_owner, Owner new_owner)
{
	Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->owner != old_owner) continue;

		switch (w->window_class) {
			case WC_COMPANY_COLOUR:
			case WC_FINANCES:
			case WC_STATION_LIST:
			case WC_TRAINS_LIST:
			case WC_ROADVEH_LIST:
			case WC_SHIPS_LIST:
			case WC_AIRCRAFT_LIST:
			case WC_BUY_COMPANY:
			case WC_COMPANY:
				continue;

			default:
				w->owner = new_owner;
				break;
		}
	}
}

static void BringWindowToFront(Window *w);

/**
 * Find a window and make it the top-window on the screen.
 * The window gets unshaded if it was shaded, and a white border is drawn at its edges for a brief period of time to visualize its "activation".
 * @param cls WindowClass of the window to activate
 * @param number WindowNumber of the window to activate
 * @return a pointer to the window thus activated
 */
Window *BringWindowToFrontById(WindowClass cls, WindowNumber number)
{
	Window *w = FindWindowById(cls, number);

	if (w != NULL) {
		if (w->IsShaded()) w->SetShaded(false); // Restore original window size if it was shaded.

		w->flags4 |= WF_WHITE_BORDER_MASK;
		BringWindowToFront(w);
		w->SetDirty();
	}

	return w;
}

static inline bool IsVitalWindow(const Window *w)
{
	switch (w->window_class) {
		case WC_MAIN_TOOLBAR:
		case WC_STATUS_BAR:
		case WC_NEWS_WINDOW:
		case WC_SEND_NETWORK_MSG:
			return true;

		default:
			return false;
	}
}

/**
 * On clicking on a window, make it the frontmost window of all. However
 * there are certain windows that always need to be on-top; these include
 * - Toolbar, Statusbar (always on)
 * - New window, Chatbar (only if open)
 * The window is marked dirty for a repaint if the window is actually moved
 * @param w window that is put into the foreground
 * @return pointer to the window, the same as the input pointer
 */
static void BringWindowToFront(Window *w)
{
	Window *v = _z_front_window;

	/* Bring the window just below the vital windows */
	for (; v != NULL && v != w && IsVitalWindow(v); v = v->z_back) { }

	if (v == NULL || w == v) return; // window is already in the right position

	/* w cannot be at the top already! */
	assert(w != _z_front_window);

	if (w->z_back == NULL) {
		_z_back_window = w->z_front;
	} else {
		w->z_back->z_front = w->z_front;
	}
	w->z_front->z_back = w->z_back;

	w->z_front = v->z_front;
	w->z_back = v;

	if (v->z_front == NULL) {
		_z_front_window = w;
	} else {
		v->z_front->z_back = w;
	}
	v->z_front = w;

	w->SetDirty();
}

/**
 * Initializes the data (except the position and initial size) of a new Window.
 * @param desc          Window description.
 * @param window_number Number being assigned to the new window
 * @return Window pointer of the newly created window
 * @pre If nested widgets are used (\a widget is \c NULL), #nested_root and #nested_array_size must be initialized.
 *      In addition, #nested_array is either \c NULL, or already initialized.
 */
void Window::InitializeData(const WindowDesc *desc, WindowNumber window_number)
{
	/* Set up window properties; some of them are needed to set up smallest size below */
	this->window_class = desc->cls;
	this->flags4 |= WF_WHITE_BORDER_MASK; // just opened windows have a white border
	if (desc->default_pos == WDP_CENTER) this->flags4 |= WF_CENTERED;
	this->owner = INVALID_OWNER;
	this->nested_focus = NULL;
	this->window_number = window_number;
	this->desc_flags = desc->flags;

	this->OnInit();
	/* Initialize nested widget tree. */
	if (this->nested_array == NULL) {
		this->nested_array = CallocT<NWidgetBase *>(this->nested_array_size);
		this->nested_root->SetupSmallestSize(this, true);
	} else {
		this->nested_root->SetupSmallestSize(this, false);
	}
	/* Initialize to smallest size. */
	this->nested_root->AssignSizePosition(ST_SMALLEST, 0, 0, this->nested_root->smallest_x, this->nested_root->smallest_y, _current_text_dir == TD_RTL);

	/* Further set up window properties,
	 * this->left, this->top, this->width, this->height, this->resize.width, and this->resize.height are initialized later. */
	this->resize.step_width  = this->nested_root->resize_x;
	this->resize.step_height = this->nested_root->resize_y;

	/* Give focus to the opened window unless it is the OSK window or a text box
	 * of focused window has focus (so we don't interrupt typing). But if the new
	 * window has a text box, then take focus anyway. */
	if (this->window_class != WC_OSK && (!EditBoxInGlobalFocus() || this->nested_root->GetWidgetOfType(WWT_EDITBOX) != NULL)) SetFocusedWindow(this);

	/* Hacky way of specifying always-on-top windows. These windows are
	 * always above other windows because they are moved below them.
	 * status-bar is above news-window because it has been created earlier.
	 * Also, as the chat-window is excluded from this, it will always be
	 * the last window, thus always on top.
	 * XXX - Yes, ugly, probably needs something like w->always_on_top flag
	 * to implement correctly, but even then you need some kind of distinction
	 * between on-top of chat/news and status windows, because these conflict */
	Window *w = _z_front_window;
	if (w != NULL && this->window_class != WC_SEND_NETWORK_MSG && this->window_class != WC_HIGHSCORE && this->window_class != WC_ENDSCREEN) {
		if (FindWindowById(WC_MAIN_TOOLBAR, 0)     != NULL) w = w->z_back;
		if (FindWindowById(WC_STATUS_BAR, 0)       != NULL) w = w->z_back;
		if (FindWindowById(WC_NEWS_WINDOW, 0)      != NULL) w = w->z_back;
		if (FindWindowByClass(WC_SEND_NETWORK_MSG) != NULL) w = w->z_back;

		if (w == NULL) {
			_z_back_window->z_front = this;
			this->z_back = _z_back_window;
			_z_back_window = this;
		} else {
			if (w->z_front == NULL) {
				_z_front_window = this;
			} else {
				this->z_front = w->z_front;
				w->z_front->z_back = this;
			}

			this->z_back = w;
			w->z_front = this;
		}
	} else {
		this->z_back = _z_front_window;
		if (_z_front_window != NULL) {
			_z_front_window->z_front = this;
		} else {
			_z_back_window = this;
		}
		_z_front_window = this;
	}
}

/**
 * Set the position and smallest size of the window.
 * @param x          Offset in pixels from the left of the screen of the new window.
 * @param y          Offset in pixels from the top of the screen of the new window.
 * @param sm_width   Smallest width in pixels of the window.
 * @param sm_height  Smallest height in pixels of the window.
 */
void Window::InitializePositionSize(int x, int y, int sm_width, int sm_height)
{
	this->left = x;
	this->top = y;
	this->width = sm_width;
	this->height = sm_height;
}

/**
 * Resize window towards the default size.
 * Prior to construction, a position for the new window (for its default size)
 * has been found with LocalGetWindowPlacement(). Initially, the window is
 * constructed with minimal size. Resizing the window to its default size is
 * done here.
 * @param def_width default width in pixels of the window
 * @param def_height default height in pixels of the window
 * @see Window::Window(), Window::InitializeData(), Window::InitializePositionSize()
 */
void Window::FindWindowPlacementAndResize(int def_width, int def_height)
{
	def_width  = max(def_width,  this->width); // Don't allow default size to be smaller than smallest size
	def_height = max(def_height, this->height);
	/* Try to make windows smaller when our window is too small.
	 * w->(width|height) is normally the same as min_(width|height),
	 * but this way the GUIs can be made a little more dynamic;
	 * one can use the same spec for multiple windows and those
	 * can then determine the real minimum size of the window. */
	if (this->width != def_width || this->height != def_height) {
		/* Think about the overlapping toolbars when determining the minimum window size */
		int free_height = _screen.height;
		const Window *wt = FindWindowById(WC_STATUS_BAR, 0);
		if (wt != NULL) free_height -= wt->height;
		wt = FindWindowById(WC_MAIN_TOOLBAR, 0);
		if (wt != NULL) free_height -= wt->height;

		int enlarge_x = max(min(def_width  - this->width,  _screen.width - this->width),  0);
		int enlarge_y = max(min(def_height - this->height, free_height   - this->height), 0);

		/* X and Y has to go by step.. calculate it.
		 * The cast to int is necessary else x/y are implicitly casted to
		 * unsigned int, which won't work. */
		if (this->resize.step_width  > 1) enlarge_x -= enlarge_x % (int)this->resize.step_width;
		if (this->resize.step_height > 1) enlarge_y -= enlarge_y % (int)this->resize.step_height;

		ResizeWindow(this, enlarge_x, enlarge_y);
		/* ResizeWindow() calls this->OnResize(). */
	} else {
		/* Always call OnResize; that way the scrollbars and matrices get initialized. */
		this->OnResize();
	}

	int nx = this->left;
	int ny = this->top;

	if (nx + this->width > _screen.width) nx -= (nx + this->width - _screen.width);

	const Window *wt = FindWindowById(WC_MAIN_TOOLBAR, 0);
	ny = max(ny, (wt == NULL || this == wt || this->top == 0) ? 0 : wt->height);
	nx = max(nx, 0);

	if (this->viewport != NULL) {
		this->viewport->left += nx - this->left;
		this->viewport->top  += ny - this->top;
	}
	this->left = nx;
	this->top = ny;

	this->SetDirty();
}

/**
 * Decide whether a given rectangle is a good place to open a completely visible new window.
 * The new window should be within screen borders, and not overlap with another already
 * existing window (except for the main window in the background).
 * @param left    Left edge of the rectangle
 * @param top     Top edge of the rectangle
 * @param width   Width of the rectangle
 * @param height  Height of the rectangle
 * @param pos     If rectangle is good, use this parameter to return the top-left corner of the new window
 * @return Boolean indication that the rectangle is a good place for the new window
 */
static bool IsGoodAutoPlace1(int left, int top, int width, int height, Point &pos)
{
	int right  = width + left;
	int bottom = height + top;

	const Window *main_toolbar = FindWindowByClass(WC_MAIN_TOOLBAR);
	if (left < 0 || (main_toolbar != NULL && top < main_toolbar->height) || right > _screen.width || bottom > _screen.height) return false;

	/* Make sure it is not obscured by any window. */
	const Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class == WC_MAIN_WINDOW) continue;

		if (right > w->left &&
				w->left + w->width > left &&
				bottom > w->top &&
				w->top + w->height > top) {
			return false;
		}
	}

	pos.x = left;
	pos.y = top;
	return true;
}

/**
 * Decide whether a given rectangle is a good place to open a mostly visible new window.
 * The new window should be mostly within screen borders, and not overlap with another already
 * existing window (except for the main window in the background).
 * @param left    Left edge of the rectangle
 * @param top     Top edge of the rectangle
 * @param width   Width of the rectangle
 * @param height  Height of the rectangle
 * @param pos     If rectangle is good, use this parameter to return the top-left corner of the new window
 * @return Boolean indication that the rectangle is a good place for the new window
 */
static bool IsGoodAutoPlace2(int left, int top, int width, int height, Point &pos)
{
	/* Left part of the rectangle may be at most 1/4 off-screen,
	 * right part of the rectangle may be at most 1/2 off-screen
	 */
	if (left < -(width >> 2) || left > _screen.width - (width >> 1)) return false;
	/* Bottom part of the rectangle may be at most 1/4 off-screen */
	if (top < 22 || top > _screen.height - (height >> 2)) return false;

	/* Make sure it is not obscured by any window. */
	const Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class == WC_MAIN_WINDOW) continue;

		if (left + width > w->left &&
				w->left + w->width > left &&
				top + height > w->top &&
				w->top + w->height > top) {
			return false;
		}
	}

	pos.x = left;
	pos.y = top;
	return true;
}

/**
 * Find a good place for opening a new window of a given width and height.
 * @param width  Width of the new window
 * @param height Height of the new window
 * @return Top-left coordinate of the new window
 */
static Point GetAutoPlacePosition(int width, int height)
{
	Point pt;

	/* First attempt, try top-left of the screen */
	const Window *main_toolbar = FindWindowByClass(WC_MAIN_TOOLBAR);
	if (IsGoodAutoPlace1(0, main_toolbar != NULL ? main_toolbar->height + 2 : 2, width, height, pt)) return pt;

	/* Second attempt, try around all existing windows with a distance of 2 pixels.
	 * The new window must be entirely on-screen, and not overlap with an existing window.
	 * Eight starting points are tried, two at each corner.
	 */
	const Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class == WC_MAIN_WINDOW) continue;

		if (IsGoodAutoPlace1(w->left + w->width + 2, w->top, width, height, pt)) return pt;
		if (IsGoodAutoPlace1(w->left - width - 2,    w->top, width, height, pt)) return pt;
		if (IsGoodAutoPlace1(w->left, w->top + w->height + 2, width, height, pt)) return pt;
		if (IsGoodAutoPlace1(w->left, w->top - height - 2,    width, height, pt)) return pt;
		if (IsGoodAutoPlace1(w->left + w->width + 2, w->top + w->height - height, width, height, pt)) return pt;
		if (IsGoodAutoPlace1(w->left - width - 2,    w->top + w->height - height, width, height, pt)) return pt;
		if (IsGoodAutoPlace1(w->left + w->width - width, w->top + w->height + 2, width, height, pt)) return pt;
		if (IsGoodAutoPlace1(w->left + w->width - width, w->top - height - 2,    width, height, pt)) return pt;
	}

	/* Third attempt, try around all existing windows with a distance of 2 pixels.
	 * The new window may be partly off-screen, and must not overlap with an existing window.
	 * Only four starting points are tried.
	 */
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class == WC_MAIN_WINDOW) continue;

		if (IsGoodAutoPlace2(w->left + w->width + 2, w->top, width, height, pt)) return pt;
		if (IsGoodAutoPlace2(w->left - width - 2,    w->top, width, height, pt)) return pt;
		if (IsGoodAutoPlace2(w->left, w->top + w->height + 2, width, height, pt)) return pt;
		if (IsGoodAutoPlace2(w->left, w->top - height - 2,    width, height, pt)) return pt;
	}

	/* Fourth and final attempt, put window at diagonal starting from (0, 24), try multiples
	 * of (+5, +5)
	 */
	int left = 0, top = 24;

restart:
	FOR_ALL_WINDOWS_FROM_BACK(w) {
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

/**
 * Computer the position of the top-left corner of a window to be opened right
 * under the toolbar.
 * @param window_width the width of the window to get the position for
 * @return Coordinate of the top-left corner of the new window.
 */
Point GetToolbarAlignedWindowPosition(int window_width)
{
	const Window *w = FindWindowById(WC_MAIN_TOOLBAR, 0);
	assert(w != NULL);
	Point pt = { _current_text_dir == TD_RTL ? w->left : (w->left + w->width) - window_width, w->top + w->height };
	return pt;
}

/**
 * Compute the position of the top-left corner of a new window that is opened.
 *
 * By default position a child window at an offset of 10/10 of its parent.
 * With the exception of WC_BUILD_TOOLBAR (build railway/roads/ship docks/airports)
 * and WC_SCEN_LAND_GEN (landscaping). Whose child window has an offset of 0/toolbar-height of
 * its parent. So it's exactly under the parent toolbar and no buttons will be covered.
 * However if it falls too extremely outside window positions, reposition
 * it to an automatic place.
 *
 * @param *desc         The pointer to the WindowDesc to be created.
 * @param sm_width      Smallest width of the window.
 * @param sm_height     Smallest height of the window.
 * @param window_number The window number of the new window.
 *
 * @return Coordinate of the top-left corner of the new window.
 */
static Point LocalGetWindowPlacement(const WindowDesc *desc, int16 sm_width, int16 sm_height, int window_number)
{
	Point pt;
	const Window *w;

	int16 default_width  = max(desc->default_width,  sm_width);
	int16 default_height = max(desc->default_height, sm_height);

	if (desc->parent_cls != 0 /* WC_MAIN_WINDOW */ &&
			(w = FindWindowById(desc->parent_cls, window_number)) != NULL &&
			w->left < _screen.width - 20 && w->left > -60 && w->top < _screen.height - 20) {

		pt.x = w->left + ((desc->parent_cls == WC_BUILD_TOOLBAR || desc->parent_cls == WC_SCEN_LAND_GEN) ? 0 : 10);
		if (pt.x > _screen.width + 10 - default_width) {
			pt.x = (_screen.width + 10 - default_width) - 20;
		}
		pt.y = w->top + ((desc->parent_cls == WC_BUILD_TOOLBAR || desc->parent_cls == WC_SCEN_LAND_GEN) ? w->height : 10);
		return pt;
	}

	switch (desc->default_pos) {
		case WDP_ALIGN_TOOLBAR: // Align to the toolbar
			return GetToolbarAlignedWindowPosition(default_width);

		case WDP_AUTO: // Find a good automatic position for the window
			return GetAutoPlacePosition(default_width, default_height);

		case WDP_CENTER: // Centre the window horizontally
			pt.x = (_screen.width - default_width) / 2;
			pt.y = (_screen.height - default_height) / 2;
			break;

		case WDP_MANUAL:
			pt.x = 0;
			pt.y = 0;
			break;

		default:
			NOT_REACHED();
	}

	return pt;
}

/* virtual */ Point Window::OnInitialPosition(const WindowDesc *desc, int16 sm_width, int16 sm_height, int window_number)
{
	return LocalGetWindowPlacement(desc, sm_width, sm_height, window_number);
}

/**
 * Perform the first part of the initialization of a nested widget tree.
 * Construct a nested widget tree in #nested_root, and optionally fill the #nested_array array to provide quick access to the uninitialized widgets.
 * This is mainly useful for setting very basic properties.
 * @param desc        Window description.
 * @param fill_nested Fill the #nested_array (enabling is expensive!).
 * @note Filling the nested array requires an additional traversal through the nested widget tree, and is best performed by #FinishInitNested rather than here.
 */
void Window::CreateNestedTree(const WindowDesc *desc, bool fill_nested)
{
	int biggest_index = -1;
	this->nested_root = MakeWindowNWidgetTree(desc->nwid_parts, desc->nwid_length, &biggest_index, &this->shade_select);
	this->nested_array_size = (uint)(biggest_index + 1);

	if (fill_nested) {
		this->nested_array = CallocT<NWidgetBase *>(this->nested_array_size);
		this->nested_root->FillNestedArray(this->nested_array, this->nested_array_size);
	}
}

/**
 * Perform the second part of the initialization of a nested widget tree.
 * @param desc          Window description.
 * @param window_number Number of the new window.
 */
void Window::FinishInitNested(const WindowDesc *desc, WindowNumber window_number)
{
	this->InitializeData(desc, window_number);
	Point pt = this->OnInitialPosition(desc, this->nested_root->smallest_x, this->nested_root->smallest_y, window_number);
	this->InitializePositionSize(pt.x, pt.y, this->nested_root->smallest_x, this->nested_root->smallest_y);
	this->FindWindowPlacementAndResize(desc->default_width, desc->default_height);
}

/**
 * Perform complete initialization of the #Window with nested widgets, to allow use.
 * @param desc          Window description.
 * @param window_number Number of the new window.
 */
void Window::InitNested(const WindowDesc *desc, WindowNumber window_number)
{
	this->CreateNestedTree(desc, false);
	this->FinishInitNested(desc, window_number);
}

/** Empty constructor, initialization has been moved to #InitNested() called from the constructor of the derived class. */
Window::Window() : scrolling_scrollbar(-1)
{
}

/**
 * Do a search for a window at specific coordinates. For this we start
 * at the topmost window, obviously and work our way down to the bottom
 * @param x position x to query
 * @param y position y to query
 * @return a pointer to the found window if any, NULL otherwise
 */
Window *FindWindowFromPt(int x, int y)
{
	Window *w;
	FOR_ALL_WINDOWS_FROM_FRONT(w) {
		if (IsInsideBS(x, w->left, w->width) && IsInsideBS(y, w->top, w->height)) {
			return w;
		}
	}

	return NULL;
}

/**
 * (re)initialize the windowing system
 */
void InitWindowSystem()
{
	IConsoleClose();

	_z_back_window = NULL;
	_z_front_window = NULL;
	_focused_window = NULL;
	_mouseover_last_w = NULL;
	_scrolling_viewport = false;
	_mouse_hovering = false;

	NWidgetLeaf::InvalidateDimensionCache(); // Reset cached sizes of several widgets.
}

/**
 * Close down the windowing system
 */
void UnInitWindowSystem()
{
	Window *w;
	FOR_ALL_WINDOWS_FROM_FRONT(w) delete w;

	for (w = _z_front_window; w != NULL; /* nothing */) {
		Window *to_del = w;
		w = w->z_back;
		free(to_del);
	}

	_z_front_window = NULL;
	_z_back_window = NULL;
}

/**
 * Reset the windowing system, by means of shutting it down followed by re-initialization
 */
void ResetWindowSystem()
{
	UnInitWindowSystem();
	InitWindowSystem();
	_thd.pos.x = 0;
	_thd.pos.y = 0;
	_thd.new_pos.x = 0;
	_thd.new_pos.y = 0;
}

static void DecreaseWindowCounters()
{
	Window *w;
	FOR_ALL_WINDOWS_FROM_FRONT(w) {
		if (_scroller_click_timeout == 0) {
			/* Unclick scrollbar buttons if they are pressed. */
			for (uint i = 0; i < w->nested_array_size; i++) {
				NWidgetBase *nwid = w->nested_array[i];
				if (nwid != NULL && (nwid->type == NWID_HSCROLLBAR || nwid->type == NWID_VSCROLLBAR)) {
					NWidgetScrollbar *sb = static_cast<NWidgetScrollbar*>(nwid);
					if (sb->disp_flags & (ND_SCROLLBAR_UP | ND_SCROLLBAR_DOWN)) {
						sb->disp_flags &= ~(ND_SCROLLBAR_UP | ND_SCROLLBAR_DOWN);
						sb->SetDirty(w);
					}
				}
			}
		}
		w->OnMouseLoop();
	}

	FOR_ALL_WINDOWS_FROM_FRONT(w) {
		if ((w->flags4 & WF_TIMEOUT_MASK) && !(--w->flags4 & WF_TIMEOUT_MASK)) {
			w->OnTimeout();
			if (w->desc_flags & WDF_UNCLICK_BUTTONS) w->RaiseButtons(true);
		}
	}
}

Window *GetCallbackWnd()
{
	return FindWindowById(_thd.window_class, _thd.window_number);
}

static void HandlePlacePresize()
{
	if (_special_mouse_mode != WSM_PRESIZE) return;

	Window *w = GetCallbackWnd();
	if (w == NULL) return;

	Point pt = GetTileBelowCursor();
	if (pt.x == -1) {
		_thd.selend.x = -1;
		return;
	}

	w->OnPlacePresize(pt, TileVirtXY(pt.x, pt.y));
}

/**
 * Handle drop in mouse dragging mode (#WSM_DRAGDROP).
 * @return State of handling the event.
 */
static EventState HandleDragDrop()
{
	if (_special_mouse_mode != WSM_DRAGDROP) return ES_NOT_HANDLED;
	if (_left_button_down) return ES_HANDLED;

	Window *w = GetCallbackWnd();

	if (w != NULL) {
		/* send an event in client coordinates. */
		Point pt;
		pt.x = _cursor.pos.x - w->left;
		pt.y = _cursor.pos.y - w->top;
		w->OnDragDrop(pt, GetWidgetFromPos(w, pt.x, pt.y));
	}

	ResetObjectToPlace();

	return ES_HANDLED;
}

/**
 * Handle dragging in mouse dragging mode (#WSM_DRAGDROP).
 * @return State of handling the event.
 */
static EventState HandleMouseDrag()
{
	if (_special_mouse_mode != WSM_DRAGDROP) return ES_NOT_HANDLED;
	if (!_left_button_down || (_cursor.delta.x == 0 && _cursor.delta.y == 0)) return ES_NOT_HANDLED;

	Window *w = GetCallbackWnd();

	if (w != NULL) {
		/* Send an event in client coordinates. */
		Point pt;
		pt.x = _cursor.pos.x - w->left;
		pt.y = _cursor.pos.y - w->top;
		w->OnMouseDrag(pt, GetWidgetFromPos(w, pt.x, pt.y));
	}

	return ES_HANDLED;
}

/** Report position of the mouse to the underlying window. */
static void HandleMouseOver()
{
	Window *w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);

	/* We changed window, put a MOUSEOVER event to the last window */
	if (_mouseover_last_w != NULL && _mouseover_last_w != w) {
		/* Reset mouse-over coordinates of previous window */
		Point pt = { -1, -1 };
		_mouseover_last_w->OnMouseOver(pt, 0);
	}

	/* _mouseover_last_w will get reset when the window is deleted, see DeleteWindow() */
	_mouseover_last_w = w;

	if (w != NULL) {
		/* send an event in client coordinates. */
		Point pt = { _cursor.pos.x - w->left, _cursor.pos.y - w->top };
		const NWidgetCore *widget = w->nested_root->GetWidgetFromPos(pt.x, pt.y);
		if (widget != NULL) w->OnMouseOver(pt, widget->index);
	}
}

/** The minimum number of pixels of the title bar must be visible in both the X or Y direction */
static const int MIN_VISIBLE_TITLE_BAR = 13;

/** Direction for moving the window. */
enum PreventHideDirection {
	PHD_UP,   ///< Above v is a safe position.
	PHD_DOWN, ///< Below v is a safe position.
};

/**
 * Do not allow hiding of the rectangle with base coordinates \a nx and \a ny behind window \a v.
 * If needed, move the window base coordinates to keep it visible.
 * @param nx   Base horizontal coordinate of the rectangle.
 * @param ny   Base vertical coordinate of the rectangle.
 * @param rect Rectangle that must stay visible for #MIN_VISIBLE_TITLE_BAR pixels (horizontally, vertically, or both)
 * @param v    Window lying in front of the rectangle.
 * @param px   Previous horizontal base coordinate.
 * @param dir  If no room horizontally, move the rectangle to the indicated position.
 */
static void PreventHiding(int *nx, int *ny, const Rect &rect, const Window *v, int px, PreventHideDirection dir)
{
	if (v == NULL) return;

	int v_bottom = v->top + v->height;
	int v_right = v->left + v->width;
	int safe_y = (dir == PHD_UP) ? (v->top - MIN_VISIBLE_TITLE_BAR - rect.top) : (v_bottom + MIN_VISIBLE_TITLE_BAR - rect.bottom); // Compute safe vertical position.

	if (*ny + rect.top <= v->top - MIN_VISIBLE_TITLE_BAR) return; // Above v is enough space
	if (*ny + rect.bottom >= v_bottom + MIN_VISIBLE_TITLE_BAR) return; // Below v is enough space

	/* Vertically, the rectangle is hidden behind v. */
	if (*nx + rect.left + MIN_VISIBLE_TITLE_BAR < v->left) { // At left of v.
		if (v->left < MIN_VISIBLE_TITLE_BAR) *ny = safe_y; // But enough room, force it to a safe position.
		return;
	}
	if (*nx + rect.right - MIN_VISIBLE_TITLE_BAR > v_right) { // At right of v.
		if (v_right > _screen.width - MIN_VISIBLE_TITLE_BAR) *ny = safe_y; // Not enough room, force it to a safe position.
		return;
	}

	/* Horizontally also hidden, force movement to a safe area. */
	if (px + rect.left < v->left && v->left >= MIN_VISIBLE_TITLE_BAR) { // Coming from the left, and enough room there.
		*nx = v->left - MIN_VISIBLE_TITLE_BAR - rect.left;
	} else if (px + rect.right > v_right && v_right <= _screen.width - MIN_VISIBLE_TITLE_BAR) { // Coming from the right, and enough room there.
		*nx = v_right + MIN_VISIBLE_TITLE_BAR - rect.right;
	} else {
		*ny = safe_y;
	}
}

/**
 * Make sure at least a part of the caption bar is still visible by moving
 * the window if necessary.
 * @param w The window to check.
 * @param nx The proposed new x-location of the window.
 * @param ny The proposed new y-location of the window.
 */
static void EnsureVisibleCaption(Window *w, int nx, int ny)
{
	/* Search for the title bar rectangle. */
	Rect caption_rect;
	const NWidgetBase *caption = w->nested_root->GetWidgetOfType(WWT_CAPTION);
	if (caption != NULL) {
		caption_rect.left   = caption->pos_x;
		caption_rect.right  = caption->pos_x + caption->current_x;
		caption_rect.top    = caption->pos_y;
		caption_rect.bottom = caption->pos_y + caption->current_y;

		/* Make sure the window doesn't leave the screen */
		nx = Clamp(nx, MIN_VISIBLE_TITLE_BAR - caption_rect.right, _screen.width - MIN_VISIBLE_TITLE_BAR - caption_rect.left);
		ny = Clamp(ny, 0, _screen.height - MIN_VISIBLE_TITLE_BAR);

		/* Make sure the title bar isn't hidden behind the main tool bar or the status bar. */
		PreventHiding(&nx, &ny, caption_rect, FindWindowById(WC_MAIN_TOOLBAR, 0), w->left, PHD_DOWN);
		PreventHiding(&nx, &ny, caption_rect, FindWindowById(WC_STATUS_BAR,   0), w->left, PHD_UP);

		if (w->viewport != NULL) {
			w->viewport->left += nx - w->left;
			w->viewport->top  += ny - w->top;
		}
	}
	w->left = nx;
	w->top  = ny;
}

/**
 * Resize the window.
 * Update all the widgets of a window based on their resize flags
 * Both the areas of the old window and the new sized window are set dirty
 * ensuring proper redrawal.
 * @param w       Window to resize
 * @param delta_x Delta x-size of changed window (positive if larger, etc.)
 * @param delta_y Delta y-size of changed window
 */
void ResizeWindow(Window *w, int delta_x, int delta_y)
{
	if (delta_x != 0 || delta_y != 0) {
		w->SetDirty();

		uint new_xinc = max(0, (w->nested_root->resize_x == 0) ? 0 : (int)(w->nested_root->current_x - w->nested_root->smallest_x) + delta_x);
		uint new_yinc = max(0, (w->nested_root->resize_y == 0) ? 0 : (int)(w->nested_root->current_y - w->nested_root->smallest_y) + delta_y);
		assert(w->nested_root->resize_x == 0 || new_xinc % w->nested_root->resize_x == 0);
		assert(w->nested_root->resize_y == 0 || new_yinc % w->nested_root->resize_y == 0);

		w->nested_root->AssignSizePosition(ST_RESIZE, 0, 0, w->nested_root->smallest_x + new_xinc, w->nested_root->smallest_y + new_yinc, _current_text_dir == TD_RTL);
		w->width  = w->nested_root->current_x;
		w->height = w->nested_root->current_y;
	}

	EnsureVisibleCaption(w, w->left, w->top);

	/* Always call OnResize to make sure everything is initialised correctly if it needs to be. */
	w->OnResize();
	w->SetDirty();
}

/**
 * Return the top of the main view available for general use.
 * @return Uppermost vertical coordinate available.
 * @note Above the upper y coordinate is often the main toolbar.
 */
int GetMainViewTop()
{
	Window *w = FindWindowById(WC_MAIN_TOOLBAR, 0);
	return (w == NULL) ? 0 : w->top + w->height;
}

/**
 * Return the bottom of the main view available for general use.
 * @return The vertical coordinate of the first unusable row, so 'top + height <= bottom' gives the correct result.
 * @note At and below the bottom y coordinate is often the status bar.
 */
int GetMainViewBottom()
{
	Window *w = FindWindowById(WC_STATUS_BAR, 0);
	return (w == NULL) ? _screen.height : w->top;
}

static bool _dragging_window; ///< A window is being dragged or resized.

/**
 * Handle dragging/resizing of a window.
 * @return State of handling the event.
 */
static EventState HandleWindowDragging()
{
	/* Get out immediately if no window is being dragged at all. */
	if (!_dragging_window) return ES_NOT_HANDLED;

	/* If button still down, but cursor hasn't moved, there is nothing to do. */
	if (_left_button_down && _cursor.delta.x == 0 && _cursor.delta.y == 0) return ES_HANDLED;

	/* Otherwise find the window... */
	Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->flags4 & WF_DRAGGING) {
			/* Stop the dragging if the left mouse button was released */
			if (!_left_button_down) {
				w->flags4 &= ~WF_DRAGGING;
				break;
			}

			w->SetDirty();

			int x = _cursor.pos.x + _drag_delta.x;
			int y = _cursor.pos.y + _drag_delta.y;
			int nx = x;
			int ny = y;

			if (_settings_client.gui.window_snap_radius != 0) {
				const Window *v;

				int hsnap = _settings_client.gui.window_snap_radius;
				int vsnap = _settings_client.gui.window_snap_radius;
				int delta;

				FOR_ALL_WINDOWS_FROM_BACK(v) {
					if (v == w) continue; // Don't snap at yourself

					if (y + w->height > v->top && y < v->top + v->height) {
						/* Your left border <-> other right border */
						delta = abs(v->left + v->width - x);
						if (delta <= hsnap) {
							nx = v->left + v->width;
							hsnap = delta;
						}

						/* Your right border <-> other left border */
						delta = abs(v->left - x - w->width);
						if (delta <= hsnap) {
							nx = v->left - w->width;
							hsnap = delta;
						}
					}

					if (w->top + w->height >= v->top && w->top <= v->top + v->height) {
						/* Your left border <-> other left border */
						delta = abs(v->left - x);
						if (delta <= hsnap) {
							nx = v->left;
							hsnap = delta;
						}

						/* Your right border <-> other right border */
						delta = abs(v->left + v->width - x - w->width);
						if (delta <= hsnap) {
							nx = v->left + v->width - w->width;
							hsnap = delta;
						}
					}

					if (x + w->width > v->left && x < v->left + v->width) {
						/* Your top border <-> other bottom border */
						delta = abs(v->top + v->height - y);
						if (delta <= vsnap) {
							ny = v->top + v->height;
							vsnap = delta;
						}

						/* Your bottom border <-> other top border */
						delta = abs(v->top - y - w->height);
						if (delta <= vsnap) {
							ny = v->top - w->height;
							vsnap = delta;
						}
					}

					if (w->left + w->width >= v->left && w->left <= v->left + v->width) {
						/* Your top border <-> other top border */
						delta = abs(v->top - y);
						if (delta <= vsnap) {
							ny = v->top;
							vsnap = delta;
						}

						/* Your bottom border <-> other bottom border */
						delta = abs(v->top + v->height - y - w->height);
						if (delta <= vsnap) {
							ny = v->top + v->height - w->height;
							vsnap = delta;
						}
					}
				}
			}

			EnsureVisibleCaption(w, nx, ny);

			w->SetDirty();
			return ES_HANDLED;
		} else if (w->flags4 & WF_SIZING) {
			/* Stop the sizing if the left mouse button was released */
			if (!_left_button_down) {
				w->flags4 &= ~WF_SIZING;
				w->SetDirty();
				break;
			}

			/* Compute difference in pixels between cursor position and reference point in the window.
			 * If resizing the left edge of the window, moving to the left makes the window bigger not smaller.
			 */
			int x, y = _cursor.pos.y - _drag_delta.y;
			if (w->flags4 & WF_SIZING_LEFT) {
				x = _drag_delta.x - _cursor.pos.x;
			} else {
				x = _cursor.pos.x - _drag_delta.x;
			}

			/* resize.step_width and/or resize.step_height may be 0, which means no resize is possible. */
			if (w->resize.step_width  == 0) x = 0;
			if (w->resize.step_height == 0) y = 0;

			/* Check the resize button won't go past the bottom of the screen */
			if (w->top + w->height + y > _screen.height) {
				y = _screen.height - w->height - w->top;
			}

			/* X and Y has to go by step.. calculate it.
			 * The cast to int is necessary else x/y are implicitly casted to
			 * unsigned int, which won't work. */
			if (w->resize.step_width  > 1) x -= x % (int)w->resize.step_width;
			if (w->resize.step_height > 1) y -= y % (int)w->resize.step_height;

			/* Check that we don't go below the minimum set size */
			if ((int)w->width + x < (int)w->nested_root->smallest_x) {
				x = w->nested_root->smallest_x - w->width;
			}
			if ((int)w->height + y < (int)w->nested_root->smallest_y) {
				y = w->nested_root->smallest_y - w->height;
			}

			/* Window already on size */
			if (x == 0 && y == 0) return ES_HANDLED;

			/* Now find the new cursor pos.. this is NOT _cursor, because we move in steps. */
			_drag_delta.y += y;
			if ((w->flags4 & WF_SIZING_LEFT) && x != 0) {
				_drag_delta.x -= x; // x > 0 -> window gets longer -> left-edge moves to left -> subtract x to get new position.
				w->SetDirty();
				w->left -= x;  // If dragging left edge, move left window edge in opposite direction by the same amount.
				/* ResizeWindow() below ensures marking new position as dirty. */
			} else {
				_drag_delta.x += x;
			}

			/* ResizeWindow sets both pre- and after-size to dirty for redrawal */
			ResizeWindow(w, x, y);
			return ES_HANDLED;
		}
	}

	_dragging_window = false;
	return ES_HANDLED;
}

/**
 * Start window dragging
 * @param w Window to start dragging
 */
static void StartWindowDrag(Window *w)
{
	w->flags4 |= WF_DRAGGING;
	w->flags4 &= ~WF_CENTERED;
	_dragging_window = true;

	_drag_delta.x = w->left - _cursor.pos.x;
	_drag_delta.y = w->top  - _cursor.pos.y;

	BringWindowToFront(w);
	DeleteWindowById(WC_DROPDOWN_MENU, 0);
}

/**
 * Start resizing a window.
 * @param w       Window to start resizing.
 * @param to_left Whether to drag towards the left or not
 */
static void StartWindowSizing(Window *w, bool to_left)
{
	w->flags4 |= to_left ? WF_SIZING_LEFT : WF_SIZING_RIGHT;
	w->flags4 &= ~WF_CENTERED;
	_dragging_window = true;

	_drag_delta.x = _cursor.pos.x;
	_drag_delta.y = _cursor.pos.y;

	BringWindowToFront(w);
	DeleteWindowById(WC_DROPDOWN_MENU, 0);
}

/**
 * handle scrollbar scrolling with the mouse.
 * @return State of handling the event.
 */
static EventState HandleScrollbarScrolling()
{
	Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->scrolling_scrollbar >= 0) {
			/* Abort if no button is clicked any more. */
			if (!_left_button_down) {
				w->scrolling_scrollbar = -1;
				w->SetDirty();
				return ES_HANDLED;
			}

			int i;
			NWidgetScrollbar *sb = w->GetWidget<NWidgetScrollbar>(w->scrolling_scrollbar);
			bool rtl = false;

			if (sb->type == NWID_HSCROLLBAR) {
				i = _cursor.pos.x - _cursorpos_drag_start.x;
				rtl = _current_text_dir == TD_RTL;
			} else {
				i = _cursor.pos.y - _cursorpos_drag_start.y;
			}

			if (sb->disp_flags & ND_SCROLLBAR_BTN) {
				if (_scroller_click_timeout == 1) {
					_scroller_click_timeout = 3;
					sb->UpdatePosition(rtl == HasBit(sb->disp_flags, NDB_SCROLLBAR_UP) ? 1 : -1);
					w->SetDirty();
				}
				return ES_HANDLED;
			}

			/* Find the item we want to move to and make sure it's inside bounds. */
			int pos = min(max(0, i + _scrollbar_start_pos) * sb->GetCount() / _scrollbar_size, max(0, sb->GetCount() - sb->GetCapacity()));
			if (rtl) pos = max(0, sb->GetCount() - sb->GetCapacity() - pos);
			if (pos != sb->GetPosition()) {
				sb->SetPosition(pos);
				w->SetDirty();
			}
			return ES_HANDLED;
		}
	}

	return ES_NOT_HANDLED;
}

/**
 * Handle viewport scrolling with the mouse.
 * @return State of handling the event.
 */
static EventState HandleViewportScroll()
{
	bool scrollwheel_scrolling = _settings_client.gui.scrollwheel_scrolling == 1 && (_cursor.v_wheel != 0 || _cursor.h_wheel != 0);

	if (!_scrolling_viewport) return ES_NOT_HANDLED;

	Window *w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);

	if (!(_right_button_down || scrollwheel_scrolling || (_settings_client.gui.left_mouse_btn_scrolling && _left_button_down)) || w == NULL) {
		_cursor.fix_at = false;
		_scrolling_viewport = false;
		return ES_NOT_HANDLED;
	}

	if (w == FindWindowById(WC_MAIN_WINDOW, 0) && w->viewport->follow_vehicle != INVALID_VEHICLE) {
		/* If the main window is following a vehicle, then first let go of it! */
		const Vehicle *veh = Vehicle::Get(w->viewport->follow_vehicle);
		ScrollMainWindowTo(veh->x_pos, veh->y_pos, veh->z_pos, true); // This also resets follow_vehicle
		return ES_NOT_HANDLED;
	}

	Point delta;
	if (_settings_client.gui.reverse_scroll || (_settings_client.gui.left_mouse_btn_scrolling && _left_button_down)) {
		delta.x = -_cursor.delta.x;
		delta.y = -_cursor.delta.y;
	} else {
		delta.x = _cursor.delta.x;
		delta.y = _cursor.delta.y;
	}

	if (scrollwheel_scrolling) {
		/* We are using scrollwheels for scrolling */
		delta.x = _cursor.h_wheel;
		delta.y = _cursor.v_wheel;
		_cursor.v_wheel = 0;
		_cursor.h_wheel = 0;
	}

	/* Create a scroll-event and send it to the window */
	if (delta.x != 0 || delta.y != 0) w->OnScroll(delta);

	_cursor.delta.x = 0;
	_cursor.delta.y = 0;
	return ES_HANDLED;
}

/**
 * Check if a window can be made top-most window, and if so do
 * it. If a window does not obscure any other windows, it will not
 * be brought to the foreground. Also if the only obscuring windows
 * are so-called system-windows, the window will not be moved.
 * The function will return false when a child window of this window is a
 * modal-popup; function returns a false and child window gets a white border
 * @param w Window to bring on-top
 * @return false if the window has an active modal child, true otherwise
 */
static bool MaybeBringWindowToFront(Window *w)
{
	bool bring_to_front = false;

	if (w->window_class == WC_MAIN_WINDOW ||
			IsVitalWindow(w) ||
			w->window_class == WC_TOOLTIPS ||
			w->window_class == WC_DROPDOWN_MENU) {
		return true;
	}

	/* Use unshaded window size rather than current size for shaded windows. */
	int w_width  = w->width;
	int w_height = w->height;
	if (w->IsShaded()) {
		w_width  = w->unshaded_size.width;
		w_height = w->unshaded_size.height;
	}

	Window *u;
	FOR_ALL_WINDOWS_FROM_BACK_FROM(u, w->z_front) {
		/* A modal child will prevent the activation of the parent window */
		if (u->parent == w && (u->desc_flags & WDF_MODAL)) {
			u->flags4 |= WF_WHITE_BORDER_MASK;
			u->SetDirty();
			return false;
		}

		if (u->window_class == WC_MAIN_WINDOW ||
				IsVitalWindow(u) ||
				u->window_class == WC_TOOLTIPS ||
				u->window_class == WC_DROPDOWN_MENU) {
			continue;
		}

		/* Window sizes don't interfere, leave z-order alone */
		if (w->left + w_width <= u->left ||
				u->left + u->width <= w->left ||
				w->top  + w_height <= u->top ||
				u->top + u->height <= w->top) {
			continue;
		}

		bring_to_front = true;
	}

	if (bring_to_front) BringWindowToFront(w);
	return true;
}

/**
 * Handle keyboard input.
 * @param raw_key Lower 8 bits contain the ASCII character, the higher 16 bits the keycode
 */
void HandleKeypress(uint32 raw_key)
{
	/* World generation is multithreaded and messes with companies.
	 * But there is no company related window open anyway, so _current_company is not used. */
	assert(IsGeneratingWorld() || IsLocalCompany());

	/* Setup event */
	uint16 key     = GB(raw_key,  0, 16);
	uint16 keycode = GB(raw_key, 16, 16);

	/*
	 * The Unicode standard defines an area called the private use area. Code points in this
	 * area are reserved for private use and thus not portable between systems. For instance,
	 * Apple defines code points for the arrow keys in this area, but these are only printable
	 * on a system running OS X. We don't want these keys to show up in text fields and such,
	 * and thus we have to clear the unicode character when we encounter such a key.
	 */
	if (key >= 0xE000 && key <= 0xF8FF) key = 0;

	/*
	 * If both key and keycode is zero, we don't bother to process the event.
	 */
	if (key == 0 && keycode == 0) return;

	/* Check if the focused window has a focused editbox */
	if (EditBoxInGlobalFocus()) {
		/* All input will in this case go to the focused window */
		if (_focused_window->OnKeyPress(key, keycode) == ES_HANDLED) return;
	}

	/* Call the event, start with the uppermost window, but ignore the toolbar. */
	Window *w;
	FOR_ALL_WINDOWS_FROM_FRONT(w) {
		if (w->window_class == WC_MAIN_TOOLBAR) continue;
		if (w->OnKeyPress(key, keycode) == ES_HANDLED) return;
	}

	w = FindWindowById(WC_MAIN_TOOLBAR, 0);
	/* When there is no toolbar w is null, check for that */
	if (w != NULL && w->OnKeyPress(key, keycode) == ES_HANDLED) return;

	HandleGlobalHotkeys(key, keycode);
}

/**
 * State of CONTROL key has changed
 */
void HandleCtrlChanged()
{
	/* Call the event, start with the uppermost window. */
	Window *w;
	FOR_ALL_WINDOWS_FROM_FRONT(w) {
		if (w->OnCTRLStateChange() == ES_HANDLED) return;
	}
}

/**
 * Local counter that is incremented each time an mouse input event is detected.
 * The counter is used to stop auto-scrolling.
 * @see HandleAutoscroll()
 * @see HandleMouseEvents()
 */
static int _input_events_this_tick = 0;

/**
 * If needed and switched on, perform auto scrolling (automatically
 * moving window contents when mouse is near edge of the window).
 */
static void HandleAutoscroll()
{
	if (_settings_client.gui.autoscroll && _game_mode != GM_MENU && !IsGeneratingWorld()) {
		int x = _cursor.pos.x;
		int y = _cursor.pos.y;
		Window *w = FindWindowFromPt(x, y);
		if (w == NULL || w->flags4 & WF_DISABLE_VP_SCROLL) return;
		ViewPort *vp = IsPtInWindowViewport(w, x, y);
		if (vp != NULL) {
			x -= vp->left;
			y -= vp->top;

			/* here allows scrolling in both x and y axis */
#define scrollspeed 3
			if (x - 15 < 0) {
				w->viewport->dest_scrollpos_x += ScaleByZoom((x - 15) * scrollspeed, vp->zoom);
			} else if (15 - (vp->width - x) > 0) {
				w->viewport->dest_scrollpos_x += ScaleByZoom((15 - (vp->width - x)) * scrollspeed, vp->zoom);
			}
			if (y - 15 < 0) {
				w->viewport->dest_scrollpos_y += ScaleByZoom((y - 15) * scrollspeed, vp->zoom);
			} else if (15 - (vp->height - y) > 0) {
				w->viewport->dest_scrollpos_y += ScaleByZoom((15 - (vp->height - y)) * scrollspeed, vp->zoom);
			}
#undef scrollspeed
		}
	}
}

enum MouseClick {
	MC_NONE = 0,
	MC_LEFT,
	MC_RIGHT,
	MC_DOUBLE_LEFT,
	MC_HOVER,

	MAX_OFFSET_DOUBLE_CLICK = 5,     ///< How much the mouse is allowed to move to call it a double click
	TIME_BETWEEN_DOUBLE_CLICK = 500, ///< Time between 2 left clicks before it becoming a double click, in ms
	MAX_OFFSET_HOVER = 5,            ///< Maximum mouse movement before stopping a hover event.
};
extern EventState VpHandlePlaceSizingDrag();

static void ScrollMainViewport(int x, int y)
{
	if (_game_mode != GM_MENU) {
		Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
		assert(w);

		w->viewport->dest_scrollpos_x += ScaleByZoom(x, w->viewport->zoom);
		w->viewport->dest_scrollpos_y += ScaleByZoom(y, w->viewport->zoom);
	}
}

/**
 * Describes all the different arrow key combinations the game allows
 * when it is in scrolling mode.
 * The real arrow keys are bitwise numbered as
 * 1 = left
 * 2 = up
 * 4 = right
 * 8 = down
 */
static const int8 scrollamt[16][2] = {
	{ 0,  0}, ///<  no key specified
	{-2,  0}, ///<  1 : left
	{ 0, -2}, ///<  2 : up
	{-2, -1}, ///<  3 : left  + up
	{ 2,  0}, ///<  4 : right
	{ 0,  0}, ///<  5 : left  + right = nothing
	{ 2, -1}, ///<  6 : right + up
	{ 0, -2}, ///<  7 : right + left  + up = up
	{ 0,  2}, ///<  8 : down
	{-2,  1}, ///<  9 : down  + left
	{ 0,  0}, ///< 10 : down  + up    = nothing
	{-2,  0}, ///< 11 : left  + up    +  down = left
	{ 2,  1}, ///< 12 : down  + right
	{ 0,  2}, ///< 13 : left  + right +  down = down
	{ 2,  0}, ///< 14 : right + up    +  down = right
	{ 0,  0}, ///< 15 : left  + up    +  right + down  = nothing
};

static void HandleKeyScrolling()
{
	/*
	 * Check that any of the dirkeys is pressed and that the focused window
	 * dont has an edit-box as focused widget.
	 */
	if (_dirkeys && !EditBoxInGlobalFocus()) {
		int factor = _shift_pressed ? 50 : 10;
		ScrollMainViewport(scrollamt[_dirkeys][0] * factor, scrollamt[_dirkeys][1] * factor);
	}
}

static void MouseLoop(MouseClick click, int mousewheel)
{
	/* World generation is multithreaded and messes with companies.
	 * But there is no company related window open anyway, so _current_company is not used. */
	assert(IsGeneratingWorld() || IsLocalCompany());

	HandlePlacePresize();
	UpdateTileSelection();

	if (VpHandlePlaceSizingDrag()  == ES_HANDLED) return;
	if (HandleMouseDrag()          == ES_HANDLED) return;
	if (HandleDragDrop()           == ES_HANDLED) return;
	if (HandleWindowDragging()     == ES_HANDLED) return;
	if (HandleScrollbarScrolling() == ES_HANDLED) return;
	if (HandleViewportScroll()     == ES_HANDLED) return;

	HandleMouseOver();

	bool scrollwheel_scrolling = _settings_client.gui.scrollwheel_scrolling == 1 && (_cursor.v_wheel != 0 || _cursor.h_wheel != 0);
	if (click == MC_NONE && mousewheel == 0 && !scrollwheel_scrolling) return;

	int x = _cursor.pos.x;
	int y = _cursor.pos.y;
	Window *w = FindWindowFromPt(x, y);
	if (w == NULL) return;

	if (click != MC_HOVER && !MaybeBringWindowToFront(w)) return;
	ViewPort *vp = IsPtInWindowViewport(w, x, y);

	/* Don't allow any action in a viewport if either in menu of in generating world */
	if (vp != NULL && (_game_mode == GM_MENU || IsGeneratingWorld())) return;

	if (mousewheel != 0) {
		if (_settings_client.gui.scrollwheel_scrolling == 0) {
			/* Send mousewheel event to window */
			w->OnMouseWheel(mousewheel);
		}

		/* Dispatch a MouseWheelEvent for widgets if it is not a viewport */
		if (vp == NULL) DispatchMouseWheelEvent(w, w->nested_root->GetWidgetFromPos(x - w->left, y - w->top), mousewheel);
	}

	if (vp != NULL) {
		if (scrollwheel_scrolling) click = MC_RIGHT; // we are using the scrollwheel in a viewport, so we emulate right mouse button
		switch (click) {
			case MC_DOUBLE_LEFT:
			case MC_LEFT:
				DEBUG(misc, 2, "Cursor: 0x%X (%d)", _cursor.sprite, _cursor.sprite);
				if (!HandleViewportClicked(vp, x, y) &&
						!(w->flags4 & WF_DISABLE_VP_SCROLL) &&
						_settings_client.gui.left_mouse_btn_scrolling) {
					_scrolling_viewport = true;
					_cursor.fix_at = false;
				}
				break;

			case MC_RIGHT:
				if (!(w->flags4 & WF_DISABLE_VP_SCROLL)) {
					_scrolling_viewport = true;
					_cursor.fix_at = true;

					/* clear 2D scrolling caches before we start a 2D scroll */
					_cursor.h_wheel = 0;
					_cursor.v_wheel = 0;
				}
				break;

			default:
				break;
		}
	} else {
		switch (click) {
			case MC_LEFT:
			case MC_DOUBLE_LEFT:
				DispatchLeftClickEvent(w, x - w->left, y - w->top, click == MC_DOUBLE_LEFT ? 2 : 1);
				break;

			default:
				if (!scrollwheel_scrolling || w == NULL || w->window_class != WC_SMALLMAP) break;
				/* We try to use the scrollwheel to scroll since we didn't touch any of the buttons.
				 * Simulate a right button click so we can get started. */
				/* FALL THROUGH */

			case MC_RIGHT: DispatchRightClickEvent(w, x - w->left, y - w->top); break;

			case MC_HOVER: DispatchHoverEvent(w, x - w->left, y - w->top); break;
		}
	}
}

/**
 * Handle a mouse event from the video driver
 */
void HandleMouseEvents()
{
	/* World generation is multithreaded and messes with companies.
	 * But there is no company related window open anyway, so _current_company is not used. */
	assert(IsGeneratingWorld() || IsLocalCompany());

	static int double_click_time = 0;
	static Point double_click_pos = {0, 0};

	/* Mouse event? */
	MouseClick click = MC_NONE;
	if (_left_button_down && !_left_button_clicked) {
		click = MC_LEFT;
		if (double_click_time != 0 && _realtime_tick - double_click_time   < TIME_BETWEEN_DOUBLE_CLICK &&
				double_click_pos.x != 0 && abs(_cursor.pos.x - double_click_pos.x) < MAX_OFFSET_DOUBLE_CLICK  &&
				double_click_pos.y != 0 && abs(_cursor.pos.y - double_click_pos.y) < MAX_OFFSET_DOUBLE_CLICK) {
			click = MC_DOUBLE_LEFT;
		}
		double_click_time = _realtime_tick;
		double_click_pos = _cursor.pos;
		_left_button_clicked = true;
		_input_events_this_tick++;
	} else if (_right_button_clicked) {
		_right_button_clicked = false;
		click = MC_RIGHT;
		_input_events_this_tick++;
	}

	int mousewheel = 0;
	if (_cursor.wheel) {
		mousewheel = _cursor.wheel;
		_cursor.wheel = 0;
		_input_events_this_tick++;
	}

	static uint32 hover_time = 0;
	static Point hover_pos = {0, 0};

	if (_settings_client.gui.hover_delay > 0) {
		if (!_cursor.in_window || click != MC_NONE || mousewheel != 0 || _left_button_down || _right_button_down ||
				hover_pos.x == 0 || abs(_cursor.pos.x - hover_pos.x) >= MAX_OFFSET_HOVER  ||
				hover_pos.y == 0 || abs(_cursor.pos.y - hover_pos.y) >= MAX_OFFSET_HOVER) {
			hover_pos = _cursor.pos;
			hover_time = _realtime_tick;
			_mouse_hovering = false;
		} else {
			if (hover_time != 0 && _realtime_tick > hover_time + _settings_client.gui.hover_delay * 1000) {
				click = MC_HOVER;
				_input_events_this_tick++;
				_mouse_hovering = true;
			}
		}
	}

	/* Handle sprite picker before any GUI interaction */
	if (_newgrf_debug_sprite_picker.mode == SPM_REDRAW && _newgrf_debug_sprite_picker.click_time != _realtime_tick) {
		/* Next realtime tick? Then redraw has finished */
		_newgrf_debug_sprite_picker.mode = SPM_NONE;
		InvalidateWindowData(WC_SPRITE_ALIGNER, 0, 1);
	}

	if (click == MC_LEFT && _newgrf_debug_sprite_picker.mode == SPM_WAIT_CLICK) {
		/* Mark whole screen dirty, and wait for the next realtime tick, when drawing is finished. */
		Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
		_newgrf_debug_sprite_picker.clicked_pixel = blitter->MoveTo(_screen.dst_ptr, _cursor.pos.x, _cursor.pos.y);
		_newgrf_debug_sprite_picker.click_time = _realtime_tick;
		_newgrf_debug_sprite_picker.sprites.Clear();
		_newgrf_debug_sprite_picker.mode = SPM_REDRAW;
		MarkWholeScreenDirty();
	} else {
		MouseLoop(click, mousewheel);
	}

	/* We have moved the mouse the required distance,
	 * no need to move it at any later time. */
	_cursor.delta.x = 0;
	_cursor.delta.y = 0;
}

/**
 * Check the soft limit of deletable (non vital, non sticky) windows.
 */
static void CheckSoftLimit()
{
	if (_settings_client.gui.window_soft_limit == 0) return;

	for (;;) {
		uint deletable_count = 0;
		Window *w, *last_deletable = NULL;
		FOR_ALL_WINDOWS_FROM_FRONT(w) {
			if (w->window_class == WC_MAIN_WINDOW || IsVitalWindow(w) || (w->flags4 & WF_STICKY)) continue;

			last_deletable = w;
			deletable_count++;
		}

		/* We've ot reached the soft limit yet */
		if (deletable_count <= _settings_client.gui.window_soft_limit) break;

		assert(last_deletable != NULL);
		delete last_deletable;
	}
}

/**
 * Regular call from the global game loop
 */
void InputLoop()
{
	/* World generation is multithreaded and messes with companies.
	 * But there is no company related window open anyway, so _current_company is not used. */
	assert(IsGeneratingWorld() || IsLocalCompany());

	CheckSoftLimit();
	HandleKeyScrolling();

	/* Do the actual free of the deleted windows. */
	for (Window *v = _z_front_window; v != NULL; /* nothing */) {
		Window *w = v;
		v = v->z_back;

		if (w->window_class != WC_INVALID) continue;

		/* Find the window in the z-array, and effectively remove it
		 * by moving all windows after it one to the left. This must be
		 * done before removing the child so we cannot cause recursion
		 * between the deletion of the parent and the child. */
		if (w->z_front == NULL) {
			_z_front_window = w->z_back;
		} else {
			w->z_front->z_back = w->z_back;
		}
		if (w->z_back == NULL) {
			_z_back_window  = w->z_front;
		} else {
			w->z_back->z_front = w->z_front;
		}
		free(w);
	}

	if (_scroller_click_timeout != 0) _scroller_click_timeout--;
	DecreaseWindowCounters();

	if (_input_events_this_tick != 0) {
		/* The input loop is called only once per GameLoop() - so we can clear the counter here */
		_input_events_this_tick = 0;
		/* there were some inputs this tick, don't scroll ??? */
		return;
	}

	/* HandleMouseEvents was already called for this tick */
	HandleMouseEvents();
	HandleAutoscroll();
}

/**
 * Update the continuously changing contents of the windows, such as the viewports
 */
void UpdateWindows()
{
	Window *w;
	static int we4_timer = 0;
	int t = we4_timer + 1;

	if (t >= 100) {
		FOR_ALL_WINDOWS_FROM_FRONT(w) {
			w->OnHundredthTick();
		}
		t = 0;
	}
	we4_timer = t;

	FOR_ALL_WINDOWS_FROM_FRONT(w) {
		if (w->flags4 & WF_WHITE_BORDER_MASK) {
			w->flags4 -= WF_WHITE_BORDER_ONE;

			if (!(w->flags4 & WF_WHITE_BORDER_MASK)) w->SetDirty();
		}
	}

	DrawDirtyBlocks();

	FOR_ALL_WINDOWS_FROM_BACK(w) {
		/* Update viewport only if window is not shaded. */
		if (w->viewport != NULL && !w->IsShaded()) UpdateViewportPosition(w);
	}
	NetworkDrawChatMessage();
	/* Redraw mouse cursor in case it was hidden */
	DrawMouseCursor();
}

/**
 * Mark window as dirty (in need of repainting)
 * @param cls Window class
 * @param number Window number in that class
 */
void SetWindowDirty(WindowClass cls, WindowNumber number)
{
	const Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class == cls && w->window_number == number) w->SetDirty();
	}
}

/**
 * Mark a particular widget in a particular window as dirty (in need of repainting)
 * @param cls Window class
 * @param number Window number in that class
 * @param widget_index Index number of the widget that needs repainting
 */
void SetWindowWidgetDirty(WindowClass cls, WindowNumber number, byte widget_index)
{
	const Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class == cls && w->window_number == number) {
			w->SetWidgetDirty(widget_index);
		}
	}
}

/**
 * Mark all windows of a particular class as dirty (in need of repainting)
 * @param cls Window class
 */
void SetWindowClassesDirty(WindowClass cls)
{
	Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class == cls) w->SetDirty();
	}
}

/**
 * Mark window data of the window of a given class and specific window number as invalid (in need of re-computing)
 * @param cls Window class
 * @param number Window number within the class
 * @param data The data to invalidate with
 */
void InvalidateWindowData(WindowClass cls, WindowNumber number, int data)
{
	Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class == cls && w->window_number == number) w->InvalidateData(data);
	}
}

/**
 * Mark window data of all windows of a given class as invalid (in need of re-computing)
 * @param cls Window class
 * @param data The data to invalidate with
 */
void InvalidateWindowClassesData(WindowClass cls, int data)
{
	Window *w;

	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class == cls) w->InvalidateData(data);
	}
}

/**
 * Dispatch WE_TICK event over all windows
 */
void CallWindowTickEvent()
{
	Window *w;
	FOR_ALL_WINDOWS_FROM_FRONT(w) {
		w->OnTick();
	}
}

/**
 * Try to delete a non-vital window.
 * Non-vital windows are windows other than the game selection, main toolbar,
 * status bar, toolbar menu, and tooltip windows. Stickied windows are also
 * considered vital.
 */
void DeleteNonVitalWindows()
{
	Window *w;

restart_search:
	/* When we find the window to delete, we need to restart the search
	 * as deleting this window could cascade in deleting (many) others
	 * anywhere in the z-array */
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class != WC_MAIN_WINDOW &&
				w->window_class != WC_SELECT_GAME &&
				w->window_class != WC_MAIN_TOOLBAR &&
				w->window_class != WC_STATUS_BAR &&
				w->window_class != WC_TOOLBAR_MENU &&
				w->window_class != WC_TOOLTIPS &&
				(w->flags4 & WF_STICKY) == 0) { // do not delete windows which are 'pinned'

			delete w;
			goto restart_search;
		}
	}
}

/**
 * It is possible that a stickied window gets to a position where the
 * 'close' button is outside the gaming area. You cannot close it then; except
 * with this function. It closes all windows calling the standard function,
 * then, does a little hacked loop of closing all stickied windows. Note
 * that standard windows (status bar, etc.) are not stickied, so these aren't affected
 */
void DeleteAllNonVitalWindows()
{
	Window *w;

	/* Delete every window except for stickied ones, then sticky ones as well */
	DeleteNonVitalWindows();

restart_search:
	/* When we find the window to delete, we need to restart the search
	 * as deleting this window could cascade in deleting (many) others
	 * anywhere in the z-array */
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->flags4 & WF_STICKY) {
			delete w;
			goto restart_search;
		}
	}
}

/**
 * Delete all windows that are used for construction of vehicle etc.
 * Once done with that invalidate the others to ensure they get refreshed too.
 */
void DeleteConstructionWindows()
{
	Window *w;

restart_search:
	/* When we find the window to delete, we need to restart the search
	 * as deleting this window could cascade in deleting (many) others
	 * anywhere in the z-array */
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->desc_flags & WDF_CONSTRUCTION) {
			delete w;
			goto restart_search;
		}
	}

	FOR_ALL_WINDOWS_FROM_BACK(w) w->SetDirty();
}

/** Delete all always on-top windows to get an empty screen */
void HideVitalWindows()
{
	DeleteWindowById(WC_TOOLBAR_MENU, 0);
	DeleteWindowById(WC_MAIN_TOOLBAR, 0);
	DeleteWindowById(WC_STATUS_BAR, 0);
}

/** Re-initialize all windows. */
void ReInitAllWindows()
{
	NWidgetLeaf::InvalidateDimensionCache(); // Reset cached sizes of several widgets.

	Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		w->ReInit();
	}
#ifdef ENABLE_NETWORK
	void NetworkReInitChatBoxSize();
	NetworkReInitChatBoxSize();
#endif

	/* Make sure essential parts of all windows are visible */
	RelocateAllWindows(_cur_resolution.width, _cur_resolution.height);
	MarkWholeScreenDirty();
}

/**
 * (Re)position a window at the screen.
 * @param w       Window structure of the window, may also be \c NULL.
 * @param clss    The class of the window to position.
 * @param setting The actual setting used for the window's position.
 * @return X coordinate of left edge of the repositioned window.
 */
static int PositionWindow(Window *w, WindowClass clss, int setting)
{
	if (w == NULL || w->window_class != clss) {
		w = FindWindowById(clss, 0);
	}
	if (w == NULL) return 0;

	int old_left = w->left;
	switch (setting) {
		case 1:  w->left = (_screen.width - w->width) / 2; break;
		case 2:  w->left = _screen.width - w->width; break;
		default: w->left = 0; break;
	}
	if (w->viewport != NULL) w->viewport->left += w->left - old_left;
	SetDirtyBlocks(0, w->top, _screen.width, w->top + w->height); // invalidate the whole row
	return w->left;
}

/**
 * (Re)position main toolbar window at the screen.
 * @param w Window structure of the main toolbar window, may also be \c NULL.
 * @return X coordinate of left edge of the repositioned toolbar window.
 */
int PositionMainToolbar(Window *w)
{
	DEBUG(misc, 5, "Repositioning Main Toolbar...");
	return PositionWindow(w, WC_MAIN_TOOLBAR, _settings_client.gui.toolbar_pos);
}

/**
 * (Re)position statusbar window at the screen.
 * @param w Window structure of the statusbar window, may also be \c NULL.
 * @return X coordinate of left edge of the repositioned statusbar.
 */
int PositionStatusbar(Window *w)
{
	DEBUG(misc, 5, "Repositioning statusbar...");
	return PositionWindow(w, WC_STATUS_BAR, _settings_client.gui.statusbar_pos);
}

/**
 * (Re)position news message window at the screen.
 * @param w Window structure of the news message window, may also be \c NULL.
 * @return X coordinate of left edge of the repositioned news message.
 */
int PositionNewsMessage(Window *w)
{
	DEBUG(misc, 5, "Repositioning news message...");
	return PositionWindow(w, WC_NEWS_WINDOW, _settings_client.gui.statusbar_pos);
}


/**
 * Switches viewports following vehicles, which get autoreplaced
 * @param from_index the old vehicle ID
 * @param to_index the new vehicle ID
 */
void ChangeVehicleViewports(VehicleID from_index, VehicleID to_index)
{
	Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->viewport != NULL && w->viewport->follow_vehicle == from_index) {
			w->viewport->follow_vehicle = to_index;
			w->SetDirty();
		}
	}
}


/**
 * Relocate all windows to fit the new size of the game application screen
 * @param neww New width of the game application screen
 * @param newh New height of the game application screen.
 */
void RelocateAllWindows(int neww, int newh)
{
	Window *w;

	FOR_ALL_WINDOWS_FROM_BACK(w) {
		int left, top;

		if (w->window_class == WC_MAIN_WINDOW) {
			ViewPort *vp = w->viewport;
			vp->width = w->width = neww;
			vp->height = w->height = newh;
			vp->virtual_width = ScaleByZoom(neww, vp->zoom);
			vp->virtual_height = ScaleByZoom(newh, vp->zoom);
			continue; // don't modify top,left
		}

		/* XXX - this probably needs something more sane. For example specifying
		 * in a 'backup'-desc that the window should always be centered. */
		switch (w->window_class) {
			case WC_MAIN_TOOLBAR:
				ResizeWindow(w, min(neww, *_preferred_toolbar_size) - w->width, 0);

				top = w->top;
				left = PositionMainToolbar(w); // changes toolbar orientation
				break;

			case WC_NEWS_WINDOW:
				top = newh - w->height;
				left = PositionNewsMessage(w);
				break;

			case WC_STATUS_BAR:
				ResizeWindow(w, min(neww, *_preferred_statusbar_size) - w->width, 0);

				top = newh - w->height;
				left = PositionStatusbar(w);
				break;

			case WC_SEND_NETWORK_MSG:
				ResizeWindow(w, Clamp(neww, 320, 640) - w->width, 0);
				top = newh - w->height - FindWindowById(WC_STATUS_BAR, 0)->height;
				left = (neww - w->width) >> 1;
				break;

			case WC_CONSOLE:
				IConsoleResize(w);
				continue;

			default: {
				if (w->flags4 & WF_CENTERED) {
					top = (newh - w->height) >> 1;
					left = (neww - w->width) >> 1;
					break;
				}

				left = w->left;
				if (left + (w->width >> 1) >= neww) left = neww - w->width;
				if (left < 0) left = 0;

				top = w->top;
				if (top + (w->height >> 1) >= newh) top = newh - w->height;

				const Window *wt = FindWindowById(WC_MAIN_TOOLBAR, 0);
				if (wt != NULL) {
					if (top < wt->height && wt->left < (w->left + w->width) && (wt->left + wt->width) > w->left) top = wt->height;
					if (top >= newh) top = newh - 1;
				} else {
					if (top < 0) top = 0;
				}
				break;
			}
		}

		if (w->viewport != NULL) {
			w->viewport->left += left - w->left;
			w->viewport->top += top - w->top;
		}

		w->left = left;
		w->top = top;
	}
}

/**
 * Destructor of the base class PickerWindowBase
 * Main utility is to stop the base Window destructor from triggering
 * a free while the child will already be free, in this case by the ResetObjectToPlace().
 */
PickerWindowBase::~PickerWindowBase()
{
	this->window_class = WC_INVALID; // stop the ancestor from freeing the already (to be) child
	ResetObjectToPlace();
}
