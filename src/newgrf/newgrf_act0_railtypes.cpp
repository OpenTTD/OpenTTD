/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_act0_railtypes.cpp NewGRF Action 0x00 handler for railtypes. */

#include "../stdafx.h"
#include "../debug.h"
#include "../rail.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"
#include "newgrf_stringmapping.h"

#include "../safeguards.h"

/**
 * Define properties for railtypes
 * @param first Local ID of the first railtype.
 * @param last Local ID of the last railtype.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult RailTypeChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	extern RailTypeInfo _railtypes[RAILTYPE_END];

	if (last > RAILTYPE_END) {
		GrfMsg(1, "RailTypeChangeInfo: Rail type {} is invalid, max {}, ignoring", last, RAILTYPE_END);
		return CIR_INVALID_ID;
	}

	for (uint id = first; id < last; ++id) {
		RailType rt = _cur_gps.grffile->railtype_map[id];
		if (rt == INVALID_RAILTYPE) return CIR_INVALID_ID;

		RailTypeInfo *rti = &_railtypes[rt];

		switch (prop) {
			case 0x08: // Label of rail type
				/* Skipped here as this is loaded during reservation stage. */
				buf.ReadDWord();
				break;

			case 0x09: { // Toolbar caption of railtype (sets name as well for backwards compatibility for grf ver < 8)
				GRFStringID str{buf.ReadWord()};
				AddStringForMapping(str, &rti->strings.toolbar_caption);
				if (_cur_gps.grffile->grf_version < 8) {
					AddStringForMapping(str, &rti->strings.name);
				}
				break;
			}

			case 0x0A: // Menu text of railtype
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &rti->strings.menu_text);
				break;

			case 0x0B: // Build window caption
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &rti->strings.build_caption);
				break;

			case 0x0C: // Autoreplace text
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &rti->strings.replace_text);
				break;

			case 0x0D: // New locomotive text
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &rti->strings.new_loco);
				break;

			case 0x0E: // Compatible railtype list
			case 0x0F: // Powered railtype list
			case 0x18: // Railtype list required for date introduction
			case 0x19: // Introduced railtype list
			{
				/* Rail type compatibility bits are added to the existing bits
				 * to allow multiple GRFs to modify compatibility with the
				 * default rail types. */
				int n = buf.ReadByte();
				for (int j = 0; j != n; j++) {
					RailTypeLabel label = buf.ReadDWord();
					RailType resolved_rt = GetRailTypeByLabel(std::byteswap(label), false);
					if (resolved_rt != INVALID_RAILTYPE) {
						switch (prop) {
							case 0x0F: rti->powered_railtypes.Set(resolved_rt);               [[fallthrough]]; // Powered implies compatible.
							case 0x0E: rti->compatible_railtypes.Set(resolved_rt);            break;
							case 0x18: rti->introduction_required_railtypes.Set(resolved_rt); break;
							case 0x19: rti->introduces_railtypes.Set(resolved_rt);            break;
						}
					}
				}
				break;
			}

			case 0x10: // Rail Type flags
				rti->flags = static_cast<RailTypeFlags>(buf.ReadByte());
				break;

			case 0x11: // Curve speed advantage
				rti->curve_speed = buf.ReadByte();
				break;

			case 0x12: // Station graphic
				rti->fallback_railtype = Clamp(buf.ReadByte(), 0, 2);
				break;

			case 0x13: // Construction cost factor
				rti->cost_multiplier = buf.ReadWord();
				break;

			case 0x14: // Speed limit
				rti->max_speed = buf.ReadWord();
				break;

			case 0x15: // Acceleration model
				rti->acceleration_type = Clamp(buf.ReadByte(), 0, 2);
				break;

			case 0x16: // Map colour
				rti->map_colour = buf.ReadByte();
				break;

			case 0x17: // Introduction date
				rti->introduction_date = TimerGameCalendar::Date(buf.ReadDWord());
				break;

			case 0x1A: // Sort order
				rti->sorting_order = buf.ReadByte();
				break;

			case 0x1B: // Name of railtype (overridden by prop 09 for grf ver < 8)
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &rti->strings.name);
				break;

			case 0x1C: // Maintenance cost factor
				rti->maintenance_multiplier = buf.ReadWord();
				break;

			case 0x1D: // Alternate rail type label list
				/* Skipped here as this is loaded during reservation stage. */
				for (int j = buf.ReadByte(); j != 0; j--) buf.ReadDWord();
				break;

			case 0x1E: // Badge list
				rti->badges = ReadBadgeList(buf, GSF_RAILTYPES);
				break;

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

static ChangeInfoResult RailTypeReserveInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	extern RailTypeInfo _railtypes[RAILTYPE_END];

	if (last > RAILTYPE_END) {
		GrfMsg(1, "RailTypeReserveInfo: Rail type {} is invalid, max {}, ignoring", last, RAILTYPE_END);
		return CIR_INVALID_ID;
	}

	for (uint id = first; id < last; ++id) {
		switch (prop) {
			case 0x08: // Label of rail type
			{
				RailTypeLabel rtl = buf.ReadDWord();
				rtl = std::byteswap(rtl);

				RailType rt = GetRailTypeByLabel(rtl, false);
				if (rt == INVALID_RAILTYPE) {
					/* Set up new rail type */
					rt = AllocateRailType(rtl);
				}

				_cur_gps.grffile->railtype_map[id] = rt;
				break;
			}

			case 0x09: // Toolbar caption of railtype
			case 0x0A: // Menu text
			case 0x0B: // Build window caption
			case 0x0C: // Autoreplace text
			case 0x0D: // New loco
			case 0x13: // Construction cost
			case 0x14: // Speed limit
			case 0x1B: // Name of railtype
			case 0x1C: // Maintenance cost factor
				buf.ReadWord();
				break;

			case 0x1D: // Alternate rail type label list
				if (_cur_gps.grffile->railtype_map[id] != INVALID_RAILTYPE) {
					int n = buf.ReadByte();
					for (int j = 0; j != n; j++) {
						_railtypes[_cur_gps.grffile->railtype_map[id]].alternate_labels.push_back(std::byteswap(buf.ReadDWord()));
					}
					break;
				}
				GrfMsg(1, "RailTypeReserveInfo: Ignoring property 1D for rail type {} because no label was set", id);
				[[fallthrough]];

			case 0x0E: // Compatible railtype list
			case 0x0F: // Powered railtype list
			case 0x18: // Railtype list required for date introduction
			case 0x19: // Introduced railtype list
				for (int j = buf.ReadByte(); j != 0; j--) buf.ReadDWord();
				break;

			case 0x10: // Rail Type flags
			case 0x11: // Curve speed advantage
			case 0x12: // Station graphic
			case 0x15: // Acceleration model
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

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_RAILTYPES>::Reserve(uint first, uint last, int prop, ByteReader &buf) { return RailTypeReserveInfo(first, last, prop, buf); }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_RAILTYPES>::Activation(uint first, uint last, int prop, ByteReader &buf) { return RailTypeChangeInfo(first, last, prop, buf); }
