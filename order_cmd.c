#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "vehicle.h"
#include "command.h"
#include "station.h"
#include "player.h"
#include "news.h"
#include "saveload.h"

/**
 *
 * Unpacks a order from savegames made with TTD(Patch)
 *
 */
Order UnpackOldOrder(uint16 packed)
{
	Order order;
	order.type    = (packed & 0x000F);
	order.flags   = (packed & 0x00F0) >> 4;
	order.station = (packed & 0xFF00) >> 8;
	order.next    = NULL;

	// Sanity check
	// TTD stores invalid orders as OT_NOTHING with non-zero flags/station
	if (order.type == OT_NOTHING && (order.flags != 0 || order.station != 0)) {
		order.type = OT_DUMMY;
		order.flags = 0;
	}

	return order;
}

/**
 *
 * Unpacks a order from savegames with version 4 and lower
 *
 */
Order UnpackVersion4Order(uint16 packed)
{
	Order order;
	order.type    = (packed & 0x000F);
	order.flags   = (packed & 0x00F0) >> 4;
	order.station = (packed & 0xFF00) >> 8;
	order.next    = NULL;
	return order;
}

/**
 *
 * Updates the widgets of a vehicle which contains the order-data
 *
 */
void InvalidateVehicleOrder(const Vehicle *v)
{
	InvalidateWindow(WC_VEHICLE_VIEW,   v->index);
	InvalidateWindow(WC_VEHICLE_ORDERS, v->index);
}

/**
 *
 * Swap two orders
 *
 */
static void SwapOrders(Order *order1, Order *order2)
{
	Order temp_order;

	temp_order = *order1;
	AssignOrder(order1, *order2);
	order1->next = order2->next;
	AssignOrder(order2, temp_order);
	order2->next = temp_order.next;
}

/**
 *
 * Allocate a new order
 *
 * @return Order* if a free space is found, else NULL.
 *
 */
static Order *AllocateOrder()
{
	Order *order;

	FOR_ALL_ORDERS(order) {
		if (order->type == OT_NOTHING) {
			uint index = order->index;
			memset(order, 0, sizeof(Order));
			order->index = index;
			return order;
		}
	}

	return NULL;
}

/**
 *
 * Assign data to an order (from an other order)
 *   This function makes sure that the index is maintained correctly
 *
 */
void AssignOrder(Order *order, Order data)
{
	order->type    = data.type;
	order->flags   = data.flags;
	order->station = data.station;
}

/**
 *
 * Add an order to the orderlist of a vehicle
 *
 * @param veh_sel      First 16 bits are the ID of the vehicle. The next 16 are the selected order (if any)
 *                       If the lastone is given, order will be inserted above thatone
 * @param packed_order Packed order to insert
 *
 */
