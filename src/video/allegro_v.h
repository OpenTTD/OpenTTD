/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file allegro_v.h Base of the Allegro video driver. */

#ifndef VIDEO_ALLEGRO_H
#define VIDEO_ALLEGRO_H

#include "video_driver.hpp"

/** The allegro video driver. */
class VideoDriver_Allegro: public VideoDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ void MainLoop();

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ bool ToggleFullscreen(bool fullscreen);
	/* virtual */ const char *GetName() const { return "allegro"; }
};

/** Factory for the allegro video driver. */
class FVideoDriver_Allegro: public VideoDriverFactory<FVideoDriver_Allegro> {
public:
	static const int priority = 4;
	/* virtual */ const char *GetName() { return "allegro"; }
	/* virtual */ const char *GetDescription() { return "Allegro Video Driver"; }
	/* virtual */ Driver *CreateInstance() { return new VideoDriver_Allegro(); }
};

#endif /* VIDEO_ALLEGRO_H */
