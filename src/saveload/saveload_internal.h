/* $Id$ */

/** @file saveload_internal.h Declaration of functions used in more save/load files */

#ifndef SAVELOAD_INTERNAL_H
#define SAVELOAD_INTERNAL_H

#include "../strings_type.h"
#include "../company_manager_face.h"
#include "../order_base.h"

void InitializeOldNames();
StringID RemapOldStringID(StringID s);
char *CopyFromOldName(StringID id);
void ResetOldNames();

void FixOldWaypoints();

void AfterLoadWaypoints();
void AfterLoadVehicles(bool part_of_load);
void AfterLoadStations();
void AfterLoadTown();
void UpdateHousesAndTowns();

void UpdateOldAircraft();

void SaveViewportBeforeSaveGame();
void ResetViewportAfterLoadGame();

void ConvertOldMultiheadToNew();
void ConnectMultiheadedTrains();

extern int32 _saved_scrollpos_x;
extern int32 _saved_scrollpos_y;

CompanyManagerFace ConvertFromOldCompanyManagerFace(uint32 face);

Order UnpackOldOrder(uint16 packed);

#endif /* SAVELOAD_INTERNAL_H */
