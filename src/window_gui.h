/* $Id$ */

/** @file window_gui.h Functions, definitions and such used only by the GUI. */

#ifndef WINDOW_GUI_H
#define WINDOW_GUI_H

#include "core/bitmath_func.hpp"
#include "core/geometry_type.hpp"
#include "vehicle_type.h"
#include "viewport_type.h"
#include "player_type.h"
#include "strings_type.h"
#include "core/alloc_type.hpp"
#include "window_type.h"
#include "tile_type.h"

/**
 * The maximum number of windows that can be opened.
 */
static const int MAX_NUMBER_OF_WINDOWS = 25;

typedef void WindowProc(Window *w, WindowEvent *e);

/* How the resize system works:
    First, you need to add a WWT_RESIZEBOX to the widgets, and you need
     to add the flag WDF_RESIZABLE to the window. Now the window is ready
     to resize itself.
    As you may have noticed, all widgets have a RESIZE_XXX in their line.
     This lines controls how the widgets behave on resize. RESIZE_NONE means
     it doesn't do anything. Any other option let's one of the borders
     move with the changed width/height. So if a widget has
     RESIZE_RIGHT, and the window is made 5 pixels wider by the user,
     the right of the window will also be made 5 pixels wider.
    Now, what if you want to clamp a widget to the bottom? Give it the flag
     RESIZE_TB. This is RESIZE_TOP + RESIZE_BOTTOM. Now if the window gets
     5 pixels bigger, both the top and bottom gets 5 bigger, so the whole
     widgets moves downwards without resizing, and appears to be clamped
     to the bottom. Nice aint it?
   You should know one more thing about this system. Most windows can't
    handle an increase of 1 pixel. So there is a step function, which
    let the windowsize only be changed by X pixels. You configure this
    after making the window, like this:
      w->resize.step_height = 10;
    Now the window will only change in height in steps of 10.
   You can also give a minimum width and height. The default value is
    the default height/width of the window itself. You can change this
    AFTER window - creation, with:
     w->resize.width or w->resize.height.
   That was all.. good luck, and enjoy :) -- TrueLight */

enum ResizeFlag {
	RESIZE_NONE   = 0,  ///< no resize required

	RESIZE_LEFT   = 1,  ///< left resize flag
	RESIZE_RIGHT  = 2,  ///< rigth resize flag
	RESIZE_TOP    = 4,  ///< top resize flag
	RESIZE_BOTTOM = 8,  ///< bottom resize flag

	RESIZE_LR     = RESIZE_LEFT  | RESIZE_RIGHT,   ///<  combination of left and right resize flags
	RESIZE_RB     = RESIZE_RIGHT | RESIZE_BOTTOM,  ///<  combination of right and bottom resize flags
	RESIZE_TB     = RESIZE_TOP   | RESIZE_BOTTOM,  ///<  combination of top and bottom resize flags
	RESIZE_LRB    = RESIZE_LEFT  | RESIZE_RIGHT  | RESIZE_BOTTOM, ///< combination of left, right and bottom resize flags
	RESIZE_LRTB   = RESIZE_LEFT  | RESIZE_RIGHT  | RESIZE_TOP | RESIZE_BOTTOM,  ///<  combination of all resize flags
	RESIZE_RTB    = RESIZE_RIGHT | RESIZE_TOP    | RESIZE_BOTTOM, ///<  combination of right, top and bottom resize flag

	/* The following flags are used by the system to specify what is disabled, hidden, or clicked
	 * They are used in the same place as the above RESIZE_x flags, Widget visual_flags.
	 * These states are used in exceptions. If nothing is specified, they will indicate
	 * Enabled, visible or unclicked widgets*/
	WIDG_DISABLED = 4,  ///< widget is greyed out, not available
	WIDG_HIDDEN   = 5,  ///< widget is made invisible
	WIDG_LOWERED  = 6,  ///< widget is paint lowered, a pressed button in fact
};

enum {
	WIDGET_LIST_END = -1, ///< indicate the end of widgets' list for vararg functions
};

