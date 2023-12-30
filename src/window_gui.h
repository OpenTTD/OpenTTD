/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file window_gui.h Functions, definitions and such used only by the GUI. */

#ifndef WINDOW_GUI_H
#define WINDOW_GUI_H

#include "vehiclelist.h"
#include "vehicle_type.h"
#include "viewport_type.h"
#include "company_type.h"
#include "tile_type.h"
#include "widget_type.h"
#include "string_type.h"

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

struct WidgetDimensions {
	RectPadding imgbtn;
	RectPadding inset;
	RectPadding vscrollbar;
	RectPadding hscrollbar;
	RectPadding bevel;        ///< Widths of bevel border.
	RectPadding fullbevel;    ///< Always-scaled bevel border.
	RectPadding framerect;    ///< Offsets within frame area.
	RectPadding frametext;    ///< Offsets within a text frame area.
	RectPadding matrix;       ///< Offsets within a matrix cell.
	RectPadding shadebox;
	RectPadding stickybox;
	RectPadding debugbox;
	RectPadding defsizebox;
	RectPadding resizebox;
	RectPadding closebox;
	RectPadding captiontext;  ///< Offsets of text within a caption.
	RectPadding dropdowntext; ///< Offsets of text within a dropdown widget.
	RectPadding dropdownlist; ///< Offsets used by a dropdown list itself.
	RectPadding modalpopup;   ///< Padding for a modal popup.
	RectPadding picker;       ///< Padding for a picker (dock, station, etc) window.
	RectPadding sparse;       ///< Padding used for 'sparse' widget window, usually containing multiple frames.
	RectPadding sparse_resize; ///< Padding used for a resizeable 'sparse' widget window, usually containing multiple frames.

	int vsep_picker;          ///< Vertical spacing of picker-window widgets.
	int vsep_normal;          ///< Normal vertical spacing.
	int vsep_sparse;          ///< Normal vertical spacing for 'sparse' widget window.
	int vsep_wide;            ///< Wide vertical spacing.
	int hsep_normal;          ///< Normal horizontal spacing.
	int hsep_wide;            ///< Wide horizontal spacing.
	int hsep_indent;          ///< Width of identation for tree layouts.

	static const WidgetDimensions unscaled; ///< Unscaled widget dimensions.
	static WidgetDimensions scaled;         ///< Widget dimensions scaled for current zoom level.
};

/* widget.cpp */
void DrawFrameRect(int left, int top, int right, int bottom, Colours colour, FrameFlags flags);

static inline void DrawFrameRect(const Rect &r, Colours colour, FrameFlags flags)
{
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, flags);
}

void DrawCaption(const Rect &r, Colours colour, Owner owner, TextColour text_colour, StringID str, StringAlignment align, FontSize fs);

/* window.cpp */
using WindowList = std::list<Window *>;
extern WindowList _z_windows;
extern Window *_focused_window;


/** How do we the window to be placed? */
enum WindowPosition {
	WDP_MANUAL,        ///< Manually align the window (so no automatic location finding)
	WDP_AUTO,          ///< Find a place automatically
	WDP_CENTER,        ///< Center the window
	WDP_ALIGN_TOOLBAR, ///< Align toward the toolbar
};

Point GetToolbarAlignedWindowPosition(int window_width);

struct HotkeyList;

/**
 * High level window description
 */
struct WindowDesc : ZeroedMemoryAllocator {

	WindowDesc(const char * const file, const int line, WindowPosition default_pos, const char *ini_key, int16_t def_width_trad, int16_t def_height_trad,
			WindowClass window_class, WindowClass parent_class, uint32_t flags,
			const NWidgetPart *nwid_begin, const NWidgetPart *nwid_end, HotkeyList *hotkeys = nullptr);

	~WindowDesc();

	const char * const file; ///< Source file of this definition
	const int line; ///< Source line of this definition
	WindowPosition default_pos;    ///< Preferred position of the window. @see WindowPosition()
	WindowClass cls;               ///< Class of the window, @see WindowClass.
	WindowClass parent_cls;        ///< Class of the parent window. @see WindowClass
	const char *ini_key;           ///< Key to store window defaults in openttd.cfg. \c nullptr if nothing shall be stored.
	uint32_t flags;                  ///< Flags. @see WindowDefaultFlag
	const NWidgetPart *nwid_begin; ///< Beginning of nested widget parts describing the window.
	const NWidgetPart *nwid_end; ///< Ending of nested widget parts describing the window.
	HotkeyList *hotkeys;           ///< Hotkeys for the window.

