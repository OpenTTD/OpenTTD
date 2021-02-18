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
#include "../core/math_func.hpp"
#include "../gfx_func.h"
#include "../settings_type.h"
#include "../zoom_type.h"
#include <chrono>
#include <vector>

extern std::string _ini_videodriver;
extern std::vector<Dimension> _resolutions;
extern Dimension _cur_resolution;
extern bool _rightclick_emulate;

/** The base of all video drivers. */
class VideoDriver : public Driver {
	const uint DEFAULT_WINDOW_WIDTH = 640u;  ///< Default window width.
	const uint DEFAULT_WINDOW_HEIGHT = 480u; ///< Default window height.

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

	/**
	 * Callback invoked after the blitter was changed.
	 * This may only be called between AcquireBlitterLock and ReleaseBlitterLock.
	 * @return True if no error.
	 */
	virtual bool AfterBlitterChange()
	{
		return true;
	}

	/**
	 * Acquire any lock(s) required to be held when changing blitters.
	 * These lock(s) may not be acquired recursively.
	 */
	virtual void AcquireBlitterLock() { }

	/**
	 * Release any lock(s) required to be held when changing blitters.
	 * These lock(s) may not be acquired recursively.
	 */
	virtual void ReleaseBlitterLock() { }

	virtual bool ClaimMousePointer()
	{
		return true;
	}

	/**
	 * Whether the driver has a graphical user interface with the end user.
	 * Or in other words, whether we should spawn a thread for world generation
	 * and NewGRF scanning so the graphical updates can keep coming. Otherwise
	 * progress has to be shown on the console, which uses by definition another
	 * thread/process for display purposes.
	 * @return True for all drivers except null and dedicated.
	 */
	virtual bool HasGUI() const
	{
		return true;
	}

	/**
	 * An edit box lost the input focus. Abort character compositing if necessary.
	 */
	virtual void EditBoxLostFocus() {}

	/**
	 * An edit box gained the input focus
	 */
	virtual void EditBoxGainedFocus() {}

	/**
	 * Get a suggested default GUI zoom taking screen DPI into account.
	 */
	virtual ZoomLevel GetSuggestedUIZoom()
	{
		float dpi_scale = this->GetDPIScale();

		if (dpi_scale >= 3.0f) return ZOOM_LVL_NORMAL;
		if (dpi_scale >= 1.5f) return ZOOM_LVL_OUT_2X;
		return ZOOM_LVL_OUT_4X;
	}

	/**
	 * Get the currently active instance of the video driver.
	 */
	static VideoDriver *GetInstance() {
		return static_cast<VideoDriver*>(*DriverFactoryBase::GetActiveDriver(Driver::DT_VIDEO));
	}

protected:
	const uint ALLOWED_DRIFT = 5; ///< How many times videodriver can miss deadlines without it being overly compensated.

	/**
	 * Get the resolution of the main screen.
	 */
	virtual Dimension GetScreenSize() const { return { DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT }; }

	/**
	 * Get DPI scaling factor of the screen OTTD is displayed on.
	 * @return 1.0 for default platform DPI, > 1.0 for higher DPI values, and < 1.0 for smaller DPI values.
	 */
	virtual float GetDPIScale() { return 1.0f; }

	/**
	 * Apply resolution auto-detection and clamp to sensible defaults.
	 */
	void UpdateAutoResolution()
	{
		if (_cur_resolution.width == 0 || _cur_resolution.height == 0) {
			/* Auto-detect a good resolution. We aim for 75% of the screen size.
			 * Limit width times height times bytes per pixel to fit a 32 bit
			 * integer, This way all internal drawing routines work correctly. */
			Dimension res = this->GetScreenSize();
			_cur_resolution.width  = ClampU(res.width  * 3 / 4, DEFAULT_WINDOW_WIDTH, UINT16_MAX / 2);
			_cur_resolution.height = ClampU(res.height * 3 / 4, DEFAULT_WINDOW_HEIGHT, UINT16_MAX / 2);
		}
	}

	std::chrono::steady_clock::duration GetGameInterval()
	{
		if (_game_speed == 0) return std::chrono::microseconds(0);
		return std::chrono::microseconds(MILLISECONDS_PER_TICK * 1000 * 100 / _game_speed);
	}

	std::chrono::steady_clock::duration GetDrawInterval()
	{
		return std::chrono::microseconds(1000000 / _settings_client.gui.refresh_rate);
	}
};

#endif /* VIDEO_VIDEO_DRIVER_HPP */
