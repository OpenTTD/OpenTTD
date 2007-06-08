/* $Id$ */

/** @file newgrf_commons.cpp Implementation of the class OverrideManagerBase
 * and its descendance, present and futur
 */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "clear_map.h"
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

	for (uint16 id = max_offset; id < max_new_entities; id++) {
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

/** Function used by houses (and soon industries) to get information
 * on type of "terrain" the tile it is queries sits on.
 * @param tile TileIndex of the tile been queried
 * @return value corresponding to the grf expected format:
 *         Terrain type: 0 normal, 1 desert, 2 rainforest, 4 on or above snowline */
uint32 GetTerrainType(TileIndex tile)
{
	switch (_opt.landscape) {
		case LT_TROPIC: return GetTropicZone(tile) == TROPICZONE_DESERT ? 1 : 2;
		case LT_ARCTIC: return (GetClearGround(tile) == CLEAR_SNOW) ? 4 : 0;
		default:        return 0;
	}
}

TileIndex GetNearbyTile(byte parameter, TileIndex tile)
{
	int8 x = GB(parameter, 0, 4);
	int8 y = GB(parameter, 4, 4);

	if (x >= 8) x -= 16;
	if (y >= 8) y -= 16;

	return tile + TileDiffXY(x, y);
}
