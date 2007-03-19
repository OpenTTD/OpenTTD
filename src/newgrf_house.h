/* $Id$ */

/** @file newgrf_house.h */

#ifndef NEWGRF_HOUSE_H
#define NEWGRF_HOUSE_H

#include "town.h"

/**
 * Maps a house id stored on the map to a GRF file.
 * House IDs are stored on the map, so there needs to be a way to tie them to
 * GRF files. An array of HouseIDMapping structs is saved with the savegame so
 * that house GRFs can be loaded in a different order, or removed safely. The
 * index in the array is the house ID stored on the map.
 *
 * The substitute ID is the ID of an original house that should be used instead
 * if the GRF containing the new house is not available.
 */
struct HouseIDMapping {
	uint32 grfid;          ///< The GRF ID of the file this house belongs to
	uint8  house_id;       ///< The house ID within the GRF file
	uint8  substitute_id;  ///< The (original) house ID to use if this GRF is not available
};

/**
 * Makes class IDs unique to each GRF file.
 * Houses can be assigned class IDs which are only comparable within the GRF
 * file they were defined in. This mapping ensures that if two houses have the
 * same class as defined by the GRF file, the classes are different within the
 * game. An array of HouseClassMapping structs is created, and the array index
 * of the struct that matches both the GRF ID and the class ID is the class ID
 * used in the game.
 *
 * Although similar to the HouseIDMapping struct above, this serves a different
 * purpose. Since the class ID is not saved anywhere, this mapping does not
 * need to be persistent; it just needs to keep class ids unique.
 */
struct HouseClassMapping {
	uint32 grfid;     ////< The GRF ID of the file this class belongs to
	uint8  class_id;  ////< The class id within the grf file
};

extern HouseIDMapping _house_id_mapping[HOUSE_MAX]; ///< Declared in newgrf_house.cpp

void AddHouseOverride(uint8 local_id, uint house_type);
void ResetHouseOverrides();

void SetHouseSpec(const HouseSpec *hs);

void CheckHouseIDs();
void ResetHouseIDMapping();

HouseClassID AllocateHouseClassID(byte grf_class_id, uint32 grfid);

void InitializeBuildingCounts();
void IncreaseBuildingCount(Town *t, HouseID house_id);
void DecreaseBuildingCount(Town *t, HouseID house_id);
void AfterLoadCountBuildings();

void DrawNewHouseTile(TileInfo *ti, HouseID house_id);
void AnimateNewHouseTile(TileIndex tile);
void ChangeHouseAnimationFrame(TileIndex tile, uint16 callback_result);

uint16 GetHouseCallback(uint16 callback, uint32 param1, HouseID house_id, Town *town, TileIndex tile);

bool CanDeleteHouse(TileIndex tile);

bool NewHouseTileLoop(TileIndex tile);

#endif /* NEWGRF_HOUSE_H */
