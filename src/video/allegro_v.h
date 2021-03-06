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
class VideoDriver_Allegro : public VideoDriver {
public:
	const char *Start(const StringList &param) override;

	void Stop() override;

	void MakeDirty(int left, int top, int width, int height) override;

	void MainLoop() override;

	bool ChangeResolution(int w, int h) override;

	bool ToggleFullscreen(bool fullscreen) override;

	bool AfterBlitterChange() override;

	bool ClaimMousePointer() override;

	std::vector<int> GetListOfMonitorRefreshRates() override;

	const char *GetName() const override { return "allegro"; }

protected:
	void InputLoop() override;
	void Paint() override;
	void CheckPaletteAnim() override;
	bool PollEvent() override;
};

/** Factory for the allegro video driver. */
class FVideoDriver_Allegro : public DriverFactoryBase {
public:
	FVideoDriver_Allegro() : DriverFactoryBase(Driver::DT_VIDEO, 4, "allegro", "Allegro Video Driver") {}
	Driver *CreateInstance() const override { return new VideoDriver_Allegro(); }
};

#endif /* VIDEO_ALLEGRO_H */
