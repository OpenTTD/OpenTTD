/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file widget_type.h Definitions about widgets. */

#ifndef WIDGET_TYPE_H
#define WIDGET_TYPE_H

#include "core/alloc_type.hpp"
#include "core/bitmath_func.hpp"
#include "core/math_func.hpp"
#include "strings_type.h"
#include "gfx_type.h"
#include "window_type.h"

/** Bits of the #WWT_MATRIX widget data. */
enum MatrixWidgetValues {
	/* Number of column bits of the WWT_MATRIX widget data. */
	MAT_COL_START = 0, ///< Lowest bit of the number of columns.
	MAT_COL_BITS  = 8, ///< Number of bits for the number of columns in the matrix.

	/* Number of row bits of the WWT_MATRIX widget data. */
	MAT_ROW_START = 8, ///< Lowest bit of the number of rows.
	MAT_ROW_BITS  = 8, ///< Number of bits for the number of rows in the matrix.
};

/** Values for an arrow widget */
enum ArrowWidgetValues {
	AWV_DECREASE, ///< Arrow to the left or in case of RTL to the right
	AWV_INCREASE, ///< Arrow to the right or in case of RTL to the left
	AWV_LEFT,     ///< Force the arrow to the left
	AWV_RIGHT,    ///< Force the arrow to the right
};

/** WidgetData values for a resize box widget. */
enum ResizeWidgetValues {
	RWV_SHOW_BEVEL, ///< Bevel of resize box is shown.
	RWV_HIDE_BEVEL, ///< Bevel of resize box is hidden.
};

/**
 * Window widget types, nested widget types, and nested widget part types.
 */
enum WidgetType {
	/* Window widget types. */
	WWT_EMPTY,      ///< Empty widget, place holder to reserve space in widget tree.

	WWT_PANEL,      ///< Simple depressed panel
	WWT_INSET,      ///< Pressed (inset) panel, most commonly used as combo box _text_ area
	WWT_IMGBTN,     ///< (Toggle) Button with image
	WWT_IMGBTN_2,   ///< (Toggle) Button with diff image when clicked
	WWT_ARROWBTN,   ///< (Toggle) Button with an arrow
	WWT_TEXTBTN,    ///< (Toggle) Button with text
	WWT_TEXTBTN_2,  ///< (Toggle) Button with diff text when clicked
	WWT_LABEL,      ///< Centered label
	WWT_TEXT,       ///< Pure simple text
	WWT_MATRIX,     ///< Grid of rows and columns. @see MatrixWidgetValues
	WWT_FRAME,      ///< Frame
	WWT_CAPTION,    ///< Window caption (window title between closebox and stickybox)

	WWT_DEBUGBOX,   ///< NewGRF debug box (at top-right of a window, between WWT_CAPTION and WWT_SHADEBOX)
	WWT_SHADEBOX,   ///< Shade box (at top-right of a window, between WWT_DEBUGBOX and WWT_DEFSIZEBOX)
	WWT_DEFSIZEBOX, ///< Default window size box (at top-right of a window, between WWT_SHADEBOX and WWT_STICKYBOX)
	WWT_STICKYBOX,  ///< Sticky box (at top-right of a window, after WWT_DEFSIZEBOX)

	WWT_RESIZEBOX,  ///< Resize box (normally at bottom-right of a window)
	WWT_CLOSEBOX,   ///< Close box (at top-left of a window)
	WWT_DROPDOWN,   ///< Drop down list
	WWT_EDITBOX,    ///< a textbox for typing
	WWT_LAST,       ///< Last Item. use WIDGETS_END to fill up padding!!

	/* Nested widget types. */
	NWID_HORIZONTAL,      ///< Horizontal container.
	NWID_HORIZONTAL_LTR,  ///< Horizontal container that doesn't change the order of the widgets for RTL languages.
	NWID_VERTICAL,        ///< Vertical container.
	NWID_MATRIX,          ///< Matrix container.
	NWID_SPACER,          ///< Invisible widget that takes some space.
	NWID_SELECTION,       ///< Stacked widgets, only one visible at a time (eg in a panel with tabs).
	NWID_VIEWPORT,        ///< Nested widget containing a viewport.
	NWID_BUTTON_DROPDOWN, ///< Button with a drop-down.
	NWID_HSCROLLBAR,      ///< Horizontal scrollbar
	NWID_VSCROLLBAR,      ///< Vertical scrollbar
	NWID_CUSTOM,          ///< General Custom widget.

	/* Nested widget part types. */
	WPT_RESIZE,       ///< Widget part for specifying resizing.
	WPT_MINSIZE,      ///< Widget part for specifying minimal size.
	WPT_MINTEXTLINES, ///< Widget part for specifying minimal number of lines of text.
	WPT_FILL,         ///< Widget part for specifying fill.
	WPT_DATATIP,      ///< Widget part for specifying data and tooltip.
	WPT_PADDING,      ///< Widget part for specifying a padding.
	WPT_PIPSPACE,     ///< Widget part for specifying pre/inter/post space for containers.
	WPT_PIPRATIO,     ///< Widget part for specifying pre/inter/post ratio for containers.
	WPT_TEXTSTYLE,    ///< Widget part for specifying text colour.
	WPT_ALIGNMENT,    ///< Widget part for specifying text/image alignment.
	WPT_ENDCONTAINER, ///< Widget part to denote end of a container.
	WPT_FUNCTION,     ///< Widget part for calling a user function.
	WPT_SCROLLBAR,    ///< Widget part for attaching a scrollbar.

	/* Pushable window widget types. */
	WWT_MASK = 0x7F,

	WWB_PUSHBUTTON    = 1 << 7,

	WWT_PUSHBTN       = WWT_PANEL    | WWB_PUSHBUTTON,    ///< Normal push-button (no toggle button) with custom drawing
	WWT_PUSHTXTBTN    = WWT_TEXTBTN  | WWB_PUSHBUTTON,    ///< Normal push-button (no toggle button) with text caption
	WWT_PUSHIMGBTN    = WWT_IMGBTN   | WWB_PUSHBUTTON,    ///< Normal push-button (no toggle button) with image caption
	WWT_PUSHARROWBTN  = WWT_ARROWBTN | WWB_PUSHBUTTON,    ///< Normal push-button (no toggle button) with arrow caption
	NWID_PUSHBUTTON_DROPDOWN = NWID_BUTTON_DROPDOWN | WWB_PUSHBUTTON,
};

