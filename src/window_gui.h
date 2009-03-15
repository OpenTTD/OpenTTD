/* $Id$ */

/** @file window_gui.h Functions, definitions and such used only by the GUI. */

#ifndef WINDOW_GUI_H
#define WINDOW_GUI_H

#include "core/geometry_type.hpp"
#include "vehicle_type.h"
#include "viewport_type.h"
#include "company_type.h"
#include "core/alloc_type.hpp"
#include "window_type.h"
#include "tile_type.h"
#include "widget_type.h"

/**
 * Flags to describe the look of the frame
 */
enum FrameFlags {
	FR_NONE         =  0,
	FR_TRANSPARENT  =  1 << 0,  ///< Makes the background transparent if set
	FR_BORDERONLY   =  1 << 4,  ///< Draw border only, no background
	FR_LOWERED      =  1 << 5,  ///< If set the frame is lowered and the background colour brighter (ie. buttons when pressed)
	FR_DARKENED     =  1 << 6,  ///< If set the background is darker, allows for lowered frames with normal background colour when used with FR_LOWERED (ie. dropdown boxes)
};

DECLARE_ENUM_AS_BIT_SET(FrameFlags);

/* wiget.cpp */
void DrawFrameRect(int left, int top, int right, int bottom, Colours colour, FrameFlags flags);

/* window.cpp */
extern Window *_z_front_window;
extern Window *_z_back_window;
extern Window *_focused_window;

/**
 * High level window description
 */
struct WindowDesc : ZeroedMemoryAllocator {

	WindowDesc(int16 left, int16 top, int16 min_width, int16 min_height, int16 def_width, int16 def_height,
			WindowClass window_class, WindowClass parent_class, uint32 flags, const Widget *widgets);

	int16 left;             ///< Prefered x position of left edge of the window, @see WindowDefaultPosition()
	int16 top;              ///< Prefered y position of the top of the window, @see WindowDefaultPosition()
	int16 minimum_width;    ///< Minimal width of the window
	int16 minimum_height;   ///< Minimal height of the window
	int16 default_width;    ///< Prefered initial width of the window
	int16 default_height;   ///< Prefered initial height of the window
	WindowClass cls;        ///< Class of the window, @see WindowClass
	WindowClass parent_cls; ///< Class of the parent window, @see WindowClass
	uint32 flags;           ///< Flags, @see WindowDefaultFlags
	const Widget *widgets;  ///< List of widgets with their position and size for the window
};

/**
 * Window default widget/window handling flags
 */
enum WindowDefaultFlag {
	WDF_STD_TOOLTIPS    =   1 << 0, ///< use standard routine when displaying tooltips
	WDF_DEF_WIDGET      =   1 << 1, ///< Default widget control for some widgets in the on click event, @see DispatchLeftClickEvent()
	WDF_STD_BTN         =   1 << 2, ///< Default handling for close and titlebar widgets (widget no 0 and 1)
	WDF_CONSTRUCTION    =   1 << 3, ///< This window is used for construction; close it whenever changing company.

	WDF_UNCLICK_BUTTONS =   1 << 4, ///< Unclick buttons when the window event times out
	WDF_STICKY_BUTTON   =   1 << 5, ///< Set window to sticky mode; they are not closed unless closed with 'X' (widget 2)
	WDF_RESIZABLE       =   1 << 6, ///< Window can be resized
	WDF_MODAL           =   1 << 7, ///< The window is a modal child of some other window, meaning the parent is 'inactive'

	WDF_NO_FOCUS        =   1 << 8, ///< This window won't get focus/make any other window lose focus when click
};

/**
 * Special values for 'left' and 'top' to cause a specific placement
 */
enum WindowDefaultPosition {
	WDP_AUTO      = -1, ///< Find a place automatically
	WDP_CENTER    = -2, ///< Center the window (left/right or top/bottom)
	WDP_ALIGN_TBR = -3, ///< Align the right side of the window with the right side of the main toolbar
	WDP_ALIGN_TBL = -4, ///< Align the left side of the window with the left side of the main toolbar
};

