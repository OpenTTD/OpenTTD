/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_act7_9.cpp NewGRF Action 0x07 and Action 0x09 handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "../network/network.h"
#include "../newgrf_engine.h"
#include "../newgrf_cargo.h"
#include "../rail.h"
#include "../road.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

/* Action 0x07
 * Action 0x09 */
static void SkipIf(ByteReader &buf)
{
	/* <07/09> <param-num> <param-size> <condition-type> <value> <num-sprites>
	 *
	 * B param-num
	 * B param-size
	 * B condition-type
	 * V value
	 * B num-sprites */
	uint32_t cond_val = 0;
	uint32_t mask = 0;
	bool result;

	uint8_t param     = buf.ReadByte();
	uint8_t paramsize = buf.ReadByte();
	uint8_t condtype  = buf.ReadByte();

	if (condtype < 2) {
		/* Always 1 for bit tests, the given value should be ignored. */
		paramsize = 1;
	}

	switch (paramsize) {
		case 8: cond_val = buf.ReadDWord(); mask = buf.ReadDWord(); break;
		case 4: cond_val = buf.ReadDWord(); mask = 0xFFFFFFFF; break;
		case 2: cond_val = buf.ReadWord();  mask = 0x0000FFFF; break;
		case 1: cond_val = buf.ReadByte();  mask = 0x000000FF; break;
		default: break;
	}

	if (param < 0x80 && std::size(_cur.grffile->param) <= param) {
		GrfMsg(7, "SkipIf: Param {} undefined, skipping test", param);
		return;
	}

	GrfMsg(7, "SkipIf: Test condtype {}, param 0x{:02X}, condval 0x{:08X}", condtype, param, cond_val);

	/* condtypes that do not use 'param' are always valid.
	 * condtypes that use 'param' are either not valid for param 0x88, or they are only valid for param 0x88.
	 */
	if (condtype >= 0x0B) {
		/* Tests that ignore 'param' */
		switch (condtype) {
			case 0x0B: result = !IsValidCargoType(GetCargoTypeByLabel(CargoLabel(std::byteswap(cond_val))));
				break;
			case 0x0C: result = IsValidCargoType(GetCargoTypeByLabel(CargoLabel(std::byteswap(cond_val))));
				break;
			case 0x0D: result = GetRailTypeByLabel(std::byteswap(cond_val)) == INVALID_RAILTYPE;
				break;
			case 0x0E: result = GetRailTypeByLabel(std::byteswap(cond_val)) != INVALID_RAILTYPE;
				break;
			case 0x0F: {
				RoadType rt = GetRoadTypeByLabel(std::byteswap(cond_val));
				result = rt == INVALID_ROADTYPE || !RoadTypeIsRoad(rt);
				break;
			}
			case 0x10: {
				RoadType rt = GetRoadTypeByLabel(std::byteswap(cond_val));
				result = rt != INVALID_ROADTYPE && RoadTypeIsRoad(rt);
				break;
			}
			case 0x11: {
				RoadType rt = GetRoadTypeByLabel(std::byteswap(cond_val));
				result = rt == INVALID_ROADTYPE || !RoadTypeIsTram(rt);
				break;
			}
			case 0x12: {
				RoadType rt = GetRoadTypeByLabel(std::byteswap(cond_val));
				result = rt != INVALID_ROADTYPE && RoadTypeIsTram(rt);
				break;
			}
			default: GrfMsg(1, "SkipIf: Unsupported condition type {:02X}. Ignoring", condtype); return;
		}
	} else if (param == 0x88) {
		/* GRF ID checks */

		GRFConfig *c = GetGRFConfig(cond_val, mask);

		if (c != nullptr && c->flags.Test(GRFConfigFlag::Static) && !_cur.grfconfig->flags.Test(GRFConfigFlag::Static) && _networking) {
			DisableStaticNewGRFInfluencingNonStaticNewGRFs(*c);
			c = nullptr;
		}

		if (condtype != 10 && c == nullptr) {
			GrfMsg(7, "SkipIf: GRFID 0x{:08X} unknown, skipping test", std::byteswap(cond_val));
			return;
		}

		switch (condtype) {
			/* Tests 0x06 to 0x0A are only for param 0x88, GRFID checks */
			case 0x06: // Is GRFID active?
				result = c->status == GCS_ACTIVATED;
				break;

			case 0x07: // Is GRFID non-active?
				result = c->status != GCS_ACTIVATED;
				break;

			case 0x08: // GRFID is not but will be active?
				result = c->status == GCS_INITIALISED;
				break;

			case 0x09: // GRFID is or will be active?
				result = c->status == GCS_ACTIVATED || c->status == GCS_INITIALISED;
				break;

			case 0x0A: // GRFID is not nor will be active
				/* This is the only condtype that doesn't get ignored if the GRFID is not found */
				result = c == nullptr || c->status == GCS_DISABLED || c->status == GCS_NOT_FOUND;
				break;

			default: GrfMsg(1, "SkipIf: Unsupported GRF condition type {:02X}. Ignoring", condtype); return;
		}
	} else {
		/* Tests that use 'param' and are not GRF ID checks.  */
		uint32_t param_val = GetParamVal(param, &cond_val); // cond_val is modified for param == 0x85
		switch (condtype) {
			case 0x00: result = !!(param_val & (1 << cond_val));
				break;
			case 0x01: result = !(param_val & (1 << cond_val));
				break;
			case 0x02: result = (param_val & mask) == cond_val;
				break;
			case 0x03: result = (param_val & mask) != cond_val;
				break;
			case 0x04: result = (param_val & mask) < cond_val;
				break;
			case 0x05: result = (param_val & mask) > cond_val;
				break;
			default: GrfMsg(1, "SkipIf: Unsupported condition type {:02X}. Ignoring", condtype); return;
		}
	}

	if (!result) {
		GrfMsg(2, "SkipIf: Not skipping sprites, test was false");
		return;
	}

	uint8_t numsprites = buf.ReadByte();

	/* numsprites can be a GOTO label if it has been defined in the GRF
	 * file. The jump will always be the first matching label that follows
	 * the current nfo_line. If no matching label is found, the first matching
	 * label in the file is used. */
	const GRFLabel *choice = nullptr;
	for (const auto &label : _cur.grffile->labels) {
		if (label.label != numsprites) continue;

		/* Remember a goto before the current line */
		if (choice == nullptr) choice = &label;
		/* If we find a label here, this is definitely good */
		if (label.nfo_line > _cur.nfo_line) {
			choice = &label;
			break;
		}
	}

	if (choice != nullptr) {
		GrfMsg(2, "SkipIf: Jumping to label 0x{:X} at line {}, test was true", choice->label, choice->nfo_line);
		_cur.file->SeekTo(choice->pos, SEEK_SET);
		_cur.nfo_line = choice->nfo_line;
		return;
	}

	GrfMsg(2, "SkipIf: Skipping {} sprites, test was true", numsprites);
	_cur.skip_sprites = numsprites;
	if (_cur.skip_sprites == 0) {
		/* Zero means there are no sprites to skip, so
		 * we use -1 to indicate that all further
		 * sprites should be skipped. */
		_cur.skip_sprites = -1;

		/* If an action 8 hasn't been encountered yet, disable the grf. */
		if (_cur.grfconfig->status != (_cur.stage < GLS_RESERVE ? GCS_INITIALISED : GCS_ACTIVATED)) {
			DisableGrf();
		}
	}
}

template <> void GrfActionHandler<0x07>::FileScan(ByteReader &) { }
template <> void GrfActionHandler<0x07>::SafetyScan(ByteReader &) { }
template <> void GrfActionHandler<0x07>::LabelScan(ByteReader &) { }
template <> void GrfActionHandler<0x07>::Init(ByteReader &) { }
template <> void GrfActionHandler<0x07>::Reserve(ByteReader &buf) { SkipIf(buf); }
template <> void GrfActionHandler<0x07>::Activation(ByteReader &buf) { SkipIf(buf); }

template <> void GrfActionHandler<0x09>::FileScan(ByteReader &) { }
template <> void GrfActionHandler<0x09>::SafetyScan(ByteReader &) { }
template <> void GrfActionHandler<0x09>::LabelScan(ByteReader &) { }
template <> void GrfActionHandler<0x09>::Init(ByteReader &buf) { SkipIf(buf); }
template <> void GrfActionHandler<0x09>::Reserve(ByteReader &buf) { SkipIf(buf); }
template <> void GrfActionHandler<0x09>::Activation(ByteReader &buf) { SkipIf(buf); }
