/* $Id$ */

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

#include "table/sprites.h"
#include "table/strings.h"

static const char *UPARROW   = "\xEE\x8A\xA0"; ///< String containing an upwards pointing arrow.
static const char *DOWNARROW = "\xEE\x8A\xAA"; ///< String containing a downwards pointing arrow.

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
	top += 10;   // top    points to just below the up-button
	bottom -= 9; // bottom points to top of the down-button

	int height = (bottom - top);
	int pos = sb->GetPosition();
	int count = sb->GetCount();
	int cap = sb->GetCapacity();

	if (count != 0) top += height * pos / count;

	if (cap > count) cap = count;
	if (count != 0) bottom -= (count - pos - cap) * height / count;

	Point pt;
	if (horizontal && _current_text_dir == TD_RTL) {
		pt.x = rev_base - (bottom - 1);
		pt.y = rev_base - top;
	} else {
		pt.x = top;
		pt.y = bottom - 1;
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
	bool rtl = false;

	if (sb->type == NWID_HSCROLLBAR) {
		pos = x;
		rtl = _current_text_dir == TD_RTL;
	} else {
		pos = y;
	}
	if (pos <= mi + 9) {
		/* Pressing the upper button? */
		SetBit(sb->disp_flags, NDB_SCROLLBAR_UP);
		if (_scroller_click_timeout <= 1) {
			_scroller_click_timeout = 3;
			sb->UpdatePosition(rtl ? 1 : -1);
		}
		w->scrolling_scrollbar = sb->index;
	} else if (pos >= ma - 10) {
		/* Pressing the lower button? */
		SetBit(sb->disp_flags, NDB_SCROLLBAR_DOWN);

		if (_scroller_click_timeout <= 1) {
			_scroller_click_timeout = 3;
			sb->UpdatePosition(rtl ? -1 : 1);
		}
		w->scrolling_scrollbar = sb->index;
	} else {
		Point pt = HandleScrollbarHittest(sb, mi, ma, sb->type == NWID_HSCROLLBAR);

		if (pos < pt.x) {
			sb->UpdatePosition(rtl ? 1 : -1, Scrollbar::SS_BIG);
		} else if (pos > pt.y) {
			sb->UpdatePosition(rtl ? -1 : 1, Scrollbar::SS_BIG);
		} else {
			_scrollbar_start_pos = pt.x - mi - 9;
			_scrollbar_size = ma - mi - 23;
			w->scrolling_scrollbar = sb->index;
			_cursorpos_drag_start = _cursor.pos;
		}
	}

	w->SetDirty();
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
	ScrollbarClickPositioning(w, dynamic_cast<NWidgetScrollbar*>(nw), x, y, mi, ma);
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
	return (nw != NULL) ? nw->index : -1;
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
	uint dark         = _colour_gradient[colour][3];
	uint medium_dark  = _colour_gradient[colour][5];
	uint medium_light = _colour_gradient[colour][6];
	uint light        = _colour_gradient[colour][7];

	if (flags & FR_TRANSPARENT) {
		GfxFillRect(left, top, right, bottom, PALETTE_TO_TRANSPARENT, FILLRECT_RECOLOUR);
	} else {
		uint interior;

		if (flags & FR_LOWERED) {
			GfxFillRect(left,     top,     left,  bottom,     dark);
			GfxFillRect(left + 1, top,     right, top,        dark);
			GfxFillRect(right,    top + 1, right, bottom - 1, light);
			GfxFillRect(left + 1, bottom,  right, bottom,     light);
			interior = (flags & FR_DARKENED ? medium_dark : medium_light);
		} else {
			GfxFillRect(left,     top,    left,      bottom - 1, light);
			GfxFillRect(left + 1, top,    right - 1, top,        light);
			GfxFillRect(right,    top,    right,     bottom - 1, dark);
			GfxFillRect(left,     bottom, right,     bottom,     dark);
			interior = medium_dark;
		}
		if (!(flags & FR_BORDERONLY)) {
			GfxFillRect(left + 1, top + 1, right - 1, bottom - 1, interior);
		}
	}
}

/**
 * Draw an image button.
 * @param r       Rectangle of the button.
 * @param type    Widget type (#WWT_IMGBTN or #WWT_IMGBTN_2).
 * @param colour  Colour of the button.
 * @param clicked Button is lowered.
 * @param img     Sprite to draw.
 */
static inline void DrawImageButtons(const Rect &r, WidgetType type, Colours colour, bool clicked, SpriteID img)
{
	assert(img != 0);
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, (clicked) ? FR_LOWERED : FR_NONE);

	if ((type & WWT_MASK) == WWT_IMGBTN_2 && clicked) img++; // Show different image when clicked for #WWT_IMGBTN_2.
	DrawSprite(img, PAL_NONE, r.left + WD_IMGBTN_LEFT + clicked, r.top + WD_IMGBTN_TOP + clicked);
}

/**
 * Draw the label-part of a widget.
 * @param r       Rectangle of the label background.
 * @param type    Widget type (#WWT_TEXTBTN, #WWT_TEXTBTN_2, or #WWT_LABEL).
 * @param clicked Label is rendered lowered.
 * @param str     Text to draw.
 */
static inline void DrawLabel(const Rect &r, WidgetType type, bool clicked, StringID str)
{
	if (str == STR_NULL) return;
	if ((type & WWT_MASK) == WWT_TEXTBTN_2 && clicked) str++;
	Dimension d = GetStringBoundingBox(str);
	int offset = max(0, ((int)(r.bottom - r.top + 1) - (int)d.height) / 2); // Offset for rendering the text vertically centered
	DrawString(r.left + clicked, r.right + clicked, r.top + offset + clicked, str, TC_FROMSTRING, SA_HOR_CENTER);
}

/**
 * Draw text.
 * @param r      Rectangle of the background.
 * @param colour Colour of the text.
 * @param str    Text to draw.
 */
static inline void DrawText(const Rect &r, TextColour colour, StringID str)
{
	Dimension d = GetStringBoundingBox(str);
	int offset = max(0, ((int)(r.bottom - r.top + 1) - (int)d.height) / 2); // Offset for rendering the text vertically centered
	if (str != STR_NULL) DrawString(r.left, r.right, r.top + offset, str, colour);
}

/**
 * Draw an inset widget.
 * @param r      Rectangle of the background.
 * @param colour Colour of the inset.
 * @param str    Text to draw.
 */
static inline void DrawInset(const Rect &r, Colours colour, StringID str)
{
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, FR_LOWERED | FR_DARKENED);
	if (str != STR_NULL) DrawString(r.left + WD_INSET_LEFT, r.right - WD_INSET_RIGHT, r.top + WD_INSET_TOP, str);
}

/**
 * Draw a matrix widget.
 * @param r       Rectangle of the matrix background.
 * @param colour  Colour of the background.
 * @param clicked Matrix is rendered lowered.
 * @param data    Data of the widget, number of rows and columns of the widget.
 */
static inline void DrawMatrix(const Rect &r, Colours colour, bool clicked, uint16 data)
{
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, (clicked) ? FR_LOWERED : FR_NONE);

	int num_columns = GB(data, MAT_COL_START, MAT_COL_BITS);  // Lower 8 bits of the widget data: Number of columns in the matrix.
	int column_width = (r.right - r.left + 1) / num_columns; // Width of a single column in the matrix.

	int num_rows = GB(data, MAT_ROW_START, MAT_ROW_BITS); // Upper 8 bits of the widget data: Number of rows in the matrix.
	int row_height = (r.bottom - r.top + 1) / num_rows; // Height of a single row in the matrix.

	int col = _colour_gradient[colour & 0xF][6];

	int x = r.left;
	for (int ctr = num_columns; ctr > 1; ctr--) {
		x += column_width;
		GfxFillRect(x, r.top + 1, x, r.bottom - 1, col);
	}

	x = r.top;
	for (int ctr = num_rows; ctr > 1; ctr--) {
		x += row_height;
		GfxFillRect(r.left + 1, x, r.right - 1, x, col);
	}

	col = _colour_gradient[colour & 0xF][4];

	x = r.left - 1;
	for (int ctr = num_columns; ctr > 1; ctr--) {
		x += column_width;
		GfxFillRect(x, r.top + 1, x, r.bottom - 1, col);
	}

	x = r.top - 1;
	for (int ctr = num_rows; ctr > 1; ctr--) {
		x += row_height;
		GfxFillRect(r.left + 1, x, r.right - 1, x, col);
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
	/* draw up/down buttons */
	DrawFrameRect(r.left, r.top, r.right, r.top + 9, colour, (up_clicked) ? FR_LOWERED : FR_NONE);
	DrawString(r.left + up_clicked, r.right + up_clicked, r.top + up_clicked, UPARROW, TC_BLACK, SA_HOR_CENTER);

	DrawFrameRect(r.left, r.bottom - 9, r.right, r.bottom, colour, (down_clicked) ? FR_LOWERED : FR_NONE);
	DrawString(r.left + down_clicked, r.right + down_clicked, r.bottom - 9 + down_clicked, DOWNARROW, TC_BLACK, SA_HOR_CENTER);

	int c1 = _colour_gradient[colour & 0xF][3];
	int c2 = _colour_gradient[colour & 0xF][7];

	/* draw "shaded" background */
	GfxFillRect(r.left, r.top + 10, r.right, r.bottom - 10, c2);
	GfxFillRect(r.left, r.top + 10, r.right, r.bottom - 10, c1, FILLRECT_CHECKER);

	/* draw shaded lines */
	GfxFillRect(r.left + 2, r.top + 10, r.left + 2, r.bottom - 10, c1);
	GfxFillRect(r.left + 3, r.top + 10, r.left + 3, r.bottom - 10, c2);
	GfxFillRect(r.left + 7, r.top + 10, r.left + 7, r.bottom - 10, c1);
	GfxFillRect(r.left + 8, r.top + 10, r.left + 8, r.bottom - 10, c2);

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
	DrawFrameRect(r.left, r.top, r.left + 9, r.bottom, colour, left_clicked ? FR_LOWERED : FR_NONE);
	DrawSprite(SPR_ARROW_LEFT, PAL_NONE, r.left + 1 + left_clicked, r.top + 1 + left_clicked);

	DrawFrameRect(r.right - 9, r.top, r.right, r.bottom, colour, right_clicked ? FR_LOWERED : FR_NONE);
	DrawSprite(SPR_ARROW_RIGHT, PAL_NONE, r.right - 8 + right_clicked, r.top + 1 + right_clicked);

	int c1 = _colour_gradient[colour & 0xF][3];
	int c2 = _colour_gradient[colour & 0xF][7];

	/* draw "shaded" background */
	GfxFillRect(r.left + 10, r.top, r.right - 10, r.bottom, c2);
	GfxFillRect(r.left + 10, r.top, r.right - 10, r.bottom, c1, FILLRECT_CHECKER);

	/* draw shaded lines */
	GfxFillRect(r.left + 10, r.top + 2, r.right - 10, r.top + 2, c1);
	GfxFillRect(r.left + 10, r.top + 3, r.right - 10, r.top + 3, c2);
	GfxFillRect(r.left + 10, r.top + 7, r.right - 10, r.top + 7, c1);
	GfxFillRect(r.left + 10, r.top + 8, r.right - 10, r.top + 8, c2);

	/* draw actual scrollbar */
	Point pt = HandleScrollbarHittest(scrollbar, r.left, r.right, true);
	DrawFrameRect(pt.x, r.top, pt.y, r.bottom, colour, bar_dragged ? FR_LOWERED : FR_NONE);
}

