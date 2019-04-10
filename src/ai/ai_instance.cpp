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
#include "../error.h"

#include "../script/squirrel_class.hpp"

#include "ai_config.hpp"
#include "ai_gui.hpp"
#include "ai.hpp"

#include "../script/script_storage.hpp"
#include "ai_info.hpp"
#include "ai_instance.hpp"

/* Manually include the Text glue. */
#include "../script/api/template/template_text.hpp.sq"

/* Convert all AI related classes to Squirrel data.
 * Note: this line is a marker in squirrel_export.sh. Do not change! */
#include "../script/api/ai/ai_accounting.hpp.sq"
#include "../script/api/ai/ai_airport.hpp.sq"
#include "../script/api/ai/ai_base.hpp.sq"
#include "../script/api/ai/ai_basestation.hpp.sq"
#include "../script/api/ai/ai_bridge.hpp.sq"
#include "../script/api/ai/ai_bridgelist.hpp.sq"
#include "../script/api/ai/ai_cargo.hpp.sq"
#include "../script/api/ai/ai_cargolist.hpp.sq"
#include "../script/api/ai/ai_company.hpp.sq"
#include "../script/api/ai/ai_controller.hpp.sq"
#include "../script/api/ai/ai_date.hpp.sq"
#include "../script/api/ai/ai_depotlist.hpp.sq"
#include "../script/api/ai/ai_engine.hpp.sq"
#include "../script/api/ai/ai_enginelist.hpp.sq"
#include "../script/api/ai/ai_error.hpp.sq"
#include "../script/api/ai/ai_event.hpp.sq"
#include "../script/api/ai/ai_event_types.hpp.sq"
#include "../script/api/ai/ai_execmode.hpp.sq"
#include "../script/api/ai/ai_gamesettings.hpp.sq"
#include "../script/api/ai/ai_group.hpp.sq"
#include "../script/api/ai/ai_grouplist.hpp.sq"
#include "../script/api/ai/ai_industry.hpp.sq"
#include "../script/api/ai/ai_industrylist.hpp.sq"
#include "../script/api/ai/ai_industrytype.hpp.sq"
#include "../script/api/ai/ai_industrytypelist.hpp.sq"
#include "../script/api/ai/ai_infrastructure.hpp.sq"
#include "../script/api/ai/ai_list.hpp.sq"
#include "../script/api/ai/ai_log.hpp.sq"
#include "../script/api/ai/ai_map.hpp.sq"
#include "../script/api/ai/ai_marine.hpp.sq"
#include "../script/api/ai/ai_order.hpp.sq"
#include "../script/api/ai/ai_rail.hpp.sq"
#include "../script/api/ai/ai_railtypelist.hpp.sq"
#include "../script/api/ai/ai_road.hpp.sq"
#include "../script/api/ai/ai_sign.hpp.sq"
#include "../script/api/ai/ai_signlist.hpp.sq"
#include "../script/api/ai/ai_station.hpp.sq"
#include "../script/api/ai/ai_stationlist.hpp.sq"
#include "../script/api/ai/ai_subsidy.hpp.sq"
#include "../script/api/ai/ai_subsidylist.hpp.sq"
#include "../script/api/ai/ai_testmode.hpp.sq"
#include "../script/api/ai/ai_tile.hpp.sq"
#include "../script/api/ai/ai_tilelist.hpp.sq"
#include "../script/api/ai/ai_town.hpp.sq"
#include "../script/api/ai/ai_townlist.hpp.sq"
#include "../script/api/ai/ai_tunnel.hpp.sq"
#include "../script/api/ai/ai_vehicle.hpp.sq"
#include "../script/api/ai/ai_vehiclelist.hpp.sq"
#include "../script/api/ai/ai_waypoint.hpp.sq"
#include "../script/api/ai/ai_waypointlist.hpp.sq"

#include "../company_base.h"
#include "../company_func.h"

#include "../safeguards.h"

AIInstance::AIInstance() :
	ScriptInstance("AI")
{}

void AIInstance::Initialize(AIInfo *info)
{
	this->versionAPI = info->GetAPIVersion();

	/* Register the AIController (including the "import" command) */
	SQAIController_Register(this->engine);

	ScriptInstance::Initialize(info->GetMainScript(), info->GetInstanceName(), _current_company);
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
	SQAIEventAircraftDestTooFar_Register(this->engine);
	SQAIEventCompanyAskMerger_Register(this->engine);
	SQAIEventCompanyBankrupt_Register(this->engine);
	SQAIEventCompanyInTrouble_Register(this->engine);
	SQAIEventCompanyMerger_Register(this->engine);
	SQAIEventCompanyNew_Register(this->engine);
	SQAIEventCompanyTown_Register(this->engine);
	SQAIEventController_Register(this->engine);
	SQAIEventDisasterZeppelinerCleared_Register(this->engine);
	SQAIEventDisasterZeppelinerCrashed_Register(this->engine);
	SQAIEventEngineAvailable_Register(this->engine);
	SQAIEventEnginePreview_Register(this->engine);
	SQAIEventExclusiveTransportRights_Register(this->engine);
	SQAIEventIndustryClose_Register(this->engine);
	SQAIEventIndustryOpen_Register(this->engine);
	SQAIEventRoadReconstruction_Register(this->engine);
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
	SQAIInfrastructure_Register(this->engine);
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
	SQAIStationList_Cargo_Register(this->engine);
	SQAIStationList_CargoPlanned_Register(this->engine);
	SQAIStationList_CargoPlannedByFrom_Register(this->engine);
	SQAIStationList_CargoPlannedByVia_Register(this->engine);
	SQAIStationList_CargoPlannedFromByVia_Register(this->engine);
	SQAIStationList_CargoPlannedViaByFrom_Register(this->engine);
	SQAIStationList_CargoWaiting_Register(this->engine);
	SQAIStationList_CargoWaitingByFrom_Register(this->engine);
	SQAIStationList_CargoWaitingByVia_Register(this->engine);
	SQAIStationList_CargoWaitingFromByVia_Register(this->engine);
	SQAIStationList_CargoWaitingViaByFrom_Register(this->engine);
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

	if (!this->LoadCompatibilityScripts(this->versionAPI, AI_DIR)) this->Died();
}

void AIInstance::Died()
{
	ScriptInstance::Died();

	ShowAIDebugWindow(_current_company);

	const AIInfo *info = AIConfig::GetConfig(_current_company, AIConfig::SSS_FORCE_GAME)->GetInfo();
	if (info != nullptr) {
		ShowErrorMessage(STR_ERROR_AI_PLEASE_REPORT_CRASH, INVALID_STRING_ID, WL_WARNING);

		if (info->GetURL() != nullptr) {
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
	/*
	 * The company might not exist anymore. Check for this.
	 * The command checks are not useful since this callback
	 * is also called when the command fails, which is does
	 * when the company does not exist anymore.
	 */
	const Company *c = Company::GetIfValid(_current_company);
	if (c == nullptr || c->ai_instance == nullptr) return;

	c->ai_instance->DoCommandCallback(result, tile, p1, p2);
	c->ai_instance->Continue();
}

CommandCallback *AIInstance::GetDoCommandCallback()
{
	return &CcAI;
}