/**
 * Scrollbar data structure
 */
struct Scrollbar {
	uint16 count;  ///< Number of elements in the list
	uint16 cap;    ///< Number of visible elements of the scroll bar
	uint16 pos;    ///< Index of first visible item of the list
};

/**
 * Data structure for resizing a window
 */
struct ResizeInfo {
	uint width;       ///< Minimum allowed width of the window
	uint height;      ///< Minimum allowed height of the window
	uint step_width;  ///< Step-size of width resize changes
	uint step_height; ///< Step-size of height resize changes
};

enum SortButtonState {
	SBS_OFF,
	SBS_DOWN,
	SBS_UP,
};

/**
 * Data structure for a window viewport
 */
struct ViewportData : ViewPort {
	VehicleID follow_vehicle;
	int32 scrollpos_x;
	int32 scrollpos_y;
	int32 dest_scrollpos_x;
	int32 dest_scrollpos_y;
};

/**
 * Data structure for an opened window
 */
struct Window : ZeroedMemoryAllocator {
	/** State whether an event is handled or not */
	enum EventState {
		ES_HANDLED,     ///< The passed event is handled
		ES_NOT_HANDLED, ///< The passed event is not handled
	};

protected:
	void Initialize(int x, int y, int min_width, int min_height,
			WindowClass cls, const Widget *widget, int window_number);
	void FindWindowPlacementAndResize(int def_width, int def_height);
	void FindWindowPlacementAndResize(const WindowDesc *desc);

public:
	Window(int x, int y, int width, int height, WindowClass cls, const Widget *widget);
	Window(const WindowDesc *desc, WindowNumber number = 0);

	virtual ~Window();
	/* Don't allow arrays; arrays of Windows are pointless as you need
	 * to destruct them all at the same time too, which is kinda hard. */
	FORCEINLINE void *operator new[](size_t size) { NOT_REACHED(); }
	/* Don't free the window directly; it corrupts the linked list when iterating */
	FORCEINLINE void operator delete(void *ptr, size_t size) {}

	uint16 flags4;              ///< Window flags, @see WindowFlags
	WindowClass window_class;   ///< Window class
	WindowNumber window_number; ///< Window number within the window class

	int left;   ///< x position of left edge of the window
	int top;    ///< y position of top edge of the window
	int width;  ///< width of the window (number of pixels to the right in x direction)
	int height; ///< Height of the window (number of pixels down in y direction)

	Scrollbar hscroll;  ///< Horizontal scroll bar
	Scrollbar vscroll;  ///< First vertical scroll bar
	Scrollbar vscroll2; ///< Second vertical scroll bar
	ResizeInfo resize;  ///< Resize information

	Owner owner;        ///< The owner of the content shown in this window. Company colour is acquired from this variable.

	ViewportData *viewport;///< Pointer to viewport data, if present
	Widget *widget;        ///< Widgets of the window
	uint widget_count;     ///< Number of widgets of the window
	uint32 desc_flags;     ///< Window/widgets default flags setting, @see WindowDefaultFlag
	const Widget *focused_widget; ///< Currently focused widget or NULL, if no widget has focus

	Window *parent;        ///< Parent window
	Window *z_front;       ///< The window in front of us in z-order
	Window *z_back;        ///< The window behind us in z-order

	/**
	 * Sets the enabled/disabled status of a widget.
	 * By default, widgets are enabled.
	 * On certain conditions, they have to be disabled.
	 * @param widget_index index of this widget in the window
	 * @param disab_stat status to use ie: disabled = true, enabled = false
	 */
	inline void SetWidgetDisabledState(byte widget_index, bool disab_stat)
	{
		assert(widget_index < this->widget_count);
		SB(this->widget[widget_index].display_flags, WIDG_DISABLED, 1, !!disab_stat);
	}

	/**
	 * Sets a widget to disabled.
	 * @param widget_index index of this widget in the window
	 */
	inline void DisableWidget(byte widget_index)
	{
		SetWidgetDisabledState(widget_index, true);
	}

