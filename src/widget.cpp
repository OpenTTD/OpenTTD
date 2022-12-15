/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file widget.cpp Handling of the default/simple widgets. */

#include "stdafx.h"
#include "company_func.h"
#include "window_gui.h"
#include "viewport_func.h"
#include "zoom_func.h"
#include "strings_func.h"
#include "transparency.h"
#include "core/geometry_func.hpp"
#include "settings_type.h"
#include "querystring_gui.h"

#include "table/sprites.h"
#include "table/strings.h"
#include "table/string_colours.h"

#include "safeguards.h"

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

	WD_VSCROLLBAR_LEFT   = 2,   ///< Left offset of vertical scrollbar.
	WD_VSCROLLBAR_RIGHT  = 2,   ///< Right offset of vertical scrollbar.
	WD_VSCROLLBAR_TOP    = 3,   ///< Top offset of vertical scrollbar.
	WD_VSCROLLBAR_BOTTOM = 3,   ///< Bottom offset of vertical scrollbar.

	WD_HSCROLLBAR_LEFT   = 3,   ///< Left offset of horizontal scrollbar.
	WD_HSCROLLBAR_RIGHT  = 3,   ///< Right offset of horizontal scrollbar.
	WD_HSCROLLBAR_TOP    = 2,   ///< Top offset of horizontal scrollbar.
	WD_HSCROLLBAR_BOTTOM = 2,   ///< Bottom offset of horizontal scrollbar.

	/* Size of the pure frame bevel without any padding. */
	WD_BEVEL_LEFT       = 1,    ///< Width of left bevel border.
	WD_BEVEL_RIGHT      = 1,    ///< Width of right bevel border.
	WD_BEVEL_TOP        = 1,    ///< Height of top bevel border.
	WD_BEVEL_BOTTOM     = 1,    ///< Height of bottom bevel border.

	/* FrameRect widgets, all text buttons, panel, editbox */
	WD_FRAMERECT_LEFT   = 2,    ///< Offset at left to draw the frame rectangular area
	WD_FRAMERECT_RIGHT  = 2,    ///< Offset at right to draw the frame rectangular area
	WD_FRAMERECT_TOP    = 1,    ///< Offset at top to draw the frame rectangular area
	WD_FRAMERECT_BOTTOM = 1,    ///< Offset at bottom to draw the frame rectangular area

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

	/* WWT_DEFSIZEBOX */
	WD_DEFSIZEBOX_WIDTH  = 12,  ///< Width of a standard defsize box widget.
	WD_DEFSIZEBOX_LEFT   = 2,   ///< Left offset of defsize sprite.
	WD_DEFSIZEBOX_RIGHT  = 2,   ///< Right offset of defsize sprite.
	WD_DEFSIZEBOX_TOP    = 3,   ///< Top offset of defsize sprite.
	WD_DEFSIZEBOX_BOTTOM = 3,   ///< Bottom offset of defsize sprite.

	/* WWT_RESIZEBOX */
	WD_RESIZEBOX_WIDTH  = 12,   ///< Width of a resize box widget.
	WD_RESIZEBOX_LEFT   = 2,    ///< Left offset of resize sprite.
	WD_RESIZEBOX_RIGHT  = 2,    ///< Right offset of resize sprite.
	WD_RESIZEBOX_TOP    = 2,    ///< Top offset of resize sprite.
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
	WD_DROPDOWNTEXT_RIGHT  = 2,  ///< Right offset of the dropdown widget string.
	WD_DROPDOWNTEXT_TOP    = 1,  ///< Top offset of the dropdown widget string.
	WD_DROPDOWNTEXT_BOTTOM = 1,  ///< Bottom offset of the dropdown widget string.

	WD_PAR_VSEP_NORMAL = 2,      ///< Normal amount of vertical space between two paragraphs of text.
	WD_PAR_VSEP_WIDE   = 8,      ///< Large amount of vertical space between two paragraphs of text.
};

const WidgetDimensions WidgetDimensions::unscaled = {
	{WD_IMGBTN_LEFT,       WD_IMGBTN_TOP,       WD_IMGBTN_RIGHT,       WD_IMGBTN_BOTTOM},       ///< imgbtn
	{WD_INSET_LEFT,        WD_INSET_TOP,        WD_INSET_RIGHT,        WD_BEVEL_BOTTOM},        ///< inset
	{WD_VSCROLLBAR_LEFT,   WD_VSCROLLBAR_TOP,   WD_VSCROLLBAR_RIGHT,   WD_VSCROLLBAR_BOTTOM},   ///< vscrollbar
	{WD_HSCROLLBAR_LEFT,   WD_HSCROLLBAR_TOP,   WD_HSCROLLBAR_RIGHT,   WD_HSCROLLBAR_BOTTOM},   ///< hscrollbar
	{WD_BEVEL_LEFT,        WD_BEVEL_TOP,        WD_BEVEL_RIGHT,        WD_BEVEL_BOTTOM},        ///< bevel
	{WD_BEVEL_LEFT,        WD_BEVEL_TOP,        WD_BEVEL_RIGHT,        WD_BEVEL_BOTTOM},        ///< fullbevel
	{WD_FRAMERECT_LEFT,    WD_FRAMERECT_TOP,    WD_FRAMERECT_RIGHT,    WD_FRAMERECT_BOTTOM},    ///< framerect
	{WD_FRAMETEXT_LEFT,    WD_FRAMETEXT_TOP,    WD_FRAMETEXT_RIGHT,    WD_FRAMETEXT_BOTTOM},    ///< frametext
	{WD_MATRIX_LEFT,       WD_MATRIX_TOP,       WD_MATRIX_RIGHT,       WD_MATRIX_BOTTOM},       ///< matrix
	{WD_SHADEBOX_LEFT,     WD_SHADEBOX_TOP,     WD_SHADEBOX_RIGHT,     WD_SHADEBOX_BOTTOM},     ///< shadebox
	{WD_STICKYBOX_LEFT,    WD_STICKYBOX_TOP,    WD_STICKYBOX_RIGHT,    WD_STICKYBOX_BOTTOM},    ///< stickybox
	{WD_DEBUGBOX_LEFT,     WD_DEBUGBOX_TOP,     WD_DEBUGBOX_RIGHT,     WD_DEBUGBOX_BOTTOM},     ///< debugbox
	{WD_DEFSIZEBOX_LEFT,   WD_DEFSIZEBOX_TOP,   WD_DEFSIZEBOX_RIGHT,   WD_DEFSIZEBOX_BOTTOM},   ///< defsizebox
	{WD_RESIZEBOX_LEFT,    WD_RESIZEBOX_TOP,    WD_RESIZEBOX_RIGHT,    WD_RESIZEBOX_BOTTOM},    ///< resizebox
	{WD_CLOSEBOX_LEFT,     WD_CLOSEBOX_TOP,     WD_CLOSEBOX_RIGHT,     WD_CLOSEBOX_BOTTOM},     ///< closebox
	{WD_CAPTIONTEXT_LEFT,  WD_CAPTIONTEXT_TOP,  WD_CAPTIONTEXT_RIGHT,  WD_CAPTIONTEXT_BOTTOM},  ///< captiontext
	{WD_DROPDOWNTEXT_LEFT, WD_DROPDOWNTEXT_TOP, WD_DROPDOWNTEXT_RIGHT, WD_DROPDOWNTEXT_BOTTOM}, ///< dropdowntext
	{20, 10, 20, 10},    ///< modalpopup
	1,                   ///< pressed
	WD_PAR_VSEP_NORMAL,  ///< vsep_normal
	WD_PAR_VSEP_WIDE,    ///< vsep_wide
	2,                   ///< hsep_normal
	6,                   ///< hsep_wide
	10,                  ///< hsep_indent
};

WidgetDimensions WidgetDimensions::scaled = {};

/**
 * Scale a RectPadding to GUI zoom level.
 * @param r RectPadding at ZOOM_LVL_BASE (traditional "normal" interface size).
 * @return RectPadding at #ZOOM_LVL_GUI (current interface size).
 */
static inline RectPadding ScaleGUITrad(const RectPadding &r)
{
	return {(uint8)ScaleGUITrad(r.left), (uint8)ScaleGUITrad(r.top), (uint8)ScaleGUITrad(r.right), (uint8)ScaleGUITrad(r.bottom)};
}

/**
 * Scale a Dimension to GUI zoom level.
 * @param d Dimension at ZOOM_LVL_BASE (traditional "normal" interface size).
 * @return Dimension at #ZOOM_LVL_GUI (current interface size).
 */
static inline Dimension ScaleGUITrad(const Dimension &dim)
{
	return {(uint)ScaleGUITrad(dim.width), (uint)ScaleGUITrad(dim.height)};
}

/**
 * Scale sprite size for GUI.
 * Offset is ignored.
 */
Dimension GetScaledSpriteSize(SpriteID sprid)
{
	Point offset;
	Dimension d = GetSpriteSize(sprid, &offset, ZOOM_LVL_OUT_4X);
	d.width  -= offset.x;
	d.height -= offset.y;
	return ScaleGUITrad(d);
}

/**
 * Set up pre-scaled versions of Widget Dimensions.
 */
void SetupWidgetDimensions()
{
	WidgetDimensions::scaled.imgbtn       = ScaleGUITrad(WidgetDimensions::unscaled.imgbtn);
	WidgetDimensions::scaled.inset        = ScaleGUITrad(WidgetDimensions::unscaled.inset);
	WidgetDimensions::scaled.vscrollbar   = ScaleGUITrad(WidgetDimensions::unscaled.vscrollbar);
	WidgetDimensions::scaled.hscrollbar   = ScaleGUITrad(WidgetDimensions::unscaled.hscrollbar);
	if (_settings_client.gui.scale_bevels) {
		WidgetDimensions::scaled.bevel    = ScaleGUITrad(WidgetDimensions::unscaled.bevel);
	} else {
		WidgetDimensions::scaled.bevel    = WidgetDimensions::unscaled.bevel;
	}
	WidgetDimensions::scaled.fullbevel    = ScaleGUITrad(WidgetDimensions::unscaled.fullbevel);
	WidgetDimensions::scaled.framerect    = ScaleGUITrad(WidgetDimensions::unscaled.framerect);
	WidgetDimensions::scaled.frametext    = ScaleGUITrad(WidgetDimensions::unscaled.frametext);
	WidgetDimensions::scaled.matrix       = ScaleGUITrad(WidgetDimensions::unscaled.matrix);
	WidgetDimensions::scaled.shadebox     = ScaleGUITrad(WidgetDimensions::unscaled.shadebox);
	WidgetDimensions::scaled.stickybox    = ScaleGUITrad(WidgetDimensions::unscaled.stickybox);
	WidgetDimensions::scaled.debugbox     = ScaleGUITrad(WidgetDimensions::unscaled.debugbox);
	WidgetDimensions::scaled.defsizebox   = ScaleGUITrad(WidgetDimensions::unscaled.defsizebox);
	WidgetDimensions::scaled.resizebox    = ScaleGUITrad(WidgetDimensions::unscaled.resizebox);
	WidgetDimensions::scaled.closebox     = ScaleGUITrad(WidgetDimensions::unscaled.closebox);
	WidgetDimensions::scaled.captiontext  = ScaleGUITrad(WidgetDimensions::unscaled.captiontext);
	WidgetDimensions::scaled.dropdowntext = ScaleGUITrad(WidgetDimensions::unscaled.dropdowntext);
	WidgetDimensions::scaled.modalpopup   = ScaleGUITrad(WidgetDimensions::unscaled.modalpopup);

	WidgetDimensions::scaled.pressed      = ScaleGUITrad(WidgetDimensions::unscaled.pressed);
	WidgetDimensions::scaled.vsep_normal  = ScaleGUITrad(WidgetDimensions::unscaled.vsep_normal);
	WidgetDimensions::scaled.vsep_wide    = ScaleGUITrad(WidgetDimensions::unscaled.vsep_wide);
	WidgetDimensions::scaled.hsep_normal  = ScaleGUITrad(WidgetDimensions::unscaled.hsep_normal);
	WidgetDimensions::scaled.hsep_wide    = ScaleGUITrad(WidgetDimensions::unscaled.hsep_wide);
	WidgetDimensions::scaled.hsep_indent  = ScaleGUITrad(WidgetDimensions::unscaled.hsep_indent);
}

/**
 * Calculate x and y coordinates for an aligned object within a window.
 * @param r     Rectangle of the widget to be drawn in.
 * @param d     Dimension of the object to be drawn.
 * @param align Alignment of the object.
 * @return A point containing the position at which to draw.
 */
static inline Point GetAlignedPosition(const Rect &r, const Dimension &d, StringAlignment align)
{
	Point p;
	/* In case we have a RTL language we swap the alignment. */
	if (!(align & SA_FORCE) && _current_text_dir == TD_RTL && (align & SA_HOR_MASK) != SA_HOR_CENTER) align ^= SA_RIGHT;
	switch (align & SA_HOR_MASK) {
		case SA_LEFT:       p.x = r.left; break;
		case SA_HOR_CENTER: p.x = CenterBounds(r.left, r.right, d.width); break;
		case SA_RIGHT:      p.x = r.right + 1 - d.width; break;
		default: NOT_REACHED();
	}
	switch (align & SA_VERT_MASK) {
		case SA_TOP:         p.y = r.top; break;
		case SA_VERT_CENTER: p.y = CenterBounds(r.top, r.bottom, d.height); break;
		case SA_BOTTOM:      p.y = r.bottom + 1 - d.height; break;
		default: NOT_REACHED();
	}
	return p;
}

/**
 * Compute the vertical position of the draggable part of scrollbar
 * @param sb     Scrollbar list data
 * @param top    Top position of the scrollbar (top position of the up-button)
 * @param bottom Bottom position of the scrollbar (bottom position of the down-button)
 * @param horizontal Whether the scrollbar is horizontal or not
 * @return A Point, with x containing the top coordinate of the draggable part, and
 *                       y containing the bottom coordinate of the draggable part
 */
static Point HandleScrollbarHittest(const Scrollbar *sb, int top, int bottom, bool horizontal)
{
	/* Base for reversion */
	int rev_base = top + bottom;
	int button_size;
	if (horizontal) {
		button_size = NWidgetScrollbar::GetHorizontalDimension().width;
	} else {
		button_size = NWidgetScrollbar::GetVerticalDimension().height;
	}
	top += button_size;    // top    points to just below the up-button
	bottom -= button_size; // bottom points to top of the down-button

	int height = (bottom - top);
	int pos = sb->GetPosition();
	int count = sb->GetCount();
	int cap = sb->GetCapacity();

	if (count != 0) top += height * pos / count;

	if (cap > count) cap = count;
	if (count != 0) bottom -= (count - pos - cap) * height / count;

	Point pt;
	if (horizontal && _current_text_dir == TD_RTL) {
		pt.x = rev_base - bottom;
		pt.y = rev_base - top;
	} else {
		pt.x = top;
		pt.y = bottom;
	}
	return pt;
}

/**
 * Compute new position of the scrollbar after a click and updates the window flags.
 * @param w   Window on which a scroll was performed.
 * @param sb  Scrollbar
 * @param mi  Minimum coordinate of the scroll bar.
 * @param ma  Maximum coordinate of the scroll bar.
 * @param x   The X coordinate of the mouse click.
 * @param y   The Y coordinate of the mouse click.
 */
static void ScrollbarClickPositioning(Window *w, NWidgetScrollbar *sb, int x, int y, int mi, int ma)
{
	int pos;
	int button_size;
	bool rtl = false;
	bool changed = false;

	if (sb->type == NWID_HSCROLLBAR) {
		pos = x;
		rtl = _current_text_dir == TD_RTL;
		button_size = NWidgetScrollbar::GetHorizontalDimension().width;
	} else {
		pos = y;
		button_size = NWidgetScrollbar::GetVerticalDimension().height;
	}
	if (pos < mi + button_size) {
		/* Pressing the upper button? */
		SetBit(sb->disp_flags, NDB_SCROLLBAR_UP);
		if (_scroller_click_timeout <= 1) {
			_scroller_click_timeout = 3;
			changed = sb->UpdatePosition(rtl ? 1 : -1);
		}
		w->mouse_capture_widget = sb->index;
	} else if (pos >= ma - button_size) {
		/* Pressing the lower button? */
		SetBit(sb->disp_flags, NDB_SCROLLBAR_DOWN);

		if (_scroller_click_timeout <= 1) {
			_scroller_click_timeout = 3;
			changed = sb->UpdatePosition(rtl ? -1 : 1);
		}
		w->mouse_capture_widget = sb->index;
	} else {
		Point pt = HandleScrollbarHittest(sb, mi, ma, sb->type == NWID_HSCROLLBAR);

		if (pos < pt.x) {
			changed = sb->UpdatePosition(rtl ? 1 : -1, Scrollbar::SS_BIG);
		} else if (pos > pt.y) {
			changed = sb->UpdatePosition(rtl ? -1 : 1, Scrollbar::SS_BIG);
		} else {
			_scrollbar_start_pos = pt.x - mi - button_size;
			_scrollbar_size = ma - mi - button_size * 2;
			w->mouse_capture_widget = sb->index;
			_cursorpos_drag_start = _cursor.pos;
		}
	}

	if (changed) {
		/* Position changed so refresh the window */
		w->SetDirty();
	} else {
		/* No change so only refresh this scrollbar */
		sb->SetDirty(w);
	}
}

/**
 * Special handling for the scrollbar widget type.
 * Handles the special scrolling buttons and other scrolling.
 * @param w Window on which a scroll was performed.
 * @param nw Pointer to the scrollbar widget.
 * @param x The X coordinate of the mouse click.
 * @param y The Y coordinate of the mouse click.
 */
void ScrollbarClickHandler(Window *w, NWidgetCore *nw, int x, int y)
{
	int mi, ma;

	if (nw->type == NWID_HSCROLLBAR) {
		mi = nw->pos_x;
		ma = nw->pos_x + nw->current_x;
	} else {
		mi = nw->pos_y;
		ma = nw->pos_y + nw->current_y;
	}
	NWidgetScrollbar *scrollbar = dynamic_cast<NWidgetScrollbar*>(nw);
	assert(scrollbar != nullptr);
	ScrollbarClickPositioning(w, scrollbar, x, y, mi, ma);
}

