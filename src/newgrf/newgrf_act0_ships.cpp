/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act0_ships.cpp NewGRF Action 0x00 handler for ships. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_cargo.h"
#include "../newgrf_engine.h"
#include "../newgrf_sound.h"
#include "../vehicle_base.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal_vehicle.h"
#include "newgrf_internal.h"
#include "newgrf_stringmapping.h"

#include "../safeguards.h"

/**
 * Define properties for ships
 * @param first Local ID of the first vehicle.
 * @param last Local ID of the last vehicle.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult ShipVehicleChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	for (uint id = first; id < last; ++id) {
		Engine *e = GetNewEngine(_cur_gps.grffile, VEH_SHIP, id);
		if (e == nullptr) return CIR_INVALID_ID; // No engine could be allocated, so neither can any next vehicles

		EngineInfo *ei = &e->info;
		ShipVehicleInfo *svi = &e->VehInfo<ShipVehicleInfo>();

		switch (prop) {
			case 0x08: { // Sprite ID
				uint8_t spriteid = buf.ReadByte();
				uint8_t orig_spriteid = spriteid;

				/* ships have different custom id in the GRF file */
				if (spriteid == 0xFF) spriteid = CUSTOM_VEHICLE_SPRITENUM;

				if (spriteid < CUSTOM_VEHICLE_SPRITENUM) spriteid >>= 1;

				if (IsValidNewGRFImageIndex<VEH_SHIP>(spriteid)) {
					svi->image_index = spriteid;
				} else {
					GrfMsg(1, "ShipVehicleChangeInfo: Invalid Sprite {} specified, ignoring", orig_spriteid);
					svi->image_index = 0;
				}
				break;
			}

			case 0x09: // Refittable
				svi->old_refittable = (buf.ReadByte() != 0);
				break;

			case PROP_SHIP_COST_FACTOR: // 0x0A Cost factor
				svi->cost_factor = buf.ReadByte();
				break;

			case PROP_SHIP_SPEED: // 0x0B Speed (1 unit is 0.5 km-ish/h). Use 0x23 to achieve higher speeds.
				svi->max_speed = buf.ReadByte();
				break;

			case 0x0C: { // Cargo type
				_gted[e->index].defaultcargo_grf = _cur_gps.grffile;
				uint8_t ctype = buf.ReadByte();

				if (ctype == 0xFF) {
					/* 0xFF is specified as 'use first refittable' */
					ei->cargo_type = INVALID_CARGO;
				} else {
					/* Use translated cargo. Might result in INVALID_CARGO (first refittable), if cargo is not defined. */
					ei->cargo_type = GetCargoTranslation(ctype, _cur_gps.grffile);
					if (ei->cargo_type == INVALID_CARGO) GrfMsg(2, "ShipVehicleChangeInfo: Invalid cargo type {}, using first refittable", ctype);
				}
				ei->cargo_label = CT_INVALID;
				break;
			}

			case PROP_SHIP_CARGO_CAPACITY: // 0x0D Cargo capacity
				svi->capacity = buf.ReadWord();
				break;

			case PROP_SHIP_RUNNING_COST_FACTOR: // 0x0F Running cost factor
				svi->running_cost = buf.ReadByte();
				break;

			case 0x10: // SFX
				svi->sfx = GetNewGRFSoundID(_cur_gps.grffile, buf.ReadByte());
				break;

			case 0x11: { // Cargoes available for refitting
				uint32_t mask = buf.ReadDWord();
				_gted[e->index].UpdateRefittability(mask != 0);
				ei->refit_mask = TranslateRefitMask(mask);
				_gted[e->index].defaultcargo_grf = _cur_gps.grffile;
				break;
			}

			case 0x12: { // Callback mask
				auto mask = ei->callback_mask.base();
				SB(mask, 0, 8, buf.ReadByte());
				ei->callback_mask = VehicleCallbackMasks{mask};
				break;
			}

			case 0x13: // Refit cost
				ei->refit_cost = buf.ReadByte();
				break;

			case 0x14: // Ocean speed fraction
				svi->ocean_speed_frac = buf.ReadByte();
				break;

			case 0x15: // Canal speed fraction
				svi->canal_speed_frac = buf.ReadByte();
				break;

			case 0x16: // Retire vehicle early
				ei->retire_early = buf.ReadByte();
				break;

			case 0x17: // Miscellaneous flags
				ei->misc_flags = static_cast<EngineMiscFlags>(buf.ReadByte());
				_loaded_newgrf_features.has_2CC |= ei->misc_flags.Test(EngineMiscFlag::Uses2CC);
				break;

			case 0x18: // Cargo classes allowed
				_gted[e->index].cargo_allowed = CargoClasses{buf.ReadWord()};
				_gted[e->index].UpdateRefittability(_gted[e->index].cargo_allowed.Any());
				_gted[e->index].defaultcargo_grf = _cur_gps.grffile;
				break;

			case 0x19: // Cargo classes disallowed
				_gted[e->index].cargo_disallowed = CargoClasses{buf.ReadWord()};
				_gted[e->index].UpdateRefittability(false);
				break;

			case 0x1A: // Long format introduction date (days since year 0)
				ei->base_intro = TimerGameCalendar::Date(buf.ReadDWord());
				break;

			case 0x1B: // Alter purchase list sort order
				AlterVehicleListOrder(e->index, buf.ReadExtendedByte());
				break;

			case 0x1C: // Visual effect
				svi->visual_effect = buf.ReadByte();
				/* Avoid accidentally setting visual_effect to the default value
				 * Since bit 6 (disable effects) is set anyways, we can safely erase some bits. */
				if (svi->visual_effect == VE_DEFAULT) {
					assert(HasBit(svi->visual_effect, VE_DISABLE_EFFECT));
					SB(svi->visual_effect, VE_TYPE_START, VE_TYPE_COUNT, 0);
				}
				break;

			case PROP_SHIP_CARGO_AGE_PERIOD: // 0x1D Cargo aging period
				ei->cargo_age_period = buf.ReadWord();
				break;

			case 0x1E:   // CTT refit include list
			case 0x1F: { // CTT refit exclude list
				uint8_t count = buf.ReadByte();
				_gted[e->index].UpdateRefittability(prop == 0x1E && count != 0);
				if (prop == 0x1E) _gted[e->index].defaultcargo_grf = _cur_gps.grffile;
				CargoTypes &ctt = prop == 0x1E ? _gted[e->index].ctt_include_mask : _gted[e->index].ctt_exclude_mask;
				ctt = 0;
				while (count--) {
					CargoType ctype = GetCargoTranslation(buf.ReadByte(), _cur_gps.grffile);
					if (IsValidCargoType(ctype)) SetBit(ctt, ctype);
				}
				break;
			}

			case 0x20: // Engine variant
				ei->variant_id = static_cast<EngineID>(buf.ReadWord());
				break;

			case 0x21: // Extra miscellaneous flags
				ei->extra_flags = static_cast<ExtraEngineFlags>(buf.ReadDWord());
				break;

			case 0x22: { // Callback additional mask
				auto mask = ei->callback_mask.base();
				SB(mask, 8, 8, buf.ReadByte());
				ei->callback_mask = VehicleCallbackMasks{mask};
				break;
			}

			case 0x23: // Speed (1 unit is 0.5 km-ish/h)
				svi->max_speed = buf.ReadWord();
				break;

			case 0x24: // Acceleration (1 unit is 0.5 km-ish/h per tick)
				svi->acceleration = std::max<uint8_t>(1, buf.ReadByte());
				break;

			case 0x25: // Cargo classes required for a refit.
				_gted[e->index].cargo_allowed_required = CargoClasses{buf.ReadWord()};
				break;

			case 0x26: // Badge list
				e->badges = ReadBadgeList(buf, GSF_SHIPS);
				break;

			default:
				ret = CommonVehicleChangeInfo(ei, prop, buf);
				break;
		}
	}

	return ret;
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_SHIPS>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_SHIPS>::Activation(uint first, uint last, int prop, ByteReader &buf) { return ShipVehicleChangeInfo(first, last, prop, buf); }
