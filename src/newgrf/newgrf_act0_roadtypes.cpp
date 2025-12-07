/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act0_roadtypes.cpp NewGRF Action 0x00 handler for roadtypes. */

#include "../stdafx.h"
#include "../debug.h"
#include "../road.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"
#include "newgrf_stringmapping.h"

#include "../safeguards.h"

/**
 * Define properties for roadtypes
 * @param first Local ID of the first roadtype.
 * @param last Local ID of the last roadtype.
 * @param prop The property to change.
 * @param buf The property value.
 * @param rtt Road/tram type.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult RoadTypeChangeInfo(uint first, uint last, int prop, ByteReader &buf, RoadTramType rtt)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	extern RoadTypeInfo _roadtypes[ROADTYPE_END];
	const auto &type_map = (rtt == RTT_TRAM) ? _cur_gps.grffile->tramtype_map : _cur_gps.grffile->roadtype_map;

	if (last > std::size(type_map)) {
		GrfMsg(1, "RoadTypeChangeInfo: Road type {} is invalid, max {}, ignoring", last, std::size(type_map));
		return CIR_INVALID_ID;
	}

	for (uint id = first; id < last; ++id) {
		RoadType rt = type_map[id];
		if (rt == INVALID_ROADTYPE) return CIR_INVALID_ID;

		RoadTypeInfo *rti = &_roadtypes[rt];

		switch (prop) {
			case 0x08: // Label of road type
				/* Skipped here as this is loaded during reservation stage. */
				buf.ReadDWord();
				break;

			case 0x09: // Toolbar caption of roadtype
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &rti->strings.toolbar_caption);
				break;

			case 0x0A: // Menu text of roadtype
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &rti->strings.menu_text);
				break;

			case 0x0B: // Build window caption
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &rti->strings.build_caption);
				break;

			case 0x0C: // Autoreplace text
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &rti->strings.replace_text);
				break;

			case 0x0D: // New engine text
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &rti->strings.new_engine);
				break;

			case 0x0F: // Powered roadtype list
			case 0x18: // Roadtype list required for date introduction
			case 0x19: { // Introduced roadtype list
				/* Road type compatibility bits are added to the existing bits
				 * to allow multiple GRFs to modify compatibility with the
				 * default road types. */
				int n = buf.ReadByte();
				for (int j = 0; j != n; j++) {
					RoadTypeLabel label = buf.ReadDWord();
					RoadType resolved_rt = GetRoadTypeByLabel(std::byteswap(label), false);
					if (resolved_rt != INVALID_ROADTYPE) {
						switch (prop) {
							case 0x0F:
								if (GetRoadTramType(resolved_rt) == rtt) {
									rti->powered_roadtypes.Set(resolved_rt);
								} else {
									GrfMsg(1, "RoadTypeChangeInfo: Powered road type list: Road type {} road/tram type does not match road type {}, ignoring", resolved_rt, rt);
								}
								break;
							case 0x18: rti->introduction_required_roadtypes.Set(resolved_rt); break;
							case 0x19: rti->introduces_roadtypes.Set(resolved_rt);            break;
						}
					}
				}
				break;
			}

			case 0x10: // Road Type flags
				rti->flags = static_cast<RoadTypeFlags>(buf.ReadByte());
				break;

			case 0x13: // Construction cost factor
				rti->cost_multiplier = buf.ReadWord();
				break;

			case 0x14: // Speed limit
				rti->max_speed = buf.ReadWord();
				break;

			case 0x16: // Map colour
				rti->map_colour = PixelColour{buf.ReadByte()};
				break;

			case 0x17: // Introduction date
				rti->introduction_date = TimerGameCalendar::Date(buf.ReadDWord());
				break;

			case 0x1A: // Sort order
				rti->sorting_order = buf.ReadByte();
				break;

			case 0x1B: // Name of roadtype
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &rti->strings.name);
				break;

			case 0x1C: // Maintenance cost factor
				rti->maintenance_multiplier = buf.ReadWord();
				break;

			case 0x1D: // Alternate road type label list
				/* Skipped here as this is loaded during reservation stage. */
				for (int j = buf.ReadByte(); j != 0; j--) buf.ReadDWord();
				break;

			case 0x1E: // Badge list
				rti->badges = ReadBadgeList(buf, GSF_ROADTYPES);
				break;

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