/**
 * Draw a frame widget.
 * @param r      Rectangle of the frame.
 * @param colour Colour of the frame.
 * @param str    Text of the frame.
 */
static inline void DrawFrame(const Rect &r, Colours colour, StringID str)
{
	int x2 = r.left; // by default the left side is the left side of the widget

	if (str != STR_NULL) x2 = DrawString(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, r.top, str);

	int c1 = _colour_gradient[colour][3];
	int c2 = _colour_gradient[colour][7];

	/* If the frame has text, adjust the top bar to fit half-way through */
	int dy1 = 4;
	if (str != STR_NULL) dy1 = FONT_HEIGHT_NORMAL / 2 - 1;
	int dy2 = dy1 + 1;

	if (_current_text_dir == TD_LTR) {
		/* Line from upper left corner to start of text */
		GfxFillRect(r.left, r.top + dy1, r.left + 4, r.top + dy1, c1);
		GfxFillRect(r.left + 1, r.top + dy2, r.left + 4, r.top + dy2, c2);

		/* Line from end of text to upper right corner */
		GfxFillRect(x2, r.top + dy1, r.right - 1, r.top + dy1, c1);
		GfxFillRect(x2, r.top + dy2, r.right - 2, r.top + dy2, c2);
	} else {
		/* Line from upper left corner to start of text */
		GfxFillRect(r.left, r.top + dy1, x2 - 2, r.top + dy1, c1);
		GfxFillRect(r.left + 1, r.top + dy2, x2 - 2, r.top + dy2, c2);

		/* Line from end of text to upper right corner */
		GfxFillRect(r.right - 5, r.top + dy1, r.right - 1, r.top + dy1, c1);
		GfxFillRect(r.right - 5, r.top + dy2, r.right - 2, r.top + dy2, c2);
	}

	/* Line from upper left corner to bottom left corner */
	GfxFillRect(r.left, r.top + dy2, r.left, r.bottom - 1, c1);
	GfxFillRect(r.left + 1, r.top + dy2 + 1, r.left + 1, r.bottom - 2, c2);

	/* Line from upper right corner to bottom right corner */
	GfxFillRect(r.right - 1, r.top + dy2, r.right - 1, r.bottom - 2, c1);
	GfxFillRect(r.right, r.top + dy1, r.right, r.bottom - 1, c2);

	GfxFillRect(r.left + 1, r.bottom - 1, r.right - 1, r.bottom - 1, c1);
	GfxFillRect(r.left, r.bottom, r.right, r.bottom, c2);
}

/**
 * Draw a shade box.
 * @param r       Rectangle of the box.
 * @param colour  Colour of the shade box.
 * @param clicked Box is lowered.
 */
static inline void DrawShadeBox(const Rect &r, Colours colour, bool clicked)
{
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, (clicked) ? FR_LOWERED : FR_NONE);
	DrawSprite((clicked) ? SPR_WINDOW_SHADE : SPR_WINDOW_UNSHADE, PAL_NONE, r.left + WD_SHADEBOX_LEFT + clicked, r.top + WD_SHADEBOX_TOP + clicked);
}

/**
 * Draw a sticky box.
 * @param r       Rectangle of the box.
 * @param colour  Colour of the sticky box.
 * @param clicked Box is lowered.
 */
static inline void DrawStickyBox(const Rect &r, Colours colour, bool clicked)
{
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, (clicked) ? FR_LOWERED : FR_NONE);
	DrawSprite((clicked) ? SPR_PIN_UP : SPR_PIN_DOWN, PAL_NONE, r.left + WD_STICKYBOX_LEFT + clicked, r.top + WD_STICKYBOX_TOP + clicked);
}

/**
 * Draw a NewGRF debug box.
 * @param r       Rectangle of the box.
 * @param colour  Colour of the debug box.
 * @param clicked Box is lowered.
 */
static inline void DrawDebugBox(const Rect &r, Colours colour, bool clicked)
{
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, (clicked) ? FR_LOWERED : FR_NONE);
	DrawSprite(SPR_WINDOW_DEBUG, PAL_NONE, r.left + WD_DEBUGBOX_LEFT + clicked, r.top + WD_DEBUGBOX_TOP + clicked);
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
	if (at_left) {
		DrawSprite(SPR_WINDOW_RESIZE_LEFT, PAL_NONE, r.left + WD_RESIZEBOX_RIGHT + clicked, r.top + WD_RESIZEBOX_TOP + clicked);
	} else {
		DrawSprite(SPR_WINDOW_RESIZE_RIGHT, PAL_NONE, r.left + WD_RESIZEBOX_LEFT + clicked, r.top + WD_RESIZEBOX_TOP + clicked);
	}
}

/**
 * Draw a close box.
 * @param r      Rectangle of the box.
 * @param colour Colour of the close box.
 * @param str    Cross to draw (#STR_BLACK_CROSS or #STR_SILVER_CROSS).
 */
static inline void DrawCloseBox(const Rect &r, Colours colour, StringID str)
{
	assert(str == STR_BLACK_CROSS || str == STR_SILVER_CROSS); // black or silver cross
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, FR_NONE);
	DrawString(r.left + WD_CLOSEBOX_LEFT, r.right - WD_CLOSEBOX_RIGHT, r.top + WD_CLOSEBOX_TOP, str, TC_FROMSTRING, SA_HOR_CENTER);
}

/**
 * Draw a caption bar.
 * @param r      Rectangle of the bar.
 * @param colour Colour of the window.
 * @param owner  'Owner' of the window.
 * @param str    Text to draw in the bar.
 */
void DrawCaption(const Rect &r, Colours colour, Owner owner, StringID str)
{
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, FR_BORDERONLY);
	DrawFrameRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, colour, (owner == INVALID_OWNER) ? FR_LOWERED | FR_DARKENED : FR_LOWERED | FR_DARKENED | FR_BORDERONLY);

	if (owner != INVALID_OWNER) {
		GfxFillRect(r.left + 2, r.top + 2, r.right - 2, r.bottom - 2, _colour_gradient[_company_colours[owner]][4]);
	}

	if (str != STR_NULL) DrawString(r.left + WD_CAPTIONTEXT_LEFT, r.right - WD_CAPTIONTEXT_RIGHT, r.top + WD_CAPTIONTEXT_TOP, str, TC_FROMSTRING, SA_HOR_CENTER);
}

/**
 * Draw a button with a dropdown (#WWT_DROPDOWN and #NWID_BUTTON_DROPDOWN).
 * @param r                Rectangle containing the widget.
 * @param colour           Background colour of the widget.
 * @param clicked_button   The button-part is lowered.
 * @param clicked_dropdown The drop-down part is lowered.
 * @param str              Text of the button.
 *
 * @note Magic constants are also used in #NWidgetLeaf::ButtonHit.
 */
static inline void DrawButtonDropdown(const Rect &r, Colours colour, bool clicked_button, bool clicked_dropdown, StringID str)
{
	if (_current_text_dir == TD_LTR) {
		DrawFrameRect(r.left, r.top, r.right - 12, r.bottom, colour, clicked_button ? FR_LOWERED : FR_NONE);
		DrawFrameRect(r.right - 11, r.top, r.right, r.bottom, colour, clicked_dropdown ? FR_LOWERED : FR_NONE);
		DrawString(r.right - (clicked_dropdown ? 10 : 11), r.right, r.top + (clicked_dropdown ? 2 : 1), DOWNARROW, TC_BLACK, SA_HOR_CENTER);
		if (str != STR_NULL) DrawString(r.left + WD_DROPDOWNTEXT_LEFT + clicked_button, r.right - WD_DROPDOWNTEXT_RIGHT + clicked_button, r.top + WD_DROPDOWNTEXT_TOP + clicked_button, str, TC_BLACK);
	} else {
		DrawFrameRect(r.left + 12, r.top, r.right, r.bottom, colour, clicked_button ? FR_LOWERED : FR_NONE);
		DrawFrameRect(r.left, r.top, r.left + 11, r.bottom, colour, clicked_dropdown ? FR_LOWERED : FR_NONE);
		DrawString(r.left + clicked_dropdown, r.left + 11, r.top + (clicked_dropdown ? 2 : 1), DOWNARROW, TC_BLACK, SA_HOR_CENTER);
		if (str != STR_NULL) DrawString(r.left + WD_DROPDOWNTEXT_RIGHT + clicked_button, r.right - WD_DROPDOWNTEXT_LEFT + clicked_button, r.top + WD_DROPDOWNTEXT_TOP + clicked_button, str, TC_BLACK);
	}
}

