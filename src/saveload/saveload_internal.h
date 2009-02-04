/* $Id$ */

/** @file saveload_internal.h Declaration of functions used in more save/load files */

#ifndef SAVELOAD_INTERNAL_H
#define SAVELOAD_INTERNAL_H

#include "../strings_type.h"
#include "../company_manager_face.h"
#include "../order_base.h"
#include "../engine_type.h"
#include "saveload.h"

void InitializeOldNames();
StringID RemapOldStringID(StringID s);
char *CopyFromOldName(StringID id);
void ResetOldNames();

void FixOldWaypoints();

void AfterLoadWaypoints();
void AfterLoadVehicles(bool part_of_load);
void AfterLoadStations();
void UpdateHousesAndTowns();

void UpdateOldAircraft();

void SaveViewportBeforeSaveGame();
void ResetViewportAfterLoadGame();

void ConvertOldMultiheadToNew();
void ConnectMultiheadedTrains();

Engine *GetTempDataEngine(EngineID index);
void CopyTempEngineData();

extern int32 _saved_scrollpos_x;
extern int32 _saved_scrollpos_y;

extern SavegameType _savegame_type;
extern uint32 _ttdp_version;

CompanyManagerFace ConvertFromOldCompanyManagerFace(uint32 face);

Order UnpackOldOrder(uint16 packed);

#endif /* SAVELOAD_INTERNAL_H */
