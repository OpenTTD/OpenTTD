/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file window_gui.h Functions, definitions and such used only by the GUI. */

#ifndef WINDOW_GUI_H
#define WINDOW_GUI_H

#include "vehicle_type.h"
#include "viewport_type.h"
#include "company_type.h"
#include "tile_type.h"
#include "widget_type.h"

/** State of handling an event. */
enum EventState {
	ES_HANDLED,     ///< The passed event is handled.
	ES_NOT_HANDLED, ///< The passed event is not handled.
};

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

DECLARE_ENUM_AS_BIT_SET(FrameFlags)

/** Distances used in drawing widgets. */
enum WidgetDrawDistances {
	/* WWT_IMGBTN(_2) */
	WD_IMGBTN_LEFT    = 1,      ///< Left offset of the image in the button.
	WD_IMGBTN_RIGHT   = 2,      ///< Right offset of the image in the button.
	WD_IMGBTN_TOP     = 1,      ///< Top offset of image in the button.
	WD_IMGBTN_BOTTOM  = 2,      ///< Bottom offset of image in the button.

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

	/* Extra space at top/bottom of text panels */
	WD_TEXTPANEL_TOP    = 6,    ///< Offset at top to draw above the text
	WD_TEXTPANEL_BOTTOM = 6,    ///< Offset at bottom to draw below the text

	/* WWT_FRAME */
	WD_FRAMETEXT_LEFT   = 6,    ///< Left offset of the text of the frame.
	WD_FRAMETEXT_RIGHT  = 6,    ///< Right offset of the text of the frame.
	WD_FRAMETEXT_TOP    = 6,    ///< Top offset of the text of the frame
	WD_FRAMETEXT_BOTTOM = 6,    ///< Bottom offset of the text of the frame

	/* WWT_MATRIX */
	WD_MATRIX_LEFT   = 2,       ///< Offset at left of a matrix cell.
	WD_MATRIX_RIGHT  = 2,       ///< Offset at right of a matrix cell.
	WD_MATRIX_TOP    = 3,       ///< Offset at top of a matrix cell.
	WD_MATRIX_BOTTOM = 1,       ///< Offset at bottom of a matrix cell.

	/* WWT_SHADEBOX */
	WD_SHADEBOX_WIDTH  = 12,    ///< Width of a standard shade box widget.
	WD_SHADEBOX_LEFT   = 2,     ///< Left offset of shade sprite.
	WD_SHADEBOX_RIGHT  = 2,     ///< Right offset of shade sprite.
	WD_SHADEBOX_TOP    = 3,     ///< Top offset of shade sprite.
	WD_SHADEBOX_BOTTOM = 3,     ///< Bottom offset of shade sprite.

	/* WWT_STICKYBOX */
	WD_STICKYBOX_WIDTH  = 12,   ///< Width of a standard sticky box widget.
	WD_STICKYBOX_LEFT   = 2,    ///< Left offset of sticky sprite.
	WD_STICKYBOX_RIGHT  = 2,    ///< Right offset of sticky sprite.
	WD_STICKYBOX_TOP    = 3,    ///< Top offset of sticky sprite.
	WD_STICKYBOX_BOTTOM = 3,    ///< Bottom offset of sticky sprite.

	/* WWT_DEBUGBOX */
	WD_DEBUGBOX_WIDTH  = 12,    ///< Width of a standard debug box widget.
	WD_DEBUGBOX_LEFT   = 2,     ///< Left offset of debug sprite.
	WD_DEBUGBOX_RIGHT  = 2,     ///< Right offset of debug sprite.
	WD_DEBUGBOX_TOP    = 3,     ///< Top offset of debug sprite.
	WD_DEBUGBOX_BOTTOM = 3,     ///< Bottom offset of debug sprite.

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

	WD_PAR_VSEP_NORMAL = 2,      ///< Normal amount of vertical space between two paragraphs of text.
	WD_PAR_VSEP_WIDE   = 8,      ///< Large amount of vertical space between two paragraphs of text.
};

