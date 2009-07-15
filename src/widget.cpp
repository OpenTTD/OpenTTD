/* $Id$ */

/** @file widget.cpp Handling of the default/simple widgets. */

#include "stdafx.h"
#include "company_func.h"
#include "gfx_func.h"
#include "window_gui.h"
#include "debug.h"
#include "strings_func.h"

#include "table/sprites.h"
#include "table/strings.h"

static const char *UPARROW   = "\xEE\x8A\xA0"; ///< String containing an upwards pointing arrow.
static const char *DOWNARROW = "\xEE\x8A\xAA"; ///< String containing a downwards pointing arrow.

/**
 * Compute the vertical position of the draggable part of scrollbar
 * @param sb     Scrollbar list data
 * @param top    Top position of the scrollbar (top position of the up-button)
 * @param bottom Bottom position of the scrollbar (bottom position of the down-button)
 * @return A Point, with x containing the top coordinate of the draggable part, and
 *                       y containing the bottom coordinate of the draggable part
 */
static Point HandleScrollbarHittest(const Scrollbar *sb, int top, int bottom)
{
	Point pt;
	int height, count, pos, cap;

	top += 10;   // top    points to just below the up-button
	bottom -= 9; // bottom points to top of the down-button

	height = (bottom - top);

	pos = sb->pos;
	count = sb->count;
	cap = sb->cap;

	if (count != 0) top += height * pos / count;

	if (cap > count) cap = count;
	if (count != 0) bottom -= (count - pos - cap) * height / count;

	pt.x = top;
	pt.y = bottom - 1;
	return pt;
}

/**
 * Compute new position of the scrollbar after a click and updates the window flags.
 * @param w   Window on which a scroll was performed.
 * @param wtp Scrollbar widget type.
 * @param mi  Minimum coordinate of the scroll bar.
 * @param ma  Maximum coordinate of the scroll bar.
 * @param x   The X coordinate of the mouse click.
 * @param y   The Y coordinate of the mouse click.
 */
static void ScrollbarClickPositioning(Window *w, WidgetType wtp, int x, int y, int mi, int ma)
{
	int pos;
	Scrollbar *sb;

	switch (wtp) {
		case WWT_SCROLLBAR:
			/* vertical scroller */
			w->flags4 &= ~WF_HSCROLL;
			w->flags4 &= ~WF_SCROLL2;
			pos = y;
			sb = &w->vscroll;
			break;

		case WWT_SCROLL2BAR:
			/* 2nd vertical scroller */
			w->flags4 &= ~WF_HSCROLL;
			w->flags4 |= WF_SCROLL2;
			pos = y;
			sb = &w->vscroll2;
			break;

		case  WWT_HSCROLLBAR:
			/* horizontal scroller */
			w->flags4 &= ~WF_SCROLL2;
			w->flags4 |= WF_HSCROLL;
			pos = x;
			sb = &w->hscroll;
			break;

		default: NOT_REACHED();
	}
	if (pos <= mi + 9) {
		/* Pressing the upper button? */
		w->flags4 |= WF_SCROLL_UP;
		if (_scroller_click_timeout == 0) {
			_scroller_click_timeout = 6;
			if (sb->pos != 0) sb->pos--;
		}
		_left_button_clicked = false;
	} else if (pos >= ma - 10) {
		/* Pressing the lower button? */
		w->flags4 |= WF_SCROLL_DOWN;

		if (_scroller_click_timeout == 0) {
			_scroller_click_timeout = 6;
			if (sb->pos + sb->cap < sb->count) sb->pos++;
		}
		_left_button_clicked = false;
	} else {
		Point pt = HandleScrollbarHittest(sb, mi, ma);

		if (pos < pt.x) {
			sb->pos = max(sb->pos - sb->cap, 0);
		} else if (pos > pt.y) {
			sb->pos = min(sb->pos + sb->cap, max(sb->count - sb->cap, 0));
		} else {
			_scrollbar_start_pos = pt.x - mi - 9;
			_scrollbar_size = ma - mi - 23;
			w->flags4 |= WF_SCROLL_MIDDLE;
			_scrolling_scrollbar = true;
			_cursorpos_drag_start = _cursor.pos;
		}
	}

	w->SetDirty();
}

/** Special handling for the scrollbar widget type.
 * Handles the special scrolling buttons and other scrolling.
 * @param w  Window on which a scroll was performed.
 * @param wi Pointer to the scrollbar widget.
 * @param x  The X coordinate of the mouse click.
 * @param y  The Y coordinate of the mouse click.
 */
void ScrollbarClickHandler(Window *w, const Widget *wi, int x, int y)
{
	int mi, ma;

	switch (wi->type) {
		case WWT_SCROLLBAR:
			/* vertical scroller */
			mi = wi->top;
			ma = wi->bottom;
			break;

		case WWT_SCROLL2BAR:
			/* 2nd vertical scroller */
			mi = wi->top;
			ma = wi->bottom;
			break;

		case  WWT_HSCROLLBAR:
			/* horizontal scroller */
			mi = wi->left;
			ma = wi->right;
			break;

		default: NOT_REACHED();
	}
	ScrollbarClickPositioning(w, wi->type, x, y, mi, ma);
}

/** Special handling for the scrollbar widget type.
 * Handles the special scrolling buttons and other scrolling.
 * @param w Window on which a scroll was performed.
 * @param nw Pointer to the scrollbar widget.
 * @param x The X coordinate of the mouse click.
 * @param y The Y coordinate of the mouse click.
 */
void ScrollbarClickHandler(Window *w, const NWidgetCore *nw, int x, int y)
{
	int mi, ma;

	switch (nw->type) {
		case WWT_SCROLLBAR:
			/* vertical scroller */
			mi = nw->pos_y;
			ma = nw->pos_y + nw->current_y;
			break;

		case WWT_SCROLL2BAR:
			/* 2nd vertical scroller */
			mi = nw->pos_y;
			ma = nw->pos_y + nw->current_y;
			break;

		case  WWT_HSCROLLBAR:
			/* horizontal scroller */
			mi = nw->pos_x;
			ma = nw->pos_x + nw->current_x;
			break;

		default: NOT_REACHED();
	}
	ScrollbarClickPositioning(w, nw->type, x, y, mi, ma);
}

/** Returns the index for the widget located at the given position
 * relative to the window. It includes all widget-corner pixels as well.
 * @param *w Window to look inside
 * @param  x The Window client X coordinate
 * @param  y The Window client y coordinate
 * @return A widget index, or -1 if no widget was found.
 */
int GetWidgetFromPos(const Window *w, int x, int y)
{
	if (w->nested_root != NULL) {
		NWidgetCore *nw = w->nested_root->GetWidgetFromPos(x, y);
		return (nw != NULL) ? nw->index : -1;
	}

	int found_index = -1;
	/* Go through the widgets and check if we find the widget that the coordinate is inside. */
	for (uint index = 0; index < w->widget_count; index++) {
		const Widget *wi = &w->widget[index];
		if (wi->type == WWT_EMPTY || wi->type == WWT_FRAME) continue;

		if (x >= wi->left && x <= wi->right && y >= wi->top &&  y <= wi->bottom &&
				!w->IsWidgetHidden(index)) {
			found_index = index;
		}
	}

	return found_index;
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

	int left, top;
	if ((type & WWT_MASK) == WWT_IMGBTN_2) {
		if (clicked) img++; // Show different image when clicked for #WWT_IMGBTN_2.
		left = WD_IMGBTN2_LEFT;
		top  = WD_IMGBTN2_TOP;
	} else {
		left = WD_IMGBTN_LEFT;
		top  = WD_IMGBTN_TOP;
	}
	DrawSprite(img, PAL_NONE, r.left + left + clicked, r.top + top + clicked);
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
	DrawString(r.left + clicked, r.right + clicked, ((r.top + r.bottom + 1) >> 1) - (FONT_HEIGHT_NORMAL / 2) + clicked, str, TC_FROMSTRING, SA_CENTER);
}

/**
 * Draw text.
 * @param r      Rectangle of the background.
 * @param colour Colour of the text.
 * @param str    Text to draw.
 */
