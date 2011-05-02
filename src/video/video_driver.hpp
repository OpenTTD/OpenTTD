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

/** The base of all video drivers. */
class VideoDriver: public Driver {
public:
	/**
	 * Mark a particular area dirty.
	 * @param left   The left most line of the dirty area.
	 * @param top    The top most line of the dirty area.
	 * @param width  The width of the dirty area.
	 * @param height The height of the dirty area.
	 */
	virtual void MakeDirty(int left, int top, int width, int height) = 0;

	/**
	 * Perform the actual drawing.
	 */
	virtual void MainLoop() = 0;

	/**
	 * Change the resolution of the window.
	 * @param w The new width.
	 * @param h The new height.
	 * @return True if the change succeeded.
	 */
	virtual bool ChangeResolution(int w, int h) = 0;

	/**
	 * Change the full screen setting.
	 * @param fullscreen The new setting.
	 * @return True if the change succeeded.
	 */
	virtual bool ToggleFullscreen(bool fullscreen) = 0;
};

/** Base of the factory for the video drivers. */
class VideoDriverFactoryBase: public DriverFactoryBase {
};

/**
 * Factory for the video drivers.
 * @tparam T The type of the video factory to register.
 */
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
extern bool _rightclick_emulate;

#endif /* VIDEO_VIDEO_DRIVER_HPP */
