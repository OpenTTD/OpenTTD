#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "vehicle.h"
#include "command.h"
#include "station.h"
#include "player.h"
#include "news.h"

/* p1 & 0xFFFF = vehicle
 * p1 >> 16 = index in order list
 * p2 = order command to insert
 */
int32 CmdInsertOrder(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v = &_vehicles[p1 & 0xFFFF];
	int sel = p1 >> 16;
	Order new_order = UnpackOrder(p2);

	if (sel > v->num_orders) return_cmd_error(STR_EMPTY);
	if (_ptr_to_next_order == endof(_order_array)) return_cmd_error(STR_8831_NO_MORE_SPACE_FOR_ORDERS);
	if (v->num_orders >= 40) return_cmd_error(STR_8832_TOO_MANY_ORDERS);

	// for ships, make sure that the station is not too far away from the previous destination.
	if (v->type == VEH_Ship && IS_HUMAN_PLAYER(v->owner) &&
			sel != 0 && v->schedule_ptr[sel - 1].type == OT_GOTO_STATION) {

		int dist = GetTileDist(
			DEREF_STATION(v->schedule_ptr[sel - 1].station)->xy,
			DEREF_STATION(new_order.station)->xy
		);
		if (dist >= 130)
			return_cmd_error(STR_0210_TOO_FAR_FROM_PREVIOUS_DESTINATIO);
	}

	if (flags & DC_EXEC) {
		Order *s1;
		Order *s2;
		Vehicle *u;

		s1 = &v->schedule_ptr[sel];
		s2 = _ptr_to_next_order++;
		do s2[1] = s2[0]; while (--s2 >= s1);
		*s1 = new_order;

		s1 = v->schedule_ptr;

		FOR_ALL_VEHICLES(u) {
			if (u->type != 0 && u->schedule_ptr != NULL) {
				if (s1 < u->schedule_ptr) {
					u->schedule_ptr++;
				} else if (s1 == u->schedule_ptr) { // handle shared orders
					u->num_orders++;

					if ((byte)sel <= u->cur_order_index) {
						sel++;
						if ((byte)sel < u->num_orders)
							u->cur_order_index = sel;
					}
					InvalidateWindow(WC_VEHICLE_VIEW, u->index);
					InvalidateWindow(WC_VEHICLE_ORDERS, u->index);
				}
			}
		}

		RebuildVehicleLists();
	}

	return 0;
}

static int32 DecloneOrder(Vehicle *dst, uint32 flags)
{
	if (_ptr_to_next_order == endof(_order_array))
		return_cmd_error(STR_8831_NO_MORE_SPACE_FOR_ORDERS);

	if (flags & DC_EXEC) {
		DeleteVehicleSchedule(dst);

		dst->num_orders = 0;
		_ptr_to_next_order->type = OT_NOTHING;
		_ptr_to_next_order->flags = 0;
		dst->schedule_ptr = _ptr_to_next_order++;

		InvalidateWindow(WC_VEHICLE_ORDERS, dst->index);

		RebuildVehicleLists();
	}
	return 0;
}

/* p1 = vehicle
 * p2 = sel
 */
int32 CmdDeleteOrder(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v = &_vehicles[p1], *u;
	uint sel = (uint)p2;

	_error_message = STR_EMPTY;
	if (sel >= v->num_orders)
		return DecloneOrder(v, flags);

	if (flags & DC_EXEC) {
		Order *s1 = &v->schedule_ptr[sel];

		// copy all orders to get rid of the hole
		do s1[0] = s1[1]; while (++s1 != _ptr_to_next_order);
		_ptr_to_next_order--;

		s1 = v->schedule_ptr;

		FOR_ALL_VEHICLES(u) {
			if (u->type != 0 && u->schedule_ptr != NULL) {
				if (s1 < u->schedule_ptr) {
					u->schedule_ptr--;
				} else if (s1 == u->schedule_ptr) {// handle shared orders
					u->num_orders--;
					if ((byte)sel < u->cur_order_index)
						u->cur_order_index--;

					if ((byte)sel == u->cur_order_index &&
							u->current_order.type == OT_LOADING &&
							u->current_order.flags & OF_NON_STOP) {
						u->current_order.flags = 0;
					}

					InvalidateWindow(WC_VEHICLE_VIEW, u->index);
					InvalidateWindow(WC_VEHICLE_ORDERS, u->index);
				}
			}
		}

		RebuildVehicleLists();
	}

	return 0;
}

