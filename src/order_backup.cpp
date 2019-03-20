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
#include "network/network.h"
#include "network/network_func.h"
#include "order_backup.h"
#include "vehicle_base.h"
#include "window_func.h"
#include "station_map.h"

#include "safeguards.h"

OrderBackupPool _order_backup_pool("BackupOrder");
INSTANTIATE_POOL_METHODS(OrderBackup)

/** Free everything that is allocated. */
OrderBackup::~OrderBackup()
{
	if (CleaningPool()) return;

	Order *o = this->orders;
	while (o != NULL) {
		Order *next = o->next;
		delete o;
		o = next;
	}
}

/**
 * Create an order backup for the given vehicle.
 * @param v    The vehicle to make a backup of.
 * @param user The user that is requesting the backup.
 */
OrderBackup::OrderBackup(const Vehicle *v, uint32 user)
{
	this->user             = user;
	this->tile             = v->tile;
	this->group            = v->group_id;

	this->CopyConsistPropertiesFrom(v);

	/* If we have shared orders, store the vehicle we share the order with. */
	if (v->IsOrderListShared()) {
		this->clone = (v->FirstShared() == v) ? v->NextShared() : v->FirstShared();
	} else {
		/* Else copy the orders */
		Order **tail = &this->orders;

		/* Count the number of orders */
		const Order *order;
		FOR_VEHICLE_ORDERS(v, order) {
			Order *copy = new Order();
			copy->AssignOrder(*order);
			*tail = copy;
			tail = &copy->next;
		}
	}
}

/**
 * Restore the data of this order to the given vehicle.
 * @param v The vehicle to restore to.
 */
void OrderBackup::DoRestore(Vehicle *v)
{
	/* If we had shared orders, recover that */
	if (this->clone != NULL) {
		DoCommand(0, v->index | CO_SHARE << 30, this->clone->index, DC_EXEC, CMD_CLONE_ORDER);
	} else if (this->orders != NULL && OrderList::CanAllocateItem()) {
		v->orders.list = new OrderList(this->orders, v);
		this->orders = NULL;
		/* Make sure buoys/oil rigs are updated in the station list. */
		InvalidateWindowClassesData(WC_STATION_LIST, 0);
	}

	v->CopyConsistPropertiesFrom(this);

	/* Make sure orders are in range */
	v->UpdateRealOrderIndex();
	if (v->cur_implicit_order_index >= v->GetNumOrders()) v->cur_implicit_order_index = v->cur_real_order_index;

	/* Restore vehicle group */
	DoCommand(0, this->group, v->index, DC_EXEC, CMD_ADD_VEHICLE_GROUP);
}

/**
 * Create an order backup for the given vehicle.
 * @param v    The vehicle to make a backup of.
 * @param user The user that is requesting the backup.
 * @note Will automatically remove any previous backups of this user.
 */
/* static */ void OrderBackup::Backup(const Vehicle *v, uint32 user)
{
	/* Don't use reset as that broadcasts over the network to reset the variable,
	 * which is what we are doing at the moment. */
	OrderBackup *ob;
	FOR_ALL_ORDER_BACKUPS(ob) {
		if (ob->user == user) delete ob;
	}
	if (OrderBackup::CanAllocateItem()) {
		new OrderBackup(v, user);
	}
}

/**
 * Restore the data of this order to the given vehicle.
 * @param v    The vehicle to restore to.
 * @param user The user that built the vehicle, thus wants to restore.
 * @note After restoration the backup will automatically be removed.
 */
/* static */ void OrderBackup::Restore(Vehicle *v, uint32 user)
{
	OrderBackup *ob;
	FOR_ALL_ORDER_BACKUPS(ob) {
		if (v->tile != ob->tile || ob->user != user) continue;

		ob->DoRestore(v);
		delete ob;
	}
}

/**
 * Reset an OrderBackup given a tile and user.
 * @param tile The tile associated with the OrderBackup.
 * @param user The user associated with the OrderBackup.
 * @note Must not be used from the GUI!
 */
/* static */ void OrderBackup::ResetOfUser(TileIndex tile, uint32 user)
{
	OrderBackup *ob;
	FOR_ALL_ORDER_BACKUPS(ob) {
		if (ob->user == user && (ob->tile == tile || tile == INVALID_TILE)) delete ob;
	}
}

/**
 * Clear an OrderBackup
 * @param tile  Tile related to the to-be-cleared OrderBackup.
 * @param flags For command.
 * @param p1    Unused.
 * @param p2    User that had the OrderBackup.
 * @param text  Unused.
 * @return The cost of this operation or an error.
 */