	bool pref_sticky;              ///< Preferred stickyness.
	int16_t pref_width;              ///< User-preferred width of the window. Zero if unset.
	int16_t pref_height;             ///< User-preferred height of the window. Zero if unset.

	int16_t GetDefaultWidth() const;
	int16_t GetDefaultHeight() const;

	static void LoadFromConfig();
	static void SaveToConfig();

private:
	int16_t default_width_trad;      ///< Preferred initial width of the window (pixels at 1x zoom).
	int16_t default_height_trad;     ///< Preferred initial height of the window (pixels at 1x zoom).

	/**
	 * Dummy private copy constructor to prevent compilers from
	 * copying the structure, which fails due to _window_descs.
	 */
	WindowDesc(const WindowDesc &other);
};

/**
 * Window default widget/window handling flags
 */
enum WindowDefaultFlag {
	WDF_CONSTRUCTION    =   1 << 0, ///< This window is used for construction; close it whenever changing company.
	WDF_MODAL           =   1 << 1, ///< The window is a modal child of some other window, meaning the parent is 'inactive'
	WDF_NO_FOCUS        =   1 << 2, ///< This window won't get focus/make any other window lose focus when click
	WDF_NO_CLOSE        =   1 << 3, ///< This window can't be interactively closed
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
 * Window flags.
 */
enum WindowFlags {
	WF_TIMEOUT           = 1 <<  0, ///< Window timeout counter.

	WF_DRAGGING          = 1 <<  3, ///< Window is being dragged.
	WF_SIZING_RIGHT      = 1 <<  4, ///< Window is being resized towards the right.
	WF_SIZING_LEFT       = 1 <<  5, ///< Window is being resized towards the left.
	WF_SIZING            = WF_SIZING_RIGHT | WF_SIZING_LEFT, ///< Window is being resized.
	WF_STICKY            = 1 <<  6, ///< Window is made sticky by user
	WF_DISABLE_VP_SCROLL = 1 <<  7, ///< Window does not do autoscroll, @see HandleAutoscroll().
	WF_WHITE_BORDER      = 1 <<  8, ///< Window white border counter bit mask.
	WF_HIGHLIGHTED       = 1 <<  9, ///< Window has a widget that has a highlight.
	WF_CENTERED          = 1 << 10, ///< Window is centered and shall stay centered after ReInit.
};
DECLARE_ENUM_AS_BIT_SET(WindowFlags)

static const int TIMEOUT_DURATION = 7; ///< The initial timeout value for WF_TIMEOUT.
static const int WHITE_BORDER_DURATION = 3; ///< The initial timeout value for WF_WHITE_BORDER.

/**
 * Data structure for a window viewport.
 * A viewport is either following a vehicle (its id in then in #follow_vehicle), or it aims to display a specific
 * location #dest_scrollpos_x, #dest_scrollpos_y (#follow_vehicle is then #INVALID_VEHICLE).
 * The actual location being shown is #scrollpos_x, #scrollpos_y.
 * @see InitializeViewport(), UpdateViewportPosition(), UpdateViewportCoordinates().
 */
struct ViewportData : Viewport {
	VehicleID follow_vehicle; ///< VehicleID to follow if following a vehicle, #INVALID_VEHICLE otherwise.
	int32_t scrollpos_x;        ///< Currently shown x coordinate (virtual screen coordinate of topleft corner of the viewport).
	int32_t scrollpos_y;        ///< Currently shown y coordinate (virtual screen coordinate of topleft corner of the viewport).
	int32_t dest_scrollpos_x;   ///< Current destination x coordinate to display (virtual screen coordinate of topleft corner of the viewport).
	int32_t dest_scrollpos_y;   ///< Current destination y coordinate to display (virtual screen coordinate of topleft corner of the viewport).
};

struct QueryString;

/* misc_gui.cpp */
enum TooltipCloseCondition {
	TCC_RIGHT_CLICK,
	TCC_HOVER,
	TCC_NONE,
	TCC_EXIT_VIEWPORT,
};

/**
 * Data structure for an opened window
 */
struct Window : ZeroedMemoryAllocator {
private:
	static std::vector<Window *> closed_windows;

protected:
	void InitializeData(WindowNumber window_number);
	void InitializePositionSize(int x, int y, int min_width, int min_height);
	virtual void FindWindowPlacementAndResize(int def_width, int def_height);

