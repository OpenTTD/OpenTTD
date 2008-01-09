/* $Id$ */

/** @file widget.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "player.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "gfx_func.h"
#include "window_gui.h"
#include "window_func.h"


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
		case WWT_SCROLLBAR: {
			/* vertical scroller */
			w->flags4 &= ~WF_HSCROLL;
			w->flags4 &= ~WF_SCROLL2;
			mi = wi->top;
			ma = wi->bottom;
			pos = y;
			sb = &w->vscroll;
			break;
		}
		case WWT_SCROLL2BAR: {
			/* 2nd vertical scroller */
			w->flags4 &= ~WF_HSCROLL;
			w->flags4 |= WF_SCROLL2;
			mi = wi->top;
			ma = wi->bottom;
			pos = y;
			sb = &w->vscroll2;
			break;
		}
		case  WWT_HSCROLLBAR: {
			/* horizontal scroller */
			w->flags4 &= ~WF_SCROLL2;
			w->flags4 |= WF_HSCROLL;
			mi = wi->left;
			ma = wi->right;
			pos = x;
			sb = &w->hscroll;
			break;
		}
		default: return; //this should never happen
	}
	if (pos <= mi+9) {
		/* Pressing the upper button? */
		w->flags4 |= WF_SCROLL_UP;
		if (_scroller_click_timeout == 0) {
			_scroller_click_timeout = 6;
			if (sb->pos != 0) sb->pos--;
		}
		_left_button_clicked = false;
	} else if (pos >= ma-10) {
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
			goto draw_default;
		}

		case WWT_PANEL: {
			assert(wi->data == 0);
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			goto draw_default;
		}

		case WWT_TEXTBTN:
		case WWT_TEXTBTN_2: {
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			}
		/* fall through */

		case WWT_LABEL: {
			StringID str = wi->data;

			if ((wi->type & WWT_MASK) == WWT_TEXTBTN_2 && clicked) str++;

			DrawStringCentered(((r.left + r.right + 1) >> 1) + clicked, ((r.top + r.bottom + 1) >> 1) - 5 + clicked, str, TC_FROMSTRING);
			goto draw_default;
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
			goto draw_default;
		}

		case WWT_MATRIX: {
			int c, d, ctr;
			int x, amt1, amt2;
			int color;

			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);

			c = GB(wi->data, 0, 8);
			amt1 = (wi->right - wi->left + 1) / c;

			d = GB(wi->data, 8, 8);
			amt2 = (wi->bottom - wi->top + 1) / d;

			color = _colour_gradient[wi->color & 0xF][6];

			x = r.left;
			for (ctr = c; ctr > 1; ctr--) {
				x += amt1;
				GfxFillRect(x, r.top + 1, x, r.bottom - 1, color);
			}

			x = r.top;
			for (ctr = d; ctr > 1; ctr--) {
				x += amt2;
				GfxFillRect(r.left + 1, x, r.right - 1, x, color);
			}

			color = _colour_gradient[wi->color & 0xF][4];

			x = r.left - 1;
			for (ctr = c; ctr > 1; ctr--) {
				x += amt1;
				GfxFillRect(x, r.top + 1, x, r.bottom - 1, color);
			}

			x = r.top - 1;
			for (ctr = d; ctr > 1; ctr--) {
				x += amt2;
				GfxFillRect(r.left + 1, x, r.right - 1, x, color);
			}

			goto draw_default;
		}

		/* vertical scrollbar */
		case WWT_SCROLLBAR: {
			Point pt;
			int c1, c2;

			assert(wi->data == 0);
			assert(r.right - r.left == 11); // XXX - to ensure the same sizes are used everywhere!

			/* draw up/down buttons */
			clicked = ((w->flags4 & (WF_SCROLL_UP | WF_HSCROLL | WF_SCROLL2)) == WF_SCROLL_UP);
			DrawFrameRect(r.left, r.top, r.right, r.top + 9, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			DoDrawString(UPARROW, r.left + 2 + clicked, r.top + clicked, TC_BLACK);

			clicked = (((w->flags4 & (WF_SCROLL_DOWN | WF_HSCROLL | WF_SCROLL2)) == WF_SCROLL_DOWN));
			DrawFrameRect(r.left, r.bottom - 9, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			DoDrawString(DOWNARROW, r.left + 2 + clicked, r.bottom - 9 + clicked, TC_BLACK);

			c1 = _colour_gradient[wi->color & 0xF][3];
			c2 = _colour_gradient[wi->color & 0xF][7];

			/* draw "shaded" background */
			GfxFillRect(r.left, r.top + 10, r.right, r.bottom - 10, c2);
			GfxFillRect(r.left, r.top + 10, r.right, r.bottom - 10, c1 | (1 << PALETTE_MODIFIER_GREYOUT));

			/* draw shaded lines */
			GfxFillRect(r.left + 2, r.top + 10, r.left + 2, r.bottom - 10, c1);
			GfxFillRect(r.left + 3, r.top + 10, r.left + 3, r.bottom - 10, c2);
			GfxFillRect(r.left + 7, r.top + 10, r.left + 7, r.bottom - 10, c1);
			GfxFillRect(r.left + 8, r.top + 10, r.left + 8, r.bottom - 10, c2);

			pt = HandleScrollbarHittest(&w->vscroll, r.top, r.bottom);
			DrawFrameRect(r.left, pt.x, r.right, pt.y, wi->color, (w->flags4 & (WF_SCROLL_MIDDLE | WF_HSCROLL | WF_SCROLL2)) == WF_SCROLL_MIDDLE ? FR_LOWERED : FR_NONE);
			break;
		}
		case WWT_SCROLL2BAR: {
			Point pt;
			int c1, c2;

			assert(wi->data == 0);
			assert(r.right - r.left == 11); // XXX - to ensure the same sizes are used everywhere!

			/* draw up/down buttons */
			clicked = ((w->flags4 & (WF_SCROLL_UP | WF_HSCROLL | WF_SCROLL2)) == (WF_SCROLL_UP | WF_SCROLL2));
			DrawFrameRect(r.left, r.top, r.right, r.top + 9, wi->color,  (clicked) ? FR_LOWERED : FR_NONE);
			DoDrawString(UPARROW, r.left + 2 + clicked, r.top + clicked, TC_BLACK);

			clicked = ((w->flags4 & (WF_SCROLL_DOWN | WF_HSCROLL | WF_SCROLL2)) == (WF_SCROLL_DOWN | WF_SCROLL2));
			DrawFrameRect(r.left, r.bottom - 9, r.right, r.bottom, wi->color,  (clicked) ? FR_LOWERED : FR_NONE);
			DoDrawString(DOWNARROW, r.left + 2 + clicked, r.bottom - 9 + clicked, TC_BLACK);

			c1 = _colour_gradient[wi->color & 0xF][3];
			c2 = _colour_gradient[wi->color & 0xF][7];

			/* draw "shaded" background */
			GfxFillRect(r.left, r.top + 10, r.right, r.bottom - 10, c2);
			GfxFillRect(r.left, r.top + 10, r.right, r.bottom - 10, c1 | (1 << PALETTE_MODIFIER_GREYOUT));

			/* draw shaded lines */
			GfxFillRect(r.left + 2, r.top + 10, r.left + 2, r.bottom - 10, c1);
			GfxFillRect(r.left + 3, r.top + 10, r.left + 3, r.bottom - 10, c2);
			GfxFillRect(r.left + 7, r.top + 10, r.left + 7, r.bottom - 10, c1);
			GfxFillRect(r.left + 8, r.top + 10, r.left + 8, r.bottom - 10, c2);

			pt = HandleScrollbarHittest(&w->vscroll2, r.top, r.bottom);
			DrawFrameRect(r.left, pt.x, r.right, pt.y, wi->color, (w->flags4 & (WF_SCROLL_MIDDLE | WF_HSCROLL | WF_SCROLL2)) == (WF_SCROLL_MIDDLE | WF_SCROLL2) ? FR_LOWERED : FR_NONE);
			break;
		}

		/* horizontal scrollbar */
		case WWT_HSCROLLBAR: {
			Point pt;
			int c1, c2;

			assert(wi->data == 0);
			assert(r.bottom - r.top == 11); // XXX - to ensure the same sizes are used everywhere!

			clicked = ((w->flags4 & (WF_SCROLL_UP | WF_HSCROLL)) == (WF_SCROLL_UP | WF_HSCROLL));
			DrawFrameRect(r.left, r.top, r.left + 9, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			DrawSprite(SPR_ARROW_LEFT, PAL_NONE, r.left + 1 + clicked, r.top + 1 + clicked);

			clicked = ((w->flags4 & (WF_SCROLL_DOWN | WF_HSCROLL)) == (WF_SCROLL_DOWN | WF_HSCROLL));
			DrawFrameRect(r.right - 9, r.top, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			DrawSprite(SPR_ARROW_RIGHT, PAL_NONE, r.right - 8 + clicked, r.top + 1 + clicked);

			c1 = _colour_gradient[wi->color & 0xF][3];
			c2 = _colour_gradient[wi->color & 0xF][7];

			/* draw "shaded" background */
			GfxFillRect(r.left + 10, r.top, r.right - 10, r.bottom, c2);
			GfxFillRect(r.left + 10, r.top, r.right - 10, r.bottom, c1 | (1 << PALETTE_MODIFIER_GREYOUT));

			/* draw shaded lines */
			GfxFillRect(r.left + 10, r.top + 2, r.right - 10, r.top + 2, c1);
			GfxFillRect(r.left + 10, r.top + 3, r.right - 10, r.top + 3, c2);
			GfxFillRect(r.left + 10, r.top + 7, r.right - 10, r.top + 7, c1);
			GfxFillRect(r.left + 10, r.top + 8, r.right - 10, r.top + 8, c2);

			/* draw actual scrollbar */
			pt = HandleScrollbarHittest(&w->hscroll, r.left, r.right);
			DrawFrameRect(pt.x, r.top, pt.y, r.bottom, wi->color, (w->flags4 & (WF_SCROLL_MIDDLE | WF_HSCROLL)) == (WF_SCROLL_MIDDLE | WF_HSCROLL) ? FR_LOWERED : FR_NONE);

			break;
		}

		case WWT_FRAME: {
			const StringID str = wi->data;
			int c1, c2;
			int x2 = r.left; // by default the left side is the left side of the widget

			if (str != STR_NULL) x2 = DrawString(r.left + 6, r.top, str, TC_FROMSTRING);

			c1 = _colour_gradient[wi->color][3];
			c2 = _colour_gradient[wi->color][7];

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

			goto draw_default;
		}

		case WWT_STICKYBOX: {
			assert(wi->data == 0);
			assert(r.right - r.left == 11); // XXX - to ensure the same sizes are used everywhere!

			clicked = !!(w->flags4 & WF_STICKY);
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			DrawSprite((clicked) ? SPR_PIN_UP : SPR_PIN_DOWN, PAL_NONE, r.left + 2 + clicked, r.top + 3 + clicked);
			break;
		}

		case WWT_RESIZEBOX: {
			assert(wi->data == 0);
			assert(r.right - r.left == 11); // XXX - to ensure the same sizes are used everywhere!

			clicked = !!(w->flags4 & WF_SIZING);
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, (clicked) ? FR_LOWERED : FR_NONE);
			DrawSprite(SPR_WINDOW_RESIZE, PAL_NONE, r.left + 3 + clicked, r.top + 3 + clicked);
			break;
		}

		case WWT_CLOSEBOX: {
			const StringID str = wi->data;

			assert(str == STR_00C5 || str == STR_00C6); // black or silver cross
			assert(r.right - r.left == 10); // ensure the same sizes are used everywhere

			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, FR_NONE);
			DrawString(r.left + 2, r.top + 2, str, TC_FROMSTRING);
			break;
		}

		case WWT_CAPTION: {
			assert(r.bottom - r.top == 13); // XXX - to ensure the same sizes are used everywhere!
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, FR_BORDERONLY);
			DrawFrameRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, wi->color, (w->caption_color == 0xFF) ? FR_LOWERED | FR_DARKENED : FR_LOWERED | FR_DARKENED | FR_BORDERONLY);

			if (w->caption_color != 0xFF) {
				GfxFillRect(r.left + 2, r.top + 2, r.right - 2, r.bottom - 2, _colour_gradient[_player_colors[w->caption_color]][4]);
			}

			DrawStringCenteredTruncated(r.left + 2, r.right - 2, r.top + 2, wi->data, 0x84);
draw_default:;
			if (w->IsWidgetDisabled(i)) {
				GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, _colour_gradient[wi->color & 0xF][2] | (1 << PALETTE_MODIFIER_GREYOUT));
			}
		}
		}
	}


	if (w->flags4 & WF_WHITE_BORDER_MASK) {
		DrawFrameRect(0, 0, w->width - 1, w->height - 1, 0xF, FR_BORDERONLY);
	}

}