/**
 * Window widget data structure
 */
struct Widget {
	byte type;                        ///< Widget type, see WindowWidgetTypes
	byte display_flags;               ///< Resize direction, alignment, etc. during resizing, see ResizeFlags
	byte color;                       ///< Widget colour, see docs/ottd-colourtext-palette.png
	int16 left, right, top, bottom;   ///< The position offsets inside the window
	uint16 data;                      ///< The String/Image or special code (list-matrixes) of a widget
	StringID tooltips;                ///< Tooltips that are shown when rightclicking on a widget
};

/**
 * Flags to describe the look of the frame
 */
enum FrameFlags {
	FR_NONE         =  0,
	FR_TRANSPARENT  =  1 << 0,  ///< Makes the background transparent if set
	FR_BORDERONLY   =  1 << 4,  ///< Draw border only, no background
	FR_LOWERED      =  1 << 5,  ///< If set the frame is lowered and the background color brighter (ie. buttons when pressed)
	FR_DARKENED     =  1 << 6,  ///< If set the background is darker, allows for lowered frames with normal background color when used with FR_LOWERED (ie. dropdown boxes)
};

DECLARE_ENUM_AS_BIT_SET(FrameFlags);

/* wiget.cpp */
void DrawFrameRect(int left, int top, int right, int bottom, int color, FrameFlags flags);

/**
 * Available window events
 */
enum WindowEventCodes {
	WE_CREATE,       ///< Initialize the Window
	WE_DESTROY,      ///< Prepare for deletion of the window
	WE_PAINT,        ///< Repaint the window contents
	WE_KEYPRESS,     ///< Key pressed
	WE_CLICK,        ///< Left mouse button click
	WE_MOUSELOOP,    ///< Event for each mouse event in the game (at least once every game tick)
	WE_TICK,         ///< Regularly occurring event (every game tick)
	WE_100_TICKS,    ///< Regularly occurring event (every 100 game ticks, approximatelly 3 seconds)
	WE_TIMEOUT,
	WE_PLACE_OBJ,
	WE_ABORT_PLACE_OBJ,
	WE_ON_EDIT_TEXT,
	WE_PLACE_DRAG,
	WE_PLACE_MOUSEUP,
	WE_PLACE_PRESIZE,
	WE_DROPDOWN_SELECT,
	WE_RESIZE,          ///< Request to resize the window, @see WindowEvent.we.resize
	WE_INVALIDATE_DATA, ///< Notification that data displayed by the window is obsolete
	WE_CTRL_CHANGED,    ///< CTRL key has changed state
};

/**
 * Data structures for additional data associated with a window event
 * @see WindowEventCodes
 */
struct WindowEvent {
	byte event;
	union {
		struct {
			Point pt;
			int widget;
		} click;

		struct {
			Point pt;
			TileIndex tile;
			TileIndex starttile;
			ViewportPlaceMethod select_method;
			byte select_proc;
		} place;

		struct {
			Point pt;
			int widget;
		} dragdrop;

		struct {
			Point size;
			Point diff;
		} sizing;

		struct {
			char *str;
		} edittext;

		struct {
			int button;
			int index;
		} dropdown;

		struct {
			bool cont;      ///< continue the search? (default true)
			uint16 key;     ///< 16-bit Unicode value of the key
			uint16 keycode; ///< untranslated key (including shift-state)
		} keypress;

		struct {
			int data;
		} invalidate;

		struct {
			bool cont;     ///< continue the search? (default true)
		} ctrl;
	} we;
};

/**
 * High level window description
 */
struct WindowDesc {
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
	WindowProc *proc;       ///< Window event handler function for the window
};

/**
 * Window default widget/window handling flags
 */
enum WindowDefaultFlag {
	WDF_STD_TOOLTIPS    =   1 << 0, ///< use standard routine when displaying tooltips
	WDF_DEF_WIDGET      =   1 << 1, ///< Default widget control for some widgets in the on click event, @see DispatchLeftClickEvent()
	WDF_STD_BTN         =   1 << 2, ///< Default handling for close and titlebar widgets (widget no 0 and 1)