/* widget.cpp */
void DrawFrameRect(int left, int top, int right, int bottom, Colours colour, FrameFlags flags);
void DrawCaption(const Rect &r, Colours colour, Owner owner, StringID str);

/* window.cpp */
extern Window *_z_front_window;
extern Window *_z_back_window;
extern Window *_focused_window;


/** How do we the window to be placed? */
enum WindowPosition {
	WDP_MANUAL,        ///< Manually align the window (so no automatic location finding)
	WDP_AUTO,          ///< Find a place automatically
	WDP_CENTER,        ///< Center the window
	WDP_ALIGN_TOOLBAR, ///< Align toward the toolbar
};

Point GetToolbarAlignedWindowPosition(int window_width);

/**
 * High level window description
 */
struct WindowDesc : ZeroedMemoryAllocator {

	WindowDesc(WindowPosition default_pos, int16 def_width, int16 def_height,
			WindowClass window_class, WindowClass parent_class, uint32 flags,
			const NWidgetPart *nwid_parts, int16 nwid_length);

	~WindowDesc();

	WindowPosition default_pos;    ///< Prefered position of the window. @see WindowPosition()
	int16 default_width;           ///< Prefered initial width of the window.
	int16 default_height;          ///< Prefered initial height of the window.
	WindowClass cls;               ///< Class of the window, @see WindowClass.
	WindowClass parent_cls;        ///< Class of the parent window. @see WindowClass
	uint32 flags;                  ///< Flags. @see WindowDefaultFlags
	const NWidgetPart *nwid_parts; ///< Nested widget parts describing the window.
	int16 nwid_length;             ///< Length of the #nwid_parts array.
};

/**
 * Window default widget/window handling flags
 */
enum WindowDefaultFlag {
	WDF_CONSTRUCTION    =   1 << 0, ///< This window is used for construction; close it whenever changing company.
	WDF_UNCLICK_BUTTONS =   1 << 1, ///< Unclick buttons when the window event times out
	WDF_MODAL           =   1 << 2, ///< The window is a modal child of some other window, meaning the parent is 'inactive'
	WDF_NO_FOCUS        =   1 << 3, ///< This window won't get focus/make any other window lose focus when click
};

/**
 * Data structure for resizing a window
 */
struct ResizeInfo {
	uint step_width;  ///< Step-size of width resize changes
	uint step_height; ///< Step-size of height resize changes
};

/** State of a sort direction button. */
enum SortButtonState {
	SBS_OFF,  ///< Do not sort (with this button).
	SBS_DOWN, ///< Sort ascending.
	SBS_UP,   ///< Sort descending.
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
protected:
	void InitializeData(const WindowDesc *desc, WindowNumber window_number);
	void InitializePositionSize(int x, int y, int min_width, int min_height);
	void FindWindowPlacementAndResize(int def_width, int def_height);

public:
	Window();

	virtual ~Window();

	/**
	 * Helper allocation function to disallow something.
	 * Don't allow arrays; arrays of Windows are pointless as you need
	 * to destruct them all at the same time too, which is kinda hard.
	 * @param size the amount of space not to allocate
	 */
	FORCEINLINE void *operator new[](size_t size)
	{
		NOT_REACHED();
	}

	/**
	 * Helper allocation function to disallow something.
	 * Don't free the window directly; it corrupts the linked list when iterating
	 * @param ptr the pointer not to free
	 */
	FORCEINLINE void operator delete(void *ptr)
	{
	}

	uint16 flags4;              ///< Window flags, @see WindowFlags
	WindowClass window_class;   ///< Window class
	WindowNumber window_number; ///< Window number within the window class

	int left;   ///< x position of left edge of the window
	int top;    ///< y position of top edge of the window
	int width;  ///< width of the window (number of pixels to the right in x direction)
	int height; ///< Height of the window (number of pixels down in y direction)

