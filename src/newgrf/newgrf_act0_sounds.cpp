/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_act0_sounds.cpp NewGRF Action 0x00 handler for sounds. */

#include "../stdafx.h"
#include "../debug.h"
#include "../newgrf_sound.h"
#include "newgrf_bytereader.h"
#include "newgrf_internal.h"

#include "../safeguards.h"

/**
 * Define properties for sound effects
 * @param first Local ID of the first sound.
 * @param last Local ID of the last sound.
 * @param prop The property to change.
 * @param buf The property value.
 * @return ChangeInfoResult.
 */
static ChangeInfoResult SoundEffectChangeInfo(uint first, uint last, int prop, ByteReader &buf)
{
	ChangeInfoResult ret = CIR_SUCCESS;
	if (first == last) return ret;

	if (_cur_gps.grffile->sound_offset == 0) {
		GrfMsg(1, "SoundEffectChangeInfo: No effects defined, skipping");
		return CIR_INVALID_ID;
	}

	if (last - ORIGINAL_SAMPLE_COUNT > _cur_gps.grffile->num_sounds) {
		GrfMsg(1, "SoundEffectChangeInfo: Attempting to change undefined sound effect ({}), max ({}). Ignoring.", last, ORIGINAL_SAMPLE_COUNT + _cur_gps.grffile->num_sounds);
		return CIR_INVALID_ID;
	}

	for (uint id = first; id < last; ++id) {
		SoundEntry *sound = GetSound(first + _cur_gps.grffile->sound_offset - ORIGINAL_SAMPLE_COUNT);

		switch (prop) {
			case 0x08: // Relative volume
				sound->volume = Clamp(buf.ReadByte(), 0, SOUND_EFFECT_MAX_VOLUME);
				break;

			case 0x09: // Priority
				sound->priority = buf.ReadByte();
				break;

			case 0x0A: { // Override old sound
				SoundID orig_sound = buf.ReadByte();

				if (orig_sound >= ORIGINAL_SAMPLE_COUNT) {
					GrfMsg(1, "SoundEffectChangeInfo: Original sound {} not defined (max {})", orig_sound, ORIGINAL_SAMPLE_COUNT);
				} else {
					SoundEntry *old_sound = GetSound(orig_sound);

					/* Literally copy the data of the new sound over the original */
					*old_sound = *sound;
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

template <> ChangeInfoResult GrfChangeInfoHandler<GSF_SOUNDFX>::Reserve(uint, uint, int, ByteReader &) { return CIR_UNHANDLED; }
template <> ChangeInfoResult GrfChangeInfoHandler<GSF_SOUNDFX>::Activation(uint first, uint last, int prop, ByteReader &buf) { return SoundEffectChangeInfo(first, last, prop, buf); }
