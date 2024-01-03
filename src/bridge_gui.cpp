/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bridge_gui.cpp Graphical user interface for bridge construction */

#include "stdafx.h"
#include "error.h"
#include "command_func.h"
#include "rail.h"
#include "road.h"
#include "strings_func.h"
#include "window_func.h"
#include "sound_func.h"
#include "gfx_func.h"
#include "tunnelbridge.h"
#include "sortlist_type.h"
#include "widgets/dropdown_func.h"
#include "core/geometry_func.hpp"
#include "tunnelbridge_map.h"
#include "road_gui.h"
#include "tunnelbridge_cmd.h"

#include "widgets/bridge_widget.h"

#include "table/strings.h"

#include "safeguards.h"

/** The type of the last built rail bridge */
static BridgeType _last_railbridge_type = 0;
/** The type of the last built road bridge */
static BridgeType _last_roadbridge_type = 0;

/**
 * Carriage for the data we need if we want to build a bridge
 */
struct BuildBridgeData {
	BridgeType index;
	const BridgeSpec *spec;
	Money cost;
};

typedef GUIList<BuildBridgeData> GUIBridgeList; ///< List of bridges, used in #BuildBridgeWindow.

/**
 * Callback executed after a build Bridge CMD has been called
 *
 * @param result Whether the build succeeded
 * @param end_tile End tile of the bridge.
 * @param tile_start start tile
 * @param transport_type transport type.
 */
void CcBuildBridge(Commands, const CommandCost &result, TileIndex end_tile, TileIndex tile_start, TransportType transport_type, BridgeType, byte)
{
	if (result.Failed()) return;
	if (_settings_client.sound.confirm) SndPlayTileFx(SND_27_CONSTRUCTION_BRIDGE, end_tile);

	if (transport_type == TRANSPORT_ROAD) {
		DiagDirection end_direction = ReverseDiagDir(GetTunnelBridgeDirection(end_tile));
		ConnectRoadToStructure(end_tile, end_direction);

		DiagDirection start_direction = ReverseDiagDir(GetTunnelBridgeDirection(tile_start));
		ConnectRoadToStructure(tile_start, start_direction);
	}
}

/** Window class for handling the bridge-build GUI. */
class BuildBridgeWindow : public Window {
private:
	/* Runtime saved values */
	static Listing last_sorting; ///< Last setting of the sort.

	/* Constants for sorting the bridges */
	static const StringID sorter_names[];
	static GUIBridgeList::SortFunction * const sorter_funcs[];

	/* Internal variables */
	TileIndex start_tile;
	TileIndex end_tile;
	TransportType transport_type;
	byte road_rail_type;
	GUIBridgeList bridges;
	int icon_width; ///< Scaled width of the the bridge icon sprite.
	Scrollbar *vscroll;

	/** Sort the bridges by their index */
	static bool BridgeIndexSorter(const BuildBridgeData &a, const BuildBridgeData &b)
	{
		return a.index < b.index;
	}

	/** Sort the bridges by their price */
	static bool BridgePriceSorter(const BuildBridgeData &a, const BuildBridgeData &b)
	{
		return a.cost < b.cost;
	}

	/** Sort the bridges by their maximum speed */
	static bool BridgeSpeedSorter(const BuildBridgeData &a, const BuildBridgeData &b)
	{
		return a.spec->speed < b.spec->speed;
	}

	void BuildBridge(BridgeType type)
	{
		switch (this->transport_type) {
			case TRANSPORT_RAIL: _last_railbridge_type = type; break;
			case TRANSPORT_ROAD: _last_roadbridge_type = type; break;
			default: break;
		}
		Command<CMD_BUILD_BRIDGE>::Post(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE, CcBuildBridge,
					this->end_tile, this->start_tile, this->transport_type, type, this->road_rail_type);
	}

	/** Sort the builable bridges */
	void SortBridgeList()
	{
		this->bridges.Sort();

		/* Display the current sort variant */
		this->GetWidget<NWidgetCore>(WID_BBS_DROPDOWN_CRITERIA)->widget_data = this->sorter_names[this->bridges.SortType()];

		/* Set the modified widgets dirty */
		this->SetWidgetDirty(WID_BBS_DROPDOWN_CRITERIA);
		this->SetWidgetDirty(WID_BBS_BRIDGE_LIST);
	}

