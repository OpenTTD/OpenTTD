/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act0_canals.cpp NewGRF Action 0x00 handler for canals. */

#include "../stdafx.h"
#include "../debug.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

/**
 * Define properties for water features
 * @param first Local ID of the first water feature.
 * @param last Local ID of the last water feature.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult CanalChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	if (last > CF_END) {
		GrfMsg(1, "CanalChangeInfo: Canal feature 0x{:02X} is invalid, max {}, ignoring", last, CF_END);
		return CIR_INVALID_ID;
	}

	for (uint id = first; id < last; ++id) {
		CanalProperties *cp = &_cur_gps.grffile->canal_local_properties[id];

		switch (prop) {
			case 0x08:
				cp->callback_mask = static_cast<CanalCallbackMasks>(buf.ReadByte());
				break;

			case 0x09:
				cp->flags = buf.ReadByte();
				break;

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_CANALS>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_CANALS>::Activation(uint first, uint last, int prop, ByteReader &buf) { return CanalChangeInfo(first, last, prop, buf); }
