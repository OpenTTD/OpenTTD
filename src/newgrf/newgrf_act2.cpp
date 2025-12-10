/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act2.cpp NewGRF Action 0x02 handler. */

#include "../stdafx.h"
#include <ranges>
#include "../debug.h"
#include "../newgrf_engine.h"
#include "../newgrf_cargo.h"
#include "../error.h"
#include "../vehicle_base.h"
#include "../road.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "table/strings.h"

#include "../safeguards.h"

constexpr uint16_t GROUPID_CALLBACK_FAILED = 0x7FFF; ///< Explicit "failure" result.
constexpr uint16_t GROUPID_CALCULATED_RESULT = 0x7FFE; ///< Return calculated result from VarAction2.

/**
 * Map the colour modifiers of TTDPatch to those that Open is using.
 * @param grf_sprite Pointer to the structure been modified.
 */
void MapSpriteMappingRecolour(PalSpriteID *grf_sprite)
{
	if (HasBit(grf_sprite->pal, 14)) {
		ClrBit(grf_sprite->pal, 14);
		SetBit(grf_sprite->sprite, SPRITE_MODIFIER_OPAQUE);
	}

	if (HasBit(grf_sprite->sprite, 14)) {
		ClrBit(grf_sprite->sprite, 14);
		SetBit(grf_sprite->sprite, PALETTE_MODIFIER_TRANSPARENT);
	}

	if (HasBit(grf_sprite->sprite, 15)) {
		ClrBit(grf_sprite->sprite, 15);
		SetBit(grf_sprite->sprite, PALETTE_MODIFIER_COLOUR);
	}
}

/**
 * Read a sprite and a palette from the GRF and convert them into a format
 * suitable to OpenTTD.
 * @param buf                 Input stream.
 * @param read_flags          Whether to read TileLayoutFlags.
 * @param invert_action1_flag Set to true, if palette bit 15 means 'not from action 1'.
 * @param use_cur_spritesets  Whether to use currently referenceable action 1 sets.
 * @param feature             GrfSpecFeature to use spritesets from.
 * @param[out] grf_sprite     Read sprite and palette.
 * @param[out] max_sprite_offset  Optionally returns the number of sprites in the spriteset of the sprite. (0 if no spritset)
 * @param[out] max_palette_offset Optionally returns the number of sprites in the spriteset of the palette. (0 if no spritset)
 * @return Read TileLayoutFlags.
 */
TileLayoutFlags ReadSpriteLayoutSprite(ByteReader &buf, bool read_flags, bool invert_action1_flag, bool use_cur_spritesets, GrfSpecFeature feature, PalSpriteID *grf_sprite, uint16_t *max_sprite_offset, uint16_t *max_palette_offset)
{
	grf_sprite->sprite = buf.ReadWord();
	grf_sprite->pal = buf.ReadWord();
	TileLayoutFlags flags = read_flags ? (TileLayoutFlags)buf.ReadWord() : TLF_NOTHING;

	MapSpriteMappingRecolour(grf_sprite);

	bool custom_sprite = HasBit(grf_sprite->pal, 15) != invert_action1_flag;
	ClrBit(grf_sprite->pal, 15);
	if (custom_sprite) {
		/* Use sprite from Action 1 */
		uint index = GB(grf_sprite->sprite, 0, 14);
		if (use_cur_spritesets && (!_cur_gps.IsValidSpriteSet(feature, index) || _cur_gps.GetNumEnts(feature, index) == 0)) {
			GrfMsg(1, "ReadSpriteLayoutSprite: Spritelayout uses undefined custom spriteset {}", index);
			grf_sprite->sprite = SPR_IMG_QUERY;
			grf_sprite->pal = PAL_NONE;
		} else {
			SpriteID sprite = use_cur_spritesets ? _cur_gps.GetSprite(feature, index) : index;
			if (max_sprite_offset != nullptr) *max_sprite_offset = use_cur_spritesets ? _cur_gps.GetNumEnts(feature, index) : UINT16_MAX;
			SB(grf_sprite->sprite, 0, SPRITE_WIDTH, sprite);
			SetBit(grf_sprite->sprite, SPRITE_MODIFIER_CUSTOM_SPRITE);
		}
	} else if ((flags & TLF_SPRITE_VAR10) && !(flags & TLF_SPRITE_REG_FLAGS)) {
		GrfMsg(1, "ReadSpriteLayoutSprite: Spritelayout specifies var10 value for non-action-1 sprite");
		DisableGrf(STR_NEWGRF_ERROR_INVALID_SPRITE_LAYOUT);
		return flags;
	}

	if (flags & TLF_CUSTOM_PALETTE) {
		/* Use palette from Action 1 */
		uint index = GB(grf_sprite->pal, 0, 14);
		if (use_cur_spritesets && (!_cur_gps.IsValidSpriteSet(feature, index) || _cur_gps.GetNumEnts(feature, index) == 0)) {
			GrfMsg(1, "ReadSpriteLayoutSprite: Spritelayout uses undefined custom spriteset {} for 'palette'", index);
			grf_sprite->pal = PAL_NONE;
		} else {
			SpriteID sprite = use_cur_spritesets ? _cur_gps.GetSprite(feature, index) : index;
			if (max_palette_offset != nullptr) *max_palette_offset = use_cur_spritesets ? _cur_gps.GetNumEnts(feature, index) : UINT16_MAX;
			SB(grf_sprite->pal, 0, SPRITE_WIDTH, sprite);
			SetBit(grf_sprite->pal, SPRITE_MODIFIER_CUSTOM_SPRITE);
		}
	} else if ((flags & TLF_PALETTE_VAR10) && !(flags & TLF_PALETTE_REG_FLAGS)) {
		GrfMsg(1, "ReadSpriteLayoutRegisters: Spritelayout specifies var10 value for non-action-1 palette");
		DisableGrf(STR_NEWGRF_ERROR_INVALID_SPRITE_LAYOUT);
		return flags;
	}

	return flags;
}

