/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file order_sl.cpp Code handling saving and loading of orders */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/order_sl_compat.h"

#include "saveload_internal.h"
#include "../order_backup.h"
#include "../settings_type.h"
#include "../network/network.h"

#include "../safeguards.h"

/**
 * Converts this order from an old savegame's version;
 * it moves all bits to the new location.
 */
void Order::ConvertFromOldSavegame()
{
	uint8_t old_flags = this->flags;
	this->flags = 0;

	/* First handle non-stop - use value from savegame if possible, else use value from config file */
	if (_settings_client.gui.sg_new_nonstop || (IsSavegameVersionBefore(SLV_22) && _savegame_type != SGT_TTO && _savegame_type != SGT_TTD && _settings_client.gui.new_nonstop)) {
		/* OFB_NON_STOP */
		this->SetNonStopType((old_flags & 8) ? OrderNonStopFlags{OrderNonStopFlag::NoIntermediate, OrderNonStopFlag::NoDestination} : OrderNonStopFlag::NoIntermediate);
	} else {
		this->SetNonStopType((old_flags & 8) ? OrderNonStopFlag::NoIntermediate : OrderNonStopFlags{});
	}

	switch (this->GetType()) {
		/* Only a few types need the other savegame conversions. */
		case OT_GOTO_DEPOT: case OT_GOTO_STATION: case OT_LOADING: break;
		default: return;
	}

	if (this->GetType() != OT_GOTO_DEPOT) {
		/* Then the load flags */
		if ((old_flags & 2) != 0) { // OFB_UNLOAD
			this->SetLoadType(OrderLoadType::NoLoad);
		} else if ((old_flags & 4) == 0) { // !OFB_FULL_LOAD
			this->SetLoadType(OrderLoadType::LoadIfPossible);
		} else {
			/* old OTTD versions stored full_load_any in config file - assume it was enabled when loading */
			this->SetLoadType(_settings_client.gui.sg_full_load_any || IsSavegameVersionBefore(SLV_22) ? OrderLoadType::FullLoadAny : OrderLoadType::FullLoad);
		}

		if (this->IsType(OT_GOTO_STATION)) this->SetStopLocation(OrderStopLocation::FarEnd);

		/* Finally fix the unload flags */
		if ((old_flags & 1) != 0) { // OFB_TRANSFER
			this->SetUnloadType(OrderUnloadType::Transfer);
		} else if ((old_flags & 2) != 0) { // OFB_UNLOAD
			this->SetUnloadType(OrderUnloadType::Unload);
		} else {
			this->SetUnloadType(OrderUnloadType::UnloadIfPossible);
		}
	} else {
		/* Then the depot action flags */
		OrderDepotActionFlags action_flags{};
		if ((old_flags & 6) == 4) action_flags.Set(OrderDepotActionFlag::Halt);
		this->SetDepotActionType(action_flags);

		/* Finally fix the depot type flags */
		OrderDepotTypeFlags type_flags{};
		if ((old_flags & 6) == 6) type_flags.Set(OrderDepotTypeFlag::Service);
		if ((old_flags & 2) != 0) type_flags.Set(OrderDepotTypeFlag::PartOfOrders);
		this->SetDepotOrderType(type_flags);
	}
}

/**
 * Unpacks a order from savegames with version 4 and lower
 * @param packed packed order
 * @return unpacked order
 */
static Order UnpackVersion4Order(uint16_t packed)
{
	return Order(GB(packed, 0, 4), GB(packed, 4, 4), GB(packed, 8, 8));
}

/**
 * Unpacks a order from savegames made with TTD(Patch)
 * @param packed packed order
 * @return unpacked order
 */
Order UnpackOldOrder(uint16_t packed)
{
	Order order = UnpackVersion4Order(packed);

	/*
	 * Sanity check
	 * TTD stores invalid orders as OT_NOTHING with non-zero flags/station
	 */
	if (order.IsType(OT_NOTHING) && packed != 0) order.MakeDummy();

	return order;
}

/** Temporary storage for conversion from old order pool. */
static std::vector<OldOrderSaveLoadItem> _old_order_saveload_pool;

