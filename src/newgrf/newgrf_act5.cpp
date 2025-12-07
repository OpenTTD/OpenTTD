/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act5.cpp NewGRF Action 0x05 handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "../gfx_type.h"
#include "../newgrf_act5.h"
#include "../spritecache.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "../table/sprites.h"

#include "../safeguards.h"

/**
 * Sanitize incoming sprite offsets for Action 5 graphics replacements.
 * @param num         The number of sprites to load.
 * @param offset      Offset from the base.
 * @param max_sprites The maximum number of sprites that can be loaded in this action 5.
 * @param name        Used for error warnings.
 * @return The number of sprites that is going to be skipped.
 */
static uint16_t SanitizeSpriteOffset(uint16_t &num, uint16_t offset, int max_sprites, std::string_view name)
{

	if (offset >= max_sprites) {
		GrfMsg(1, "GraphicsNew: {} sprite offset must be less than {}, skipping", name, max_sprites);
		uint orig_num = num;
		num = 0;
		return orig_num;
	}

	if (offset + num > max_sprites) {
		GrfMsg(4, "GraphicsNew: {} sprite overflow, truncating...", name);
		uint orig_num = num;
		num = std::max(max_sprites - offset, 0);
		return orig_num - num;
	}

	return 0;
}


/** The information about action 5 types. */
static constexpr auto _action5_types = std::to_array<Action5Type>({
	/* Note: min_sprites should not be changed. Therefore these constants are directly here and not in sprites.h */
	/* 0x00 */ { A5BLOCK_INVALID,      0,                            0, 0,                                           "Type 0x00"                },
	/* 0x01 */ { A5BLOCK_INVALID,      0,                            0, 0,                                           "Type 0x01"                },
	/* 0x02 */ { A5BLOCK_INVALID,      0,                            0, 0,                                           "Type 0x02"                },
	/* 0x03 */ { A5BLOCK_INVALID,      0,                            0, 0,                                           "Type 0x03"                },
	/* 0x04 */ { A5BLOCK_ALLOW_OFFSET, SPR_SIGNALS_BASE,             1, PRESIGNAL_SEMAPHORE_AND_PBS_SPRITE_COUNT,    "Signal graphics"          },
	/* 0x05 */ { A5BLOCK_ALLOW_OFFSET, SPR_ELRAIL_BASE,              1, ELRAIL_SPRITE_COUNT,                         "Rail catenary graphics"   },
	/* 0x06 */ { A5BLOCK_ALLOW_OFFSET, SPR_SLOPES_BASE,              1, NORMAL_AND_HALFTILE_FOUNDATION_SPRITE_COUNT, "Foundation graphics"      },
	/* 0x07 */ { A5BLOCK_INVALID,      0,                           75, 0,                                           "TTDP GUI graphics"        }, // Not used by OTTD.
	/* 0x08 */ { A5BLOCK_ALLOW_OFFSET, SPR_CANALS_BASE,              1, CANALS_SPRITE_COUNT,                         "Canal graphics"           },
	/* 0x09 */ { A5BLOCK_ALLOW_OFFSET, SPR_ONEWAY_BASE,              1, ONEWAY_SPRITE_COUNT,                         "One way road graphics"    },
	/* 0x0A */ { A5BLOCK_ALLOW_OFFSET, SPR_2CCMAP_BASE,              1, TWOCCMAP_SPRITE_COUNT,                       "2CC colour maps"          },
	/* 0x0B */ { A5BLOCK_ALLOW_OFFSET, SPR_TRAMWAY_BASE,             1, TRAMWAY_SPRITE_COUNT,                        "Tramway graphics"         },
	/* 0x0C */ { A5BLOCK_INVALID,      0,                          133, 0,                                           "Snowy temperate tree"     }, // Not yet used by OTTD.
	/* 0x0D */ { A5BLOCK_FIXED,        SPR_SHORE_BASE,              16, SHORE_SPRITE_COUNT,                          "Shore graphics"           },
	/* 0x0E */ { A5BLOCK_INVALID,      0,                            0, 0,                                           "New Signals graphics"     }, // Not yet used by OTTD.
	/* 0x0F */ { A5BLOCK_ALLOW_OFFSET, SPR_TRACKS_FOR_SLOPES_BASE,   1, TRACKS_FOR_SLOPES_SPRITE_COUNT,              "Sloped rail track"        },
	/* 0x10 */ { A5BLOCK_ALLOW_OFFSET, SPR_AIRPORTX_BASE,            1, AIRPORTX_SPRITE_COUNT,                       "Airport graphics"         },
	/* 0x11 */ { A5BLOCK_ALLOW_OFFSET, SPR_ROADSTOP_BASE,            1, ROADSTOP_SPRITE_COUNT,                       "Road stop graphics"       },
	/* 0x12 */ { A5BLOCK_ALLOW_OFFSET, SPR_AQUEDUCT_BASE,            1, AQUEDUCT_SPRITE_COUNT,                       "Aqueduct graphics"        },
	/* 0x13 */ { A5BLOCK_ALLOW_OFFSET, SPR_AUTORAIL_BASE,            1, AUTORAIL_SPRITE_COUNT,                       "Autorail graphics"        },
	/* 0x14 */ { A5BLOCK_INVALID,      0,                            1, 0,                                           "Flag graphics"            }, // deprecated, no longer used.
	/* 0x15 */ { A5BLOCK_ALLOW_OFFSET, SPR_OPENTTD_BASE,             1, OPENTTD_SPRITE_COUNT,                        "OpenTTD GUI graphics"     },
	/* 0x16 */ { A5BLOCK_ALLOW_OFFSET, SPR_AIRPORT_PREVIEW_BASE,     1, AIRPORT_PREVIEW_SPRITE_COUNT,                "Airport preview graphics" },
	/* 0x17 */ { A5BLOCK_ALLOW_OFFSET, SPR_RAILTYPE_TUNNEL_BASE,     1, RAILTYPE_TUNNEL_BASE_COUNT,                  "Railtype tunnel base"     },
	/* 0x18 */ { A5BLOCK_ALLOW_OFFSET, SPR_PALETTE_BASE,             1, PALETTE_SPRITE_COUNT,                        "Palette"                  },
	/* 0x19 */ { A5BLOCK_ALLOW_OFFSET, SPR_ROAD_WAYPOINTS_BASE,      1, ROAD_WAYPOINTS_SPRITE_COUNT,                 "Road waypoints"           },
	/* 0x1A */ { A5BLOCK_ALLOW_OFFSET, SPR_OVERLAY_ROCKS_BASE,       1, OVERLAY_ROCKS_SPRITE_COUNT,                  "Overlay rocks"            },
	/* 0x1B */ { A5BLOCK_ALLOW_OFFSET, SPR_BRIDGE_DECKS_BASE,        1, BRIDGE_DECKS_SPRITE_COUNT,                   "Bridge decks"             },
});