	/**
	 * Get the StringID to draw in the selection list and set the appropriate DParams.
	 * @param bridge_data the bridge to get the StringID of.
	 * @return the StringID.
	 */
	StringID GetBridgeSelectString(const BuildBridgeData &bridge_data) const
	{
		SetDParam(0, bridge_data.spec->material);
		SetDParam(1, PackVelocity(bridge_data.spec->speed, static_cast<VehicleType>(this->transport_type)));
		SetDParam(2, bridge_data.cost);
		/* If the bridge has no meaningful speed limit, don't display it. */
		if (bridge_data.spec->speed == UINT16_MAX) {
			return _game_mode == GM_EDITOR ? STR_SELECT_BRIDGE_INFO_NAME : STR_SELECT_BRIDGE_INFO_NAME_COST;
		}
		return _game_mode == GM_EDITOR ? STR_SELECT_BRIDGE_INFO_NAME_MAX_SPEED : STR_SELECT_BRIDGE_INFO_NAME_MAX_SPEED_COST;
	}

public:
	BuildBridgeWindow(WindowDesc *desc, TileIndex start, TileIndex end, TransportType transport_type, byte road_rail_type, GUIBridgeList &&bl) : Window(desc),
		start_tile(start),
		end_tile(end),
		transport_type(transport_type),
		road_rail_type(road_rail_type),
		bridges(std::move(bl))
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_BBS_SCROLLBAR);
		/* Change the data, or the caption of the gui. Set it to road or rail, accordingly. */
		this->GetWidget<NWidgetCore>(WID_BBS_CAPTION)->widget_data = (transport_type == TRANSPORT_ROAD) ? STR_SELECT_ROAD_BRIDGE_CAPTION : STR_SELECT_RAIL_BRIDGE_CAPTION;
		this->FinishInitNested(transport_type); // Initializes 'this->icon_width'.

		this->parent = FindWindowById(WC_BUILD_TOOLBAR, transport_type);
		this->bridges.SetListing(this->last_sorting);
		this->bridges.SetSortFuncs(this->sorter_funcs);
		this->bridges.NeedResort();
		this->SortBridgeList();

		this->vscroll->SetCount(this->bridges.size());
	}

	~BuildBridgeWindow()
	{
		this->last_sorting = this->bridges.GetListing();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_BBS_DROPDOWN_ORDER: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
			case WID_BBS_DROPDOWN_CRITERIA: {
				Dimension d = {0, 0};
				for (const StringID *str = this->sorter_names; *str != INVALID_STRING_ID; str++) {
					d = maxdim(d, GetStringBoundingBox(*str));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
			case WID_BBS_BRIDGE_LIST: {
				Dimension sprite_dim = {0, 0}; // Biggest bridge sprite dimension
				Dimension text_dim   = {0, 0}; // Biggest text dimension
				for (const BuildBridgeData &bridge_data : this->bridges) {
					sprite_dim = maxdim(sprite_dim, GetScaledSpriteSize(bridge_data.spec->sprite));
					text_dim = maxdim(text_dim, GetStringBoundingBox(GetBridgeSelectString(bridge_data)));
				}
				resize->height = std::max(sprite_dim.height, text_dim.height) + padding.height; // Max of both sizes + account for matrix edges.

				this->icon_width = sprite_dim.width; // Width of bridge icon.
				size->width = this->icon_width + WidgetDimensions::scaled.hsep_normal + text_dim.width + padding.width;
				size->height = 4 * resize->height; // Smallest bridge gui is 4 entries high in the matrix.
				break;
			}
		}
	}

	Point OnInitialPosition([[maybe_unused]] int16_t sm_width, [[maybe_unused]] int16_t sm_height, [[maybe_unused]] int window_number) override
	{
		/* Position the window so hopefully the first bridge from the list is under the mouse pointer. */
		NWidgetBase *list = this->GetWidget<NWidgetBase>(WID_BBS_BRIDGE_LIST);
		Point corner; // point of the top left corner of the window.
		corner.y = Clamp(_cursor.pos.y - list->pos_y - 5, GetMainViewTop(), GetMainViewBottom() - sm_height);
		corner.x = Clamp(_cursor.pos.x - list->pos_x - 5, 0, _screen.width - sm_width);
		return corner;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_BBS_DROPDOWN_ORDER:
				this->DrawSortButtonState(widget, this->bridges.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case WID_BBS_BRIDGE_LIST: {
				Rect tr = r.WithHeight(this->resize.step_height).Shrink(WidgetDimensions::scaled.matrix);
				bool rtl = _current_text_dir == TD_RTL;
				for (int i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && i < (int)this->bridges.size(); i++) {
					const BuildBridgeData &bridge_data = this->bridges.at(i);
					const BridgeSpec *b = bridge_data.spec;
					DrawSpriteIgnorePadding(b->sprite, b->pal, tr.WithWidth(this->icon_width, rtl), SA_HOR_CENTER | SA_BOTTOM);
					DrawStringMultiLine(tr.Indent(this->icon_width + WidgetDimensions::scaled.hsep_normal, rtl), GetBridgeSelectString(bridge_data));
					tr = tr.Translate(0, this->resize.step_height);
				}
				break;
			}
		}
	}

	EventState OnKeyPress([[maybe_unused]] char32_t key, uint16_t keycode) override
	{
		const uint8_t i = keycode - '1';
		if (i < 9 && i < this->bridges.size()) {
			/* Build the requested bridge */
			this->BuildBridge(this->bridges[i].index);
			this->Close();
			return ES_HANDLED;
		}
		return ES_NOT_HANDLED;
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			default: break;
			case WID_BBS_BRIDGE_LIST: {
				auto it = this->vscroll->GetScrolledItemFromWidget(this->bridges, pt.y, this, WID_BBS_BRIDGE_LIST);
				if (it != this->bridges.end()) {
					this->BuildBridge(it->index);
					this->Close();
				}
				break;
			}

			case WID_BBS_DROPDOWN_ORDER:
				this->bridges.ToggleSortOrder();
				this->SetDirty();
				break;

			case WID_BBS_DROPDOWN_CRITERIA:
				ShowDropDownMenu(this, this->sorter_names, this->bridges.SortType(), WID_BBS_DROPDOWN_CRITERIA, 0, 0);
				break;
		}
	}

	void OnDropdownSelect(WidgetID widget, int index) override
	{
		if (widget == WID_BBS_DROPDOWN_CRITERIA && this->bridges.SortType() != index) {
			this->bridges.SetSortType(index);

			this->SortBridgeList();
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_BBS_BRIDGE_LIST);
	}
};