int32 CmdInsertOrder(int x, int y, uint32 flags, uint32 veh_sel, uint32 packed_order)
{
	Vehicle *v      = GetVehicle(veh_sel & 0xFFFF);
	int sel         = veh_sel >> 16;
	Order new_order = UnpackOrder(packed_order);

	if (sel > v->num_orders)
		return_cmd_error(STR_EMPTY);

	if (IsOrderPoolFull())
		return_cmd_error(STR_8831_NO_MORE_SPACE_FOR_ORDERS);

	/* XXX - This limit is only here because the backuppedorders can't
	    handle any more then this.. */
	if (v->num_orders >= 40)
		return_cmd_error(STR_8832_TOO_MANY_ORDERS);

	/* For ships, make sure that the station is not too far away from the previous destination. */
	if (v->type == VEH_Ship && IS_HUMAN_PLAYER(v->owner) &&
		sel != 0 && GetVehicleOrder(v, sel - 1)->type == OT_GOTO_STATION) {

		int dist = GetTileDist(
			GetStation(GetVehicleOrder(v, sel - 1)->station)->xy,
			GetStation(new_order.station)->xy
		);
		if (dist >= 130)
			return_cmd_error(STR_0210_TOO_FAR_FROM_PREVIOUS_DESTINATIO);
	}

	if (flags & DC_EXEC) {
		Order *new;
		Vehicle *u;

		new = AllocateOrder();
		AssignOrder(new, new_order);

		/* Create new order and link in list */
		if (v->orders == NULL) {
			v->orders = new;
		} else {
			/* Try to get the previous item (we are inserting above the
			    selected) */
			Order *order = GetVehicleOrder(v, sel - 1);

			if (order == NULL && GetVehicleOrder(v, sel) != NULL) {
				/* There is no previous item, so we are altering v->orders itself
				    But because the orders can be shared, we copy the info over
				    the v->orders, so we don't have to change the pointers of
				    all vehicles */
				SwapOrders(v->orders, new);
				/* Now update the next pointers */
				v->orders->next = new;
			} else if (order == NULL) {
				/* 'sel' is a non-existing order, add him to the end */
				order = GetLastVehicleOrder(v);
				order->next = new;
			} else {
				/* Put the new order in between */
				new->next = order->next;
				order->next = new;
			}
		}

		u = GetFirstVehicleFromSharedList(v);
		while (u != NULL) {
			/* Increase amount of orders */
			u->num_orders++;

			/* If the orderlist was empty, assign it */
			if (u->orders == NULL)
				u->orders = v->orders;

			assert(v->orders == u->orders);

			/* If there is added an order before the current one, we need
			to update the selected order */
			if (sel <= u->cur_order_index) {
				uint cur = u->cur_order_index + 1;
				/* Check if we don't go out of bound */
				if (cur < u->num_orders)
					u->cur_order_index = cur;
			}
			/* Update any possible open window of the vehicle */
			InvalidateVehicleOrder(u);

			u = u->next_shared;
		}

		/* Make sure to rebuild the whole list */
		RebuildVehicleLists();
	}

	return 0;
}

/**
 *
 * Declone an order-list
 *
 */
static int32 DecloneOrder(Vehicle *dst, uint32 flags)
{
	if (flags & DC_EXEC) {
		/* Delete orders from vehicle */
		DeleteVehicleOrders(dst);

		InvalidateVehicleOrder(dst);
		RebuildVehicleLists();
	}
	return 0;
}

/**
 *
 * Delete an order from the orderlist of a vehicle
 *
 * @param vehicle_id The ID of the vehicle
 * @param selected   The order to delete
 *
 */
int32 CmdDeleteOrder(int x, int y, uint32 flags, uint32 vehicle_id, uint32 selected)
{
	Vehicle *v = GetVehicle(vehicle_id), *u;
	uint sel   = selected;
	Order *order;

	/* XXX -- Why is this here? :s */
	_error_message = STR_EMPTY;

	/* If we did not select an order, we maybe want to de-clone the orders */
	if (sel >= v->num_orders)
		return DecloneOrder(v, flags);

	order = GetVehicleOrder(v, sel);
	if (order == NULL)
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (GetVehicleOrder(v, sel - 1) == NULL) {
			if (GetVehicleOrder(v, sel + 1) != NULL) {
				/* First item, but not the last, so we need to alter v->orders
				    Because we can have shared order, we copy the data
				    from the next item over the deleted */
				order = GetVehicleOrder(v, sel + 1);
				SwapOrders(v->orders, order);
			} else {
				/* Last item, so clean the list */
				v->orders = NULL;
			}
		} else {
			GetVehicleOrder(v, sel - 1)->next = order->next;
		}

		/* Give the item free */
		order->type = OT_NOTHING;

		u = GetFirstVehicleFromSharedList(v);
		while (u != NULL) {
			u->num_orders--;

			if (sel < u->cur_order_index)
				u->cur_order_index--;

			/* If we removed the last order, make sure the shared vehicles
			    also set their orders to NULL */
			if (v->orders == NULL)
				u->orders = NULL;

			assert(v->orders == u->orders);

			/* NON-stop flag is misused to see if a train is in a station that is
			    on his order list or not */
			if (sel == u->cur_order_index &&
					u->current_order.type == OT_LOADING &&
					HASBIT(u->current_order.flags, OFB_NON_STOP)) {
				u->current_order.flags = 0;
			}

			/* Update any possible open window of the vehicle */
			InvalidateVehicleOrder(u);

			u = u->next_shared;
		}

		RebuildVehicleLists();
	}

	return 0;
}