/** Different forms of sizing nested widgets, using NWidgetBase::AssignSizePosition() */
enum SizingType {
	ST_SMALLEST, ///< Initialize nested widget tree to smallest size. Also updates \e current_x and \e current_y.
	ST_RESIZE,   ///< Resize the nested widget tree.
};

/* Forward declarations. */
class NWidgetCore;
class Scrollbar;

/** Lookup between widget IDs and NWidget objects. */
using WidgetLookup = std::map<WidgetID, class NWidgetBase *>;

/**
 * Baseclass for nested widgets.
 * @invariant After initialization, \f$current\_x = smallest\_x + n * resize\_x, for n \geq 0\f$.
 * @invariant After initialization, \f$current\_y = smallest\_y + m * resize\_y, for m \geq 0\f$.
 * @ingroup NestedWidgets
 */
class NWidgetBase : public ZeroedMemoryAllocator {
public:
	NWidgetBase(WidgetType tp);

	virtual void AdjustPaddingForZoom();
	virtual void SetupSmallestSize(Window *w) = 0;
	virtual void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) = 0;

	virtual void FillWidgetLookup(WidgetLookup &widget_lookup) = 0;

	virtual NWidgetCore *GetWidgetFromPos(int x, int y) = 0;
	virtual NWidgetBase *GetWidgetOfType(WidgetType tp);

	/**
	 * Get parent widget of type NWID.
	 * @tparam NWID Type of the nested widget.
	 * @returns Parent widget, or nullptr if no widget of the specified type is found.
	 */
	template <class NWID>
	NWID *GetParentWidget()
	{
		for (NWidgetBase *nwid_parent = this->parent; nwid_parent != nullptr; nwid_parent = nwid_parent->parent) {
			if (NWID *nwid = dynamic_cast<NWID *>(nwid_parent); nwid != nullptr) return nwid;
		}
		return nullptr;
	}

	/**
	 * Get parent widget of type NWID.
	 * @tparam NWID Type of the nested widget.
	 * @returns Parent widget, or nullptr if no widget of the specified type is found.
	 */
	template <class NWID>
	const NWID *GetParentWidget() const
	{
		for (const NWidgetBase *nwid_parent = this->parent; nwid_parent != nullptr; nwid_parent = nwid_parent->parent) {
			if (const NWID *nwid = dynamic_cast<const NWID *>(nwid_parent); nwid != nullptr) return nwid;
		}
		return nullptr;
	}

	virtual bool IsHighlighted() const { return false; }
	virtual TextColour GetHighlightColour() const { return TC_INVALID; }
	virtual void SetHighlighted([[maybe_unused]] TextColour highlight_colour) {}

	/**
	 * Set additional space (padding) around the widget.
	 * @param top    Amount of additional space above the widget.
	 * @param right  Amount of additional space right of the widget.
	 * @param bottom Amount of additional space below the widget.
	 * @param left   Amount of additional space left of the widget.
	 */
	inline void SetPadding(uint8_t top, uint8_t right, uint8_t bottom, uint8_t left)
	{
		this->uz_padding.top = top;
		this->uz_padding.right = right;
		this->uz_padding.bottom = bottom;
		this->uz_padding.left = left;
		this->AdjustPaddingForZoom();
	}

	/**
	 * Set additional space (padding) around the widget.
	 * @param padding Amount of padding around the widget.
	 */
	inline void SetPadding(const RectPadding &padding)
	{
		this->uz_padding = padding;
		this->AdjustPaddingForZoom();
	}

	inline uint GetHorizontalStepSize(SizingType sizing) const;
	inline uint GetVerticalStepSize(SizingType sizing) const;

	virtual void Draw(const Window *w) = 0;
	virtual void SetDirty(const Window *w) const;

	Rect GetCurrentRect() const
	{
		Rect r;
		r.left = this->pos_x;
		r.top = this->pos_y;
		r.right = this->pos_x + this->current_x - 1;
		r.bottom = this->pos_y + this->current_y - 1;
		return r;
	}

	WidgetType type;      ///< Type of the widget / nested widget.
	uint fill_x;          ///< Horizontal fill stepsize (from initial size, \c 0 means not resizable).
	uint fill_y;          ///< Vertical fill stepsize (from initial size, \c 0 means not resizable).
	uint resize_x;        ///< Horizontal resize step (\c 0 means not resizable).
	uint resize_y;        ///< Vertical resize step (\c 0 means not resizable).
	/* Size of the widget in the smallest window possible.
	 * Computed by #SetupSmallestSize() followed by #AssignSizePosition().
	 */
	uint smallest_x;      ///< Smallest horizontal size of the widget in a filled window.
	uint smallest_y;      ///< Smallest vertical size of the widget in a filled window.
	/* Current widget size (that is, after resizing). */
	uint current_x;       ///< Current horizontal size (after resizing).
	uint current_y;       ///< Current vertical size (after resizing).

	int pos_x;            ///< Horizontal position of top-left corner of the widget in the window.
	int pos_y;            ///< Vertical position of top-left corner of the widget in the window.

	RectPadding padding;    ///< Padding added to the widget. Managed by parent container widget. (parent container may swap left and right for RTL)
	RectPadding uz_padding; ///< Unscaled padding, for resize calculation.

	NWidgetBase *parent; ///< Parent widget of this widget, automatically filled in when added to container.

protected:
	inline void StoreSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height);
};

/**
 * Get the horizontal sizing step.
 * @param sizing Type of resize being performed.
 */
inline uint NWidgetBase::GetHorizontalStepSize(SizingType sizing) const
{
	return (sizing == ST_RESIZE) ? this->resize_x : this->fill_x;
}

/**
 * Get the vertical sizing step.
 * @param sizing Type of resize being performed.
 */
inline uint NWidgetBase::GetVerticalStepSize(SizingType sizing) const
{
	return (sizing == ST_RESIZE) ? this->resize_y : this->fill_y;
}

/**
 * Store size and position.
 * @param sizing       Type of resizing to perform.
 * @param x            Horizontal offset of the widget relative to the left edge of the window.
 * @param y            Vertical offset of the widget relative to the top edge of the window.
 * @param given_width  Width allocated to the widget.
 * @param given_height Height allocated to the widget.
 */
inline void NWidgetBase::StoreSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height)
{
	this->pos_x = x;
	this->pos_y = y;
	if (sizing == ST_SMALLEST) {
		this->smallest_x = given_width;
		this->smallest_y = given_height;
	}
	this->current_x = given_width;
	this->current_y = given_height;
}


/**
 * Base class for a resizable nested widget.
 * @ingroup NestedWidgets
 */
