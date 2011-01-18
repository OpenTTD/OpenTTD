/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file order_sl.cpp Code handling saving and loading of orders */

#include "../stdafx.h"
#include "../order_base.h"
#include "../order_backup.h"
#include "../settings_type.h"
#include "../network/network.h"

#include "saveload.h"

/**
 * Converts this order from an old savegame's version;
 * it moves all bits to the new location.
 */
void Order::ConvertFromOldSavegame()
{
	uint8 old_flags = this->flags;
	this->flags = 0;

	/* First handle non-stop - use value from savegame if possible, else use value from config file */
	if (_settings_client.gui.sg_new_nonstop || (IsSavegameVersionBefore(22) && _settings_client.gui.new_nonstop)) {
		/* OFB_NON_STOP */
		this->SetNonStopType((old_flags & 8) ? ONSF_NO_STOP_AT_ANY_STATION : ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
	} else {
		this->SetNonStopType((old_flags & 8) ? ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS : ONSF_STOP_EVERYWHERE);
	}

	switch (this->GetType()) {
		/* Only a few types need the other savegame conversions. */
		case OT_GOTO_DEPOT: case OT_GOTO_STATION: case OT_LOADING: break;
		default: return;
	}

	if (this->GetType() != OT_GOTO_DEPOT) {
		/* Then the load flags */
		if ((old_flags & 2) != 0) { // OFB_UNLOAD
			this->SetLoadType(OLFB_NO_LOAD);
		} else if ((old_flags & 4) == 0) { // !OFB_FULL_LOAD
			this->SetLoadType(OLF_LOAD_IF_POSSIBLE);
		} else {
			/* old OTTD versions stored full_load_any in config file - assume it was enabled when loading */
			this->SetLoadType(_settings_client.gui.sg_full_load_any || IsSavegameVersionBefore(22) ? OLF_FULL_LOAD_ANY : OLFB_FULL_LOAD);
		}

		if (this->IsType(OT_GOTO_STATION)) this->SetStopLocation(OSL_PLATFORM_FAR_END);

		/* Finally fix the unload flags */
		if ((old_flags & 1) != 0) { // OFB_TRANSFER
			this->SetUnloadType(OUFB_TRANSFER);
		} else if ((old_flags & 2) != 0) { // OFB_UNLOAD
			this->SetUnloadType(OUFB_UNLOAD);
		} else {
			this->SetUnloadType(OUF_UNLOAD_IF_POSSIBLE);
		}
	} else {
		/* Then the depot action flags */
		this->SetDepotActionType(((old_flags & 6) == 4) ? ODATFB_HALT : ODATF_SERVICE_ONLY);

		/* Finally fix the depot type flags */
		uint t = ((old_flags & 6) == 6) ? ODTFB_SERVICE : ODTF_MANUAL;
		if ((old_flags & 2) != 0) t |= ODTFB_PART_OF_ORDERS;
		this->SetDepotOrderType((OrderDepotTypeFlags)t);
	}
}

/**
 * Unpacks a order from savegames with version 4 and lower
 * @param packed packed order
 * @return unpacked order
 */
static Order UnpackVersion4Order(uint16 packed)
{
	return Order(GB(packed, 8, 8) << 16 | GB(packed, 4, 4) << 8 | GB(packed, 0, 4));
}

/**
 * Unpacks a order from savegames made with TTD(Patch)
 * @param packed packed order
 * @return unpacked order
 */
Order UnpackOldOrder(uint16 packed)
{
	Order order = UnpackVersion4Order(packed);

	/*
	 * Sanity check
	 * TTD stores invalid orders as OT_NOTHING with non-zero flags/station
	 */
	if (order.IsType(OT_NOTHING) && packed != 0) order.MakeDummy();

	return order;
}

const SaveLoad *GetOrderDescription()
{
	static const SaveLoad _order_desc[] = {
		     SLE_VAR(Order, type,           SLE_UINT8),
		     SLE_VAR(Order, flags,          SLE_UINT8),
		     SLE_VAR(Order, dest,           SLE_UINT16),
		     SLE_REF(Order, next,           REF_ORDER),
		 SLE_CONDVAR(Order, refit_cargo,    SLE_UINT8,   36, SL_MAX_VERSION),
		 SLE_CONDVAR(Order, refit_subtype,  SLE_UINT8,   36, SL_MAX_VERSION),
		 SLE_CONDVAR(Order, wait_time,      SLE_UINT16,  67, SL_MAX_VERSION),
		 SLE_CONDVAR(Order, travel_time,    SLE_UINT16,  67, SL_MAX_VERSION),

		/* Leftover from the minor savegame version stuff
		 * We will never use those free bytes, but we have to keep this line to allow loading of old savegames */
		SLE_CONDNULL(10,                                  5,  35),
		     SLE_END()
	};

	return _order_desc;
}

static void Save_ORDR()
{
	Order *order;

	FOR_ALL_ORDERS(order) {
		SlSetArrayIndex(order->index);
		SlObject(order, GetOrderDescription());
	}
}

static void Load_ORDR()
{
	if (IsSavegameVersionBefore(5, 2)) {
		/* Version older than 5.2 did not have a ->next pointer. Convert them
		 * (in the old days, the orderlist was 5000 items big) */
		size_t len = SlGetFieldLength();

		if (IsSavegameVersionBefore(5)) {
			/* Pre-version 5 had another layout for orders
			 * (uint16 instead of uint32) */
			len /= sizeof(uint16);
			uint16 *orders = MallocT<uint16>(len + 1);

			SlArray(orders, len, SLE_UINT16);

			for (size_t i = 0; i < len; ++i) {
				Order *o = new (i) Order();
				o->AssignOrder(UnpackVersion4Order(orders[i]));
			}

			free(orders);
		} else if (IsSavegameVersionBefore(5, 2)) {
			len /= sizeof(uint32);
			uint32 *orders = MallocT<uint32>(len + 1);

			SlArray(orders, len, SLE_UINT32);

			for (size_t i = 0; i < len; ++i) {
				new (i) Order(orders[i]);
			}

			free(orders);
		}

		/* Update all the next pointer */
		Order *o;
		FOR_ALL_ORDERS(o) {
			/* Delete invalid orders */
			if (o->IsType(OT_NOTHING)) {
				delete o;
				continue;
			}
			/* The orders were built like this:
			 * While the order is valid, set the previous will get its next pointer set */
			Order *prev = Order::GetIfValid(order_index - 1);
			if (prev != NULL) prev->next = o;
		}
	} else {
		int index;

		while ((index = SlIterateArray()) != -1) {
			Order *order = new (index) Order();
			SlObject(order, GetOrderDescription());
		}
	}
}

static void Ptrs_ORDR()
{
	/* Orders from old savegames have pointers corrected in Load_ORDR */
	if (IsSavegameVersionBefore(5, 2)) return;

	Order *o;

	FOR_ALL_ORDERS(o) {
		SlObject(o, GetOrderDescription());
	}
}

const SaveLoad *GetOrderListDescription()
{
	static const SaveLoad _orderlist_desc[] = {
		SLE_REF(OrderList, first,              REF_ORDER),
		SLE_END()
	};

	return _orderlist_desc;
}

static void Save_ORDL()
{
	OrderList *list;

	FOR_ALL_ORDER_LISTS(list) {
		SlSetArrayIndex(list->index);
		SlObject(list, GetOrderListDescription());
	}
}

static void Load_ORDL()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		/* set num_orders to 0 so it's a valid OrderList */
		OrderList *list = new (index) OrderList(0);
		SlObject(list, GetOrderListDescription());
	}

}

