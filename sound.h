#ifndef SOUND_H
#define SOUND_H

typedef struct Mixer Mixer;

typedef struct MusicFileSettings {
	byte playlist;
	byte music_vol;
	byte effect_vol;
	byte custom_1[33];
	byte custom_2[33];
	bool btn_down;
	bool shuffle;
} MusicFileSettings;

VARDEF byte _music_wnd_cursong;
VARDEF bool _song_is_active;
VARDEF byte _cur_playlist[33];
VARDEF MusicFileSettings msf;
VARDEF Mixer *_mixer;

bool MxInitialize(uint rate, const char *filename);
void MxMixSamples(Mixer *mx, void *buffer, uint samples);

#endif /* SOUND_H */