static ChangeInfoResult RoadTypeReserveInfo(uint first, uint last, int prop, ByteReader &buf, RoadTramType rtt)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	extern RoadTypeInfo _roadtypes[ROADTYPE_END];
	auto &type_map = (rtt == RTT_TRAM) ? _cur_gps.grffile->tramtype_map : _cur_gps.grffile->roadtype_map;

	if (last > std::size(type_map)) {
		GrfMsg(1, "RoadTypeReserveInfo: Road type {} is invalid, max {}, ignoring", last, std::size(type_map));
		return CIR_INVALID_ID;
	}

	for (uint id = first; id < last; ++id) {
		switch (prop) {
			case 0x08: { // Label of road type
				RoadTypeLabel rtl = buf.ReadDWord();
				rtl = std::byteswap(rtl);

				RoadType rt = GetRoadTypeByLabel(rtl, false);
				if (rt == INVALID_ROADTYPE) {
					/* Set up new road type */
					rt = AllocateRoadType(rtl, rtt);
				} else if (GetRoadTramType(rt) != rtt) {
					GrfMsg(1, "RoadTypeReserveInfo: Road type {} is invalid type (road/tram), ignoring", id);
					return CIR_INVALID_ID;
				}

				type_map[id] = rt;
				break;
			}
			case 0x09: // Toolbar caption of roadtype
			case 0x0A: // Menu text
			case 0x0B: // Build window caption
			case 0x0C: // Autoreplace text
			case 0x0D: // New loco
			case 0x13: // Construction cost
			case 0x14: // Speed limit
			case 0x1B: // Name of roadtype
			case 0x1C: // Maintenance cost factor
				buf.ReadWord();
				break;

			case 0x1D: // Alternate road type label list
				if (type_map[id] != INVALID_ROADTYPE) {
					int n = buf.ReadByte();
					for (int j = 0; j != n; j++) {
						_roadtypes[type_map[id]].alternate_labels.insert(std::byteswap(buf.ReadDWord()));
					}
					break;
				}
				GrfMsg(1, "RoadTypeReserveInfo: Ignoring property 1D for road type {} because no label was set", id);
				/* FALL THROUGH */

			case 0x0F: // Powered roadtype list
			case 0x18: // Roadtype list required for date introduction
			case 0x19: // Introduced roadtype list
				for (int j = buf.ReadByte(); j != 0; j--) buf.ReadDWord();
				break;

			case 0x10: // Road Type flags
			case 0x16: // Map colour
			case 0x1A: // Sort order
				buf.ReadByte();
				break;

			case 0x17: // Introduction date
				buf.ReadDWord();
				break;

			case 0x1E: // Badge list
				SkipBadgeList(buf);
				break;

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_ROADTYPES>::Reserve(uint first, uint last, int prop, ByteReader &buf) { return RoadTypeReserveInfo(first, last, prop, buf, RTT_ROAD); }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_ROADTYPES>::Activation(uint first, uint last, int prop, ByteReader &buf) { return RoadTypeChangeInfo(first, last, prop, buf, RTT_ROAD); }

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_TRAMTYPES>::Reserve(uint first, uint last, int prop, ByteReader &buf) { return RoadTypeReserveInfo(first, last, prop, buf, RTT_TRAM); }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_TRAMTYPES>::Activation(uint first, uint last, int prop, ByteReader &buf) { return RoadTypeChangeInfo(first, last, prop, buf, RTT_TRAM); }
