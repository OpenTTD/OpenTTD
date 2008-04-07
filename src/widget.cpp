/* $Id$ */

/** @file widget.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "core/math_func.hpp"
#include "player_func.h"
#include "gfx_func.h"
#include "window_gui.h"
#include "window_func.h"
#include "widgets/dropdown_func.h"

#include "table/sprites.h"
#include "table/strings.h"

static const char *UPARROW   = "\xEE\x8A\xA0";
static const char *DOWNARROW = "\xEE\x8A\xAA";

static Point HandleScrollbarHittest(const Scrollbar *sb, int top, int bottom)
{
	Point pt;
	int height, count, pos, cap;

	top += 10;
	bottom -= 9;

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

/** Special handling for the scrollbar widget type.
 * Handles the special scrolling buttons and other
 * scrolling.
 * @param w Window on which a scroll was performed.
 * @param wi Pointer to the scrollbar widget.
 * @param x The X coordinate of the mouse click.
 * @param y The Y coordinate of the mouse click. */
void ScrollbarClickHandler(Window *w, const Widget *wi, int x, int y)
{
	int mi, ma, pos;
	Scrollbar *sb;

	switch (wi->type) {
		case WWT_SCROLLBAR:
			/* vertical scroller */
			w->flags4 &= ~WF_HSCROLL;
			w->flags4 &= ~WF_SCROLL2;
			mi = wi->top;
			ma = wi->bottom;
			pos = y;
			sb = &w->vscroll;
			break;

		case WWT_SCROLL2BAR:
			/* 2nd vertical scroller */
			w->flags4 &= ~WF_HSCROLL;
			w->flags4 |= WF_SCROLL2;
			mi = wi->top;
			ma = wi->bottom;
			pos = y;
			sb = &w->vscroll2;
			break;

		case  WWT_HSCROLLBAR:
			/* horizontal scroller */
			w->flags4 &= ~WF_SCROLL2;
			w->flags4 |= WF_HSCROLL;
			mi = wi->left;
			ma = wi->right;
			pos = x;
			sb = &w->hscroll;
			break;

		default: NOT_REACHED();
	}
	if (pos <= mi+9) {
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
			if ((byte)(sb->pos + sb->cap) < sb->count)
				sb->pos++;
		}
		_left_button_clicked = false;
	} else {
		Point pt = HandleScrollbarHittest(sb, mi, ma);

		if (pos < pt.x) {
			sb->pos = max(sb->pos - sb->cap, 0);
		} else if (pos > pt.y) {
			sb->pos = min(
				sb->pos + sb->cap,
				max(sb->count - sb->cap, 0)
			);
		} else {
			_scrollbar_start_pos = pt.x - mi - 9;
			_scrollbar_size = ma - mi - 23;
			w->flags4 |= WF_SCROLL_MIDDLE;
			_scrolling_scrollbar = true;
			_cursorpos_drag_start = _cursor.pos;
		}
	}

	SetWindowDirty(w);
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
	uint index;
	int found_index = -1;

	/* Go through the widgets and check if we find the widget that the coordinate is
	 * inside. */
	for (index = 0; index < w->widget_count; index++) {
		const Widget *wi = &w->widget[index];
		if (wi->type == WWT_EMPTY || wi->type == WWT_FRAME) continue;

		if (x >= wi->left && x <= wi->right && y >= wi->top &&  y <= wi->bottom &&
				!w->IsWidgetHidden(index)) {
			found_index = index;
		}
	}

	return found_index;
}


