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
class VideoDriver_Dedicated : public VideoDriver {
public:
	const char *Start(const StringList &param) override;

	void Stop() override;

	void MakeDirty(int left, int top, int width, int height) override;

	void MainLoop() override;

	bool ChangeResolution(int w, int h) override;

	bool ToggleFullscreen(bool fullscreen) override;
	const char *GetName() const override { return "dedicated"; }
	bool HasGUI() const override { return false; }

	std::chrono::steady_clock::duration GetGameInterval() override
	{
		/* Infinite speed, as quickly as you can. */
		if (_game_speed == 0) return std::chrono::microseconds(0);

		long min = MILLISECONDS_PER_TICK * 1000;
		long ms = 1000000 / _settings_client.gui.refresh_rate;
		if (ms < min)
			ms = min;

		return std::chrono::microseconds(ms * 100 / _game_speed);
	}
};

/** Factory for the dedicated server video driver. */
class FVideoDriver_Dedicated : public DriverFactoryBase {
public:
#ifdef DEDICATED
	/* Automatically select this dedicated driver when making a dedicated
	 * server build. */
	static const int PRIORITY = 10;
#else
	static const int PRIORITY = 0;
#endif
	FVideoDriver_Dedicated() : DriverFactoryBase(Driver::DT_VIDEO, PRIORITY, "dedicated", "Dedicated Video Driver") {}
	Driver *CreateInstance() const override { return new VideoDriver_Dedicated(); }
};

#endif /* VIDEO_DEDICATED_H */