	WDF_UNCLICK_BUTTONS =   1 << 4, ///< Unclick buttons when the window event times out
	WDF_STICKY_BUTTON   =   1 << 5, ///< Set window to sticky mode; they are not closed unless closed with 'X' (widget 2)
	WDF_RESIZABLE       =   1 << 6, ///< Window can be resized
	WDF_MODAL           =   1 << 7, ///< The window is a modal child of some other window, meaning the parent is 'inactive'
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
	uint16 pos;
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
private:
	WindowProc *wndproc;   ///< Event handler function for the window. Do not use directly, call HandleWindowEvent() instead.
	void HandleWindowEvent(WindowEvent *e);

protected:
	void Initialize(int x, int y, int min_width, int min_height,
			WindowProc *proc, WindowClass cls, const Widget *widget, int window_number);
	void FindWindowPlacementAndResize(int def_width, int def_height);
	void FindWindowPlacementAndResize(const WindowDesc *desc);

public:
	Window(int x, int y, int width, int height, WindowProc *proc, WindowClass cls, const Widget *widget);
	Window(const WindowDesc *desc, WindowNumber number = 0);

	virtual ~Window();

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

	byte caption_color; ///< Background color of the window caption, contains PlayerID

	ViewportData *viewport;      ///< Pointer to viewport data, if present
	const Widget *original_widget; ///< Original widget layout, copied from WindowDesc
	Widget *widget;        ///< Widgets of the window
	uint widget_count;     ///< Number of widgets of the window
	uint32 desc_flags;     ///< Window/widgets default flags setting, @see WindowDefaultFlag

	Window *parent;        ///< Parent window

	void HandleButtonClick(byte widget);

	void SetWidgetDisabledState(byte widget_index, bool disab_stat);
	void DisableWidget(byte widget_index);
	void EnableWidget(byte widget_index);
	bool IsWidgetDisabled(byte widget_index) const;
	void SetWidgetHiddenState(byte widget_index, bool hidden_stat);
	void HideWidget(byte widget_index);
	void ShowWidget(byte widget_index);
	bool IsWidgetHidden(byte widget_index) const;
	void SetWidgetLoweredState(byte widget_index, bool lowered_stat);
	void ToggleWidgetLoweredState(byte widget_index);
	void LowerWidget(byte widget_index);
	void RaiseWidget(byte widget_index);
	bool IsWidgetLowered(byte widget_index) const;

	void RaiseButtons();
	void CDECL SetWidgetsDisabledState(bool disab_stat, int widgets, ...);
	void CDECL SetWidgetsHiddenState(bool hidden_stat, int widgets, ...);
	void CDECL SetWidgetsLoweredState(bool lowered_stat, int widgets, ...);
	void InvalidateWidget(byte widget_index) const;

	void DrawWidgets() const;
	void DrawViewport() const;
	void DrawSortButtonState(int widget, SortButtonState state) const;

	void SetDirty() const;

	/*** Event handling ***/

	/**
	 * This window is currently being repainted.
	 */
	virtual void OnPaint();


	/**
	 * A key has been pressed.
	 * @param key     the Unicode value of the key.
	 * @param keycode the untranslated key code including shift state.
	 * @return true if the key press has been handled and no other
	 *         window should receive the event.
	 */
	virtual bool OnKeyPress(uint16 key, uint16 keycode);

	/**
	 * The state of the control key has changed
	 * @return true if the change has been handled and no other
	 *         window should receive the event.
	 */
	virtual bool OnCTRLStateChange();


	/**
	 * A click with the left mouse button has been made on the window.
	 * @param pt     the point inside the window that has been clicked.
	 * @param widget the clicked widget.
	 */
	virtual void OnClick(Point pt, int widget);

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
	virtual void OnMouseLoop();

	/**
	 * Called once per (game) tick.
	 */
	virtual void OnTick();

