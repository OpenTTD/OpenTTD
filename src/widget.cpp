/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file widget.cpp Handling of the default/simple widgets. */

#include "stdafx.h"
#include "core/backup_type.hpp"
#include "company_func.h"
#include "settings_gui.h"
#include "strings_type.h"
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

WidgetDimensions WidgetDimensions::scaled = {};

static std::string GetStringForWidget(const Window *w, const NWidgetCore *nwid, bool secondary = false)
{
	StringID stringid = nwid->GetString();
	if (nwid->GetIndex() < 0) {
		if (stringid == STR_NULL) return {};

		return GetString(stringid + (secondary ? 1 : 0));
	}

	return w->GetWidgetString(nwid->GetIndex(), stringid + (secondary ? 1 : 0));
}

/**
 * Scale a RectPadding to GUI zoom level.
 * @param r RectPadding at ZOOM_BASE (traditional "normal" interface size).
 * @return RectPadding at current interface size.
 */
static inline RectPadding ScaleGUITrad(const RectPadding &r)
{
	return {(uint8_t)ScaleGUITrad(r.left), (uint8_t)ScaleGUITrad(r.top), (uint8_t)ScaleGUITrad(r.right), (uint8_t)ScaleGUITrad(r.bottom)};
}

/**
 * Scale a Dimension to GUI zoom level.
 * @param d Dimension at ZOOM_BASE (traditional "normal" interface size).
 * @return Dimension at current interface size.
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
	Dimension d = GetSpriteSize(sprid, &offset, ZoomLevel::Normal);
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
	WidgetDimensions::scaled.dropdownlist = ScaleGUITrad(WidgetDimensions::unscaled.dropdownlist);
	WidgetDimensions::scaled.modalpopup   = ScaleGUITrad(WidgetDimensions::unscaled.modalpopup);

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
		case SA_HOR_CENTER: p.x = CentreBounds(r.left, r.right, d.width); break;
		case SA_RIGHT:      p.x = r.right + 1 - d.width; break;
		default: NOT_REACHED();
	}
	switch (align & SA_VERT_MASK) {
		case SA_TOP:         p.y = r.top; break;
		case SA_VERT_CENTER: p.y = CentreBounds(r.top, r.bottom, d.height); break;
		case SA_BOTTOM:      p.y = r.bottom + 1 - d.height; break;
		default: NOT_REACHED();
	}
	return p;
}

/**
 * Compute the vertical position of the draggable part of scrollbar
 * @param sb Scrollbar list data
 * @param mi Minimum coordinate of the scroll bar.
 * @param ma Maximum coordinate of the scroll bar.
 * @param horizontal Whether the scrollbar is horizontal or not
 * @return A pair, with first containing the minimum coordinate of the draggable part, and
 *                      second containing the maximum coordinate of the draggable part
 */
static std::pair<int, int> HandleScrollbarHittest(const Scrollbar *sb, int mi, int ma, bool horizontal)
{
	/* Base for reversion */
	int rev_base = mi + ma;
	int button_size = horizontal ? NWidgetScrollbar::GetHorizontalDimension().width : NWidgetScrollbar::GetVerticalDimension().height;

	mi += button_size; // now points to just after the up/left-button
	ma -= button_size; // now points to just before the down/right-button

	int count = sb->GetCount();
	int cap = sb->GetCapacity();

	if (count > cap) {
		int height = ma + 1 - mi;
		int slider_height = std::max(button_size, cap * height / count);
		height -= slider_height;

		mi += height * sb->GetPosition() / (count - cap);
		ma = mi + slider_height - 1;
	}

	/* Reverse coordinates for RTL. */
	if (horizontal && _current_text_dir == TD_RTL) return {rev_base - ma, rev_base - mi};

	return {mi, ma};
}

/**
 * Compute new position of the scrollbar after a click and updates the window flags.
 * @param w Window on which a scroll was performed.
 * @param sb Scrollbar
 * @param x The X coordinate of the mouse click.
 * @param y The Y coordinate of the mouse click.
 * @param mi Minimum coordinate of the scroll bar.
 * @param ma Maximum coordinate of the scroll bar.
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
		sb->disp_flags.Set(NWidgetDisplayFlag::ScrollbarUp);
		if (_scroller_click_timeout <= 1) {
			_scroller_click_timeout = 3;
			changed = sb->UpdatePosition(rtl ? 1 : -1);
		}
		w->mouse_capture_widget = sb->GetIndex();
	} else if (pos >= ma - button_size) {
		/* Pressing the lower button? */
		sb->disp_flags.Set(NWidgetDisplayFlag::ScrollbarDown);

		if (_scroller_click_timeout <= 1) {
			_scroller_click_timeout = 3;
			changed = sb->UpdatePosition(rtl ? -1 : 1);
		}
		w->mouse_capture_widget = sb->GetIndex();
	} else {
		auto [start, end] = HandleScrollbarHittest(sb, mi, ma, sb->type == NWID_HSCROLLBAR);

		if (pos < start) {
			changed = sb->UpdatePosition(rtl ? 1 : -1, Scrollbar::SS_BIG);
		} else if (pos > end) {
			changed = sb->UpdatePosition(rtl ? -1 : 1, Scrollbar::SS_BIG);
		} else {
			_scrollbar_start_pos = start - mi - button_size;
			_scrollbar_size = ma - mi - button_size * 2 - (end - start);
			w->mouse_capture_widget = sb->GetIndex();
			_cursorpos_drag_start = _cursor.pos;
		}
	}

	if (changed) {
		/* Position changed so refresh the window */
		w->OnScrollbarScroll(sb->GetIndex());
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
 * @return A widget index, or \c INVALID_WIDGET if no widget was found.
 */
WidgetID GetWidgetFromPos(const Window *w, int x, int y)
{
	NWidgetCore *nw = w->nested_root->GetWidgetFromPos(x, y);
	return (nw != nullptr) ? nw->GetIndex() : INVALID_WIDGET;
}

/**
 * Draw frame rectangle.
 * @param left   Left edge of the frame
 * @param top    Top edge of the frame
 * @param right  Right edge of the frame
 * @param bottom Bottom edge of the frame
 * @param colour Colour table to use. @see Colours
 * @param flags  Flags controlling how to draw the frame. @see FrameFlags
 */
void DrawFrameRect(int left, int top, int right, int bottom, Colours colour, FrameFlags flags)
{
	if (flags.Test(FrameFlag::Transparent)) {
		GfxFillRect(left, top, right, bottom, PALETTE_TO_TRANSPARENT, FILLRECT_RECOLOUR);
	} else {
		assert(colour < COLOUR_END);

		const PixelColour dark         = GetColourGradient(colour, SHADE_DARK);
		const PixelColour medium_dark  = GetColourGradient(colour, SHADE_LIGHT);
		const PixelColour medium_light = GetColourGradient(colour, SHADE_LIGHTER);
		const PixelColour light        = GetColourGradient(colour, SHADE_LIGHTEST);
		PixelColour interior;

		Rect outer = {left, top, right, bottom};                   // Outside rectangle
		Rect inner = outer.Shrink(WidgetDimensions::scaled.bevel); // Inside rectangle

		if (flags.Test(FrameFlag::Lowered)) {
			GfxFillRect(outer.left,      outer.top,        inner.left - 1,  outer.bottom, dark);   // Left
			GfxFillRect(inner.left,      outer.top,        outer.right,     inner.top - 1, dark);  // Top
			GfxFillRect(inner.right + 1, inner.top,        outer.right,     inner.bottom,  light); // Right
			GfxFillRect(inner.left,      inner.bottom + 1, outer.right,     outer.bottom, light);  // Bottom
			interior = (flags.Test(FrameFlag::Darkened) ? medium_dark : medium_light);
		} else {
			GfxFillRect(outer.left,      outer.top,        inner.left - 1, inner.bottom,  light); // Left
			GfxFillRect(inner.left,      outer.top,        inner.right,    inner.top - 1, light); // Top
			GfxFillRect(inner.right + 1, outer.top,        outer.right,    inner.bottom,  dark);  // Right
			GfxFillRect(outer.left,      inner.bottom + 1, outer.right,    outer.bottom, dark);   // Bottom
			interior = medium_dark;
		}
		if (!flags.Test(FrameFlag::BorderOnly)) {
			GfxFillRect(inner.left,  inner.top, inner.right, inner.bottom, interior); // Inner
		}
	}
}

void DrawSpriteIgnorePadding(SpriteID img, PaletteID pal, const Rect &r, StringAlignment align)
{
	Point offset;
	Dimension d = GetSpriteSize(img, &offset);
	d.width  -= offset.x;
	d.height -= offset.y;

	Point p = GetAlignedPosition(r, d, align);
	DrawSprite(img, pal, p.x - offset.x, p.y - offset.y);
}

/**
 * Draw an image button.
 * @param r       Rectangle of the button.
 * @param type    Widget type (#WWT_IMGBTN or #WWT_IMGBTN_2).
 * @param colour  Colour of the button.
 * @param clicked Button is clicked.
 * @param img     Sprite to draw.
 * @param align   Alignment of the sprite.
 */
static inline void DrawImageButtons(const Rect &r, WidgetType type, Colours colour, bool clicked, SpriteID img, StringAlignment align)
{
	assert(img != 0);
	DrawFrameRect(r, colour, clicked ? FrameFlag::Lowered : FrameFlags{});

	if ((type & WWT_MASK) == WWT_IMGBTN_2 && clicked) img++; // Show different image when clicked for #WWT_IMGBTN_2.
	DrawSpriteIgnorePadding(img, PAL_NONE, r, align);
}

/**
 * Draw a button with image and rext.
 * @param r       Rectangle of the button.
 * @param colour  Colour of the button.
 * @param clicked Button is clicked.
 * @param img     Image caption.
 * @param text_colour Colour of the text.
 * @param text    Text caption.
 * @param align   Alignment of the caption.
 * @param fs      Font size of the text.
 */
static inline void DrawImageTextButtons(const Rect &r, Colours colour, bool clicked, SpriteID img, TextColour text_colour, const std::string &text, StringAlignment align, FontSize fs)
{
	DrawFrameRect(r, colour, clicked ? FrameFlag::Lowered : FrameFlags{});

	bool rtl = _current_text_dir == TD_RTL;
	int image_width = img != 0 ? GetScaledSpriteSize(img).width : 0;
	Rect r_img = r.Shrink(WidgetDimensions::scaled.framerect).WithWidth(image_width, rtl);
	Rect r_text = r.Shrink(WidgetDimensions::scaled.framerect).Indent(image_width + WidgetDimensions::scaled.hsep_wide, rtl);

	if (img != 0) {
		DrawSpriteIgnorePadding(img, PAL_NONE, r_img, SA_HOR_CENTER | (align & SA_VERT_MASK));
	}

	if (!text.empty()) {
		Dimension d = GetStringBoundingBox(text, fs);
		Point p = GetAlignedPosition(r_text, d, align);
		DrawString(r_text.left, r_text.right, p.y, text, text_colour, align, false, fs);
	}
}

/**
 * Draw the label-part of a widget.
 * @param r       Rectangle of the label background.
 * @param colour  Colour of the text.
 * @param str     Text to draw.
 * @param align   Alignment of the text.
 * @param fs      Font size of the text.
 */
static inline void DrawLabel(const Rect &r, TextColour colour, std::string_view str, StringAlignment align, FontSize fs)
{
	if (str.empty()) return;

	Dimension d = GetStringBoundingBox(str, fs);
	Point p = GetAlignedPosition(r, d, align);
	DrawString(r.left, r.right, p.y, str, colour, align, false, fs);
}

/**
 * Draw text.
 * @param r      Rectangle of the background.
 * @param colour Colour of the text.
 * @param str    Text to draw.
 * @param align  Alignment of the text.
 * @param fs     Font size of the text.
 */
static inline void DrawText(const Rect &r, TextColour colour, std::string_view str, StringAlignment align, FontSize fs)
{
	if (str.empty()) return;

	Dimension d = GetStringBoundingBox(str, fs);
	Point p = GetAlignedPosition(r, d, align);
	DrawString(r.left, r.right, p.y, str, colour, align, false, fs);
}

/**
 * Draw an inset widget.
 * @param r           Rectangle of the background.
 * @param colour      Colour of the inset.
 * @param text_colour Colour of the text.
 * @param str         Text to draw.
 * @param align       Alignment of the text.
 * @param fs          Font size of the text.
 */
static inline void DrawInset(const Rect &r, Colours colour, TextColour text_colour, std::string_view str, StringAlignment align, FontSize fs)
{
	DrawFrameRect(r, colour, {FrameFlag::Lowered, FrameFlag::Darkened});
	if (!str.empty()) DrawString(r.Shrink(WidgetDimensions::scaled.inset), str, text_colour, align, false, fs);
}

/**
 * Draw a matrix widget.
 * @param r       Rectangle of the matrix background.
 * @param colour  Colour of the background.
 * @param clicked Matrix is rendered lowered.
 * @param num_columns The number of columns in the matrix.
 * @param num_rows The number of rows in the matrix.
 * @param resize_x Matrix resize unit size.
 * @param resize_y Matrix resize unit size.
 */
static inline void DrawMatrix(const Rect &r, Colours colour, bool clicked, uint32_t num_columns, uint32_t num_rows, uint resize_x, uint resize_y)
{
	DrawFrameRect(r, colour, clicked ? FrameFlag::Lowered : FrameFlags{});

	int column_width; // Width of a single column in the matrix.
	if (num_columns == 0) {
		column_width = resize_x;
		num_columns = r.Width() / column_width;
	} else {
		column_width = r.Width() / num_columns;
	}

	int row_height; // Height of a single row in the matrix.
	if (num_rows == 0) {
		row_height = resize_y;
		num_rows = r.Height() / row_height;
	} else {
		row_height = r.Height() / num_rows;
	}

	PixelColour col = GetColourGradient(colour, SHADE_LIGHTER);

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

	col = GetColourGradient(colour, SHADE_NORMAL);

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

	PixelColour c1 = GetColourGradient(colour, SHADE_DARK);
	PixelColour c2 = GetColourGradient(colour, SHADE_LIGHTEST);

	/* draw "shaded" background */
	Rect bg = r.Shrink(0, height);
	GfxFillRect(bg, c2);
	GfxFillRect(bg, c1, FILLRECT_CHECKER);

	/* track positions. These fractions are based on original 1x dimensions, but scale better. */
	int left  = r.left + r.Width() * 3 / 11; /*  left track is positioned 3/11ths from the left */
	int right = r.left + r.Width() * 8 / 11; /* right track is positioned 8/11ths from the left */
	const uint8_t bl = WidgetDimensions::scaled.bevel.left;
	const uint8_t br = WidgetDimensions::scaled.bevel.right;

	/* draw shaded lines */
	GfxFillRect(bg.WithX(left - bl,  left       - 1), c1);
	GfxFillRect(bg.WithX(left,       left + br  - 1), c2);
	GfxFillRect(bg.WithX(right - bl, right      - 1), c1);
	GfxFillRect(bg.WithX(right,      right + br - 1), c2);

	auto [top, bottom] = HandleScrollbarHittest(scrollbar, r.top, r.bottom, false);
	DrawFrameRect(r.left, top, r.right, bottom, colour, bar_dragged ? FrameFlag::Lowered : FrameFlags{});
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

	PixelColour c1 = GetColourGradient(colour, SHADE_DARK);
	PixelColour c2 = GetColourGradient(colour, SHADE_LIGHTEST);

	/* draw "shaded" background */
	Rect bg = r.Shrink(width, 0);
	GfxFillRect(bg, c2);
	GfxFillRect(bg, c1, FILLRECT_CHECKER);

	/* track positions. These fractions are based on original 1x dimensions, but scale better. */
	int top    = r.top + r.Height() * 3 / 11; /*    top track is positioned 3/11ths from the top */
	int bottom = r.top + r.Height() * 8 / 11; /* bottom track is positioned 8/11ths from the top */
	const uint8_t bt = WidgetDimensions::scaled.bevel.top;
	const uint8_t bb = WidgetDimensions::scaled.bevel.bottom;

	/* draw shaded lines */
	GfxFillRect(bg.WithY(top - bt,    top         - 1), c1);
	GfxFillRect(bg.WithY(top,         top + bb    - 1), c2);
	GfxFillRect(bg.WithY(bottom - bt, bottom      - 1), c1);
	GfxFillRect(bg.WithY(bottom,      bottom + bb - 1), c2);

	/* draw actual scrollbar */
	auto [left, right] = HandleScrollbarHittest(scrollbar, r.left, r.right, true);
	DrawFrameRect(left, r.top, right, r.bottom, colour, bar_dragged ? FrameFlag::Lowered : FrameFlags{});
}

/**
 * Draw a frame widget.
 * @param r           Rectangle of the frame.
 * @param colour      Colour of the frame.
 * @param text_colour Colour of the text.
 * @param str         Text of the frame.
 * @param align       Alignment of the text in the frame.
 * @param fs          Font size of the text.
 */
static inline void DrawFrame(const Rect &r, Colours colour, TextColour text_colour, std::string_view str, StringAlignment align, FontSize fs)
{
	int x2 = r.left; // by default the left side is the left side of the widget

	if (!str.empty()) x2 = DrawString(r.left + WidgetDimensions::scaled.frametext.left, r.right - WidgetDimensions::scaled.frametext.right, r.top, str, text_colour, align, false, fs);

	PixelColour c1 = GetColourGradient(colour, SHADE_DARK);
	PixelColour c2 = GetColourGradient(colour, SHADE_LIGHTEST);

	/* If the frame has text, adjust the top bar to fit half-way through */
	Rect inner = r.Shrink(ScaleGUITrad(1));
	if (!str.empty()) inner.top = r.top + GetCharacterHeight(FS_NORMAL) / 2;

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
 * @param bevel   Draw bevel iff set.
 */
static inline void DrawResizeBox(const Rect &r, Colours colour, bool at_left, bool clicked, bool bevel)
{
	if (bevel) {
		DrawFrameRect(r, colour, clicked ? FrameFlag::Lowered : FrameFlags{});
	} else if (clicked) {
		GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), GetColourGradient(colour, SHADE_LIGHTER));
	}
	DrawSpriteIgnorePadding(at_left ? SPR_WINDOW_RESIZE_LEFT : SPR_WINDOW_RESIZE_RIGHT, PAL_NONE, r.Shrink(ScaleGUITrad(2)), at_left ? (SA_LEFT | SA_BOTTOM | SA_FORCE) : (SA_RIGHT | SA_BOTTOM | SA_FORCE));
}

