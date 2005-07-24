/* $Id$ */

#ifndef MIXER_H
#define MIXER_H

typedef struct Mixer Mixer;
typedef struct MixerChannel MixerChannel;

enum {
	MX_AUTOFREE = 1,
//	MX_8BIT = 2,
//	MX_STEREO = 4,
//	MX_UNSIGNED = 8,
};

VARDEF Mixer *_mixer;

bool MxInitialize(uint rate);
void MxMixSamples(Mixer *mx, void *buffer, uint samples);

MixerChannel *MxAllocateChannel(Mixer *mx);
void MxSetChannelRawSrc(MixerChannel *mc, int8 *mem, uint size, uint rate, uint flags);
void MxSetChannelVolume(MixerChannel *mc, uint left, uint right);
void MxActivateChannel(MixerChannel*);

#endif