	std::vector<int> scheduled_invalidation_data;  ///< Data of scheduled OnInvalidateData() calls.

	/* Protected to prevent deletion anywhere outside Window::DeleteClosedWindows(). */
	virtual ~Window();

public:
	Window(WindowDesc *desc);

	/**
	 * Helper allocation function to disallow something.
	 * Don't allow arrays; arrays of Windows are pointless as you need
	 * to destruct them all at the same time too, which is kinda hard.
	 * @param size the amount of space not to allocate
	 */
	inline void *operator new[](size_t size) = delete;

	WindowDesc *window_desc;    ///< Window description
	WindowFlags flags;          ///< Window flags
	WindowClass window_class;   ///< Window class
	WindowNumber window_number; ///< Window number within the window class

	int scale; ///< Scale of this window -- used to determine how to resize.

	uint8_t timeout_timer;      ///< Timer value of the WF_TIMEOUT for flags.
	uint8_t white_border_timer; ///< Timer value of the WF_WHITE_BORDER for flags.

	int left;   ///< x position of left edge of the window
	int top;    ///< y position of top edge of the window
	int width;  ///< width of the window (number of pixels to the right in x direction)
	int height; ///< Height of the window (number of pixels down in y direction)

	ResizeInfo resize;  ///< Resize information

	Owner owner;        ///< The owner of the content shown in this window. Company colour is acquired from this variable.

	ViewportData *viewport;          ///< Pointer to viewport data, if present.
	const NWidgetCore *nested_focus; ///< Currently focused nested widget, or \c nullptr if no nested widget has focus.
	std::map<int, QueryString*> querystrings; ///< QueryString associated to WWT_EDITBOX widgets.
	std::unique_ptr<NWidgetBase> nested_root; ///< Root of the nested tree.
	WidgetLookup widget_lookup; ///< Indexed access to the nested widget tree. Do not access directly, use #Window::GetWidget() instead.
	NWidgetStacked *shade_select;    ///< Selection widget (#NWID_SELECTION) to use for shading the window. If \c nullptr, window cannot shade.
	Dimension unshaded_size;         ///< Last known unshaded size (only valid while shaded).

	WidgetID mouse_capture_widget;   ///< ID of current mouse capture widget (e.g. dragged scrollbar). -1 if no widget has mouse capture.

	Window *parent;                  ///< Parent window.
	WindowList::iterator z_position;

	template <class NWID>
	inline const NWID *GetWidget(WidgetID widnum) const;
	template <class NWID>
	inline NWID *GetWidget(WidgetID widnum);

	const Scrollbar *GetScrollbar(WidgetID widnum) const;
	Scrollbar *GetScrollbar(WidgetID widnum);

	const QueryString *GetQueryString(WidgetID widnum) const;
	QueryString *GetQueryString(WidgetID widnum);
	void UpdateQueryStringSize();

	virtual const struct Textbuf *GetFocusedTextbuf() const;
	virtual Point GetCaretPosition() const;
	virtual Rect GetTextBoundingRect(const char *from, const char *to) const;
	virtual ptrdiff_t GetTextCharacterAtPosition(const Point &pt) const;

	void InitNested(WindowNumber number = 0);
	void CreateNestedTree();
	void FinishInitNested(WindowNumber window_number = 0);

	template<typename T, std::enable_if_t<std::is_base_of<StrongTypedefBase, T>::value, int> = 0>
	void FinishInitNested(T number)
	{
		this->FinishInitNested(number.base());
	}

	/**
	 * Set the timeout flag of the window and initiate the timer.
	 */
	inline void SetTimeout()
	{
		this->flags |= WF_TIMEOUT;
		this->timeout_timer = TIMEOUT_DURATION;
	}

	/**
	 * Set the timeout flag of the window and initiate the timer.
	 */
	inline void SetWhiteBorder()
	{
		this->flags |= WF_WHITE_BORDER;
		this->white_border_timer = WHITE_BORDER_DURATION;
	}

