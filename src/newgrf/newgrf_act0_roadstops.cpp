/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_act0_roadstops.cpp NewGRF Action 0x00 handler for roadstops. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_engine.h"
#include "../newgrf_roadstop.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"
#include "newgrf_stringmapping.h"

#include "../safeguards.h"

/**
 * Ignore properties for roadstops
 * @param prop The property to ignore.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult IgnoreRoadStopProperty(uint prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	switch (prop) {
		case 0x09:
		case 0x0C:
		case 0x0F:
		case 0x11:
			buf.ReadByte();
			break;

		case 0x0A:
		case 0x0B:
		case 0x0E:
		case 0x10:
		case 0x15:
			buf.ReadWord();
			break;

		case 0x08:
		case 0x0D:
		case 0x12:
			buf.ReadDWord();
			break;

		case 0x16: // Badge list
			SkipBadgeList(buf);
			break;

		default:
			ret = CIR_UNKNOWN;
			break;
	}

	return ret;
}

static ChangeInfoResult RoadStopChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	if (last > NUM_ROADSTOPS_PER_GRF) {
		GrfMsg(1, "RoadStopChangeInfo: RoadStop {} is invalid, max {}, ignoring", last, NUM_ROADSTOPS_PER_GRF);
		return CIR_INVALID_ID;
	}

	if (_cur_gps.grffile->roadstops.size() < last) _cur_gps.grffile->roadstops.resize(last);

	for (uint id = first; id < last; ++id) {
		auto &rs = _cur_gps.grffile->roadstops[id];

		if (rs == nullptr && prop != 0x08) {
			GrfMsg(1, "RoadStopChangeInfo: Attempt to modify undefined road stop {}, ignoring", id);
			ChangeInfoResult cir = IgnoreRoadStopProperty(prop, buf);
			if (cir > ret) ret = cir;
			continue;
		}

		switch (prop) {
			case 0x08: { // Road Stop Class ID
				if (rs == nullptr) {
					rs = std::make_unique<RoadStopSpec>();
				}

				uint32_t classid = buf.ReadDWord();
				rs->class_index = RoadStopClass::Allocate(std::byteswap(classid));
				break;
			}

			case 0x09: // Road stop type
				rs->stop_type = (RoadStopAvailabilityType)buf.ReadByte();
				break;

			case 0x0A: // Road Stop Name
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &rs->name);
				break;

			case 0x0B: // Road Stop Class name
				AddStringForMapping(GRFStringID{buf.ReadWord()}, [rs = rs.get()](StringID str) { RoadStopClass::Get(rs->class_index)->name = str; });
				break;

			case 0x0C: // The draw mode
				rs->draw_mode = static_cast<RoadStopDrawMode>(buf.ReadByte());
				break;

			case 0x0D: // Cargo types for random triggers
				rs->cargo_triggers = TranslateRefitMask(buf.ReadDWord());
				break;

			case 0x0E: // Animation info
				rs->animation.frames = buf.ReadByte();
				rs->animation.status = buf.ReadByte();
				break;

			case 0x0F: // Animation speed
				rs->animation.speed = buf.ReadByte();
				break;

			case 0x10: // Animation triggers
				rs->animation.triggers = buf.ReadWord();
				break;

			case 0x11: // Callback mask
				rs->callback_mask = static_cast<RoadStopCallbackMasks>(buf.ReadByte());
				break;

			case 0x12: // General flags
				rs->flags = static_cast<RoadStopSpecFlags>(buf.ReadDWord()); // Future-proofing, size this as 4 bytes, but we only need two byte's worth of flags at present
				break;

			case 0x15: // Cost multipliers
				rs->build_cost_multiplier = buf.ReadByte();
				rs->clear_cost_multiplier = buf.ReadByte();
				break;

			case 0x16: // Badge list
				rs->badges = ReadBadgeList(buf, GSF_ROADSTOPS);
				break;

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_ROADSTOPS>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_ROADSTOPS>::Activation(uint first, uint last, int prop, ByteReader &buf) { return RoadStopChangeInfo(first, last, prop, buf); }