/**
 * Returns the index for the widget located at the given position
 * relative to the window. It includes all widget-corner pixels as well.
 * @param *w Window to look inside
 * @param  x The Window client X coordinate
 * @param  y The Window client y coordinate
 * @return A widget index, or -1 if no widget was found.
 */
int GetWidgetFromPos(const Window *w, int x, int y)
{
	NWidgetCore *nw = w->nested_root->GetWidgetFromPos(x, y);
	return (nw != nullptr) ? nw->index : -1;
}

/**
 * Draw frame rectangle.
 * @param left   Left edge of the frame
 * @param top    Top edge of the frame
 * @param right  Right edge of the frame
 * @param bottom Bottom edge of the frame
 * @param colour Colour table to use. @see _colour_gradient
 * @param flags  Flags controlling how to draw the frame. @see FrameFlags
 */
void DrawFrameRect(int left, int top, int right, int bottom, Colours colour, FrameFlags flags)
{
	assert(colour < COLOUR_END);

	uint dark         = _colour_gradient[colour][3];
	uint medium_dark  = _colour_gradient[colour][5];
	uint medium_light = _colour_gradient[colour][6];
	uint light        = _colour_gradient[colour][7];

	if (flags & FR_TRANSPARENT) {
		GfxFillRect(left, top, right, bottom, PALETTE_TO_TRANSPARENT, FILLRECT_RECOLOUR);
	} else {
		uint interior;

		Rect outer = {left, top, right, bottom};                   // Outside rectangle
		Rect inner = outer.Shrink(WidgetDimensions::scaled.bevel); // Inside rectangle

		if (flags & FR_LOWERED) {
			GfxFillRect(outer.left,      outer.top,        inner.left - 1,  outer.bottom, dark);   // Left
			GfxFillRect(inner.left,      outer.top,        outer.right,     inner.top - 1, dark);  // Top
			GfxFillRect(inner.right + 1, inner.top,        outer.right,     inner.bottom,  light); // Right
			GfxFillRect(inner.left,      inner.bottom + 1, outer.right,     outer.bottom, light);  // Bottom
			interior = (flags & FR_DARKENED ? medium_dark : medium_light);
		} else {
			GfxFillRect(outer.left,      outer.top,        inner.left - 1, inner.bottom,  light); // Left
			GfxFillRect(inner.left,      outer.top,        inner.right,    inner.top - 1, light); // Top
			GfxFillRect(inner.right + 1, outer.top,        outer.right,    inner.bottom,  dark);  // Right
			GfxFillRect(outer.left,      inner.bottom + 1, outer.right,    outer.bottom, dark);   // Bottom
			interior = medium_dark;
		}
		if (!(flags & FR_BORDERONLY)) {
			GfxFillRect(inner.left,  inner.top, inner.right, inner.bottom, interior); // Inner
		}
	}
}

void DrawSpriteIgnorePadding(SpriteID img, PaletteID pal, const Rect &r, bool clicked, StringAlignment align)
{
	Point offset;
	Dimension d = GetSpriteSize(img, &offset);
	d.width  -= offset.x;
	d.height -= offset.y;

	Point p = GetAlignedPosition(r, d, align);
	int o = clicked ? WidgetDimensions::scaled.pressed : 0;
	DrawSprite(img, pal, p.x + o - offset.x, p.y + o - offset.y);
}

/**
 * Draw an image button.
 * @param r       Rectangle of the button.
 * @param type    Widget type (#WWT_IMGBTN or #WWT_IMGBTN_2).
 * @param colour  Colour of the button.
 * @param clicked Button is lowered.
 * @param img     Sprite to draw.
 * @param align   Alignment of the sprite.
 */
static inline void DrawImageButtons(const Rect &r, WidgetType type, Colours colour, bool clicked, SpriteID img, StringAlignment align)
{
	assert(img != 0);
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, (clicked) ? FR_LOWERED : FR_NONE);

	if ((type & WWT_MASK) == WWT_IMGBTN_2 && clicked) img++; // Show different image when clicked for #WWT_IMGBTN_2.
	DrawSpriteIgnorePadding(img, PAL_NONE, r, clicked, align);
}

/**
 * Draw the label-part of a widget.
 * @param r       Rectangle of the label background.
 * @param type    Widget type (#WWT_TEXTBTN, #WWT_TEXTBTN_2, or #WWT_LABEL).
 * @param clicked Label is rendered lowered.
 * @param colour  Colour of the text.
 * @param str     Text to draw.
 * @param align   Alignment of the text.
 */
static inline void DrawLabel(const Rect &r, WidgetType type, bool clicked, TextColour colour, StringID str, StringAlignment align)
{
	if (str == STR_NULL) return;
	if ((type & WWT_MASK) == WWT_TEXTBTN_2 && clicked) str++;
	Dimension d = GetStringBoundingBox(str);
	Point p = GetAlignedPosition(r, d, align);
	int o = clicked ? WidgetDimensions::scaled.pressed : 0;
	DrawString(r.left + o, r.right + o, p.y + o, str, colour, align);
}

/**
 * Draw text.
 * @param r      Rectangle of the background.
 * @param colour Colour of the text.
 * @param str    Text to draw.
 * @param align  Alignment of the text.
 */
static inline void DrawText(const Rect &r, TextColour colour, StringID str, StringAlignment align)
{
	Dimension d = GetStringBoundingBox(str);
	Point p = GetAlignedPosition(r, d, align);
	if (str != STR_NULL) DrawString(r.left, r.right, p.y, str, colour, align);
}

/**
 * Draw an inset widget.
 * @param r           Rectangle of the background.
 * @param colour      Colour of the inset.
 * @param text_colour Colour of the text.
 * @param str         Text to draw.
 * @param align       Alignment of the text.
 */
static inline void DrawInset(const Rect &r, Colours colour, TextColour text_colour, StringID str, StringAlignment align)
{
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, FR_LOWERED | FR_DARKENED);
	if (str != STR_NULL) DrawString(r.Shrink(WidgetDimensions::scaled.inset), str, text_colour, align);
}

/**
 * Draw a matrix widget.
 * @param r       Rectangle of the matrix background.
 * @param colour  Colour of the background.
 * @param clicked Matrix is rendered lowered.
 * @param data    Data of the widget, number of rows and columns of the widget.
 * @param resize_x Matrix resize unit size.
 * @param resize_y Matrix resize unit size.
 */
static inline void DrawMatrix(const Rect &r, Colours colour, bool clicked, uint16 data, uint resize_x, uint resize_y)
{
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, (clicked) ? FR_LOWERED : FR_NONE);

	int num_columns = GB(data, MAT_COL_START, MAT_COL_BITS);  // Lower 8 bits of the widget data: Number of columns in the matrix.
	int column_width; // Width of a single column in the matrix.
	if (num_columns == 0) {
		column_width = resize_x;
		num_columns = r.Width() / column_width;
	} else {
		column_width = r.Width() / num_columns;
	}

	int num_rows = GB(data, MAT_ROW_START, MAT_ROW_BITS); // Upper 8 bits of the widget data: Number of rows in the matrix.
	int row_height; // Height of a single row in the matrix.
	if (num_rows == 0) {
		row_height = resize_y;
		num_rows = r.Height() / row_height;
	} else {
		row_height = r.Height() / num_rows;
	}

	int col = _colour_gradient[colour & 0xF][6];

	int x = r.left;
	for (int ctr = num_columns; ctr > 1; ctr--) {
		x += column_width;
		GfxFillRect(x, r.top + WidgetDimensions::scaled.bevel.top, x + WidgetDimensions::scaled.bevel.left - 1, r.bottom - WidgetDimensions::scaled.bevel.bottom, col);
	}

	x = r.top;
	for (int ctr = num_rows; ctr > 1; ctr--) {
		x += row_height;
		GfxFillRect(r.left + WidgetDimensions::scaled.bevel.left, x, r.right - WidgetDimensions::scaled.bevel.right, x + WidgetDimensions::scaled.bevel.top - 1, col);
	}

	col = _colour_gradient[colour & 0xF][4];

	x = r.left - 1;
	for (int ctr = num_columns; ctr > 1; ctr--) {
		x += column_width;
		GfxFillRect(x - WidgetDimensions::scaled.bevel.right + 1, r.top + WidgetDimensions::scaled.bevel.top, x, r.bottom - WidgetDimensions::scaled.bevel.bottom, col);
	}

	x = r.top - 1;
	for (int ctr = num_rows; ctr > 1; ctr--) {
		x += row_height;
		GfxFillRect(r.left + WidgetDimensions::scaled.bevel.left, x - WidgetDimensions::scaled.bevel.bottom + 1, r.right - WidgetDimensions::scaled.bevel.right, x, col);
	}
}

/**
 * Draw a vertical scrollbar.
 * @param r            Rectangle of the scrollbar widget.
 * @param colour       Colour of the scrollbar widget.
 * @param up_clicked   Up-arrow is clicked.
 * @param bar_dragged  Bar is dragged.
 * @param down_clicked Down-arrow is clicked.
 * @param scrollbar    Scrollbar size, offset, and capacity information.
 */
static inline void DrawVerticalScrollbar(const Rect &r, Colours colour, bool up_clicked, bool bar_dragged, bool down_clicked, const Scrollbar *scrollbar)
{
	int height = NWidgetScrollbar::GetVerticalDimension().height;

	/* draw up/down buttons */
	DrawImageButtons(r.WithHeight(height, false),  NWID_VSCROLLBAR, colour, up_clicked,   SPR_ARROW_UP,   SA_CENTER);
	DrawImageButtons(r.WithHeight(height, true),   NWID_VSCROLLBAR, colour, down_clicked, SPR_ARROW_DOWN, SA_CENTER);

	int c1 = _colour_gradient[colour & 0xF][3];
	int c2 = _colour_gradient[colour & 0xF][7];

	/* draw "shaded" background */
	GfxFillRect(r.left, r.top + height, r.right, r.bottom - height, c2);
	GfxFillRect(r.left, r.top + height, r.right, r.bottom - height, c1, FILLRECT_CHECKER);

	/* track positions. These fractions are based on original 1x dimensions, but scale better. */
	int left  = r.left + r.Width() * 3 / 11; /*  left track is positioned 3/11ths from the left */
	int right = r.left + r.Width() * 8 / 11; /* right track is positioned 8/11ths from the left */
	const uint8 bl = WidgetDimensions::scaled.bevel.left;
	const uint8 br = WidgetDimensions::scaled.bevel.right;

	/* draw shaded lines */
	GfxFillRect(left - bl,  r.top + height, left       - 1, r.bottom - height, c1);
	GfxFillRect(left,       r.top + height, left + br  - 1, r.bottom - height, c2);
	GfxFillRect(right - bl, r.top + height, right      - 1, r.bottom - height, c1);
	GfxFillRect(right,      r.top + height, right + br - 1, r.bottom - height, c2);

	Point pt = HandleScrollbarHittest(scrollbar, r.top, r.bottom, false);
	DrawFrameRect(r.left, pt.x, r.right, pt.y, colour, bar_dragged ? FR_LOWERED : FR_NONE);
}

/**
 * Draw a horizontal scrollbar.
 * @param r             Rectangle of the scrollbar widget.
 * @param colour        Colour of the scrollbar widget.
 * @param left_clicked  Left-arrow is clicked.
 * @param bar_dragged   Bar is dragged.
 * @param right_clicked Right-arrow is clicked.
 * @param scrollbar     Scrollbar size, offset, and capacity information.
 */
static inline void DrawHorizontalScrollbar(const Rect &r, Colours colour, bool left_clicked, bool bar_dragged, bool right_clicked, const Scrollbar *scrollbar)
{
	int width = NWidgetScrollbar::GetHorizontalDimension().width;

	DrawImageButtons(r.WithWidth(width, false), NWID_HSCROLLBAR, colour, left_clicked,  SPR_ARROW_LEFT,  SA_CENTER);
	DrawImageButtons(r.WithWidth(width, true),  NWID_HSCROLLBAR, colour, right_clicked, SPR_ARROW_RIGHT, SA_CENTER);

	int c1 = _colour_gradient[colour & 0xF][3];
	int c2 = _colour_gradient[colour & 0xF][7];

	/* draw "shaded" background */
	GfxFillRect(r.left + width, r.top, r.right - width, r.bottom, c2);
	GfxFillRect(r.left + width, r.top, r.right - width, r.bottom, c1, FILLRECT_CHECKER);

	/* track positions. These fractions are based on original 1x dimensions, but scale better. */
	int top    = r.top + r.Height() * 3 / 11; /*    top track is positioned 3/11ths from the top */
	int bottom = r.top + r.Height() * 8 / 11; /* bottom track is positioned 8/11ths from the top */
	const uint8 bt = WidgetDimensions::scaled.bevel.top;
	const uint8 bb = WidgetDimensions::scaled.bevel.bottom;

	/* draw shaded lines */
	GfxFillRect(r.left + width, top - bt,    r.right - width, top         - 1, c1);
	GfxFillRect(r.left + width, top,         r.right - width, top + bb    - 1, c2);
	GfxFillRect(r.left + width, bottom - bt, r.right - width, bottom      - 1, c1);
	GfxFillRect(r.left + width, bottom,      r.right - width, bottom + bb - 1, c2);

	/* draw actual scrollbar */
	Point pt = HandleScrollbarHittest(scrollbar, r.left, r.right, true);
	DrawFrameRect(pt.x, r.top, pt.y, r.bottom, colour, bar_dragged ? FR_LOWERED : FR_NONE);
}

/**
 * Draw a frame widget.
 * @param r           Rectangle of the frame.
 * @param colour      Colour of the frame.
 * @param text_colour Colour of the text.
 * @param str         Text of the frame.
 * @param align       Alignment of the text in the frame.
 */
static inline void DrawFrame(const Rect &r, Colours colour, TextColour text_colour, StringID str, StringAlignment align)
{
	int x2 = r.left; // by default the left side is the left side of the widget

	if (str != STR_NULL) x2 = DrawString(r.left + WidgetDimensions::scaled.frametext.left, r.right - WidgetDimensions::scaled.frametext.right, r.top, str, text_colour, align);

	int c1 = _colour_gradient[colour][3];
	int c2 = _colour_gradient[colour][7];

	/* If the frame has text, adjust the top bar to fit half-way through */
	Rect inner = r.Shrink(ScaleGUITrad(1));
	if (str != STR_NULL) inner.top = r.top + FONT_HEIGHT_NORMAL / 2;

	Rect outer  = inner.Expand(WidgetDimensions::scaled.bevel);
	Rect inside = inner.Shrink(WidgetDimensions::scaled.bevel);

	if (_current_text_dir == TD_LTR) {
		/* Line from upper left corner to start of text */
		GfxFillRect(outer.left, outer.top, r.left + WidgetDimensions::scaled.frametext.left - WidgetDimensions::scaled.bevel.left - 1, inner.top  - 1, c1);
		GfxFillRect(inner.left, inner.top, r.left + WidgetDimensions::scaled.frametext.left - WidgetDimensions::scaled.bevel.left - 1, inside.top - 1, c2);

		/* Line from end of text to upper right corner */
		GfxFillRect(x2 + WidgetDimensions::scaled.bevel.right, outer.top, inner.right,  inner.top  - 1, c1);
		GfxFillRect(x2 + WidgetDimensions::scaled.bevel.right, inner.top, inside.right, inside.top - 1, c2);
	} else {
		/* Line from upper left corner to start of text */
		GfxFillRect(outer.left, outer.top, x2 - WidgetDimensions::scaled.bevel.left - 1, inner.top  - 1, c1);
		GfxFillRect(inner.left, inner.top, x2 - WidgetDimensions::scaled.bevel.left - 1, inside.top - 1, c2);

		/* Line from end of text to upper right corner */
		GfxFillRect(r.right - WidgetDimensions::scaled.frametext.right + WidgetDimensions::scaled.bevel.right, outer.top, inner.right,  inner.top  - 1, c1);
		GfxFillRect(r.right - WidgetDimensions::scaled.frametext.right + WidgetDimensions::scaled.bevel.right, inner.top, inside.right, inside.top - 1, c2);
	}

	/* Line from upper left corner to bottom left corner */
	GfxFillRect(outer.left, inner.top,  inner.left  - 1, inner.bottom,  c1);
	GfxFillRect(inner.left, inside.top, inside.left - 1, inside.bottom, c2);

	/* Line from upper right corner to bottom right corner */
	GfxFillRect(inside.right + 1, inner.top, inner.right, inside.bottom, c1);
	GfxFillRect(inner.right  + 1, outer.top, outer.right, inner.bottom,  c2);

	/* Line from bottom left corner to bottom right corner */
	GfxFillRect(inner.left, inside.bottom + 1, inner.right, inner.bottom, c1);
	GfxFillRect(outer.left, inner.bottom  + 1, outer.right, outer.bottom, c2);
}

/**
 * Draw a shade box.
 * @param r       Rectangle of the box.
 * @param colour  Colour of the shade box.
 * @param clicked Box is lowered.
 */
static inline void DrawShadeBox(const Rect &r, Colours colour, bool clicked)
{
	DrawImageButtons(r, WWT_SHADEBOX, colour, clicked, clicked ? SPR_WINDOW_SHADE: SPR_WINDOW_UNSHADE, SA_CENTER);
}

/**
 * Draw a sticky box.
 * @param r       Rectangle of the box.
 * @param colour  Colour of the sticky box.
 * @param clicked Box is lowered.
 */
static inline void DrawStickyBox(const Rect &r, Colours colour, bool clicked)
{
	DrawImageButtons(r, WWT_STICKYBOX, colour, clicked, clicked ? SPR_PIN_UP : SPR_PIN_DOWN, SA_CENTER);
}

/**
 * Draw a defsize box.
 * @param r       Rectangle of the box.
 * @param colour  Colour of the defsize box.
 * @param clicked Box is lowered.
 */
