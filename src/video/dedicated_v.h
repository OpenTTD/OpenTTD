/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dedicated_v.h Base for the dedicated video driver. */

#ifndef VIDEO_DEDICATED_H
#define VIDEO_DEDICATED_H

#include "video_driver.hpp"

/** The dedicated server video driver. */
class VideoDriver_Dedicated: public VideoDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ void MainLoop();

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ bool ToggleFullscreen(bool fullscreen);
	/* virtual */ const char *GetName() const { return "dedicated"; }
};

/** Factory for the dedicated server video driver. */
class FVideoDriver_Dedicated: public VideoDriverFactory<FVideoDriver_Dedicated> {
public:
#ifdef DEDICATED
	/* Automatically select this dedicated driver when making a dedicated
	 * server build. */
	static const int priority = 10;
#else
	static const int priority = 0;
#endif
	/* virtual */ const char *GetName() { return "dedicated"; }
	/* virtual */ const char *GetDescription() { return "Dedicated Video Driver"; }
	/* virtual */ Driver *CreateInstance() { return new VideoDriver_Dedicated(); }
};

#endif /* VIDEO_DEDICATED_H */
