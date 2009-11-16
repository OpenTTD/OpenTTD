/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file order_gui.cpp GUI related to orders. */

#include "stdafx.h"
#include "window_gui.h"
#include "command_func.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "depot_base.h"
#include "vehicle_base.h"
#include "vehicle_gui.h"
#include "timetable.h"
#include "cargotype.h"
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

#include "table/sprites.h"
#include "table/strings.h"

/** Widget numbers of the order window. */
enum OrderWindowWidgets {
	ORDER_WIDGET_CLOSEBOX = 0,
	ORDER_WIDGET_CAPTION,
	ORDER_WIDGET_TIMETABLE_VIEW,
	ORDER_WIDGET_STICKY,
	ORDER_WIDGET_ORDER_LIST,
	ORDER_WIDGET_SCROLLBAR,
	ORDER_WIDGET_SKIP,
	ORDER_WIDGET_DELETE,
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
	ORDER_WIDGET_SHARED_ORDER_LIST,
	ORDER_WIDGET_RESIZE,
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
	INVALID_STRING_ID
};

static const StringID _order_goto_dropdown_aircraft[] = {
	STR_ORDER_GO_TO,
	STR_ORDER_GO_TO_NEAREST_HANGAR,
	STR_ORDER_CONDITIONAL,
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

void DrawOrderString(const Vehicle *v, const Order *order, int order_index, int y, bool selected, bool timetable, int left, int right)
{
	StringID str = (v->cur_order_index == order_index) ? STR_ORDER_SELECTED : STR_ORDER;
	SetDParam(6, STR_EMPTY);

	switch (order->GetType()) {
		case OT_DUMMY:
			SetDParam(1, STR_INVALID_ORDER);
			SetDParam(2, order->GetDestination());
			break;

		case OT_GOTO_STATION: {
			OrderLoadFlags load = order->GetLoadType();
			OrderUnloadFlags unload = order->GetUnloadType();

			SetDParam(1, STR_ORDER_GO_TO_STATION);
			SetDParam(2, STR_ORDER_GO_TO + ((v->type == VEH_TRAIN || v->type == VEH_ROAD) ? order->GetNonStopType() : 0));
			SetDParam(3, order->GetDestination());

			if (timetable) {
				SetDParam(4, STR_EMPTY);

				if (order->wait_time > 0) {
					SetDParam(6, STR_TIMETABLE_STAY_FOR);
					SetTimetableParams(7, 8, order->wait_time);
				}
			} else {
				SetDParam(4, (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) ? STR_EMPTY : _station_load_types[unload][load]);
				if (v->type == VEH_TRAIN && (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) == 0) {
					SetDParam(6, order->GetStopLocation() + STR_ORDER_STOP_LOCATION_NEAR_END);
				}
			}
		} break;

		case OT_GOTO_DEPOT:
			if (v->type == VEH_AIRCRAFT) {
				if (order->GetDepotActionType() & ODATFB_NEAREST_DEPOT) {
					SetDParam(1, STR_ORDER_GO_TO_NEAREST_DEPOT_FORMAT);
					SetDParam(3, STR_ORDER_NEAREST_HANGAR);
				} else {
					SetDParam(1, STR_ORDER_GO_TO_HANGAR_FORMAT);
					SetDParam(3, order->GetDestination());
				}
				SetDParam(4, STR_EMPTY);
			} else {
				if (order->GetDepotActionType() & ODATFB_NEAREST_DEPOT) {
					SetDParam(1, STR_ORDER_GO_TO_NEAREST_DEPOT_FORMAT);
					SetDParam(3, STR_ORDER_NEAREST_DEPOT);
				} else {
					SetDParam(1, STR_ORDER_GO_TO_DEPOT_FORMAT);
					SetDParam(3, Depot::Get(order->GetDestination())->town_index);
				}

				SetDParam(4, STR_ORDER_TRAIN_DEPOT + v->type);
			}

			if (order->GetDepotOrderType() & ODTFB_SERVICE) {
				SetDParam(2, (order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS) ? STR_ORDER_SERVICE_NON_STOP_AT : STR_ORDER_SERVICE_AT);
			} else {
				SetDParam(2, (order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS) ? STR_ORDER_GO_NON_STOP_TO : STR_ORDER_GO_TO);
			}

			if (!timetable && (order->GetDepotActionType() & ODATFB_HALT)) {
				SetDParam(6, STR_ORDER_STOP_ORDER);
			}

			if (!timetable && order->IsRefit()) {
				SetDParam(6, (order->GetDepotActionType() & ODATFB_HALT) ? STR_ORDER_REFIT_STOP_ORDER : STR_ORDER_REFIT_ORDER);
				SetDParam(7, CargoSpec::Get(order->GetRefitCargo())->name);
			}
			break;

		case OT_GOTO_WAYPOINT:
			SetDParam(1, (order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS) ? STR_ORDER_GO_NON_STOP_TO_WAYPOINT : STR_ORDER_GO_TO_WAYPOINT);
			SetDParam(2, order->GetDestination());
			break;

		case OT_CONDITIONAL:
			SetDParam(2, order->GetConditionSkipToOrder() + 1);
			if (order->GetConditionVariable() == OCV_UNCONDITIONALLY) {
				SetDParam(1, STR_ORDER_CONDITIONAL_UNCONDITIONAL);
			} else {
				OrderConditionComparator occ = order->GetConditionComparator();
				SetDParam(1, (occ == OCC_IS_TRUE || occ == OCC_IS_FALSE) ? STR_ORDER_CONDITIONAL_TRUE_FALSE : STR_ORDER_CONDITIONAL_NUM);
				SetDParam(3, STR_ORDER_CONDITIONAL_LOAD_PERCENTAGE + order->GetConditionVariable());
				SetDParam(4, STR_ORDER_CONDITIONAL_COMPARATOR_EQUALS + occ);

				uint value = order->GetConditionValue();
				if (order->GetConditionVariable() == OCV_MAX_SPEED) value = ConvertSpeedToDisplaySpeed(value);
				SetDParam(5, value);
			}

			if (timetable && order->wait_time > 0) {
				SetDParam(6, STR_TIMETABLE_AND_TRAVEL_FOR);
				SetTimetableParams(7, 8, order->wait_time);
			} else {
				SetDParam(6, STR_EMPTY);
			}
			break;

		default: NOT_REACHED();
	}

	SetDParam(0, order_index + 1);
	DrawString(left, right, y, str, selected ? TC_WHITE : TC_BLACK);
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
			(facil = FACIL_BUS_STOP, v->type == VEH_ROAD && IsCargoInClass(v->cargo_type, CC_PASSENGERS)) ||
			(facil = FACIL_TRUCK_STOP, 1);
			if (st->facilities & facil) {
				order.MakeGoToStation(st_index);
				if (_ctrl_pressed) order.SetLoadType(OLF_FULL_LOAD_ANY);
				if (_settings_client.gui.new_nonstop && (v->type == VEH_TRAIN || v->type == VEH_ROAD)) order.SetNonStopType(ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
				order.SetStopLocation(v->type == VEH_TRAIN ? (OrderStopLocation)(_settings_client.gui.stop_location) : OSL_PLATFORM_FAR_END);
				return order;
			}
		}
	}

	/* not found */
	order.Free();
	return order;
}