/**
 * Preprocess the TileLayoutFlags and read register modifiers from the GRF.
 * @param buf        Input stream.
 * @param flags      TileLayoutFlags to process.
 * @param is_parent  Whether the sprite is a parentsprite with a bounding box.
 * @param dts        Sprite layout to insert data into.
 * @param index      Sprite index to process; 0 for ground sprite.
 */
static void ReadSpriteLayoutRegisters(ByteReader &buf, TileLayoutFlags flags, bool is_parent, NewGRFSpriteLayout *dts, uint index)
{
	if (!(flags & TLF_DRAWING_FLAGS)) return;

	if (dts->registers.empty()) dts->AllocateRegisters();
	TileLayoutRegisters &regs = const_cast<TileLayoutRegisters&>(dts->registers[index]);
	regs.flags = flags & TLF_DRAWING_FLAGS;

	if (flags & TLF_DODRAW)  regs.dodraw  = buf.ReadByte();
	if (flags & TLF_SPRITE)  regs.sprite  = buf.ReadByte();
	if (flags & TLF_PALETTE) regs.palette = buf.ReadByte();

	if (is_parent) {
		if (flags & TLF_BB_XY_OFFSET) {
			regs.delta.parent[0] = buf.ReadByte();
			regs.delta.parent[1] = buf.ReadByte();
		}
		if (flags & TLF_BB_Z_OFFSET)    regs.delta.parent[2] = buf.ReadByte();
	} else {
		if (flags & TLF_CHILD_X_OFFSET) regs.delta.child[0]  = buf.ReadByte();
		if (flags & TLF_CHILD_Y_OFFSET) regs.delta.child[1]  = buf.ReadByte();
	}

	if (flags & TLF_SPRITE_VAR10) {
		regs.sprite_var10 = buf.ReadByte();
		if (regs.sprite_var10 > TLR_MAX_VAR10) {
			GrfMsg(1, "ReadSpriteLayoutRegisters: Spritelayout specifies var10 ({}) exceeding the maximal allowed value {}", regs.sprite_var10, TLR_MAX_VAR10);
			DisableGrf(STR_NEWGRF_ERROR_INVALID_SPRITE_LAYOUT);
			return;
		}
	}

	if (flags & TLF_PALETTE_VAR10) {
		regs.palette_var10 = buf.ReadByte();
		if (regs.palette_var10 > TLR_MAX_VAR10) {
			GrfMsg(1, "ReadSpriteLayoutRegisters: Spritelayout specifies var10 ({}) exceeding the maximal allowed value {}", regs.palette_var10, TLR_MAX_VAR10);
			DisableGrf(STR_NEWGRF_ERROR_INVALID_SPRITE_LAYOUT);
			return;
		}
	}
}

