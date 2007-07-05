/* $Id$ */

#ifndef MUSIC_LIBTIMIDITY_H
#define MUSIC_LIBTIMIDITY_H

#include "music_driver.hpp"

class MusicDriver_LibTimidity: public MusicDriver {
public:
	/* virtual */ bool CanProbe() { return true; }

	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void PlaySong(const char *filename);

	/* virtual */ void StopSong();

	/* virtual */ bool IsSongPlaying();

	/* virtual */ void SetVolume(byte vol);
};

class FMusicDriver_LibTimidity: public MusicDriverFactory<FMusicDriver_LibTimidity> {
public:
	/* virtual */ const char *GetName() { return "libtimidity"; }
	/* virtual */ const char *GetDescription() { return "LibTimidity MIDI Driver"; }
	/* virtual */ Driver *CreateInstance() { return new MusicDriver_LibTimidity(); }
};

#endif /* MUSIC_LIBTIMIDITY_H */