CommandCost CmdClearOrderBackup(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	/* No need to check anything. If the tile or user don't exist we just ignore it. */
	if (flags & DC_EXEC) OrderBackup::ResetOfUser(tile == 0 ? INVALID_TILE : tile, p2);

	return CommandCost();
}

/**
 * Reset an user's OrderBackup if needed.
 * @param user The user associated with the OrderBackup.
 * @pre _network_server.
 * @note Must not be used from a command.
 */
/* static */ void OrderBackup::ResetUser(uint32 user)
{
	assert(_network_server);

	OrderBackup *ob;
	FOR_ALL_ORDER_BACKUPS(ob) {
		/* If it's not an backup of us, so ignore it. */
		if (ob->user != user) continue;

		DoCommandP(0, 0, user, CMD_CLEAR_ORDER_BACKUP);
		return;
	}
}

/**
 * Reset the OrderBackups from GUI/game logic.
 * @param t        The tile of the order backup.
 * @param from_gui Whether the call came from the GUI, i.e. whether
 *                 it must be synced over the network.
 */
/* static */ void OrderBackup::Reset(TileIndex t, bool from_gui)
{
	/* The user has CLIENT_ID_SERVER as default when network play is not active,
	 * but compiled it. A network client has its own variable for the unique
	 * client/user identifier. Finally if networking isn't compiled in the
	 * default is just plain and simple: 0. */
	uint32 user = _networking && !_network_server ? _network_own_client_id : CLIENT_ID_SERVER;

	OrderBackup *ob;
	FOR_ALL_ORDER_BACKUPS(ob) {
		/* If it's not an backup of us, so ignore it. */
		if (ob->user != user) continue;
		/* If it's not for our chosen tile either, ignore it. */
		if (t != INVALID_TILE && t != ob->tile) continue;

		if (from_gui) {
			/* We need to circumvent the "prevention" from this command being executed
			 * while the game is paused, so use the internal method. Nor do we want
			 * this command to get its cost estimated when shift is pressed. */
			DoCommandPInternal(ob->tile, 0, user, CMD_CLEAR_ORDER_BACKUP, NULL, NULL, true, false);
		} else {
			/* The command came from the game logic, i.e. the clearing of a tile.
			 * In that case we have no need to actually sync this, just do it. */
			delete ob;
		}
	}
}

/**
 * Clear the group of all backups having this group ID.
 * @param group The group to clear.
 */
/* static */ void OrderBackup::ClearGroup(GroupID group)
{
	OrderBackup *ob;
	FOR_ALL_ORDER_BACKUPS(ob) {
		if (ob->group == group) ob->group = DEFAULT_GROUP;
	}
}

/**
 * Clear/update the (clone) vehicle from an order backup.
 * @param v The vehicle to clear.
 * @pre v != NULL
 * @note If it is not possible to set another vehicle as clone
 *       "example", then this backed up order will be removed.
 */
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

/**
 * Removes an order from all vehicles. Triggers when, say, a station is removed.
 * @param type The type of the order (OT_GOTO_[STATION|DEPOT|WAYPOINT]).
 * @param destination The destination. Can be a StationID, DepotID or WaypointID.
 * @param hangar Only used for airports in the destination.
 *               When false, remove airport and hangar orders.
 *               When true, remove either airport or hangar order.
 */
/* static */ void OrderBackup::RemoveOrder(OrderType type, DestinationID destination, bool hangar)
{
	OrderBackup *ob;
	FOR_ALL_ORDER_BACKUPS(ob) {
		for (Order *order = ob->orders; order != NULL; order = order->next) {
			OrderType ot = order->GetType();
			if (ot == OT_GOTO_DEPOT && (order->GetDepotActionType() & ODATFB_NEAREST_DEPOT) != 0) continue;
			if (ot == OT_GOTO_DEPOT && hangar && !IsHangarTile(ob->tile)) continue; // Not an aircraft? Can't have a hangar order.
			if (ot == OT_IMPLICIT || (IsHangarTile(ob->tile) && ot == OT_GOTO_DEPOT && !hangar)) ot = OT_GOTO_STATION;
			if (ot == type && order->GetDestination() == destination) {
				/* Remove the order backup! If a station/depot gets removed, we can't/shouldn't restore those broken orders. */
				delete ob;
				break;
			}
		}
	}
}