	ResizeInfo resize;  ///< Resize information

	Owner owner;        ///< The owner of the content shown in this window. Company colour is acquired from this variable.

	ViewportData *viewport;          ///< Pointer to viewport data, if present.
	uint32 desc_flags;               ///< Window/widgets default flags setting. @see WindowDefaultFlag
	const NWidgetCore *nested_focus; ///< Currently focused nested widget, or \c NULL if no nested widget has focus.
	NWidgetBase *nested_root;        ///< Root of the nested tree.
	NWidgetBase **nested_array;      ///< Array of pointers into the tree. Do not access directly, use #Window::GetWidget() instead.
	uint nested_array_size;          ///< Size of the nested array.
	NWidgetStacked *shade_select;    ///< Selection widget (#NWID_SELECTION) to use for shading the window. If \c NULL, window cannot shade.
	Dimension unshaded_size;         ///< Last known unshaded size (only valid while shaded).

	int scrolling_scrollbar;         ///< Widgetindex of just being dragged scrollbar. -1 of none is active.

	Window *parent;                  ///< Parent window.
	Window *z_front;                 ///< The window in front of us in z-order.
	Window *z_back;                  ///< The window behind us in z-order.

	template <class NWID>
	inline const NWID *GetWidget(uint widnum) const;
	template <class NWID>
	inline NWID *GetWidget(uint widnum);

	const Scrollbar *GetScrollbar(uint widnum) const;
	Scrollbar *GetScrollbar(uint widnum);

	void InitNested(const WindowDesc *desc, WindowNumber number = 0);
	void CreateNestedTree(const WindowDesc *desc, bool fill_nested = true);
	void FinishInitNested(const WindowDesc *desc, WindowNumber window_number = 0);

	/**
	 * Sets the enabled/disabled status of a widget.
	 * By default, widgets are enabled.
	 * On certain conditions, they have to be disabled.
	 * @param widget_index index of this widget in the window
	 * @param disab_stat status to use ie: disabled = true, enabled = false
	 */
	inline void SetWidgetDisabledState(byte widget_index, bool disab_stat)
	{
		assert(widget_index < this->nested_array_size);
		if (this->nested_array[widget_index] != NULL) this->GetWidget<NWidgetCore>(widget_index)->SetDisabled(disab_stat);
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
		assert(widget_index < this->nested_array_size);
		return this->GetWidget<NWidgetCore>(widget_index)->IsDisabled();
	}

	/**
	 * Check if given widget is focused within this window
	 * @param widget_index : index of the widget in the window to check
	 * @return true if given widget is the focused window in this window
	 */
	inline bool IsWidgetFocused(byte widget_index) const
	{
		return this->nested_focus != NULL && this->nested_focus->index == widget_index;
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
		assert(widget_index < this->nested_array_size);
		this->GetWidget<NWidgetCore>(widget_index)->SetLowered(lowered_stat);
	}

	/**
	 * Invert the lowered/raised  status of a widget.
	 * @param widget_index index of this widget in the window
	 */
	inline void ToggleWidgetLoweredState(byte widget_index)
	{
		assert(widget_index < this->nested_array_size);
		bool lowered_state = this->GetWidget<NWidgetCore>(widget_index)->IsLowered();
		this->GetWidget<NWidgetCore>(widget_index)->SetLowered(!lowered_state);
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
		assert(widget_index < this->nested_array_size);
		return this->GetWidget<NWidgetCore>(widget_index)->IsLowered();
	}

	void UnfocusFocusedWidget();
	bool SetFocusedWidget(byte widget_index);

	void HandleButtonClick(byte widget);
	int GetRowFromWidget(int clickpos, int widget, int padding, int line_height = -1) const;

	void RaiseButtons(bool autoraise = false);
	void CDECL SetWidgetsDisabledState(bool disab_stat, int widgets, ...);
	void CDECL SetWidgetsLoweredState(bool lowered_stat, int widgets, ...);
	void SetWidgetDirty(byte widget_index) const;

