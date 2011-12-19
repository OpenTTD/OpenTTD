/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_instance.cpp Implementation of GameInstance. */

#include "../stdafx.h"
#include "../debug.h"
#include "../saveload/saveload.h"

#include "../script/squirrel_class.hpp"

#include "../script/script_storage.hpp"
#include "game_config.hpp"
#include "game_info.hpp"
#include "game_instance.hpp"
#include "game.hpp"

/* Convert all Game related classes to Squirrel data.
 * Note: this line is a marker in squirrel_export.sh. Do not change! */
#include "../script/api/game/game_accounting.hpp.sq"
#include "../script/api/game/game_airport.hpp.sq"
#include "../script/api/game/game_base.hpp.sq"
#include "../script/api/game/game_basestation.hpp.sq"
#include "../script/api/game/game_bridge.hpp.sq"
#include "../script/api/game/game_bridgelist.hpp.sq"
#include "../script/api/game/game_cargo.hpp.sq"
#include "../script/api/game/game_cargolist.hpp.sq"
#include "../script/api/game/game_company.hpp.sq"
#include "../script/api/game/game_controller.hpp.sq"
#include "../script/api/game/game_date.hpp.sq"
#include "../script/api/game/game_depotlist.hpp.sq"
#include "../script/api/game/game_engine.hpp.sq"
#include "../script/api/game/game_enginelist.hpp.sq"
#include "../script/api/game/game_error.hpp.sq"
#include "../script/api/game/game_event.hpp.sq"
#include "../script/api/game/game_execmode.hpp.sq"
#include "../script/api/game/game_game.hpp.sq"
#include "../script/api/game/game_gamesettings.hpp.sq"
#include "../script/api/game/game_industry.hpp.sq"
#include "../script/api/game/game_industrylist.hpp.sq"
#include "../script/api/game/game_industrytype.hpp.sq"
#include "../script/api/game/game_industrytypelist.hpp.sq"
#include "../script/api/game/game_infrastructure.hpp.sq"
#include "../script/api/game/game_list.hpp.sq"
#include "../script/api/game/game_log.hpp.sq"
#include "../script/api/game/game_map.hpp.sq"
#include "../script/api/game/game_marine.hpp.sq"
#include "../script/api/game/game_rail.hpp.sq"
#include "../script/api/game/game_railtypelist.hpp.sq"
#include "../script/api/game/game_road.hpp.sq"
#include "../script/api/game/game_sign.hpp.sq"
#include "../script/api/game/game_signlist.hpp.sq"
#include "../script/api/game/game_station.hpp.sq"
#include "../script/api/game/game_stationlist.hpp.sq"
#include "../script/api/game/game_subsidy.hpp.sq"
#include "../script/api/game/game_subsidylist.hpp.sq"
#include "../script/api/game/game_testmode.hpp.sq"
#include "../script/api/game/game_tile.hpp.sq"
#include "../script/api/game/game_tilelist.hpp.sq"
#include "../script/api/game/game_town.hpp.sq"
#include "../script/api/game/game_townlist.hpp.sq"
#include "../script/api/game/game_tunnel.hpp.sq"
#include "../script/api/game/game_vehicle.hpp.sq"
#include "../script/api/game/game_vehiclelist.hpp.sq"
#include "../script/api/game/game_waypoint.hpp.sq"
#include "../script/api/game/game_waypointlist.hpp.sq"


GameInstance::GameInstance() :
	ScriptInstance("GS")
{}

void GameInstance::Initialize(GameInfo *info)
{
	/* Register the GameController */
	SQGSController_Register(this->engine);

	ScriptInstance::Initialize(info->GetMainScript(), info->GetInstanceName());
}

