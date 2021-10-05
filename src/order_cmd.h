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

CommandProc CmdModifyOrder;
CommandProc CmdSkipToOrder;
CommandProc CmdDeleteOrder;
CommandProc CmdInsertOrder;
CommandProc CmdOrderRefit;
CommandProc CmdCloneOrder;
CommandProc CmdMoveOrder;
CommandProc CmdClearOrderBackup;

DEF_CMD_TRAIT(CMD_MODIFY_ORDER,       CmdModifyOrder,       0,             CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SKIP_TO_ORDER,      CmdSkipToOrder,       0,             CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_DELETE_ORDER,       CmdDeleteOrder,       0,             CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_INSERT_ORDER,       CmdInsertOrder,       0,             CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_ORDER_REFIT,        CmdOrderRefit,        0,             CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_CLONE_ORDER,        CmdCloneOrder,        0,             CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_MOVE_ORDER,         CmdMoveOrder,         0,             CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_CLEAR_ORDER_BACKUP, CmdClearOrderBackup,  CMD_CLIENT_ID, CMDT_SERVER_SETTING)

#endif /* ORDER_CMD_H */
