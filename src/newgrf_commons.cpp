/* $Id$ */

/** @file newgrf_commons.cpp Implementation of the class OverrideManagerBase
 * and its descendance, present and futur
 */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "landscape.h"
#include "town.h"
#include "industry.h"
#include "newgrf.h"
#include "newgrf_commons.h"

/** Constructor of generic class
 * @param offset end of original data for this entity. i.e: houses = 110
 * @param maximum of entities this manager can deal with. i.e: houses = 512
 * @param invalid is the ID used to identify an invalid entity id
 */
OverrideManagerBase::OverrideManagerBase(uint16 offset, uint16 maximum, uint16 invalid)
{
	max_offset = offset;
	max_new_entities = maximum;
	invalid_ID = invalid;

	mapping_ID = CallocT<EntityIDMapping>(max_new_entities);
	entity_overrides = MallocT<uint16>(max_offset);
	memset(entity_overrides, invalid, sizeof(entity_overrides));
}

/** Destructor of the generic class.
 * Frees allocated memory of constructor
 */
OverrideManagerBase::~OverrideManagerBase()
{
	free(mapping_ID);
	free(entity_overrides);
}

/** Since the entity IDs defined by the GRF file does not necessarily correlate
 * to those used by the game, the IDs used for overriding old entities must be
 * translated when the entity spec is set.
 * @param local_id id in grf file
 * @param entity_type original entity type
 */
void OverrideManagerBase::Add(uint8 local_id, uint entity_type)
{
	assert(entity_type < max_offset);
	entity_overrides[entity_type] = local_id;
}

/** Resets the mapping, which is used while initializing game */
void OverrideManagerBase::ResetMapping()
{
	memset(mapping_ID, 0, (max_new_entities - 1) * sizeof(EntityIDMapping));
}

/** Resets the override, which is used while initializing game */
void OverrideManagerBase::ResetOverride()
{
	for (uint16 i = 0; i < max_offset; i++) {
		entity_overrides[i] = invalid_ID;
	}
}

/** Return the ID (if ever available) of a previously inserted entity.
 * @param grf_local_id ID of this enity withing the grfID
 * @param grfid ID of the grf file
 * @return the ID of the candidate, of the Invalid flag item ID
 */
uint16 OverrideManagerBase::GetID(uint8 grf_local_id, uint32 grfid)
{
	const EntityIDMapping *map;

	for (uint16 id = 0; id < max_new_entities; id++) {
		map = &mapping_ID[id];
		if (map->entity_id == grf_local_id && map->grfid == grfid) {
			return id;
		}
	}
	return invalid_ID;
}

/** Reserves a place in the mapping array for an entity to be installed
 * @param grf_local_id is an arbitrary id given by the grf's author.  Also known as setid
 * @param grfid is the id of the grf file itself
 * @param substitute_id is the original entity from which data is copied for the new one
 * @return the proper usable slot id, or invalid marker if none is found
 */
uint16 OverrideManagerBase::AddEntityID(byte grf_local_id, uint32 grfid, byte substitute_id)
{
	uint16 id = this->GetID(grf_local_id, grfid);
	EntityIDMapping *map;

	/* Look to see if this entity has already been added. This is done
	 * separately from the loop below in case a GRF has been deleted, and there
	 * are any gaps in the array.
	 */
	if (id != invalid_ID) {
		return id;
	}

	/* This entity hasn't been defined before, so give it an ID now. */
	for (id = max_offset; id < max_new_entities; id++) {
		map = &mapping_ID[id];

		if (map->entity_id == 0 && map->grfid == 0) {
			map->entity_id     = grf_local_id;
			map->grfid         = grfid;
			map->substitute_id = substitute_id;
			return id;
		}
	}

	return invalid_ID;
}

/** Gives the substitute of the entity, as specified by the grf file
 * @param entity_id of the entity being queried
 * @return mapped id
 */
uint16 OverrideManagerBase::GetSubstituteID(byte entity_id)
{
	return mapping_ID[entity_id].substitute_id;
}

/** Install the specs into the HouseSpecs array
 * It will find itself the proper slot onwhich it will go
 * @param hs HouseSpec read from the grf file, ready for inclusion
 */
void HouseOverrideManager::SetEntitySpec(const HouseSpec *hs)
{
	HouseID house_id = this->AddEntityID(hs->local_id, hs->grffile->grfid, hs->substitute_id);

	if (house_id == invalid_ID) {
		grfmsg(1, "House.SetEntitySpec: Too many houses allocated. Ignoring.");
		return;
	}

	memcpy(&_house_specs[house_id], hs, sizeof(*hs));

	/* Now add the overrides. */
	for (int i = 0; i != max_offset; i++) {
		HouseSpec *overridden_hs = GetHouseSpecs(i);

		if (entity_overrides[i] != hs->local_id) continue;

		overridden_hs->override = house_id;
		entity_overrides[i] = invalid_ID;
	}
}