/**
 * Draw a close box.
 * @param r      Rectangle of the box.`
 * @param colour Colour of the close box.
 */
static inline void DrawCloseBox(const Rect &r, Colours colour)
{
	if (colour != COLOUR_WHITE) DrawFrameRect(r, colour, {});
	Point offset;
	Dimension d = GetSpriteSize(SPR_CLOSEBOX, &offset);
	d.width  -= offset.x;
	d.height -= offset.y;
	int s = ScaleSpriteTrad(1); /* Offset to account for shadow of SPR_CLOSEBOX */
	DrawSprite(SPR_CLOSEBOX, (colour != COLOUR_WHITE ? TC_BLACK : TC_SILVER) | (1U << PALETTE_TEXT_RECOLOUR), CentreBounds(r.left, r.right, d.width - s) - offset.x, CentreBounds(r.top, r.bottom, d.height - s) - offset.y);
}

/**
 * Draw a caption bar.
 * @param r           Rectangle of the bar.
 * @param colour      Colour of the window.
 * @param owner       'Owner' of the window.
 * @param text_colour Colour of the text.
 * @param str         Text to draw in the bar.
 * @param align       Alignment of the text.
 * @param fs          Font size of the text.
 */
void DrawCaption(const Rect &r, Colours colour, Owner owner, TextColour text_colour, std::string_view str, StringAlignment align, FontSize fs)
{
	bool company_owned = owner < MAX_COMPANIES;

	DrawFrameRect(r, colour, FrameFlag::BorderOnly);
	Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
	DrawFrameRect(ir, colour, company_owned ? FrameFlags{FrameFlag::Lowered, FrameFlag::Darkened, FrameFlag::BorderOnly} : FrameFlags{FrameFlag::Lowered, FrameFlag::Darkened});

	if (company_owned) {
		GfxFillRect(ir.Shrink(WidgetDimensions::scaled.bevel), GetColourGradient(_company_colours[owner], SHADE_NORMAL));
	}

	if (str.empty()) return;

	Dimension d = GetStringBoundingBox(str);
	Point p = GetAlignedPosition(r, d, align);
	DrawString(r.left + WidgetDimensions::scaled.captiontext.left, r.right - WidgetDimensions::scaled.captiontext.left, p.y, str, text_colour, align, false, fs);
}

/**
 * Draw a button with a dropdown (#WWT_DROPDOWN and #NWID_BUTTON_DROPDOWN).
 * @param r                Rectangle containing the widget.
 * @param colour           Background colour of the widget.
 * @param clicked_button   The button-part is clicked.
 * @param clicked_dropdown The drop-down part is clicked.
 * @param str              Text of the button.
 * @param align            Alignment of the text within the dropdown.
 *
 * @note Magic constants are also used in #NWidgetLeaf::ButtonHit.
 */
static inline void DrawButtonDropdown(const Rect &r, Colours colour, bool clicked_button, bool clicked_dropdown, std::string_view str, StringAlignment align)
{
	bool rtl = _current_text_dir == TD_RTL;

	Rect text = r.Indent(NWidgetLeaf::dropdown_dimension.width, !rtl);
	DrawFrameRect(text, colour, clicked_button ? FrameFlag::Lowered : FrameFlags{});
	if (!str.empty()) {
		text = text.CentreToHeight(GetCharacterHeight(FS_NORMAL)).Shrink(WidgetDimensions::scaled.dropdowntext, RectPadding::zero);
		DrawString(text, str, TC_BLACK, align);
	}

	Rect button = r.WithWidth(NWidgetLeaf::dropdown_dimension.width, !rtl);
	DrawImageButtons(button, WWT_DROPDOWN, colour, clicked_dropdown, SPR_ARROW_DOWN, SA_CENTER);
}

/**
 * Paint all widgets of a window.
 */
