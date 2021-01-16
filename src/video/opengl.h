/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file opengl.h OpenGL video driver support. */

#ifndef VIDEO_OPENGL_H
#define VIDEO_OPENGL_H

#include "../core/alloc_type.hpp"
#include "../core/geometry_type.hpp"
#include "../gfx_type.h"

typedef void (*OGLProc)();
typedef OGLProc (*GetOGLProcAddressProc)(const char *proc);

bool IsOpenGLVersionAtLeast(byte major, byte minor);
const char *FindStringInExtensionList(const char *string, const char *substring);

/** Platform-independent back-end class for OpenGL video drivers. */
class OpenGLBackend : public ZeroedMemoryAllocator {
private:
	static OpenGLBackend *instance; ///< Singleton instance pointer.

	GLuint vid_pbo;     ///< Pixel buffer object storing the memory used for the video driver to draw to.
	GLuint vid_texture; ///< Texture handle for the video buffer texture.
	GLuint vid_program; ///< Shader program for rendering a RGBA video buffer.
	GLuint pal_program; ///< Shader program for rendering a paletted video buffer.
	GLuint vao_quad;    ///< Vertex array object storing the rendering state for the fullscreen quad.
	GLuint vbo_quad;    ///< Vertex buffer with a fullscreen quad.
	GLuint pal_texture; ///< Palette lookup texture.

	OpenGLBackend();
	~OpenGLBackend();

	const char *Init();
	bool InitShaders();

public:
	/** Get singleton instance of this class. */
	static inline OpenGLBackend *Get()
	{
		return OpenGLBackend::instance;
	}
	static const char *Create(GetOGLProcAddressProc get_proc);
	static void Destroy();

	void UpdatePalette(const Colour *pal, uint first, uint length);
	bool Resize(int w, int h, bool force = false);
	void Paint();

	void *GetVideoBuffer();
	void ReleaseVideoBuffer(const Rect &update_rect);
};

#endif /* VIDEO_OPENGL_H */