	/**
	 * Sets a widget to Enabled.
	 * @param widget_index index of this widget in the window
	 */
	inline void EnableWidget(byte widget_index)
	{
		SetWidgetDisabledState(widget_index, false);
	}

	/**
	 * Gets the enabled/disabled status of a widget.
	 * @param widget_index index of this widget in the window
	 * @return status of the widget ie: disabled = true, enabled = false
	 */
	inline bool IsWidgetDisabled(byte widget_index) const
	{
		assert(widget_index < this->widget_count);
		return HasBit(this->widget[widget_index].display_flags, WIDG_DISABLED);
	}

	/**
	 * Sets the hidden/shown status of a widget.
	 * By default, widgets are visible.
	 * On certain conditions, they have to be hidden.
	 * @param widget_index index of this widget in the window
	 * @param hidden_stat status to use ie. hidden = true, visible = false
	 */
	inline void SetWidgetHiddenState(byte widget_index, bool hidden_stat)
	{
		assert(widget_index < this->widget_count);
		SB(this->widget[widget_index].display_flags, WIDG_HIDDEN, 1, !!hidden_stat);
	}

	/**
	 * Sets a widget hidden.
	 * @param widget_index index of this widget in the window
	 */
	inline void HideWidget(byte widget_index)
	{
		SetWidgetHiddenState(widget_index, true);
	}

	/**
	 * Sets a widget visible.
	 * @param widget_index index of this widget in the window
	 */
	inline void ShowWidget(byte widget_index)
	{
		SetWidgetHiddenState(widget_index, false);
	}

	/**
	 * Gets the visibility of a widget.
	 * @param widget_index index of this widget in the window
	 * @return status of the widget ie: hidden = true, visible = false
	 */
	inline bool IsWidgetHidden(byte widget_index) const
	{
		assert(widget_index < this->widget_count);
		return HasBit(this->widget[widget_index].display_flags, WIDG_HIDDEN);
	}

	/**
	 * Set focus within this window to given widget. The function however don't
	 * change which window that has focus.
	 * @param widget_index : index of the widget in the window to set focus to
	 */
	inline void SetFocusedWidget(byte widget_index)
	{
		if (widget_index < this->widget_count) {
			/* Repaint the widget that loss focus. A focused edit box may else leave the caret left on the screen */
			if (this->focused_widget && this->focused_widget - this->widget != widget_index) {
				this->InvalidateWidget(this->focused_widget - this->widget);
			}
			this->focused_widget = &this->widget[widget_index];
		}
	}

	/**
	 * Check if given widget is focused within this window
	 * @param widget_index : index of the widget in the window to check
	 * @return true if given widget is the focused window in this window
	 */
	inline bool IsWidgetFocused(byte widget_index) const
	{
		return this->focused_widget == &this->widget[widget_index];
	}

	/**
	 * Check if given widget has user input focus. This means that both the window
	 * has focus and that the given widget has focus within the window.
	 * @param widget_index : index of the widget in the window to check
	 * @return true if given widget is the focused window in this window and this window has focus
	 */
	inline bool IsWidgetGloballyFocused(byte widget_index) const
	{
		return _focused_window == this && IsWidgetFocused(widget_index);
	}

	/**
	 * Sets the lowered/raised status of a widget.
	 * @param widget_index index of this widget in the window
	 * @param lowered_stat status to use ie: lowered = true, raised = false
	 */
	inline void SetWidgetLoweredState(byte widget_index, bool lowered_stat)
	{
		assert(widget_index < this->widget_count);
		SB(this->widget[widget_index].display_flags, WIDG_LOWERED, 1, !!lowered_stat);
	}

	/**
	 * Invert the lowered/raised  status of a widget.
	 * @param widget_index index of this widget in the window
	 */
	inline void ToggleWidgetLoweredState(byte widget_index)
	{
		assert(widget_index < this->widget_count);
		ToggleBit(this->widget[widget_index].display_flags, WIDG_LOWERED);
	}

