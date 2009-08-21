/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file video_driver.hpp Base of all video drivers. */

#ifndef VIDEO_VIDEO_DRIVER_HPP
#define VIDEO_VIDEO_DRIVER_HPP

#include "../driver.h"
#include "../core/geometry_type.hpp"

class VideoDriver: public Driver {
public:
	virtual void MakeDirty(int left, int top, int width, int height) = 0;

	virtual void MainLoop() = 0;

	virtual bool ChangeResolution(int w, int h) = 0;

	virtual bool ToggleFullscreen(bool fullscreen) = 0;
};

class VideoDriverFactoryBase: public DriverFactoryBase {
};

template <class T>
class VideoDriverFactory: public VideoDriverFactoryBase {
public:
	VideoDriverFactory() { this->RegisterDriver(((T *)this)->GetName(), Driver::DT_VIDEO, ((T *)this)->priority); }

	/**
	 * Get the long, human readable, name for the Driver-class.
	 */
	const char *GetName();
};

extern VideoDriver *_video_driver;
extern char *_ini_videodriver;
extern int _num_resolutions;
extern Dimension _resolutions[32];
extern Dimension _cur_resolution;

#endif /* VIDEO_VIDEO_DRIVER_HPP */
