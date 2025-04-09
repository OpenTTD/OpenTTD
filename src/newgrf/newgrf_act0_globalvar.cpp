/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_act0_globalvar.cpp NewGRF Action 0x00 handler for global variables. */

#include "../stdafx.h"
#include "../debug.h"
#include "../currency.h"
#include "../landscape.h"
#include "../language.h"
#include "../rev.h"
#include "../string_func.h"
#include "../core/utf8.hpp"
#include "../newgrf.h"
#include "../newgrf_badge.h"
#include "../newgrf_badge_type.h"
#include "../newgrf_cargo.h"
#include "../newgrf_engine.h"
#include "../newgrf_sound.h"
#include "../vehicle_base.h"
#include "../rail.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal_vehicle.h"
#include "newgrf_internal.h"
#include "newgrf_stringmapping.h"

#include "table/strings.h"

#include "../safeguards.h"

/**
 * Load a cargo- or railtype-translation table.
 * @param first ID of the first translation table entry.
 * @param last ID of the last translation table entry.
 * @param buf The property value.
 * @param gettable Function to get storage for the translation table.
 * @param name Name of the table for debug output.
 * @return ChangeInfoResult.
 */
template <typename T, typename TGetTableFunc>
static ChangeInfoResult LoadTranslationTable(uint first, uint last, ByteReader &buf, TGetTableFunc gettable, std::string_view name)
{
	if (first != 0) {
		GrfMsg(1, "LoadTranslationTable: {} translation table must start at zero", name);
		return CIR_INVALID_ID;
	}

	std::vector<T> &translation_table = gettable(*_cur_gps.grffile);
	translation_table.clear();
	translation_table.reserve(last);
	for (uint id = first; id < last; ++id) {
		translation_table.push_back(T(std::byteswap(buf.ReadDWord())));
	}

	GRFFile *grf_override = GetCurrentGRFOverride();
	if (grf_override != nullptr) {
		/* GRF override is present, copy the translation table to the overridden GRF as well. */
		GrfMsg(1, "LoadTranslationTable: Copying {} translation table to override GRFID '{}'", name, std::byteswap(grf_override->grfid));
		std::vector<T> &override_table = gettable(*grf_override);
		override_table = translation_table;
	}

	return CIR_SUCCESS;
}

static ChangeInfoResult LoadBadgeTranslationTable(uint first, uint last, ByteReader &buf, std::vector<BadgeID> &translation_table, const char *name)
{
	if (first != 0 && first != std::size(translation_table)) {
		GrfMsg(1, "LoadBadgeTranslationTable: {} translation table must start at zero or {}", name, std::size(translation_table));
		return CIR_INVALID_ID;
	}

	if (first == 0) translation_table.clear();
	translation_table.reserve(last);
	for (uint id = first; id < last; ++id) {
		std::string_view label = buf.ReadString();
		translation_table.push_back(GetOrCreateBadge(label).index);
	}

	return CIR_SUCCESS;
}

/**
 * Helper to read a DWord worth of bytes from the reader
 * and to return it as a valid string.
 * @param reader The source of the DWord.
 * @return The read DWord as string.
 */
static std::string ReadDWordAsString(ByteReader &reader)
{
	std::string output;
	for (int i = 0; i < 4; i++) output.push_back(reader.ReadByte());
	return StrMakeValid(output);
}