static const Widget _dropdown_menu_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,     0,     0, 0,     0, 0, 0x0, STR_NULL},
{  WWT_SCROLLBAR,   RESIZE_NONE,     0,     0, 0,     0, 0, 0x0, STR_0190_SCROLL_BAR_SCROLLS_LIST},
{   WIDGETS_END},
};

static int GetDropdownItem(const Window *w)
{
	byte item, counter;
	int y;

	if (GetWidgetFromPos(w, _cursor.pos.x - w->left, _cursor.pos.y - w->top) < 0)
		return -1;

	y = _cursor.pos.y - w->top - 2 + w->vscroll.pos * 10;

	if (y < 0)
		return - 1;

	item = y / 10;
	if (item >= WP(w, dropdown_d).num_items || (HasBit(WP(w,dropdown_d).disabled_state, item) && !HasBit(WP(w,dropdown_d).hidden_state, item)) || WP(w,dropdown_d).items[item] == 0)
		return - 1;

	/* Skip hidden items -- +1 for each hidden item before the clicked item. */
	for (counter = 0; item >= counter; ++counter)
		if (HasBit(WP(w, dropdown_d).hidden_state, counter)) item++;

	return item;
}

static void DropdownMenuWndProc(Window *w, WindowEvent *e)
{
	int item;

	switch (e->event) {
		case WE_PAINT: {
			int x,y,i,sel;
			int width, height;

			DrawWindowWidgets(w);

			x = 1;
			y = 2 - w->vscroll.pos * 10;

			sel    = WP(w, dropdown_d).selected_index;
			width  = w->widget[0].right - 3;
			height = w->widget[0].bottom - 3;

			for (i = 0; WP(w, dropdown_d).items[i] != INVALID_STRING_ID; i++, sel--) {
				if (HasBit(WP(w, dropdown_d).hidden_state, i)) continue;

				if (y >= 0 && y <= height) {
					if (WP(w, dropdown_d).items[i] != STR_NULL) {
						if (sel == 0) GfxFillRect(x + 1, y, x + width, y + 9, 0);
						DrawStringTruncated(x + 2, y, WP(w, dropdown_d).items[i], sel == 0 ? TC_WHITE : TC_BLACK, x + width);

						if (HasBit(WP(w, dropdown_d).disabled_state, i)) {
							GfxFillRect(x, y, x + width, y + 9,
								(1 << PALETTE_MODIFIER_GREYOUT) | _colour_gradient[_dropdown_menu_widgets[0].color][5]
							);
						}
					} else {
						int c1 = _colour_gradient[_dropdown_menu_widgets[0].color][3];
						int c2 = _colour_gradient[_dropdown_menu_widgets[0].color][7];

						GfxFillRect(x + 1, y + 3, x + w->width - 5, y + 3, c1);
						GfxFillRect(x + 1, y + 4, x + w->width - 5, y + 4, c2);
					}
				}
				y += 10;
			}
		} break;

		case WE_CLICK: {
			if (e->we.click.widget != 0) break;
			item = GetDropdownItem(w);
			if (item >= 0) {
				WP(w, dropdown_d).click_delay = 4;
				WP(w, dropdown_d).selected_index = item;
				SetWindowDirty(w);
			}
		} break;

		case WE_MOUSELOOP: {
			Window *w2 = FindWindowById(WP(w, dropdown_d).parent_wnd_class, WP(w,dropdown_d).parent_wnd_num);
			if (w2 == NULL) {
				DeleteWindow(w);
				return;
			}

			if (WP(w, dropdown_d).click_delay != 0 && --WP(w,dropdown_d).click_delay == 0) {
				WindowEvent e;
				e.event = WE_DROPDOWN_SELECT;
				e.we.dropdown.button = WP(w, dropdown_d).parent_button;
				e.we.dropdown.index  = WP(w, dropdown_d).selected_index;
				w2->wndproc(w2, &e);
				DeleteWindow(w);
				return;
			}

			if (WP(w, dropdown_d).drag_mode) {
				item = GetDropdownItem(w);

				if (!_left_button_clicked) {
					WP(w, dropdown_d).drag_mode = false;
					if (item < 0) return;
					WP(w, dropdown_d).click_delay = 2;
				} else {
					if (item < 0) return;
				}

				WP(w, dropdown_d).selected_index = item;
				SetWindowDirty(w);
			}
		} break;

		case WE_DESTROY: {
			Window *w2 = FindWindowById(WP(w, dropdown_d).parent_wnd_class, WP(w,dropdown_d).parent_wnd_num);
			if (w2 != NULL) {
				w2->RaiseWidget(WP(w, dropdown_d).parent_button);
				w2->InvalidateWidget(WP(w, dropdown_d).parent_button);
			}
		} break;
	}
}

