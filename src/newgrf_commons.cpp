/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file newgrf_commons.cpp Implementation of the class OverrideManagerBase
 * and its descendance, present and futur
 */

#include "stdafx.h"
#include "landscape.h"
#include "house.h"
#include "industrytype.h"
#include "newgrf.h"
#include "clear_map.h"
#include "station_map.h"
#include "tree_map.h"
#include "tunnelbridge_map.h"
#include "newgrf_object.h"
#include "genworld.h"

/**
 * Constructor of generic class
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
	for (size_t i = 0; i < max_offset; i++) entity_overrides[i] = invalid;
	grfid_overrides = CallocT<uint32>(max_offset);
}

/**
 * Destructor of the generic class.
 * Frees allocated memory of constructor
 */
OverrideManagerBase::~OverrideManagerBase()
{
	free(mapping_ID);
	free(entity_overrides);
	free(grfid_overrides);
}

/**
 * Since the entity IDs defined by the GRF file does not necessarily correlate
 * to those used by the game, the IDs used for overriding old entities must be
 * translated when the entity spec is set.
 * @param local_id ID in grf file
 * @param grfid  ID of the grf file
 * @param entity_type original entity type
 */
void OverrideManagerBase::Add(uint8 local_id, uint32 grfid, uint entity_type)
{
	assert(entity_type < max_offset);
	/* An override can be set only once */
	if (entity_overrides[entity_type] != invalid_ID) return;
	entity_overrides[entity_type] = local_id;
	grfid_overrides[entity_type] = grfid;
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
		grfid_overrides[i] = 0;
	}
}

/**
 * Return the ID (if ever available) of a previously inserted entity.
 * @param grf_local_id ID of this enity withing the grfID
 * @param grfid ID of the grf file
 * @return the ID of the candidate, of the Invalid flag item ID
 */
uint16 OverrideManagerBase::GetID(uint8 grf_local_id, uint32 grfid) const
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

/**
 * Reserves a place in the mapping array for an entity to be installed
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

		if (CheckValidNewID(id) && map->entity_id == 0 && map->grfid == 0) {
			map->entity_id     = grf_local_id;
			map->grfid         = grfid;
			map->substitute_id = substitute_id;
			return id;
		}
	}

	return invalid_ID;
}

/**
 * Gives the substitute of the entity, as specified by the grf file
 * @param entity_id of the entity being queried
 * @return mapped id
 */
uint16 OverrideManagerBase::GetSubstituteID(uint16 entity_id) const
{
	return mapping_ID[entity_id].substitute_id;
}

/**
 * Install the specs into the HouseSpecs array
 * It will find itself the proper slot onwhich it will go
 * @param hs HouseSpec read from the grf file, ready for inclusion
 */
void HouseOverrideManager::SetEntitySpec(const HouseSpec *hs)
{
	HouseID house_id = this->AddEntityID(hs->grf_prop.local_id, hs->grf_prop.grffile->grfid, hs->grf_prop.subst_id);

	if (house_id == invalid_ID) {
		grfmsg(1, "House.SetEntitySpec: Too many houses allocated. Ignoring.");
		return;
	}

	MemCpyT(HouseSpec::Get(house_id), hs);

	/* Now add the overrides. */
	for (int i = 0; i != max_offset; i++) {
		HouseSpec *overridden_hs = HouseSpec::Get(i);

		if (entity_overrides[i] != hs->grf_prop.local_id || grfid_overrides[i] != hs->grf_prop.grffile->grfid) continue;

		overridden_hs->grf_prop.override = house_id;
		entity_overrides[i] = invalid_ID;
		grfid_overrides[i] = 0;
	}
}

/**
 * Return the ID (if ever available) of a previously inserted entity.
 * @param grf_local_id ID of this enity withing the grfID
 * @param grfid ID of the grf file
 * @return the ID of the candidate, of the Invalid flag item ID
 */
uint16 IndustryOverrideManager::GetID(uint8 grf_local_id, uint32 grfid) const
{
	uint16 id = OverrideManagerBase::GetID(grf_local_id, grfid);
	if (id != invalid_ID) return id;

	/* No mapping found, try the overrides */
	for (id = 0; id < max_offset; id++) {
		if (entity_overrides[id] == grf_local_id && grfid_overrides[id] == grfid) return id;
	}

	return invalid_ID;
}

