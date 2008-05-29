/* $Id$ */

/** @file elrail_func.h header file for electrified rail specific functions */

#ifndef ELRAIL_FUNC_H
#define ELRAIL_FUNC_H

#include "rail.h"
#include "transparency.h"
#include "tile_cmd.h"
#include "settings_type.h"

/**
 * Test if a rail type has catenary
 * @param rt Rail type to test
 */
static inline bool HasCatenary(RailType rt)
{
	return HasBit(GetRailTypeInfo(rt)->flags, RTF_CATENARY);
}

/**
 * Test if we should draw rail catenary
 * @param rt Rail type to test
 */
static inline bool HasCatenaryDrawn(RailType rt)
{
	return HasCatenary(rt) && !IsInvisibilitySet(TO_CATENARY) && !_settings_game.vehicle.disable_elrails;
}

/**
 * Draws overhead wires and pylons for electric railways.
 * @param ti The TileInfo struct of the tile being drawn
 * @see DrawCatenaryRailway
 */
void DrawCatenary(const TileInfo *ti);
void DrawCatenaryOnTunnel(const TileInfo *ti);
void DrawCatenaryOnBridge(const TileInfo *ti);

int32 SettingsDisableElrail(int32 p1); ///< _settings_game.disable_elrail callback

#endif /* ELRAIL_FUNC_H */