static inline void DrawDefSizeBox(const Rect &r, Colours colour, bool clicked)
{
	DrawImageButtons(r, WWT_DEFSIZEBOX, colour, clicked, SPR_WINDOW_DEFSIZE, SA_CENTER);
}

/**
 * Draw a NewGRF debug box.
 * @param r       Rectangle of the box.
 * @param colour  Colour of the debug box.
 * @param clicked Box is lowered.
 */
static inline void DrawDebugBox(const Rect &r, Colours colour, bool clicked)
{
	DrawImageButtons(r, WWT_DEBUGBOX, colour, clicked, SPR_WINDOW_DEBUG, SA_CENTER);
}

/**
 * Draw a resize box.
 * @param r       Rectangle of the box.
 * @param colour  Colour of the resize box.
 * @param at_left Resize box is at left-side of the window,
 * @param clicked Box is lowered.
 */
static inline void DrawResizeBox(const Rect &r, Colours colour, bool at_left, bool clicked)
{
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, (clicked) ? FR_LOWERED : FR_NONE);
	DrawSpriteIgnorePadding(at_left ? SPR_WINDOW_RESIZE_LEFT : SPR_WINDOW_RESIZE_RIGHT, PAL_NONE, r.Shrink(ScaleGUITrad(2)), clicked, at_left ? (SA_LEFT | SA_BOTTOM | SA_FORCE) : (SA_RIGHT | SA_BOTTOM | SA_FORCE));
}

/**
 * Draw a close box.
 * @param r      Rectangle of the box.`
 * @param colour Colour of the close box.
 */
static inline void DrawCloseBox(const Rect &r, Colours colour)
{
	if (colour != COLOUR_WHITE) DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, FR_NONE);
	Point offset;
	Dimension d = GetSpriteSize(SPR_CLOSEBOX, &offset);
	d.width  -= offset.x;
	d.height -= offset.y;
	int s = ScaleSpriteTrad(1); /* Offset to account for shadow of SPR_CLOSEBOX */
	DrawSprite(SPR_CLOSEBOX, (colour != COLOUR_WHITE ? TC_BLACK : TC_SILVER) | (1U << PALETTE_TEXT_RECOLOUR), CenterBounds(r.left, r.right, d.width - s) - offset.x, CenterBounds(r.top, r.bottom, d.height - s) - offset.y);
}

/**
 * Draw a caption bar.
 * @param r           Rectangle of the bar.
 * @param colour      Colour of the window.
 * @param owner       'Owner' of the window.
 * @param text_colour Colour of the text.
 * @param str         Text to draw in the bar.
 * @param align       Alignment of the text.
 */
void DrawCaption(const Rect &r, Colours colour, Owner owner, TextColour text_colour, StringID str, StringAlignment align)
{
	bool company_owned = owner < MAX_COMPANIES;

	DrawFrameRect(r, colour, FR_BORDERONLY);
	Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
	DrawFrameRect(ir, colour, company_owned ? FR_LOWERED | FR_DARKENED | FR_BORDERONLY : FR_LOWERED | FR_DARKENED);

	if (company_owned) {
		GfxFillRect(ir.Shrink(WidgetDimensions::scaled.bevel), _colour_gradient[_company_colours[owner]][4]);
	}

	if (str != STR_NULL) {
		Dimension d = GetStringBoundingBox(str);
		Point p = GetAlignedPosition(r, d, align);
		DrawString(r.left + WidgetDimensions::scaled.captiontext.left, r.right - WidgetDimensions::scaled.captiontext.left, p.y, str, text_colour, align);
	}
}

/**
 * Draw a button with a dropdown (#WWT_DROPDOWN and #NWID_BUTTON_DROPDOWN).
 * @param r                Rectangle containing the widget.
 * @param colour           Background colour of the widget.
 * @param clicked_button   The button-part is lowered.
 * @param clicked_dropdown The drop-down part is lowered.
 * @param str              Text of the button.
 * @param align            Alignment of the text within the dropdown.
 *
 * @note Magic constants are also used in #NWidgetLeaf::ButtonHit.
 */
static inline void DrawButtonDropdown(const Rect &r, Colours colour, bool clicked_button, bool clicked_dropdown, StringID str, StringAlignment align)
{
	int dd_width  = NWidgetLeaf::dropdown_dimension.width;

	if (_current_text_dir == TD_LTR) {
		DrawFrameRect(r.left, r.top, r.right - dd_width, r.bottom, colour, clicked_button ? FR_LOWERED : FR_NONE);
		DrawImageButtons(r.WithWidth(dd_width, true), WWT_DROPDOWN, colour, clicked_dropdown, SPR_ARROW_DOWN, SA_CENTER);
		if (str != STR_NULL) {
			int o = clicked_button ? WidgetDimensions::scaled.pressed : 0;
			DrawString(r.left + WidgetDimensions::scaled.dropdowntext.left + o, r.right - dd_width - WidgetDimensions::scaled.dropdowntext.right + o, CenterBounds(r.top, r.bottom, FONT_HEIGHT_NORMAL) + o, str, TC_BLACK, align);
		}
	} else {
		DrawFrameRect(r.left + dd_width, r.top, r.right, r.bottom, colour, clicked_button ? FR_LOWERED : FR_NONE);
		DrawImageButtons(r.WithWidth(dd_width, false), WWT_DROPDOWN, colour, clicked_dropdown, SPR_ARROW_DOWN, SA_CENTER);
		if (str != STR_NULL) {
			int o = clicked_button ? WidgetDimensions::scaled.pressed : 0;
			DrawString(r.left + dd_width + WidgetDimensions::scaled.dropdowntext.left + o, r.right - WidgetDimensions::scaled.dropdowntext.right + o, CenterBounds(r.top, r.bottom, FONT_HEIGHT_NORMAL) + o, str, TC_BLACK, align);
		}
	}
}

/**
 * Paint all widgets of a window.
 */
void Window::DrawWidgets() const
{
	this->nested_root->Draw(this);

	if (this->flags & WF_WHITE_BORDER) {
		DrawFrameRect(0, 0, this->width - 1, this->height - 1, COLOUR_WHITE, FR_BORDERONLY);
	}

	if (this->flags & WF_HIGHLIGHTED) {
		extern bool _window_highlight_colour;
		for (uint i = 0; i < this->nested_array_size; i++) {
			const NWidgetBase *widget = this->GetWidget<NWidgetBase>(i);
			if (widget == nullptr || !widget->IsHighlighted()) continue;

			Rect outer = widget->GetCurrentRect();
			Rect inner = outer.Shrink(WidgetDimensions::scaled.bevel).Expand(1);

			int colour = _string_colourmap[_window_highlight_colour ? widget->GetHighlightColour() : TC_WHITE];

			GfxFillRect(outer.left,     outer.top,    inner.left,      inner.bottom, colour);
			GfxFillRect(inner.left + 1, outer.top,    inner.right - 1, inner.top,    colour);
			GfxFillRect(inner.right,    outer.top,    outer.right,     inner.bottom, colour);
			GfxFillRect(outer.left + 1, inner.bottom, outer.right - 1, outer.bottom, colour);
		}
	}
}

/**
 * Draw a sort button's up or down arrow symbol.
 * @param widget Sort button widget
 * @param state State of sort button
 */
void Window::DrawSortButtonState(int widget, SortButtonState state) const
{
	if (state == SBS_OFF) return;

	assert(this->nested_array != nullptr);
	Rect r = this->GetWidget<NWidgetBase>(widget)->GetCurrentRect();

	/* Sort button uses the same sprites as vertical scrollbar */
	Dimension dim = NWidgetScrollbar::GetVerticalDimension();

	DrawSpriteIgnorePadding(state == SBS_DOWN ? SPR_ARROW_DOWN : SPR_ARROW_UP, PAL_NONE, r.WithWidth(dim.width, _current_text_dir == TD_LTR), this->IsWidgetLowered(widget), SA_CENTER);
}

/**
 * Get width of up/down arrow of sort button state.
 * @return Width of space required by sort button arrow.
 */
int Window::SortButtonWidth()
{
	return NWidgetScrollbar::GetVerticalDimension().width + 1;
}


/**
 * @defgroup NestedWidgets Hierarchical widgets
 * Hierarchical widgets, also known as nested widgets, are widgets stored in a tree. At the leafs of the tree are (mostly) the 'real' widgets
 * visible to the user. At higher levels, widgets get organized in container widgets, until all widgets of the window are merged.
 *
 * \section nestedwidgetkinds Hierarchical widget kinds
 * A leaf widget is one of
 * <ul>
 * <li> #NWidgetLeaf for widgets visible for the user, or
 * <li> #NWidgetSpacer for creating (flexible) empty space between widgets.
 * </ul>
 * The purpose of a leaf widget is to provide interaction with the user by displaying settings, and/or allowing changing the settings.
 *
 * A container widget is one of
 * <ul>
 * <li> #NWidgetHorizontal for organizing child widgets in a (horizontal) row. The row switches order depending on the language setting (thus supporting
 *      right-to-left languages),
 * <li> #NWidgetHorizontalLTR for organizing child widgets in a (horizontal) row, always in the same order. All children below this container will also
 *      never swap order.
 * <li> #NWidgetVertical for organizing child widgets underneath each other.
 * <li> #NWidgetMatrix for organizing child widgets in a matrix form.
 * <li> #NWidgetBackground for adding a background behind its child widget.
 * <li> #NWidgetStacked for stacking child widgets on top of each other.
 * </ul>
 * The purpose of a container widget is to structure its leafs and sub-containers to allow proper resizing.
 *
 * \section nestedwidgetscomputations Hierarchical widget computations
 * The first 'computation' is the creation of the nested widgets tree by calling the constructors of the widgets listed above and calling \c Add() for every child,
 * or by means of specifying the tree as a collection of nested widgets parts and instantiating the tree from the array.
 *
 * After the creation step,
 * - The leafs have their own minimal size (\e min_x, \e min_y), filling (\e fill_x, \e fill_y), and resize steps (\e resize_x, \e resize_y).
 * - Containers only know what their children are, \e fill_x, \e fill_y, \e resize_x, and \e resize_y are not initialized.
 *
 * Computations in the nested widgets take place as follows:
 * <ol>
 * <li> A bottom-up sweep by recursively calling NWidgetBase::SetupSmallestSize() to initialize the smallest size (\e smallest_x, \e smallest_y) and
 *      to propagate filling and resize steps upwards to the root of the tree.
 * <li> A top-down sweep by recursively calling NWidgetBase::AssignSizePosition() with #ST_SMALLEST to make the smallest sizes consistent over
 *      the entire tree, and to assign the top-left (\e pos_x, \e pos_y) position of each widget in the tree. This step uses \e fill_x and \e fill_y at each
 *      node in the tree to decide how to fill each widget towards consistent sizes. Also the current size (\e current_x and \e current_y) is set.
 * <li> After initializing the smallest size in the widget tree with #ST_SMALLEST, the tree can be resized (the current size modified) by calling
 *      NWidgetBase::AssignSizePosition() at the root with #ST_RESIZE and the new size of the window. For proper functioning, the new size should be the smallest
 *      size + a whole number of resize steps in both directions (ie you can only resize in steps of length resize_{x,y} from smallest_{x,y}).
 * </ol>
 * After the second step, the current size of the widgets are set to the smallest size.
 *
 * To resize, perform the last step with the new window size. This can be done as often as desired.
 * When the smallest size of at least one widget changes, the whole procedure has to be redone from the start.
 *
 * @see NestedWidgetParts
 */

/**
 * Base class constructor.
 * @param tp Nested widget type.
 */
NWidgetBase::NWidgetBase(WidgetType tp) : ZeroedMemoryAllocator()
{
	this->type = tp;
}

/* ~NWidgetContainer() takes care of #next and #prev data members. */

/**
 * @fn void NWidgetBase::SetupSmallestSize(Window *w, bool init_array)
 * Compute smallest size needed by the widget.
 *
 * The smallest size of a widget is the smallest size that a widget needs to
 * display itself properly. In addition, filling and resizing of the widget are computed.
 * The function calls #Window::UpdateWidgetSize for each leaf widget and
 * background widget without child with a non-negative index.
 *
 * @param w          Window owning the widget.
 * @param init_array Initialize the \c w->nested_array.
 *
 * @note After the computation, the results can be queried by accessing the #smallest_x and #smallest_y data members of the widget.
 */

/**
 * @fn void NWidgetBase::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool rtl)
 * Assign size and position to the widget.
 * @param sizing       Type of resizing to perform.
 * @param x            Horizontal offset of the widget relative to the left edge of the window.
 * @param y            Vertical offset of the widget relative to the top edge of the window.
 * @param given_width  Width allocated to the widget.
 * @param given_height Height allocated to the widget.
 * @param rtl          Adapt for right-to-left languages (position contents of horizontal containers backwards).
 *
 * Afterwards, \e pos_x and \e pos_y contain the top-left position of the widget, \e smallest_x and \e smallest_y contain
 * the smallest size such that all widgets of the window are consistent, and \e current_x and \e current_y contain the current size.
 */

/**
 * @fn void NWidgetBase::FillNestedArray(NWidgetBase **array, uint length)
 * Fill the Window::nested_array array with pointers to nested widgets in the tree.
 * @param array Base pointer of the array.
 * @param length Length of the array.
 */

/**
 * @fn void NWidgetBase::Draw(const Window *w)
 * Draw the widgets of the tree.
 * The function calls #Window::DrawWidget for each widget with a non-negative index, after the widget itself is painted.
 * @param w Window that owns the tree.
 */

/**
 * Mark the widget as 'dirty' (in need of repaint).
 * @param w Window owning the widget.
 */
void NWidgetBase::SetDirty(const Window *w) const
{
	int abs_left = w->left + this->pos_x;
	int abs_top = w->top + this->pos_y;
	AddDirtyBlock(abs_left, abs_top, abs_left + this->current_x, abs_top + this->current_y);
}

/**
 * @fn NWidgetCore *NWidgetBase::GetWidgetFromPos(int x, int y)
 * Retrieve a widget by its position.
 * @param x Horizontal position relative to the left edge of the window.
 * @param y Vertical position relative to the top edge of the window.
 * @return Returns the deepest nested widget that covers the given position, or \c nullptr if no widget can be found.
 */

/**
 * Retrieve a widget by its type.
 * @param tp Widget type to search for.
 * @return Returns the first widget of the specified type, or \c nullptr if no widget can be found.
 */
NWidgetBase *NWidgetBase::GetWidgetOfType(WidgetType tp)
{
	return (this->type == tp) ? this : nullptr;
}

void NWidgetBase::AdjustPaddingForZoom()
{
	this->padding = ScaleGUITrad(this->uz_padding);
}

/**
 * Constructor for resizable nested widgets.
 * @param tp     Nested widget type.
 * @param fill_x Horizontal fill step size, \c 0 means no filling is allowed.
 * @param fill_y Vertical fill step size, \c 0 means no filling is allowed.
 */
NWidgetResizeBase::NWidgetResizeBase(WidgetType tp, uint fill_x, uint fill_y) : NWidgetBase(tp)
{
	this->fill_x = fill_x;
	this->fill_y = fill_y;
}

void NWidgetResizeBase::AdjustPaddingForZoom()
{
	if (!this->absolute) {
		this->min_x = ScaleGUITrad(this->uz_min_x);
		this->min_y = std::max(ScaleGUITrad(this->uz_min_y), this->uz_text_lines * GetCharacterHeight(this->uz_text_size) + ScaleGUITrad(this->uz_text_spacing));
	}
	NWidgetBase::AdjustPaddingForZoom();
}

/**
 * Set minimal size of the widget.
 * @param min_x Horizontal minimal size of the widget.
 * @param min_y Vertical minimal size of the widget.
 */
void NWidgetResizeBase::SetMinimalSize(uint min_x, uint min_y)
{
	this->uz_min_x = std::max(this->uz_min_x, min_x);
	this->uz_min_y = std::max(this->uz_min_y, min_y);
	this->min_x = ScaleGUITrad(this->uz_min_x);
	this->min_y = std::max(ScaleGUITrad(this->uz_min_y), this->uz_text_lines * GetCharacterHeight(this->uz_text_size) + ScaleGUITrad(this->uz_text_spacing));
}

/**
 * Set absolute (post-scaling) minimal size of the widget.
 * @param min_x Horizontal minimal size of the widget.
 * @param min_y Vertical minimal size of the widget.
 */
void NWidgetResizeBase::SetMinimalSizeAbsolute(uint min_x, uint min_y)
{
	this->absolute = true;
	this->min_x = std::max(this->min_x, min_x);
	this->min_y = std::max(this->min_y, min_y);
}

/**
 * Set minimal text lines for the widget.
 * @param min_lines Number of text lines of the widget.
 * @param spacing   Extra unscaled spacing (eg WidgetDimensions::unscaled.framerect.Vertical()) of the widget.
 * @param size      Font size of text.
 */
void NWidgetResizeBase::SetMinimalTextLines(uint8 min_lines, uint8 spacing, FontSize size)
{
	this->uz_text_lines = min_lines;
	this->uz_text_spacing = spacing;
	this->uz_text_size = size;
	this->min_y = std::max(ScaleGUITrad(this->uz_min_y), this->uz_text_lines * GetCharacterHeight(this->uz_text_size) + ScaleGUITrad(this->uz_text_spacing));
}

/**
 * Set the filling of the widget from initial size.
 * @param fill_x Horizontal fill step size, \c 0 means no filling is allowed.
 * @param fill_y Vertical fill step size, \c 0 means no filling is allowed.
 */
void NWidgetResizeBase::SetFill(uint fill_x, uint fill_y)
{
	this->fill_x = fill_x;
	this->fill_y = fill_y;
}

/**
 * Set resize step of the widget.
 * @param resize_x Resize step in horizontal direction, value \c 0 means no resize, otherwise the step size in pixels.
 * @param resize_y Resize step in vertical direction, value \c 0 means no resize, otherwise the step size in pixels.
 */