class NWidgetResizeBase : public NWidgetBase {
public:
	NWidgetResizeBase(WidgetType tp, uint fill_x, uint fill_y);

	void AdjustPaddingForZoom() override;
	void SetMinimalSize(uint min_x, uint min_y);
	void SetMinimalSizeAbsolute(uint min_x, uint min_y);
	void SetMinimalTextLines(uint8_t min_lines, uint8_t spacing, FontSize size);
	void SetFill(uint fill_x, uint fill_y);
	void SetResize(uint resize_x, uint resize_y);

	bool UpdateMultilineWidgetSize(const std::string &str, int max_lines);
	bool UpdateSize(uint min_x, uint min_y);
	bool UpdateVerticalSize(uint min_y);

	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override;

	uint min_x; ///< Minimal horizontal size of only this widget.
	uint min_y; ///< Minimal vertical size of only this widget.

	bool absolute; ///< Set if minimum size is fixed and should not be resized.
	uint uz_min_x; ///< Unscaled Minimal horizontal size of only this widget.
	uint uz_min_y; ///< Unscaled Minimal vertical size of only this widget.

	uint8_t uz_text_lines;   ///< 'Unscaled' text lines, stored for resize calculation.
	uint8_t uz_text_spacing; ///< 'Unscaled' text padding, stored for resize calculation.
	FontSize uz_text_size; ///< 'Unscaled' font size, stored for resize calculation.
};

/** Nested widget flags that affect display and interaction with 'real' widgets. */
enum NWidgetDisplay {
	/* Generic. */
	NDB_LOWERED         = 0, ///< Widget is lowered (pressed down) bit.
	NDB_DISABLED        = 1, ///< Widget is disabled (greyed out) bit.
	/* Viewport widget. */
	NDB_NO_TRANSPARENCY = 2, ///< Viewport is never transparent.
	NDB_SHADE_GREY      = 3, ///< Shade viewport to grey-scale.
	NDB_SHADE_DIMMED    = 4, ///< Display dimmed colours in the viewport.
	/* Button dropdown widget. */
	NDB_DROPDOWN_ACTIVE = 5, ///< Dropdown menu of the button dropdown widget is active. @see #NWID_BUTTON_DROPDOWN
	/* Scrollbar widget. */
	NDB_SCROLLBAR_UP    = 6, ///< Up-button is lowered bit.
	NDB_SCROLLBAR_DOWN  = 7, ///< Down-button is lowered bit.
	/* Generic. */
	NDB_HIGHLIGHT       = 8, ///< Highlight of widget is on.
	NDB_DROPDOWN_CLOSED = 9, ///< Dropdown menu of the dropdown widget has closed.

	ND_LOWERED  = 1 << NDB_LOWERED,                ///< Bit value of the lowered flag.
	ND_DISABLED = 1 << NDB_DISABLED,               ///< Bit value of the disabled flag.
	ND_HIGHLIGHT = 1 << NDB_HIGHLIGHT,             ///< Bit value of the highlight flag.
	ND_NO_TRANSPARENCY = 1 << NDB_NO_TRANSPARENCY, ///< Bit value of the 'no transparency' flag.
	ND_SHADE_GREY      = 1 << NDB_SHADE_GREY,      ///< Bit value of the 'shade to grey' flag.
	ND_SHADE_DIMMED    = 1 << NDB_SHADE_DIMMED,    ///< Bit value of the 'dimmed colours' flag.
	ND_DROPDOWN_ACTIVE = 1 << NDB_DROPDOWN_ACTIVE, ///< Bit value of the 'dropdown active' flag.
	ND_SCROLLBAR_UP    = 1 << NDB_SCROLLBAR_UP,    ///< Bit value of the 'scrollbar up' flag.
	ND_SCROLLBAR_DOWN  = 1 << NDB_SCROLLBAR_DOWN,  ///< Bit value of the 'scrollbar down' flag.
	ND_SCROLLBAR_BTN   = ND_SCROLLBAR_UP | ND_SCROLLBAR_DOWN, ///< Bit value of the 'scrollbar up' or 'scrollbar down' flag.
	ND_DROPDOWN_CLOSED = 1 << NDB_DROPDOWN_CLOSED, ///< Bit value of the 'dropdown closed' flag.
};
DECLARE_ENUM_AS_BIT_SET(NWidgetDisplay)

/**
 * Base class for a 'real' widget.
 * @ingroup NestedWidgets
 */
class NWidgetCore : public NWidgetResizeBase {
public:
	NWidgetCore(WidgetType tp, Colours colour, WidgetID index, uint fill_x, uint fill_y, uint32_t widget_data, StringID tool_tip);

	void SetDataTip(uint32_t widget_data, StringID tool_tip);
	void SetToolTip(StringID tool_tip);
	void SetTextStyle(TextColour colour, FontSize size);
	void SetAlignment(StringAlignment align);

	inline void SetLowered(bool lowered);
	inline bool IsLowered() const;
	inline void SetDisabled(bool disabled);
	inline bool IsDisabled() const;

	void FillWidgetLookup(WidgetLookup &widget_lookup) override;
	NWidgetCore *GetWidgetFromPos(int x, int y) override;
	bool IsHighlighted() const override;
	TextColour GetHighlightColour() const override;
	void SetHighlighted(TextColour highlight_colour) override;

	NWidgetDisplay disp_flags; ///< Flags that affect display and interaction with the widget.
	Colours colour;            ///< Colour of this widget.
	const WidgetID index;      ///< Index of the nested widget (\c -1 means 'not used').
	uint32_t widget_data;        ///< Data of the widget. @see Widget::data
	StringID tool_tip;         ///< Tooltip of the widget. @see Widget::tootips
	WidgetID scrollbar_index;  ///< Index of an attached scrollbar.
	TextColour highlight_colour; ///< Colour of highlight.
	TextColour text_colour;    ///< Colour of text within widget.
	FontSize text_size;        ///< Size of text within widget.
	StringAlignment align;     ///< Alignment of text/image within widget.
};

/**
 * Highlight the widget or not.
 * @param highlight_colour Widget must be highlighted (blink).
 */
inline void NWidgetCore::SetHighlighted(TextColour highlight_colour)
{
	this->disp_flags = highlight_colour != TC_INVALID ? SETBITS(this->disp_flags, ND_HIGHLIGHT) : CLRBITS(this->disp_flags, ND_HIGHLIGHT);
	this->highlight_colour = highlight_colour;
}

/** Return whether the widget is highlighted. */
inline bool NWidgetCore::IsHighlighted() const
{
	return HasBit(this->disp_flags, NDB_HIGHLIGHT);
}

