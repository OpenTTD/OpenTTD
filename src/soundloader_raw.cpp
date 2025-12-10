/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file soundloader_raw.cpp Loading of raw sounds. */

#include "stdafx.h"
#include "random_access_file_type.h"
#include "sound_type.h"
#include "soundloader_type.h"

#include "safeguards.h"

/** Raw PCM sound loader, used as a fallback if other sound loaders fail. */
class SoundLoader_Raw : public SoundLoader {
public:
	SoundLoader_Raw() : SoundLoader("raw", "Raw PCM loader", INT_MAX) {}

	static constexpr uint16_t RAW_SAMPLE_RATE = 11025; ///< Sample rate of raw pcm samples.
	static constexpr uint8_t RAW_SAMPLE_BITS = 8; ///< Bit depths of raw pcm samples.

	bool Load(SoundEntry &sound, bool new_format, std::vector<std::byte> &data) const override
	{
		/* Raw sounds are apecial case for the jackhammer sound (name in Windows sample.cat is "Corrupt sound")
		 * It's not a RIFF file, but raw PCM data.
		 * We no longer compare by name as the same file in the DOS sample.cat does not have a unique name. */

		/* Raw sounds are not permitted in a new format file. */
		if (new_format) return false;

		sound.channels = 1;
		sound.rate = RAW_SAMPLE_RATE;
		sound.bits_per_sample = RAW_SAMPLE_BITS;

		/* Allocate an extra sample to ensure the runtime resampler doesn't go out of bounds.*/
		data.reserve(sound.file_size + 1);
		data.resize(sound.file_size);
		sound.file->ReadBlock(std::data(data), std::size(data));

		/* Convert 8-bit samples from unsigned to signed. */
		for (auto &sample : data) {
			sample ^= std::byte{0x80};
		}

		return true;
	}

private:
	static SoundLoader_Raw instance;
};

/* static */ SoundLoader_Raw SoundLoader_Raw::instance{};
