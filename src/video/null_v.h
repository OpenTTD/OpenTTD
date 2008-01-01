/* $Id$ */

#ifndef VIDEO_NULL_H
#define VIDEO_NULL_H

#include "video_driver.hpp"

class VideoDriver_Null: public VideoDriver {
private:
	uint ticks;

public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ void MainLoop();

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ bool ToggleFullscreen(bool fullscreen);
};

class FVideoDriver_Null: public VideoDriverFactory<FVideoDriver_Null> {
public:
	static const int priority = 1;
	/* virtual */ const char *GetName() { return "null"; }
	/* virtual */ const char *GetDescription() { return "Null Video Driver"; }
	/* virtual */ Driver *CreateInstance() { return new VideoDriver_Null(); }
};

#endif /* VIDEO_NULL_H */
