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

#include "sprite.h"
#include "core/alloc_type.hpp"
#include "command_type.h"
#include "direction_type.h"
#include "company_type.h"

/** Context for tile accesses */
enum TileContext : uint8_t {
	TCX_NORMAL,         ///< Nothing special.
	TCX_UPPER_HALFTILE, ///< Querying information about the upper part of a tile with halftile foundation.
	TCX_ON_BRIDGE,      ///< Querying information about stuff on the bridge (via some bridgehead).
};

/**
 * Flags to enable register usage in sprite layouts.
 */
enum TileLayoutFlags : uint8_t {
	TLF_NOTHING           = 0x00,

	TLF_DODRAW            = 0x01,   ///< Only draw sprite if value of register TileLayoutRegisters::dodraw is non-zero.
	TLF_SPRITE            = 0x02,   ///< Add signed offset to sprite from register TileLayoutRegisters::sprite.
	TLF_PALETTE           = 0x04,   ///< Add signed offset to palette from register TileLayoutRegisters::palette.
	TLF_CUSTOM_PALETTE    = 0x08,   ///< Palette is from Action 1 (moved to SPRITE_MODIFIER_CUSTOM_SPRITE in palette during loading).

	TLF_BB_XY_OFFSET      = 0x10,   ///< Add signed offset to bounding box X and Y positions from register TileLayoutRegisters::delta.parent[0..1].
	TLF_BB_Z_OFFSET       = 0x20,   ///< Add signed offset to bounding box Z positions from register TileLayoutRegisters::delta.parent[2].

	TLF_CHILD_X_OFFSET    = 0x10,   ///< Add signed offset to child sprite X positions from register TileLayoutRegisters::delta.child[0].
	TLF_CHILD_Y_OFFSET    = 0x20,   ///< Add signed offset to child sprite Y positions from register TileLayoutRegisters::delta.child[1].

	TLF_SPRITE_VAR10      = 0x40,   ///< Resolve sprite with a specific value in variable 10.
	TLF_PALETTE_VAR10     = 0x80,   ///< Resolve palette with a specific value in variable 10.

	TLF_KNOWN_FLAGS       = 0xFF,   ///< Known flags. Any unknown set flag will disable the GRF.

	/** Flags which are still required after loading the GRF. */
	TLF_DRAWING_FLAGS     = TLF_KNOWN_FLAGS & ~TLF_CUSTOM_PALETTE,

	/** Flags which do not work for the (first) ground sprite. */
	TLF_NON_GROUND_FLAGS  = TLF_BB_XY_OFFSET | TLF_BB_Z_OFFSET | TLF_CHILD_X_OFFSET | TLF_CHILD_Y_OFFSET,

	/** Flags which refer to using multiple action-1-2-3 chains. */
	TLF_VAR10_FLAGS       = TLF_SPRITE_VAR10 | TLF_PALETTE_VAR10,

	/** Flags which require resolving the action-1-2-3 chain for the sprite, even if it is no action-1 sprite. */
	TLF_SPRITE_REG_FLAGS  = TLF_DODRAW | TLF_SPRITE | TLF_BB_XY_OFFSET | TLF_BB_Z_OFFSET | TLF_CHILD_X_OFFSET | TLF_CHILD_Y_OFFSET,

	/** Flags which require resolving the action-1-2-3 chain for the palette, even if it is no action-1 palette. */
	TLF_PALETTE_REG_FLAGS = TLF_PALETTE,
};
DECLARE_ENUM_AS_BIT_SET(TileLayoutFlags)

/**
 * Determines which sprite to use from a spriteset for a specific construction stage.
 * @param construction_stage Construction stage 0 - 3.
 * @param num_sprites Number of available sprites to select stage from.
 * @return Sprite to use
 */
inline uint GetConstructionStageOffset(uint construction_stage, uint num_sprites)
{
	assert(num_sprites > 0);
	if (num_sprites > 4) num_sprites = 4;
	switch (construction_stage) {
		case 0: return 0;
		case 1: return num_sprites > 2 ? 1 : 0;
		case 2: return num_sprites > 2 ? num_sprites - 2 : 0;
		case 3: return num_sprites - 1;
		default: NOT_REACHED();
	}
}

/**
 * Additional modifiers for items in sprite layouts.
 */
struct TileLayoutRegisters {
	TileLayoutFlags flags; ///< Flags defining which members are valid and to be used.
	uint8_t dodraw;          ///< Register deciding whether the sprite shall be drawn at all. Non-zero means drawing.
	uint8_t sprite;          ///< Register specifying a signed offset for the sprite.
	uint8_t palette;         ///< Register specifying a signed offset for the palette.
	uint16_t max_sprite_offset;  ///< Maximum offset to add to the sprite. (limited by size of the spriteset)
	uint16_t max_palette_offset; ///< Maximum offset to add to the palette. (limited by size of the spriteset)
	union {
		uint8_t parent[3];   ///< Registers for signed offsets for the bounding box position of parent sprites.
		uint8_t child[2];    ///< Registers for signed offsets for the position of child sprites.
	} delta;
	uint8_t sprite_var10;    ///< Value for variable 10 when resolving the sprite.
	uint8_t palette_var10;   ///< Value for variable 10 when resolving the palette.
};

