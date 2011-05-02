/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file null_v.h Base of the video driver that doesn't blit. */

#ifndef VIDEO_NULL_H
#define VIDEO_NULL_H

#include "video_driver.hpp"

/** The null video driver. */
class VideoDriver_Null: public VideoDriver {
private:
	uint ticks; ///< Amount of ticks to run.

public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ void MainLoop();

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ bool ToggleFullscreen(bool fullscreen);
	/* virtual */ const char *GetName() const { return "null"; }
};

/** Factory the null video driver. */
class FVideoDriver_Null: public VideoDriverFactory<FVideoDriver_Null> {
public:
	static const int priority = 0;
	/* virtual */ const char *GetName() { return "null"; }
	/* virtual */ const char *GetDescription() { return "Null Video Driver"; }
	/* virtual */ Driver *CreateInstance() { return new VideoDriver_Null(); }
};

#endif /* VIDEO_NULL_H */
