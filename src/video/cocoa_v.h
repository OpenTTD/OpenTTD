/* $Id$ */

#ifndef VIDEO_COCOA_H
#define VIDEO_COCOA_H

#include "video_driver.hpp"

class VideoDriver_Cocoa: public VideoDriver {
public:
	/* virtual */ bool CanProbe() { return true; }

	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ void MainLoop();

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ void ToggleFullscreen(bool fullscreen);
};

class FVideoDriver_Cocoa: public VideoDriverFactory<FVideoDriver_Cocoa> {
public:
	/* virtual */ const char *GetName() { return "cocoa"; }
	/* virtual */ const char *GetDescription() { return "Cocoa Video Driver"; }
	/* virtual */ Driver *CreateInstance() { return new VideoDriver_Cocoa(); }
};

#endif /* VIDEO_COCOA_H */
