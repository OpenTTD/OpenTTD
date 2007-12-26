/* $Id$ */

/** @file newgrf_industries.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "variables.h"
#include "landscape.h"
#include "table/strings.h"
#include "industry.h"
#include "industry_map.h"
#include "newgrf.h"
#include "newgrf_callbacks.h"
#include "newgrf_spritegroup.h"
#include "newgrf_industries.h"
#include "newgrf_industrytiles.h"
#include "newgrf_commons.h"
#include "newgrf_text.h"
#include "newgrf_town.h"
#include "window_func.h"

/* Since the industry IDs defined by the GRF file don't necessarily correlate
 * to those used by the game, the IDs used for overriding old industries must be
 * translated when the idustry spec is set. */
IndustryOverrideManager _industry_mngr(NEW_INDUSTRYOFFSET, NUM_INDUSTRYTYPES, INVALID_INDUSTRYTYPE);
IndustryTileOverrideManager _industile_mngr(NEW_INDUSTRYTILEOFFSET, NUM_INDUSTRYTILES, INVALID_INDUSTRYTILE);

IndustryType MapNewGRFIndustryType(IndustryType grf_type, uint32 grf_id)
{
	if (grf_type == IT_INVALID) return IT_INVALID;
	if (!HasBit(grf_type, 7)) return GB(grf_type, 0, 6);

	return _industry_mngr.GetID(GB(grf_type, 0, 6), grf_id);
}

static uint32 GetGRFParameter(IndustryType ind_id, byte parameter)
{
	const IndustrySpec *indspec = GetIndustrySpec(ind_id);
	const GRFFile *file = indspec->grf_prop.grffile;

	if (parameter >= file->param_end) return 0;
	return file->param[parameter];
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
	if (t == MapSize() && !water) return 0x200;
	best_dist = DistanceManhattan(tile, t);

	for (; t < MapSize(); t++) {
		int dist = DistanceManhattan(tile, t);
		if (dist < best_dist) {
			if (IsTileType(t, MP_WATER) == water) best_dist = dist;
		} else {
			/* When the Y distance between the current row and the 'source' tile
			 * is larger than the best distance, we've found the best distance */
			if ((int)TileY(t) - (int)TileY(tile) > best_dist) break;
			if ((int)TileX(t) - (int)TileX(tile) > best_dist) {
				/* We can safely skip this many tiles; from here all tiles have a
				 * higher or equal distance than the best distance */
				t |= MapMaxX();
				continue;
			} else if (TileX(tile) < TileX(t)) {
				/* We can safely skip this many tiles; up to here all tiles have a
				 * higher or equal distance than the best distance */
				t += max(best_dist - dist, 0);
				continue;
			}
		}
	}

	return min(best_dist, water ? 0x7F : 0x1FF);
}

/** Make an analysis of a tile and check for its belonging to the same
 * industry, and/or the same grf file
 * @param tile TileIndex of the tile to query
 * @param i Industry to which to compare the tile to
 * @return value encoded as per NFO specs */
