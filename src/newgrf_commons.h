/* $Id$ */

/** @file newgrf_commons.h This file simplyfies and embeds a common mechanism of
 * loading/saving and mapping of grf entities.
 */

#ifndef NEWGRF_COMMONS_H
#define NEWGRF_COMMONS_H

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

class OverrideManagerBase
{
protected:
	uint16 *entity_overrides;

	uint16 max_offset;       ///< what is the length of the original entity's array of specs
	uint16 max_new_entities; ///< what is the amount of entities, old and new summed

	uint16 invalid_ID;       ///< ID used to dected invalid entities;

	virtual uint16 AddEntityID(byte grf_local_id, uint32 grfid, byte substitute_id);
public:
	EntityIDMapping *mapping_ID; ///< mapping of ids from grf files.  Public out of convenience

	OverrideManagerBase(uint16 offset, uint16 maximum, uint16 invalid);
	virtual ~OverrideManagerBase();

	void ResetOverride();
	void ResetMapping();

	void Add(uint8 local_id, uint entity_type);

	uint16 GetSubstituteID(byte entity_id);
	uint16 GetID(uint8 grf_local_id, uint32 grfid);

	inline uint16 GetMaxMapping() { return max_new_entities; };
	inline uint16 GetMaxOffset() { return max_offset; };
};


struct HouseSpec;
class HouseOverrideManager : public OverrideManagerBase
{
public:
	HouseOverrideManager(uint16 offset, uint16 maximum, uint16 invalid) : OverrideManagerBase(offset, maximum, invalid) {};
	void SetEntitySpec(const HouseSpec *hs);
};


extern HouseOverrideManager _house_mngr;

#endif /* NEWGRF_COMMONS_H */
