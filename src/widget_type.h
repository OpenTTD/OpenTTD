/* $Id$ */

/** @file widget_type.h Definitions about widgets. */

#ifndef WIDGET_TYPE_H
#define WIDGET_TYPE_H

#include "core/bitmath_func.hpp"
#include "strings_type.h"
#include "gfx_type.h"

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

enum DisplayFlags {
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
DECLARE_ENUM_AS_BIT_SET(DisplayFlags);

enum {
	WIDGET_LIST_END = -1, ///< indicate the end of widgets' list for vararg functions
};

/**
 * Window widget types, nested widget types, and nested widget part types.
 */
enum WidgetType {
	/* Window widget types. */
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
	WWT_DROPDOWN,   ///< Drop down list
	WWT_EDITBOX,    ///< a textbox for typing
	WWT_LAST,       ///< Last Item. use WIDGETS_END to fill up padding!!

	/* Nested widget types. */
	NWID_HORIZONTAL,  ///< Horizontal container.
	NWID_VERTICAL,    ///< Vertical container.
	NWID_SPACER,      ///< Invisible widget that takes some space.

	/* Nested widget part types. */
	WPT_RESIZE,       ///< Widget part for specifying resizing.
	WPT_RESIZE_PTR,   ///< Widget part for specifying resizing via a pointer.
	WPT_MINSIZE,      ///< Widget part for specifying minimal size.
	WPT_MINSIZE_PTR,  ///< Widget part for specifying minimal size via a pointer.
	WPT_FILL,         ///< Widget part for specifying fill.
	WPT_DATATIP,      ///< Widget part for specifying data and tooltip.
	WPT_DATATIP_PTR,  ///< Widget part for specifying data and tooltip via a pointer.
	WPT_ENDCONTAINER, ///< Widget part to denote end of a container.

	/* Pushable window widget types. */
	WWT_MASK = 0x7F,

	WWB_PUSHBUTTON  = 1 << 7,

	WWT_PUSHBTN     = WWT_PANEL   | WWB_PUSHBUTTON,
	WWT_PUSHTXTBTN  = WWT_TEXTBTN | WWB_PUSHBUTTON,
	WWT_PUSHIMGBTN  = WWT_IMGBTN  | WWB_PUSHBUTTON,
};

/** Marker for the "end of widgets" in a Window(Desc) widget table. */
#define WIDGETS_END WWT_LAST, RESIZE_NONE, INVALID_COLOUR, 0, 0, 0, 0, 0, STR_NULL

/**
 * Window widget data structure
 */
struct Widget {
	WidgetType type;                  ///< Widget type
	DisplayFlags display_flags;       ///< Resize direction, alignment, etc. during resizing
	Colours colour;                   ///< Widget colour, see docs/ottd-colourtext-palette.png
	int16 left;                       ///< The left edge of the widget
	int16 right;                      ///< The right edge of the widget
	int16 top;                        ///< The top edge of the widget
	int16 bottom;                     ///< The bottom edge of the widget
	uint16 data;                      ///< The String/Image or special code (list-matrixes) of a widget
	StringID tooltips;                ///< Tooltips that are shown when rightclicking on a widget
};

/**
 * Baseclass for nested widgets.
 */
class NWidgetBase : public ZeroedMemoryAllocator {
public:
	NWidgetBase(WidgetType tp);

	virtual int ComputeMinimalSize() = 0;
	virtual void AssignMinimalPosition(uint x, uint y, uint given_width, uint given_height, bool allow_resize_x, bool allow_resize_y, bool rtl) = 0;

	virtual void StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl) = 0;

	WidgetType type;   ///< Type of the widget / nested widget.
	uint min_x;        ///< Minimal horizontal size.
	uint min_y;        ///< Minimal vertical size.
	bool fill_x;       ///< Allow horizontal filling from initial size.
	bool fill_y;       ///< Allow vertical filling from initial size.
	uint resize_x;     ///< Horizontal resize step (\c 0 means not resizable).
	uint resize_y;     ///< Vertical resize step (\c 0 means not resizable).

	uint pos_x;        ///< Horizontal position of top-left corner of the widget in the window.
	uint pos_y;        ///< Vertical position of top-left corner of the widget in the window.

