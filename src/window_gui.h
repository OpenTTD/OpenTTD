/* $Id$ */

/** @file window_gui.h Functions, definitions and such used only by the GUI. */

#ifndef WINDOW_GUI_H
#define WINDOW_GUI_H

#include "core/geometry_func.hpp"
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

/** Distances used in drawing widgets. */
enum WidgetDrawDistances {
	/* WWT_IMGBTN */
	WD_IMGBTN_LEFT    = 1,      ///< Left offset of the image in the button.
	WD_IMGBTN_RIGHT   = 2,      ///< Right offset of the image in the button.
	WD_IMGBTN_TOP     = 1,      ///< Top offset of image in the button.
	WD_IMGBTN_BOTTOM  = 2,      ///< Bottom offset of image in the button.

	/* WWT_IMGBTN_2 */
	WD_IMGBTN2_LEFT   = 1,      ///< Left offset of the images in the button.
	WD_IMGBTN2_RIGHT  = 3,      ///< Right offset of the images in the button.
	WD_IMGBTN2_TOP    = 1,      ///< Top offset of images in the button.
	WD_IMGBTN2_BOTTOM = 3,      ///< Bottom offset of images in the button.

	/* WWT_INSET */
	WD_INSET_LEFT  = 2,         ///< Left offset of string.
	WD_INSET_RIGHT = 2,         ///< Right offset of string.
	WD_INSET_TOP   = 1,         ///< Top offset of string.

	WD_VSCROLLBAR_WIDTH  = 12,  ///< Width of a vertical scrollbar.

	WD_HSCROLLBAR_HEIGHT = 12,  ///< Height of a horizontal scrollbar.

	/* FrameRect widgets, all text buttons, panel, editbox */
	WD_FRAMERECT_LEFT   = 2,    ///< Offset at left to draw the frame rectangular area
	WD_FRAMERECT_RIGHT  = 2,    ///< Offset at right to draw the frame rectangular area
	WD_FRAMERECT_TOP    = 1,    ///< Offset at top to draw the frame rectangular area
	WD_FRAMERECT_BOTTOM = 1,    ///< Offset at bottom to draw the frame rectangular area

	/* WWT_FRAME */
	WD_FRAMETEXT_LEFT  = 6,     ///< Left offset of the text of the frame.
	WD_FRAMETEXT_RIGHT = 6,     ///< Right offset of the text of the frame.

	/* WWT_MATRIX */
	WD_MATRIX_LEFT   = 2,       ///< Offset at left of a matrix cell.
	WD_MATRIX_RIGHT  = 2,       ///< Offset at right of a matrix cell.
	WD_MATRIX_TOP    = 3,       ///< Offset at top of a matrix cell.
	WD_MATRIX_BOTTOM = 1,       ///< Offset at bottom of a matrix cell.

	/* WWT_STICKYBOX */
	WD_STICKYBOX_WIDTH  = 12,   ///< Width of a standard sticky box widget.
	WD_STICKYBOX_LEFT   = 2,    ///< Left offset of sticky sprite.
	WD_STICKYBOX_RIGHT  = 2,    ///< Right offset of sticky sprite.
	WD_STICKYBOX_TOP    = 3,    ///< Top offset of sticky sprite.
	WD_STICKYBOX_BOTTOM = 3,    ///< Bottom offset of sticky sprite.

	/* WWT_RESIZEBOX */
	WD_RESIZEBOX_WIDTH  = 12,   ///< Width of a resize box widget.
	WD_RESIZEBOX_LEFT   = 3,    ///< Left offset of resize sprite.
	WD_RESIZEBOX_RIGHT  = 2,    ///< Right offset of resize sprite.
	WD_RESIZEBOX_TOP    = 3,    ///< Top offset of resize sprite.
	WD_RESIZEBOX_BOTTOM = 2,    ///< Bottom offset of resize sprite.

	/* WWT_CLOSEBOX */
	WD_CLOSEBOX_WIDTH  = 11,    ///< Width of a close box widget.
	WD_CLOSEBOX_LEFT   = 2,     ///< Left offset of closebox string.
	WD_CLOSEBOX_RIGHT  = 1,     ///< Right offset of closebox string.
	WD_CLOSEBOX_TOP    = 2,     ///< Top offset of closebox string.
	WD_CLOSEBOX_BOTTOM = 2,     ///< Bottom offset of closebox string.

