/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act0_stations.cpp NewGRF Action 0x00 handler for stations. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_station.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"
#include "newgrf_stringmapping.h"

#include "../safeguards.h"

/** The maximum amount of stations a single GRF is allowed to add */
static const uint NUM_STATIONS_PER_GRF = UINT16_MAX - 1;

/**
 * Define properties for stations
 * @param first Local ID of the first station.
 * @param last Local ID of the last station.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult StationChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	if (last > NUM_STATIONS_PER_GRF) {
		GrfMsg(1, "StationChangeInfo: Station {} is invalid, max {}, ignoring", last, NUM_STATIONS_PER_GRF);
		return CIR_INVALID_ID;
	}

	/* Allocate station specs if necessary */
	if (_cur_gps.grffile->stations.size() < last) _cur_gps.grffile->stations.resize(last);

	for (uint id = first; id < last; ++id) {
		auto &statspec = _cur_gps.grffile->stations[id];

		/* Check that the station we are modifying is defined. */
		if (statspec == nullptr && prop != 0x08) {
			GrfMsg(2, "StationChangeInfo: Attempt to modify undefined station {}, ignoring", id);
			return CIR_INVALID_ID;
		}

		switch (prop) {
			case 0x08: { // Class ID
				/* Property 0x08 is special; it is where the station is allocated */
				if (statspec == nullptr) {
					statspec = std::make_unique<StationSpec>();
				}

				/* Swap classid because we read it in BE meaning WAYP or DFLT */
				uint32_t classid = buf.ReadDWord();
				statspec->class_index = StationClass::Allocate(std::byteswap(classid));
				break;
			}

			case 0x09: { // Define sprite layout
				uint16_t tiles = buf.ReadExtendedByte();
				statspec->renderdata.clear(); // delete earlier loaded stuff
				statspec->renderdata.reserve(tiles);

				for (uint t = 0; t < tiles; t++) {
					NewGRFSpriteLayout *dts = &statspec->renderdata.emplace_back();
					dts->consistent_max_offset = UINT16_MAX; // Spritesets are unknown, so no limit.

					if (buf.HasData(4) && buf.PeekDWord() == 0) {
						buf.Skip(4);
						extern const DrawTileSpriteSpan _station_display_datas_rail[8];
						const DrawTileSpriteSpan &dtss = _station_display_datas_rail[t % 8];
						dts->ground = dtss.ground;
						dts->seq.insert(dts->seq.end(), dtss.GetSequence().begin(), dtss.GetSequence().end());
						continue;
					}

					ReadSpriteLayoutSprite(buf, false, false, false, GSF_STATIONS, &dts->ground);
					/* On error, bail out immediately. Temporary GRF data was already freed */
					if (_cur_gps.skip_sprites < 0) return CIR_DISABLED;

					std::vector<DrawTileSeqStruct> tmp_layout;
					for (;;) {
						uint8_t delta_x = buf.ReadByte();
						if (delta_x == 0x80) break;

						/* no relative bounding box support */
						DrawTileSeqStruct &dtss = tmp_layout.emplace_back();
						dtss.origin.x = delta_x;
						dtss.origin.y = buf.ReadByte();
						dtss.origin.z = buf.ReadByte();
						dtss.extent.x = buf.ReadByte();
						dtss.extent.y = buf.ReadByte();
						dtss.extent.z = buf.ReadByte();

						ReadSpriteLayoutSprite(buf, false, true, false, GSF_STATIONS, &dtss.image);
						/* On error, bail out immediately. Temporary GRF data was already freed */
						if (_cur_gps.skip_sprites < 0) return CIR_DISABLED;
					}
					dts->seq = std::move(tmp_layout);
				}

				/* Number of layouts must be even, alternating X and Y */
				if (statspec->renderdata.size() & 1) {
					GrfMsg(1, "StationChangeInfo: Station {} defines an odd number of sprite layouts, dropping the last item", id);
					statspec->renderdata.pop_back();
				}
				break;
			}

			case 0x0A: { // Copy sprite layout
				uint16_t srcid = buf.ReadExtendedByte();
				const StationSpec *srcstatspec = srcid >= _cur_gps.grffile->stations.size() ? nullptr : _cur_gps.grffile->stations[srcid].get();

				if (srcstatspec == nullptr) {
					GrfMsg(1, "StationChangeInfo: Station {} is not defined, cannot copy sprite layout to {}.", srcid, id);
					continue;
				}

				statspec->renderdata.clear(); // delete earlier loaded stuff
				statspec->renderdata.reserve(srcstatspec->renderdata.size());

				for (const auto &it : srcstatspec->renderdata) {
					statspec->renderdata.emplace_back(it);
				}
				break;
			}

			case 0x0B: // Callback mask
				statspec->callback_mask = static_cast<StationCallbackMasks>(buf.ReadByte());
				break;

			case 0x0C: // Disallowed number of platforms
				statspec->disallowed_platforms = buf.ReadByte();
				break;

			case 0x0D: // Disallowed platform lengths
				statspec->disallowed_lengths = buf.ReadByte();
				break;

			case 0x0E: // Define custom layout
				while (buf.HasData()) {
					uint8_t length = buf.ReadByte();
					uint8_t number = buf.ReadByte();

					if (length == 0 || number == 0) break;

					const uint8_t *buf_layout = buf.ReadBytes(length * number);

					/* Create entry in layouts and assign the layout to it. */
					auto &layout = statspec->layouts[GetStationLayoutKey(number, length)];
					layout.assign(buf_layout, buf_layout + length * number);

					/* Ensure the first bit, axis, is zero. The rest of the value is validated during rendering, as we don't know the range yet. */
					for (auto &tile : layout) {
						if ((tile & ~1U) != tile) {
							GrfMsg(1, "StationChangeInfo: Invalid tile {} in layout {}x{}", tile, length, number);
							tile &= ~1U;
						}
					}
				}
				break;

			case 0x0F: { // Copy custom layout
				uint16_t srcid = buf.ReadExtendedByte();
				const StationSpec *srcstatspec = srcid >= _cur_gps.grffile->stations.size() ? nullptr : _cur_gps.grffile->stations[srcid].get();

				if (srcstatspec == nullptr) {
					GrfMsg(1, "StationChangeInfo: Station {} is not defined, cannot copy tile layout to {}.", srcid, id);
					continue;
				}

				statspec->layouts = srcstatspec->layouts;
				break;
			}

			case 0x10: // Little/lots cargo threshold
				statspec->cargo_threshold = buf.ReadWord();
				break;

			case 0x11: { // Pylon placement
				uint8_t pylons = buf.ReadByte();
				if (statspec->tileflags.size() < 8) statspec->tileflags.resize(8);
				for (int j = 0; j < 8; ++j) {
					if (HasBit(pylons, j)) {
						statspec->tileflags[j].Set(StationSpec::TileFlag::Pylons);
					} else {
						statspec->tileflags[j].Reset(StationSpec::TileFlag::Pylons);
					}
				}
				break;
			}

			case 0x12: // Cargo types for random triggers
				if (_cur_gps.grffile->grf_version >= 7) {
					statspec->cargo_triggers = TranslateRefitMask(buf.ReadDWord());
				} else {
					statspec->cargo_triggers = (CargoTypes)buf.ReadDWord();
				}
				break;

			case 0x13: // General flags
				statspec->flags = StationSpecFlags{buf.ReadByte()};
				break;

			case 0x14: { // Overhead wire placement
				uint8_t wires = buf.ReadByte();
				if (statspec->tileflags.size() < 8) statspec->tileflags.resize(8);
				for (int j = 0; j < 8; ++j) {
					if (HasBit(wires, j)) {
						statspec->tileflags[j].Set(StationSpec::TileFlag::NoWires);
					} else {
						statspec->tileflags[j].Reset(StationSpec::TileFlag::NoWires);
					}
				}
				break;
			}

			case 0x15: { // Blocked tiles
				uint8_t blocked = buf.ReadByte();
				if (statspec->tileflags.size() < 8) statspec->tileflags.resize(8);
				for (int j = 0; j < 8; ++j) {
					if (HasBit(blocked, j)) {
						statspec->tileflags[j].Set(StationSpec::TileFlag::Blocked);
					} else {
						statspec->tileflags[j].Reset(StationSpec::TileFlag::Blocked);
					}
				}
				break;
			}

			case 0x16: // Animation info
				statspec->animation.frames = buf.ReadByte();
				statspec->animation.status = static_cast<AnimationStatus>(buf.ReadByte());
				break;

			case 0x17: // Animation speed
				statspec->animation.speed = buf.ReadByte();
				break;

			case 0x18: // Animation triggers
				statspec->animation.triggers = static_cast<StationAnimationTriggers>(buf.ReadWord());
				break;

			/* 0x19 road routing (not implemented) */

			case 0x1A: { // Advanced sprite layout
				uint16_t tiles = buf.ReadExtendedByte();
				statspec->renderdata.clear(); // delete earlier loaded stuff
				statspec->renderdata.reserve(tiles);

				for (uint t = 0; t < tiles; t++) {
					NewGRFSpriteLayout *dts = &statspec->renderdata.emplace_back();
					uint num_building_sprites = buf.ReadByte();
					/* On error, bail out immediately. Temporary GRF data was already freed */
					if (ReadSpriteLayout(buf, num_building_sprites, false, GSF_STATIONS, true, false, dts)) return CIR_DISABLED;
				}

				/* Number of layouts must be even, alternating X and Y */
				if (statspec->renderdata.size() & 1) {
					GrfMsg(1, "StationChangeInfo: Station {} defines an odd number of sprite layouts, dropping the last item", id);
					statspec->renderdata.pop_back();
				}
				break;
			}

			case 0x1B: // Minimum bridge height (not implemented)
				buf.ReadWord();
				buf.ReadWord();
				buf.ReadWord();
				buf.ReadWord();
				break;

			case 0x1C: // Station Name
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &statspec->name);
				break;

			case 0x1D: // Station Class name
				AddStringForMapping(GRFStringID{buf.ReadWord()}, [statspec = statspec.get()](StringID str) { StationClass::Get(statspec->class_index)->name = str; });
				break;

			case 0x1E: { // Extended tile flags (replaces prop 11, 14 and 15)
				uint16_t tiles = buf.ReadExtendedByte();
				auto flags = reinterpret_cast<const StationSpec::TileFlags *>(buf.ReadBytes(tiles));
				statspec->tileflags.assign(flags, flags + tiles);
				break;
			}

			case 0x1F: // Badge list
				statspec->badges = ReadBadgeList(buf, GSF_STATIONS);
				break;

			case 0x20: { // Minimum bridge height (extended)
				uint16_t tiles = buf.ReadExtendedByte();
				if (statspec->bridgeable_info.size() < tiles) statspec->bridgeable_info.resize(tiles);
				for (int j = 0; j != tiles; ++j) {
					statspec->bridgeable_info[j].height = buf.ReadByte();
				}
				break;
			}

			case 0x21: { // Disallowed bridge pillars
				uint16_t tiles = buf.ReadExtendedByte();
				if (statspec->bridgeable_info.size() < tiles) statspec->bridgeable_info.resize(tiles);
				for (int j = 0; j != tiles; ++j) {
					statspec->bridgeable_info[j].disallowed_pillars = BridgePillarFlags{buf.ReadByte()};
				}
				break;
			}

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_STATIONS>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_STATIONS>::Activation(uint first, uint last, int prop, ByteReader &buf) { return StationChangeInfo(first, last, prop, buf); }