	NWidgetBase *next; ///< Pointer to next widget in container. Managed by parent container widget.
	NWidgetBase *prev; ///< Pointer to previous widget in container. Managed by parent container widget.
};

/** Base class for a resizable nested widget. */
class NWidgetResizeBase : public NWidgetBase {
public:
	NWidgetResizeBase(WidgetType tp, bool fill_x, bool fill_y);

	void SetMinimalSize(uint min_x, uint min_y);
	void SetFill(bool fill_x, bool fill_y);
	void SetResize(uint resize_x, uint resize_y);

	void AssignMinimalPosition(uint x, uint y, uint given_width, uint given_height, bool allow_resize_x, bool allow_resize_y, bool rtl);
};

/** Base class for a 'real' widget. */
class NWidgetCore : public NWidgetResizeBase {
public:
	NWidgetCore(WidgetType tp, Colours colour, bool def_fill_x, bool def_fill_y, uint16 widget_data, StringID tool_tip);

	void SetIndex(int index);
	void SetDataTip(uint16 widget_data, StringID tool_tip);

	int ComputeMinimalSize();
	void StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl);

	Colours colour;     ///< Colour of this widget.
	int index;          ///< Index of the nested widget in the widget array of the window (\c -1 means 'not used').
	uint16 widget_data; ///< Data of the widget. @see Widget::data
	StringID tool_tip;  ///< Tooltip of the widget. @see Widget::tootips
};

/** Baseclass for container widgets. */
class NWidgetContainer : public NWidgetBase {
public:
	NWidgetContainer(WidgetType tp);
	~NWidgetContainer();

	void Add(NWidgetBase *wid);
protected:
	NWidgetBase *head; ///< Pointer to first widget in container.
	NWidgetBase *tail; ///< Pointer to last widget in container.
};

/** Horizontal container. */
class NWidgetHorizontal : public NWidgetContainer {
public:
	NWidgetHorizontal();

	int ComputeMinimalSize();
	void AssignMinimalPosition(uint x, uint y, uint given_width, uint given_height, bool allow_resize_x, bool allow_resize_y, bool rtl);

	void StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl);
};

/** Vertical container */
class NWidgetVertical : public NWidgetContainer {
public:
	NWidgetVertical();

	int ComputeMinimalSize();
	void AssignMinimalPosition(uint x, uint y, uint given_width, uint given_height, bool allow_resize_x, bool allow_resize_y, bool rtl);

	void StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl);
};


/** Spacer widget */
class NWidgetSpacer : public NWidgetResizeBase {
public:
	NWidgetSpacer(int length, int height);

	int ComputeMinimalSize();
	void StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl);
};

/** Nested widget with a child. */
class NWidgetBackground : public NWidgetCore {
public:
	NWidgetBackground(WidgetType tp, Colours colour, int index, NWidgetContainer *child = NULL);
	~NWidgetBackground();

	void Add(NWidgetBase *nwid);

	int ComputeMinimalSize();
	void AssignMinimalPosition(uint x, uint y, uint given_width, uint given_height, bool allow_resize_x, bool allow_resize_y, bool rtl);

	void StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl);
private:
	NWidgetContainer *child; ///< Child widget.
};

/** Leaf widget. */
class NWidgetLeaf : public NWidgetCore {
public:
	NWidgetLeaf(WidgetType tp, Colours colour, int index, uint16 data, StringID tip);
};

Widget *InitializeNWidgets(NWidgetBase *nwid, bool rtl = false);
bool CompareWidgetArrays(const Widget *orig, const Widget *gen, bool report = true);

/* == Nested widget parts == */

/** Widget part for storing data and tooltip information. */
struct NWidgetPartDataTip {
	uint16 data;      ///< Data value of the widget.
	StringID tooltip; ///< Tooltip of the widget.
};

/** Widget part for storing basic widget information. */
struct NWidgetPartWidget {
	Colours colour; ///< Widget colour.
	int16 index;    ///< Widget index in the widget array.
};

/** Partial widget specification to allow NWidgets to be written nested. */
struct NWidgetPart {
	WidgetType type;                         ///< Type of the part. @see NWidgetPartType.
	union {
		Point xy;                        ///< Part with an x/y size.
		Point *xy_ptr;                   ///< Part with a pointer to an x/y size.
		NWidgetPartDataTip data_tip;     ///< Part with a data/tooltip.
		NWidgetPartDataTip *datatip_ptr; ///< Part with a pointer to data/tooltip.
		NWidgetPartWidget widget;        ///< Part with a start of a widget.
	} u;
};