/**
 * Draw a dropdown #WWT_DROPDOWN widget.
 * @param r       Rectangle containing the widget.
 * @param colour  Background colour of the widget.
 * @param clicked The widget is lowered.
 * @param str     Text of the button.
 */
static inline void DrawDropdown(const Rect &r, Colours colour, bool clicked, StringID str)
{
	DrawButtonDropdown(r, colour, false, clicked, str);
}

/**
 * Paint all widgets of a window.
 */
void Window::DrawWidgets() const
{
	this->nested_root->Draw(this);

	if (this->flags4 & WF_WHITE_BORDER_MASK) {
		DrawFrameRect(0, 0, this->width - 1, this->height - 1, COLOUR_WHITE, FR_BORDERONLY);
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

	assert(this->nested_array != NULL);
	const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(widget);

	int offset = this->IsWidgetLowered(widget) ? 1 : 0;
	int base = offset + nwid->pos_x + (_current_text_dir == TD_LTR ? nwid->current_x - WD_SORTBUTTON_ARROW_WIDTH : 0);
	int top = nwid->pos_y;

	DrawString(base, base + WD_SORTBUTTON_ARROW_WIDTH, top + 1 + offset, state == SBS_DOWN ? DOWNARROW : UPARROW, TC_BLACK, SA_HOR_CENTER);
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
 * <li> #NWidgetHorizontalLTR for organizing child widgets in a (horizontal) row, always in the same order. All childs below this container will also
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
 * @fn void FillNestedArray(NWidgetBase **array, uint length)
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
	SetDirtyBlocks(abs_left, abs_top, abs_left + this->current_x, abs_top + this->current_y);
}

/**
 * @fn NWidgetCore *NWidgetBase::GetWidgetFromPos(int x, int y)
 * Retrieve a widget by its position.
 * @param x Horizontal position relative to the left edge of the window.
 * @param y Vertical position relative to the top edge of the window.
 * @return Returns the deepest nested widget that covers the given position, or \c NULL if no widget can be found.
 */

/**
 * Retrieve a widget by its type.
 * @param tp Widget type to search for.
 * @return Returns the first widget of the specified type, or \c NULL if no widget can be found.
 */
NWidgetBase *NWidgetBase::GetWidgetOfType(WidgetType tp)
{
	return (this->type == tp) ? this : NULL;
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

/**
 * Set minimal size of the widget.
 * @param min_x Horizontal minimal size of the widget.
 * @param min_y Vertical minimal size of the widget.
 */
void NWidgetResizeBase::SetMinimalSize(uint min_x, uint min_y)
{
	this->min_x = min_x;
	this->min_y = min_y;
}

/**
 * Set minimal text lines for the widget.
 * @param min_lines Number of text lines of the widget.
 * @param spacing   Extra spacing (eg WD_FRAMERECT_TOP + _BOTTOM) of the widget.
 * @param size      Font size of text.
 */
void NWidgetResizeBase::SetMinimalTextLines(uint8 min_lines, uint8 spacing, FontSize size)
{
	this->min_y = min_lines * GetCharacterHeight(size) + spacing;
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
 * @param tool_tip    Tool tip of the widget. @see Widget::tootips
 */
NWidgetCore::NWidgetCore(WidgetType tp, Colours colour, uint fill_x, uint fill_y, uint16 widget_data, StringID tool_tip) : NWidgetResizeBase(tp, fill_x, fill_y)
{
	this->colour = colour;
	this->index = -1;
	this->widget_data = widget_data;
	this->tool_tip = tool_tip;
	this->scrollbar_index = -1;
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
void NWidgetCore::SetDataTip(uint16 widget_data, StringID tool_tip)
{
	this->widget_data = widget_data;
	this->tool_tip = tool_tip;
}

void NWidgetCore::FillNestedArray(NWidgetBase **array, uint length)
{
	if (this->index >= 0 && (uint)(this->index) < length) array[this->index] = this;
}

NWidgetCore *NWidgetCore::GetWidgetFromPos(int x, int y)
{
	return (IsInsideBS(x, this->pos_x, this->current_x) && IsInsideBS(y, this->pos_y, this->current_y)) ? this : NULL;
}

/**
 * Constructor container baseclass.
 * @param tp Type of the container.
 */
NWidgetContainer::NWidgetContainer(WidgetType tp) : NWidgetBase(tp)
{
	this->head = NULL;
	this->tail = NULL;
}

NWidgetContainer::~NWidgetContainer()
{
	while (this->head != NULL) {
		NWidgetBase *wid = this->head->next;
		delete this->head;
		this->head = wid;
	}
	this->tail = NULL;
}

NWidgetBase *NWidgetContainer::GetWidgetOfType(WidgetType tp)
{
	if (this->type == tp) return this;
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		NWidgetBase *nwid = child_wid->GetWidgetOfType(tp);
		if (nwid != NULL) return nwid;
	}
	return NULL;
}

/**
 * Append widget \a wid to container.
 * @param wid Widget to append.
 */
void NWidgetContainer::Add(NWidgetBase *wid)
{
	assert(wid->next == NULL && wid->prev == NULL);

	if (this->head == NULL) {
		this->head = wid;
		this->tail = wid;
	} else {
		assert(this->tail != NULL);
		assert(this->tail->next == NULL);

		this->tail->next = wid;
		wid->prev = this->tail;
		this->tail = wid;
	}
}

void NWidgetContainer::FillNestedArray(NWidgetBase **array, uint length)
{
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
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
	this->fill_x = (this->head != NULL) ? 1 : 0;
	this->fill_y = (this->head != NULL) ? 1 : 0;
	this->resize_x = (this->head != NULL) ? 1 : 0;
	this->resize_y = (this->head != NULL) ? 1 : 0;
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		child_wid->SetupSmallestSize(w, init_array);

		this->smallest_x = max(this->smallest_x, child_wid->smallest_x + child_wid->padding_left + child_wid->padding_right);
		this->smallest_y = max(this->smallest_y, child_wid->smallest_y + child_wid->padding_top + child_wid->padding_bottom);
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

	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		uint hor_step = (sizing == ST_SMALLEST) ? 1 : child_wid->GetHorizontalStepSize(sizing);
		uint child_width = ComputeMaxSize(child_wid->smallest_x, given_width - child_wid->padding_left - child_wid->padding_right, hor_step);
		uint child_pos_x = (rtl ? child_wid->padding_right : child_wid->padding_left);

		uint vert_step = (sizing == ST_SMALLEST) ? 1 : child_wid->GetVerticalStepSize(sizing);
		uint child_height = ComputeMaxSize(child_wid->smallest_y, given_height - child_wid->padding_top - child_wid->padding_bottom, vert_step);
		uint child_pos_y = child_wid->padding_top;

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
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; plane++, child_wid = child_wid->next) {
		if (plane == this->shown_plane) {
			child_wid->Draw(w);
			return;
		}
	}

	NOT_REACHED();
}

NWidgetCore *NWidgetStacked::GetWidgetFromPos(int x, int y)
{
	if (this->shown_plane >= SZSP_BEGIN) return NULL;

	if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return NULL;
	int plane = 0;
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; plane++, child_wid = child_wid->next) {
		if (plane == this->shown_plane) {
			return child_wid->GetWidgetFromPos(x, y);
		}
	}
	return NULL;
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
	this->pip_pre = pip_pre;
	this->pip_inter = pip_inter;
	this->pip_post = pip_post;
}

void NWidgetPIPContainer::Draw(const Window *w)
{
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		child_wid->Draw(w);
	}
}

NWidgetCore *NWidgetPIPContainer::GetWidgetFromPos(int x, int y)
{
	if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return NULL;

	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		NWidgetCore *nwid = child_wid->GetWidgetFromPos(x, y);
		if (nwid != NULL) return nwid;
	}
	return NULL;
}

/** Horizontal container widget. */
NWidgetHorizontal::NWidgetHorizontal(NWidContainerFlags flags) : NWidgetPIPContainer(NWID_HORIZONTAL, flags)
{
}