uint32 GetIndustryIDAtOffset(TileIndex tile, const Industry *i)
{
	if (!IsTileType(tile, MP_INDUSTRY) || GetIndustryIndex(tile) != i->index) {
		/* No industry and/or the tile does not have the same industry as the one we match it with */
		return 0xFFFF;
	}

	IndustryGfx gfx = GetCleanIndustryGfx(tile);
	const IndustryTileSpec *indtsp = GetIndustryTileSpec(gfx);
	const IndustrySpec *indold = GetIndustrySpec(i->type);

	if (gfx < NEW_INDUSTRYOFFSET) { // Does it belongs to an old type?
		/* It is an old tile.  We have to see if it's been overriden */
		if (indtsp->grf_prop.override == INVALID_INDUSTRYTILE) { // has it been overridden?
			return 0xFF << 8 | gfx; // no. Tag FF + the gfx id of that tile
		}
		/* Not overriden */
		const IndustryTileSpec *tile_ovr = GetIndustryTileSpec(indtsp->grf_prop.override);

		if (tile_ovr->grf_prop.grffile->grfid == indold->grf_prop.grffile->grfid) {
			return tile_ovr->grf_prop.local_id; // same grf file
		} else {
			return 0xFFFE; // not the same grf file
		}
	}
	/* Not an 'old type' tile */
	if (indtsp->grf_prop.spritegroup != NULL) { // tile has a spritegroup ?
		if (indtsp->grf_prop.grffile->grfid == indold->grf_prop.grffile->grfid) { // same industry, same grf ?
			return indtsp->grf_prop.local_id;
		} else {
			return 0xFFFE; // Defined in another grf file
		}
	}
	/* The tile has no spritegroup */
	return 0xFF << 8 | indtsp->grf_prop.subst_id; // so just give him the substitute
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
			GrfID = GetIndustrySpec(current->type)->grf_prop.grffile->grfid;
			/* Fall through */

		default: //use the grfid specified in register 100h
			SetBit(param_setID, 7); // bit 7 means it is not an old type
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
			if (HasBit(callback, CBM_IND_PRODUCTION_CARGO_ARRIVAL) || HasBit(callback, CBM_IND_PRODUCTION_256_TICKS)) {
				if ((indspec->behaviour & INDUSTRYBEH_PROD_MULTI_HNDLING) != 0) {
					return min(industry->incoming_cargo_waiting[variable - 0x40] / industry->prod_level, (uint16)0xFFFF);
				} else {
					return min(industry->incoming_cargo_waiting[variable - 0x40], (uint16)0xFFFF);
				}
			} else {
				return 0;
			}
		}

		/* Manhattan distance of closes dry/water tile */
		case 0x43: return GetClosestWaterDistance(tile, (indspec->behaviour & INDUSTRYBEH_BUILT_ONWATER) == 0);

		/* Layout number */
		case 0x44: return industry->selected_layout;

		/* player info */
		case 0x45: {
			byte colours;
			bool is_ai = false;

			if (IsValidPlayer(industry->founder)) {
				const Player *p = GetPlayer(industry->founder);
				const Livery *l = &p->livery[LS_DEFAULT];

				is_ai = p->is_ai;
				colours = l->colour1 + l->colour2 * 16;
			} else {
				colours = GB(Random(), 0, 8);
			}

			return industry->founder | (is_ai ? 0x10000 : 0) | (colours << 24);
		}

		/* Get industry ID at offset param */
		case 0x60: return GetIndustryIDAtOffset(GetNearbyTile(parameter, industry->xy), industry);

		/* Get random tile bits at offset param */
		case 0x61:
			tile = GetNearbyTile(parameter, tile);
			return (IsTileType(tile, MP_INDUSTRY) && GetIndustryByTile(tile) == industry) ? GetIndustryRandomBits(tile) : 0;

		/* Land info of nearby tiles */
		case 0x62: return GetNearbyIndustryTileInformation(parameter, tile, INVALID_INDUSTRY);

		/* Animation stage of nearby tiles */
		case 0x63:
			tile = GetNearbyTile(parameter, tile);
			if (IsTileType(tile, MP_INDUSTRY) && GetIndustryByTile(tile) == industry) {
				return GetIndustryAnimationState(tile);
			}
			return 0xFFFFFFFF;

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

		/* Get a variable from the persistent storage */
		case 0x7C: return industry->psa.Get(parameter);

		/* Read GRF parameter */
		case 0x7F: return GetGRFParameter(industry->type, parameter);

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
		case 0x89: return industry->produced_cargo[variable - 0x88];
		case 0x8A: return industry->produced_cargo_waiting[0];
		case 0x8B: return GB(industry->produced_cargo_waiting[0], 8, 8);
		case 0x8C: return industry->produced_cargo_waiting[1];
		case 0x8D: return GB(industry->produced_cargo_waiting[1], 8, 8);
		case 0x8E:
		case 0x8F: return industry->production_rate[variable - 0x8E];
		case 0x90:
		case 0x91:
		case 0x92: return industry->accepts_cargo[variable - 0x90];
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
		case 0xA9: return Clamp(0, industry->last_prod_year - 1920, 255);
		case 0xAA: return industry->counter;
		case 0xAB: return GB(industry->counter, 8, 8);
		case 0xAC: return industry->was_cargo_delivered;

		case 0xB0: return Clamp(0, industry->construction_date - DAYS_TILL_ORIGINAL_BASE_YEAR, 65535); // Date when built since 1920 (in days)
		case 0xB3: return industry->construction_type; // Construction type
		case 0xB4: return Clamp(0, industry->last_cargo_accepted_at - DAYS_TILL_ORIGINAL_BASE_YEAR, 65535); // Date last cargo accepted since 1920 (in days)
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

static uint32 IndustryGetRandomBits(const ResolverObject *object)
{
	return object->u.industry.ind == NULL ? 0 : object->u.industry.ind->random;
}

static uint32 IndustryGetTriggers(const ResolverObject *object)
{
	return object->u.industry.ind == NULL ? 0 : object->u.industry.ind->random_triggers;
}

static void IndustrySetTriggers(const ResolverObject *object, int triggers)
{
	if (object->u.industry.ind == NULL) return;
	object->u.industry.ind->random_triggers = triggers;
}

