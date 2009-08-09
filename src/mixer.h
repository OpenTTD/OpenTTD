/* $Id$ */

/** @file mixer.h Functions to mix sound samples. */

#ifndef MIXER_H
#define MIXER_H

struct MixerChannel;

bool MxInitialize(uint rate);
void MxMixSamples(void *buffer, uint samples);

MixerChannel *MxAllocateChannel();
void MxSetChannelRawSrc(MixerChannel *mc, int8 *mem, size_t size, uint rate, bool is16bit);
void MxSetChannelVolume(MixerChannel *mc, uint left, uint right);
void MxActivateChannel(MixerChannel*);

#endif /* MIXER_H */