static inline void DrawText(const Rect &r, TextColour colour, StringID str)
{
	if (str != STR_NULL) DrawString(r.left, r.right, r.top, str, colour);
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
 * @param data    Data of the widget.
 */
static inline void DrawMatrix(const Rect &r, Colours colour, bool clicked, uint16 data)
{
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, (clicked) ? FR_LOWERED : FR_NONE);

	int c = GB(data, 0, 8);
	int amt1 = (r.right - r.left + 1) / c;

	int d = GB(data, 8, 8);
	int amt2 = (r.bottom - r.top + 1) / d;

	int col = _colour_gradient[colour & 0xF][6];

	int x = r.left;
	for (int ctr = c; ctr > 1; ctr--) {
		x += amt1;
		GfxFillRect(x, r.top + 1, x, r.bottom - 1, col);
	}

	x = r.top;
	for (int ctr = d; ctr > 1; ctr--) {
		x += amt2;
		GfxFillRect(r.left + 1, x, r.right - 1, x, col);
	}

	col = _colour_gradient[colour & 0xF][4];

	x = r.left - 1;
	for (int ctr = c; ctr > 1; ctr--) {
		x += amt1;
		GfxFillRect(x, r.top + 1, x, r.bottom - 1, col);
	}

	x = r.top - 1;
	for (int ctr = d; ctr > 1; ctr--) {
		x += amt2;
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
	DrawString(r.left + up_clicked, r.right + up_clicked, r.top + up_clicked, UPARROW, TC_BLACK, SA_CENTER);

	DrawFrameRect(r.left, r.bottom - 9, r.right, r.bottom, colour, (down_clicked) ? FR_LOWERED : FR_NONE);
	DrawString(r.left + down_clicked, r.right + down_clicked, r.bottom - 9 + down_clicked, DOWNARROW, TC_BLACK, SA_CENTER);

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

	Point pt = HandleScrollbarHittest(scrollbar, r.top, r.bottom);
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
	Point pt = HandleScrollbarHittest(scrollbar, r.left, r.right);
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

	if (_dynlang.text_dir == TD_LTR) {
		/* Line from upper left corner to start of text */
		GfxFillRect(r.left, r.top + 4, r.left + 4, r.top + 4, c1);
		GfxFillRect(r.left + 1, r.top + 5, r.left + 4, r.top + 5, c2);

		/* Line from end of text to upper right corner */
		GfxFillRect(x2, r.top + 4, r.right - 1, r.top + 4, c1);
		GfxFillRect(x2, r.top + 5, r.right - 2, r.top + 5, c2);
	} else {
		/* Line from upper left corner to start of text */
		GfxFillRect(r.left, r.top + 4, x2 - 2, r.top + 4, c1);
		GfxFillRect(r.left + 1, r.top + 5, x2 - 2, r.top + 5, c2);

		/* Line from end of text to upper right corner */
		GfxFillRect(r.right - 5, r.top + 4, r.right - 1, r.top + 4, c1);
		GfxFillRect(r.right - 5, r.top + 5, r.right - 2, r.top + 5, c2);
	}

	/* Line from upper left corner to bottom left corner */
	GfxFillRect(r.left, r.top + 5, r.left, r.bottom - 1, c1);
	GfxFillRect(r.left + 1, r.top + 6, r.left + 1, r.bottom - 2, c2);

	/* Line from upper right corner to bottom right corner */
	GfxFillRect(r.right - 1, r.top + 5, r.right - 1, r.bottom - 2, c1);
	GfxFillRect(r.right, r.top + 4, r.right, r.bottom - 1, c2);

	GfxFillRect(r.left + 1, r.bottom - 1, r.right - 1, r.bottom - 1, c1);
	GfxFillRect(r.left, r.bottom, r.right, r.bottom, c2);
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
	DrawString(r.left + WD_CLOSEBOX_LEFT, r.right - WD_CLOSEBOX_RIGHT, r.top + WD_CLOSEBOX_TOP, str, TC_FROMSTRING, SA_CENTER);
}

/**
 * Draw a caption bar.
 * @param r      Rectangle of the bar.
 * @param colour Colour of the window.
 * @param owner  'Owner' of the window.
 * @param str    Text to draw in the bar.
 */
static inline void DrawCaption(const Rect &r, Colours colour, Owner owner, StringID str)
{
	DrawFrameRect(r.left, r.top, r.right, r.bottom, colour, FR_BORDERONLY);
	DrawFrameRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, colour, (owner == INVALID_OWNER) ? FR_LOWERED | FR_DARKENED : FR_LOWERED | FR_DARKENED | FR_BORDERONLY);

	if (owner != INVALID_OWNER) {
		GfxFillRect(r.left + 2, r.top + 2, r.right - 2, r.bottom - 2, _colour_gradient[_company_colours[owner]][4]);
	}

	if (str != STR_NULL) DrawString(r.left + WD_CAPTIONTEXT_LEFT, r.right - WD_CAPTIONTEXT_RIGHT, r.top + WD_CAPTIONTEXT_TOP, str, TC_FROMSTRING, SA_CENTER);
}

static inline void DrawDropdown(const Rect &r, Colours colour, bool clicked, StringID str)
{
	if (_dynlang.text_dir == TD_LTR) {
		DrawFrameRect(r.left, r.top, r.right - 12, r.bottom, colour, FR_NONE);
		DrawFrameRect(r.right - 11, r.top, r.right, r.bottom, colour, clicked ? FR_LOWERED : FR_NONE);
		DrawString(r.right - (clicked ? 10 : 11), r.right, r.top + (clicked ? 2 : 1), STR_ARROW_DOWN, TC_BLACK, SA_CENTER);
		if (str != STR_NULL) DrawString(r.left + WD_DROPDOWNTEXT_LEFT, r.right - WD_DROPDOWNTEXT_RIGHT, r.top + WD_DROPDOWNTEXT_TOP, str, TC_BLACK);
	} else {
		DrawFrameRect(r.left + 12, r.top, r.right, r.bottom, colour, FR_NONE);
		DrawFrameRect(r.left, r.top, r.left + 11, r.bottom, colour, clicked ? FR_LOWERED : FR_NONE);
		DrawString(r.left + clicked, r.left + 11, r.top + (clicked ? 2 : 1), STR_ARROW_DOWN, TC_BLACK, SA_CENTER);
		if (str != STR_NULL) DrawString(r.left + WD_DROPDOWNTEXT_RIGHT, r.right - WD_DROPDOWNTEXT_LEFT, r.top + WD_DROPDOWNTEXT_TOP, str, TC_BLACK);
	}
}

/**
 * Paint all widgets of a window.
 */
void Window::DrawWidgets() const
{
	if (this->nested_root != NULL) {
		this->nested_root->Draw(this);
		return;
	}

	const DrawPixelInfo *dpi = _cur_dpi;

	for (uint i = 0; i < this->widget_count; i++) {
		const Widget *wi = &this->widget[i];
		bool clicked = this->IsWidgetLowered(i);
		Rect r;

		if (dpi->left > (r.right = wi->right) ||
				dpi->left + dpi->width <= (r.left = wi->left) ||
				dpi->top > (r.bottom = wi->bottom) ||
				dpi->top + dpi->height <= (r.top = wi->top) ||
				this->IsWidgetHidden(i)) {
			continue;
		}

		switch (wi->type & WWT_MASK) {
		case WWT_IMGBTN:
		case WWT_IMGBTN_2:
			DrawImageButtons(r, wi->type,wi->colour, clicked, wi->data);
			break;

		case WWT_PANEL:
			assert(wi->data == 0);
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->colour, (clicked) ? FR_LOWERED : FR_NONE);
			break;

		case WWT_EDITBOX:
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->colour, FR_LOWERED | FR_DARKENED);
			break;

		case WWT_TEXTBTN:
		case WWT_TEXTBTN_2:
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->colour, (clicked) ? FR_LOWERED : FR_NONE);
			/* FALL THROUGH */

		case WWT_LABEL:
			DrawLabel(r, wi->type, clicked, wi->data);
			break;

		case WWT_TEXT:
			DrawText(r, (TextColour)wi->colour, wi->data);
			break;

		case WWT_INSET:
			DrawInset(r, wi->colour, wi->data);
			break;

		case WWT_MATRIX:
			DrawMatrix(r, wi->colour, clicked, wi->data);
			break;

		/* vertical scrollbar */
		case WWT_SCROLLBAR:
			assert(wi->data == 0);
			DrawVerticalScrollbar(r, wi->colour, (this->flags4 & (WF_SCROLL_UP | WF_HSCROLL | WF_SCROLL2)) == WF_SCROLL_UP,
								(this->flags4 & (WF_SCROLL_MIDDLE | WF_HSCROLL | WF_SCROLL2)) == WF_SCROLL_MIDDLE,
								(this->flags4 & (WF_SCROLL_DOWN | WF_HSCROLL | WF_SCROLL2)) == WF_SCROLL_DOWN, &this->vscroll);
			break;

		case WWT_SCROLL2BAR:
			assert(wi->data == 0);
			DrawVerticalScrollbar(r, wi->colour, (this->flags4 & (WF_SCROLL_UP | WF_HSCROLL | WF_SCROLL2)) == (WF_SCROLL_UP | WF_SCROLL2),
								(this->flags4 & (WF_SCROLL_MIDDLE | WF_HSCROLL | WF_SCROLL2)) == (WF_SCROLL_MIDDLE | WF_SCROLL2),
								(this->flags4 & (WF_SCROLL_DOWN | WF_HSCROLL | WF_SCROLL2)) == (WF_SCROLL_DOWN | WF_SCROLL2), &this->vscroll2);
			break;

		/* horizontal scrollbar */
		case WWT_HSCROLLBAR:
			assert(wi->data == 0);
			DrawHorizontalScrollbar(r, wi->colour, (this->flags4 & (WF_SCROLL_UP | WF_HSCROLL)) == (WF_SCROLL_UP | WF_HSCROLL),
								(this->flags4 & (WF_SCROLL_MIDDLE | WF_HSCROLL)) == (WF_SCROLL_MIDDLE | WF_HSCROLL),
								(this->flags4 & (WF_SCROLL_DOWN | WF_HSCROLL)) == (WF_SCROLL_DOWN | WF_HSCROLL), &this->hscroll);
			break;

		case WWT_FRAME:
			DrawFrame(r, wi->colour, wi->data);
			break;

		case WWT_STICKYBOX:
			assert(wi->data == 0);
			DrawStickyBox(r, wi->colour, !!(this->flags4 & WF_STICKY));
			break;

		case WWT_RESIZEBOX:
			assert(wi->data == 0);
			DrawResizeBox(r, wi->colour, wi->left < (this->width / 2), !!(this->flags4 & WF_SIZING));
			break;

		case WWT_CLOSEBOX:
			DrawCloseBox(r, wi->colour, wi->data);
			break;

		case WWT_CAPTION:
			DrawCaption(r, wi->colour, this->owner, wi->data);
			break;

		case WWT_DROPDOWN:
			DrawDropdown(r, wi->colour, clicked, wi->data);
			break;
		}

		if (this->IsWidgetDisabled(i)) {
			GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, _colour_gradient[wi->colour & 0xF][2], FILLRECT_CHECKER);
		}
	}


	if (this->flags4 & WF_WHITE_BORDER_MASK) {
		DrawFrameRect(0, 0, this->width - 1, this->height - 1, COLOUR_WHITE, FR_BORDERONLY);
	}

}

