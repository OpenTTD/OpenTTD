/* $Id$ */

#ifndef VIDEO_DEDICATED_H
#define VIDEO_DEDICATED_H

#include "video_driver.hpp"

class VideoDriver_Dedicated: public VideoDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ void MainLoop();

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ bool ToggleFullscreen(bool fullscreen);
};

class FVideoDriver_Dedicated: public VideoDriverFactory<FVideoDriver_Dedicated> {
public:
	static const int priority = 0;
	/* virtual */ const char *GetName() { return "dedicated"; }
	/* virtual */ const char *GetDescription() { return "Dedicated Video Driver"; }
	/* virtual */ Driver *CreateInstance() { return new VideoDriver_Dedicated(); }
};

#endif /* VIDEO_DEDICATED_H */
