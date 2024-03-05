/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sound_opus.cpp Loading of opus sounds. */

#include "stdafx.h"
#include "random_access_file_type.h"
#include "sound_type.h"
#include "soundloader_type.h"

#include <opusfile.h>

#include "safeguards.h"

/** Opus sound loader. */
class SoundLoader_Opus : public SoundLoader {
public:
	SoundLoader_Opus() : SoundLoader("opus", "Opus sound loader", 10) {}

	static constexpr uint16_t OPUS_SAMPLE_RATE = 48000; ///< OpusFile always decodes at 48kHz.
	static constexpr uint8_t OPUS_SAMPLE_BITS = 16; ///< OpusFile op_read() uses 16 bits per sample.

	/* For good results, you will need at least 57 bytes (for a pure Opus-only stream). */
	static constexpr size_t MIN_OPUS_FILE_SIZE = 57U;

	/* It is recommended that this be large enough for at least 120 ms of data at 48 kHz per channel (5760 values per channel).
	 * Smaller buffers will simply return less data, possibly consuming more memory to buffer the data internally. */
	static constexpr size_t DECODE_BUFFER_SAMPLES = 5760 * 2;
	static constexpr size_t DECODE_BUFFER_BYTES = DECODE_BUFFER_SAMPLES * sizeof(opus_int16);

	bool Load(SoundEntry &sound, bool new_format, std::vector<uint8_t> &data) override
	{
		if (!new_format) return false;

		/* At least 57 bytes are needed for an Opus-only file. */
		if (sound.file_size < MIN_OPUS_FILE_SIZE) return false;

		/* Test if data is an Ogg Opus stream, as identified by the initial file header. */
		auto filepos = sound.file->GetPos();
		std::vector<uint8_t> tmp(MIN_OPUS_FILE_SIZE);
		sound.file->ReadBlock(tmp.data(), tmp.size());
		if (op_test(nullptr, tmp.data(), tmp.size()) != 0) return false;

		/* Read the whole file into memory. */
		tmp.resize(sound.file_size);
		sound.file->SeekTo(filepos, SEEK_SET);
		sound.file->ReadBlock(tmp.data(), tmp.size());

		int error = 0;
		auto of = std::unique_ptr<OggOpusFile, OggOpusFileDeleter>(op_open_memory(tmp.data(), tmp.size(), &error));
		if (error != 0) return false;

		size_t datapos = 0;
		for (;;) {
			data.resize(datapos + DECODE_BUFFER_BYTES);

			int link_index;
			int read = op_read(of.get(), reinterpret_cast<opus_int16 *>(&data[datapos]), DECODE_BUFFER_BYTES, &link_index);
			if (read == 0) break;

			if (read < 0 || op_channel_count(of.get(), link_index) != 1) {
				/* Error reading, or incorrect channel count. */
				data.clear();
				return false;
			}

			datapos += read * sizeof(opus_int16);
		}

		/* OpusFile always decodes at 48kHz. */
		sound.channels = 1;
		sound.bits_per_sample = OPUS_SAMPLE_BITS;
		sound.rate = OPUS_SAMPLE_RATE;

		/* We resized by DECODE_BUFFER_BYTES just before finally reading zero bytes, undo this. */
		data.resize(data.size() - DECODE_BUFFER_BYTES);

		return true;
	}

private:
	/** Helper class to RAII release an OggOpusFile. */
	struct OggOpusFileDeleter {
		void operator()(OggOpusFile *of)
		{
			if (of != nullptr) op_free(of);
		}
	};
};

static SoundLoader_Opus s_sound_loader_opus;