	void DrawWidgets() const;
	void DrawViewport() const;
	void DrawSortButtonState(int widget, SortButtonState state) const;

	void DeleteChildWindows(WindowClass wc = WC_INVALID) const;

	void SetDirty() const;
	void ReInit(int rx = 0, int ry = 0);

	/** Is window shaded currently? */
	inline bool IsShaded() const
	{
		return this->shade_select != NULL && this->shade_select->shown_plane == SZSP_HORIZONTAL;
	}

	void SetShaded(bool make_shaded);

	/**
	 * Mark this window's data as invalid (in need of re-computing)
	 * @param data The data to invalidate with
	 */
	void InvalidateData(int data = 0)
	{
		this->SetDirty();
		this->OnInvalidateData(data);
	}

	/*** Event handling ***/

	/**
	 * Notification that the nested widget tree gets initialized. The event can be used to perform general computations.
	 * @note #nested_root and/or #nested_array (normally accessed via #GetWidget()) may not exist during this call.
	 */
	virtual void OnInit() { }

	/**
	 * Compute the initial position of the window.
	 * @param *desc         The pointer to the WindowDesc of the window to create.
	 * @param sm_width      Smallest width of the window.
	 * @param sm_height     Smallest height of the window.
	 * @param window_number The window number of the new window.
	 * @return Initial position of the top-left corner of the window.
	 */
	virtual Point OnInitialPosition(const WindowDesc *desc, int16 sm_width, int16 sm_height, int window_number);

	/**
	 * The window must be repainted.
	 * @note This method should not change any state, it should only use drawing functions.
	 */
	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

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
	 * @param fill    Fill step of the widget.
	 * @param resize  Resize step of the widget.
	 */
	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) {}

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
	 * @return #ES_HANDLED if the key press has been handled and no other
	 *         window should receive the event.
	 */
	virtual EventState OnKeyPress(uint16 key, uint16 keycode) { return ES_NOT_HANDLED; }

	/**
	 * The state of the control key has changed
	 * @return #ES_HANDLED if the change has been handled and no other
	 *         window should receive the event.
	 */
	virtual EventState OnCTRLStateChange() { return ES_NOT_HANDLED; }


	/**
	 * A click with the left mouse button has been made on the window.
	 * @param pt     the point inside the window that has been clicked.
	 * @param widget the clicked widget.
	 * @param click_count Number of fast consecutive clicks at same position
	 */
	virtual void OnClick(Point pt, int widget, int click_count) {}

	/**
	 * A click with the right mouse button has been made on the window.
	 * @param pt     the point inside the window that has been clicked.
	 * @param widget the clicked widget.
	 * @return true if the click was actually handled, i.e. do not show a
	 *         tooltip if tooltip-on-right-click is enabled.
	 */
	virtual bool OnRightClick(Point pt, int widget) { return false; }

	/**
	 * The mouse is hovering over a widget in the window, perform an action for it, like opening a custom tooltip.
	 * @param pt     The point where the mouse is hovering.
	 * @param widget The widget where the mouse is hovering.
	 */
	virtual void OnHover(Point pt, int widget) {}

	/**
	 * An 'object' is being dragged at the provided position, highlight the target if possible.
	 * @param pt     The point inside the window that the mouse hovers over.
	 * @param widget The widget the mouse hovers over.
	 */
	virtual void OnMouseDrag(Point pt, int widget) {}

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
	 */
	virtual void OnResize() {}

	/**
	 * A dropdown option associated to this window has been selected.
	 * @param widget the widget (button) that the dropdown is associated with.
	 * @param index  the element in the dropdown that is selected.
	 */
	virtual void OnDropdownSelect(int widget, int index) {}

	/**
	 * The query window opened from this window has closed.
	 * @param str the new value of the string, NULL if the window
	 *            was cancelled or an empty string when the default
	 *            button was pressed, i.e. StrEmpty(str).
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
	 * The user clicked on a vehicle while HT_VEHICLE has been set.
	 * @param v clicked vehicle. It is guaranteed to be v->IsPrimaryVehicle() == true
	 */
	virtual void OnVehicleSelect(const struct Vehicle *v) {}

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

	/**
	 * Is the data related to this window NewGRF inspectable?
	 * @return true iff it is inspectable.
	 */
	virtual bool IsNewGRFInspectable() const { return false; }

	/**
	 * Show the NewGRF inspection window. When this function is called it is
	 * up to the window to call and pass the right parameters to the
	 * ShowInspectWindow function.
	 * @pre this->IsNewGRFInspectable()
	 */
	virtual void ShowNewGRFInspectWindow() const { NOT_REACHED(); }
};

