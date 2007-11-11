/* $Id$ */

/** @file newgrf_house.h */

#ifndef NEWGRF_HOUSE_H
#define NEWGRF_HOUSE_H

#include "town.h"
#include "newgrf_callbacks.h"

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

void CheckHouseIDs();

HouseClassID AllocateHouseClassID(byte grf_class_id, uint32 grfid);

void InitializeBuildingCounts();
void IncreaseBuildingCount(Town *t, HouseID house_id);
void DecreaseBuildingCount(Town *t, HouseID house_id);
void AfterLoadCountBuildings();

void DrawNewHouseTile(TileInfo *ti, HouseID house_id);
void AnimateNewHouseTile(TileIndex tile);
void ChangeHouseAnimationFrame(TileIndex tile, uint16 callback_result);

uint16 GetHouseCallback(CallbackID callback, uint32 param1, uint32 param2, HouseID house_id, Town *town, TileIndex tile);

bool CanDeleteHouse(TileIndex tile);

bool NewHouseTileLoop(TileIndex tile);

enum HouseTrigger {
	/* The tile of the house has been triggered during the tileloop. */
	HOUSE_TRIGGER_TILE_LOOP     = 0x01,
	/*
	 * The top tile of a (multitile) building has been triggered during and all
	 * the tileloop other tiles of the same building get the same random value.
	 */
	HOUSE_TRIGGER_TILE_LOOP_TOP = 0x02,
};
void TriggerHouse(TileIndex t, HouseTrigger trigger);

#endif /* NEWGRF_HOUSE_H */