/**
 * Define properties for global variables
 * @param first ID of the first global var.
 * @param last ID of the last global var.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult GlobalVarChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	/* Properties which are handled as a whole */
	switch (prop) {
		case 0x09: // Cargo Translation Table; loading during both reservation and activation stage (in case it is selected depending on defined cargos)
			return LoadTranslationTable<CargoLabel>(first, last, buf, [](GRFFile &grf) -> std::vector<CargoLabel> & { return grf.cargo_list; }, "Cargo");

		case 0x12: // Rail type translation table; loading during both reservation and activation stage (in case it is selected depending on defined railtypes)
			return LoadTranslationTable<RailTypeLabel>(first, last, buf, [](GRFFile &grf) -> std::vector<RailTypeLabel> & { return grf.railtype_list; }, "Rail type");

		case 0x16: // Road type translation table; loading during both reservation and activation stage (in case it is selected depending on defined roadtypes)
			return LoadTranslationTable<RoadTypeLabel>(first, last, buf, [](GRFFile &grf) -> std::vector<RoadTypeLabel> & { return grf.roadtype_list; }, "Road type");

		case 0x17: // Tram type translation table; loading during both reservation and activation stage (in case it is selected depending on defined tramtypes)
			return LoadTranslationTable<RoadTypeLabel>(first, last, buf, [](GRFFile &grf) -> std::vector<RoadTypeLabel> & { return grf.tramtype_list; }, "Tram type");

		case 0x18: // Badge translation table
			return LoadBadgeTranslationTable(first, last, buf, _cur_gps.grffile->badge_list, "Badge");

		default:
			break;
	}

	/* Properties which are handled per item */
	ChangeInfoResult ret = CIR_SUCCESS;
	for (uint id = first; id < last; ++id) {
		switch (prop) {
			case 0x08: { // Cost base factor
				int factor = buf.ReadByte();

				if (id < PR_END) {
					_cur_gps.grffile->price_base_multipliers[id] = std::min<int>(factor - 8, MAX_PRICE_MODIFIER);
				} else {
					GrfMsg(1, "GlobalVarChangeInfo: Price {} out of range, ignoring", id);
				}
				break;
			}

			case 0x0A: { // Currency display names
				uint curidx = GetNewgrfCurrencyIdConverted(id);
				if (curidx < CURRENCY_END) {
					AddStringForMapping(GRFStringID{buf.ReadWord()}, [curidx](StringID str) {
						_currency_specs[curidx].name = str;
						_currency_specs[curidx].code.clear();
					});
				} else {
					buf.ReadWord();
				}
				break;
			}

			case 0x0B: { // Currency multipliers
				uint curidx = GetNewgrfCurrencyIdConverted(id);
				uint32_t rate = buf.ReadDWord();

				if (curidx < CURRENCY_END) {
					/* TTDPatch uses a multiple of 1000 for its conversion calculations,
					 * which OTTD does not. For this reason, divide grf value by 1000,
					 * to be compatible */
					_currency_specs[curidx].rate = rate / 1000;
				} else {
					GrfMsg(1, "GlobalVarChangeInfo: Currency multipliers {} out of range, ignoring", curidx);
				}
				break;
			}

			case 0x0C: { // Currency options
				uint curidx = GetNewgrfCurrencyIdConverted(id);
				uint16_t options = buf.ReadWord();

				if (curidx < CURRENCY_END) {
					_currency_specs[curidx].separator.clear();
					_currency_specs[curidx].separator.push_back(GB(options, 0, 8));
					/* By specifying only one bit, we prevent errors,
					 * since newgrf specs said that only 0 and 1 can be set for symbol_pos */
					_currency_specs[curidx].symbol_pos = GB(options, 8, 1);
				} else {
					GrfMsg(1, "GlobalVarChangeInfo: Currency option {} out of range, ignoring", curidx);
				}
				break;
			}

			case 0x0D: { // Currency prefix symbol
				uint curidx = GetNewgrfCurrencyIdConverted(id);
				std::string prefix = ReadDWordAsString(buf);

				if (curidx < CURRENCY_END) {
					_currency_specs[curidx].prefix = std::move(prefix);
				} else {
					GrfMsg(1, "GlobalVarChangeInfo: Currency symbol {} out of range, ignoring", curidx);
				}
				break;
			}

			case 0x0E: { // Currency suffix symbol
				uint curidx = GetNewgrfCurrencyIdConverted(id);
				std::string suffix = ReadDWordAsString(buf);

				if (curidx < CURRENCY_END) {
					_currency_specs[curidx].suffix = std::move(suffix);
				} else {
					GrfMsg(1, "GlobalVarChangeInfo: Currency symbol {} out of range, ignoring", curidx);
				}
				break;
			}

			case 0x0F: { //  Euro introduction dates
				uint curidx = GetNewgrfCurrencyIdConverted(id);
				TimerGameCalendar::Year year_euro{buf.ReadWord()};

				if (curidx < CURRENCY_END) {
					_currency_specs[curidx].to_euro = year_euro;
				} else {
					GrfMsg(1, "GlobalVarChangeInfo: Euro intro date {} out of range, ignoring", curidx);
				}
				break;
			}

			case 0x10: // Snow line height table
				if (last > 1 || IsSnowLineSet()) {
					GrfMsg(1, "GlobalVarChangeInfo: The snowline can only be set once ({})", last);
				} else if (buf.Remaining() < SNOW_LINE_MONTHS * SNOW_LINE_DAYS) {
					GrfMsg(1, "GlobalVarChangeInfo: Not enough entries set in the snowline table ({})", buf.Remaining());
				} else {
					auto snow_line = std::make_unique<SnowLine>();

					for (uint i = 0; i < SNOW_LINE_MONTHS; i++) {
						for (uint j = 0; j < SNOW_LINE_DAYS; j++) {
							uint8_t &level = snow_line->table[i][j];
							level = buf.ReadByte();
							if (_cur_gps.grffile->grf_version >= 8) {
								if (level != 0xFF) level = level * (1 + _settings_game.construction.map_height_limit) / 256;
							} else {
								if (level >= 128) {
									/* no snow */
									level = 0xFF;
								} else {
									level = level * (1 + _settings_game.construction.map_height_limit) / 128;
								}
							}

							snow_line->highest_value = std::max(snow_line->highest_value, level);
							snow_line->lowest_value = std::min(snow_line->lowest_value, level);
						}
					}
					SetSnowLine(std::move(snow_line));
				}
				break;

			case 0x11: // GRF match for engine allocation
				/* This is loaded during the reservation stage, so just skip it here. */
				/* Each entry is 8 bytes. */
				buf.Skip(8);
				break;

			case 0x13:   // Gender translation table
			case 0x14:   // Case translation table
			case 0x15: { // Plural form translation
				uint curidx = id; // The current index, i.e. language.
				const LanguageMetadata *lang = curidx < MAX_LANG ? GetLanguage(curidx) : nullptr;
				if (lang == nullptr) {
					GrfMsg(1, "GlobalVarChangeInfo: Language {} is not known, ignoring", curidx);
					/* Skip over the data. */
					if (prop == 0x15) {
						buf.ReadByte();
					} else {
						while (buf.ReadByte() != 0) {
							buf.ReadString();
						}
					}
					break;
				}

				if (prop == 0x15) {
					uint plural_form = buf.ReadByte();
					if (plural_form >= LANGUAGE_MAX_PLURAL) {
						GrfMsg(1, "GlobalVarChanceInfo: Plural form {} is out of range, ignoring", plural_form);
					} else {
						_cur_gps.grffile->language_map[curidx].plural_form = plural_form;
					}
					break;
				}

				uint8_t newgrf_id = buf.ReadByte(); // The NewGRF (custom) identifier.
				while (newgrf_id != 0) {
					std::string_view name = buf.ReadString(); // The name for the OpenTTD identifier.

					/* We'll just ignore the UTF8 identifier character. This is (fairly)
					 * safe as OpenTTD's strings gender/cases are usually in ASCII which
					 * is just a subset of UTF8, or they need the bigger UTF8 characters
					 * such as Cyrillic. Thus we will simply assume they're all UTF8. */
					auto [len, c] = DecodeUtf8(name);
					if (c == NFO_UTF8_IDENTIFIER) name.remove_prefix(len);

					LanguageMap::Mapping map;
					map.newgrf_id = newgrf_id;
					if (prop == 0x13) {
						map.openttd_id = lang->GetGenderIndex(name);
						if (map.openttd_id >= MAX_NUM_GENDERS) {
							GrfMsg(1, "GlobalVarChangeInfo: Gender name {} is not known, ignoring", StrMakeValid(name));
						} else {
							_cur_gps.grffile->language_map[curidx].gender_map.push_back(map);
						}
					} else {
						map.openttd_id = lang->GetCaseIndex(name);
						if (map.openttd_id >= MAX_NUM_CASES) {
							GrfMsg(1, "GlobalVarChangeInfo: Case name {} is not known, ignoring", StrMakeValid(name));
						} else {
							_cur_gps.grffile->language_map[curidx].case_map.push_back(map);
						}
					}
					newgrf_id = buf.ReadByte();
				}
				break;
			}

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

static ChangeInfoResult GlobalVarReserveInfo(uint first, uint last, int prop, ByteReader &buf)
{
	/* Properties which are handled as a whole */
	switch (prop) {
		case 0x09: // Cargo Translation Table; loading during both reservation and activation stage (in case it is selected depending on defined cargos)
			return LoadTranslationTable<CargoLabel>(first, last, buf, [](GRFFile &grf) -> std::vector<CargoLabel> & { return grf.cargo_list; }, "Cargo");

		case 0x12: // Rail type translation table; loading during both reservation and activation stage (in case it is selected depending on defined railtypes)
			return LoadTranslationTable<RailTypeLabel>(first, last, buf, [](GRFFile &grf) -> std::vector<RailTypeLabel> & { return grf.railtype_list; }, "Rail type");

		case 0x16: // Road type translation table; loading during both reservation and activation stage (in case it is selected depending on defined roadtypes)
			return LoadTranslationTable<RoadTypeLabel>(first, last, buf, [](GRFFile &grf) -> std::vector<RoadTypeLabel> & { return grf.roadtype_list; }, "Road type");

		case 0x17: // Tram type translation table; loading during both reservation and activation stage (in case it is selected depending on defined tramtypes)
			return LoadTranslationTable<RoadTypeLabel>(first, last, buf, [](GRFFile &grf) -> std::vector<RoadTypeLabel> & { return grf.tramtype_list; }, "Tram type");

		case 0x18: // Badge translation table
			return LoadBadgeTranslationTable(first, last, buf, _cur_gps.grffile->badge_list, "Badge");

		default:
			break;
	}

	/* Properties which are handled per item */
	ChangeInfoResult ret = CIR_SUCCESS;

	for (uint id = first; id < last; ++id) {
		switch (prop) {
			case 0x08: // Cost base factor
			case 0x15: // Plural form translation
				buf.ReadByte();
				break;

			case 0x0A: // Currency display names
			case 0x0C: // Currency options
			case 0x0F: // Euro introduction dates
				buf.ReadWord();
				break;

			case 0x0B: // Currency multipliers
			case 0x0D: // Currency prefix symbol
			case 0x0E: // Currency suffix symbol
				buf.ReadDWord();
				break;

			case 0x10: // Snow line height table
				buf.Skip(SNOW_LINE_MONTHS * SNOW_LINE_DAYS);
				break;

			case 0x11: { // GRF match for engine allocation
				uint32_t s = buf.ReadDWord();
				uint32_t t = buf.ReadDWord();
				SetNewGRFOverride(s, t);
				break;
			}

			case 0x13: // Gender translation table
			case 0x14: // Case translation table
				while (buf.ReadByte() != 0) {
					buf.ReadString();
				}
				break;

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}


/**
 * Reads a variable common to VarAction2 and Action7/9/D.
 *
 * Returns VarAction2 variable 'param' resp. Action7/9/D variable '0x80 + param'.
 * If a variable is not accessible from all four actions, it is handled in the action specific functions.
 *
 * @param param variable number (as for VarAction2, for Action7/9/D you have to subtract 0x80 first).
 * @param value returns the value of the variable.
 * @param grffile NewGRF querying the variable
 * @return true iff the variable is known and the value is returned in 'value'.
 */
bool GetGlobalVariable(uint8_t param, uint32_t *value, const GRFFile *grffile)
{
	switch (param) {
		case 0x00: // current date
			*value = std::max(TimerGameCalendar::date - CalendarTime::DAYS_TILL_ORIGINAL_BASE_YEAR, TimerGameCalendar::Date(0)).base();
			return true;

		case 0x01: // current year
			*value = (Clamp(TimerGameCalendar::year, CalendarTime::ORIGINAL_BASE_YEAR, CalendarTime::ORIGINAL_MAX_YEAR) - CalendarTime::ORIGINAL_BASE_YEAR).base();
			return true;

		case 0x02: { // detailed date information: month of year (bit 0-7), day of month (bit 8-12), leap year (bit 15), day of year (bit 16-24)
			TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date);
			TimerGameCalendar::Date start_of_year = TimerGameCalendar::ConvertYMDToDate(ymd.year, 0, 1);
			*value = ymd.month | (ymd.day - 1) << 8 | (TimerGameCalendar::IsLeapYear(ymd.year) ? 1 << 15 : 0) | (TimerGameCalendar::date - start_of_year).base() << 16;
			return true;
		}

		case 0x03: // current climate, 0=temp, 1=arctic, 2=trop, 3=toyland
			*value = to_underlying(_settings_game.game_creation.landscape);
			return true;

		case 0x06: // road traffic side, bit 4 clear=left, set=right
			*value = _settings_game.vehicle.road_side << 4;
			return true;

		case 0x09: // date fraction
			*value = TimerGameCalendar::date_fract * 885;
			return true;

		case 0x0A: // animation counter
			*value = GB(TimerGameTick::counter, 0, 16);
			return true;

		case 0x0B: { // TTDPatch version
			uint major    = 2;
			uint minor    = 6;
			uint revision = 1; // special case: 2.0.1 is 2.0.10
			uint build    = 1382;
			*value = (major << 24) | (minor << 20) | (revision << 16) | build;
			return true;
		}

		case 0x0D: // TTD Version, 00=DOS, 01=Windows
			*value = GetGRFConfig(grffile->grfid)->palette & GRFP_USE_MASK;
			return true;

		case 0x0E: // Y-offset for train sprites
			*value = grffile->traininfo_vehicle_pitch;
			return true;

		case 0x0F: // Rail track type cost factors
			*value = 0;
			SB(*value, 0, 8, GetRailTypeInfo(RAILTYPE_RAIL)->cost_multiplier); // normal rail
			if (_settings_game.vehicle.disable_elrails) {
				/* skip elrail multiplier - disabled */
				SB(*value, 8, 8, GetRailTypeInfo(RAILTYPE_MONO)->cost_multiplier); // monorail
			} else {
				SB(*value, 8, 8, GetRailTypeInfo(RAILTYPE_ELECTRIC)->cost_multiplier); // electified railway
				/* Skip monorail multiplier - no space in result */
			}
			SB(*value, 16, 8, GetRailTypeInfo(RAILTYPE_MAGLEV)->cost_multiplier); // maglev
			return true;

		case 0x11: // current rail tool type
			*value = 0; // constant fake value to avoid desync
			return true;

		case 0x12: // Game mode
			*value = _game_mode;
			return true;

		/* case 0x13: // Tile refresh offset to left    not implemented */
		/* case 0x14: // Tile refresh offset to right   not implemented */
		/* case 0x15: // Tile refresh offset upwards    not implemented */
		/* case 0x16: // Tile refresh offset downwards  not implemented */
		/* case 0x17: // temperate snow line            not implemented */

		case 0x1A: // Always -1
			*value = UINT_MAX;
			return true;

		case 0x1B: // Display options
			*value = 0x3F; // constant fake value to avoid desync
			return true;

		case 0x1D: // TTD Platform, 00=TTDPatch, 01=OpenTTD
			*value = 1;
			return true;

		case 0x1E: { // Miscellaneous GRF features
			GrfMiscBits bits = _misc_grf_features;

			/* Add the local flags */
			assert(!bits.Test(GrfMiscBit::TrainWidth32Pixels));
			if (grffile->traininfo_vehicle_width == VEHICLEINFO_FULL_VEHICLE_WIDTH) bits.Set(GrfMiscBit::TrainWidth32Pixels);

			*value = bits.base();
			return true;
		}

		/* case 0x1F: // locale dependent settings not implemented to avoid desync */

		case 0x20: { // snow line height
			uint8_t snowline = GetSnowLine();
			if (_settings_game.game_creation.landscape == LandscapeType::Arctic && snowline <= _settings_game.construction.map_height_limit) {
				*value = Clamp(snowline * (grffile->grf_version >= 8 ? 1 : TILE_HEIGHT), 0, 0xFE);
			} else {
				/* No snow */
				*value = 0xFF;
			}
			return true;
		}

		case 0x21: // OpenTTD version
			*value = _openttd_newgrf_version;
			return true;

		case 0x22: // difficulty level
			*value = SP_CUSTOM;
			return true;

		case 0x23: // long format date
			*value = TimerGameCalendar::date.base();
			return true;

		case 0x24: // long format year
			*value = TimerGameCalendar::year.base();
			return true;

		default: return false;
	}
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_GLOBALVAR>::Reserve(uint first, uint last, int prop, ByteReader &buf) { return GlobalVarReserveInfo(first, last, prop, buf); }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_GLOBALVAR>::Activation(uint first, uint last, int prop, ByteReader &buf) { return GlobalVarChangeInfo(first, last, prop, buf); }
