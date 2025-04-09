/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_act0_airports.cpp NewGRF Action 0x00 handler for airports. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_airporttiles.h"
#include "../newgrf_airport.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"
#include "newgrf_stringmapping.h"

#include "../safeguards.h"

/**
 * Define properties for airports
 * @param first Local ID of the first airport.
 * @param last Local ID of the last airport.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult AirportChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	if (last > NUM_AIRPORTS_PER_GRF) {
		GrfMsg(1, "AirportChangeInfo: Too many airports, trying id ({}), max ({}). Ignoring.", last, NUM_AIRPORTS_PER_GRF);
		return CIR_INVALID_ID;
	}

	/* Allocate industry specs if they haven't been allocated already. */
	if (_cur_gps.grffile->airportspec.size() < last) _cur_gps.grffile->airportspec.resize(last);

	for (uint id = first; id < last; ++id) {
		auto &as = _cur_gps.grffile->airportspec[id];

		if (as == nullptr && prop != 0x08 && prop != 0x09) {
			GrfMsg(2, "AirportChangeInfo: Attempt to modify undefined airport {}, ignoring", id);
			return CIR_INVALID_ID;
		}

		switch (prop) {
			case 0x08: { // Modify original airport
				uint8_t subs_id = buf.ReadByte();
				if (subs_id == 0xFF) {
					/* Instead of defining a new airport, an airport id
					 * of 0xFF disables the old airport with the current id. */
					AirportSpec::GetWithoutOverride(id)->enabled = false;
					continue;
				} else if (subs_id >= NEW_AIRPORT_OFFSET) {
					/* The substitute id must be one of the original airports. */
					GrfMsg(2, "AirportChangeInfo: Attempt to use new airport {} as substitute airport for {}. Ignoring.", subs_id, id);
					continue;
				}

				/* Allocate space for this airport.
				 * Only need to do it once. If ever it is called again, it should not
				 * do anything */
				if (as == nullptr) {
					as = std::make_unique<AirportSpec>(*AirportSpec::GetWithoutOverride(subs_id));

					as->enabled = true;
					as->grf_prop.local_id = id;
					as->grf_prop.subst_id = subs_id;
					as->grf_prop.SetGRFFile(_cur_gps.grffile);
					/* override the default airport */
					_airport_mngr.Add(id, _cur_gps.grffile->grfid, subs_id);
				}
				break;
			}

			case 0x0A: { // Set airport layout
				uint8_t num_layouts = buf.ReadByte();
				buf.ReadDWord(); // Total size of definition, unneeded.
				uint8_t size_x = 0;
				uint8_t size_y = 0;

				std::vector<AirportTileLayout> layouts;
				layouts.reserve(num_layouts);

				for (uint8_t j = 0; j != num_layouts; ++j) {
					auto &layout = layouts.emplace_back();
					layout.rotation = static_cast<Direction>(buf.ReadByte() & 6); // Rotation can only be DIR_NORTH, DIR_EAST, DIR_SOUTH or DIR_WEST.

					for (;;) {
						auto &tile = layout.tiles.emplace_back();
						tile.ti.x = buf.ReadByte();
						tile.ti.y = buf.ReadByte();
						if (tile.ti.x == 0 && tile.ti.y == 0x80) {
							/* Convert terminator to our own. */
							tile.ti.x = -0x80;
							tile.ti.y = 0;
							tile.gfx = 0;
							break;
						}

						tile.gfx = buf.ReadByte();

						if (tile.gfx == 0xFE) {
							/* Use a new tile from this GRF */
							int local_tile_id = buf.ReadWord();

							/* Read the ID from the _airporttile_mngr. */
							uint16_t tempid = _airporttile_mngr.GetID(local_tile_id, _cur_gps.grffile->grfid);

							if (tempid == INVALID_AIRPORTTILE) {
								GrfMsg(2, "AirportChangeInfo: Attempt to use airport tile {} with airport id {}, not yet defined. Ignoring.", local_tile_id, id);
							} else {
								/* Declared as been valid, can be used */
								tile.gfx = tempid;
							}
						} else if (tile.gfx == 0xFF) {
							tile.ti.x = static_cast<int8_t>(GB(tile.ti.x, 0, 8));
							tile.ti.y = static_cast<int8_t>(GB(tile.ti.y, 0, 8));
						}

						/* Determine largest size. */
						if (layout.rotation == DIR_E || layout.rotation == DIR_W) {
							size_x = std::max<uint8_t>(size_x, tile.ti.y + 1);
							size_y = std::max<uint8_t>(size_y, tile.ti.x + 1);
						} else {
							size_x = std::max<uint8_t>(size_x, tile.ti.x + 1);
							size_y = std::max<uint8_t>(size_y, tile.ti.y + 1);
						}
					}
				}
				as->layouts = std::move(layouts);
				as->size_x = size_x;
				as->size_y = size_y;
				break;
			}

			case 0x0C:
				as->min_year = TimerGameCalendar::Year{buf.ReadWord()};
				as->max_year = TimerGameCalendar::Year{buf.ReadWord()};
				if (as->max_year == 0xFFFF) as->max_year = CalendarTime::MAX_YEAR;
				break;

			case 0x0D:
				as->ttd_airport_type = (TTDPAirportType)buf.ReadByte();
				break;

			case 0x0E:
				as->catchment = Clamp(buf.ReadByte(), 1, MAX_CATCHMENT);
				break;

			case 0x0F:
				as->noise_level = buf.ReadByte();
				break;

			case 0x10:
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &as->name);
				break;

			case 0x11: // Maintenance cost factor
				as->maintenance_cost = buf.ReadWord();
				break;

			case 0x12: // Badge list
				as->badges = ReadBadgeList(buf, GSF_AIRPORTS);
				break;

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

static ChangeInfoResult AirportTilesChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	if (last > NUM_AIRPORTTILES_PER_GRF) {
		GrfMsg(1, "AirportTileChangeInfo: Too many airport tiles loaded ({}), max ({}). Ignoring.", last, NUM_AIRPORTTILES_PER_GRF);
		return CIR_INVALID_ID;
	}

	/* Allocate airport tile specs if they haven't been allocated already. */
	if (_cur_gps.grffile->airtspec.size() < last) _cur_gps.grffile->airtspec.resize(last);

	for (uint id = first; id < last; ++id) {
		auto &tsp = _cur_gps.grffile->airtspec[id];

		if (prop != 0x08 && tsp == nullptr) {
			GrfMsg(2, "AirportTileChangeInfo: Attempt to modify undefined airport tile {}. Ignoring.", id);
			return CIR_INVALID_ID;
		}

		switch (prop) {
			case 0x08: { // Substitute airport tile type
				uint8_t subs_id = buf.ReadByte();
				if (subs_id >= NEW_AIRPORTTILE_OFFSET) {
					/* The substitute id must be one of the original airport tiles. */
					GrfMsg(2, "AirportTileChangeInfo: Attempt to use new airport tile {} as substitute airport tile for {}. Ignoring.", subs_id, id);
					continue;
				}

				/* Allocate space for this airport tile. */
				if (tsp == nullptr) {
					tsp = std::make_unique<AirportTileSpec>(*AirportTileSpec::Get(subs_id));

					tsp->enabled = true;

					tsp->animation.status = ANIM_STATUS_NO_ANIMATION;

					tsp->grf_prop.local_id = id;
					tsp->grf_prop.subst_id = subs_id;
					tsp->grf_prop.SetGRFFile(_cur_gps.grffile);
					_airporttile_mngr.AddEntityID(id, _cur_gps.grffile->grfid, subs_id); // pre-reserve the tile slot
				}
				break;
			}

			case 0x09: { // Airport tile override
				uint8_t override_id = buf.ReadByte();

				/* The airport tile being overridden must be an original airport tile. */
				if (override_id >= NEW_AIRPORTTILE_OFFSET) {
					GrfMsg(2, "AirportTileChangeInfo: Attempt to override new airport tile {} with airport tile id {}. Ignoring.", override_id, id);
					continue;
				}

				_airporttile_mngr.Add(id, _cur_gps.grffile->grfid, override_id);
				break;
			}

			case 0x0E: // Callback mask
				tsp->callback_mask = static_cast<AirportTileCallbackMasks>(buf.ReadByte());
				break;

			case 0x0F: // Animation information
				tsp->animation.frames = buf.ReadByte();
				tsp->animation.status = buf.ReadByte();
				break;

			case 0x10: // Animation speed
				tsp->animation.speed = buf.ReadByte();
				break;

			case 0x11: // Animation triggers
				tsp->animation.triggers = buf.ReadByte();
				break;

			case 0x12: // Badge list
				tsp->badges = ReadBadgeList(buf, GSF_TRAMTYPES);
				break;

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_AIRPORTS>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_AIRPORTS>::Activation(uint first, uint last, int prop, ByteReader &buf) { return AirportChangeInfo(first, last, prop, buf); }

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_AIRPORTTILES>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_AIRPORTTILES>::Activation(uint first, uint last, int prop, ByteReader &buf) { return AirportTilesChangeInfo(first, last, prop, buf); }
