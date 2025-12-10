/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act0_cargo.cpp NewGRF Action 0x00 handler for cargo. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_cargo.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"
#include "newgrf_stringmapping.h"

#include "../safeguards.h"

/**
 * Define properties for cargoes
 * @param first ID of the first cargo.
 * @param last ID of the last cargo.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult CargoReserveInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;

	if (last > NUM_CARGO) {
		GrfMsg(2, "CargoChangeInfo: Cargo type {} out of range (max {})", last, NUM_CARGO - 1);
		return CIR_INVALID_ID;
	}

	for (uint id = first; id < last; ++id) {
		CargoSpec *cs = CargoSpec::Get(id);

		switch (prop) {
			case 0x08: // Bit number of cargo
				cs->bitnum = buf.ReadByte();
				if (cs->IsValid()) {
					cs->grffile = _cur_gps.grffile;
					SetBit(_cargo_mask, id);
				} else {
					ClrBit(_cargo_mask, id);
				}
				BuildCargoLabelMap();
				break;

			case 0x09: // String ID for cargo type name
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &cs->name);
				break;

			case 0x0A: // String for 1 unit of cargo
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &cs->name_single);
				break;

			case 0x0B: // String for singular quantity of cargo (e.g. 1 tonne of coal)
			case 0x1B: // String for cargo units
				/* String for units of cargo. This is different in OpenTTD
				 * (e.g. tonnes) to TTDPatch (e.g. {COMMA} tonne of coal).
				 * Property 1B is used to set OpenTTD's behaviour. */
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &cs->units_volume);
				break;

			case 0x0C: // String for plural quantity of cargo (e.g. 10 tonnes of coal)
			case 0x1C: // String for any amount of cargo
				/* Strings for an amount of cargo. This is different in OpenTTD
				 * (e.g. {WEIGHT} of coal) to TTDPatch (e.g. {COMMA} tonnes of coal).
				 * Property 1C is used to set OpenTTD's behaviour. */
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &cs->quantifier);
				break;

			case 0x0D: // String for two letter cargo abbreviation
				AddStringForMapping(GRFStringID{buf.ReadWord()}, &cs->abbrev);
				break;

			case 0x0E: // Sprite ID for cargo icon
				cs->sprite = buf.ReadWord();
				break;

			case 0x0F: // Weight of one unit of cargo
				cs->weight = buf.ReadByte();
				break;

			case 0x10: // Used for payment calculation
				cs->transit_periods[0] = buf.ReadByte();
				break;

			case 0x11: // Used for payment calculation
				cs->transit_periods[1] = buf.ReadByte();
				break;

			case 0x12: // Base cargo price
				cs->initial_payment = buf.ReadDWord();
				break;

			case 0x13: // Colour for station rating bars
				cs->rating_colour = PixelColour{buf.ReadByte()};
				break;

			case 0x14: // Colour for cargo graph
				cs->legend_colour = PixelColour{buf.ReadByte()};
				break;

			case 0x15: // Freight status
				cs->is_freight = (buf.ReadByte() != 0);
				break;

			case 0x16: // Cargo classes
				cs->classes = CargoClasses{buf.ReadWord()};
				break;

			case 0x17: // Cargo label
				cs->label = CargoLabel{std::byteswap(buf.ReadDWord())};
				BuildCargoLabelMap();
				break;

			case 0x18: { // Town growth substitute type
				uint8_t substitute_type = buf.ReadByte();

				switch (substitute_type) {
					case 0x00: cs->town_acceptance_effect = TAE_PASSENGERS; break;
					case 0x02: cs->town_acceptance_effect = TAE_MAIL; break;
					case 0x05: cs->town_acceptance_effect = TAE_GOODS; break;
					case 0x09: cs->town_acceptance_effect = TAE_WATER; break;
					case 0x0B: cs->town_acceptance_effect = TAE_FOOD; break;
					default:
						GrfMsg(1, "CargoChangeInfo: Unknown town growth substitute value {}, setting to none.", substitute_type);
						[[fallthrough]];
					case 0xFF: cs->town_acceptance_effect = TAE_NONE; break;
				}
				break;
			}

			case 0x19: // Town growth coefficient
				buf.ReadWord();
				break;

			case 0x1A: // Bitmask of callbacks to use
				cs->callback_mask = static_cast<CargoCallbackMasks>(buf.ReadByte());
				break;

			case 0x1D: // Vehicle capacity multiplier
				cs->multiplier = std::max<uint16_t>(1u, buf.ReadWord());
				break;

			case 0x1E: { // Town production substitute type
				uint8_t substitute_type = buf.ReadByte();

				switch (substitute_type) {
					case 0x00: cs->town_production_effect = TPE_PASSENGERS; break;
					case 0x02: cs->town_production_effect = TPE_MAIL; break;
					default:
						GrfMsg(1, "CargoChangeInfo: Unknown town production substitute value {}, setting to none.", substitute_type);
						[[fallthrough]];
					case 0xFF: cs->town_production_effect = TPE_NONE; break;
				}
				break;
			}

			case 0x1F: // Town production multiplier
				cs->town_production_multiplier = std::max<uint16_t>(1U, buf.ReadWord());
				break;

			default:
				ret = CIR_UNKNOWN;
				break;
		}
	}

	return ret;
}

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_CARGOES>::Reserve(uint first, uint last, int prop, ByteReader &buf) { return CargoReserveInfo(first, last, prop, buf); }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_CARGOES>::Activation(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