/**
 * Evenly distribute the combined horizontal length of two consecutive widgets.
 * @param w Window containing the widgets.
 * @param a Left widget to resize.
 * @param b Right widget to resize.
 * @note Widgets are assumed to lie against each other.
 */
static void ResizeWidgets(Window *w, byte a, byte b)
{
	int16 offset = w->widget[a].left;
	int16 length = w->widget[b].right - offset;

	w->widget[a].right = (length / 2) + offset;

	w->widget[b].left  = w->widget[a].right + 1;
}

/**
 * Evenly distribute the combined horizontal length of three consecutive widgets.
 * @param w Window containing the widgets.
 * @param a Left widget to resize.
 * @param b Middle widget to resize.
 * @param c Right widget to resize.
 * @note Widgets are assumed to lie against each other.
 */
static void ResizeWidgets(Window *w, byte a, byte b, byte c)
{
	int16 offset = w->widget[a].left;
	int16 length = w->widget[c].right - offset;

	w->widget[a].right = length / 3;
	w->widget[b].right = w->widget[a].right * 2;

	w->widget[a].right += offset;
	w->widget[b].right += offset;

	/* Now the right side of the buttons are set. We will now set the left sides next to them */
	w->widget[b].left  = w->widget[a].right + 1;
	w->widget[c].left  = w->widget[b].right + 1;
}

/** Evenly distribute some widgets when resizing horizontally (often a button row)
 *  When only two arguments are given, the widgets are presumed to be on a line and only the ends are given
 * @param w Window to modify
 * @param left The leftmost widget to resize
 * @param right The rightmost widget to resize. Since right side of it is used, remember to set it to RESIZE_RIGHT
 */
void ResizeButtons(Window *w, byte left, byte right)
{
	int16 num_widgets = right - left + 1;

	if (num_widgets < 2) NOT_REACHED();

	switch (num_widgets) {
		case 2: ResizeWidgets(w, left, right); break;
		case 3: ResizeWidgets(w, left, left + 1, right); break;
		default: {
			/* Looks like we got more than 3 widgets to resize
			 * Now we will find the middle of the space desinated for the widgets
			 * and place half of the widgets on each side of it and call recursively.
			 * Eventually we will get down to blocks of 2-3 widgets and we got code to handle those cases */
			int16 offset = w->widget[left].left;
			int16 length = w->widget[right].right - offset;
			byte widget = ((num_widgets - 1)/ 2) + left; // rightmost widget of the left side

			/* Now we need to find the middle of the widgets.
			 * It will not always be the middle because if we got an uneven number of widgets,
			 *   we will need it to be 2/5, 3/7 and so on
			 * To get this, we multiply with num_widgets/num_widgets. Since we calculate in int, we will get:
			 *
			 *    num_widgets/2 (rounding down)
			 *   ---------------
			 *     num_widgets
			 *
			 * as multiplier to length. We just multiply before divide to that we stay in the int area though */
			int16 middle = ((length * num_widgets) / (2 * num_widgets)) + offset;

			/* Set left and right on the widgets, that's next to our "middle" */
			w->widget[widget].right = middle;
			w->widget[widget + 1].left = w->widget[widget].right + 1;
			/* Now resize the left and right of the middle */
			ResizeButtons(w, left, widget);
			ResizeButtons(w, widget + 1, right);
		}
	}
}

/** Resize a widget and shuffle other widgets around to fit. */
void ResizeWindowForWidget(Window *w, uint widget, int delta_x, int delta_y)
{
	int right  = w->widget[widget].right;
	int bottom = w->widget[widget].bottom;

	for (uint i = 0; i < w->widget_count; i++) {
		if (w->widget[i].left >= right && i != widget) w->widget[i].left += delta_x;
		if (w->widget[i].right >= right) w->widget[i].right += delta_x;
		if (w->widget[i].top >= bottom && i != widget) w->widget[i].top += delta_y;
		if (w->widget[i].bottom >= bottom) w->widget[i].bottom += delta_y;
	}

	/* A hidden widget has bottom == top or right == left, we need to make it
	 * one less to fit in its new gap. */
	if (right  == w->widget[widget].left) w->widget[widget].right--;
	if (bottom == w->widget[widget].top)  w->widget[widget].bottom--;

	if (w->widget[widget].left > w->widget[widget].right)  w->widget[widget].right  = w->widget[widget].left;
	if (w->widget[widget].top  > w->widget[widget].bottom) w->widget[widget].bottom = w->widget[widget].top;

	w->width  += delta_x;
	w->height += delta_y;
	w->resize.width  += delta_x;
	w->resize.height += delta_y;
}

/**
 * Draw a sort button's up or down arrow symbol.
 * @param widget Sort button widget
 * @param state State of sort button
 */
void Window::DrawSortButtonState(int widget, SortButtonState state) const
{
	if (state == SBS_OFF) return;

	int offset = this->IsWidgetLowered(widget) ? 1 : 0;
	int base, top;
	if (this->widget != NULL) {
		base = offset + (_dynlang.text_dir == TD_LTR ? this->widget[widget].right - WD_SORTBUTTON_ARROW_WIDTH : this->widget[widget].left);
		top = this->widget[widget].top;
	} else {
		assert(this->nested_array != NULL);
		base = offset + this->nested_array[widget]->pos_x + (_dynlang.text_dir == TD_LTR ? this->nested_array[widget]->current_x - WD_SORTBUTTON_ARROW_WIDTH : 0);
		top = this->nested_array[widget]->pos_y;
	}
	DrawString(base, base + WD_SORTBUTTON_ARROW_WIDTH, top + 1 + offset, state == SBS_DOWN ? DOWNARROW : UPARROW, TC_BLACK, SA_CENTER);
}


/**
 * @defgroup NestedWidgets Hierarchical widgets.
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
 * <li> A top-down sweep by recursively calling NWidgetBase::AssignSizePosition() with #ST_ARRAY or #ST_SMALLEST to make the smallest sizes consistent over
 *      the entire tree, and to assign the top-left (\e pos_x, \e pos_y) position of each widget in the tree. This step uses \e fill_x and \e fill_y at each
 *      node in the tree to decide how to fill each widget towards consistent sizes. Also the current size (\e current_x and \e current_y) is set.
 *      For generating a widget array (#ST_ARRAY), resize step sizes are made consistent.
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
 * If \a w is not \c NULL, the function calls #Window::GetWidgetContentSize for each leaf widget and
 * background widget without child with a non-negative index.
 *
 * @param w Optional window owning the widget.
 * @param init_array Initialize the \c w->nested_array as well. Should only be set if \a w != NULL.
 *
 * @note After the computation, the results can be queried by accessing the #smallest_x and #smallest_y data members of the widget.
 */