void NWidgetHorizontal::SetupSmallestSize(Window *w, bool init_array)
{
	this->smallest_x = 0; // Sum of minimal size of all childs.
	this->smallest_y = 0; // Biggest child.
	this->fill_x = 0;     // smallest non-zero child widget fill step.
	this->fill_y = 1;     // smallest common child fill step.
	this->resize_x = 0;   // smallest non-zero child widget resize step.
	this->resize_y = 1;   // smallest common child resize step.

	/* 1a. Forward call, collect biggest nested array index, and longest/widest child length. */
	uint longest = 0; // Longest child found.
	uint max_vert_fill = 0; // Biggest vertical fill step.
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		child_wid->SetupSmallestSize(w, init_array);
		longest = max(longest, child_wid->smallest_x);
		max_vert_fill = max(max_vert_fill, child_wid->GetVerticalStepSize(ST_SMALLEST));
		this->smallest_y = max(this->smallest_y, child_wid->smallest_y + child_wid->padding_top + child_wid->padding_bottom);
	}
	/* 1b. Make the container higher if needed to accomadate all childs nicely. */
	uint max_smallest = this->smallest_y + 3 * max_vert_fill; // Upper limit to computing smallest height.
	uint cur_height = this->smallest_y;
	while (true) {
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			uint step_size = child_wid->GetVerticalStepSize(ST_SMALLEST);
			uint child_height = child_wid->smallest_y + child_wid->padding_top + child_wid->padding_bottom;
			if (step_size > 1 && child_height < cur_height) { // Small step sizes or already fitting childs are not interesting.
				uint remainder = (cur_height - child_height) % step_size;
				if (remainder > 0) { // Child did not fit entirely, widen the container.
					cur_height += step_size - remainder;
					assert(cur_height < max_smallest); // Safeguard against infinite height expansion.
					/* Remaining childs will adapt to the new cur_height, thus speeding up the computation. */
				}
			}
		}
		if (this->smallest_y == cur_height) break;
		this->smallest_y = cur_height; // Smallest height got changed, try again.
	}
	/* 2. For containers that must maintain equal width, extend child minimal size. */
	if (this->flags & NC_EQUALSIZE) {
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			if (child_wid->fill_x == 1) child_wid->smallest_x = longest;
		}
	}
	/* 3. Move PIP space to the childs, compute smallest, fill, and resize values of the container. */
	if (this->head != NULL) this->head->padding_left += this->pip_pre;
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		if (child_wid->next != NULL) {
			child_wid->padding_right += this->pip_inter;
		} else {
			child_wid->padding_right += this->pip_post;
		}

		this->smallest_x += child_wid->smallest_x + child_wid->padding_left + child_wid->padding_right;
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

	uint additional_length = given_width - this->smallest_x; // Additional width given to us.
	this->StoreSizePosition(sizing, x, y, given_width, given_height);

	/* In principle, the additional horizontal space is distributed evenly over the available resizable childs. Due to step sizes, this may not always be feasible.
	 * To make resizing work as good as possible, first childs with biggest step sizes are done. These may get less due to rounding down.
	 * This additional space is then given to childs with smaller step sizes. This will give a good result when resize steps of each child is a multiple
	 * of the child with the smallest non-zero stepsize.
	 *
	 * Since child sizes are computed out of order, positions cannot be calculated until all sizes are known. That means it is not possible to compute the child
	 * size and position, and directly call child->AssignSizePosition() with the computed values.
	 * Instead, computed child widths and heights are stored in child->current_x and child->current_y values. That is allowed, since this method overwrites those values
	 * then we call the child.
	 */

	/* First loop: Find biggest stepsize, find number of childs that want a piece of the pie, handle vertical size for all childs, handle horizontal size for non-resizing childs. */
	int num_changing_childs = 0; // Number of childs that can change size.
	uint biggest_stepsize = 0;
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		uint hor_step = child_wid->GetHorizontalStepSize(sizing);
		if (hor_step > 0) {
			num_changing_childs++;
			biggest_stepsize = max(biggest_stepsize, hor_step);
		} else {
			child_wid->current_x = child_wid->smallest_x;
		}

		uint vert_step = (sizing == ST_SMALLEST) ? 1 : child_wid->GetVerticalStepSize(sizing);
		child_wid->current_y = ComputeMaxSize(child_wid->smallest_y, given_height - child_wid->padding_top - child_wid->padding_bottom, vert_step);
	}

	/* Second loop: Allocate the additional horizontal space over the resizing childs, starting with the biggest resize steps. */
	while (biggest_stepsize > 0) {
		uint next_biggest_stepsize = 0;
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
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
			next_biggest_stepsize = max(next_biggest_stepsize, hor_step);
		}
		biggest_stepsize = next_biggest_stepsize;
	}
	assert(num_changing_childs == 0);

	/* Third loop: Compute position and call the child. */
	uint position = 0; // Place to put next child relative to origin of the container.
	NWidgetBase *child_wid = rtl ? this->tail : this->head;
	while (child_wid != NULL) {
		uint child_width = child_wid->current_x;
		uint child_x = x + position + (rtl ? child_wid->padding_right : child_wid->padding_left);
		uint child_y = y + child_wid->padding_top;

		child_wid->AssignSizePosition(sizing, child_x, child_y, child_width, child_wid->current_y, rtl);
		position += child_width + child_wid->padding_right + child_wid->padding_left;

		child_wid = rtl ? child_wid->prev : child_wid->next;
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
	this->smallest_y = 0; // Sum of minimal size of all childs.
	this->fill_x = 1;     // smallest common child fill step.
	this->fill_y = 0;     // smallest non-zero child widget fill step.
	this->resize_x = 1;   // smallest common child resize step.
	this->resize_y = 0;   // smallest non-zero child widget resize step.

	/* 1a. Forward call, collect biggest nested array index, and longest/widest child length. */
	uint highest = 0; // Highest child found.
	uint max_hor_fill = 0; // Biggest horizontal fill step.
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		child_wid->SetupSmallestSize(w, init_array);
		highest = max(highest, child_wid->smallest_y);
		max_hor_fill = max(max_hor_fill, child_wid->GetHorizontalStepSize(ST_SMALLEST));
		this->smallest_x = max(this->smallest_x, child_wid->smallest_x + child_wid->padding_left + child_wid->padding_right);
	}
	/* 1b. Make the container wider if needed to accomadate all childs nicely. */
	uint max_smallest = this->smallest_x + 3 * max_hor_fill; // Upper limit to computing smallest height.
	uint cur_width = this->smallest_x;
	while (true) {
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			uint step_size = child_wid->GetHorizontalStepSize(ST_SMALLEST);
			uint child_width = child_wid->smallest_x + child_wid->padding_left + child_wid->padding_right;
			if (step_size > 1 && child_width < cur_width) { // Small step sizes or already fitting childs are not interesting.
				uint remainder = (cur_width - child_width) % step_size;
				if (remainder > 0) { // Child did not fit entirely, widen the container.
					cur_width += step_size - remainder;
					assert(cur_width < max_smallest); // Safeguard against infinite width expansion.
					/* Remaining childs will adapt to the new cur_width, thus speeding up the computation. */
				}
			}
		}
		if (this->smallest_x == cur_width) break;
		this->smallest_x = cur_width; // Smallest width got changed, try again.
	}
	/* 2. For containers that must maintain equal width, extend child minimal size. */
	if (this->flags & NC_EQUALSIZE) {
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			if (child_wid->fill_y == 1) child_wid->smallest_y = highest;
		}
	}
	/* 3. Move PIP space to the childs, compute smallest, fill, and resize values of the container. */
	if (this->head != NULL) this->head->padding_top += this->pip_pre;
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		if (child_wid->next != NULL) {
			child_wid->padding_bottom += this->pip_inter;
		} else {
			child_wid->padding_bottom += this->pip_post;
		}

		this->smallest_y += child_wid->smallest_y + child_wid->padding_top + child_wid->padding_bottom;
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

	int additional_length = given_height - this->smallest_y; // Additional height given to us.
	this->StoreSizePosition(sizing, x, y, given_width, given_height);

	/* Like the horizontal container, the vertical container also distributes additional height evenly, starting with the childs with the biggest resize steps.
	 * It also stores computed widths and heights into current_x and current_y values of the child.
	 */

	/* First loop: Find biggest stepsize, find number of childs that want a piece of the pie, handle horizontal size for all childs, handle vertical size for non-resizing childs. */
	int num_changing_childs = 0; // Number of childs that can change size.
	uint biggest_stepsize = 0;
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		uint vert_step = child_wid->GetVerticalStepSize(sizing);
		if (vert_step > 0) {
			num_changing_childs++;
			biggest_stepsize = max(biggest_stepsize, vert_step);
		} else {
			child_wid->current_y = child_wid->smallest_y;
		}

		uint hor_step = (sizing == ST_SMALLEST) ? 1 : child_wid->GetHorizontalStepSize(sizing);
		child_wid->current_x = ComputeMaxSize(child_wid->smallest_x, given_width - child_wid->padding_left - child_wid->padding_right, hor_step);
	}

	/* Second loop: Allocate the additional vertical space over the resizing childs, starting with the biggest resize steps. */
	while (biggest_stepsize > 0) {
		uint next_biggest_stepsize = 0;
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
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
			next_biggest_stepsize = max(next_biggest_stepsize, vert_step);
		}
		biggest_stepsize = next_biggest_stepsize;
	}
	assert(num_changing_childs == 0);

	/* Third loop: Compute position and call the child. */
	uint position = 0; // Place to put next child relative to origin of the container.
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		uint child_x = x + (rtl ? child_wid->padding_right : child_wid->padding_left);
		uint child_height = child_wid->current_y;

		child_wid->AssignSizePosition(sizing, child_x, y + position + child_wid->padding_top, child_wid->current_x, child_height, rtl);
		position += child_height + child_wid->padding_top + child_wid->padding_bottom;
	}
}

/**
 * Generic spacer widget.
 * @param length Horizontal size of the spacer widget.
 * @param height Vertical size of the spacer widget.
 */