/** Return the colour of the highlight. */
inline TextColour NWidgetCore::GetHighlightColour() const
{
	return this->highlight_colour;
}

/**
 * Lower or raise the widget.
 * @param lowered Widget must be lowered (drawn pressed down).
 */
inline void NWidgetCore::SetLowered(bool lowered)
{
	this->disp_flags = lowered ? SETBITS(this->disp_flags, ND_LOWERED) : CLRBITS(this->disp_flags, ND_LOWERED);
}

/** Return whether the widget is lowered. */
inline bool NWidgetCore::IsLowered() const
{
	return HasBit(this->disp_flags, NDB_LOWERED);
}

/**
 * Disable (grey-out) or enable the widget.
 * @param disabled Widget must be disabled.
 */
inline void NWidgetCore::SetDisabled(bool disabled)
{
	this->disp_flags = disabled ? SETBITS(this->disp_flags, ND_DISABLED) : CLRBITS(this->disp_flags, ND_DISABLED);
}

/** Return whether the widget is disabled. */
inline bool NWidgetCore::IsDisabled() const
{
	return HasBit(this->disp_flags, NDB_DISABLED);
}


/**
 * Baseclass for container widgets.
 * @ingroup NestedWidgets
 */
class NWidgetContainer : public NWidgetBase {
public:
	NWidgetContainer(WidgetType tp) : NWidgetBase(tp) { }

	void AdjustPaddingForZoom() override;
	void Add(std::unique_ptr<NWidgetBase> &&wid);
	void FillWidgetLookup(WidgetLookup &widget_lookup) override;

	void Draw(const Window *w) override;
	NWidgetCore *GetWidgetFromPos(int x, int y) override;

	/** Return whether the container is empty. */
	inline bool IsEmpty() { return this->children.empty(); }

	NWidgetBase *GetWidgetOfType(WidgetType tp) override;

protected:
	std::vector<std::unique_ptr<NWidgetBase>> children; ///< Child widgets in contaier.
};

/** Display planes with zero size for #NWidgetStacked. */
enum StackedZeroSizePlanes {
	SZSP_VERTICAL = INT_MAX / 2, ///< Display plane with zero size horizontally, and filling and resizing vertically.
	SZSP_HORIZONTAL,             ///< Display plane with zero size vertically, and filling and resizing horizontally.
	SZSP_NONE,                   ///< Display plane with zero size in both directions (none filling and resizing).

	SZSP_BEGIN = SZSP_VERTICAL,  ///< First zero-size plane.
};

/**
 * Stacked widgets, widgets all occupying the same space in the window.
 * #NWID_SELECTION allows for selecting one of several panels (planes) to tbe displayed. All planes must have the same size.
 * Since all planes are also initialized, switching between different planes can be done while the window is displayed.
 *
 * There are also a number of special planes (defined in #StackedZeroSizePlanes) that have zero size in one direction (and are stretchable in
 * the other direction) or have zero size in both directions. They are used to make all child planes of the widget disappear.
 * Unlike switching between the regular display planes (that all have the same size), switching from or to one of the zero-sized planes means that
 * a #Window::ReInit() is needed to re-initialize the window since its size changes.
 */
class NWidgetStacked : public NWidgetContainer {
public:
	NWidgetStacked(WidgetID index);

	void AdjustPaddingForZoom() override;
	void SetupSmallestSize(Window *w) override;
	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override;
	void FillWidgetLookup(WidgetLookup &widget_lookup) override;

	void Draw(const Window *w) override;
	NWidgetCore *GetWidgetFromPos(int x, int y) override;

	bool SetDisplayedPlane(int plane);

	int shown_plane; ///< Plane being displayed (for #NWID_SELECTION only).
	const WidgetID index; ///< If non-negative, index in the #Window::widget_lookup.
};

/** Nested widget container flags, */
enum NWidContainerFlags {
	NCB_EQUALSIZE = 0, ///< Containers should keep all their (resizing) children equally large.
	NCB_BIGFIRST  = 1, ///< Allocate space to biggest resize first.

	NC_NONE = 0,                       ///< All flags cleared.
	NC_EQUALSIZE = 1 << NCB_EQUALSIZE, ///< Value of the #NCB_EQUALSIZE flag.
	NC_BIGFIRST  = 1 << NCB_BIGFIRST,  ///< Value of the #NCB_BIGFIRST flag.
};
DECLARE_ENUM_AS_BIT_SET(NWidContainerFlags)

/** Container with pre/inter/post child space. */
class NWidgetPIPContainer : public NWidgetContainer {
public:
	NWidgetPIPContainer(WidgetType tp, NWidContainerFlags flags = NC_NONE);

	void AdjustPaddingForZoom() override;
	void SetPIP(uint8_t pip_pre, uint8_t pip_inter, uint8_t pip_post);
	void SetPIPRatio(uint8_t pip_ratio_pre, uint8_t pip_ratio_inter, uint8_t pip_rato_post);

protected:
	NWidContainerFlags flags; ///< Flags of the container.
	uint8_t pip_pre;            ///< Amount of space before first widget.
	uint8_t pip_inter;          ///< Amount of space between widgets.
	uint8_t pip_post;           ///< Amount of space after last widget.
	uint8_t pip_ratio_pre;      ///< Ratio of remaining space before first widget.
	uint8_t pip_ratio_inter;    ///< Ratio of remaining space between widgets.
	uint8_t pip_ratio_post;     ///< Ratio of remaining space after last widget.

	uint8_t uz_pip_pre;         ///< Unscaled space before first widget.
	uint8_t uz_pip_inter;       ///< Unscaled space between widgets.
	uint8_t uz_pip_post;        ///< Unscaled space after last widget.

	uint8_t gaps; ///< Number of gaps between widgets.
};

/**
 * Horizontal container.
 * @ingroup NestedWidgets
 */
class NWidgetHorizontal : public NWidgetPIPContainer {
public:
	NWidgetHorizontal(NWidContainerFlags flags = NC_NONE);

	void SetupSmallestSize(Window *w) override;
	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override;
};

/**
 * Horizontal container that doesn't change the direction of the widgets for RTL languages.
 * @ingroup NestedWidgets
 */
class NWidgetHorizontalLTR : public NWidgetHorizontal {
public:
	NWidgetHorizontalLTR(NWidContainerFlags flags = NC_NONE);

	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override;
};

/**
 * Vertical container.
 * @ingroup NestedWidgets
 */
class NWidgetVertical : public NWidgetPIPContainer {
public:
	NWidgetVertical(NWidContainerFlags flags = NC_NONE);