/**
 * Clear all old orders.
 */
void ClearOldOrders()
{
	_old_order_saveload_pool.clear();
	_old_order_saveload_pool.shrink_to_fit();
}

/**
 * Get a pointer to an old order with the given reference index.
 * @param ref_index Reference index (one-based) to get.
 * @return Pointer to old order, or nullptr if not present.
 */
OldOrderSaveLoadItem *GetOldOrder(size_t ref_index)
{
	if (ref_index == 0) return nullptr;
	assert(ref_index <= _old_order_saveload_pool.size());
	return &_old_order_saveload_pool[ref_index - 1];
}

/**
 * Allocate an old order with the given pool index.
 * @param pool_index Pool index (zero-based) to allocate.
 * @return Reference to allocated old order.
 */
OldOrderSaveLoadItem &AllocateOldOrder(size_t pool_index)
{
	assert(pool_index < UINT32_MAX);
	if (pool_index >= _old_order_saveload_pool.size()) _old_order_saveload_pool.resize(pool_index + 1);
	return _old_order_saveload_pool[pool_index];
}

SaveLoadTable GetOrderDescription()
{
	static const SaveLoad _order_desc[] = {
		     SLE_VARNAME(OldOrderSaveLoadItem, order.type,  "type",  SLE_UINT8),
		     SLE_VARNAME(OldOrderSaveLoadItem, order.flags, "flags", SLE_UINT8),
		     SLE_VARNAME(OldOrderSaveLoadItem, order.dest,  "dest",  SLE_UINT16),
		 SLE_CONDVARNAME(OldOrderSaveLoadItem, next,        "next",  SLE_FILE_U16 | SLE_VAR_U32, SL_MIN_VERSION, SLV_69),
		 SLE_CONDVARNAME(OldOrderSaveLoadItem, next,        "next",  SLE_UINT32,                 SLV_69, SL_MAX_VERSION),
		 SLE_CONDVARNAME(OldOrderSaveLoadItem, order.refit_cargo, "refit_cargo", SLE_UINT8,   SLV_36, SL_MAX_VERSION),
		 SLE_CONDVARNAME(OldOrderSaveLoadItem, order.wait_time,   "wait_time",   SLE_UINT16,  SLV_67, SL_MAX_VERSION),
		 SLE_CONDVARNAME(OldOrderSaveLoadItem, order.travel_time, "travel_time", SLE_UINT16,  SLV_67, SL_MAX_VERSION),
		 SLE_CONDVARNAME(OldOrderSaveLoadItem, order.max_speed,   "max_speed",   SLE_UINT16, SLV_172, SL_MAX_VERSION),
	};

	return _order_desc;
}

struct ORDRChunkHandler : ChunkHandler {
	ORDRChunkHandler() : ChunkHandler('ORDR', CH_READONLY) {}

	void Load() const override
	{
		if (IsSavegameVersionBefore(SLV_5, 2)) {
			/* Version older than 5.2 did not have a ->next pointer. Convert them
			 * (in the old days, the orderlist was 5000 items big) */
			size_t len = SlGetFieldLength();

			if (IsSavegameVersionBefore(SLV_5)) {
				/* Pre-version 5 had another layout for orders
				 * (uint16_t instead of uint32_t) */
				len /= sizeof(uint16_t);
				std::vector<uint16_t> orders(len);

				SlCopy(&orders[0], len, SLE_UINT16);

				for (size_t i = 0; i < len; ++i) {
					auto &item = AllocateOldOrder(i);
					item.order.AssignOrder(UnpackVersion4Order(orders[i]));
				}
			} else if (IsSavegameVersionBefore(SLV_5, 2)) {
				len /= sizeof(uint32_t);
				std::vector<uint32_t> orders(len);

				SlCopy(&orders[0], len, SLE_UINT32);

				for (size_t i = 0; i < len; ++i) {
					auto &item = AllocateOldOrder(i);
					item.order = Order(GB(orders[i], 0, 8), GB(orders[i], 8, 8), GB(orders[i], 16, 16));
				}
			}

			/* Update all the next pointer. The orders were built like this:
			 * While the order is valid, the previous order will get its next pointer set */
			for (uint32_t num = 1; const OldOrderSaveLoadItem &item : _old_order_saveload_pool) {
				if (!item.order.IsType(OT_NOTHING) && num > 1) {
					OldOrderSaveLoadItem *prev = GetOldOrder(num - 1);
					if (prev != nullptr) prev->next = num;
				}
				++num;
			}
		} else {
			const std::vector<SaveLoad> slt = SlCompatTableHeader(GetOrderDescription(), _order_sl_compat);

			int index;

			while ((index = SlIterateArray()) != -1) {
				auto &item = AllocateOldOrder(index);
				SlObject(&item, slt);
			}
		}
	}
};

