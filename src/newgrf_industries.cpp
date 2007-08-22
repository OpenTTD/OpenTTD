/* $Id$ */

/** @file newgrf_industries.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "landscape.h"
#include "table/strings.h"
#include "industry.h"
#include "industry_map.h"
#include "newgrf.h"
#include "newgrf_callbacks.h"
#include "newgrf_spritegroup.h"
#include "newgrf_industries.h"
#include "newgrf_commons.h"
#include "newgrf_text.h"
#include "newgrf_town.h"
#include "date.h"

/* Since the industry IDs defined by the GRF file don't necessarily correlate
 * to those used by the game, the IDs used for overriding old industries must be
 * translated when the idustry spec is set. */
IndustryOverrideManager _industry_mngr(NEW_INDUSTRYOFFSET, NUM_INDUSTRYTYPES, INVALID_INDUSTRYTYPE);
IndustryTileOverrideManager _industile_mngr(NEW_INDUSTRYTILEOFFSET, NUM_INDUSTRYTILES, INVALID_INDUSTRYTILE);

IndustryType MapNewGRFIndustryType(IndustryType grf_type, uint32 grf_id)
{
	if (grf_type == IT_INVALID) return IT_INVALID;
	if (!HASBIT(grf_type, 7)) return GB(grf_type, 0, 6);

	return _industry_mngr.GetID(GB(grf_type, 0, 6), grf_id);
}

/**
 * Finds the distance for the closest tile with water/land given a tile
 * @param tile  the tile to find the distance too
 * @param water whether to find water or land
 * @note FAILS when an industry should be seen as water
 */
static uint GetClosestWaterDistance(TileIndex tile, bool water)
{
	TileIndex t;
	int best_dist;
	for (t = 0; t < MapSize(); t++) {
		if (IsTileType(t, MP_WATER) == water) break;
	}
	best_dist = DistanceManhattan(tile, t);

	for (; t < MapSize(); t++) {
		int dist = DistanceManhattan(tile, t);
		if (dist < best_dist) {
			if (IsTileType(t, MP_WATER) == water) best_dist = dist;
		} else {
			/* When the Y distance between the current row and the 'source' tile
			 * is larger than the best distance, we've found the best distance */
			if ((int)TileY(t) - (int)TileY(tile) > best_dist) break;
			if (TileX(tile) > TileX(t)) {
				/* We can safely skip this many tiles; from here all tiles have a
				 * higher or equal distance than the best distance */
				t |= MapMaxX();
				continue;
			} else {
				/* We can safely skip this many tiles; up to here all tiles have a
				 * higher or equal distance than the best distance */
				t += max(best_dist - dist, 0);
				continue;
			}
		}
	}

	return best_dist;
}

/** Make an analysis of a tile and check for its belonging to the same
 * industry, and/or the same grf file
 * @param new_tile TileIndex of the tile to query
 * @param old_tile TileIndex of the reference tile
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

static uint32 GetClosestIndustry(TileIndex tile, IndustryType type, const Industry *current)
{
	uint32 best_dist = MAX_UVALUE(uint32);
	const Industry *i;
	FOR_ALL_INDUSTRIES(i) {
		if (i->type != type || i == current) continue;

		best_dist = min(best_dist, DistanceManhattan(tile, i->xy));
	}

	return best_dist;
}

/** Implementation of both var 67 and 68
 * since the mechanism is almost the same, it is easier to regroup them on the same
 * function.
 * @param param_setID parameter given to the callback, which is the set id, or the local id, in our terminology
 * @param layout_filter on what layout do we filter?
 * @param current Industry for which the inquiry is made
 * @return the formatted answer to the callback : rr(reserved) cc(count) dddd(manhattan distance of closest sister)
 */
