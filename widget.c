#include "stdafx.h"
#include "ttd.h"
#include "window.h"
#include "gfx.h"
#include "viewport.h"

static Point HandleScrollbarHittest(Scrollbar *sb, int top, int bottom)
{
	Point pt;
	int height, count, pos, cap;

	top += 10;
	bottom -= 9;
	
	height = (bottom - top);

	pos = sb->pos;
	count = sb->count;
	cap = sb->cap;

	if (count != 0)	top += height * pos / count;
	
	if (cap > count) cap = count;
	if (count != 0)
		bottom -= (count - pos - cap) * height / count;

	pt.x = top;
	pt.y = bottom - 1;
	return pt;
}

/*****************************************************
 * Special handling for the scrollbar widget type.
 * Handles the special scrolling buttons and other
 * scrolling.
 * Parameters:
 *   w   - Window.
 *   wi  - Pointer to the scrollbar widget.
 *   x   - The X coordinate of the mouse click.
 *   y   - The Y coordinate of the mouse click.
 */

void ScrollbarClickHandler(Window *w, const Widget *wi, int x, int y)
{
	int mi, ma, pos;
	Scrollbar *sb;

	if (wi->type == WWT_SCROLLBAR) {
		// vertical scroller
		w->flags4 &= ~WF_HSCROLL;
		mi = wi->top;
		ma = wi->bottom;
		pos = y;
		sb = &w->vscroll;
	} else {
		// horizontal scroller
		assert(wi->type == WWT_HSCROLLBAR);
		w->flags4 |= WF_HSCROLL;
		mi = wi->left;
		ma = wi->right;
		pos = x;
		sb = &w->hscroll;
	}
	
	if (pos <= mi+9) {
		// Pressing the upper button?
		if (!_demo_mode) {
			w->flags4 |= WF_SCROLL_UP;
			if (_scroller_click_timeout == 0) {
				_scroller_click_timeout = 6;
				if (sb->pos != 0) sb->pos--;
			}
			_left_button_clicked = false;
		}
	} else if (pos >= ma-10) {
		// Pressing the lower button?
		if (!_demo_mode) {
			w->flags4 |= WF_SCROLL_DOWN;
			
			if (_scroller_click_timeout == 0) {
				_scroller_click_timeout = 6;
				if ((byte)(sb->pos + sb->cap) < sb->count)
					sb->pos++;
			}
			_left_button_clicked = false;
		}
	} else {
		// 
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

/*****************************************************
 * Returns the index for the widget located at the given
 * position relative to the window.
 * Parameters:
 *   w   - Window
 *   x/y - Window client coordinates
 * Returns:
 *   A widget index, or -1 if no widget was found.
 */
int GetWidgetFromPos(Window *w, int x, int y)
{
	const Widget *wi;
	int index, found_index = -1;

	// Go through the widgets and check if we find the widget that the coordinate is
	// inside.
	for(index=0,wi=w->widget; wi->type != WWT_LAST; index++, wi++) {
		if (wi->type == WWT_EMPTY || wi->type == WWT_FRAME)
			continue;

		if (x >= wi->left &&
		    x < wi->right &&
				y >= wi->top &&
				y < wi->bottom && !HASBIT(w->hidden_state,index)) {
				found_index = index;
		}
	}

	// Return the index
	return found_index;
}


void DrawWindowWidgets(Window *w)
{
	const Widget *wi;
	DrawPixelInfo *dpi = _cur_dpi;
	Rect r;
	uint32 dparam_backup[10];
	uint32 cur_click, cur_disabled, cur_hidden;
	
	if (w->desc_flags & WDF_RESTORE_DPARAM)	COPY_OUT_DPARAM(dparam_backup, 0, lengthof(dparam_backup));
	
	wi = w->widget;

	cur_click = w->click_state;
	cur_disabled = w->disabled_state;
	cur_hidden = w->hidden_state;

	do {
		if (dpi->left > (r.right=/*w->left + */wi->right) ||
		    dpi->left + dpi->width <= (r.left=wi->left/* + w->left*/) ||
				dpi->top > (r.bottom=/*w->top +*/ wi->bottom) ||
				dpi->top + dpi->height <= (r.top = /*w->top +*/ wi->top) ||
				(cur_hidden&1))
					continue;

		switch(wi->type&WWT_MASK) {
		case WWT_PANEL:
		case WWT_PANEL_2: {
			int img;

			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color,
				(cur_click & 1) ? 0x20 : 0);

			if ((img=wi->unkA) != 0) {
				if ((wi->type&WWT_MASK) == WWT_PANEL_2 && (cur_click&1)) img++;
				DrawSprite(img, r.left+1, r.top+1);
			}
			goto draw_default;
		}

		case WWT_CLOSEBOX:
		case WWT_4: {
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color,
					(cur_click & 1) ? 0x20 : 0);
			}
		/* fall through */

		case WWT_5: {
			StringID str = wi->unkA;

			if ((wi->type&WWT_MASK) == WWT_4 && (cur_click&1)) str++;
			
			DrawStringCentered((r.left + r.right+1)>>1, ((r.top+r.bottom + 1)>>1) - 5, str, 0);
			goto restore_dparam;
		}

		case WWT_6: {
			StringID str;
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, 0x60);

			if ((str = wi->unkA) != 0) {
				DrawString(r.left+2, r.top+1, str, 0);
				goto restore_dparam;
			}
			goto draw_default;
		}

		case WWT_MATRIX: {
			int c, d, ctr;	
			int x, amt1, amt2;
			int color;
		
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color,
					(cur_click & 1) ? 0x20 : 0);
			
			c = (wi->unkA&0xFF);
			amt1 = (wi->right - wi->left + 1) / c;

			d = (wi->unkA >> 8);
			amt2 = (wi->bottom - wi->top + 1) / d;

			color = _color_list[wi->color&0xF].window_color_bgb;
			
			x = r.left;
			for(ctr=c; --ctr; ) {
				x += amt1;
				GfxFillRect(x, r.top+1, x, r.bottom-1, color);
			}

			x = r.top;
			for(ctr=d; --ctr; ) {
				x += amt2;
				GfxFillRect(r.left+1, x, r.right-1, x, color);
			}

			color = _color_list[wi->color&0xF].window_color_1b;

			x = r.left-1;
			for(ctr=c; --ctr; ) {
				x += amt1;
				GfxFillRect(x, r.top+1, x, r.bottom-1, color);
			}

			x = r.top-1;
			for(ctr=d; --ctr; ) {
				x += amt2;
				GfxFillRect(r.left+1, x, r.right-1, x, color);
			}

			goto draw_default;			
		}

		// vertical scrollbar
		case WWT_SCROLLBAR: {
			Point pt;
			int c1,c2;

			// draw up/down buttons
			DrawFrameRect(r.left, r.top, r.right, r.top+9, wi->color, (w->flags4 & (WF_SCROLL_UP | WF_HSCROLL)) == WF_SCROLL_UP ? 0x20 : 0);
			DrawFrameRect(r.left, r.bottom-9, r.right, r.bottom, wi->color, (w->flags4 & (WF_SCROLL_DOWN | WF_HSCROLL)) == WF_SCROLL_DOWN ? 0x20 : 0);

			// draw icons in up/down buttons
			DoDrawString("\xA0", r.left+2, r.top, 0x10);
			DoDrawString("\xAA", r.left+2, r.bottom-9, 0x10);

			c1 = _color_list[wi->color&0xF].window_color_1a;
			c2 = _color_list[wi->color&0xF].window_color_2;

			// draw "shaded" background
			GfxFillRect(r.left, r.top+10, r.right, r.bottom-10, c2);
			GfxFillRect(r.left, r.top+10, r.right, r.bottom-10, c1 | 0x8000);

			// draw shaded lines
			GfxFillRect(r.left+2, r.top+10, r.left+2, r.bottom-10, c1);
			GfxFillRect(r.left+3, r.top+10, r.left+3, r.bottom-10, c2);
			GfxFillRect(r.left+7, r.top+10, r.left+7, r.bottom-10, c1);
			GfxFillRect(r.left+8, r.top+10, r.left+8, r.bottom-10, c2);
			
			pt = HandleScrollbarHittest(&w->vscroll, r.top, r.bottom);
			DrawFrameRect(r.left, pt.x, r.right, pt.y, wi->color, (w->flags4 & (WF_SCROLL_MIDDLE | WF_HSCROLL)) == WF_SCROLL_MIDDLE ? 0x20 : 0);
			break;
		}

		// horizontal scrollbar
		case WWT_HSCROLLBAR: {
			Point pt;
			int c1,c2;

			DrawFrameRect(r.left, r.top, r.left + 9, r.bottom, wi->color, (w->flags4 & (WF_SCROLL_UP | WF_HSCROLL)) == (WF_SCROLL_UP | WF_HSCROLL) ? 0x20 : 0);
			DrawFrameRect(r.right-9, r.top, r.right, r.bottom, wi->color, (w->flags4 & (WF_SCROLL_DOWN | WF_HSCROLL)) == (WF_SCROLL_DOWN | WF_HSCROLL) ? 0x20 : 0);

			// draw icons in up/down buttons
			DoDrawString("\xA0", r.left+1, r.top+1, 0x10);
			DoDrawString("\xAA", r.right-8, r.top+1, 0x10);

			c1 = _color_list[wi->color&0xF].window_color_1a;
			c2 = _color_list[wi->color&0xF].window_color_2;

			// draw "shaded" background
			GfxFillRect(r.left+10, r.top, r.right-10, r.bottom, c2);
			GfxFillRect(r.left+10, r.top, r.right-10, r.bottom, c1 | 0x8000);

			// draw shaded lines
			GfxFillRect(r.left+10, r.top+2, r.right-10, r.top+2, c1);
			GfxFillRect(r.left+10, r.top+3, r.right-10, r.top+3, c2);
			GfxFillRect(r.left+10, r.top+7, r.right-10, r.top+7, c1);
			GfxFillRect(r.left+10, r.top+8, r.right-10, r.top+8, c2);

			// draw actual scrollbar
			pt = HandleScrollbarHittest(&w->hscroll, r.left, r.right);
			DrawFrameRect(pt.x, r.top, pt.y, r.bottom, wi->color, (w->flags4 & (WF_SCROLL_MIDDLE | WF_HSCROLL)) == (WF_SCROLL_MIDDLE | WF_HSCROLL) ? 0x20 : 0);

			break;
		}

		case WWT_FRAME: {
			int c1,c2;
			int x2 = 0;

			if (wi->unkA != 0) {
				x2 = DrawString(r.left+6, r.top, wi->unkA, 0);
			}

			c1 = _color_list[wi->color].window_color_1a;
			c2 = _color_list[wi->color].window_color_2;

			//Line from upper left corner to start of text
			GfxFillRect(r.left, r.top+4, r.left+4,r.top+4, c1);
			GfxFillRect(r.left+1, r.top+5, r.left+4,r.top+5, c2);

			// Line from end of text to upper right corner
			GfxFillRect(x2, r.top+4, r.right-1,r.top+4,c1);
			GfxFillRect(x2, r.top+5, r.right-2,r.top+5,c2);

			// Line from upper left corner to bottom left corner
			GfxFillRect(r.left, r.top+5, r.left, r.bottom-1, c1);
			GfxFillRect(r.left+1, r.top+6, r.left+1, r.bottom-2, c2);

			//Line from upper right corner to bottom right corner
			GfxFillRect(r.right-1, r.top+5, r.right-1, r.bottom-2, c1);
			GfxFillRect(r.right, r.top+4, r.right, r.bottom-1, c2);

			GfxFillRect(r.left+1, r.bottom-1, r.right-1, r.bottom-1, c1);
			GfxFillRect(r.left, r.bottom, r.right, r.bottom, c2);

			goto restore_dparam;
		}

		case WWT_CAPTION: {
			DrawFrameRect(r.left, r.top, r.right, r.bottom, wi->color, 0x10);
			DrawFrameRect(r.left+1, r.top+1, r.right-1, r.bottom-1, wi->color, (w->caption_color == 0xFF) ? 0x60 : 0x70);
			
			if (w->caption_color != 0xFF) {
				GfxFillRect(r.left+2, r.top+2, r.right-2, r.bottom-2, _color_list[_player_colors[w->caption_color]].window_color_1b);
			}

			DrawStringCentered( (r.left+r.right+1)>>1, r.top+2, wi->unkA, 0x84);
restore_dparam:;
			if (w->desc_flags & WDF_RESTORE_DPARAM)	COPY_IN_DPARAM(0, dparam_backup, lengthof(dparam_backup));
draw_default:;
			if (cur_disabled & 1) {
				GfxFillRect(r.left+1, r.top+1, r.right-1, r.bottom-1, _color_list[wi->color&0xF].unk2 | 0x8000);
			}
		}
		}
	} while (cur_click>>=1, cur_disabled>>=1, cur_hidden >>= 1, (++wi)->type != WWT_LAST);
	

	if (w->flags4 & WF_WHITE_BORDER_MASK) {
		//DrawFrameRect(w->left, w->top, w->left + w->width-1, w->top+w->height-1, 0xF, 0x10);
		DrawFrameRect(0, 0, w->width-1, w->height-1, 0xF, 0x10);
	}

}
