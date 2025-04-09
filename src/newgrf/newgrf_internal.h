/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_internal.h NewGRF internal processing state. */

#ifndef NEWGRF_INTERNAL_H
#define NEWGRF_INTERNAL_H

#include "../newgrf.h"
#include "../newgrf_commons.h"
#include "../newgrf_config.h"
#include "../spriteloader/sprite_file_type.hpp"
#include "newgrf_bytereader.h"

/** Possible return values for the GrfChangeInfoHandler functions */
enum ChangeInfoResult : uint8_t {
	CIR_SUCCESS,    ///< Variable was parsed and read
	CIR_DISABLED,   ///< GRF was disabled due to error
	CIR_UNHANDLED,  ///< Variable was parsed but unread
	CIR_UNKNOWN,    ///< Variable is unknown
	CIR_INVALID_ID, ///< Attempt to modify an invalid ID
};

/** GRF feature handler */
template <GrfSpecFeature TFeature>
struct GrfChangeInfoHandler {
	static ChangeInfoResult Reserve(uint first, uint last, int prop, ByteReader &buf);
	static ChangeInfoResult Activation(uint first, uint last, int prop, ByteReader &buf);
};

/** GRF action handler */
template <uint8_t TAction>
struct GrfActionHandler {
	static void FileScan(ByteReader &buf);
	static void SafetyScan(ByteReader &buf);
	static void LabelScan(ByteReader &buf);
	static void Init(ByteReader &buf);
	static void Reserve(ByteReader &buf);
	static void Activation(ByteReader &buf);
};

static constexpr uint MAX_SPRITEGROUP = UINT8_MAX; ///< Maximum GRF-local ID for a spritegroup.

/** Temporary data during loading of GRFs */
struct GrfProcessingState {
private:
	/** Definition of a single Action1 spriteset */
	struct SpriteSet {
		SpriteID sprite;  ///< SpriteID of the first sprite of the set.
		uint num_sprites; ///< Number of sprites in the set.
	};

	/** Currently referenceable spritesets */
	std::map<uint, SpriteSet> spritesets[GSF_END];

public:
	/* Global state */
	GrfLoadingStage stage;    ///< Current loading stage
	SpriteID spriteid;        ///< First available SpriteID for loading realsprites.

	/* Local state in the file */
	SpriteFile *file;         ///< File of currently processed GRF file.
	GRFFile *grffile;         ///< Currently processed GRF file.
	GRFConfig *grfconfig;     ///< Config of the currently processed GRF file.
	uint32_t nfo_line;          ///< Currently processed pseudo sprite number in the GRF.

	/* Kind of return values when processing certain actions */
	int skip_sprites;         ///< Number of pseudo sprites to skip before processing the next one. (-1 to skip to end of file)

	/* Currently referenceable spritegroups */
	std::array<const SpriteGroup *, MAX_SPRITEGROUP + 1> spritegroups{};

	/** Clear temporary data before processing the next file in the current loading stage */
	void ClearDataForNextFile()
	{
		this->nfo_line = 0;
		this->skip_sprites = 0;

		for (uint i = 0; i < GSF_END; i++) {
			this->spritesets[i].clear();
		}

		this->spritegroups = {};
	}

	/**
	 * Records new spritesets.
	 * @param feature GrfSpecFeature the set is defined for.
	 * @param first_sprite SpriteID of the first sprite in the set.
	 * @param first_set First spriteset to define.
	 * @param numsets Number of sets to define.
	 * @param numents Number of sprites per set to define.
	 */
	void AddSpriteSets(uint8_t feature, SpriteID first_sprite, uint first_set, uint numsets, uint numents)
	{
		assert(feature < GSF_END);
		for (uint i = 0; i < numsets; i++) {
			SpriteSet &set = this->spritesets[feature][first_set + i];
			set.sprite = first_sprite + i * numents;
			set.num_sprites = numents;
		}
	}

