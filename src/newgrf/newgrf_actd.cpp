/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_actd.cpp NewGRF Action 0x0D handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf.h"
#include "../newgrf_engine.h"
#include "../newgrf_cargo.h"
#include "../timer/timer_game_calendar.h"
#include "../error.h"
#include "../engine_func.h"
#include "../vehicle_base.h"
#include "../rail.h"
#include "../road.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "table/strings.h"

#include "../safeguards.h"

/**
 * Contains the GRF ID of the owner of a vehicle if it has been reserved.
 * GRM for vehicles is only used if dynamic engine allocation is disabled,
 * so 256 is the number of original engines. */
static std::array<uint32_t, 256> _grm_engines{};

/** Contains the GRF ID of the owner of a cargo if it has been reserved */
static std::array<uint32_t, NUM_CARGO * 2> _grm_cargoes{};

void ResetGRM()
{
	_grm_engines.fill(0);
	_grm_cargoes.fill(0);
}

/* Action 0x0D (GLS_SAFETYSCAN) */
static void SafeParamSet(ByteReader &buf)
{
	uint8_t target = buf.ReadByte();

	/* Writing GRF parameters and some bits of 'misc GRF features' are safe. */
	if (target < 0x80 || target == 0x9E) return;

	/* GRM could be unsafe, but as here it can only happen after other GRFs
	 * are loaded, it should be okay. If the GRF tried to use the slots it
	 * reserved, it would be marked unsafe anyway. GRM for (e.g. bridge)
	 * sprites  is considered safe. */
	GRFUnsafe(buf);
}

static uint32_t GetPatchVariable(uint8_t param)
{
	switch (param) {
		/* start year - 1920 */
		case 0x0B: return (std::max(_settings_game.game_creation.starting_year, CalendarTime::ORIGINAL_BASE_YEAR) - CalendarTime::ORIGINAL_BASE_YEAR).base();

		/* freight trains weight factor */
		case 0x0E: return _settings_game.vehicle.freight_trains;

		/* empty wagon speed increase */
		case 0x0F: return 0;

		/* plane speed factor; our patch option is reversed from TTDPatch's,
		 * the following is good for 1x, 2x and 4x (most common?) and...
		 * well not really for 3x. */
		case 0x10:
			switch (_settings_game.vehicle.plane_speed) {
				default:
				case 4: return 1;
				case 3: return 2;
				case 2: return 2;
				case 1: return 4;
			}


		/* 2CC colourmap base sprite */
		case 0x11: return SPR_2CCMAP_BASE;

		/* map size: format = -MABXYSS
		 * M  : the type of map
		 *       bit 0 : set   : squared map. Bit 1 is now not relevant
		 *               clear : rectangle map. Bit 1 will indicate the bigger edge of the map
		 *       bit 1 : set   : Y is the bigger edge. Bit 0 is clear
		 *               clear : X is the bigger edge.
		 * A  : minimum edge(log2) of the map
		 * B  : maximum edge(log2) of the map
		 * XY : edges(log2) of each side of the map.
		 * SS : combination of both X and Y, thus giving the size(log2) of the map
		 */
		case 0x13: {
			uint8_t map_bits = 0;
			uint8_t log_X = Map::LogX() - 6; // subtraction is required to make the minimal size (64) zero based
			uint8_t log_Y = Map::LogY() - 6;
			uint8_t max_edge = std::max(log_X, log_Y);

			if (log_X == log_Y) { // we have a squared map, since both edges are identical
				SetBit(map_bits, 0);
			} else {
				if (max_edge == log_Y) SetBit(map_bits, 1); // edge Y been the biggest, mark it
			}

			return (map_bits << 24) | (std::min(log_X, log_Y) << 20) | (max_edge << 16) |
				(log_X << 12) | (log_Y << 8) | (log_X + log_Y);
		}

		/* The maximum height of the map. */
		case 0x14:
			return _settings_game.construction.map_height_limit;

		/* Extra foundations base sprite */
		case 0x15:
			return SPR_SLOPES_BASE;

		/* Shore base sprite */
		case 0x16:
			return SPR_SHORE_BASE;

		/* Game map seed */
		case 0x17:
			return _settings_game.game_creation.generation_seed;

		default:
			GrfMsg(2, "ParamSet: Unknown Patch variable 0x{:02X}.", param);
			return 0;
	}
}