/**
 * @fn void NWidgetBase::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool allow_resize_x, bool allow_resize_y, bool rtl)
 * Assign size and position to the widget.
 * @param sizing         Type of resizing to perform.
 * @param x              Horizontal offset of the widget relative to the left edge of the window.
 * @param y              Vertical offset of the widget relative to the top edge of the window.
 * @param given_width    Width allocated to the widget.
 * @param given_height   Height allocated to the widget.
 * @param allow_resize_x Horizontal resizing is allowed (only used when \a sizing is #ST_ARRAY).
 * @param allow_resize_y Vertical resizing is allowed (only used when \a sizing in #ST_ARRAY).
 * @param rtl            Adapt for right-to-left languages (position contents of horizontal containers backwards).
 *
 * Afterwards, \e pos_x and \e pos_y contain the top-left position of the widget, \e smallest_x and \e smallest_y contain
 * the smallest size such that all widgets of the window are consistent, and \e current_x and \e current_y contain the current size.
 */

/**
 * @fn void FillNestedArray(NWidgetCore **array, uint length)
 * Fill the Window::nested_array array with pointers to nested widgets in the tree.
 * @param array Base pointer of the array.
 * @param length Length of the array.
 */

/**
 * Store size and position.
 * @param sizing         Type of resizing to perform.
 * @param x              Horizontal offset of the widget relative to the left edge of the window.
 * @param y              Vertical offset of the widget relative to the top edge of the window.
 * @param given_width    Width allocated to the widget.
 * @param given_height   Height allocated to the widget.
 * @param allow_resize_x Horizontal resizing is allowed (only used when \a sizing is #ST_ARRAY).
 * @param allow_resize_y Vertical resizing is allowed (only used when \a sizing in #ST_ARRAY).
 */
inline void NWidgetBase::StoreSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool allow_resize_x, bool allow_resize_y)
{
	this->pos_x = x;
	this->pos_y = y;
	if (sizing == ST_ARRAY || sizing == ST_SMALLEST) {
		this->smallest_x = given_width;
		this->smallest_y = given_height;
	}
	this->current_x = given_width;
	this->current_y = given_height;
	if (sizing == ST_ARRAY && !allow_resize_x) this->resize_x = 0;
	if (sizing == ST_ARRAY && !allow_resize_y) this->resize_y = 0;
}

/**
 * @fn void NWidgetBase::StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl)
 * Store all child widgets with a valid index into the widget array.
 * @param widgets     Widget array to store the nested widgets in.
 * @param length      Length of the array.
 * @param left_moving Left edge of the widget may move due to resizing (right edge if \a rtl).
 * @param top_moving  Top edge of the widget may move due to resizing.
 * @param rtl         Adapt for right-to-left languages (position contents of horizontal containers backwards).
 *
 * @note When storing a nested widget, the function should check first that the type in the \a widgets array is #WWT_LAST.
 *       This is used to detect double widget allocations as well as holes in the widget array.
 */

/**
 * @fn void Draw(const Window *w)
 * Draw the widgets of the tree.
 * The function calls #Window::DrawWidget for each widget with a non-negative index, after the widget itself is painted.
 * @param w Window that owns the tree.
 */

/**
 * Mark the widget as 'dirty' (in need of repaint).
 * @param w Window owning the widget.
 */
void NWidgetBase::Invalidate(const Window *w) const
{
	int abs_left = w->left + this->pos_x;
	int abs_top = w->top + this->pos_y;
	SetDirtyBlocks(abs_left, abs_top, abs_left + this->current_x, abs_top + this->current_y);
}

/**
 * @fn NWidgetCore *GetWidgetFromPos(int x, int y)
 * Retrieve a widget by its position.
 * @param x Horizontal position relative to the left edge of the window.
 * @param y Vertical position relative to the top edge of the window.
 * @return Returns the deepest nested widget that covers the given position, or \c NULL if no widget can be found.
 */

/**
 * @fn NWidgetBase *GetWidgetOfType(WidgetType tp)
 * Retrieve a widget by its type.
 * @param tp Widget type to search for.
 * @return Returns the first widget of the specified type, or \c NULL if no widget can be found.
 */

/**
 * Constructor for resizable nested widgets.
 * @param tp     Nested widget type.
 * @param fill_x Allow horizontal filling from initial size.
 * @param fill_y Allow vertical filling from initial size.
 */
NWidgetResizeBase::NWidgetResizeBase(WidgetType tp, bool fill_x, bool fill_y) : NWidgetBase(tp)
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
 * Set the filling of the widget from initial size.
 * @param fill_x Allow horizontal filling from initial size.
 * @param fill_y Allow vertical filling from initial size.
 */
void NWidgetResizeBase::SetFill(bool fill_x, bool fill_y)
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

void NWidgetResizeBase::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool allow_resize_x, bool allow_resize_y, bool rtl)
{
	StoreSizePosition(sizing, x, y, given_width, given_height, allow_resize_x, allow_resize_y);
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
NWidgetCore::NWidgetCore(WidgetType tp, Colours colour, bool fill_x, bool fill_y, uint16 widget_data, StringID tool_tip) : NWidgetResizeBase(tp, fill_x, fill_y)
{
	this->colour = colour;
	this->index = -1;
	this->widget_data = widget_data;
	this->tool_tip = tool_tip;
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

void NWidgetCore::FillNestedArray(NWidgetCore **array, uint length)
{
	if (this->index >= 0 && (uint)(this->index) < length) array[this->index] = this;
}

void NWidgetCore::StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl)
{
	if (this->index < 0) return;

	assert(this->index < length);
	Widget *w = widgets + this->index;
	assert(w->type == WWT_LAST);

	DisplayFlags flags = RESIZE_NONE; // resize flags.
	/* Compute vertical resizing. */
	if (top_moving) {
		flags |= RESIZE_TB; // Only 1 widget can resize in the widget array.
	} else if(this->resize_y > 0) {
		flags |= RESIZE_BOTTOM;
	}
	/* Compute horizontal resizing. */
	if (left_moving) {
		flags |= RESIZE_LR; // Only 1 widget can resize in the widget array.
	} else if (this->resize_x > 0) {
		flags |= RESIZE_RIGHT;
	}

	/* Copy nested widget data into its widget array entry. */
	w->type = this->type;
	w->display_flags = flags;
	w->colour = this->colour;
	w->left = this->pos_x;
	w->right = this->pos_x + this->smallest_x - 1;
	w->top = this->pos_y;
	w->bottom = this->pos_y + this->smallest_y - 1;
	w->data = this->widget_data;
	w->tooltips = this->tool_tip;
}

/**
 * @fn Scrollbar *NWidgetCore::FindScrollbar(Window *w, bool allow_next = true)
 * Find the scrollbar of the widget through the Window::nested_array.
 * @param w          Window containing the widgets and the scrollbar,
 * @param allow_next Search may be extended to the next widget.
 *
 * @todo This implementation uses the constraint that a scrollbar must be the next item in the #Window::nested_array, and the scrollbar
 *       data is stored in the #Window structure (#Window::vscroll, #Window::vscroll2, and #Window::hscroll).
 *       Alternative light-weight implementations may be considered, eg by sub-classing a canvas-like widget, and/or by having
 *       an explicit link between the scrollbar and the widget being scrolled.
 */

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

void NWidgetContainer::FillNestedArray(NWidgetCore **array, uint length)
{
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		child_wid->FillNestedArray(array, length);
	}
}

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
	int increment = max_space - base;
	increment -= increment % step;
	return base + increment;
}

/**
 * Compute the offset of a widget due to not entirely using the available space.
 * @param space     Space used by the widget.
 * @param max_space Available space for the widget.
 * @return Offset for centering widget.
 */
static inline uint ComputeOffset(uint space, uint max_space)
{
	if (space >= max_space) return 0;
	return (max_space - space) / 2;
}


/**
 * Widgets stacked on top of each other.
 * @param tp Kind of stacking, must be either #NWID_SELECTION or #NWID_LAYERED.
 */
NWidgetStacked::NWidgetStacked(WidgetType tp) : NWidgetContainer(tp)
{
}

void NWidgetStacked::SetupSmallestSize(Window *w, bool init_array)
{
	/* First sweep, recurse down and compute minimal size and filling. */
	this->smallest_x = 0;
	this->smallest_y = 0;
	this->fill_x = (this->head != NULL);
	this->fill_y = (this->head != NULL);
	this->resize_x = (this->head != NULL) ? 1 : 0;
	this->resize_y = (this->head != NULL) ? 1 : 0;
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		child_wid->SetupSmallestSize(w, init_array);

		this->smallest_x = max(this->smallest_x, child_wid->smallest_x + child_wid->padding_left + child_wid->padding_right);
		this->smallest_y = max(this->smallest_y, child_wid->smallest_y + child_wid->padding_top + child_wid->padding_bottom);
		this->fill_x &= child_wid->fill_x;
		this->fill_y &= child_wid->fill_y;
		this->resize_x = LeastCommonMultiple(this->resize_x, child_wid->resize_x);
		this->resize_y = LeastCommonMultiple(this->resize_y, child_wid->resize_y);
	}
}