static const uint TLR_MAX_VAR10 = 7; ///< Maximum value for var 10.

/**
 * NewGRF supplied spritelayout.
 * In contrast to #DrawTileSprites this struct is for allocated
 * layouts on the heap. It allocates data and frees them on destruction.
 */
struct NewGRFSpriteLayout : DrawTileSprites {
	std::vector<DrawTileSeqStruct> seq{};
	std::vector<TileLayoutRegisters> registers{};

	/**
	 * Number of sprites in all referenced spritesets.
	 * If these numbers are inconsistent, then this is 0 and the real values are in \c registers.
	 */
	uint consistent_max_offset = 0;

	void Allocate(uint num_sprites);
	void AllocateRegisters();

	/**
	 * Tests whether this spritelayout needs preprocessing by
	 * #PrepareLayout() and #ProcessRegisters(), or whether it can be
	 * used directly.
	 * @return true if preprocessing is needed
	 */
	bool NeedsPreprocessing() const
	{
		return !this->registers.empty();
	}

	uint32_t PrepareLayout(uint32_t orig_offset, uint32_t newgrf_ground_offset, uint32_t newgrf_offset, uint constr_stage, bool separate_ground) const;
	void ProcessRegisters(uint8_t resolved_var10, uint32_t resolved_sprite, bool separate_ground) const;

	/**
	 * Returns the result spritelayout after preprocessing.
	 * @pre #PrepareLayout() and #ProcessRegisters() need calling first.
	 * @return result spritelayout
	 */
	std::span<DrawTileSeqStruct> GetLayout(PalSpriteID *ground) const
	{
		*ground = result_seq[0].image;
		return {++result_seq.begin(), result_seq.end()};
	}

	std::span<const DrawTileSeqStruct> GetSequence() const override { return {this->seq.begin(), this->seq.end()}; }


private:
	static std::vector<DrawTileSeqStruct> result_seq; ///< Temporary storage when preprocessing spritelayouts.
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
	uint32_t grfid;          ///< The GRF ID of the file the entity belongs to
	uint16_t entity_id; ///< The entity ID within the GRF file
	uint16_t substitute_id; ///< The (original) entity ID to use if this GRF is not available
};

class OverrideManagerBase {
protected:
	std::vector<uint16_t> entity_overrides;
	std::vector<uint32_t> grfid_overrides;

	uint16_t max_offset;   ///< what is the length of the original entity's array of specs
	uint16_t max_entities; ///< what is the amount of entities, old and new summed

	uint16_t invalid_id;   ///< ID used to detected invalid entities
	virtual bool CheckValidNewID([[maybe_unused]] uint16_t testid) { return true; }

public:
	std::vector<EntityIDMapping> mappings; ///< mapping of ids from grf files.  Public out of convenience

	OverrideManagerBase(uint16_t offset, uint16_t maximum, uint16_t invalid);
	virtual ~OverrideManagerBase() = default;

	void ResetOverride();
	void ResetMapping();

	void Add(uint16_t local_id, uint32_t grfid, uint entity_type);
	virtual uint16_t AddEntityID(uint16_t grf_local_id, uint32_t grfid, uint16_t substitute_id);

	uint32_t GetGRFID(uint16_t entity_id) const;
	uint16_t GetSubstituteID(uint16_t entity_id) const;
	virtual uint16_t GetID(uint16_t grf_local_id, uint32_t grfid) const;

	inline uint16_t GetMaxMapping() const { return this->max_entities; }
	inline uint16_t GetMaxOffset() const { return this->max_offset; }
};


struct HouseSpec;
class HouseOverrideManager : public OverrideManagerBase {
public:
	HouseOverrideManager(uint16_t offset, uint16_t maximum, uint16_t invalid) :
			OverrideManagerBase(offset, maximum, invalid) {}

	void SetEntitySpec(const HouseSpec *hs);
};


struct IndustrySpec;
class IndustryOverrideManager : public OverrideManagerBase {
public:
	IndustryOverrideManager(uint16_t offset, uint16_t maximum, uint16_t invalid) :
			OverrideManagerBase(offset, maximum, invalid) {}

	uint16_t AddEntityID(uint16_t grf_local_id, uint32_t grfid, uint16_t substitute_id) override;
	uint16_t GetID(uint16_t grf_local_id, uint32_t grfid) const override;

	void SetEntitySpec(IndustrySpec *inds);
};


struct IndustryTileSpec;
class IndustryTileOverrideManager : public OverrideManagerBase {
protected:
	bool CheckValidNewID(uint16_t testid) override { return testid != 0xFF; }
public:
	IndustryTileOverrideManager(uint16_t offset, uint16_t maximum, uint16_t invalid) :
			OverrideManagerBase(offset, maximum, invalid) {}

	void SetEntitySpec(const IndustryTileSpec *indts);
};

