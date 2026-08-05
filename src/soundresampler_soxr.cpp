/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file soundresampler_soxr.cpp SOXR sound resampler. */

#include "stdafx.h"
#include "core/math_func.hpp"
#include "debug.h"
#include "sound_type.h"
#include "soundresampler_type.h"

#include <soxr.h>

#include "safeguards.h"

/** Soxr SoundResampler implementation. */
class SoundResampler_Soxr : public SoundResampler {
public:
	/** Create this SoundResample_Soxr instance. */
	SoundResampler_Soxr() : SoundResampler("soxr", "SOXR sound resampler", 0) {}

	/**
	 * Convert samples from 8 bits to 16 bits.
	 * @param in Span of samples to convert.
	 * @param out Span to place converted samples.
	 * @pre \c out must be exactly twice the size of \c in.
	 */
	static void ConvertInt8toInt16(std::span<const std::byte> in, std::span<std::byte> out)
	{
		assert(std::size(out) == std::size(in) * 2);

		auto out_it = std::begin(out);
		for (const std::byte &value : in) {
			if constexpr (std::endian::native != std::endian::little) {
				*out_it++ = value;
				*out_it++ = std::byte{0};
			} else {
				*out_it++ = std::byte{0};
				*out_it++ = value;
			}
		}
	}

	bool Resample(SoundEntry &sound, uint32_t play_rate) const override
	{
		/* The sound data to work on. */
		std::vector<std::byte> &data = *sound.data;

		std::vector<std::byte> tmp;
		if (sound.bits_per_sample == 16) {
			/* No conversion necessary so just move from sound data to temporary buffer. */
			data.swap(tmp);
		} else {
			/* SoxR cannot resample 8-bit audio, so convert from 8-bit to 16-bit into temporary buffer. */
			tmp.resize(std::size(data) * sizeof(int16_t));
			ConvertInt8toInt16(data, tmp);
			sound.bits_per_sample = 16;
		}

		/* Resize buffer ensuring it is correctly aligned. */
		uint align = sound.channels * sound.bits_per_sample / 8;
		data.resize(Align(std::size(tmp) * play_rate / sound.rate, align));

		soxr_error_t error = soxr_oneshot(sound.rate, play_rate, sound.channels,
			std::data(tmp), std::size(tmp) / align, nullptr,
			std::data(data), std::size(data) / align, nullptr,
			&this->io, &this->quality, &this->runtime);

		if (error != nullptr) {
			/* Could not resample, try using the original data as-is without resampling instead. */
			Debug(misc, 0, "Failed to resample: {}", soxr_strerror(error));
			data.swap(tmp);
		} else {
			sound.rate = play_rate;
		}

		return true;
	}

private:
	static SoundResampler_Soxr instance; ///< Instance of this SoundResampler.

	const soxr_io_spec_t io = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I); ///< Input/output settings.
	const soxr_quality_spec_t quality = soxr_quality_spec(SOXR_VHQ, 0); ///< Quality settings.
	const soxr_runtime_spec_t runtime = soxr_runtime_spec(1); ///< Runtime settings.
};

/* static */ SoundResampler_Soxr SoundResampler_Soxr::instance{};
