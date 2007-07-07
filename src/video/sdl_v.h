/* $Id$ */

#ifndef VIDEO_SDL_H
#define VIDEO_SDL_H

#include "video_driver.hpp"

class VideoDriver_SDL: public VideoDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ void MainLoop();

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ void ToggleFullscreen(bool fullscreen);
};

class FVideoDriver_SDL: public VideoDriverFactory<FVideoDriver_SDL> {
public:
	static const int priority = 5;
	/* virtual */ const char *GetName() { return "sdl"; }
	/* virtual */ const char *GetDescription() { return "SDL Video Driver"; }
	/* virtual */ Driver *CreateInstance() { return new VideoDriver_SDL(); }
};

#endif /* VIDEO_SDL_H */
