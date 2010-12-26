/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file order_gui.cpp GUI related to orders. */

#include "stdafx.h"
#include "command_func.h"
#include "viewport_func.h"
#include "depot_map.h"
#include "vehicle_gui.h"
#include "roadveh.h"
#include "timetable.h"
#include "strings_func.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "company_func.h"
#include "widgets/dropdown_func.h"
#include "textbuf_gui.h"
#include "string_func.h"
#include "tilehighlight_func.h"
#include "network/network.h"
#include "station_base.h"
#include "waypoint_base.h"
#include "core/geometry_func.hpp"
#include "hotkeys.h"

#include "table/strings.h"

/** Widget numbers of the order window. */
enum OrderWindowWidgets {
	ORDER_WIDGET_CAPTION,
	ORDER_WIDGET_TIMETABLE_VIEW,
	ORDER_WIDGET_ORDER_LIST,
	ORDER_WIDGET_SCROLLBAR,
	ORDER_WIDGET_SKIP,
	ORDER_WIDGET_DELETE,
	ORDER_WIDGET_STOP_SHARING,
	ORDER_WIDGET_NON_STOP,
	ORDER_WIDGET_GOTO,
	ORDER_WIDGET_FULL_LOAD,
	ORDER_WIDGET_UNLOAD,
	ORDER_WIDGET_REFIT,
	ORDER_WIDGET_SERVICE,
	ORDER_WIDGET_COND_VARIABLE,
	ORDER_WIDGET_COND_COMPARATOR,
	ORDER_WIDGET_COND_VALUE,
	ORDER_WIDGET_SEL_TOP_LEFT,   ///< #NWID_SELECTION widget for left part of the top row of the 'your train' order window.
	ORDER_WIDGET_SEL_TOP_MIDDLE, ///< #NWID_SELECTION widget for middle part of the top row of the 'your train' order window.
	ORDER_WIDGET_SEL_TOP_RIGHT,  ///< #NWID_SELECTION widget for right part of the top row of the 'your train' order window.
	ORDER_WIDGET_SEL_TOP_ROW,    ///< #NWID_SELECTION widget for the top row of the 'your non-trains' order window.
	ORDER_WIDGET_SEL_BOTTOM_MIDDLE, ///< #NWID_SELECTION widget for the middle part of the bottom row of the 'your train' order window.
	ORDER_WIDGET_SHARED_ORDER_LIST,
};

/** Order load types that could be given to station orders. */
static const StringID _station_load_types[][5] = {
	{
		STR_EMPTY,
		INVALID_STRING_ID,
		STR_ORDER_FULL_LOAD,
		STR_ORDER_FULL_LOAD_ANY,
		STR_ORDER_NO_LOAD,
	}, {
		STR_ORDER_UNLOAD,
		INVALID_STRING_ID,
		STR_ORDER_UNLOAD_FULL_LOAD,
		STR_ORDER_UNLOAD_FULL_LOAD_ANY,
		STR_ORDER_UNLOAD_NO_LOAD,
	}, {
		STR_ORDER_TRANSFER,
		INVALID_STRING_ID,
		STR_ORDER_TRANSFER_FULL_LOAD,
		STR_ORDER_TRANSFER_FULL_LOAD_ANY,
		STR_ORDER_TRANSFER_NO_LOAD,
	}, {
		/* Unload and transfer do not work together. */
		INVALID_STRING_ID,
		INVALID_STRING_ID,
		INVALID_STRING_ID,
		INVALID_STRING_ID,
	}, {
		STR_ORDER_NO_UNLOAD,
		INVALID_STRING_ID,
		STR_ORDER_NO_UNLOAD_FULL_LOAD,
		STR_ORDER_NO_UNLOAD_FULL_LOAD_ANY,
		INVALID_STRING_ID,
	}
};

static const StringID _order_non_stop_drowdown[] = {
	STR_ORDER_GO_TO,
	STR_ORDER_GO_NON_STOP_TO,
	STR_ORDER_GO_VIA,
	STR_ORDER_GO_NON_STOP_VIA,
	INVALID_STRING_ID
};

static const StringID _order_full_load_drowdown[] = {
	STR_ORDER_DROP_LOAD_IF_POSSIBLE,
	STR_EMPTY,
	STR_ORDER_DROP_FULL_LOAD_ALL,
	STR_ORDER_DROP_FULL_LOAD_ANY,
	STR_ORDER_DROP_NO_LOADING,
	INVALID_STRING_ID
};

static const StringID _order_unload_drowdown[] = {
	STR_ORDER_DROP_UNLOAD_IF_ACCEPTED,
	STR_ORDER_DROP_UNLOAD,
	STR_ORDER_DROP_TRANSFER,
	STR_EMPTY,
	STR_ORDER_DROP_NO_UNLOADING,
	INVALID_STRING_ID
};

static const StringID _order_goto_dropdown[] = {
	STR_ORDER_GO_TO,
	STR_ORDER_GO_TO_NEAREST_DEPOT,
	STR_ORDER_CONDITIONAL,
	STR_ORDER_SHARE,
	INVALID_STRING_ID
};

static const StringID _order_goto_dropdown_aircraft[] = {
	STR_ORDER_GO_TO,
	STR_ORDER_GO_TO_NEAREST_HANGAR,
	STR_ORDER_CONDITIONAL,
	STR_ORDER_SHARE,
	INVALID_STRING_ID
};

static const StringID _order_conditional_variable[] = {
	STR_ORDER_CONDITIONAL_LOAD_PERCENTAGE,
	STR_ORDER_CONDITIONAL_RELIABILITY,
	STR_ORDER_CONDITIONAL_MAX_SPEED,
	STR_ORDER_CONDITIONAL_AGE,
	STR_ORDER_CONDITIONAL_REQUIRES_SERVICE,
	STR_ORDER_CONDITIONAL_UNCONDITIONALLY,
	INVALID_STRING_ID,
};

static const StringID _order_conditional_condition[] = {
	STR_ORDER_CONDITIONAL_COMPARATOR_EQUALS,
	STR_ORDER_CONDITIONAL_COMPARATOR_NOT_EQUALS,
	STR_ORDER_CONDITIONAL_COMPARATOR_LESS_THAN,
	STR_ORDER_CONDITIONAL_COMPARATOR_LESS_EQUALS,
	STR_ORDER_CONDITIONAL_COMPARATOR_MORE_THAN,
	STR_ORDER_CONDITIONAL_COMPARATOR_MORE_EQUALS,
	STR_ORDER_CONDITIONAL_COMPARATOR_IS_TRUE,
	STR_ORDER_CONDITIONAL_COMPARATOR_IS_FALSE,
	INVALID_STRING_ID,
};

extern uint ConvertSpeedToDisplaySpeed(uint speed);
extern uint ConvertDisplaySpeedToSpeed(uint speed);

static const StringID _order_depot_action_dropdown[] = {
	STR_ORDER_DROP_GO_ALWAYS_DEPOT,
	STR_ORDER_DROP_SERVICE_DEPOT,
	STR_ORDER_DROP_HALT_DEPOT,
	INVALID_STRING_ID
};

static int DepotActionStringIndex(const Order *order)
{
	if (order->GetDepotActionType() & ODATFB_HALT) {
		return DA_STOP;
	} else if (order->GetDepotOrderType() & ODTFB_SERVICE) {
		return DA_SERVICE;
	} else {
		return DA_ALWAYS_GO;
	}
}

/**
 * Draws an order in order or timetable GUI
 * @param v Vehicle the order belongs to
 * @param order The order to draw
 * @param order_index Index of the order in the orders of the vehicle
 * @param y Y position for drawing
 * @param selected True, if the order is selected
 * @param timetable True, when drawing in the timetable GUI
 * @param left Left border for text drawing
 * @param middle X position between order index and order text
 * @param right Right border for text drawing
 */