void Window::DrawWidgets() const
{
	this->nested_root->Draw(this);

	if (this->flags.Test(WindowFlag::WhiteBorder)) {
		DrawFrameRect(0, 0, this->width - 1, this->height - 1, COLOUR_WHITE, FrameFlag::BorderOnly);
	}

	if (this->flags.Test(WindowFlag::Highlighted)) {
		extern bool _window_highlight_colour;
		for (const auto &pair : this->widget_lookup) {
			const NWidgetBase *widget = pair.second;
			if (!widget->IsHighlighted()) continue;

			Rect outer = widget->GetCurrentRect();
			Rect inner = outer.Shrink(WidgetDimensions::scaled.bevel).Expand(1);

			PixelColour colour = _string_colourmap[_window_highlight_colour ? widget->GetHighlightColour() : TC_WHITE];

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
void Window::DrawSortButtonState(WidgetID widget, SortButtonState state) const
{
	if (state == SBS_OFF) return;

	assert(!this->widget_lookup.empty());
	Rect r = this->GetWidget<NWidgetBase>(widget)->GetCurrentRect();

	/* Sort button uses the same sprites as vertical scrollbar */
	Dimension dim = NWidgetScrollbar::GetVerticalDimension();

	DrawSpriteIgnorePadding(state == SBS_DOWN ? SPR_ARROW_DOWN : SPR_ARROW_UP, PAL_NONE, r.WithWidth(dim.width, _current_text_dir == TD_LTR), SA_CENTER);
}

/**
 * Get width of up/down arrow of sort button state.
 * @return Width of space required by sort button arrow.
 */
int Window::SortButtonWidth()
{
	return NWidgetScrollbar::GetVerticalDimension().width + 1;
}

bool _draw_widget_outlines;

static void DrawOutline(const Window *, const NWidgetBase *wid)
{
	if (!_draw_widget_outlines || wid->current_x == 0 || wid->current_y == 0) return;

	DrawRectOutline(wid->GetCurrentRect(), PC_WHITE, 1, 4);
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
 * @fn void NWidgetBase::SetupSmallestSize(Window *w)
 * Compute smallest size needed by the widget.
 *
 * The smallest size of a widget is the smallest size that a widget needs to
 * display itself properly. In addition, filling and resizing of the widget are computed.
 * The function calls #Window::UpdateWidgetSize for each leaf widget and
 * background widget without child with a non-negative index.
 *
 * @param w          Window owning the widget.
 *
 * @note After the computation, the results can be queried by accessing the #smallest_x and #smallest_y data members of the widget.
 */

/**
 * @fn void NWidgetBase::AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl)
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
 * @fn void NWidgetBase::FillWidgetLookup(WidgetLookup &widget_lookup)
 * Fill the Window::widget_lookup with pointers to nested widgets in the tree.
 * @param widget_lookup The WidgetLookup.
 */
void NWidgetBase::FillWidgetLookup(WidgetLookup &widget_lookup)
{
	if (this->index >= 0) widget_lookup[this->index] = this;
}

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

void NWidgetBase::ApplyAspectRatio()
{
	if (this->aspect_ratio == 0) return;
	if (this->smallest_x == 0 || this->smallest_y == 0) return;

	uint x = this->smallest_x;
	uint y = this->smallest_y;
	if (this->aspect_flags.Test(AspectFlag::ResizeX)) x = std::max(this->smallest_x, static_cast<uint>(this->smallest_y * std::abs(this->aspect_ratio)));
	if (this->aspect_flags.Test(AspectFlag::ResizeY)) y = std::max(this->smallest_y, static_cast<uint>(this->smallest_x / std::abs(this->aspect_ratio)));

	this->smallest_x = x;
	this->smallest_y = y;
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
NWidgetResizeBase::NWidgetResizeBase(WidgetType tp, WidgetID index, uint fill_x, uint fill_y) : NWidgetBase(tp, index)
{
	this->fill_x = fill_x;
	this->fill_y = fill_y;
}

/**
 * Set desired aspect ratio of this widget.
 * @param ratio Desired aspect ratio, or 0 for none.
 * @param flags Dimensions which should be resized.
 */
void NWidgetResizeBase::SetAspect(float ratio, AspectFlags flags)
{
	this->aspect_ratio = ratio;
	this->aspect_flags = flags;
}

/**
 * Set desired aspect ratio of this widget, in terms of horizontal and vertical dimensions.
 * @param x_ratio Desired horizontal component of aspect ratio.
 * @param y_ratio Desired vertical component of aspect ratio.
 * @param flags Dimensions which should be resized.
 */
void NWidgetResizeBase::SetAspect(int x_ratio, int y_ratio, AspectFlags flags)
{
	this->SetAspect(static_cast<float>(x_ratio) / static_cast<float>(y_ratio), flags);
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
void NWidgetResizeBase::SetMinimalTextLines(uint8_t min_lines, uint8_t spacing, FontSize size)
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

/**
 * Try to set optimum widget size for a multiline text widget.
 * The window will need to be reinited if the size is changed.
 * @param str Multiline string contents that will fill the widget.
 * @param max_line Maximum number of lines.
 * @return true iff the widget minimum size has changed.
 */
bool NWidgetResizeBase::UpdateMultilineWidgetSize(const std::string &str, int max_lines)
{
	int y = GetStringHeight(str, this->current_x);
	if (y > max_lines * GetCharacterHeight(FS_NORMAL)) {
		/* Text at the current width is too tall, so try to guess a better width. */
		Dimension d = GetStringBoundingBox(str);
		d.height *= max_lines;
		d.width /= 2;
		return this->UpdateSize(d.width, d.height);
	}
	return this->UpdateVerticalSize(y);
}

/**
 * Set absolute (post-scaling) minimal size of the widget.
 * The window will need to be reinited if the size is changed.
 * @param min_x Horizontal minimal size of the widget.
 * @param min_y Vertical minimal size of the widget.
 * @return true iff the widget minimum size has changed.
 */
bool NWidgetResizeBase::UpdateSize(uint min_x, uint min_y)
{
	if (min_x == this->min_x && min_y == this->min_y) return false;
	this->min_x = min_x;
	this->min_y = min_y;
	return true;
}

/**
 * Set absolute (post-scaling) minimal size of the widget.
 * The window will need to be reinited if the size is changed.
 * @param min_y Vertical minimal size of the widget.
 * @return true iff the widget minimum size has changed.
 */
bool NWidgetResizeBase::UpdateVerticalSize(uint min_y)
{
	if (min_y == this->min_y) return false;
	this->min_y = min_y;
	return true;
}

void NWidgetResizeBase::AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool)
{
	this->StoreSizePosition(sizing, x, y, given_width, given_height);
}

/**
 * Initialization of a 'real' widget.
 * @param tp          Type of the widget.
 * @param colour      Colour of the widget.
 * @param index       Index of the widget.
 * @param fill_x      Default horizontal filling.
 * @param fill_y      Default vertical filling.
 * @param widget_data Data component of the widget. @see Widget::data
 * @param tool_tip    Tool tip of the widget. @see Widget::tooltips
 */
NWidgetCore::NWidgetCore(WidgetType tp, Colours colour, WidgetID index, uint fill_x, uint fill_y, const WidgetData &widget_data, StringID tool_tip) : NWidgetResizeBase(tp, index, fill_x, fill_y)
{
	this->colour = colour;
	this->widget_data = widget_data;
	this->SetToolTip(tool_tip);
	this->text_colour = tp == WWT_CAPTION ? TC_WHITE : TC_BLACK;
}

/**
 * Set string of the nested widget.
 * @param string The new string.
 */
void NWidgetCore::SetString(StringID string)
{
	this->widget_data.string = string;
}

/**
 * Set string and tool tip of the nested widget.
 * @param string The new string.
 * @param tool_tip The new tool_tip.
 */
void NWidgetCore::SetStringTip(StringID string, StringID tool_tip)
{
	this->SetString(string);
	this->SetToolTip(tool_tip);
}

/**
 * Set sprite of the nested widget.
 * @param sprite The new sprite.
 */
void NWidgetCore::SetSprite(SpriteID sprite)
{
	this->widget_data.sprite = sprite;
}

/**
 * Set sprite and tool tip of the nested widget.
 * @param sprite The new sprite.
 * @param tool_tip The new tool_tip.
 */
void NWidgetCore::SetSpriteTip(SpriteID sprite, StringID tool_tip)
{
	this->SetSprite(sprite);
	this->SetToolTip(tool_tip);
}

/**
 * Set the matrix dimension.
 * @param columns The number of columns in the matrix (0 for autoscaling).
 * @param rows The number of rows in the matrix (0 for autoscaling).
 */
void NWidgetCore::SetMatrixDimension(uint32_t columns, uint32_t rows)
{
	this->widget_data.matrix = { columns, rows };
}

/**
 * Set the resize widget type of the nested widget.
 * @param type The new resize widget.
 */
void NWidgetCore::SetResizeWidgetType(ResizeWidgetValues type)
{
	this->widget_data.resize_widget_type = type;
}

/**
 * Set the text style of the nested widget.
 * @param colour TextColour to use.
 * @param size Font size to use.
 */
void NWidgetCore::SetTextStyle(TextColour colour, FontSize size)
{
	this->text_colour = colour;
	this->text_size = size;
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
 * Get the tool tip of the nested widget.
 * @return The tool tip string.
 */
StringID NWidgetCore::GetToolTip() const
{
	return this->tool_tip;
}

/**
 * Set the text/image alignment of the nested widget.
 * @param align Alignment to use.
 */
void NWidgetCore::SetAlignment(StringAlignment align)
{
	this->align = align;
}

/**
 * Get the string that has been set for this nested widget.
 * @return The string.
 */
StringID NWidgetCore::GetString() const
{
	return this->widget_data.string;
}

/**
 * Get the \c WidgetID of this nested widget's scrollbar.
 * @return The \c WidgetID.
 */
WidgetID NWidgetCore::GetScrollbarIndex() const
{
	return this->scrollbar_index;
}

NWidgetCore *NWidgetCore::GetWidgetFromPos(int x, int y)
{
	return (IsInsideBS(x, this->pos_x, this->current_x) && IsInsideBS(y, this->pos_y, this->current_y)) ? this : nullptr;
}

NWidgetBase *NWidgetContainer::GetWidgetOfType(WidgetType tp)
{
	if (this->type == tp) return this;
	for (const auto &child_wid : this->children) {
		NWidgetBase *nwid = child_wid->GetWidgetOfType(tp);
		if (nwid != nullptr) return nwid;
	}
	return nullptr;
}

void NWidgetContainer::AdjustPaddingForZoom()
{
	for (const auto &child_wid : this->children) {
		child_wid->AdjustPaddingForZoom();
	}
	NWidgetBase::AdjustPaddingForZoom();
}

/**
 * Append widget \a wid to container.
 * @param wid Widget to append.
 */
void NWidgetContainer::Add(std::unique_ptr<NWidgetBase> &&wid)
{
	assert(wid != nullptr);
	wid->parent = this;
	this->children.push_back(std::move(wid));
}

void NWidgetContainer::FillWidgetLookup(WidgetLookup &widget_lookup)
{
	this->NWidgetBase::FillWidgetLookup(widget_lookup);
	for (const auto &child_wid : this->children) {
		child_wid->FillWidgetLookup(widget_lookup);
	}
}

void NWidgetContainer::Draw(const Window *w)
{
	for (const auto &child_wid : this->children) {
		child_wid->Draw(w);
	}

	DrawOutline(w, this);
}

NWidgetCore *NWidgetContainer::GetWidgetFromPos(int x, int y)
{
	if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return nullptr;

	for (const auto &child_wid : this->children) {
		NWidgetCore *nwid = child_wid->GetWidgetFromPos(x, y);
		if (nwid != nullptr) return nwid;
	}
	return nullptr;
}

void NWidgetStacked::SetupSmallestSize(Window *w)
{
	/* Zero size plane selected */
	if (this->shown_plane >= SZSP_BEGIN) {
		Dimension size    = {0, 0};
		Dimension padding = {0, 0};
		Dimension fill    = {(this->shown_plane == SZSP_HORIZONTAL), (this->shown_plane == SZSP_VERTICAL)};
		Dimension resize  = {(this->shown_plane == SZSP_HORIZONTAL), (this->shown_plane == SZSP_VERTICAL)};
		/* Here we're primarily interested in the value of resize */
		if (this->index >= 0) w->UpdateWidgetSize(this->index, size, padding, fill, resize);

		this->smallest_x = size.width;
		this->smallest_y = size.height;
		this->fill_x = fill.width;
		this->fill_y = fill.height;
		this->resize_x = resize.width;
		this->resize_y = resize.height;
		this->ApplyAspectRatio();
		return;
	}

	/* First sweep, recurse down and compute minimal size and filling. */
	this->smallest_x = 0;
	this->smallest_y = 0;
	this->fill_x = this->IsEmpty() ? 0 : 1;
	this->fill_y = this->IsEmpty() ? 0 : 1;
	this->resize_x = this->IsEmpty() ? 0 : 1;
	this->resize_y = this->IsEmpty() ? 0 : 1;
	for (const auto &child_wid : this->children) {
		child_wid->SetupSmallestSize(w);

		this->smallest_x = std::max(this->smallest_x, child_wid->smallest_x + child_wid->padding.Horizontal());
		this->smallest_y = std::max(this->smallest_y, child_wid->smallest_y + child_wid->padding.Vertical());
		this->fill_x = std::lcm(this->fill_x, child_wid->fill_x);
		this->fill_y = std::lcm(this->fill_y, child_wid->fill_y);
		this->resize_x = std::lcm(this->resize_x, child_wid->resize_x);
		this->resize_y = std::lcm(this->resize_y, child_wid->resize_y);
		this->ApplyAspectRatio();
	}
}

void NWidgetStacked::AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl)
{
	assert(given_width >= this->smallest_x && given_height >= this->smallest_y);
	this->StoreSizePosition(sizing, x, y, given_width, given_height);

	if (this->shown_plane >= SZSP_BEGIN) return;

	for (const auto &child_wid : this->children) {
		uint hor_step = (sizing == ST_SMALLEST) ? 1 : child_wid->GetHorizontalStepSize(sizing);
		uint child_width = ComputeMaxSize(child_wid->smallest_x, given_width - child_wid->padding.Horizontal(), hor_step);
		uint child_pos_x = (rtl ? child_wid->padding.right : child_wid->padding.left);

		uint vert_step = (sizing == ST_SMALLEST) ? 1 : child_wid->GetVerticalStepSize(sizing);
		uint child_height = ComputeMaxSize(child_wid->smallest_y, given_height - child_wid->padding.Vertical(), vert_step);
		uint child_pos_y = child_wid->padding.top;

		child_wid->AssignSizePosition(sizing, x + child_pos_x, y + child_pos_y, child_width, child_height, rtl);
	}
}

void NWidgetStacked::FillWidgetLookup(WidgetLookup &widget_lookup)
{
	/* We need to update widget_lookup later. */
	this->widget_lookup = &widget_lookup;

	this->NWidgetContainer::FillWidgetLookup(widget_lookup);
	/* In case widget IDs are repeated, make sure Window::GetWidget works on displayed widgets. */
	if (static_cast<size_t>(this->shown_plane) < this->children.size()) this->children[shown_plane]->FillWidgetLookup(widget_lookup);
}

void NWidgetStacked::Draw(const Window *w)
{
	if (this->shown_plane >= SZSP_BEGIN) return;

	assert(static_cast<size_t>(this->shown_plane) < this->children.size());
	this->children[shown_plane]->Draw(w);
	DrawOutline(w, this);
}

NWidgetCore *NWidgetStacked::GetWidgetFromPos(int x, int y)
{
	if (this->shown_plane >= SZSP_BEGIN) return nullptr;

	if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return nullptr;

	if (static_cast<size_t>(this->shown_plane) >= this->children.size()) return nullptr;
	return this->children[shown_plane]->GetWidgetFromPos(x, y);
}

/**
 * Select which plane to show (for #NWID_SELECTION only).
 * @param plane Plane number to display.
 * @return true iff the shown plane changed.
 */
bool NWidgetStacked::SetDisplayedPlane(int plane)
{
	if (this->shown_plane == plane) return false;
	this->shown_plane = plane;
	/* In case widget IDs are repeated, make sure Window::GetWidget works on displayed widgets. */
	if (static_cast<size_t>(this->shown_plane) < this->children.size()) this->children[shown_plane]->FillWidgetLookup(*this->widget_lookup);
	return true;
}

class NWidgetLayer : public NWidgetContainer {
public:
	NWidgetLayer(WidgetID index) : NWidgetContainer(NWID_LAYER, index) {}

	void SetupSmallestSize(Window *w) override;
	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override;

	void Draw(const Window *w) override;
};

void NWidgetLayer::SetupSmallestSize(Window *w)
{
	/* First sweep, recurse down and compute minimal size and filling. */
	this->smallest_x = 0;
	this->smallest_y = 0;
	this->fill_x = this->IsEmpty() ? 0 : 1;
	this->fill_y = this->IsEmpty() ? 0 : 1;
	this->resize_x = this->IsEmpty() ? 0 : 1;
	this->resize_y = this->IsEmpty() ? 0 : 1;
	for (const auto &child_wid : this->children) {
		child_wid->SetupSmallestSize(w);

		this->smallest_x = std::max(this->smallest_x, child_wid->smallest_x + child_wid->padding.Horizontal());
		this->smallest_y = std::max(this->smallest_y, child_wid->smallest_y + child_wid->padding.Vertical());
		this->fill_x = std::lcm(this->fill_x, child_wid->fill_x);
		this->fill_y = std::lcm(this->fill_y, child_wid->fill_y);
		this->resize_x = std::lcm(this->resize_x, child_wid->resize_x);
		this->resize_y = std::lcm(this->resize_y, child_wid->resize_y);
		this->ApplyAspectRatio();
	}
}

void NWidgetLayer::AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl)
{
	assert(given_width >= this->smallest_x && given_height >= this->smallest_y);
	this->StoreSizePosition(sizing, x, y, given_width, given_height);

	for (const auto &child_wid : this->children) {
		uint hor_step = (sizing == ST_SMALLEST) ? 1 : child_wid->GetHorizontalStepSize(sizing);
		uint child_width = ComputeMaxSize(child_wid->smallest_x, given_width - child_wid->padding.Horizontal(), hor_step);
		uint child_pos_x = (rtl ? child_wid->padding.right : child_wid->padding.left);

		uint vert_step = (sizing == ST_SMALLEST) ? 1 : child_wid->GetVerticalStepSize(sizing);
		uint child_height = ComputeMaxSize(child_wid->smallest_y, given_height - child_wid->padding.Vertical(), vert_step);
		uint child_pos_y = child_wid->padding.top;

		child_wid->AssignSizePosition(sizing, x + child_pos_x, y + child_pos_y, child_width, child_height, rtl);
	}
}

void NWidgetLayer::Draw(const Window *w)
{
	/* Draw in reverse order, as layers are arranged top-down. */
	for (auto it = std::rbegin(this->children); it != std::rend(this->children); ++it) {
		(*it)->Draw(w);
	}

	DrawOutline(w, this);
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
void NWidgetPIPContainer::SetPIP(uint8_t pip_pre, uint8_t pip_inter, uint8_t pip_post)
{
	this->uz_pip_pre = pip_pre;
	this->uz_pip_inter = pip_inter;
	this->uz_pip_post = pip_post;

	this->pip_pre = ScaleGUITrad(this->uz_pip_pre);
	this->pip_inter = ScaleGUITrad(this->uz_pip_inter);
	this->pip_post = ScaleGUITrad(this->uz_pip_post);
}

/**
 * Set additional pre/inter/post space for the container.
 *
 * @param pip_ratio_pre   Ratio of additional space in front of the first child widget (above
 *                        for the vertical container, at the left for the horizontal container).
 * @param pip_ratio_inter Ratio of additional space between two child widgets.
 * @param pip_ratio_post  Ratio of additional space after the last child widget (below for the
 *                        vertical container, at the right for the horizontal container).
 */
void NWidgetPIPContainer::SetPIPRatio(uint8_t pip_ratio_pre, uint8_t pip_ratio_inter, uint8_t pip_ratio_post)
{
	this->pip_ratio_pre = pip_ratio_pre;
	this->pip_ratio_inter = pip_ratio_inter;
	this->pip_ratio_post = pip_ratio_post;
}

void NWidgetHorizontal::SetupSmallestSize(Window *w)
{
	this->smallest_x = 0; // Sum of minimal size of all children.
	this->smallest_y = 0; // Biggest child.
	this->fill_x = 0;     // smallest non-zero child widget fill step.
	this->fill_y = 1;     // smallest common child fill step.
	this->resize_x = 0;   // smallest non-zero child widget resize step.
	this->resize_y = 1;   // smallest common child resize step.
	this->gaps = 0;

	/* 1a. Forward call, collect longest/widest child length. */
	uint longest = 0; // Longest child found.
	uint max_vert_fill = 0; // Biggest vertical fill step.
	for (const auto &child_wid : this->children) {
		child_wid->SetupSmallestSize(w);
		longest = std::max(longest, child_wid->smallest_x);
		max_vert_fill = std::max(max_vert_fill, child_wid->GetVerticalStepSize(ST_SMALLEST));
		this->smallest_y = std::max(this->smallest_y, child_wid->smallest_y + child_wid->padding.Vertical());
		if (child_wid->smallest_x != 0 || child_wid->fill_x != 0) this->gaps++;
	}
	if (this->gaps > 0) this->gaps--; // Number of gaps is number of widgets less one.
	/* 1b. Make the container higher if needed to accommodate all children nicely. */
	[[maybe_unused]] uint max_smallest = this->smallest_y + 3 * max_vert_fill; // Upper limit to computing smallest height.
	uint cur_height = this->smallest_y;
	for (;;) {
		for (const auto &child_wid : this->children) {
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
	for (const auto &child_wid : this->children) {
		child_wid->smallest_y = this->smallest_y - child_wid->padding.Vertical();
		child_wid->ApplyAspectRatio();
		longest = std::max(longest, child_wid->smallest_x);
	}
	if (this->flags.Test(NWidContainerFlag::EqualSize)) {
		for (const auto &child_wid : this->children) {
			if (child_wid->fill_x == 1) child_wid->smallest_x = longest;
		}
	}
	/* 3. Compute smallest, fill, and resize values of the container. */
	for (const auto &child_wid : this->children) {
		this->smallest_x += child_wid->smallest_x + child_wid->padding.Horizontal();
		if (child_wid->fill_x > 0) {
			if (this->fill_x == 0 || this->fill_x > child_wid->fill_x) this->fill_x = child_wid->fill_x;
		}
		this->fill_y = std::lcm(this->fill_y, child_wid->fill_y);

		if (child_wid->resize_x > 0) {
			if (this->resize_x == 0 || this->resize_x > child_wid->resize_x) this->resize_x = child_wid->resize_x;
		}
		this->resize_y = std::lcm(this->resize_y, child_wid->resize_y);
	}
	if (this->fill_x == 0 && this->pip_ratio_pre + this->pip_ratio_inter + this->pip_ratio_post > 0) this->fill_x = 1;
	/* 4. Increase by required PIP space. */
	this->smallest_x += this->pip_pre + this->gaps * this->pip_inter + this->pip_post;
}

void NWidgetHorizontal::AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl)
{
	assert(given_width >= this->smallest_x && given_height >= this->smallest_y);

	/* Compute additional width given to us. */
	uint additional_length = given_width - (this->pip_pre + this->gaps * this->pip_inter + this->pip_post);
	for (const auto &child_wid : this->children) {
		if (child_wid->smallest_x != 0 || child_wid->fill_x != 0) additional_length -= child_wid->smallest_x + child_wid->padding.Horizontal();
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
	for (const auto &child_wid : this->children) {
		uint hor_step = child_wid->GetHorizontalStepSize(sizing);
		if (hor_step > 0) {
			if (!flags.Test(NWidContainerFlag::BigFirst)) num_changing_childs++;
			biggest_stepsize = std::max(biggest_stepsize, hor_step);
		} else {
			child_wid->current_x = child_wid->smallest_x;
		}

		uint vert_step = (sizing == ST_SMALLEST) ? 1 : child_wid->GetVerticalStepSize(sizing);
		child_wid->current_y = ComputeMaxSize(child_wid->smallest_y, given_height - child_wid->padding.Vertical(), vert_step);
	}

	/* First.5 loop: count how many children are of the biggest step size. */
	if (flags.Test(NWidContainerFlag::BigFirst) && biggest_stepsize > 0) {
		for (const auto &child_wid : this->children) {
			uint hor_step = child_wid->GetHorizontalStepSize(sizing);
			if (hor_step == biggest_stepsize) {
				num_changing_childs++;
			}
		}
	}

	/* Second loop: Allocate the additional horizontal space over the resizing children, starting with the biggest resize steps. */
	while (biggest_stepsize > 0) {
		uint next_biggest_stepsize = 0;
		for (const auto &child_wid : this->children) {
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

		if (num_changing_childs == 0 && flags.Test(NWidContainerFlag::BigFirst) && biggest_stepsize > 0) {
			/* Second.5 loop: count how many children are of the updated biggest step size. */
			for (const auto &child_wid : this->children) {
				uint hor_step = child_wid->GetHorizontalStepSize(sizing);
				if (hor_step == biggest_stepsize) {
					num_changing_childs++;
				}
			}
		}
	}
	assert(num_changing_childs == 0);

	uint pre = this->pip_pre;
	uint inter = this->pip_inter;

	if (additional_length > 0) {
		/* Allocate remaining space by pip ratios. If this doesn't round exactly, the unused space will fall into pip_post
		 * which is never explicitly needed. */
		int r = this->pip_ratio_pre + this->gaps * this->pip_ratio_inter + this->pip_ratio_post;
		if (r > 0) {
			pre += this->pip_ratio_pre * additional_length / r;
			if (this->gaps > 0) inter += this->pip_ratio_inter * additional_length / r;
		}
	}

	/* Third loop: Compute position and call the child. */
	uint position = rtl ? this->current_x - pre : pre; // Place to put next child relative to origin of the container.
	for (const auto &child_wid : this->children) {
		uint child_width = child_wid->current_x;
		uint child_x = x + (rtl ? position - child_width - child_wid->padding.left : position + child_wid->padding.left);
		uint child_y = y + child_wid->padding.top;

		child_wid->AssignSizePosition(sizing, child_x, child_y, child_width, child_wid->current_y, rtl);
		if (child_wid->current_x != 0) {
			uint padded_child_width = child_width + child_wid->padding.Horizontal() + inter;
			position = rtl ? position - padded_child_width : position + padded_child_width;
		}
	}
}

void NWidgetHorizontalLTR::AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool)
{
	NWidgetHorizontal::AssignSizePosition(sizing, x, y, given_width, given_height, false);
}

void NWidgetVertical::SetupSmallestSize(Window *w)
{
	this->smallest_x = 0; // Biggest child.
	this->smallest_y = 0; // Sum of minimal size of all children.
	this->fill_x = 1;     // smallest common child fill step.
	this->fill_y = 0;     // smallest non-zero child widget fill step.
	this->resize_x = 1;   // smallest common child resize step.
	this->resize_y = 0;   // smallest non-zero child widget resize step.
	this->gaps = 0;

	/* 1a. Forward call, collect longest/widest child length. */
	uint highest = 0; // Highest child found.
	uint max_hor_fill = 0; // Biggest horizontal fill step.
	for (const auto &child_wid : this->children) {
		child_wid->SetupSmallestSize(w);
		highest = std::max(highest, child_wid->smallest_y);
		max_hor_fill = std::max(max_hor_fill, child_wid->GetHorizontalStepSize(ST_SMALLEST));
		this->smallest_x = std::max(this->smallest_x, child_wid->smallest_x + child_wid->padding.Horizontal());
		if (child_wid->smallest_y != 0 || child_wid->fill_y != 0) this->gaps++;
	}
	if (this->gaps > 0) this->gaps--; // Number of gaps is number of widgets less one.
	/* 1b. Make the container wider if needed to accommodate all children nicely. */
	[[maybe_unused]] uint max_smallest = this->smallest_x + 3 * max_hor_fill; // Upper limit to computing smallest height.
	uint cur_width = this->smallest_x;
	for (;;) {
		for (const auto &child_wid : this->children) {
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
	for (const auto &child_wid : this->children) {
		child_wid->smallest_x = this->smallest_x - child_wid->padding.Horizontal();
		child_wid->ApplyAspectRatio();
		highest = std::max(highest, child_wid->smallest_y);
	}
	if (this->flags.Test(NWidContainerFlag::EqualSize)) {
		for (const auto &child_wid : this->children) {
			if (child_wid->fill_y == 1) child_wid->smallest_y = highest;
		}
	}
	/* 3. Compute smallest, fill, and resize values of the container. */
	for (const auto &child_wid : this->children) {
		this->smallest_y += child_wid->smallest_y + child_wid->padding.Vertical();
		if (child_wid->fill_y > 0) {
			if (this->fill_y == 0 || this->fill_y > child_wid->fill_y) this->fill_y = child_wid->fill_y;
		}
		this->fill_x = std::lcm(this->fill_x, child_wid->fill_x);

		if (child_wid->resize_y > 0) {
			if (this->resize_y == 0 || this->resize_y > child_wid->resize_y) this->resize_y = child_wid->resize_y;
		}
		this->resize_x = std::lcm(this->resize_x, child_wid->resize_x);
	}
	if (this->fill_y == 0 && this->pip_ratio_pre + this->pip_ratio_inter + this->pip_ratio_post > 0) this->fill_y = 1;
	/* 4. Increase by required PIP space. */
	this->smallest_y += this->pip_pre + this->gaps * this->pip_inter + this->pip_post;
}

void NWidgetVertical::AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl)
{
	assert(given_width >= this->smallest_x && given_height >= this->smallest_y);

	/* Compute additional height given to us. */
	uint additional_length = given_height - (this->pip_pre + this->gaps * this->pip_inter + this->pip_post);
	for (const auto &child_wid : this->children) {
		if (child_wid->smallest_y != 0 || child_wid->fill_y != 0) additional_length -= child_wid->smallest_y + child_wid->padding.Vertical();
	}

	this->StoreSizePosition(sizing, x, y, given_width, given_height);

	/* Like the horizontal container, the vertical container also distributes additional height evenly, starting with the children with the biggest resize steps.
	 * It also stores computed widths and heights into current_x and current_y values of the child.
	 */

	/* First loop: Find biggest stepsize, find number of children that want a piece of the pie, handle horizontal size for all children, handle vertical size for non-resizing child. */
	int num_changing_childs = 0; // Number of children that can change size.
	uint biggest_stepsize = 0;
	for (const auto &child_wid : this->children) {
		uint vert_step = child_wid->GetVerticalStepSize(sizing);
		if (vert_step > 0) {
			if (!flags.Test(NWidContainerFlag::BigFirst)) num_changing_childs++;
			biggest_stepsize = std::max(biggest_stepsize, vert_step);
		} else {
			child_wid->current_y = child_wid->smallest_y;
		}

		uint hor_step = (sizing == ST_SMALLEST) ? 1 : child_wid->GetHorizontalStepSize(sizing);
		child_wid->current_x = ComputeMaxSize(child_wid->smallest_x, given_width - child_wid->padding.Horizontal(), hor_step);
	}

	/* First.5 loop: count how many children are of the biggest step size. */
	if (this->flags.Test(NWidContainerFlag::BigFirst) && biggest_stepsize > 0) {
		for (const auto &child_wid : this->children) {
			uint vert_step = child_wid->GetVerticalStepSize(sizing);
			if (vert_step == biggest_stepsize) {
				num_changing_childs++;
			}
		}
	}

	/* Second loop: Allocate the additional vertical space over the resizing children, starting with the biggest resize steps. */
	while (biggest_stepsize > 0) {
		uint next_biggest_stepsize = 0;
		for (const auto &child_wid : this->children) {
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

		if (num_changing_childs == 0 && flags.Test(NWidContainerFlag::BigFirst) && biggest_stepsize > 0) {
			/* Second.5 loop: count how many children are of the updated biggest step size. */
			for (const auto &child_wid : this->children) {
				uint vert_step = child_wid->GetVerticalStepSize(sizing);
				if (vert_step == biggest_stepsize) {
					num_changing_childs++;
				}
			}
		}
	}
	assert(num_changing_childs == 0);

	uint pre = this->pip_pre;
	uint inter = this->pip_inter;

	if (additional_length > 0) {
		/* Allocate remaining space by pip ratios. If this doesn't round exactly, the unused space will fall into pip_post
		 * which is never explicitly needed. */
		int r = this->pip_ratio_pre + this->gaps * this->pip_ratio_inter + this->pip_ratio_post;
		if (r > 0) {
			pre += this->pip_ratio_pre * additional_length / r;
			if (this->gaps > 0) inter += this->pip_ratio_inter * additional_length / r;
		}
	}

	/* Third loop: Compute position and call the child. */
	uint position = pre; // Place to put next child relative to origin of the container.
	for (const auto &child_wid : this->children) {
		uint child_x = x + (rtl ? child_wid->padding.right : child_wid->padding.left);
		uint child_height = child_wid->current_y;

		child_wid->AssignSizePosition(sizing, child_x, y + position + child_wid->padding.top, child_wid->current_x, child_height, rtl);
		if (child_wid->current_y != 0) {
			position += child_height + child_wid->padding.Vertical() + inter;
		}
	}
}

/**
 * Generic spacer widget.
 * @param width  Horizontal size of the spacer widget.
 * @param height Vertical size of the spacer widget.
 */
NWidgetSpacer::NWidgetSpacer(int width, int height) : NWidgetResizeBase(NWID_SPACER, INVALID_WIDGET, 0, 0)
{
	this->SetMinimalSize(width, height);
	this->SetResize(0, 0);
}

void NWidgetSpacer::SetupSmallestSize(Window *)
{
	this->smallest_x = this->min_x;
	this->smallest_y = this->min_y;
	this->ApplyAspectRatio();
}

void NWidgetSpacer::Draw(const Window *w)
{
	/* Spacer widget is never normally visible. */

	if (_draw_widget_outlines && this->current_x != 0 && this->current_y != 0) {
		/* Spacers indicate a potential design issue, so get extra highlighting. */
		GfxFillRect(this->GetCurrentRect(), PC_WHITE, FILLRECT_CHECKER);

		DrawOutline(w, this);
	}
}

void NWidgetSpacer::SetDirty(const Window *) const
{
	/* Spacer widget never need repainting. */
}

NWidgetCore *NWidgetSpacer::GetWidgetFromPos(int, int)
{
	return nullptr;
}

/**
 * Sets the clicked element in the matrix.
 * @param clicked The clicked element.
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
	count *= (this->sb->IsVertical() ? this->children.front()->smallest_y : this->children.front()->smallest_x) + this->pip_inter;
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

/**
 * Get current element.
 * @returns index of current element.
 */
int NWidgetMatrix::GetCurrentElement() const
{
	return this->current_element;
}

void NWidgetMatrix::SetupSmallestSize(Window *w)
{
	assert(this->children.size() == 1);

	this->children.front()->SetupSmallestSize(w);

	Dimension padding = { (uint)this->pip_pre + this->pip_post, (uint)this->pip_pre + this->pip_post};
	Dimension size    = {this->children.front()->smallest_x + padding.width, this->children.front()->smallest_y + padding.height};
	Dimension fill    = {0, 0};
	Dimension resize  = {this->pip_inter + this->children.front()->smallest_x, this->pip_inter + this->children.front()->smallest_y};

	if (this->index >= 0) w->UpdateWidgetSize(this->index, size, padding, fill, resize);

	this->smallest_x = size.width;
	this->smallest_y = size.height;
	this->fill_x = fill.width;
	this->fill_y = fill.height;
	this->resize_x = resize.width;
	this->resize_y = resize.height;
	this->ApplyAspectRatio();
}

void NWidgetMatrix::AssignSizePosition(SizingType, int x, int y, uint given_width, uint given_height, bool)
{
	assert(given_width >= this->smallest_x && given_height >= this->smallest_y);

	this->pos_x = x;
	this->pos_y = y;
	this->current_x = given_width;
	this->current_y = given_height;

	/* Determine the size of the widgets, and the number of visible widgets on each of the axis. */
	this->widget_w = this->children.front()->smallest_x + this->pip_inter;
	this->widget_h = this->children.front()->smallest_y + this->pip_inter;

	/* Account for the pip_inter is between widgets, so we need to account for that when
	 * the division assumes pip_inter is used for all widgets. */
	this->widgets_x = CeilDiv(this->current_x - this->pip_pre - this->pip_post + this->pip_inter, this->widget_w);
	this->widgets_y = CeilDiv(this->current_y - this->pip_pre - this->pip_post + this->pip_inter, this->widget_h);

	/* When resizing, update the scrollbar's count. E.g. with a vertical
	 * scrollbar becoming wider or narrower means the amount of rows in
	 * the scrollbar becomes respectively smaller or higher. */
	this->SetCount(this->count);
}

NWidgetCore *NWidgetMatrix::GetWidgetFromPos(int x, int y)
{
	/* Falls outside of the matrix widget. */
	if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return nullptr;

	int start_x, start_y, base_offs_x, base_offs_y;
	this->GetScrollOffsets(start_x, start_y, base_offs_x, base_offs_y);

	bool rtl = _current_text_dir == TD_RTL;

	int widget_col = (rtl ?
				-x + (int)this->pip_post + this->pos_x + base_offs_x + this->widget_w - 1 - (int)this->pip_inter :
				 x - (int)this->pip_pre  - this->pos_x - base_offs_x
			) / this->widget_w;

	int widget_row = (y - base_offs_y - (int)this->pip_pre - this->pos_y) / this->widget_h;

	this->current_element = (widget_row + start_y) * this->widgets_x + start_x + widget_col;
	if (this->current_element >= this->count) return nullptr;

	NWidgetCore *child = dynamic_cast<NWidgetCore *>(this->children.front().get());
	assert(child != nullptr);
	child->AssignSizePosition(ST_RESIZE,
			this->pos_x + (rtl ? this->pip_post - widget_col * this->widget_w : this->pip_pre + widget_col * this->widget_w) + base_offs_x,
			this->pos_y + this->pip_pre + widget_row * this->widget_h + base_offs_y,
			child->smallest_x, child->smallest_y, rtl);

	return child->GetWidgetFromPos(x, y);
}

/* virtual */ void NWidgetMatrix::Draw(const Window *w)
{
	/* Fill the background. */
	GfxFillRect(this->GetCurrentRect(), GetColourGradient(this->colour, SHADE_LIGHT));

	/* Set up a clipping area for the previews. */
	bool rtl = _current_text_dir == TD_RTL;
	DrawPixelInfo tmp_dpi;
	if (!FillDrawPixelInfo(&tmp_dpi, this->pos_x + (rtl ? this->pip_post : this->pip_pre), this->pos_y + this->pip_pre, this->current_x - this->pip_pre - this->pip_post, this->current_y - this->pip_pre - this->pip_post)) return;

	{
		AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);

		/* Get the appropriate offsets so we can draw the right widgets. */
		NWidgetCore *child = dynamic_cast<NWidgetCore *>(this->children.front().get());
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
				this->current_element = y * this->widgets_x + x;
				if (this->current_element >= this->count) break;

				child->AssignSizePosition(ST_RESIZE, offs_x, offs_y, child->smallest_x, child->smallest_y, rtl);
				child->SetLowered(this->clicked == this->current_element);
				child->Draw(w);
			}
		}
	}

	DrawOutline(w, this);
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
 * @param index  Index of the widget.
 * @param child  Child container widget (if supplied). If not supplied, a
 *               vertical container will be inserted while adding the first
 *               child widget.
 */
NWidgetBackground::NWidgetBackground(WidgetType tp, Colours colour, WidgetID index, std::unique_ptr<NWidgetPIPContainer> &&child) : NWidgetCore(tp, colour, index, 1, 1, {}, STR_NULL)
{
	assert(tp == WWT_PANEL || tp == WWT_INSET || tp == WWT_FRAME);
	this->child = std::move(child);
	if (this->child != nullptr) this->child->parent = this;
	this->SetAlignment(SA_TOP | SA_LEFT);
}

/**
 * Add a child to the parent.
 * @param nwid Nested widget to add to the background widget.
 *
 * Unless a child container has been given in the constructor, a parent behaves as a vertical container.
 * You can add several children to it, and they are put underneath each other.
 */
void NWidgetBackground::Add(std::unique_ptr<NWidgetBase> &&nwid)
{
	if (this->child == nullptr) {
		this->child = std::make_unique<NWidgetVertical>();
	}
	nwid->parent = this->child.get();
	this->child->Add(std::move(nwid));
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
void NWidgetBackground::SetPIP(uint8_t pip_pre, uint8_t pip_inter, uint8_t pip_post)
{
	if (this->child == nullptr) {
		this->child = std::make_unique<NWidgetVertical>();
	}
	this->child->parent = this;
	this->child->SetPIP(pip_pre, pip_inter, pip_post);
}

/**
 * Set additional pre/inter/post space ratios for the background widget.
 *
 * @param pip_ratio_pre   Ratio of additional space in front of the first child widget (above
 *                        for the vertical container, at the left for the horizontal container).
 * @param pip_ratio_inter Ratio of additional space between two child widgets.
 * @param pip_ratio_post  Ratio of additional space after the last child widget (below for the
 *                        vertical container, at the right for the horizontal container).
 * @note Using this function implies that the widget has (or will have) child widgets.
 */
void NWidgetBackground::SetPIPRatio(uint8_t pip_ratio_pre, uint8_t pip_ratio_inter, uint8_t pip_ratio_post)
{
	if (this->child == nullptr) {
		this->child = std::make_unique<NWidgetVertical>();
	}
	this->child->parent = this;
	this->child->SetPIPRatio(pip_ratio_pre, pip_ratio_inter, pip_ratio_post);
}

void NWidgetBackground::AdjustPaddingForZoom()
{
	if (child != nullptr) child->AdjustPaddingForZoom();
	NWidgetCore::AdjustPaddingForZoom();
}

void NWidgetBackground::SetupSmallestSize(Window *w)
{
	if (this->child != nullptr) {
		this->child->SetupSmallestSize(w);

		this->smallest_x = this->child->smallest_x;
		this->smallest_y = this->child->smallest_y;
		this->fill_x = this->child->fill_x;
		this->fill_y = this->child->fill_y;
		this->resize_x = this->child->resize_x;
		this->resize_y = this->child->resize_y;

		/* Don't apply automatic padding if there is no child widget. */
		if (w == nullptr) return;

		if (this->type == WWT_FRAME) {
			std::string text = GetStringForWidget(w, this);
			Dimension text_size = text.empty() ? Dimension{0, 0} : GetStringBoundingBox(text, this->text_size);

			/* Account for the size of the frame's text if that exists */
			this->child->padding     = WidgetDimensions::scaled.frametext;
			this->child->padding.top = std::max<uint8_t>(WidgetDimensions::scaled.frametext.top, text_size.height != 0 ? text_size.height + WidgetDimensions::scaled.frametext.top / 2 : 0);

			this->smallest_x += this->child->padding.Horizontal();
			this->smallest_y += this->child->padding.Vertical();

			this->smallest_x = std::max(this->smallest_x, text_size.width + WidgetDimensions::scaled.frametext.Horizontal());
		} else if (this->type == WWT_INSET) {
			/* Apply automatic padding for bevel thickness. */
			this->child->padding = WidgetDimensions::scaled.bevel;

			this->smallest_x += this->child->padding.Horizontal();
			this->smallest_y += this->child->padding.Vertical();
		}
		this->ApplyAspectRatio();
	} else {
		Dimension d = {this->min_x, this->min_y};
		Dimension fill = {this->fill_x, this->fill_y};
		Dimension resize  = {this->resize_x, this->resize_y};
		if (w != nullptr) { // A non-nullptr window pointer acts as switch to turn dynamic widget size on.
			if (this->type == WWT_FRAME || this->type == WWT_INSET) {
				std::string text = GetStringForWidget(w, this);
				if (!text.empty()) {
					Dimension background = GetStringBoundingBox(text, this->text_size);
					background.width += (this->type == WWT_FRAME) ? (WidgetDimensions::scaled.frametext.Horizontal()) : (WidgetDimensions::scaled.inset.Horizontal());
					d = maxdim(d, background);
				}
			}
			if (this->index >= 0) {
				Dimension padding;
				switch (this->type) {
					default: NOT_REACHED();
					case WWT_PANEL: padding = {WidgetDimensions::scaled.framerect.Horizontal(), WidgetDimensions::scaled.framerect.Vertical()}; break;
					case WWT_FRAME: padding = {WidgetDimensions::scaled.frametext.Horizontal(), WidgetDimensions::scaled.frametext.Vertical()}; break;
					case WWT_INSET: padding = {WidgetDimensions::scaled.inset.Horizontal(),     WidgetDimensions::scaled.inset.Vertical()};     break;
				}
				w->UpdateWidgetSize(this->index, d, padding, fill, resize);
			}
		}
		this->smallest_x = d.width;
		this->smallest_y = d.height;
		this->fill_x = fill.width;
		this->fill_y = fill.height;
		this->resize_x = resize.width;
		this->resize_y = resize.height;
		this->ApplyAspectRatio();
	}
}

void NWidgetBackground::AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl)
{
	this->StoreSizePosition(sizing, x, y, given_width, given_height);

	if (this->child != nullptr) {
		uint x_offset = (rtl ? this->child->padding.right : this->child->padding.left);
		uint width = given_width - this->child->padding.Horizontal();
		uint height = given_height - this->child->padding.Vertical();
		this->child->AssignSizePosition(sizing, x + x_offset, y + this->child->padding.top, width, height, rtl);
	}
}

void NWidgetBackground::FillWidgetLookup(WidgetLookup &widget_lookup)
{
	this->NWidgetCore::FillWidgetLookup(widget_lookup);
	if (this->child != nullptr) this->child->FillWidgetLookup(widget_lookup);
}

void NWidgetBackground::Draw(const Window *w)
{
	if (this->current_x == 0 || this->current_y == 0) return;

	Rect r = this->GetCurrentRect();

	const DrawPixelInfo *dpi = _cur_dpi;
	if (dpi->left > r.right || dpi->left + dpi->width <= r.left || dpi->top > r.bottom || dpi->top + dpi->height <= r.top) return;

	switch (this->type) {
		case WWT_PANEL:
			DrawFrameRect(r, this->colour, this->IsLowered() ? FrameFlag::Lowered : FrameFlags{});
			break;

		case WWT_FRAME:
			DrawFrame(r, this->colour, this->text_colour, GetStringForWidget(w, this), this->align, this->text_size);
			break;

		case WWT_INSET:
			DrawInset(r, this->colour, this->text_colour, GetStringForWidget(w, this), this->align, this->text_size);
			break;

		default:
			NOT_REACHED();
	}

	if (this->index >= 0) w->DrawWidget(r, this->index);
	if (this->child != nullptr) this->child->Draw(w);

	if (this->IsDisabled()) {
		GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), GetColourGradient(this->colour, SHADE_DARKER), FILLRECT_CHECKER);
	}

	DrawOutline(w, this);
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

NWidgetViewport::NWidgetViewport(WidgetID index) : NWidgetCore(NWID_VIEWPORT, INVALID_COLOUR, index, 1, 1, {}, STR_NULL)
{
}

void NWidgetViewport::SetupSmallestSize(Window *)
{
	this->smallest_x = this->min_x;
	this->smallest_y = this->min_y;
	this->ApplyAspectRatio();
}

void NWidgetViewport::Draw(const Window *w)
{
	if (this->current_x == 0 || this->current_y == 0) return;

	if (this->disp_flags.Test(NWidgetDisplayFlag::NoTransparency)) {
		TransparencyOptionBits to_backup = _transparency_opt;
		_transparency_opt &= (1 << TO_SIGNS) | (1 << TO_TEXT); // Disable all transparency, except textual stuff
		w->DrawViewport();
		_transparency_opt = to_backup;
	} else {
		w->DrawViewport();
	}

	/* Optionally shade the viewport. */
	if (this->disp_flags.Any({NWidgetDisplayFlag::ShadeGrey, NWidgetDisplayFlag::ShadeDimmed})) {
		GfxFillRect(this->GetCurrentRect(), this->disp_flags.Test(NWidgetDisplayFlag::ShadeDimmed) ? PALETTE_TO_TRANSPARENT : PALETTE_NEWSPAPER, FILLRECT_RECOLOUR);
	}

	DrawOutline(w, this);
}

/**
 * Initialize the viewport of the window.
 * @param w            Window owning the viewport.
 * @param focus        Either the tile index or vehicle ID to focus.
 * @param zoom         Zoom level.
 */
void NWidgetViewport::InitializeViewport(Window *w, std::variant<TileIndex, VehicleID> focus, ZoomLevel zoom)
{
	InitializeWindowViewport(w, this->pos_x, this->pos_y, this->current_x, this->current_y, focus, zoom);
}

/**
 * Update the position and size of the viewport (after eg a resize).
 * @param w Window owning the viewport.
 */
void NWidgetViewport::UpdateViewportCoordinates(Window *w)
{
	if (w->viewport == nullptr) return;

	Viewport &vp = *w->viewport;
	vp.left = w->left + this->pos_x;
	vp.top  = w->top + this->pos_y;
	vp.width  = this->current_x;
	vp.height = this->current_y;

	vp.virtual_width  = ScaleByZoom(vp.width, vp.zoom);
	vp.virtual_height = ScaleByZoom(vp.height, vp.zoom);
}

/**
 * Compute the row of a scrolled widget that a user clicked in.
 * @param clickpos    Vertical position of the mouse click (without taking scrolling into account).
 * @param w           The window the click was in.
 * @param widget      Widget number of the widget clicked in.
 * @param padding     Amount of empty space between the widget edge and the top of the first row. Default value is \c 0.
 * @param line_height Height of a single row. A negative value means using the vertical resize step of the widget.
 * @return Row number clicked at. If clicked at a wrong position, #Scrollbar::npos is returned.
 */
Scrollbar::size_type Scrollbar::GetScrolledRowFromWidget(int clickpos, const Window * const w, WidgetID widget, int padding, int line_height) const
{
	int pos = w->GetRowFromWidget(clickpos, widget, padding, line_height);
	if (pos != INT_MAX) pos += this->GetPosition();
	return (pos < 0 || pos >= this->GetCount()) ? Scrollbar::npos : pos;
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
EventState Scrollbar::UpdateListPositionOnKeyPress(int &list_position, uint16_t keycode) const
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
void Scrollbar::SetCapacityFromWidget(Window *w, WidgetID widget, int padding)
{
	NWidgetBase *nwid = w->GetWidget<NWidgetBase>(widget);
	if (this->IsVertical()) {
		this->SetCapacity(((int)nwid->current_y - padding) / (int)nwid->resize_y);
	} else {
		this->SetCapacity(((int)nwid->current_x - padding) / (int)nwid->resize_x);
	}
}

/**
 * Apply 'scroll' to a rect to be drawn in.
 * @param r Rect to be 'scrolled'.
 * @param sb The scrollbar affecting the scroll.
 * @param resize_step Resize step of the widget/scrollbar (1 if the scrollbar is pixel-based.)
 * @returns Scrolled rect.
 */
Rect ScrollRect(Rect r, const Scrollbar &sb, int resize_step)
{
	const int count = sb.GetCount() * resize_step;
	const int position = sb.GetPosition() * resize_step;

	if (sb.IsVertical()) {
		r.top -= position;
		r.bottom = r.top + count;
	} else {
		bool rtl = _current_text_dir == TD_RTL;
		if (rtl) {
			r.right += position;
			r.left = r.right - count;
		} else {
			r.left -= position;
			r.right = r.left + count;
		}
	}

	return r;
}

/**
 * Scrollbar widget.
 * @param tp     Scrollbar type. (horizontal/vertical)
 * @param colour Colour of the scrollbar.
 * @param index  Index of the widget.
 */
NWidgetScrollbar::NWidgetScrollbar(WidgetType tp, Colours colour, WidgetID index) : NWidgetCore(tp, colour, index, 1, 1, {}, STR_NULL), Scrollbar(tp != NWID_HSCROLLBAR)
{
	assert(tp == NWID_HSCROLLBAR || tp == NWID_VSCROLLBAR);

	switch (this->type) {
		case NWID_HSCROLLBAR:
			this->SetResize(1, 0);
			this->SetFill(1, 0);
			this->SetToolTip(STR_TOOLTIP_HSCROLL_BAR_SCROLLS_LIST);
			break;

		case NWID_VSCROLLBAR:
			this->SetResize(0, 1);
			this->SetFill(0, 1);
			this->SetToolTip(STR_TOOLTIP_VSCROLL_BAR_SCROLLS_LIST);
			break;

		default: NOT_REACHED();
	}
}

void NWidgetScrollbar::SetupSmallestSize(Window *)
{
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

	bool up_lowered = this->disp_flags.Test(NWidgetDisplayFlag::ScrollbarUp);
	bool down_lowered = this->disp_flags.Test(NWidgetDisplayFlag::ScrollbarDown);
	bool middle_lowered = !this->disp_flags.Any({NWidgetDisplayFlag::ScrollbarUp, NWidgetDisplayFlag::ScrollbarDown}) && w->mouse_capture_widget == this->index;

	if (this->type == NWID_HSCROLLBAR) {
		DrawHorizontalScrollbar(r, this->colour, up_lowered, middle_lowered, down_lowered, this);
	} else {
		DrawVerticalScrollbar(r, this->colour, up_lowered, middle_lowered, down_lowered, this);
	}

	if (this->IsDisabled()) {
		GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), GetColourGradient(this->colour, SHADE_DARKER), FILLRECT_CHECKER);
	}

	DrawOutline(w, this);
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
 * @param index  Index of the widget.
 * @param data   Data of the widget.
 * @param tip    Tooltip of the widget.
 */
NWidgetLeaf::NWidgetLeaf(WidgetType tp, Colours colour, WidgetID index, const WidgetData &data, StringID tip) : NWidgetCore(tp, colour, index, 1, 1, data, tip)
{
	assert(index >= 0 || tp == WWT_LABEL || tp == WWT_TEXT || tp == WWT_CAPTION || tp == WWT_RESIZEBOX || tp == WWT_SHADEBOX || tp == WWT_DEFSIZEBOX || tp == WWT_DEBUGBOX || tp == WWT_STICKYBOX || tp == WWT_CLOSEBOX);
	this->min_x = 0;
	this->min_y = 0;
	this->SetResize(0, 0);

	switch (tp) {
		case WWT_EMPTY:
			if (colour != INVALID_COLOUR) [[unlikely]] throw std::runtime_error("WWT_EMPTY should not have a colour");
			break;

		case WWT_TEXT:
			if (colour != INVALID_COLOUR) [[unlikely]] throw std::runtime_error("WWT_TEXT should not have a colour");
			this->SetFill(0, 0);
			this->SetAlignment(SA_LEFT | SA_VERT_CENTER);
			break;

		case WWT_LABEL:
			if (colour != INVALID_COLOUR) [[unlikely]] throw std::runtime_error("WWT_LABEL should not have a colour");
			[[fallthrough]];

		case WWT_PUSHBTN:
		case WWT_IMGBTN:
		case WWT_PUSHIMGBTN:
		case WWT_IMGBTN_2:
		case WWT_TEXTBTN:
		case WWT_PUSHTXTBTN:
		case WWT_TEXTBTN_2:
		case WWT_IMGTEXTBTN:
		case WWT_PUSHIMGTEXTBTN:
		case WWT_BOOLBTN:
		case WWT_MATRIX:
		case NWID_BUTTON_DROPDOWN:
		case NWID_PUSHBUTTON_DROPDOWN:
			this->SetFill(0, 0);
			break;

		case WWT_ARROWBTN:
		case WWT_PUSHARROWBTN:
			this->SetFill(0, 0);
			this->SetAspect(WidgetDimensions::ASPECT_LEFT_RIGHT_BUTTON);
			break;

		case WWT_EDITBOX:
			this->SetFill(0, 0);
			break;

		case WWT_CAPTION:
			this->SetFill(1, 0);
			this->SetResize(1, 0);
			this->SetMinimalSize(0, WidgetDimensions::WD_CAPTION_HEIGHT);
			this->SetMinimalTextLines(1, WidgetDimensions::unscaled.captiontext.Vertical(), FS_NORMAL);
			this->SetToolTip(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS);
			break;

		case WWT_STICKYBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WidgetDimensions::WD_STICKYBOX_WIDTH, WidgetDimensions::WD_CAPTION_HEIGHT);
			this->SetToolTip(STR_TOOLTIP_STICKY);
			this->SetAspect(this->min_x, this->min_y);
			break;

		case WWT_SHADEBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WidgetDimensions::WD_SHADEBOX_WIDTH, WidgetDimensions::WD_CAPTION_HEIGHT);
			this->SetToolTip(STR_TOOLTIP_SHADE);
			this->SetAspect(this->min_x, this->min_y);
			break;

		case WWT_DEBUGBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WidgetDimensions::WD_DEBUGBOX_WIDTH, WidgetDimensions::WD_CAPTION_HEIGHT);
			this->SetToolTip(STR_TOOLTIP_DEBUG);
			this->SetAspect(this->min_x, this->min_y);
			break;

		case WWT_DEFSIZEBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WidgetDimensions::WD_DEFSIZEBOX_WIDTH, WidgetDimensions::WD_CAPTION_HEIGHT);
			this->SetToolTip(STR_TOOLTIP_DEFSIZE);
			this->SetAspect(this->min_x, this->min_y);
			break;

		case WWT_RESIZEBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WidgetDimensions::WD_RESIZEBOX_WIDTH, 12);
			this->SetResizeWidgetType(RWV_SHOW_BEVEL);
			this->SetToolTip(STR_TOOLTIP_RESIZE);
			break;

		case WWT_CLOSEBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WidgetDimensions::WD_CLOSEBOX_WIDTH, WidgetDimensions::WD_CAPTION_HEIGHT);
			this->SetToolTip(STR_TOOLTIP_CLOSE_WINDOW);
			this->SetAspect(this->min_x, this->min_y);
			break;

		case WWT_DROPDOWN:
			this->SetFill(0, 0);
			this->SetMinimalSize(0, WidgetDimensions::WD_DROPDOWN_HEIGHT);
			this->SetAlignment(SA_TOP | SA_LEFT);
			break;

		default:
			NOT_REACHED();
	}
}