void NWidgetResizeBase::SetResize(uint resize_x, uint resize_y)
{
	this->resize_x = resize_x;
	this->resize_y = resize_y;
}

void NWidgetResizeBase::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool rtl)
{
	this->StoreSizePosition(sizing, x, y, given_width, given_height);
}

/**
 * Initialization of a 'real' widget.
 * @param tp          Type of the widget.
 * @param colour      Colour of the widget.
 * @param fill_x      Default horizontal filling.
 * @param fill_y      Default vertical filling.
 * @param widget_data Data component of the widget. @see Widget::data
 * @param tool_tip    Tool tip of the widget. @see Widget::tooltips
 */
NWidgetCore::NWidgetCore(WidgetType tp, Colours colour, uint fill_x, uint fill_y, uint32 widget_data, StringID tool_tip) : NWidgetResizeBase(tp, fill_x, fill_y)
{
	this->colour = colour;
	this->index = -1;
	this->widget_data = widget_data;
	this->tool_tip = tool_tip;
	this->scrollbar_index = -1;
	this->text_colour = TC_BLACK;
	this->align = SA_CENTER;
}

/**
 * Set index of the nested widget in the widget array.
 * @param index Index to use.
 */
void NWidgetCore::SetIndex(int index)
{
	assert(index >= 0);
	this->index = index;
}

/**
 * Set data and tool tip of the nested widget.
 * @param widget_data Data to use.
 * @param tool_tip    Tool tip string to use.
 */
void NWidgetCore::SetDataTip(uint32 widget_data, StringID tool_tip)
{
	this->widget_data = widget_data;
	this->tool_tip = tool_tip;
}

/**
 * Set the text colour of the nested widget.
 * @param colour TextColour to use.
 */
void NWidgetCore::SetTextColour(TextColour colour)
{
	this->text_colour = colour;
}

/**
 * Set the tool tip of the nested widget.
 * @param tool_tip Tool tip string to use.
 */
void NWidgetCore::SetToolTip(StringID tool_tip)
{
	this->tool_tip = tool_tip;
}

/**
 * Set the text/image alignment of the nested widget.
 * @param align Alignment to use.
 */
void NWidgetCore::SetAlignment(StringAlignment align)
{
	this->align = align;
}

void NWidgetCore::FillNestedArray(NWidgetBase **array, uint length)
{
	if (this->index >= 0 && (uint)(this->index) < length) array[this->index] = this;
}

NWidgetCore *NWidgetCore::GetWidgetFromPos(int x, int y)
{
	return (IsInsideBS(x, this->pos_x, this->current_x) && IsInsideBS(y, this->pos_y, this->current_y)) ? this : nullptr;
}

/**
 * Constructor container baseclass.
 * @param tp Type of the container.
 */
NWidgetContainer::NWidgetContainer(WidgetType tp) : NWidgetBase(tp)
{
	this->head = nullptr;
	this->tail = nullptr;
}

NWidgetContainer::~NWidgetContainer()
{
	while (this->head != nullptr) {
		NWidgetBase *wid = this->head->next;
		delete this->head;
		this->head = wid;
	}
	this->tail = nullptr;
}

NWidgetBase *NWidgetContainer::GetWidgetOfType(WidgetType tp)
{
	if (this->type == tp) return this;
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		NWidgetBase *nwid = child_wid->GetWidgetOfType(tp);
		if (nwid != nullptr) return nwid;
	}
	return nullptr;
}

void NWidgetContainer::AdjustPaddingForZoom()
{
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		child_wid->AdjustPaddingForZoom();
	}
	NWidgetBase::AdjustPaddingForZoom();
}

/**
 * Append widget \a wid to container.
 * @param wid Widget to append.
 */
void NWidgetContainer::Add(NWidgetBase *wid)
{
	assert(wid->next == nullptr && wid->prev == nullptr);

	if (this->head == nullptr) {
		this->head = wid;
		this->tail = wid;
	} else {
		assert(this->tail != nullptr);
		assert(this->tail->next == nullptr);

		this->tail->next = wid;
		wid->prev = this->tail;
		this->tail = wid;
	}
}

void NWidgetContainer::FillNestedArray(NWidgetBase **array, uint length)
{
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		child_wid->FillNestedArray(array, length);
	}
}

/**
 * Widgets stacked on top of each other.
 */
NWidgetStacked::NWidgetStacked() : NWidgetContainer(NWID_SELECTION)
{
	this->index = -1;
}

void NWidgetStacked::SetIndex(int index)
{
	this->index = index;
}

void NWidgetStacked::AdjustPaddingForZoom()
{
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		child_wid->AdjustPaddingForZoom();
	}
	NWidgetContainer::AdjustPaddingForZoom();
}

void NWidgetStacked::SetupSmallestSize(Window *w, bool init_array)
{
	if (this->index >= 0 && init_array) { // Fill w->nested_array[]
		assert(w->nested_array_size > (uint)this->index);
		w->nested_array[this->index] = this;
	}

	/* Zero size plane selected */
	if (this->shown_plane >= SZSP_BEGIN) {
		Dimension size    = {0, 0};
		Dimension padding = {0, 0};
		Dimension fill    = {(this->shown_plane == SZSP_HORIZONTAL), (this->shown_plane == SZSP_VERTICAL)};
		Dimension resize  = {(this->shown_plane == SZSP_HORIZONTAL), (this->shown_plane == SZSP_VERTICAL)};
		/* Here we're primarily interested in the value of resize */
		if (this->index >= 0) w->UpdateWidgetSize(this->index, &size, padding, &fill, &resize);

		this->smallest_x = size.width;
		this->smallest_y = size.height;
		this->fill_x = fill.width;
		this->fill_y = fill.height;
		this->resize_x = resize.width;
		this->resize_y = resize.height;
		return;
	}

	/* First sweep, recurse down and compute minimal size and filling. */
	this->smallest_x = 0;
	this->smallest_y = 0;
	this->fill_x = (this->head != nullptr) ? 1 : 0;
	this->fill_y = (this->head != nullptr) ? 1 : 0;
	this->resize_x = (this->head != nullptr) ? 1 : 0;
	this->resize_y = (this->head != nullptr) ? 1 : 0;
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		child_wid->SetupSmallestSize(w, init_array);

		this->smallest_x = std::max(this->smallest_x, child_wid->smallest_x + child_wid->padding.Horizontal());
		this->smallest_y = std::max(this->smallest_y, child_wid->smallest_y + child_wid->padding.Vertical());
		this->fill_x = LeastCommonMultiple(this->fill_x, child_wid->fill_x);
		this->fill_y = LeastCommonMultiple(this->fill_y, child_wid->fill_y);
		this->resize_x = LeastCommonMultiple(this->resize_x, child_wid->resize_x);
		this->resize_y = LeastCommonMultiple(this->resize_y, child_wid->resize_y);
	}
}

void NWidgetStacked::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool rtl)
{
	assert(given_width >= this->smallest_x && given_height >= this->smallest_y);
	this->StoreSizePosition(sizing, x, y, given_width, given_height);

	if (this->shown_plane >= SZSP_BEGIN) return;

	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		uint hor_step = (sizing == ST_SMALLEST) ? 1 : child_wid->GetHorizontalStepSize(sizing);
		uint child_width = ComputeMaxSize(child_wid->smallest_x, given_width - child_wid->padding.Horizontal(), hor_step);
		uint child_pos_x = (rtl ? child_wid->padding.right : child_wid->padding.left);

		uint vert_step = (sizing == ST_SMALLEST) ? 1 : child_wid->GetVerticalStepSize(sizing);
		uint child_height = ComputeMaxSize(child_wid->smallest_y, given_height - child_wid->padding.Vertical(), vert_step);
		uint child_pos_y = child_wid->padding.top;

		child_wid->AssignSizePosition(sizing, x + child_pos_x, y + child_pos_y, child_width, child_height, rtl);
	}
}

void NWidgetStacked::FillNestedArray(NWidgetBase **array, uint length)
{
	if (this->index >= 0 && (uint)(this->index) < length) array[this->index] = this;
	NWidgetContainer::FillNestedArray(array, length);
}

void NWidgetStacked::Draw(const Window *w)
{
	if (this->shown_plane >= SZSP_BEGIN) return;

	int plane = 0;
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; plane++, child_wid = child_wid->next) {
		if (plane == this->shown_plane) {
			child_wid->Draw(w);
			return;
		}
	}

	NOT_REACHED();
}

NWidgetCore *NWidgetStacked::GetWidgetFromPos(int x, int y)
{
	if (this->shown_plane >= SZSP_BEGIN) return nullptr;

	if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return nullptr;
	int plane = 0;
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; plane++, child_wid = child_wid->next) {
		if (plane == this->shown_plane) {
			return child_wid->GetWidgetFromPos(x, y);
		}
	}
	return nullptr;
}

/**
 * Select which plane to show (for #NWID_SELECTION only).
 * @param plane Plane number to display.
 */
void NWidgetStacked::SetDisplayedPlane(int plane)
{
	this->shown_plane = plane;
}

NWidgetPIPContainer::NWidgetPIPContainer(WidgetType tp, NWidContainerFlags flags) : NWidgetContainer(tp)
{
	this->flags = flags;
}

void NWidgetPIPContainer::AdjustPaddingForZoom()
{
	this->pip_pre = ScaleGUITrad(this->uz_pip_pre);
	this->pip_inter = ScaleGUITrad(this->uz_pip_inter);
	this->pip_post = ScaleGUITrad(this->uz_pip_post);
	NWidgetContainer::AdjustPaddingForZoom();
}

/**
 * Set additional pre/inter/post space for the container.
 *
 * @param pip_pre   Additional space in front of the first child widget (above
 *                  for the vertical container, at the left for the horizontal container).
 * @param pip_inter Additional space between two child widgets.
 * @param pip_post  Additional space after the last child widget (below for the
 *                  vertical container, at the right for the horizontal container).
 */
void NWidgetPIPContainer::SetPIP(uint8 pip_pre, uint8 pip_inter, uint8 pip_post)
{
	this->uz_pip_pre = pip_pre;
	this->uz_pip_inter = pip_inter;
	this->uz_pip_post = pip_post;

	this->pip_pre = ScaleGUITrad(this->uz_pip_pre);
	this->pip_inter = ScaleGUITrad(this->uz_pip_inter);
	this->pip_post = ScaleGUITrad(this->uz_pip_post);
}

void NWidgetPIPContainer::Draw(const Window *w)
{
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		child_wid->Draw(w);
	}
}

NWidgetCore *NWidgetPIPContainer::GetWidgetFromPos(int x, int y)
{
	if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return nullptr;

	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		NWidgetCore *nwid = child_wid->GetWidgetFromPos(x, y);
		if (nwid != nullptr) return nwid;
	}
	return nullptr;
}

/** Horizontal container widget. */
NWidgetHorizontal::NWidgetHorizontal(NWidContainerFlags flags) : NWidgetPIPContainer(NWID_HORIZONTAL, flags)
{
}

void NWidgetHorizontal::SetupSmallestSize(Window *w, bool init_array)
{
	this->smallest_x = 0; // Sum of minimal size of all children.
	this->smallest_y = 0; // Biggest child.
	this->fill_x = 0;     // smallest non-zero child widget fill step.
	this->fill_y = 1;     // smallest common child fill step.
	this->resize_x = 0;   // smallest non-zero child widget resize step.
	this->resize_y = 1;   // smallest common child resize step.

	/* 1a. Forward call, collect biggest nested array index, and longest/widest child length. */
	uint longest = 0; // Longest child found.
	uint max_vert_fill = 0; // Biggest vertical fill step.
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		child_wid->SetupSmallestSize(w, init_array);
		longest = std::max(longest, child_wid->smallest_x);
		max_vert_fill = std::max(max_vert_fill, child_wid->GetVerticalStepSize(ST_SMALLEST));
		this->smallest_y = std::max(this->smallest_y, child_wid->smallest_y + child_wid->padding.Vertical());
	}
	/* 1b. Make the container higher if needed to accommodate all children nicely. */
	[[maybe_unused]] uint max_smallest = this->smallest_y + 3 * max_vert_fill; // Upper limit to computing smallest height.
	uint cur_height = this->smallest_y;
	for (;;) {
		for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
			uint step_size = child_wid->GetVerticalStepSize(ST_SMALLEST);
			uint child_height = child_wid->smallest_y + child_wid->padding.Vertical();
			if (step_size > 1 && child_height < cur_height) { // Small step sizes or already fitting children are not interesting.
				uint remainder = (cur_height - child_height) % step_size;
				if (remainder > 0) { // Child did not fit entirely, widen the container.
					cur_height += step_size - remainder;
					assert(cur_height < max_smallest); // Safeguard against infinite height expansion.
					/* Remaining children will adapt to the new cur_height, thus speeding up the computation. */
				}
			}
		}
		if (this->smallest_y == cur_height) break;
		this->smallest_y = cur_height; // Smallest height got changed, try again.
	}
	/* 2. For containers that must maintain equal width, extend child minimal size. */
	if (this->flags & NC_EQUALSIZE) {
		for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
			if (child_wid->fill_x == 1) child_wid->smallest_x = longest;
		}
	}
	/* 3. Move PIP space to the children, compute smallest, fill, and resize values of the container. */
	if (this->head != nullptr) this->head->padding.left += this->pip_pre;
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		if (child_wid->next != nullptr) {
			child_wid->padding.right += this->pip_inter;
		} else {
			child_wid->padding.right += this->pip_post;
		}

		this->smallest_x += child_wid->smallest_x + child_wid->padding.Horizontal();
		if (child_wid->fill_x > 0) {
			if (this->fill_x == 0 || this->fill_x > child_wid->fill_x) this->fill_x = child_wid->fill_x;
		}
		this->fill_y = LeastCommonMultiple(this->fill_y, child_wid->fill_y);

		if (child_wid->resize_x > 0) {
			if (this->resize_x == 0 || this->resize_x > child_wid->resize_x) this->resize_x = child_wid->resize_x;
		}
		this->resize_y = LeastCommonMultiple(this->resize_y, child_wid->resize_y);
	}
	/* We need to zero the PIP settings so we can re-initialize the tree. */
	this->pip_pre = this->pip_inter = this->pip_post = 0;
}

void NWidgetHorizontal::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool rtl)
{
	assert(given_width >= this->smallest_x && given_height >= this->smallest_y);

	/* Compute additional width given to us. */
	uint additional_length = given_width;
	if (sizing == ST_SMALLEST && (this->flags & NC_EQUALSIZE)) {
		/* For EQUALSIZE containers this does not sum to smallest_x during initialisation */
		for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
			additional_length -= child_wid->smallest_x + child_wid->padding.Horizontal();
		}
	} else {
		additional_length -= this->smallest_x;
	}

	this->StoreSizePosition(sizing, x, y, given_width, given_height);

	/* In principle, the additional horizontal space is distributed evenly over the available resizable children. Due to step sizes, this may not always be feasible.
	 * To make resizing work as good as possible, first children with biggest step sizes are done. These may get less due to rounding down.
	 * This additional space is then given to children with smaller step sizes. This will give a good result when resize steps of each child is a multiple
	 * of the child with the smallest non-zero stepsize.
	 *
	 * Since child sizes are computed out of order, positions cannot be calculated until all sizes are known. That means it is not possible to compute the child
	 * size and position, and directly call child->AssignSizePosition() with the computed values.
	 * Instead, computed child widths and heights are stored in child->current_x and child->current_y values. That is allowed, since this method overwrites those values
	 * then we call the child.
	 */

	/* First loop: Find biggest stepsize, find number of children that want a piece of the pie, handle vertical size for all children,
	 * handle horizontal size for non-resizing children.
	 */
	int num_changing_childs = 0; // Number of children that can change size.
	uint biggest_stepsize = 0;
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		uint hor_step = child_wid->GetHorizontalStepSize(sizing);
		if (hor_step > 0) {
			if (!(flags & NC_BIGFIRST)) num_changing_childs++;
			biggest_stepsize = std::max(biggest_stepsize, hor_step);
		} else {
			child_wid->current_x = child_wid->smallest_x;
		}

		uint vert_step = (sizing == ST_SMALLEST) ? 1 : child_wid->GetVerticalStepSize(sizing);
		child_wid->current_y = ComputeMaxSize(child_wid->smallest_y, given_height - child_wid->padding.Vertical(), vert_step);
	}

	/* First.5 loop: count how many children are of the biggest step size. */
	if ((flags & NC_BIGFIRST) && biggest_stepsize > 0) {
		for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
			uint hor_step = child_wid->GetHorizontalStepSize(sizing);
			if (hor_step == biggest_stepsize) {
				num_changing_childs++;
			}
		}
	}

	/* Second loop: Allocate the additional horizontal space over the resizing children, starting with the biggest resize steps. */
	while (biggest_stepsize > 0) {
		uint next_biggest_stepsize = 0;
		for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
			uint hor_step = child_wid->GetHorizontalStepSize(sizing);
			if (hor_step > biggest_stepsize) continue; // Already done
			if (hor_step == biggest_stepsize) {
				uint increment = additional_length / num_changing_childs;
				num_changing_childs--;
				if (hor_step > 1) increment -= increment % hor_step;
				child_wid->current_x = child_wid->smallest_x + increment;
				additional_length -= increment;
				continue;
			}
			next_biggest_stepsize = std::max(next_biggest_stepsize, hor_step);
		}
		biggest_stepsize = next_biggest_stepsize;

		if (num_changing_childs == 0 && (flags & NC_BIGFIRST) && biggest_stepsize > 0) {
			/* Second.5 loop: count how many children are of the updated biggest step size. */
			for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
				uint hor_step = child_wid->GetHorizontalStepSize(sizing);
				if (hor_step == biggest_stepsize) {
					num_changing_childs++;
				}
			}
		}
	}
	assert(num_changing_childs == 0);

	/* Third loop: Compute position and call the child. */
	uint position = rtl ? this->current_x : 0; // Place to put next child relative to origin of the container.
	NWidgetBase *child_wid = this->head;
	while (child_wid != nullptr) {
		uint child_width = child_wid->current_x;
		uint child_x = x + (rtl ? position - child_width - child_wid->padding.left : position + child_wid->padding.left);
		uint child_y = y + child_wid->padding.top;

		child_wid->AssignSizePosition(sizing, child_x, child_y, child_width, child_wid->current_y, rtl);
		uint padded_child_width = child_width + child_wid->padding.Horizontal();
		position = rtl ? position - padded_child_width : position + padded_child_width;

		child_wid = child_wid->next;
	}
}