	void DisableAllWidgetHighlight();
	void SetWidgetHighlight(WidgetID widget_index, TextColour highlighted_colour);
	bool IsWidgetHighlighted(WidgetID widget_index) const;

	/**
	 * Sets the enabled/disabled status of a widget.
	 * By default, widgets are enabled.
	 * On certain conditions, they have to be disabled.
	 * @param widget_index index of this widget in the window
	 * @param disab_stat status to use ie: disabled = true, enabled = false
	 */
	inline void SetWidgetDisabledState(WidgetID widget_index, bool disab_stat)
	{
		NWidgetCore *nwid = this->GetWidget<NWidgetCore>(widget_index);
		if (nwid != nullptr) nwid->SetDisabled(disab_stat);
	}

	/**
	 * Sets a widget to disabled.
	 * @param widget_index index of this widget in the window
	 */
	inline void DisableWidget(WidgetID widget_index)
	{
		SetWidgetDisabledState(widget_index, true);
	}

	/**
	 * Sets a widget to Enabled.
	 * @param widget_index index of this widget in the window
	 */
	inline void EnableWidget(WidgetID widget_index)
	{
		SetWidgetDisabledState(widget_index, false);
	}

	/**
	 * Gets the enabled/disabled status of a widget.
	 * @param widget_index index of this widget in the window
	 * @return status of the widget ie: disabled = true, enabled = false
	 */
	inline bool IsWidgetDisabled(WidgetID widget_index) const
	{
		return this->GetWidget<NWidgetCore>(widget_index)->IsDisabled();
	}

	/**
	 * Check if given widget is focused within this window
	 * @param widget_index : index of the widget in the window to check
	 * @return true if given widget is the focused window in this window
	 */
	inline bool IsWidgetFocused(WidgetID widget_index) const
	{
		return this->nested_focus != nullptr && this->nested_focus->index == widget_index;
	}

	/**
	 * Check if given widget has user input focus. This means that both the window
	 * has focus and that the given widget has focus within the window.
	 * @param widget_index : index of the widget in the window to check
	 * @return true if given widget is the focused window in this window and this window has focus
	 */
	inline bool IsWidgetGloballyFocused(WidgetID widget_index) const
	{
		return _focused_window == this && IsWidgetFocused(widget_index);
	}

	/**
	 * Sets the lowered/raised status of a widget.
	 * @param widget_index index of this widget in the window
	 * @param lowered_stat status to use ie: lowered = true, raised = false
	 */
	inline void SetWidgetLoweredState(WidgetID widget_index, bool lowered_stat)
	{
		this->GetWidget<NWidgetCore>(widget_index)->SetLowered(lowered_stat);
	}

	/**
	 * Invert the lowered/raised  status of a widget.
	 * @param widget_index index of this widget in the window
	 */
	inline void ToggleWidgetLoweredState(WidgetID widget_index)
	{
		bool lowered_state = this->GetWidget<NWidgetCore>(widget_index)->IsLowered();
		this->GetWidget<NWidgetCore>(widget_index)->SetLowered(!lowered_state);
	}

	/**
	 * Marks a widget as lowered.
	 * @param widget_index index of this widget in the window
	 */
	inline void LowerWidget(WidgetID widget_index)
	{
		SetWidgetLoweredState(widget_index, true);
	}

	/**
	 * Marks a widget as raised.
	 * @param widget_index index of this widget in the window
	 */
	inline void RaiseWidget(WidgetID widget_index)
	{
		SetWidgetLoweredState(widget_index, false);
	}

	/**
	 * Marks a widget as raised and dirty (redraw), when it is marked as lowered.
	 * @param widget_index index of this widget in the window
	 */
	inline void RaiseWidgetWhenLowered(byte widget_index)
	{
		if (this->IsWidgetLowered(widget_index)) {
			this->RaiseWidget(widget_index);
			this->SetWidgetDirty(widget_index);
		}
	}

	/**
	 * Gets the lowered state of a widget.
	 * @param widget_index index of this widget in the window
	 * @return status of the widget ie: lowered = true, raised= false
	 */
	inline bool IsWidgetLowered(WidgetID widget_index) const
	{
		return this->GetWidget<NWidgetCore>(widget_index)->IsLowered();
	}

	void UnfocusFocusedWidget();
	bool SetFocusedWidget(WidgetID widget_index);