/** Set the default sorting for the bridges */
Listing BuildBridgeWindow::last_sorting = {true, 2};

/** Available bridge sorting functions. */
GUIBridgeList::SortFunction * const BuildBridgeWindow::sorter_funcs[] = {
	&BridgeIndexSorter,
	&BridgePriceSorter,
	&BridgeSpeedSorter
};

/** Names of the sorting functions. */
const StringID BuildBridgeWindow::sorter_names[] = {
	STR_SORT_BY_NUMBER,
	STR_SORT_BY_COST,
	STR_SORT_BY_MAX_SPEED,
	INVALID_STRING_ID
};

/** Widgets of the bridge gui. */
static const NWidgetPart _nested_build_bridge_widgets[] = {
	/* Header */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_BBS_CAPTION), SetDataTip(STR_SELECT_RAIL_BRIDGE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),

	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			/* Sort order + criteria buttons */
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, COLOUR_DARK_GREEN, WID_BBS_DROPDOWN_ORDER), SetFill(1, 0), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
				NWidget(WWT_DROPDOWN, COLOUR_DARK_GREEN, WID_BBS_DROPDOWN_CRITERIA), SetFill(1, 0), SetDataTip(0x0, STR_TOOLTIP_SORT_CRITERIA),
			EndContainer(),
			/* Matrix. */
			NWidget(WWT_MATRIX, COLOUR_DARK_GREEN, WID_BBS_BRIDGE_LIST), SetFill(1, 0), SetResize(0, 22), SetMatrixDataTip(1, 0, STR_SELECT_BRIDGE_SELECTION_TOOLTIP), SetScrollbar(WID_BBS_SCROLLBAR),
		EndContainer(),

		/* scrollbar + resize button */
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_BBS_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
		EndContainer(),
	EndContainer(),
};

/** Window definition for the rail bridge selection window. */
static WindowDesc _build_bridge_desc(__FILE__, __LINE__,
	WDP_AUTO, "build_bridge", 200, 114,
	WC_BUILD_BRIDGE, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_bridge_widgets), std::end(_nested_build_bridge_widgets)
);