/** Horizontal left-to-right container widget. */
NWidgetHorizontalLTR::NWidgetHorizontalLTR(NWidContainerFlags flags) : NWidgetHorizontal(flags)
{
	this->type = NWID_HORIZONTAL_LTR;
}

void NWidgetHorizontalLTR::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool rtl)
{
	NWidgetHorizontal::AssignSizePosition(sizing, x, y, given_width, given_height, false);
}

/** Vertical container widget. */
NWidgetVertical::NWidgetVertical(NWidContainerFlags flags) : NWidgetPIPContainer(NWID_VERTICAL, flags)
{
}

void NWidgetVertical::SetupSmallestSize(Window *w, bool init_array)
{
	this->smallest_x = 0; // Biggest child.
	this->smallest_y = 0; // Sum of minimal size of all children.
	this->fill_x = 1;     // smallest common child fill step.
	this->fill_y = 0;     // smallest non-zero child widget fill step.
	this->resize_x = 1;   // smallest common child resize step.
	this->resize_y = 0;   // smallest non-zero child widget resize step.

	/* 1a. Forward call, collect biggest nested array index, and longest/widest child length. */
	uint highest = 0; // Highest child found.
	uint max_hor_fill = 0; // Biggest horizontal fill step.
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		child_wid->SetupSmallestSize(w, init_array);
		highest = std::max(highest, child_wid->smallest_y);
		max_hor_fill = std::max(max_hor_fill, child_wid->GetHorizontalStepSize(ST_SMALLEST));
		this->smallest_x = std::max(this->smallest_x, child_wid->smallest_x + child_wid->padding.Horizontal());
	}
	/* 1b. Make the container wider if needed to accommodate all children nicely. */
	[[maybe_unused]] uint max_smallest = this->smallest_x + 3 * max_hor_fill; // Upper limit to computing smallest height.
	uint cur_width = this->smallest_x;
	for (;;) {
		for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
			uint step_size = child_wid->GetHorizontalStepSize(ST_SMALLEST);
			uint child_width = child_wid->smallest_x + child_wid->padding.Horizontal();
			if (step_size > 1 && child_width < cur_width) { // Small step sizes or already fitting children are not interesting.
				uint remainder = (cur_width - child_width) % step_size;
				if (remainder > 0) { // Child did not fit entirely, widen the container.
					cur_width += step_size - remainder;
					assert(cur_width < max_smallest); // Safeguard against infinite width expansion.
					/* Remaining children will adapt to the new cur_width, thus speeding up the computation. */
				}
			}
		}
		if (this->smallest_x == cur_width) break;
		this->smallest_x = cur_width; // Smallest width got changed, try again.
	}
	/* 2. For containers that must maintain equal width, extend children minimal size. */
	if (this->flags & NC_EQUALSIZE) {
		for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
			if (child_wid->fill_y == 1) child_wid->smallest_y = highest;
		}
	}
	/* 3. Move PIP space to the child, compute smallest, fill, and resize values of the container. */
	if (this->head != nullptr) this->head->padding.top += this->pip_pre;
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		if (child_wid->next != nullptr) {
			child_wid->padding.bottom += this->pip_inter;
		} else {
			child_wid->padding.bottom += this->pip_post;
		}

		this->smallest_y += child_wid->smallest_y + child_wid->padding.Vertical();
		if (child_wid->fill_y > 0) {
			if (this->fill_y == 0 || this->fill_y > child_wid->fill_y) this->fill_y = child_wid->fill_y;
		}
		this->fill_x = LeastCommonMultiple(this->fill_x, child_wid->fill_x);

		if (child_wid->resize_y > 0) {
			if (this->resize_y == 0 || this->resize_y > child_wid->resize_y) this->resize_y = child_wid->resize_y;
		}
		this->resize_x = LeastCommonMultiple(this->resize_x, child_wid->resize_x);
	}
	/* We need to zero the PIP settings so we can re-initialize the tree. */
	this->pip_pre = this->pip_inter = this->pip_post = 0;
}

void NWidgetVertical::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool rtl)
{
	assert(given_width >= this->smallest_x && given_height >= this->smallest_y);

	/* Compute additional height given to us. */
	uint additional_length = given_height;
	if (sizing == ST_SMALLEST && (this->flags & NC_EQUALSIZE)) {
		/* For EQUALSIZE containers this does not sum to smallest_y during initialisation */
		for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
			additional_length -= child_wid->smallest_y + child_wid->padding.Vertical();
		}
	} else {
		additional_length -= this->smallest_y;
	}

	this->StoreSizePosition(sizing, x, y, given_width, given_height);

	/* Like the horizontal container, the vertical container also distributes additional height evenly, starting with the children with the biggest resize steps.
	 * It also stores computed widths and heights into current_x and current_y values of the child.
	 */

	/* First loop: Find biggest stepsize, find number of children that want a piece of the pie, handle horizontal size for all children, handle vertical size for non-resizing child. */
	int num_changing_childs = 0; // Number of children that can change size.
	uint biggest_stepsize = 0;
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		uint vert_step = child_wid->GetVerticalStepSize(sizing);
		if (vert_step > 0) {
			if (!(flags & NC_BIGFIRST)) num_changing_childs++;
			biggest_stepsize = std::max(biggest_stepsize, vert_step);
		} else {
			child_wid->current_y = child_wid->smallest_y;
		}

		uint hor_step = (sizing == ST_SMALLEST) ? 1 : child_wid->GetHorizontalStepSize(sizing);
		child_wid->current_x = ComputeMaxSize(child_wid->smallest_x, given_width - child_wid->padding.Horizontal(), hor_step);
	}

	/* First.5 loop: count how many children are of the biggest step size. */
	if ((this->flags & NC_BIGFIRST) && biggest_stepsize > 0) {
		for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
			uint vert_step = child_wid->GetVerticalStepSize(sizing);
			if (vert_step == biggest_stepsize) {
				num_changing_childs++;
			}
		}
	}

	/* Second loop: Allocate the additional vertical space over the resizing children, starting with the biggest resize steps. */
	while (biggest_stepsize > 0) {
		uint next_biggest_stepsize = 0;
		for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
			uint vert_step = child_wid->GetVerticalStepSize(sizing);
			if (vert_step > biggest_stepsize) continue; // Already done
			if (vert_step == biggest_stepsize) {
				uint increment = additional_length / num_changing_childs;
				num_changing_childs--;
				if (vert_step > 1) increment -= increment % vert_step;
				child_wid->current_y = child_wid->smallest_y + increment;
				additional_length -= increment;
				continue;
			}
			next_biggest_stepsize = std::max(next_biggest_stepsize, vert_step);
		}
		biggest_stepsize = next_biggest_stepsize;

		if (num_changing_childs == 0 && (flags & NC_BIGFIRST) && biggest_stepsize > 0) {
			/* Second.5 loop: count how many children are of the updated biggest step size. */
			for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
				uint vert_step = child_wid->GetVerticalStepSize(sizing);
				if (vert_step == biggest_stepsize) {
					num_changing_childs++;
				}
			}
		}
	}
	assert(num_changing_childs == 0);

	/* Third loop: Compute position and call the child. */
	uint position = 0; // Place to put next child relative to origin of the container.
	for (NWidgetBase *child_wid = this->head; child_wid != nullptr; child_wid = child_wid->next) {
		uint child_x = x + (rtl ? child_wid->padding.right : child_wid->padding.left);
		uint child_height = child_wid->current_y;

		child_wid->AssignSizePosition(sizing, child_x, y + position + child_wid->padding.top, child_wid->current_x, child_height, rtl);
		position += child_height + child_wid->padding.Vertical();
	}
}

/**
 * Generic spacer widget.
 * @param width  Horizontal size of the spacer widget.
 * @param height Vertical size of the spacer widget.
 */
NWidgetSpacer::NWidgetSpacer(int width, int height) : NWidgetResizeBase(NWID_SPACER, 0, 0)
{
	this->SetMinimalSize(width, height);
	this->SetResize(0, 0);
}

void NWidgetSpacer::SetupSmallestSize(Window *w, bool init_array)
{
	this->smallest_x = this->min_x;
	this->smallest_y = this->min_y;
}

void NWidgetSpacer::FillNestedArray(NWidgetBase **array, uint length)
{
}

void NWidgetSpacer::Draw(const Window *w)
{
	/* Spacer widget is never visible. */
}

void NWidgetSpacer::SetDirty(const Window *w) const
{
	/* Spacer widget never need repainting. */
}

NWidgetCore *NWidgetSpacer::GetWidgetFromPos(int x, int y)
{
	return nullptr;
}

NWidgetMatrix::NWidgetMatrix() : NWidgetPIPContainer(NWID_MATRIX, NC_EQUALSIZE), index(-1), clicked(-1), count(-1)
{
}

void NWidgetMatrix::SetIndex(int index)
{
	this->index = index;
}

void NWidgetMatrix::SetColour(Colours colour)
{
	this->colour = colour;
}

/**
 * Sets the clicked widget in the matrix.
 * @param clicked The clicked widget.
 */
void NWidgetMatrix::SetClicked(int clicked)
{
	this->clicked = clicked;
	if (this->clicked >= 0 && this->sb != nullptr && this->widgets_x != 0) {
		int vpos = (this->clicked / this->widgets_x) * this->widget_h; // Vertical position of the top.
		/* Need to scroll down -> Scroll to the bottom.
		 * However, last entry has no 'this->pip_inter' underneath, and we must stay below this->sb->GetCount() */
		if (this->sb->GetPosition() < vpos) vpos += this->widget_h - this->pip_inter - 1;
		this->sb->ScrollTowards(vpos);
	}
}

/**
 * Set the number of elements in this matrix.
 * @note Updates the number of elements/capacity of the real scrollbar.
 * @param count The number of elements.
 */
void NWidgetMatrix::SetCount(int count)
{
	this->count = count;

	if (this->sb == nullptr || this->widgets_x == 0) return;

	/* We need to get the number of pixels the matrix is high/wide.
	 * So, determine the number of rows/columns based on the number of
	 * columns/rows (one is constant/unscrollable).
	 * Then multiply that by the height of a widget, and add the pre
	 * and post spacing "offsets". */
	count = CeilDiv(count, this->sb->IsVertical() ? this->widgets_x : this->widgets_y);
	count *= (this->sb->IsVertical() ? this->head->smallest_y : this->head->smallest_x) + this->pip_inter;
	if (count > 0) count -= this->pip_inter; // We counted an inter too much in the multiplication above
	count += this->pip_pre + this->pip_post;
	this->sb->SetCount(count);
	this->sb->SetCapacity(this->sb->IsVertical() ? this->current_y : this->current_x);
	this->sb->SetStepSize(this->sb->IsVertical() ? this->widget_h  : this->widget_w);
}

/**
 * Assign a scrollbar to this matrix.
 * @param sb The scrollbar to assign to us.
 */
void NWidgetMatrix::SetScrollbar(Scrollbar *sb)
{
	this->sb = sb;
}

void NWidgetMatrix::SetupSmallestSize(Window *w, bool init_array)
{
	assert(this->head != nullptr);
	assert(this->head->next == nullptr);

	if (this->index >= 0 && init_array) { // Fill w->nested_array[]
		assert(w->nested_array_size > (uint)this->index);
		w->nested_array[this->index] = this;
	}

	/* Reset the widget number. */
	NWidgetCore *nw = dynamic_cast<NWidgetCore *>(this->head);
	assert(nw != nullptr);
	SB(nw->index, 16, 16, 0);
	this->head->SetupSmallestSize(w, init_array);

	Dimension padding = { (uint)this->pip_pre + this->pip_post, (uint)this->pip_pre + this->pip_post};
	Dimension size    = {this->head->smallest_x + padding.width, this->head->smallest_y + padding.height};
	Dimension fill    = {0, 0};
	Dimension resize  = {this->pip_inter + this->head->smallest_x, this->pip_inter + this->head->smallest_y};

	if (this->index >= 0) w->UpdateWidgetSize(this->index, &size, padding, &fill, &resize);

	this->smallest_x = size.width;
	this->smallest_y = size.height;
	this->fill_x = fill.width;
	this->fill_y = fill.height;
	this->resize_x = resize.width;
	this->resize_y = resize.height;
}

void NWidgetMatrix::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool rtl)
{
	assert(given_width >= this->smallest_x && given_height >= this->smallest_y);

	this->pos_x = x;
	this->pos_y = y;
	this->current_x = given_width;
	this->current_y = given_height;

	/* Determine the size of the widgets, and the number of visible widgets on each of the axis. */
	this->widget_w = this->head->smallest_x + this->pip_inter;
	this->widget_h = this->head->smallest_y + this->pip_inter;

	/* Account for the pip_inter is between widgets, so we need to account for that when
	 * the division assumes pip_inter is used for all widgets. */
	this->widgets_x = CeilDiv(this->current_x - this->pip_pre - this->pip_post + this->pip_inter, this->widget_w);
	this->widgets_y = CeilDiv(this->current_y - this->pip_pre - this->pip_post + this->pip_inter, this->widget_h);

	/* When resizing, update the scrollbar's count. E.g. with a vertical
	 * scrollbar becoming wider or narrower means the amount of rows in
	 * the scrollbar becomes respectively smaller or higher. */
	this->SetCount(this->count);
}

void NWidgetMatrix::FillNestedArray(NWidgetBase **array, uint length)
{
	if (this->index >= 0 && (uint)(this->index) < length) array[this->index] = this;
	NWidgetContainer::FillNestedArray(array, length);
}

NWidgetCore *NWidgetMatrix::GetWidgetFromPos(int x, int y)
{
	/* Falls outside of the matrix widget. */
	if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return nullptr;

	int start_x, start_y, base_offs_x, base_offs_y;
	this->GetScrollOffsets(start_x, start_y, base_offs_x, base_offs_y);

	bool rtl = _current_text_dir == TD_RTL;

	int widget_col = (rtl ?
				-x + (int)this->pip_post + (int)this->pos_x + base_offs_x + (int)this->widget_w - 1 - (int)this->pip_inter :
				 x - (int)this->pip_pre  - (int)this->pos_x - base_offs_x
			) / this->widget_w;

	int widget_row = (y - base_offs_y - (int)this->pip_pre - (int)this->pos_y) / this->widget_h;

	int sub_wid = (widget_row + start_y) * this->widgets_x + start_x + widget_col;
	if (sub_wid >= this->count) return nullptr;

	NWidgetCore *child = dynamic_cast<NWidgetCore *>(this->head);
	assert(child != nullptr);
	child->AssignSizePosition(ST_RESIZE,
			this->pos_x + (rtl ? this->pip_post - widget_col * this->widget_w : this->pip_pre + widget_col * this->widget_w) + base_offs_x,
			this->pos_y + this->pip_pre + widget_row * this->widget_h + base_offs_y,
			child->smallest_x, child->smallest_y, rtl);

	SB(child->index, 16, 16, sub_wid);

	return child->GetWidgetFromPos(x, y);
}

/* virtual */ void NWidgetMatrix::Draw(const Window *w)
{
	/* Fill the background. */
	GfxFillRect(this->pos_x, this->pos_y, this->pos_x + this->current_x - 1, this->pos_y + this->current_y - 1, _colour_gradient[this->colour & 0xF][5]);

	/* Set up a clipping area for the previews. */
	bool rtl = _current_text_dir == TD_RTL;
	DrawPixelInfo tmp_dpi;
	if (!FillDrawPixelInfo(&tmp_dpi, this->pos_x + (rtl ? this->pip_post : this->pip_pre), this->pos_y + this->pip_pre, this->current_x - this->pip_pre - this->pip_post, this->current_y - this->pip_pre - this->pip_post)) return;
	DrawPixelInfo *old_dpi = _cur_dpi;
	_cur_dpi = &tmp_dpi;

	/* Get the appropriate offsets so we can draw the right widgets. */
	NWidgetCore *child = dynamic_cast<NWidgetCore *>(this->head);
	assert(child != nullptr);
	int start_x, start_y, base_offs_x, base_offs_y;
	this->GetScrollOffsets(start_x, start_y, base_offs_x, base_offs_y);

	int offs_y = base_offs_y;
	for (int y = start_y; y < start_y + this->widgets_y + 1; y++, offs_y += this->widget_h) {
		/* Are we within bounds? */
		if (offs_y + child->smallest_y <= 0) continue;
		if (offs_y >= (int)this->current_y) break;

		/* We've passed our amount of widgets. */
		if (y * this->widgets_x >= this->count) break;

		int offs_x = base_offs_x;
		for (int x = start_x; x < start_x + this->widgets_x + 1; x++, offs_x += rtl ? -this->widget_w : this->widget_w) {
			/* Are we within bounds? */
			if (offs_x + child->smallest_x <= 0) continue;
			if (offs_x >= (int)this->current_x) continue;

			/* Do we have this many widgets? */
			int sub_wid = y * this->widgets_x + x;
			if (sub_wid >= this->count) break;

			child->AssignSizePosition(ST_RESIZE, offs_x, offs_y, child->smallest_x, child->smallest_y, rtl);
			child->SetLowered(this->clicked == sub_wid);
			SB(child->index, 16, 16, sub_wid);
			child->Draw(w);
		}
	}

	/* Restore the clipping area. */
	_cur_dpi = old_dpi;
}

/**
 * Get the different offsets that are influenced by scrolling.
 * @param[out] start_x     The start position in columns (index of the left-most column, swapped in RTL).
 * @param[out] start_y     The start position in rows.
 * @param[out] base_offs_x The base horizontal offset in pixels (X position of the column \a start_x).
 * @param[out] base_offs_y The base vertical offset in pixels (Y position of the column \a start_y).
 */