/** %Order window code for all vehicles.
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
	};

	int selected_order;
	OrderPlaceObjectState goto_type;
	const Vehicle *vehicle; ///< Vehicle owning the orders being displayed and manipulated.

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
		int sel = (y - this->GetWidget<NWidgetBase>(ORDER_WIDGET_ORDER_LIST)->pos_y - WD_FRAMERECT_TOP) / this->resize.step_height; // Selected line in the ORDER_WIDGET_ORDER_LIST panel.

		if ((uint)sel >= this->vscroll.GetCapacity()) return INVALID_ORDER;

		sel += this->vscroll.GetPosition();

		return (sel <= vehicle->GetNumOrders() && sel >= 0) ? sel : INVALID_ORDER;
	}

	bool HandleOrderVehClick(const Vehicle *u)
	{
		if (u->type != this->vehicle->type) return false;

		if (!u->IsPrimaryVehicle()) {
			u = u->First();
			if (!u->IsPrimaryVehicle()) return false;
		}

		/* v is vehicle getting orders. Only copy/clone orders if vehicle doesn't have any orders yet
		 * obviously if you press CTRL on a non-empty orders vehicle you know what you are doing */
		if (this->vehicle->GetNumOrders() != 0 && _ctrl_pressed == 0) return false;

		if (DoCommandP(this->vehicle->tile, this->vehicle->index | (u->index << 16), _ctrl_pressed ? CO_SHARE : CO_COPY,
			_ctrl_pressed ? CMD_CLONE_ORDER | CMD_MSG(STR_ERROR_CAN_T_SHARE_ORDER_LIST) : CMD_CLONE_ORDER | CMD_MSG(STR_ERROR_CAN_T_COPY_ORDER_LIST))) {
			this->selected_order = -1;
			ResetObjectToPlace();
		}

		return true;
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
			_place_clicked_vehicle = NULL;
			SetObjectToPlaceWnd(ANIMCURSOR_PICKSTATION, PAL_NONE, HT_RECT, this);
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
		DoCommandP(this->vehicle->tile, this->vehicle->index + (sel_ord << 16), MOF_LOAD | (load_type << 4), CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
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
		DoCommandP(this->vehicle->tile, this->vehicle->index + (sel_ord << 16), MOF_DEPOT_ACTION | (i << 4), CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
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
				_settings_client.gui.new_nonstop && (this->vehicle->type == VEH_TRAIN || this->vehicle->type == VEH_ROAD) ? ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS : ONSF_STOP_EVERYWHERE);
		order.SetDepotActionType(ODATFB_NEAREST_DEPOT);

		DoCommandP(this->vehicle->tile, this->vehicle->index + (this->OrderGetSel() << 16), order.Pack(), CMD_INSERT_ORDER | CMD_MSG(STR_ERROR_CAN_T_INSERT_NEW_ORDER));
	}

	/**
	 * Handle the click on the conditional order button.
	 * @param i Dummy parameter.
	 */
	void OrderClick_Conditional(int i)
	{
		this->SetWidgetDirty(ORDER_WIDGET_GOTO);
		this->LowerWidget(ORDER_WIDGET_GOTO);
		SetObjectToPlaceWnd(ANIMCURSOR_PICKSTATION, PAL_NONE, HT_RECT, this);
		this->goto_type = OPOS_CONDITIONAL;
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

		DoCommandP(this->vehicle->tile, this->vehicle->index + (sel_ord << 16), MOF_UNLOAD | (unload_type << 4), CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
	}

	/**
	 * Handle the click on the nonstop button.
	 * @param non_stop what non-stop type to use; -1 to use the 'next' one.
	 */
	void OrderClick_Nonstop(int non_stop)
	{
		VehicleOrderID sel_ord = this->OrderGetSel();
		const Order *order = this->vehicle->GetOrder(sel_ord);

		if (order == NULL || order->GetNonStopType() == non_stop) return;

		/* Keypress if negative, so 'toggle' to the next */
		if (non_stop < 0) {
			non_stop = order->GetNonStopType() ^ ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS;
		}

		this->SetWidgetDirty(ORDER_WIDGET_NON_STOP);
		DoCommandP(this->vehicle->tile, this->vehicle->index + (sel_ord << 16), MOF_NON_STOP | non_stop << 4,  CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
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
	typedef void (OrdersWindow::*Handler)(int);
	struct KeyToEvent {
		uint16 keycode;
		Handler proc;
	};

public:
	OrdersWindow(const WindowDesc *desc, const Vehicle *v) : Window()
	{
		this->vehicle = v;

		this->InitNested(desc, v->index);

		this->selected_order = -1;
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

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
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

			default: {
				/* Moving an order. If one of these is INVALID_VEH_ORDER_ID, then
				 * the order is being created / removed */
				if (this->selected_order == -1) break;

				VehicleOrderID from = GB(data, 0, 8);
				VehicleOrderID to   = GB(data, 8, 8);

				if (from == to) break; // no need to change anything

				if (from != this->selected_order) {
					/* Moving from preceeding order? */
					this->selected_order -= (int)(from <= this->selected_order);
					/* Moving to   preceeding order? */
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
			} break;
		}

		this->vscroll.SetCount(this->vehicle->GetNumOrders() + 1);
		this->UpdateButtonState();
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

		/* delete */
		this->SetWidgetDisabledState(ORDER_WIDGET_DELETE,
				(uint)this->vehicle->GetNumOrders() + ((shared_orders || this->vehicle->GetNumOrders() != 0) ? 1 : 0) <= (uint)this->selected_order);

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
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != ORDER_WIDGET_ORDER_LIST) return;

		int y = r.top + WD_FRAMERECT_TOP;

		int i = this->vscroll.GetPosition();
		const Order *order = this->vehicle->GetOrder(i);
		StringID str;
		while (order != NULL) {
			/* Don't draw anything if it extends past the end of the window. */
			if (!this->vscroll.IsVisible(i)) break;

			DrawOrderString(this->vehicle, order, i, y, i == this->selected_order, false, r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT);
			y += this->resize.step_height;

			i++;
			order = order->next;
		}

		if (this->vscroll.IsVisible(i)) {
			str = this->vehicle->IsOrderListShared() ? STR_ORDERS_END_OF_SHARED_ORDERS : STR_ORDERS_END_OF_ORDERS;
			DrawString(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, y, str, (i == this->selected_order) ? TC_WHITE : TC_BLACK);
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

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case ORDER_WIDGET_ORDER_LIST: {
				ResetObjectToPlace();

				int sel = this->GetOrderFromPt(pt.y);

				if (_ctrl_pressed && sel < this->vehicle->GetNumOrders()) {
					const Order *ord = this->vehicle->GetOrder(sel);
					TileIndex xy = INVALID_TILE;

					switch (ord->GetType()) {
						case OT_GOTO_WAYPOINT:
							if (this->vehicle->type == VEH_TRAIN) {
								xy = Waypoint::Get(ord->GetDestination())->xy;
								break;
							}
							/* FALL THROUGH */
						case OT_GOTO_STATION:
							xy = Station::Get(ord->GetDestination())->xy;
							break;

						case OT_GOTO_DEPOT:
							if ((ord->GetDepotActionType() & ODATFB_NEAREST_DEPOT) != 0) break;
							xy = (this->vehicle->type == VEH_AIRCRAFT) ?  Station::Get(ord->GetDestination())->xy : Depot::Get(ord->GetDestination())->xy;
							break;
						default:
							break;
					}

					if (xy != INVALID_TILE) ScrollMainWindowToTile(xy);
					return;
				}

				/* This order won't be selected any more, close all child windows and dropdowns */
				this->DeleteChildWindows();
				HideDropDownMenu(this);

				if (sel == INVALID_ORDER) {
					/* Deselect clicked order */
					this->selected_order = -1;
				} else if (sel == this->selected_order) {
					if (this->vehicle->type == VEH_TRAIN && sel < this->vehicle->GetNumOrders()) {
						DoCommandP(this->vehicle->tile, this->vehicle->index + (sel << 16),
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
			} break;

			case ORDER_WIDGET_SKIP:
				this->OrderClick_Skip(0);
				break;

			case ORDER_WIDGET_DELETE:
				this->OrderClick_Delete(0);
				break;

			case ORDER_WIDGET_NON_STOP:
				if (GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->OrderClick_Nonstop(-1);
				} else {
					const Order *o = this->vehicle->GetOrder(this->OrderGetSel());
					ShowDropDownMenu(this, _order_non_stop_drowdown, o->GetNonStopType(), ORDER_WIDGET_NON_STOP, 0,
													o->IsType(OT_GOTO_STATION) ? 0 : (o->IsType(OT_GOTO_WAYPOINT) ? 3 : 12));
				}
				break;

			case ORDER_WIDGET_GOTO:
				if (GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->OrderClick_Goto(0);
				} else {
					ShowDropDownMenu(this, this->vehicle->type == VEH_AIRCRAFT ? _order_goto_dropdown_aircraft : _order_goto_dropdown, 0, ORDER_WIDGET_GOTO, 0, 0);
				}
				break;

			case ORDER_WIDGET_FULL_LOAD:
				if (GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->OrderClick_FullLoad(-1);
				} else {
					ShowDropDownMenu(this, _order_full_load_drowdown, this->vehicle->GetOrder(this->OrderGetSel())->GetLoadType(), ORDER_WIDGET_FULL_LOAD, 0, 2);
				}
				break;

			case ORDER_WIDGET_UNLOAD:
				if (GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->OrderClick_Unload(-1);
				} else {
					ShowDropDownMenu(this, _order_unload_drowdown, this->vehicle->GetOrder(this->OrderGetSel())->GetUnloadType(), ORDER_WIDGET_UNLOAD, 0, 8);
				}
				break;

			case ORDER_WIDGET_REFIT:
				this->OrderClick_Refit(0);
				break;

			case ORDER_WIDGET_SERVICE:
				if (GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
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
			} break;

			case ORDER_WIDGET_COND_VALUE: {
				const Order *order = this->vehicle->GetOrder(this->OrderGetSel());
				uint value = order->GetConditionValue();
				if (order->GetConditionVariable() == OCV_MAX_SPEED) value = ConvertSpeedToDisplaySpeed(value);
				SetDParam(0, value);
				ShowQueryString(STR_JUST_INT, STR_ORDER_CONDITIONAL_VALUE_CAPT, 5, 100, this, CS_NUMERAL, QSF_NONE);
			} break;

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
			DoCommandP(this->vehicle->tile, this->vehicle->index + (sel << 16), MOF_COND_VALUE | Clamp(value, 0, 2047) << 4, CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
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
					default: NOT_REACHED();
				}
				break;

			case ORDER_WIDGET_SERVICE:
				this->OrderClick_Service(index);
				break;

			case ORDER_WIDGET_COND_VARIABLE:
				DoCommandP(this->vehicle->tile, this->vehicle->index + (this->OrderGetSel() << 16), MOF_COND_VARIABLE | index << 4,  CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
				break;

			case ORDER_WIDGET_COND_COMPARATOR:
				DoCommandP(this->vehicle->tile, this->vehicle->index + (this->OrderGetSel() << 16), MOF_COND_COMPARATOR | index << 4,  CMD_MODIFY_ORDER | CMD_MSG(STR_ERROR_CAN_T_MODIFY_THIS_ORDER));
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
			} break;

			case ORDER_WIDGET_DELETE:
				this->OrderClick_Delete(0);
				break;
		}

		ResetObjectToPlace();
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		static const KeyToEvent keytoevent[] = {
			{'D', &OrdersWindow::OrderClick_Skip},
			{'F', &OrdersWindow::OrderClick_Delete},
			{'G', &OrdersWindow::OrderClick_Goto},
			{'H', &OrdersWindow::OrderClick_Nonstop},
			{'J', &OrdersWindow::OrderClick_FullLoad},
			{'K', &OrdersWindow::OrderClick_Unload},
			//('?', &OrdersWindow::OrderClick_Service},
		};

		if (this->vehicle->owner != _local_company) return ES_NOT_HANDLED;

		for (uint i = 0; i < lengthof(keytoevent); i++) {
			if (keycode == keytoevent[i].keycode) {
				(this->*(keytoevent[i].proc))(-1);
				return ES_HANDLED;
			}
		}
		return ES_NOT_HANDLED;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		if (this->goto_type == OPOS_GOTO) {
			/* check if we're clicking on a vehicle first.. clone orders in that case. */
			const Vehicle *v = CheckMouseOverVehicle();
			if (v != NULL && this->HandleOrderVehClick(v)) return;

			const Order cmd = GetOrderCmdFromTile(this->vehicle, tile);
			if (cmd.IsType(OT_NOTHING)) return;

			if (DoCommandP(this->vehicle->tile, this->vehicle->index + (this->OrderGetSel() << 16), cmd.Pack(), CMD_INSERT_ORDER | CMD_MSG(STR_ERROR_CAN_T_INSERT_NEW_ORDER))) {
				/* With quick goto the Go To button stays active */
				if (!_settings_client.gui.quick_goto) ResetObjectToPlace();
			}
		}
	}

	virtual void OnPlaceObjectAbort()
	{
		if (this->goto_type == OPOS_CONDITIONAL) {
			this->goto_type = OPOS_GOTO;
			NWidgetBase *nwid = this->GetWidget<NWidgetBase>(ORDER_WIDGET_ORDER_LIST);
			if (IsInsideBS(_cursor.pos.x, this->left + nwid->pos_x, nwid->current_x) && IsInsideBS(_cursor.pos.y, this->top + nwid->pos_y, nwid->current_y)) {
				int order_id = this->GetOrderFromPt(_cursor.pos.y - this->top);
				if (order_id != INVALID_ORDER) {
					Order order;
					order.next = NULL;
					order.index = 0;
					order.MakeConditional(order_id);

					DoCommandP(this->vehicle->tile, this->vehicle->index + (this->OrderGetSel() << 16), order.Pack(), CMD_INSERT_ORDER | CMD_MSG(STR_ERROR_CAN_T_INSERT_NEW_ORDER));
				}
			}
		}
		this->RaiseWidget(ORDER_WIDGET_GOTO);
		this->SetWidgetDirty(ORDER_WIDGET_GOTO);
	}

	virtual void OnMouseLoop()
	{
		const Vehicle *v = _place_clicked_vehicle;
		/*
		 * Check if we clicked on a vehicle
		 * and if the GOTO button of this window is pressed
		 * This is because of all open order windows WE_MOUSELOOP is called
		 * and if you have 3 windows open, and this check is not done
		 * the order is copied to the last open window instead of the
		 * one where GOTO is enabled
		 */
		if (v != NULL && this->IsWidgetLowered(ORDER_WIDGET_GOTO)) {
			_place_clicked_vehicle = NULL;
			this->HandleOrderVehClick(v);
		}
	}

	virtual void OnResize()
	{
		/* Update the scroll bar */
		this->vscroll.SetCapacity(this->GetWidget<NWidgetBase>(ORDER_WIDGET_ORDER_LIST)->current_y / this->resize.step_height);
	}

	virtual void OnTimeout()
	{
		/* unclick all buttons except for the 'goto' button (ORDER_WIDGET_GOTO), which is 'persistent' */
		for (uint i = 0; i < this->nested_array_size; i++) {
			if (this->nested_array[i] != NULL && i != ORDER_WIDGET_GOTO &&
					i != ORDER_WIDGET_SEL_TOP_LEFT && i != ORDER_WIDGET_SEL_TOP_MIDDLE && i != ORDER_WIDGET_SEL_TOP_RIGHT &&
					i != ORDER_WIDGET_SEL_TOP_ROW && this->IsWidgetLowered(i)) {
				this->RaiseWidget(i);
				this->SetWidgetDirty(i);
			}
		}
	}
};

/** Nested widget definition for "your" train orders. */
static const NWidgetPart _nested_orders_train_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, ORDER_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, ORDER_WIDGET_CAPTION), SetDataTip(STR_ORDERS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_TIMETABLE_VIEW), SetMinimalSize(61, 14), SetDataTip(STR_ORDERS_TIMETABLE_VIEW, STR_ORDERS_TIMETABLE_VIEW_TOOLTIP),
		NWidget(WWT_STICKYBOX, COLOUR_GREY, ORDER_WIDGET_STICKY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, ORDER_WIDGET_ORDER_LIST), SetMinimalSize(372, 62), SetDataTip(0x0, STR_ORDERS_LIST_TOOLTIP), SetResize(1, 1), EndContainer(),
		NWidget(WWT_SCROLLBAR, COLOUR_GREY, ORDER_WIDGET_SCROLLBAR),
	EndContainer(),

	/* First button row. */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(NWID_SELECTION, INVALID_COLOUR, ORDER_WIDGET_SEL_TOP_LEFT),
				NWidget(NWID_BUTTON_DRPDOWN, COLOUR_GREY, ORDER_WIDGET_NON_STOP), SetMinimalSize(124, 12), SetFill(true, false),
														SetDataTip(STR_ORDER_NON_STOP, STR_ORDER_TOOLTIP_NON_STOP), SetResize(1, 0),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_COND_VARIABLE), SetMinimalSize(124, 12), SetFill(true, false),
														SetDataTip(STR_NULL, STR_ORDER_CONDITIONAL_VARIABLE_TOOLTIP), SetResize(1, 0),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, ORDER_WIDGET_SEL_TOP_MIDDLE),
				NWidget(NWID_BUTTON_DRPDOWN, COLOUR_GREY, ORDER_WIDGET_FULL_LOAD), SetMinimalSize(124, 12), SetFill(true, false),
														SetDataTip(STR_ORDER_TOGGLE_FULL_LOAD, STR_ORDER_TOOLTIP_FULL_LOAD), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_REFIT), SetMinimalSize(124, 12), SetFill(true, false),
														SetDataTip(STR_ORDER_REFIT, STR_ORDER_REFIT_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_COND_COMPARATOR), SetMinimalSize(124, 12), SetFill(true, false),
														SetDataTip(STR_NULL, STR_ORDER_CONDITIONAL_COMPARATOR_TOOLTIP), SetResize(1, 0),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, ORDER_WIDGET_SEL_TOP_RIGHT),
				NWidget(NWID_BUTTON_DRPDOWN, COLOUR_GREY, ORDER_WIDGET_UNLOAD), SetMinimalSize(124, 12), SetFill(true, false),
														SetDataTip(STR_ORDER_TOGGLE_UNLOAD, STR_ORDER_TOOLTIP_UNLOAD), SetResize(1, 0),
				NWidget(NWID_BUTTON_DRPDOWN, COLOUR_GREY, ORDER_WIDGET_SERVICE), SetMinimalSize(124, 12), SetFill(true, false),
														SetDataTip(STR_ORDER_SERVICE, STR_ORDER_SERVICE_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_COND_VALUE), SetMinimalSize(124, 12), SetFill(true, false),
														SetDataTip(STR_BLACK_COMMA, STR_ORDER_CONDITIONAL_VALUE_TOOLTIP), SetResize(1, 0),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, ORDER_WIDGET_SHARED_ORDER_LIST), SetMinimalSize(12, 12), SetDataTip(SPR_SHARED_ORDERS_ICON, STR_ORDERS_VEH_WITH_SHARED_ORDERS_LIST_TOOLTIP),
	EndContainer(),

	/* Second button row. */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_SKIP), SetMinimalSize(124, 12), SetFill(true, false),
													SetDataTip(STR_ORDERS_SKIP_BUTTON, STR_ORDERS_SKIP_TOOLTIP), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_DELETE), SetMinimalSize(124, 12), SetFill(true, false),
													SetDataTip(STR_ORDERS_DELETE_BUTTON, STR_ORDERS_DELETE_TOOLTIP), SetResize(1, 0),
			NWidget(NWID_BUTTON_DRPDOWN, COLOUR_GREY, ORDER_WIDGET_GOTO), SetMinimalSize(124, 12), SetFill(true, false),
													SetDataTip(STR_ORDERS_GO_TO_BUTTON, STR_ORDERS_GO_TO_TOOLTIP), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY, ORDER_WIDGET_RESIZE),
	EndContainer(),
};