/* p1 = vehicle */
int32 CmdSkipOrder(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (flags & DC_EXEC) {
		Vehicle *v = &_vehicles[p1];

		{
			byte b = v->cur_order_index + 1;
			if (b >= v->num_orders) b = 0;
			v->cur_order_index = b;

			if (v->type == VEH_Train)
				v->u.rail.days_since_order_progr = 0;
		}

		if (v->current_order.type == OT_LOADING &&
				v->current_order.flags & OF_NON_STOP) {
			v->current_order.flags = 0;
		}

		InvalidateWindow(WC_VEHICLE_ORDERS, v->index);
	}
	return 0;
}

/* p1 = vehicle
 * p2&0xFF = sel
 * p2>>8 = mode
 */
int32 CmdModifyOrder(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v = &_vehicles[p1];
	byte sel = (byte)p2;
	Order *sched;

	if (sel >= v->num_orders)
		return CMD_ERROR;

	sched = &v->schedule_ptr[sel];
	if (sched->type != OT_GOTO_STATION &&
			(sched->type != OT_GOTO_DEPOT || (p2 >> 8) == 1))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		switch (p2 >> 8) {
		case 0: // full load
			sched->flags ^= OF_FULL_LOAD;
			if (sched->type != OT_GOTO_DEPOT) sched->flags &= ~OF_UNLOAD;
			break;
		case 1: // unload
			sched->flags ^= OF_UNLOAD;
			sched->flags &= ~OF_FULL_LOAD;
			break;
		case 2: // non stop
			sched->flags ^= OF_NON_STOP;
			break;
		}
		sched = v->schedule_ptr;

		FOR_ALL_VEHICLES(v) {
			if (v->schedule_ptr == sched)
				InvalidateWindow(WC_VEHICLE_ORDERS, v->index);
		}

	}

	return 0;
}

// Clone an order
// p1 & 0xFFFF is destination vehicle
// p1 >> 16 is source vehicle

// p2 is
//   0 - clone
//   1 - copy
//   2 - unclone


int32 CmdCloneOrder(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *dst = &_vehicles[p1 & 0xFFFF];

	if (!(dst->type && dst->owner == _current_player))
		return CMD_ERROR;

	switch(p2) {

	// share vehicle orders?
	case 0: {
		Vehicle *src = &_vehicles[p1 >> 16];

		// sanity checks
		if (!(src->owner == _current_player && dst->type == src->type && dst != src))
			return CMD_ERROR;

		// let's see what happens with road vehicles
		if (src->type == VEH_Road) {
			if (src->cargo_type != dst->cargo_type && (src->cargo_type == CT_PASSENGERS || dst->cargo_type == CT_PASSENGERS))
				return CMD_ERROR;
		}

		if (flags & DC_EXEC) {
			DeleteVehicleSchedule(dst);
			dst->schedule_ptr = src->schedule_ptr;
			dst->num_orders = src->num_orders;

			InvalidateWindow(WC_VEHICLE_ORDERS, src->index);
			InvalidateWindow(WC_VEHICLE_ORDERS, dst->index);

			RebuildVehicleLists();
		}
		break;
	}

	// copy vehicle orders?
	case 1: {
		Vehicle *src = &_vehicles[p1 >> 16];
		int delta;

		// sanity checks
		if (!(src->owner == _current_player && dst->type == src->type && dst != src))
			return CMD_ERROR;

		// let's see what happens with road vehicles
		if (src->type == VEH_Road) {
			const Order *i;
			TileIndex required_dst;

			for (i = src->schedule_ptr; i->type != OT_NOTHING; ++i) {
				if (i->type == OT_GOTO_STATION) {
					const Station *st = DEREF_STATION(i->station);
					required_dst = (dst->cargo_type == CT_PASSENGERS) ? st->bus_tile : st->lorry_tile;
					if ( !required_dst )
						return CMD_ERROR;
				}
			}
		}

		// make sure there's orders available
		delta = IsScheduleShared(dst) ? src->num_orders + 1 : src->num_orders - dst->num_orders;
		if (delta > endof(_order_array) - _ptr_to_next_order)
			return_cmd_error(STR_8831_NO_MORE_SPACE_FOR_ORDERS);

		if (flags & DC_EXEC) {
			DeleteVehicleSchedule(dst);
			dst->schedule_ptr = _ptr_to_next_order;
			dst->num_orders = src->num_orders;
			_ptr_to_next_order += src->num_orders + 1;
			memcpy(dst->schedule_ptr, src->schedule_ptr, (src->num_orders + 1) * sizeof(uint16));

			InvalidateWindow(WC_VEHICLE_ORDERS, dst->index);

			RebuildVehicleLists();
		}
		break;
	}

	// declone vehicle orders?
	case 2: return DecloneOrder(dst, flags);
	}

	return 0;
}

