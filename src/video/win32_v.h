/* $Id$ */

#ifndef VIDEO_WIN32_H
#define VIDEO_WIN32_H

#include "video_driver.hpp"

class VideoDriver_Win32: public VideoDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ void MainLoop();

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ bool ToggleFullscreen(bool fullscreen);
};

class FVideoDriver_Win32: public VideoDriverFactory<FVideoDriver_Win32> {
public:
	static const int priority = 10;
	/* virtual */ const char *GetName() { return "win32"; }
	/* virtual */ const char *GetDescription() { return "Win32 GDI Video Driver"; }
	/* virtual */ Driver *CreateInstance() { return new VideoDriver_Win32(); }
};

#endif /* VIDEO_WIN32_H */