static void NewIndustryResolver(ResolverObject *res, TileIndex tile, Industry *indus)
{
	res->GetRandomBits = IndustryGetRandomBits;
	res->GetTriggers   = IndustryGetTriggers;
	res->SetTriggers   = IndustrySetTriggers;
	res->GetVariable   = IndustryGetVariable;
	res->ResolveReal   = IndustryResolveReal;

	res->psa             = &indus->psa;
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
	const Industry *industry = object->u.industry.ind;
	TileIndex tile = object->u.industry.tile;

	if (object->scope == VSG_SCOPE_PARENT) {
		return TownGetVariable(variable, parameter, available, industry->town);
	}

	switch (variable) {
		case 0x80: return tile;
		case 0x81: return GB(tile, 8, 8);

		/* Pointer to the town the industry is associated with */
		case 0x82: return industry->town->index;
		case 0x83:
		case 0x84:
		case 0x85: DEBUG(grf, 0, "NewGRFs shouldn't be doing pointer magic"); break; // not supported

		/* Number of the layout */
		case 0x86: return industry->selected_layout;

		/* Ground type */
		case 0x87: return GetTerrainType(tile);

		/* Town zone */
		case 0x88: return GetTownRadiusGroup(industry->town, tile);

		/* Manhattan distance of the closest town */
		case 0x89: return min(DistanceManhattan(industry->town->xy, tile), 255);

		/* Lowest height of the tile */
		case 0x8A: return GetTileZ(tile);

		/* Distance to the nearest water/land tile */
		case 0x8B: return GetClosestWaterDistance(tile, (GetIndustrySpec(industry->type)->behaviour & INDUSTRYBEH_BUILT_ONWATER) == 0);

		/* Square of Euclidian distance from town */
		case 0x8D: return min(DistanceSquare(industry->town->xy, tile), 65535);
	}

	/* None of the special ones, so try the general ones */
	return IndustryGetVariable(object, variable, parameter, available);
}

bool CheckIfCallBackAllowsCreation(TileIndex tile, IndustryType type, uint itspec_index)
{
	const IndustrySpec *indspec = GetIndustrySpec(type);

	ResolverObject object;
	const SpriteGroup *group;

	Industry ind;
	ind.index = INVALID_INDUSTRY;
	ind.xy = tile;
	ind.width = 0;
	ind.type = type;
	ind.selected_layout = itspec_index;
	ind.town = ClosestTownFromTile(tile, (uint)-1);

	NewIndustryResolver(&object, tile, &ind);
	object.GetVariable = IndustryLocationGetVariable;
	object.callback = CBID_INDUSTRY_LOCATION;

	group = Resolve(GetIndustrySpec(type)->grf_prop.spritegroup, &object);

	/* Unlike the "normal" cases, not having a valid result means we allow
	 * the building of the industry, as that's how it's done in TTDP. */
	if (group == NULL || group->type != SGT_CALLBACK) return true;

	/* Copy some parameters from the registers to the error message text ref. stack */
	SwitchToErrorRefStack();
	PrepareTextRefStackUsage(4);
	SwitchToNormalRefStack();

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

	if (HasBit(indspec->callback_flags, CBM_IND_AVAILABLE)) {
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
	const IndustrySpec *spec = GetIndustrySpec(ind->type);
	ResolverObject object;
	NewIndustryResolver(&object, ind->xy, ind);
	if ((spec->behaviour & INDUSTRYBEH_PRODCALLBACK_RANDOM) != 0) object.callback_param1 = Random();
	int multiplier = 1;
	if ((spec->behaviour & INDUSTRYBEH_PROD_MULTI_HNDLING) != 0) multiplier = ind->prod_level;
	object.callback_param2 = reason;

	for (uint loop = 0;; loop++) {
		SB(object.callback_param2, 8, 16, loop);
		const SpriteGroup *group = Resolve(spec->grf_prop.spritegroup, &object);
		if (group == NULL || group->type != SGT_INDUSTRY_PRODUCTION) break;

		bool deref = (group->g.indprod.version == 1);

		for (uint i = 0; i < 3; i++) {
			ind->incoming_cargo_waiting[i] = Clamp(ind->incoming_cargo_waiting[i] - DerefIndProd(group->g.indprod.substract_input[i], deref) * multiplier, 0, 0xFFFF);
		}
		for (uint i = 0; i < 2; i++) {
			ind->produced_cargo_waiting[i] = Clamp(ind->produced_cargo_waiting[i] + max(DerefIndProd(group->g.indprod.add_output[i], deref), 0) * multiplier, 0, 0xFFFF);
		}

		int32 again = DerefIndProd(group->g.indprod.again, deref);
		if (again == 0) break;

		SB(object.callback_param2, 24, 8, again);
	}

	InvalidateWindow(WC_INDUSTRY_VIEW, ind->index);
}