void NWidgetLeaf::SetupSmallestSize(Window *w)
{
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
		[[fallthrough]];
		case WWT_PUSHBTN: {
			padding = {WidgetDimensions::scaled.frametext.Horizontal(), WidgetDimensions::scaled.framerect.Vertical()};
			break;
		}

		case WWT_BOOLBTN:
			size.width = SETTING_BUTTON_WIDTH;
			size.height = SETTING_BUTTON_HEIGHT;
			break;

		case WWT_IMGBTN:
		case WWT_IMGBTN_2:
		case WWT_PUSHIMGBTN: {
			padding = {WidgetDimensions::scaled.imgbtn.Horizontal(), WidgetDimensions::scaled.imgbtn.Vertical()};
			Dimension d2 = GetScaledSpriteSize(this->widget_data.sprite);
			if (this->type == WWT_IMGBTN_2) d2 = maxdim(d2, GetScaledSpriteSize(this->widget_data.sprite + 1));
			d2.width += padding.width;
			d2.height += padding.height;
			size = maxdim(size, d2);
			break;
		}

		case WWT_IMGTEXTBTN:
		case WWT_PUSHIMGTEXTBTN: {
			padding = {WidgetDimensions::scaled.framerect.Horizontal(), WidgetDimensions::scaled.framerect.Vertical()};
			Dimension di = GetScaledSpriteSize(this->widget_data.sprite);
			Dimension dt = GetStringBoundingBox(GetStringForWidget(w, this), this->text_size);
			Dimension d2{
				padding.width + di.width + WidgetDimensions::scaled.hsep_wide + dt.width,
				padding.height + std::max(di.height, dt.height)
			};
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
			Dimension d2 = GetStringBoundingBox(GetStringForWidget(w, this), this->text_size);
			d2.width += padding.width;
			d2.height += padding.height;
			size = maxdim(size, d2);
			break;
		}
		case WWT_LABEL:
		case WWT_TEXT: {
			size = maxdim(size, GetStringBoundingBox(GetStringForWidget(w, this), this->text_size));
			break;
		}
		case WWT_CAPTION: {
			padding = {WidgetDimensions::scaled.captiontext.Horizontal(), WidgetDimensions::scaled.captiontext.Vertical()};
			Dimension d2 = GetStringBoundingBox(GetStringForWidget(w, this), this->text_size);
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
			padding = {WidgetDimensions::scaled.dropdowntext.Horizontal() + NWidgetLeaf::dropdown_dimension.width + WidgetDimensions::scaled.fullbevel.Horizontal(), WidgetDimensions::scaled.dropdowntext.Vertical()};
			Dimension d2 = GetStringBoundingBox(GetStringForWidget(w, this), this->text_size);
			d2.width += padding.width;
			d2.height = std::max(d2.height + padding.height, NWidgetLeaf::dropdown_dimension.height);
			size = maxdim(size, d2);
			break;
		}
		default:
			NOT_REACHED();
	}

	if (this->index >= 0) w->UpdateWidgetSize(this->index, size, padding, fill, resize);

	this->smallest_x = size.width;
	this->smallest_y = size.height;
	this->fill_x = fill.width;
	this->fill_y = fill.height;
	this->resize_x = resize.width;
	this->resize_y = resize.height;
	this->ApplyAspectRatio();
}

