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

	/**
	 * Create an order backup for the given vehicle.
	 * @param v    The vehicle to make a backup of.
	 * @param user The user that is requesting the backup.
	 */
	OrderBackup(const Vehicle *v, uint32 user);

	/**
	 * Restore the data of this order to the given vehicle.
	 * @param v The vehicle to restore to.
	 */
	void DoRestore(Vehicle *v);

public:
	/** Free everything that is allocated. */
	~OrderBackup();

	/**
	 * Create an order backup for the given vehicle.
	 * @param v    The vehicle to make a backup of.
	 * @param user The user that is requesting the backup.
	 * @note Will automatically remove any previous backups of this user.
	 */
	static void Backup(const Vehicle *v, uint32 user);

	/**
	 * Restore the data of this order to the given vehicle.
	 * @param v    The vehicle to restore to.
	 * @param user The user that built the vehicle, thus wants to restore.
	 * @note After restoration the backup will automatically be removed.
	 */
	static void Restore(Vehicle *v, uint32 user);

	/**
	 * Reset an OrderBackup given a tile and user.
	 * @param tile The tile associated with the OrderBackup.
	 * @param user The user associated with the OrderBackup.
	 * @note Must not be used from the GUI!
	 */
	static void ResetOfUser(TileIndex tile, uint32 user);

	/**
	 * Reset an user's OrderBackup if needed.
	 * @param user The user associated with the OrderBackup.
	 * @pre _network_server.
	 * @note Must not be used from a command.
	 */
	static void ResetUser(uint32 user);

	/**
	 * Reset the OrderBackups from GUI/game logic.
	 * @param tile     The tile of the order backup.
	 * @param from_gui Whether the call came from the GUI, i.e. whether
	 *                 it must be synced over the network.
	 */
	static void Reset(TileIndex tile = INVALID_TILE, bool from_gui = true);

	/**
	 * Clear the group of all backups having this group ID.
	 * @param group The group to clear.
	 */
	static void ClearGroup(GroupID group);

	/**
	 * Clear/update the (clone) vehicle from an order backup.
	 * @param v The vehicle to clear.
	 * @pre v != NULL
	 * @note If it is not possible to set another vehicle as clone
	 *       "example", then this backed up order will be removed.
	 */
	static void ClearVehicle(const Vehicle *v);
};

#define FOR_ALL_ORDER_BACKUPS_FROM(var, start) FOR_ALL_ITEMS_FROM(OrderBackup, order_backup_index, var, start)
#define FOR_ALL_ORDER_BACKUPS(var) FOR_ALL_ORDER_BACKUPS_FROM(var, 0)

#endif /* ORDER_BACKUP_H */