void DrawOrderString(const Vehicle *v, const Order *order, int order_index, int y, bool selected, bool timetable, int left, int middle, int right)
{
	bool rtl = _current_text_dir == TD_RTL;

	SpriteID sprite = rtl ? SPR_ARROW_LEFT : SPR_ARROW_RIGHT;
	Dimension sprite_size = GetSpriteSize(sprite);
	if (v->cur_order_index == order_index) {
		DrawSprite(sprite, PAL_NONE, rtl ? right - sprite_size.width : left, y + ((int)FONT_HEIGHT_NORMAL - (int)sprite_size.height) / 2);
	}

	TextColour colour = TC_BLACK;
	if (order->IsType(OT_AUTOMATIC)) {
		colour = (selected ? TC_SILVER : TC_GREY) | TC_NO_SHADE;
	} else if (selected) {
		colour = TC_WHITE;
	}

	SetDParam(0, order_index + 1);
	DrawString(left, rtl ? right - sprite_size.width - 3 : middle, y, STR_ORDER_INDEX, colour, SA_RIGHT | SA_FORCE);

	SetDParam(5, STR_EMPTY);

	switch (order->GetType()) {
		case OT_DUMMY:
			SetDParam(0, STR_INVALID_ORDER);
			SetDParam(1, order->GetDestination());
			break;

		case OT_AUTOMATIC:
			SetDParam(0, STR_ORDER_GO_TO_STATION);
			SetDParam(1, STR_ORDER_GO_TO);
			SetDParam(2, order->GetDestination());
			SetDParam(3, timetable ? STR_EMPTY : STR_ORDER_AUTOMATIC);
			break;

		case OT_GOTO_STATION: {
			OrderLoadFlags load = order->GetLoadType();
			OrderUnloadFlags unload = order->GetUnloadType();

			SetDParam(0, STR_ORDER_GO_TO_STATION);
			SetDParam(1, STR_ORDER_GO_TO + (v->IsGroundVehicle() ? order->GetNonStopType() : 0));
			SetDParam(2, order->GetDestination());

			if (timetable) {
				SetDParam(3, STR_EMPTY);

				if (order->wait_time > 0) {
					SetDParam(5, STR_TIMETABLE_STAY_FOR);
					SetTimetableParams(6, 7, order->wait_time);
				}
			} else {
				SetDParam(3, (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) ? STR_EMPTY : _station_load_types[unload][load]);
				if (v->type == VEH_TRAIN && (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) == 0) {
					SetDParam(5, order->GetStopLocation() + STR_ORDER_STOP_LOCATION_NEAR_END);
				}
			}
			break;
		}

		case OT_GOTO_DEPOT:
			if (order->GetDepotActionType() & ODATFB_NEAREST_DEPOT) {
				SetDParam(0, STR_ORDER_GO_TO_NEAREST_DEPOT_FORMAT);
				if (v->type == VEH_AIRCRAFT) {
					SetDParam(2, STR_ORDER_NEAREST_HANGAR);
					SetDParam(3, STR_EMPTY);
				} else {
					SetDParam(2, STR_ORDER_NEAREST_DEPOT);
					SetDParam(3, STR_ORDER_TRAIN_DEPOT + v->type);
				}
			} else {
				SetDParam(0, STR_ORDER_GO_TO_DEPOT_FORMAT);
				SetDParam(2, v->type);
				SetDParam(3, order->GetDestination());
			}

			if (order->GetDepotOrderType() & ODTFB_SERVICE) {
				SetDParam(1, (order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS) ? STR_ORDER_SERVICE_NON_STOP_AT : STR_ORDER_SERVICE_AT);
			} else {
				SetDParam(1, (order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS) ? STR_ORDER_GO_NON_STOP_TO : STR_ORDER_GO_TO);
			}

			if (!timetable && (order->GetDepotActionType() & ODATFB_HALT)) {
				SetDParam(5, STR_ORDER_STOP_ORDER);
			}

			if (!timetable && order->IsRefit()) {
				SetDParam(5, (order->GetDepotActionType() & ODATFB_HALT) ? STR_ORDER_REFIT_STOP_ORDER : STR_ORDER_REFIT_ORDER);
				SetDParam(6, CargoSpec::Get(order->GetRefitCargo())->name);
			}
			break;

		case OT_GOTO_WAYPOINT:
			SetDParam(0, (order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS) ? STR_ORDER_GO_NON_STOP_TO_WAYPOINT : STR_ORDER_GO_TO_WAYPOINT);
			SetDParam(1, order->GetDestination());
			break;

		case OT_CONDITIONAL:
			SetDParam(1, order->GetConditionSkipToOrder() + 1);
			if (order->GetConditionVariable() == OCV_UNCONDITIONALLY) {
				SetDParam(0, STR_ORDER_CONDITIONAL_UNCONDITIONAL);
			} else {
				OrderConditionComparator occ = order->GetConditionComparator();
				SetDParam(0, (occ == OCC_IS_TRUE || occ == OCC_IS_FALSE) ? STR_ORDER_CONDITIONAL_TRUE_FALSE : STR_ORDER_CONDITIONAL_NUM);
				SetDParam(2, STR_ORDER_CONDITIONAL_LOAD_PERCENTAGE + order->GetConditionVariable());
				SetDParam(3, STR_ORDER_CONDITIONAL_COMPARATOR_EQUALS + occ);

				uint value = order->GetConditionValue();
				if (order->GetConditionVariable() == OCV_MAX_SPEED) value = ConvertSpeedToDisplaySpeed(value);
				SetDParam(4, value);
			}

			if (timetable && order->wait_time > 0) {
				SetDParam(5, STR_TIMETABLE_AND_TRAVEL_FOR);
				SetTimetableParams(6, 7, order->wait_time);
			} else {
				SetDParam(5, STR_EMPTY);
			}
			break;

		default: NOT_REACHED();
	}

	DrawString(rtl ? left : middle, rtl ? middle : right, y, STR_ORDER_TEXT, colour);
}