void ShowDropDownMenu(Window *w, const StringID *strings, int selected, int button, uint32 disabled_mask, uint32 hidden_mask)
{
	int i;
	const Widget *wi;
	Window *w2;
	const Window *w3;
	bool is_dropdown_menu_shown = w->IsWidgetLowered(button);
	int top, height;
	int screen_top, screen_bottom;
	bool scroll = false;

	DeleteWindowById(WC_DROPDOWN_MENU, 0);

	if (is_dropdown_menu_shown) return;

	w->LowerWidget(button);

	w->InvalidateWidget(button);

	for (i = 0; strings[i] != INVALID_STRING_ID; i++) {}
	if (i == 0) return;

	wi = &w->widget[button];

	if (hidden_mask != 0) {
		uint j;

		for (j = 0; strings[j] != INVALID_STRING_ID; j++) {
			if (HasBit(hidden_mask, j)) i--;
		}
	}

	/* The preferred position is just below the dropdown calling widget */
	top = w->top + wi->bottom + 2;
	height = i * 10 + 4;

	w3 = FindWindowById(WC_STATUS_BAR, 0);
	screen_bottom = w3 == NULL ? _screen.height : w3->top;

	/* Check if the dropdown will fully fit below the widget */
	if (top + height >= screen_bottom) {
		w3 = FindWindowById(WC_MAIN_TOOLBAR, 0);
		screen_top = w3 == NULL ? 0 : w3->top + w3->height;

		/* If not, check if it will fit above the widget */
		if (w->top + wi->top - height - 1 > screen_top) {
			top = w->top + wi->top - height - 1;
		} else {
			/* ... and lastly if it won't, enable the scroll bar and fit the
			 * list in below the widget */
			int rows = (screen_bottom - 4 - top) / 10;
			height = rows * 10 + 4;
			scroll = true;
		}
	}

	w2 = AllocateWindow(
		w->left + wi[-1].left + 1,
		top,
		wi->right - wi[-1].left + 1,
		height,
		DropdownMenuWndProc,
		WC_DROPDOWN_MENU,
		_dropdown_menu_widgets);

	w2->widget[0].color = wi->color;
	w2->widget[0].right = wi->right - wi[-1].left;
	w2->widget[0].bottom = height - 1;

	w2->SetWidgetHiddenState(1, !scroll);

	if (scroll) {
		/* We're scrolling, so enable the scroll bar and shrink the list by
		 * the scrollbar's width */
		w2->widget[1].color  = wi->color;
		w2->widget[1].right  = w2->widget[0].right;
		w2->widget[1].left   = w2->widget[1].right - 11;
		w2->widget[1].bottom = height - 1;
		w2->widget[0].right -= 12;

		w2->vscroll.cap   = (height - 4) / 10;
		w2->vscroll.count = i;
	}

	w2->desc_flags = WDF_DEF_WIDGET;
	w2->flags4 &= ~WF_WHITE_BORDER_MASK;

	WP(w2, dropdown_d).disabled_state = disabled_mask;
	WP(w2, dropdown_d).hidden_state = hidden_mask;

	WP(w2, dropdown_d).parent_wnd_class = w->window_class;
	WP(w2, dropdown_d).parent_wnd_num = w->window_number;
	WP(w2, dropdown_d).parent_button = button;

	WP(w2, dropdown_d).num_items = i;
	WP(w2, dropdown_d).selected_index = selected;
	WP(w2, dropdown_d).items = strings;

	WP(w2, dropdown_d).click_delay = 0;
	WP(w2, dropdown_d).drag_mode = true;
}


static void ResizeWidgets(Window *w, byte a, byte b)
{
	int16 offset = w->widget[a].left;
	int16 length = w->widget[b].right - offset;

	w->widget[a].right  = (length / 2) + offset;

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
