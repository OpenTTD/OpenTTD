/* $Id$ */

#ifndef MUSIC_H
#define MUSIC_H

#define NUM_SONGS_PLAYLIST 33
#define NUM_SONGS_AVAILABLE 22

typedef struct SongSpecs {
	char filename[256];
	char song_name[64];
} SongSpecs;

extern const SongSpecs origin_songs_specs[NUM_SONGS_AVAILABLE];

#endif //MUSIC_H
