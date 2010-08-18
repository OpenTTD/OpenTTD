/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file order_backup.h Functions related to order backups. */

#ifndef ORDER_BACKUP_H
#define ORDER_BACKUP_H

#include "core/pool_type.hpp"
#include "order_type.h"
#include "vehicle_type.h"
#include "tile_type.h"
#include "group_type.h"
#include "company_type.h"

/** Unique identifier for an order backup. */
typedef uint8 OrderBackupID;
struct OrderBackup;

/** The pool type for order backups. */
typedef Pool<OrderBackup, OrderBackupID, 1, 256> OrderBackupPool;
/** The pool with order backups. */
extern OrderBackupPool _order_backup_pool;

/**
 * Data for backing up an order of a vehicle so it can be
 * restored after a vehicle is rebuilt in the same depot.
 */
struct OrderBackup : OrderBackupPool::PoolItem<&_order_backup_pool> {
private:
	TileIndex tile;            ///< Tile of the depot where the order was changed.
	GroupID group;             ///< The group the vehicle was part of.
	uint16 service_interval;   ///< The service interval of the vehicle.
	char *name;                ///< The custom name of the vehicle.

	VehicleID clone;           ///< VehicleID this vehicle was a clone of, or INVALID_VEHICLE.
	VehicleOrderID orderindex; ///< The order-index the vehicle had.
	Order *orders;             ///< The actual orders if the vehicle was not a clone.

public:
	/**
	 * Create an order backup for the given vehicle.
	 * @param v The vehicle to make a backup of.
	 */
	OrderBackup(const Vehicle *v);

	/** Free everything that is allocated. */
	~OrderBackup();

	/**
	 * Restore the data of this order to the given vehicle.
	 * @param v The vehicle to restore to.
	 * @note After restoration the backup will automatically be removed.
	 */
	void RestoreTo(const Vehicle *v);

	/**
	 * Get the order backup associated with a given tile.
	 * @param t The tile to get the order backup for.
	 * @return The order backup, or NULL if it doesn't exist.
	 */
	static OrderBackup *GetByTile(TileIndex t);

	/**
	 * Reset the OrderBackups.
	 */
	static void Reset();

	/**
	 * Clear the group of all backups having this group ID.
	 * @param group The group to clear
	 */
	static void ClearGroup(GroupID group);
};

#define FOR_ALL_ORDER_BACKUPS_FROM(var, start) FOR_ALL_ITEMS_FROM(OrderBackup, order_backup_index, var, start)
#define FOR_ALL_ORDER_BACKUPS(var) FOR_ALL_ORDER_BACKUPS_FROM(var, 0)

#endif /* ORDER_BACKUP_H */