void BackupVehicleOrders(Vehicle *v, BackuppedOrders *bak)
{
	Vehicle *u = IsScheduleShared(v);

	bak->orderindex = v->cur_order_index;
	bak->service_interval = v->service_interval;

	if ((v->string_id & 0xF800) != 0x7800) {
		bak->name[0] = 0;
	} else {
		GetName(v->string_id & 0x7FF, bak->name);
	}

	// stored shared orders in this special way?
	if (u != NULL) {
		bak->clone = u->index;
	} else {
		Order *sched = v->schedule_ptr;
		Order *os = bak->order;

		bak->clone = INVALID_VEHICLE;

		do {
			*os++ = *sched++;
		} while (sched->type != OT_NOTHING);
	}
}

void RestoreVehicleOrders(Vehicle *v, BackuppedOrders *bak)
{
	int i;

	if (bak->name[0]) {
		strcpy((char*)_decode_parameters, bak->name);
		DoCommandP(0, v->index, 0, NULL, CMD_NAME_VEHICLE);
	}

	DoCommandP(0, v->index, bak->orderindex|(bak->service_interval<<16) , NULL, CMD_RESTORE_ORDER_INDEX);

	if (bak->clone != INVALID_VEHICLE) {
		DoCommandP(0, v->index | bak->clone << 16, 0, NULL, CMD_CLONE_ORDER);
		return;
	}

	// CMD_NO_TEST_IF_IN_NETWORK is used here, because CMD_INSERT_ORDER checks if the
	//  order number is one more then the current amount of orders, and because
	//  in network the commands are queued before send, the second insert always
	//  fails in test mode. By bypassing the test-mode, that no longer is a problem.
	for (i = 0; bak->order[i].type != OT_NOTHING; ++i)
		if (!DoCommandP(0, v->index + (i << 16), PackOrder(&bak->order[i]), NULL, CMD_INSERT_ORDER | CMD_NO_TEST_IF_IN_NETWORK))
			break;
}

/*	p1 = vehicle
 *	upper 16 bits p2 = service_interval
 *	lower 16 bits p2 = cur_order_index
 */
int32 CmdRestoreOrderIndex(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	// nonsense to update the windows, since, train rebought will have its window deleted
	if (flags & DC_EXEC) {
		Vehicle *v = &_vehicles[p1];
		v->service_interval = (uint16)(p2>>16);
		v->cur_order_index = (byte)(p2&0xFFFF);
	}
	return 0;
}

int CheckOrders(Vehicle *v)
{
	if (!_patches.order_review_system)	//User doesn't want things to be checked
		return 0;

	if(v->vehstatus & VS_CRASHED)
		return 0;

	if ( (_patches.order_review_system == 1) && (v->vehstatus & VS_STOPPED) )
		return 0;

	/* only check every 20 days, so that we don't flood the message log */
	if ( ( ( v->day_counter % 20) == 0 ) && (v->owner == _local_player) ) {
		Order order;
		Order old_order;
		int i, n_st, problem_type = -1;
		Station *st;
		int message=0;
		TileIndex required_tile=-1;

		/* check the order list */
		order = v->schedule_ptr[0];
		n_st = 0;

		old_order.type = OT_NOTHING;
		old_order.flags = 0;
		for (i = 0; order.type != OT_NOTHING; i++) {
			order = v->schedule_ptr[i];
			if (order.type == old_order.type &&
					order.flags == old_order.flags &&
					order.station == old_order.station) {
				problem_type = 2;
				break;
			}
			if (order.type == OT_DUMMY) {
				problem_type = 1;
				break;
			}
			if (order.type == OT_GOTO_STATION /*&& (order != old_order) */) {
				//I uncommented this in order not to get two error messages
				//when two identical entries are in the list
				n_st++;
				st = DEREF_STATION(order.station);
				required_tile = GetStationTileForVehicle(v,st);
				if (!required_tile) problem_type = 3;
			}
			old_order = order; //store the old order
		}

		//Now, check the last and the first order
		//as the last order is the end of order marker, jump back 2
		if (i > 2 &&
				v->schedule_ptr[0].type == v->schedule_ptr[i - 2].type &&
				v->schedule_ptr[0].flags == v->schedule_ptr[i - 2].flags &&
				v->schedule_ptr[0].station == v->schedule_ptr[i - 2].station)
			problem_type = 2;

		if ( (n_st < 2) && (problem_type == -1) ) problem_type = 0;

		SetDParam(0, v->unitnumber);

		message = (STR_TRAIN_HAS_TOO_FEW_ORDERS) + (((v->type) - VEH_Train) << 2) + problem_type;

		if (problem_type < 0) return 0;

		AddNewsItem(
			message,
			NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0),
			v->index,
			0);
	}
	// End of order check

	return 1;
}