/**
 *
 * Goto next order of order-list
 *
 * @param vehicle_id The ID of the vehicle
 *
 */
int32 CmdSkipOrder(int x, int y, uint32 flags, uint32 vehicle_id, uint32 not_used)
{
	Vehicle *v = GetVehicle(vehicle_id);

	if (flags & DC_EXEC) {
		/* Goto next order */
		{
			byte b = v->cur_order_index + 1;
			if (b >= v->num_orders)
				b = 0;

			v->cur_order_index = b;

			if (v->type == VEH_Train)
				v->u.rail.days_since_order_progr = 0;
		}

		/* NON-stop flag is misused to see if a train is in a station that is
		    on his order list or not */
		if (v->current_order.type == OT_LOADING &&
				HASBIT(v->current_order.flags, OFB_NON_STOP)) {
			v->current_order.flags = 0;
		}

		InvalidateVehicleOrder(v);
	}

	/* We have an aircraft/ship, they have a mini-schedule, so update them all */
	if (v->type == VEH_Aircraft) InvalidateWindowClasses(WC_AIRCRAFT_LIST);
	if (v->type == VEH_Ship) InvalidateWindowClasses(WC_SHIPS_LIST);

	return 0;
}


/**
 *
 * Add an order to the orderlist of a vehicle
 *
 * @param veh_sel      First 16 bits are the ID of the vehicle. The next 16 are the selected order (if any)
 *                       If the lastone is given, order will be inserted above thatone
 * @param mode         Mode to change the order to
 *
 */
int32 CmdModifyOrder(int x, int y, uint32 flags, uint32 veh_sel, uint32 mode)
{
	Vehicle *v = GetVehicle(veh_sel & 0xFFFF);
	byte sel = veh_sel >> 16;
	Order *order;

	/* Is it a valid order? */
	if (sel >= v->num_orders)
		return CMD_ERROR;

	order = GetVehicleOrder(v, sel);
	if (order->type != OT_GOTO_STATION &&
			(order->type != OT_GOTO_DEPOT || mode == OFB_UNLOAD) &&
			(order->type != OT_GOTO_WAYPOINT || mode != OFB_NON_STOP))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		switch (mode) {
		case OFB_FULL_LOAD:
			TOGGLEBIT(order->flags, OFB_FULL_LOAD);
			if (order->type != OT_GOTO_DEPOT)
				CLRBIT(order->flags, OFB_UNLOAD);
			break;
		case OFB_UNLOAD:
			TOGGLEBIT(order->flags, OFB_UNLOAD);
			CLRBIT(order->flags, OFB_FULL_LOAD);
			break;
		case OFB_NON_STOP:
			TOGGLEBIT(order->flags, OFB_NON_STOP);
			break;
		}

		/* Update the windows, also for vehicles that share the same order list */
		{
			Vehicle *u = GetFirstVehicleFromSharedList(v);
			while (u != NULL) {
				InvalidateVehicleOrder(u);
				u = u->next_shared;
			}
		}
	}

	return 0;
}

/**
 *
 * Clone/share/copy an order-list of an other vehicle
 *
 * @param veh1_veh2 First 16 bits are of destination vehicle, last 16 of source vehicle
 * @param mode      Mode of cloning (CO_SHARE, CO_COPY, CO_UNSHARE)
 *
 */
