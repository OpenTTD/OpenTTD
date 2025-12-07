/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file widget_type.h Definitions about widgets. */

#ifndef WIDGET_TYPE_H
#define WIDGET_TYPE_H

#include "core/math_func.hpp"
#include "strings_type.h"
#include "gfx_type.h"
#include "window_type.h"

/** Values for an arrow widget */
enum ArrowWidgetValues : uint8_t {
	AWV_DECREASE, ///< Arrow to the left or in case of RTL to the right
	AWV_INCREASE, ///< Arrow to the right or in case of RTL to the left
	AWV_LEFT,     ///< Force the arrow to the left
	AWV_RIGHT,    ///< Force the arrow to the right
};

/** WidgetData values for a resize box widget. */
enum ResizeWidgetValues : uint8_t {
	RWV_SHOW_BEVEL, ///< Bevel of resize box is shown.
	RWV_HIDE_BEVEL, ///< Bevel of resize box is hidden.
};

/**
 * Window widget types, nested widget types, and nested widget part types.
 */
enum WidgetType : uint8_t {
	/* Window widget types. */
	WWT_EMPTY,      ///< Empty widget, place holder to reserve space in widget tree.

	WWT_PANEL,      ///< Simple depressed panel
	WWT_INSET,      ///< Pressed (inset) panel, most commonly used as combo box _text_ area
	WWT_IMGBTN,     ///< (Toggle) Button with image
	WWT_IMGBTN_2,   ///< (Toggle) Button with diff image when clicked
	WWT_ARROWBTN,   ///< (Toggle) Button with an arrow
	WWT_TEXTBTN,    ///< (Toggle) Button with text
	WWT_TEXTBTN_2,  ///< (Toggle) Button with diff text when clicked
	WWT_BOOLBTN,    ///< Standard boolean toggle button.
	WWT_IMGTEXTBTN, ///< (Toggle) Button with image and text
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
	NWID_LAYER,           ///< Layered widgets, all visible together.
	NWID_VIEWPORT,        ///< Nested widget containing a viewport.
	NWID_BUTTON_DROPDOWN, ///< Button with a drop-down.
	NWID_HSCROLLBAR,      ///< Horizontal scrollbar
	NWID_VSCROLLBAR,      ///< Vertical scrollbar
	NWID_CUSTOM,          ///< General Custom widget.

	/* Nested widget part types. */
	WPT_ATTRIBUTE_BEGIN, ///< Begin marker for attribute NWidgetPart types.
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
	WPT_SCROLLBAR,    ///< Widget part for attaching a scrollbar.
	WPT_ASPECT,       ///< Widget part for specifying aspect ratio.
	WPT_ATTRIBUTE_END, ///< End marker for attribute NWidgetPart types.

	WPT_FUNCTION, ///< Widget part for calling a user function.
	WPT_ENDCONTAINER, ///< Widget part to denote end of a container.

	/* Pushable window widget types. */
	WWT_MASK = 0x7F,

	WWB_PUSHBUTTON    = 1 << 7,

	WWT_PUSHBTN       = WWT_PANEL    | WWB_PUSHBUTTON,    ///< Normal push-button (no toggle button) with custom drawing
	WWT_PUSHTXTBTN    = WWT_TEXTBTN  | WWB_PUSHBUTTON,    ///< Normal push-button (no toggle button) with text caption
	WWT_PUSHIMGBTN    = WWT_IMGBTN   | WWB_PUSHBUTTON,    ///< Normal push-button (no toggle button) with image caption
	WWT_PUSHARROWBTN  = WWT_ARROWBTN | WWB_PUSHBUTTON,    ///< Normal push-button (no toggle button) with arrow caption
	WWT_PUSHIMGTEXTBTN = WWT_IMGTEXTBTN | WWB_PUSHBUTTON, ///< Normal push-button (no toggle button) with image and text caption
	NWID_PUSHBUTTON_DROPDOWN = NWID_BUTTON_DROPDOWN | WWB_PUSHBUTTON,
};

/** Different forms of sizing nested widgets, using NWidgetBase::AssignSizePosition() */
enum SizingType : uint8_t {
	ST_SMALLEST, ///< Initialize nested widget tree to smallest size. Also updates \e current_x and \e current_y.
	ST_RESIZE,   ///< Resize the nested widget tree.
};

enum class AspectFlag : uint8_t {
	ResizeX,
	ResizeY,
};
using AspectFlags = EnumBitSet<AspectFlag, uint8_t>;

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
class NWidgetBase {
public:
	NWidgetBase(WidgetType tp, WidgetID index = INVALID_WIDGET) : type(tp), index(index) {}
	virtual ~NWidgetBase() = default;