template <typename T>
class SlOrders : public VectorSaveLoadHandler<SlOrders<T>, T, Order> {
public:
	static inline const SaveLoad description[] = {
		SLE_VAR(Order, type,        SLE_UINT8),
		SLE_VAR(Order, flags,       SLE_UINT8),
		SLE_VAR(Order, dest,        SLE_UINT16),
		SLE_VAR(Order, refit_cargo, SLE_UINT8),
		SLE_VAR(Order, wait_time,   SLE_UINT16),
		SLE_VAR(Order, travel_time, SLE_UINT16),
		SLE_VAR(Order, max_speed,   SLE_UINT16),
	};
	static inline const SaveLoadCompatTable compat_description = {};

	std::vector<Order> &GetVector(T *container) const override { return container->orders; }

	void LoadCheck(T *container) const override { this->Load(container); }
};

/* Instantiate SlOrders classes. */
template class SlOrders<OrderList>;
template class SlOrders<OrderBackup>;

SaveLoadTable GetOrderListDescription()
{
	static const SaveLoad _orderlist_desc[] = {
		SLE_CONDVARNAME(OrderList, old_order_index, "first", SLE_FILE_U16 | SLE_VAR_U32, SL_MIN_VERSION, SLV_69),
		SLE_CONDVARNAME(OrderList, old_order_index, "first", SLE_UINT32,                 SLV_69, SLV_ORDERS_OWNED_BY_ORDERLIST),
		SLEG_CONDSTRUCTLIST("orders", SlOrders<OrderList>, SLV_ORDERS_OWNED_BY_ORDERLIST, SL_MAX_VERSION),
	};

	return _orderlist_desc;
}

struct ORDLChunkHandler : ChunkHandler {
	ORDLChunkHandler() : ChunkHandler('ORDL', CH_TABLE) {}