	/**
	 * Marks a widget as lowered.
	 * @param widget_index index of this widget in the window
	 */
	inline void LowerWidget(byte widget_index)
	{
		SetWidgetLoweredState(widget_index, true);
	}

	/**
	 * Marks a widget as raised.
	 * @param widget_index index of this widget in the window
	 */
	inline void RaiseWidget(byte widget_index)
	{
		SetWidgetLoweredState(widget_index, false);
	}

	/**
	 * Gets the lowered state of a widget.
	 * @param widget_index index of this widget in the window
	 * @return status of the widget ie: lowered = true, raised= false
	 */
	inline bool IsWidgetLowered(byte widget_index) const
	{
		assert(widget_index < this->widget_count);
		return HasBit(this->widget[widget_index].display_flags, WIDG_LOWERED);
	}

	/**
	 * Align widgets a and b next to each other.
	 * @param widget_index_a  the left widget
	 * @param widget_index_b  the right widget (fixed)
	 */
	inline void AlignWidgetRight(byte widget_index_a, byte widget_index_b)
	{
		assert(widget_index_a < this->widget_count);
		assert(widget_index_b < this->widget_count);
		int w = this->widget[widget_index_a].right - this->widget[widget_index_a].left;
		this->widget[widget_index_a].right = this->widget[widget_index_b].left - 1;
		this->widget[widget_index_a].left  = this->widget[widget_index_a].right - w;
	}

	/**
	 * Get the width of a widget.
	 * @param widget_index  the widget
	 * @return width of the widget
	 */
	inline int GetWidgetWidth(byte widget_index) const
	{
		assert(widget_index < this->widget_count);
		return this->widget[widget_index].right - this->widget[widget_index].left + 1;
	}

	void HandleButtonClick(byte widget);
	bool HasWidgetOfType(WidgetType widget_type) const;

	void RaiseButtons();
	void CDECL SetWidgetsDisabledState(bool disab_stat, int widgets, ...);
	void CDECL SetWidgetsHiddenState(bool hidden_stat, int widgets, ...);
	void CDECL SetWidgetsLoweredState(bool lowered_stat, int widgets, ...);
	void InvalidateWidget(byte widget_index) const;

	void DrawWidgets() const;
	void DrawViewport() const;
	void DrawSortButtonState(int widget, SortButtonState state) const;

	void DeleteChildWindows() const;

	void SetDirty() const;

	/*** Event handling ***/

	/**
	 * This window is currently being repainted.
	 */
	virtual void OnPaint() {}

	/**
	 * Called when window gains focus
	 */
	virtual void OnFocus() {}

	/**
	 * Called when window looses focus
	 */
	virtual void OnFocusLost() {}

	/**
	 * A key has been pressed.
	 * @param key     the Unicode value of the key.
	 * @param keycode the untranslated key code including shift state.
	 * @return ES_HANDLED if the key press has been handled and no other
	 *         window should receive the event.
	 */
	virtual EventState OnKeyPress(uint16 key, uint16 keycode) { return ES_NOT_HANDLED; }

	/**
	 * The state of the control key has changed
	 * @return ES_HANDLED if the change has been handled and no other
	 *         window should receive the event.
	 */
	virtual EventState OnCTRLStateChange() { return ES_NOT_HANDLED; }


	/**
	 * A click with the left mouse button has been made on the window.
	 * @param pt     the point inside the window that has been clicked.
	 * @param widget the clicked widget.
	 */
	virtual void OnClick(Point pt, int widget) {}

	/**
	 * A double click with the left mouse button has been made on the window.
	 * @param pt     the point inside the window that has been clicked.
	 * @param widget the clicked widget.
	 */
	virtual void OnDoubleClick(Point pt, int widget) {}

	/**
	 * A click with the right mouse button has been made on the window.
	 * @param pt     the point inside the window that has been clicked.
	 * @param widget the clicked widget.
	 */
	virtual void OnRightClick(Point pt, int widget) {}

