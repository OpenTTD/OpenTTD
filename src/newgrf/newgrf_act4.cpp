/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act4.cpp NewGRF Action 0x04 handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "../engine_base.h"
#include "../house.h"
#include "../newgrf_engine.h"
#include "../newgrf_text.h"
#include "../newgrf_badge.h"
#include "../newgrf_badge_type.h"
#include "../newgrf_cargo.h"
#include "../newgrf_station.h"
#include "../newgrf_airporttiles.h"
#include "../string_func.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal_vehicle.h"
#include "newgrf_internal.h"

#include "table/strings.h"

#include "../safeguards.h"

/* Action 0x04 */
static void FeatureNewName(ByteReader &buf)
{
	/* <04> <veh-type> <language-id> <num-veh> <offset> <data...>
	 *
	 * B veh-type      see action 0 (as 00..07, + 0A
	 *                 But IF veh-type = 48, then generic text
	 * B language-id   If bit 6 is set, This is the extended language scheme,
	 *                 with up to 64 language.
	 *                 Otherwise, it is a mapping where set bits have meaning
	 *                 0 = american, 1 = english, 2 = german, 3 = french, 4 = spanish
	 *                 Bit 7 set means this is a generic text, not a vehicle one (or else)
	 * B num-veh       number of vehicles which are getting a new name
	 * B/W offset      number of the first vehicle that gets a new name
	 *                 Byte : ID of vehicle to change
	 *                 Word : ID of string to change/add
	 * S data          new texts, each of them zero-terminated, after
	 *                 which the next name begins. */

	bool new_scheme = _cur_gps.grffile->grf_version >= 7;

	GrfSpecFeature feature{buf.ReadByte()};
	if (feature >= GSF_END && feature != GSF_ORIGINAL_STRINGS) {
		GrfMsg(1, "FeatureNewName: Unsupported feature 0x{:02X}, skipping", feature);
		return;
	}

	uint8_t lang     = buf.ReadByte();
	uint8_t num      = buf.ReadByte();
	bool generic   = HasBit(lang, 7);
	uint16_t id;
	if (generic) {
		id = buf.ReadWord();
	} else if (feature <= GSF_AIRCRAFT || feature == GSF_BADGES) {
		id = buf.ReadExtendedByte();
	} else {
		id = buf.ReadByte();
	}

	ClrBit(lang, 7);

	uint16_t endid = id + num;

	GrfMsg(6, "FeatureNewName: About to rename engines {}..{} (feature 0x{:02X}) in language 0x{:02X}",
	               id, endid, feature, lang);

	/* Feature overlay to make non-generic strings unique in their feature. We use feature + 1 so that generic strings stay as they are. */
	uint32_t feature_overlay = generic ? 0 : ((feature + 1) << 16);

	for (; id < endid && buf.HasData(); id++) {
		std::string_view name = buf.ReadString();
		GrfMsg(8, "FeatureNewName: 0x{:04X} <- {}", id, StrMakeValid(name));

		switch (feature) {
			case GSF_TRAINS:
			case GSF_ROADVEHICLES:
			case GSF_SHIPS:
			case GSF_AIRCRAFT:
				if (!generic) {
					Engine *e = GetNewEngine(_cur_gps.grffile, (VehicleType)feature, id, _cur_gps.grfconfig->flags.Test(GRFConfigFlag::Static));
					if (e == nullptr) break;
					StringID string = AddGRFString(_cur_gps.grffile->grfid, GRFStringID{feature_overlay | e->index.base()}, lang, new_scheme, false, name, e->info.string_id);
					e->info.string_id = string;
				} else {
					AddGRFString(_cur_gps.grffile->grfid, GRFStringID{id}, lang, new_scheme, true, name, STR_UNDEFINED);
				}
				break;

			case GSF_BADGES: {
				if (!generic) {
					auto found = _cur_gps.grffile->badge_map.find(id);
					if (found == std::end(_cur_gps.grffile->badge_map)) {
						GrfMsg(1, "FeatureNewName: Attempt to name undefined badge 0x{:X}, ignoring", id);
					} else {
						Badge &badge = *GetBadge(found->second);
						badge.name = AddGRFString(_cur_gps.grffile->grfid, GRFStringID{feature_overlay | id}, lang, true, false, name, STR_UNDEFINED);
					}
				} else {
					AddGRFString(_cur_gps.grffile->grfid, GRFStringID{id}, lang, new_scheme, true, name, STR_UNDEFINED);
				}
				break;
			}

			default:
				if (IsInsideMM(id, 0xD000, 0xD400) || IsInsideMM(id, 0xD800, 0x10000)) {
					AddGRFString(_cur_gps.grffile->grfid, GRFStringID{id}, lang, new_scheme, true, name, STR_UNDEFINED);
					break;
				}

				switch (GB(id, 8, 8)) {
					case 0xC4: // Station class name
						if (GB(id, 0, 8) >= _cur_gps.grffile->stations.size() || _cur_gps.grffile->stations[GB(id, 0, 8)] == nullptr) {
							GrfMsg(1, "FeatureNewName: Attempt to name undefined station 0x{:X}, ignoring", GB(id, 0, 8));
						} else {
							StationClassID class_index = _cur_gps.grffile->stations[GB(id, 0, 8)]->class_index;
							StationClass::Get(class_index)->name = AddGRFString(_cur_gps.grffile->grfid, GRFStringID{id}, lang, new_scheme, false, name, STR_UNDEFINED);
						}
						break;

					case 0xC5: // Station name
						if (GB(id, 0, 8) >= _cur_gps.grffile->stations.size() || _cur_gps.grffile->stations[GB(id, 0, 8)] == nullptr) {
							GrfMsg(1, "FeatureNewName: Attempt to name undefined station 0x{:X}, ignoring", GB(id, 0, 8));
						} else {
							_cur_gps.grffile->stations[GB(id, 0, 8)]->name = AddGRFString(_cur_gps.grffile->grfid, GRFStringID{id}, lang, new_scheme, false, name, STR_UNDEFINED);
						}
						break;

					case 0xC7: // Airporttile name
						if (GB(id, 0, 8) >= _cur_gps.grffile->airtspec.size() || _cur_gps.grffile->airtspec[GB(id, 0, 8)] == nullptr) {
							GrfMsg(1, "FeatureNewName: Attempt to name undefined airport tile 0x{:X}, ignoring", GB(id, 0, 8));
						} else {
							_cur_gps.grffile->airtspec[GB(id, 0, 8)]->name = AddGRFString(_cur_gps.grffile->grfid, GRFStringID{id}, lang, new_scheme, false, name, STR_UNDEFINED);
						}
						break;

					case 0xC9: // House name
						if (GB(id, 0, 8) >= _cur_gps.grffile->housespec.size() || _cur_gps.grffile->housespec[GB(id, 0, 8)] == nullptr) {
							GrfMsg(1, "FeatureNewName: Attempt to name undefined house 0x{:X}, ignoring.", GB(id, 0, 8));
						} else {
							_cur_gps.grffile->housespec[GB(id, 0, 8)]->building_name = AddGRFString(_cur_gps.grffile->grfid, GRFStringID{id}, lang, new_scheme, false, name, STR_UNDEFINED);
						}
						break;

					default:
						GrfMsg(7, "FeatureNewName: Unsupported ID (0x{:04X})", id);
						break;
				}
				break;
		}
	}
}

template <> void GrfActionHandler<0x04>::FileScan(ByteReader &) { }
template <> void GrfActionHandler<0x04>::SafetyScan(ByteReader &) { }
template <> void GrfActionHandler<0x04>::LabelScan(ByteReader &) { }
template <> void GrfActionHandler<0x04>::Init(ByteReader &) { }
template <> void GrfActionHandler<0x04>::Reserve(ByteReader &) { }
template <> void GrfActionHandler<0x04>::Activation(ByteReader &buf) { FeatureNewName(buf); }
