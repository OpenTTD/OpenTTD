/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_act0_roadvehs.cpp NewGRF Action 0x00 handler for road vehicles. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_cargo.h"
#include "../newgrf_engine.h"
#include "../newgrf_sound.h"
#include "../vehicle_base.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal_vehicle.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

/**
 * Define properties for road vehicles
 * @param first Local ID of the first vehicle.
 * @param last Local ID of the last vehicle.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult RoadVehicleChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	for (uint id = first; id < last; ++id) {
		Engine *e = GetNewEngine(_cur_gps.grffile, VEH_ROAD, id);
		if (e == nullptr) return CIR_INVALID_ID; // No engine could be allocated, so neither can any next vehicles

		EngineInfo *ei = &e->info;
		RoadVehicleInfo *rvi = &e->u.road;

		switch (prop) {
			case 0x05: // Road/tram type
				/* RoadTypeLabel is looked up later after the engine's road/tram
				 * flag is set, however 0 means the value has not been set. */
				_gted[e->index].roadtramtype = buf.ReadByte() + 1;
				break;

			case 0x08: // Speed (1 unit is 0.5 kmh)
				rvi->max_speed = buf.ReadByte();
				break;

			case PROP_ROADVEH_RUNNING_COST_FACTOR: // 0x09 Running cost factor
				rvi->running_cost = buf.ReadByte();
				break;

			case 0x0A: // Running cost base
				ConvertTTDBasePrice(buf.ReadDWord(), "RoadVehicleChangeInfo", &rvi->running_cost_class);
				break;

			case 0x0E: { // Sprite ID
				uint8_t spriteid = buf.ReadByte();
				uint8_t orig_spriteid = spriteid;

				/* cars have different custom id in the GRF file */
				if (spriteid == 0xFF) spriteid = 0xFD;

				if (spriteid < 0xFD) spriteid >>= 1;

				if (IsValidNewGRFImageIndex<VEH_ROAD>(spriteid)) {
					rvi->image_index = spriteid;
				} else {
					GrfMsg(1, "RoadVehicleChangeInfo: Invalid Sprite {} specified, ignoring", orig_spriteid);
					rvi->image_index = 0;
				}
				break;
			}

			case PROP_ROADVEH_CARGO_CAPACITY: // 0x0F Cargo capacity
				rvi->capacity = buf.ReadByte();
				break;

			case 0x10: { // Cargo type
				_gted[e->index].defaultcargo_grf = _cur_gps.grffile;
				uint8_t ctype = buf.ReadByte();

				if (ctype == 0xFF) {
					/* 0xFF is specified as 'use first refittable' */
					ei->cargo_type = INVALID_CARGO;
				} else {
					/* Use translated cargo. Might result in INVALID_CARGO (first refittable), if cargo is not defined. */
					ei->cargo_type = GetCargoTranslation(ctype, _cur_gps.grffile);
					if (ei->cargo_type == INVALID_CARGO) GrfMsg(2, "RoadVehicleChangeInfo: Invalid cargo type {}, using first refittable", ctype);
				}
				ei->cargo_label = CT_INVALID;
				break;
			}

			case PROP_ROADVEH_COST_FACTOR: // 0x11 Cost factor
				rvi->cost_factor = buf.ReadByte();
				break;

			case 0x12: // SFX
				rvi->sfx = GetNewGRFSoundID(_cur_gps.grffile, buf.ReadByte());
				break;

			case PROP_ROADVEH_POWER: // Power in units of 10 HP.
				rvi->power = buf.ReadByte();
				break;

			case PROP_ROADVEH_WEIGHT: // Weight in units of 1/4 tons.
				rvi->weight = buf.ReadByte();
				break;

			case PROP_ROADVEH_SPEED: // Speed in mph/0.8
				_gted[e->index].rv_max_speed = buf.ReadByte();
				break;

			case 0x16: { // Cargoes available for refitting
				uint32_t mask = buf.ReadDWord();
				_gted[e->index].UpdateRefittability(mask != 0);
				ei->refit_mask = TranslateRefitMask(mask);
				_gted[e->index].defaultcargo_grf = _cur_gps.grffile;
				break;
			}

			case 0x17: { // Callback mask
				auto mask = ei->callback_mask.base();
				SB(mask, 0, 8, buf.ReadByte());
				ei->callback_mask = VehicleCallbackMasks{mask};
				break;
			}

			case PROP_ROADVEH_TRACTIVE_EFFORT: // Tractive effort coefficient in 1/256.
				rvi->tractive_effort = buf.ReadByte();
				break;

			case 0x19: // Air drag
				rvi->air_drag = buf.ReadByte();
				break;

			case 0x1A: // Refit cost
				ei->refit_cost = buf.ReadByte();
				break;

			case 0x1B: // Retire vehicle early
				ei->retire_early = buf.ReadByte();
				break;

			case 0x1C: // Miscellaneous flags
				ei->misc_flags = static_cast<EngineMiscFlags>(buf.ReadByte());
				_loaded_newgrf_features.has_2CC |= ei->misc_flags.Test(EngineMiscFlag::Uses2CC);
				break;

			case 0x1D: // Cargo classes allowed
				_gted[e->index].cargo_allowed = CargoClasses{buf.ReadWord()};
				_gted[e->index].UpdateRefittability(_gted[e->index].cargo_allowed.Any());
				_gted[e->index].defaultcargo_grf = _cur_gps.grffile;
				break;

			case 0x1E: // Cargo classes disallowed
				_gted[e->index].cargo_disallowed = CargoClasses{buf.ReadWord()};
				_gted[e->index].UpdateRefittability(false);
				break;

			case 0x1F: // Long format introduction date (days since year 0)
				ei->base_intro = TimerGameCalendar::Date(buf.ReadDWord());
				break;

			case 0x20: // Alter purchase list sort order
				AlterVehicleListOrder(e->index, buf.ReadExtendedByte());
				break;

			case 0x21: // Visual effect
				rvi->visual_effect = buf.ReadByte();
				/* Avoid accidentally setting visual_effect to the default value
				 * Since bit 6 (disable effects) is set anyways, we can safely erase some bits. */
				if (rvi->visual_effect == VE_DEFAULT) {
					assert(HasBit(rvi->visual_effect, VE_DISABLE_EFFECT));
					SB(rvi->visual_effect, VE_TYPE_START, VE_TYPE_COUNT, 0);
				}
				break;

			case PROP_ROADVEH_CARGO_AGE_PERIOD: // 0x22 Cargo aging period
				ei->cargo_age_period = buf.ReadWord();
				break;

			case PROP_ROADVEH_SHORTEN_FACTOR: // 0x23 Shorter vehicle
				rvi->shorten_factor = buf.ReadByte();
				break;

			case 0x24:   // CTT refit include list
			case 0x25: { // CTT refit exclude list
				uint8_t count = buf.ReadByte();
				_gted[e->index].UpdateRefittability(prop == 0x24 && count != 0);
				if (prop == 0x24) _gted[e->index].defaultcargo_grf = _cur_gps.grffile;
				CargoTypes &ctt = prop == 0x24 ? _gted[e->index].ctt_include_mask : _gted[e->index].ctt_exclude_mask;
				ctt = 0;
				while (count--) {
					CargoType ctype = GetCargoTranslation(buf.ReadByte(), _cur_gps.grffile);
					if (IsValidCargoType(ctype)) SetBit(ctt, ctype);
				}
				break;
			}

			case 0x26: // Engine variant
				ei->variant_id = static_cast<EngineID>(buf.ReadWord());
				break;

			case 0x27: // Extra miscellaneous flags
				ei->extra_flags = static_cast<ExtraEngineFlags>(buf.ReadDWord());
				break;

			case 0x28: { // Callback additional mask
				auto mask = ei->callback_mask.base();
				SB(mask, 8, 8, buf.ReadByte());
				ei->callback_mask = VehicleCallbackMasks{mask};
				break;
			}

			case 0x29: // Cargo classes required for a refit.
				_gted[e->index].cargo_allowed_required = CargoClasses{buf.ReadWord()};
				break;

			case 0x2A: // Badge list
				e->badges = ReadBadgeList(buf, GSF_ROADVEHICLES);
				break;

			default:
				ret = CommonVehicleChangeInfo(ei, prop, buf);
				break;
		}
	}

	return ret;
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_ROADVEHICLES>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_ROADVEHICLES>::Activation(uint first, uint last, int prop, ByteReader &buf) { return RoadVehicleChangeInfo(first, last, prop, buf); }
