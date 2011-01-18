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
#include "date_type.h"
#include "group_type.h"
#include "order_type.h"
#include "tile_type.h"
#include "vehicle_type.h"

/** Unique identifier for an order backup. */
typedef uint8 OrderBackupID;
struct OrderBackup;

/** The pool type for order backups. */
typedef Pool<OrderBackup, OrderBackupID, 1, 256> OrderBackupPool;
/** The pool with order backups. */
extern OrderBackupPool _order_backup_pool;

/** Flag to pass to the vehicle construction command when an order should be preserved. */
static const uint32 MAKE_ORDER_BACKUP_FLAG = 1U << 31;

/**
 * Data for backing up an order of a vehicle so it can be
 * restored after a vehicle is rebuilt in the same depot.
 */
struct OrderBackup : OrderBackupPool::PoolItem<&_order_backup_pool> {
private:
	friend const struct SaveLoad *GetOrderBackupDescription(); ///< Saving and loading of order backups.
	friend void Load_BKOR();   ///< Creating empty orders upon savegame loading.
	uint32 user;               ///< The user that requested the backup.
	TileIndex tile;            ///< Tile of the depot where the order was changed.
	GroupID group;             ///< The group the vehicle was part of.
	Date service_interval;     ///< The service interval of the vehicle.
	char *name;                ///< The custom name of the vehicle.

	const Vehicle *clone;      ///< Vehicle this vehicle was a clone of.
	VehicleOrderID orderindex; ///< The order-index the vehicle had.
	Order *orders;             ///< The actual orders if the vehicle was not a clone.

	/** Creation for savegame restoration. */
	OrderBackup() {}
	OrderBackup(const Vehicle *v, uint32 user);

	void DoRestore(Vehicle *v);

public:
	~OrderBackup();

	static void Backup(const Vehicle *v, uint32 user);
	static void Restore(Vehicle *v, uint32 user);

	static void ResetOfUser(TileIndex tile, uint32 user);
	static void ResetUser(uint32 user);
	static void Reset(TileIndex tile = INVALID_TILE, bool from_gui = true);

	static void ClearGroup(GroupID group);
	static void ClearVehicle(const Vehicle *v);
};

#define FOR_ALL_ORDER_BACKUPS_FROM(var, start) FOR_ALL_ITEMS_FROM(OrderBackup, order_backup_index, var, start)
#define FOR_ALL_ORDER_BACKUPS(var) FOR_ALL_ORDER_BACKUPS_FROM(var, 0)

#endif /* ORDER_BACKUP_H */
