/* $Id$ */

#ifndef NEWGRF_RAILTYPE_H
#define NEWGRF_RAILTYPE_H

#include "rail.h"
#include "newgrf_commons.h"

SpriteID GetCustomRailSprite(const RailtypeInfo *rti, TileIndex tile, RailTypeSpriteGroup rtsg, TileContext context = TCX_NORMAL);

uint8 GetReverseRailTypeTranslation(RailType railtype, const GRFFile *grffile);

#endif /* NEWGRF_RAILTYPE_H */