/**
 * Read a spritelayout from the GRF.
 * @param buf                  Input
 * @param num_building_sprites Number of building sprites to read
 * @param use_cur_spritesets   Whether to use currently referenceable action 1 sets.
 * @param feature              GrfSpecFeature to use spritesets from.
 * @param allow_var10          Whether the spritelayout may specify var10 values for resolving multiple action-1-2-3 chains
 * @param no_z_position        Whether bounding boxes have no Z offset
 * @param dts                  Layout container to output into
 * @return True on error (GRF was disabled).
 */
bool ReadSpriteLayout(ByteReader &buf, uint num_building_sprites, bool use_cur_spritesets, GrfSpecFeature feature, bool allow_var10, bool no_z_position, NewGRFSpriteLayout *dts)
{
	bool has_flags = HasBit(num_building_sprites, 6);
	ClrBit(num_building_sprites, 6);
	TileLayoutFlags valid_flags = TLF_KNOWN_FLAGS;
	if (!allow_var10) valid_flags &= ~TLF_VAR10_FLAGS;
	dts->Allocate(num_building_sprites); // allocate before reading groundsprite flags

	std::vector<uint16_t> max_sprite_offset(num_building_sprites + 1, 0);
	std::vector<uint16_t> max_palette_offset(num_building_sprites + 1, 0);

	/* Groundsprite */
	TileLayoutFlags flags = ReadSpriteLayoutSprite(buf, has_flags, false, use_cur_spritesets, feature, &dts->ground, max_sprite_offset.data(), max_palette_offset.data());
	if (_cur_gps.skip_sprites < 0) return true;

	if (flags & ~(valid_flags & ~TLF_NON_GROUND_FLAGS)) {
		GrfMsg(1, "ReadSpriteLayout: Spritelayout uses invalid flag 0x{:X} for ground sprite", flags & ~(valid_flags & ~TLF_NON_GROUND_FLAGS));
		DisableGrf(STR_NEWGRF_ERROR_INVALID_SPRITE_LAYOUT);
		return true;
	}

	ReadSpriteLayoutRegisters(buf, flags, false, dts, 0);
	if (_cur_gps.skip_sprites < 0) return true;

	for (uint i = 0; i < num_building_sprites; i++) {
		DrawTileSeqStruct *seq = const_cast<DrawTileSeqStruct*>(&dts->seq[i]);

		flags = ReadSpriteLayoutSprite(buf, has_flags, false, use_cur_spritesets, feature, &seq->image, max_sprite_offset.data() + i + 1, max_palette_offset.data() + i + 1);
		if (_cur_gps.skip_sprites < 0) return true;

		if (flags & ~valid_flags) {
			GrfMsg(1, "ReadSpriteLayout: Spritelayout uses unknown flag 0x{:X}", flags & ~valid_flags);
			DisableGrf(STR_NEWGRF_ERROR_INVALID_SPRITE_LAYOUT);
			return true;
		}

		seq->origin.x = buf.ReadByte();
		seq->origin.y = buf.ReadByte();

		if (!no_z_position) seq->origin.z = buf.ReadByte();

		if (seq->IsParentSprite()) {
			seq->extent.x = buf.ReadByte();
			seq->extent.y = buf.ReadByte();
			seq->extent.z = buf.ReadByte();
		}

		ReadSpriteLayoutRegisters(buf, flags, seq->IsParentSprite(), dts, i + 1);
		if (_cur_gps.skip_sprites < 0) return true;
	}

	/* Check if the number of sprites per spriteset is consistent */
	bool is_consistent = true;
	dts->consistent_max_offset = 0;
	for (uint i = 0; i < num_building_sprites + 1; i++) {
		if (max_sprite_offset[i] > 0) {
			if (dts->consistent_max_offset == 0) {
				dts->consistent_max_offset = max_sprite_offset[i];
			} else if (dts->consistent_max_offset != max_sprite_offset[i]) {
				is_consistent = false;
				break;
			}
		}
		if (max_palette_offset[i] > 0) {
			if (dts->consistent_max_offset == 0) {
				dts->consistent_max_offset = max_palette_offset[i];
			} else if (dts->consistent_max_offset != max_palette_offset[i]) {
				is_consistent = false;
				break;
			}
		}
	}

	/* When the Action1 sets are unknown, everything should be 0 (no spriteset usage) or UINT16_MAX (some spriteset usage) */
	assert(use_cur_spritesets || (is_consistent && (dts->consistent_max_offset == 0 || dts->consistent_max_offset == UINT16_MAX)));

	if (!is_consistent || !dts->registers.empty()) {
		dts->consistent_max_offset = 0;
		if (dts->registers.empty()) dts->AllocateRegisters();

		for (uint i = 0; i < num_building_sprites + 1; i++) {
			TileLayoutRegisters &regs = const_cast<TileLayoutRegisters&>(dts->registers[i]);
			regs.max_sprite_offset = max_sprite_offset[i];
			regs.max_palette_offset = max_palette_offset[i];
		}
	}

	return false;
}