/**
 * Widget part function for setting the resize step.
 * @param dx Horizontal resize step. 0 means no horizontal resizing.
 * @param dy Vertical resize step. 0 means no horizontal resizing.
 */
static inline NWidgetPart SetResize(int16 dx, int16 dy)
{
	NWidgetPart part;

	part.type = WPT_RESIZE;
	part.u.xy.x = dx;
	part.u.xy.y = dy;

	return part;
}

/**
 * Widget part function for using a pointer to set the resize step.
 * @param ptr Pointer to horizontal and vertical resize step.
 */
static inline NWidgetPart SetResize(Point *ptr)
{
	NWidgetPart part;

	part.type = WPT_RESIZE_PTR;
	part.u.xy_ptr = ptr;

	return part;
}

/**
 * Widget part function for setting the minimal size.
 * @param dx Horizontal minimal size.
 * @param dy Vertical minimal size.
 */
static inline NWidgetPart SetMinimalSize(int16 x, int16 y)
{
	NWidgetPart part;

	part.type = WPT_MINSIZE;
	part.u.xy.x = x;
	part.u.xy.y = y;

	return part;
}

/**
 * Widget part function for using a pointer to set the minimal size.
 * @param ptr Pointer to horizontal and vertical minimal size.
 */
static inline NWidgetPart SetMinimalSize(Point *ptr)
{
	NWidgetPart part;

	part.type = WPT_MINSIZE_PTR;
	part.u.xy_ptr = ptr;

	return part;
}

/**
 * Widget part function for setting filling.
 * @param x_fill Allow horizontal filling from minimal size.
 * @param y_fill Allow vertical filling from minimal size.
 */
static inline NWidgetPart SetFill(bool x_fill, bool y_fill)
{
	NWidgetPart part;

	part.type = WPT_FILL;
	part.u.xy.x = x_fill;
	part.u.xy.y = y_fill;

	return part;
}

/**
 * Widget part function for denoting the end of a container
 * (horizontal, vertical, WWT_FRAME, WWT_INSET, or WWT_PANEL).
 */
static inline NWidgetPart EndContainer()
{
	NWidgetPart part;

	part.type = WPT_ENDCONTAINER;

	return part;
}

/** Widget part function for setting the data and tooltip.
 * @param data Data of the widget.
 * @param tip  Tooltip of the widget.
 */
static inline NWidgetPart SetDataTip(uint16 data, StringID tip)
{
	NWidgetPart part;

	part.type = WPT_DATATIP;
	part.u.data_tip.data = data;
	part.u.data_tip.tooltip = tip;

	return part;
}

/**
 * Widget part function for setting the data and tooltip via a pointer.
 * @param ptr Pointer to the data and tooltip of the widget.
 */
static inline NWidgetPart SetDataTip(NWidgetPartDataTip *ptr)
{
	NWidgetPart part;

	part.type = WPT_DATATIP_PTR;
	part.u.datatip_ptr = ptr;

	return part;
}

/**
 * Widget part function for starting a new 'real' widget.
 * @param tp  Type of the new nested widget.
 * @param col Colour of the new widget.
 * @param idx Index of the widget in the widget array.
 * @note with #WWT_PANEL, #WWT_FRAME, #WWT_INSET, a new container is started.
 *       Child widgets must have a index bigger than the parent index.
 */
static inline NWidgetPart NWidget(WidgetType tp, Colours col, int16 idx)
{
	NWidgetPart part;

	part.type = tp;
	part.u.widget.colour = col;
	part.u.widget.index = idx;

	return part;
}

/**
 * Widget part function for starting a new horizontal container, vertical container, or spacer widget.
 * @param tp Type of the new nested widget, #NWID_HORIZONTAL, #NWID_VERTICAL, or #NWID_SPACER
 */
static inline NWidgetPart NWidget(WidgetType tp)
{
	NWidgetPart part;

	part.type = tp;

	return part;
}

NWidgetContainer *MakeNWidgets(const NWidgetPart *parts, int count);

#endif /* WIDGET_TYPE_H */
