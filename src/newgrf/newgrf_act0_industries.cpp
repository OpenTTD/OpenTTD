/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act0_industries.cpp NewGRF Action 0x00 handler for industries and industrytiles. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_cargo.h"
#include "../industry.h"
#include "../industrytype.h"
#include "../industry_map.h"
#include "../newgrf_industries.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"
#include "newgrf_stringmapping.h"

#include "table/strings.h"
#include "../table/build_industry.h"

#include "../safeguards.h"

/**
 * Ignore an industry tile property
 * @param prop The property to ignore.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult IgnoreIndustryTileProperty(int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	switch (prop) {
		case 0x09:
		case 0x0D:
		case 0x0E:
		case 0x10:
		case 0x11:
		case 0x12:
			buf.ReadByte();
			break;

		case 0x0A:
		case 0x0B:
		case 0x0C:
		case 0x0F:
			buf.ReadWord();
			break;

		case 0x13:
			buf.Skip(buf.ReadByte() * 2);
			break;

		default:
			ret = CIR_UNKNOWN;
			break;
	}
	return ret;
}

/**
 * Define properties for industry tiles
 * @param first Local ID of the first industry tile.
 * @param last Local ID of the last industry tile.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult IndustrytilesChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	if (last > NUM_INDUSTRYTILES_PER_GRF) {
		GrfMsg(1, "IndustryTilesChangeInfo: Too many industry tiles loaded ({}), max ({}). Ignoring.", last, NUM_INDUSTRYTILES_PER_GRF);
		return CIR_INVALID_ID;
	}

	/* Allocate industry tile specs if they haven't been allocated already. */
	if (_cur_gps.grffile->indtspec.size() < last) _cur_gps.grffile->indtspec.resize(last);

	for (uint id = first; id < last; ++id) {
		auto &tsp = _cur_gps.grffile->indtspec[id];

		if (prop != 0x08 && tsp == nullptr) {
			ChangeInfoResult cir = IgnoreIndustryTileProperty(prop, buf);
			if (cir > ret) ret = cir;
			continue;
		}

		switch (prop) {
			case 0x08: { // Substitute industry tile type
				uint8_t subs_id = buf.ReadByte();
				if (subs_id >= NEW_INDUSTRYTILEOFFSET) {
					/* The substitute id must be one of the original industry tile. */
					GrfMsg(2, "IndustryTilesChangeInfo: Attempt to use new industry tile {} as substitute industry tile for {}. Ignoring.", subs_id, id);
					continue;
				}

				/* Allocate space for this industry. */
				if (tsp == nullptr) {
					tsp = std::make_unique<IndustryTileSpec>(_industry_tile_specs[subs_id]);

					tsp->enabled = true;

					/* A copied tile should not have the animation infos copied too.
					 * The anim_state should be left untouched, though
					 * It is up to the author to animate them */
					tsp->anim_production = INDUSTRYTILE_NOANIM;
					tsp->anim_next = INDUSTRYTILE_NOANIM;

					tsp->grf_prop.local_id = id;
					tsp->grf_prop.subst_id = subs_id;
					tsp->grf_prop.SetGRFFile(_cur_gps.grffile);
					_industile_mngr.AddEntityID(id, _cur_gps.grffile->grfid, subs_id); // pre-reserve the tile slot
				}
				break;
			}

			case 0x09: { // Industry tile override
				uint8_t ovrid = buf.ReadByte();

				/* The industry being overridden must be an original industry. */
				if (ovrid >= NEW_INDUSTRYTILEOFFSET) {
					GrfMsg(2, "IndustryTilesChangeInfo: Attempt to override new industry tile {} with industry tile id {}. Ignoring.", ovrid, id);
					continue;
				}

				_industile_mngr.Add(id, _cur_gps.grffile->grfid, ovrid);
				break;
			}

			case 0x0A: // Tile acceptance
			case 0x0B:
			case 0x0C: {
				uint16_t acctp = buf.ReadWord();
				tsp->accepts_cargo[prop - 0x0A] = GetCargoTranslation(GB(acctp, 0, 8), _cur_gps.grffile);
				tsp->acceptance[prop - 0x0A] = Clamp(GB(acctp, 8, 8), 0, 16);
				tsp->accepts_cargo_label[prop - 0x0A] = CT_INVALID;
				break;
			}

			case 0x0D: // Land shape flags
				tsp->slopes_refused = (Slope)buf.ReadByte();
				break;

			case 0x0E: // Callback mask
				tsp->callback_mask = static_cast<IndustryTileCallbackMasks>(buf.ReadByte());
				break;

			case 0x0F: // Animation information
				tsp->animation.frames = buf.ReadByte();
				tsp->animation.status = static_cast<AnimationStatus>(buf.ReadByte());
				break;

			case 0x10: // Animation speed
				tsp->animation.speed = buf.ReadByte();
				break;

			case 0x11: // Triggers for callback 25
				tsp->animation.triggers = static_cast<IndustryAnimationTriggers>(buf.ReadByte());
				break;

			case 0x12: // Special flags
				tsp->special_flags = IndustryTileSpecialFlags{buf.ReadByte()};
				break;

			case 0x13: { // variable length cargo acceptance
				uint8_t num_cargoes = buf.ReadByte();
				if (num_cargoes > std::size(tsp->acceptance)) {
					GRFError *error = DisableGrf(STR_NEWGRF_ERROR_LIST_PROPERTY_TOO_LONG);
					error->param_value[1] = prop;
					return CIR_DISABLED;
				}
				for (uint i = 0; i < std::size(tsp->acceptance); i++) {
					if (i < num_cargoes) {
						tsp->accepts_cargo[i] = GetCargoTranslation(buf.ReadByte(), _cur_gps.grffile);
						/* Tile acceptance can be negative to counteract the IndustryTileSpecialFlag::AcceptsAllCargo flag */
						tsp->acceptance[i] = (int8_t)buf.ReadByte();
					} else {
						tsp->accepts_cargo[i] = INVALID_CARGO;
						tsp->acceptance[i] = 0;
					}
					if (i < std::size(tsp->accepts_cargo_label)) tsp->accepts_cargo_label[i] = CT_INVALID;
				}
				break;
			}

			case 0x14: // Badge list
				tsp->badges = ReadBadgeList(buf, GSF_INDUSTRYTILES);
				break;

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

/**
 * Ignore an industry property
 * @param prop The property to ignore.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult IgnoreIndustryProperty(int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	switch (prop) {
		case 0x09:
		case 0x0B:
		case 0x0F:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x21:
		case 0x22:
			buf.ReadByte();
			break;

		case 0x0C:
		case 0x0D:
		case 0x0E:
		case 0x10: // INDUSTRY_ORIGINAL_NUM_OUTPUTS bytes
		case 0x1B:
		case 0x1F:
		case 0x24:
			buf.ReadWord();
			break;

		case 0x11: // INDUSTRY_ORIGINAL_NUM_INPUTS bytes + 1
		case 0x1A:
		case 0x1C:
		case 0x1D:
		case 0x1E:
		case 0x20:
		case 0x23:
			buf.ReadDWord();
			break;

		case 0x0A: {
			uint8_t num_table = buf.ReadByte();
			for (uint8_t j = 0; j < num_table; j++) {
				for (uint k = 0;; k++) {
					uint8_t x = buf.ReadByte();
					if (x == 0xFE && k == 0) {
						buf.ReadByte();
						buf.ReadByte();
						break;
					}

					uint8_t y = buf.ReadByte();
					if (x == 0 && y == 0x80) break;

					uint8_t gfx = buf.ReadByte();
					if (gfx == 0xFE) buf.ReadWord();
				}
			}
			break;
		}

		case 0x16:
			for (uint8_t j = 0; j < INDUSTRY_ORIGINAL_NUM_INPUTS; j++) buf.ReadByte();
			break;

		case 0x15:
		case 0x25:
		case 0x26:
		case 0x27:
			buf.Skip(buf.ReadByte());
			break;

		case 0x28: {
			int num_inputs = buf.ReadByte();
			int num_outputs = buf.ReadByte();
			buf.Skip(num_inputs * num_outputs * 2);
			break;
		}

		case 0x29: // Badge list
			SkipBadgeList(buf);
			break;

		default:
			ret = CIR_UNKNOWN;
			break;
	}
	return ret;
}

/**
 * Validate the industry layout; e.g. to prevent duplicate tiles.
 * @param layout The layout to check.
 * @return True if the layout is deemed valid.
 */
static bool ValidateIndustryLayout(const IndustryTileLayout &layout)
{
	const size_t size = layout.size();
	if (size == 0) return false;

	for (size_t i = 0; i < size - 1; i++) {
		for (size_t j = i + 1; j < size; j++) {
			if (layout[i].ti.x == layout[j].ti.x &&
					layout[i].ti.y == layout[j].ti.y) {
				return false;
			}
		}
	}

	bool have_regular_tile = false;
	for (const auto &tilelayout : layout) {
		if (tilelayout.gfx != GFX_WATERTILE_SPECIALCHECK) {
			have_regular_tile = true;
			break;
		}
	}

	return have_regular_tile;
}

/**
 * Define properties for industries
 * @param first Local ID of the first industry.
 * @param last Local ID of the last industry.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult IndustriesChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	if (last > NUM_INDUSTRYTYPES_PER_GRF) {
		GrfMsg(1, "IndustriesChangeInfo: Too many industries loaded ({}), max ({}). Ignoring.", last, NUM_INDUSTRYTYPES_PER_GRF);
		return CIR_INVALID_ID;
	}

	/* Allocate industry specs if they haven't been allocated already. */
	if (_cur_gps.grffile->industryspec.size() < last) _cur_gps.grffile->industryspec.resize(last);

	for (uint id = first; id < last; ++id) {
		auto &indsp = _cur_gps.grffile->industryspec[id];

		if (prop != 0x08 && indsp == nullptr) {
			ChangeInfoResult cir = IgnoreIndustryProperty(prop, buf);
			if (cir > ret) ret = cir;
			continue;
		}

		switch (prop) {
			case 0x08: { // Substitute industry type
				uint8_t subs_id = buf.ReadByte();
				if (subs_id == 0xFF) {
					/* Instead of defining a new industry, a substitute industry id
					 * of 0xFF disables the old industry with the current id. */
					_industry_specs[id].enabled = false;
					continue;
				} else if (subs_id >= NEW_INDUSTRYOFFSET) {
					/* The substitute id must be one of the original industry. */
					GrfMsg(2, "_industry_specs: Attempt to use new industry {} as substitute industry for {}. Ignoring.", subs_id, id);
					continue;
				}

				/* Allocate space for this industry.
				 * Only need to do it once. If ever it is called again, it should not
				 * do anything */
				if (indsp == nullptr) {
					indsp = std::make_unique<IndustrySpec>(_origin_industry_specs[subs_id]);

					indsp->enabled = true;
					indsp->grf_prop.local_id = id;
					indsp->grf_prop.subst_id = subs_id;
					indsp->grf_prop.SetGRFFile(_cur_gps.grffile);
					/* If the grf industry needs to check its surrounding upon creation, it should
					 * rely on callbacks, not on the original placement functions */
					indsp->check_proc = CHECK_NOTHING;
				}
				break;
			}

			case 0x09: { // Industry type override
				uint8_t ovrid = buf.ReadByte();

				/* The industry being overridden must be an original industry. */
				if (ovrid >= NEW_INDUSTRYOFFSET) {
					GrfMsg(2, "IndustriesChangeInfo: Attempt to override new industry {} with industry id {}. Ignoring.", ovrid, id);
					continue;
				}
				indsp->grf_prop.override_id = ovrid;
				_industry_mngr.Add(id, _cur_gps.grffile->grfid, ovrid);
				break;
			}

			case 0x0A: { // Set industry layout(s)
				uint8_t new_num_layouts = buf.ReadByte();
				uint32_t definition_size = buf.ReadDWord();
				uint32_t bytes_read = 0;
				std::vector<IndustryTileLayout> new_layouts;
				IndustryTileLayout layout;

				for (uint8_t j = 0; j < new_num_layouts; j++) {
					layout.clear();

					for (uint k = 0;; k++) {
						if (bytes_read >= definition_size) {
							GrfMsg(3, "IndustriesChangeInfo: Incorrect size for industry tile layout definition for industry {}.", id);
							/* Avoid warning twice */
							definition_size = UINT32_MAX;
						}

						IndustryTileLayoutTile &it = layout.emplace_back();

						it.ti.x = buf.ReadByte(); // Offsets from northernmost tile
						++bytes_read;

						if (it.ti.x == 0xFE && k == 0) {
							/* This means we have to borrow the layout from an old industry */
							IndustryType type = buf.ReadByte();
							uint8_t laynbr = buf.ReadByte();
							bytes_read += 2;

							if (type >= lengthof(_origin_industry_specs)) {
								GrfMsg(1, "IndustriesChangeInfo: Invalid original industry number for layout import, industry {}", id);
								DisableGrf(STR_NEWGRF_ERROR_INVALID_ID);
								return CIR_DISABLED;
							}
							if (laynbr >= _origin_industry_specs[type].layouts.size()) {
								GrfMsg(1, "IndustriesChangeInfo: Invalid original industry layout index for layout import, industry {}", id);
								DisableGrf(STR_NEWGRF_ERROR_INVALID_ID);
								return CIR_DISABLED;
							}
							layout = _origin_industry_specs[type].layouts[laynbr];
							break;
						}

						it.ti.y = buf.ReadByte(); // Or table definition finalisation
						++bytes_read;

						if (it.ti.x == 0 && it.ti.y == 0x80) {
							/* Terminator, remove and finish up */
							layout.pop_back();
							break;
						}

						it.gfx = buf.ReadByte();
						++bytes_read;

						if (it.gfx == 0xFE) {
							/* Use a new tile from this GRF */
							int local_tile_id = buf.ReadWord();
							bytes_read += 2;

							/* Read the ID from the _industile_mngr. */
							int tempid = _industile_mngr.GetID(local_tile_id, _cur_gps.grffile->grfid);

							if (tempid == INVALID_INDUSTRYTILE) {
								GrfMsg(2, "IndustriesChangeInfo: Attempt to use industry tile {} with industry id {}, not yet defined. Ignoring.", local_tile_id, id);
							} else {
								/* Declared as been valid, can be used */
								it.gfx = tempid;
							}
						} else if (it.gfx == GFX_WATERTILE_SPECIALCHECK) {
							it.ti.x = (int8_t)GB(it.ti.x, 0, 8);
							it.ti.y = (int8_t)GB(it.ti.y, 0, 8);

							/* When there were only 256x256 maps, TileIndex was a uint16_t and
							 * it.ti was just a TileIndexDiff that was added to it.
							 * As such negative "x" values were shifted into the "y" position.
							 *   x = -1, y = 1 -> x = 255, y = 0
							 * Since GRF version 8 the position is interpreted as pair of independent int8.
							 * For GRF version < 8 we need to emulate the old shifting behaviour.
							 */
							if (_cur_gps.grffile->grf_version < 8 && it.ti.x < 0) it.ti.y += 1;
						}
					}

					if (!ValidateIndustryLayout(layout)) {
						/* The industry layout was not valid, so skip this one. */
						GrfMsg(1, "IndustriesChangeInfo: Invalid industry layout for industry id {}. Ignoring", id);
					} else {
						new_layouts.push_back(layout);
					}
				}

				/* Install final layout construction in the industry spec */
				indsp->layouts = std::move(new_layouts);
				break;
			}

			case 0x0B: // Industry production flags
				indsp->life_type = IndustryLifeTypes{buf.ReadByte()};
				break;

			case 0x0C: // Industry closure message
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &indsp->closure_text);
				break;

			case 0x0D: // Production increase message
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &indsp->production_up_text);
				break;

			case 0x0E: // Production decrease message
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &indsp->production_down_text);
				break;

			case 0x0F: // Fund cost multiplier
				indsp->cost_multiplier = buf.ReadByte();
				break;

			case 0x10: // Production cargo types
				for (uint8_t j = 0; j < INDUSTRY_ORIGINAL_NUM_OUTPUTS; j++) {
					indsp->produced_cargo[j] = GetCargoTranslation(buf.ReadByte(), _cur_gps.grffile);
					indsp->produced_cargo_label[j] = CT_INVALID;
				}
				break;

			case 0x11: // Acceptance cargo types
				for (uint8_t j = 0; j < INDUSTRY_ORIGINAL_NUM_INPUTS; j++) {
					indsp->accepts_cargo[j] = GetCargoTranslation(buf.ReadByte(), _cur_gps.grffile);
					indsp->accepts_cargo_label[j] = CT_INVALID;
				}
				buf.ReadByte(); // Unnused, eat it up
				break;

			case 0x12: // Production multipliers
			case 0x13:
				indsp->production_rate[prop - 0x12] = buf.ReadByte();
				break;

			case 0x14: // Minimal amount of cargo distributed
				indsp->minimal_cargo = buf.ReadByte();
				break;

			case 0x15: { // Random sound effects
				uint8_t num_sounds = buf.ReadByte();

				std::vector<uint8_t> sounds;
				sounds.reserve(num_sounds);
				for (uint8_t j = 0; j < num_sounds; ++j) {
					sounds.push_back(buf.ReadByte());
				}

				indsp->random_sounds = std::move(sounds);
				break;
			}

			case 0x16: // Conflicting industry types
				for (uint8_t j = 0; j < 3; j++) indsp->conflicting[j] = buf.ReadByte();
				break;

			case 0x17: // Probability in random game
				indsp->appear_creation[to_underlying(_settings_game.game_creation.landscape)] = buf.ReadByte();
				break;

			case 0x18: // Probability during gameplay
				indsp->appear_ingame[to_underlying(_settings_game.game_creation.landscape)] = buf.ReadByte();
				break;

			case 0x19: // Map colour
				indsp->map_colour = PixelColour{buf.ReadByte()};
				break;

			case 0x1A: // Special industry flags to define special behavior
				indsp->behaviour = IndustryBehaviours{buf.ReadDWord()};
				break;

			case 0x1B: // New industry text ID
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &indsp->new_industry_text);
				break;

			case 0x1C: // Input cargo multipliers for the three input cargo types
			case 0x1D:
			case 0x1E: {
					uint32_t multiples = buf.ReadDWord();
					indsp->input_cargo_multiplier[prop - 0x1C][0] = GB(multiples, 0, 16);
					indsp->input_cargo_multiplier[prop - 0x1C][1] = GB(multiples, 16, 16);
					break;
				}

			case 0x1F: // Industry name
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &indsp->name);
				break;

			case 0x20: // Prospecting success chance
				indsp->prospecting_chance = buf.ReadDWord();
				break;

			case 0x21:   // Callback mask
			case 0x22: { // Callback additional mask
				auto mask = indsp->callback_mask.base();
				SB(mask, (prop - 0x21) * 8, 8, buf.ReadByte());
				indsp->callback_mask = IndustryCallbackMasks{mask};
				break;
			}

			case 0x23: // removal cost multiplier
				indsp->removal_cost_multiplier = buf.ReadDWord();
				break;

			case 0x24: { // name for nearby station
				GRFStringID str{buf.ReadWord()};
				if (str == 0) {
					indsp->station_name = STR_NULL;
				} else {
					AddStringForMapping(str, &indsp->station_name);
				}
				break;
			}

			case 0x25: { // variable length produced cargoes
				uint8_t num_cargoes = buf.ReadByte();
				if (num_cargoes > std::size(indsp->produced_cargo)) {
					GRFError *error = DisableGrf(STR_NEWGRF_ERROR_LIST_PROPERTY_TOO_LONG);
					error->param_value[1] = prop;
					return CIR_DISABLED;
				}
				for (size_t i = 0; i < std::size(indsp->produced_cargo); i++) {
					if (i < num_cargoes) {
						CargoType cargo = GetCargoTranslation(buf.ReadByte(), _cur_gps.grffile);
						indsp->produced_cargo[i] = cargo;
					} else {
						indsp->produced_cargo[i] = INVALID_CARGO;
					}
					if (i < std::size(indsp->produced_cargo_label)) indsp->produced_cargo_label[i] = CT_INVALID;
				}
				break;
			}

			case 0x26: { // variable length accepted cargoes
				uint8_t num_cargoes = buf.ReadByte();
				if (num_cargoes > std::size(indsp->accepts_cargo)) {
					GRFError *error = DisableGrf(STR_NEWGRF_ERROR_LIST_PROPERTY_TOO_LONG);
					error->param_value[1] = prop;
					return CIR_DISABLED;
				}
				for (size_t i = 0; i < std::size(indsp->accepts_cargo); i++) {
					if (i < num_cargoes) {
						CargoType cargo = GetCargoTranslation(buf.ReadByte(), _cur_gps.grffile);
						indsp->accepts_cargo[i] = cargo;
					} else {
						indsp->accepts_cargo[i] = INVALID_CARGO;
					}
					if (i < std::size(indsp->accepts_cargo_label)) indsp->accepts_cargo_label[i] = CT_INVALID;
				}
				break;
			}

			case 0x27: { // variable length production rates
				uint8_t num_cargoes = buf.ReadByte();
				if (num_cargoes > lengthof(indsp->production_rate)) {
					GRFError *error = DisableGrf(STR_NEWGRF_ERROR_LIST_PROPERTY_TOO_LONG);
					error->param_value[1] = prop;
					return CIR_DISABLED;
				}
				for (uint i = 0; i < lengthof(indsp->production_rate); i++) {
					if (i < num_cargoes) {
						indsp->production_rate[i] = buf.ReadByte();
					} else {
						indsp->production_rate[i] = 0;
					}
				}
				break;
			}

			case 0x28: { // variable size input/output production multiplier table
				uint8_t num_inputs = buf.ReadByte();
				uint8_t num_outputs = buf.ReadByte();
				if (num_inputs > std::size(indsp->accepts_cargo) || num_outputs > std::size(indsp->produced_cargo)) {
					GRFError *error = DisableGrf(STR_NEWGRF_ERROR_LIST_PROPERTY_TOO_LONG);
					error->param_value[1] = prop;
					return CIR_DISABLED;
				}
				for (size_t i = 0; i < std::size(indsp->accepts_cargo); i++) {
					for (size_t j = 0; j < std::size(indsp->produced_cargo); j++) {
						uint16_t mult = 0;
						if (i < num_inputs && j < num_outputs) mult = buf.ReadWord();
						indsp->input_cargo_multiplier[i][j] = mult;
					}
				}
				break;
			}

			case 0x29: // Badge list
				indsp->badges = ReadBadgeList(buf, GSF_INDUSTRIES);
				break;

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_INDUSTRYTILES>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_INDUSTRYTILES>::Activation(uint first, uint last, int prop, ByteReader &buf) { return IndustrytilesChangeInfo(first, last, prop, buf); }

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_INDUSTRIES>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_INDUSTRIES>::Activation(uint first, uint last, int prop, ByteReader &buf) { return IndustriesChangeInfo(first, last, prop, buf); }