void DrawFrameRect(int left, int top, int right, int bottom, int ctab, FrameFlags flags)
{
	uint dark         = _colour_gradient[ctab][3];
	uint medium_dark  = _colour_gradient[ctab][5];
	uint medium_light = _colour_gradient[ctab][6];
	uint light        = _colour_gradient[ctab][7];

	if (flags & FR_TRANSPARENT) {
		GfxFillRect(left, top, right, bottom, PALETTE_TO_TRANSPARENT | (1 << USE_COLORTABLE));
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
 * Paint all widgets of a window.
 * @param w Window
 */
void DrawWindowWidgets(const Window *w)
{
	const DrawPixelInfo* dpi = _cur_dpi;

	for (uint i = 0; i < w->widget_count; i++) {
		const Widget *wi = &w->widget[i];
		bool clicked = w->IsWidgetLowered(i);
		Rect r;

		if (dpi->left > (r.right = wi->right) ||
				dpi->left + dpi->width <= (r.left = wi->left) ||
				dpi->top > (r.bottom = wi->bottom) ||
				dpi->top + dpi->height <= (r.top = wi->top) ||
				w->IsWidgetHidden(i)) {
			continue;
		}

		switch (wi->type & WWT_MASK) {
		case WWT_IMGBTN:
		case WWT_IMGBTN_2: {
			SpriteID img = wi->data;
			assert(img != 0);
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);

			/* show different image when clicked for WWT_IMGBTN_2 */
			if ((wi->type & WWT_MASK) == WWT_IMGBTN_2 && clicked) img++;
			DrawSprite(img, PAL_NONE, r.left + 1 + clicked, r.top + 1 + clicked);
			break;
		}

		case WWT_PANEL:
			assert(wi->data == 0);
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			break;

		case WWT_EDITBOX:
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, FR_LOWERED | FR_DARKENED);
			break;

		case WWT_TEXTBTN:
		case WWT_TEXTBTN_2:
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			/* FALL THROUGH */

		case WWT_LABEL: {
			StringID str = wi->data;

			if ((wi->type & WWT_MASK) == WWT_TEXTBTN_2 && clicked) str++;

			DrawStringCentered(((r.left + r.right + 1) >> 1) + clicked, ((r.top + r.bottom + 1) >> 1) - 5 + clicked, str, TC_FROMSTRING);
			break;
		}

		case WWT_TEXT: {
			const StringID str = wi->data;

			if (str != STR_NULL) DrawStringTruncated(r.left, r.top, str, wi->color, r.right - r.left);
			break;
		}

		case WWT_INSET: {
			const StringID str = wi->data;
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, FR_LOWERED | FR_DARKENED);

			if (str != STR_NULL) DrawStringTruncated(r.left + 2, r.top + 1, str, TC_FROMSTRING, r.right - r.left - 10);
			break;
		}

		case WWT_MATRIX: {
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);

			int c = GB(wi->data, 0, 8);
			int amt1 = (wi->right - wi->left + 1) / c;

			int d = GB(wi->data, 8, 8);
			int amt2 = (wi->bottom - wi->top + 1) / d;

			int color = _colour_gradient[wi->color & 0xF][6];

			int x = r.left;
			for (int ctr = c; ctr > 1; ctr--) {
				x += amt1;
				GfxFillRect(x, r.top + 1, x, r.bottom - 1, color);
			}

			x = r.top;
			for (int ctr = d; ctr > 1; ctr--) {
				x += amt2;
				GfxFillRect(r.left + 1, x, r.right - 1, x, color);
			}

			color = _colour_gradient[wi->color & 0xF][4];

			x = r.left - 1;
			for (int ctr = c; ctr > 1; ctr--) {
				x += amt1;
				GfxFillRect(x, r.top + 1, x, r.bottom - 1, color);
			}

			x = r.top - 1;
			for (int ctr = d; ctr > 1; ctr--) {
				x += amt2;
				GfxFillRect(r.left + 1, x, r.right - 1, x, color);
			}

			break;
		}

		/* vertical scrollbar */
		case WWT_SCROLLBAR: {
			assert(wi->data == 0);
			assert(r.right - r.left == 11); // To ensure the same sizes are used everywhere!

			/* draw up/down buttons */
			clicked = ((w->flags4 & (WF_SCROLL_UP | WF_HSCROLL | WF_SCROLL2)) == WF_SCROLL_UP);
			DrawFrameRect(r.left, r.top, r.right, r.top + 9, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			DoDrawString(UPARROW, r.left + 2 + clicked, r.top + clicked, TC_BLACK);

			clicked = (((w->flags4 & (WF_SCROLL_DOWN | WF_HSCROLL | WF_SCROLL2)) == WF_SCROLL_DOWN));
			DrawFrameRect(r.left, r.bottom - 9, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			DoDrawString(DOWNARROW, r.left + 2 + clicked, r.bottom - 9 + clicked, TC_BLACK);

			int c1 = _colour_gradient[wi->color & 0xF][3];
			int c2 = _colour_gradient[wi->color & 0xF][7];

			/* draw "shaded" background */
			GfxFillRect(r.left, r.top + 10, r.right, r.bottom - 10, c2);
			GfxFillRect(r.left, r.top + 10, r.right, r.bottom - 10, c1 | (1 << PALETTE_MODIFIER_GREYOUT));

			/* draw shaded lines */
			GfxFillRect(r.left + 2, r.top + 10, r.left + 2, r.bottom - 10, c1);
			GfxFillRect(r.left + 3, r.top + 10, r.left + 3, r.bottom - 10, c2);
			GfxFillRect(r.left + 7, r.top + 10, r.left + 7, r.bottom - 10, c1);
			GfxFillRect(r.left + 8, r.top + 10, r.left + 8, r.bottom - 10, c2);

			Point pt = HandleScrollbarHittest(&w->vscroll, r.top, r.bottom);
			DrawFrameRect(r.left, pt.x, r.right, pt.y, wi->color, (w->flags4 & (WF_SCROLL_MIDDLE | WF_HSCROLL | WF_SCROLL2)) == WF_SCROLL_MIDDLE ? FR_LOWERED : FR_NONE);
			break;
		}

		case WWT_SCROLL2BAR: {
			assert(wi->data == 0);
			assert(r.right - r.left == 11); // To ensure the same sizes are used everywhere!

			/* draw up/down buttons */
			clicked = ((w->flags4 & (WF_SCROLL_UP | WF_HSCROLL | WF_SCROLL2)) == (WF_SCROLL_UP | WF_SCROLL2));
			DrawFrameRect(r.left, r.top, r.right, r.top + 9, wi->color,  (clicked) ? FR_LOWERED : FR_NONE);
			DoDrawString(UPARROW, r.left + 2 + clicked, r.top + clicked, TC_BLACK);

			clicked = ((w->flags4 & (WF_SCROLL_DOWN | WF_HSCROLL | WF_SCROLL2)) == (WF_SCROLL_DOWN | WF_SCROLL2));
			DrawFrameRect(r.left, r.bottom - 9, r.right, r.bottom, wi->color,  (clicked) ? FR_LOWERED : FR_NONE);
			DoDrawString(DOWNARROW, r.left + 2 + clicked, r.bottom - 9 + clicked, TC_BLACK);

			int c1 = _colour_gradient[wi->color & 0xF][3];
			int c2 = _colour_gradient[wi->color & 0xF][7];

			/* draw "shaded" background */
			GfxFillRect(r.left, r.top + 10, r.right, r.bottom - 10, c2);
			GfxFillRect(r.left, r.top + 10, r.right, r.bottom - 10, c1 | (1 << PALETTE_MODIFIER_GREYOUT));

			/* draw shaded lines */
			GfxFillRect(r.left + 2, r.top + 10, r.left + 2, r.bottom - 10, c1);
			GfxFillRect(r.left + 3, r.top + 10, r.left + 3, r.bottom - 10, c2);
			GfxFillRect(r.left + 7, r.top + 10, r.left + 7, r.bottom - 10, c1);
			GfxFillRect(r.left + 8, r.top + 10, r.left + 8, r.bottom - 10, c2);

			Point pt = HandleScrollbarHittest(&w->vscroll2, r.top, r.bottom);
			DrawFrameRect(r.left, pt.x, r.right, pt.y, wi->color, (w->flags4 & (WF_SCROLL_MIDDLE | WF_HSCROLL | WF_SCROLL2)) == (WF_SCROLL_MIDDLE | WF_SCROLL2) ? FR_LOWERED : FR_NONE);
			break;
		}

		/* horizontal scrollbar */
		case WWT_HSCROLLBAR: {
			assert(wi->data == 0);
			assert(r.bottom - r.top == 11); // To ensure the same sizes are used everywhere!

			clicked = ((w->flags4 & (WF_SCROLL_UP | WF_HSCROLL)) == (WF_SCROLL_UP | WF_HSCROLL));
			DrawFrameRect(r.left, r.top, r.left + 9, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			DrawSprite(SPR_ARROW_LEFT, PAL_NONE, r.left + 1 + clicked, r.top + 1 + clicked);

			clicked = ((w->flags4 & (WF_SCROLL_DOWN | WF_HSCROLL)) == (WF_SCROLL_DOWN | WF_HSCROLL));
			DrawFrameRect(r.right - 9, r.top, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			DrawSprite(SPR_ARROW_RIGHT, PAL_NONE, r.right - 8 + clicked, r.top + 1 + clicked);

			int c1 = _colour_gradient[wi->color & 0xF][3];
			int c2 = _colour_gradient[wi->color & 0xF][7];

			/* draw "shaded" background */
			GfxFillRect(r.left + 10, r.top, r.right - 10, r.bottom, c2);
			GfxFillRect(r.left + 10, r.top, r.right - 10, r.bottom, c1 | (1 << PALETTE_MODIFIER_GREYOUT));

			/* draw shaded lines */
			GfxFillRect(r.left + 10, r.top + 2, r.right - 10, r.top + 2, c1);
			GfxFillRect(r.left + 10, r.top + 3, r.right - 10, r.top + 3, c2);
			GfxFillRect(r.left + 10, r.top + 7, r.right - 10, r.top + 7, c1);
			GfxFillRect(r.left + 10, r.top + 8, r.right - 10, r.top + 8, c2);

			/* draw actual scrollbar */
			Point pt = HandleScrollbarHittest(&w->hscroll, r.left, r.right);
			DrawFrameRect(pt.x, r.top, pt.y, r.bottom, wi->color, (w->flags4 & (WF_SCROLL_MIDDLE | WF_HSCROLL)) == (WF_SCROLL_MIDDLE | WF_HSCROLL) ? FR_LOWERED : FR_NONE);

			break;
		}

		case WWT_FRAME: {
			const StringID str = wi->data;
			int x2 = r.left; // by default the left side is the left side of the widget

			if (str != STR_NULL) x2 = DrawString(r.left + 6, r.top, str, TC_FROMSTRING);

			int c1 = _colour_gradient[wi->color][3];
			int c2 = _colour_gradient[wi->color][7];

			/* Line from upper left corner to start of text */
			GfxFillRect(r.left, r.top + 4, r.left + 4, r.top + 4, c1);
			GfxFillRect(r.left + 1, r.top + 5, r.left + 4, r.top + 5, c2);

			/* Line from end of text to upper right corner */
			GfxFillRect(x2, r.top + 4, r.right - 1, r.top + 4, c1);
			GfxFillRect(x2, r.top + 5, r.right - 2, r.top + 5, c2);

			/* Line from upper left corner to bottom left corner */
			GfxFillRect(r.left, r.top + 5, r.left, r.bottom - 1, c1);
			GfxFillRect(r.left + 1, r.top + 6, r.left + 1, r.bottom - 2, c2);

			/*Line from upper right corner to bottom right corner */
			GfxFillRect(r.right - 1, r.top + 5, r.right - 1, r.bottom - 2, c1);
			GfxFillRect(r.right, r.top + 4, r.right, r.bottom - 1, c2);

			GfxFillRect(r.left + 1, r.bottom - 1, r.right - 1, r.bottom - 1, c1);
			GfxFillRect(r.left, r.bottom, r.right, r.bottom, c2);

			break;
		}

		case WWT_STICKYBOX:
			assert(wi->data == 0);
			assert(r.right - r.left == 11); // To ensure the same sizes are used everywhere!

			clicked = !!(w->flags4 & WF_STICKY);
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			DrawSprite((clicked) ? SPR_PIN_UP : SPR_PIN_DOWN, PAL_NONE, r.left + 2 + clicked, r.top + 3 + clicked);
			break;

		case WWT_RESIZEBOX:
			assert(wi->data == 0);
			assert(r.right - r.left == 11); // To ensure the same sizes are used everywhere!

			clicked = !!(w->flags4 & WF_SIZING);
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			DrawSprite(SPR_WINDOW_RESIZE, PAL_NONE, r.left + 3 + clicked, r.top + 3 + clicked);
			break;

		case WWT_CLOSEBOX: {
			const StringID str = wi->data;

			assert(str == STR_00C5 || str == STR_00C6); // black or silver cross
			assert(r.right - r.left == 10); // To ensure the same sizes are used everywhere

			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, FR_NONE);
			DrawString(r.left + 2, r.top + 2, str, TC_FROMSTRING);
			break;
		}

		case WWT_CAPTION:
			assert(r.bottom - r.top == 13); // To ensure the same sizes are used everywhere!
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, FR_BORDERONLY);
			DrawFrameRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, wi->color, (w->caption_color == 0xFF) ? FR_LOWERED | FR_DARKENED : FR_LOWERED | FR_DARKENED | FR_BORDERONLY);

			if (w->caption_color != 0xFF) {
				GfxFillRect(r.left + 2, r.top + 2, r.right - 2, r.bottom - 2, _colour_gradient[_player_colors[w->caption_color]][4]);
			}

			DrawStringCenteredTruncated(r.left + 2, r.right - 2, r.top + 2, wi->data, 0x84);
			break;

		case WWT_DROPDOWN: {
			assert(r.bottom - r.top == 11); // ensure consistent size

			StringID str = wi->data;
			DrawFrameRect(r.left, r.top, r.right - 12, r.bottom, wi->color, FR_NONE);
			DrawFrameRect(r.right - 11, r.top, r.right, r.bottom, wi->color, clicked ? FR_LOWERED : FR_NONE);
			DrawString(r.right - (clicked ? 8 : 9), r.top + (clicked ? 2 : 1), STR_0225, TC_BLACK);
			if (str != STR_NULL) DrawStringTruncated(r.left + 2, r.top + 1, str, TC_BLACK, r.right - r.left - 12);
			break;
		}

		case WWT_DROPDOWNIN: {
			assert(r.bottom - r.top == 11); // ensure consistent size

			StringID str = wi->data;
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, FR_LOWERED | FR_DARKENED);
			DrawFrameRect(r.right - 11, r.top + 1, r.right - 1, r.bottom - 1, wi->color, clicked ? FR_LOWERED : FR_NONE);
			DrawString(r.right - (clicked ? 8 : 9), r.top + (clicked ? 2 : 1), STR_0225, TC_BLACK);
			if (str != STR_NULL) DrawStringTruncated(r.left + 2, r.top + 2, str, TC_BLACK, r.right - r.left - 12);
			break;
		}
		}

		if (w->IsWidgetDisabled(i)) {
			GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, _colour_gradient[wi->color & 0xF][2] | (1 << PALETTE_MODIFIER_GREYOUT));
		}
	}


	if (w->flags4 & WF_WHITE_BORDER_MASK) {
		DrawFrameRect(0, 0, w->width - 1, w->height - 1, 0xF, FR_BORDERONLY);
	}

}