	void ApplyAspectRatio();
	virtual void AdjustPaddingForZoom();
	virtual void SetupSmallestSize(Window *w) = 0;
	virtual void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) = 0;

	virtual void FillWidgetLookup(WidgetLookup &widget_lookup);

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

	inline WidgetID GetIndex() const { return this->index; }
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

	WidgetType type{}; ///< Type of the widget / nested widget.
	uint fill_x = 0; ///< Horizontal fill stepsize (from initial size, \c 0 means not resizable).
	uint fill_y = 0; ///< Vertical fill stepsize (from initial size, \c 0 means not resizable).
	uint resize_x = 0; ///< Horizontal resize step (\c 0 means not resizable).
	uint resize_y = 0; ///< Vertical resize step (\c 0 means not resizable).
	/* Size of the widget in the smallest window possible.
	 * Computed by #SetupSmallestSize() followed by #AssignSizePosition().
	 */
	uint smallest_x = 0; ///< Smallest horizontal size of the widget in a filled window.
	uint smallest_y = 0; ///< Smallest vertical size of the widget in a filled window.
	/* Current widget size (that is, after resizing). */
	uint current_x = 0; ///< Current horizontal size (after resizing).
	uint current_y = 0; ///< Current vertical size (after resizing).
	float aspect_ratio = 0; ///< Desired aspect ratio of widget.
	AspectFlags aspect_flags = AspectFlag::ResizeX; ///< Which dimensions can be resized.

	int pos_x = 0; ///< Horizontal position of top-left corner of the widget in the window.
	int pos_y = 0; ///< Vertical position of top-left corner of the widget in the window.

	RectPadding padding{}; ///< Padding added to the widget. Managed by parent container widget. (parent container may swap left and right for RTL)
	RectPadding uz_padding{}; ///< Unscaled padding, for resize calculation.

	NWidgetBase *parent = nullptr; ///< Parent widget of this widget, automatically filled in when added to container.

protected:
	const WidgetID index = INVALID_WIDGET; ///< Index of the nested widget (\c INVALID_WIDGET means 'not used').

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
	NWidgetResizeBase(WidgetType tp, WidgetID index, uint fill_x, uint fill_y);

	void AdjustPaddingForZoom() override;
	void SetMinimalSize(uint min_x, uint min_y);
	void SetMinimalSizeAbsolute(uint min_x, uint min_y);
	void SetMinimalTextLines(uint8_t min_lines, uint8_t spacing, FontSize size);
	void SetFill(uint fill_x, uint fill_y);
	void SetResize(uint resize_x, uint resize_y);
	void SetAspect(float ratio, AspectFlags flags = AspectFlag::ResizeX);
	void SetAspect(int x_ratio, int y_ratio, AspectFlags flags = AspectFlag::ResizeX);

	bool UpdateMultilineWidgetSize(const std::string &str, int max_lines);
	bool UpdateSize(uint min_x, uint min_y);
	bool UpdateVerticalSize(uint min_y);

	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override;

	uint min_x = 0; ///< Minimal horizontal size of only this widget.
	uint min_y = 0; ///< Minimal vertical size of only this widget.

	bool absolute = false; ///< Set if minimum size is fixed and should not be resized.
	uint uz_min_x = 0; ///< Unscaled Minimal horizontal size of only this widget.
	uint uz_min_y = 0; ///< Unscaled Minimal vertical size of only this widget.

	uint8_t uz_text_lines = 0; ///< 'Unscaled' text lines, stored for resize calculation.
	uint8_t uz_text_spacing = 0; ///< 'Unscaled' text padding, stored for resize calculation.
	FontSize uz_text_size{}; ///< 'Unscaled' font size, stored for resize calculation.
};

/** Nested widget flags that affect display and interaction with 'real' widgets. */
enum NWidgetDisplayFlag : uint8_t {
	/* Generic. */
	Lowered, ///< Widget is lowered (pressed down) bit.
	Disabled, ///< Widget is disabled (greyed out) bit.

	/* Viewport widget. */
	NoTransparency, ///< Viewport is never transparent.
	ShadeGrey, ///< Shade viewport to grey-scale.
	ShadeDimmed, ///< Display dimmed colours in the viewport.

	/* Button dropdown widget. */
	DropdownActive, ///< Dropdown menu of the button dropdown widget is active. @see #NWID_BUTTON_DROPDOWN

	/* Scrollbar widget. */
	ScrollbarUp, ///< Up-button is lowered bit.
	ScrollbarDown, ///< Down-button is lowered bit.

	/* Generic. */
	Highlight, ///< Highlight of widget is on.
	DropdownClosed, ///< Dropdown menu of the dropdown widget has closed.
};
using NWidgetDisplayFlags = EnumBitSet<NWidgetDisplayFlag, uint16_t>;

/** Container with the data associated to a single widget. */
struct WidgetData {
	StringID string{};
	SpriteID sprite{};
	ArrowWidgetValues arrow_widget_type{};
	ResizeWidgetValues resize_widget_type{};
	Colours alternate_colour = INVALID_COLOUR;
	Dimension matrix{};
};

/**
 * Base class for a 'real' widget.
 * @ingroup NestedWidgets
 */
class NWidgetCore : public NWidgetResizeBase {
public:
	NWidgetCore(WidgetType tp, Colours colour, WidgetID index, uint fill_x, uint fill_y, const WidgetData &widget_data, StringID tool_tip);