	EventState HandleEditBoxKey(WidgetID wid, char32_t key, uint16_t keycode);
	virtual void InsertTextString(WidgetID wid, const char *str, bool marked, const char *caret, const char *insert_location, const char *replacement_end);

	void HandleButtonClick(WidgetID widget);
	int GetRowFromWidget(int clickpos, WidgetID widget, int padding, int line_height = -1) const;

	void RaiseButtons(bool autoraise = false);

	/**
	 * Sets the enabled/disabled status of a list of widgets.
	 * By default, widgets are enabled.
	 * On certain conditions, they have to be disabled.
	 * @param disab_stat status to use ie: disabled = true, enabled = false
	 * @param widgets list of widgets
	 */
	template<typename... Args>
	void SetWidgetsDisabledState(bool disab_stat, Args... widgets)
	{
		(SetWidgetDisabledState(widgets, disab_stat), ...);
	}

	/**
	 * Sets the lowered/raised status of a list of widgets.
	 * @param lowered_stat status to use ie: lowered = true, raised = false
	 * @param widgets list of widgets
	 */
	template<typename... Args>
	void SetWidgetsLoweredState(bool lowered_stat, Args... widgets)
	{
		(SetWidgetLoweredState(widgets, lowered_stat), ...);
	}

	/**
	 * Raises the widgets and sets widgets dirty that are lowered.
	 * @param widgets list of widgets
	 */
	template<typename... Args>
	void RaiseWidgetsWhenLowered(Args... widgets)
	{
		(this->RaiseWidgetWhenLowered(widgets), ...);
	}

	void SetWidgetDirty(WidgetID widget_index) const;

	void DrawWidgets() const;
	void DrawViewport() const;
	void DrawSortButtonState(WidgetID widget, SortButtonState state) const;
	static int SortButtonWidth();

	void CloseChildWindows(WindowClass wc = WC_INVALID) const;
	virtual void Close(int data = 0);
	static void DeleteClosedWindows();

	void SetDirty() const;
	void ReInit(int rx = 0, int ry = 0, bool reposition = false);

	/** Is window shaded currently? */
	inline bool IsShaded() const
	{
		return this->shade_select != nullptr && this->shade_select->shown_plane == SZSP_HORIZONTAL;
	}

	void SetShaded(bool make_shaded);

	void InvalidateData(int data = 0, bool gui_scope = true);
	void ProcessScheduledInvalidations();
	void ProcessHighlightedInvalidations();

	/*** Event handling ***/

	/**
	 * Notification that the nested widget tree gets initialized. The event can be used to perform general computations.
	 * @note #nested_root and/or #widget_lookup (normally accessed via #GetWidget()) may not exist during this call.
	 */
	virtual void OnInit() { }

	virtual void ApplyDefaults();

	/**
	 * Compute the initial position of the window.
	 * @param sm_width      Smallest width of the window.
	 * @param sm_height     Smallest height of the window.
	 * @param window_number The window number of the new window.
	 * @return Initial position of the top-left corner of the window.
	 */
	virtual Point OnInitialPosition(int16_t sm_width, int16_t sm_height, int window_number);

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
	virtual void DrawWidget([[maybe_unused]] const Rect &r, [[maybe_unused]] WidgetID widget) const {}

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
	virtual void UpdateWidgetSize([[maybe_unused]] WidgetID widget, [[maybe_unused]] Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) {}

	/**
	 * Initialize string parameters for a widget.
	 * Calls to this function are made during initialization to measure the size (that is as part of #InitNested()), during drawing,
	 * and while re-initializing the window. Only for widgets that render text initializing is requested.
	 * @param widget  Widget number.
	 */
	virtual void SetStringParameters([[maybe_unused]] WidgetID widget) const {}

	/**
	 * The window has gained focus.
	 */
	virtual void OnFocus();

	/**
	 * The window has lost focus.
	 * @param closing True iff the window has lost focus in the process of closing.
	 */
	virtual void OnFocusLost(bool closing);

	/**
	 * A key has been pressed.
	 * @param key     the Unicode value of the key.
	 * @param keycode the untranslated key code including shift state.
	 * @return #ES_HANDLED if the key press has been handled and no other
	 *         window should receive the event.
	 */
	virtual EventState OnKeyPress([[maybe_unused]] char32_t key, [[maybe_unused]] uint16_t keycode) { return ES_NOT_HANDLED; }