	/**
	 * Called once every 100 (game) ticks.
	 */
	virtual void OnHundredthTick();

	/**
	 * Called when this window's timeout has been reached.
	 */
	virtual void OnTimeout();


	/**
	 * Called when the window got resized.
	 * @param new_size the new size of the window.
	 * @param delta    the amount of which the window size changed.
	 */
	virtual void OnResize(Point new_size, Point delta);

	/**
	 * A dropdown option associated to this window has been selected.
	 * @param widget the widget (button) that the dropdown is associated with.
	 * @param index  the element in the dropdown that is selected.
	 */
	virtual void OnDropdownSelect(int widget, int index);

	/**
	 * The query window opened from this window has closed.
	 * @param str the new value of the string or NULL if the window
	 *            was cancelled.
	 */
	virtual void OnQueryTextFinished(char *str);

	/**
	 * Some data on this window has become invalid.
	 * @param data information about the changed data.
	 */
	virtual void OnInvalidateData(int data = 0);


	/**
	 * The user clicked some place on the map when a tile highlight mode
	 * has been set.
	 * @param pt   the exact point on the map that has been clicked.
	 * @param tile the tile on the map that has been clicked.
	 */
	virtual void OnPlaceObject(Point pt, TileIndex tile);

	/**
	 * The user cancelled a tile highlight mode that has been set.
	 */
	virtual void OnPlaceObjectAbort();


	/**
	 * The user is dragging over the map when the tile highlight mode
	 * has been set.
	 * @param select_method the method of selection (allowed directions)
	 * @param select_proc   what will be created when the drag is over.
	 * @param pt            the exact point on the map where the mouse is.
	 */
	virtual void OnPlaceDrag(ViewportPlaceMethod select_method, byte select_proc, Point pt);

	/**
	 * The user has dragged over the map when the tile highlight mode
	 * has been set.
	 * @param select_method the method of selection (allowed directions)
	 * @param select_proc   what should be created.
	 * @param pt            the exact point on the map where the mouse was released.
	 * @param start_tile    the begin tile of the drag.
	 * @param end_tile      the end tile of the drag.
	 */
	virtual void OnPlaceMouseUp(ViewportPlaceMethod select_method, byte select_proc, Point pt, TileIndex start_tile, TileIndex end_tile);

	/**
	 * The user moves over the map when a tile highlight mode has been set
	 * when the special mouse mode has been set to 'PRESIZE' mode. An
	 * example of this is the tile highlight for dock building.
	 * @param pt   the exact point on the map where the mouse is.
	 * @param tile the tile on the map where the mouse is.
	 */
	virtual void OnPlacePresize(Point pt, TileIndex tile);

	/*** End of the event handling ***/
};

/**
 * Data structure for a window opened from a toolbar
 */
class PickerWindowBase : public Window {

public:
	PickerWindowBase(const WindowDesc *desc) : Window(desc) {}; // nothing special yet, just propagation

	virtual ~PickerWindowBase();
};

enum SortListFlags {
	VL_NONE    = 0,      ///< no sort
	VL_DESC    = 1 << 0, ///< sort descending or ascending
	VL_RESORT  = 1 << 1, ///< instruct the code to resort the list in the next loop
	VL_REBUILD = 1 << 2, ///< create sort-listing to use for qsort and friends
	VL_END     = 1 << 3,
};
DECLARE_ENUM_AS_BIT_SET(SortListFlags);

struct Listing {
	bool order;    ///< Ascending/descending
	byte criteria; ///< Sorting criteria
};

template <typename T>
struct GUIList {
	T* sort_list;        ///< The items to sort.
	SortListFlags flags; ///< used to control sorting/resorting/etc.
	uint16 list_length;  ///< length of the list being sorted
	uint16 resort_timer; ///< resort list after a given amount of ticks if set
	byte sort_type;      ///< what criteria to sort on
};

/****************** THESE ARE NOT WIDGET TYPES!!!!! *******************/
enum WindowWidgetBehaviours {
	WWB_PUSHBUTTON  = 1 << 5,