static void ResizeWidgets(Window *w, byte a, byte b)
{
	int16 offset = w->widget[a].left;
	int16 length = w->widget[b].right - offset;

	w->widget[a].right = (length / 2) + offset;

	w->widget[b].left  = w->widget[a].right + 1;
}

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
void ResizeWindowForWidget(Window *w, int widget, int delta_x, int delta_y)
{
	int right  = w->widget[widget].right;
	int bottom = w->widget[widget].bottom;

	for (uint i = 0; i < w->widget_count; i++) {
		if (w->widget[i].left >= right) w->widget[i].left += delta_x;
		if (w->widget[i].right >= right) w->widget[i].right += delta_x;
		if (w->widget[i].top >= bottom) w->widget[i].top += delta_y;
		if (w->widget[i].bottom >= bottom) w->widget[i].bottom += delta_y;
	}

	w->width  += delta_x;
	w->height += delta_y;
	w->resize.width  += delta_x;
	w->resize.height += delta_y;
}

/** Draw a sort button's up or down arrow symbol.
 * @param w Window of widget
 * @param widget Sort button widget
 * @param state State of sort button
 */
void DrawSortButtonState(const Window *w, int widget, SortButtonState state)
{
	if (state == SBS_OFF) return;

	int offset = w->IsWidgetLowered(widget) ? 1 : 0;
	DoDrawString(state == SBS_DOWN ? DOWNARROW : UPARROW, w->widget[widget].right - 11 + offset, w->widget[widget].top + 1 + offset, TC_BLACK);
}