	void SetupSmallestSize(Window *w) override;
	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override;
};

/**
 * Matrix container with implicitly equal sized (virtual) sub-widgets.
 * This widget must have exactly one sub-widget. After that this sub-widget
 * is used to draw all of the data within the matrix piece by piece.
 * DrawWidget and OnClick calls will be done to that sub-widget, where the
 * 16 high bits are used to encode the index into the matrix.
 * @ingroup NestedWidgets
 */
class NWidgetMatrix : public NWidgetPIPContainer {
public:
	NWidgetMatrix(Colours colour, WidgetID index);

	void SetClicked(int clicked);
	void SetCount(int count);
	void SetScrollbar(Scrollbar *sb);
	int GetCurrentElement() const;

	void SetupSmallestSize(Window *w) override;
	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override;
	void FillWidgetLookup(WidgetLookup &widget_lookup) override;

	NWidgetCore *GetWidgetFromPos(int x, int y) override;
	void Draw(const Window *w) override;
protected:
	const WidgetID index; ///< If non-negative, index in the #Window::widget_lookup.
	Colours colour; ///< Colour of this widget.
	int clicked;    ///< The currently clicked element.
	int count;      ///< Amount of valid elements.
	int current_element; ///< The element currently being processed.
	Scrollbar *sb;  ///< The scrollbar we're associated with.
private:
	int widget_w;   ///< The width of the child widget including inter spacing.
	int widget_h;   ///< The height of the child widget including inter spacing.
	int widgets_x;  ///< The number of visible widgets in horizontal direction.
	int widgets_y;  ///< The number of visible widgets in vertical direction.

	void GetScrollOffsets(int &start_x, int &start_y, int &base_offs_x, int &base_offs_y);
};


/**
 * Spacer widget.
 * @ingroup NestedWidgets
 */
class NWidgetSpacer : public NWidgetResizeBase {
public:
	NWidgetSpacer(int width, int height);

	void SetupSmallestSize(Window *w) override;
	void FillWidgetLookup(WidgetLookup &widget_lookup) override;

	void Draw(const Window *w) override;
	void SetDirty(const Window *w) const override;
	NWidgetCore *GetWidgetFromPos(int x, int y) override;
};

/**
 * Nested widget with a child.
 * @ingroup NestedWidgets
 */
class NWidgetBackground : public NWidgetCore {
public:
	NWidgetBackground(WidgetType tp, Colours colour, WidgetID index, std::unique_ptr<NWidgetPIPContainer> &&child = nullptr);

	void Add(std::unique_ptr<NWidgetBase> &&nwid);
	void SetPIP(uint8_t pip_pre, uint8_t pip_inter, uint8_t pip_post);
	void SetPIPRatio(uint8_t pip_ratio_pre, uint8_t pip_ratio_inter, uint8_t pip_ratio_post);

	void AdjustPaddingForZoom() override;
	void SetupSmallestSize(Window *w) override;
	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override;

	void FillWidgetLookup(WidgetLookup &widget_lookup) override;

	void Draw(const Window *w) override;
	NWidgetCore *GetWidgetFromPos(int x, int y) override;
	NWidgetBase *GetWidgetOfType(WidgetType tp) override;

private:
	std::unique_ptr<NWidgetPIPContainer> child; ///< Child widget.
};

/**
 * Nested widget to display a viewport in a window.
 * After initializing the nested widget tree, call #InitializeViewport(). After changing the window size,
 * call #UpdateViewportCoordinates() eg from Window::OnResize().
 * If the #disp_flags field contains the #ND_NO_TRANSPARENCY bit, the viewport will disable transparency.
 * Shading to grey-scale is controlled with the #ND_SHADE_GREY bit (used for B&W news papers), the #ND_SHADE_DIMMED gives dimmed colours (for colour news papers).
 * @todo Class derives from #NWidgetCore, but does not use #colour, #widget_data, or #tool_tip.
 * @ingroup NestedWidgets
 */
class NWidgetViewport : public NWidgetCore {
public:
	NWidgetViewport(WidgetID index);

	void SetupSmallestSize(Window *w) override;
	void Draw(const Window *w) override;

	void InitializeViewport(Window *w, std::variant<TileIndex, VehicleID> focus, ZoomLevel zoom);
	void UpdateViewportCoordinates(Window *w);
};

/**
 * Scrollbar data structure
 */
class Scrollbar {
private:
	const bool is_vertical; ///< Scrollbar has vertical orientation.
	uint16_t count;           ///< Number of elements in the list.
	uint16_t cap;             ///< Number of visible elements of the scroll bar.
	uint16_t pos;             ///< Index of first visible item of the list.
	uint16_t stepsize;        ///< Distance to scroll, when pressing the buttons or using the wheel.

public:
	/** Stepping sizes when scrolling */
	enum ScrollbarStepping {
		SS_RAW,             ///< Step in single units.
		SS_SMALL,           ///< Step in #stepsize units.
		SS_BIG,             ///< Step in #cap units.
	};

	Scrollbar(bool is_vertical) : is_vertical(is_vertical), stepsize(1)
	{
	}

	/**
	 * Gets the number of elements in the list
	 * @return the number of elements
	 */
	inline uint16_t GetCount() const
	{
		return this->count;
	}

	/**
	 * Gets the number of visible elements of the scrollbar
	 * @return the number of visible elements
	 */
	inline uint16_t GetCapacity() const
	{
		return this->cap;
	}

	/**
	 * Gets the position of the first visible element in the list
	 * @return the position of the element
	 */
	inline uint16_t GetPosition() const
	{
		return this->pos;
	}

	/**
	 * Checks whether given current item is visible in the list
	 * @param item to check
	 * @return true iff the item is visible
	 */
	inline bool IsVisible(uint16_t item) const
	{
		return IsInsideBS(item, this->GetPosition(), this->GetCapacity());
	}

	/**
	 * Is the scrollbar vertical or not?
	 * @return True iff the scrollbar is vertical.
	 */
	inline bool IsVertical() const
	{
		return this->is_vertical;
	}

	/**
	 * Set the distance to scroll when using the buttons or the wheel.
	 * @param stepsize Scrolling speed.
	 */
	void SetStepSize(size_t stepsize)
	{
		assert(stepsize > 0);

		this->stepsize = ClampTo<uint16_t>(stepsize);
	}

	/**
	 * Sets the number of elements in the list
	 * @param num the number of elements in the list
	 * @note updates the position if needed
	 */
	void SetCount(size_t num)
	{
		assert(num <= MAX_UVALUE(uint16_t));

		this->count = ClampTo<uint16_t>(num);
		/* Ensure position is within bounds */
		this->SetPosition(this->pos);
	}