static Order GetOrderCmdFromTile(const Vehicle *v, TileIndex tile)
{
	Order order;
	order.next  = NULL;
	order.index = 0;

	/* check depot first */
	if (_settings_game.order.gotodepot) {
		switch (GetTileType(tile)) {
			case MP_RAILWAY:
				if (v->type == VEH_TRAIN && IsTileOwner(tile, _local_company)) {
					if (IsRailDepot(tile)) {
						order.MakeGoToDepot(GetDepotIndex(tile), ODTFB_PART_OF_ORDERS,
								_settings_client.gui.new_nonstop ? ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS : ONSF_STOP_EVERYWHERE);
						if (_ctrl_pressed) order.SetDepotOrderType((OrderDepotTypeFlags)(order.GetDepotOrderType() ^ ODTFB_SERVICE));
						return order;
					}
				}
				break;

			case MP_ROAD:
				if (IsRoadDepot(tile) && v->type == VEH_ROAD && IsTileOwner(tile, _local_company)) {
					order.MakeGoToDepot(GetDepotIndex(tile), ODTFB_PART_OF_ORDERS,
							_settings_client.gui.new_nonstop ? ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS : ONSF_STOP_EVERYWHERE);
					if (_ctrl_pressed) order.SetDepotOrderType((OrderDepotTypeFlags)(order.GetDepotOrderType() ^ ODTFB_SERVICE));
					return order;
				}
				break;

			case MP_STATION:
				if (v->type != VEH_AIRCRAFT) break;
				if (IsHangar(tile) && IsTileOwner(tile, _local_company)) {
					order.MakeGoToDepot(GetStationIndex(tile), ODTFB_PART_OF_ORDERS, ONSF_STOP_EVERYWHERE);
					if (_ctrl_pressed) order.SetDepotOrderType((OrderDepotTypeFlags)(order.GetDepotOrderType() ^ ODTFB_SERVICE));
					return order;
				}
				break;

			case MP_WATER:
				if (v->type != VEH_SHIP) break;
				if (IsShipDepot(tile) && IsTileOwner(tile, _local_company)) {
					order.MakeGoToDepot(GetDepotIndex(tile), ODTFB_PART_OF_ORDERS, ONSF_STOP_EVERYWHERE);
					if (_ctrl_pressed) order.SetDepotOrderType((OrderDepotTypeFlags)(order.GetDepotOrderType() ^ ODTFB_SERVICE));
					return order;
				}
				break;

			default:
				break;
		}
	}

	/* check waypoint */
	if (IsRailWaypointTile(tile) &&
			v->type == VEH_TRAIN &&
			IsTileOwner(tile, _local_company)) {
		order.MakeGoToWaypoint(Waypoint::GetByTile(tile)->index);
		if (_settings_client.gui.new_nonstop != _ctrl_pressed) order.SetNonStopType(ONSF_NO_STOP_AT_ANY_STATION);
		return order;
	}

	if ((IsBuoyTile(tile) && v->type == VEH_SHIP) || (IsRailWaypointTile(tile) && v->type == VEH_TRAIN)) {
		order.MakeGoToWaypoint(GetStationIndex(tile));
		return order;
	}

	if (IsTileType(tile, MP_STATION)) {
		StationID st_index = GetStationIndex(tile);
		const Station *st = Station::Get(st_index);

		if (st->owner == _local_company || st->owner == OWNER_NONE) {
			byte facil;
			(facil = FACIL_DOCK, v->type == VEH_SHIP) ||
			(facil = FACIL_TRAIN, v->type == VEH_TRAIN) ||
			(facil = FACIL_AIRPORT, v->type == VEH_AIRCRAFT) ||
			(facil = FACIL_BUS_STOP, v->type == VEH_ROAD && RoadVehicle::From(v)->IsBus()) ||
			(facil = FACIL_TRUCK_STOP, 1);
			if (st->facilities & facil) {
				order.MakeGoToStation(st_index);
				if (_ctrl_pressed) order.SetLoadType(OLF_FULL_LOAD_ANY);
				if (_settings_client.gui.new_nonstop && v->IsGroundVehicle()) order.SetNonStopType(ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
				order.SetStopLocation(v->type == VEH_TRAIN ? (OrderStopLocation)(_settings_client.gui.stop_location) : OSL_PLATFORM_FAR_END);
				return order;
			}
		}
	}

	/* not found */
	order.Free();
	return order;
}

/**
 * %Order window code for all vehicles.
 *
 * At the bottom of the window two button rows are located for changing the orders of the vehicle.
 *
 * \section top-row Top row
 * The top-row is for manipulating an individual order. What row is displayed depends on the type of vehicle, and whether or not you are the owner of the vehicle.
 *
 * The top-row buttons of one of your trains or road vehicles is one of the following three cases:
 * \verbatim
 * +-----------------+-----------------+-----------------+
 * |    NON-STOP     |    FULL_LOAD    |     UNLOAD      | (normal)
 * +-----------------+-----------------+-----------------+
 * |    COND_VAR     | COND_COMPARATOR |   COND_VALUE    | (for conditional orders)
 * +-----------------+-----------------+-----------------+
 * |    NON-STOP     |      REFIT      |     SERVICE     | (for depot orders)
 * +-----------------+-----------------+-----------------+
 * \endverbatim
 *
 * Airplanes and ships have one of the following three top-row button rows:
 * \verbatim
 * +--------------------------+--------------------------+
 * |          FULL_LOAD       |          UNLOAD          | (normal)
 * +-----------------+--------+--------+-----------------+
 * |    COND_VAR     | COND_COMPARATOR |   COND_VALUE    | (for conditional orders)
 * +-----------------+--------+--------+-----------------+
 * |            REFIT         |          SERVICE         | (for depot order)
 * +--------------------------+--------------------------+
 * \endverbatim
 *
 * \section bottom-row Bottom row
 * The second row (the bottom row) is for manipulating the list of orders:
 * \verbatim
 * +-----------------+-----------------+-----------------+
 * |      SKIP       |     DELETE      |      GOTO       |
 * +-----------------+-----------------+-----------------+
 * \endverbatim
 *
 * For vehicles of other companies, both button rows are not displayed.
 */
struct OrdersWindow : public Window {
private:
	/** Under what reason are we using the PlaceObject functionality? */
	enum OrderPlaceObjectState {
		OPOS_GOTO,
		OPOS_CONDITIONAL,
		OPOS_SHARE,
	};

	/** Displayed planes of the #NWID_SELECTION widgets. */
	enum DisplayPane {
		/* ORDER_WIDGET_SEL_TOP_LEFT */
		DP_LEFT_NONSTOP    = 0, ///< Display 'non stop' in the left button of the top row of the train/rv order window.
		DP_LEFT_CONDVAR    = 1, ///< Display condition variable in the left button of the top row of the train/rv order window.

		/* ORDER_WIDGET_SEL_TOP_MIDDLE */
		DP_MIDDLE_LOAD     = 0, ///< Display 'load' in the middle button of the top row of the train/rv order window.
		DP_MIDDLE_REFIT    = 1, ///< Display 'refit' in the middle button of the top row of the train/rv order window.
		DP_MIDDLE_COMPARE  = 2, ///< Display compare operator in the middle button of the top row of the train/rv order window.

		/* ORDER_WIDGET_SEL_TOP_RIGHT */
		DP_RIGHT_UNLOAD    = 0, ///< Display 'unload' in the right button of the top row of the train/rv order window.
		DP_RIGHT_SERVICE   = 1, ///< Display 'service' in the right button of the top row of the train/rv order window.
		DP_RIGHT_CONDVAL   = 2, ///< Display condition value in the right button of the top row of the train/rv order window.

		/* ORDER_WIDGET_SEL_TOP_ROW */
		DP_ROW_LOAD        = 0, ///< Display 'load' / 'unload' buttons in the top row of the ship/airplane order window.
		DP_ROW_DEPOT       = 1, ///< Display 'refit' / 'service' buttons in the top row of the ship/airplane order window.
		DP_ROW_CONDITIONAL = 2, ///< Display the conditional order buttons in the top row of the ship/airplane order window.

		/* ORDER_WIDGET_SEL_BOTTOM_MIDDLE */
		DP_BOTTOM_MIDDLE_DELETE       = 0, ///< Display 'delete' in the middle button of the bottom row of the vehicle order window.
		DP_BOTTOM_MIDDLE_STOP_SHARING = 1, ///< Display 'stop sharing' in the middle button of the bottom row of the vehicle order window.
	};

	int selected_order;
	OrderID order_over;     ///< Order over which another order is dragged, \c INVALID_ORDER if none.
	OrderPlaceObjectState goto_type;
	const Vehicle *vehicle; ///< Vehicle owning the orders being displayed and manipulated.
	Scrollbar *vscroll;

	/**
	 * Return the memorised selected order.
	 * @return the memorised order if it is a vaild one
	 *  else return the number of orders
	 */
	int OrderGetSel() const
	{
		int num = this->selected_order;
		return (num >= 0 && num < vehicle->GetNumOrders()) ? num : vehicle->GetNumOrders();
	}

	/**
	 * Calculate the selected order.
	 * The calculation is based on the relative (to the window) y click position and
	 *  the position of the scrollbar.
	 *
	 * @param y Y-value of the click relative to the window origin
	 * @return The selected order if the order is valid, else return \c INVALID_ORDER.
	 */
	int GetOrderFromPt(int y)
	{
		NWidgetBase *nwid = this->GetWidget<NWidgetBase>(ORDER_WIDGET_ORDER_LIST);
		int sel = (y - nwid->pos_y - WD_FRAMERECT_TOP) / nwid->resize_y; // Selected line in the ORDER_WIDGET_ORDER_LIST panel.

		if ((uint)sel >= this->vscroll->GetCapacity()) return INVALID_ORDER;

		sel += this->vscroll->GetPosition();

		return (sel <= vehicle->GetNumOrders() && sel >= 0) ? sel : INVALID_ORDER;
	}

	/**
	 * Handle the click on the goto button.
	 * @param i Dummy parameter.
	 */
	void OrderClick_Goto(int i)
	{
		this->SetWidgetDirty(ORDER_WIDGET_GOTO);
		this->ToggleWidgetLoweredState(ORDER_WIDGET_GOTO);
		if (this->IsWidgetLowered(ORDER_WIDGET_GOTO)) {
			SetObjectToPlaceWnd(ANIMCURSOR_PICKSTATION, PAL_NONE, HT_RECT | HT_VEHICLE, this);
			this->goto_type = OPOS_GOTO;
		} else {
			ResetObjectToPlace();
		}
	}

	/**
	 * Handle the click on the full load button.
	 * @param load_type the way to load.
	 */
	void OrderClick_FullLoad(int load_type)
	{
		VehicleOrderID sel_ord = this->OrderGetSel();
		const Order *order = this->vehicle->GetOrder(sel_ord);

		if (order == NULL || order->GetLoadType() == load_type) return;

		if (load_type < 0) {
			load_type = order->GetLoadType() == OLF_LOAD_IF_POSSIBLE ? OLF_FULL_LOAD_ANY : OLF_LOAD_IF_POSSIBLE;
		}
		DoCommandP(this->vehicle->tile, this->vehicle->index + (sel_ord << 20), MOF_LOAD | (load_type << 4), CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
	}

	/**
	 * Handle the 'no loading' hotkey
	 */
	void OrderHotkey_NoLoad(int i)
	{
		this->OrderClick_FullLoad(OLFB_NO_LOAD);
	}

	/**
	 * Handle the click on the service.
	 */
	void OrderClick_Service(int i)
	{
		VehicleOrderID sel_ord = this->OrderGetSel();

		if (i < 0) {
			const Order *order = this->vehicle->GetOrder(sel_ord);
			if (order == NULL) return;
			i = (order->GetDepotOrderType() & ODTFB_SERVICE) ? DA_ALWAYS_GO : DA_SERVICE;
		}
		DoCommandP(this->vehicle->tile, this->vehicle->index + (sel_ord << 20), MOF_DEPOT_ACTION | (i << 4), CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
	}

	/**
	 * Handle the click on the service in nearest depot button.
	 * @param i Dummy parameter.
	 */
	void OrderClick_NearestDepot(int i)
	{
		Order order;
		order.next = NULL;
		order.index = 0;
		order.MakeGoToDepot(0, ODTFB_PART_OF_ORDERS,
				_settings_client.gui.new_nonstop && this->vehicle->IsGroundVehicle() ? ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS : ONSF_STOP_EVERYWHERE);
		order.SetDepotActionType(ODATFB_NEAREST_DEPOT);

		DoCommandP(this->vehicle->tile, this->vehicle->index + (this->OrderGetSel() << 20), order.Pack(), CMD_INSERT_ORDER | CMD_MSG(STR_ERROR_CAN_T_INSERT_NEW_ORDER));
	}

	/**
	 * Handle the click on the conditional order button.
	 * @param i Dummy parameter.
	 */
	void OrderClick_Conditional(int i)
	{
		this->LowerWidget(ORDER_WIDGET_GOTO);
		this->SetWidgetDirty(ORDER_WIDGET_GOTO);
		SetObjectToPlaceWnd(ANIMCURSOR_PICKSTATION, PAL_NONE, HT_NONE, this);
		this->goto_type = OPOS_CONDITIONAL;
	}

	/**
	 * Handle the click on the share button.
	 * @param i Dummy parameter.
	 */
	void OrderClick_Share(int i)
	{
		this->LowerWidget(ORDER_WIDGET_GOTO);
		this->SetWidgetDirty(ORDER_WIDGET_GOTO);
		SetObjectToPlaceWnd(ANIMCURSOR_PICKSTATION, PAL_NONE, HT_VEHICLE, this);
		this->goto_type = OPOS_SHARE;
	}

	/**
	 * Handle the click on the unload button.
	 */
	void OrderClick_Unload(int unload_type)
	{
		VehicleOrderID sel_ord = this->OrderGetSel();
		const Order *order = this->vehicle->GetOrder(sel_ord);

		if (order == NULL || order->GetUnloadType() == unload_type) return;

		if (unload_type < 0) {
			unload_type = order->GetUnloadType() == OUF_UNLOAD_IF_POSSIBLE ? OUFB_UNLOAD : OUF_UNLOAD_IF_POSSIBLE;
		}

		DoCommandP(this->vehicle->tile, this->vehicle->index + (sel_ord << 20), MOF_UNLOAD | (unload_type << 4), CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));

		/* Transfer orders with leave empty as default */
		if (unload_type == OUFB_TRANSFER) {
			DoCommandP(this->vehicle->tile, this->vehicle->index + (sel_ord << 20), MOF_LOAD | (OLFB_NO_LOAD << 4), CMD_MODIFY_ORDER);
			this->SetWidgetDirty(ORDER_WIDGET_FULL_LOAD);
		}
	}

	/**
	 * Handle the transfer hotkey
	 */
	void OrderHotkey_Transfer(int i)
	{
		this->OrderClick_Unload(OUFB_TRANSFER);
	}

	/**
	 * Handle the 'no unload' hotkey
	 */
	void OrderHotkey_NoUnload(int i)
	{
		this->OrderClick_Unload(OUFB_NO_UNLOAD);
	}

	/**
	 * Handle the click on the nonstop button.
	 * @param non_stop what non-stop type to use; -1 to use the 'next' one.
	 */
	void OrderClick_Nonstop(int non_stop)
	{
		if (!this->vehicle->IsGroundVehicle()) return;

		VehicleOrderID sel_ord = this->OrderGetSel();
		const Order *order = this->vehicle->GetOrder(sel_ord);

		if (order == NULL || order->GetNonStopType() == non_stop) return;

		/* Keypress if negative, so 'toggle' to the next */
		if (non_stop < 0) {
			non_stop = order->GetNonStopType() ^ ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS;
		}

		this->SetWidgetDirty(ORDER_WIDGET_NON_STOP);
		DoCommandP(this->vehicle->tile, this->vehicle->index + (sel_ord << 20), MOF_NON_STOP | non_stop << 4,  CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
	}

	/**
	 * Handle the click on the skip button.
	 * If ctrl is pressed, skip to selected order, else skip to current order + 1
	 * @param i Dummy parameter.
	 */
	void OrderClick_Skip(int i)
	{
		/* Don't skip when there's nothing to skip */
		if (_ctrl_pressed && this->vehicle->cur_order_index == this->OrderGetSel()) return;
		if (this->vehicle->GetNumOrders() <= 1) return;

		DoCommandP(this->vehicle->tile, this->vehicle->index, _ctrl_pressed ? this->OrderGetSel() : ((this->vehicle->cur_order_index + 1) % this->vehicle->GetNumOrders()),
				CMD_SKIP_TO_ORDER | CMD_MSG(_ctrl_pressed ? STR_ERROR_CAN_T_SKIP_TO_ORDER : STR_ERROR_CAN_T_SKIP_ORDER));
	}

	/**
	 * Handle the click on the delete button.
	 * @param i Dummy parameter.
	 */
	void OrderClick_Delete(int i)
	{
		/* When networking, move one order lower */
		int selected = this->selected_order + (int)_networking;

		if (DoCommandP(this->vehicle->tile, this->vehicle->index, this->OrderGetSel(), CMD_DELETE_ORDER | CMD_MSG(STR_ERROR_CAN_T_DELETE_THIS_ORDER))) {
			this->selected_order = selected >= this->vehicle->GetNumOrders() ? -1 : selected;
			this->UpdateButtonState();
		}
	}

	/**
	 * Handle the click on the 'stop sharing' button.
	 * If 'End of Shared Orders' isn't selected, do nothing. If Ctrl is pressed, call OrderClick_Delete and exit.
	 * To stop sharing this vehicle order list, we copy the orders of a vehicle that share this order list. That way we
	 * exit the group of shared vehicles while keeping the same order list.
	 * @param i Dummy parameter.
	 */
	void OrderClick_StopSharing(int i)
	{
		/* Don't try to stop sharing orders if 'End of Shared Orders' isn't selected. */
		if (!this->vehicle->IsOrderListShared() || this->selected_order != this->vehicle->GetNumOrders()) return;
		/* If Ctrl is pressed, delete the order list as if we clicked the 'Delete' button. */
		if (_ctrl_pressed) {
			this->OrderClick_Delete(0);
			return;
		}

		/* Get another vehicle that share orders with this vehicle. */
		Vehicle *other_shared = (this->vehicle->FirstShared() == this->vehicle) ? this->vehicle->NextShared() : this->vehicle->PreviousShared();
		/* Copy the order list of the other vehicle. */
		if (DoCommandP(this->vehicle->tile, this->vehicle->index | CO_COPY << 30, other_shared->index, CMD_CLONE_ORDER | CMD_MSG(STR_ERROR_CAN_T_STOP_SHARING_ORDER_LIST))) {
			this->UpdateButtonState();
		}
	}

	/**
	 * Handle the click on the refit button.
	 * If ctrl is pressed, cancel refitting, else show the refit window.
	 * @param i Dummy parameter.
	 */
	void OrderClick_Refit(int i)
	{
		if (_ctrl_pressed) {
			/* Cancel refitting */
			DoCommandP(this->vehicle->tile, this->vehicle->index, (this->OrderGetSel() << 16) | (CT_NO_REFIT << 8) | CT_NO_REFIT, CMD_ORDER_REFIT);
		} else {
			ShowVehicleRefitWindow(this->vehicle, this->OrderGetSel(), this);
		}
	}

public:
	OrdersWindow(const WindowDesc *desc, const Vehicle *v) : Window()
	{
		this->vehicle = v;

		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(ORDER_WIDGET_SCROLLBAR);
		this->FinishInitNested(desc, v->index);

		this->selected_order = -1;
		this->order_over = INVALID_ORDER;
		this->owner = v->owner;

		if (_settings_client.gui.quick_goto && v->owner == _local_company) {
			/* If there are less than 2 station, make Go To active. */
			int station_orders = 0;
			const Order *order;
			FOR_VEHICLE_ORDERS(v, order) {
				if (order->IsType(OT_GOTO_STATION)) station_orders++;
			}

			if (station_orders < 2) this->OrderClick_Goto(0);
		}
		this->OnInvalidateData(-2);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case ORDER_WIDGET_TIMETABLE_VIEW:
				if (!_settings_game.order.timetabling) size->width = 0;
				break;

			case ORDER_WIDGET_ORDER_LIST:
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = 6 * resize->height + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				break;

			case ORDER_WIDGET_COND_VARIABLE: {
				Dimension d = {0, 0};
				for (int i = 0; _order_conditional_variable[i] != INVALID_STRING_ID; i++) {
					d = maxdim(d, GetStringBoundingBox(_order_conditional_variable[i]));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case ORDER_WIDGET_COND_COMPARATOR: {
				Dimension d = {0, 0};
				for (int i = 0; _order_conditional_condition[i] != INVALID_STRING_ID; i++) {
					d = maxdim(d, GetStringBoundingBox(_order_conditional_condition[i]));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
		}
	}

	virtual void OnInvalidateData(int data)
	{
		VehicleOrderID from = GB(data, 0, 8);
		VehicleOrderID to   = GB(data, 8, 8);

		switch (data) {
			case 0:
				/* Autoreplace replaced the vehicle */
				this->vehicle = Vehicle::Get(this->window_number);
				break;

			case -1:
				/* Removed / replaced all orders (after deleting / sharing) */
				if (this->selected_order == -1) break;

				this->DeleteChildWindows();
				HideDropDownMenu(this);
				this->selected_order = -1;
				break;

			case -2:
				/* Some other order changes */
				break;

			default:
				/* Moving an order. If one of these is INVALID_VEH_ORDER_ID, then
				 * the order is being created / removed */
				if (this->selected_order == -1) break;

				if (from == to) break; // no need to change anything

				if (from != this->selected_order) {
					/* Moving from preceding order? */
					this->selected_order -= (int)(from <= this->selected_order);
					/* Moving to   preceding order? */
					this->selected_order += (int)(to   <= this->selected_order);
					break;
				}

				/* Now we are modifying the selected order */
				if (to == INVALID_VEH_ORDER_ID) {
					/* Deleting selected order */
					this->DeleteChildWindows();
					HideDropDownMenu(this);
					this->selected_order = -1;
					break;
				}

				/* Moving selected order */
				this->selected_order = to;
				break;
		}

		this->vscroll->SetCount(this->vehicle->GetNumOrders() + 1);
		this->UpdateButtonState();

		/* Scroll to the new order. */
		if (from == INVALID_VEH_ORDER_ID && to != INVALID_VEH_ORDER_ID && !this->vscroll->IsVisible(to)) {
			this->vscroll->ScrollTowards(to);
		}
	}

	void UpdateButtonState()
	{
		if (this->vehicle->owner != _local_company) return; // No buttons are displayed with competitor order windows.

		bool shared_orders = this->vehicle->IsOrderListShared();
		int sel = this->OrderGetSel();
		const Order *order = this->vehicle->GetOrder(sel);

		/* Second row. */
		/* skip */
		this->SetWidgetDisabledState(ORDER_WIDGET_SKIP, this->vehicle->GetNumOrders() <= 1);

		/* delete / stop sharing */
		NWidgetStacked *delete_sel = this->GetWidget<NWidgetStacked>(ORDER_WIDGET_SEL_BOTTOM_MIDDLE);
		if (shared_orders && this->selected_order == this->vehicle->GetNumOrders()) {
			/* The 'End of Shared Orders' order is selected, show the 'stop sharing' button. */
			delete_sel->SetDisplayedPlane(DP_BOTTOM_MIDDLE_STOP_SHARING);
		} else {
			/* The 'End of Shared Orders' order isn't selected, show the 'delete' button. */
			delete_sel->SetDisplayedPlane(DP_BOTTOM_MIDDLE_DELETE);
			this->SetWidgetDisabledState(ORDER_WIDGET_DELETE,
				(uint)this->vehicle->GetNumOrders() + ((shared_orders || this->vehicle->GetNumOrders() != 0) ? 1 : 0) <= (uint)this->selected_order);

			/* Set the tooltip of the 'delete' button depending on whether the
			 * 'End of Orders' order or a regular order is selected. */
			NWidgetCore *nwi = this->GetWidget<NWidgetCore>(ORDER_WIDGET_DELETE);
			if (this->selected_order == this->vehicle->GetNumOrders()) {
				nwi->SetDataTip(STR_ORDERS_DELETE_BUTTON, STR_ORDERS_DELETE_ALL_TOOLTIP);
			} else {
				nwi->SetDataTip(STR_ORDERS_DELETE_BUTTON, STR_ORDERS_DELETE_TOOLTIP);
			}
		}

		/* First row. */
		this->RaiseWidget(ORDER_WIDGET_FULL_LOAD);
		this->RaiseWidget(ORDER_WIDGET_UNLOAD);
		this->RaiseWidget(ORDER_WIDGET_SERVICE);

		/* Selection widgets. */
		/* Train or road vehicle. */
		NWidgetStacked *left_sel   = this->GetWidget<NWidgetStacked>(ORDER_WIDGET_SEL_TOP_LEFT);
		NWidgetStacked *middle_sel = this->GetWidget<NWidgetStacked>(ORDER_WIDGET_SEL_TOP_MIDDLE);
		NWidgetStacked *right_sel  = this->GetWidget<NWidgetStacked>(ORDER_WIDGET_SEL_TOP_RIGHT);
		/* Ship or airplane. */
		NWidgetStacked *row_sel = this->GetWidget<NWidgetStacked>(ORDER_WIDGET_SEL_TOP_ROW);
		assert(row_sel != NULL || (left_sel != NULL && middle_sel != NULL && right_sel != NULL));


		if (order == NULL) {
			if (row_sel != NULL) {
				row_sel->SetDisplayedPlane(DP_ROW_LOAD);
			} else {
				left_sel->SetDisplayedPlane(DP_LEFT_NONSTOP);
				middle_sel->SetDisplayedPlane(DP_MIDDLE_LOAD);
				right_sel->SetDisplayedPlane(DP_RIGHT_UNLOAD);
				this->DisableWidget(ORDER_WIDGET_NON_STOP);
				this->RaiseWidget(ORDER_WIDGET_NON_STOP);
			}
			this->DisableWidget(ORDER_WIDGET_FULL_LOAD);
			this->DisableWidget(ORDER_WIDGET_UNLOAD);
		} else {
			this->SetWidgetDisabledState(ORDER_WIDGET_FULL_LOAD, (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) != 0); // full load
			this->SetWidgetDisabledState(ORDER_WIDGET_UNLOAD,    (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) != 0); // unload

			switch (order->GetType()) {
				case OT_GOTO_STATION:
					if (row_sel != NULL) {
						row_sel->SetDisplayedPlane(DP_ROW_LOAD);
					} else {
						left_sel->SetDisplayedPlane(DP_LEFT_NONSTOP);
						middle_sel->SetDisplayedPlane(DP_MIDDLE_LOAD);
						right_sel->SetDisplayedPlane(DP_RIGHT_UNLOAD);
						this->EnableWidget(ORDER_WIDGET_NON_STOP);
						this->SetWidgetLoweredState(ORDER_WIDGET_NON_STOP, order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
					}
					this->SetWidgetLoweredState(ORDER_WIDGET_FULL_LOAD, order->GetLoadType() == OLF_FULL_LOAD_ANY);
					this->SetWidgetLoweredState(ORDER_WIDGET_UNLOAD, order->GetUnloadType() == OUFB_UNLOAD);
					break;

				case OT_GOTO_WAYPOINT:
					if (row_sel != NULL) {
						row_sel->SetDisplayedPlane(DP_ROW_LOAD);
					} else {
						left_sel->SetDisplayedPlane(DP_LEFT_NONSTOP);
						middle_sel->SetDisplayedPlane(DP_MIDDLE_LOAD);
						right_sel->SetDisplayedPlane(DP_RIGHT_UNLOAD);
						this->EnableWidget(ORDER_WIDGET_NON_STOP);
						this->SetWidgetLoweredState(ORDER_WIDGET_NON_STOP, order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
					}
					this->DisableWidget(ORDER_WIDGET_FULL_LOAD);
					this->DisableWidget(ORDER_WIDGET_UNLOAD);
					break;

				case OT_GOTO_DEPOT:
					if (row_sel != NULL) {
						row_sel->SetDisplayedPlane(DP_ROW_DEPOT);
					} else {
						left_sel->SetDisplayedPlane(DP_LEFT_NONSTOP);
						middle_sel->SetDisplayedPlane(DP_MIDDLE_REFIT);
						right_sel->SetDisplayedPlane(DP_RIGHT_SERVICE);
						this->EnableWidget(ORDER_WIDGET_NON_STOP);
						this->SetWidgetLoweredState(ORDER_WIDGET_NON_STOP, order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
					}
					this->SetWidgetLoweredState(ORDER_WIDGET_SERVICE, order->GetDepotOrderType() & ODTFB_SERVICE);
					break;

				case OT_CONDITIONAL: {
					if (row_sel != NULL) {
						row_sel->SetDisplayedPlane(DP_ROW_CONDITIONAL);
					} else {
						left_sel->SetDisplayedPlane(DP_LEFT_CONDVAR);
						middle_sel->SetDisplayedPlane(DP_MIDDLE_COMPARE);
						right_sel->SetDisplayedPlane(DP_RIGHT_CONDVAL);
					}
					OrderConditionVariable ocv = order->GetConditionVariable();
					/* Set the strings for the dropdown boxes. */
					this->GetWidget<NWidgetCore>(ORDER_WIDGET_COND_VARIABLE)->widget_data   = _order_conditional_variable[order == NULL ? 0 : ocv];
					this->GetWidget<NWidgetCore>(ORDER_WIDGET_COND_COMPARATOR)->widget_data = _order_conditional_condition[order == NULL ? 0 : order->GetConditionComparator()];
					this->SetWidgetDisabledState(ORDER_WIDGET_COND_COMPARATOR, ocv == OCV_UNCONDITIONALLY);
					this->SetWidgetDisabledState(ORDER_WIDGET_COND_VALUE, ocv == OCV_REQUIRES_SERVICE || ocv == OCV_UNCONDITIONALLY);
					break;
				}

				default: // every other order
					if (row_sel != NULL) {
						row_sel->SetDisplayedPlane(DP_ROW_LOAD);
					} else {
						left_sel->SetDisplayedPlane(DP_LEFT_NONSTOP);
						middle_sel->SetDisplayedPlane(DP_MIDDLE_LOAD);
						right_sel->SetDisplayedPlane(DP_RIGHT_UNLOAD);
						this->DisableWidget(ORDER_WIDGET_NON_STOP);
					}
					this->DisableWidget(ORDER_WIDGET_FULL_LOAD);
					this->DisableWidget(ORDER_WIDGET_UNLOAD);
					break;
			}
		}

		/* Disable list of vehicles with the same shared orders if there is no list */
		this->SetWidgetDisabledState(ORDER_WIDGET_SHARED_ORDER_LIST, !shared_orders);

		this->SetDirty();
	}

	virtual void OnPaint()
	{
		if (this->vehicle->owner != _local_company) this->selected_order = -1; // Disable selection any selected row at a competitor order window.
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != ORDER_WIDGET_ORDER_LIST) return;

		bool rtl = _current_text_dir == TD_RTL;
		SetDParam(0, 99);
		int index_column_width = GetStringBoundingBox(STR_ORDER_INDEX).width + GetSpriteSize(rtl ? SPR_ARROW_RIGHT : SPR_ARROW_LEFT).width + 3;
		int middle = rtl ? r.right - WD_FRAMETEXT_RIGHT - index_column_width : r.left + WD_FRAMETEXT_LEFT + index_column_width;

		int y = r.top + WD_FRAMERECT_TOP;
		int line_height = this->GetWidget<NWidgetBase>(ORDER_WIDGET_ORDER_LIST)->resize_y;

		int i = this->vscroll->GetPosition();
		const Order *order = this->vehicle->GetOrder(i);
		/* First draw the highlighting underground if it exists. */
		if (this->order_over != INVALID_ORDER) {
			while (order != NULL) {
				/* Don't draw anything if it extends past the end of the window. */
				if (!this->vscroll->IsVisible(i)) break;

				if (i != this->selected_order && i == this->order_over) {
					/* Highlight dragged order destination. */
					int top = (this->order_over < this->selected_order ? y : y + line_height) - WD_FRAMERECT_TOP;
					int bottom = min(top + 2, r.bottom - WD_FRAMERECT_BOTTOM);
					top = max(top - 3, r.top + WD_FRAMERECT_TOP);
					GfxFillRect(r.left + WD_FRAMETEXT_LEFT, top, r.right - WD_FRAMETEXT_RIGHT, bottom, _colour_gradient[COLOUR_GREY][7]);
					break;
				}
				y += line_height;

				i++;
				order = order->next;
			}

			/* Reset counters for drawing the orders. */
			y = r.top + WD_FRAMERECT_TOP;
			i = this->vscroll->GetPosition();
			order = this->vehicle->GetOrder(i);
		}

		/* Draw the orders. */
		while (order != NULL) {
			/* Don't draw anything if it extends past the end of the window. */
			if (!this->vscroll->IsVisible(i)) break;

			DrawOrderString(this->vehicle, order, i, y, i == this->selected_order, false, r.left + WD_FRAMETEXT_LEFT, middle, r.right - WD_FRAMETEXT_RIGHT);
			y += line_height;

			i++;
			order = order->next;
		}

		if (this->vscroll->IsVisible(i)) {
			StringID str = this->vehicle->IsOrderListShared() ? STR_ORDERS_END_OF_SHARED_ORDERS : STR_ORDERS_END_OF_ORDERS;
			DrawString(rtl ? r.left + WD_FRAMETEXT_LEFT : middle, rtl ? middle : r.right - WD_FRAMETEXT_RIGHT, y, str, (i == this->selected_order) ? TC_WHITE : TC_BLACK);
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case ORDER_WIDGET_COND_VALUE: {
				int sel = this->OrderGetSel();
				const Order *order = this->vehicle->GetOrder(sel);

				if (order != NULL && order->IsType(OT_CONDITIONAL)) {
					uint value = order->GetConditionValue();
					if (order->GetConditionVariable() == OCV_MAX_SPEED) value = ConvertSpeedToDisplaySpeed(value);
					SetDParam(0, value);
				}
				break;
			}

			case ORDER_WIDGET_CAPTION:
				SetDParam(0, this->vehicle->index);
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case ORDER_WIDGET_ORDER_LIST: {
				if (this->goto_type == OPOS_CONDITIONAL) {
					this->goto_type = OPOS_GOTO;
					int order_id = this->GetOrderFromPt(_cursor.pos.y - this->top);
					if (order_id != INVALID_ORDER) {
						Order order;
						order.next = NULL;
						order.index = 0;
						order.MakeConditional(order_id);

						DoCommandP(this->vehicle->tile, this->vehicle->index + (this->OrderGetSel() << 20), order.Pack(), CMD_INSERT_ORDER | CMD_MSG(STR_ERROR_CAN_T_INSERT_NEW_ORDER));
					}
					ResetObjectToPlace();
					break;
				}

				int sel = this->GetOrderFromPt(pt.y);

				if (_ctrl_pressed && sel < this->vehicle->GetNumOrders()) {
					TileIndex xy = this->vehicle->GetOrder(sel)->GetLocation(this->vehicle);
					if (xy != INVALID_TILE) ScrollMainWindowToTile(xy);
					return;
				}

				/* This order won't be selected any more, close all child windows and dropdowns */
				this->DeleteChildWindows();
				HideDropDownMenu(this);

				if (sel == INVALID_ORDER || this->vehicle->owner != _local_company) {
					/* Deselect clicked order */
					this->selected_order = -1;
				} else if (sel == this->selected_order) {
					if (this->vehicle->type == VEH_TRAIN && sel < this->vehicle->GetNumOrders()) {
						DoCommandP(this->vehicle->tile, this->vehicle->index + (sel << 20),
								MOF_STOP_LOCATION | ((this->vehicle->GetOrder(sel)->GetStopLocation() + 1) % OSL_END) << 4,
								CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
					}
				} else {
					/* Select clicked order */
					this->selected_order = sel;

					if (this->vehicle->owner == _local_company) {
						/* Activate drag and drop */
						SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_DRAG, this);
					}
				}

				this->UpdateButtonState();
				break;
			}

			case ORDER_WIDGET_SKIP:
				this->OrderClick_Skip(0);
				break;

			case ORDER_WIDGET_DELETE:
				this->OrderClick_Delete(0);
				break;

			case ORDER_WIDGET_STOP_SHARING:
				this->OrderClick_StopSharing(0);
				break;

			case ORDER_WIDGET_NON_STOP:
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->OrderClick_Nonstop(-1);
				} else {
					const Order *o = this->vehicle->GetOrder(this->OrderGetSel());
					ShowDropDownMenu(this, _order_non_stop_drowdown, o->GetNonStopType(), ORDER_WIDGET_NON_STOP, 0,
													o->IsType(OT_GOTO_STATION) ? 0 : (o->IsType(OT_GOTO_WAYPOINT) ? 3 : 12));
				}
				break;

			case ORDER_WIDGET_GOTO:
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->OrderClick_Goto(0);
				} else {
					ShowDropDownMenu(this, this->vehicle->type == VEH_AIRCRAFT ? _order_goto_dropdown_aircraft : _order_goto_dropdown, 0, ORDER_WIDGET_GOTO, 0, 0);
				}
				break;

			case ORDER_WIDGET_FULL_LOAD:
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->OrderClick_FullLoad(-1);
				} else {
					ShowDropDownMenu(this, _order_full_load_drowdown, this->vehicle->GetOrder(this->OrderGetSel())->GetLoadType(), ORDER_WIDGET_FULL_LOAD, 0, 2);
				}
				break;

			case ORDER_WIDGET_UNLOAD:
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->OrderClick_Unload(-1);
				} else {
					ShowDropDownMenu(this, _order_unload_drowdown, this->vehicle->GetOrder(this->OrderGetSel())->GetUnloadType(), ORDER_WIDGET_UNLOAD, 0, 8);
				}
				break;

			case ORDER_WIDGET_REFIT:
				this->OrderClick_Refit(0);
				break;

			case ORDER_WIDGET_SERVICE:
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->OrderClick_Service(-1);
				} else {
					ShowDropDownMenu(this, _order_depot_action_dropdown, DepotActionStringIndex(this->vehicle->GetOrder(this->OrderGetSel())), ORDER_WIDGET_SERVICE, 0, 0);
				}
				break;

			case ORDER_WIDGET_TIMETABLE_VIEW:
				ShowTimetableWindow(this->vehicle);
				break;

			case ORDER_WIDGET_COND_VARIABLE:
				ShowDropDownMenu(this, _order_conditional_variable, this->vehicle->GetOrder(this->OrderGetSel())->GetConditionVariable(), ORDER_WIDGET_COND_VARIABLE, 0, 0);
				break;

			case ORDER_WIDGET_COND_COMPARATOR: {
				const Order *o = this->vehicle->GetOrder(this->OrderGetSel());
				ShowDropDownMenu(this, _order_conditional_condition, o->GetConditionComparator(), ORDER_WIDGET_COND_COMPARATOR, 0, (o->GetConditionVariable() == OCV_REQUIRES_SERVICE) ? 0x3F : 0xC0);
				break;
			}

			case ORDER_WIDGET_COND_VALUE: {
				const Order *order = this->vehicle->GetOrder(this->OrderGetSel());
				uint value = order->GetConditionValue();
				if (order->GetConditionVariable() == OCV_MAX_SPEED) value = ConvertSpeedToDisplaySpeed(value);
				SetDParam(0, value);
				ShowQueryString(STR_JUST_INT, STR_ORDER_CONDITIONAL_VALUE_CAPT, 5, 100, this, CS_NUMERAL, QSF_NONE);
				break;
			}

			case ORDER_WIDGET_SHARED_ORDER_LIST:
				ShowVehicleListWindow(this->vehicle);
				break;
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (!StrEmpty(str)) {
			VehicleOrderID sel = this->OrderGetSel();
			uint value = atoi(str);

			switch (this->vehicle->GetOrder(sel)->GetConditionVariable()) {
				case OCV_MAX_SPEED:
					value = ConvertDisplaySpeedToSpeed(value);
					break;

				case OCV_RELIABILITY:
				case OCV_LOAD_PERCENTAGE:
					value = Clamp(value, 0, 100);

				default:
					break;
			}
			DoCommandP(this->vehicle->tile, this->vehicle->index + (sel << 20), MOF_COND_VALUE | Clamp(value, 0, 2047) << 4, CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case ORDER_WIDGET_NON_STOP:
				this->OrderClick_Nonstop(index);
				break;

			case ORDER_WIDGET_FULL_LOAD:
				this->OrderClick_FullLoad(index);
				break;

			case ORDER_WIDGET_UNLOAD:
				this->OrderClick_Unload(index);
				break;

			case ORDER_WIDGET_GOTO:
				switch (index) {
					case 0: this->OrderClick_Goto(0); break;
					case 1: this->OrderClick_NearestDepot(0); break;
					case 2: this->OrderClick_Conditional(0); break;
					case 3: this->OrderClick_Share(0); break;
					default: NOT_REACHED();
				}
				break;

			case ORDER_WIDGET_SERVICE:
				this->OrderClick_Service(index);
				break;

			case ORDER_WIDGET_COND_VARIABLE:
				DoCommandP(this->vehicle->tile, this->vehicle->index + (this->OrderGetSel() << 20), MOF_COND_VARIABLE | index << 4,  CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
				break;

			case ORDER_WIDGET_COND_COMPARATOR:
				DoCommandP(this->vehicle->tile, this->vehicle->index + (this->OrderGetSel() << 20), MOF_COND_COMPARATOR | index << 4,  CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
				break;
		}
	}

	virtual void OnDragDrop(Point pt, int widget)
	{
		switch (widget) {
			case ORDER_WIDGET_ORDER_LIST: {
				int from_order = this->OrderGetSel();
				int to_order = this->GetOrderFromPt(pt.y);

				if (!(from_order == to_order || from_order == INVALID_ORDER || from_order > this->vehicle->GetNumOrders() || to_order == INVALID_ORDER || to_order > this->vehicle->GetNumOrders()) &&
						DoCommandP(this->vehicle->tile, this->vehicle->index, from_order | (to_order << 16), CMD_MOVE_ORDER | CMD_MSG(STR_ERROR_CAN_T_MOVE_THIS_ORDER))) {
					this->selected_order = -1;
					this->UpdateButtonState();
				}
				break;
			}

			case ORDER_WIDGET_DELETE:
				this->OrderClick_Delete(0);
				break;

			case ORDER_WIDGET_STOP_SHARING:
				this->OrderClick_StopSharing(0);
				break;
		}

		ResetObjectToPlace();

		if (this->order_over != INVALID_ORDER) {
			/* End of drag-and-drop, hide dragged order destination highlight. */
			this->order_over = INVALID_ORDER;
			this->SetWidgetDirty(ORDER_WIDGET_ORDER_LIST);
		}
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{

		if (this->vehicle->owner != _local_company) return ES_NOT_HANDLED;

		return CheckHotkeyMatch<OrdersWindow>(order_hotkeys, keycode, this) != -1 ? ES_HANDLED : ES_NOT_HANDLED;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		if (this->goto_type == OPOS_GOTO) {
			const Order cmd = GetOrderCmdFromTile(this->vehicle, tile);
			if (cmd.IsType(OT_NOTHING)) return;

			if (DoCommandP(this->vehicle->tile, this->vehicle->index + (this->OrderGetSel() << 20), cmd.Pack(), CMD_INSERT_ORDER | CMD_MSG(STR_ERROR_CAN_T_INSERT_NEW_ORDER))) {
				/* With quick goto the Go To button stays active */
				if (!_settings_client.gui.quick_goto) ResetObjectToPlace();
			}
		}
	}

	virtual void OnVehicleSelect(const Vehicle *v)
	{
		/* v is vehicle getting orders. Only copy/clone orders if vehicle doesn't have any orders yet.
		 * We disallow copying orders of other vehicles if we already have at least one order entry
		 * ourself as it easily copies orders of vehicles within a station when we mean the station.
		 * Obviously if you press CTRL on a non-empty orders vehicle you know what you are doing
		 * TODO: give a warning message */
		bool share_order = _ctrl_pressed || this->goto_type == OPOS_SHARE;
		if (this->vehicle->GetNumOrders() != 0 && !share_order) return;

		if (DoCommandP(this->vehicle->tile, this->vehicle->index | (share_order ? CO_SHARE : CO_COPY) << 30, v->index,
				share_order ? CMD_CLONE_ORDER | CMD_MSG(STR_ERROR_CAN_T_SHARE_ORDER_LIST) : CMD_CLONE_ORDER | CMD_MSG(STR_ERROR_CAN_T_COPY_ORDER_LIST))) {
			this->selected_order = -1;
			ResetObjectToPlace();
		}
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseWidget(ORDER_WIDGET_GOTO);
		this->SetWidgetDirty(ORDER_WIDGET_GOTO);

		/* Remove drag highlighting if it exists. */
		if (this->order_over != INVALID_ORDER) {
			this->order_over = INVALID_ORDER;
			this->SetWidgetDirty(ORDER_WIDGET_ORDER_LIST);
		}
	}

	virtual void OnMouseDrag(Point pt, int widget)
	{
		if (this->selected_order != -1 && widget == ORDER_WIDGET_ORDER_LIST) {
			/* An order is dragged.. */
			OrderID from_order = this->OrderGetSel();
			OrderID to_order = this->GetOrderFromPt(pt.y);
			uint num_orders = this->vehicle->GetNumOrders();

			if (from_order != INVALID_ORDER && from_order <= num_orders) {
				if (to_order != INVALID_ORDER && to_order <= num_orders) { // ..over an existing order.
					this->order_over = to_order;
					this->SetWidgetDirty(widget);
				} else if (from_order != to_order && this->order_over != INVALID_ORDER) { // ..outside of the order list.
					this->order_over = INVALID_ORDER;
					this->SetWidgetDirty(widget);
				}
			}
		}
	}

	virtual void OnResize()
	{
		/* Update the scroll bar */
		this->vscroll->SetCapacityFromWidget(this, ORDER_WIDGET_ORDER_LIST);
	}

	virtual void OnTimeout()
	{
		static const int raise_widgets[] = {
			ORDER_WIDGET_TIMETABLE_VIEW, ORDER_WIDGET_SKIP, ORDER_WIDGET_DELETE, ORDER_WIDGET_STOP_SHARING, ORDER_WIDGET_REFIT, ORDER_WIDGET_SHARED_ORDER_LIST, WIDGET_LIST_END,
		};

		/* Unclick all buttons in raise_widgets[]. */
		for (const int *widnum = raise_widgets; *widnum != WIDGET_LIST_END; widnum++) {
			if (this->IsWidgetLowered(*widnum)) {
				this->RaiseWidget(*widnum);
				this->SetWidgetDirty(*widnum);
			}
		}
	}

	static Hotkey<OrdersWindow> order_hotkeys[];
};

Hotkey<OrdersWindow> OrdersWindow::order_hotkeys[]= {
	Hotkey<OrdersWindow>('D', "skip", 0, &OrdersWindow::OrderClick_Skip),
	Hotkey<OrdersWindow>('F', "delete", 0, &OrdersWindow::OrderClick_Delete),
	Hotkey<OrdersWindow>('G', "goto", 0, &OrdersWindow::OrderClick_Goto),
	Hotkey<OrdersWindow>('H', "nonstop", 0, &OrdersWindow::OrderClick_Nonstop),
	Hotkey<OrdersWindow>('J', "fullload", 0, &OrdersWindow::OrderClick_FullLoad),
	Hotkey<OrdersWindow>('K', "unload", 0, &OrdersWindow::OrderClick_Unload),
	Hotkey<OrdersWindow>((uint16)0, "nearest_depot", 0, &OrdersWindow::OrderClick_NearestDepot),
	Hotkey<OrdersWindow>((uint16)0, "always_service", 0, &OrdersWindow::OrderClick_Service),
	Hotkey<OrdersWindow>((uint16)0, "force_unload", 0, &OrdersWindow::OrderClick_Unload),
	Hotkey<OrdersWindow>((uint16)0, "transfer", 0, &OrdersWindow::OrderHotkey_Transfer),
	Hotkey<OrdersWindow>((uint16)0, "no_unload", 0, &OrdersWindow::OrderHotkey_NoUnload),
	Hotkey<OrdersWindow>((uint16)0, "no_load", 0, &OrdersWindow::OrderHotkey_NoLoad),
	HOTKEY_LIST_END(OrdersWindow)
};
Hotkey<OrdersWindow> *_order_hotkeys = OrdersWindow::order_hotkeys;

/** Nested widget definition for "your" train orders. */
static const NWidgetPart _nested_orders_train_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, ORDER_WIDGET_CAPTION), SetDataTip(STR_ORDERS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_TIMETABLE_VIEW), SetMinimalSize(61, 14), SetDataTip(STR_ORDERS_TIMETABLE_VIEW, STR_ORDERS_TIMETABLE_VIEW_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, ORDER_WIDGET_ORDER_LIST), SetMinimalSize(372, 62), SetDataTip(0x0, STR_ORDERS_LIST_TOOLTIP), SetResize(1, 1), SetScrollbar(ORDER_WIDGET_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, ORDER_WIDGET_SCROLLBAR),
	EndContainer(),

	/* First button row. */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(NWID_SELECTION, INVALID_COLOUR, ORDER_WIDGET_SEL_TOP_LEFT),
				NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_NON_STOP), SetMinimalSize(124, 12), SetFill(1, 0),
														SetDataTip(STR_ORDER_NON_STOP, STR_ORDER_TOOLTIP_NON_STOP), SetResize(1, 0),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_COND_VARIABLE), SetMinimalSize(124, 12), SetFill(1, 0),
														SetDataTip(STR_NULL, STR_ORDER_CONDITIONAL_VARIABLE_TOOLTIP), SetResize(1, 0),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, ORDER_WIDGET_SEL_TOP_MIDDLE),
				NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_FULL_LOAD), SetMinimalSize(124, 12), SetFill(1, 0),
														SetDataTip(STR_ORDER_TOGGLE_FULL_LOAD, STR_ORDER_TOOLTIP_FULL_LOAD), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_REFIT), SetMinimalSize(124, 12), SetFill(1, 0),
														SetDataTip(STR_ORDER_REFIT, STR_ORDER_REFIT_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_COND_COMPARATOR), SetMinimalSize(124, 12), SetFill(1, 0),
														SetDataTip(STR_NULL, STR_ORDER_CONDITIONAL_COMPARATOR_TOOLTIP), SetResize(1, 0),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, ORDER_WIDGET_SEL_TOP_RIGHT),
				NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_UNLOAD), SetMinimalSize(124, 12), SetFill(1, 0),
														SetDataTip(STR_ORDER_TOGGLE_UNLOAD, STR_ORDER_TOOLTIP_UNLOAD), SetResize(1, 0),
				NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_SERVICE), SetMinimalSize(124, 12), SetFill(1, 0),
														SetDataTip(STR_ORDER_SERVICE, STR_ORDER_SERVICE_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_COND_VALUE), SetMinimalSize(124, 12), SetFill(1, 0),
														SetDataTip(STR_BLACK_COMMA, STR_ORDER_CONDITIONAL_VALUE_TOOLTIP), SetResize(1, 0),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, ORDER_WIDGET_SHARED_ORDER_LIST), SetMinimalSize(12, 12), SetDataTip(SPR_SHARED_ORDERS_ICON, STR_ORDERS_VEH_WITH_SHARED_ORDERS_LIST_TOOLTIP),
	EndContainer(),

	/* Second button row. */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_SKIP), SetMinimalSize(124, 12), SetFill(1, 0),
													SetDataTip(STR_ORDERS_SKIP_BUTTON, STR_ORDERS_SKIP_TOOLTIP), SetResize(1, 0),
			NWidget(NWID_SELECTION, INVALID_COLOUR, ORDER_WIDGET_SEL_BOTTOM_MIDDLE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_DELETE), SetMinimalSize(124, 12), SetFill(1, 0),
														SetDataTip(STR_ORDERS_DELETE_BUTTON, STR_ORDERS_DELETE_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_STOP_SHARING), SetMinimalSize(124, 12), SetFill(1, 0),
														SetDataTip(STR_ORDERS_STOP_SHARING_BUTTON, STR_ORDERS_STOP_SHARING_TOOLTIP), SetResize(1, 0),
			EndContainer(),
			NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_GOTO), SetMinimalSize(124, 12), SetFill(1, 0),
													SetDataTip(STR_ORDERS_GO_TO_BUTTON, STR_ORDERS_GO_TO_TOOLTIP), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static const WindowDesc _orders_train_desc(
	WDP_AUTO, 384, 100,
	WC_VEHICLE_ORDERS, WC_VEHICLE_VIEW,
	WDF_CONSTRUCTION,
	_nested_orders_train_widgets, lengthof(_nested_orders_train_widgets)
);