static uint32 GetCountAndDistanceOfClosestInstance(byte param_setID, byte layout_filter, const Industry *current)
{
	uint32 GrfID = GetRegister(0x100);  ///< Get the GRFID of the definition to look for in register 100h
	IndustryType ind_index;
	uint32 closest_dist = MAX_UVALUE(uint32);
	byte count = 0;

	/* Determine what will be the industry type to look for */
	switch (GrfID) {
		case 0:  // this is a default industry type
			ind_index = param_setID;
			break;

		case 0xFFFFFFFF: // current grf
			ind_index = GetIndustrySpec(current->type)->grf_prop.grffile->grfid;
			/*Fall through*/

		default: //use the grfid specified in register 100h
			ind_index = MapNewGRFIndustryType(param_setID, GrfID);
			break;
	}

	if (layout_filter == 0) {
		/* If the filter is 0, it could be because none was specified as well as being really a 0.
		 * In either case, just do the regular var67 */
		closest_dist = GetClosestIndustry(current->xy, ind_index, current);
		count = GetIndustryTypeCount(ind_index);
	} else {
		/* Count only those who match the same industry type and layout filter
		 * Unfortunately, we have to do it manually */
		const Industry *i;
		FOR_ALL_INDUSTRIES(i) {
			if (i->type == ind_index && i != current && i->selected_layout == layout_filter) {
				closest_dist = min(closest_dist, DistanceManhattan(current->xy, i->xy));
				count++;
			}
		}
	}

	return count << 16 | GB(closest_dist, 0, 16);
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
	TileIndex tile = object->u.industry.tile;
	const IndustrySpec *indspec = GetIndustrySpec(industry->type);

	switch (variable) {
		case 0x40:
		case 0x41:
		case 0x42: { // waiting cargo, but only if those two callback flags are set
			uint16 callback = indspec->callback_flags;
			if (HASBIT(callback, CBM_IND_PRODUCTION_CARGO_ARRIVAL) || HASBIT(callback, CBM_IND_PRODUCTION_256_TICKS)) {
				return min(industry->incoming_cargo_waiting[variable - 0x40], (uint16)0xFFFF);
			} else {
				return 0;
			}
		}

		/* Manhattan distance of closes dry/water tile */
		case 0x43: return GetClosestWaterDistance(tile, (object->u.industry_location.spec->behaviour & INDUSTRYBEH_BUILT_ONWATER) == 0);

		/* Layout number */
		case 0x44: return industry->selected_layout;

		/* Get industry ID at offset param */
		case 0x60: return GetIndustryIDAtOffset(GetNearbyTile(parameter, industry->xy), tile, industry);

		case 0x61: return 0; // Get random tile bits at offset param

		/* Land info of nearby tiles */
		case 0x62: return GetNearbyIndustryTileInformation(parameter, tile, INVALID_INDUSTRY);

		/* Animation stage of nearby tiles */
		case 0x63: {
			tile = GetNearbyTile(parameter, tile);
			if (IsTileType(tile, MP_INDUSTRY) && GetIndustryByTile(tile) == industry) {
				return GetIndustryAnimationState(tile);
			}
			return 0xFFFFFFFF;
		}

		/* Distance of nearest industry of given type */
		case 0x64: return GetClosestIndustry(tile, MapNewGRFIndustryType(parameter, indspec->grf_prop.grffile->grfid), industry);
		/* Get town zone and Manhattan distance of closest town */
 		case 0x65: return GetTownRadiusGroup(industry->town, tile) << 16 | min(DistanceManhattan(tile, industry->town->xy), 0xFFFF);
		/* Get square of Euclidian distance of closes town */
		case 0x66: return GetTownRadiusGroup(industry->town, tile) << 16 | min(DistanceSquare(tile, industry->town->xy), 0xFFFF);

		/* Count of industry, distance of closest instance
		 * 68 is the same as 67, but with a filtering on selected layout */
		case 0x67:
		case 0x68: return GetCountAndDistanceOfClosestInstance(parameter, variable == 0x68 ? GB(GetRegister(0x101), 0, 8) : 0, industry);

		/* Industry structure access*/
		case 0x80: return industry->xy;
		case 0x81: return GB(industry->xy, 8, 8);
		/* Pointer to the town the industry is associated with */
		case 0x82: return industry->town->index;
		case 0x83:
		case 0x84:
		case 0x85: DEBUG(grf, 0, "NewGRFs shouldn't be doing pointer magic"); break; // not supported
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
		case 0xA7: return industry->founder;
		case 0xA8: return industry->random_color;
		case 0xA9: return clamp(0, industry->last_prod_year - 1920, 255);
		case 0xAA: return industry->counter;
		case 0xAB: return GB(industry->counter, 8, 8);
		case 0xAC: return industry->was_cargo_delivered;

		case 0xB0: return clamp(0, industry->construction_date - DAYS_TILL_ORIGINAL_BASE_YEAR, 65535); // Date when built since 1920 (in days)
		case 0xB3: return industry->construction_type; // Construction type
		case 0xB4: return clamp(0, industry->last_cargo_accepted_at - DAYS_TILL_ORIGINAL_BASE_YEAR, 65535); // Date last cargo accepted since 1920 (in days)
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

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
}

uint16 GetIndustryCallback(CallbackID callback, uint32 param1, uint32 param2, Industry *industry, IndustryType type, TileIndex tile)
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

uint32 IndustryLocationGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	TileIndex tile = object->u.industry_location.tile;

	if (object->scope == VSG_SCOPE_PARENT) {
		return TownGetVariable(variable, parameter, available, ClosestTownFromTile(tile, (uint)-1));
	}

	switch (variable) {
		/* Land info of nearby tiles */
		case 0x62: return GetNearbyIndustryTileInformation(parameter, tile, INVALID_INDUSTRY);

		/* Distance of nearest industry of given type */
		case 0x64: return GetClosestIndustry(tile, MapNewGRFIndustryType(parameter, object->u.industry_location.spec->grf_prop.grffile->grfid), NULL);

		/* Location where to build the industry */
		case 0x80: return tile;
		case 0x81: return GB(tile, 8, 8);

		/* Pointer to the town the industry is associated with */
		case 0x82: return ClosestTownFromTile(tile, (uint)-1)->index;
		case 0x83:
		case 0x84:
		case 0x85: DEBUG(grf, 0, "NewGRFs shouldn't be doing pointer magic"); break; // not supported

		/* Number of the layout */
		case 0x86: return object->u.industry_location.itspec_index;

		/* Ground type */
		case 0x87: return GetTerrainType(tile);

		/* Town zone */
		case 0x88: return GetTownRadiusGroup(ClosestTownFromTile(tile, (uint)-1), tile);

		/* Manhattan distance of the closest town */
		case 0x89: return min(DistanceManhattan(ClosestTownFromTile(tile, (uint)-1)->xy, tile), 255);

		/* Lowest height of the tile */
		case 0x8A: return GetTileZ(tile);

		/* Distance to the nearest water/land tile */
		case 0x8B: return GetClosestWaterDistance(tile, (object->u.industry_location.spec->behaviour & INDUSTRYBEH_BUILT_ONWATER) == 0);

		/* Square of Euclidian distance from town */
		case 0x8D: return min(DistanceSquare(ClosestTownFromTile(tile, (uint)-1)->xy, tile), 65535);
	}

	DEBUG(grf, 1, "Unhandled location industry property 0x%X", variable);

	*available = false;
	return (uint32)-1;
}

