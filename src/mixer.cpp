/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file mixer.cpp Mixing of sound samples. */

#include "stdafx.h"
#include <mutex>
#include <atomic>
#include "core/math_func.hpp"
#include "framerate_type.h"
#include "mixer.h"
#include "settings_type.h"

#include "safeguards.h"

struct MixerChannel {
	/* pointer to allocated buffer memory */
	int8_t *memory;

	/* current position in memory */
	uint32_t pos;
	uint32_t frac_pos;
	uint32_t frac_speed;
	uint32_t samples_left;

	/* Mixing volume */
	int volume_left;
	int volume_right;

	bool is16bit;
};

static std::atomic<uint8_t> _active_channels;
static MixerChannel _channels[8];
static uint32_t _play_rate = 11025;
static uint32_t _max_size = UINT_MAX;
static MxStreamCallback _music_stream = nullptr;
static std::mutex _music_stream_mutex;
static std::atomic<uint8_t> _effect_vol;

/**
 * The theoretical maximum volume for a single sound sample. Multiple sound
 * samples should not exceed this limit as it will sound too loud. It also
 * stops overflowing when too many sounds are played at the same time, which
 * causes an even worse sound quality.
 */
static const int MAX_VOLUME = 32767;

/**
 * Perform the rate conversion between the input and output.
 * @param b the buffer to read the data from
 * @param frac_pos the position from the begin of the buffer till the next element
 * @tparam T the size of the buffer (8 or 16 bits)
 * @return the converted value.
 */
template <typename T>
static int RateConversion(T *b, int frac_pos)
{
	return ((b[0] * ((1 << 16) - frac_pos)) + (b[1] * frac_pos)) >> 16;
}

template <typename T>
static void mix_int16(MixerChannel *sc, int16_t *buffer, uint samples, uint8_t effect_vol)
{
	/* Shift required to get sample value into range for the data type. */
	const uint SHIFT = sizeof(T) * CHAR_BIT;

	if (samples > sc->samples_left) samples = sc->samples_left;
	sc->samples_left -= samples;
	assert(samples > 0);

	const T *b = (const T *)sc->memory + sc->pos;
	uint32_t frac_pos = sc->frac_pos;
	uint32_t frac_speed = sc->frac_speed;
	int volume_left = sc->volume_left * effect_vol / 255;
	int volume_right = sc->volume_right * effect_vol / 255;

	if (frac_speed == 0x10000) {
		/* Special case when frac_speed is 0x10000 */
		do {
			buffer[0] = Clamp(buffer[0] + (*b * volume_left  >> SHIFT), -MAX_VOLUME, MAX_VOLUME);
			buffer[1] = Clamp(buffer[1] + (*b * volume_right >> SHIFT), -MAX_VOLUME, MAX_VOLUME);
			b++;
			buffer += 2;
		} while (--samples > 0);
	} else {
		do {
			int data = RateConversion(b, frac_pos);
			buffer[0] = Clamp(buffer[0] + (data * volume_left  >> SHIFT), -MAX_VOLUME, MAX_VOLUME);
			buffer[1] = Clamp(buffer[1] + (data * volume_right >> SHIFT), -MAX_VOLUME, MAX_VOLUME);
			buffer += 2;
			frac_pos += frac_speed;
			b += frac_pos >> 16;
			frac_pos &= 0xffff;
		} while (--samples > 0);
	}

	sc->frac_pos = frac_pos;
	sc->pos = b - (const T *)sc->memory;
}

static void MxCloseChannel(uint8_t channel_index)
{
	_active_channels.fetch_and(~(1 << channel_index), std::memory_order_release);
}