static uint32_t PerformGRM(std::span<uint32_t> grm, uint16_t count, uint8_t op, uint8_t target, const char *type)
{
	uint start = 0;
	uint size  = 0;

	if (op == 6) {
		/* Return GRFID of set that reserved ID */
		return grm[_cur_gps.grffile->GetParam(target)];
	}

	/* With an operation of 2 or 3, we want to reserve a specific block of IDs */
	if (op == 2 || op == 3) start = _cur_gps.grffile->GetParam(target);

	for (uint i = start; i < std::size(grm); i++) {
		if (grm[i] == 0) {
			size++;
		} else {
			if (op == 2 || op == 3) break;
			start = i + 1;
			size = 0;
		}

		if (size == count) break;
	}

	if (size == count) {
		/* Got the slot... */
		if (op == 0 || op == 3) {
			GrfMsg(2, "ParamSet: GRM: Reserving {} {} at {}", count, type, start);
			for (uint i = 0; i < count; i++) grm[start + i] = _cur_gps.grffile->grfid;
		}
		return start;
	}

	/* Unable to allocate */
	if (op != 4 && op != 5) {
		/* Deactivate GRF */
		GrfMsg(0, "ParamSet: GRM: Unable to allocate {} {}, deactivating", count, type);
		DisableGrf(STR_NEWGRF_ERROR_GRM_FAILED);
		return UINT_MAX;
	}

	GrfMsg(1, "ParamSet: GRM: Unable to allocate {} {}", count, type);
	return UINT_MAX;
}

