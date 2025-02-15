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

DEF_CMD_TRAIT(CMD_MODIFY_ORDER,       CmdModifyOrder,       CommandFlag::Location, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SKIP_TO_ORDER,      CmdSkipToOrder,       CommandFlag::Location, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_DELETE_ORDER,       CmdDeleteOrder,       CommandFlag::Location, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_INSERT_ORDER,       CmdInsertOrder,       CommandFlag::Location, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_ORDER_REFIT,        CmdOrderRefit,        CommandFlag::Location, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_CLONE_ORDER,        CmdCloneOrder,        CommandFlag::Location, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_MOVE_ORDER,         CmdMoveOrder,         CommandFlag::Location, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_CLEAR_ORDER_BACKUP, CmdClearOrderBackup,  CommandFlag::ClientID, CMDT_SERVER_SETTING)

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
