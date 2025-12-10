/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act12.cpp NewGRF Action 0x12 handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "../fontcache.h"
#include "../spritecache.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

/** Action 0x12 */
static void LoadFontGlyph(ByteReader &buf)
{
	/* <12> <num_def> <font_size> <num_char> <base_char>
	 *
	 * B num_def      Number of definitions
	 * B font_size    Size of font (0 = normal, 1 = small, 2 = large, 3 = mono)
	 * B num_char     Number of consecutive glyphs
	 * W base_char    First character index */

	uint8_t num_def = buf.ReadByte();

	for (uint i = 0; i < num_def; i++) {
		FontSize size    = (FontSize)buf.ReadByte();
		uint8_t  num_char  = buf.ReadByte();
		uint16_t base_char = buf.ReadWord();

		if (size >= FS_END) {
			GrfMsg(1, "LoadFontGlyph: Size {} is not supported, ignoring", size);
		}

		GrfMsg(7, "LoadFontGlyph: Loading {} glyph(s) at 0x{:04X} for size {}", num_char, base_char, size);

		for (uint c = 0; c < num_char; c++) {
			if (size < FS_END) SetUnicodeGlyph(size, base_char + c, _cur_gps.spriteid);
			_cur_gps.nfo_line++;
			LoadNextSprite(_cur_gps.spriteid++, *_cur_gps.file, _cur_gps.nfo_line);
		}
	}
}

/** Action 0x12 (SKIP) */
static void SkipAct12(ByteReader &buf)
{
	/* <12> <num_def> <font_size> <num_char> <base_char>
	 *
	 * B num_def      Number of definitions
	 * B font_size    Size of font (0 = normal, 1 = small, 2 = large)
	 * B num_char     Number of consecutive glyphs
	 * W base_char    First character index */

	uint8_t num_def = buf.ReadByte();

	for (uint i = 0; i < num_def; i++) {
		/* Ignore 'size' byte */
		buf.ReadByte();

		/* Sum up number of characters */
		_cur_gps.skip_sprites += buf.ReadByte();

		/* Ignore 'base_char' word */
		buf.ReadWord();
	}

	GrfMsg(3, "SkipAct12: Skipping {} sprites", _cur_gps.skip_sprites);
}

template <> void GrfActionHandler<0x12>::FileScan(ByteReader &buf) { SkipAct12(buf); }
template <> void GrfActionHandler<0x12>::SafetyScan(ByteReader &buf) { SkipAct12(buf); }
template <> void GrfActionHandler<0x12>::LabelScan(ByteReader &buf) { SkipAct12(buf); }
template <> void GrfActionHandler<0x12>::Init(ByteReader &buf) { SkipAct12(buf); }
template <> void GrfActionHandler<0x12>::Reserve(ByteReader &buf) { SkipAct12(buf); }
template <> void GrfActionHandler<0x12>::Activation(ByteReader &buf) { LoadFontGlyph(buf); }
