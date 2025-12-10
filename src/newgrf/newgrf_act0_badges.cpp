/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act0_badges.cpp NewGRF Action 0x00 handler for badges. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_badge.h"
#include "../newgrf_badge_type.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

static ChangeInfoResult BadgeChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	if (last >= UINT16_MAX) {
		GrfMsg(1, "BadgeChangeInfo: Tag {} is invalid, max {}, ignoring", last, UINT16_MAX - 1);
		return CIR_INVALID_ID;
	}

	for (uint id = first; id < last; ++id) {
		auto it = _cur_gps.grffile->badge_map.find(id);
		if (prop != 0x08 && it == std::end(_cur_gps.grffile->badge_map)) {
			GrfMsg(1, "BadgeChangeInfo: Attempt to modify undefined tag {}, ignoring", id);
			return CIR_INVALID_ID;
		}

		Badge *badge = nullptr;
		if (prop != 0x08) badge = GetBadge(it->second);

		switch (prop) {
			case 0x08: { // Label
				std::string_view label = buf.ReadString();
				_cur_gps.grffile->badge_map[id] = GetOrCreateBadge(label).index;
				break;
			}

			case 0x09: // Flags
				badge->flags = static_cast<BadgeFlags>(buf.ReadDWord());
				break;

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_BADGES>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_BADGES>::Activation(uint first, uint last, int prop, ByteReader &buf) { return BadgeChangeInfo(first, last, prop, buf); }