	void Save() const override
	{
		const SaveLoadTable slt = GetOrderListDescription();
		SlTableHeader(slt);

		for (OrderList *list : OrderList::Iterate()) {
			SlSetArrayIndex(list->index);
			SlObject(list, slt);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(GetOrderListDescription(), _orderlist_sl_compat);

		int index;

		while ((index = SlIterateArray()) != -1) {
			OrderList *list = new (OrderListID(index)) OrderList();
			SlObject(list, slt);
		}

	}

	void FixPointers() const override
	{
		bool migrate_orders = IsSavegameVersionBefore(SLV_ORDERS_OWNED_BY_ORDERLIST);

		for (OrderList *list : OrderList::Iterate()) {
			SlObject(list, GetOrderListDescription());

			if (migrate_orders) {
				std::vector<Order> orders;
				for (OldOrderSaveLoadItem *old_order = GetOldOrder(list->old_order_index); old_order != nullptr; old_order = GetOldOrder(old_order->next)) {
					orders.push_back(std::move(old_order->order));
				}
				list->orders = std::move(orders);
			}
		}
	}
};

SaveLoadTable GetOrderBackupDescription()
{
	static const SaveLoad _order_backup_desc[] = {
		     SLE_VAR(OrderBackup, user,                     SLE_UINT32),
		     SLE_VAR(OrderBackup, tile,                     SLE_UINT32),
		     SLE_VAR(OrderBackup, group,                    SLE_UINT16),
		 SLE_CONDVAR(OrderBackup, service_interval,         SLE_FILE_U32 | SLE_VAR_U16,  SL_MIN_VERSION, SLV_192),
		 SLE_CONDVAR(OrderBackup, service_interval,         SLE_UINT16,                SLV_192, SL_MAX_VERSION),
		    SLE_SSTR(OrderBackup, name,                     SLE_STR),
		 SLE_CONDREF(OrderBackup, clone,                    REF_VEHICLE,               SLV_192, SL_MAX_VERSION),
		     SLE_VAR(OrderBackup, cur_real_order_index,     SLE_UINT8),
		 SLE_CONDVAR(OrderBackup, cur_implicit_order_index, SLE_UINT8,                 SLV_176, SL_MAX_VERSION),
		 SLE_CONDVAR(OrderBackup, current_order_time,       SLE_UINT32,                SLV_176, SL_MAX_VERSION),
		 SLE_CONDVAR(OrderBackup, lateness_counter,         SLE_INT32,                 SLV_176, SL_MAX_VERSION),
		 SLE_CONDVAR(OrderBackup, timetable_start,          SLE_FILE_I32 | SLE_VAR_U64, SLV_176, SLV_TIMETABLE_START_TICKS_FIX),
		 SLE_CONDVAR(OrderBackup, timetable_start,          SLE_UINT64,                 SLV_TIMETABLE_START_TICKS_FIX, SL_MAX_VERSION),
		 SLE_CONDVAR(OrderBackup, vehicle_flags,            SLE_FILE_U8 | SLE_VAR_U16, SLV_176, SLV_180),
		 SLE_CONDVAR(OrderBackup, vehicle_flags,            SLE_UINT16,                SLV_180, SL_MAX_VERSION),
		SLE_CONDVARNAME(OrderBackup, old_order_index, "orders", SLE_FILE_U16 | SLE_VAR_U32, SL_MIN_VERSION, SLV_69),
		SLE_CONDVARNAME(OrderBackup, old_order_index, "orders", SLE_UINT32,                 SLV_69, SLV_ORDERS_OWNED_BY_ORDERLIST),
		SLEG_CONDSTRUCTLIST("orders", SlOrders<OrderBackup>, SLV_ORDERS_OWNED_BY_ORDERLIST, SL_MAX_VERSION),
	};

	return _order_backup_desc;
}

struct BKORChunkHandler : ChunkHandler {
	BKORChunkHandler() : ChunkHandler('BKOR', CH_TABLE) {}

	void Save() const override
	{
		const SaveLoadTable slt = GetOrderBackupDescription();
		SlTableHeader(slt);

		/* We only save this when we're a network server
		 * as we want this information on our clients. For
		 * normal games this information isn't needed. */
		if (!_networking || !_network_server) return;

		for (OrderBackup *ob : OrderBackup::Iterate()) {
			SlSetArrayIndex(ob->index);
			SlObject(ob, slt);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(GetOrderBackupDescription(), _order_backup_sl_compat);

		int index;

		while ((index = SlIterateArray()) != -1) {
			/* set num_orders to 0 so it's a valid OrderList */
			OrderBackup *ob = new (OrderBackupID(index)) OrderBackup();
			SlObject(ob, slt);
		}
	}

	void FixPointers() const override
	{
		bool migrate_orders = IsSavegameVersionBefore(SLV_ORDERS_OWNED_BY_ORDERLIST);

		for (OrderBackup *ob : OrderBackup::Iterate()) {
			SlObject(ob, GetOrderBackupDescription());

			if (migrate_orders) {
				std::vector<Order> orders;
				for (OldOrderSaveLoadItem *old_order = GetOldOrder(ob->old_order_index); old_order != nullptr; old_order = GetOldOrder(old_order->next)) {
					orders.push_back(std::move(old_order->order));
				}
				ob->orders = std::move(orders);
			}
		}
	}
};

static const BKORChunkHandler BKOR;
static const ORDRChunkHandler ORDR;
static const ORDLChunkHandler ORDL;
static const ChunkHandlerRef order_chunk_handlers[] = {
	BKOR,
	ORDR,
	ORDL,
};

extern const ChunkHandlerTable _order_chunk_handlers(order_chunk_handlers);
