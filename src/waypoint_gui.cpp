/* $Id$ */

/** @file waypoint_gui.cpp Handling of waypoints gui. */

#include "stdafx.h"
#include "window_gui.h"
#include "gui.h"
#include "textbuf_gui.h"
#include "vehicle_gui.h"
#include "viewport_func.h"
#include "strings_func.h"
#include "gfx_func.h"
#include "command_func.h"
#include "company_func.h"
#include "window_func.h"

#include "table/strings.h"

struct WaypointWindow : Window {
private:
	Waypoint *wp;

	enum WaypointViewWidget {
		WAYPVW_CLOSEBOX = 0,
		WAYPVW_CAPTION,
		WAYPVW_STICKY,
		WAYPVW_VIEWPORTPANEL,
		WAYPVW_SPACER,
		WAYPVW_CENTERVIEW,
		WAYPVW_RENAME,
		WAYPVW_SHOW_TRAINS,
	};

public:
	WaypointWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->wp = GetWaypoint(this->window_number);
		if (this->wp->owner != OWNER_NONE) this->owner = this->wp->owner;

		this->flags4 |= WF_DISABLE_VP_SCROLL;
		InitializeWindowViewport(this, 3, 17, 254, 86, this->wp->xy, ZOOM_LVL_MIN);

		this->FindWindowPlacementAndResize(desc);
	}

	~WaypointWindow()
	{
		DeleteWindowById(WC_TRAINS_LIST, (this->window_number << 16) | (VEH_TRAIN << 11) | VLW_WAYPOINT_LIST | this->wp->owner);
	}

	virtual void OnPaint()
	{
		/* You can only change your own waypoints */
		this->SetWidgetDisabledState(WAYPVW_RENAME, this->wp->owner != _local_company);
		/* Disable the widget for waypoints with no owner (after company bankrupt) */
		this->SetWidgetDisabledState(WAYPVW_SHOW_TRAINS, this->wp->owner == OWNER_NONE);

		SetDParam(0, this->wp->index);
		this->DrawWidgets();

		this->DrawViewport();
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case WAYPVW_CENTERVIEW: // scroll to location
				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(this->wp->xy);
				} else {
					ScrollMainWindowToTile(this->wp->xy);
				}
				break;

			case WAYPVW_RENAME: // rename
				SetDParam(0, this->wp->index);
				ShowQueryString(STR_WAYPOINT_RAW, STR_EDIT_WAYPOINT_NAME, MAX_LENGTH_WAYPOINT_NAME_BYTES, MAX_LENGTH_WAYPOINT_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT);
				break;

			case WAYPVW_SHOW_TRAINS: // show list of trains having this waypoint in their orders
				ShowVehicleListWindow(this->wp);
				break;
		}
	}

	virtual void OnInvalidateData(int data)
	{
		int x = TileX(this->wp->xy) * TILE_SIZE;
		int y = TileY(this->wp->xy) * TILE_SIZE;
		ScrollWindowTo(x, y, -1, this);
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		DoCommandP(0, this->window_number, 0, CMD_RENAME_WAYPOINT | CMD_MSG(STR_CANT_CHANGE_WAYPOINT_NAME), NULL, str);
	}

};

static const Widget _waypoint_view_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,              STR_018B_CLOSE_WINDOW},                 // WAYPVW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   247,     0,    13, STR_WAYPOINT_VIEWPORT, STR_018C_WINDOW_TITLE_DRAG_THIS},       // WAYPVW_CAPTION
{  WWT_STICKYBOX,   RESIZE_NONE,  COLOUR_GREY,   248,   259,     0,    13, 0x0,                   STR_STICKY_BUTTON},                     // WAYPVW_STICKY
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   259,    14,   105, 0x0,                   STR_NULL},                              // WAYPVW_VIEWPORTPANEL
{      WWT_INSET,   RESIZE_NONE,  COLOUR_GREY,     2,   257,    16,   103, 0x0,                   STR_NULL},                              // WAYPVW_SPACER
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,   121,   106,   117, STR_00E4_LOCATION,     STR_3053_CENTER_MAIN_VIEW_ON_STATION},  // WAYPVW_CENTERVIEW
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   122,   244,   106,   117, STR_0130_RENAME,       STR_CHANGE_WAYPOINT_NAME},              // WAYPVW_RENAME
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   245,   259,   106,   117, STR_TRAIN,             STR_SCHEDULED_TRAINS_TIP },             // WAYPVW_SHOW_TRAINS
{   WIDGETS_END},
};

static const WindowDesc _waypoint_view_desc(
	WDP_AUTO, WDP_AUTO, 260, 118, 260, 118,
	WC_WAYPOINT_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_waypoint_view_widgets
);

void ShowWaypointWindow(const Waypoint *wp)
{
	if (!wp->IsValid()) return;  // little safety
	AllocateWindowDescFront<WaypointWindow>(&_waypoint_view_desc, wp->index);
}