int32 CmdCloneOrder(int x, int y, uint32 flags, uint32 veh1_veh2, uint32 mode)
{
	Vehicle *dst = GetVehicle(veh1_veh2 & 0xFFFF);

	if (dst->type == 0 || dst->owner != _current_player)
		return CMD_ERROR;

	switch(mode) {
		case CO_SHARE: {
			Vehicle *src = GetVehicle(veh1_veh2 >> 16);

			/* Sanity checks */
			if (src->type == 0 || src->owner != _current_player || dst->type != src->type || dst == src)
				return CMD_ERROR;

			/* Trucks can't share orders with busses (and visa versa) */
			if (src->type == VEH_Road) {
				if (src->cargo_type != dst->cargo_type && (src->cargo_type == CT_PASSENGERS || dst->cargo_type == CT_PASSENGERS))
					return CMD_ERROR;
			}

			/* Is the vehicle already in the shared list? */
			{
				Vehicle *u = GetFirstVehicleFromSharedList(src);
				while (u != NULL) {
					if (u == dst)
						return CMD_ERROR;
					u = u->next_shared;
				}
			}

			if (flags & DC_EXEC) {
				/* If the destination vehicle had a OrderList, destroy it */
				DeleteVehicleOrders(dst);

				dst->orders = src->orders;
				dst->num_orders = src->num_orders;

				/* Link this vehicle in the shared-list */
				dst->next_shared = src->next_shared;
				dst->prev_shared = src;
				if (src->next_shared != NULL)
					src->next_shared->prev_shared = dst;
				src->next_shared = dst;

				InvalidateVehicleOrder(dst);
				InvalidateVehicleOrder(src);

				RebuildVehicleLists();
			}
		} break;

		case CO_COPY: {
			Vehicle *src = GetVehicle(veh1_veh2 >> 16);
			int delta;

			/* Sanity checks */
			if (src->type == 0 || src->owner != _current_player || dst->type != src->type || dst == src)
				return CMD_ERROR;

			/* Trucks can't copy all the orders from busses (and visa versa) */
			if (src->type == VEH_Road) {
				const Order *order;
				TileIndex required_dst;

				FOR_VEHICLE_ORDERS(src, order) {
					if (order->type == OT_GOTO_STATION) {
						const Station *st = GetStation(order->station);
						required_dst = (dst->cargo_type == CT_PASSENGERS) ? st->bus_tile : st->lorry_tile;
						/* This station has not the correct road-bay, so we can't copy! */
						if (!required_dst)
							return CMD_ERROR;
					}
				}
			}

			/* make sure there are orders available */
			delta = IsOrderListShared(dst) ? src->num_orders + 1 : src->num_orders - dst->num_orders;
			if (!HasOrderPoolFree(delta))
				return_cmd_error(STR_8831_NO_MORE_SPACE_FOR_ORDERS);

			if (flags & DC_EXEC) {
				const Order *order;
				Order **order_dst;

				/* If the destination vehicle had a OrderList, destroy it */
				DeleteVehicleOrders(dst);

				order_dst = &dst->orders;
				FOR_VEHICLE_ORDERS(src, order) {
					*order_dst = AllocateOrder();
					AssignOrder(*order_dst, *order);
					order_dst = &(*order_dst)->next;
				}

				dst->num_orders = src->num_orders;

				InvalidateVehicleOrder(dst);

				RebuildVehicleLists();
			}
		} break;

		case CO_UNSHARE:
			return DecloneOrder(dst, flags);
	}

	return 0;
}

/**
 *
 * Backup a vehicle order-list, so you can replace a vehicle
 *  without loosing the order-list
 *
 */
void BackupVehicleOrders(Vehicle *v, BackuppedOrders *bak)
{
	bool shared = IsOrderListShared(v);

	/* Save general info */
	bak->orderindex       = v->cur_order_index;
	bak->service_interval = v->service_interval;

	/* Safe custom string, if any */
	if ((v->string_id & 0xF800) != 0x7800) {
		bak->name[0] = 0;
	} else {
		GetName(v->string_id & 0x7FF, bak->name);
	}

	/* If we have shared orders, store it on a special way */
	if (shared) {
		Vehicle *u;
		if (v->next_shared)
			u = v->next_shared;
		else
			u = v->prev_shared;

		bak->clone = u->index;
	} else {
		/* Else copy the orders */
		Order *order, *dest;

		dest  = bak->order;

		/* We do not have shared orders */
		bak->clone = INVALID_VEHICLE;

		/* Copy the orders */
		FOR_VEHICLE_ORDERS(v, order) {
			*dest = *order;
			dest++;
		}
		/* End the list with an OT_NOTHING */
		dest->type = OT_NOTHING;
	}
}