using CachedCallback = std::pair<uint16_t, SpriteGroupID>;
static std::vector<CachedCallback> _cached_callback_groups; ///< Sorted list of cached callback result spritegroups.

void ResetCallbacks(bool final)
{
	_cached_callback_groups.clear();
	if (final) _cached_callback_groups.shrink_to_fit();
}

static const SpriteGroup *GetCallbackResultGroup(uint16_t value)
{
	/* Old style callback results (only valid for version < 8) have the highest byte 0xFF to signify it is a callback result.
	 * New style ones only have the highest bit set (allows 15-bit results, instead of just 8) */
	if (_cur_gps.grffile->grf_version < 8 && GB(value, 8, 8) == 0xFF) {
		value &= ~0xFF00;
	} else {
		value &= ~0x8000;
	}

	/* Find position for value within the cached callback list. */
	auto it = std::ranges::lower_bound(_cached_callback_groups, value, std::less{}, &CachedCallback::first);
	if (it != std::end(_cached_callback_groups) && it->first == value) return SpriteGroup::Get(it->second);

	/* Result value is not present, so make it and add to cache. */
	assert(CallbackResultSpriteGroup::CanAllocateItem());
	const SpriteGroup *group = new CallbackResultSpriteGroup(value);
	it = _cached_callback_groups.emplace(it, value, group->index);
	return group;
}

/* Helper function to either create a callback or link to a previously
 * defined spritegroup. */
static const SpriteGroup *GetGroupFromGroupID(uint8_t setid, uint8_t type, uint16_t groupid)
{
	if (HasBit(groupid, 15)) return GetCallbackResultGroup(groupid);
	if (groupid == GROUPID_CALLBACK_FAILED) return nullptr;

	if (groupid > MAX_SPRITEGROUP || _cur_gps.spritegroups[groupid] == nullptr) {
		GrfMsg(1, "GetGroupFromGroupID(0x{:02X}:0x{:02X}): Groupid 0x{:04X} does not exist, leaving empty", setid, type, groupid);
		return nullptr;
	}

	return _cur_gps.spritegroups[groupid];
}

/**
 * Helper function to either create a callback or a result sprite group.
 * @param feature GrfSpecFeature to define spritegroup for.
 * @param setid SetID of the currently being parsed Action2. (only for debug output)
 * @param type Type of the currently being parsed Action2. (only for debug output)
 * @param spriteid Raw value from the GRF for the new spritegroup; describes either the return value or the referenced spritegroup.
 * @return Created spritegroup.
 */
static const SpriteGroup *CreateGroupFromGroupID(GrfSpecFeature feature, uint8_t setid, uint8_t type, uint16_t spriteid)
{
	if (HasBit(spriteid, 15)) return GetCallbackResultGroup(spriteid);

	if (!_cur_gps.IsValidSpriteSet(feature, spriteid)) {
		GrfMsg(1, "CreateGroupFromGroupID(0x{:02X}:0x{:02X}): Sprite set {} invalid", setid, type, spriteid);
		return nullptr;
	}

	SpriteID spriteset_start = _cur_gps.GetSprite(feature, spriteid);
	uint num_sprites = _cur_gps.GetNumEnts(feature, spriteid);

	/* Ensure that the sprites are loeded */
	assert(spriteset_start + num_sprites <= _cur_gps.spriteid);

	assert(ResultSpriteGroup::CanAllocateItem());
	return new ResultSpriteGroup(spriteset_start, num_sprites);
}