bool CheckIfCallBackAllowsCreation(TileIndex tile, IndustryType type, uint itspec_index)
{
	const IndustrySpec *indspec = GetIndustrySpec(type);

	ResolverObject object;
	const SpriteGroup *group;

	NewIndustryResolver(&object, tile, NULL);
	object.GetVariable = IndustryLocationGetVariable;
	object.callback = CBID_INDUSTRY_LOCATION;
	object.u.industry_location.tile = tile;
	object.u.industry_location.spec = indspec;
	object.u.industry_location.itspec_index = itspec_index;

	group = Resolve(GetIndustrySpec(type)->grf_prop.spritegroup, &object);

	if (group == NULL || group->type != SGT_CALLBACK) return false;

	switch (group->g.callback.result) {
		case 0x400: return true;
		case 0x401: _error_message = STR_0239_SITE_UNSUITABLE; break;
		case 0x402: _error_message = STR_0317_CAN_ONLY_BE_BUILT_IN_RAINFOREST; break;
		case 0x403: _error_message = STR_0318_CAN_ONLY_BE_BUILT_IN_DESERT; break;
		default: _error_message = GetGRFStringID(indspec->grf_prop.grffile->grfid, 0xD000 + group->g.callback.result); break;
	}

	return false;
}

bool CheckIfCallBackAllowsAvailability(IndustryType type, IndustryAvailabilityCallType creation_type)
{
	const IndustrySpec *indspec = GetIndustrySpec(type);

	if (HASBIT(indspec->callback_flags, CBM_IND_AVAILABLE)) {
		uint16 res = GetIndustryCallback(CBID_INDUSTRY_AVAILABLE, 0, creation_type, NULL, type, INVALID_TILE);
		if (res != CALLBACK_FAILED) {
			return (res == 0);
		}
	}
	return true;
}

static int32 DerefIndProd(uint field, bool use_register)
{
	return use_register ? (int32)GetRegister(field) : field;
}

/**
 * Get the industry production callback and apply it to the industry.
 * @param ind    the industry this callback has to be called for
 * @param reason the reason it is called (0 = incoming cargo, 1 = periodic tick callback)
 */
void IndustryProductionCallback(Industry *ind, int reason)
{
	ResolverObject object;
	NewIndustryResolver(&object, ind->xy, ind);
	object.callback_param2 = reason;

	for (uint loop = 0;; loop++) {
		SB(object.callback_param2, 8, 16, loop);
		const SpriteGroup *group = Resolve(GetIndustrySpec(ind->type)->grf_prop.spritegroup, &object);
		if (group == NULL || group->type != SGT_INDUSTRY_PRODUCTION) break;

		bool deref = (group->g.indprod.version == 1);

		for (uint i = 0; i < 3; i++) {
			ind->incoming_cargo_waiting[i] = clamp(ind->incoming_cargo_waiting[i] - DerefIndProd(group->g.indprod.substract_input[i], deref), 0, 0xFFFF);
		}
		for (uint i = 0; i < 2; i++) {
			ind->produced_cargo_waiting[i] = clamp(ind->produced_cargo_waiting[i] + DerefIndProd(group->g.indprod.add_output[i], deref), 0, 0xFFFF);
		}

		int32 again = DerefIndProd(group->g.indprod.again, deref);
		if (again == 0) break;

		SB(object.callback_param2, 24, 8, again);
	}

	InvalidateWindow(WC_INDUSTRY_VIEW, ind->index);
}
