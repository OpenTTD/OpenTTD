/* $Id$ */

/** @file newgrf_industries.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "macros.h"
#include "industry.h"
#include "industry_map.h"
#include "newgrf.h"
#include "newgrf_callbacks.h"
#include "newgrf_spritegroup.h"
#include "newgrf_industries.h"
#include "newgrf_commons.h"

/* Since the industry IDs defined by the GRF file don't necessarily correlate
 * to those used by the game, the IDs used for overriding old industries must be
 * translated when the idustry spec is set. */
IndustryOverrideManager _industry_mngr(NEW_INDUSTRYOFFSET, NUM_INDUSTRYTYPES, INVALID_INDUSTRYTYPE);
IndustryTileOverrideManager _industile_mngr(NEW_INDUSTRYTILEOFFSET, NUM_INDUSTRYTILES, INVALID_INDUSTRYTILE);

/**
 * Finds the distance for the closest tile with water/land given a tile
 * @param tile  the tile to find the distance too
 * @param water whether to find water or land
 * @note FAILS when an industry should be seen as water
 */
static uint GetClosestWaterDistance(TileIndex tile, bool water)
{
	TileIndex t;
	uint best_dist;
	for (t = 1; t < MapSize(); t++) {
		if (IsTileType(t, MP_WATER) == water) break;
	}
	best_dist = DistanceManhattan(tile, t);

	for (; t < MapSize(); t++) {
		uint dist = DistanceManhattan(tile, t);
		if (dist < best_dist) {
			if (IsTileType(t, MP_WATER) == water) best_dist = dist;
		} else {
			/* When the Y distance between the current row and the 'source' tile
			 * is larger than the best distance, we've found the best distance */
			if (TileY(t) - TileY(tile) > best_dist) return best_dist;
			if (TileX(tile) > TileX(t)) {
				/* We can safely skip this many tiles; from here all tiles have a
				 * higher or equal distance than the best distance */
				t |= MapMaxX();
				continue;
			} else {
				/* We can safely skip this many tiles; up to here all tiles have a
				 * higher or equal distance than the best distance */
				t += best_dist - dist;
				continue;
			}
		}
	}

	return best_dist;
}

/** Make an analysis of a tile and check for its belonging to the same
 * industry, and/or the same grf file
 * @param new_tile TileIndex of the tile to query
 * @param old_tile TileINdex of teh reference tile
 * @param i Industry to which old_tile belongs to
 * @return value encoded as per NFO specs */
uint32 GetIndustryIDAtOffset(TileIndex new_tile, TileIndex old_tile, const Industry *i)
{
	if (IsTileType(new_tile, MP_INDUSTRY)) {  // Is this an industry tile?

		if (GetIndustryIndex(new_tile) == i->index) {  // Does it belong to the same industry?
			IndustryGfx gfx = GetIndustryGfx(new_tile);
			const IndustryTileSpec *indtsp = GetIndustryTileSpec(gfx);
			const IndustryTileSpec *indold = GetIndustryTileSpec(GetIndustryGfx(old_tile));

			if (gfx < NEW_INDUSTRYOFFSET) {  // Does it belongs to an old type?
				/* It is an old tile.  We have to see if it's been overriden */
				if (indtsp->grf_prop.override == INVALID_INDUSTRYTILE) {  // has it been overridden?
					return 0xFF << 8 | gfx; // no. Tag FF + the gfx id of that tile
				} else { // yes.  FInd out if it is from the same grf file or not
					const IndustryTileSpec *old_tile_ovr = GetIndustryTileSpec(indtsp->grf_prop.override);

					if (old_tile_ovr->grf_prop.grffile->grfid == indold->grf_prop.grffile->grfid) {
						return old_tile_ovr->grf_prop.local_id; // same grf file
					} else {
						return 0xFFFE; // not the same grf file
					}
				}
			} else {
				if (indtsp->grf_prop.spritegroup != NULL) {  // tile has a spritegroup ?
					if (indtsp->grf_prop.grffile->grfid == indold->grf_prop.grffile->grfid) {  // same industry, same grf ?
						return indtsp->grf_prop.local_id;
					} else {
						return 0xFFFE;  // Defined in another grf file
					}
				} else {  // tile has no spritegroup
					return 0xFF << 8 | indtsp->grf_prop.subst_id;  // so just give him the substitute
				}
			}
		}
	}

	return 0xFFFF; // tile is not an industry one or  does not belong to the current industry
}

/** This function implements the industries variables that newGRF defines.
 * @param variable that is queried
 * @param parameter unused
 * @param available will return false if ever the variable asked for does not exist
 * @param ind is of course the industry we are inquiring
 * @return the value stored in the corresponding variable*/