	/* WWT_CAPTION */
	WD_CAPTION_HEIGHT     = 14, ///< Height of a title bar.
	WD_CAPTIONTEXT_LEFT   = 2,  ///< Offset of the caption text at the left.
	WD_CAPTIONTEXT_RIGHT  = 2,  ///< Offset of the caption text at the right.
	WD_CAPTIONTEXT_TOP    = 2,  ///< Offset of the caption text at the top.
	WD_CAPTIONTEXT_BOTTOM = 2,  ///< Offset of the caption text at the bottom.

	/* Dropdown widget. */
	WD_DROPDOWN_HEIGHT     = 12, ///< Height of a drop down widget.
	WD_DROPDOWNTEXT_LEFT   = 2,  ///< Left offset of the dropdown widget string.
	WD_DROPDOWNTEXT_RIGHT  = 14, ///< Right offset of the dropdown widget string.
	WD_DROPDOWNTEXT_TOP    = 1,  ///< Top offset of the dropdown widget string.
	WD_DROPDOWNTEXT_BOTTOM = 1,  ///< Bottom offset of the dropdown widget string.

	WD_SORTBUTTON_ARROW_WIDTH = 11, ///< Width of up/down arrow of sort button state.

	WD_PAR_VSEP_NORMAL = 2,      ///< Amount of vertical space between two paragraphs of text.
};

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
			WindowClass window_class, WindowClass parent_class, uint32 flags, const Widget *widgets,
			const NWidgetPart *nwid_parts = NULL, int16 nwid_length = 0);

	~WindowDesc();

	int16 left;                    ///< Prefered x position of left edge of the window. @see WindowDefaultPosition()
	int16 top;                     ///< Prefered y position of the top of the window. @see WindowDefaultPosition()
	int16 minimum_width;           ///< Minimal width of the window.
	int16 minimum_height;          ///< Minimal height of the window.
	int16 default_width;           ///< Prefered initial width of the window.
	int16 default_height;          ///< Prefered initial height of the window.
	WindowClass cls;               ///< Class of the window, @see WindowClass.
	WindowClass parent_cls;        ///< Class of the parent window. @see WindowClass
	uint32 flags;                  ///< Flags. @see WindowDefaultFlags
	const Widget *widgets;         ///< List of widgets with their position and size for the window.
	const NWidgetPart *nwid_parts; ///< Nested widget parts describing the window.
	int16 nwid_length;             ///< Length of the #nwid_parts array.
	mutable Widget *new_widgets;   ///< Widgets generated from #nwid_parts.

	const Widget *GetWidgets() const;
};

/**
 * Window default widget/window handling flags
 */
enum WindowDefaultFlag {
	WDF_STD_TOOLTIPS    =   1 << 0, ///< use standard routine when displaying tooltips
	WDF_DEF_WIDGET      =   1 << 1, ///< Default widget control for some widgets in the on click event, @see DispatchLeftClickEvent()
	WDF_STD_BTN         =   1 << 2, ///< Default handling for close and titlebar widgets (the widgets with type WWT_CLOSEBOX and WWT_CAPTION).
	WDF_CONSTRUCTION    =   1 << 3, ///< This window is used for construction; close it whenever changing company.