/** Nested widget definition for "your" orders (non-train). */
static const NWidgetPart _nested_orders_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, ORDER_WIDGET_CAPTION), SetDataTip(STR_ORDERS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_TIMETABLE_VIEW), SetMinimalSize(61, 14), SetDataTip(STR_ORDERS_TIMETABLE_VIEW, STR_ORDERS_TIMETABLE_VIEW_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, ORDER_WIDGET_ORDER_LIST), SetMinimalSize(372, 62), SetDataTip(0x0, STR_ORDERS_LIST_TOOLTIP), SetResize(1, 1), SetScrollbar(ORDER_WIDGET_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, ORDER_WIDGET_SCROLLBAR),
	EndContainer(),

	/* First button row. */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SELECTION, INVALID_COLOUR, ORDER_WIDGET_SEL_TOP_ROW),
			/* load + unload buttons. */
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_FULL_LOAD), SetMinimalSize(186, 12), SetFill(1, 0),
													SetDataTip(STR_ORDER_TOGGLE_FULL_LOAD, STR_ORDER_TOOLTIP_FULL_LOAD), SetResize(1, 0),
				NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_UNLOAD), SetMinimalSize(186, 12), SetFill(1, 0),
													SetDataTip(STR_ORDER_TOGGLE_UNLOAD, STR_ORDER_TOOLTIP_UNLOAD), SetResize(1, 0),
			EndContainer(),
			/* Refit + service buttons. */
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_REFIT), SetMinimalSize(186, 12), SetFill(1, 0),
													SetDataTip(STR_ORDER_REFIT, STR_ORDER_REFIT_TOOLTIP), SetResize(1, 0),
				NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_SERVICE), SetMinimalSize(124, 12), SetFill(1, 0),
													SetDataTip(STR_ORDER_SERVICE, STR_ORDER_SERVICE_TOOLTIP), SetResize(1, 0),
			EndContainer(),

			/* Buttons for setting a condition. */
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_COND_VARIABLE), SetMinimalSize(124, 12), SetFill(1, 0),
													SetDataTip(STR_NULL, STR_ORDER_CONDITIONAL_VARIABLE_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_COND_COMPARATOR), SetMinimalSize(124, 12), SetFill(1, 0),
													SetDataTip(STR_NULL, STR_ORDER_CONDITIONAL_COMPARATOR_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_COND_VALUE), SetMinimalSize(124, 12), SetFill(1, 0),
													SetDataTip(STR_BLACK_COMMA, STR_ORDER_CONDITIONAL_VALUE_TOOLTIP), SetResize(1, 0),
			EndContainer(),
		EndContainer(),

		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, ORDER_WIDGET_SHARED_ORDER_LIST), SetMinimalSize(12, 12), SetDataTip(SPR_SHARED_ORDERS_ICON, STR_ORDERS_VEH_WITH_SHARED_ORDERS_LIST_TOOLTIP),
	EndContainer(),

	/* Second button row. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_SKIP), SetMinimalSize(124, 12), SetFill(1, 0),
											SetDataTip(STR_ORDERS_SKIP_BUTTON, STR_ORDERS_SKIP_TOOLTIP), SetResize(1, 0),
		NWidget(NWID_SELECTION, INVALID_COLOUR, ORDER_WIDGET_SEL_BOTTOM_MIDDLE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_DELETE), SetMinimalSize(124, 12), SetFill(1, 0),
													SetDataTip(STR_ORDERS_DELETE_BUTTON, STR_ORDERS_DELETE_TOOLTIP), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_STOP_SHARING), SetMinimalSize(124, 12), SetFill(1, 0),
													SetDataTip(STR_ORDERS_STOP_SHARING_BUTTON, STR_ORDERS_STOP_SHARING_TOOLTIP), SetResize(1, 0),
		EndContainer(),
		NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_GOTO), SetMinimalSize(124, 12), SetFill(1, 0),
											SetDataTip(STR_ORDERS_GO_TO_BUTTON, STR_ORDERS_GO_TO_TOOLTIP), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static const WindowDesc _orders_desc(
	WDP_AUTO, 384, 100,
	WC_VEHICLE_ORDERS, WC_VEHICLE_VIEW,
	WDF_CONSTRUCTION,
	_nested_orders_widgets, lengthof(_nested_orders_widgets)
);

/** Nested widget definition for competitor orders. */
static const NWidgetPart _nested_other_orders_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, ORDER_WIDGET_CAPTION), SetDataTip(STR_ORDERS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_TIMETABLE_VIEW), SetMinimalSize(61, 14), SetDataTip(STR_ORDERS_TIMETABLE_VIEW, STR_ORDERS_TIMETABLE_VIEW_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, ORDER_WIDGET_ORDER_LIST), SetMinimalSize(372, 72), SetDataTip(0x0, STR_ORDERS_LIST_TOOLTIP), SetResize(1, 1), SetScrollbar(ORDER_WIDGET_SCROLLBAR), EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_GREY, ORDER_WIDGET_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _other_orders_desc(
	WDP_AUTO, 384, 86,
	WC_VEHICLE_ORDERS, WC_VEHICLE_VIEW,
	WDF_UNCLICK_BUTTONS | WDF_CONSTRUCTION,
	_nested_other_orders_widgets, lengthof(_nested_other_orders_widgets)
);

void ShowOrdersWindow(const Vehicle *v)
{
	DeleteWindowById(WC_VEHICLE_DETAILS, v->index, false);
	DeleteWindowById(WC_VEHICLE_TIMETABLE, v->index, false);
	if (BringWindowToFrontById(WC_VEHICLE_ORDERS, v->index) != NULL) return;

	if (v->owner != _local_company) {
		new OrdersWindow(&_other_orders_desc, v);
	} else {
		new OrdersWindow(v->IsGroundVehicle() ? &_orders_train_desc : &_orders_desc, v);
	}
}