struct AirportSpec;
class AirportOverrideManager : public OverrideManagerBase {
public:
	AirportOverrideManager(uint16_t offset, uint16_t maximum, uint16_t invalid) :
			OverrideManagerBase(offset, maximum, invalid) {}

	void SetEntitySpec(AirportSpec *inds);
};

struct AirportTileSpec;
class AirportTileOverrideManager : public OverrideManagerBase {
protected:
	bool CheckValidNewID(uint16_t testid) override { return testid != 0xFF; }
public:
	AirportTileOverrideManager(uint16_t offset, uint16_t maximum, uint16_t invalid) :
			OverrideManagerBase(offset, maximum, invalid) {}

	void SetEntitySpec(const AirportTileSpec *ats);
};

struct ObjectSpec;
class ObjectOverrideManager : public OverrideManagerBase {
protected:
	bool CheckValidNewID(uint16_t testid) override { return testid != 0xFF; }
public:
	ObjectOverrideManager(uint16_t offset, uint16_t maximum, uint16_t invalid) :
			OverrideManagerBase(offset, maximum, invalid) {}

	void SetEntitySpec(ObjectSpec *spec);
};

extern HouseOverrideManager _house_mngr;
extern IndustryOverrideManager _industry_mngr;
extern IndustryTileOverrideManager _industile_mngr;
extern AirportOverrideManager _airport_mngr;
extern AirportTileOverrideManager _airporttile_mngr;
extern ObjectOverrideManager _object_mngr;

uint32_t GetTerrainType(TileIndex tile, TileContext context = TCX_NORMAL);
TileIndex GetNearbyTile(uint8_t parameter, TileIndex tile, bool signed_offsets = true, Axis axis = INVALID_AXIS);
uint32_t GetNearbyTileInformation(TileIndex tile, bool grf_version8);
uint32_t GetCompanyInfo(CompanyID owner, const struct Livery *l = nullptr);
CommandCost GetErrorMessageFromLocationCallbackResult(uint16_t cb_res, const GRFFile *grffile, StringID default_error);

void ErrorUnknownCallbackResult(uint32_t grfid, uint16_t cbid, uint16_t cb_res);
bool ConvertBooleanCallback(const struct GRFFile *grffile, uint16_t cbid, uint16_t cb_res);
bool Convert8bitBooleanCallback(const struct GRFFile *grffile, uint16_t cbid, uint16_t cb_res);

/**
 * Base data related to the handling of grf files.
 */
struct GRFFilePropsBase {
	uint16_t local_id = 0; ///< id defined by the grf file for this entity
	uint32_t grfid = 0; ///< grfid that introduced this entity.
	const struct GRFFile *grffile = nullptr; ///< grf file that introduced this entity

	void SetGRFFile(const struct GRFFile *grffile);

	/**
	 * Test if this entity was introduced by NewGRF.
	 * @returns true iff the grfid property is set.
	 */
	inline bool HasGrfFile() const { return this->grffile != nullptr; }
};

/**
 * Fixed-length list of sprite groups for an entity.
 * @tparam Tcount Number of spritegroups
 */
template <size_t Tcount>
struct FixedGRFFileProps : GRFFilePropsBase {
	std::array<const struct SpriteGroup *, Tcount> spritegroups{}; ///< pointers to the different sprite groups of the entity

	/**
	 * Get the SpriteGroup at the specified index.
	 * @param index Index to get.
	 * @returns SpriteGroup at index, or nullptr if not present.
	 */
	const struct SpriteGroup *GetSpriteGroup(size_t index = 0) const { return this->spritegroups[index]; }

	/**
	 * Set the SpriteGroup at the specified index.
	 * @param index Index to set.
	 * @param spritegroup SpriteGroup to set.
	 */
	void SetSpriteGroup(size_t index, const struct SpriteGroup *spritegroup) { this->spritegroups[index] = spritegroup; }
};

/**
 * Variable-length list of sprite groups for an entity.
 */
struct VariableGRFFileProps : GRFFilePropsBase {
	using CargoSpriteGroup = std::pair<size_t, const struct SpriteGroup *>;
	std::vector<CargoSpriteGroup> spritegroups; ///< pointers to the different sprite groups of the entity

	const struct SpriteGroup *GetSpriteGroup(size_t index) const;
	void SetSpriteGroup(size_t index, const struct SpriteGroup *spritegroup);
};

/** Data related to the handling of grf files. */
struct GRFFileProps : FixedGRFFileProps<1> {
	/** Set all default data constructor for the props. */
	constexpr GRFFileProps(uint16_t subst_id = 0) : subst_id(subst_id), override_id(subst_id) {}

	uint16_t subst_id;
	uint16_t override_id; ///< id of the entity been replaced by
};

/** Container for a label for rail or road type conversion. */
template <typename T>
struct LabelObject {
	T label = {}; ///< Label of rail or road type.
	uint8_t subtype = 0; ///< Subtype of type (road or tram).
};

#endif /* NEWGRF_COMMONS_H */