	WDF_UNCLICK_BUTTONS =   1 << 4, ///< Unclick buttons when the window event times out
	WDF_STICKY_BUTTON   =   1 << 5, ///< Set window to sticky mode; they are not closed unless closed with 'X' (widget with type WWT_STICKYBOX).
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
 * Data structure for a window viewport.
 * A viewport is either following a vehicle (its id in then in #follow_vehicle), or it aims to display a specific
 * location #dest_scrollpos_x, #dest_scrollpos_y (#follow_vehicle is then #INVALID_VEHICLE).
 * The actual location being shown is #scrollpos_x, #scrollpos_y.
 * @see InitializeViewport(), UpdateViewportPosition(), UpdateViewportCoordinates().
 */
struct ViewportData : ViewPort {
	VehicleID follow_vehicle; ///< VehicleID to follow if following a vehicle, #INVALID_VEHICLE otherwise.
	int32 scrollpos_x;        ///< Currently shown x coordinate (virtual screen coordinate of topleft corner of the viewport).
	int32 scrollpos_y;        ///< Currently shown y coordinate (virtual screen coordinate of topleft corner of the viewport).
	int32 dest_scrollpos_x;   ///< Current destination x coordinate to display (virtual screen coordinate of topleft corner of the viewport).
	int32 dest_scrollpos_y;   ///< Current destination y coordinate to display (virtual screen coordinate of topleft corner of the viewport).
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
	void InitializeData(WindowClass cls, const Widget *widget, int window_number);
	void InitializePositionSize(int x, int y, int min_width, int min_height);
	void FindWindowPlacementAndResize(int def_width, int def_height);
	void FindWindowPlacementAndResize(const WindowDesc *desc);

public:
	Window(int x, int y, int width, int height, WindowClass cls, const Widget *widget);
	Window(const WindowDesc *desc, WindowNumber number = 0);
	Window();

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

	ViewportData *viewport;          ///< Pointer to viewport data, if present.
	Widget *widget;                  ///< Widgets of the window.
	uint widget_count;               ///< Number of widgets of the window.
	uint32 desc_flags;               ///< Window/widgets default flags setting. @see WindowDefaultFlag
	const Widget *focused_widget;    ///< Currently focused widget, or \c NULL if no widget has focus.
	const NWidgetCore *nested_focus; ///< Currently focused nested widget, or \c NULL if no nested widget has focus.
	NWidgetBase *nested_root;        ///< Root of the nested tree.
	NWidgetCore **nested_array;      ///< Array of pointers into the tree.
	uint nested_array_size;          ///< Size of the nested array.

	Window *parent;                  ///< Parent window.
	Window *z_front;                 ///< The window in front of us in z-order.
	Window *z_back;                  ///< The window behind us in z-order.

	void InitNested(const WindowDesc *desc, WindowNumber number = 0);
	void CreateNestedTree(const WindowDesc *desc, bool fill_nested = true);
	void FinishInitNested(const WindowDesc *desc, WindowNumber window_number);

	/**
	 * Sets the enabled/disabled status of a widget.
	 * By default, widgets are enabled.
	 * On certain conditions, they have to be disabled.
	 * @param widget_index index of this widget in the window
	 * @param disab_stat status to use ie: disabled = true, enabled = false
	 */
	inline void SetWidgetDisabledState(byte widget_index, bool disab_stat)
	{
		if (this->widget != NULL) {
			assert(widget_index < this->widget_count);
			SB(this->widget[widget_index].display_flags, WIDG_DISABLED, 1, !!disab_stat);
		}
		if (this->nested_array != NULL) {
			assert(widget_index < this->nested_array_size);
			if (this->nested_array[widget_index] != NULL) this->nested_array[widget_index]->SetDisabled(disab_stat);
		}
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
		if (this->nested_array != NULL) {
			assert(widget_index < this->nested_array_size);
			return this->nested_array[widget_index]->IsDisabled();
		}
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
	 * Set focus within this window to the given widget. The function however doesn't change which window has focus.
	 * @param widget_index Index of the widget in the window to set the focus to.
	 * @return Focus has changed.
	 */
	inline bool SetFocusedWidget(byte widget_index)
	{
		if (widget_index >= this->widget_count || this->widget + widget_index == this->focused_widget) {
			return false;
		}

		if (this->focused_widget != NULL) {
			/* Repaint the widget that lost focus. A focused edit box may else leave the caret on the screen. */
			this->InvalidateWidget(this->focused_widget - this->widget);
		}
		this->focused_widget = &this->widget[widget_index];
		return true;
	}

	/**
	 * Check if given widget is focused within this window
	 * @param widget_index : index of the widget in the window to check
	 * @return true if given widget is the focused window in this window
	 */
	inline bool IsWidgetFocused(byte widget_index) const
	{
		return (this->widget != NULL && this->focused_widget == &this->widget[widget_index]) ||
			(this->nested_focus != NULL && this->nested_focus->index == widget_index);
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
		if (this->widget != NULL) {
			assert(widget_index < this->widget_count);
			SB(this->widget[widget_index].display_flags, WIDG_LOWERED, 1, !!lowered_stat);
		}
		if (this->nested_array != NULL) {
			assert(widget_index < this->nested_array_size);
			this->nested_array[widget_index]->SetLowered(lowered_stat);
		}
	}

	/**
	 * Invert the lowered/raised  status of a widget.
	 * @param widget_index index of this widget in the window
	 */
	inline void ToggleWidgetLoweredState(byte widget_index)
	{
		if (this->widget != NULL) {
			assert(widget_index < this->widget_count);
			ToggleBit(this->widget[widget_index].display_flags, WIDG_LOWERED);
		}
		if (this->nested_array != NULL) {
			assert(widget_index < this->nested_array_size);
			bool lowered_state = this->nested_array[widget_index]->IsLowered();
			this->nested_array[widget_index]->SetLowered(!lowered_state);
		}
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
		if (this->nested_array != NULL) {
			assert(widget_index < this->nested_array_size);
			return this->nested_array[widget_index]->IsLowered();
		}
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
	const Widget *GetWidgetOfType(WidgetType widget_type) const;

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
	void ReInit();

	/*** Event handling ***/

	/**
	 * The window must be repainted.
	 * @note This method should not change any state, it should only use drawing functions.
	 */
	virtual void OnPaint() {}

	/**
	 * Draw the contents of a nested widget.
	 * @param r      Rectangle occupied by the widget.
	 * @param widget Number of the widget to draw.
	 * @note This method may not change any state, it may only use drawing functions.
	 */
	virtual void DrawWidget(const Rect &r, int widget) const {}

	/**
	 * Update size and resize step of a widget in the window.
	 * After retrieval of the minimal size and the resize-steps of a widget, this function is called to allow further refinement,
	 * typically by computing the real maximal size of the content. Afterwards, \a size is taken to be the minimal size of the widget
	 * and \a resize is taken to contain the resize steps. For the convenience of the callee, \a padding contains the amount of
	 * padding between the content and the edge of the widget. This should be added to the returned size.
	 * @param widget  Widget number.
	 * @param size    Size of the widget.
	 * @param padding Recommended amount of space between the widget content and the widget edge.
	 * @param resize  Resize step of the widget.
	 */
	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize) {}

	/**
	 * Initialize string parameters for a widget.
	 * Calls to this function are made during initialization to measure the size (that is as part of #InitNested()), during drawing,
	 * and while re-initializing the window. Only for widgets that render text initializing is requested.
	 * @param widget  Widget number.
	 */
	virtual void SetStringParameters(int widget) const {}

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
	 * Called after the window got resized.
	 * For nested windows with a viewport, call NWidgetViewport::UpdateViewportCoordinates.
	 * @param delta The amount of which the window size changed.
	 */
	virtual void OnResize(Point delta) {}

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
 * Base class for windows opened from a toolbar.
 */
class PickerWindowBase : public Window {

public:
	PickerWindowBase(const WindowDesc *desc, Window *parent, WindowNumber number = 0) : Window(desc, number)
	{
		this->parent = parent;
	};

	PickerWindowBase(Window *parent) : Window()
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
	WF_SCROLL2           = 1 <<  7,
	WF_HSCROLL           = 1 <<  8,
	WF_SIZING_RIGHT      = 1 <<  9, ///< Window is being resized towards the right.
	WF_SIZING_LEFT       = 1 << 10, ///< Window is being resized towards the left.
	WF_SIZING            = WF_SIZING_RIGHT | WF_SIZING_LEFT, ///< Window is being resized.
	WF_STICKY            = 1 << 11, ///< Window is made sticky by user

	WF_DISABLE_VP_SCROLL = 1 << 12, ///< Window does not do autoscroll, @see HandleAutoscroll()

	WF_WHITE_BORDER_ONE  = 1 << 13,
	WF_WHITE_BORDER_MASK = 1 << 14 | WF_WHITE_BORDER_ONE,
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
bool EditBoxInGlobalFocus();

void ScrollbarClickHandler(Window *w, const Widget *wi, int x, int y);
void ScrollbarClickHandler(Window *w, const NWidgetCore *nw, int x, int y);

void ResizeButtons(Window *w, byte left, byte right);

void ResizeWindowForWidget(Window *w, uint widget, int delta_x, int delta_y);

void SetVScrollCount(Window *w, int num);
void SetVScroll2Count(Window *w, int num);
void SetHScrollCount(Window *w, int num);

#endif /* WINDOW_GUI_H */