void NWidgetLeaf::Draw(const Window *w)
{
	if (this->current_x == 0 || this->current_y == 0) return;

	/* Setup a clipping rectangle... for WWT_EMPTY or WWT_TEXT, an extra scaled pixel is allowed in case text shadow encroaches. */
	int extra = (this->type == WWT_EMPTY || this->type == WWT_TEXT) ? ScaleGUITrad(1) : 0;
	DrawPixelInfo new_dpi;
	if (!FillDrawPixelInfo(&new_dpi, this->pos_x, this->pos_y, this->current_x + extra, this->current_y + extra)) return;
	/* ...but keep coordinates relative to the window. */
	new_dpi.left += this->pos_x;
	new_dpi.top += this->pos_y;

	AutoRestoreBackup dpi_backup(_cur_dpi, &new_dpi);

	Rect r = this->GetCurrentRect();

	bool clicked = this->IsLowered();
	switch (this->type) {
		case WWT_EMPTY:
			/* WWT_EMPTY used as a spacer indicates a potential design issue. */
			if (this->index == -1 && _draw_widget_outlines) {
				GfxFillRect(r, PC_BLACK, FILLRECT_CHECKER);
			}
			break;

		case WWT_PUSHBTN:
			DrawFrameRect(r, this->colour, clicked ? FrameFlag::Lowered : FrameFlags{});
			break;

		case WWT_BOOLBTN: {
			Point pt = GetAlignedPosition(r, Dimension(SETTING_BUTTON_WIDTH, SETTING_BUTTON_HEIGHT), this->align);
			Colours button_colour = this->widget_data.alternate_colour;
			if (button_colour == INVALID_COLOUR) button_colour = this->colour;
			DrawBoolButton(pt.x, pt.y, button_colour, this->colour, clicked, !this->IsDisabled());
			break;
		}

		case WWT_IMGBTN:
		case WWT_PUSHIMGBTN:
		case WWT_IMGBTN_2:
			DrawImageButtons(r, this->type, this->colour, clicked, this->widget_data.sprite, this->align);
			break;

		case WWT_TEXTBTN:
		case WWT_PUSHTXTBTN:
		case WWT_TEXTBTN_2:
			DrawFrameRect(r, this->colour, clicked ? FrameFlag::Lowered : FrameFlags{});
			DrawLabel(r, this->text_colour, GetStringForWidget(w, this, (type & WWT_MASK) == WWT_TEXTBTN_2 && clicked), this->align, this->text_size);
			break;

		case WWT_IMGTEXTBTN:
		case WWT_PUSHIMGTEXTBTN:
			DrawImageTextButtons(r, this->colour, clicked, this->widget_data.sprite, this->text_colour, GetStringForWidget(w, this), this->align, this->text_size);
			break;

		case WWT_ARROWBTN:
		case WWT_PUSHARROWBTN: {
			SpriteID sprite;
			switch (this->widget_data.arrow_widget_type) {
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
			DrawLabel(r, this->text_colour, GetStringForWidget(w, this), this->align, this->text_size);
			break;

		case WWT_TEXT:
			DrawText(r, this->text_colour, GetStringForWidget(w, this), this->align, this->text_size);
			break;

		case WWT_MATRIX:
			DrawMatrix(r, this->colour, clicked, this->widget_data.matrix.width, this->widget_data.matrix.height, this->resize_x, this->resize_y);
			break;

		case WWT_EDITBOX: {
			const QueryString *query = w->GetQueryString(this->index);
			if (query != nullptr) query->DrawEditBox(w, this->index);
			break;
		}

		case WWT_CAPTION:
			DrawCaption(r, this->colour, w->owner, this->text_colour, GetStringForWidget(w, this), this->align, this->text_size);
			break;

		case WWT_SHADEBOX:
			DrawShadeBox(r, this->colour, w->IsShaded());
			break;

		case WWT_DEBUGBOX:
			DrawDebugBox(r, this->colour, clicked);
			break;

		case WWT_STICKYBOX:
			DrawStickyBox(r, this->colour, w->flags.Test(WindowFlag::Sticky));
			break;

		case WWT_DEFSIZEBOX:
			DrawDefSizeBox(r, this->colour, clicked);
			break;

		case WWT_RESIZEBOX:
			DrawResizeBox(r, this->colour, this->pos_x < (w->width / 2), w->flags.Test(WindowFlag::SizingLeft) || w->flags.Test(WindowFlag::SizingRight), this->widget_data.resize_widget_type == RWV_SHOW_BEVEL);
			break;

		case WWT_CLOSEBOX:
			DrawCloseBox(r, this->colour);
			break;

		case WWT_DROPDOWN:
			DrawButtonDropdown(r, this->colour, false, clicked, GetStringForWidget(w, this), this->align);
			break;

		case NWID_BUTTON_DROPDOWN:
		case NWID_PUSHBUTTON_DROPDOWN:
			DrawButtonDropdown(r, this->colour, clicked, this->disp_flags.Test(NWidgetDisplayFlag::DropdownActive), GetStringForWidget(w, this), this->align);
			break;

		default:
			NOT_REACHED();
	}
	if (this->index >= 0) w->DrawWidget(r, this->index);

	if (this->IsDisabled() && this->type != WWT_BOOLBTN) {
		/* WWT_BOOLBTN is excluded as it draws its own disabled state. */
		GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), GetColourGradient(this->colour, SHADE_DARKER), FILLRECT_CHECKER);
	}

	DrawOutline(w, this);
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
 * Test if (an NWidgetPart) WidgetType is an attribute widget part type.
 * @param tp WidgetType to test.
 * @return True iff WidgetType is an attribute widget.
 */
static bool IsAttributeWidgetPartType(WidgetType tp)
{
	return tp > WPT_ATTRIBUTE_BEGIN && tp < WPT_ATTRIBUTE_END;
}

/**
 * Apply an attribute NWidgetPart to an NWidget.
 * @param nwid Attribute NWidgetPart
 * @param dest NWidget to apply attribute to.
 * @pre NWidgetPart must be an attribute NWidgetPart.
 */
void ApplyNWidgetPartAttribute(const NWidgetPart &nwid, NWidgetBase *dest)
{
	switch (nwid.type) {
		case WPT_RESIZE: {
			NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(dest);
			if (nwrb == nullptr) [[unlikely]] throw std::runtime_error("WPT_RESIZE requires NWidgetResizeBase");
			assert(nwid.u.xy.x >= 0 && nwid.u.xy.y >= 0);
			nwrb->SetResize(nwid.u.xy.x, nwid.u.xy.y);
			break;
		}

		case WPT_MINSIZE: {
			NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(dest);
			if (nwrb == nullptr) [[unlikely]] throw std::runtime_error("WPT_MINSIZE requires NWidgetResizeBase");
			assert(nwid.u.xy.x >= 0 && nwid.u.xy.y >= 0);
			nwrb->SetMinimalSize(nwid.u.xy.x, nwid.u.xy.y);
			break;
		}

		case WPT_MINTEXTLINES: {
			NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(dest);
			if (nwrb == nullptr) [[unlikely]] throw std::runtime_error("WPT_MINTEXTLINES requires NWidgetResizeBase");
			assert(nwid.u.text_lines.size >= FS_BEGIN && nwid.u.text_lines.size < FS_END);
			nwrb->SetMinimalTextLines(nwid.u.text_lines.lines, nwid.u.text_lines.spacing, nwid.u.text_lines.size);
			break;
		}

		case WPT_TEXTSTYLE: {
			NWidgetCore *nwc = dynamic_cast<NWidgetCore *>(dest);
			if (nwc == nullptr) [[unlikely]] throw std::runtime_error("WPT_TEXTSTYLE requires NWidgetCore");
			nwc->SetTextStyle(nwid.u.text_style.colour, nwid.u.text_style.size);
			break;
		}

		case WPT_ALIGNMENT: {
			NWidgetCore *nwc = dynamic_cast<NWidgetCore *>(dest);
			if (nwc == nullptr) [[unlikely]] throw std::runtime_error("WPT_ALIGNMENT requires NWidgetCore");
			nwc->SetAlignment(nwid.u.align.align);
			break;
		}

		case WPT_FILL: {
			NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(dest);
			if (nwrb == nullptr) [[unlikely]] throw std::runtime_error("WPT_FILL requires NWidgetResizeBase");
			nwrb->SetFill(nwid.u.xy.x, nwid.u.xy.y);
			break;
		}

		case WPT_DATATIP: {
			NWidgetCore *nwc = dynamic_cast<NWidgetCore *>(dest);
			if (nwc == nullptr) [[unlikely]] throw std::runtime_error("WPT_DATATIP requires NWidgetCore");
			nwc->widget_data = nwid.u.data_tip.data;
			nwc->SetToolTip(nwid.u.data_tip.tooltip);
			break;
		}

		case WPT_PADDING:
			if (dest == nullptr) [[unlikely]] throw std::runtime_error("WPT_PADDING requires NWidgetBase");
			dest->SetPadding(nwid.u.padding);
			break;

		case WPT_PIPSPACE: {
			NWidgetPIPContainer *nwc = dynamic_cast<NWidgetPIPContainer *>(dest);
			if (nwc != nullptr) nwc->SetPIP(nwid.u.pip.pre, nwid.u.pip.inter, nwid.u.pip.post);

			NWidgetBackground *nwb = dynamic_cast<NWidgetBackground *>(dest);
			if (nwb != nullptr) nwb->SetPIP(nwid.u.pip.pre, nwid.u.pip.inter, nwid.u.pip.post);

			if (nwc == nullptr && nwb == nullptr) [[unlikely]] throw std::runtime_error("WPT_PIPSPACE requires NWidgetPIPContainer or NWidgetBackground");
			break;
		}

		case WPT_PIPRATIO: {
			NWidgetPIPContainer *nwc = dynamic_cast<NWidgetPIPContainer *>(dest);
			if (nwc != nullptr) nwc->SetPIPRatio(nwid.u.pip.pre, nwid.u.pip.inter, nwid.u.pip.post);

			NWidgetBackground *nwb = dynamic_cast<NWidgetBackground *>(dest);
			if (nwb != nullptr) nwb->SetPIPRatio(nwid.u.pip.pre, nwid.u.pip.inter, nwid.u.pip.post);

			if (nwc == nullptr && nwb == nullptr) [[unlikely]] throw std::runtime_error("WPT_PIPRATIO requires NWidgetPIPContainer or NWidgetBackground");
			break;
		}

		case WPT_SCROLLBAR: {
			NWidgetCore *nwc = dynamic_cast<NWidgetCore *>(dest);
			if (nwc == nullptr) [[unlikely]] throw std::runtime_error("WPT_SCROLLBAR requires NWidgetCore");
			nwc->scrollbar_index = nwid.u.widget.index;
			break;
		}

		case WPT_ASPECT: {
			if (dest == nullptr) [[unlikely]] throw std::runtime_error("WPT_ASPECT requires NWidgetBase");
			dest->aspect_ratio = nwid.u.aspect.ratio;
			dest->aspect_flags = nwid.u.aspect.flags;
			break;
		}

		default:
			NOT_REACHED();
	}
}

/**
 * Make NWidget from an NWidgetPart.
 * @param nwid NWidgetPart.
 * @pre NWidgetPart must not be an attribute NWidgetPart nor WPT_ENDCONTAINER.
 * @return Pointer to created NWidget.
 */
static std::unique_ptr<NWidgetBase> MakeNWidget(const NWidgetPart &nwid)
{
	assert(!IsAttributeWidgetPartType(nwid.type));
	assert(nwid.type != WPT_ENDCONTAINER);

	switch (nwid.type) {
		case NWID_SPACER: return std::make_unique<NWidgetSpacer>(0, 0);

		case WWT_PANEL: [[fallthrough]];
		case WWT_INSET: [[fallthrough]];
		case WWT_FRAME: return std::make_unique<NWidgetBackground>(nwid.type, nwid.u.widget.colour, nwid.u.widget.index);

		case NWID_HORIZONTAL: return std::make_unique<NWidgetHorizontal>(nwid.u.container.flags, nwid.u.container.index);
		case NWID_HORIZONTAL_LTR: return std::make_unique<NWidgetHorizontalLTR>(nwid.u.container.flags, nwid.u.container.index);
		case NWID_VERTICAL: return std::make_unique<NWidgetVertical>(nwid.u.container.flags, nwid.u.container.index);
		case NWID_SELECTION: return std::make_unique<NWidgetStacked>(nwid.u.widget.index);
		case NWID_MATRIX: return std::make_unique<NWidgetMatrix>(nwid.u.widget.colour, nwid.u.widget.index);
		case NWID_VIEWPORT: return std::make_unique<NWidgetViewport>(nwid.u.widget.index);
		case NWID_LAYER: return std::make_unique<NWidgetLayer>(nwid.u.widget.index);

		case NWID_HSCROLLBAR: [[fallthrough]];
		case NWID_VSCROLLBAR: return std::make_unique<NWidgetScrollbar>(nwid.type, nwid.u.widget.colour, nwid.u.widget.index);

		case WPT_FUNCTION: return nwid.u.func_ptr();

		default:
			assert((nwid.type & WWT_MASK) < WWT_LAST || (nwid.type & WWT_MASK) == NWID_BUTTON_DROPDOWN);
			return std::make_unique<NWidgetLeaf>(nwid.type, nwid.u.widget.colour, nwid.u.widget.index, WidgetData{}, STR_NULL);
	}
}

/**
 * Construct a single nested widget in \a *dest from its parts.
 *
 * Construct a NWidgetBase object from a #NWidget function, and apply all
 * attributes that follow it, until encountering a #EndContainer, another
 * #NWidget, or the end of the parts array.
 *
 * @param nwid_begin Iterator to beginning of nested widget parts.
 * @param nwid_end Iterator to ending of nested widget parts.
 * @param[out] dest Address of pointer to use for returning the composed widget.
 * @param[out] fill_dest Fill the composed widget with child widgets.
 * @return Iterator to remaining nested widget parts.
 */
static std::span<const NWidgetPart>::iterator MakeNWidget(std::span<const NWidgetPart>::iterator nwid_begin, std::span<const NWidgetPart>::iterator nwid_end, std::unique_ptr<NWidgetBase> &dest, bool &fill_dest)
{
	dest = nullptr;

	if (IsAttributeWidgetPartType(nwid_begin->type)) [[unlikely]] throw std::runtime_error("Expected non-attribute NWidgetPart type");
	if (nwid_begin->type == WPT_ENDCONTAINER) return nwid_begin;

	fill_dest = IsContainerWidgetType(nwid_begin->type);
	dest = MakeNWidget(*nwid_begin);
	if (dest == nullptr) return nwid_begin;

	++nwid_begin;

	/* Once a widget is created, we're now looking for attributes. */
	while (nwid_begin != nwid_end && IsAttributeWidgetPartType(nwid_begin->type)) {
		ApplyNWidgetPartAttribute(*nwid_begin, dest.get());
		++nwid_begin;
	}

	return nwid_begin;
}

/**
 * Test if WidgetType is a container widget.
 * @param tp WidgetType to test.
 * @return True iff WidgetType is a container widget.
 */
bool IsContainerWidgetType(WidgetType tp)
{
	return tp == NWID_HORIZONTAL || tp == NWID_HORIZONTAL_LTR || tp == NWID_VERTICAL || tp == NWID_MATRIX
		|| tp == WWT_PANEL || tp == WWT_FRAME || tp == WWT_INSET || tp == NWID_SELECTION || tp == NWID_LAYER;
}

/**
 * Build a nested widget tree by recursively filling containers with nested widgets read from their parts.
 * @param nwid_begin Iterator to beginning of nested widget parts.
 * @param nwid_end Iterator to ending of nested widget parts.
 * @param parent Pointer or container to use for storing the child widgets (*parent == nullptr or *parent == container or background widget).
 * @return Iterator to remaining nested widget parts.
 */
static std::span<const NWidgetPart>::iterator MakeWidgetTree(std::span<const NWidgetPart>::iterator nwid_begin, std::span<const NWidgetPart>::iterator nwid_end, std::unique_ptr<NWidgetBase> &parent)
{
	/* If *parent == nullptr, only the first widget is read and returned. Otherwise, *parent must point to either
	 * a #NWidgetContainer or a #NWidgetBackground object, and parts are added as much as possible. */
	NWidgetContainer *nwid_cont = dynamic_cast<NWidgetContainer *>(parent.get());
	NWidgetBackground *nwid_parent = dynamic_cast<NWidgetBackground *>(parent.get());
	assert(parent == nullptr || (nwid_cont != nullptr && nwid_parent == nullptr) || (nwid_cont == nullptr && nwid_parent != nullptr));

	while (nwid_begin != nwid_end) {
		std::unique_ptr<NWidgetBase> sub_widget = nullptr;
		bool fill_sub = false;
		nwid_begin = MakeNWidget(nwid_begin, nwid_end, sub_widget, fill_sub);

		/* Break out of loop when end reached */
		if (sub_widget == nullptr) break;

		/* If sub-widget is a container, recursively fill that container. */
		if (fill_sub && IsContainerWidgetType(sub_widget->type)) {
			nwid_begin = MakeWidgetTree(nwid_begin, nwid_end, sub_widget);
		}

		/* Add sub_widget to parent container if available, otherwise return the widget to the caller. */
		if (nwid_cont != nullptr) nwid_cont->Add(std::move(sub_widget));
		if (nwid_parent != nullptr) nwid_parent->Add(std::move(sub_widget));
		if (nwid_cont == nullptr && nwid_parent == nullptr) {
			parent = std::move(sub_widget);
			return nwid_begin;
		}
	}

	if (nwid_begin == nwid_end) return nwid_begin; // Reached the end of the array of parts?

	assert(nwid_begin < nwid_end);
	assert(nwid_begin->type == WPT_ENDCONTAINER);
	return std::next(nwid_begin); // *nwid_begin is also 'used'
}

/**
 * Construct a nested widget tree from an array of parts.
 * @param nwid_parts Span of nested widget parts.
 * @param container Container to add the nested widgets to. In case it is nullptr a vertical container is used.
 * @return Root of the nested widget tree, a vertical container containing the entire GUI.
 * @ingroup NestedWidgetParts
 */
std::unique_ptr<NWidgetBase> MakeNWidgets(std::span<const NWidgetPart> nwid_parts, std::unique_ptr<NWidgetBase> &&container)
{
	if (container == nullptr) container = std::make_unique<NWidgetVertical>();
	[[maybe_unused]] auto nwid_part = MakeWidgetTree(std::begin(nwid_parts), std::end(nwid_parts), container);
#ifdef WITH_ASSERT
	if (nwid_part != std::end(nwid_parts)) [[unlikely]] throw std::runtime_error("Did not consume all NWidgetParts");
#endif
	return std::move(container);
}

/**
 * Make a nested widget tree for a window from a parts array. Besides loading, it inserts a shading selection widget
 * between the title bar and the window body if the first widget in the parts array looks like a title bar (it is a horizontal
 * container with a caption widget) and has a shade box widget.
 * @param nwid_parts Span of nested widget parts.
 * @param[out] shade_select Pointer to the inserted shade selection widget (\c nullptr if not inserted).
 * @return Root of the nested widget tree, a vertical container containing the entire GUI.
 * @ingroup NestedWidgetParts
 */
std::unique_ptr<NWidgetBase> MakeWindowNWidgetTree(std::span<const NWidgetPart> nwid_parts, NWidgetStacked **shade_select)
{
	auto nwid_begin = std::begin(nwid_parts);
	auto nwid_end = std::end(nwid_parts);

	*shade_select = nullptr;

	/* Read the first widget recursively from the array. */
	std::unique_ptr<NWidgetBase> nwid = nullptr;
	nwid_begin = MakeWidgetTree(nwid_begin, nwid_end, nwid);
	assert(nwid != nullptr);

	NWidgetHorizontal *hor_cont = dynamic_cast<NWidgetHorizontal *>(nwid.get());

	auto root = std::make_unique<NWidgetVertical>();
	root->Add(std::move(nwid));
	if (nwid_begin == nwid_end) return root; // There is no body at all.

	if (hor_cont != nullptr && hor_cont->GetWidgetOfType(WWT_CAPTION) != nullptr && hor_cont->GetWidgetOfType(WWT_SHADEBOX) != nullptr) {
		/* If the first widget has a title bar and a shade box, silently add a shade selection widget in the tree. */
		auto shade_stack = std::make_unique<NWidgetStacked>(INVALID_WIDGET);
		*shade_select = shade_stack.get();
		/* Load the remaining parts into the shade stack. */
		shade_stack->Add(MakeNWidgets({nwid_begin, nwid_end}, std::make_unique<NWidgetVertical>()));
		root->Add(std::move(shade_stack));
		return root;
	}

	/* Load the remaining parts into 'root'. */
	return MakeNWidgets({nwid_begin, nwid_end}, std::move(root));
}

/**
 * Make a number of rows with button-like graphics, for enabling/disabling each company.
 * @param widget_first The first widget index to use.
 * @param widget_last The last widget index to use.
 * @param colour The colour in which to draw the button.
 * @param max_length Maximal number of company buttons in one row.
 * @param button_tooltip The tooltip-string of every button.
 * @param resizable Whether the rows are resizable.
 * @return Panel with rows of company buttons.
 */
std::unique_ptr<NWidgetBase> MakeCompanyButtonRows(WidgetID widget_first, WidgetID widget_last, Colours button_colour, int max_length, StringID button_tooltip, bool resizable)
{
	assert(max_length >= 1);
	std::unique_ptr<NWidgetVertical> vert = nullptr; // Storage for all rows.
	std::unique_ptr<NWidgetHorizontal> hor = nullptr; // Storage for buttons in one row.
	int hor_length = 0;

	Dimension sprite_size = GetSpriteSize(SPR_COMPANY_ICON, nullptr, ZoomLevel::Normal);
	sprite_size.width  += WidgetDimensions::unscaled.matrix.Horizontal();
	sprite_size.height += WidgetDimensions::unscaled.matrix.Vertical();

	for (WidgetID widnum = widget_first; widnum <= widget_last; widnum++) {
		/* Ensure there is room in 'hor' for another button. */
		if (hor_length == max_length) {
			if (vert == nullptr) vert = std::make_unique<NWidgetVertical>();
			vert->Add(std::move(hor));
			hor = nullptr;
			hor_length = 0;
		}
		if (hor == nullptr) {
			hor = std::make_unique<NWidgetHorizontal>();
			hor_length = 0;
		}

		auto panel = std::make_unique<NWidgetBackground>(WWT_PANEL, button_colour, widnum);
		panel->SetMinimalSize(sprite_size.width, sprite_size.height);
		panel->SetFill(1, 1);
		if (resizable) panel->SetResize(1, 0);
		panel->SetToolTip(button_tooltip);
		hor->Add(std::move(panel));
		hor_length++;
	}
	if (vert == nullptr) return hor; // All buttons fit in a single row.

	if (hor_length > 0 && hor_length < max_length) {
		/* Last row is partial, add a spacer at the end to force all buttons to the left. */
		auto spc = std::make_unique<NWidgetSpacer>(sprite_size.width, sprite_size.height);
		spc->SetFill(1, 1);
		if (resizable) spc->SetResize(1, 0);
		hor->Add(std::move(spc));
	}
	if (hor != nullptr) vert->Add(std::move(hor));
	return vert;
}

/**
 * Unfocuses the focused widget of the window,
 * if the focused widget is contained inside the container.
 * @param parent_window Window which contains this container.
 */
void NWidgetContainer::UnfocusWidgets(Window *parent_window)
{
	assert(parent_window != nullptr);
	if (parent_window->nested_focus != nullptr) {
		for (auto &widget : this->children) {
			if (parent_window->nested_focus == widget.get()) {
				parent_window->UnfocusFocusedWidget();
			}
		}
	}
}