/** Method to find an entity ID and to mark it as reserved for the Industry to be included.
 * @param grf_local_id ID used by the grf file for pre-installation work (equivalent of TTDPatch's setid
 * @param grfid ID of the current grf file
 * @param substitute_id industry from which data has been copied
 * @return a free entity id (slotid) if ever one has been found, or Invalid_ID marker otherwise
 */
uint16 IndustryOverrideManager::AddEntityID(byte grf_local_id, uint32 grfid, byte substitute_id)
{
	/* This entity hasn't been defined before, so give it an ID now. */
	for (uint16 id = 0; id < max_new_entities; id++) {
		/* Get the real live industry */
		const IndustrySpec *inds = GetIndustrySpec(id);

		/* This industry must be one that is not available(enabled), mostly because of climate.
		 * And it must not already be used by a grf (grffile == NULL).
		 * So reseve this slot here, as it is the chosen one */
		if (!inds->enabled && inds->grf_prop.grffile == NULL) {
			EntityIDMapping *map = &mapping_ID[id];

			if (map->entity_id == 0 && map->grfid == 0) {
				/* winning slot, mark it as been used */
				map->entity_id     = grf_local_id;
				map->grfid         = grfid;
				map->substitute_id = substitute_id;
				return id;
			}
		}
	}

	return invalid_ID;
}

/** Method to install the new indistry data in its proper slot
 * The slot assigment is internal of this method, since it requires
 * checking what is available
 * @param inds Industryspec that comes from the grf decoding process
 */
void IndustryOverrideManager::SetEntitySpec(const IndustrySpec *inds)
{
	/* First step : We need to find if this industry is already specified in the savegame data */
	IndustryType ind_id = this->GetID(inds->grf_prop.local_id, inds->grf_prop.grffile->grfid);

	if (ind_id == invalid_ID) { // not found?  So this is the introduction of a new industry
		/* Second step is dealing with the override. */
		if (inds->grf_prop.override != invalid_ID && _industry_specs[inds->grf_prop.override].grf_prop.override == invalid_ID) {
			/* this is an override, which means it will take the place of the industry it is
			 * designed to replace. Before we conclude that the override is allowed,
			* we first need to verify that the slot is not holding another override
			* If it's the case,it will be considered as a normal substitute */
			ind_id = inds->grf_prop.override;
		} else {
			/* It has already been overriden, so you've lost your place old boy.
			 * Or it is a simple substitute.
			 * In both case, we need to find a free available slot */
			ind_id = this->AddEntityID(inds->grf_prop.local_id, inds->grf_prop.grffile->grfid, inds->grf_prop.subst_id);
		}
	}

	if (ind_id == invalid_ID) {
		grfmsg(1, "Industry.SetEntitySpec: Too many industries allocated. Ignoring.");
		return;
	}

	/* Now that we know we can use the given id, copy the spech to its final destination*/
	memcpy(&_industry_specs[ind_id], inds, sizeof(*inds));
	/* and mark it as usable*/
	_industry_specs[ind_id].enabled = true;
}

void IndustryTileOverrideManager::SetEntitySpec(const IndustryTileSpec *its)
{
	IndustryGfx indt_id = this->AddEntityID(its->grf_prop.local_id, its->grf_prop.grffile->grfid, its->grf_prop.subst_id);

	if (indt_id == invalid_ID) {
		grfmsg(1, "IndustryTile.SetEntitySpec: Too many industry tiles allocated. Ignoring.");
		return;
	}

	memcpy(&_industry_tile_specs[indt_id], its, sizeof(*its));

	/* Now add the overrides. */
	for (int i = 0; i < max_offset; i++) {
		IndustryTileSpec *overridden_its = &_industry_tile_specs[i];

		if (entity_overrides[i] != its->grf_prop.local_id) continue;

		overridden_its->grf_prop.override = indt_id;
		overridden_its->enabled = false;
		entity_overrides[i] = invalid_ID;
	}
}

/** Function used by houses (and soon industries) to get information
 * on type of "terrain" the tile it is queries sits on.
 * @param tile TileIndex of the tile been queried
 * @return value corresponding to the grf expected format:
 *         Terrain type: 0 normal, 1 desert, 2 rainforest, 4 on or above snowline */
uint32 GetTerrainType(TileIndex tile)
{
	switch (_opt.landscape) {
		case LT_TROPIC: return GetTropicZone(tile) == TROPICZONE_DESERT ? 1 : 2;
		case LT_ARCTIC: return GetTileZ(tile) > GetSnowLine() ? 4 : 0;
		default:        return 0;
	}
}

TileIndex GetNearbyTile(byte parameter, TileIndex tile)
{
	int8 x = GB(parameter, 0, 4);
	int8 y = GB(parameter, 4, 4);

	if (x >= 8) x -= 16;
	if (y >= 8) y -= 16;

	/* Make sure we never roam outside of the map */
	return TILE_MASK(tile + TileDiffXY(x, y));
}
