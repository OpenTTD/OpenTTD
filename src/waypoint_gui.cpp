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
#include "waypoint_cmd.h"
#include "zoom_func.h"

#include "widgets/waypoint_widget.h"

#include "table/strings.h"

#include "safeguards.h"

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
	WaypointWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->wp = Waypoint::Get(window_number);
		this->vt = (wp->string_id == STR_SV_STNAME_WAYPOINT) ? VEH_TRAIN : VEH_SHIP;

		this->CreateNestedTree();
		if (this->vt == VEH_TRAIN) {
			this->GetWidget<NWidgetCore>(WID_W_SHOW_VEHICLES)->SetDataTip(STR_TRAIN, STR_STATION_VIEW_SCHEDULED_TRAINS_TOOLTIP);
			this->GetWidget<NWidgetCore>(WID_W_CENTER_VIEW)->tool_tip = STR_WAYPOINT_VIEW_CENTER_TOOLTIP;
			this->GetWidget<NWidgetCore>(WID_W_RENAME)->tool_tip = STR_WAYPOINT_VIEW_CHANGE_WAYPOINT_NAME;
		}
		this->FinishInitNested(window_number);

		this->owner = this->wp->owner;
		this->flags |= WF_DISABLE_VP_SCROLL;

		NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WID_W_VIEWPORT);
		nvp->InitializeViewport(this, this->GetCenterTile(), ScaleZoomGUI(ZOOM_LVL_VIEWPORT));

		this->OnInvalidateData(0);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		CloseWindowById(GetWindowClassForVehicleType(this->vt), VehicleListIdentifier(VL_STATION_LIST, this->vt, this->owner, this->window_number).Pack(), false);
		SetViewportCatchmentWaypoint(Waypoint::Get(this->window_number), false);
		this->Window::Close();
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_W_CAPTION) SetDParam(0, this->wp->index);
	}

	void OnPaint() override
	{
		extern const Waypoint *_viewport_highlight_waypoint;
		this->SetWidgetDisabledState(WID_W_CATCHMENT, !this->wp->IsInUse());
		this->SetWidgetLoweredState(WID_W_CATCHMENT, _viewport_highlight_waypoint == this->wp);

		this->DrawWidgets();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_W_CENTER_VIEW: // scroll to location
				if (_ctrl_pressed) {
					ShowExtraViewportWindow(this->GetCenterTile());
				} else {
					ScrollMainWindowToTile(this->GetCenterTile());
				}
				break;

			case WID_W_RENAME: // rename
				SetDParam(0, this->wp->index);
				ShowQueryString(STR_WAYPOINT_NAME, STR_EDIT_WAYPOINT_NAME, MAX_LENGTH_STATION_NAME_CHARS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT | QSF_LEN_IN_CHARS);
				break;

			case WID_W_SHOW_VEHICLES: // show list of vehicles having this waypoint in their orders
				ShowVehicleListWindow(this->wp->owner, this->vt, this->wp->index);
				break;

			case WID_W_CATCHMENT:
				SetViewportCatchmentWaypoint(Waypoint::Get(this->window_number), !this->IsWidgetLowered(WID_W_CATCHMENT));
				break;
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		/* You can only change your own waypoints */
		this->SetWidgetDisabledState(WID_W_RENAME, !this->wp->IsInUse() || (this->wp->owner != _local_company && this->wp->owner != OWNER_NONE));
		/* Disable the widget for waypoints with no use */
		this->SetWidgetDisabledState(WID_W_SHOW_VEHICLES, !this->wp->IsInUse());

		ScrollWindowToTile(this->GetCenterTile(), this, true);
	}

	void OnResize() override
	{
		if (this->viewport != nullptr) {
			NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WID_W_VIEWPORT);
			nvp->UpdateViewportCoordinates(this);
			this->wp->UpdateVirtCoord();

			ScrollWindowToTile(this->GetCenterTile(), this, true); // Re-center viewport.
		}
	}

	void OnQueryTextFinished(char *str) override
	{
		if (str == nullptr) return;

		Command<CMD_RENAME_WAYPOINT>::Post(STR_ERROR_CAN_T_CHANGE_WAYPOINT_NAME, this->window_number, str);
	}

};

/** The widgets of the waypoint view. */
static const NWidgetPart _nested_waypoint_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_W_RENAME), SetMinimalSize(12, 14), SetDataTip(SPR_RENAME, STR_BUOY_VIEW_CHANGE_BUOY_NAME),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_W_CAPTION), SetDataTip(STR_WAYPOINT_VIEW_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_W_CENTER_VIEW), SetMinimalSize(12, 14), SetDataTip(SPR_GOTO_LOCATION, STR_BUOY_VIEW_CENTER_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(WWT_INSET, COLOUR_GREY), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, COLOUR_GREY, WID_W_VIEWPORT), SetMinimalSize(256, 88), SetResize(1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_W_CATCHMENT), SetMinimalSize(50, 12), SetResize(1, 0), SetFill(1, 1), SetDataTip(STR_BUTTON_CATCHMENT, STR_TOOLTIP_CATCHMENT),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_W_SHOW_VEHICLES), SetMinimalSize(15, 12), SetDataTip(STR_SHIP, STR_STATION_VIEW_SCHEDULED_SHIPS_TOOLTIP),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

/** The description of the waypoint view. */
static WindowDesc _waypoint_view_desc(__FILE__, __LINE__,
	WDP_AUTO, "view_waypoint", 260, 118,
	WC_WAYPOINT_VIEW, WC_NONE,
	0,
	std::begin(_nested_waypoint_view_widgets), std::end(_nested_waypoint_view_widgets)
);

/**
 * Show the window for the given waypoint.
 * @param wp The waypoint to show the window for.
 */
void ShowWaypointWindow(const Waypoint *wp)
{
	AllocateWindowDescFront<WaypointWindow>(&_waypoint_view_desc, wp->index);
}
