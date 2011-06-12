/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_industries.cpp Handling of NewGRF industries. */

#include "stdafx.h"
#include "debug.h"
#include "industry.h"
#include "newgrf.h"
#include "newgrf_industries.h"
#include "newgrf_text.h"
#include "newgrf_town.h"
#include "newgrf_cargo.h"
#include "window_func.h"
#include "town.h"
#include "company_base.h"
#include "command_func.h"
#include "gui.h"
#include "strings_func.h"
#include "core/random_func.hpp"

#include "table/strings.h"

static uint32 _industry_creation_random_bits;

/* Since the industry IDs defined by the GRF file don't necessarily correlate
 * to those used by the game, the IDs used for overriding old industries must be
 * translated when the idustry spec is set. */
IndustryOverrideManager _industry_mngr(NEW_INDUSTRYOFFSET, NUM_INDUSTRYTYPES, INVALID_INDUSTRYTYPE);
IndustryTileOverrideManager _industile_mngr(NEW_INDUSTRYTILEOFFSET, NUM_INDUSTRYTILES, INVALID_INDUSTRYTILE);

/**
 * Map the GRF local type to an industry type.
 * @param grf_type The GRF local type.
 * @param grf_id The GRF of the local type.
 * @return The industry type in the global scope.
 */
IndustryType MapNewGRFIndustryType(IndustryType grf_type, uint32 grf_id)
{
	if (grf_type == IT_INVALID) return IT_INVALID;
	if (!HasBit(grf_type, 7)) return GB(grf_type, 0, 6);

	return _industry_mngr.GetID(GB(grf_type, 0, 6), grf_id);
}

/**
 * Make an analysis of a tile and check for its belonging to the same
 * industry, and/or the same grf file
 * @param tile TileIndex of the tile to query
 * @param i Industry to which to compare the tile to
 * @param cur_grfid GRFID of the current callback chain
 * @return value encoded as per NFO specs
 */