/**
 *
 * Restore vehicle orders that are backupped via BackupVehicleOrders
 *
 */
void RestoreVehicleOrders(Vehicle *v, BackuppedOrders *bak)
{
	int i;

	/* If we have a custom name, process that */
	if (bak->name[0] != 0) {
		strcpy((char*)_decode_parameters, bak->name);
		DoCommandP(0, v->index, 0, NULL, CMD_NAME_VEHICLE);
	}

	/* Restore vehicle number and service interval */
	DoCommandP(0, v->index, bak->orderindex | (bak->service_interval << 16) , NULL, CMD_RESTORE_ORDER_INDEX);

	/* If we had shared orders, recover that */
	if (bak->clone != INVALID_VEHICLE) {
		DoCommandP(0, v->index | (bak->clone << 16), 0, NULL, CMD_CLONE_ORDER);
		return;
	}

	/* CMD_NO_TEST_IF_IN_NETWORK is used here, because CMD_INSERT_ORDER checks if the
	    order number is one more than the current amount of orders, and because
	    in network the commands are queued before send, the second insert always
	    fails in test mode. By bypassing the test-mode, that no longer is a problem. */
	for (i = 0; bak->order[i].type != OT_NOTHING; i++)
		if (!DoCommandP(0, v->index + (i << 16), PackOrder(&bak->order[i]), NULL, CMD_INSERT_ORDER | CMD_NO_TEST_IF_IN_NETWORK))
			break;
}

/**
 *
 * Restore the current-order-index of a vehicle and sets service-interval
 *
 * @param vehicle_id The ID of the vehicle
 * @param data       First 16 bits are the current-order-index
 *                   The last 16 bits are the service-interval
 *
 */
int32 CmdRestoreOrderIndex(int x, int y, uint32 flags, uint32 vehicle_id, uint32 data)
{
	if (flags & DC_EXEC) {
		Vehicle *v = GetVehicle(vehicle_id);
		v->service_interval = data >> 16;
		v->cur_order_index = data & 0xFFFF;
	}

	return 0;
}

/**
 *
 * Check the orders of a vehicle, to see if there are invalid orders and stuff
 *
 */
bool CheckOrders(uint data_a, uint data_b)
{
	Vehicle *v = GetVehicle(data_a);
	/* Does the user wants us to check things? */
	if (_patches.order_review_system == 0)
		return false;

	/* Do nothing for crashed vehicles */
	if(v->vehstatus & VS_CRASHED)
		return false;

	/* Do nothing for stopped vehicles if setting is '1' */
	if ( (_patches.order_review_system == 1) && (v->vehstatus & VS_STOPPED) )
		return false;

	/* do nothing we we're not the first vehicle in a share-chain */
	if (v->next_shared != NULL)
		return false;

	/* Only check every 20 days, so that we don't flood the message log */
	if ( ( ( v->day_counter % 20) == 0 ) && (v->owner == _local_player) ) {
		int n_st, problem_type = -1;
		const Order *order;
		const Station *st;
		int message = 0;

		/* Check the order list */
		n_st = 0;

		/*if (data_b == OC_INIT) {
			DEBUG(misc, 3) ("CheckOrder called in mode 0 (initiation mode) for %d", v->index);
		} else {
			DEBUG(misc, 3) ("CheckOrder called in mode 1 (validation mode) for %d", v->index);
		}*/
		
		FOR_VEHICLE_ORDERS(v, order) {
			/* Dummy order? */
			if (order->type == OT_DUMMY) {
				problem_type = 1;
				break;
			}
			/* Does station have a load-bay for this vehicle? */
			if (order->type == OT_GOTO_STATION) {
				TileIndex required_tile;

				n_st++;
				st = GetStation(order->station);
				required_tile = GetStationTileForVehicle(v, st);
				if (!required_tile)
					problem_type = 3;
			}
		}

		/* Check if the last and the first order are the same */
		if (v->num_orders > 1 &&
				v->orders->type    == GetLastVehicleOrder(v)->type &&
				v->orders->flags   == GetLastVehicleOrder(v)->flags &&
				v->orders->station == GetLastVehicleOrder(v)->station)
			problem_type = 2;

		/* Do we only have 1 station in our order list? */
		if ((n_st < 2) && (problem_type == -1))
			problem_type = 0;

		/* We don't have a problem */
		if (problem_type < 0) {
			/*if (data_b == OC_INIT) {
				DEBUG(misc, 3) ("CheckOrder mode 0: no problems found for %d", v->index);
			} else {
				DEBUG(misc, 3) ("CheckOrder mode 1: news item surpressed for %d", v->index);
			}*/
			return false;
		}

		/* we have a problem, are we're just in the validation process
		   so don't display an error message */
		if (data_b == OC_VALIDATE) {
			/*DEBUG(misc, 3) ("CheckOrder mode 1: new item validated for %d", v->index);*/
			return true;
		}

		message = (STR_TRAIN_HAS_TOO_FEW_ORDERS) + (((v->type) - VEH_Train) << 2) + problem_type;
		/*DEBUG(misc, 3) ("Checkorder mode 0: Triggered News Item for %d", v->index);*/

		SetDParam(0, v->unitnumber);
		AddValidatedNewsItem(
			message,
			NEWS_FLAGS(NM_SMALL, NF_VIEWPORT | NF_VEHICLE, NT_ADVICE, 0),
			v->index,
			OC_VALIDATE,	//next time, just validate the orders
			CheckOrders);
	}

	return true;
}

