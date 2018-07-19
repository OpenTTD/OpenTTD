/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file mixer.cpp Mixing of sound samples. */

#include "stdafx.h"
#include <math.h>
#include "core/math_func.hpp"
#include "framerate_type.h"

#include "safeguards.h"

struct MixerChannel {
	bool active;

	/* pointer to allocated buffer memory */
	int8 *memory;

	/* current position in memory */
	uint32 pos;
	uint32 frac_pos;
	uint32 frac_speed;
	uint32 samples_left;

	/* Mixing volume */
	int volume_left;
	int volume_right;

	bool is16bit;
};

static MixerChannel _channels[8];
static uint32 _play_rate = 11025;
static uint32 _max_size = UINT_MAX;

/**
 * The theoretical maximum volume for a single sound sample. Multiple sound
 * samples should not exceed this limit as it will sound too loud. It also
 * stops overflowing when too many sounds are played at the same time, which
 * causes an even worse sound quality.
 */
static const int MAX_VOLUME = 128 * 128;

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

static void mix_int16(MixerChannel *sc, int16 *buffer, uint samples)
{
	if (samples > sc->samples_left) samples = sc->samples_left;
	sc->samples_left -= samples;
	assert(samples > 0);

	const int16 *b = (const int16 *)sc->memory + sc->pos;
	uint32 frac_pos = sc->frac_pos;
	uint32 frac_speed = sc->frac_speed;
	int volume_left = sc->volume_left;
	int volume_right = sc->volume_right;

	if (frac_speed == 0x10000) {
		/* Special case when frac_speed is 0x10000 */
		do {
			buffer[0] = Clamp(buffer[0] + (*b * volume_left  >> 16), -MAX_VOLUME, MAX_VOLUME);
			buffer[1] = Clamp(buffer[1] + (*b * volume_right >> 16), -MAX_VOLUME, MAX_VOLUME);
			b++;
			buffer += 2;
		} while (--samples > 0);
	} else {
		do {
			int data = RateConversion(b, frac_pos);
			buffer[0] = Clamp(buffer[0] + (data * volume_left  >> 16), -MAX_VOLUME, MAX_VOLUME);
			buffer[1] = Clamp(buffer[1] + (data * volume_right >> 16), -MAX_VOLUME, MAX_VOLUME);
			buffer += 2;
			frac_pos += frac_speed;
			b += frac_pos >> 16;
			frac_pos &= 0xffff;
		} while (--samples > 0);
	}

	sc->frac_pos = frac_pos;
	sc->pos = b - (const int16 *)sc->memory;
}

static void mix_int8_to_int16(MixerChannel *sc, int16 *buffer, uint samples)
{
	if (samples > sc->samples_left) samples = sc->samples_left;
	sc->samples_left -= samples;
	assert(samples > 0);

	const int8 *b = sc->memory + sc->pos;
	uint32 frac_pos = sc->frac_pos;
	uint32 frac_speed = sc->frac_speed;
	int volume_left = sc->volume_left;
	int volume_right = sc->volume_right;

	if (frac_speed == 0x10000) {
		/* Special case when frac_speed is 0x10000 */
		do {
			buffer[0] = Clamp(buffer[0] + (*b * volume_left  >> 8), -MAX_VOLUME, MAX_VOLUME);
			buffer[1] = Clamp(buffer[1] + (*b * volume_right >> 8), -MAX_VOLUME, MAX_VOLUME);
			b++;
			buffer += 2;
		} while (--samples > 0);
	} else {
		do {
			int data = RateConversion(b, frac_pos);
			buffer[0] = Clamp(buffer[0] + (data * volume_left  >> 8), -MAX_VOLUME, MAX_VOLUME);
			buffer[1] = Clamp(buffer[1] + (data * volume_right >> 8), -MAX_VOLUME, MAX_VOLUME);
			buffer += 2;
			frac_pos += frac_speed;
			b += frac_pos >> 16;
			frac_pos &= 0xffff;
		} while (--samples > 0);
	}

	sc->frac_pos = frac_pos;
	sc->pos = b - sc->memory;
}

static void MxCloseChannel(MixerChannel *mc)
{
	mc->active = false;
}

void MxMixSamples(void *buffer, uint samples)
{
	PerformanceMeasurer framerate(PFE_SOUND);
	static uint last_samples = 0;
	if (samples != last_samples) {
		framerate.SetExpectedRate((double)_play_rate / samples);
		last_samples = samples;
	}

	MixerChannel *mc;

	/* Clear the buffer */
	memset(buffer, 0, sizeof(int16) * 2 * samples);

	/* Mix each channel */
	for (mc = _channels; mc != endof(_channels); mc++) {
		if (mc->active) {
			if (mc->is16bit) {
				mix_int16(mc, (int16*)buffer, samples);
			} else {
				mix_int8_to_int16(mc, (int16*)buffer, samples);
			}
			if (mc->samples_left == 0) MxCloseChannel(mc);
		}
	}
}

MixerChannel *MxAllocateChannel()
{
	MixerChannel *mc;
	for (mc = _channels; mc != endof(_channels); mc++) {
		if (!mc->active) {
			free(mc->memory);
			mc->memory = NULL;
			return mc;
		}
	}
	return NULL;
}

void MxSetChannelRawSrc(MixerChannel *mc, int8 *mem, size_t size, uint rate, bool is16bit)
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
	mc->active = true;
}


bool MxInitialize(uint rate)
{
	_play_rate = rate;
	_max_size  = UINT_MAX / _play_rate;
	return true;
}
