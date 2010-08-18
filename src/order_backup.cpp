/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file order_backup.cpp Handling of order backups. */

#include "stdafx.h"
#include "command_func.h"
#include "core/pool_func.hpp"
#include "order_backup.h"
#include "order_base.h"
#include "vehicle_base.h"
#include "settings_type.h"

OrderBackupPool _order_backup_pool("BackupOrder");
INSTANTIATE_POOL_METHODS(OrderBackup)

OrderBackup::~OrderBackup()
{
	free(this->name);
	free(this->orders);
}

OrderBackup::OrderBackup(const Vehicle *v)
{
	this->tile             = v->tile;
	this->orderindex       = v->cur_order_index;
	this->group            = v->group_id;
	this->service_interval = v->service_interval;

	if (v->name != NULL) this->name = strdup(v->name);

	/* If we have shared orders, store the vehicle we share the order with. */
	if (v->IsOrderListShared()) {
		this->clone = (v->FirstShared() == v) ? v->NextShared() : v->FirstShared();
	} else {
		/* Else copy the orders */

		/* Count the number of orders */
		uint cnt = 0;
		const Order *order;
		FOR_VEHICLE_ORDERS(v, order) cnt++;

		/* Allocate memory for the orders plus an end-of-orders marker */
		this->orders = MallocT<Order>(cnt + 1);

		Order *dest = this->orders;

		/* Copy the orders */
		FOR_VEHICLE_ORDERS(v, order) {
			memcpy(dest, order, sizeof(Order));
			dest++;
		}
		/* End the list with an empty order */
		dest->Free();
	}
}

void OrderBackup::DoRestore(const Vehicle *v)
{
	/* If we have a custom name, process that */
	if (this->name != NULL) DoCommandP(0, v->index, 0, CMD_RENAME_VEHICLE, NULL, this->name);

	/* If we had shared orders, recover that */
	if (this->clone != NULL) {
		DoCommandP(0, v->index | (this->clone->index << 16), CO_SHARE, CMD_CLONE_ORDER);
	} else if (this->orders != NULL) {

		/* CMD_NO_TEST_IF_IN_NETWORK is used here, because CMD_INSERT_ORDER checks if the
		 *  order number is one more than the current amount of orders, and because
		 *  in network the commands are queued before send, the second insert always
		 *  fails in test mode. By bypassing the test-mode, that no longer is a problem. */
		for (uint i = 0; !this->orders[i].IsType(OT_NOTHING); i++) {
			Order o = this->orders[i];
			/* Conditional orders need to have their destination to be valid on insertion. */
			if (o.IsType(OT_CONDITIONAL)) o.SetConditionSkipToOrder(0);

			if (!DoCommandP(0, v->index + (i << 16), o.Pack(),
					CMD_INSERT_ORDER | CMD_NO_TEST_IF_IN_NETWORK)) {
				break;
			}

			/* Copy timetable if enabled */
			if (_settings_game.order.timetabling && !DoCommandP(0, v->index | (i << 16) | (1 << 25),
					o.wait_time << 16 | o.travel_time,
					CMD_CHANGE_TIMETABLE | CMD_NO_TEST_IF_IN_NETWORK)) {
				break;
			}
		}

			/* Fix the conditional orders' destination. */
		for (uint i = 0; !this->orders[i].IsType(OT_NOTHING); i++) {
			if (!this->orders[i].IsType(OT_CONDITIONAL)) continue;

			if (!DoCommandP(0, v->index + (i << 16), MOF_LOAD | (this->orders[i].GetConditionSkipToOrder() << 4),
					CMD_MODIFY_ORDER | CMD_NO_TEST_IF_IN_NETWORK)) {
				break;
			}
		}
	}

	/* Restore vehicle order-index and service interval */
	DoCommandP(0, v->index, this->orderindex | (this->service_interval << 16), CMD_RESTORE_ORDER_INDEX);

	/* Restore vehicle group */
	DoCommandP(0, this->group, v->index, CMD_ADD_VEHICLE_GROUP);
}

/* static */ void OrderBackup::Backup(const Vehicle *v)
{
	OrderBackup::Reset();
	new OrderBackup(v);
}

/* static */ void OrderBackup::Restore(const Vehicle *v)
{
	OrderBackup *ob;
	FOR_ALL_ORDER_BACKUPS(ob) {
		if (v->tile != ob->tile) continue;

		ob->DoRestore(v);
		delete ob;
	}
}

/* static */ void OrderBackup::Reset(TileIndex t)
{
	OrderBackup *ob;
	FOR_ALL_ORDER_BACKUPS(ob) {
		if (t == INVALID_TILE || t == ob->tile) delete ob;
	}
}

/* static */ void OrderBackup::ClearGroup(GroupID group)
{
	OrderBackup *ob;
	FOR_ALL_ORDER_BACKUPS(ob) {
		if (ob->group == group) ob->group = DEFAULT_GROUP;
	}
}

/* static */ void OrderBackup::ClearVehicle(const Vehicle *v)
{
	assert(v != NULL);
	OrderBackup *ob;
	FOR_ALL_ORDER_BACKUPS(ob) {
		if (ob->clone == v) {
			/* Get another item in the shared list. */
			ob->clone = (v->FirstShared() == v) ? v->NextShared() : v->FirstShared();
			/* But if that isn't there, remove it. */
			if (ob->clone == NULL) delete ob;
		}
	}
}

void InitializeOrderBackups()
{
	_order_backup_pool.CleanPool();
}