void NWidgetStacked::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool allow_resize_x, bool allow_resize_y, bool rtl)
{
	assert(given_width >= this->smallest_x && given_height >= this->smallest_y);
	StoreSizePosition(sizing, x, y, given_width, given_height, allow_resize_x, allow_resize_y);

	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		uint hor_step = child_wid->GetHorizontalStepSize(sizing);
		uint child_width = ComputeMaxSize(child_wid->smallest_x, given_width - child_wid->padding_left - child_wid->padding_right, hor_step);
		uint child_pos_x = (rtl ? child_wid->padding_right : child_wid->padding_left) + ComputeOffset(child_width, given_width - child_wid->padding_left - child_wid->padding_right);

		uint vert_step = child_wid->GetVerticalStepSize(sizing);
		uint child_height = ComputeMaxSize(child_wid->smallest_y, given_height - child_wid->padding_top - child_wid->padding_bottom, vert_step);
		uint child_pos_y = child_wid->padding_top + ComputeOffset(child_height,  given_height - child_wid->padding_top - child_wid->padding_bottom);

		child_wid->AssignSizePosition(sizing, x + child_pos_x, y + child_pos_y, child_width, child_height, (this->resize_x > 0), (this->resize_y > 0), rtl);
	}
}

void NWidgetStacked::StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl)
{
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		child_wid->StoreWidgets(widgets, length, left_moving, top_moving, rtl);
	}
}

void NWidgetStacked::Draw(const Window *w)
{
	assert(this->type == NWID_LAYERED); // Currently, NWID_SELECTION is not supported.
	/* Render from back to front. */
	for (NWidgetBase *child_wid = this->tail; child_wid != NULL; child_wid = child_wid->prev) {
		child_wid->Draw(w);
	}
}

NWidgetCore *NWidgetStacked::GetWidgetFromPos(int x, int y)
{
	if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return NULL;
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		NWidgetCore *nwid = child_wid->GetWidgetFromPos(x, y);
		if (nwid != NULL) return nwid;
	}
	return NULL;
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
	this->smallest_x = 0;   // Sum of minimal size of all childs.
	this->smallest_y = 0;   // Biggest child.
	this->fill_x = false;   // true if at least one child allows fill_x.
	this->fill_y = true;    // true if all childs allow fill_y.
	this->resize_x = 0;     // smallest non-zero child widget resize step.
	this->resize_y = 1;     // smallest common child resize step

	/* 1. Forward call, collect biggest nested array index, and longest child length. */
	uint longest = 0; // Longest child found.
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		child_wid->SetupSmallestSize(w, init_array);
		longest = max(longest, child_wid->smallest_x);
	}
	/* 2. For containers that must maintain equal width, extend child minimal size. */
	if (this->flags & NC_EQUALSIZE) {
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			if (child_wid->fill_x) child_wid->smallest_x = longest;
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
		this->smallest_y = max(this->smallest_y, child_wid->smallest_y + child_wid->padding_top + child_wid->padding_bottom);
		this->fill_x |= child_wid->fill_x;
		this->fill_y &= child_wid->fill_y;

		if (child_wid->resize_x > 0) {
			if (this->resize_x == 0 || this->resize_x > child_wid->resize_x) this->resize_x = child_wid->resize_x;
		}
		this->resize_y = LeastCommonMultiple(this->resize_y, child_wid->resize_y);
	}
	/* We need to zero the PIP settings so we can re-initialize the tree. */
	this->pip_pre = this->pip_inter = this->pip_post = 0;
}

void NWidgetHorizontal::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool allow_resize_x, bool allow_resize_y, bool rtl)
{
	assert(given_width >= this->smallest_x && given_height >= this->smallest_y);

	uint additional_length = given_width - this->smallest_x; // Additional width given to us.
	StoreSizePosition(sizing, x, y, given_width, given_height, allow_resize_x, allow_resize_y);

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

		uint vert_step = child_wid->GetVerticalStepSize(sizing);
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
	allow_resize_x = (this->resize_x > 0);
	NWidgetBase *child_wid = rtl ? this->tail : this->head;
	while (child_wid != NULL) {
		uint child_width = child_wid->current_x;
		uint child_x = x + position + (rtl ? child_wid->padding_right : child_wid->padding_left);
		uint child_y = y + child_wid->padding_top + ComputeOffset(child_wid->current_y,  given_height - child_wid->padding_top - child_wid->padding_bottom);

		child_wid->AssignSizePosition(sizing, child_x, child_y, child_width, child_wid->current_y, allow_resize_x, (this->resize_y > 0), rtl);
		position += child_width + child_wid->padding_right + child_wid->padding_left;
		if (child_wid->resize_x > 0) allow_resize_x = false; // Widget array allows only one child resizing

		child_wid = rtl ? child_wid->prev : child_wid->next;
	}
}

void NWidgetHorizontal::StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl)
{
	NWidgetBase *child_wid = rtl ? this->tail : this->head;
	while (child_wid != NULL) {
		child_wid->StoreWidgets(widgets, length, left_moving, top_moving, rtl);
		left_moving |= (child_wid->resize_x > 0);

		child_wid = rtl ? child_wid->prev : child_wid->next;
	}
}

/** Horizontal left-to-right container widget. */
NWidgetHorizontalLTR::NWidgetHorizontalLTR(NWidContainerFlags flags) : NWidgetHorizontal(flags)
{
	this->type = NWID_HORIZONTAL_LTR;
}

void NWidgetHorizontalLTR::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool allow_resize_x, bool allow_resize_y, bool rtl)
{
	NWidgetHorizontal::AssignSizePosition(sizing, x, y, given_width, given_height, allow_resize_x, allow_resize_y, false);
}

void NWidgetHorizontalLTR::StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl)
{
	NWidgetHorizontal::StoreWidgets(widgets, length, left_moving, top_moving, false);
}

/** Vertical container widget. */
NWidgetVertical::NWidgetVertical(NWidContainerFlags flags) : NWidgetPIPContainer(NWID_VERTICAL, flags)
{
}

void NWidgetVertical::SetupSmallestSize(Window *w, bool init_array)
{
	this->smallest_x = 0;   // Biggest child.
	this->smallest_y = 0;   // Sum of minimal size of all childs.
	this->fill_x = true;    // true if all childs allow fill_x.
	this->fill_y = false;   // true if at least one child allows fill_y.
	this->resize_x = 1;     // smallest common child resize step
	this->resize_y = 0;     // smallest non-zero child widget resize step.

	/* 1. Forward call, collect biggest nested array index, and longest child length. */
	uint highest = 0; // Highest child found.
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		child_wid->SetupSmallestSize(w, init_array);
		highest = max(highest, child_wid->smallest_y);
	}
	/* 2. For containers that must maintain equal width, extend child minimal size. */
	if (this->flags & NC_EQUALSIZE) {
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			if (child_wid->fill_y) child_wid->smallest_y = highest;
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
		this->smallest_x = max(this->smallest_x, child_wid->smallest_x + child_wid->padding_left + child_wid->padding_right);
		this->fill_y |= child_wid->fill_y;
		this->fill_x &= child_wid->fill_x;

		if (child_wid->resize_y > 0) {
			if (this->resize_y == 0 || this->resize_y > child_wid->resize_y) this->resize_y = child_wid->resize_y;
		}
		this->resize_x = LeastCommonMultiple(this->resize_x, child_wid->resize_x);
	}
	/* We need to zero the PIP settings so we can re-initialize the tree. */
	this->pip_pre = this->pip_inter = this->pip_post = 0;
}

void NWidgetVertical::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool allow_resize_x, bool allow_resize_y, bool rtl)
{
	assert(given_width >= this->smallest_x && given_height >= this->smallest_y);

	int additional_length = given_height - this->smallest_y; // Additional height given to us.
	StoreSizePosition(sizing, x, y, given_width, given_height, allow_resize_x, allow_resize_y);

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

		uint hor_step = child_wid->GetHorizontalStepSize(sizing);
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
	allow_resize_y = (this->resize_y > 0);
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		uint child_x = x + (rtl ? child_wid->padding_right : child_wid->padding_left) +
									ComputeOffset(child_wid->current_x, given_width - child_wid->padding_left - child_wid->padding_right);
		uint child_height = child_wid->current_y;

		child_wid->AssignSizePosition(sizing, child_x, y + position + child_wid->padding_top, child_wid->current_x, child_height, (this->resize_x > 0), allow_resize_y, rtl);
		position += child_height + child_wid->padding_top + child_wid->padding_bottom;
		if (child_wid->resize_y > 0) allow_resize_y = false; // Widget array allows only one child resizing
	}
}

void NWidgetVertical::StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl)
{
	for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
		child_wid->StoreWidgets(widgets, length, left_moving, top_moving, rtl);
		top_moving |= (child_wid->resize_y > 0);
	}
}

