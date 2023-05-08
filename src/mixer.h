/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file mixer.h Functions to mix sound samples. */

#ifndef MIXER_H
#define MIXER_H

struct MixerChannel;

/**
 * Type of callback functions for supplying PCM music.
 * A music decoder/renderer implements this function and installs it with MxSetMusicSource, which also returns the sample rate used.
 * @param buffer Pointer to interleaved 2-channel signed 16 bit PCM data buffer, guaranteed to be 0-initialized.
 * @param samples number of samples that must be filled into \c buffer.
 */
typedef void(*MxStreamCallback)(int16_t *buffer, size_t samples);

bool MxInitialize(uint rate);
void MxMixSamples(void *buffer, uint samples);

MixerChannel *MxAllocateChannel();
void MxSetChannelRawSrc(MixerChannel *mc, int8_t *mem, size_t size, uint rate, bool is16bit);
void MxSetChannelVolume(MixerChannel *mc, uint volume, float pan);
void MxActivateChannel(MixerChannel*);

uint32_t MxSetMusicSource(MxStreamCallback music_callback);

void SetEffectVolume(uint8_t volume);

#endif /* MIXER_H */