	virtual EventState OnHotkey(int hotkey);

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
	virtual void OnClick([[maybe_unused]] Point pt, [[maybe_unused]] WidgetID widget, [[maybe_unused]] int click_count) {}

	/**
	 * A click with the right mouse button has been made on the window.
	 * @param pt     the point inside the window that has been clicked.
	 * @param widget the clicked widget.
	 * @return true if the click was actually handled, i.e. do not show a
	 *         tooltip if tooltip-on-right-click is enabled.
	 */
	virtual bool OnRightClick([[maybe_unused]] Point pt, [[maybe_unused]] WidgetID widget) { return false; }

	/**
	 * The mouse is hovering over a widget in the window, perform an action for it.
	 * @param pt     The point where the mouse is hovering.
	 * @param widget The widget where the mouse is hovering.
	 */
	virtual void OnHover([[maybe_unused]] Point pt, [[maybe_unused]] WidgetID widget) {}

	/**
	 * Event to display a custom tooltip.
	 * @param pt     The point where the mouse is located.
	 * @param widget The widget where the mouse is located.
	 * @return True if the event is handled, false if it is ignored.
	 */
	virtual bool OnTooltip([[maybe_unused]] Point pt, [[maybe_unused]] WidgetID widget, [[maybe_unused]] TooltipCloseCondition close_cond) { return false; }

	/**
	 * An 'object' is being dragged at the provided position, highlight the target if possible.
	 * @param pt     The point inside the window that the mouse hovers over.
	 * @param widget The widget the mouse hovers over.
	 */
	virtual void OnMouseDrag([[maybe_unused]] Point pt, [[maybe_unused]] WidgetID widget) {}

	/**
	 * A dragged 'object' has been released.
	 * @param pt     the point inside the window where the release took place.
	 * @param widget the widget where the release took place.
	 */
	virtual void OnDragDrop([[maybe_unused]] Point pt, [[maybe_unused]] WidgetID widget) {}

	/**
	 * Handle the request for (viewport) scrolling.
	 * @param delta the amount the viewport must be scrolled.
	 */
	virtual void OnScroll([[maybe_unused]] Point delta) {}

	/**
	 * The mouse is currently moving over the window or has just moved outside
	 * of the window. In the latter case pt is (-1, -1).
	 * @param pt     the point inside the window that the mouse hovers over.
	 * @param widget the widget the mouse hovers over.
	 */
	virtual void OnMouseOver([[maybe_unused]] Point pt, [[maybe_unused]] WidgetID widget) {}

	/**
	 * The mouse wheel has been turned.
	 * @param wheel the amount of movement of the mouse wheel.
	 */
	virtual void OnMouseWheel([[maybe_unused]] int wheel) {}


	/**
	 * Called for every mouse loop run, which is at least once per (game) tick.
	 */
	virtual void OnMouseLoop() {}

	/**
	 * Called once per (game) tick.
	 */
	virtual void OnGameTick() {}

	/**
	 * Called periodically.
	 */
	virtual void OnRealtimeTick([[maybe_unused]] uint delta_ms) {}

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
	virtual void OnDropdownSelect([[maybe_unused]] WidgetID widget, [[maybe_unused]] int index) {}

	virtual void OnDropdownClose(Point pt, WidgetID widget, int index, bool instant_close);

	/**
	 * The text in an editbox has been edited.
	 * @param widget The widget of the editbox.
	 */
	virtual void OnEditboxChanged([[maybe_unused]] WidgetID widget) {}

	/**
	 * The query window opened from this window has closed.
	 * @param str the new value of the string, nullptr if the window
	 *            was cancelled or an empty string when the default
	 *            button was pressed, i.e. StrEmpty(str).
	 */
	virtual void OnQueryTextFinished([[maybe_unused]] char *str) {}

	/**
	 * Some data on this window has become invalid.
	 * @param data information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) {}

	/**
	 * The user clicked some place on the map when a tile highlight mode
	 * has been set.
	 * @param pt   the exact point on the map that has been clicked.
	 * @param tile the tile on the map that has been clicked.
	 */
	virtual void OnPlaceObject([[maybe_unused]] Point pt, [[maybe_unused]] TileIndex tile) {}

