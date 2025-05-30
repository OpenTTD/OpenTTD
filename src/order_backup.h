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
#include "group_type.h"
#include "tile_type.h"
#include "vehicle_type.h"
#include "base_consist.h"
#include "saveload/saveload.h"

/** Unique identifier for an order backup. */
using OrderBackupID = PoolID<uint8_t, struct OrderBackupIDTag, 255, 0xFF>;
struct OrderBackup;

/** The pool type for order backups. */
using OrderBackupPool = Pool<OrderBackup, OrderBackupID, 1>;
/** The pool with order backups. */
extern OrderBackupPool _order_backup_pool;

/**
 * Data for backing up an order of a vehicle so it can be
 * restored after a vehicle is rebuilt in the same depot.
 */
struct OrderBackup : OrderBackupPool::PoolItem<&_order_backup_pool>, BaseConsist {
private:
	friend SaveLoadTable GetOrderBackupDescription(); ///< Saving and loading of order backups.
	friend struct BKORChunkHandler; ///< Creating empty orders upon savegame loading.
	template <typename T>
	friend class SlOrders;

	uint32_t user = 0; ///< The user that requested the backup.
	TileIndex tile = INVALID_TILE; ///< Tile of the depot where the order was changed.
	GroupID group = GroupID::Invalid(); ///< The group the vehicle was part of.

	const Vehicle *clone = nullptr; ///< Vehicle this vehicle was a clone of.
	std::vector<Order> orders; ///< The actual orders if the vehicle was not a clone.
	uint32_t old_order_index = 0;

	/** Creation for savegame restoration. */
	OrderBackup() = default;
	OrderBackup(const Vehicle *v, uint32_t user);

	void DoRestore(Vehicle *v);

public:
	~OrderBackup();

	static void Backup(const Vehicle *v, uint32_t user);
	static void Restore(Vehicle *v, uint32_t user);

	static void ResetOfUser(TileIndex tile, uint32_t user);
	static void ResetUser(uint32_t user);
	static void Reset(TileIndex tile = INVALID_TILE, bool from_gui = true);

	static void ClearGroup(GroupID group);
	static void ClearVehicle(const Vehicle *v);
	static void RemoveOrder(OrderType type, DestinationID destination, bool hangar);
};

#endif /* ORDER_BACKUP_H */