void MxMixSamples(void *buffer, uint samples)
{
	PerformanceMeasurer framerate(PFE_SOUND);
	static uint last_samples = 0;
	if (samples != last_samples) {
		framerate.SetExpectedRate((double)_play_rate / samples);
		last_samples = samples;
	}

	/* Clear the buffer */
	memset(buffer, 0, sizeof(int16_t) * 2 * samples);

	{
		std::lock_guard<std::mutex> lock{ _music_stream_mutex };
		/* Fetch music if a sampled stream is available */
		if (_music_stream) _music_stream((int16_t*)buffer, samples);
	}

	/* Apply simple x^3 scaling to master effect volume. This increases the
	 * perceived difference in loudness to better match expectations. effect_vol
	 * is expected to be in the range 0-127 hence the division by 127 * 127 to
	 * get back into range. */
	uint8_t effect_vol_setting = _effect_vol.load(std::memory_order_relaxed);
	uint8_t effect_vol = (effect_vol_setting *
	                    effect_vol_setting *
	                    effect_vol_setting) / (127 * 127);

	/* Mix each channel */
	uint8_t active = _active_channels.load(std::memory_order_acquire);
	for (uint8_t idx : SetBitIterator(active)) {
		MixerChannel *mc = &_channels[idx];
		if (mc->is16bit) {
			mix_int16<int16_t>(mc, (int16_t*)buffer, samples, effect_vol);
		} else {
			mix_int16<int8_t>(mc, (int16_t*)buffer, samples, effect_vol);
		}
		if (mc->samples_left == 0) MxCloseChannel(idx);
	}
}

MixerChannel *MxAllocateChannel()
{
	uint8_t currently_active = _active_channels.load(std::memory_order_acquire);
	uint8_t available = ~currently_active;
	if (available == 0) return nullptr;

	uint8_t channel_index = FindFirstBit(available);

	MixerChannel *mc = &_channels[channel_index];
	free(mc->memory);
	mc->memory = nullptr;
	return mc;
}

void MxSetChannelRawSrc(MixerChannel *mc, int8_t *mem, size_t size, uint rate, bool is16bit)
{
	mc->memory = mem;
	mc->frac_pos = 0;
	mc->pos = 0;

	mc->frac_speed = (rate << 16) / _play_rate;

	if (is16bit) size /= 2;

	/* adjust the magnitude to prevent overflow */
	while (size >= _max_size) {
		size >>= 1;
		rate = (rate >> 1) + 1;
	}

	mc->samples_left = (uint)size * _play_rate / rate;
	mc->is16bit = is16bit;
}

/**
 * Set volume and pan parameters for a sound.
 * @param mc     MixerChannel to set
 * @param volume Volume level for sound, range is 0..16384
 * @param pan    Pan position for sound, range is 0..1
 */
void MxSetChannelVolume(MixerChannel *mc, uint volume, float pan)
{
	/* Use sinusoidal pan to maintain overall sound power level regardless
	 * of position. */
	mc->volume_left = (uint)(sin((1.0 - pan) * M_PI / 2.0) * volume);
	mc->volume_right = (uint)(sin(pan * M_PI / 2.0) * volume);
}


void MxActivateChannel(MixerChannel *mc)
{
	uint8_t channel_index = mc - _channels;
	_active_channels.fetch_or((1 << channel_index), std::memory_order_release);
}

/**
 * Set source of PCM music
 * @param music_callback Function that will be called to fill sample buffers with music data.
 * @return Sample rate of mixer, which the buffers supplied to the callback must be rendered at.
 */
uint32_t MxSetMusicSource(MxStreamCallback music_callback)
{
	std::lock_guard<std::mutex> lock{ _music_stream_mutex };
	_music_stream = music_callback;
	return _play_rate;
}


bool MxInitialize(uint rate)
{
	std::lock_guard<std::mutex> lock{ _music_stream_mutex };
	_play_rate = rate;
	_max_size  = UINT_MAX / _play_rate;
	_music_stream = nullptr; /* rate may have changed, any music source is now invalid */
	return true;
}

void SetEffectVolume(uint8_t volume)
{
	_effect_vol.store(volume, std::memory_order_relaxed);
}