/**
 *
 * Delete a destination (like station, waypoint, ..) from the orders of vehicles
 *
 * @param dest type and station has to be set. This order will be removed from all orders of vehicles
 *
 */
void DeleteDestinationFromVehicleOrder(Order dest)
{
	Vehicle *v;
	Order *order;
	bool need_invalidate;

	/* Go through all vehicles */
	FOR_ALL_VEHICLES(v) {
		if (v->type == 0 || v->orders == NULL)
			continue;

		/* Forget about this station if this station is removed */
		if (v->last_station_visited == dest.station && dest.type == OT_GOTO_STATION)
			v->last_station_visited = 0xFFFF;

		/* Check the current order */
		if (v->current_order.type    == dest.type &&
				v->current_order.station == dest.station) {
			/* Mark the order as DUMMY */
			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindow(WC_VEHICLE_VIEW, v->index);
		}

		/* Clear the order from the order-list */
		need_invalidate = false;
		FOR_VEHICLE_ORDERS(v, order) {
			if (order->type == dest.type && order->station == dest.station) {
				/* Mark the order as DUMMY */
				order->type = OT_DUMMY;
				order->flags = 0;

				need_invalidate = true;
			}
		}

		/* Only invalidate once, and if needed */
		if (need_invalidate)
			InvalidateWindow(WC_VEHICLE_ORDERS, v->index);
	}
}

/**
 *
 * Checks if a vehicle has a GOTO_DEPOT in his order list
 *
 * @return True if this is true (lol ;))
 *
 */
bool VehicleHasDepotOrders(const Vehicle *v)
{
	const Order *order;

	FOR_VEHICLE_ORDERS(v, order) {
		if (order->type == OT_GOTO_DEPOT)
			return true;
	}

	return false;
}

/**
 *
 * Delete all orders from a vehicle
 *
 */
