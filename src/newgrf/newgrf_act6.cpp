/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_act6.cpp NewGRF Action 0x06 handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

std::map<GRFLocation, std::pair<SpriteID, uint16_t>> _grm_sprites;
GRFLineToSpriteOverride _grf_line_to_action6_sprite_override;

/* Action 0x06 */
static void CfgApply(ByteReader &buf)
{
	/* <06> <param-num> <param-size> <offset> ... <FF>
	 *
	 * B param-num     Number of parameter to substitute (First = "zero")
	 *                 Ignored if that parameter was not specified in newgrf.cfg
	 * B param-size    How many bytes to replace.  If larger than 4, the
	 *                 bytes of the following parameter are used.  In that
	 *                 case, nothing is applied unless *all* parameters
	 *                 were specified.
	 * B offset        Offset into data from beginning of next sprite
	 *                 to place where parameter is to be stored. */

	/* Preload the next sprite */
	SpriteFile &file = *_cur_gps.file;
	size_t pos = file.GetPos();
	uint32_t num = file.GetContainerVersion() >= 2 ? file.ReadDword() : file.ReadWord();
	uint8_t type = file.ReadByte();

	/* Check if the sprite is a pseudo sprite. We can't operate on real sprites. */
	if (type != 0xFF) {
		GrfMsg(2, "CfgApply: Ignoring (next sprite is real, unsupported)");

		/* Reset the file position to the start of the next sprite */
		file.SeekTo(pos, SEEK_SET);
		return;
	}

	/* Get (or create) the override for the next sprite. */
	GRFLocation location(_cur_gps.grfconfig->ident.grfid, _cur_gps.nfo_line + 1);
	std::vector<uint8_t> &preload_sprite = _grf_line_to_action6_sprite_override[location];

	/* Load new sprite data if it hasn't already been loaded. */
	if (preload_sprite.empty()) {
		preload_sprite.resize(num);
		file.ReadBlock(preload_sprite.data(), num);
	}

	/* Reset the file position to the start of the next sprite */
	file.SeekTo(pos, SEEK_SET);

	/* Now perform the Action 0x06 on our data. */
	for (;;) {
		uint i;
		uint param_num;
		uint param_size;
		uint offset;
		bool add_value;

		/* Read the parameter to apply. 0xFF indicates no more data to change. */
		param_num = buf.ReadByte();
		if (param_num == 0xFF) break;

		/* Get the size of the parameter to use. If the size covers multiple
		 * double words, sequential parameter values are used. */
		param_size = buf.ReadByte();

		/* Bit 7 of param_size indicates we should add to the original value
		 * instead of replacing it. */
		add_value  = HasBit(param_size, 7);
		param_size = GB(param_size, 0, 7);

		/* Where to apply the data to within the pseudo sprite data. */
		offset     = buf.ReadExtendedByte();

		/* If the parameter is a GRF parameter (not an internal variable) check
		 * if it (and all further sequential parameters) has been defined. */
		if (param_num < 0x80 && (param_num + (param_size - 1) / 4) >= std::size(_cur_gps.grffile->param)) {
			GrfMsg(2, "CfgApply: Ignoring (param {} not set)", (param_num + (param_size - 1) / 4));
			break;
		}

		GrfMsg(8, "CfgApply: Applying {} bytes from parameter 0x{:02X} at offset 0x{:04X}", param_size, param_num, offset);

		bool carry = false;
		for (i = 0; i < param_size && offset + i < num; i++) {
			uint32_t value = GetParamVal(param_num + i / 4, nullptr);
			/* Reset carry flag for each iteration of the variable (only really
			 * matters if param_size is greater than 4) */
			if (i % 4 == 0) carry = false;

			if (add_value) {
				uint new_value = preload_sprite[offset + i] + GB(value, (i % 4) * 8, 8) + (carry ? 1 : 0);
				preload_sprite[offset + i] = GB(new_value, 0, 8);
				/* Check if the addition overflowed */
				carry = new_value >= 256;
			} else {
				preload_sprite[offset + i] = GB(value, (i % 4) * 8, 8);
			}
		}
	}
}

template <> void GrfActionHandler<0x06>::FileScan(ByteReader &) { }
template <> void GrfActionHandler<0x06>::SafetyScan(ByteReader &) { }
template <> void GrfActionHandler<0x06>::LabelScan(ByteReader &) { }
template <> void GrfActionHandler<0x06>::Init(ByteReader &buf) { CfgApply(buf); }
template <> void GrfActionHandler<0x06>::Reserve(ByteReader &buf) { CfgApply(buf); }
template <> void GrfActionHandler<0x06>::Activation(ByteReader &buf) { CfgApply(buf); }
