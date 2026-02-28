/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ios_v.h The iOS video driver. */

#ifndef VIDEO_IOS_H
#define VIDEO_IOS_H

#include <sys/types.h>

typedef unsigned int uint;

#include "../video_driver.hpp"
#include "../../core/geometry_type.hpp"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if TARGET_OS_IOS

extern bool _ios_video_started;

@class UIWindow;
@class OTTD_iOSViewController;
@class OTTD_MetalView;
@class CADisplayLink;

class VideoDriver_iOS : public VideoDriver {
private:
	Dimension orig_res;       ///< Saved window size.
	bool refresh_sys_sprites; ///< System sprites need refreshing.
	std::atomic<bool> ready_for_tick{false};

	// Touch drag detection
	bool touch_is_dragging;
	float touch_start_x;
	float touch_start_y;
	uintptr_t active_touch_id;
	static constexpr float DRAG_THRESHOLD = 15.0f; // points

public:
	bool setup; ///< Window is currently being created.

	UIWindow *window;                    ///< Pointer to window object
	OTTD_iOSViewController *viewController; ///< Pointer to view controller
	OTTD_MetalView *metalView;           ///< Pointer to Metal view
	CADisplayLink *displayLink;

	Rect safe_area; ///< Safe area insets (top, left, bottom, right)

public:
	VideoDriver_iOS(bool uses_hardware_acceleration = false);

	void Stop() override;
	void MainLoop() override;

	void MakeDirty(int left, int top, int width, int height) override;
	bool AfterBlitterChange() override;

	bool ChangeResolution(int w, int h) override;
	bool ToggleFullscreen(bool fullscreen) override;
	void ToggleVsync(bool vsync) override;

	void ClearSystemSprites() override;
	void PopulateSystemSprites() override;

	void EditBoxLostFocus() override;

	std::vector<int> GetListOfMonitorRefreshRates() override;

	int GetBufferPitch() const { return _screen.pitch; }
	
	void SetSafeAreaInsets(float top, float bottom, float left, float right);

	/* --- The following methods should be private, but can't be due to Obj-C limitations. --- */

	void MainLoopReal();

	virtual void AllocateBackingStore(bool force = false) = 0;

	// Touch handling
	void HandleTouchBegan(float x, float y, uintptr_t touch_id);
	void HandleTouchMoved(float x, float y, uintptr_t touch_id);
	void HandleTouchEnded(float x, float y, uintptr_t touch_id);

    // Gesture handling
    void HandleTap(float x, float y);
    void HandleRightClick(float x, float y);
    void HandlePan(float dx, float dy);
    void HandleZoomIn();
    void HandleZoomOut();

	/** Called when the drawable size changes. */
	void GameSizeChanged();

	/** Check if driver is ready to process ticks. */
	bool IsReadyForTick() const { return this->ready_for_tick.load(); }

	/** Wrapper to call Tick() from Objective-C code. */
	void TickWrapper() { if (this->ready_for_tick.load()) this->Tick(); }

	/** Get a pointer to the buffer for display (always 32bpp RGBA/BGRA). */
	virtual void *GetDisplayBuffer() = 0;

protected:
	Rect dirty_rect;    ///< Region of the screen that needs redrawing.
	bool buffer_locked; ///< Video buffer was locked by the main thread.

	Dimension GetScreenSize() const override;
	void InputLoop() override;
	bool LockVideoBuffer() override;
	void UnlockVideoBuffer() override;
	bool PollEvent() override;

	std::optional<std::string_view> Initialize();

	bool MakeWindow(int width, int height);

	/** Get a pointer to the video buffer. */
	virtual void *GetVideoPointer() = 0;
	/** Hand video buffer back to the drawing backend. */
	virtual void ReleaseVideoPointer() {}
};

class VideoDriver_iOSMetal : public VideoDriver_iOS {
private:
	int buffer_depth;     ///< Colour depth of used frame buffer
	std::unique_ptr<uint8_t[]> pixel_buffer; ///< used for direct pixel access
	std::unique_ptr<uint32_t[]> window_buffer; ///< Colour translation from palette to screen
	std::unique_ptr<uint8_t[]> anim_buffer; ///< Animation buffer for 32bpp animated blitter

	int window_width;     ///< Current window width in pixel
	int window_height;    ///< Current window height in pixel
	int window_pitch;

	uint32_t palette[256];  ///< Colour Palette

	void BlitIndexedToView32(int left, int top, int right, int bottom);
	void UpdatePalette(uint first_colour, uint num_colours);

public:
	VideoDriver_iOSMetal();

	std::optional<std::string_view> Start(const StringList &param) override;
	void Stop() override;

	/** Return driver name */
	std::string_view GetName() const override { return "ios"; }

	void AllocateBackingStore(bool force = false) override;

	bool HasAnimBuffer() override { return true; }
	uint8_t *GetAnimBuffer() override { return this->anim_buffer.get(); }

protected:
	void Paint() override;
	void CheckPaletteAnim() override;

	void *GetVideoPointer() override { return this->buffer_depth == 8 ? static_cast<void *>(this->pixel_buffer.get()) : static_cast<void *>(this->window_buffer.get()); }
    void *GetDisplayBuffer() override { return static_cast<void *>(this->window_buffer.get()); }
};

class FVideoDriver_iOSMetal : public DriverFactoryBase {
public:
	FVideoDriver_iOSMetal() : DriverFactoryBase(Driver::Type::Video, 10, "ios", "iOS Video Driver") {}
	std::unique_ptr<Driver> CreateInstance() const override { return std::make_unique<VideoDriver_iOSMetal>(); }
};

#endif /* TARGET_OS_IOS */

#endif /* VIDEO_IOS_H */
