/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_instance.cpp Implementation of AIInstance. */

#include "../stdafx.h"
#include "../debug.h"
#include "../saveload/saveload.h"
#include "../gui.h"

#include "../script/squirrel_class.hpp"

#include "ai_config.hpp"
#include "ai_gui.hpp"
#include "ai.hpp"

#include "../script/script_fatalerror.hpp"
#include "../script/script_suspend.hpp"
#include "../script/script_storage.hpp"
#include "ai_info.hpp"
#include "ai_instance.hpp"

/* Convert all AI related classes to Squirrel data.
 * Note: this line is a marker in squirrel_export.sh. Do not change! */
#include "api/ai_accounting.hpp.sq"
#include "api/ai_airport.hpp.sq"
#include "api/ai_base.hpp.sq"
#include "api/ai_basestation.hpp.sq"
#include "api/ai_bridge.hpp.sq"
#include "api/ai_bridgelist.hpp.sq"
#include "api/ai_cargo.hpp.sq"
#include "api/ai_cargolist.hpp.sq"
#include "api/ai_company.hpp.sq"
#include "api/ai_controller.hpp.sq"
#include "api/ai_date.hpp.sq"
#include "api/ai_depotlist.hpp.sq"
#include "api/ai_engine.hpp.sq"
#include "api/ai_enginelist.hpp.sq"
#include "api/ai_error.hpp.sq"
#include "api/ai_event.hpp.sq"
#include "api/ai_event_types.hpp.sq"
#include "api/ai_execmode.hpp.sq"
#include "api/ai_gamesettings.hpp.sq"
#include "api/ai_group.hpp.sq"
#include "api/ai_grouplist.hpp.sq"
#include "api/ai_industry.hpp.sq"
#include "api/ai_industrylist.hpp.sq"
#include "api/ai_industrytype.hpp.sq"
#include "api/ai_industrytypelist.hpp.sq"
#include "api/ai_list.hpp.sq"
#include "api/ai_log.hpp.sq"
#include "api/ai_map.hpp.sq"
#include "api/ai_marine.hpp.sq"
#include "api/ai_order.hpp.sq"
#include "api/ai_rail.hpp.sq"
#include "api/ai_railtypelist.hpp.sq"
#include "api/ai_road.hpp.sq"
#include "api/ai_sign.hpp.sq"
#include "api/ai_signlist.hpp.sq"
#include "api/ai_station.hpp.sq"
#include "api/ai_stationlist.hpp.sq"
#include "api/ai_subsidy.hpp.sq"
#include "api/ai_subsidylist.hpp.sq"
#include "api/ai_testmode.hpp.sq"
#include "api/ai_tile.hpp.sq"
#include "api/ai_tilelist.hpp.sq"
#include "api/ai_town.hpp.sq"
#include "api/ai_townlist.hpp.sq"
#include "api/ai_tunnel.hpp.sq"
#include "api/ai_vehicle.hpp.sq"
#include "api/ai_vehiclelist.hpp.sq"
#include "api/ai_waypoint.hpp.sq"
#include "api/ai_waypointlist.hpp.sq"

#include "../company_base.h"
#include "../company_func.h"
#include "../fileio_func.h"

AIInstance::AIInstance() :
	ScriptInstance("AI")
{}

void AIInstance::Initialize(AIInfo *info)
{
	this->versionAPI = info->GetAPIVersion();

	/* Register the AIController (including the "import" command) */
	SQAIController_Register(this->engine);

	ScriptInstance::Initialize(info->GetMainScript(), info->GetInstanceName());
}