/**
 * Generic spacer widget.
 * @param length Horizontal size of the spacer widget.
 * @param height Vertical size of the spacer widget.
 */
NWidgetSpacer::NWidgetSpacer(int length, int height) : NWidgetResizeBase(NWID_SPACER, false, false)
{
	this->SetMinimalSize(length, height);
	this->SetResize(0, 0);
}

void NWidgetSpacer::SetupSmallestSize(Window *w, bool init_array)
{
	this->smallest_x = this->min_x;
	this->smallest_y = this->min_y;
}

void NWidgetSpacer::FillNestedArray(NWidgetCore **array, uint length)
{
}

void NWidgetSpacer::StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl)
{
	/* Spacer widgets are never stored in the widget array. */
}

void NWidgetSpacer::Draw(const Window *w)
{
	/* Spacer widget is never visible. */
}

void NWidgetSpacer::Invalidate(const Window *w) const
{
	/* Spacer widget never need repainting. */
}

NWidgetCore *NWidgetSpacer::GetWidgetFromPos(int x, int y)
{
	return NULL;
}

NWidgetBase *NWidgetSpacer::GetWidgetOfType(WidgetType tp)
{
	return (this->type == tp) ? this : NULL;
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
NWidgetBackground::NWidgetBackground(WidgetType tp, Colours colour, int index, NWidgetPIPContainer *child) : NWidgetCore(tp, colour, true, true, 0x0, STR_NULL)
{
	this->SetIndex(index);
	assert(tp == WWT_PANEL || tp == WWT_INSET || tp == WWT_FRAME);
	assert(index >= 0);
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
	} else {
		Dimension d = {this->min_x, this->min_y};
		if (w != NULL) { // A non-NULL window pointer acts as switch to turn dynamic widget size on.
			if (this->index >= 0) d = maxdim(d, w->GetWidgetContentSize(this->index));
			if (this->type == WWT_FRAME || this->type == WWT_INSET) d = maxdim(d, GetStringBoundingBox(this->widget_data));
		}
		this->smallest_x = d.width;
		this->smallest_y = d.height;
	}
}

void NWidgetBackground::AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool allow_resize_x, bool allow_resize_y, bool rtl)
{
	StoreSizePosition(sizing, x, y, given_width, given_height, allow_resize_x, allow_resize_y);

	if (this->child != NULL) {
		uint x_offset = (rtl ? this->child->padding_right : this->child->padding_left);
		uint width = given_width - this->child->padding_right - this->child->padding_left;
		uint height = given_height - this->child->padding_top - this->child->padding_bottom;
		this->child->AssignSizePosition(sizing, x + x_offset, y + this->child->padding_top, width, height, (this->resize_x > 0), (this->resize_y > 0), rtl);
	}
}

void NWidgetBackground::StoreWidgets(Widget *widgets, int length, bool left_moving, bool top_moving, bool rtl)
{
	NWidgetCore::StoreWidgets(widgets, length, left_moving, top_moving, rtl);
	if (this->child != NULL) this->child->StoreWidgets(widgets, length, left_moving, top_moving, rtl);
}

void NWidgetBackground::FillNestedArray(NWidgetCore **array, uint length)
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
			DrawFrame(r, this->colour, this->widget_data);
			break;

		case WWT_INSET:
			DrawInset(r, this->colour, this->widget_data);
			break;

		default:
			NOT_REACHED();
	}

	if (this->child != NULL) this->child->Draw(w);
	if (this->index >= 0) w->DrawWidget(r, this->index);

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

Scrollbar *NWidgetBackground::FindScrollbar(Window *w, bool allow_next)
{
	if (this->index > 0 && allow_next && this->child == NULL && (uint)(this->index) + 1 < w->nested_array_size) {
		NWidgetCore *next_wid = w->nested_array[this->index + 1];
		if (next_wid != NULL) return next_wid->FindScrollbar(w, false);
	}
	return NULL;
}

NWidgetBase *NWidgetBackground::GetWidgetOfType(WidgetType tp)
{
	NWidgetBase *nwid = NULL;
	if (this->child != NULL) nwid = this->child->GetWidgetOfType(tp);
	if (nwid == NULL && this->type == tp) nwid = this;
	return nwid;
}

/** Reset the cached dimensions. */
/* static */ void NWidgetLeaf::InvalidateDimensionCache()
{
	stickybox_dimension.width = stickybox_dimension.height = 0;
	resizebox_dimension.width = resizebox_dimension.height = 0;
	closebox_dimension.width  = closebox_dimension.height  = 0;
}

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
NWidgetLeaf::NWidgetLeaf(WidgetType tp, Colours colour, int index, uint16 data, StringID tip) : NWidgetCore(tp, colour, true, true, data, tip)
{
	this->SetIndex(index);
	this->SetMinimalSize(0, 0);
	this->SetResize(0, 0);

	switch (tp) {
		case WWT_EMPTY:
			break;

		case WWT_PUSHBTN:
			this->SetFill(false, false);
			break;

		case WWT_IMGBTN:
		case WWT_PUSHIMGBTN:
		case WWT_IMGBTN_2:
			this->SetFill(false, false);
			break;

		case WWT_TEXTBTN:
		case WWT_PUSHTXTBTN:
		case WWT_TEXTBTN_2:
		case WWT_LABEL:
		case WWT_TEXT:
		case WWT_MATRIX:
		case WWT_EDITBOX:
			this->SetFill(false, false);
			break;

		case WWT_SCROLLBAR:
		case WWT_SCROLL2BAR:
			this->SetFill(false, true);
			this->SetResize(0, 1);
			this->min_x = WD_VSCROLLBAR_WIDTH;
			this->SetDataTip(0x0, STR_TOOLTIP_VSCROLL_BAR_SCROLLS_LIST);
			break;

		case WWT_CAPTION:
			this->SetFill(true, false);
			this->SetResize(1, 0);
			this->min_y = WD_CAPTION_HEIGHT;
			this->SetDataTip(data, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS);
			break;

		case WWT_HSCROLLBAR:
			this->SetFill(true, false);
			this->SetResize(1, 0);
			this->min_y = WD_HSCROLLBAR_HEIGHT;
			this->SetDataTip(0x0, STR_TOOLTIP_HSCROLL_BAR_SCROLLS_LIST);
			break;

		case WWT_STICKYBOX:
			this->SetFill(false, false);
			this->SetMinimalSize(WD_STICKYBOX_WIDTH, 14);
			this->SetDataTip(STR_NULL, STR_STICKY_BUTTON);
			break;

		case WWT_RESIZEBOX:
			this->SetFill(false, false);
			this->SetMinimalSize(WD_RESIZEBOX_WIDTH, 12);
			this->SetDataTip(STR_NULL, STR_RESIZE_BUTTON);
			break;

		case WWT_CLOSEBOX:
			this->SetFill(false, false);
			this->SetMinimalSize(WD_CLOSEBOX_WIDTH, 14);
			this->SetDataTip(STR_BLACK_CROSS, STR_TOOLTIP_CLOSE_WINDOW);
			break;

		case WWT_DROPDOWN:
			this->SetFill(false, false);
			this->min_y = WD_DROPDOWN_HEIGHT;
			break;

		default:
			NOT_REACHED();
	}
}

