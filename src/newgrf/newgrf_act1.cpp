/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act1.cpp NewGRF Action 0x01 handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "../spritecache.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

/* Action 0x01 */
static void NewSpriteSet(ByteReader &buf)
{
	/* Basic format:    <01> <feature> <num-sets> <num-ent>
	 * Extended format: <01> <feature> 00 <first-set> <num-sets> <num-ent>
	 *
	 * B feature       feature to define sprites for
	 *                 0, 1, 2, 3: veh-type, 4: train stations
	 * E first-set     first sprite set to define
	 * B num-sets      number of sprite sets (extended byte in extended format)
	 * E num-ent       how many entries per sprite set
	 *                 For vehicles, this is the number of different
	 *                         vehicle directions in each sprite set
	 *                         Set num-dirs=8, unless your sprites are symmetric.
	 *                         In that case, use num-dirs=4.
	 */

	GrfSpecFeature feature{buf.ReadByte()};
	uint16_t num_sets  = buf.ReadByte();
	uint16_t first_set = 0;

	if (num_sets == 0 && buf.HasData(3)) {
		/* Extended Action1 format.
		 * Some GRFs define zero sets of zero sprites, though there is actually no use in that. Ignore them. */
		first_set = buf.ReadExtendedByte();
		num_sets = buf.ReadExtendedByte();
	}
	uint16_t num_ents = buf.ReadExtendedByte();

	if (feature >= GSF_END) {
		_cur_gps.skip_sprites = num_sets * num_ents;
		GrfMsg(1, "NewSpriteSet: Unsupported feature 0x{:02X}, skipping {} sprites", feature, _cur_gps.skip_sprites);
		return;
	}

	_cur_gps.AddSpriteSets(feature, _cur_gps.spriteid, first_set, num_sets, num_ents);

	GrfMsg(7, "New sprite set at {} of feature 0x{:02X}, consisting of {} sets with {} views each (total {})",
		_cur_gps.spriteid, feature, num_sets, num_ents, num_sets * num_ents
	);

	for (int i = 0; i < num_sets * num_ents; i++) {
		_cur_gps.nfo_line++;
		LoadNextSprite(_cur_gps.spriteid++, *_cur_gps.file, _cur_gps.nfo_line);
	}
}

/* Action 0x01 (SKIP) */
static void SkipAct1(ByteReader &buf)
{
	buf.ReadByte();
	uint16_t num_sets  = buf.ReadByte();

	if (num_sets == 0 && buf.HasData(3)) {
		/* Extended Action1 format.
		 * Some GRFs define zero sets of zero sprites, though there is actually no use in that. Ignore them. */
		buf.ReadExtendedByte(); // first_set
		num_sets = buf.ReadExtendedByte();
	}
	uint16_t num_ents = buf.ReadExtendedByte();

	_cur_gps.skip_sprites = num_sets * num_ents;

	GrfMsg(3, "SkipAct1: Skipping {} sprites", _cur_gps.skip_sprites);
}

template <> void GrfActionHandler<0x01>::FileScan(ByteReader &buf) { SkipAct1(buf); }
template <> void GrfActionHandler<0x01>::SafetyScan(ByteReader &buf) { SkipAct1(buf); }
template <> void GrfActionHandler<0x01>::LabelScan(ByteReader &buf) { SkipAct1(buf); }
template <> void GrfActionHandler<0x01>::Init(ByteReader &buf) { SkipAct1(buf); }
template <> void GrfActionHandler<0x01>::Reserve(ByteReader &buf) { SkipAct1(buf); }
template <> void GrfActionHandler<0x01>::Activation(ByteReader &buf) { NewSpriteSet(buf); }