/**
 * Method to find an entity ID and to mark it as reserved for the Industry to be included.
 * @param grf_local_id ID used by the grf file for pre-installation work (equivalent of TTDPatch's setid
 * @param grfid ID of the current grf file
 * @param substitute_id industry from which data has been copied
 * @return a free entity id (slotid) if ever one has been found, or Invalid_ID marker otherwise
 */
uint16 IndustryOverrideManager::AddEntityID(byte grf_local_id, uint32 grfid, byte substitute_id)
{
	/* This entity hasn't been defined before, so give it an ID now. */
	for (uint16 id = 0; id < max_new_entities; id++) {
		/* Skip overriden industries */
		if (id < max_offset && entity_overrides[id] != invalid_ID) continue;

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

/**
 * Method to install the new indistry data in its proper slot
 * The slot assigment is internal of this method, since it requires
 * checking what is available
 * @param inds Industryspec that comes from the grf decoding process
 */
void IndustryOverrideManager::SetEntitySpec(IndustrySpec *inds)
{
	/* First step : We need to find if this industry is already specified in the savegame data. */
	IndustryType ind_id = this->GetID(inds->grf_prop.local_id, inds->grf_prop.grffile->grfid);

	if (ind_id == invalid_ID) {
		/* Not found.
		 * Or it has already been overriden, so you've lost your place old boy.
		 * Or it is a simple substitute.
		 * We need to find a free available slot */
		ind_id = this->AddEntityID(inds->grf_prop.local_id, inds->grf_prop.grffile->grfid, inds->grf_prop.subst_id);
		inds->grf_prop.override = invalid_ID;  // make sure it will not be detected as overriden
	}

	if (ind_id == invalid_ID) {
		grfmsg(1, "Industry.SetEntitySpec: Too many industries allocated. Ignoring.");
		return;
	}

	/* Now that we know we can use the given id, copy the spec to its final destination... */
	memcpy(&_industry_specs[ind_id], inds, sizeof(*inds));
	/* ... and mark it as usable*/
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

		if (entity_overrides[i] != its->grf_prop.local_id || grfid_overrides[i] != its->grf_prop.grffile->grfid) continue;

		overridden_its->grf_prop.override = indt_id;
		overridden_its->enabled = false;
		entity_overrides[i] = invalid_ID;
		grfid_overrides[i] = 0;
	}
}

/**
 * Method to install the new object data in its proper slot
 * The slot assigment is internal of this method, since it requires
 * checking what is available
 * @param spec ObjectSpec that comes from the grf decoding process
 */
void ObjectOverrideManager::SetEntitySpec(ObjectSpec *spec)
{
	/* First step : We need to find if this object is already specified in the savegame data. */
	ObjectType type = this->GetID(spec->grf_prop.local_id, spec->grf_prop.grffile->grfid);

	if (type == invalid_ID) {
		/* Not found.
		 * Or it has already been overriden, so you've lost your place old boy.
		 * Or it is a simple substitute.
		 * We need to find a free available slot */
		type = this->AddEntityID(spec->grf_prop.local_id, spec->grf_prop.grffile->grfid, OBJECT_TRANSMITTER);
	}

	if (type == invalid_ID) {
		grfmsg(1, "Object.SetEntitySpec: Too many objects allocated. Ignoring.");
		return;
	}

	extern ObjectSpec _object_specs[NUM_OBJECTS];

	/* Now that we know we can use the given id, copy the spec to its final destination. */
	memcpy(&_object_specs[type], spec, sizeof(*spec));
	ObjectClass::Assign(&_object_specs[type]);
}

/**
 * Function used by houses (and soon industries) to get information
 * on type of "terrain" the tile it is queries sits on.
 * @param tile TileIndex of the tile been queried
 * @param upper_halftile If true, query upper halftile in case of rail tiles.
 * @return value corresponding to the grf expected format:
 *         Terrain type: 0 normal, 1 desert, 2 rainforest, 4 on or above snowline
 */
uint32 GetTerrainType(TileIndex tile, TileContext context)
{
	switch (_settings_game.game_creation.landscape) {
		case LT_TROPIC: return GetTropicZone(tile);
		case LT_ARCTIC: {
			bool has_snow;
			switch (GetTileType(tile)) {
				case MP_CLEAR:
					/* During map generation the snowstate may not be valid yet, as the tileloop may not have run yet. */
					if (_generating_world) goto genworld;
					has_snow = IsSnowTile(tile) && GetClearDensity(tile) >= 2;
					break;

				case MP_RAILWAY: {
					/* During map generation the snowstate may not be valid yet, as the tileloop may not have run yet. */
					if (_generating_world) goto genworld; // we do not care about foundations here
					RailGroundType ground = GetRailGroundType(tile);
					has_snow = (ground == RAIL_GROUND_ICE_DESERT || (context == TCX_UPPER_HALFTILE && ground == RAIL_GROUND_HALF_SNOW));
					break;
				}

				case MP_ROAD:
					/* During map generation the snowstate may not be valid yet, as the tileloop may not have run yet. */
					if (_generating_world) goto genworld; // we do not care about foundations here
					has_snow = IsOnSnow(tile);
					break;

				case MP_TREES: {
					/* During map generation the snowstate may not be valid yet, as the tileloop may not have run yet. */
					if (_generating_world) goto genworld;
					TreeGround ground = GetTreeGround(tile);
					has_snow = (ground == TREE_GROUND_SNOW_DESERT || ground == TREE_GROUND_ROUGH_SNOW) && GetTreeDensity(tile) >= 2;
					break;
				}

				case MP_TUNNELBRIDGE:
					if (context == TCX_ON_BRIDGE) {
						has_snow = (GetBridgeHeight(tile) > GetSnowLine());
					} else {
						/* During map generation the snowstate may not be valid yet, as the tileloop may not have run yet. */
						if (_generating_world) goto genworld; // we do not care about foundations here
						has_snow = HasTunnelBridgeSnowOrDesert(tile);
					}
					break;

				case MP_STATION:
				case MP_HOUSE:
				case MP_INDUSTRY:
				case MP_OBJECT:
					/* These tiles usually have a levelling foundation. So use max Z */
					has_snow = (GetTileMaxZ(tile) > GetSnowLine());
					break;

				case MP_VOID:
				case MP_WATER:
				genworld:
					has_snow = (GetTileZ(tile) > GetSnowLine());
					break;

				default: NOT_REACHED();
			}
			return has_snow ? 4 : 0;
		}
		default:        return 0;
	}
}

TileIndex GetNearbyTile(byte parameter, TileIndex tile, bool signed_offsets)
{
	int8 x = GB(parameter, 0, 4);
	int8 y = GB(parameter, 4, 4);

	if (signed_offsets && x >= 8) x -= 16;
	if (signed_offsets && y >= 8) y -= 16;

	/* Swap width and height depending on axis for railway stations */
	if (HasStationTileRail(tile) && GetRailStationAxis(tile) == AXIS_Y) Swap(x, y);

	/* Make sure we never roam outside of the map, better wrap in that case */
	return TILE_MASK(tile + TileDiffXY(x, y));
}

/**
 * Common part of station var 0x67, house var 0x62, indtile var 0x60, industry var 0x62.
 *
 * @param tile the tile of interest.
 * @return 0czzbbss: c = TileType; zz = TileZ; bb: 7-3 zero, 4-2 TerrainType, 1 water/shore, 0 zero; ss = TileSlope
 */
uint32 GetNearbyTileInformation(TileIndex tile)
{
	TileType tile_type = GetTileType(tile);

	/* Fake tile type for trees on shore */
	if (IsTileType(tile, MP_TREES) && GetTreeGround(tile) == TREE_GROUND_SHORE) tile_type = MP_WATER;

	uint z;
	Slope tileh = GetTileSlope(tile, &z);
	byte terrain_type = GetTerrainType(tile) << 2 | (tile_type == MP_WATER ? 1 : 0) << 1;
	return tile_type << 24 | z << 16 | terrain_type << 8 | tileh;
}
