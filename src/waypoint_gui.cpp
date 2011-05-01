/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file waypoint_gui.cpp Handling of waypoints gui. */

#include "stdafx.h"
#include "window_gui.h"
#include "gui.h"
#include "textbuf_gui.h"
#include "vehiclelist.h"
#include "vehicle_gui.h"
#include "viewport_func.h"
#include "strings_func.h"
#include "command_func.h"
#include "company_func.h"
#include "company_base.h"
#include "window_func.h"
#include "waypoint_base.h"

#include "table/strings.h"

/** Widget definitions for the waypoint window. */
enum WaypointWindowWidgets {
	WAYPVW_CAPTION,
	WAYPVW_VIEWPORT,
	WAYPVW_CENTERVIEW,
	WAYPVW_RENAME,
	WAYPVW_SHOW_VEHICLES,
};

/** GUI for accessing waypoints and buoys. */
struct WaypointWindow : Window {
private:
	VehicleType vt; ///< Vehicle type using the waypoint.
	Waypoint *wp;   ///< Waypoint displayed by the window.

	/**
	 * Get the center tile of the waypoint.
	 * @return The center tile if the waypoint exists, otherwise the tile with the waypoint name.
	 */
	TileIndex GetCenterTile() const
	{
		if (!this->wp->IsInUse()) return this->wp->xy;

		TileArea ta;
		this->wp->GetTileArea(&ta, this->vt == VEH_TRAIN ? STATION_WAYPOINT : STATION_BUOY);
		return ta.GetCenterTile();
	}

public:
	/**
	 * Construct the window.
	 * @param desc The description of the window.
	 * @param window_number The window number, in this case the waypoint's ID.
	 */
	WaypointWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->wp = Waypoint::Get(window_number);
		this->vt = (wp->string_id == STR_SV_STNAME_WAYPOINT) ? VEH_TRAIN : VEH_SHIP;

		this->CreateNestedTree(desc);
		if (this->vt == VEH_TRAIN) {
			this->GetWidget<NWidgetCore>(WAYPVW_SHOW_VEHICLES)->SetDataTip(STR_TRAIN, STR_STATION_VIEW_SCHEDULED_TRAINS_TOOLTIP);
			this->GetWidget<NWidgetCore>(WAYPVW_CENTERVIEW)->tool_tip = STR_WAYPOINT_VIEW_CENTER_TOOLTIP;
			this->GetWidget<NWidgetCore>(WAYPVW_RENAME)->tool_tip = STR_WAYPOINT_VIEW_CHANGE_WAYPOINT_NAME;
		}
		this->FinishInitNested(desc, window_number);

		if (this->wp->owner != OWNER_NONE) this->owner = this->wp->owner;
		this->flags4 |= WF_DISABLE_VP_SCROLL;

		NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WAYPVW_VIEWPORT);
		nvp->InitializeViewport(this, this->GetCenterTile(), ZOOM_LVL_MIN);

		this->OnInvalidateData(0);
	}

	~WaypointWindow()
	{
		Owner owner = this->owner;

		/* Buoys have no owner and can be used by everyone. Show only 'our' vehicles */
		if (!Company::IsValidID(owner)) owner = _local_company;

		/* Well, spectators otoh */
		if (Company::IsValidID(owner)) DeleteWindowById(GetWindowClassForVehicleType(this->vt), VehicleListIdentifier(VL_STATION_LIST, this->vt, owner, this->window_number).Pack(), false);
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == WAYPVW_CAPTION) SetDParam(0, this->wp->index);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WAYPVW_CENTERVIEW: // scroll to location
				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(this->GetCenterTile());
				} else {
					ScrollMainWindowToTile(this->GetCenterTile());
				}
				break;

			case WAYPVW_RENAME: // rename
				SetDParam(0, this->wp->index);
				ShowQueryString(STR_WAYPOINT_NAME, STR_EDIT_WAYPOINT_NAME, MAX_LENGTH_STATION_NAME_CHARS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT | QSF_LEN_IN_CHARS);
				break;

			case WAYPVW_SHOW_VEHICLES: // show list of vehicles having this waypoint in their orders
				ShowVehicleListWindow(this->owner, this->vt, this->wp->index);
				break;
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
		/* You can only change your own waypoints */
		this->SetWidgetDisabledState(WAYPVW_RENAME, !this->wp->IsInUse() || (this->wp->owner != _local_company && this->wp->owner != OWNER_NONE));
		/* Disable the widget for waypoints with no use */
		this->SetWidgetDisabledState(WAYPVW_SHOW_VEHICLES, !this->wp->IsInUse());

		ScrollWindowToTile(this->GetCenterTile(), this, true);
	}

	virtual void OnResize()
	{
		if (this->viewport != NULL) {
			NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WAYPVW_VIEWPORT);
			nvp->UpdateViewportCoordinates(this);
			this->wp->UpdateVirtCoord();

			ScrollWindowToTile(this->GetCenterTile(), this, true); // Re-center viewport.
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		DoCommandP(0, this->window_number, 0, CMD_RENAME_WAYPOINT | CMD_MSG(STR_ERROR_CAN_T_CHANGE_WAYPOINT_NAME), NULL, str);
	}

};

/** The widgets of the waypoint view. */
static const NWidgetPart _nested_waypoint_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WAYPVW_CAPTION), SetDataTip(STR_WAYPOINT_VIEW_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(WWT_INSET, COLOUR_GREY), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, COLOUR_GREY, WAYPVW_VIEWPORT), SetMinimalSize(256, 88), SetPadding(1, 1, 1, 1), SetResize(1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WAYPVW_CENTERVIEW), SetMinimalSize(100, 12), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_BUTTON_LOCATION, STR_BUOY_VIEW_CENTER_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WAYPVW_RENAME), SetMinimalSize(100, 12), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_BUTTON_RENAME, STR_BUOY_VIEW_CHANGE_BUOY_NAME),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WAYPVW_SHOW_VEHICLES), SetMinimalSize(15, 12), SetDataTip(STR_SHIP, STR_STATION_VIEW_SCHEDULED_SHIPS_TOOLTIP),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

/** The description of the waypoint view. */
static const WindowDesc _waypoint_view_desc(
	WDP_AUTO, 260, 118,
	WC_WAYPOINT_VIEW, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_waypoint_view_widgets, lengthof(_nested_waypoint_view_widgets)
);

/**
 * Show the window for the given waypoint.
 * @param wp The waypoint to show the window for.
 */
void ShowWaypointWindow(const Waypoint *wp)
{
	AllocateWindowDescFront<WaypointWindow>(&_waypoint_view_desc, wp->index);
}