void DeleteVehicleOrders(Vehicle *v)
{
	Order *order, *cur;

	/* If we have a shared order-list, don't delete the list, but just
	    remove our pointer */
	if (IsOrderListShared(v)) {
		const Vehicle *u = v;

		v->orders = NULL;
		v->num_orders = 0;

		/* Unlink ourself */
		if (v->prev_shared != NULL) {
			v->prev_shared->next_shared = v->next_shared;
			u = v->prev_shared;
		}
		if (v->next_shared != NULL) {
			v->next_shared->prev_shared = v->prev_shared;
			u = v->next_shared;
		}
		v->prev_shared = NULL;
		v->next_shared = NULL;

		/* We only need to update this-one, because if there is a third
		    vehicle which shares the same order-list, nothing will change. If
		    this is the last vehicle, the last line of the order-window
		    will change from Shared order list, to Order list, so it needs
		    an update */
		InvalidateVehicleOrder(u);
		return;
	}

	/* Remove the orders */
	cur = v->orders;
	v->orders = NULL;
	v->num_orders = 0;

	order = NULL;
	while (cur != NULL) {
		if (order != NULL) {
			order->type = OT_NOTHING;
			order->next = NULL;
		}

		order = cur;
		cur = cur->next;
	}

	if (order != NULL) {
		order->type = OT_NOTHING;
		order->next = NULL;
	}
}

/**
 *
 * Check if we share our orders with an other vehicle
 *
 * @return Returns the vehicle who has the same order
 *
 */
bool IsOrderListShared(const Vehicle *v)
{
	if (v->next_shared != NULL)
		return true;

	if (v->prev_shared != NULL)
		return true;

	return false;
}

/**
 *
 * Check if a vehicle has any valid orders
 *
 * @return false if there are no valid orders
 *
 */
bool CheckForValidOrders(Vehicle *v)
{
	const Order *order;

	FOR_VEHICLE_ORDERS(v, order)
		if (order->type != OT_DUMMY)
			return true;

	return false;
}

void InitializeOrders(void)
{
	Order *order;
	int i;

	memset(&_orders, 0, sizeof(_orders[0]) * _orders_size);

	i = 0;
	FOR_ALL_ORDERS(order)
		order->index = i++;

	_backup_orders_tile = 0;
}

static const byte _order_desc[] = {
	SLE_VAR(Order,type,					SLE_UINT8),
	SLE_VAR(Order,flags,				SLE_UINT8),
	SLE_VAR(Order,station,			SLE_UINT16),
	SLE_REF(Order,next,					REF_ORDER),

	// reserve extra space in savegame here. (currently 10 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U8 | SLE_VAR_NULL, 10, 5, 255),
	SLE_END()
};

static void Save_ORDR()
{
	Order *order;

	FOR_ALL_ORDERS(order) {
		if (order->type != OT_NOTHING) {
			SlSetArrayIndex(order->index);
			SlObject(order, _order_desc);
		}
	}
}

static void Load_ORDR()
{
	if (_sl.full_version <= 0x501) {
		/* Version older than 0x502 did not have a ->next pointer. Convert them
		    (in the old days, the orderlist was 5000 items big) */
		uint len = SlGetFieldLength();
		uint i;

		if (_sl.version < 5) {
			/* Pre-version 5 had an other layout for orders
			    (uint16 instead of uint32) */
			uint16 orders[5000];

			len /= sizeof(uint16);
			assert (len <= _orders_size);

			SlArray(orders, len, SLE_UINT16);

			for (i = 0; i < len; ++i) {
				AssignOrder(GetOrder(i), UnpackVersion4Order(orders[i]));
			}
		} else if (_sl.full_version <= 0x501) {
			uint32 orders[5000];

			len /= sizeof(uint32);
			assert (len <= _orders_size);

			SlArray(orders, len, SLE_UINT32);

			for (i = 0; i < len; ++i) {
				AssignOrder(GetOrder(i), UnpackOrder(orders[i]));
			}
		}

		/* Update all the next pointer */
		for (i = 1; i < len; ++i) {
			/* The orders were built like this:
			     Vehicle one had order[0], and as long as order++.type was not
			     OT_NOTHING, it was part of the order-list of that vehicle */
			if (GetOrder(i)->type != OT_NOTHING)
				GetOrder(i - 1)->next = GetOrder(i);
		}
	} else {
		int index;

		while ((index = SlIterateArray()) != -1) {
			Order *order = GetOrder(index);

			SlObject(order, _order_desc);
		}
	}
}

const ChunkHandler _order_chunk_handlers[] = {
	{ 'ORDR', Save_ORDR, Load_ORDR, CH_ARRAY | CH_LAST},
};