	/**
	 * The user clicked on a vehicle while HT_VEHICLE has been set.
	 * @param v clicked vehicle
	 * @return true if the click is handled, false if it is ignored
	 * @pre v->IsPrimaryVehicle() == true
	 */
	virtual bool OnVehicleSelect([[maybe_unused]] const struct Vehicle *v) { return false; }

	/**
	 * The user clicked on a vehicle while HT_VEHICLE has been set.
	 * @param v clicked vehicle
	 * @return True if the click is handled, false if it is ignored
	 * @pre v->IsPrimaryVehicle() == true
	 */
	virtual bool OnVehicleSelect([[maybe_unused]] VehicleList::const_iterator begin, [[maybe_unused]] VehicleList::const_iterator end) { return false; }

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
	virtual void OnPlaceDrag([[maybe_unused]] ViewportPlaceMethod select_method, [[maybe_unused]] ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt) {}

	/**
	 * The user has dragged over the map when the tile highlight mode
	 * has been set.
	 * @param select_method the method of selection (allowed directions)
	 * @param select_proc   what should be created.
	 * @param pt            the exact point on the map where the mouse was released.
	 * @param start_tile    the begin tile of the drag.
	 * @param end_tile      the end tile of the drag.
	 */
	virtual void OnPlaceMouseUp([[maybe_unused]] ViewportPlaceMethod select_method, [[maybe_unused]] ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt, [[maybe_unused]] TileIndex start_tile, [[maybe_unused]] TileIndex end_tile) {}

	/**
	 * The user moves over the map when a tile highlight mode has been set
	 * when the special mouse mode has been set to 'PRESIZE' mode. An
	 * example of this is the tile highlight for dock building.
	 * @param pt   the exact point on the map where the mouse is.
	 * @param tile the tile on the map where the mouse is.
	 */
	virtual void OnPlacePresize([[maybe_unused]] Point pt, [[maybe_unused]] TileIndex tile) {}

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

	/**
	 * Iterator to iterate all valid Windows
	 * @tparam TtoBack whether we iterate towards the back.
	 */
	template <bool TtoBack>
	struct WindowIterator {
		typedef Window *value_type;
		typedef value_type *pointer;
		typedef value_type &reference;
		typedef size_t difference_type;
		typedef std::forward_iterator_tag iterator_category;

		explicit WindowIterator(WindowList::iterator start) : it(start)
		{
			this->Validate();
		}
		explicit WindowIterator(const Window *w) : it(w->z_position) {}

		bool operator==(const WindowIterator &other) const { return this->it == other.it; }
		bool operator!=(const WindowIterator &other) const { return !(*this == other); }
		Window * operator*() const { return *this->it; }
		WindowIterator & operator++() { this->Next(); this->Validate(); return *this; }

		bool IsEnd() const { return this->it == _z_windows.end(); }

	private:
		WindowList::iterator it;
		void Validate()
		{
			while (!this->IsEnd() && *this->it == nullptr) this->Next();
		}
		void Next()
		{
			if constexpr (!TtoBack) {
				++this->it;
			} else if (this->it == _z_windows.begin()) {
				this->it = _z_windows.end();
			} else {
				--this->it;
			}
		}
	};
	using IteratorToFront = WindowIterator<false>; //!< Iterate in Z order towards front.
	using IteratorToBack = WindowIterator<true>; //!< Iterate in Z order towards back.