/**
 * Get the nested widget with number \a widnum from the nested widget tree.
 * @tparam NWID Type of the nested widget.
 * @param widnum Widget number of the widget to retrieve.
 * @return The requested widget if it is instantiated, \c NULL otherwise.
 */
template <class NWID>
inline NWID *Window::GetWidget(uint widnum)
{
	if (widnum >= this->nested_array_size || this->nested_array[widnum] == NULL) return NULL;
	NWID *nwid = dynamic_cast<NWID *>(this->nested_array[widnum]);
	assert(nwid != NULL);
	return nwid;
}

/** Specialized case of #Window::GetWidget for the nested widget base class. */
template <>
inline const NWidgetBase *Window::GetWidget<NWidgetBase>(uint widnum) const
{
	if (widnum >= this->nested_array_size) return NULL;
	return this->nested_array[widnum];
}

/**
 * Get the nested widget with number \a widnum from the nested widget tree.
 * @tparam NWID Type of the nested widget.
 * @param widnum Widget number of the widget to retrieve.
 * @return The requested widget if it is instantiated, \c NULL otherwise.
 */
template <class NWID>
inline const NWID *Window::GetWidget(uint widnum) const
{
	return const_cast<Window *>(this)->GetWidget<NWID>(widnum);
}


/**
 * Base class for windows opened from a toolbar.
 */
class PickerWindowBase : public Window {

public:
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
	WF_SIZING_RIGHT      = 1 <<  4, ///< Window is being resized towards the right.
	WF_SIZING_LEFT       = 1 <<  5, ///< Window is being resized towards the left.
	WF_SIZING            = WF_SIZING_RIGHT | WF_SIZING_LEFT, ///< Window is being resized.
	WF_STICKY            = 1 <<  6, ///< Window is made sticky by user

	WF_DISABLE_VP_SCROLL = 1 <<  7, ///< Window does not do autoscroll, @see HandleAutoscroll()

	WF_WHITE_BORDER_ONE  = 1 <<  8,
	WF_WHITE_BORDER_MASK = 1 <<  9 | WF_WHITE_BORDER_ONE,

	WF_CENTERED          = 1 << 10, ///< Window is centered and shall stay centered after ReInit
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
enum TooltipCloseCondition {
	TCC_RIGHT_CLICK,
	TCC_LEFT_CLICK,
	TCC_HOVER,
};

void GuiShowTooltips(Window *parent, StringID str, uint paramcount = 0, const uint64 params[] = NULL, TooltipCloseCondition close_tooltip = TCC_HOVER);

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

extern bool _scrolling_viewport;
extern bool _mouse_hovering;

/** Mouse modes. */
enum SpecialMouseMode {
	WSM_NONE,     ///< No special mouse mode.
	WSM_DRAGDROP, ///< Dragging an object.
	WSM_SIZING,   ///< Sizing mode.
	WSM_PRESIZE,  ///< Presizing mode (docks, tunnels).
};
extern SpecialMouseMode _special_mouse_mode;

void SetFocusedWindow(Window *w);

void ScrollbarClickHandler(Window *w, NWidgetCore *nw, int x, int y);

#endif /* WINDOW_GUI_H */
