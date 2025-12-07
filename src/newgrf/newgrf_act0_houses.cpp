/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act0_houses.cpp NewGRF Action 0x00 handler for houses. */

#include "../stdafx.h"
#include "../debug.h"
#include "../town.h"
#include "../newgrf_cargo.h"
#include "../newgrf_house.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"
#include "newgrf_stringmapping.h"

#include "table/strings.h"

#include "../safeguards.h"

/**
 * Ignore a house property
 * @param prop Property to read.
 * @param buf Property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult IgnoreTownHouseProperty(int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	switch (prop) {
		case 0x09:
		case 0x0B:
		case 0x0C:
		case 0x0D:
		case 0x0E:
		case 0x0F:
		case 0x11:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x18:
		case 0x19:
		case 0x1A:
		case 0x1B:
		case 0x1C:
		case 0x1D:
		case 0x1F:
			buf.ReadByte();
			break;

		case 0x0A:
		case 0x10:
		case 0x12:
		case 0x13:
		case 0x21:
		case 0x22:
			buf.ReadWord();
			break;

		case 0x1E:
			buf.ReadDWord();
			break;

		case 0x17:
			for (uint j = 0; j < 4; j++) buf.ReadByte();
			break;

		case 0x20: {
			uint8_t count = buf.ReadByte();
			for (uint8_t j = 0; j < count; j++) buf.ReadByte();
			break;
		}

		case 0x23:
			buf.Skip(buf.ReadByte() * 2);
			break;

		default:
			ret = CIR_UNKNOWN;
			break;
	}
	return ret;
}

/**
 * Define properties for houses
 * @param first Local ID of the first house.
 * @param last Local ID of the last house.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult TownHouseChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	if (last > NUM_HOUSES_PER_GRF) {
		GrfMsg(1, "TownHouseChangeInfo: Too many houses loaded ({}), max ({}). Ignoring.", last, NUM_HOUSES_PER_GRF);
		return CIR_INVALID_ID;
	}

	/* Allocate house specs if they haven't been allocated already. */
	if (_cur_gps.grffile->housespec.size() < last) _cur_gps.grffile->housespec.resize(last);

	for (uint id = first; id < last; ++id) {
		auto &housespec = _cur_gps.grffile->housespec[id];

		if (prop != 0x08 && housespec == nullptr) {
			/* If the house property 08 is not yet set, ignore this property */
			ChangeInfoResult cir = IgnoreTownHouseProperty(prop, buf);
			if (cir > ret) ret = cir;
			continue;
		}

		switch (prop) {
			case 0x08: { // Substitute building type, and definition of a new house
				uint8_t subs_id = buf.ReadByte();
				if (subs_id == 0xFF) {
					/* Instead of defining a new house, a substitute house id
					 * of 0xFF disables the old house with the current id. */
					if (id < NEW_HOUSE_OFFSET) HouseSpec::Get(id)->enabled = false;
					continue;
				} else if (subs_id >= NEW_HOUSE_OFFSET) {
					/* The substitute id must be one of the original houses. */
					GrfMsg(2, "TownHouseChangeInfo: Attempt to use new house {} as substitute house for {}. Ignoring.", subs_id, id);
					continue;
				}

				/* Allocate space for this house. */
				if (housespec == nullptr) {
					/* Only the first property 08 setting copies properties; if you later change it, properties will stay. */
					housespec = std::make_unique<HouseSpec>(*HouseSpec::Get(subs_id));

					housespec->enabled = true;
					housespec->grf_prop.local_id = id;
					housespec->grf_prop.subst_id = subs_id;
					housespec->grf_prop.SetGRFFile(_cur_gps.grffile);
					/* Set default colours for randomization, used if not overridden. */
					housespec->random_colour[0] = COLOUR_RED;
					housespec->random_colour[1] = COLOUR_BLUE;
					housespec->random_colour[2] = COLOUR_ORANGE;
					housespec->random_colour[3] = COLOUR_GREEN;

					/* House flags 40 and 80 are exceptions; these flags are never set automatically. */
					housespec->building_flags.Reset({BuildingFlag::IsChurch, BuildingFlag::IsStadium});

					/* Make sure that the third cargo type is valid in this
					 * climate. This can cause problems when copying the properties
					 * of a house that accepts food, where the new house is valid
					 * in the temperate climate. */
					CargoType cargo_type = housespec->accepts_cargo[2];
					if (!IsValidCargoType(cargo_type)) cargo_type = GetCargoTypeByLabel(housespec->accepts_cargo_label[2]);
					if (!IsValidCargoType(cargo_type)) {
						housespec->cargo_acceptance[2] = 0;
					}
				}
				break;
			}

			case 0x09: // Building flags
				housespec->building_flags = (BuildingFlags)buf.ReadByte();
				break;

			case 0x0A: { // Availability years
				uint16_t years = buf.ReadWord();
				housespec->min_year = GB(years, 0, 8) > 150 ? CalendarTime::MAX_YEAR : CalendarTime::ORIGINAL_BASE_YEAR + GB(years, 0, 8);
				housespec->max_year = GB(years, 8, 8) > 150 ? CalendarTime::MAX_YEAR : CalendarTime::ORIGINAL_BASE_YEAR + GB(years, 8, 8);
				break;
			}

			case 0x0B: // Population
				housespec->population = buf.ReadByte();
				break;

			case 0x0C: // Mail generation multiplier
				housespec->mail_generation = buf.ReadByte();
				break;

			case 0x0D: // Passenger acceptance
			case 0x0E: // Mail acceptance
				housespec->cargo_acceptance[prop - 0x0D] = buf.ReadByte();
				break;

			case 0x0F: { // Goods/candy, food/fizzy drinks acceptance
				int8_t goods = buf.ReadByte();

				/* If value of goods is negative, it means in fact food or, if in toyland, fizzy_drink acceptance.
				 * Else, we have "standard" 3rd cargo type, goods or candy, for toyland once more */
				CargoType cargo_type = (goods >= 0) ? ((_settings_game.game_creation.landscape == LandscapeType::Toyland) ? GetCargoTypeByLabel(CT_CANDY) : GetCargoTypeByLabel(CT_GOODS)) :
						((_settings_game.game_creation.landscape == LandscapeType::Toyland) ? GetCargoTypeByLabel(CT_FIZZY_DRINKS) : GetCargoTypeByLabel(CT_FOOD));

				/* Make sure the cargo type is valid in this climate. */
				if (!IsValidCargoType(cargo_type)) goods = 0;

				housespec->accepts_cargo[2] = cargo_type;
				housespec->accepts_cargo_label[2] = CT_INVALID;
				housespec->cargo_acceptance[2] = abs(goods); // but we do need positive value here
				break;
			}

			case 0x10: // Local authority rating decrease on removal
				housespec->remove_rating_decrease = buf.ReadWord();
				break;

			case 0x11: // Removal cost multiplier
				housespec->removal_cost = buf.ReadByte();
				break;

			case 0x12: // Building name ID
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &housespec->building_name);
				break;

			case 0x13: // Building availability mask
				housespec->building_availability = (HouseZones)buf.ReadWord();
				break;

			case 0x14: { // House callback mask
				auto mask = housespec->callback_mask.base();
				SB(mask, 0, 8, buf.ReadByte());
				housespec->callback_mask = HouseCallbackMasks{mask};
				break;
			}

			case 0x15: { // House override byte
				uint8_t override_id = buf.ReadByte();

				/* The house being overridden must be an original house. */
				if (override_id >= NEW_HOUSE_OFFSET) {
					GrfMsg(2, "TownHouseChangeInfo: Attempt to override new house {} with house id {}. Ignoring.", override_id, id);
					continue;
				}

				_house_mngr.Add(id, _cur_gps.grffile->grfid, override_id);
				break;
			}

			case 0x16: // Periodic refresh multiplier
				housespec->processing_time = std::min<uint8_t>(buf.ReadByte(), 63u);
				break;

			case 0x17: // Four random colours to use
				for (uint j = 0; j < 4; j++) housespec->random_colour[j] = static_cast<Colours>(GB(buf.ReadByte(), 0, 4));
				break;

			case 0x18: // Relative probability of appearing
				housespec->probability = buf.ReadByte();
				break;

			case 0x19: // Extra flags
				housespec->extra_flags = static_cast<HouseExtraFlags>(buf.ReadByte());
				break;

			case 0x1A: { // Animation frames
				uint8_t info = buf.ReadByte();
				housespec->animation.frames = GB(info, 0, 7);
				housespec->animation.status = HasBit(info, 7) ? AnimationStatus::Looping : AnimationStatus::NonLooping;
				break;
			}

			case 0x1B: // Animation speed
				housespec->animation.speed = Clamp(buf.ReadByte(), 2, 16);
				break;

			case 0x1C: // Class of the building type
				housespec->class_id = AllocateHouseClassID(buf.ReadByte(), _cur_gps.grffile->grfid);
				break;

			case 0x1D: { // Callback mask part 2
				auto mask = housespec->callback_mask.base();
				SB(mask, 8, 8, buf.ReadByte());
				housespec->callback_mask = HouseCallbackMasks{mask};
				break;
			}

			case 0x1E: { // Accepted cargo types
				uint32_t cargotypes = buf.ReadDWord();

				/* Check if the cargo types should not be changed */
				if (cargotypes == 0xFFFFFFFF) break;

				for (uint j = 0; j < HOUSE_ORIGINAL_NUM_ACCEPTS; j++) {
					/* Get the cargo number from the 'list' */
					uint8_t cargo_part = GB(cargotypes, 8 * j, 8);
					CargoType cargo = GetCargoTranslation(cargo_part, _cur_gps.grffile);

					if (!IsValidCargoType(cargo)) {
						/* Disable acceptance of invalid cargo type */
						housespec->cargo_acceptance[j] = 0;
					} else {
						housespec->accepts_cargo[j] = cargo;
					}
					housespec->accepts_cargo_label[j] = CT_INVALID;
				}
				break;
			}

			case 0x1F: // Minimum life span
				housespec->minimum_life = buf.ReadByte();
				break;

			case 0x20: { // Cargo acceptance watch list
				uint8_t count = buf.ReadByte();
				for (uint8_t j = 0; j < count; j++) {
					CargoType cargo = GetCargoTranslation(buf.ReadByte(), _cur_gps.grffile);
					if (IsValidCargoType(cargo)) SetBit(housespec->watched_cargoes, cargo);
				}
				break;
			}

			case 0x21: // long introduction year
				housespec->min_year = TimerGameCalendar::Year{buf.ReadWord()};
				break;

			case 0x22: // long maximum year
				housespec->max_year = TimerGameCalendar::Year{buf.ReadWord()};
				if (housespec->max_year == UINT16_MAX) housespec->max_year = CalendarTime::MAX_YEAR;
				break;

			case 0x23: { // variable length cargo types accepted
				uint count = buf.ReadByte();
				if (count > lengthof(housespec->accepts_cargo)) {
					GRFError *error = DisableGrf(STR_NEWGRF_ERROR_LIST_PROPERTY_TOO_LONG);
					error->param_value[1] = prop;
					return CIR_DISABLED;
				}
				/* Always write the full accepts_cargo array, and check each index for being inside the
				 * provided data. This ensures all values are properly initialized, and also avoids
				 * any risks of array overrun. */
				for (uint i = 0; i < lengthof(housespec->accepts_cargo); i++) {
					if (i < count) {
						housespec->accepts_cargo[i] = GetCargoTranslation(buf.ReadByte(), _cur_gps.grffile);
						housespec->cargo_acceptance[i] = buf.ReadByte();
					} else {
						housespec->accepts_cargo[i] = INVALID_CARGO;
						housespec->cargo_acceptance[i] = 0;
					}
					if (i < std::size(housespec->accepts_cargo_label)) housespec->accepts_cargo_label[i] = CT_INVALID;
				}
				break;
			}

			case 0x24: // Badge list
				housespec->badges = ReadBadgeList(buf, GSF_HOUSES);
				break;

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_HOUSES>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_HOUSES>::Activation(uint first, uint last, int prop, ByteReader &buf) { return TownHouseChangeInfo(first, last, prop, buf); }