	void SetString(StringID string);
	void SetStringTip(StringID string, StringID tool_tip);
	void SetSprite(SpriteID sprite);
	void SetSpriteTip(SpriteID sprite, StringID tool_tip);
	void SetMatrixDimension(uint32_t columns, uint32_t rows);
	void SetResizeWidgetType(ResizeWidgetValues type);
	void SetToolTip(StringID tool_tip);
	StringID GetToolTip() const;
	void SetTextStyle(TextColour colour, FontSize size);
	void SetAlignment(StringAlignment align);

	StringID GetString() const;
	WidgetID GetScrollbarIndex() const;

	inline void SetLowered(bool lowered);
	inline bool IsLowered() const;
	inline void SetDisabled(bool disabled);
	inline bool IsDisabled() const;

	inline TextColour GetTextColour() const { return this->text_colour; }
	inline FontSize GetFontSize() const { return this->text_size; }

	NWidgetCore *GetWidgetFromPos(int x, int y) override;
	bool IsHighlighted() const override;
	TextColour GetHighlightColour() const override;
	void SetHighlighted(TextColour highlight_colour) override;

	NWidgetDisplayFlags disp_flags; ///< Flags that affect display and interaction with the widget.
	Colours colour;            ///< Colour of this widget.
protected:
	WidgetData widget_data{}; ///< Data of the widget. @see Widget::data
	StringID tool_tip{}; ///< Tooltip of the widget. @see Widget::tool_tips
	WidgetID scrollbar_index = INVALID_WIDGET; ///< Index of an attached scrollbar.
	TextColour highlight_colour{}; ///< Colour of highlight.
	TextColour text_colour{}; ///< Colour of text within widget.
	FontSize text_size = FS_NORMAL; ///< Size of text within widget.
	StringAlignment align = SA_CENTER; ///< Alignment of text/image within widget.

	/* This function constructs the widgets, so it should be able to write the variables. */
	friend void ApplyNWidgetPartAttribute(const struct NWidgetPart &nwid, NWidgetBase *dest);
};

/**
 * Highlight the widget or not.
 * @param highlight_colour Widget must be highlighted (blink).
 */
inline void NWidgetCore::SetHighlighted(TextColour highlight_colour)
{
	highlight_colour != TC_INVALID ? this->disp_flags.Set(NWidgetDisplayFlag::Highlight) : this->disp_flags.Reset(NWidgetDisplayFlag::Highlight);
	this->highlight_colour = highlight_colour;
}

/** Return whether the widget is highlighted. */
inline bool NWidgetCore::IsHighlighted() const
{
	return this->disp_flags.Test(NWidgetDisplayFlag::Highlight);
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
	lowered ? this->disp_flags.Set(NWidgetDisplayFlag::Lowered) : this->disp_flags.Reset(NWidgetDisplayFlag::Lowered);
}

/** Return whether the widget is lowered. */
inline bool NWidgetCore::IsLowered() const
{
	return this->disp_flags.Test(NWidgetDisplayFlag::Lowered);
}

/**
 * Disable (grey-out) or enable the widget.
 * @param disabled Widget must be disabled.
 */
inline void NWidgetCore::SetDisabled(bool disabled)
{
	this->disp_flags = disabled ? this->disp_flags.Set(NWidgetDisplayFlag::Disabled) : this->disp_flags.Reset(NWidgetDisplayFlag::Disabled);
}

/** Return whether the widget is disabled. */
inline bool NWidgetCore::IsDisabled() const
{
	return this->disp_flags.Test(NWidgetDisplayFlag::Disabled);
}


/**
 * Baseclass for container widgets.
 * @ingroup NestedWidgets
 */
class NWidgetContainer : public NWidgetBase {
public:
	NWidgetContainer(WidgetType tp, WidgetID index = INVALID_WIDGET) : NWidgetBase(tp, index) {}

	void AdjustPaddingForZoom() override;
	void Add(std::unique_ptr<NWidgetBase> &&wid);
	void FillWidgetLookup(WidgetLookup &widget_lookup) override;

	void Draw(const Window *w) override;
	NWidgetCore *GetWidgetFromPos(int x, int y) override;

	/** Return whether the container is empty. */
	inline bool IsEmpty() { return this->children.empty(); }

	NWidgetBase *GetWidgetOfType(WidgetType tp) override;
	void UnfocusWidgets(Window *parent_window);

	/**
	 * Clears the container, deleting all widgets that were contained.
	 * @param parent_window Window that contains the container.
	 */
	inline void Clear(Window *parent_window)
	{
		this->UnfocusWidgets(parent_window);
		this->children.clear();
	}

protected:
	std::vector<std::unique_ptr<NWidgetBase>> children{}; ///< Child widgets in container.
};

/** Display planes with zero size for #NWidgetStacked. */
enum StackedZeroSizePlanes : int {
	SZSP_VERTICAL = INT_MAX / 2, ///< Display plane with zero size horizontally, and filling and resizing vertically.
	SZSP_HORIZONTAL,             ///< Display plane with zero size vertically, and filling and resizing horizontally.
	SZSP_NONE,                   ///< Display plane with zero size in both directions (none filling and resizing).