void NWidgetMatrix::GetScrollOffsets(int &start_x, int &start_y, int &base_offs_x, int &base_offs_y)
{
	base_offs_x = _current_text_dir == TD_RTL ? this->widget_w * (this->widgets_x - 1) : 0;
	base_offs_y = 0;
	start_x = 0;
	start_y = 0;
	if (this->sb != nullptr) {
		if (this->sb->IsVertical()) {
			start_y = this->sb->GetPosition() / this->widget_h;
			base_offs_y += -this->sb->GetPosition() + start_y * this->widget_h;
		} else {
			start_x = this->sb->GetPosition() / this->widget_w;
			int sub_x = this->sb->GetPosition() - start_x * this->widget_w;
			if (_current_text_dir == TD_RTL) {
				base_offs_x += sub_x;
			} else {
				base_offs_x -= sub_x;
			}
		}
	}
}

/**
 * Constructor parent nested widgets.
 * @param tp     Type of parent widget.
 * @param colour Colour of the parent widget.
 * @param index  Index in the widget array used by the window system.
 * @param child  Child container widget (if supplied). If not supplied, a
 *               vertical container will be inserted while adding the first
 *               child widget.
 */
NWidgetBackground::NWidgetBackground(WidgetType tp, Colours colour, int index, NWidgetPIPContainer *child) : NWidgetCore(tp, colour, 1, 1, 0x0, STR_NULL)
{
	assert(tp == WWT_PANEL || tp == WWT_INSET || tp == WWT_FRAME);
	if (index >= 0) this->SetIndex(index);
	this->child = child;
	this->SetAlignment(SA_TOP | SA_LEFT);
}

NWidgetBackground::~NWidgetBackground()
{
	if (this->child != nullptr) delete this->child;
}

/**
 * Add a child to the parent.
 * @param nwid Nested widget to add to the background widget.
 *
 * Unless a child container has been given in the constructor, a parent behaves as a vertical container.
 * You can add several children to it, and they are put underneath each other.
 */
void NWidgetBackground::Add(NWidgetBase *nwid)
{
	if (this->child == nullptr) {
		this->child = new NWidgetVertical();
	}
	this->child->Add(nwid);
}

/**
 * Set additional pre/inter/post space for the background widget.
 *
 * @param pip_pre   Additional space in front of the first child widget (above
 *                  for the vertical container, at the left for the horizontal container).
 * @param pip_inter Additional space between two child widgets.
 * @param pip_post  Additional space after the last child widget (below for the
 *                  vertical container, at the right for the horizontal container).
 * @note Using this function implies that the widget has (or will have) child widgets.
 */
void NWidgetBackground::SetPIP(uint8 pip_pre, uint8 pip_inter, uint8 pip_post)
{
	if (this->child == nullptr) {
		this->child = new NWidgetVertical();
	}
	this->child->SetPIP(pip_pre, pip_inter, pip_post);
}

void NWidgetBackground::AdjustPaddingForZoom()
{
	if (child != nullptr) child->AdjustPaddingForZoom();
	NWidgetCore::AdjustPaddingForZoom();
}

void NWidgetBackground::SetupSmallestSize(Window *w, bool init_array)
{
	if (init_array && this->index >= 0) {
		assert(w->nested_array_size > (uint)this->index);
		w->nested_array[this->index] = this;
	}
	if (this->child != nullptr) {
		this->child->SetupSmallestSize(w, init_array);

		this->smallest_x = this->child->smallest_x;
		this->smallest_y = this->child->smallest_y;
		this->fill_x = this->child->fill_x;
		this->fill_y = this->child->fill_y;
		this->resize_x = this->child->resize_x;
		this->resize_y = this->child->resize_y;

		/* Don't apply automatic padding if there is no child widget. */
		if (w == nullptr) return;

		if (this->type == WWT_FRAME) {
			/* Account for the size of the frame's text if that exists */
			this->child->padding     = WidgetDimensions::scaled.frametext;
			this->child->padding.top = std::max<uint8>(WidgetDimensions::scaled.frametext.top, this->widget_data != STR_NULL ? FONT_HEIGHT_NORMAL + WidgetDimensions::scaled.frametext.top / 2 : 0);

			this->smallest_x += this->child->padding.Horizontal();
			this->smallest_y += this->child->padding.Vertical();

			if (this->index >= 0) w->SetStringParameters(this->index);
			this->smallest_x = std::max(this->smallest_x, GetStringBoundingBox(this->widget_data).width + WidgetDimensions::scaled.frametext.Horizontal());
		} else if (this->type == WWT_INSET) {
			/* Apply automatic padding for bevel thickness. */
			this->child->padding = WidgetDimensions::scaled.bevel;

			this->smallest_x += this->child->padding.Horizontal();
			this->smallest_y += this->child->padding.Vertical();
		}
	} else {
		Dimension d = {this->min_x, this->min_y};
		Dimension fill = {this->fill_x, this->fill_y};
		Dimension resize  = {this->resize_x, this->resize_y};
		if (w != nullptr) { // A non-nullptr window pointer acts as switch to turn dynamic widget size on.
			if (this->type == WWT_FRAME || this->type == WWT_INSET) {
				if (this->index >= 0) w->SetStringParameters(this->index);
				Dimension background = GetStringBoundingBox(this->widget_data);
				background.width += (this->type == WWT_FRAME) ? (WidgetDimensions::scaled.frametext.Horizontal()) : (WidgetDimensions::scaled.inset.Horizontal());
				d = maxdim(d, background);
			}
			if (this->index >= 0) {
				Dimension padding;
				switch (this->type) {
					default: NOT_REACHED();
					case WWT_PANEL: padding = {WidgetDimensions::scaled.framerect.Horizontal(), WidgetDimensions::scaled.framerect.Vertical()}; break;
					case WWT_FRAME: padding = {WidgetDimensions::scaled.frametext.Horizontal(), WidgetDimensions::scaled.frametext.Vertical()}; break;
					case WWT_INSET: padding = {WidgetDimensions::scaled.inset.Horizontal(),     WidgetDimensions::scaled.inset.Vertical()};     break;
				}
				w->UpdateWidgetSize(this->index, &d, padding, &fill, &resize);
			}
		}
		this->smallest_x = d.width;
		this->smallest_y = d.height;
		this->fill_x = fill.width;
		this->fill_y = fill.height;
		this->resize_x = resize.width;
		this->resize_y = resize.height;
	}
}

void NWidgetBackground::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool rtl)
{
	this->StoreSizePosition(sizing, x, y, given_width, given_height);

	if (this->child != nullptr) {
		uint x_offset = (rtl ? this->child->padding.right : this->child->padding.left);
		uint width = given_width - this->child->padding.Horizontal();
		uint height = given_height - this->child->padding.Vertical();
		this->child->AssignSizePosition(sizing, x + x_offset, y + this->child->padding.top, width, height, rtl);
	}
}

void NWidgetBackground::FillNestedArray(NWidgetBase **array, uint length)
{
	if (this->index >= 0 && (uint)(this->index) < length) array[this->index] = this;
	if (this->child != nullptr) this->child->FillNestedArray(array, length);
}

void NWidgetBackground::Draw(const Window *w)
{
	if (this->current_x == 0 || this->current_y == 0) return;

	Rect r = this->GetCurrentRect();

	const DrawPixelInfo *dpi = _cur_dpi;
	if (dpi->left > r.right || dpi->left + dpi->width <= r.left || dpi->top > r.bottom || dpi->top + dpi->height <= r.top) return;

	switch (this->type) {
		case WWT_PANEL:
			assert(this->widget_data == 0);
			DrawFrameRect(r.left, r.top, r.right, r.bottom, this->colour, this->IsLowered() ? FR_LOWERED : FR_NONE);
			break;

		case WWT_FRAME:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawFrame(r, this->colour, this->text_colour, this->widget_data, this->align);
			break;

		case WWT_INSET:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawInset(r, this->colour, this->text_colour, this->widget_data, this->align);
			break;

		default:
			NOT_REACHED();
	}

	if (this->index >= 0) w->DrawWidget(r, this->index);
	if (this->child != nullptr) this->child->Draw(w);

	if (this->IsDisabled()) {
		GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), _colour_gradient[this->colour & 0xF][2], FILLRECT_CHECKER);
	}
}

NWidgetCore *NWidgetBackground::GetWidgetFromPos(int x, int y)
{
	NWidgetCore *nwid = nullptr;
	if (IsInsideBS(x, this->pos_x, this->current_x) && IsInsideBS(y, this->pos_y, this->current_y)) {
		if (this->child != nullptr) nwid = this->child->GetWidgetFromPos(x, y);
		if (nwid == nullptr) nwid = this;
	}
	return nwid;
}

NWidgetBase *NWidgetBackground::GetWidgetOfType(WidgetType tp)
{
	NWidgetBase *nwid = nullptr;
	if (this->child != nullptr) nwid = this->child->GetWidgetOfType(tp);
	if (nwid == nullptr && this->type == tp) nwid = this;
	return nwid;
}

NWidgetViewport::NWidgetViewport(int index) : NWidgetCore(NWID_VIEWPORT, INVALID_COLOUR, 1, 1, 0x0, STR_NULL)
{
	this->SetIndex(index);
}

void NWidgetViewport::SetupSmallestSize(Window *w, bool init_array)
{
	if (init_array && this->index >= 0) {
		assert(w->nested_array_size > (uint)this->index);
		w->nested_array[this->index] = this;
	}
	this->smallest_x = this->min_x;
	this->smallest_y = this->min_y;
}

void NWidgetViewport::Draw(const Window *w)
{
	if (this->current_x == 0 || this->current_y == 0) return;

	if (this->disp_flags & ND_NO_TRANSPARENCY) {
		TransparencyOptionBits to_backup = _transparency_opt;
		_transparency_opt &= (1 << TO_SIGNS) | (1 << TO_LOADING); // Disable all transparency, except textual stuff
		w->DrawViewport();
		_transparency_opt = to_backup;
	} else {
		w->DrawViewport();
	}

	/* Optionally shade the viewport. */
	if (this->disp_flags & (ND_SHADE_GREY | ND_SHADE_DIMMED)) {
		GfxFillRect(this->pos_x, this->pos_y, this->pos_x + this->current_x - 1, this->pos_y + this->current_y - 1,
				(this->disp_flags & ND_SHADE_DIMMED) ? PALETTE_TO_TRANSPARENT : PALETTE_NEWSPAPER, FILLRECT_RECOLOUR);
	}
}

/**
 * Initialize the viewport of the window.
 * @param w            Window owning the viewport.
 * @param follow_flags Type of viewport, see #InitializeWindowViewport().
 * @param zoom         Zoom level.
 */
void NWidgetViewport::InitializeViewport(Window *w, uint32 follow_flags, ZoomLevel zoom)
{
	InitializeWindowViewport(w, this->pos_x, this->pos_y, this->current_x, this->current_y, follow_flags, zoom);
}

/**
 * Update the position and size of the viewport (after eg a resize).
 * @param w Window owning the viewport.
 */
void NWidgetViewport::UpdateViewportCoordinates(Window *w)
{
	Viewport *vp = w->viewport;
	if (vp != nullptr) {
		vp->left = w->left + this->pos_x;
		vp->top  = w->top + this->pos_y;
		vp->width  = this->current_x;
		vp->height = this->current_y;

		vp->virtual_width  = ScaleByZoom(vp->width, vp->zoom);
		vp->virtual_height = ScaleByZoom(vp->height, vp->zoom);
	}
}

/**
 * Compute the row of a scrolled widget that a user clicked in.
 * @param clickpos    Vertical position of the mouse click (without taking scrolling into account).
 * @param w           The window the click was in.
 * @param widget      Widget number of the widget clicked in.
 * @param padding     Amount of empty space between the widget edge and the top of the first row. Default value is \c 0.
 * @return Row number clicked at. If clicked at a wrong position, #INT_MAX is returned.
 */
int Scrollbar::GetScrolledRowFromWidget(int clickpos, const Window * const w, int widget, int padding) const
{
	uint pos = w->GetRowFromWidget(clickpos, widget, padding, -1);
	if (pos != INT_MAX) pos += this->GetPosition();
	return (pos >= this->GetCount()) ? INT_MAX : pos;
}

/**
 * Update the given list position as if it were on this scroll bar when the given keycode was pressed.
 * This does not update the actual position of this scroll bar, that is left to the caller. It does,
 * however use the capacity and count of the scroll bar for the bounds and amount to scroll.
 *
 * When the count is 0 or the return is ES_NOT_HANDLED, then the position is not updated.
 * With WKC_UP and WKC_DOWN the position goes one up or down respectively.
 * With WKC_PAGEUP and WKC_PAGEDOWN the position goes one capacity up or down respectively.
 * With WKC_HOME the first position is selected and with WKC_END the last position is selected.
 * This function ensures that pos is in the range [0..count).
 * @param list_position The current position in the list.
 * @param key_code      The pressed key code.
 * @return ES_NOT_HANDLED when another key than the 6 specific keys was pressed, otherwise ES_HANDLED.
 */
EventState Scrollbar::UpdateListPositionOnKeyPress(int &list_position, uint16 keycode) const
{
	int new_pos = list_position;
	switch (keycode) {
		case WKC_UP:
			/* scroll up by one */
			new_pos--;
			break;

		case WKC_DOWN:
			/* scroll down by one */
			new_pos++;
			break;

		case WKC_PAGEUP:
			/* scroll up a page */
			new_pos -= this->GetCapacity();
			break;

		case WKC_PAGEDOWN:
			/* scroll down a page */
			new_pos += this->GetCapacity();
			break;

		case WKC_HOME:
			/* jump to beginning */
			new_pos = 0;
			break;

		case WKC_END:
			/* jump to end */
			new_pos = this->GetCount() - 1;
			break;

		default:
			return ES_NOT_HANDLED;
	}

	/* If there are no elements, there is nothing to scroll/update. */
	if (this->GetCount() != 0) {
		list_position = Clamp(new_pos, 0, this->GetCount() - 1);
	}
	return ES_HANDLED;
}


/**
 * Set capacity of visible elements from the size and resize properties of a widget.
 * @param w       Window.
 * @param widget  Widget with size and resize properties.
 * @param padding Padding to subtract from the size.
 * @note Updates the position if needed.
 */
void Scrollbar::SetCapacityFromWidget(Window *w, int widget, int padding)
{
	NWidgetBase *nwid = w->GetWidget<NWidgetBase>(widget);
	if (this->IsVertical()) {
		this->SetCapacity(((int)nwid->current_y - padding) / (int)nwid->resize_y);
	} else {
		this->SetCapacity(((int)nwid->current_x - padding) / (int)nwid->resize_x);
	}
}

/**
 * Scrollbar widget.
 * @param tp     Scrollbar type. (horizontal/vertical)
 * @param colour Colour of the scrollbar.
 * @param index  Index in the widget array used by the window system.
 */
NWidgetScrollbar::NWidgetScrollbar(WidgetType tp, Colours colour, int index) : NWidgetCore(tp, colour, 1, 1, 0x0, STR_NULL), Scrollbar(tp != NWID_HSCROLLBAR)
{
	assert(tp == NWID_HSCROLLBAR || tp == NWID_VSCROLLBAR);
	this->SetIndex(index);

	switch (this->type) {
		case NWID_HSCROLLBAR:
			this->SetResize(1, 0);
			this->SetFill(1, 0);
			this->SetDataTip(0x0, STR_TOOLTIP_HSCROLL_BAR_SCROLLS_LIST);
			break;

		case NWID_VSCROLLBAR:
			this->SetResize(0, 1);
			this->SetFill(0, 1);
			this->SetDataTip(0x0, STR_TOOLTIP_VSCROLL_BAR_SCROLLS_LIST);
			break;

		default: NOT_REACHED();
	}
}

void NWidgetScrollbar::SetupSmallestSize(Window *w, bool init_array)
{
	if (init_array && this->index >= 0) {
		assert(w->nested_array_size > (uint)this->index);
		w->nested_array[this->index] = this;
	}
	this->min_x = 0;
	this->min_y = 0;

	switch (this->type) {
		case NWID_HSCROLLBAR:
			this->SetMinimalSizeAbsolute(NWidgetScrollbar::GetHorizontalDimension().width * 3, NWidgetScrollbar::GetHorizontalDimension().height);
			break;

		case NWID_VSCROLLBAR:
			this->SetMinimalSizeAbsolute(NWidgetScrollbar::GetVerticalDimension().width, NWidgetScrollbar::GetVerticalDimension().height * 3);
			break;

		default: NOT_REACHED();
	}

	this->smallest_x = this->min_x;
	this->smallest_y = this->min_y;
}

void NWidgetScrollbar::Draw(const Window *w)
{
	if (this->current_x == 0 || this->current_y == 0) return;

	Rect r = this->GetCurrentRect();

	const DrawPixelInfo *dpi = _cur_dpi;
	if (dpi->left > r.right || dpi->left + dpi->width <= r.left || dpi->top > r.bottom || dpi->top + dpi->height <= r.top) return;

	bool up_lowered = HasBit(this->disp_flags, NDB_SCROLLBAR_UP);
	bool down_lowered = HasBit(this->disp_flags, NDB_SCROLLBAR_DOWN);
	bool middle_lowered = !(this->disp_flags & ND_SCROLLBAR_BTN) && w->mouse_capture_widget == this->index;

	if (this->type == NWID_HSCROLLBAR) {
		DrawHorizontalScrollbar(r, this->colour, up_lowered, middle_lowered, down_lowered, this);
	} else {
		DrawVerticalScrollbar(r, this->colour, up_lowered, middle_lowered, down_lowered, this);
	}

	if (this->IsDisabled()) {
		GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), _colour_gradient[this->colour & 0xF][2], FILLRECT_CHECKER);
	}
}

/* static */ void NWidgetScrollbar::InvalidateDimensionCache()
{
	vertical_dimension.width   = vertical_dimension.height   = 0;
	horizontal_dimension.width = horizontal_dimension.height = 0;
}

