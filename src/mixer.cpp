/* $Id$ */

/** @file mixer.cpp*/

#include "stdafx.h"
#include "openttd.h"
#include "mixer.h"

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
	uint volume_left;
	uint volume_right;

	uint flags;
};

static MixerChannel _channels[8];
static uint32 _play_rate;


static void mix_int8_to_int16(MixerChannel *sc, int16 *buffer, uint samples)
{
	int8 *b;
	uint32 frac_pos;
	uint32 frac_speed;
	uint volume_left;
	uint volume_right;

	if (samples > sc->samples_left) samples = sc->samples_left;
	sc->samples_left -= samples;
	assert(samples > 0);

	b = sc->memory + sc->pos;
	frac_pos = sc->frac_pos;
	frac_speed = sc->frac_speed;
	volume_left = sc->volume_left;
	volume_right = sc->volume_right;

	if (frac_speed == 0x10000) {
		/* Special case when frac_speed is 0x10000 */
		do {
			buffer[0] += *b * volume_left >> 8;
			buffer[1] += *b * volume_right >> 8;
			b++;
			buffer += 2;
		} while (--samples > 0);
	} else {
		do {
			buffer[0] += *b * volume_left >> 8;
			buffer[1] += *b * volume_right >> 8;
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
	if (mc->flags & MX_AUTOFREE) free(mc->memory);
	mc->active = false;
	mc->memory = NULL;
}

void MxMixSamples(void *buffer, uint samples)
{
	MixerChannel *mc;

	/* Clear the buffer */
	memset(buffer, 0, sizeof(int16) * 2 * samples);

	/* Mix each channel */
	for (mc = _channels; mc != endof(_channels); mc++) {
		if (mc->active) {
			mix_int8_to_int16(mc, (int16*)buffer, samples);
			if (mc->samples_left == 0) MxCloseChannel(mc);
		}
	}
}

MixerChannel *MxAllocateChannel(void)
{
	MixerChannel *mc;
	for (mc = _channels; mc != endof(_channels); mc++)
		if (mc->memory == NULL) {
			mc->active = false;
			return mc;
		}
	return NULL;
}

void MxSetChannelRawSrc(MixerChannel *mc, int8 *mem, uint size, uint rate, uint flags)
{
	mc->memory = mem;
	mc->flags = flags;
	mc->frac_pos = 0;
	mc->pos = 0;

	mc->frac_speed = (rate << 16) / _play_rate;

	/* adjust the magnitude to prevent overflow */
	while (size & 0xFFFF0000) {
		size >>= 1;
		rate = (rate >> 1) + 1;
	}

	mc->samples_left = size * _play_rate / rate;
}

void MxSetChannelVolume(MixerChannel *mc, uint left, uint right)
{
	mc->volume_left = left;
	mc->volume_right = right;
}


void MxActivateChannel(MixerChannel* mc)
{
	mc->active = true;
}


bool MxInitialize(uint rate)
{
	_play_rate = rate;
	return true;
}