uint32 GetIndustryIDAtOffset(TileIndex tile, const Industry *i, uint32 cur_grfid)
{
	if (!IsTileType(tile, MP_INDUSTRY) || GetIndustryIndex(tile) != i->index) {
		/* No industry and/or the tile does not have the same industry as the one we match it with */
		return 0xFFFF;
	}

	IndustryGfx gfx = GetCleanIndustryGfx(tile);
	const IndustryTileSpec *indtsp = GetIndustryTileSpec(gfx);

	if (gfx < NEW_INDUSTRYTILEOFFSET) { // Does it belongs to an old type?
		/* It is an old tile.  We have to see if it's been overriden */
		if (indtsp->grf_prop.override == INVALID_INDUSTRYTILE) { // has it been overridden?
			return 0xFF << 8 | gfx; // no. Tag FF + the gfx id of that tile
		}
		/* Overriden */
		const IndustryTileSpec *tile_ovr = GetIndustryTileSpec(indtsp->grf_prop.override);

		if (tile_ovr->grf_prop.grffile->grfid == cur_grfid) {
			return tile_ovr->grf_prop.local_id; // same grf file
		} else {
			return 0xFFFE; // not the same grf file
		}
	}
	/* Not an 'old type' tile */
	if (indtsp->grf_prop.spritegroup[0] != NULL) { // tile has a spritegroup ?
		if (indtsp->grf_prop.grffile->grfid == cur_grfid) { // same industry, same grf ?
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
	uint32 best_dist = UINT32_MAX;
	const Industry *i;
	FOR_ALL_INDUSTRIES(i) {
		if (i->type != type || i == current) continue;

		best_dist = min(best_dist, DistanceManhattan(tile, i->location.tile));
	}

	return best_dist;
}

/**
 * Implementation of both var 67 and 68
 * since the mechanism is almost the same, it is easier to regroup them on the same
 * function.
 * @param param_setID parameter given to the callback, which is the set id, or the local id, in our terminology
 * @param layout_filter on what layout do we filter?
 * @param town_filter Do we filter on the same town as the current industry?
 * @param current Industry for which the inquiry is made
 * @return the formatted answer to the callback : rr(reserved) cc(count) dddd(manhattan distance of closest sister)
 */
static uint32 GetCountAndDistanceOfClosestInstance(byte param_setID, byte layout_filter, bool town_filter, const Industry *current)
{
	uint32 GrfID = GetRegister(0x100);  ///< Get the GRFID of the definition to look for in register 100h
	IndustryType ind_index;
	uint32 closest_dist = UINT32_MAX;
	byte count = 0;

	/* Determine what will be the industry type to look for */
	switch (GrfID) {
		case 0:  // this is a default industry type
			ind_index = param_setID;
			break;

		case 0xFFFFFFFF: // current grf
			GrfID = GetIndustrySpec(current->type)->grf_prop.grffile->grfid;
			/* FALL THROUGH */

		default: // use the grfid specified in register 100h
			SetBit(param_setID, 7); // bit 7 means it is not an old type
			ind_index = MapNewGRFIndustryType(param_setID, GrfID);
			break;
	}

	/* If the industry type is invalid, there is none and the closest is far away. */
	if (ind_index >= NUM_INDUSTRYTYPES) return 0 | 0xFFFF;

	if (layout_filter == 0 && !town_filter) {
		/* If the filter is 0, it could be because none was specified as well as being really a 0.
		 * In either case, just do the regular var67 */
		closest_dist = GetClosestIndustry(current->location.tile, ind_index, current);
		count = min(Industry::GetIndustryTypeCount(ind_index), UINT8_MAX); // clamp to 8 bit
	} else {
		/* Count only those who match the same industry type and layout filter
		 * Unfortunately, we have to do it manually */
		const Industry *i;
		FOR_ALL_INDUSTRIES(i) {
			if (i->type == ind_index && i != current && (i->selected_layout == layout_filter || layout_filter == 0) && (!town_filter || i->town == current->town)) {
				closest_dist = min(closest_dist, DistanceManhattan(current->location.tile, i->location.tile));
				count++;
			}
		}
	}

	return count << 16 | GB(closest_dist, 0, 16);
}

/**
 * This function implements the industries variables that newGRF defines.
 * @param object the object that we want to query
 * @param variable that is queried
 * @param parameter unused
 * @param available will return false if ever the variable asked for does not exist
 * @return the value stored in the corresponding variable
 */
uint32 IndustryGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	const Industry *industry = object->u.industry.ind;
	TileIndex tile = object->u.industry.tile;
	IndustryType type = object->u.industry.type;
	const IndustrySpec *indspec = GetIndustrySpec(type);

	/* Shall the variable get resolved in parent scope and are we not yet in parent scope? */
	if (object->u.industry.gfx == INVALID_INDUSTRYTILE && object->scope == VSG_SCOPE_PARENT) {
		/* Pass the request on to the town of the industry */
		Town *t;

		if (industry != NULL) {
			t = industry->town;
		} else if (tile != INVALID_TILE) {
			t = ClosestTownFromTile(tile, UINT_MAX);
		} else {
			*available = false;
			return UINT_MAX;
		}

		return TownGetVariable(variable, parameter, available, t, object->grffile);
	}

	if (industry == NULL) {
		DEBUG(grf, 1, "Unhandled variable 0x%X (no available industry) in callback 0x%x", variable, object->callback);

		*available = false;
		return UINT_MAX;
	}

	switch (variable) {
		case 0x40:
		case 0x41:
		case 0x42: { // waiting cargo, but only if those two callback flags are set
			uint16 callback = indspec->callback_mask;
			if (HasBit(callback, CBM_IND_PRODUCTION_CARGO_ARRIVAL) || HasBit(callback, CBM_IND_PRODUCTION_256_TICKS)) {
				if ((indspec->behaviour & INDUSTRYBEH_PROD_MULTI_HNDLING) != 0) {
					if (industry->prod_level == 0) return 0;
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

		/* Company info */
		case 0x45: {
			byte colours = 0;
			bool is_ai = false;

			const Company *c = Company::GetIfValid(industry->founder);
			if (c != NULL) {
				const Livery *l = &c->livery[LS_DEFAULT];

				is_ai = c->is_ai;
				colours = l->colour1 + l->colour2 * 16;
			}

			return industry->founder | (is_ai ? 0x10000 : 0) | (colours << 24);
		}

		case 0x46: return industry->construction_date; // Date when built - long format - (in days)

		/* Get industry ID at offset param */
		case 0x60: return GetIndustryIDAtOffset(GetNearbyTile(parameter, industry->location.tile, false), industry, object->grffile->grfid);

		/* Get random tile bits at offset param */
		case 0x61:
			tile = GetNearbyTile(parameter, tile, false);
			return (IsTileType(tile, MP_INDUSTRY) && Industry::GetByTile(tile) == industry) ? GetIndustryRandomBits(tile) : 0;

		/* Land info of nearby tiles */
		case 0x62: return GetNearbyIndustryTileInformation(parameter, tile, INVALID_INDUSTRY, false);

		/* Animation stage of nearby tiles */
		case 0x63:
			tile = GetNearbyTile(parameter, tile, false);
			if (IsTileType(tile, MP_INDUSTRY) && Industry::GetByTile(tile) == industry) {
				return GetAnimationFrame(tile);
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
		case 0x68: {
			byte layout_filter = 0;
			bool town_filter = false;
			if (variable == 0x68) {
				uint32 reg = GetRegister(0x101);
				layout_filter = GB(reg, 0, 8);
				town_filter = HasBit(reg, 8);
			}
			return GetCountAndDistanceOfClosestInstance(parameter, layout_filter, town_filter, industry);
		}

		/* Get a variable from the persistent storage */
		case 0x7C: return (industry->psa != NULL) ? industry->psa->GetValue(parameter) : 0;

		/* Industry structure access*/
		case 0x80: return industry->location.tile;
		case 0x81: return GB(industry->location.tile, 8, 8);
		/* Pointer to the town the industry is associated with */
		case 0x82: return industry->town->index;
		case 0x83:
		case 0x84:
		case 0x85: DEBUG(grf, 0, "NewGRFs shouldn't be doing pointer magic"); break; // not supported
		case 0x86: return industry->location.w;
		case 0x87: return industry->location.h;// xy dimensions

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
		case 0x9B: return GB(industry->this_month_transported[1], 8, 8);
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
		case 0xA5: return GB(industry->last_month_transported[1], 8, 8);

		case 0xA6: return industry->type;
		case 0xA7: return industry->founder;
		case 0xA8: return industry->random_colour;
		case 0xA9: return Clamp(industry->last_prod_year - ORIGINAL_BASE_YEAR, 0, 255);
		case 0xAA: return industry->counter;
		case 0xAB: return GB(industry->counter, 8, 8);
		case 0xAC: return industry->was_cargo_delivered;

		case 0xB0: return Clamp(industry->construction_date - DAYS_TILL_ORIGINAL_BASE_YEAR, 0, 65535); // Date when built since 1920 (in days)
		case 0xB3: return industry->construction_type; // Construction type
		case 0xB4: return Clamp(industry->last_cargo_accepted_at - DAYS_TILL_ORIGINAL_BASE_YEAR, 0, 65535); // Date last cargo accepted since 1920 (in days)
	}

	DEBUG(grf, 1, "Unhandled industry variable 0x%X", variable);

	*available = false;
	return UINT_MAX;
}

static const SpriteGroup *IndustryResolveReal(const ResolverObject *object, const RealSpriteGroup *group)
{
	/* IndustryTile do not have 'real' groups */
	return NULL;
}

static uint32 IndustryGetRandomBits(const ResolverObject *object)
{
	const Industry *ind = object->u.industry.ind;
	return ind != NULL ? ind->random: 0;
}

static uint32 IndustryGetTriggers(const ResolverObject *object)
{
	const Industry *ind = object->u.industry.ind;
	return ind != NULL ? ind->random_triggers : 0;
}

static void IndustrySetTriggers(const ResolverObject *object, int triggers)
{
	Industry *ind = object->u.industry.ind;
	assert(ind != NULL && ind->index != INVALID_INDUSTRY);
	ind->random_triggers = triggers;
}

/**
 * Store a value into the object's persistent storage.
 * @param object Object that we want to query.
 * @param pos Position in the persistent storage to use.
 * @param value Value to store.
 */
void IndustryStorePSA(ResolverObject *object, uint pos, int32 value)
{
	Industry *ind = object->u.industry.ind;
	if (ind->index == INVALID_INDUSTRY) return;

	if (object->scope != VSG_SCOPE_SELF) {
		/* Pass the request on to the town of the industry. */
		TownStorePSA(ind->town, object->grffile, pos, value);
		return;
	}

	if (ind->psa == NULL) {
		/* There is no need to create a storage if the value is zero. */
		if (value == 0) return;

		/* Create storage on first modification. */
		const IndustrySpec *indsp = GetIndustrySpec(ind->type);
		uint32 grfid = (indsp->grf_prop.grffile != NULL) ? indsp->grf_prop.grffile->grfid : 0;
		assert(PersistentStorage::CanAllocateItem());
		ind->psa = new PersistentStorage(grfid);
	}

	ind->psa->StoreValue(pos, value);
}

static void NewIndustryResolver(ResolverObject *res, TileIndex tile, Industry *indus, IndustryType type)
{
	res->GetRandomBits = IndustryGetRandomBits;
	res->GetTriggers   = IndustryGetTriggers;
	res->SetTriggers   = IndustrySetTriggers;
	res->GetVariable   = IndustryGetVariable;
	res->ResolveReal   = IndustryResolveReal;
	res->StorePSA      = IndustryStorePSA;

	res->u.industry.tile = tile;
	res->u.industry.ind  = indus;
	res->u.industry.gfx  = INVALID_INDUSTRYTILE;
	res->u.industry.type = type;

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
	res->count           = 0;

	const IndustrySpec *indspec = GetIndustrySpec(type);
	res->grffile         = (indspec != NULL ? indspec->grf_prop.grffile : NULL);
}

/**
 * Perform an industry callback.
 * @param callback The callback to perform.
 * @param param1 The first parameter.
 * @param param2 The second parameter.
 * @param industry The industry to do the callback for.
 * @param type The type of industry to do the callback for.
 * @param tile The tile associated with the callback.
 * @return The callback result.
 */
uint16 GetIndustryCallback(CallbackID callback, uint32 param1, uint32 param2, Industry *industry, IndustryType type, TileIndex tile)
{
	ResolverObject object;
	const SpriteGroup *group;

	NewIndustryResolver(&object, tile, industry, type);
	object.callback = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	group = SpriteGroup::Resolve(GetIndustrySpec(type)->grf_prop.spritegroup[0], &object);
	if (group == NULL) return CALLBACK_FAILED;

	return group->GetCallbackResult();
}

uint32 IndustryLocationGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	const Industry *industry = object->u.industry.ind;
	TileIndex tile = object->u.industry.tile;

	if (object->scope == VSG_SCOPE_PARENT) {
		return TownGetVariable(variable, parameter, available, industry->town, object->grffile);
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

		/* 32 random bits */
		case 0x8F: return _industry_creation_random_bits;
	}

	/* None of the special ones, so try the general ones */
	return IndustryGetVariable(object, variable, parameter, available);
}

/**
 * Check that the industry callback allows creation of the industry.
 * @param tile %Tile to build the industry.
 * @param type Type of industry to build.
 * @param layout Layout number.
 * @param seed Seed for the random generator.
 * @param initial_random_bits The random bits the industry is going to have after construction.
 * @param founder Industry founder
 * @param creation_type The circumstances the industry is created under.
 * @return Succeeded or failed command.
 */
CommandCost CheckIfCallBackAllowsCreation(TileIndex tile, IndustryType type, uint layout, uint32 seed, uint16 initial_random_bits, Owner founder, IndustryAvailabilityCallType creation_type)
{
	const IndustrySpec *indspec = GetIndustrySpec(type);

	ResolverObject object;
	const SpriteGroup *group;

	Industry ind;
	ind.index = INVALID_INDUSTRY;
	ind.location.tile = tile;
	ind.location.w = 0; // important to mark the industry invalid
	ind.type = type;
	ind.selected_layout = layout;
	ind.town = ClosestTownFromTile(tile, UINT_MAX);
	ind.random = initial_random_bits;
	ind.founder = founder;

	NewIndustryResolver(&object, tile, &ind, type);
	object.GetVariable = IndustryLocationGetVariable;
	object.callback = CBID_INDUSTRY_LOCATION;
	object.callback_param2 = creation_type;
	_industry_creation_random_bits = seed;

	group = SpriteGroup::Resolve(GetIndustrySpec(type)->grf_prop.spritegroup[0], &object);

	/* Unlike the "normal" cases, not having a valid result means we allow
	 * the building of the industry, as that's how it's done in TTDP. */
	if (group == NULL) return CommandCost();
	uint16 result = group->GetCallbackResult();
	if (result == 0x400 || result == CALLBACK_FAILED) return CommandCost();

	/* Copy some parameters from the registers to the error message text ref. stack */
	SwitchToErrorRefStack();
	PrepareTextRefStackUsage(4);
	SwitchToNormalRefStack();

	switch (result) {
		case 0x401: return_cmd_error(STR_ERROR_SITE_UNSUITABLE);
		case 0x402: return_cmd_error(STR_ERROR_CAN_ONLY_BE_BUILT_IN_RAINFOREST);
		case 0x403: return_cmd_error(STR_ERROR_CAN_ONLY_BE_BUILT_IN_DESERT);
		default:    return_cmd_error(GetGRFStringID(indspec->grf_prop.grffile->grfid, 0xD000 + result));
	}
	NOT_REACHED();
}

/**
 * Check with callback #CBM_IND_AVAILABLE whether the industry can be built.
 * @param type Industry type to check.
 * @param creation_type Reason to construct a new industry.
 * @return If the industry has no callback or allows building, \c true is returned. Otherwise, \c false is returned.
 */
bool CheckIfCallBackAllowsAvailability(IndustryType type, IndustryAvailabilityCallType creation_type)
{
	const IndustrySpec *indspec = GetIndustrySpec(type);

	if (HasBit(indspec->callback_mask, CBM_IND_AVAILABLE)) {
		uint16 res = GetIndustryCallback(CBID_INDUSTRY_AVAILABLE, 0, creation_type, NULL, type, INVALID_TILE);
		if (res != CALLBACK_FAILED) {
			return (res == 0);
		}
	}
	return true;
}

static int32 DerefIndProd(int field, bool use_register)
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
	NewIndustryResolver(&object, ind->location.tile, ind, ind->type);
	if ((spec->behaviour & INDUSTRYBEH_PRODCALLBACK_RANDOM) != 0) object.callback_param1 = Random();
	int multiplier = 1;
	if ((spec->behaviour & INDUSTRYBEH_PROD_MULTI_HNDLING) != 0) multiplier = ind->prod_level;
	object.callback_param2 = reason;

	for (uint loop = 0;; loop++) {
		/* limit the number of calls to break infinite loops.
		 * 'loop' is provided as 16 bits to the newgrf, so abort when those are exceeded. */
		if (loop >= 0x10000) {
			/* display error message */
			SetDParamStr(0, spec->grf_prop.grffile->filename);
			SetDParam(1, spec->name);
			ShowErrorMessage(STR_NEWGRF_BUGGY, STR_NEWGRF_BUGGY_ENDLESS_PRODUCTION_CALLBACK, WL_WARNING);

			/* abort the function early, this error isn't critical and will allow the game to continue to run */
			break;
		}

		SB(object.callback_param2, 8, 16, loop);
		const SpriteGroup *tgroup = SpriteGroup::Resolve(spec->grf_prop.spritegroup[0], &object);
		if (tgroup == NULL || tgroup->type != SGT_INDUSTRY_PRODUCTION) break;
		const IndustryProductionSpriteGroup *group = (const IndustryProductionSpriteGroup *)tgroup;

		bool deref = (group->version == 1);

		for (uint i = 0; i < 3; i++) {
			ind->incoming_cargo_waiting[i] = Clamp(ind->incoming_cargo_waiting[i] - DerefIndProd(group->subtract_input[i], deref) * multiplier, 0, 0xFFFF);
		}
		for (uint i = 0; i < 2; i++) {
			ind->produced_cargo_waiting[i] = Clamp(ind->produced_cargo_waiting[i] + max(DerefIndProd(group->add_output[i], deref), 0) * multiplier, 0, 0xFFFF);
		}

		int32 again = DerefIndProd(group->again, deref);
		if (again == 0) break;

		SB(object.callback_param2, 24, 8, again);
	}

	SetWindowDirty(WC_INDUSTRY_VIEW, ind->index);
}

/**
 * Resolve a industry's spec and such so we can get a variable.
 * @param ro    The resolver object to fill.
 * @param index The industry ID to get the data from.
 */
void GetIndustryResolver(ResolverObject *ro, uint index)
{
	Industry *i = Industry::Get(index);
	NewIndustryResolver(ro, i->location.tile, i, i->type);
}

/**
 * Check whether an industry temporarily refuses to accept a certain cargo.
 * @param ind The industry to query.
 * @param cargo_type The cargo to get information about.
 * @pre cargo_type is in ind->accepts_cargo.
 * @return Whether the given industry refuses to accept this cargo type.
 */
bool IndustryTemporarilyRefusesCargo(Industry *ind, CargoID cargo_type)
{
	assert(cargo_type == ind->accepts_cargo[0] || cargo_type == ind->accepts_cargo[1] || cargo_type == ind->accepts_cargo[2]);

	const IndustrySpec *indspec = GetIndustrySpec(ind->type);
	if (HasBit(indspec->callback_mask, CBM_IND_REFUSE_CARGO)) {
		uint16 res = GetIndustryCallback(CBID_INDUSTRY_REFUSE_CARGO,
				0, GetReverseCargoTranslation(cargo_type, indspec->grf_prop.grffile),
				ind, ind->type, ind->location.tile);
		return res == 0;
	}
	return false;
}