	/**
	 * Set the capacity of visible elements.
	 * @param capacity the new capacity
	 * @note updates the position if needed
	 */
	void SetCapacity(size_t capacity)
	{
		assert(capacity <= MAX_UVALUE(uint16_t));

		this->cap = ClampTo<uint16_t>(capacity);
		/* Ensure position is within bounds */
		this->SetPosition(this->pos);
	}

	void SetCapacityFromWidget(Window *w, WidgetID widget, int padding = 0);

	/**
	 * Sets the position of the first visible element
	 * @param position the position of the element
	 * @return true iff the position has changed
	 */
	bool SetPosition(int position)
	{
		uint16_t old_pos = this->pos;
		this->pos = Clamp(position, 0, std::max(this->count - this->cap, 0));
		return this->pos != old_pos;
	}

	/**
	 * Updates the position of the first visible element by the given amount.
	 * If the position would be too low or high it will be clamped appropriately
	 * @param difference the amount of change requested
	 * @param unit The stepping unit of \a difference
	 * @return true iff the position has changed
	 */
	bool UpdatePosition(int difference, ScrollbarStepping unit = SS_SMALL)
	{
		if (difference == 0) return false;
		switch (unit) {
			case SS_SMALL: difference *= this->stepsize; break;
			case SS_BIG:   difference *= this->cap; break;
			default: break;
		}
		return this->SetPosition(this->pos + difference);
	}

	/**
	 * Scroll towards the given position; if the item is visible nothing
	 * happens, otherwise it will be shown either at the bottom or top of
	 * the window depending on where in the list it was.
	 * @param position the position to scroll towards.
	 */
	void ScrollTowards(int position)
	{
		if (position < this->GetPosition()) {
			/* scroll up to the item */
			this->SetPosition(position);
		} else if (position >= this->GetPosition() + this->GetCapacity()) {
			/* scroll down so that the item is at the bottom */
			this->SetPosition(position - this->GetCapacity() + 1);
		}
	}

	int GetScrolledRowFromWidget(int clickpos, const Window * const w, WidgetID widget, int padding = 0, int line_height = -1) const;

	/**
	 * Return an iterator pointing to the element of a scrolled widget that a user clicked in.
	 * @param container   Container of elements represented by the scrollbar.
	 * @param clickpos    Vertical position of the mouse click (without taking scrolling into account).
	 * @param w           The window the click was in.
	 * @param widget      Widget number of the widget clicked in.
	 * @param padding     Amount of empty space between the widget edge and the top of the first row. Default value is \c 0.
	 * @param line_height Height of a single row. A negative value means using the vertical resize step of the widget.
	 * @return Iterator to the element clicked at. If clicked at a wrong position, returns as interator to the end of the container.
	 */
	template <typename Tcontainer>
	typename Tcontainer::iterator GetScrolledItemFromWidget(Tcontainer &container, int clickpos, const Window * const w, WidgetID widget, int padding = 0, int line_height = -1) const
	{
		assert(this->GetCount() == container.size()); // Scrollbar and container size must match.
		int row = this->GetScrolledRowFromWidget(clickpos, w, widget, padding, line_height);
		if (row == INT_MAX) return std::end(container);

		typename Tcontainer::iterator it = std::begin(container);
		std::advance(it, row);
		return it;
	}

	EventState UpdateListPositionOnKeyPress(int &list_position, uint16_t keycode) const;
};

/**
 * Nested widget to display and control a scrollbar in a window.
 * Also assign the scrollbar to other widgets using #SetScrollbar() to make the mousewheel work.
 * @ingroup NestedWidgets
 */
class NWidgetScrollbar : public NWidgetCore, public Scrollbar {
public:
	NWidgetScrollbar(WidgetType tp, Colours colour, WidgetID index);

	void SetupSmallestSize(Window *w) override;
	void Draw(const Window *w) override;

	static void InvalidateDimensionCache();
	static Dimension GetVerticalDimension();
	static Dimension GetHorizontalDimension();

private:
	static Dimension vertical_dimension;   ///< Cached size of vertical scrollbar button.
	static Dimension horizontal_dimension; ///< Cached size of horizontal scrollbar button.
};

/**
 * Leaf widget.
 * @ingroup NestedWidgets
 */
class NWidgetLeaf : public NWidgetCore {
public:
	NWidgetLeaf(WidgetType tp, Colours colour, WidgetID index, uint32_t data, StringID tip);

	void SetupSmallestSize(Window *w) override;
	void Draw(const Window *w) override;

	bool ButtonHit(const Point &pt);

	static void InvalidateDimensionCache();

	static Dimension dropdown_dimension;  ///< Cached size of a dropdown widget.
	static Dimension resizebox_dimension; ///< Cached size of a resizebox widget.
	static Dimension closebox_dimension;  ///< Cached size of a closebox widget.
private:
	static Dimension shadebox_dimension;  ///< Cached size of a shadebox widget.
	static Dimension debugbox_dimension;  ///< Cached size of a debugbox widget.
	static Dimension defsizebox_dimension; ///< Cached size of a defsizebox widget.
	static Dimension stickybox_dimension; ///< Cached size of a stickybox widget.
};

/**
 * Return the biggest possible size of a nested widget.
 * @param base      Base size of the widget.
 * @param max_space Available space for the widget.
 * @param step      Stepsize of the widget.
 * @return Biggest possible size of the widget, assuming that \a base may only be incremented by \a step size steps.
 */
static inline uint ComputeMaxSize(uint base, uint max_space, uint step)
{
	if (base >= max_space || step == 0) return base;
	if (step == 1) return max_space;
	uint increment = max_space - base;
	increment -= increment % step;
	return base + increment;
}

