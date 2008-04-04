/* $Id$ */

/** @file music.h */

#ifndef MUSIC_H
#define MUSIC_H

#define NUM_SONGS_PLAYLIST 33
#define NUM_SONGS_AVAILABLE 22

struct SongSpecs {
	char filename[MAX_PATH];
	char song_name[64];
};

extern const SongSpecs origin_songs_specs[NUM_SONGS_AVAILABLE];

#endif //MUSIC_H
