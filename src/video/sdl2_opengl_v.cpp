/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl2_opengl_v.cpp Implementation of the OpenGL backend for SDL2 video driver. */

/* XXX -- Temporary hack for Windows compile */
#define WINGDIAPI
#define APIENTRY

#include "../stdafx.h"
#include "../openttd.h"
#include "../gfx_func.h"
#include "../rev.h"
#include "../blitter/factory.hpp"
#include "../network/network.h"
#include "../thread.h"
#include "../progress.h"
#include "../core/random_func.hpp"
#include "../core/math_func.hpp"
#include "../core/mem_func.hpp"
#include "../core/geometry_func.hpp"
#include "../fileio_func.h"
#include "../framerate_type.h"
#include "../window_func.h"
#include "sdl2_opengl_v.h"
#include <SDL.h>
#include <mutex>
#include <condition_variable>
#include <GL/gl.h>
#include "../3rdparty/opengl/glext.h"
#include "opengl.h"
#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#	include <emscripten/html5.h>
#endif

#include "../safeguards.h"

static FVideoDriver_SDL_OpenGL iFVideoDriver_SDL_OpenGL;

/** Platform-specific callback to get an OpenGL funtion pointer. */
static OGLProc GetOGLProcAddressCallback(const char *proc)
{
	return reinterpret_cast<OGLProc>(SDL_GL_GetProcAddress(proc));
}

bool VideoDriver_SDL_OpenGL::CreateMainWindow(uint w, uint h, uint flags)
{
	return this->VideoDriver_SDL_Base::CreateMainWindow(w, h, flags | SDL_WINDOW_OPENGL);
}

const char *VideoDriver_SDL_OpenGL::Start(const StringList &param)
{
	const char *error = VideoDriver_SDL_Base::Start(param);
	if (error != nullptr) return error;

	error = this->AllocateContext();
	if (error != nullptr) {
		this->Stop();
		return error;
	}

	this->driver_info += " (";
	this->driver_info += OpenGLBackend::Get()->GetDriverName();
	this->driver_info += ")";

	/* Now we have a OpenGL context, force a client-size-changed event,
	 * so all buffers are allocated correctly. */
	int w, h;
	SDL_GetWindowSize(this->sdl_window, &w, &h);
	this->ClientSizeChanged(w, h, true);
	/* We should have a valid screen buffer now. If not, something went wrong and we should abort. */
	if (_screen.dst_ptr == nullptr) {
		this->Stop();
		return "Can't get pointer to screen buffer";
	}
	/* Main loop expects to start with the buffer unmapped. */
	this->ReleaseVideoPointer();

	return nullptr;
}

void VideoDriver_SDL_OpenGL::Stop()
{
	this->DestroyContext();
	this->VideoDriver_SDL_Base::Stop();
}

void VideoDriver_SDL_OpenGL::DestroyContext()
{
	OpenGLBackend::Destroy();

	if (this->gl_context != nullptr) {
		SDL_GL_DeleteContext(this->gl_context);
		this->gl_context = nullptr;
	}
}

void VideoDriver_SDL_OpenGL::ToggleVsync(bool vsync)
{
	SDL_GL_SetSwapInterval(vsync);
}

const char *VideoDriver_SDL_OpenGL::AllocateContext()
{
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	if (_debug_driver_level >= 8) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	}

	this->gl_context = SDL_GL_CreateContext(this->sdl_window);
	if (this->gl_context == nullptr) return "SDL2: Can't active GL context";

	ToggleVsync(_video_vsync);

	return OpenGLBackend::Create(&GetOGLProcAddressCallback, this->GetScreenSize());
}

void VideoDriver_SDL_OpenGL::PopulateSystemSprites()
{
	OpenGLBackend::Get()->PopulateCursorCache();
}

void VideoDriver_SDL_OpenGL::ClearSystemSprites()
{
	OpenGLBackend::Get()->ClearCursorCache();
}

bool VideoDriver_SDL_OpenGL::AllocateBackingStore(int w, int h, bool force)
{
	if (this->gl_context == nullptr) return false;

	if (_screen.dst_ptr != nullptr) this->ReleaseVideoPointer();

	w = std::max(w, 64);
	h = std::max(h, 64);
	MemSetT(&this->dirty_rect, 0);

	bool res = OpenGLBackend::Get()->Resize(w, h, force);
	SDL_GL_SwapWindow(this->sdl_window);
	_screen.dst_ptr = this->GetVideoPointer();

	CopyPalette(this->local_palette, true);

	return res;
}

void *VideoDriver_SDL_OpenGL::GetVideoPointer()
{
	if (BlitterFactory::GetCurrentBlitter()->NeedsAnimationBuffer()) {
		this->anim_buffer = OpenGLBackend::Get()->GetAnimBuffer();
	}
	return OpenGLBackend::Get()->GetVideoBuffer();
}

void VideoDriver_SDL_OpenGL::ReleaseVideoPointer()
{
	if (this->anim_buffer != nullptr) OpenGLBackend::Get()->ReleaseAnimBuffer(this->dirty_rect);
	OpenGLBackend::Get()->ReleaseVideoBuffer(this->dirty_rect);
	MemSetT(&this->dirty_rect, 0);
	this->anim_buffer = nullptr;
}

void VideoDriver_SDL_OpenGL::Paint()
{
	PerformanceMeasurer framerate(PFE_VIDEO);

	if (this->local_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		/* Always push a changed palette to OpenGL. */
		OpenGLBackend::Get()->UpdatePalette(this->local_palette.palette, this->local_palette.first_dirty, this->local_palette.count_dirty);
		if (blitter->UsePaletteAnimation() == Blitter::PALETTE_ANIMATION_BLITTER) {
			blitter->PaletteAnimate(this->local_palette);
		}

		this->local_palette.count_dirty = 0;
	}

	OpenGLBackend::Get()->Paint();
	OpenGLBackend::Get()->DrawMouseCursor();

	SDL_GL_SwapWindow(this->sdl_window);
}
