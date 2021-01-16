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

/** Platform-independent back-end singleton class for OpenGL video drivers. */
class OpenGLBackend : public ZeroedMemoryAllocator {
private:
	static OpenGLBackend *instance; ///< Singleton instance pointer.

	void *vid_buffer;   ///< Pointer to the memory used for the video driver to draw to.
	GLuint vid_texture; ///< Texture handle for the video buffer texture.

	OpenGLBackend();
	~OpenGLBackend();

	const char *Init();

public:
	/** Get singleton instance of this class. */
	static inline OpenGLBackend *Get()
	{
		return OpenGLBackend::instance;
	}
	static const char *Create();
	static void Destroy();

	bool Resize(int w, int h, bool force = false);
	void Paint();

	/**
	 * Get a pointer to the memory for the video driver to draw to.
	 * @return Pointer to draw on.
	 */
	void *GetVideoBuffer() { return this->vid_buffer; }
};

#endif /* VIDEO_OPENGL_H */
