/* $Id$ */

#include "../stdafx.h"
#include "null_m.h"

static const char* NullMidiStart(const char* const* parm) { return NULL; }
static void NullMidiStop() {}
static void NullMidiPlaySong(const char *filename) {}
static void NullMidiStopSong() {}
static bool NullMidiIsSongPlaying() { return true; }
static void NullMidiSetVolume(byte vol) {}

const HalMusicDriver _null_music_driver = {
	NullMidiStart,
	NullMidiStop,
	NullMidiPlaySong,
	NullMidiStopSong,
	NullMidiIsSongPlaying,
	NullMidiSetVolume,
};