void NWidgetLeaf::SetupSmallestSize(Window *w, bool init_array)
{
	Dimension d = {this->min_x, this->min_y}; // At least minimal size is needed.

	if (w != NULL) { // A non-NULL window pointer acts as switch to turn dynamic widget sizing on.
		Dimension d2 = {0, 0};
		if (this->index >= 0) {
			if (init_array) {
				assert(w->nested_array_size > (uint)this->index);
				w->nested_array[this->index] = this;
			}
			d2 = maxdim(d2, w->GetWidgetContentSize(this->index)); // If appropriate, ask window for smallest size.
		}

		/* Check size requirements of the widget itself too.
		 * Also, add the offset used for rendering.
		 */
		switch (this->type) {
			case WWT_EMPTY:
			case WWT_MATRIX:
			case WWT_SCROLLBAR:
			case WWT_SCROLL2BAR:
			case WWT_HSCROLLBAR:
				break;

			case WWT_STICKYBOX:
				if (NWidgetLeaf::stickybox_dimension.width == 0) {
					NWidgetLeaf::stickybox_dimension = maxdim(GetSpriteSize(SPR_PIN_UP), GetSpriteSize(SPR_PIN_DOWN));
					NWidgetLeaf::stickybox_dimension.width += WD_STICKYBOX_LEFT + WD_STICKYBOX_RIGHT;
					NWidgetLeaf::stickybox_dimension.height += WD_STICKYBOX_TOP + WD_STICKYBOX_BOTTOM;
				}
				d2 = maxdim(d2, NWidgetLeaf::stickybox_dimension);
				break;

			case WWT_RESIZEBOX:
				if (NWidgetLeaf::resizebox_dimension.width == 0) {
					NWidgetLeaf::resizebox_dimension = maxdim(GetSpriteSize(SPR_WINDOW_RESIZE_LEFT), GetSpriteSize(SPR_WINDOW_RESIZE_RIGHT));
					NWidgetLeaf::resizebox_dimension.width += WD_RESIZEBOX_LEFT + WD_RESIZEBOX_RIGHT;
					NWidgetLeaf::resizebox_dimension.height += WD_RESIZEBOX_TOP + WD_RESIZEBOX_BOTTOM;
				}
				d2 = maxdim(d2, NWidgetLeaf::resizebox_dimension);
				break;

			case WWT_PUSHBTN:
			case WWT_EDITBOX:
				d2.width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				d2.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				break;

			case WWT_IMGBTN:
			case WWT_PUSHIMGBTN:
				d2 = maxdim(d2, GetSpriteSize(this->widget_data));
				d2.height += WD_IMGBTN_TOP + WD_IMGBTN_BOTTOM;
				d2.width += WD_IMGBTN_LEFT + WD_IMGBTN_RIGHT;
				break;

			case WWT_IMGBTN_2:
				d2 = maxdim(d2, GetSpriteSize(this->widget_data));
				d2 = maxdim(d2, GetSpriteSize(this->widget_data + 1));
				d2.height += WD_IMGBTN2_TOP + WD_IMGBTN2_BOTTOM;
				d2.width += WD_IMGBTN2_LEFT + WD_IMGBTN2_RIGHT;
				break;

			case WWT_CLOSEBOX:
				if (NWidgetLeaf::closebox_dimension.width == 0) {
					NWidgetLeaf::closebox_dimension = maxdim(GetStringBoundingBox(STR_BLACK_CROSS), GetStringBoundingBox(STR_SILVER_CROSS));
					NWidgetLeaf::closebox_dimension.width += WD_CLOSEBOX_LEFT + WD_CLOSEBOX_RIGHT;
					NWidgetLeaf::closebox_dimension.height += WD_CLOSEBOX_TOP + WD_CLOSEBOX_BOTTOM;
				}
				d2 = maxdim(d2, NWidgetLeaf::closebox_dimension);
				break;

			case WWT_TEXTBTN:
			case WWT_PUSHTXTBTN:
			case WWT_TEXTBTN_2:
				d2 = maxdim(d2, GetStringBoundingBox(this->widget_data));
				d2.width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				d2.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				break;

			case WWT_LABEL:
			case WWT_TEXT:
				d2 = maxdim(d2, GetStringBoundingBox(this->widget_data));
				break;

			case WWT_CAPTION:
				d2 = maxdim(d2, GetStringBoundingBox(this->widget_data));
				d2.width += WD_CAPTIONTEXT_LEFT + WD_CAPTIONTEXT_RIGHT;
				d2.height += WD_CAPTIONTEXT_TOP + WD_CAPTIONTEXT_BOTTOM;
				break;

			case WWT_DROPDOWN:
				d2 = maxdim(d2, GetStringBoundingBox(this->widget_data));
				d2.width += WD_DROPDOWNTEXT_LEFT + WD_DROPDOWNTEXT_RIGHT;
				d2.height += WD_DROPDOWNTEXT_TOP + WD_DROPDOWNTEXT_BOTTOM;
				break;

			default:
				NOT_REACHED();
		}
		d = maxdim(d, d2);
	}
	this->smallest_x = d.width;
	this->smallest_y = d.height;
	/* All other data is already at the right place. */
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
			DrawImageButtons(r, this->type,this->colour, clicked, this->widget_data);
			break;

		case WWT_TEXTBTN:
		case WWT_PUSHTXTBTN:
		case WWT_TEXTBTN_2:
			DrawFrameRect(r.left, r.top, r.right, r.bottom, this->colour, (clicked) ? FR_LOWERED : FR_NONE);
			DrawLabel(r, this->type, clicked, this->widget_data);
			break;

		case WWT_LABEL:
			DrawLabel(r, this->type, clicked, this->widget_data);
			break;

		case WWT_TEXT:
			DrawText(r, (TextColour)this->colour, this->widget_data);
			break;

		case WWT_MATRIX:
			DrawMatrix(r, this->colour, clicked, this->widget_data);
			break;

		case WWT_EDITBOX:
			DrawFrameRect(r.left, r.top, r.right, r.bottom, this->colour, FR_LOWERED | FR_DARKENED);
			break;

		case WWT_SCROLLBAR:
			assert(this->widget_data == 0);
			DrawVerticalScrollbar(r, this->colour, (w->flags4 & (WF_SCROLL_UP | WF_HSCROLL | WF_SCROLL2)) == WF_SCROLL_UP,
								(w->flags4 & (WF_SCROLL_MIDDLE | WF_HSCROLL | WF_SCROLL2)) == WF_SCROLL_MIDDLE,
								(w->flags4 & (WF_SCROLL_DOWN | WF_HSCROLL | WF_SCROLL2)) == WF_SCROLL_DOWN, &w->vscroll);
			break;

		case WWT_SCROLL2BAR:
			assert(this->widget_data == 0);
			DrawVerticalScrollbar(r, this->colour, (w->flags4 & (WF_SCROLL_UP | WF_HSCROLL | WF_SCROLL2)) == (WF_SCROLL_UP | WF_SCROLL2),
								(w->flags4 & (WF_SCROLL_MIDDLE | WF_HSCROLL | WF_SCROLL2)) == (WF_SCROLL_MIDDLE | WF_SCROLL2),
								(w->flags4 & (WF_SCROLL_DOWN | WF_HSCROLL | WF_SCROLL2)) == (WF_SCROLL_DOWN | WF_SCROLL2), &w->vscroll2);
			break;

		case WWT_CAPTION:
			DrawCaption(r, this->colour, w->owner, this->widget_data);
			break;

		case WWT_HSCROLLBAR:
			assert(this->widget_data == 0);
			DrawHorizontalScrollbar(r, this->colour, (w->flags4 & (WF_SCROLL_UP | WF_HSCROLL)) == (WF_SCROLL_UP | WF_HSCROLL),
								(w->flags4 & (WF_SCROLL_MIDDLE | WF_HSCROLL)) == (WF_SCROLL_MIDDLE | WF_HSCROLL),
								(w->flags4 & (WF_SCROLL_DOWN | WF_HSCROLL)) == (WF_SCROLL_DOWN | WF_HSCROLL), &w->hscroll);
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
			DrawDropdown(r, this->colour, clicked, this->widget_data);
			break;

		default:
			NOT_REACHED();
	}
	if (this->index >= 0) w->DrawWidget(r, this->index);

	if (this->IsDisabled()) {
		GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, _colour_gradient[this->colour & 0xF][2], FILLRECT_CHECKER);
	}
}

void NWidgetLeaf::Invalidate(const Window *w) const
{
	if (this->type == WWT_EMPTY) return; // Don't repaint dummy widgets.
	NWidgetBase::Invalidate(w);
}

NWidgetCore *NWidgetLeaf::GetWidgetFromPos(int x, int y)
{
	return (IsInsideBS(x, this->pos_x, this->current_x) && IsInsideBS(y, this->pos_y, this->current_y)) ? this : NULL;
}

Scrollbar *NWidgetLeaf::FindScrollbar(Window *w, bool allow_next)
{
	if (this->type == WWT_SCROLLBAR) return &w->vscroll;
	if (this->type == WWT_SCROLL2BAR) return &w->vscroll2;

	if (this->index > 0 && allow_next && (uint)(this->index) + 1 < w->nested_array_size) {
		NWidgetCore *next_wid = w->nested_array[this->index + 1];
		if (next_wid != NULL) return next_wid->FindScrollbar(w, false);
	}
	return NULL;
}

NWidgetBase *NWidgetLeaf::GetWidgetOfType(WidgetType tp)
{
	return (this->type == tp) ? this : NULL;
}

/**
 * Intialize nested widget tree and convert to widget array.
 * @param nwid Nested widget tree.
 * @param rtl  Direction of the language.
 * @param biggest_index Biggest index used in the nested widget tree.
 * @return Widget array with the converted widgets.
 * @note Caller should release returned widget array with \c free(widgets).
 * @ingroup NestedWidgets
 */