/**
 * Get list of all action 5 types
 * @return Read-only span of action 5 type information.
 */
std::span<const Action5Type> GetAction5Types()
{
	return _action5_types;
}

/* Action 0x05 */
static void GraphicsNew(ByteReader &buf)
{
	/* <05> <graphics-type> <num-sprites> <other data...>
	 *
	 * B graphics-type What set of graphics the sprites define.
	 * E num-sprites   How many sprites are in this set?
	 * V other data    Graphics type specific data.  Currently unused. */

	uint8_t type = buf.ReadByte();
	uint16_t num = buf.ReadExtendedByte();
	uint16_t offset = HasBit(type, 7) ? buf.ReadExtendedByte() : 0;
	ClrBit(type, 7); // Clear the high bit as that only indicates whether there is an offset.

	if ((type == 0x0D) && (num == 10) && _cur_gps.grfconfig->flags.Test(GRFConfigFlag::System)) {
		/* Special not-TTDP-compatible case used in openttd.grf
		 * Missing shore sprites and initialisation of SPR_SHORE_BASE */
		GrfMsg(2, "GraphicsNew: Loading 10 missing shore sprites from extra grf.");
		LoadNextSprite(SPR_SHORE_BASE +  0, *_cur_gps.file, _cur_gps.nfo_line++); // SLOPE_STEEP_S
		LoadNextSprite(SPR_SHORE_BASE +  5, *_cur_gps.file, _cur_gps.nfo_line++); // SLOPE_STEEP_W
		LoadNextSprite(SPR_SHORE_BASE +  7, *_cur_gps.file, _cur_gps.nfo_line++); // SLOPE_WSE
		LoadNextSprite(SPR_SHORE_BASE + 10, *_cur_gps.file, _cur_gps.nfo_line++); // SLOPE_STEEP_N
		LoadNextSprite(SPR_SHORE_BASE + 11, *_cur_gps.file, _cur_gps.nfo_line++); // SLOPE_NWS
		LoadNextSprite(SPR_SHORE_BASE + 13, *_cur_gps.file, _cur_gps.nfo_line++); // SLOPE_ENW
		LoadNextSprite(SPR_SHORE_BASE + 14, *_cur_gps.file, _cur_gps.nfo_line++); // SLOPE_SEN
		LoadNextSprite(SPR_SHORE_BASE + 15, *_cur_gps.file, _cur_gps.nfo_line++); // SLOPE_STEEP_E
		LoadNextSprite(SPR_SHORE_BASE + 16, *_cur_gps.file, _cur_gps.nfo_line++); // SLOPE_EW
		LoadNextSprite(SPR_SHORE_BASE + 17, *_cur_gps.file, _cur_gps.nfo_line++); // SLOPE_NS
		if (_loaded_newgrf_features.shore == SHORE_REPLACE_NONE) _loaded_newgrf_features.shore = SHORE_REPLACE_ONLY_NEW;
		return;
	}

	/* Supported type? */
	if ((type >= std::size(_action5_types)) || (_action5_types[type].block_type == A5BLOCK_INVALID)) {
		GrfMsg(2, "GraphicsNew: Custom graphics (type 0x{:02X}) sprite block of length {} (unimplemented, ignoring)", type, num);
		_cur_gps.skip_sprites = num;
		return;
	}

	const Action5Type *action5_type = &_action5_types[type];

	/* Contrary to TTDP we allow always to specify too few sprites as we allow always an offset,
	 * except for the long version of the shore type:
	 * Ignore offset if not allowed */
	if ((action5_type->block_type != A5BLOCK_ALLOW_OFFSET) && (offset != 0)) {
		GrfMsg(1, "GraphicsNew: {} (type 0x{:02X}) do not allow an <offset> field. Ignoring offset.", action5_type->name, type);
		offset = 0;
	}

	/* Ignore action5 if too few sprites are specified. (for TTDP compatibility)
	 * This does not make sense, if <offset> is allowed */
	if ((action5_type->block_type == A5BLOCK_FIXED) && (num < action5_type->min_sprites)) {
		GrfMsg(1, "GraphicsNew: {} (type 0x{:02X}) count must be at least {}. Only {} were specified. Skipping.", action5_type->name, type, action5_type->min_sprites, num);
		_cur_gps.skip_sprites = num;
		return;
	}

	/* Load at most max_sprites sprites. Skip remaining sprites. (for compatibility with TTDP and future extensions) */
	uint16_t skip_num = SanitizeSpriteOffset(num, offset, action5_type->max_sprites, action5_type->name);
	SpriteID replace = action5_type->sprite_base + offset;

	/* Load <num> sprites starting from <replace>, then skip <skip_num> sprites. */
	GrfMsg(2, "GraphicsNew: Replacing sprites {} to {} of {} (type 0x{:02X}) at SpriteID 0x{:04X}", offset, offset + num - 1, action5_type->name, type, replace);

	if (type == 0x0D) _loaded_newgrf_features.shore = SHORE_REPLACE_ACTION_5;

	if (type == 0x0B) {
		static const SpriteID depot_with_track_offset = SPR_TRAMWAY_DEPOT_WITH_TRACK - SPR_TRAMWAY_BASE;
		static const SpriteID depot_no_track_offset = SPR_TRAMWAY_DEPOT_NO_TRACK - SPR_TRAMWAY_BASE;
		if (offset <= depot_with_track_offset && offset + num > depot_with_track_offset) _loaded_newgrf_features.tram = TRAMWAY_REPLACE_DEPOT_WITH_TRACK;
		if (offset <= depot_no_track_offset && offset + num > depot_no_track_offset) _loaded_newgrf_features.tram = TRAMWAY_REPLACE_DEPOT_NO_TRACK;
	}

	/* If the baseset or grf only provides sprites for flat tiles (pre #10282), duplicate those for use on slopes. */
	bool dup_oneway_sprites = ((type == 0x09) && (offset + num <= ONEWAY_SLOPE_N_OFFSET));

	for (; num > 0; num--) {
		_cur_gps.nfo_line++;
		SpriteID load_index = (replace == 0 ? _cur_gps.spriteid++ : replace++);
		LoadNextSprite(load_index, *_cur_gps.file, _cur_gps.nfo_line);
		if (dup_oneway_sprites) {
			DupSprite(load_index, load_index + ONEWAY_SLOPE_N_OFFSET);
			DupSprite(load_index, load_index + ONEWAY_SLOPE_S_OFFSET);
		}
	}

	_cur_gps.skip_sprites = skip_num;
}

/* Action 0x05 (SKIP) */
static void SkipAct5(ByteReader &buf)
{
	/* Ignore type byte */
	buf.ReadByte();

	/* Skip the sprites of this action */
	_cur_gps.skip_sprites = buf.ReadExtendedByte();

	GrfMsg(3, "SkipAct5: Skipping {} sprites", _cur_gps.skip_sprites);
}

template <> void GrfActionHandler<0x05>::FileScan(ByteReader &buf) { SkipAct5(buf); }
template <> void GrfActionHandler<0x05>::SafetyScan(ByteReader &buf) { SkipAct5(buf); }
template <> void GrfActionHandler<0x05>::LabelScan(ByteReader &buf) { SkipAct5(buf); }
template <> void GrfActionHandler<0x05>::Init(ByteReader &buf) { SkipAct5(buf); }
template <> void GrfActionHandler<0x05>::Reserve(ByteReader &buf) { SkipAct5(buf); }
template <> void GrfActionHandler<0x05>::Activation(ByteReader &buf) { GraphicsNew(buf); }