NWidgetSpacer::NWidgetSpacer(int length, int height) : NWidgetResizeBase(NWID_SPACER, 0, 0)
{
	this->SetMinimalSize(length, height);
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
	return NULL;
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
	if (this->sb != NULL && this->widgets_x != 0) {
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

	if (this->sb == NULL || this->widgets_x == 0) return;

	/* We need to get the number of pixels the matrix is high/wide.
	 * So, determine the number of rows/columns based on the number of
	 * columns/rows (one is constant/unscrollable).
	 * Then multiply that by the height of a widget, and add the pre
	 * and post spacing "offsets". */
	count = CeilDiv(count, this->sb->IsVertical() ? this->widgets_x : this->widgets_y);
	count *= (this->sb->IsVertical() ? this->head->smallest_y : this->head->smallest_x) + this->pip_inter;
	count += -this->pip_inter + this->pip_pre + this->pip_post; // We counted an inter too much in the multiplication above
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
	assert(this->head != NULL);
	assert(this->head->next == NULL);

	if (this->index >= 0 && init_array) { // Fill w->nested_array[]
		assert(w->nested_array_size > (uint)this->index);
		w->nested_array[this->index] = this;
	}

	/* Reset the widget number. */
	SB(dynamic_cast<NWidgetCore *>(this->head)->index, 16, 16, 0);
	this->head->SetupSmallestSize(w, init_array);

	Dimension padding = {this->pip_pre + this->pip_post, this->pip_pre + this->pip_post};
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
	if (sizing == ST_RESIZE) {
		this->SetCount(this->count);
	}
}

void NWidgetMatrix::FillNestedArray(NWidgetBase **array, uint length)
{
	if (this->index >= 0 && (uint)(this->index) < length) array[this->index] = this;
	NWidgetContainer::FillNestedArray(array, length);
}

NWidgetCore *NWidgetMatrix::GetWidgetFromPos(int x, int y)
{
	/* Falls outside of the matrix widget. */
	if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return NULL;

	int start_x, start_y, base_offs_x, base_offs_y;
	this->GetScrollOffsets(start_x, start_y, base_offs_x, base_offs_y);

	/* Swap the x offset around for RTL, so it'll behave like LTR for RTL as well. */
	bool rtl = _current_text_dir == TD_RTL;
	if (rtl) base_offs_x -= (this->widgets_x - 1) * this->widget_w;

	int widget_col = (x - base_offs_x - (int)this->pip_pre - (int)this->pos_x) / this->widget_w;
	int widget_row = (y - base_offs_y - (int)this->pip_pre - (int)this->pos_y) / this->widget_h;

	if (widget_row * this->widgets_x + widget_col >= this->count) return NULL;

	NWidgetCore *child = dynamic_cast<NWidgetCore *>(this->head);
	child->AssignSizePosition(ST_RESIZE,
			this->pos_x + this->pip_pre + widget_col * this->widget_w + base_offs_x,
			this->pos_y + this->pip_pre + widget_row * this->widget_h + base_offs_y,
			child->smallest_x, child->smallest_y, rtl);

	/* "Undo" the RTL swap here to get the right widget index. */
	if (rtl) widget_col = this->widgets_x - widget_col - 1;
	SB(child->index, 16, 16, (widget_row + start_y) * this->widgets_x + widget_col + start_x);

	return child->GetWidgetFromPos(x, y);
}

/* virtual */ void NWidgetMatrix::Draw(const Window *w)
{
	/* Fill the background. */
	GfxFillRect(this->pos_x, this->pos_y, this->pos_x + this->current_x - 1, this->pos_y + this->current_y - 1, _colour_gradient[this->colour & 0xF][5]);

	/* Set up a clipping area for the previews. */
	DrawPixelInfo tmp_dpi;
	if (!FillDrawPixelInfo(&tmp_dpi, this->pos_x + this->pip_pre, this->pos_y + this->pip_pre, this->current_x - this->pip_pre - this->pip_post, this->current_y - this->pip_pre - this->pip_post)) return;
	DrawPixelInfo *old_dpi = _cur_dpi;
	_cur_dpi = &tmp_dpi;

	/* Get the appropriate offsets so we can draw the right widgets. */
	NWidgetCore *child = dynamic_cast<NWidgetCore *>(this->head);
	bool rtl = _current_text_dir == TD_RTL;
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
 * @param [out] start_x     The start position in columns,
 * @param [out] start_y     The start position in rows.
 * @param [out] base_offs_x The base horizontal offset in pixels.
 * @param [out] base_offs_y The base vertical offset in pixels.
 */
void NWidgetMatrix::GetScrollOffsets(int &start_x, int &start_y, int &base_offs_x, int &base_offs_y)
{
	base_offs_x = 0;
	base_offs_y = 0;
	start_x = 0;
	start_y = 0;
	if (this->sb != NULL) {
		if (this->sb->IsVertical()) {
			start_y = this->sb->GetPosition() / this->widget_h;
			base_offs_y = -this->sb->GetPosition() + start_y * this->widget_h;
			if (_current_text_dir == TD_RTL) base_offs_x = this->pip_pre + this->widget_w * (this->widgets_x - 1) - this->pip_inter;
		} else {
			start_x = this->sb->GetPosition() / this->widget_w;
			if (_current_text_dir == TD_RTL) {
				base_offs_x = this->sb->GetCapacity() + this->sb->GetPosition() - (start_x + 1) * this->widget_w + this->pip_inter - this->pip_post - this->pip_pre;
			} else {
				base_offs_x = -this->sb->GetPosition() + start_x * this->widget_w;
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
}

NWidgetBackground::~NWidgetBackground()
{
	if (this->child != NULL) delete this->child;
}

/**
 * Add a child to the parent.
 * @param nwid Nested widget to add to the background widget.
 *
 * Unless a child container has been given in the constructor, a parent behaves as a vertical container.
 * You can add several childs to it, and they are put underneath each other.
 */
void NWidgetBackground::Add(NWidgetBase *nwid)
{
	if (this->child == NULL) {
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
	if (this->child == NULL) {
		this->child = new NWidgetVertical();
	}
	this->child->SetPIP(pip_pre, pip_inter, pip_post);
}

void NWidgetBackground::SetupSmallestSize(Window *w, bool init_array)
{
	if (init_array && this->index >= 0) {
		assert(w->nested_array_size > (uint)this->index);
		w->nested_array[this->index] = this;
	}
	if (this->child != NULL) {
		this->child->SetupSmallestSize(w, init_array);

		this->smallest_x = this->child->smallest_x;
		this->smallest_y = this->child->smallest_y;
		this->fill_x = this->child->fill_x;
		this->fill_y = this->child->fill_y;
		this->resize_x = this->child->resize_x;
		this->resize_y = this->child->resize_y;

		/* Account for the size of the frame's text if that exists */
		if (w != NULL && this->type == WWT_FRAME) {
			this->child->padding_left   = WD_FRAMETEXT_LEFT;
			this->child->padding_right  = WD_FRAMETEXT_RIGHT;
			this->child->padding_top    = max((int)WD_FRAMETEXT_TOP, this->widget_data != STR_NULL ? FONT_HEIGHT_NORMAL + WD_FRAMETEXT_TOP / 2 : 0);
			this->child->padding_bottom = WD_FRAMETEXT_BOTTOM;

			this->smallest_x += this->child->padding_left + this->child->padding_right;
			this->smallest_y += this->child->padding_top + this->child->padding_bottom;

			if (this->index >= 0) w->SetStringParameters(this->index);
			this->smallest_x = max(this->smallest_x, GetStringBoundingBox(this->widget_data).width + WD_FRAMETEXT_LEFT + WD_FRAMETEXT_RIGHT);
		}
	} else {
		Dimension d = {this->min_x, this->min_y};
		Dimension fill = {this->fill_x, this->fill_y};
		Dimension resize  = {this->resize_x, this->resize_y};
		if (w != NULL) { // A non-NULL window pointer acts as switch to turn dynamic widget size on.
			if (this->type == WWT_FRAME || this->type == WWT_INSET) {
				if (this->index >= 0) w->SetStringParameters(this->index);
				Dimension background = GetStringBoundingBox(this->widget_data);
				background.width += (this->type == WWT_FRAME) ? (WD_FRAMETEXT_LEFT + WD_FRAMERECT_RIGHT) : (WD_INSET_LEFT + WD_INSET_RIGHT);
				d = maxdim(d, background);
			}
			if (this->index >= 0) {
				static const Dimension padding = {0, 0};
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

	if (this->child != NULL) {
		uint x_offset = (rtl ? this->child->padding_right : this->child->padding_left);
		uint width = given_width - this->child->padding_right - this->child->padding_left;
		uint height = given_height - this->child->padding_top - this->child->padding_bottom;
		this->child->AssignSizePosition(sizing, x + x_offset, y + this->child->padding_top, width, height, rtl);
	}
}

void NWidgetBackground::FillNestedArray(NWidgetBase **array, uint length)
{
	if (this->index >= 0 && (uint)(this->index) < length) array[this->index] = this;
	if (this->child != NULL) this->child->FillNestedArray(array, length);
}

void NWidgetBackground::Draw(const Window *w)
{
	if (this->current_x == 0 || this->current_y == 0) return;

	Rect r;
	r.left = this->pos_x;
	r.right = this->pos_x + this->current_x - 1;
	r.top = this->pos_y;
	r.bottom = this->pos_y + this->current_y - 1;

	const DrawPixelInfo *dpi = _cur_dpi;
	if (dpi->left > r.right || dpi->left + dpi->width <= r.left || dpi->top > r.bottom || dpi->top + dpi->height <= r.top) return;

	switch (this->type) {
		case WWT_PANEL:
			assert(this->widget_data == 0);
			DrawFrameRect(r.left, r.top, r.right, r.bottom, this->colour, this->IsLowered() ? FR_LOWERED : FR_NONE);
			break;

		case WWT_FRAME:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawFrame(r, this->colour, this->widget_data);
			break;

		case WWT_INSET:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawInset(r, this->colour, this->widget_data);
			break;

		default:
			NOT_REACHED();
	}

	if (this->index >= 0) w->DrawWidget(r, this->index);
	if (this->child != NULL) this->child->Draw(w);

	if (this->IsDisabled()) {
		GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, _colour_gradient[this->colour & 0xF][2], FILLRECT_CHECKER);
	}
}

NWidgetCore *NWidgetBackground::GetWidgetFromPos(int x, int y)
{
	NWidgetCore *nwid = NULL;
	if (IsInsideBS(x, this->pos_x, this->current_x) && IsInsideBS(y, this->pos_y, this->current_y)) {
		if (this->child != NULL) nwid = this->child->GetWidgetFromPos(x, y);
		if (nwid == NULL) nwid = this;
	}
	return nwid;
}

NWidgetBase *NWidgetBackground::GetWidgetOfType(WidgetType tp)
{
	NWidgetBase *nwid = NULL;
	if (this->child != NULL) nwid = this->child->GetWidgetOfType(tp);
	if (nwid == NULL && this->type == tp) nwid = this;
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
				(this->disp_flags & ND_SHADE_DIMMED) ? PALETTE_TO_TRANSPARENT : PALETTE_TO_STRUCT_GREY, FILLRECT_RECOLOUR);
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
	ViewPort *vp = w->viewport;
	if (vp != NULL) {
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
 * @param widget      Widget number of the widget clicked in.
 * @param padding     Amount of empty space between the widget edge and the top of the first row. Default value is \c 0.
 * @param line_height Height of a single row. A negative value means using the vertical resize step of the widget.
 * @return Row number clicked at. If clicked at a wrong position, #INT_MAX is returned.
 */
int Scrollbar::GetScrolledRowFromWidget(int clickpos, const Window * const w, int widget, int padding, int line_height) const
{
	uint pos = w->GetRowFromWidget(clickpos, widget, padding, line_height);
	if (pos != INT_MAX) pos += this->GetPosition();
	return (pos >= this->GetCount()) ? INT_MAX : pos;
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
			this->SetMinimalSize(0, WD_HSCROLLBAR_HEIGHT);
			this->SetResize(1, 0);
			this->SetFill(1, 0);
			this->SetDataTip(0x0, STR_TOOLTIP_HSCROLL_BAR_SCROLLS_LIST);
			break;

		case NWID_VSCROLLBAR:
			this->SetMinimalSize(WD_VSCROLLBAR_WIDTH, 0);
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
	this->smallest_x = this->min_x;
	this->smallest_y = this->min_y;
}

void NWidgetScrollbar::Draw(const Window *w)
{
	if (this->current_x == 0 || this->current_y == 0) return;

	Rect r;
	r.left = this->pos_x;
	r.right = this->pos_x + this->current_x - 1;
	r.top = this->pos_y;
	r.bottom = this->pos_y + this->current_y - 1;

	const DrawPixelInfo *dpi = _cur_dpi;
	if (dpi->left > r.right || dpi->left + dpi->width <= r.left || dpi->top > r.bottom || dpi->top + dpi->height <= r.top) return;

	bool up_lowered = HasBit(this->disp_flags, NDB_SCROLLBAR_UP);
	bool down_lowered = HasBit(this->disp_flags, NDB_SCROLLBAR_DOWN);
	bool middle_lowered = !(this->disp_flags & ND_SCROLLBAR_BTN) && w->scrolling_scrollbar == this->index;

	if (this->type == NWID_HSCROLLBAR) {
		DrawHorizontalScrollbar(r, this->colour, up_lowered, middle_lowered, down_lowered, this);
	} else {
		DrawVerticalScrollbar(r, this->colour, up_lowered, middle_lowered, down_lowered, this);
	}

	if (this->IsDisabled()) {
		GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, _colour_gradient[this->colour & 0xF][2], FILLRECT_CHECKER);
	}
}

/** Reset the cached dimensions. */
/* static */ void NWidgetLeaf::InvalidateDimensionCache()
{
	shadebox_dimension.width  = shadebox_dimension.height  = 0;
	debugbox_dimension.width  = debugbox_dimension.height  = 0;
	stickybox_dimension.width = stickybox_dimension.height = 0;
	resizebox_dimension.width = resizebox_dimension.height = 0;
	closebox_dimension.width  = closebox_dimension.height  = 0;
}

Dimension NWidgetLeaf::shadebox_dimension  = {0, 0};
Dimension NWidgetLeaf::debugbox_dimension  = {0, 0};
Dimension NWidgetLeaf::stickybox_dimension = {0, 0};
Dimension NWidgetLeaf::resizebox_dimension = {0, 0};
Dimension NWidgetLeaf::closebox_dimension  = {0, 0};

/**
 * Nested leaf widget.
 * @param tp     Type of leaf widget.
 * @param colour Colour of the leaf widget.
 * @param index  Index in the widget array used by the window system.
 * @param data   Data of the widget.
 * @param tip    Tooltip of the widget.
 */
NWidgetLeaf::NWidgetLeaf(WidgetType tp, Colours colour, int index, uint16 data, StringID tip) : NWidgetCore(tp, colour, 1, 1, data, tip)
{
	assert(index >= 0 || tp == WWT_LABEL || tp == WWT_TEXT || tp == WWT_CAPTION || tp == WWT_RESIZEBOX || tp == WWT_SHADEBOX || tp == WWT_DEBUGBOX || tp == WWT_STICKYBOX || tp == WWT_CLOSEBOX);
	if (index >= 0) this->SetIndex(index);
	this->SetMinimalSize(0, 0);
	this->SetResize(0, 0);

	switch (tp) {
		case WWT_EMPTY:
			break;

		case WWT_PUSHBTN:
		case WWT_IMGBTN:
		case WWT_PUSHIMGBTN:
		case WWT_IMGBTN_2:
		case WWT_TEXTBTN:
		case WWT_PUSHTXTBTN:
		case WWT_TEXTBTN_2:
		case WWT_LABEL:
		case WWT_TEXT:
		case WWT_MATRIX:
		case NWID_BUTTON_DROPDOWN:
		case WWT_ARROWBTN:
		case WWT_PUSHARROWBTN:
			this->SetFill(0, 0);
			break;

		case WWT_EDITBOX:
			this->SetMinimalSize(10, 0);
			this->SetFill(0, 0);
			break;

		case WWT_CAPTION:
			this->SetFill(1, 0);
			this->SetResize(1, 0);
			this->min_y = WD_CAPTION_HEIGHT;
			this->SetDataTip(data, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS);
			break;

		case WWT_STICKYBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WD_STICKYBOX_WIDTH, 14);
			this->SetDataTip(STR_NULL, STR_TOOLTIP_STICKY);
			break;

		case WWT_SHADEBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WD_SHADEBOX_TOP, 14);
			this->SetDataTip(STR_NULL, STR_TOOLTIP_SHADE);
			break;

		case WWT_DEBUGBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WD_DEBUGBOX_TOP, 14);
			this->SetDataTip(STR_NULL, STR_TOOLTIP_DEBUG);
			break;

		case WWT_RESIZEBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WD_RESIZEBOX_WIDTH, 12);
			this->SetDataTip(STR_NULL, STR_TOOLTIP_RESIZE);
			break;

		case WWT_CLOSEBOX:
			this->SetFill(0, 0);
			this->SetMinimalSize(WD_CLOSEBOX_WIDTH, 14);
			this->SetDataTip(STR_BLACK_CROSS, STR_TOOLTIP_CLOSE_WINDOW);
			break;

		case WWT_DROPDOWN:
			this->SetFill(0, 0);
			this->min_y = WD_DROPDOWN_HEIGHT;
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

	Dimension size = {this->min_x, this->min_y};
	Dimension fill = {this->fill_x, this->fill_y};
	Dimension resize = {this->resize_x, this->resize_y};
	/* Get padding, and update size with the real content size if appropriate. */
	const Dimension *padding = NULL;
	switch (this->type) {
		case WWT_EMPTY: {
			static const Dimension extra = {0, 0};
			padding = &extra;
			break;
		}
		case WWT_MATRIX: {
			static const Dimension extra = {WD_MATRIX_LEFT + WD_MATRIX_RIGHT, WD_MATRIX_TOP + WD_MATRIX_BOTTOM};
			padding = &extra;
			break;
		}
		case WWT_SHADEBOX: {
			static const Dimension extra = {WD_SHADEBOX_LEFT + WD_SHADEBOX_RIGHT, WD_SHADEBOX_TOP + WD_SHADEBOX_BOTTOM};
			padding = &extra;
			if (NWidgetLeaf::shadebox_dimension.width == 0) {
				NWidgetLeaf::shadebox_dimension = maxdim(GetSpriteSize(SPR_WINDOW_SHADE), GetSpriteSize(SPR_WINDOW_UNSHADE));
				NWidgetLeaf::shadebox_dimension.width += extra.width;
				NWidgetLeaf::shadebox_dimension.height += extra.height;
			}
			size = maxdim(size, NWidgetLeaf::shadebox_dimension);
			break;
		}
		case WWT_DEBUGBOX:
			if (_settings_client.gui.newgrf_developer_tools && w->IsNewGRFInspectable()) {
				static const Dimension extra = {WD_DEBUGBOX_LEFT + WD_DEBUGBOX_RIGHT, WD_DEBUGBOX_TOP + WD_DEBUGBOX_BOTTOM};
				padding = &extra;
				if (NWidgetLeaf::debugbox_dimension.width == 0) {
					NWidgetLeaf::debugbox_dimension = GetSpriteSize(SPR_WINDOW_DEBUG);
					NWidgetLeaf::debugbox_dimension.width += extra.width;
					NWidgetLeaf::debugbox_dimension.height += extra.height;
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
			static const Dimension extra = {WD_STICKYBOX_LEFT + WD_STICKYBOX_RIGHT, WD_STICKYBOX_TOP + WD_STICKYBOX_BOTTOM};
			padding = &extra;
			if (NWidgetLeaf::stickybox_dimension.width == 0) {
				NWidgetLeaf::stickybox_dimension = maxdim(GetSpriteSize(SPR_PIN_UP), GetSpriteSize(SPR_PIN_DOWN));
				NWidgetLeaf::stickybox_dimension.width += extra.width;
				NWidgetLeaf::stickybox_dimension.height += extra.height;
			}
			size = maxdim(size, NWidgetLeaf::stickybox_dimension);
			break;
		}
		case WWT_RESIZEBOX: {
			static const Dimension extra = {WD_RESIZEBOX_LEFT + WD_RESIZEBOX_RIGHT, WD_RESIZEBOX_TOP + WD_RESIZEBOX_BOTTOM};
			padding = &extra;
			if (NWidgetLeaf::resizebox_dimension.width == 0) {
				NWidgetLeaf::resizebox_dimension = maxdim(GetSpriteSize(SPR_WINDOW_RESIZE_LEFT), GetSpriteSize(SPR_WINDOW_RESIZE_RIGHT));
				NWidgetLeaf::resizebox_dimension.width += extra.width;
				NWidgetLeaf::resizebox_dimension.height += extra.height;
			}
			size = maxdim(size, NWidgetLeaf::resizebox_dimension);
			break;
		}
		case WWT_EDITBOX:
			size.height = max(size.height, GetStringBoundingBox("_").height + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM);
			/* FALL THROUGH */
		case WWT_PUSHBTN: {
			static const Dimension extra = {WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM};
			padding = &extra;
			break;
		}
		case WWT_IMGBTN:
		case WWT_IMGBTN_2:
		case WWT_PUSHIMGBTN: {
			static const Dimension extra = {WD_IMGBTN_LEFT + WD_IMGBTN_RIGHT,  WD_IMGBTN_TOP + WD_IMGBTN_BOTTOM};
			padding = &extra;
			Dimension d2 = GetSpriteSize(this->widget_data);
			if (this->type == WWT_IMGBTN_2) d2 = maxdim(d2, GetSpriteSize(this->widget_data + 1));
			d2.width += extra.width;
			d2.height += extra.height;
			size = maxdim(size, d2);
			break;
		}
		case WWT_ARROWBTN:
		case WWT_PUSHARROWBTN: {
			static const Dimension extra = {WD_IMGBTN_LEFT + WD_IMGBTN_RIGHT,  WD_IMGBTN_TOP + WD_IMGBTN_BOTTOM};
			padding = &extra;
			Dimension d2 = maxdim(GetSpriteSize(SPR_ARROW_LEFT), GetSpriteSize(SPR_ARROW_RIGHT));
			d2.width += extra.width;
			d2.height += extra.height;
			size = maxdim(size, d2);
			break;
		}

		case WWT_CLOSEBOX: {
			static const Dimension extra = {WD_CLOSEBOX_LEFT + WD_CLOSEBOX_RIGHT, WD_CLOSEBOX_TOP + WD_CLOSEBOX_BOTTOM};
			padding = &extra;
			if (NWidgetLeaf::closebox_dimension.width == 0) {
				NWidgetLeaf::closebox_dimension = maxdim(GetStringBoundingBox(STR_BLACK_CROSS), GetStringBoundingBox(STR_SILVER_CROSS));
				NWidgetLeaf::closebox_dimension.width += extra.width;
				NWidgetLeaf::closebox_dimension.height += extra.height;
			}
			size = maxdim(size, NWidgetLeaf::closebox_dimension);
			break;
		}
		case WWT_TEXTBTN:
		case WWT_PUSHTXTBTN:
		case WWT_TEXTBTN_2: {
			static const Dimension extra = {WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT,  WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM};
			padding = &extra;
			if (this->index >= 0) w->SetStringParameters(this->index);
			Dimension d2 = GetStringBoundingBox(this->widget_data);
			d2.width += extra.width;
			d2.height += extra.height;
			size = maxdim(size, d2);
			break;
		}
		case WWT_LABEL:
		case WWT_TEXT: {
			static const Dimension extra = {0, 0};
			padding = &extra;
			if (this->index >= 0) w->SetStringParameters(this->index);
			size = maxdim(size, GetStringBoundingBox(this->widget_data));
			break;
		}
		case WWT_CAPTION: {
			static const Dimension extra = {WD_CAPTIONTEXT_LEFT + WD_CAPTIONTEXT_RIGHT, WD_CAPTIONTEXT_TOP + WD_CAPTIONTEXT_BOTTOM};
			padding = &extra;
			if (this->index >= 0) w->SetStringParameters(this->index);
			Dimension d2 = GetStringBoundingBox(this->widget_data);
			d2.width += extra.width;
			d2.height += extra.height;
			size = maxdim(size, d2);
			break;
		}
		case WWT_DROPDOWN:
		case NWID_BUTTON_DROPDOWN: {
			static const Dimension extra = {WD_DROPDOWNTEXT_LEFT + WD_DROPDOWNTEXT_RIGHT, WD_DROPDOWNTEXT_TOP + WD_DROPDOWNTEXT_BOTTOM};
			padding = &extra;
			if (this->index >= 0) w->SetStringParameters(this->index);
			Dimension d2 = GetStringBoundingBox(this->widget_data);
			d2.width += extra.width;
			d2.height += extra.height;
			size = maxdim(size, d2);
			break;
		}
		default:
			NOT_REACHED();
	}

	if (this->index >= 0) w->UpdateWidgetSize(this->index, &size, *padding, &fill, &resize);

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

	Rect r;
	r.left = this->pos_x;
	r.right = this->pos_x + this->current_x - 1;
	r.top = this->pos_y;
	r.bottom = this->pos_y + this->current_y - 1;

	const DrawPixelInfo *dpi = _cur_dpi;
	if (dpi->left > r.right || dpi->left + dpi->width <= r.left || dpi->top > r.bottom || dpi->top + dpi->height <= r.top) return;

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
			DrawImageButtons(r, this->type, this->colour, clicked, this->widget_data);
			break;

		case WWT_TEXTBTN:
		case WWT_PUSHTXTBTN:
		case WWT_TEXTBTN_2:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawFrameRect(r.left, r.top, r.right, r.bottom, this->colour, (clicked) ? FR_LOWERED : FR_NONE);
			DrawLabel(r, this->type, clicked, this->widget_data);
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
			DrawImageButtons(r, WWT_PUSHIMGBTN, this->colour, clicked, sprite);
		}

		case WWT_LABEL:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawLabel(r, this->type, clicked, this->widget_data);
			break;

		case WWT_TEXT:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawText(r, (TextColour)this->colour, this->widget_data);
			break;

		case WWT_MATRIX:
			DrawMatrix(r, this->colour, clicked, this->widget_data);
			break;

		case WWT_EDITBOX:
			DrawFrameRect(r.left, r.top, r.right, r.bottom, this->colour, FR_LOWERED | FR_DARKENED);
			break;

		case WWT_CAPTION:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawCaption(r, this->colour, w->owner, this->widget_data);
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
			DrawStickyBox(r, this->colour, !!(w->flags4 & WF_STICKY));
			break;

		case WWT_RESIZEBOX:
			assert(this->widget_data == 0);
			DrawResizeBox(r, this->colour, this->pos_x < (uint)(w->width / 2), !!(w->flags4 & WF_SIZING));
			break;

		case WWT_CLOSEBOX:
			DrawCloseBox(r, this->colour, this->widget_data);
			break;

		case WWT_DROPDOWN:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawDropdown(r, this->colour, clicked, this->widget_data);
			break;

		case NWID_BUTTON_DROPDOWN:
			if (this->index >= 0) w->SetStringParameters(this->index);
			DrawButtonDropdown(r, this->colour, clicked, (this->disp_flags & ND_DROPDOWN_ACTIVE) != 0, this->widget_data);
			break;

		default:
			NOT_REACHED();
	}
	if (this->index >= 0) w->DrawWidget(r, this->index);

	if (this->IsDisabled()) {
		GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, _colour_gradient[this->colour & 0xF][2], FILLRECT_CHECKER);
	}
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
		int button_width = this->pos_x + this->current_x - 12;
		return pt.x < button_width;
	} else {
		int button_left = this->pos_x + 12;
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
 * @pre \c biggest_index != NULL.
 */
static int MakeNWidget(const NWidgetPart *parts, int count, NWidgetBase **dest, bool *fill_dest, int *biggest_index)
{
	int num_used = 0;

	*dest = NULL;
	*fill_dest = false;

	while (count > num_used) {
		switch (parts->type) {
			case NWID_SPACER:
				if (*dest != NULL) return num_used;
				*dest = new NWidgetSpacer(0, 0);
				break;

			case NWID_HORIZONTAL:
				if (*dest != NULL) return num_used;
				*dest = new NWidgetHorizontal(parts->u.cont_flags);
				*fill_dest = true;
				break;

			case NWID_HORIZONTAL_LTR:
				if (*dest != NULL) return num_used;
				*dest = new NWidgetHorizontalLTR(parts->u.cont_flags);
				*fill_dest = true;
				break;

			case WWT_PANEL:
			case WWT_INSET:
			case WWT_FRAME:
				if (*dest != NULL) return num_used;
				*dest = new NWidgetBackground(parts->type, parts->u.widget.colour, parts->u.widget.index);
				*biggest_index = max(*biggest_index, (int)parts->u.widget.index);
				*fill_dest = true;
				break;

			case NWID_VERTICAL:
				if (*dest != NULL) return num_used;
				*dest = new NWidgetVertical(parts->u.cont_flags);
				*fill_dest = true;
				break;

			case NWID_MATRIX: {
				if (*dest != NULL) return num_used;
				NWidgetMatrix *nwm = new NWidgetMatrix();
				*dest = nwm;
				*fill_dest = true;
				nwm->SetIndex(parts->u.widget.index);
				nwm->SetColour(parts->u.widget.colour);
				*biggest_index = max(*biggest_index, (int)parts->u.widget.index);
				break;
			}

			case WPT_FUNCTION: {
				if (*dest != NULL) return num_used;
				/* Ensure proper functioning even when the called code simply writes its largest index. */
				int biggest = -1;
				*dest = parts->u.func_ptr(&biggest);
				*biggest_index = max(*biggest_index, biggest);
				*fill_dest = false;
				break;
			}

			case WPT_RESIZE: {
				NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(*dest);
				if (nwrb != NULL) {
					assert(parts->u.xy.x >= 0 && parts->u.xy.y >= 0);
					nwrb->SetResize(parts->u.xy.x, parts->u.xy.y);
				}
				break;
			}

			case WPT_MINSIZE: {
				NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(*dest);
				if (nwrb != NULL) {
					assert(parts->u.xy.x >= 0 && parts->u.xy.y >= 0);
					nwrb->SetMinimalSize(parts->u.xy.x, parts->u.xy.y);
				}
				break;
			}

			case WPT_MINTEXTLINES: {
				NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(*dest);
				if (nwrb != NULL) {
					assert(parts->u.text_lines.size >= FS_BEGIN && parts->u.text_lines.size < FS_END);
					nwrb->SetMinimalTextLines(parts->u.text_lines.lines, parts->u.text_lines.spacing, parts->u.text_lines.size);
				}
				break;
			}

			case WPT_FILL: {
				NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(*dest);
				if (nwrb != NULL) nwrb->SetFill(parts->u.xy.x, parts->u.xy.y);
				break;
			}

			case WPT_DATATIP: {
				NWidgetCore *nwc = dynamic_cast<NWidgetCore *>(*dest);
				if (nwc != NULL) {
					nwc->widget_data = parts->u.data_tip.data;
					nwc->tool_tip = parts->u.data_tip.tooltip;
				}
				break;
			}

			case WPT_PADDING:
				if (*dest != NULL) (*dest)->SetPadding(parts->u.padding.top, parts->u.padding.right, parts->u.padding.bottom, parts->u.padding.left);
				break;

			case WPT_PIPSPACE: {
				NWidgetPIPContainer *nwc = dynamic_cast<NWidgetPIPContainer *>(*dest);
				if (nwc != NULL) nwc->SetPIP(parts->u.pip.pre,  parts->u.pip.inter, parts->u.pip.post);

				NWidgetBackground *nwb = dynamic_cast<NWidgetBackground *>(*dest);
				if (nwb != NULL) nwb->SetPIP(parts->u.pip.pre,  parts->u.pip.inter, parts->u.pip.post);
				break;
			}

			case WPT_SCROLLBAR: {
				NWidgetCore *nwc = dynamic_cast<NWidgetCore *>(*dest);
				if (nwc != NULL) {
					nwc->scrollbar_index = parts->u.widget.index;
				}
				break;
			}

			case WPT_ENDCONTAINER:
				return num_used;

			case NWID_VIEWPORT:
				if (*dest != NULL) return num_used;
				*dest = new NWidgetViewport(parts->u.widget.index);
				*biggest_index = max(*biggest_index, (int)parts->u.widget.index);
				break;

			case NWID_HSCROLLBAR:
			case NWID_VSCROLLBAR:
				if (*dest != NULL) return num_used;
				*dest = new NWidgetScrollbar(parts->type, parts->u.widget.colour, parts->u.widget.index);
				*biggest_index = max(*biggest_index, (int)parts->u.widget.index);
				break;

			case NWID_SELECTION: {
				if (*dest != NULL) return num_used;
				NWidgetStacked *nws = new NWidgetStacked();
				*dest = nws;
				*fill_dest = true;
				nws->SetIndex(parts->u.widget.index);
				*biggest_index = max(*biggest_index, (int)parts->u.widget.index);
				break;
			}

			default:
				if (*dest != NULL) return num_used;
				assert((parts->type & WWT_MASK) < WWT_LAST || parts->type == NWID_BUTTON_DROPDOWN);
				*dest = new NWidgetLeaf(parts->type, parts->u.widget.colour, parts->u.widget.index, 0x0, STR_NULL);
				*biggest_index = max(*biggest_index, (int)parts->u.widget.index);
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
 * @param parent Pointer or container to use for storing the child widgets (*parent == NULL or *parent == container or background widget).
 * @param biggest_index Pointer to biggest nested widget index in the tree.
 * @return Number of widget part elements used to fill the container.
 * @post \c *biggest_index contains the largest widget index of the tree and \c -1 if no index is used.
 */
static int MakeWidgetTree(const NWidgetPart *parts, int count, NWidgetBase **parent, int *biggest_index)
{
	/* If *parent == NULL, only the first widget is read and returned. Otherwise, *parent must point to either
	 * a #NWidgetContainer or a #NWidgetBackground object, and parts are added as much as possible. */
	NWidgetContainer *nwid_cont = dynamic_cast<NWidgetContainer *>(*parent);
	NWidgetBackground *nwid_parent = dynamic_cast<NWidgetBackground *>(*parent);
	assert(*parent == NULL || (nwid_cont != NULL && nwid_parent == NULL) || (nwid_cont == NULL && nwid_parent != NULL));

	int total_used = 0;
	while (true) {
		NWidgetBase *sub_widget = NULL;
		bool fill_sub = false;
		int num_used = MakeNWidget(parts, count - total_used, &sub_widget, &fill_sub, biggest_index);
		parts += num_used;
		total_used += num_used;

		/* Break out of loop when end reached */
		if (sub_widget == NULL) break;

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
		if (nwid_cont) nwid_cont->Add(sub_widget);
		if (nwid_parent) nwid_parent->Add(sub_widget);
		if (!nwid_cont && !nwid_parent) {
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
 * @param container Container to add the nested widgets to. In case it is NULL a vertical container is used.
 * @return Root of the nested widget tree, a vertical container containing the entire GUI.
 * @ingroup NestedWidgetParts
 * @pre \c biggest_index != NULL
 * @post \c *biggest_index contains the largest widget index of the tree and \c -1 if no index is used.
 */
NWidgetContainer *MakeNWidgets(const NWidgetPart *parts, int count, int *biggest_index, NWidgetContainer *container)
{
	*biggest_index = -1;
	if (container == NULL) container = new NWidgetVertical();
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
 * @param [out] shade_select Pointer to the inserted shade selection widget (\c NULL if not unserted).
 * @return Root of the nested widget tree, a vertical container containing the entire GUI.
 * @ingroup NestedWidgetParts
 * @pre \c biggest_index != NULL
 * @post \c *biggest_index contains the largest widget index of the tree and \c -1 if no index is used.
 */
NWidgetContainer *MakeWindowNWidgetTree(const NWidgetPart *parts, int count, int *biggest_index, NWidgetStacked **shade_select)
{
	*biggest_index = -1;

	/* Read the first widget recursively from the array. */
	NWidgetBase *nwid = NULL;
	int num_used = MakeWidgetTree(parts, count, &nwid, biggest_index);
	assert(nwid != NULL);
	parts += num_used;
	count -= num_used;

	NWidgetContainer *root = new NWidgetVertical;
	root->Add(nwid);
	if (count == 0) { // There is no body at all.
		*shade_select = NULL;
		return root;
	}

	/* If the first widget looks like a titlebar, treat it as such.
	 * If it has a shading box, silently add a shade selection widget in the tree. */
	NWidgetHorizontal *hor_cont = dynamic_cast<NWidgetHorizontal *>(nwid);
	NWidgetContainer *body;
	if (hor_cont != NULL && hor_cont->GetWidgetOfType(WWT_CAPTION) != NULL && hor_cont->GetWidgetOfType(WWT_SHADEBOX) != NULL) {
		*shade_select = new NWidgetStacked;
		root->Add(*shade_select);
		body = new NWidgetVertical;
		(*shade_select)->Add(body);
	} else {
		*shade_select = NULL;
		body = root;
	}

	/* Load the remaining parts into 'body'. */
	int biggest2 = -1;
	MakeNWidgets(parts, count, &biggest2, body);

	*biggest_index = max(*biggest_index, biggest2);
	return root;
}

/**
 * Make a number of rows with button-like graphics, for enabling/disabling each company.
 * @param biggest_index Storage for collecting the biggest index used in the returned tree.
 * @param widget_first The first widget index to use.
 * @param widget_last The last widget index to use.
 * @param max_length Maximal number of company buttons in one row.
 * @param button_tooltip The tooltip-string of every button.
 * @return Panel with rows of company buttons.
 * @post \c *biggest_index contains the largest used index in the tree.
 */
NWidgetBase *MakeCompanyButtonRows(int *biggest_index, int widget_first, int widget_last, int max_length, StringID button_tooltip)
{
	NWidgetVertical *vert = NULL; // Storage for all rows.
	NWidgetHorizontal *hor = NULL; // Storage for buttons in one row.
	int hor_length = 0;

	Dimension sprite_size = GetSpriteSize(SPR_COMPANY_ICON);
	sprite_size.width  += WD_MATRIX_LEFT + WD_MATRIX_RIGHT;
	sprite_size.height += WD_MATRIX_TOP + WD_MATRIX_BOTTOM + 1; // 1 for the 'offset' of being pressed

	for (int widnum = widget_first; widnum <= widget_last; widnum++) {
		/* Ensure there is room in 'hor' for another button. */
		if (hor_length == max_length) {
			if (vert == NULL) vert = new NWidgetVertical();
			vert->Add(hor);
			hor = NULL;
			hor_length = 0;
		}
		if (hor == NULL) {
			hor = new NWidgetHorizontal();
			hor_length = 0;
		}

		NWidgetBackground *panel = new NWidgetBackground(WWT_PANEL, COLOUR_GREY, widnum);
		panel->SetMinimalSize(sprite_size.width, sprite_size.height);
		panel->SetFill(1, 0);
		panel->SetResize(1, 0);
		panel->SetDataTip(0x0, button_tooltip);
		hor->Add(panel);
		hor_length++;
	}
	*biggest_index = widget_last;
	if (vert == NULL) return hor; // All buttons fit in a single row.

	if (hor_length > 0 && hor_length < max_length) {
		/* Last row is partial, add a spacer at the end to force all buttons to the left. */
		NWidgetSpacer *spc = new NWidgetSpacer(sprite_size.width, sprite_size.height);
		spc->SetFill(1, 0);
		spc->SetResize(1, 0);
		hor->Add(spc);
	}
	if (hor != NULL) vert->Add(hor);
	return vert;
}
