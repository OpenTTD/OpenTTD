/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file viewport_gui.cpp Extra viewport window. */

#include "stdafx.h"
#include "landscape.h"
#include "window_gui.h"
#include "viewport_func.h"
#include "strings_func.h"
#include "zoom_func.h"
#include "window_func.h"

#include "widgets/viewport_widget.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "safeguards.h"

/* Extra ViewPort Window Stuff */
static const NWidgetPart _nested_extra_view_port_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_EV_CAPTION), SetDataTip(STR_EXTRA_VIEW_PORT_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VIEWPORT, INVALID_COLOUR, WID_EV_VIEWPORT), SetPadding(2, 2, 2, 2), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_EV_ZOOM_IN), SetDataTip(SPR_IMG_ZOOMIN, STR_TOOLBAR_TOOLTIP_ZOOM_THE_VIEW_IN),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_EV_ZOOM_OUT), SetDataTip(SPR_IMG_ZOOMOUT, STR_TOOLBAR_TOOLTIP_ZOOM_THE_VIEW_OUT),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_EV_MAIN_TO_VIEW), SetFill(1, 1), SetResize(1, 0),
										SetDataTip(STR_EXTRA_VIEW_MOVE_MAIN_TO_VIEW, STR_EXTRA_VIEW_MOVE_MAIN_TO_VIEW_TT),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_EV_VIEW_TO_MAIN), SetFill(1, 1), SetResize(1, 0),
										SetDataTip(STR_EXTRA_VIEW_MOVE_VIEW_TO_MAIN, STR_EXTRA_VIEW_MOVE_VIEW_TO_MAIN_TT),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1), SetResize(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

class ExtraViewportWindow : public Window {
public:
	ExtraViewportWindow(WindowDesc *desc, int window_number, TileIndex tile) : Window(desc)
	{
		this->InitNested(window_number);

		NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WID_EV_VIEWPORT);
		nvp->InitializeViewport(this, 0, ZOOM_LVL_VIEWPORT);
		if (_settings_client.gui.zoom_min == ZOOM_LVL_VIEWPORT) this->DisableWidget(WID_EV_ZOOM_IN);

		Point pt;
		if (tile == INVALID_TILE) {
			/* No tile? Use center of main viewport. */
			const Window *w = FindWindowById(WC_MAIN_WINDOW, 0);

			/* center on same place as main window (zoom is maximum, no adjustment needed) */
			pt.x = w->viewport->scrollpos_x + w->viewport->virtual_width / 2;
			pt.y = w->viewport->scrollpos_y + w->viewport->virtual_height / 2;
		} else {
			pt = RemapCoords(TileX(tile) * TILE_SIZE + TILE_SIZE / 2, TileY(tile) * TILE_SIZE + TILE_SIZE / 2, TilePixelHeight(tile));
		}

		this->viewport->scrollpos_x = pt.x - this->viewport->virtual_width / 2;
		this->viewport->scrollpos_y = pt.y - this->viewport->virtual_height / 2;
		this->viewport->dest_scrollpos_x = this->viewport->scrollpos_x;
		this->viewport->dest_scrollpos_y = this->viewport->scrollpos_y;
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_EV_CAPTION:
				/* set the number in the title bar */
				SetDParam(0, this->window_number + 1);
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_EV_ZOOM_IN: DoZoomInOutWindow(ZOOM_IN,  this); break;
			case WID_EV_ZOOM_OUT: DoZoomInOutWindow(ZOOM_OUT, this); break;

			case WID_EV_MAIN_TO_VIEW: { // location button (move main view to same spot as this view) 'Paste Location'
				Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
				int x = this->viewport->scrollpos_x; // Where is the main looking at
				int y = this->viewport->scrollpos_y;

				/* set this view to same location. Based on the center, adjusting for zoom */
				w->viewport->dest_scrollpos_x =  x - (w->viewport->virtual_width -  this->viewport->virtual_width) / 2;
				w->viewport->dest_scrollpos_y =  y - (w->viewport->virtual_height - this->viewport->virtual_height) / 2;
				w->viewport->follow_vehicle   = INVALID_VEHICLE;
				break;
			}

			case WID_EV_VIEW_TO_MAIN: { // inverse location button (move this view to same spot as main view) 'Copy Location'
				const Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
				int x = w->viewport->scrollpos_x;
				int y = w->viewport->scrollpos_y;

				this->viewport->dest_scrollpos_x =  x + (w->viewport->virtual_width -  this->viewport->virtual_width) / 2;
				this->viewport->dest_scrollpos_y =  y + (w->viewport->virtual_height - this->viewport->virtual_height) / 2;
				break;
			}
		}
	}

	virtual void OnResize()
	{
		if (this->viewport != NULL) {
			NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WID_EV_VIEWPORT);
			nvp->UpdateViewportCoordinates(this);
		}
	}

	virtual void OnScroll(Point delta)
	{
		this->viewport->scrollpos_x += ScaleByZoom(delta.x, this->viewport->zoom);
		this->viewport->scrollpos_y += ScaleByZoom(delta.y, this->viewport->zoom);
		this->viewport->dest_scrollpos_x = this->viewport->scrollpos_x;
		this->viewport->dest_scrollpos_y = this->viewport->scrollpos_y;
	}

	virtual void OnMouseWheel(int wheel)
	{
		if (_settings_client.gui.scrollwheel_scrolling == 0) {
			ZoomInOrOutToCursorWindow(wheel < 0, this);
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		/* Only handle zoom message if intended for us (msg ZOOM_IN/ZOOM_OUT) */
		HandleZoomMessage(this, this->viewport, WID_EV_ZOOM_IN, WID_EV_ZOOM_OUT);
	}
};

static WindowDesc _extra_view_port_desc(
	WDP_AUTO, "extra_viewport", 300, 268,
	WC_EXTRA_VIEW_PORT, WC_NONE,
	0,
	_nested_extra_view_port_widgets, lengthof(_nested_extra_view_port_widgets)
);

/**
 * Show a new Extra Viewport window.
 * @param tile Tile to center the view on. INVALID_TILE means to use the center of main viewport.
 */
void ShowExtraViewPortWindow(TileIndex tile)
{
	int i = 0;

	/* find next free window number for extra viewport */
	while (FindWindowById(WC_EXTRA_VIEW_PORT, i) != NULL) i++;

	new ExtraViewportWindow(&_extra_view_port_desc, i, tile);
}

/**
 * Show a new Extra Viewport window.
 * Center it on the tile under the cursor, if the cursor is inside a viewport.
 * If that fails, center it on main viewport center.
 */
void ShowExtraViewPortWindowForTileUnderCursor()
{
	/* Use tile under mouse as center for new viewport.
	 * Do this before creating the window, it might appear just below the mouse. */
	Point pt = GetTileBelowCursor();
	ShowExtraViewPortWindow(pt.x != -1 ? TileVirtXY(pt.x, pt.y) : INVALID_TILE);
}
