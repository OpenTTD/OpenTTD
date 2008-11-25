/* $Id$ */

/** @file allegro_v.h Base of the Allegro video driver. */

#ifndef VIDEO_ALLEGRO_H
#define VIDEO_ALLEGRO_H

#include "video_driver.hpp"

class VideoDriver_Allegro: public VideoDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ void MainLoop();

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ bool ToggleFullscreen(bool fullscreen);
};

class FVideoDriver_Allegro: public VideoDriverFactory<FVideoDriver_Allegro> {
public:
	static const int priority = 5;
	/* virtual */ const char *GetName() { return "allegro"; }
	/* virtual */ const char *GetDescription() { return "Allegro Video Driver"; }
	/* virtual */ Driver *CreateInstance() { return new VideoDriver_Allegro(); }
};

#endif /* VIDEO_ALLEGRO_H */
