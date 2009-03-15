/* $Id$ */

/** @file music.h Base for the music handling. */

#ifndef MUSIC_H
#define MUSIC_H

#define NUM_SONGS_PLAYLIST 33
#define NUM_SONGS_AVAILABLE 22

struct SongSpecs {
	char filename[MAX_PATH];
	char song_name[64];
};

extern const SongSpecs _origin_songs_specs[];

#endif /* MUSIC_H */
