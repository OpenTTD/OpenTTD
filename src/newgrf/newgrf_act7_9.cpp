/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_act7_9.cpp NewGRF Action 0x07 and Action 0x09 handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "../genworld.h"
#include "../network/network.h"
#include "../newgrf_engine.h"
#include "../newgrf_cargo.h"
#include "../rail.h"
#include "../road.h"
#include "../settings_type.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

/** 32 * 8 = 256 flags. Apparently TTDPatch uses this many.. */
static std::array<uint32_t, 8> _ttdpatch_flags;

/** Initialize the TTDPatch flags */
void InitializePatchFlags()
{
	_ttdpatch_flags[0] = ((_settings_game.station.never_expire_airports ? 1U : 0U) << 0x0C)  // keepsmallairport
	                   |                                                       (1U << 0x0D)  // newairports
	                   |                                                       (1U << 0x0E)  // largestations
	                   | ((_settings_game.construction.max_bridge_length > 16 ? 1U : 0U) << 0x0F)  // longbridges
	                   |                                                       (0U << 0x10)  // loadtime
	                   |                                                       (1U << 0x12)  // presignals
	                   |                                                       (1U << 0x13)  // extpresignals
	                   | ((_settings_game.vehicle.never_expire_vehicles ? 1U : 0U) << 0x16)  // enginespersist
	                   |                                                       (1U << 0x1B)  // multihead
	                   |                                                       (1U << 0x1D)  // lowmemory
	                   |                                                       (1U << 0x1E); // generalfixes

	_ttdpatch_flags[1] =   ((_settings_game.economy.station_noise_level ? 1U : 0U) << 0x07)  // moreairports - based on units of noise
	                   |                                                       (1U << 0x08)  // mammothtrains
	                   |                                                       (1U << 0x09)  // trainrefit
	                   |                                                       (0U << 0x0B)  // subsidiaries
	                   |         ((_settings_game.order.gradual_loading ? 1U : 0U) << 0x0C)  // gradualloading
	                   |                                                       (1U << 0x12)  // unifiedmaglevmode - set bit 0 mode. Not revelant to OTTD
	                   |                                                       (1U << 0x13)  // unifiedmaglevmode - set bit 1 mode
	                   |                                                       (1U << 0x14)  // bridgespeedlimits
	                   |                                                       (1U << 0x16)  // eternalgame
	                   |                                                       (1U << 0x17)  // newtrains
	                   |                                                       (1U << 0x18)  // newrvs
	                   |                                                       (1U << 0x19)  // newships
	                   |                                                       (1U << 0x1A)  // newplanes
	                   | ((_settings_game.construction.train_signal_side == 1 ? 1U : 0U) << 0x1B)  // signalsontrafficside
	                   |       ((_settings_game.vehicle.disable_elrails ? 0U : 1U) << 0x1C); // electrifiedrailway

	_ttdpatch_flags[2] =                                                       (1U << 0x01)  // loadallgraphics - obsolote
	                   |                                                       (1U << 0x03)  // semaphores
	                   |                                                       (1U << 0x0A)  // newobjects
	                   |                                                       (0U << 0x0B)  // enhancedgui
	                   |                                                       (0U << 0x0C)  // newagerating
	                   |  ((_settings_game.construction.build_on_slopes ? 1U : 0U) << 0x0D)  // buildonslopes
	                   |                                                       (1U << 0x0E)  // fullloadany
	                   |                                                       (1U << 0x0F)  // planespeed
	                   |                                                       (0U << 0x10)  // moreindustriesperclimate - obsolete
	                   |                                                       (0U << 0x11)  // moretoylandfeatures
	                   |                                                       (1U << 0x12)  // newstations
	                   |                                                       (1U << 0x13)  // tracktypecostdiff
	                   |                                                       (1U << 0x14)  // manualconvert
	                   |  ((_settings_game.construction.build_on_slopes ? 1U : 0U) << 0x15)  // buildoncoasts
	                   |                                                       (1U << 0x16)  // canals
	                   |                                                       (1U << 0x17)  // newstartyear
	                   |    ((_settings_game.vehicle.freight_trains > 1 ? 1U : 0U) << 0x18)  // freighttrains
	                   |                                                       (1U << 0x19)  // newhouses
	                   |                                                       (1U << 0x1A)  // newbridges
	                   |                                                       (1U << 0x1B)  // newtownnames
	                   |                                                       (1U << 0x1C)  // moreanimation
	                   |    ((_settings_game.vehicle.wagon_speed_limits ? 1U : 0U) << 0x1D)  // wagonspeedlimits
	                   |                                                       (1U << 0x1E)  // newshistory
	                   |                                                       (0U << 0x1F); // custombridgeheads

	_ttdpatch_flags[3] =                                                       (0U << 0x00)  // newcargodistribution
	                   |                                                       (1U << 0x01)  // windowsnap
	                   | ((_settings_game.economy.allow_town_roads || _generating_world ? 0U : 1U) << 0x02)  // townbuildnoroad
	                   |                                                       (1U << 0x03)  // pathbasedsignalling
	                   |                                                       (0U << 0x04)  // aichoosechance
	                   |                                                       (1U << 0x05)  // resolutionwidth
	                   |                                                       (1U << 0x06)  // resolutionheight
	                   |                                                       (1U << 0x07)  // newindustries
	                   |           ((_settings_game.order.improved_load ? 1U : 0U) << 0x08)  // fifoloading
	                   |                                                       (0U << 0x09)  // townroadbranchprob
	                   |                                                       (0U << 0x0A)  // tempsnowline
	                   |                                                       (1U << 0x0B)  // newcargo
	                   |                                                       (1U << 0x0C)  // enhancemultiplayer
	                   |                                                       (1U << 0x0D)  // onewayroads
	                   |                                                       (1U << 0x0E)  // irregularstations
	                   |                                                       (1U << 0x0F)  // statistics
	                   |                                                       (1U << 0x10)  // newsounds
	                   |                                                       (1U << 0x11)  // autoreplace
	                   |                                                       (1U << 0x12)  // autoslope
	                   |                                                       (0U << 0x13)  // followvehicle
	                   |                                                       (1U << 0x14)  // trams
	                   |                                                       (0U << 0x15)  // enhancetunnels
	                   |                                                       (1U << 0x16)  // shortrvs
	                   |                                                       (1U << 0x17)  // articulatedrvs
	                   |       ((_settings_game.vehicle.dynamic_engines ? 1U : 0U) << 0x18)  // dynamic engines
	                   |                                                       (1U << 0x1E)  // variablerunningcosts
	                   |                                                       (1U << 0x1F); // any switch is on

	_ttdpatch_flags[4] =                                                       (1U << 0x00)  // larger persistent storage
	                   |             ((_settings_game.economy.inflation ? 1U : 0U) << 0x01)  // inflation is on
	                   |                                                       (1U << 0x02); // extended string range
}