	/**
	 * A dragged 'object' has been released.
	 * @param pt     the point inside the window where the release took place.
	 * @param widget the widget where the release took place.
	 */
	virtual void OnDragDrop(Point pt, int widget) {}

	/**
	 * Handle the request for (viewport) scrolling.
	 * @param delta the amount the viewport must be scrolled.
	 */
	virtual void OnScroll(Point delta) {}

	/**
	 * The mouse is currently moving over the window or has just moved outside
	 * of the window. In the latter case pt is (-1, -1).
	 * @param pt     the point inside the window that the mouse hovers over.
	 * @param widget the widget the mouse hovers over.
	 */
	virtual void OnMouseOver(Point pt, int widget) {}

	/**
	 * The mouse wheel has been turned.
	 * @param wheel the amount of movement of the mouse wheel.
	 */
	virtual void OnMouseWheel(int wheel) {}


	/**
	 * Called for every mouse loop run, which is at least once per (game) tick.
	 */
	virtual void OnMouseLoop() {}

	/**
	 * Called once per (game) tick.
	 */
	virtual void OnTick() {}

	/**
	 * Called once every 100 (game) ticks.
	 */
	virtual void OnHundredthTick() {}

	/**
	 * Called when this window's timeout has been reached.
	 */
	virtual void OnTimeout() {}


	/**
	 * Called when the window got resized.
	 * @param new_size the new size of the window.
	 * @param delta    the amount of which the window size changed.
	 */
	virtual void OnResize(Point new_size, Point delta) {}

	/**
	 * A dropdown option associated to this window has been selected.
	 * @param widget the widget (button) that the dropdown is associated with.
	 * @param index  the element in the dropdown that is selected.
	 */
	virtual void OnDropdownSelect(int widget, int index) {}

	/**
	 * The query window opened from this window has closed.
	 * @param str the new value of the string or NULL if the window
	 *            was cancelled.
	 */
	virtual void OnQueryTextFinished(char *str) {}

	/**
	 * Some data on this window has become invalid.
	 * @param data information about the changed data.
	 */
	virtual void OnInvalidateData(int data = 0) {}


	/**
	 * The user clicked some place on the map when a tile highlight mode
	 * has been set.
	 * @param pt   the exact point on the map that has been clicked.
	 * @param tile the tile on the map that has been clicked.
	 */
	virtual void OnPlaceObject(Point pt, TileIndex tile) {}

	/**
	 * The user cancelled a tile highlight mode that has been set.
	 */
	virtual void OnPlaceObjectAbort() {}


	/**
	 * The user is dragging over the map when the tile highlight mode
	 * has been set.
	 * @param select_method the method of selection (allowed directions)
	 * @param select_proc   what will be created when the drag is over.
	 * @param pt            the exact point on the map where the mouse is.
	 */
	virtual void OnPlaceDrag(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt) {}

	/**
	 * The user has dragged over the map when the tile highlight mode
	 * has been set.
	 * @param select_method the method of selection (allowed directions)
	 * @param select_proc   what should be created.
	 * @param pt            the exact point on the map where the mouse was released.
	 * @param start_tile    the begin tile of the drag.
	 * @param end_tile      the end tile of the drag.
	 */
	virtual void OnPlaceMouseUp(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt, TileIndex start_tile, TileIndex end_tile) {}

	/**
	 * The user moves over the map when a tile highlight mode has been set
	 * when the special mouse mode has been set to 'PRESIZE' mode. An
	 * example of this is the tile highlight for dock building.
	 * @param pt   the exact point on the map where the mouse is.
	 * @param tile the tile on the map where the mouse is.
	 */
	virtual void OnPlacePresize(Point pt, TileIndex tile) {}

	/*** End of the event handling ***/
};

/**
 * Data structure for a window opened from a toolbar
 */
class PickerWindowBase : public Window {

public:
	PickerWindowBase(const WindowDesc *desc, Window *parent) : Window(desc)
	{
		this->parent = parent;
	};

	virtual ~PickerWindowBase();
};

/**
 * Window flags
 */