	/**
	 * Check whether there are any valid spritesets for a feature.
	 * @param feature GrfSpecFeature to check.
	 * @return true if there are any valid sets.
	 * @note Spritesets with zero sprites are valid to allow callback-failures.
	 */
	bool HasValidSpriteSets(uint8_t feature) const
	{
		assert(feature < GSF_END);
		return !this->spritesets[feature].empty();
	}

	/**
	 * Check whether a specific set is defined.
	 * @param feature GrfSpecFeature to check.
	 * @param set Set to check.
	 * @return true if the set is valid.
	 * @note Spritesets with zero sprites are valid to allow callback-failures.
	 */
	bool IsValidSpriteSet(uint8_t feature, uint set) const
	{
		assert(feature < GSF_END);
		return this->spritesets[feature].find(set) != this->spritesets[feature].end();
	}

	/**
	 * Returns the first sprite of a spriteset.
	 * @param feature GrfSpecFeature to query.
	 * @param set Set to query.
	 * @return First sprite of the set.
	 */
	SpriteID GetSprite(uint8_t feature, uint set) const
	{
		assert(IsValidSpriteSet(feature, set));
		return this->spritesets[feature].find(set)->second.sprite;
	}

	/**
	 * Returns the number of sprites in a spriteset
	 * @param feature GrfSpecFeature to query.
	 * @param set Set to query.
	 * @return Number of sprites in the set.
	 */
	uint GetNumEnts(uint8_t feature, uint set) const
	{
		assert(IsValidSpriteSet(feature, set));
		return this->spritesets[feature].find(set)->second.num_sprites;
	}
};

extern GrfProcessingState _cur_gps;

struct GRFLocation {
	uint32_t grfid;
	uint32_t nfoline;

	GRFLocation(uint32_t grfid, uint32_t nfoline) : grfid(grfid), nfoline(nfoline) { }

	bool operator <(const GRFLocation &other) const
	{
		return this->grfid < other.grfid || (this->grfid == other.grfid && this->nfoline < other.nfoline);
	}

	bool operator ==(const GRFLocation &other) const
	{
		return this->grfid == other.grfid && this->nfoline == other.nfoline;
	}
};

using GRFLineToSpriteOverride = std::map<GRFLocation, std::vector<uint8_t>>;

extern std::map<GRFLocation, std::pair<SpriteID, uint16_t>> _grm_sprites;
extern GRFLineToSpriteOverride _grf_line_to_action6_sprite_override;

extern GrfMiscBits _misc_grf_features;

void SetNewGRFOverride(uint32_t source_grfid, uint32_t target_grfid);
GRFFile *GetCurrentGRFOverride();

std::span<const CargoLabel> GetCargoTranslationTable(const GRFFile &grffile);
CargoTypes TranslateRefitMask(uint32_t refit_mask);

void SkipBadgeList(ByteReader &buf);

std::vector<BadgeID> ReadBadgeList(ByteReader &buf, GrfSpecFeature feature);

void MapSpriteMappingRecolour(PalSpriteID *grf_sprite);
TileLayoutFlags ReadSpriteLayoutSprite(ByteReader &buf, bool read_flags, bool invert_action1_flag, bool use_cur_spritesets, int feature, PalSpriteID *grf_sprite, uint16_t *max_sprite_offset = nullptr, uint16_t *max_palette_offset = nullptr);
bool ReadSpriteLayout(ByteReader &buf, uint num_building_sprites, bool use_cur_spritesets, uint8_t feature, bool allow_var10, bool no_z_position, NewGRFSpriteLayout *dts);

GRFFile *GetFileByGRFID(uint32_t grfid);
GRFError *DisableGrf(StringID message = {}, GRFConfig *config = nullptr);
void DisableStaticNewGRFInfluencingNonStaticNewGRFs(GRFConfig &c);
bool HandleChangeInfoResult(const char *caller, ChangeInfoResult cir, uint8_t feature, uint8_t property);
uint32_t GetParamVal(uint8_t param, uint32_t *cond_val);
void GRFUnsafe(ByteReader &);

void InitializePatchFlags();

#endif /* NEWGRF_INTERNAL_H */
