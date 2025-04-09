/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_acta.cpp NewGRF Action 0x0A handler. */

#include "../stdafx.h"
#include "../debug.h"
#include "../spritecache.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "../table/sprites.h"

#include "../safeguards.h"

/**
 * Check if a sprite ID range is within the GRM reversed range for the currently loading NewGRF.
 * @param first_sprite First sprite of range.
 * @param num_sprites Number of sprites in the range.
 * @return True iff the NewGRF has reserved a range equal to or greater than the provided range.
 */
static bool IsGRMReservedSprite(SpriteID first_sprite, uint16_t num_sprites)
{
	for (const auto &grm_sprite : _grm_sprites) {
		if (grm_sprite.first.grfid != _cur_gps.grffile->grfid) continue;
		if (grm_sprite.second.first <= first_sprite && grm_sprite.second.first + grm_sprite.second.second >= first_sprite + num_sprites) return true;
	}
	return false;
}

/* Action 0x0A */
static void SpriteReplace(ByteReader &buf)
{
	/* <0A> <num-sets> <set1> [<set2> ...]
	 * <set>: <num-sprites> <first-sprite>
	 *
	 * B num-sets      How many sets of sprites to replace.
	 * Each set:
	 * B num-sprites   How many sprites are in this set
	 * W first-sprite  First sprite number to replace */

	uint8_t num_sets = buf.ReadByte();

	for (uint i = 0; i < num_sets; i++) {
		uint8_t num_sprites = buf.ReadByte();
		uint16_t first_sprite = buf.ReadWord();

		GrfMsg(2, "SpriteReplace: [Set {}] Changing {} sprites, beginning with {}",
			i, num_sprites, first_sprite
		);

		if (first_sprite + num_sprites >= SPR_OPENTTD_BASE) {
			/* Outside allowed range, check for GRM sprite reservations. */
			if (!IsGRMReservedSprite(first_sprite, num_sprites)) {
				GrfMsg(0, "SpriteReplace: [Set {}] Changing {} sprites, beginning with {}, above limit of {} and not within reserved range, ignoring.",
					i, num_sprites, first_sprite, SPR_OPENTTD_BASE);

				/* Load the sprites at the current location so they will do nothing instead of appearing to work. */
				first_sprite = _cur_gps.spriteid;
				_cur_gps.spriteid += num_sprites;
			}
		}

		for (uint j = 0; j < num_sprites; j++) {
			SpriteID load_index = first_sprite + j;
			_cur_gps.nfo_line++;
			LoadNextSprite(load_index, *_cur_gps.file, _cur_gps.nfo_line); // XXX

			/* Shore sprites now located at different addresses.
			 * So detect when the old ones get replaced. */
			if (IsInsideMM(load_index, SPR_ORIGINALSHORE_START, SPR_ORIGINALSHORE_END + 1)) {
				if (_loaded_newgrf_features.shore != SHORE_REPLACE_ACTION_5) _loaded_newgrf_features.shore = SHORE_REPLACE_ACTION_A;
			}
		}
	}
}

/* Action 0x0A (SKIP) */
static void SkipActA(ByteReader &buf)
{
	uint8_t num_sets = buf.ReadByte();

	for (uint i = 0; i < num_sets; i++) {
		/* Skip the sprites this replaces */
		_cur_gps.skip_sprites += buf.ReadByte();
		/* But ignore where they go */
		buf.ReadWord();
	}

	GrfMsg(3, "SkipActA: Skipping {} sprites", _cur_gps.skip_sprites);
}

template <> void GrfActionHandler<0x0A>::FileScan(ByteReader &buf) { SkipActA(buf); }
template <> void GrfActionHandler<0x0A>::SafetyScan(ByteReader &buf) { SkipActA(buf); }
template <> void GrfActionHandler<0x0A>::LabelScan(ByteReader &buf) { SkipActA(buf); }
template <> void GrfActionHandler<0x0A>::Init(ByteReader &buf) { SkipActA(buf); }
template <> void GrfActionHandler<0x0A>::Reserve(ByteReader &buf) { SkipActA(buf); }
template <> void GrfActionHandler<0x0A>::Activation(ByteReader &buf) { SpriteReplace(buf); }