	SZSP_BEGIN = SZSP_VERTICAL,  ///< First zero-size plane.
};

/**
 * Stacked widgets, widgets all occupying the same space in the window.
 * #NWID_SELECTION allows for selecting one of several panels (planes) to be displayed. All planes must have the same size.
 * Since all planes are also initialized, switching between different planes can be done while the window is displayed.
 *
 * There are also a number of special planes (defined in #StackedZeroSizePlanes) that have zero size in one direction (and are stretchable in
 * the other direction) or have zero size in both directions. They are used to make all child planes of the widget disappear.
 * Unlike switching between the regular display planes (that all have the same size), switching from or to one of the zero-sized planes means that
 * a #Window::ReInit() is needed to re-initialize the window since its size changes.
 */
class NWidgetStacked : public NWidgetContainer {
public:
	NWidgetStacked(WidgetID index) : NWidgetContainer(NWID_SELECTION, index) {}

	void SetupSmallestSize(Window *w) override;
	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override;
	void FillWidgetLookup(WidgetLookup &widget_lookup) override;

	void Draw(const Window *w) override;
	NWidgetCore *GetWidgetFromPos(int x, int y) override;

	bool SetDisplayedPlane(int plane);

	int shown_plane = 0; ///< Plane being displayed (for #NWID_SELECTION only).
private:
	WidgetLookup *widget_lookup = nullptr; ///< Window's widget lookup, updated in SetDisplayedPlane().
};

/** Nested widget container flags, */
enum NWidContainerFlag : uint8_t {
	EqualSize, ///< Containers should keep all their (resizing) children equally large.
	BigFirst, ///< Allocate space to biggest resize first.
};
using NWidContainerFlags = EnumBitSet<NWidContainerFlag, uint8_t>;

/** Container with pre/inter/post child space. */
class NWidgetPIPContainer : public NWidgetContainer {
public:
	NWidgetPIPContainer(WidgetType tp, NWidContainerFlags flags = {}, WidgetID index = INVALID_WIDGET) : NWidgetContainer(tp, index), flags(flags) {}

	void AdjustPaddingForZoom() override;
	void SetPIP(uint8_t pip_pre, uint8_t pip_inter, uint8_t pip_post);
	void SetPIPRatio(uint8_t pip_ratio_pre, uint8_t pip_ratio_inter, uint8_t pip_ratio_post);

protected:
	NWidContainerFlags flags{}; ///< Flags of the container.
	uint8_t pip_pre = 0; ///< Amount of space before first widget.
	uint8_t pip_inter = 0; ///< Amount of space between widgets.
	uint8_t pip_post = 0; ///< Amount of space after last widget.
	uint8_t pip_ratio_pre = 0; ///< Ratio of remaining space before first widget.
	uint8_t pip_ratio_inter = 0; ///< Ratio of remaining space between widgets.
	uint8_t pip_ratio_post = 0; ///< Ratio of remaining space after last widget.

	uint8_t uz_pip_pre = 0; ///< Unscaled space before first widget.
	uint8_t uz_pip_inter = 0; ///< Unscaled space between widgets.
	uint8_t uz_pip_post = 0; ///< Unscaled space after last widget.

	uint8_t gaps = 0; ///< Number of gaps between widgets.
};

/**
 * Horizontal container.
 * @ingroup NestedWidgets
 */
class NWidgetHorizontal : public NWidgetPIPContainer {
public:
	NWidgetHorizontal(NWidContainerFlags flags = {}, WidgetID index = INVALID_WIDGET, WidgetType type = NWID_HORIZONTAL) : NWidgetPIPContainer(type, flags, index) {}

	void SetupSmallestSize(Window *w) override;
	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override;
};

/**
 * Horizontal container that doesn't change the direction of the widgets for RTL languages.
 * @ingroup NestedWidgets
 */
class NWidgetHorizontalLTR : public NWidgetHorizontal {
public:
	NWidgetHorizontalLTR(NWidContainerFlags flags = {}, WidgetID index = INVALID_WIDGET) : NWidgetHorizontal(flags, index, NWID_HORIZONTAL_LTR) {}

	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override;
};

/**
 * Vertical container.
 * @ingroup NestedWidgets
 */
class NWidgetVertical : public NWidgetPIPContainer {
public:
	NWidgetVertical(NWidContainerFlags flags = {}, WidgetID index = INVALID_WIDGET) : NWidgetPIPContainer(NWID_VERTICAL, flags, index) {}

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
	NWidgetMatrix(Colours colour, WidgetID index) : NWidgetPIPContainer(NWID_MATRIX, NWidContainerFlag::EqualSize, index), colour(colour) {}

	void SetClicked(int clicked);
	void SetCount(int count);
	void SetScrollbar(Scrollbar *sb);
	int GetCurrentElement() const;