/**
 * @defgroup NestedWidgetParts Hierarchical widget parts
 * To make nested widgets easier to enter, nested widget parts have been created. They allow the tree to be defined in a flat array of parts.
 *
 * - Leaf widgets start with a #NWidget(WidgetType tp, Colours col, int16_t idx) part.
 *   Next, specify its properties with one or more of
 *   - #SetMinimalSize Define the minimal size of the widget.
 *   - #SetFill Define how the widget may grow to make it nicely.
 *   - #SetDataTip Define the data and the tooltip of the widget.
 *   - #SetResize Define how the widget may resize.
 *   - #SetPadding Create additional space around the widget.
 *
 * - To insert a nested widget tree from an external source, nested widget part #NWidgetFunction exists.
 *   For further customization, the #SetPadding part may be used.
 *
 * - Space widgets (#NWidgetSpacer) start with a #NWidget(WidgetType tp), followed by one or more of
 *   - #SetMinimalSize Define the minimal size of the widget.
 *   - #SetFill Define how the widget may grow to make it nicely.
 *   - #SetResize Define how the widget may resize.
 *   - #SetPadding Create additional space around the widget.
 *
 * - Container widgets #NWidgetHorizontal, #NWidgetHorizontalLTR, #NWidgetVertical, and #NWidgetMatrix, start with a #NWidget(WidgetType tp) part.
 *   Their properties are derived from the child widgets so they cannot be specified.
 *   You can however use
 *   - #SetPadding Define additional padding around the container.
 *   - #SetPIP Set additional pre/inter/post child widget space.
 *   .
 *   Underneath these properties, all child widgets of the container must be defined. To denote that they are childs, add an indent before the nested widget parts of
 *   the child widgets (it has no meaning for the compiler but it makes the widget parts easier to read).
 *   Below the last child widget, use an #EndContainer part. This part should be aligned with the #NWidget part that started the container.
 *
 * - Stacked widgets #NWidgetStacked map each of their children onto the same space. It behaves like a container, except there is no pre/inter/post space,
 *   so the widget does not support #SetPIP. #SetPadding is allowed though.
 *   Like the other container widgets, below the last child widgets, a #EndContainer part should be used to denote the end of the stacked widget.
 *
 * - Background widgets #NWidgetBackground start with a #NWidget(WidgetType tp, Colours col, int16_t idx) part.
 *   What follows depends on how the widget is used.
 *   - If the widget is used as a leaf widget, that is, to create some space in the window to display a viewport or some text, use the properties of the
 *     leaf widgets to define how it behaves.
 *   - If the widget is used a background behind other widgets, it is considered to be a container widgets. Use the properties listed there to define its
 *     behaviour.
 *   .
 *   In both cases, the background widget \b MUST end with a #EndContainer widget part.
 *
 * @see NestedWidgets
 */

/**
 * Widget part for storing data and tooltip information.
 * @ingroup NestedWidgetParts
 */
struct NWidgetPartDataTip {
	uint32_t data;      ///< Data value of the widget.
	StringID tooltip; ///< Tooltip of the widget.
};

/**
 * Widget part for storing basic widget information.
 * @ingroup NestedWidgetParts
 */
struct NWidgetPartWidget {
	Colours colour; ///< Widget colour.
	WidgetID index; ///< Index of the widget.
};

/**
 * Widget part for storing padding.
 * @ingroup NestedWidgetParts
 */
struct NWidgetPartPaddings : RectPadding {
};

/**
 * Widget part for storing pre/inter/post spaces.
 * @ingroup NestedWidgetParts
 */
struct NWidgetPartPIP {
	uint8_t pre, inter, post; ///< Amount of space before/between/after child widgets.
};

/**
 * Widget part for storing minimal text line data.
 * @ingroup NestedWidgetParts
 */
struct NWidgetPartTextLines {
	uint8_t lines;   ///< Number of text lines.
	uint8_t spacing; ///< Extra spacing around lines.
	FontSize size; ///< Font size of text lines.
};

/**
 * Widget part for storing text colour.
 * @ingroup NestedWidgetParts
 */
struct NWidgetPartTextStyle {
	TextColour colour; ///< TextColour for DrawString.
	FontSize size; ///< Font size of text.
};

/**
 * Widget part for setting text/image alignment within a widget.
 * @ingroup NestedWidgetParts
 */
struct NWidgetPartAlignment {
	StringAlignment align; ///< Alignment of text/image.
};

/**
 * Pointer to function returning a nested widget.
 * @return Nested widget (tree).
 */
typedef std::unique_ptr<NWidgetBase> NWidgetFunctionType();

/**
 * Partial widget specification to allow NWidgets to be written nested.
 * @ingroup NestedWidgetParts
 */
struct NWidgetPart {
	WidgetType type;                         ///< Type of the part. @see NWidgetPartType.
	union {
		Point xy;                        ///< Part with an x/y size.
		NWidgetPartDataTip data_tip;     ///< Part with a data/tooltip.
		NWidgetPartWidget widget;        ///< Part with a start of a widget.
		NWidgetPartPaddings padding;     ///< Part with paddings.
		NWidgetPartPIP pip;              ///< Part with pre/inter/post spaces.
		NWidgetPartTextLines text_lines; ///< Part with text line data.
		NWidgetPartTextStyle text_style; ///< Part with text style data.
		NWidgetPartAlignment align;      ///< Part with internal alignment.
		NWidgetFunctionType *func_ptr;   ///< Part with a function call.
		NWidContainerFlags cont_flags;   ///< Part with container flags.
	} u;
};