enum WindowFlags {
	WF_TIMEOUT_TRIGGER   = 1,       ///< When the timeout should start triggering
	WF_TIMEOUT_BEGIN     = 7,       ///< The initial value for the timeout
	WF_TIMEOUT_MASK      = 7,       ///< Window timeout counter bit mask (3 bits)
	WF_DRAGGING          = 1 <<  3, ///< Window is being dragged
	WF_SCROLL_UP         = 1 <<  4, ///< Upper scroll button has been pressed, @see ScrollbarClickHandler()
	WF_SCROLL_DOWN       = 1 <<  5, ///< Lower scroll button has been pressed, @see ScrollbarClickHandler()
	WF_SCROLL_MIDDLE     = 1 <<  6, ///< Scrollbar scrolling, @see ScrollbarClickHandler()
	WF_HSCROLL           = 1 <<  7,
	WF_SIZING            = 1 <<  8, ///< Window is being resized.
	WF_STICKY            = 1 <<  9, ///< Window is made sticky by user

	WF_DISABLE_VP_SCROLL = 1 << 10, ///< Window does not do autoscroll, @see HandleAutoscroll()

	WF_WHITE_BORDER_ONE  = 1 << 11,
	WF_WHITE_BORDER_MASK = 1 << 12 | WF_WHITE_BORDER_ONE,
	WF_SCROLL2           = 1 << 13,
};

Window *BringWindowToFrontById(WindowClass cls, WindowNumber number);
Window *FindWindowFromPt(int x, int y);

/**
 * Open a new window.
 * @param desc The pointer to the WindowDesc to be created
 * @param window_number the window number of the new window
 * @return see Window pointer of the newly created window
 */
template <typename Wcls>
Wcls *AllocateWindowDescFront(const WindowDesc *desc, int window_number)
{
	if (BringWindowToFrontById(desc->cls, window_number)) return NULL;
	return new Wcls(desc, window_number);
}

void RelocateAllWindows(int neww, int newh);

/* misc_gui.cpp */
void GuiShowTooltips(StringID str, uint paramcount = 0, const uint64 params[] = NULL, bool use_left_mouse_button = false);

/* widget.cpp */
int GetWidgetFromPos(const Window *w, int x, int y);

/** Iterate over all windows */
#define FOR_ALL_WINDOWS_FROM_BACK_FROM(w, start)  for (w = start; w != NULL; w = w->z_front) if (w->window_class != WC_INVALID)
#define FOR_ALL_WINDOWS_FROM_FRONT_FROM(w, start) for (w = start; w != NULL; w = w->z_back) if (w->window_class != WC_INVALID)
#define FOR_ALL_WINDOWS_FROM_BACK(w)  FOR_ALL_WINDOWS_FROM_BACK_FROM(w, _z_back_window)
#define FOR_ALL_WINDOWS_FROM_FRONT(w) FOR_ALL_WINDOWS_FROM_FRONT_FROM(w, _z_front_window)

extern Point _cursorpos_drag_start;

extern int _scrollbar_start_pos;
extern int _scrollbar_size;
extern byte _scroller_click_timeout;

extern bool _scrolling_scrollbar;
extern bool _scrolling_viewport;

extern byte _special_mouse_mode;
enum SpecialMouseMode {
	WSM_NONE     = 0,
	WSM_DRAGDROP = 1,
	WSM_SIZING   = 2,
	WSM_PRESIZE  = 3,
};

Window *GetCallbackWnd();

void SetFocusedWindow(Window *w);
const Widget *GetGloballyFocusedWidget();
bool EditBoxInGlobalFocus();

void ScrollbarClickHandler(Window *w, const Widget *wi, int x, int y);

void ResizeButtons(Window *w, byte left, byte right);

void ResizeWindowForWidget(Window *w, uint widget, int delta_x, int delta_y);

void SetVScrollCount(Window *w, int num);
void SetVScroll2Count(Window *w, int num);
void SetHScrollCount(Window *w, int num);

#endif /* WINDOW_GUI_H */
