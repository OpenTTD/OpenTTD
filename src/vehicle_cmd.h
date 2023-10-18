/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehicle_cmd.h Command definitions for vehicles. */

#ifndef VEHICLE_CMD_H
#define VEHICLE_CMD_H

#include "command_type.h"
#include "engine_type.h"
#include "vehicle_type.h"
#include "vehiclelist.h"
#include "vehiclelist_cmd.h"
#include "cargo_type.h"

std::tuple<CommandCost, VehicleID, uint, uint16_t, CargoArray> CmdBuildVehicle(DoCommandFlag flags, TileIndex tile, EngineID eid, bool use_free_vehicles, CargoID cargo, ClientID client_id);
CommandCost CmdSellVehicle(DoCommandFlag flags, VehicleID v_id, bool sell_chain, bool backup_order, ClientID client_id);
std::tuple<CommandCost, uint, uint16_t, CargoArray> CmdRefitVehicle(DoCommandFlag flags, VehicleID veh_id, CargoID new_cid, byte new_subtype, bool auto_refit, bool only_this, uint8_t num_vehicles);
CommandCost CmdSendVehicleToDepot(DoCommandFlag flags, VehicleID veh_id, DepotCommand depot_cmd, const VehicleListIdentifier &vli);
CommandCost CmdChangeServiceInt(DoCommandFlag flags, VehicleID veh_id, uint16_t serv_int, bool is_custom, bool is_percent);
CommandCost CmdRenameVehicle(DoCommandFlag flags, VehicleID veh_id, const std::string &text);
std::tuple<CommandCost, VehicleID> CmdCloneVehicle(DoCommandFlag flags, TileIndex tile, VehicleID veh_id, bool share_orders);
CommandCost CmdStartStopVehicle(DoCommandFlag flags, VehicleID veh_id, bool evaluate_startstop_cb);
CommandCost CmdMassStartStopVehicle(DoCommandFlag flags, TileIndex tile, bool do_start, bool vehicle_list_window, const VehicleListIdentifier &vli);
CommandCost CmdDepotSellAllVehicles(DoCommandFlag flags, TileIndex tile, VehicleType vehicle_type);
CommandCost CmdDepotMassAutoReplace(DoCommandFlag flags, TileIndex tile, VehicleType vehicle_type);

DEF_CMD_TRAIT(CMD_BUILD_VEHICLE,           CmdBuildVehicle,         CMD_CLIENT_ID,                CMDT_VEHICLE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_SELL_VEHICLE,            CmdSellVehicle,          CMD_CLIENT_ID | CMD_LOCATION, CMDT_VEHICLE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_REFIT_VEHICLE,           CmdRefitVehicle,         CMD_LOCATION,                 CMDT_VEHICLE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_SEND_VEHICLE_TO_DEPOT,   CmdSendVehicleToDepot,   0,                            CMDT_VEHICLE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_CHANGE_SERVICE_INT,      CmdChangeServiceInt,     0,                            CMDT_VEHICLE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_RENAME_VEHICLE,          CmdRenameVehicle,        0,                            CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_CLONE_VEHICLE,           CmdCloneVehicle,         CMD_NO_TEST,                  CMDT_VEHICLE_CONSTRUCTION) // NewGRF callbacks influence building and refitting making it impossible to correctly estimate the cost
DEF_CMD_TRAIT(CMD_START_STOP_VEHICLE,      CmdStartStopVehicle,     CMD_LOCATION,                 CMDT_VEHICLE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_MASS_START_STOP,         CmdMassStartStopVehicle, 0,                            CMDT_VEHICLE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_DEPOT_SELL_ALL_VEHICLES, CmdDepotSellAllVehicles, 0,                            CMDT_VEHICLE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_DEPOT_MASS_AUTOREPLACE,  CmdDepotMassAutoReplace, 0,                            CMDT_VEHICLE_CONSTRUCTION)

void CcBuildPrimaryVehicle(Commands cmd, const CommandCost &result, VehicleID new_veh_id, uint, uint16_t, CargoArray);
void CcStartStopVehicle(Commands cmd, const CommandCost &result, VehicleID veh_id, bool);

template <typename Tcont, typename Titer>
inline EndianBufferWriter<Tcont, Titer> &operator <<(EndianBufferWriter<Tcont, Titer> &buffer, const CargoArray &cargo_array)
{
	for (const uint &amt : cargo_array) {
		buffer << amt;
	}
	return buffer;
}

inline EndianBufferReader &operator >>(EndianBufferReader &buffer, CargoArray &cargo_array)
{
	for (uint &amt : cargo_array) {
		buffer >> amt;
	}
	return buffer;
}

#endif /* VEHICLE_CMD_H */
