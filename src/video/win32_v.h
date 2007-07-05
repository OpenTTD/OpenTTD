/* $Id$ */

#ifndef VIDEO_WIN32_H
#define VIDEO_WIN32_H

#include "video_driver.hpp"

class VideoDriver_Win32: public VideoDriver {
public:
	/* virtual */ bool CanProbe() { return true; }

	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ void MainLoop();

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ void ToggleFullscreen(bool fullscreen);
};

class FVideoDriver_Win32: public VideoDriverFactory<FVideoDriver_Win32> {
public:
	/* virtual */ const char *GetName() { return "win32"; }
	/* virtual */ const char *GetDescription() { return "Win32 Video Driver"; }
	/* virtual */ Driver *CreateInstance() { return new VideoDriver_Win32(); }
};

#endif /* VIDEO_WIN32_H */
