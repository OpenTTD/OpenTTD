/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file libretro_core.h Libretro core interface for OpenTTD. */

#ifndef VIDEO_LIBRETRO_CORE_H
#define VIDEO_LIBRETRO_CORE_H

#include "libretro.h"
#include <cstdint>

/**
 * Namespace containing functions to interface between OpenTTD and the libretro core.
 * These functions allow OpenTTD's video, audio, and input drivers to communicate
 * with the libretro frontend.
 */
namespace LibretroCore {

/**
 * Check if the libretro core is currently running.
 * @return true if the core is running and game loop should continue.
 */
bool IsRunning();

/**
 * Set the video buffer that contains the rendered frame.
 * Called by OpenTTD's video driver after rendering.
 * @param buffer Pointer to the pixel data (XRGB8888 format).
 * @param width Width of the frame in pixels.
 * @param height Height of the frame in pixels.
 * @param pitch Number of bytes per row.
 */
void SetVideoBuffer(void *buffer, unsigned width, unsigned height, unsigned pitch);

/**
 * Get the current video dimensions and pitch.
 * @param width Output: width of the video buffer.
 * @param height Output: height of the video buffer.
 * @param pitch Output: pitch (bytes per row) of the video buffer.
 */
void GetVideoInfo(unsigned *width, unsigned *height, unsigned *pitch);

bool SetVideoGeometry(unsigned width, unsigned height);

/**
 * Get a pointer to the video buffer for direct rendering.
 * @return Pointer to the video buffer, or nullptr if not available.
 */
void *GetVideoBuffer();

/**
 * Get the current mouse state.
 * @param x Output: mouse X coordinate.
 * @param y Output: mouse Y coordinate.
 * @param left Output: left button state.
 * @param right Output: right button state.
 * @param middle Output: middle button state.
 * @param wheel Output: mouse wheel delta (cleared after reading).
 */
void GetMouseState(int *x, int *y, bool *left, bool *right, bool *middle, int *wheel = nullptr);

/**
 * Check if the left mouse button was just clicked (pressed this frame).
 * @return true if left click occurred.
 */
bool GetMouseLeftClick();

/**
 * Check if the right mouse button was just clicked (pressed this frame).
 * @return true if right click occurred.
 */
bool GetMouseRightClick();

/**
 * Check if a keyboard key is currently pressed.
 * @param keycode The retro_key keycode to check.
 * @return true if the key is pressed.
 */
bool IsKeyPressed(unsigned keycode);

/**
 * Get the current keyboard modifier state.
 * @return Bitmask of retro_mod modifiers.
 */
uint16_t GetKeyboardModifiers();

/**
 * Get the next pending keyboard event.
 * @param down Output: true if key was pressed, false if released.
 * @param keycode Output: the libretro keycode (retro_key).
 * @param character Output: the Unicode character, or 0 if none.
 * @param modifiers Output: modifier state when event occurred.
 * @return true if an event was available, false if queue was empty.
 */
bool GetNextKeyEvent(bool *down, unsigned *keycode, uint32_t *character, uint16_t *modifiers);

/**
 * Submit mixed audio data to the libretro frontend.
 * Called by OpenTTD's sound driver/mixer.
 * @param buffer Stereo interleaved int16_t samples.
 * @param frames Number of frames (each frame is 2 samples for stereo).
 */
void MixAudio(int16_t *buffer, unsigned frames);

/**
 * Get the system directory path where core files should be stored.
 * This is where OpenTTD data files (baseset, etc.) should be located.
 * @return Path to the system directory.
 */
const char *GetSystemDirectory();

/**
 * Get the save directory path where saves and configs should be stored.
 * @return Path to the save directory.
 */
const char *GetSaveDirectory();

/**
 * Check if the VFS interface is available.
 * @return true if VFS is available.
 */
bool HasVFS();

/**
 * Get the VFS interface for file operations.
 * @return Pointer to the VFS interface, or nullptr if not available.
 */
struct retro_vfs_interface *GetVFS();

/**
 * Get the OpenGL proc address lookup function.
 * @return Function pointer for looking up GL functions.
 */
retro_hw_get_proc_address_t GetGLProcAddress();

/**
 * Get the current OpenGL framebuffer object to render to.
 * @return The FBO handle, or 0 if not available.
 */
uintptr_t GetCurrentFramebuffer();

/**
 * Check if OpenGL hardware rendering is active.
 * @return true if OpenGL rendering should be used.
 */
bool IsUsingOpenGL();

/**
 * Log a message through the libretro logging interface.
 * @param level Log level (debug, info, warn, error).
 * @param fmt Printf-style format string.
 * @param ... Format arguments.
 */
void Log(retro_log_level level, const char *fmt, ...);

} /* namespace LibretroCore */

#endif /* VIDEO_LIBRETRO_CORE_H */
