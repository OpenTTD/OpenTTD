/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file libretro_v.h Libretro video driver header. */

#ifndef VIDEO_LIBRETRO_V_H
#define VIDEO_LIBRETRO_V_H

#ifdef WITH_LIBRETRO

#include "../video_driver.hpp"
#include "../../gfx_type.h"

/** Libretro video driver. */
class VideoDriver_Libretro : public VideoDriver {
public:
	VideoDriver_Libretro() : VideoDriver(false) {}

	std::optional<std::string_view> Start(const StringList &param) override;
	void Stop() override;
	void MakeDirty(int left, int top, int width, int height) override;
	void MainLoop() override;
	bool ChangeResolution(int w, int h) override;
	bool ToggleFullscreen(bool fullscreen) override;

	std::string_view GetName() const override { return "libretro"; }
	bool HasGUI() const override { return true; }
	std::vector<int> GetListOfMonitorRefreshRates() override;

	/* Libretro-specific methods */
	void RunFrame();
	void ProcessLibretroInput();
	uint32_t *GetVideoBuffer();
	void GetVideoSize(unsigned *w, unsigned *h, unsigned *pitch);

	static VideoDriver_Libretro *GetInstance();

protected:
	void InputLoop() override;
	void Paint() override;
	void CheckPaletteAnim() override;
	bool PollEvent() override;

private:
	bool AllocateBackingStore(int w, int h);
	void FreeBackingStore();

	int screen_width = 1280;
	int screen_height = 720;
	uint32_t *video_buffer = nullptr;
	size_t video_buffer_size = 0;

	Rect dirty_rect = {};
};

/** Factory for the libretro video driver. */
class FVideoDriver_Libretro : public DriverFactoryBase {
public:
	FVideoDriver_Libretro() : DriverFactoryBase(Driver::DT_VIDEO, 1, "libretro", "Libretro Video Driver") {}
	std::unique_ptr<Driver> CreateInstance() const override { return std::make_unique<VideoDriver_Libretro>(); }
};

#endif /* WITH_LIBRETRO */
#endif /* VIDEO_LIBRETRO_V_H */
