/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file newgrf_commons.h This file simplyfies and embeds a common mechanism of
 * loading/saving and mapping of grf entities.
 */

#ifndef NEWGRF_COMMONS_H
#define NEWGRF_COMMONS_H

#include "tile_type.h"
#include "sprite.h"
#include "core/alloc_type.hpp"

/** Context for tile accesses */
enum TileContext {
	TCX_NORMAL,         ///< Nothing special.
	TCX_UPPER_HALFTILE, ///< Querying information about the upper part of a tile with halftile foundation.
	TCX_ON_BRIDGE,      ///< Querying information about stuff on the bridge (via some bridgehead).
};

/**
 * NewGRF supplied spritelayout.
 * In contrast to #DrawTileSprites this struct is for allocated
 * layouts on the heap. It allocates data and frees them on destruction.
 */
struct NewGRFSpriteLayout : ZeroedMemoryAllocator, DrawTileSprites {
	void Allocate(uint num_sprites);
	void Clone(const DrawTileSeqStruct *source);

	/**
	 * Clone a spritelayout.
	 * @param source The spritelayout to copy.
	 */
	void Clone(const DrawTileSprites *source)
	{
		assert(source != NULL && this != source);
		this->ground = source->ground;
		this->Clone(source->seq);
	}

	virtual ~NewGRFSpriteLayout()
	{
		free(const_cast<DrawTileSeqStruct*>(this->seq));
	}
};

/**
 * Maps an entity id stored on the map to a GRF file.
 * Entities are objects used ingame (houses, industries, industry tiles) for
 * which we need to correlate the ids from the grf files with the ones in the
 * the savegames themselves.
 * An array of EntityIDMapping structs is saved with the savegame so
 * that those GRFs can be loaded in a different order, or removed safely. The
 * index in the array is the entity's ID stored on the map.
 *
 * The substitute ID is the ID of an original entity that should be used instead
 * if the GRF containing the new entity is not available.
 */
struct EntityIDMapping {
	uint32 grfid;          ///< The GRF ID of the file the entity belongs to
	uint8  entity_id;      ///< The entity ID within the GRF file
	uint8  substitute_id;  ///< The (original) entity ID to use if this GRF is not available
};

class OverrideManagerBase {
protected:
	uint16 *entity_overrides;
	uint32 *grfid_overrides;

	uint16 max_offset;       ///< what is the length of the original entity's array of specs
	uint16 max_new_entities; ///< what is the amount of entities, old and new summed

	uint16 invalid_ID;       ///< ID used to dected invalid entities;
	virtual bool CheckValidNewID(uint16 testid) { return true; }

public:
	EntityIDMapping *mapping_ID; ///< mapping of ids from grf files.  Public out of convenience

	OverrideManagerBase(uint16 offset, uint16 maximum, uint16 invalid);
	virtual ~OverrideManagerBase();

	void ResetOverride();
	void ResetMapping();

	void Add(uint8 local_id, uint32 grfid, uint entity_type);
	virtual uint16 AddEntityID(byte grf_local_id, uint32 grfid, byte substitute_id);

	uint16 GetSubstituteID(uint16 entity_id) const;
	virtual uint16 GetID(uint8 grf_local_id, uint32 grfid) const;

	inline uint16 GetMaxMapping() const { return max_new_entities; }
	inline uint16 GetMaxOffset() const { return max_offset; }
};


struct HouseSpec;
class HouseOverrideManager : public OverrideManagerBase {
public:
	HouseOverrideManager(uint16 offset, uint16 maximum, uint16 invalid) :
			OverrideManagerBase(offset, maximum, invalid) {}
	void SetEntitySpec(const HouseSpec *hs);
};


struct IndustrySpec;
class IndustryOverrideManager : public OverrideManagerBase {
public:
	IndustryOverrideManager(uint16 offset, uint16 maximum, uint16 invalid) :
			OverrideManagerBase(offset, maximum, invalid) {}

	virtual uint16 AddEntityID(byte grf_local_id, uint32 grfid, byte substitute_id);
	virtual uint16 GetID(uint8 grf_local_id, uint32 grfid) const;
	void SetEntitySpec(IndustrySpec *inds);
};


struct IndustryTileSpec;
class IndustryTileOverrideManager : public OverrideManagerBase {
protected:
	virtual bool CheckValidNewID(uint16 testid) { return testid != 0xFF; }
public:
	IndustryTileOverrideManager(uint16 offset, uint16 maximum, uint16 invalid) :
			OverrideManagerBase(offset, maximum, invalid) {}

	void SetEntitySpec(const IndustryTileSpec *indts);
};

struct AirportSpec;
class AirportOverrideManager : public OverrideManagerBase {
public:
	AirportOverrideManager(uint16 offset, uint16 maximum, uint16 invalid) :
			OverrideManagerBase(offset, maximum, invalid) {}

	void SetEntitySpec(AirportSpec *inds);
};

struct AirportTileSpec;
class AirportTileOverrideManager : public OverrideManagerBase {
protected:
	virtual bool CheckValidNewID(uint16 testid) { return testid != 0xFF; }
public:
	AirportTileOverrideManager(uint16 offset, uint16 maximum, uint16 invalid) :
			OverrideManagerBase(offset, maximum, invalid) {}

	void SetEntitySpec(const AirportTileSpec *ats);
};

struct ObjectSpec;
class ObjectOverrideManager : public OverrideManagerBase {
protected:
	virtual bool CheckValidNewID(uint16 testid) { return testid != 0xFF; }
public:
	ObjectOverrideManager(uint16 offset, uint16 maximum, uint16 invalid) :
			OverrideManagerBase(offset, maximum, invalid) {}

	void SetEntitySpec(ObjectSpec *spec);
};

extern HouseOverrideManager _house_mngr;
extern IndustryOverrideManager _industry_mngr;
extern IndustryTileOverrideManager _industile_mngr;
extern AirportOverrideManager _airport_mngr;
extern AirportTileOverrideManager _airporttile_mngr;
extern ObjectOverrideManager _object_mngr;

uint32 GetTerrainType(TileIndex tile, TileContext context = TCX_NORMAL);
TileIndex GetNearbyTile(byte parameter, TileIndex tile, bool signed_offsets = true);
uint32 GetNearbyTileInformation(TileIndex tile);

/**
 * Data related to the handling of grf files.
 * @tparam Tcnt Number of spritegroups
 */
template <size_t Tcnt>
struct GRFFilePropsBase {
	GRFFilePropsBase() : local_id(0), grffile(0)
	{
		/* The lack of some compilers to provide default constructors complying to the specs
		 * requires us to zero the stuff ourself. */
		memset(spritegroup, 0, sizeof(spritegroup));
	}

	uint16 local_id;                             ///< id defined by the grf file for this entity
	const struct GRFFile *grffile;               ///< grf file that introduced this entity
	const struct SpriteGroup *spritegroup[Tcnt]; ///< pointer to the different sprites of the entity
};

/** Data related to the handling of grf files. */
struct GRFFileProps : GRFFilePropsBase<1> {
	/** Set all default data constructor for the props. */
	GRFFileProps(uint16 subst_id) :
			GRFFilePropsBase<1>(), subst_id(subst_id), override(subst_id)
	{
	}

	/** Simple constructor for the props. */
	GRFFileProps() : GRFFilePropsBase<1>() {}
	uint16 subst_id;
	uint16 override;                      ///< id of the entity been replaced by
};

#endif /* NEWGRF_COMMONS_H */
