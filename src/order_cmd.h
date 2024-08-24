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

CommandCost CmdModifyOrder(DoCommandFlags flags, VehicleID veh, VehicleOrderID sel_ord, ModifyOrderFlags mof, uint16_t data);
CommandCost CmdSkipToOrder(DoCommandFlags flags, VehicleID veh_id, VehicleOrderID sel_ord);
CommandCost CmdDeleteOrder(DoCommandFlags flags, VehicleID veh_id, VehicleOrderID sel_ord);
CommandCost CmdInsertOrder(DoCommandFlags flags, VehicleID veh, VehicleOrderID sel_ord, const Order &new_order);
CommandCost CmdOrderRefit(DoCommandFlags flags, VehicleID veh, VehicleOrderID order_number, CargoType cargo);
CommandCost CmdCloneOrder(DoCommandFlags flags, CloneOptions action, VehicleID veh_dst, VehicleID veh_src);
CommandCost CmdMoveOrder(DoCommandFlags flags, VehicleID veh, VehicleOrderID moving_order, VehicleOrderID target_order);
CommandCost CmdClearOrderBackup(DoCommandFlags flags, TileIndex tile, ClientID user_id);

template <> struct CommandTraits<CMD_MODIFY_ORDER>       : DefaultCommandTraits<CMD_MODIFY_ORDER,       "CmdModifyOrder",      CmdModifyOrder,      CMD_LOCATION,  CMDT_ROUTE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_SKIP_TO_ORDER>      : DefaultCommandTraits<CMD_SKIP_TO_ORDER,      "CmdSkipToOrder",      CmdSkipToOrder,      CMD_LOCATION,  CMDT_ROUTE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_DELETE_ORDER>       : DefaultCommandTraits<CMD_DELETE_ORDER,       "CmdDeleteOrder",      CmdDeleteOrder,      CMD_LOCATION,  CMDT_ROUTE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_INSERT_ORDER>       : DefaultCommandTraits<CMD_INSERT_ORDER,       "CmdInsertOrder",      CmdInsertOrder,      CMD_LOCATION,  CMDT_ROUTE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_ORDER_REFIT>        : DefaultCommandTraits<CMD_ORDER_REFIT,        "CmdOrderRefit",       CmdOrderRefit,       CMD_LOCATION,  CMDT_ROUTE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_CLONE_ORDER>        : DefaultCommandTraits<CMD_CLONE_ORDER,        "CmdCloneOrder",       CmdCloneOrder,       CMD_LOCATION,  CMDT_ROUTE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_MOVE_ORDER>         : DefaultCommandTraits<CMD_MOVE_ORDER,         "CmdMoveOrder",        CmdMoveOrder,        CMD_LOCATION,  CMDT_ROUTE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_CLEAR_ORDER_BACKUP> : DefaultCommandTraits<CMD_CLEAR_ORDER_BACKUP, "CmdClearOrderBackup", CmdClearOrderBackup, CMD_CLIENT_ID, CMDT_SERVER_SETTING> {};

template <typename Tcont, typename Titer>
inline EndianBufferWriter<Tcont, Titer> &operator <<(EndianBufferWriter<Tcont, Titer> &buffer, const Order &order)
{
	return buffer << order.type << order.flags << order.dest.value << order.refit_cargo << order.wait_time << order.travel_time << order.max_speed;
}

inline EndianBufferReader &operator >>(EndianBufferReader &buffer, Order &order)
{
	return buffer >> order.type >> order.flags >> order.dest.value >> order.refit_cargo >> order.wait_time >> order.travel_time >> order.max_speed;
}

#endif /* ORDER_CMD_H */
