/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act0_trains.cpp NewGRF Action 0x00 handler for trains. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_cargo.h"
#include "../newgrf_engine.h"
#include "../vehicle_base.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal_vehicle.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

/**
 * Define properties for rail vehicles
 * @param first Local ID of the first vehicle.
 * @param last Local ID of the last vehicle.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
ChangeInfoResult RailVehicleChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	for (uint id = first; id < last; ++id) {
		Engine *e = GetNewEngine(_cur_gps.grffile, VEH_TRAIN, id);
		if (e == nullptr) return CIR_INVALID_ID; // No engine could be allocated, so neither can any next vehicles

		EngineInfo *ei = &e->info;
		RailVehicleInfo *rvi = &e->VehInfo<RailVehicleInfo>();

		switch (prop) {
			case 0x05: { // Track type
				uint8_t tracktype = buf.ReadByte();

				_gted[e->index].railtypelabels.clear();
				if (tracktype < _cur_gps.grffile->railtype_list.size()) {
					_gted[e->index].railtypelabels.push_back(_cur_gps.grffile->railtype_list[tracktype]);
					break;
				}

				switch (tracktype) {
					case 0: _gted[e->index].railtypelabels.push_back(rvi->engclass >= 2 ? RAILTYPE_LABEL_ELECTRIC : RAILTYPE_LABEL_RAIL); break;
					case 1: _gted[e->index].railtypelabels.push_back(RAILTYPE_LABEL_MONO); break;
					case 2: _gted[e->index].railtypelabels.push_back(RAILTYPE_LABEL_MAGLEV); break;
					default:
						GrfMsg(1, "RailVehicleChangeInfo: Invalid track type {} specified, ignoring", tracktype);
						break;
				}
				break;
			}

			case 0x08: // AI passenger service
				/* Tells the AI that this engine is designed for
				 * passenger services and shouldn't be used for freight. */
				rvi->ai_passenger_only = buf.ReadByte();
				break;

			case PROP_TRAIN_SPEED: { // 0x09 Speed (1 unit is 1 km-ish/h)
				uint16_t speed = buf.ReadWord();
				if (speed == 0xFFFF) speed = 0;

				rvi->max_speed = speed;
				break;
			}

			case PROP_TRAIN_POWER: // 0x0B Power
				rvi->power = buf.ReadWord();

				/* Set engine / wagon state based on power */
				if (rvi->power != 0) {
					if (rvi->railveh_type == RAILVEH_WAGON) {
						rvi->railveh_type = RAILVEH_SINGLEHEAD;
					}
				} else {
					rvi->railveh_type = RAILVEH_WAGON;
				}
				break;

			case PROP_TRAIN_RUNNING_COST_FACTOR: // 0x0D Running cost factor
				rvi->running_cost = buf.ReadByte();
				break;

			case 0x0E: // Running cost base
				ConvertTTDBasePrice(buf.ReadDWord(), "RailVehicleChangeInfo", &rvi->running_cost_class);
				break;

			case 0x12: { // Sprite ID
				uint8_t spriteid = buf.ReadByte();
				uint8_t orig_spriteid = spriteid;

				/* TTD sprite IDs point to a location in a 16bit array, but we use it
				 * as an array index, so we need it to be half the original value. */
				if (spriteid < CUSTOM_VEHICLE_SPRITENUM) spriteid >>= 1;

				if (IsValidNewGRFImageIndex<VEH_TRAIN>(spriteid)) {
					rvi->image_index = spriteid;
				} else {
					GrfMsg(1, "RailVehicleChangeInfo: Invalid Sprite {} specified, ignoring", orig_spriteid);
					rvi->image_index = 0;
				}
				break;
			}

			case 0x13: { // Dual-headed
				uint8_t dual = buf.ReadByte();

				if (dual != 0) {
					rvi->railveh_type = RAILVEH_MULTIHEAD;
				} else {
					rvi->railveh_type = rvi->power == 0 ?
						RAILVEH_WAGON : RAILVEH_SINGLEHEAD;
				}
				break;
			}

			case PROP_TRAIN_CARGO_CAPACITY: // 0x14 Cargo capacity
				rvi->capacity = buf.ReadByte();
				break;

			case 0x15: { // Cargo type
				_gted[e->index].defaultcargo_grf = _cur_gps.grffile;
				uint8_t ctype = buf.ReadByte();

				if (ctype == 0xFF) {
					/* 0xFF is specified as 'use first refittable' */
					ei->cargo_type = INVALID_CARGO;
				} else {
					/* Use translated cargo. Might result in INVALID_CARGO (first refittable), if cargo is not defined. */
					ei->cargo_type = GetCargoTranslation(ctype, _cur_gps.grffile);
					if (ei->cargo_type == INVALID_CARGO) GrfMsg(2, "RailVehicleChangeInfo: Invalid cargo type {}, using first refittable", ctype);
				}
				ei->cargo_label = CT_INVALID;
				break;
			}

			case PROP_TRAIN_WEIGHT: // 0x16 Weight
				SB(rvi->weight, 0, 8, buf.ReadByte());
				break;

			case PROP_TRAIN_COST_FACTOR: // 0x17 Cost factor
				rvi->cost_factor = buf.ReadByte();
				break;

			case 0x18: // AI rank
				GrfMsg(2, "RailVehicleChangeInfo: Property 0x18 'AI rank' not used by NoAI, ignored.");
				buf.ReadByte();
				break;

			case 0x19: { // Engine traction type
				/* What do the individual numbers mean?
				 * 0x00 .. 0x07: Steam
				 * 0x08 .. 0x27: Diesel
				 * 0x28 .. 0x31: Electric
				 * 0x32 .. 0x37: Monorail
				 * 0x38 .. 0x41: Maglev
				 */
				uint8_t traction = buf.ReadByte();
				EngineClass engclass;

				if (traction <= 0x07) {
					engclass = EC_STEAM;
				} else if (traction <= 0x27) {
					engclass = EC_DIESEL;
				} else if (traction <= 0x31) {
					engclass = EC_ELECTRIC;
				} else if (traction <= 0x37) {
					engclass = EC_MONORAIL;
				} else if (traction <= 0x41) {
					engclass = EC_MAGLEV;
				} else {
					break;
				}

				if (_cur_gps.grffile->railtype_list.empty() && !_gted[e->index].railtypelabels.empty()) {
					/* Use traction type to select between normal and electrified
					 * rail only when no translation list is in place. */
					if (_gted[e->index].railtypelabels[0] == RAILTYPE_LABEL_RAIL && engclass >= EC_ELECTRIC) _gted[e->index].railtypelabels[0] = RAILTYPE_LABEL_ELECTRIC;
					if (_gted[e->index].railtypelabels[0] == RAILTYPE_LABEL_ELECTRIC && engclass < EC_ELECTRIC) _gted[e->index].railtypelabels[0] = RAILTYPE_LABEL_RAIL;
				}

				rvi->engclass = engclass;
				break;
			}

			case 0x1A: // Alter purchase list sort order
				AlterVehicleListOrder(e->index, buf.ReadExtendedByte());
				break;

			case 0x1B: // Powered wagons power bonus
				rvi->pow_wag_power = buf.ReadWord();
				break;

			case 0x1C: // Refit cost
				ei->refit_cost = buf.ReadByte();
				break;

			case 0x1D: { // Refit cargo
				uint32_t mask = buf.ReadDWord();
				_gted[e->index].UpdateRefittability(mask != 0);
				ei->refit_mask = TranslateRefitMask(mask);
				_gted[e->index].defaultcargo_grf = _cur_gps.grffile;
				break;
			}

			case 0x1E: { // Callback
				auto mask = ei->callback_mask.base();
				SB(mask, 0, 8, buf.ReadByte());
				ei->callback_mask = VehicleCallbackMasks{mask};
				break;
			}

			case PROP_TRAIN_TRACTIVE_EFFORT: // 0x1F Tractive effort coefficient
				rvi->tractive_effort = buf.ReadByte();
				break;

			case 0x20: // Air drag
				rvi->air_drag = buf.ReadByte();
				break;

			case PROP_TRAIN_SHORTEN_FACTOR: // 0x21 Shorter vehicle
				rvi->shorten_factor = buf.ReadByte();
				break;

			case 0x22: // Visual effect
				rvi->visual_effect = buf.ReadByte();
				/* Avoid accidentally setting visual_effect to the default value
				 * Since bit 6 (disable effects) is set anyways, we can safely erase some bits. */
				if (rvi->visual_effect == VE_DEFAULT) {
					assert(HasBit(rvi->visual_effect, VE_DISABLE_EFFECT));
					SB(rvi->visual_effect, VE_TYPE_START, VE_TYPE_COUNT, 0);
				}
				break;

			case 0x23: // Powered wagons weight bonus
				rvi->pow_wag_weight = buf.ReadByte();
				break;

			case 0x24: { // High byte of vehicle weight
				uint8_t weight = buf.ReadByte();

				if (weight > 4) {
					GrfMsg(2, "RailVehicleChangeInfo: Nonsensical weight of {} tons, ignoring", weight << 8);
				} else {
					SB(rvi->weight, 8, 8, weight);
				}
				break;
			}

			case PROP_TRAIN_USER_DATA: // 0x25 User-defined bit mask to set when checking veh. var. 42
				rvi->user_def_data = buf.ReadByte();
				break;

			case 0x26: // Retire vehicle early
				ei->retire_early = buf.ReadByte();
				break;

			case 0x27: // Miscellaneous flags
				ei->misc_flags = static_cast<EngineMiscFlags>(buf.ReadByte());
				_loaded_newgrf_features.has_2CC |= ei->misc_flags.Test(EngineMiscFlag::Uses2CC);
				break;

			case 0x28: // Cargo classes allowed
				_gted[e->index].cargo_allowed = CargoClasses{buf.ReadWord()};
				_gted[e->index].UpdateRefittability(_gted[e->index].cargo_allowed.Any());
				_gted[e->index].defaultcargo_grf = _cur_gps.grffile;
				break;

			case 0x29: // Cargo classes disallowed
				_gted[e->index].cargo_disallowed = CargoClasses{buf.ReadWord()};
				_gted[e->index].UpdateRefittability(false);
				break;

			case 0x2A: // Long format introduction date (days since year 0)
				ei->base_intro = TimerGameCalendar::Date(buf.ReadDWord());
				break;

			case PROP_TRAIN_CARGO_AGE_PERIOD: // 0x2B Cargo aging period
				ei->cargo_age_period = buf.ReadWord();
				break;

			case 0x2C:   // CTT refit include list
			case 0x2D: { // CTT refit exclude list
				uint8_t count = buf.ReadByte();
				_gted[e->index].UpdateRefittability(prop == 0x2C && count != 0);
				if (prop == 0x2C) _gted[e->index].defaultcargo_grf = _cur_gps.grffile;
				CargoTypes &ctt = prop == 0x2C ? _gted[e->index].ctt_include_mask : _gted[e->index].ctt_exclude_mask;
				ctt = 0;
				while (count--) {
					CargoType ctype = GetCargoTranslation(buf.ReadByte(), _cur_gps.grffile);
					if (IsValidCargoType(ctype)) SetBit(ctt, ctype);
				}
				break;
			}

			case PROP_TRAIN_CURVE_SPEED_MOD: // 0x2E Curve speed modifier
				rvi->curve_speed_mod = buf.ReadWord();
				break;

			case 0x2F: // Engine variant
				ei->variant_id = static_cast<EngineID>(buf.ReadWord());
				break;

			case 0x30: // Extra miscellaneous flags
				ei->extra_flags = static_cast<ExtraEngineFlags>(buf.ReadDWord());
				break;

			case 0x31: { // Callback additional mask
				auto mask = ei->callback_mask.base();
				SB(mask, 8, 8, buf.ReadByte());
				ei->callback_mask = VehicleCallbackMasks{mask};
				break;
			}

			case 0x32: // Cargo classes required for a refit.
				_gted[e->index].cargo_allowed_required = CargoClasses{buf.ReadWord()};
				break;

			case 0x33: // Badge list
				e->badges = ReadBadgeList(buf, GSF_TRAINS);
				break;

			case 0x34: { // List of track types
				uint8_t count = buf.ReadByte();

				_gted[e->index].railtypelabels.clear();
				while (count--) {
					uint8_t tracktype = buf.ReadByte();

					if (tracktype < _cur_gps.grffile->railtype_list.size()) {
						_gted[e->index].railtypelabels.push_back(_cur_gps.grffile->railtype_list[tracktype]);
					} else {
						GrfMsg(1, "RailVehicleChangeInfo: Invalid track type {} specified, ignoring", tracktype);
					}
				}
				break;
			}

			default:
				ret = CommonVehicleChangeInfo(ei, prop, buf);
				break;
		}
	}

	return ret;
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_TRAINS>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_TRAINS>::Activation(uint first, uint last, int prop, ByteReader &buf) { return RailVehicleChangeInfo(first, last, prop, buf); }
