/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file saveload_internal.h Declaration of functions used in more save/load files */

#ifndef SAVELOAD_INTERNAL_H
#define SAVELOAD_INTERNAL_H

#include "../company_manager_face.h"
#include "../order_base.h"
#include "../engine_type.h"
#include "saveload.h"

void InitializeOldNames();
StringID RemapOldStringID(StringID s);
std::string CopyFromOldName(StringID id);
void ResetOldNames();

void ResetOldWaypoints();
void MoveBuoysToWaypoints();
void MoveWaypointsToBaseStations();

void AfterLoadVehicles(bool part_of_load);
void FixupTrainLengths();
void AfterLoadStations();
void AfterLoadRoadStops();
void ResetLabelMaps();
void AfterLoadLabelMaps();
void AfterLoadStoryBook();
void AfterLoadLinkGraphs();
void AfterLoadCompanyStats();
void UpdateHousesAndTowns();

void UpdateOldAircraft();

void SaveViewportBeforeSaveGame();
void ResetViewportAfterLoadGame();

void ConvertOldMultiheadToNew();
void ConnectMultiheadedTrains();

void ResetTempEngineData();
Engine *GetTempDataEngine(EngineID index);
void CopyTempEngineData();

extern int32_t _saved_scrollpos_x;
extern int32_t _saved_scrollpos_y;
extern ZoomLevel _saved_scrollpos_zoom;

extern SavegameType _savegame_type;
extern uint32_t _ttdp_version;

CompanyManagerFace ConvertFromOldCompanyManagerFace(uint32_t face);

Order UnpackOldOrder(uint16_t packed);

#endif /* SAVELOAD_INTERNAL_H */
