/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file order_cmd.h Command definitions related to orders. */

#ifndef ORDER_CMD_H
#define ORDER_CMD_H

#include "command_type.h"
#include "order_base.h"
#include "misc/endian_buffer.hpp"

CommandCost CmdModifyOrder(DoCommandFlag flags, VehicleID veh, VehicleOrderID sel_ord, ModifyOrderFlags mof, uint16_t data);
CommandCost CmdSkipToOrder(DoCommandFlag flags, VehicleID veh_id, VehicleOrderID sel_ord);
CommandCost CmdDeleteOrder(DoCommandFlag flags, VehicleID veh_id, VehicleOrderID sel_ord);
CommandCost CmdInsertOrder(DoCommandFlag flags, VehicleID veh, VehicleOrderID sel_ord, const Order &new_order);
CommandCost CmdOrderRefit(DoCommandFlag flags, VehicleID veh, VehicleOrderID order_number, CargoID cargo);
CommandCost CmdCloneOrder(DoCommandFlag flags, CloneOptions action, VehicleID veh_dst, VehicleID veh_src);
CommandCost CmdMoveOrder(DoCommandFlag flags, VehicleID veh, VehicleOrderID moving_order, VehicleOrderID target_order);
CommandCost CmdClearOrderBackup(DoCommandFlag flags, TileIndex tile, ClientID user_id);

DEF_CMD_TRAIT(CMD_MODIFY_ORDER,       CmdModifyOrder,       CMD_LOCATION,  CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SKIP_TO_ORDER,      CmdSkipToOrder,       CMD_LOCATION,  CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_DELETE_ORDER,       CmdDeleteOrder,       CMD_LOCATION,  CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_INSERT_ORDER,       CmdInsertOrder,       CMD_LOCATION,  CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_ORDER_REFIT,        CmdOrderRefit,        CMD_LOCATION,  CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_CLONE_ORDER,        CmdCloneOrder,        CMD_LOCATION,  CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_MOVE_ORDER,         CmdMoveOrder,         CMD_LOCATION,  CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_CLEAR_ORDER_BACKUP, CmdClearOrderBackup,  CMD_CLIENT_ID, CMDT_SERVER_SETTING)

template <typename Tcont, typename Titer>
inline EndianBufferWriter<Tcont, Titer> &operator <<(EndianBufferWriter<Tcont, Titer> &buffer, const Order &order)
{
	return buffer << order.type << order.flags << order.dest << order.refit_cargo << order.wait_time << order.travel_time << order.max_speed;
}

inline EndianBufferReader &operator >>(EndianBufferReader &buffer, Order &order)
{
	return buffer >> order.type >> order.flags >> order.dest >> order.refit_cargo >> order.wait_time >> order.travel_time >> order.max_speed;
}

#endif /* ORDER_CMD_H */