static void Ptrs_ORDL()
{
	OrderList *list;

	FOR_ALL_ORDER_LISTS(list) {
		SlObject(list, GetOrderListDescription());
	}
}

const SaveLoad *GetOrderBackupDescription()
{
	static const SaveLoad _order_backup_desc[] = {
		SLE_VAR(OrderBackup, user,                  SLE_UINT32),
		SLE_VAR(OrderBackup, tile,                  SLE_UINT32),
		SLE_VAR(OrderBackup, group,                 SLE_UINT16),
		SLE_VAR(OrderBackup, service_interval,      SLE_INT32),
		SLE_STR(OrderBackup, name,                  SLE_STR, 0),
		SLE_VAR(OrderBackup, clone,                 SLE_UINT16),
		SLE_VAR(OrderBackup, orderindex,            SLE_UINT8),
		SLE_REF(OrderBackup, orders,                REF_ORDER),
		SLE_END()
	};

	return _order_backup_desc;
}

static void Save_BKOR()
{
	/* We only save this when we're a network server
	 * as we want this information on our clients. For
	 * normal games this information isn't needed. */
	if (!_networking || !_network_server) return;

	OrderBackup *ob;
	FOR_ALL_ORDER_BACKUPS(ob) {
		SlSetArrayIndex(ob->index);
		SlObject(ob, GetOrderBackupDescription());
	}
}

void Load_BKOR()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		/* set num_orders to 0 so it's a valid OrderList */
		OrderBackup *ob = new (index) OrderBackup();
		SlObject(ob, GetOrderBackupDescription());
	}

	/* If we are a network server, then we just loaded
	 * a previously saved-by-server savegame. There are
	 * no clients with a backup anymore, so clear it. */
	if (_networking && _network_server) {
		_order_backup_pool.CleanPool();
	}
}

static void Ptrs_BKOR()
{
	OrderBackup *ob;
	FOR_ALL_ORDER_BACKUPS(ob) {
		SlObject(ob, GetOrderBackupDescription());
	}
}

extern const ChunkHandler _order_chunk_handlers[] = {
	{ 'BKOR', Save_BKOR, Load_BKOR, Ptrs_BKOR, NULL, CH_ARRAY},
	{ 'ORDR', Save_ORDR, Load_ORDR, Ptrs_ORDR, NULL, CH_ARRAY},
	{ 'ORDL', Save_ORDL, Load_ORDL, Ptrs_ORDL, NULL, CH_ARRAY | CH_LAST},
};