/* static */ Dimension NWidgetScrollbar::GetVerticalDimension()
{
	if (vertical_dimension.width == 0) {
		vertical_dimension = maxdim(GetScaledSpriteSize(SPR_ARROW_UP), GetScaledSpriteSize(SPR_ARROW_DOWN));
		vertical_dimension.width += WidgetDimensions::scaled.vscrollbar.Horizontal();
		vertical_dimension.height += WidgetDimensions::scaled.vscrollbar.Vertical();
	}
	return vertical_dimension;
}

/* static */ Dimension NWidgetScrollbar::GetHorizontalDimension()
{
	if (horizontal_dimension.width == 0) {
		horizontal_dimension = maxdim(GetScaledSpriteSize(SPR_ARROW_LEFT), GetScaledSpriteSize(SPR_ARROW_RIGHT));
		horizontal_dimension.width += WidgetDimensions::scaled.hscrollbar.Horizontal();
		horizontal_dimension.height += WidgetDimensions::scaled.hscrollbar.Vertical();
	}
	return horizontal_dimension;
}

Dimension NWidgetScrollbar::vertical_dimension = {0, 0};
Dimension NWidgetScrollbar::horizontal_dimension = {0, 0};

/** Reset the cached dimensions. */
/* static */ void NWidgetLeaf::InvalidateDimensionCache()
{
	shadebox_dimension.width   = shadebox_dimension.height   = 0;
	debugbox_dimension.width   = debugbox_dimension.height   = 0;
	defsizebox_dimension.width = defsizebox_dimension.height = 0;
	stickybox_dimension.width  = stickybox_dimension.height  = 0;
	resizebox_dimension.width  = resizebox_dimension.height  = 0;
	closebox_dimension.width   = closebox_dimension.height   = 0;
	dropdown_dimension.width   = dropdown_dimension.height   = 0;
}

Dimension NWidgetLeaf::shadebox_dimension   = {0, 0};
Dimension NWidgetLeaf::debugbox_dimension   = {0, 0};
Dimension NWidgetLeaf::defsizebox_dimension = {0, 0};
Dimension NWidgetLeaf::stickybox_dimension  = {0, 0};
Dimension NWidgetLeaf::resizebox_dimension  = {0, 0};
Dimension NWidgetLeaf::closebox_dimension   = {0, 0};
Dimension NWidgetLeaf::dropdown_dimension   = {0, 0};

/**
 * Nested leaf widget.
 * @param tp     Type of leaf widget.
 * @param colour Colour of the leaf widget.
 * @param index  Index in the widget array used by the window system.
 * @param data   Data of the widget.
 * @param tip    Tooltip of the widget.
 */
NWidgetLeaf::NWidgetLeaf(WidgetType tp, Colours colour, int index, uint32 data, StringID tip) : NWidgetCore(tp, colour, 1, 1, data, tip)
{
	assert(index >= 0 || tp == WWT_LABEL || tp == WWT_TEXT || tp == WWT_CAPTION || tp == WWT_RESIZEBOX || tp == WWT_SHADEBOX || tp == WWT_DEFSIZEBOX || tp == WWT_DEBUGBOX || tp == WWT_STICKYBOX || tp == WWT_CLOSEBOX);
	if (index >= 0) this->SetIndex(index);
	this->min_x = 0;
	this->min_y = 0;
	this->SetResize(0, 0);

	switch (tp) {
		case WWT_EMPTY:
			break;

		case WWT_TEXT:
			this->SetFill(0, 0);
			this->SetAlignment(SA_LEFT | SA_VERT_CENTER);
			break;

		case WWT_PUSHBTN:
		case WWT_IMGBTN:
		case WWT_PUSHIMGBTN:
		case WWT_IMGBTN_2:
		case WWT_TEXTBTN:
		case WWT_PUSHTXTBTN:
		case WWT_TEXTBTN_2:
		case WWT_LABEL:
		case WWT_MATRIX:
		case NWID_BUTTON_DROPDOWN:
		case NWID_PUSHBUTTON_DROPDOWN:
		case WWT_ARROWBTN:
		case WWT_PUSHARROWBTN:
			this->SetFill(0, 0);
			break;

		case WWT_EDITBOX:
			this->SetFill(0, 0);
			break;

		case WWT_CAPTION:
			this->SetFill(1, 0);
			this->SetResize(1, 0);
			this->SetMinimalSize(0, WD_CAPTION_HEIGHT);
			this->SetMinimalTextLines(1, WidgetDimensions::unscaled.framerect.Vertical(), FS_NORMAL);
			this->SetDataTip(data, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS);
			break;

		case WWT_STICKYBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WD_STICKYBOX_WIDTH, WD_CAPTION_HEIGHT);
			this->SetDataTip(STR_NULL, STR_TOOLTIP_STICKY);
			break;

		case WWT_SHADEBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WD_SHADEBOX_WIDTH, WD_CAPTION_HEIGHT);
			this->SetDataTip(STR_NULL, STR_TOOLTIP_SHADE);
			break;

		case WWT_DEBUGBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WD_DEBUGBOX_WIDTH, WD_CAPTION_HEIGHT);
			this->SetDataTip(STR_NULL, STR_TOOLTIP_DEBUG);
			break;

		case WWT_DEFSIZEBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WD_DEFSIZEBOX_WIDTH, WD_CAPTION_HEIGHT);
			this->SetDataTip(STR_NULL, STR_TOOLTIP_DEFSIZE);
			break;

		case WWT_RESIZEBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WD_RESIZEBOX_WIDTH, 12);
			this->SetDataTip(STR_NULL, STR_TOOLTIP_RESIZE);
			break;

		case WWT_CLOSEBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WD_CLOSEBOX_WIDTH, WD_CAPTION_HEIGHT);
			this->SetDataTip(STR_NULL, STR_TOOLTIP_CLOSE_WINDOW);
			break;

		case WWT_DROPDOWN:
			this->SetFill(0, 0);
			this->SetMinimalSize(0, WD_DROPDOWN_HEIGHT);
			this->SetAlignment(SA_TOP | SA_LEFT);
			break;

		default:
			NOT_REACHED();
	}
}

void NWidgetLeaf::SetupSmallestSize(Window *w, bool init_array)
{
	if (this->index >= 0 && init_array) { // Fill w->nested_array[]
		assert(w->nested_array_size > (uint)this->index);
		w->nested_array[this->index] = this;
	}

	Dimension padding = {0, 0};
	Dimension size = {this->min_x, this->min_y};
	Dimension fill = {this->fill_x, this->fill_y};
	Dimension resize = {this->resize_x, this->resize_y};
	switch (this->type) {
		case WWT_EMPTY: {
			break;
		}
		case WWT_MATRIX: {
			padding = {WidgetDimensions::scaled.matrix.Horizontal(), WidgetDimensions::scaled.matrix.Vertical()};
			break;
		}
		case WWT_SHADEBOX: {
			padding = {WidgetDimensions::scaled.shadebox.Horizontal(), WidgetDimensions::scaled.shadebox.Vertical()};
			if (NWidgetLeaf::shadebox_dimension.width == 0) {
				NWidgetLeaf::shadebox_dimension = maxdim(GetScaledSpriteSize(SPR_WINDOW_SHADE), GetScaledSpriteSize(SPR_WINDOW_UNSHADE));
				NWidgetLeaf::shadebox_dimension.width += padding.width;
				NWidgetLeaf::shadebox_dimension.height += padding.height;
			}
			size = maxdim(size, NWidgetLeaf::shadebox_dimension);
			break;
		}
		case WWT_DEBUGBOX:
			if (_settings_client.gui.newgrf_developer_tools && w->IsNewGRFInspectable()) {
				padding = {WidgetDimensions::scaled.debugbox.Horizontal(), WidgetDimensions::scaled.debugbox.Vertical()};
				if (NWidgetLeaf::debugbox_dimension.width == 0) {
					NWidgetLeaf::debugbox_dimension = GetScaledSpriteSize(SPR_WINDOW_DEBUG);
					NWidgetLeaf::debugbox_dimension.width += padding.width;
					NWidgetLeaf::debugbox_dimension.height += padding.height;
				}
				size = maxdim(size, NWidgetLeaf::debugbox_dimension);
			} else {
				/* If the setting is disabled we don't want to see it! */
				size.width = 0;
				fill.width = 0;
				resize.width = 0;
			}
			break;

		case WWT_STICKYBOX: {
			padding = {WidgetDimensions::scaled.stickybox.Horizontal(), WidgetDimensions::scaled.stickybox.Vertical()};
			if (NWidgetLeaf::stickybox_dimension.width == 0) {
				NWidgetLeaf::stickybox_dimension = maxdim(GetScaledSpriteSize(SPR_PIN_UP), GetScaledSpriteSize(SPR_PIN_DOWN));
				NWidgetLeaf::stickybox_dimension.width += padding.width;
				NWidgetLeaf::stickybox_dimension.height += padding.height;
			}
			size = maxdim(size, NWidgetLeaf::stickybox_dimension);
			break;
		}

		case WWT_DEFSIZEBOX: {
			padding = {WidgetDimensions::scaled.defsizebox.Horizontal(), WidgetDimensions::scaled.defsizebox.Vertical()};
			if (NWidgetLeaf::defsizebox_dimension.width == 0) {
				NWidgetLeaf::defsizebox_dimension = GetScaledSpriteSize(SPR_WINDOW_DEFSIZE);
				NWidgetLeaf::defsizebox_dimension.width += padding.width;
				NWidgetLeaf::defsizebox_dimension.height += padding.height;
			}
			size = maxdim(size, NWidgetLeaf::defsizebox_dimension);
			break;
		}

		case WWT_RESIZEBOX: {
			padding = {WidgetDimensions::scaled.resizebox.Horizontal(), WidgetDimensions::scaled.resizebox.Vertical()};
			if (NWidgetLeaf::resizebox_dimension.width == 0) {
				NWidgetLeaf::resizebox_dimension = maxdim(GetScaledSpriteSize(SPR_WINDOW_RESIZE_LEFT), GetScaledSpriteSize(SPR_WINDOW_RESIZE_RIGHT));
				NWidgetLeaf::resizebox_dimension.width += padding.width;
				NWidgetLeaf::resizebox_dimension.height += padding.height;
			}
			size = maxdim(size, NWidgetLeaf::resizebox_dimension);
			break;
		}
		case WWT_EDITBOX: {
			Dimension sprite_size = GetScaledSpriteSize(_current_text_dir == TD_RTL ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT);
			size.width = std::max(size.width, ScaleGUITrad(30) + sprite_size.width);
			size.height = std::max(sprite_size.height, GetStringBoundingBox("_").height + WidgetDimensions::scaled.framerect.Vertical());
		}
		FALLTHROUGH;
		case WWT_PUSHBTN: {
			padding = {WidgetDimensions::scaled.frametext.Horizontal(), WidgetDimensions::scaled.framerect.Vertical()};
			break;
		}
		case WWT_IMGBTN:
		case WWT_IMGBTN_2:
		case WWT_PUSHIMGBTN: {
			padding = {WidgetDimensions::scaled.imgbtn.Horizontal(), WidgetDimensions::scaled.imgbtn.Vertical()};
			Dimension d2 = GetScaledSpriteSize(this->widget_data);
			if (this->type == WWT_IMGBTN_2) d2 = maxdim(d2, GetScaledSpriteSize(this->widget_data + 1));
			d2.width += padding.width;
			d2.height += padding.height;
			size = maxdim(size, d2);
			break;
		}
		case WWT_ARROWBTN:
		case WWT_PUSHARROWBTN: {
			padding = {WidgetDimensions::scaled.imgbtn.Horizontal(), WidgetDimensions::scaled.imgbtn.Vertical()};
			Dimension d2 = maxdim(GetScaledSpriteSize(SPR_ARROW_LEFT), GetScaledSpriteSize(SPR_ARROW_RIGHT));
			d2.width += padding.width;
			d2.height += padding.height;
			size = maxdim(size, d2);
			break;
		}

		case WWT_CLOSEBOX: {
			padding = {WidgetDimensions::scaled.closebox.Horizontal(), WidgetDimensions::scaled.closebox.Vertical()};
			if (NWidgetLeaf::closebox_dimension.width == 0) {
				NWidgetLeaf::closebox_dimension = GetScaledSpriteSize(SPR_CLOSEBOX);
				NWidgetLeaf::closebox_dimension.width += padding.width;
				NWidgetLeaf::closebox_dimension.height += padding.height;
			}
			size = maxdim(size, NWidgetLeaf::closebox_dimension);
			break;
		}
		case WWT_TEXTBTN:
		case WWT_PUSHTXTBTN:
		case WWT_TEXTBTN_2: {
			padding = {WidgetDimensions::scaled.framerect.Horizontal(), WidgetDimensions::scaled.framerect.Vertical()};
			if (this->index >= 0) w->SetStringParameters(this->index);
			Dimension d2 = GetStringBoundingBox(this->widget_data);
			d2.width += padding.width;
			d2.height += padding.height;
			size = maxdim(size, d2);
			break;
		}
		case WWT_LABEL:
		case WWT_TEXT: {
			if (this->index >= 0) w->SetStringParameters(this->index);
			size = maxdim(size, GetStringBoundingBox(this->widget_data));
			break;
		}
		case WWT_CAPTION: {
			padding = {WidgetDimensions::scaled.captiontext.Horizontal(), WidgetDimensions::scaled.captiontext.Vertical()};
			if (this->index >= 0) w->SetStringParameters(this->index);
			Dimension d2 = GetStringBoundingBox(this->widget_data);
			d2.width += padding.width;
			d2.height += padding.height;
			size = maxdim(size, d2);
			break;
		}
		case WWT_DROPDOWN:
		case NWID_BUTTON_DROPDOWN:
		case NWID_PUSHBUTTON_DROPDOWN: {
			if (NWidgetLeaf::dropdown_dimension.width == 0) {
				NWidgetLeaf::dropdown_dimension = GetScaledSpriteSize(SPR_ARROW_DOWN);
				NWidgetLeaf::dropdown_dimension.width += WidgetDimensions::scaled.vscrollbar.Horizontal();
				NWidgetLeaf::dropdown_dimension.height += WidgetDimensions::scaled.vscrollbar.Vertical();
			}
			padding = {WidgetDimensions::scaled.dropdowntext.Horizontal() + NWidgetLeaf::dropdown_dimension.width, WidgetDimensions::scaled.dropdowntext.Vertical()};
			if (this->index >= 0) w->SetStringParameters(this->index);
			Dimension d2 = GetStringBoundingBox(this->widget_data);
			d2.width += padding.width;
			d2.height = std::max(d2.height + padding.height, NWidgetLeaf::dropdown_dimension.height);
			size = maxdim(size, d2);
			break;
		}
		default:
			NOT_REACHED();
	}

	if (this->index >= 0) w->UpdateWidgetSize(this->index, &size, padding, &fill, &resize);

	this->smallest_x = size.width;
	this->smallest_y = size.height;
	this->fill_x = fill.width;
	this->fill_y = fill.height;
	this->resize_x = resize.width;
	this->resize_y = resize.height;
}

void NWidgetLeaf::Draw(const Window *w)
{
	if (this->current_x == 0 || this->current_y == 0) return;

	/* Setup a clipping rectangle... for WWT_EMPTY or WWT_TEXT, an extra scaled pixel is allowed vertically in case text shadow encroaches. */
	int extra_y = (this->type == WWT_EMPTY || this->type == WWT_TEXT) ? ScaleGUITrad(1) : 0;
	DrawPixelInfo new_dpi;
	if (!FillDrawPixelInfo(&new_dpi, this->pos_x, this->pos_y, this->current_x, this->current_y + extra_y)) return;
	/* ...but keep coordinates relative to the window. */
	new_dpi.left += this->pos_x;
	new_dpi.top += this->pos_y;

	DrawPixelInfo *old_dpi = _cur_dpi;
	_cur_dpi = &new_dpi;

	Rect r = this->GetCurrentRect();

	bool clicked = this->IsLowered();
	switch (this->type) {
		case WWT_EMPTY:
			break;

		case WWT_PUSHBTN:
			assert(this->widget_data == 0);
			DrawFrameRect(r.left, r.top, r.right, r.bottom, this->colour, (clicked) ? FR_LOWERED : FR_NONE);
			break;

		case WWT_IMGBTN:
		case WWT_PUSHIMGBTN:
		case WWT_IMGBTN_2:
			DrawImageButtons(r, this->type, this->colour, clicked, this->widget_data, this->align);
			break;

		case WWT_TEXTBTN:
		case WWT_PUSHTXTBTN:
		case WWT_TEXTBTN_2:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawFrameRect(r.left, r.top, r.right, r.bottom, this->colour, (clicked) ? FR_LOWERED : FR_NONE);
			DrawLabel(r, this->type, clicked, this->text_colour, this->widget_data, this->align);
			break;

		case WWT_ARROWBTN:
		case WWT_PUSHARROWBTN: {
			SpriteID sprite;
			switch (this->widget_data) {
				case AWV_DECREASE: sprite = _current_text_dir != TD_RTL ? SPR_ARROW_LEFT : SPR_ARROW_RIGHT; break;
				case AWV_INCREASE: sprite = _current_text_dir == TD_RTL ? SPR_ARROW_LEFT : SPR_ARROW_RIGHT; break;
				case AWV_LEFT:     sprite = SPR_ARROW_LEFT;  break;
				case AWV_RIGHT:    sprite = SPR_ARROW_RIGHT; break;
				default: NOT_REACHED();
			}
			DrawImageButtons(r, WWT_PUSHIMGBTN, this->colour, clicked, sprite, this->align);
			break;
		}

		case WWT_LABEL:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawLabel(r, this->type, clicked, this->text_colour, this->widget_data, this->align);
			break;

		case WWT_TEXT:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawText(r, this->text_colour, this->widget_data, this->align);
			break;

		case WWT_MATRIX:
			DrawMatrix(r, this->colour, clicked, this->widget_data, this->resize_x, this->resize_y);
			break;

		case WWT_EDITBOX: {
			const QueryString *query = w->GetQueryString(this->index);
			if (query != nullptr) query->DrawEditBox(w, this->index);
			break;
		}

		case WWT_CAPTION:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawCaption(r, this->colour, w->owner, this->text_colour, this->widget_data, this->align);
			break;

		case WWT_SHADEBOX:
			assert(this->widget_data == 0);
			DrawShadeBox(r, this->colour, w->IsShaded());
			break;

		case WWT_DEBUGBOX:
			DrawDebugBox(r, this->colour, clicked);
			break;

		case WWT_STICKYBOX:
			assert(this->widget_data == 0);
			DrawStickyBox(r, this->colour, !!(w->flags & WF_STICKY));
			break;

		case WWT_DEFSIZEBOX:
			assert(this->widget_data == 0);
			DrawDefSizeBox(r, this->colour, clicked);
			break;

		case WWT_RESIZEBOX:
			assert(this->widget_data == 0);
			DrawResizeBox(r, this->colour, this->pos_x < (w->width / 2), !!(w->flags & WF_SIZING));
			break;

		case WWT_CLOSEBOX:
			DrawCloseBox(r, this->colour);
			break;

		case WWT_DROPDOWN:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawButtonDropdown(r, this->colour, false, clicked, this->widget_data, this->align);
			break;

		case NWID_BUTTON_DROPDOWN:
		case NWID_PUSHBUTTON_DROPDOWN:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawButtonDropdown(r, this->colour, clicked, (this->disp_flags & ND_DROPDOWN_ACTIVE) != 0, this->widget_data, this->align);
			break;

		default:
			NOT_REACHED();
	}
	if (this->index >= 0) w->DrawWidget(r, this->index);

	if (this->IsDisabled()) {
		GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), _colour_gradient[this->colour & 0xF][2], FILLRECT_CHECKER);
	}

	_cur_dpi = old_dpi;
}