	/**
	 * Iterable ensemble of all valid Windows
	 * @tparam Tfront Wether we iterate from front
	 */
	template <bool Tfront>
	struct AllWindows {
		AllWindows() {}
		WindowIterator<Tfront> begin()
		{
			if constexpr (Tfront) {
				auto back = _z_windows.end();
				if (back != _z_windows.begin()) --back;
				return WindowIterator<Tfront>(back);
			} else {
				return WindowIterator<Tfront>(_z_windows.begin());
			}
		}
		WindowIterator<Tfront> end() { return WindowIterator<Tfront>(_z_windows.end()); }
	};
	using Iterate = AllWindows<false>; //!< Iterate all windows in whatever order is easiest.
	using IterateFromBack = AllWindows<false>; //!< Iterate all windows in Z order from back to front.
	using IterateFromFront = AllWindows<true>; //!< Iterate all windows in Z order from front to back.
};

/**
 * Generic helper function that checks if all elements of the range are equal with respect to the given predicate.
 * @param begin The start of the range.
 * @param end The end of the range.
 * @param pred The predicate to use.
 * @return True if all elements are equal, false otherwise.
 */
template <class It, class Pred>
inline bool AllEqual(It begin, It end, Pred pred)
{
	return std::adjacent_find(begin, end, std::not_fn(pred)) == end;
}

/**
 * Get the nested widget with number \a widnum from the nested widget tree.
 * @tparam NWID Type of the nested widget.
 * @param widnum Widget number of the widget to retrieve.
 * @return The requested widget if it is instantiated, \c nullptr otherwise.
 */
template <class NWID>
inline NWID *Window::GetWidget(WidgetID widnum)
{
	auto it = this->widget_lookup.find(widnum);
	if (it == std::end(this->widget_lookup)) return nullptr;
	NWID *nwid = dynamic_cast<NWID *>(it->second);
	assert(nwid != nullptr);
	return nwid;
}

/** Specialized case of #Window::GetWidget for the nested widget base class. */
template <>
inline const NWidgetBase *Window::GetWidget<NWidgetBase>(WidgetID widnum) const
{
	auto it = this->widget_lookup.find(widnum);
	if (it == std::end(this->widget_lookup)) return nullptr;
	return it->second;
}

/**
 * Get the nested widget with number \a widnum from the nested widget tree.
 * @tparam NWID Type of the nested widget.
 * @param widnum Widget number of the widget to retrieve.
 * @return The requested widget if it is instantiated, \c nullptr otherwise.
 */
template <class NWID>
inline const NWID *Window::GetWidget(WidgetID widnum) const
{
	return const_cast<Window *>(this)->GetWidget<NWID>(widnum);
}


/**
 * Base class for windows opened from a toolbar.
 */
class PickerWindowBase : public Window {

public:
	PickerWindowBase(WindowDesc *desc, Window *parent) : Window(desc)
	{
		this->parent = parent;
	}

	void Close([[maybe_unused]] int data = 0) override;
};

Window *BringWindowToFrontById(WindowClass cls, WindowNumber number);
Window *FindWindowFromPt(int x, int y);

template<typename T, std::enable_if_t<std::is_base_of<StrongTypedefBase, T>::value, int> = 0>
Window *BringWindowToFrontById(WindowClass cls, T number)
{
	return BringWindowToFrontById(cls, number.base());
}

/**
 * Open a new window.
 * @tparam Wcls %Window class to use if the window does not exist.
 * @param desc The pointer to the WindowDesc to be created
 * @param window_number the window number of the new window
 * @param return_existing If set, also return the window if it already existed.
 * @return %Window pointer of the newly created window, or the existing one if \a return_existing is set, or \c nullptr.
 */
template <typename Wcls>
Wcls *AllocateWindowDescFront(WindowDesc *desc, int window_number, bool return_existing = false)
{
	Wcls *w = static_cast<Wcls *>(BringWindowToFrontById(desc->cls, window_number));
	if (w != nullptr) return return_existing ? w : nullptr;
	return new Wcls(desc, window_number);
}

void RelocateAllWindows(int neww, int newh);

void GuiShowTooltips(Window *parent, StringID str, TooltipCloseCondition close_tooltip, uint paramcount = 0);

/* widget.cpp */
WidgetID GetWidgetFromPos(const Window *w, int x, int y);

extern Point _cursorpos_drag_start;

extern int _scrollbar_start_pos;
extern int _scrollbar_size;
extern byte _scroller_click_timeout;

extern bool _scrolling_viewport;
extern bool _mouse_hovering;

/** Mouse modes. */
enum SpecialMouseMode {
	WSM_NONE,     ///< No special mouse mode.
	WSM_DRAGDROP, ///< Drag&drop an object.
	WSM_SIZING,   ///< Sizing mode.
	WSM_PRESIZE,  ///< Presizing mode (docks, tunnels).
	WSM_DRAGGING, ///< Dragging mode (trees).
};
extern SpecialMouseMode _special_mouse_mode;

void SetFocusedWindow(Window *w);

void ScrollbarClickHandler(Window *w, NWidgetCore *nw, int x, int y);

#endif /* WINDOW_GUI_H */
