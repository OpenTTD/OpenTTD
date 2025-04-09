/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_act0_aircraft.cpp NewGRF Action 0x00 handler for aircraft. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_cargo.h"
#include "../newgrf_engine.h"
#include "../newgrf_sound.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal_vehicle.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

/**
 * Define properties for aircraft
 * @param first Local ID of the first vehicle.
 * @param last Local ID of the last vehicle.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult AircraftVehicleChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	for (uint id = first; id < last; ++id) {
		Engine *e = GetNewEngine(_cur_gps.grffile, VEH_AIRCRAFT, id);
		if (e == nullptr) return CIR_INVALID_ID; // No engine could be allocated, so neither can any next vehicles

		EngineInfo *ei = &e->info;
		AircraftVehicleInfo *avi = &e->u.air;

		switch (prop) {
			case 0x08: { // Sprite ID
				uint8_t spriteid = buf.ReadByte();
				uint8_t orig_spriteid = spriteid;

				/* aircraft have different custom id in the GRF file */
				if (spriteid == 0xFF) spriteid = 0xFD;

				if (spriteid < 0xFD) spriteid >>= 1;

				if (IsValidNewGRFImageIndex<VEH_AIRCRAFT>(spriteid)) {
					avi->image_index = spriteid;
				} else {
					GrfMsg(1, "AircraftVehicleChangeInfo: Invalid Sprite {} specified, ignoring", orig_spriteid);
					avi->image_index = 0;
				}
				break;
			}

			case 0x09: // Helicopter
				if (buf.ReadByte() == 0) {
					avi->subtype = AIR_HELI;
				} else {
					SB(avi->subtype, 0, 1, 1); // AIR_CTOL
				}
				break;

			case 0x0A: // Large
				AssignBit(avi->subtype, 1, buf.ReadByte() != 0); // AIR_FAST
				break;

			case PROP_AIRCRAFT_COST_FACTOR: // 0x0B Cost factor
				avi->cost_factor = buf.ReadByte();
				break;

			case PROP_AIRCRAFT_SPEED: // 0x0C Speed (1 unit is 8 mph, we translate to 1 unit is 1 km-ish/h)
				avi->max_speed = (buf.ReadByte() * 128) / 10;
				break;

			case 0x0D: // Acceleration
				avi->acceleration = buf.ReadByte();
				break;

			case PROP_AIRCRAFT_RUNNING_COST_FACTOR: // 0x0E Running cost factor
				avi->running_cost = buf.ReadByte();
				break;

			case PROP_AIRCRAFT_PASSENGER_CAPACITY: // 0x0F Passenger capacity
				avi->passenger_capacity = buf.ReadWord();
				break;

			case PROP_AIRCRAFT_MAIL_CAPACITY: // 0x11 Mail capacity
				avi->mail_capacity = buf.ReadByte();
				break;

			case 0x12: // SFX
				avi->sfx = GetNewGRFSoundID(_cur_gps.grffile, buf.ReadByte());
				break;

			case 0x13: { // Cargoes available for refitting
				uint32_t mask = buf.ReadDWord();
				_gted[e->index].UpdateRefittability(mask != 0);
				ei->refit_mask = TranslateRefitMask(mask);
				_gted[e->index].defaultcargo_grf = _cur_gps.grffile;
				break;
			}

			case 0x14: { // Callback mask
				auto mask = ei->callback_mask.base();
				SB(mask, 0, 8, buf.ReadByte());
				ei->callback_mask = VehicleCallbackMasks{mask};
				break;
			}

			case 0x15: // Refit cost
				ei->refit_cost = buf.ReadByte();
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

			case PROP_AIRCRAFT_CARGO_AGE_PERIOD: // 0x1C Cargo aging period
				ei->cargo_age_period = buf.ReadWord();
				break;

			case 0x1D:   // CTT refit include list
			case 0x1E: { // CTT refit exclude list
				uint8_t count = buf.ReadByte();
				_gted[e->index].UpdateRefittability(prop == 0x1D && count != 0);
				if (prop == 0x1D) _gted[e->index].defaultcargo_grf = _cur_gps.grffile;
				CargoTypes &ctt = prop == 0x1D ? _gted[e->index].ctt_include_mask : _gted[e->index].ctt_exclude_mask;
				ctt = 0;
				while (count--) {
					CargoType ctype = GetCargoTranslation(buf.ReadByte(), _cur_gps.grffile);
					if (IsValidCargoType(ctype)) SetBit(ctt, ctype);
				}
				break;
			}

			case PROP_AIRCRAFT_RANGE: // 0x1F Max aircraft range
				avi->max_range = buf.ReadWord();
				break;

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

			case 0x23: // Cargo classes required for a refit.
				_gted[e->index].cargo_allowed_required = CargoClasses{buf.ReadWord()};
				break;

			case 0x24: // Badge list
				e->badges = ReadBadgeList(buf, GSF_AIRCRAFT);
				break;

			default:
				ret = CommonVehicleChangeInfo(ei, prop, buf);
				break;
		}
	}

	return ret;
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_AIRCRAFT>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_AIRCRAFT>::Activation(uint first, uint last, int prop, ByteReader &buf) { return AircraftVehicleChangeInfo(first, last, prop, buf); }