/**
 * Prepare the data for the build a bridge window.
 *  If we can't build a bridge under the given conditions
 *  show an error message.
 *
 * @param start The start tile of the bridge
 * @param end The end tile of the bridge
 * @param transport_type The transport type
 * @param road_rail_type The road/rail type
 */
void ShowBuildBridgeWindow(TileIndex start, TileIndex end, TransportType transport_type, byte road_rail_type)
{
	CloseWindowByClass(WC_BUILD_BRIDGE);

	/* The bridge length without ramps. */
	const uint bridge_len = GetTunnelBridgeLength(start, end);

	/* If Ctrl is being pressed, check whether the last bridge built is available
	 * If so, return this bridge type. Otherwise continue normally.
	 * We store bridge types for each transport type, so we have to check for
	 * the transport type beforehand.
	 */
	BridgeType last_bridge_type = 0;
	switch (transport_type) {
		case TRANSPORT_ROAD: last_bridge_type = _last_roadbridge_type; break;
		case TRANSPORT_RAIL: last_bridge_type = _last_railbridge_type; break;
		default: break; // water ways and air routes don't have bridge types
	}
	if (_ctrl_pressed && CheckBridgeAvailability(last_bridge_type, bridge_len).Succeeded()) {
		Command<CMD_BUILD_BRIDGE>::Post(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE, CcBuildBridge, end, start, transport_type, last_bridge_type, road_rail_type);
		return;
	}

	/* only query bridge building possibility once, result is the same for all bridges!
	 * returns CMD_ERROR on failure, and price on success */
	StringID errmsg = INVALID_STRING_ID;
	CommandCost ret = Command<CMD_BUILD_BRIDGE>::Do(CommandFlagsToDCFlags(GetCommandFlags<CMD_BUILD_BRIDGE>()) | DC_QUERY_COST, end, start, transport_type, 0, road_rail_type);

	GUIBridgeList bl;
	if (ret.Failed()) {
		errmsg = ret.GetErrorMessage();
	} else {
		/* check which bridges can be built */
		const uint tot_bridgedata_len = CalcBridgeLenCostFactor(bridge_len + 2);

		Money infra_cost = 0;
		switch (transport_type) {
			case TRANSPORT_ROAD: {
				/* In case we add a new road type as well, we must be aware of those costs. */
				RoadType road_rt = INVALID_ROADTYPE;
				RoadType tram_rt = INVALID_ROADTYPE;
				if (IsBridgeTile(start)) {
					road_rt = GetRoadTypeRoad(start);
					tram_rt = GetRoadTypeTram(start);
				}
				if (RoadTypeIsRoad((RoadType)road_rail_type)) {
					road_rt = (RoadType)road_rail_type;
				} else {
					tram_rt = (RoadType)road_rail_type;
				}

				if (road_rt != INVALID_ROADTYPE) infra_cost += (bridge_len + 2) * 2 * RoadBuildCost(road_rt);
				if (tram_rt != INVALID_ROADTYPE) infra_cost += (bridge_len + 2) * 2 * RoadBuildCost(tram_rt);

				break;
			}
			case TRANSPORT_RAIL: infra_cost = (bridge_len + 2) * RailBuildCost((RailType)road_rail_type); break;
			default: break;
		}

		bool any_available = false;
		CommandCost type_check;
		/* loop for all bridgetypes */
		for (BridgeType brd_type = 0; brd_type != MAX_BRIDGES; brd_type++) {
			type_check = CheckBridgeAvailability(brd_type, bridge_len);
			if (type_check.Succeeded()) {
				/* bridge is accepted, add to list */
				BuildBridgeData &item = bl.emplace_back();
				item.index = brd_type;
				item.spec = GetBridgeSpec(brd_type);
				/* Add to terraforming & bulldozing costs the cost of the
				 * bridge itself (not computed with DC_QUERY_COST) */
				item.cost = ret.GetCost() + (((int64_t)tot_bridgedata_len * _price[PR_BUILD_BRIDGE] * item.spec->price) >> 8) + infra_cost;
				any_available = true;
			}
		}
		/* give error cause if no bridges available here*/
		if (!any_available)
		{
			errmsg = type_check.GetErrorMessage();
		}
	}

	if (!bl.empty()) {
		new BuildBridgeWindow(&_build_bridge_desc, start, end, transport_type, road_rail_type, std::move(bl));
	} else {
		ShowErrorMessage(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE, errmsg, WL_INFO, TileX(end) * TILE_SIZE, TileY(end) * TILE_SIZE);
	}
}