	void SetupSmallestSize(Window *w) override;
	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override;

	NWidgetCore *GetWidgetFromPos(int x, int y) override;
	void Draw(const Window *w) override;
protected:
	Colours colour{}; ///< Colour of this widget.
	int clicked = -1; ///< The currently clicked element.
	int count = 0; ///< Amount of valid elements.
	int current_element = 0; ///< The element currently being processed.
	Scrollbar *sb = nullptr;  ///< The scrollbar we're associated with.
private:
	int widget_w = 0; ///< The width of the child widget including inter spacing.
	int widget_h = 0; ///< The height of the child widget including inter spacing.
	int widgets_x = 0; ///< The number of visible widgets in horizontal direction.
	int widgets_y = 0; ///< The number of visible widgets in vertical direction.

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
	std::unique_ptr<NWidgetPIPContainer> child{}; ///< Child widget.
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
public:
	using size_type = int32_t;
	static constexpr size_type max_size_type = std::numeric_limits<size_type>::max();
	static constexpr size_type npos = max_size_type;
private:
	const bool is_vertical = false; ///< Scrollbar has vertical orientation.
	size_type count = 0; ///< Number of elements in the list.
	size_type cap = 0; ///< Number of visible elements of the scroll bar.
	size_type pos = 0; ///< Index of first visible item of the list.
	size_type stepsize = 1; ///< Distance to scroll, when pressing the buttons or using the wheel.

public:
	/** Stepping sizes when scrolling */
	enum ScrollbarStepping : uint8_t {
		SS_RAW,             ///< Step in single units.
		SS_SMALL,           ///< Step in #stepsize units.
		SS_BIG,             ///< Step in #cap units.
	};

	Scrollbar(bool is_vertical) : is_vertical(is_vertical) {}

	/**
	 * Gets the number of elements in the list
	 * @return the number of elements
	 */
	inline size_type GetCount() const
	{
		return this->count;
	}

	/**
	 * Gets the number of visible elements of the scrollbar
	 * @return the number of visible elements
	 */
	inline size_type GetCapacity() const
	{
		return this->cap;
	}

	/**
	 * Gets the position of the first visible element in the list
	 * @return the position of the element
	 */
	inline size_type GetPosition() const
	{
		return this->pos;
	}

	/**
	 * Checks whether given current item is visible in the list
	 * @param item to check
	 * @return true iff the item is visible
	 */
	inline bool IsVisible(size_type item) const
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