/* Action 0x02 */
static void NewSpriteGroup(ByteReader &buf)
{
	/* <02> <feature> <set-id> <type/num-entries> <feature-specific-data...>
	 *
	 * B feature       see action 1
	 * B set-id        ID of this particular definition
	 * B type/num-entries
	 *                 if 80 or greater, this is a randomized or variational
	 *                 list definition, see below
	 *                 otherwise it specifies a number of entries, the exact
	 *                 meaning depends on the feature
	 * V feature-specific-data (huge mess, don't even look it up --pasky) */
	const SpriteGroup *act_group = nullptr;

	GrfSpecFeature feature{buf.ReadByte()};
	if (feature >= GSF_END) {
		GrfMsg(1, "NewSpriteGroup: Unsupported feature 0x{:02X}, skipping", feature);
		return;
	}

	uint8_t setid   = buf.ReadByte();
	uint8_t type    = buf.ReadByte();

	/* Sprite Groups are created here but they are allocated from a pool, so
	 * we do not need to delete anything if there is an exception from the
	 * ByteReader. */

	switch (type) {
		/* Deterministic Sprite Group */
		case 0x81: // Self scope, byte
		case 0x82: // Parent scope, byte
		case 0x85: // Self scope, word
		case 0x86: // Parent scope, word
		case 0x89: // Self scope, dword
		case 0x8A: // Parent scope, dword
		{
			uint8_t varadjust;
			uint8_t varsize;

			assert(DeterministicSpriteGroup::CanAllocateItem());
			DeterministicSpriteGroup *group = new DeterministicSpriteGroup();
			group->nfo_line = _cur_gps.nfo_line;
			act_group = group;
			group->var_scope = HasBit(type, 1) ? VSG_SCOPE_PARENT : VSG_SCOPE_SELF;

			switch (GB(type, 2, 2)) {
				default: NOT_REACHED();
				case 0: group->size = DSG_SIZE_BYTE;  varsize = 1; break;
				case 1: group->size = DSG_SIZE_WORD;  varsize = 2; break;
				case 2: group->size = DSG_SIZE_DWORD; varsize = 4; break;
			}

			/* Loop through the var adjusts. Unfortunately we don't know how many we have
			 * from the outset, so we shall have to keep reallocing. */
			do {
				DeterministicSpriteGroupAdjust &adjust = group->adjusts.emplace_back();

				/* The first var adjust doesn't have an operation specified, so we set it to add. */
				adjust.operation = group->adjusts.size() == 1 ? DSGA_OP_ADD : (DeterministicSpriteGroupAdjustOperation)buf.ReadByte();
				adjust.variable  = buf.ReadByte();
				if (adjust.variable == 0x7E) {
					/* Link subroutine group */
					adjust.subroutine = GetGroupFromGroupID(setid, type, buf.ReadByte());
				} else {
					adjust.parameter = IsInsideMM(adjust.variable, 0x60, 0x80) ? buf.ReadByte() : 0;
				}

				varadjust = buf.ReadByte();
				adjust.shift_num = GB(varadjust, 0, 5);
				adjust.type      = (DeterministicSpriteGroupAdjustType)GB(varadjust, 6, 2);
				adjust.and_mask  = buf.ReadVarSize(varsize);

				if (adjust.type != DSGA_TYPE_NONE) {
					adjust.add_val    = buf.ReadVarSize(varsize);
					adjust.divmod_val = buf.ReadVarSize(varsize);
					if (adjust.divmod_val == 0) adjust.divmod_val = 1; // Ensure that divide by zero cannot occur
				} else {
					adjust.add_val    = 0;
					adjust.divmod_val = 0;
				}

				/* Continue reading var adjusts while bit 5 is set. */
			} while (HasBit(varadjust, 5));

			std::vector<DeterministicSpriteGroupRange> ranges;
			ranges.resize(buf.ReadByte());
			for (auto &range : ranges) {
				auto groupid = buf.ReadWord();
				if (groupid == GROUPID_CALCULATED_RESULT) {
					range.result.calculated_result = true;
				} else {
					range.result.group = GetGroupFromGroupID(setid, type, groupid);
				}
				range.low   = buf.ReadVarSize(varsize);
				range.high  = buf.ReadVarSize(varsize);
			}

			auto defgroupid = buf.ReadWord();
			if (defgroupid == GROUPID_CALCULATED_RESULT) {
				group->default_result.calculated_result = true;
			} else {
				group->default_result.group = GetGroupFromGroupID(setid, type, defgroupid);
			}
			/* 'calculated_result' makes no sense for the 'error' case. Use callback failure (nullptr) instead */
			group->error_group = ranges.empty() ? group->default_result.group : ranges[0].result.group;
			/* nvar == 0 is a special case:
			 * - set "default_result" to "calculated_result".
			 * - the old value specifies the "error_group". */
			if (ranges.empty()) {
				group->default_result.calculated_result = true;
				group->default_result.group = nullptr;
			}

			/* Sort ranges ascending. When ranges overlap, this may required clamping or splitting them */
			std::vector<uint32_t> bounds;
			bounds.reserve(ranges.size());
			for (const auto &range : ranges) {
				bounds.push_back(range.low);
				if (range.high != UINT32_MAX) bounds.push_back(range.high + 1);
			}
			std::sort(bounds.begin(), bounds.end());
			bounds.erase(std::unique(bounds.begin(), bounds.end()), bounds.end());

			std::vector<DeterministicSpriteGroupResult> target;
			target.reserve(bounds.size());
			for (const auto &bound : bounds) {
				auto t = group->default_result;
				for (const auto &range : ranges) {
					if (range.low <= bound && bound <= range.high) {
						t = range.result;
						break;
					}
				}
				target.push_back(t);
			}
			assert(target.size() == bounds.size());

			for (uint j = 0; j < bounds.size(); ) {
				if (target[j] != group->default_result) {
					DeterministicSpriteGroupRange &r = group->ranges.emplace_back();
					r.result = target[j];
					r.low = bounds[j];
					while (j < bounds.size() && target[j] == r.result) {
						j++;
					}
					r.high = j < bounds.size() ? bounds[j] - 1 : UINT32_MAX;
				} else {
					j++;
				}
			}

			break;
		}

		/* Randomized Sprite Group */
		case 0x80: // Self scope
		case 0x83: // Parent scope
		case 0x84: // Relative scope
		{
			assert(RandomizedSpriteGroup::CanAllocateItem());
			RandomizedSpriteGroup *group = new RandomizedSpriteGroup();
			group->nfo_line = _cur_gps.nfo_line;
			act_group = group;
			group->var_scope = HasBit(type, 1) ? VSG_SCOPE_PARENT : VSG_SCOPE_SELF;

			if (HasBit(type, 2)) {
				if (feature <= GSF_AIRCRAFT) group->var_scope = VSG_SCOPE_RELATIVE;
				group->count = buf.ReadByte();
			}

			uint8_t triggers = buf.ReadByte();
			group->triggers       = GB(triggers, 0, 7);
			group->cmp_mode       = HasBit(triggers, 7) ? RSG_CMP_ALL : RSG_CMP_ANY;
			group->lowest_randbit = buf.ReadByte();

			uint8_t num_groups = buf.ReadByte();
			if (!HasExactlyOneBit(num_groups)) {
				GrfMsg(1, "NewSpriteGroup: Random Action 2 nrand should be power of 2");
			}

			group->groups.reserve(num_groups);
			for (uint i = 0; i < num_groups; i++) {
				group->groups.push_back(GetGroupFromGroupID(setid, type, buf.ReadWord()));
			}

			break;
		}

		/* Neither a variable or randomized sprite group... must be a real group */
		default:
		{
			if (type >= 0x80) {
				GrfMsg(0, "NewSpriteGroup: Reserved group type 0x{:02X}, skipping", type);
				return;
			}

			switch (feature) {
				case GSF_TRAINS:
				case GSF_ROADVEHICLES:
				case GSF_SHIPS:
				case GSF_AIRCRAFT:
				case GSF_STATIONS:
				case GSF_CANALS:
				case GSF_CARGOES:
				case GSF_AIRPORTS:
				case GSF_RAILTYPES:
				case GSF_ROADTYPES:
				case GSF_TRAMTYPES:
				case GSF_BADGES:
				{
					uint8_t num_loaded  = type;
					uint8_t num_loading = buf.ReadByte();

					if (!_cur_gps.HasValidSpriteSets(feature)) {
						GrfMsg(0, "NewSpriteGroup: No sprite set to work on! Skipping");
						return;
					}

					GrfMsg(6, "NewSpriteGroup: New SpriteGroup 0x{:02X}, {} loaded, {} loading",
							setid, num_loaded, num_loading);

					if (num_loaded + num_loading == 0) {
						GrfMsg(1, "NewSpriteGroup: no result, skipping invalid RealSpriteGroup");
						break;
					}

					if (num_loaded + num_loading == 1) {
						/* Avoid creating 'Real' sprite group if only one option. */
						uint16_t spriteid = buf.ReadWord();
						act_group = CreateGroupFromGroupID(feature, setid, type, spriteid);
						GrfMsg(8, "NewSpriteGroup: one result, skipping RealSpriteGroup = subset {}", spriteid);
						break;
					}

					std::vector<uint16_t> loaded;
					std::vector<uint16_t> loading;

					loaded.reserve(num_loaded);
					for (uint i = 0; i < num_loaded; i++) {
						loaded.push_back(buf.ReadWord());
						GrfMsg(8, "NewSpriteGroup: + rg->loaded[{}]  = subset {}", i, loaded[i]);
					}

					loading.reserve(num_loading);
					for (uint i = 0; i < num_loading; i++) {
						loading.push_back(buf.ReadWord());
						GrfMsg(8, "NewSpriteGroup: + rg->loading[{}] = subset {}", i, loading[i]);
					}

					bool loaded_same = !loaded.empty() && std::adjacent_find(loaded.begin(),  loaded.end(),  std::not_equal_to<>()) == loaded.end();
					bool loading_same = !loading.empty() && std::adjacent_find(loading.begin(), loading.end(), std::not_equal_to<>()) == loading.end();
					if (loaded_same && loading_same && loaded[0] == loading[0]) {
						/* Both lists only contain the same value, so don't create 'Real' sprite group */
						act_group = CreateGroupFromGroupID(feature, setid, type, loaded[0]);
						GrfMsg(8, "NewSpriteGroup: same result, skipping RealSpriteGroup = subset {}", loaded[0]);
						break;
					}

					assert(RealSpriteGroup::CanAllocateItem());
					RealSpriteGroup *group = new RealSpriteGroup();
					group->nfo_line = _cur_gps.nfo_line;
					act_group = group;

					if (loaded_same && loaded.size() > 1) loaded.resize(1);
					group->loaded.reserve(loaded.size());
					for (uint16_t spriteid : loaded) {
						const SpriteGroup *t = CreateGroupFromGroupID(feature, setid, type, spriteid);
						group->loaded.push_back(t);
					}

					if (loading_same && loading.size() > 1) loading.resize(1);
					group->loading.reserve(loading.size());
					for (uint16_t spriteid : loading) {
						const SpriteGroup *t = CreateGroupFromGroupID(feature, setid, type, spriteid);
						group->loading.push_back(t);
					}

					break;
				}

				case GSF_HOUSES:
				case GSF_AIRPORTTILES:
				case GSF_OBJECTS:
				case GSF_INDUSTRYTILES:
				case GSF_ROADSTOPS: {
					uint8_t num_building_sprites = std::max((uint8_t)1, type);

					assert(TileLayoutSpriteGroup::CanAllocateItem());
					TileLayoutSpriteGroup *group = new TileLayoutSpriteGroup();
					group->nfo_line = _cur_gps.nfo_line;
					act_group = group;

					/* On error, bail out immediately. Temporary GRF data was already freed */
					if (ReadSpriteLayout(buf, num_building_sprites, true, feature, false, type == 0, &group->dts)) return;
					break;
				}

				case GSF_INDUSTRIES: {
					if (type > 2) {
						GrfMsg(1, "NewSpriteGroup: Unsupported industry production version {}, skipping", type);
						break;
					}

					assert(IndustryProductionSpriteGroup::CanAllocateItem());
					IndustryProductionSpriteGroup *group = new IndustryProductionSpriteGroup();
					group->nfo_line = _cur_gps.nfo_line;
					act_group = group;
					group->version = type;
					if (type == 0) {
						group->num_input = INDUSTRY_ORIGINAL_NUM_INPUTS;
						for (uint i = 0; i < INDUSTRY_ORIGINAL_NUM_INPUTS; i++) {
							group->subtract_input[i] = (int16_t)buf.ReadWord(); // signed
						}
						group->num_output = INDUSTRY_ORIGINAL_NUM_OUTPUTS;
						for (uint i = 0; i < INDUSTRY_ORIGINAL_NUM_OUTPUTS; i++) {
							group->add_output[i] = buf.ReadWord(); // unsigned
						}
						group->again = buf.ReadByte();
					} else if (type == 1) {
						group->num_input = INDUSTRY_ORIGINAL_NUM_INPUTS;
						for (uint i = 0; i < INDUSTRY_ORIGINAL_NUM_INPUTS; i++) {
							group->subtract_input[i] = buf.ReadByte();
						}
						group->num_output = INDUSTRY_ORIGINAL_NUM_OUTPUTS;
						for (uint i = 0; i < INDUSTRY_ORIGINAL_NUM_OUTPUTS; i++) {
							group->add_output[i] = buf.ReadByte();
						}
						group->again = buf.ReadByte();
					} else if (type == 2) {
						group->num_input = buf.ReadByte();
						if (group->num_input > std::size(group->subtract_input)) {
							GRFError *error = DisableGrf(STR_NEWGRF_ERROR_INDPROD_CALLBACK);
							error->data = "too many inputs (max 16)";
							return;
						}
						for (uint i = 0; i < group->num_input; i++) {
							uint8_t rawcargo = buf.ReadByte();
							CargoType cargo = GetCargoTranslation(rawcargo, _cur_gps.grffile);
							if (!IsValidCargoType(cargo)) {
								/* The mapped cargo is invalid. This is permitted at this point,
								 * as long as the result is not used. Mark it invalid so this
								 * can be tested later. */
								group->version = 0xFF;
							} else if (auto v = group->cargo_input | std::views::take(i); std::ranges::find(v, cargo) != v.end()) {
								GRFError *error = DisableGrf(STR_NEWGRF_ERROR_INDPROD_CALLBACK);
								error->data = "duplicate input cargo";
								return;
							}
							group->cargo_input[i] = cargo;
							group->subtract_input[i] = buf.ReadByte();
						}
						group->num_output = buf.ReadByte();
						if (group->num_output > std::size(group->add_output)) {
							GRFError *error = DisableGrf(STR_NEWGRF_ERROR_INDPROD_CALLBACK);
							error->data = "too many outputs (max 16)";
							return;
						}
						for (uint i = 0; i < group->num_output; i++) {
							uint8_t rawcargo = buf.ReadByte();
							CargoType cargo = GetCargoTranslation(rawcargo, _cur_gps.grffile);
							if (!IsValidCargoType(cargo)) {
								/* Mark this result as invalid to use */
								group->version = 0xFF;
							} else if (auto v = group->cargo_output | std::views::take(i); std::ranges::find(v, cargo) != v.end()) {
								GRFError *error = DisableGrf(STR_NEWGRF_ERROR_INDPROD_CALLBACK);
								error->data = "duplicate output cargo";
								return;
							}
							group->cargo_output[i] = cargo;
							group->add_output[i] = buf.ReadByte();
						}
						group->again = buf.ReadByte();
					} else {
						NOT_REACHED();
					}
					break;
				}

				/* Loading of Tile Layout and Production Callback groups would happen here */
				default: GrfMsg(1, "NewSpriteGroup: Unsupported feature 0x{:02X}, skipping", feature);
			}
		}
	}

	_cur_gps.spritegroups[setid] = act_group;
}

template <> void GrfActionHandler<0x02>::FileScan(ByteReader &) { }
template <> void GrfActionHandler<0x02>::SafetyScan(ByteReader &) { }
template <> void GrfActionHandler<0x02>::LabelScan(ByteReader &) { }
template <> void GrfActionHandler<0x02>::Init(ByteReader &) { }
template <> void GrfActionHandler<0x02>::Reserve(ByteReader &) { }
template <> void GrfActionHandler<0x02>::Activation(ByteReader &buf) { NewSpriteGroup(buf); }