Widget *InitializeNWidgets(NWidgetBase *nwid, bool rtl, int biggest_index)
{
	/* Initialize nested widgets. */
	nwid->SetupSmallestSize(NULL, false);
	nwid->AssignSizePosition(ST_ARRAY, 0, 0, nwid->smallest_x, nwid->smallest_y, (nwid->resize_x > 0), (nwid->resize_y > 0), rtl);

	/* Construct a local widget array and initialize all its types to #WWT_LAST. */
	Widget *widgets = MallocT<Widget>(biggest_index + 2);
	int i;
	for (i = 0; i < biggest_index + 2; i++) {
		widgets[i].type = WWT_LAST;
	}

	/* Store nested widgets in the array. */
	nwid->StoreWidgets(widgets, biggest_index + 1, false, false, rtl);

	/* Check that all widgets are used. */
	for (i = 0; i < biggest_index + 2; i++) {
		if (widgets[i].type == WWT_LAST) break;
	}
	assert(i == biggest_index + 1);

	/* Fill terminating widget */
	static const Widget last_widget = {WIDGETS_END};
	widgets[biggest_index + 1] = last_widget;

	return widgets;
}

/**
 * Compare two widget arrays with each other, and report differences.
 * @param orig Pointer to original widget array.
 * @param gen  Pointer to generated widget array (from the nested widgets).
 * @param report Report differences to 'misc' debug stream.
 * @return Both widget arrays are equal.
 */
bool CompareWidgetArrays(const Widget *orig, const Widget *gen, bool report)
{
#define CHECK(var, prn) \
	if (ow->var != gw->var) { \
		same = false; \
		if (report) DEBUG(misc, 1, "index %d, \"" #var "\" field: original " prn ", generated " prn, idx, ow->var, gw->var); \
	}
#define CHECK_COORD(var) \
	if (ow->var != gw->var) { \
		same = false; \
		if (report) DEBUG(misc, 1, "index %d, \"" #var "\" field: original %d, generated %d, (difference %d)", idx, ow->var, gw->var, ow->var - gw->var); \
	}

	bool same = true;
	for(int idx = 0; ; idx++) {
		const Widget *ow = orig + idx;
		const Widget *gw = gen + idx;

		CHECK(type, "%d")
		CHECK(display_flags, "0x%x")
		CHECK(colour, "%d")
		CHECK_COORD(left)
		CHECK_COORD(right)
		CHECK_COORD(top)
		CHECK_COORD(bottom)
		CHECK(data, "%u")
		CHECK(tooltips, "%u")

		if (ow->type == WWT_LAST || gw->type == WWT_LAST) break;
	}

	return same;

#undef CHECK
#undef CHECK_COORD
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
 * @precond \c biggest_index != NULL.
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

			case WPT_FUNCTION: {
				if (*dest != NULL) return num_used;
				/* Ensure proper functioning even when the called code simply writes its largest index. */
				int biggest = -1;
				*dest = parts->u.func_ptr(&biggest);
				*biggest_index = max(*biggest_index, biggest);
				*fill_dest = false;
				break;
			}

			case NWID_SELECTION:
			case NWID_LAYERED:
				if (*dest != NULL) return num_used;
				*dest = new NWidgetStacked(parts->type);
				*fill_dest = true;
				break;


			case WPT_RESIZE: {
				NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(*dest);
				if (nwrb != NULL) {
					assert(parts->u.xy.x >= 0 && parts->u.xy.y >= 0);
					nwrb->SetResize(parts->u.xy.x, parts->u.xy.y);
				}
				break;
			}

			case WPT_RESIZE_PTR: {
				NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(*dest);
				if (nwrb != NULL) {
					assert(parts->u.xy_ptr->x >= 0 && parts->u.xy_ptr->y >= 0);
					nwrb->SetResize(parts->u.xy_ptr->x, parts->u.xy_ptr->y);
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

			case WPT_MINSIZE_PTR: {
				NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(*dest);
				if (nwrb != NULL) {
					assert(parts->u.xy_ptr->x >= 0 && parts->u.xy_ptr->y >= 0);
					nwrb->SetMinimalSize((uint)(parts->u.xy_ptr->x), (uint)(parts->u.xy_ptr->y));
				}
				break;
			}

			case WPT_FILL: {
				NWidgetResizeBase *nwrb = dynamic_cast<NWidgetResizeBase *>(*dest);
				if (nwrb != NULL) nwrb->SetFill(parts->u.xy.x != 0, parts->u.xy.y != 0);
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

			case WPT_DATATIP_PTR: {
				NWidgetCore *nwc = dynamic_cast<NWidgetCore *>(*dest);
				if (nwc != NULL) {
					nwc->widget_data = parts->u.datatip_ptr->data;
					nwc->tool_tip = parts->u.datatip_ptr->tooltip;
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

			case WPT_ENDCONTAINER:
				return num_used;

			default:
				if (*dest != NULL) return num_used;
				assert((parts->type & WWT_MASK) < NWID_HORIZONTAL);
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
 * @param parent Container to use for storing the child widgets.
 * @param biggest_index Pointer to biggest nested widget index in the tree.
 * @return Number of widget part elements used to fill the container.
 * @postcond \c *biggest_index contains the largest widget index of the tree and \c -1 if no index is used.
 */
static int MakeWidgetTree(const NWidgetPart *parts, int count, NWidgetBase *parent, int *biggest_index)
{
	/* Given parent must be either a #NWidgetContainer or a #NWidgetBackground object. */
	NWidgetContainer *nwid_cont = dynamic_cast<NWidgetContainer *>(parent);
	NWidgetBackground *nwid_parent = dynamic_cast<NWidgetBackground *>(parent);
	assert((nwid_cont != NULL && nwid_parent == NULL) || (nwid_cont == NULL && nwid_parent != NULL));

	int total_used = 0;
	while (true) {
		NWidgetBase *sub_widget = NULL;
		bool fill_sub = false;
		int num_used = MakeNWidget(parts, count - total_used, &sub_widget, &fill_sub, biggest_index);
		parts += num_used;
		total_used += num_used;

		/* Break out of loop when end reached */
		if (sub_widget == NULL) break;

		/* Add sub_widget to parent container. */
		if (nwid_cont) nwid_cont->Add(sub_widget);
		if (nwid_parent) nwid_parent->Add(sub_widget);

		/* If sub-widget is a container, recursively fill that container. */
		WidgetType tp = sub_widget->type;
		if (fill_sub && (tp == NWID_HORIZONTAL || tp == NWID_HORIZONTAL_LTR || tp == NWID_VERTICAL
							|| tp == WWT_PANEL || tp == WWT_FRAME || tp == WWT_INSET || tp == NWID_SELECTION || tp == NWID_LAYERED)) {
			int num_used = MakeWidgetTree(parts, count - total_used, sub_widget, biggest_index);
			parts += num_used;
			total_used += num_used;
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
 * @return Root of the nested widget tree, a vertical container containing the entire GUI.
 * @ingroup NestedWidgetParts
 * @precond \c biggest_index != NULL
 * @postcond \c *biggest_index contains the largest widget index of the tree and \c -1 if no index is used.
 */
NWidgetContainer *MakeNWidgets(const NWidgetPart *parts, int count, int *biggest_index)
{
	*biggest_index = -1;
	NWidgetContainer *cont = new NWidgetVertical();
	MakeWidgetTree(parts, count, cont, biggest_index);
	return cont;
}

/**
 * Construct a #Widget array from a nested widget parts array, taking care of all the steps and checks.
 * Also cache the result and use the cache if possible.
 * @param[in] parts        Array with parts of the widgets.
 * @param     parts_length Length of the \a parts array.
 * @param[in] orig_wid     Pointer to original widget array.
 * @param     wid_cache    Pointer to the cache for storing the generated widget array (use \c NULL to prevent caching).
 * @return Cached value if available, otherwise the generated widget array. If \a wid_cache is \c NULL, the caller should free the returned array.
 *
 * @pre Before the first call, \c *wid_cache should be \c NULL.
 * @post The widget array stored in the \c *wid_cache should be free-ed by the caller.
 */
const Widget *InitializeWidgetArrayFromNestedWidgets(const NWidgetPart *parts, int parts_length, const Widget *orig_wid, Widget **wid_cache)
{
	const bool rtl = false; // Direction of the language is left-to-right

	if (wid_cache != NULL && *wid_cache != NULL) return *wid_cache;

	assert(parts != NULL && parts_length > 0);
	int biggest_index = -1;
	NWidgetContainer *nwid = MakeNWidgets(parts, parts_length, &biggest_index);
	Widget *gen_wid = InitializeNWidgets(nwid, rtl, biggest_index);

	if (!rtl && orig_wid) {
		/* There are two descriptions, compare them.
		 * Comparing only makes sense when using a left-to-right language.
		 */
		bool ok = CompareWidgetArrays(orig_wid, gen_wid, false);
		if (ok) {
			DEBUG(misc, 1, "Nested widgets are equal, min-size(%u, %u)", nwid->smallest_x, nwid->smallest_y);
		} else {
			DEBUG(misc, 0, "Nested widgets give different results");
			CompareWidgetArrays(orig_wid, gen_wid, true);
		}
	}
	delete nwid;

	if (wid_cache != NULL) *wid_cache = gen_wid;
	return gen_wid;
}