uint32 IndustryGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	const Industry *industry = object->u.industry.ind;
	TileIndex tile   = object->u.industry.tile;
	const IndustrySpec *indspec = GetIndustrySpec(industry->type);

	switch (variable) {
		case 0x40:
		case 0x41:
		case 0x42: { // waiting cargo, but only if those two callback flags are set
			uint16 callback = indspec->callback_flags;
			if (callback & (CBM_IND_PRODUCTION_CARGO_ARRIVAL | CBM_IND_PRODUCTION_256_TICKS)) {
				return max(industry->incoming_cargo_waiting[variable - 0x40], (uint16)0x7FFF);
			} else {
				return 0;
			}
		}
		/* TODO: somehow determine whether we're in water or not */
		case 0x43: return GetClosestWaterDistance(tile, true); // Manhattan distance of closes dry/water tile

		/* Get industry ID at offset param */
		case 0x60: return GetIndustryIDAtOffset(GetNearbyTile(parameter, industry->xy), tile, industry);

		case 0x61: return 0; // Get random tile bits at offset param

		case 0x62: // Land info of nearby tiles
		case 0x63: // Animation stage of nerby tiles
		case 0x64: break; // Distance of nearest industry of given type
		/* Get town zone and Manhattan distance of closest town */
 		case 0x65: return GetTownRadiusGroup(industry->town, tile) << 16 | min(DistanceManhattan(tile, industry->town->xy), 0xFFFF);
		/* Get square of Euclidian distance of closes town */
		case 0x66: return GetTownRadiusGroup(industry->town, tile) << 16 | min(DistanceSquare(tile, industry->town->xy), 0xFFFF);

		/* Count of industry, distance of closest instance
		 * format is rr(reserved) cc(count)  dddd(manhattan distance of closest sister)
		 * A lot more should be done, since it has to check for local id, grf id etc...
		 * let's just say it is a beginning ;) */
		case 0x67: return GetIndustryTypeCount(industry->type) << 16 | 0;

		/* Industry founder information.
		 * 0x10 if randomly created or from a map pre-newindustry.
		 * Else, the company who funded it */
		case 0xA7: return 0x10;

		case 0xB0: // Date when built since 1920 (in days)
		case 0xB3: // Construction type
		case 0xB4: break; // Date last cargo accepted since 1920 (in days)

		/* Industry structure access*/
		case 0x80: return industry->xy;
		case 0x81: return GB(industry->xy, 8, 8);
		/* Pointer to the town the industry is associated with */
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85: break; // not supported
		case 0x86: return industry->width;
		case 0x87: return industry->height;// xy dimensions
		/*  */
		case 0x88:
		case 0x89: return indspec->produced_cargo[variable - 0x88];
		case 0x8A: return industry->produced_cargo_waiting[0];
		case 0x8B: return GB(industry->produced_cargo_waiting[0], 8, 8);
		case 0x8C: return industry->produced_cargo_waiting[1];
		case 0x8D: return GB(industry->produced_cargo_waiting[1], 8, 8);
		case 0x8E:
		case 0x8F: return industry->production_rate[variable - 0x8E];
		case 0x90:
		case 0x91:
		case 0x92: return indspec->accepts_cargo[variable - 0x90];
		case 0x93: return industry->prod_level;
		/* amount of cargo produced so far THIS month. */
		case 0x94: return industry->this_month_production[0];
		case 0x95: return GB(industry->this_month_production[0], 8, 8);
		case 0x96: return industry->this_month_production[1];
		case 0x97: return GB(industry->this_month_production[1], 8, 8);
		/* amount of cargo transported so far THIS month. */
		case 0x98: return industry->this_month_transported[0];
		case 0x99: return GB(industry->this_month_transported[0], 8, 8);
		case 0x9A: return industry->this_month_transported[1];
		case 0x9B: return GB(industry->this_month_transported[0], 8, 8);
		/* fraction of cargo transported LAST month. */
		case 0x9C:
		case 0x9D: return industry->last_month_pct_transported[variable - 0x9C];
		/* amount of cargo produced LAST month. */
		case 0x9E: return industry->last_month_production[0];
		case 0x9F: return GB(industry->last_month_production[0], 8, 8);
		case 0xA0: return industry->last_month_production[1];
		case 0xA1: return GB(industry->last_month_production[1], 8, 8);
		/* amount of cargo transported last month. */
		case 0xA2: return industry->last_month_transported[0];
		case 0xA3: return GB(industry->last_month_transported[0], 8, 8);
		case 0xA4: return industry->last_month_transported[1];
		case 0xA5: return GB(industry->last_month_transported[0], 8, 8);

		case 0xA6: return industry->type;

		case 0xA8: return industry->random_color;
		case 0xA9: return industry->last_prod_year; // capped?
		case 0xAA: return industry->counter;
		case 0xAB: return GB(industry->counter, 8, 8);
		case 0xAC: return industry->was_cargo_delivered;
	}

	DEBUG(grf, 1, "Unhandled industry property 0x%X", variable);

	*available = false;
	return (uint32)-1;
}

static const SpriteGroup *IndustryResolveReal(const ResolverObject *object, const SpriteGroup *group)
{
	/* IndustryTile do not have 'real' groups */
	return NULL;
}

static void NewIndustryResolver(ResolverObject *res, TileIndex tile, Industry *indus)
{
	res->GetRandomBits = IndustryTileGetRandomBits;
	res->GetTriggers   = IndustryTileGetTriggers;
	res->SetTriggers   = IndustryTileSetTriggers;
	res->GetVariable   = IndustryGetVariable;
	res->ResolveReal   = IndustryResolveReal;

	res->u.industry.tile = tile;
	res->u.industry.ind  = indus;
	res->u.industry.gfx  = INVALID_INDUSTRYTILE;

	res->callback        = 0;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
}

uint16 GetIndustryCallback(uint16 callback, uint32 param1, uint32 param2, Industry *industry, IndustryType type, TileIndex tile)
{
	ResolverObject object;
	const SpriteGroup *group;

	NewIndustryResolver(&object, tile, industry);
	object.callback = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	group = Resolve(GetIndustrySpec(type)->grf_prop.spritegroup, &object);
	if (group == NULL || group->type != SGT_CALLBACK) return CALLBACK_FAILED;

	return group->g.callback.result;
}