/**
 * For a #NWID_BUTTON_DROPDOWN, test whether \a pt refers to the button or to the drop-down.
 * @param pt Point in the widget.
 * @return The point refers to the button.
 *
 * @note The magic constants are also used at #DrawButtonDropdown.
 */
bool NWidgetLeaf::ButtonHit(const Point &pt)
{
	if (_current_text_dir == TD_LTR) {
		int button_width = this->pos_x + this->current_x - NWidgetLeaf::dropdown_dimension.width;
		return pt.x < button_width;
	} else {
		int button_left = this->pos_x + NWidgetLeaf::dropdown_dimension.width;
		return pt.x >= button_left;
	}
}

/* == Conversion code from NWidgetPart array to NWidgetBase* tree == */

/**
 * Construct a single nested widget in \a *dest from its parts.
 *
 * Construct a NWidgetBase object from a #NWidget function, and apply all
 * settings that follow it, until encountering a #EndContainer, another
 * #NWidget, or the end of the parts array.
 *
 * @param parts Array with parts of the nested widget.
 * @param count Length of the \a parts array.
 * @param dest  Address of pointer to use for returning the composed widget.
 * @param fill_dest Fill the composed widget with child widgets.
 * @param biggest_index Pointer to biggest nested widget index in the tree encountered so far.
 * @return Number of widget part elements used to compose the widget.
 * @pre \c biggest_index != nullptr.
 */
static int MakeNWidget(const NWidgetPart *parts, int count, NWidgetBase **dest, bool *fill_dest, int *biggest_index)
{
	int num_used = 0;

	*dest = nullptr;
	*fill_dest = false;

	while (count > num_used) {
		switch (parts->type) {
			case NWID_SPACER:
				if (*dest != nullptr) return num_used;
				*dest = new NWidgetSpacer(0, 0);
				break;

			case NWID_HORIZONTAL:
				if (*dest != nullptr) return num_used;
				*dest = new NWidgetHorizontal(parts->u.cont_flags);
				*fill_dest = true;
				break;

			case NWID_HORIZONTAL_LTR:
				if (*dest != nullptr) return num_used;
				*dest = new NWidgetHorizontalLTR(parts->u.cont_flags);
				*fill_dest = true;
				break;

			case WWT_PANEL:
			case WWT_INSET:
			case WWT_FRAME:
				if (*dest != nullptr) return num_used;
				*dest = new NWidgetBackground(parts->type, parts->u.widget.colour, parts->u.widget.index);
				*biggest_index = std::max(*biggest_index, (int)parts->u.widget.index);
				*fill_dest = true;
				break;

			case NWID_VERTICAL:
				if (*dest != nullptr) return num_used;
				*dest = new NWidgetVertical(parts->u.cont_flags);
				*fill_dest = true;
				break;

			case NWID_MATRIX: {
				if (*dest != nullptr) return num_used;
				NWidgetMatrix *nwm = new NWidgetMatrix();
				*dest = nwm;
				*fill_dest = true;
				nwm->SetIndex(parts->u.widget.index);
				nwm->SetColour(parts->u.widget.colour);
				*biggest_index = std::max(*biggest_index, (int)parts->u.widget.index);
				break;
			}

			case WPT_FUNCTION: {
				if (*dest != nullptr) return num_used;
				/* Ensure proper functioning even when the called code simply writes its largest index. */
				int biggest = -1;
				*dest = parts->u.func_ptr(&biggest);
				*biggest_index = std::max(*biggest_index, biggest);
				*fill_dest = false;
				break;
			}

			case WPT_RESIZE: {
				NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(*dest);
				if (nwrb != nullptr) {
					assert(parts->u.xy.x >= 0 && parts->u.xy.y >= 0);
					nwrb->SetResize(parts->u.xy.x, parts->u.xy.y);
				}
				break;
			}

			case WPT_MINSIZE: {
				NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(*dest);
				if (nwrb != nullptr) {
					assert(parts->u.xy.x >= 0 && parts->u.xy.y >= 0);
					nwrb->SetMinimalSize(parts->u.xy.x, parts->u.xy.y);
				}
				break;
			}

			case WPT_MINTEXTLINES: {
				NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(*dest);
				if (nwrb != nullptr) {
					assert(parts->u.text_lines.size >= FS_BEGIN && parts->u.text_lines.size < FS_END);
					nwrb->SetMinimalTextLines(parts->u.text_lines.lines, parts->u.text_lines.spacing, parts->u.text_lines.size);
				}
				break;
			}

			case WPT_TEXTCOLOUR: {
				NWidgetCore *nwc = dynamic_cast<NWidgetCore *>(*dest);
				if (nwc != nullptr) {
					nwc->SetTextColour(parts->u.colour.colour);
				}
				break;
			}

			case WPT_ALIGNMENT: {
				NWidgetCore *nwc = dynamic_cast<NWidgetCore *>(*dest);
				if (nwc != nullptr) {
					nwc->SetAlignment(parts->u.align.align);
				}
				break;
			}

			case WPT_FILL: {
				NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(*dest);
				if (nwrb != nullptr) nwrb->SetFill(parts->u.xy.x, parts->u.xy.y);
				break;
			}

			case WPT_DATATIP: {
				NWidgetCore *nwc = dynamic_cast<NWidgetCore *>(*dest);
				if (nwc != nullptr) {
					nwc->widget_data = parts->u.data_tip.data;
					nwc->tool_tip = parts->u.data_tip.tooltip;
				}
				break;
			}

			case WPT_PADDING:
				if (*dest != nullptr) (*dest)->SetPadding(parts->u.padding);
				break;

			case WPT_PIPSPACE: {
				NWidgetPIPContainer *nwc = dynamic_cast<NWidgetPIPContainer *>(*dest);
				if (nwc != nullptr) nwc->SetPIP(parts->u.pip.pre, parts->u.pip.inter, parts->u.pip.post);

				NWidgetBackground *nwb = dynamic_cast<NWidgetBackground *>(*dest);
				if (nwb != nullptr) nwb->SetPIP(parts->u.pip.pre, parts->u.pip.inter, parts->u.pip.post);
				break;
			}

			case WPT_SCROLLBAR: {
				NWidgetCore *nwc = dynamic_cast<NWidgetCore *>(*dest);
				if (nwc != nullptr) {
					nwc->scrollbar_index = parts->u.widget.index;
				}
				break;
			}

			case WPT_ENDCONTAINER:
				return num_used;

			case NWID_VIEWPORT:
				if (*dest != nullptr) return num_used;
				*dest = new NWidgetViewport(parts->u.widget.index);
				*biggest_index = std::max(*biggest_index, (int)parts->u.widget.index);
				break;

			case NWID_HSCROLLBAR:
			case NWID_VSCROLLBAR:
				if (*dest != nullptr) return num_used;
				*dest = new NWidgetScrollbar(parts->type, parts->u.widget.colour, parts->u.widget.index);
				*biggest_index = std::max(*biggest_index, (int)parts->u.widget.index);
				break;

			case NWID_SELECTION: {
				if (*dest != nullptr) return num_used;
				NWidgetStacked *nws = new NWidgetStacked();
				*dest = nws;
				*fill_dest = true;
				nws->SetIndex(parts->u.widget.index);
				*biggest_index = std::max(*biggest_index, (int)parts->u.widget.index);
				break;
			}

			default:
				if (*dest != nullptr) return num_used;
				assert((parts->type & WWT_MASK) < WWT_LAST || (parts->type & WWT_MASK) == NWID_BUTTON_DROPDOWN);
				*dest = new NWidgetLeaf(parts->type, parts->u.widget.colour, parts->u.widget.index, 0x0, STR_NULL);
				*biggest_index = std::max(*biggest_index, (int)parts->u.widget.index);
				break;
		}
		num_used++;
		parts++;
	}

	return num_used;
}

/**
 * Build a nested widget tree by recursively filling containers with nested widgets read from their parts.
 * @param parts  Array with parts of the nested widgets.
 * @param count  Length of the \a parts array.
 * @param parent Pointer or container to use for storing the child widgets (*parent == nullptr or *parent == container or background widget).
 * @param biggest_index Pointer to biggest nested widget index in the tree.
 * @return Number of widget part elements used to fill the container.
 * @post \c *biggest_index contains the largest widget index of the tree and \c -1 if no index is used.
 */
static int MakeWidgetTree(const NWidgetPart *parts, int count, NWidgetBase **parent, int *biggest_index)
{
	/* If *parent == nullptr, only the first widget is read and returned. Otherwise, *parent must point to either
	 * a #NWidgetContainer or a #NWidgetBackground object, and parts are added as much as possible. */
	NWidgetContainer *nwid_cont = dynamic_cast<NWidgetContainer *>(*parent);
	NWidgetBackground *nwid_parent = dynamic_cast<NWidgetBackground *>(*parent);
	assert(*parent == nullptr || (nwid_cont != nullptr && nwid_parent == nullptr) || (nwid_cont == nullptr && nwid_parent != nullptr));

	int total_used = 0;
	for (;;) {
		NWidgetBase *sub_widget = nullptr;
		bool fill_sub = false;
		int num_used = MakeNWidget(parts, count - total_used, &sub_widget, &fill_sub, biggest_index);
		parts += num_used;
		total_used += num_used;

		/* Break out of loop when end reached */
		if (sub_widget == nullptr) break;

		/* If sub-widget is a container, recursively fill that container. */
		WidgetType tp = sub_widget->type;
		if (fill_sub && (tp == NWID_HORIZONTAL || tp == NWID_HORIZONTAL_LTR || tp == NWID_VERTICAL || tp == NWID_MATRIX
							|| tp == WWT_PANEL || tp == WWT_FRAME || tp == WWT_INSET || tp == NWID_SELECTION)) {
			NWidgetBase *sub_ptr = sub_widget;
			int num_used = MakeWidgetTree(parts, count - total_used, &sub_ptr, biggest_index);
			parts += num_used;
			total_used += num_used;
		}

		/* Add sub_widget to parent container if available, otherwise return the widget to the caller. */
		if (nwid_cont != nullptr) nwid_cont->Add(sub_widget);
		if (nwid_parent != nullptr) nwid_parent->Add(sub_widget);
		if (nwid_cont == nullptr && nwid_parent == nullptr) {
			*parent = sub_widget;
			return total_used;
		}
	}

	if (count == total_used) return total_used; // Reached the end of the array of parts?

	assert(total_used < count);
	assert(parts->type == WPT_ENDCONTAINER);
	return total_used + 1; // *parts is also 'used'
}

/**
 * Construct a nested widget tree from an array of parts.
 * @param parts Array with parts of the widgets.
 * @param count Length of the \a parts array.
 * @param biggest_index Pointer to biggest nested widget index collected in the tree.
 * @param container Container to add the nested widgets to. In case it is nullptr a vertical container is used.
 * @return Root of the nested widget tree, a vertical container containing the entire GUI.
 * @ingroup NestedWidgetParts
 * @pre \c biggest_index != nullptr
 * @post \c *biggest_index contains the largest widget index of the tree and \c -1 if no index is used.
 */
NWidgetContainer *MakeNWidgets(const NWidgetPart *parts, int count, int *biggest_index, NWidgetContainer *container)
{
	*biggest_index = -1;
	if (container == nullptr) container = new NWidgetVertical();
	NWidgetBase *cont_ptr = container;
	MakeWidgetTree(parts, count, &cont_ptr, biggest_index);
	return container;
}

/**
 * Make a nested widget tree for a window from a parts array. Besides loading, it inserts a shading selection widget
 * between the title bar and the window body if the first widget in the parts array looks like a title bar (it is a horizontal
 * container with a caption widget) and has a shade box widget.
 * @param parts Array with parts of the widgets.
 * @param count Length of the \a parts array.
 * @param biggest_index Pointer to biggest nested widget index collected in the tree.
 * @param[out] shade_select Pointer to the inserted shade selection widget (\c nullptr if not unserted).
 * @return Root of the nested widget tree, a vertical container containing the entire GUI.
 * @ingroup NestedWidgetParts
 * @pre \c biggest_index != nullptr
 * @post \c *biggest_index contains the largest widget index of the tree and \c -1 if no index is used.
 */
NWidgetContainer *MakeWindowNWidgetTree(const NWidgetPart *parts, int count, int *biggest_index, NWidgetStacked **shade_select)
{
	*biggest_index = -1;

	/* Read the first widget recursively from the array. */
	NWidgetBase *nwid = nullptr;
	int num_used = MakeWidgetTree(parts, count, &nwid, biggest_index);
	assert(nwid != nullptr);
	parts += num_used;
	count -= num_used;

	NWidgetContainer *root = new NWidgetVertical;
	root->Add(nwid);
	if (count == 0) { // There is no body at all.
		*shade_select = nullptr;
		return root;
	}

	/* If the first widget looks like a titlebar, treat it as such.
	 * If it has a shading box, silently add a shade selection widget in the tree. */
	NWidgetHorizontal *hor_cont = dynamic_cast<NWidgetHorizontal *>(nwid);
	NWidgetContainer *body;
	if (hor_cont != nullptr && hor_cont->GetWidgetOfType(WWT_CAPTION) != nullptr && hor_cont->GetWidgetOfType(WWT_SHADEBOX) != nullptr) {
		*shade_select = new NWidgetStacked;
		root->Add(*shade_select);
		body = new NWidgetVertical;
		(*shade_select)->Add(body);
	} else {
		*shade_select = nullptr;
		body = root;
	}

	/* Load the remaining parts into 'body'. */
	int biggest2 = -1;
	MakeNWidgets(parts, count, &biggest2, body);

	*biggest_index = std::max(*biggest_index, biggest2);
	return root;
}

/**
 * Make a number of rows with button-like graphics, for enabling/disabling each company.
 * @param biggest_index Storage for collecting the biggest index used in the returned tree.
 * @param widget_first The first widget index to use.
 * @param widget_last The last widget index to use.
 * @param colour The colour in which to draw the button.
 * @param max_length Maximal number of company buttons in one row.
 * @param button_tooltip The tooltip-string of every button.
 * @return Panel with rows of company buttons.
 * @post \c *biggest_index contains the largest used index in the tree.
 */
NWidgetBase *MakeCompanyButtonRows(int *biggest_index, int widget_first, int widget_last, Colours button_colour, int max_length, StringID button_tooltip)
{
	assert(max_length >= 1);
	NWidgetVertical *vert = nullptr; // Storage for all rows.
	NWidgetHorizontal *hor = nullptr; // Storage for buttons in one row.
	int hor_length = 0;

	Dimension sprite_size = GetSpriteSize(SPR_COMPANY_ICON, nullptr, ZOOM_LVL_OUT_4X);
	sprite_size.width  += WidgetDimensions::unscaled.matrix.Horizontal();
	sprite_size.height += WidgetDimensions::unscaled.matrix.Vertical() + 1; // 1 for the 'offset' of being pressed

	for (int widnum = widget_first; widnum <= widget_last; widnum++) {
		/* Ensure there is room in 'hor' for another button. */
		if (hor_length == max_length) {
			if (vert == nullptr) vert = new NWidgetVertical();
			vert->Add(hor);
			hor = nullptr;
			hor_length = 0;
		}
		if (hor == nullptr) {
			hor = new NWidgetHorizontal();
			hor_length = 0;
		}

		NWidgetBackground *panel = new NWidgetBackground(WWT_PANEL, button_colour, widnum);
		panel->SetMinimalSize(sprite_size.width, sprite_size.height);
		panel->SetFill(1, 1);
		panel->SetResize(1, 0);
		panel->SetDataTip(0x0, button_tooltip);
		hor->Add(panel);
		hor_length++;
	}
	*biggest_index = widget_last;
	if (vert == nullptr) return hor; // All buttons fit in a single row.

	if (hor_length > 0 && hor_length < max_length) {
		/* Last row is partial, add a spacer at the end to force all buttons to the left. */
		NWidgetSpacer *spc = new NWidgetSpacer(sprite_size.width, sprite_size.height);
		spc->SetFill(1, 1);
		spc->SetResize(1, 0);
		hor->Add(spc);
	}
	if (hor != nullptr) vert->Add(hor);
	return vert;
}