void AIInstance::RegisterAPI()
{
	ScriptInstance::RegisterAPI();

/* Register all classes */
	SQAIList_Register(this->engine);
	SQAIAccounting_Register(this->engine);
	SQAIAirport_Register(this->engine);
	SQAIBase_Register(this->engine);
	SQAIBaseStation_Register(this->engine);
	SQAIBridge_Register(this->engine);
	SQAIBridgeList_Register(this->engine);
	SQAIBridgeList_Length_Register(this->engine);
	SQAICargo_Register(this->engine);
	SQAICargoList_Register(this->engine);
	SQAICargoList_IndustryAccepting_Register(this->engine);
	SQAICargoList_IndustryProducing_Register(this->engine);
	SQAICargoList_StationAccepting_Register(this->engine);
	SQAICompany_Register(this->engine);
	SQAIDate_Register(this->engine);
	SQAIDepotList_Register(this->engine);
	SQAIEngine_Register(this->engine);
	SQAIEngineList_Register(this->engine);
	SQAIError_Register(this->engine);
	SQAIEvent_Register(this->engine);
	SQAIEventCompanyAskMerger_Register(this->engine);
	SQAIEventCompanyBankrupt_Register(this->engine);
	SQAIEventCompanyInTrouble_Register(this->engine);
	SQAIEventCompanyMerger_Register(this->engine);
	SQAIEventCompanyNew_Register(this->engine);
	SQAIEventController_Register(this->engine);
	SQAIEventDisasterZeppelinerCleared_Register(this->engine);
	SQAIEventDisasterZeppelinerCrashed_Register(this->engine);
	SQAIEventEngineAvailable_Register(this->engine);
	SQAIEventEnginePreview_Register(this->engine);
	SQAIEventIndustryClose_Register(this->engine);
	SQAIEventIndustryOpen_Register(this->engine);
	SQAIEventStationFirstVehicle_Register(this->engine);
	SQAIEventSubsidyAwarded_Register(this->engine);
	SQAIEventSubsidyExpired_Register(this->engine);
	SQAIEventSubsidyOffer_Register(this->engine);
	SQAIEventSubsidyOfferExpired_Register(this->engine);
	SQAIEventTownFounded_Register(this->engine);
	SQAIEventVehicleCrashed_Register(this->engine);
	SQAIEventVehicleLost_Register(this->engine);
	SQAIEventVehicleUnprofitable_Register(this->engine);
	SQAIEventVehicleWaitingInDepot_Register(this->engine);
	SQAIExecMode_Register(this->engine);
	SQAIGameSettings_Register(this->engine);
	SQAIGroup_Register(this->engine);
	SQAIGroupList_Register(this->engine);
	SQAIIndustry_Register(this->engine);
	SQAIIndustryList_Register(this->engine);
	SQAIIndustryList_CargoAccepting_Register(this->engine);
	SQAIIndustryList_CargoProducing_Register(this->engine);
	SQAIIndustryType_Register(this->engine);
	SQAIIndustryTypeList_Register(this->engine);
	SQAILog_Register(this->engine);
	SQAIMap_Register(this->engine);
	SQAIMarine_Register(this->engine);
	SQAIOrder_Register(this->engine);
	SQAIRail_Register(this->engine);
	SQAIRailTypeList_Register(this->engine);
	SQAIRoad_Register(this->engine);
	SQAISign_Register(this->engine);
	SQAISignList_Register(this->engine);
	SQAIStation_Register(this->engine);
	SQAIStationList_Register(this->engine);
	SQAIStationList_Vehicle_Register(this->engine);
	SQAISubsidy_Register(this->engine);
	SQAISubsidyList_Register(this->engine);
	SQAITestMode_Register(this->engine);
	SQAITile_Register(this->engine);
	SQAITileList_Register(this->engine);
	SQAITileList_IndustryAccepting_Register(this->engine);
	SQAITileList_IndustryProducing_Register(this->engine);
	SQAITileList_StationType_Register(this->engine);
	SQAITown_Register(this->engine);
	SQAITownEffectList_Register(this->engine);
	SQAITownList_Register(this->engine);
	SQAITunnel_Register(this->engine);
	SQAIVehicle_Register(this->engine);
	SQAIVehicleList_Register(this->engine);
	SQAIVehicleList_DefaultGroup_Register(this->engine);
	SQAIVehicleList_Depot_Register(this->engine);
	SQAIVehicleList_Group_Register(this->engine);
	SQAIVehicleList_SharedOrders_Register(this->engine);
	SQAIVehicleList_Station_Register(this->engine);
	SQAIWaypoint_Register(this->engine);
	SQAIWaypointList_Register(this->engine);
	SQAIWaypointList_Vehicle_Register(this->engine);

	if (!this->LoadCompatibilityScripts(this->versionAPI)) this->Died();
}

bool AIInstance::LoadCompatibilityScripts(const char *api_version)
{
	char script_name[32];
	seprintf(script_name, lastof(script_name), "compat_%s.nut", api_version);
	char buf[MAX_PATH];
	Searchpath sp;
	FOR_ALL_SEARCHPATHS(sp) {
		FioAppendDirectory(buf, MAX_PATH, sp, AI_DIR);
		ttd_strlcat(buf, script_name, MAX_PATH);
		if (!FileExists(buf)) continue;

		if (this->engine->LoadScript(buf)) return true;

		ScriptLog::Error("Failed to load API compatibility script");
		DEBUG(ai, 0, "Error compiling / running API compatibility script: %s", buf);
		return false;
	}

	ScriptLog::Warning("API compatibility script not found");
	return true;
}

void AIInstance::Died()
{
	ScriptInstance::Died();

	ShowAIDebugWindow(_current_company);

	const AIInfo *info = AIConfig::GetConfig(_current_company)->GetInfo();
	if (info != NULL) {
		ShowErrorMessage(STR_ERROR_AI_PLEASE_REPORT_CRASH, INVALID_STRING_ID, WL_WARNING);

		if (info->GetURL() != NULL) {
			ScriptLog::Info("Please report the error to the following URL:");
			ScriptLog::Info(info->GetURL());
		}
	}
}

void AIInstance::LoadDummyScript()
{
	extern void Script_CreateDummy(HSQUIRRELVM vm, StringID string, const char *type);
	Script_CreateDummy(this->engine->GetVM(), STR_ERROR_AI_NO_AI_FOUND, "AI");
}

int AIInstance::GetSetting(const char *name)
{
	return AIConfig::GetConfig(_current_company)->GetSetting(name);
}

ScriptInfo *AIInstance::FindLibrary(const char *library, int version)
{
	return (ScriptInfo *)AI::FindLibrary(library, version);
}

/**
 * DoCommand callback function for all commands executed by AIs.
 * @param result The result of the command.
 * @param tile The tile on which the command was executed.
 * @param p1 p1 as given to DoCommandPInternal.
 * @param p2 p2 as given to DoCommandPInternal.
 */
void CcAI(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	Company::Get(_current_company)->ai_instance->DoCommandCallback(result, tile, p1, p2);
	Company::Get(_current_company)->ai_instance->Continue();
}

CommandCallback *AIInstance::GetDoCommandCallback()
{
	return &CcAI;
}