static const WindowDesc _orders_train_desc(
	WDP_AUTO, WDP_AUTO, 384, 100, 384, 100,
	WC_VEHICLE_ORDERS, WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_nested_orders_train_widgets, lengthof(_nested_orders_train_widgets)
);

/** Nested widget definition for "your" orders (non-train). */
static const NWidgetPart _nested_orders_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, ORDER_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, ORDER_WIDGET_CAPTION), SetDataTip(STR_ORDERS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_TIMETABLE_VIEW), SetMinimalSize(61, 14), SetDataTip(STR_ORDERS_TIMETABLE_VIEW, STR_ORDERS_TIMETABLE_VIEW_TOOLTIP),
		NWidget(WWT_STICKYBOX, COLOUR_GREY, ORDER_WIDGET_STICKY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, ORDER_WIDGET_ORDER_LIST), SetMinimalSize(372, 62), SetDataTip(0x0, STR_ORDERS_LIST_TOOLTIP), SetResize(1, 1), EndContainer(),
		NWidget(WWT_SCROLLBAR, COLOUR_GREY, ORDER_WIDGET_SCROLLBAR),
	EndContainer(),

	/* First button row. */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SELECTION, INVALID_COLOUR, ORDER_WIDGET_SEL_TOP_ROW),
			/* load + unload buttons. */
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_BUTTON_DRPDOWN, COLOUR_GREY, ORDER_WIDGET_FULL_LOAD), SetMinimalSize(186, 12), SetFill(true, false),
													SetDataTip(STR_ORDER_TOGGLE_FULL_LOAD, STR_ORDER_TOOLTIP_FULL_LOAD), SetResize(1, 0),
				NWidget(NWID_BUTTON_DRPDOWN, COLOUR_GREY, ORDER_WIDGET_UNLOAD), SetMinimalSize(186, 12), SetFill(true, false),
													SetDataTip(STR_ORDER_TOGGLE_UNLOAD, STR_ORDER_TOOLTIP_UNLOAD), SetResize(1, 0),
			EndContainer(),
			/* Refit + service buttons. */
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_REFIT), SetMinimalSize(186, 12), SetFill(true, false),
													SetDataTip(STR_ORDER_REFIT, STR_ORDER_REFIT_TOOLTIP), SetResize(1, 0),
				NWidget(NWID_BUTTON_DRPDOWN, COLOUR_GREY, ORDER_WIDGET_SERVICE), SetMinimalSize(124, 12), SetFill(true, false),
													SetDataTip(STR_ORDER_SERVICE, STR_ORDER_SERVICE_TOOLTIP), SetResize(1, 0),
			EndContainer(),

			/* Buttons for setting a condition. */
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_COND_VARIABLE), SetMinimalSize(124, 12), SetFill(true, false),
													SetDataTip(STR_NULL, STR_ORDER_CONDITIONAL_VARIABLE_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, ORDER_WIDGET_COND_COMPARATOR), SetMinimalSize(124, 12), SetFill(true, false),
													SetDataTip(STR_NULL, STR_ORDER_CONDITIONAL_COMPARATOR_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_COND_VALUE), SetMinimalSize(124, 12), SetFill(true, false),
													SetDataTip(STR_BLACK_COMMA, STR_ORDER_CONDITIONAL_VALUE_TOOLTIP), SetResize(1, 0),
			EndContainer(),
		EndContainer(),

		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, ORDER_WIDGET_SHARED_ORDER_LIST), SetMinimalSize(12, 12), SetDataTip(SPR_SHARED_ORDERS_ICON, STR_ORDERS_VEH_WITH_SHARED_ORDERS_LIST_TOOLTIP),
	EndContainer(),

	/* Second button row. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_SKIP), SetMinimalSize(124, 12), SetFill(true, false),
											SetDataTip(STR_ORDERS_SKIP_BUTTON, STR_ORDERS_SKIP_TOOLTIP), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_DELETE), SetMinimalSize(124, 12), SetFill(true, false),
											SetDataTip(STR_ORDERS_DELETE_BUTTON, STR_ORDERS_DELETE_TOOLTIP), SetResize(1, 0),
		NWidget(NWID_BUTTON_DRPDOWN, COLOUR_GREY, ORDER_WIDGET_GOTO), SetMinimalSize(124, 12), SetFill(true, false),
											SetDataTip(STR_ORDERS_GO_TO_BUTTON, STR_ORDERS_GO_TO_TOOLTIP), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY, ORDER_WIDGET_RESIZE),
	EndContainer(),
};

static const WindowDesc _orders_desc(
	WDP_AUTO, WDP_AUTO, 384, 100, 384, 100,
	WC_VEHICLE_ORDERS, WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_nested_orders_widgets, lengthof(_nested_orders_widgets)
);

/** Nested widget definition for competitor orders. */
static const NWidgetPart _nested_other_orders_widgets[] = {
	NWidget(NWID_VERTICAL),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_CLOSEBOX, COLOUR_GREY, ORDER_WIDGET_CLOSEBOX),
			NWidget(WWT_CAPTION, COLOUR_GREY, ORDER_WIDGET_CAPTION), SetDataTip(STR_ORDERS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ORDER_WIDGET_TIMETABLE_VIEW), SetMinimalSize(61, 14), SetDataTip(STR_ORDERS_TIMETABLE_VIEW, STR_ORDERS_TIMETABLE_VIEW_TOOLTIP),
			NWidget(WWT_STICKYBOX, COLOUR_GREY, ORDER_WIDGET_STICKY),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_GREY, ORDER_WIDGET_ORDER_LIST), SetMinimalSize(372, 72), SetDataTip(0x0, STR_ORDERS_LIST_TOOLTIP), SetResize(1, 1), EndContainer(),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_SCROLLBAR, COLOUR_GREY, ORDER_WIDGET_SCROLLBAR),
				NWidget(WWT_RESIZEBOX, COLOUR_GREY, ORDER_WIDGET_RESIZE),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _other_orders_desc(
	WDP_AUTO, WDP_AUTO, 384, 86, 384, 86,
	WC_VEHICLE_ORDERS, WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE | WDF_CONSTRUCTION,
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
		new OrdersWindow((v->type == VEH_TRAIN || v->type == VEH_ROAD) ? &_orders_train_desc : &_orders_desc, v);
	}
}