	WWB_MASK        = 0xE0,
};


/**
 * Window widget types
 */
enum WindowWidgetTypes {
	WWT_EMPTY,      ///< Empty widget, place holder to reserve space in widget array

	WWT_PANEL,      ///< Simple depressed panel
	WWT_INSET,      ///< Pressed (inset) panel, most commonly used as combo box _text_ area
	WWT_IMGBTN,     ///< Button with image
	WWT_IMGBTN_2,   ///< Button with diff image when clicked

	WWT_TEXTBTN,    ///< Button with text
	WWT_TEXTBTN_2,  ///< Button with diff text when clicked
	WWT_LABEL,      ///< Centered label
	WWT_TEXT,       ///< Pure simple text
	WWT_MATRIX,     ///< List of items underneath each other
	WWT_SCROLLBAR,  ///< Vertical scrollbar
	WWT_FRAME,      ///< Frame
	WWT_CAPTION,    ///< Window caption (window title between closebox and stickybox)

	WWT_HSCROLLBAR, ///< Horizontal scrollbar
	WWT_STICKYBOX,  ///< Sticky box (normally at top-right of a window)
	WWT_SCROLL2BAR, ///< 2nd vertical scrollbar
	WWT_RESIZEBOX,  ///< Resize box (normally at bottom-right of a window)
	WWT_CLOSEBOX,   ///< Close box (at top-left of a window)
	WWT_DROPDOWN,   ///< Raised drop down list (regular)
	WWT_DROPDOWNIN, ///< Inset drop down list (used on game options only)
	WWT_EDITBOX,    ///< a textbox for typing (don't forget to call ShowOnScreenKeyboard() when clicked)
	WWT_LAST,       ///< Last Item. use WIDGETS_END to fill up padding!!

	WWT_MASK = 0x1F,

	WWT_PUSHBTN     = WWT_PANEL   | WWB_PUSHBUTTON,
	WWT_PUSHTXTBTN  = WWT_TEXTBTN | WWB_PUSHBUTTON,
	WWT_PUSHIMGBTN  = WWT_IMGBTN  | WWB_PUSHBUTTON,
};

#define WIDGETS_END WWT_LAST,   RESIZE_NONE,     0,     0,     0,     0,     0, 0, STR_NULL

/**
 * Window flags
 */