/**
 * Widget part function for setting the resize step.
 * @param dx Horizontal resize step. 0 means no horizontal resizing.
 * @param dy Vertical resize step. 0 means no vertical resizing.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart SetResize(int16_t dx, int16_t dy)
{
	NWidgetPart part;

	part.type = WPT_RESIZE;
	part.u.xy.x = dx;
	part.u.xy.y = dy;

	return part;
}

/**
 * Widget part function for setting the minimal size.
 * @param x Horizontal minimal size.
 * @param y Vertical minimal size.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart SetMinimalSize(int16_t x, int16_t y)
{
	NWidgetPart part;

	part.type = WPT_MINSIZE;
	part.u.xy.x = x;
	part.u.xy.y = y;

	return part;
}

/**
 * Widget part function for setting the minimal text lines.
 * @param lines   Number of text lines.
 * @param spacing Extra spacing required.
 * @param size    Font size of text.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart SetMinimalTextLines(uint8_t lines, uint8_t spacing, FontSize size = FS_NORMAL)
{
	NWidgetPart part;

	part.type = WPT_MINTEXTLINES;
	part.u.text_lines.lines = lines;
	part.u.text_lines.spacing = spacing;
	part.u.text_lines.size = size;

	return part;
}

/**
 * Widget part function for setting the text style.
 * @param colour Colour to draw string within widget.
 * @param size Font size to draw string within widget.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart SetTextStyle(TextColour colour, FontSize size = FS_NORMAL)
{
	NWidgetPart part;

	part.type = WPT_TEXTSTYLE;
	part.u.text_style.colour = colour;
	part.u.text_style.size = size;

	return part;
}

/**
 * Widget part function for setting the alignment of text/images.
 * @param align  Alignment of text/image within widget.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart SetAlignment(StringAlignment align)
{
	NWidgetPart part;

	part.type = WPT_ALIGNMENT;
	part.u.align.align = align;

	return part;
}

/**
 * Widget part function for setting filling.
 * @param fill_x Horizontal filling step from minimal size.
 * @param fill_y Vertical filling step from minimal size.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart SetFill(uint fill_x, uint fill_y)
{
	NWidgetPart part;

	part.type = WPT_FILL;
	part.u.xy.x = fill_x;
	part.u.xy.y = fill_y;

	return part;
}

/**
 * Widget part function for denoting the end of a container
 * (horizontal, vertical, WWT_FRAME, WWT_INSET, or WWT_PANEL).
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart EndContainer()
{
	NWidgetPart part;

	part.type = WPT_ENDCONTAINER;

	return part;
}

/**
 * Widget part function for setting the data and tooltip.
 * @param data Data of the widget.
 * @param tip  Tooltip of the widget.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart SetDataTip(uint32_t data, StringID tip)
{
	NWidgetPart part;

	part.type = WPT_DATATIP;
	part.u.data_tip.data = data;
	part.u.data_tip.tooltip = tip;

	return part;
}

/**
 * Widget part function for setting the data and tooltip of WWT_MATRIX widgets
 * @param cols Number of columns. \c 0 means to use draw columns with width according to the resize step size.
 * @param rows Number of rows. \c 0 means to use draw rows with height according to the resize step size.
 * @param tip  Tooltip of the widget.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart SetMatrixDataTip(uint8_t cols, uint8_t rows, StringID tip)
{
	return SetDataTip((rows << MAT_ROW_START) | (cols << MAT_COL_START), tip);
}

/**
 * Widget part function for setting additional space around a widget.
 * Parameters start above the widget, and are specified in clock-wise direction.
 * @param top The padding above the widget.
 * @param right The padding right of the widget.
 * @param bottom The padding below the widget.
 * @param left The padding left of the widget.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart SetPadding(uint8_t top, uint8_t right, uint8_t bottom, uint8_t left)
{
	NWidgetPart part;

	part.type = WPT_PADDING;
	part.u.padding.top = top;
	part.u.padding.right = right;
	part.u.padding.bottom = bottom;
	part.u.padding.left = left;

	return part;
}

/**
 * Widget part function for setting additional space around a widget.
 * @param r The padding around the widget.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart SetPadding(const RectPadding &padding)
{
	NWidgetPart part;

	part.type = WPT_PADDING;
	part.u.padding.left = padding.left;
	part.u.padding.top = padding.top;
	part.u.padding.right = padding.right;
	part.u.padding.bottom = padding.bottom;

	return part;
}

/**
 * Widget part function for setting a padding.
 * @param padding The padding to use for all directions.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart SetPadding(uint8_t padding)
{
	return SetPadding(padding, padding, padding, padding);
}

/**
 * Widget part function for setting a pre/inter/post spaces.
 * @param pre The amount of space before the first widget.
 * @param inter The amount of space between widgets.
 * @param post The amount of space after the last widget.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart SetPIP(uint8_t pre, uint8_t inter, uint8_t post)
{
	NWidgetPart part;

	part.type = WPT_PIPSPACE;
	part.u.pip.pre = pre;
	part.u.pip.inter = inter;
	part.u.pip.post = post;

	return part;
}

/**
 * Widget part function for setting a pre/inter/post ratio.
 * @param pre The ratio of space before the first widget.
 * @param inter The ratio of space between widgets.
 * @param post The ratio of space after the last widget.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart SetPIPRatio(uint8_t ratio_pre, uint8_t ratio_inter, uint8_t ratio_post)
{
	NWidgetPart part;

	part.type = WPT_PIPRATIO;
	part.u.pip.pre = ratio_pre;
	part.u.pip.inter = ratio_inter;
	part.u.pip.post = ratio_post;

	return part;
}

/**
 * Attach a scrollbar to a widget.
 * The scrollbar is controlled when using the mousewheel on the widget.
 * Multiple widgets can refer to the same scrollbar to make the mousewheel work in all of them.
 * @param index Widget index of the scrollbar.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart SetScrollbar(WidgetID index)
{
	NWidgetPart part;

	part.type = WPT_SCROLLBAR;
	part.u.widget.index = index;

	return part;
}

/**
 * Widget part function for starting a new 'real' widget.
 * @param tp  Type of the new nested widget.
 * @param col Colour of the new widget.
 * @param idx Index of the widget.
 * @note with #WWT_PANEL, #WWT_FRAME, #WWT_INSET, a new container is started.
 *       Child widgets must have a index bigger than the parent index.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart NWidget(WidgetType tp, Colours col, int idx = -1)
{
	NWidgetPart part;

	part.type = tp;
	part.u.widget.colour = col;
	part.u.widget.index = idx;

	return part;
}

/**
 * Widget part function for starting a new horizontal container, vertical container, or spacer widget.
 * @param tp         Type of the new nested widget, #NWID_HORIZONTAL, #NWID_VERTICAL, #NWID_SPACER, #NWID_SELECTION, and #NWID_MATRIX.
 * @param cont_flags Flags for the containers (#NWID_HORIZONTAL and #NWID_VERTICAL).
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart NWidget(WidgetType tp, NWidContainerFlags cont_flags = NC_NONE)
{
	NWidgetPart part;

	part.type = tp;
	part.u.cont_flags = cont_flags;

	return part;
}

/**
 * Obtain a nested widget (sub)tree from an external source.
 * @param func_ptr Pointer to function that returns the tree.
 * @ingroup NestedWidgetParts
 */
static inline NWidgetPart NWidgetFunction(NWidgetFunctionType *func_ptr)
{
	NWidgetPart part;

	part.type = WPT_FUNCTION;
	part.u.func_ptr = func_ptr;

	return part;
}

bool IsContainerWidgetType(WidgetType tp);
std::unique_ptr<NWidgetBase> MakeNWidgets(const NWidgetPart *nwid_begin, const NWidgetPart *nwid_end, std::unique_ptr<NWidgetBase> &&container);
std::unique_ptr<NWidgetBase> MakeWindowNWidgetTree(const NWidgetPart *nwid_begin, const NWidgetPart *nwid_end, NWidgetStacked **shade_select);

std::unique_ptr<NWidgetBase> MakeCompanyButtonRows(WidgetID widget_first, WidgetID widget_last, Colours button_colour, int max_length, StringID button_tooltip);

void SetupWidgetDimensions();

#endif /* WIDGET_TYPE_H */
