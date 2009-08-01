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
#include "waypoint_base.h"

#include "table/strings.h"

/** Widget definitions for the waypoint window. */
enum WaypointWindowWidgets {
	WAYPVW_CLOSEBOX = 0,
	WAYPVW_CAPTION,
	WAYPVW_STICKY,
	WAYPVW_VIEWPORTPANEL,
	WAYPVW_SPACER,
	WAYPVW_VIEWPORT,
	WAYPVW_CENTERVIEW,
	WAYPVW_RENAME,
	WAYPVW_SHOW_VEHICLES,
};

struct WaypointWindow : Window {
private:
	VehicleType vt;
	Waypoint *wp;

public:
	WaypointWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->wp = Waypoint::Get(window_number);
		this->vt = (wp->string_id == STR_SV_STNAME_WAYPOINT) ? VEH_TRAIN : VEH_SHIP;

		if (this->wp->owner != OWNER_NONE) this->owner = this->wp->owner;

		this->CreateNestedTree(desc);
		if (this->vt == VEH_TRAIN) this->nested_array[WAYPVW_SHOW_VEHICLES]->SetDataTip(STR_TRAIN, STR_SCHEDULED_TRAINS_TIP);
		this->FinishInitNested(desc, window_number);

		this->flags4 |= WF_DISABLE_VP_SCROLL;
		NWidgetViewport *nvp = (NWidgetViewport *)this->nested_array[WAYPVW_VIEWPORT];
		nvp->InitializeViewport(this, this->wp->xy, ZOOM_LVL_MIN);

		this->OnInvalidateData(0);
	}

	~WaypointWindow()
	{
		DeleteWindowById(WC_TRAINS_LIST, (this->window_number << 16) | (this->vt << 11) | VLW_WAYPOINT_LIST | this->wp->owner);
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == WAYPVW_CAPTION) SetDParam(0, this->wp->index);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
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
				ShowQueryString(STR_WAYPOINT_NAME, STR_EDIT_WAYPOINT_NAME, MAX_LENGTH_STATION_NAME_BYTES, MAX_LENGTH_STATION_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT);
				break;

			case WAYPVW_SHOW_VEHICLES: // show list of vehicles having this waypoint in their orders
				ShowVehicleListWindow((this->wp->owner == OWNER_NONE) ? _local_company : this->wp->owner, this->vt, this->wp);
				break;
		}
	}

	virtual void OnInvalidateData(int data)
	{
		/* You can only change your own waypoints */
		this->SetWidgetDisabledState(WAYPVW_RENAME, !this->wp->IsInUse() || (this->wp->owner != _local_company && this->wp->owner != OWNER_NONE));
		/* Disable the widget for waypoints with no use */
		this->SetWidgetDisabledState(WAYPVW_SHOW_VEHICLES, !this->wp->IsInUse());

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

static const NWidgetPart _nested_waypoint_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, WAYPVW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, WAYPVW_CAPTION), SetDataTip(STR_WAYPOINT_VIEWPORT, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_GREY, WAYPVW_STICKY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WAYPVW_VIEWPORTPANEL),
		NWidget(WWT_INSET, COLOUR_GREY, WAYPVW_SPACER), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, COLOUR_GREY, WAYPVW_VIEWPORT), SetMinimalSize(256, 88), SetPadding(1, 1, 1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WAYPVW_CENTERVIEW), SetMinimalSize(100, 12), SetFill(1, 0), SetDataTip(STR_BUTTON_LOCATION, STR_STATION_VIEW_CENTER_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WAYPVW_RENAME), SetMinimalSize(100, 12), SetFill(1, 0), SetDataTip(STR_QUERY_RENAME, STR_CHANGE_WAYPOINT_NAME),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WAYPVW_SHOW_VEHICLES), SetMinimalSize(15, 12), SetDataTip(STR_SHIP, STR_SCHEDULED_SHIPS_TIP),
	EndContainer(),
};

static const WindowDesc _waypoint_view_desc(
	WDP_AUTO, WDP_AUTO, 260, 118, 260, 118,
	WC_WAYPOINT_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	NULL, _nested_waypoint_view_widgets, lengthof(_nested_waypoint_view_widgets)
);

void ShowWaypointWindow(const Waypoint *wp)
{
	AllocateWindowDescFront<WaypointWindow>(&_waypoint_view_desc, wp->index);
}