enum WindowFlags {
	WF_TIMEOUT_SHL       = 0,       ///< Window timeout counter shift
	WF_TIMEOUT_MASK      = 7,       ///< Window timeout counter bit mask (3 bits), @see WF_TIMEOUT_SHL
	WF_DRAGGING          = 1 <<  3, ///< Window is being dragged
	WF_SCROLL_UP         = 1 <<  4, ///< Upper scroll button has been pressed, @see ScrollbarClickHandler()
	WF_SCROLL_DOWN       = 1 <<  5, ///< Lower scroll button has been pressed, @see ScrollbarClickHandler()
	WF_SCROLL_MIDDLE     = 1 <<  6, ///< Scrollbar scrolling, @see ScrollbarClickHandler()
	WF_HSCROLL           = 1 <<  7,
	WF_SIZING            = 1 <<  8,
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
 * @param *desc The pointer to the WindowDesc to be created
 * @param window_number the window number of the new window
 * @param data arbitrary data that is send with the WE_CREATE message
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
void GuiShowTooltipsWithArgs(StringID str, uint paramcount, const uint64 params[]);
static inline void GuiShowTooltips(StringID str)
{
	GuiShowTooltipsWithArgs(str, 0, NULL);
}

/* widget.cpp */
int GetWidgetFromPos(const Window *w, int x, int y);

/* window.cpp */
extern Window *_z_windows[];
extern Window **_last_z_window;
#define FOR_ALL_WINDOWS(wz) for (wz = _z_windows; wz != _last_z_window; wz++)

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
Window **FindWindowZPosition(const Window *w);

void ScrollbarClickHandler(Window *w, const Widget *wi, int x, int y);

void ResizeButtons(Window *w, byte left, byte right);

void ResizeWindowForWidget(Window *w, int widget, int delta_x, int delta_y);


/**
 * Sets the enabled/disabled status of a widget.
 * By default, widgets are enabled.
 * On certain conditions, they have to be disabled.
 * @param widget_index : index of this widget in the window
 * @param disab_stat : status to use ie: disabled = true, enabled = false
 */
inline void Window::SetWidgetDisabledState(byte widget_index, bool disab_stat)
{
	assert(widget_index < this->widget_count);
	SB(this->widget[widget_index].display_flags, WIDG_DISABLED, 1, !!disab_stat);
}

/**
 * Sets a widget to disabled.
 * @param widget_index : index of this widget in the window
 */
inline void Window::DisableWidget(byte widget_index)
{
	SetWidgetDisabledState(widget_index, true);
}

/**
 * Sets a widget to Enabled.
 * @param widget_index : index of this widget in the window
 */
inline void Window::EnableWidget(byte widget_index)
{
	SetWidgetDisabledState(widget_index, false);
}

/**
 * Gets the enabled/disabled status of a widget.
 * @param widget_index : index of this widget in the window
 * @return status of the widget ie: disabled = true, enabled = false
 */
inline bool Window::IsWidgetDisabled(byte widget_index) const
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
inline void Window::SetWidgetHiddenState(byte widget_index, bool hidden_stat)
{
	assert(widget_index < this->widget_count);
	SB(this->widget[widget_index].display_flags, WIDG_HIDDEN, 1, !!hidden_stat);
}

/**
 * Sets a widget hidden.
 * @param widget_index : index of this widget in the window
 */
inline void Window::HideWidget(byte widget_index)
{
	SetWidgetHiddenState(widget_index, true);
}

/**
 * Sets a widget visible.
 * @param widget_index : index of this widget in the window
 */
inline void Window::ShowWidget(byte widget_index)
{
	SetWidgetHiddenState(widget_index, false);
}

/**
 * Gets the visibility of a widget.
 * @param widget_index : index of this widget in the window
 * @return status of the widget ie: hidden = true, visible = false
 */
inline bool Window::IsWidgetHidden(byte widget_index) const
{
	assert(widget_index < this->widget_count);
	return HasBit(this->widget[widget_index].display_flags, WIDG_HIDDEN);
}

/**
 * Sets the lowered/raised status of a widget.
 * @param widget_index : index of this widget in the window
 * @param lowered_stat : status to use ie: lowered = true, raised = false
 */
inline void Window::SetWidgetLoweredState(byte widget_index, bool lowered_stat)
{
	assert(widget_index < this->widget_count);
	SB(this->widget[widget_index].display_flags, WIDG_LOWERED, 1, !!lowered_stat);
}

/**
 * Invert the lowered/raised  status of a widget.
 * @param widget_index : index of this widget in the window
 */
inline void Window::ToggleWidgetLoweredState(byte widget_index)
{
	assert(widget_index < this->widget_count);
	ToggleBit(this->widget[widget_index].display_flags, WIDG_LOWERED);
}

/**
 * Marks a widget as lowered.
 * @param widget_index : index of this widget in the window
 */
inline void Window::LowerWidget(byte widget_index)
{
	SetWidgetLoweredState(widget_index, true);
}

/**
 * Marks a widget as raised.
 * @param widget_index : index of this widget in the window
 */
inline void Window::RaiseWidget(byte widget_index)
{
	SetWidgetLoweredState(widget_index, false);
}

/**
 * Gets the lowered state of a widget.
 * @param widget_index : index of this widget in the window
 * @return status of the widget ie: lowered = true, raised= false
 */
inline bool Window::IsWidgetLowered(byte widget_index) const
{
	assert(widget_index < this->widget_count);
	return HasBit(this->widget[widget_index].display_flags, WIDG_LOWERED);
}

#endif /* WINDOW_GUI_H */