/** Action 0x0D: Set parameter */
static void ParamSet(ByteReader &buf)
{
	/* <0D> <target> <operation> <source1> <source2> [<data>]
	 *
	 * B target        parameter number where result is stored
	 * B operation     operation to perform, see below
	 * B source1       first source operand
	 * B source2       second source operand
	 * D data          data to use in the calculation, not necessary
	 *                 if both source1 and source2 refer to actual parameters
	 *
	 * Operations
	 * 00      Set parameter equal to source1
	 * 01      Addition, source1 + source2
	 * 02      Subtraction, source1 - source2
	 * 03      Unsigned multiplication, source1 * source2 (both unsigned)
	 * 04      Signed multiplication, source1 * source2 (both signed)
	 * 05      Unsigned bit shift, source1 by source2 (source2 taken to be a
	 *         signed quantity; left shift if positive and right shift if
	 *         negative, source1 is unsigned)
	 * 06      Signed bit shift, source1 by source2
	 *         (source2 like in 05, and source1 as well)
	 */

	uint8_t target = buf.ReadByte();
	uint8_t oper   = buf.ReadByte();
	uint32_t src1  = buf.ReadByte();
	uint32_t src2  = buf.ReadByte();

	uint32_t data = 0;
	if (buf.Remaining() >= 4) data = buf.ReadDWord();

	/* You can add 80 to the operation to make it apply only if the target
	 * is not defined yet.  In this respect, a parameter is taken to be
	 * defined if any of the following applies:
	 * - it has been set to any value in the newgrf(w).cfg parameter list
	 * - it OR A PARAMETER WITH HIGHER NUMBER has been set to any value by
	 *   an earlier action D */
	if (HasBit(oper, 7)) {
		if (target < 0x80 && target < std::size(_cur_gps.grffile->param)) {
			GrfMsg(7, "ParamSet: Param {} already defined, skipping", target);
			return;
		}

		oper = GB(oper, 0, 7);
	}

	if (src2 == 0xFE) {
		if (GB(data, 0, 8) == 0xFF) {
			if (data == 0x0000FFFF) {
				/* Patch variables */
				src1 = GetPatchVariable(src1);
			} else {
				/* GRF Resource Management */
				uint8_t  op      = src1;
				uint8_t  feature = GB(data, 8, 8);
				uint16_t count   = GB(data, 16, 16);

				if (_cur_gps.stage == GLS_RESERVE) {
					if (feature == 0x08) {
						/* General sprites */
						if (op == 0) {
							/* Check if the allocated sprites will fit below the original sprite limit */
							if (_cur_gps.spriteid + count >= 16384) {
								GrfMsg(0, "ParamSet: GRM: Unable to allocate {} sprites; try changing NewGRF order", count);
								DisableGrf(STR_NEWGRF_ERROR_GRM_FAILED);
								return;
							}

							/* Reserve space at the current sprite ID */
							GrfMsg(4, "ParamSet: GRM: Allocated {} sprites at {}", count, _cur_gps.spriteid);
							_grm_sprites[GRFLocation(_cur_gps.grffile->grfid, _cur_gps.nfo_line)] = std::make_pair(_cur_gps.spriteid, count);
							_cur_gps.spriteid += count;
						}
					}
					/* Ignore GRM result during reservation */
					src1 = 0;
				} else if (_cur_gps.stage == GLS_ACTIVATION) {
					switch (feature) {
						case 0x00: // Trains
						case 0x01: // Road Vehicles
						case 0x02: // Ships
						case 0x03: // Aircraft
							if (!_settings_game.vehicle.dynamic_engines) {
								src1 = PerformGRM({std::begin(_grm_engines) + _engine_offsets[feature], _engine_counts[feature]}, count, op, target, "vehicles");
								if (_cur_gps.skip_sprites == -1) return;
							} else {
								/* GRM does not apply for dynamic engine allocation. */
								switch (op) {
									case 2:
									case 3:
										src1 = _cur_gps.grffile->GetParam(target);
										break;

									default:
										src1 = 0;
										break;
								}
							}
							break;

						case 0x08: // General sprites
							switch (op) {
								case 0:
									/* Return space reserved during reservation stage */
									src1 = _grm_sprites[GRFLocation(_cur_gps.grffile->grfid, _cur_gps.nfo_line)].first;
									GrfMsg(4, "ParamSet: GRM: Using pre-allocated sprites at {}", src1);
									break;

								case 1:
									src1 = _cur_gps.spriteid;
									break;

								default:
									GrfMsg(1, "ParamSet: GRM: Unsupported operation {} for general sprites", op);
									return;
							}
							break;

						case 0x0B: // Cargo
							/* There are two ranges: one for cargo IDs and one for cargo bitmasks */
							src1 = PerformGRM(_grm_cargoes, count, op, target, "cargoes");
							if (_cur_gps.skip_sprites == -1) return;
							break;

						default: GrfMsg(1, "ParamSet: GRM: Unsupported feature 0x{:X}", feature); return;
					}
				} else {
					/* Ignore GRM during initialization */
					src1 = 0;
				}
			}
		} else {
			/* Read another GRF File's parameter */
			const GRFFile *file = GetFileByGRFID(data);
			GRFConfig *c = GetGRFConfig(data);
			if (c != nullptr && c->flags.Test(GRFConfigFlag::Static) && !_cur_gps.grfconfig->flags.Test(GRFConfigFlag::Static) && _networking) {
				/* Disable the read GRF if it is a static NewGRF. */
				DisableStaticNewGRFInfluencingNonStaticNewGRFs(*c);
				src1 = 0;
			} else if (file == nullptr || c == nullptr || c->status == GCS_DISABLED) {
				src1 = 0;
			} else if (src1 == 0xFE) {
				src1 = c->version;
			} else {
				src1 = file->GetParam(src1);
			}
		}
	} else {
		/* The source1 and source2 operands refer to the grf parameter number
		 * like in action 6 and 7.  In addition, they can refer to the special
		 * variables available in action 7, or they can be FF to use the value
		 * of <data>.  If referring to parameters that are undefined, a value
		 * of 0 is used instead.  */
		src1 = (src1 == 0xFF) ? data : GetParamVal(src1, nullptr);
		src2 = (src2 == 0xFF) ? data : GetParamVal(src2, nullptr);
	}

	uint32_t res;
	switch (oper) {
		case 0x00:
			res = src1;
			break;

		case 0x01:
			res = src1 + src2;
			break;

		case 0x02:
			res = src1 - src2;
			break;

		case 0x03:
			res = src1 * src2;
			break;

		case 0x04:
			res = (int32_t)src1 * (int32_t)src2;
			break;

		case 0x05:
			if ((int32_t)src2 < 0) {
				res = src1 >> -(int32_t)src2;
			} else {
				res = src1 << (src2 & 0x1F); // Same behaviour as in EvalAdjustT, mask 'value' to 5 bits, which should behave the same on all architectures.
			}
			break;

		case 0x06:
			if ((int32_t)src2 < 0) {
				res = (int32_t)src1 >> -(int32_t)src2;
			} else {
				res = (int32_t)src1 << (src2 & 0x1F); // Same behaviour as in EvalAdjustT, mask 'value' to 5 bits, which should behave the same on all architectures.
			}
			break;

		case 0x07: // Bitwise AND
			res = src1 & src2;
			break;

		case 0x08: // Bitwise OR
			res = src1 | src2;
			break;

		case 0x09: // Unsigned division
			if (src2 == 0) {
				res = src1;
			} else {
				res = src1 / src2;
			}
			break;

		case 0x0A: // Signed division
			if (src2 == 0) {
				res = src1;
			} else {
				res = (int32_t)src1 / (int32_t)src2;
			}
			break;

		case 0x0B: // Unsigned modulo
			if (src2 == 0) {
				res = src1;
			} else {
				res = src1 % src2;
			}
			break;

		case 0x0C: // Signed modulo
			if (src2 == 0) {
				res = src1;
			} else {
				res = (int32_t)src1 % (int32_t)src2;
			}
			break;

		default: GrfMsg(0, "ParamSet: Unknown operation {}, skipping", oper); return;
	}

	switch (target) {
		case 0x8E: // Y-Offset for train sprites
			_cur_gps.grffile->traininfo_vehicle_pitch = res;
			break;

		case 0x8F: { // Rail track type cost factors
			extern RailTypeInfo _railtypes[RAILTYPE_END];
			_railtypes[RAILTYPE_RAIL].cost_multiplier = GB(res, 0, 8);
			if (_settings_game.vehicle.disable_elrails) {
				_railtypes[RAILTYPE_ELECTRIC].cost_multiplier = GB(res, 0, 8);
				_railtypes[RAILTYPE_MONO].cost_multiplier = GB(res, 8, 8);
			} else {
				_railtypes[RAILTYPE_ELECTRIC].cost_multiplier = GB(res, 8, 8);
				_railtypes[RAILTYPE_MONO].cost_multiplier = GB(res, 16, 8);
			}
			_railtypes[RAILTYPE_MAGLEV].cost_multiplier = GB(res, 16, 8);
			break;
		}

		/* not implemented */
		case 0x93: // Tile refresh offset to left -- Intended to allow support for larger sprites, not necessary for OTTD
		case 0x94: // Tile refresh offset to right
		case 0x95: // Tile refresh offset upwards
		case 0x96: // Tile refresh offset downwards
		case 0x97: // Snow line height -- Better supported by feature 8 property 10h (snow line table) TODO: implement by filling the entire snow line table with the given value
		case 0x99: // Global ID offset -- Not necessary since IDs are remapped automatically
			GrfMsg(7, "ParamSet: Skipping unimplemented target 0x{:02X}", target);
			break;

		case 0x9E: { // Miscellaneous GRF features
			GrfMiscBits bits(res);

			/* Set train list engine width */
			_cur_gps.grffile->traininfo_vehicle_width = bits.Test(GrfMiscBit::TrainWidth32Pixels) ? VEHICLEINFO_FULL_VEHICLE_WIDTH : TRAININFO_DEFAULT_VEHICLE_WIDTH;
			/* Remove the local flags from the global flags */
			bits.Reset(GrfMiscBit::TrainWidth32Pixels);

			/* Only copy safe bits for static grfs */
			if (_cur_gps.grfconfig->flags.Test(GRFConfigFlag::Static)) {
				GrfMiscBits safe_bits = GrfMiscBit::SecondRockyTileSet;

				_misc_grf_features.Reset(safe_bits);
				_misc_grf_features.Set(bits & safe_bits);
			} else {
				_misc_grf_features = bits;
			}
			break;
		}

		case 0x9F: // locale-dependent settings
			GrfMsg(7, "ParamSet: Skipping unimplemented target 0x{:02X}", target);
			break;

		default:
			if (target < 0x80) {
				/* Resize (and fill with zeroes) if needed. */
				if (target >= std::size(_cur_gps.grffile->param)) _cur_gps.grffile->param.resize(target + 1);
				_cur_gps.grffile->param[target] = res;
			} else {
				GrfMsg(7, "ParamSet: Skipping unknown target 0x{:02X}", target);
			}
			break;
	}
}

template <> void GrfActionHandler<0x0D>::FileScan(ByteReader &) { }
template <> void GrfActionHandler<0x0D>::SafetyScan(ByteReader &buf) { SafeParamSet(buf); }
template <> void GrfActionHandler<0x0D>::LabelScan(ByteReader &) { }
template <> void GrfActionHandler<0x0D>::Init(ByteReader &buf) { ParamSet(buf); }
template <> void GrfActionHandler<0x0D>::Reserve(ByteReader &buf) { ParamSet(buf); }
template <> void GrfActionHandler<0x0D>::Activation(ByteReader &buf) { ParamSet(buf); }