uint32_t GetParamVal(uint8_t param, uint32_t *cond_val)
{
	/* First handle variable common with VarAction2 */
	uint32_t value;
	if (GetGlobalVariable(param - 0x80, &value, _cur_gps.grffile)) return value;


	/* Non-common variable */
	switch (param) {
		case 0x84: { // GRF loading stage
			uint32_t res = 0;

			if (_cur_gps.stage > GLS_INIT) SetBit(res, 0);
			if (_cur_gps.stage == GLS_RESERVE) SetBit(res, 8);
			if (_cur_gps.stage == GLS_ACTIVATION) SetBit(res, 9);
			return res;
		}

		case 0x85: // TTDPatch flags, only for bit tests
			if (cond_val == nullptr) {
				/* Supported in Action 0x07 and 0x09, not 0x0D */
				return 0;
			} else {
				uint32_t index = *cond_val / 0x20;
				uint32_t param_val = index < std::size(_ttdpatch_flags) ? _ttdpatch_flags[index] : 0;
				*cond_val %= 0x20;
				return param_val;
			}

		case 0x88: // GRF ID check
			return 0;

		/* case 0x99: Global ID offset not implemented */

		default:
			/* GRF Parameter */
			if (param < 0x80) return _cur_gps.grffile->GetParam(param);

			/* In-game variable. */
			GrfMsg(1, "Unsupported in-game variable 0x{:02X}", param);
			return UINT_MAX;
	}
}

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

	if (param < 0x80 && std::size(_cur_gps.grffile->param) <= param) {
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

		if (c != nullptr && c->flags.Test(GRFConfigFlag::Static) && !_cur_gps.grfconfig->flags.Test(GRFConfigFlag::Static) && _networking) {
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
	for (const auto &label : _cur_gps.grffile->labels) {
		if (label.label != numsprites) continue;

		/* Remember a goto before the current line */
		if (choice == nullptr) choice = &label;
		/* If we find a label here, this is definitely good */
		if (label.nfo_line > _cur_gps.nfo_line) {
			choice = &label;
			break;
		}
	}

	if (choice != nullptr) {
		GrfMsg(2, "SkipIf: Jumping to label 0x{:X} at line {}, test was true", choice->label, choice->nfo_line);
		_cur_gps.file->SeekTo(choice->pos, SEEK_SET);
		_cur_gps.nfo_line = choice->nfo_line;
		return;
	}

	GrfMsg(2, "SkipIf: Skipping {} sprites, test was true", numsprites);
	_cur_gps.skip_sprites = numsprites;
	if (_cur_gps.skip_sprites == 0) {
		/* Zero means there are no sprites to skip, so
		 * we use -1 to indicate that all further
		 * sprites should be skipped. */
		_cur_gps.skip_sprites = -1;

		/* If an action 8 hasn't been encountered yet, disable the grf. */
		if (_cur_gps.grfconfig->status != (_cur_gps.stage < GLS_RESERVE ? GCS_INITIALISED : GCS_ACTIVATED)) {
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