void GameInstance::RegisterAPI()
{
	ScriptInstance::RegisterAPI();

/* Register all classes */
	SQGSList_Register(this->engine);
	SQGSAccounting_Register(this->engine);
	SQGSAirport_Register(this->engine);
	SQGSBase_Register(this->engine);
	SQGSBaseStation_Register(this->engine);
	SQGSBridge_Register(this->engine);
	SQGSBridgeList_Register(this->engine);
	SQGSBridgeList_Length_Register(this->engine);
	SQGSCargo_Register(this->engine);
	SQGSCargoList_Register(this->engine);
	SQGSCargoList_IndustryAccepting_Register(this->engine);
	SQGSCargoList_IndustryProducing_Register(this->engine);
	SQGSCargoList_StationAccepting_Register(this->engine);
	SQGSCompany_Register(this->engine);
	SQGSDate_Register(this->engine);
	SQGSDepotList_Register(this->engine);
	SQGSEngine_Register(this->engine);
	SQGSEngineList_Register(this->engine);
	SQGSError_Register(this->engine);
	SQGSEvent_Register(this->engine);
	SQGSEventController_Register(this->engine);
	SQGSExecMode_Register(this->engine);
	SQGSGame_Register(this->engine);
	SQGSGameSettings_Register(this->engine);
	SQGSIndustry_Register(this->engine);
	SQGSIndustryList_Register(this->engine);
	SQGSIndustryList_CargoAccepting_Register(this->engine);
	SQGSIndustryList_CargoProducing_Register(this->engine);
	SQGSIndustryType_Register(this->engine);
	SQGSIndustryTypeList_Register(this->engine);
	SQGSInfrastructure_Register(this->engine);
	SQGSLog_Register(this->engine);
	SQGSMap_Register(this->engine);
	SQGSMarine_Register(this->engine);
	SQGSRail_Register(this->engine);
	SQGSRailTypeList_Register(this->engine);
	SQGSRoad_Register(this->engine);
	SQGSSign_Register(this->engine);
	SQGSSignList_Register(this->engine);
	SQGSStation_Register(this->engine);
	SQGSStationList_Register(this->engine);
	SQGSStationList_Vehicle_Register(this->engine);
	SQGSSubsidy_Register(this->engine);
	SQGSSubsidyList_Register(this->engine);
	SQGSTestMode_Register(this->engine);
	SQGSTile_Register(this->engine);
	SQGSTileList_Register(this->engine);
	SQGSTileList_IndustryAccepting_Register(this->engine);
	SQGSTileList_IndustryProducing_Register(this->engine);
	SQGSTileList_StationType_Register(this->engine);
	SQGSTown_Register(this->engine);
	SQGSTownEffectList_Register(this->engine);
	SQGSTownList_Register(this->engine);
	SQGSTunnel_Register(this->engine);
	SQGSVehicle_Register(this->engine);
	SQGSVehicleList_Register(this->engine);
	SQGSVehicleList_Depot_Register(this->engine);
	SQGSVehicleList_SharedOrders_Register(this->engine);
	SQGSVehicleList_Station_Register(this->engine);
	SQGSWaypoint_Register(this->engine);
	SQGSWaypointList_Register(this->engine);
	SQGSWaypointList_Vehicle_Register(this->engine);

}

int GameInstance::GetSetting(const char *name)
{
	return GameConfig::GetConfig()->GetSetting(name);
}

ScriptInfo *GameInstance::FindLibrary(const char *library, int version)
{
	return (ScriptInfo *)Game::FindLibrary(library, version);
}

/**
 * DoCommand callback function for all commands executed by Game Scripts.
 * @param result The result of the command.
 * @param tile The tile on which the command was executed.
 * @param p1 p1 as given to DoCommandPInternal.
 * @param p2 p2 as given to DoCommandPInternal.
 */
void CcGame(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	Game::GetGameInstance()->DoCommandCallback(result, tile, p1, p2);
	Game::GetGameInstance()->Continue();
}

CommandCallback *GameInstance::GetDoCommandCallback()
{
	return &CcGame;
}