		this->stepsize = ClampTo<size_type>(stepsize);
	}

	/**
	 * Sets the number of elements in the list
	 * @param num the number of elements in the list
	 * @note updates the position if needed
	 */
	void SetCount(size_t num)
	{
		assert(num < Scrollbar::max_size_type);

		this->count = ClampTo<size_type>(num);
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
		assert(capacity < Scrollbar::max_size_type);

		this->cap = ClampTo<size_type>(capacity);
		/* Ensure position is within bounds */
		this->SetPosition(this->pos);
	}

	void SetCapacityFromWidget(Window *w, WidgetID widget, int padding = 0);

	/**
	 * Sets the position of the first visible element
	 * @param position the position of the element
	 * @return true iff the position has changed
	 */
	bool SetPosition(size_type position)
	{
		size_type old_pos = this->pos;
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
	void ScrollTowards(size_type position)
	{
		if (position < this->GetPosition()) {
			/* scroll up to the item */
			this->SetPosition(position);
		} else if (position >= this->GetPosition() + this->GetCapacity()) {
			/* scroll down so that the item is at the bottom */
			this->SetPosition(position - this->GetCapacity() + 1);
		}
	}

	size_type GetScrolledRowFromWidget(int clickpos, const Window * const w, WidgetID widget, int padding = 0, int line_height = -1) const;

	/**
	 * Get a pair of iterators for the range of visible elements in a container.
	 * @param container Container of elements represented by the scrollbar.
	 * @returns Pair of iterators of visible elements.
	 */
	template <typename Tcontainer>
	auto GetVisibleRangeIterators(Tcontainer &container) const
	{
		assert(static_cast<size_t>(this->GetCount()) == container.size()); // Scrollbar and container size must match.
		auto first = std::next(std::begin(container), this->GetPosition());
		auto last = std::next(first, std::min<size_t>(this->GetCapacity(), this->GetCount() - this->GetPosition()));
		return std::make_pair(first, last);
	}

	/**
	 * Return an iterator pointing to the element of a scrolled widget that a user clicked in.
	 * @param container   Container of elements represented by the scrollbar.
	 * @param clickpos    Vertical position of the mouse click (without taking scrolling into account).
	 * @param w           The window the click was in.
	 * @param widget      Widget number of the widget clicked in.
	 * @param padding     Amount of empty space between the widget edge and the top of the first row. Default value is \c 0.
	 * @param line_height Height of a single row. A negative value means using the vertical resize step of the widget.
	 * @return Iterator to the element clicked at. If clicked at a wrong position, returns an iterator to the end of the container.
	 */
	template <typename Tcontainer>
	auto GetScrolledItemFromWidget(Tcontainer &container, int clickpos, const Window * const w, WidgetID widget, int padding = 0, int line_height = -1) const
	{
		assert(static_cast<size_t>(this->GetCount()) == container.size()); // Scrollbar and container size must match.
		size_type row = this->GetScrolledRowFromWidget(clickpos, w, widget, padding, line_height);
		if (row == Scrollbar::npos) return std::end(container);

		return std::next(std::begin(container), row);
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
	NWidgetLeaf(WidgetType tp, Colours colour, WidgetID index, const WidgetData &data, StringID tip);

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
inline uint ComputeMaxSize(uint base, uint max_space, uint step)
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
 *   - #SetStringTip Define the string and the tooltip of the widget.
 *   - #SetSpriteTip Define the sprite ID and the tooltip of the widget.
 *   - #SetToolTip Define the tooltip of the widget.
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
 *   Underneath these properties, all child widgets of the container must be defined. To denote that they are children, add an indent before the nested widget parts of
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
	WidgetData data; ///< Data value of the widget.
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

struct NWidgetPartContainer {
	NWidContainerFlags flags;
	WidgetID index;
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

struct NWidgetPartAspect {
	float ratio;
	AspectFlags flags;
};

/**
 * Pointer to function returning a nested widget.
 * @return Nested widget (tree).
 */
using NWidgetFunctionType = std::unique_ptr<NWidgetBase>();

/**
 * Partial widget specification to allow NWidgets to be written nested.
 * @ingroup NestedWidgetParts
 */
struct NWidgetPart {
	WidgetType type;                         ///< Type of the part. @see NWidgetPartType.
	union NWidgetPartUnion {
		Point xy;                        ///< Part with an x/y size.
		NWidgetPartDataTip data_tip;     ///< Part with a data/tooltip.
		NWidgetPartWidget widget;        ///< Part with a start of a widget.
		NWidgetPartPaddings padding;     ///< Part with paddings.
		NWidgetPartPIP pip;              ///< Part with pre/inter/post spaces.
		NWidgetPartTextLines text_lines; ///< Part with text line data.
		NWidgetPartTextStyle text_style; ///< Part with text style data.
		NWidgetPartAlignment align;      ///< Part with internal alignment.
		NWidgetFunctionType *func_ptr;   ///< Part with a function call.
		NWidgetPartContainer container;   ///< Part with container flags.
		NWidgetPartAspect aspect; ///< Part to set aspect ratio.

		/* Constructors for each NWidgetPartUnion data type. */
		constexpr NWidgetPartUnion() : xy() {}
		constexpr NWidgetPartUnion(Point xy) : xy(xy) {}
		constexpr NWidgetPartUnion(NWidgetPartDataTip data_tip) : data_tip(data_tip) {}
		constexpr NWidgetPartUnion(NWidgetPartWidget widget) : widget(widget) {}
		constexpr NWidgetPartUnion(NWidgetPartPaddings padding) : padding(padding) {}
		constexpr NWidgetPartUnion(NWidgetPartPIP pip) : pip(pip) {}
		constexpr NWidgetPartUnion(NWidgetPartTextLines text_lines) : text_lines(text_lines) {}
		constexpr NWidgetPartUnion(NWidgetPartTextStyle text_style) : text_style(text_style) {}
		constexpr NWidgetPartUnion(NWidgetPartAlignment align) : align(align) {}
		constexpr NWidgetPartUnion(NWidgetFunctionType *func_ptr) : func_ptr(func_ptr) {}
		constexpr NWidgetPartUnion(NWidgetPartContainer container) : container(container) {}
		constexpr NWidgetPartUnion(NWidgetPartAspect aspect) : aspect(aspect) {}
	} u;

	/* Constructors for each NWidgetPart data type. */
	explicit constexpr NWidgetPart(WidgetType type) : type(type), u() {}
	constexpr NWidgetPart(WidgetType type, Point xy) : type(type), u(xy) {}
	constexpr NWidgetPart(WidgetType type, NWidgetPartDataTip data_tip) : type(type), u(data_tip) {}
	constexpr NWidgetPart(WidgetType type, NWidgetPartWidget widget) : type(type), u(widget) {}
	constexpr NWidgetPart(WidgetType type, NWidgetPartPaddings padding) : type(type), u(padding) {}
	constexpr NWidgetPart(WidgetType type, NWidgetPartPIP pip) : type(type), u(pip) {}
	constexpr NWidgetPart(WidgetType type, NWidgetPartTextLines text_lines) : type(type), u(text_lines) {}
	constexpr NWidgetPart(WidgetType type, NWidgetPartTextStyle text_style) : type(type), u(text_style) {}
	constexpr NWidgetPart(WidgetType type, NWidgetPartAlignment align) : type(type), u(align) {}
	constexpr NWidgetPart(WidgetType type, NWidgetFunctionType *func_ptr) : type(type), u(func_ptr) {}
	constexpr NWidgetPart(WidgetType type, NWidgetPartContainer container) : type(type), u(container) {}
	constexpr NWidgetPart(WidgetType type, NWidgetPartAspect aspect) : type(type), u(aspect) {}
};

/**
 * Widget part function for setting the resize step.
 * @param dx Horizontal resize step. 0 means no horizontal resizing.
 * @param dy Vertical resize step. 0 means no vertical resizing.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetResize(int16_t dx, int16_t dy)
{
	return NWidgetPart{WPT_RESIZE, Point{dx, dy}};
}

/**
 * Widget part function for setting the minimal size.
 * @param x Horizontal minimal size.
 * @param y Vertical minimal size.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetMinimalSize(int16_t x, int16_t y)
{
	return NWidgetPart{WPT_MINSIZE, Point{x, y}};
}

/**
 * Widget part function to setting the minimal size for a toolbar spacer.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetToolbarSpacerMinimalSize()
{
	return NWidgetPart{WPT_MINSIZE, Point{4, 0}};
}

/**
 * Widget part function to setting the minimal size for a toolbar button.
 * @param width Width of button, measured in multiples of the standard toolbar button size.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetToolbarMinimalSize(int width)
{
	return NWidgetPart{WPT_MINSIZE, Point{20 * width + 2, 22}};
}

/**
 * Widget part function for setting the minimal text lines.
 * @param lines   Number of text lines.
 * @param spacing Extra spacing required.
 * @param size    Font size of text.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetMinimalTextLines(uint8_t lines, uint8_t spacing, FontSize size = FS_NORMAL)
{
	return NWidgetPart{WPT_MINTEXTLINES, NWidgetPartTextLines{lines, spacing, size}};
}

/**
 * Widget part function for setting the text style.
 * @param colour Colour to draw string within widget.
 * @param size Font size to draw string within widget.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetTextStyle(TextColour colour, FontSize size = FS_NORMAL)
{
	return NWidgetPart{WPT_TEXTSTYLE, NWidgetPartTextStyle{colour, size}};
}

/**
 * Widget part function for setting the alignment of text/images.
 * @param align  Alignment of text/image within widget.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetAlignment(StringAlignment align)
{
	return NWidgetPart{WPT_ALIGNMENT, NWidgetPartAlignment{align}};
}

/**
 * Widget part function for setting filling.
 * @param fill_x Horizontal filling step from minimal size.
 * @param fill_y Vertical filling step from minimal size.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetFill(uint16_t fill_x, uint16_t fill_y)
{
	return NWidgetPart{WPT_FILL, Point{fill_x, fill_y}};
}

/**
 * Widget part function for denoting the end of a container
 * (horizontal, vertical, WWT_FRAME, WWT_INSET, or WWT_PANEL).
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart EndContainer()
{
	return NWidgetPart{WPT_ENDCONTAINER};
}

/**
 * Widget part function for setting the string and tooltip.
 * @param string String of the widget.
 * @param tip  Tooltip of the widget.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetStringTip(StringID string, StringID tip = {})
{
	return NWidgetPart{WPT_DATATIP, NWidgetPartDataTip{{.string = string}, tip}};
}

/**
 * Widget part function for setting the sprite and tooltip.
 * @param sprite Sprite of the widget.
 * @param tip  Tooltip of the widget.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetSpriteTip(SpriteID sprite, StringID tip = {})
{
	return NWidgetPart{WPT_DATATIP, NWidgetPartDataTip{{.sprite = sprite}, tip}};
}

/**
 * Widget part function for setting the sprite, string and tooltip.
 * @param sprite Sprite of the widget.
 * @param string String of the widget.
 * @param tip  Tooltip of the widget.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetSpriteStringTip(SpriteID sprite, StringID string, StringID tip = {})
{
	return NWidgetPart{WPT_DATATIP, NWidgetPartDataTip{{.string = string, .sprite = sprite}, tip}};
}

/**
 * Widget part function for setting the arrow widget type and tooltip.
 * @param widget_type Type of the widget to draw.
 * @param tip  Tooltip of the widget.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetArrowWidgetTypeTip(ArrowWidgetValues widget_type, StringID tip = {})
{
	return NWidgetPart{WPT_DATATIP, NWidgetPartDataTip{{.arrow_widget_type = widget_type}, tip}};
}

/**
 * Widget part function for setting the resize widget type and tooltip.
 * @param widget_type Type of the widget to draw.
 * @param tip  Tooltip of the widget.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetResizeWidgetTypeTip(ResizeWidgetValues widget_type, StringID tip)
{
	return NWidgetPart{WPT_DATATIP, NWidgetPartDataTip{{.resize_widget_type = widget_type}, tip}};
}

/**
 * Widget part function for setting the alternate colour and tooltip.
 * @param colour Alternate colour of the widget.
 * @param tip Tooltip of the widget.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetAlternateColourTip(Colours colour, StringID tip)
{
	return NWidgetPart{WPT_DATATIP, NWidgetPartDataTip{{.alternate_colour = colour}, tip}};
}

/**
 * Widget part function for setting the data and tooltip of WWT_MATRIX widgets
 * @param cols Number of columns. \c 0 means to use draw columns with width according to the resize step size.
 * @param rows Number of rows. \c 0 means to use draw rows with height according to the resize step size.
 * @param tip  Tooltip of the widget.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetMatrixDataTip(uint32_t cols, uint32_t rows, StringID tip = {})
{
	return NWidgetPart{WPT_DATATIP, NWidgetPartDataTip{{.matrix{ cols, rows }}, tip}};
}

/**
 * Widget part function for setting tooltip and clearing the widget data.
 * @param tip  Tooltip of the widget.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetToolTip(StringID tip)
{
	return NWidgetPart{WPT_DATATIP, NWidgetPartDataTip{{}, tip}};
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
constexpr NWidgetPart SetPadding(uint8_t top, uint8_t right, uint8_t bottom, uint8_t left)
{
	return NWidgetPart{WPT_PADDING, NWidgetPartPaddings{left, top, right, bottom}};
}

/**
 * Widget part function for setting additional space around a widget.
 * @param horizontal The padding on either side of the widget.
 * @param vertical The padding above and below the widget.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetPadding(uint8_t horizontal, uint8_t vertical)
{
	return NWidgetPart{WPT_PADDING, NWidgetPartPaddings{horizontal, vertical, horizontal, vertical}};
}

/**
 * Widget part function for setting additional space around a widget.
 * @param r The padding around the widget.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetPadding(const RectPadding &padding)
{
	return NWidgetPart{WPT_PADDING, NWidgetPartPaddings{padding}};
}

/**
 * Widget part function for setting a padding.
 * @param padding The padding to use for all directions.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetPadding(uint8_t padding)
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
constexpr NWidgetPart SetPIP(uint8_t pre, uint8_t inter, uint8_t post)
{
	return NWidgetPart{WPT_PIPSPACE, NWidgetPartPIP{pre, inter, post}};
}

/**
 * Widget part function for setting a pre/inter/post ratio.
 * @param pre The ratio of space before the first widget.
 * @param inter The ratio of space between widgets.
 * @param post The ratio of space after the last widget.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetPIPRatio(uint8_t ratio_pre, uint8_t ratio_inter, uint8_t ratio_post)
{
	return NWidgetPart{WPT_PIPRATIO, NWidgetPartPIP{ratio_pre, ratio_inter, ratio_post}};
}

/**
 * Attach a scrollbar to a widget.
 * The scrollbar is controlled when using the mousewheel on the widget.
 * Multiple widgets can refer to the same scrollbar to make the mousewheel work in all of them.
 * @param index Widget index of the scrollbar.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetScrollbar(WidgetID index)
{
	return NWidgetPart{WPT_SCROLLBAR, NWidgetPartWidget{INVALID_COLOUR, index}};
}

/**
 * Widget part function for setting the aspect ratio.
 * @param ratio Desired aspect ratio, or 0 for none.
 * @param flags Dimensions which should be resized.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart SetAspect(float ratio, AspectFlags flags = AspectFlag::ResizeX)
{
	return NWidgetPart{WPT_ASPECT, NWidgetPartAspect{ratio, flags}};
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
constexpr NWidgetPart NWidget(WidgetType tp, Colours col, WidgetID idx = INVALID_WIDGET)
{
	return NWidgetPart{tp, NWidgetPartWidget{col, idx}};
}

/**
 * Widget part function for starting a new horizontal container, vertical container, or spacer widget.
 * @param tp         Type of the new nested widget, #NWID_HORIZONTAL, #NWID_VERTICAL, #NWID_SPACER, #NWID_SELECTION, and #NWID_MATRIX.
 * @param cont_flags Flags for the containers (#NWID_HORIZONTAL and #NWID_VERTICAL).
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart NWidget(WidgetType tp, NWidContainerFlags cont_flags = {}, WidgetID idx = INVALID_WIDGET)
{
	return NWidgetPart{tp, NWidgetPartContainer{cont_flags, idx}};
}

/**
 * Obtain a nested widget (sub)tree from an external source.
 * @param func_ptr Pointer to function that returns the tree.
 * @ingroup NestedWidgetParts
 */
constexpr NWidgetPart NWidgetFunction(NWidgetFunctionType *func_ptr)
{
	return NWidgetPart{WPT_FUNCTION, func_ptr};
}

bool IsContainerWidgetType(WidgetType tp);
std::unique_ptr<NWidgetBase> MakeNWidgets(std::span<const NWidgetPart> nwid_parts, std::unique_ptr<NWidgetBase> &&container);
std::unique_ptr<NWidgetBase> MakeWindowNWidgetTree(std::span<const NWidgetPart> nwid_parts, NWidgetStacked **shade_select);

std::unique_ptr<NWidgetBase> MakeCompanyButtonRows(WidgetID widget_first, WidgetID widget_last, Colours button_colour, int max_length, StringID button_tooltip, bool resizable = true);

void SetupWidgetDimensions();

#endif /* WIDGET_TYPE_H */
