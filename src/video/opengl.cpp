/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file opengl_v.cpp OpenGL video driver support. */

#include "../stdafx.h"

#if defined(_WIN32)
#	include <windows.h>
#endif

#if defined(__APPLE__)
#	include <OpenGL/gl3.h>
#else
#	include <GL/gl.h>
#endif
#include "../3rdparty/opengl/glext.h"

#include "opengl.h"
#include "../core/mem_func.hpp"
#include "../core/geometry_func.hpp"
#include "../gfx_func.h"
#include "../debug.h"

#include "../safeguards.h"


static PFNGLDEBUGMESSAGECONTROLPROC _glDebugMessageControl;
static PFNGLDEBUGMESSAGECALLBACKPROC _glDebugMessageCallback;

/* static */ OpenGLBackend *OpenGLBackend::instance = nullptr;

GetOGLProcAddressProc GetOGLProcAddress;

/**
 * Find a substring in a string made of space delimited elements. The substring
 * has to match the complete element, partial matches don't count.
 * @param string List of space delimited elements.
 * @param substring Substring to find.
 * @return Pointer to the start of the match or nullptr if the substring is not present.
 */
static const char *FindStringInExtensionList(const char *string, const char *substring)
{
	while (1) {
		/* Is the extension string present at all? */
		const char *pos = strstr(string, substring);
		if (pos == nullptr) break;

		/* Is this a real match, i.e. are the chars before and after the matched string
		 * indeed spaces (or the start or end of the string, respectively)? */
		const char *end = pos + strlen(substring);
		if ((pos == string || pos[-1] == ' ') && (*end == ' ' || *end == '\0')) return pos;

		/* False hit, try again for the remaining string. */
		string = end;
	}

	return nullptr;
}

/**
 * Check if an OpenGL extension is supported by the current context.
 * @param extension The extension string to test.
 * @return True if the extension is supported, false if not.
 */
static bool IsOpenGLExtensionSupported(const char *extension)
{
	static PFNGLGETSTRINGIPROC glGetStringi = nullptr;
	static bool glGetStringi_loaded = false;

	/* Starting with OpenGL 3.0 the preferred API to get the extensions
	 * has changed. Try to load the required function once. */
	if (!glGetStringi_loaded) {
		if (IsOpenGLVersionAtLeast(3, 0)) glGetStringi = (PFNGLGETSTRINGIPROC)GetOGLProcAddress("glGetStringi");
		glGetStringi_loaded = true;
	}

	if (glGetStringi != nullptr) {
		/* New style: Each supported extension can be queried and compared independently. */
		GLint num_exts;
		glGetIntegerv(GL_NUM_EXTENSIONS, &num_exts);

		for (GLint i = 0; i < num_exts; i++) {
			const char *entry = (const char *)glGetStringi(GL_EXTENSIONS, i);
			if (strcmp(entry, extension) == 0) return true;
		}
	} else {
		/* Old style: A single, space-delimited string for all extensions. */
		return FindStringInExtensionList((const char *)glGetString(GL_EXTENSIONS), extension) != nullptr;
	}

	return false;
}

static byte _gl_major_ver = 0; ///< Major OpenGL version.
static byte _gl_minor_ver = 0; ///< Minor OpenGL version.

/**
 * Check if the current OpenGL version is equal or higher than a given one.
 * @param major Minimal major version.
 * @param minor Minimal minor version.
 * @pre OpenGL was initialized.
 * @return True if the OpenGL version is equal or higher than the requested one.
 */
bool IsOpenGLVersionAtLeast(byte major, byte minor)
{
	return (_gl_major_ver > major) || (_gl_major_ver == major && _gl_minor_ver >= minor);
}

/** Callback to receive OpenGL debug messages. */
void APIENTRY DebugOutputCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
	/* Make severity human readable. */
	const char *severity_str = "";
	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:   severity_str = "high"; break;
		case GL_DEBUG_SEVERITY_MEDIUM: severity_str = "medium"; break;
		case GL_DEBUG_SEVERITY_LOW:    severity_str = "low"; break;
	}

	/* Make type human readable.*/
	const char *type_str = "Other";
	switch (type) {
		case GL_DEBUG_TYPE_ERROR:               type_str = "Error"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_str = "Deprecated"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  type_str = "Undefined behaviour"; break;
		case GL_DEBUG_TYPE_PERFORMANCE:         type_str = "Performance"; break;
		case GL_DEBUG_TYPE_PORTABILITY:         type_str = "Portability"; break;
	}

	DEBUG(driver, 6, "OpenGL: %s (%s) - %s", type_str, severity_str, message);
}

/** Enable OpenGL debug messages if supported. */
void SetupDebugOutput()
{
#ifndef NO_DEBUG_MESSAGES
	if (_debug_driver_level < 6) return;

	if (IsOpenGLVersionAtLeast(4, 3)) {
		_glDebugMessageControl = (PFNGLDEBUGMESSAGECONTROLPROC)GetOGLProcAddress("glDebugMessageControl");
		_glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)GetOGLProcAddress("glDebugMessageCallback");
	} else if (IsOpenGLExtensionSupported("GL_ARB_debug_output")) {
		_glDebugMessageControl = (PFNGLDEBUGMESSAGECONTROLPROC)GetOGLProcAddress("glDebugMessageControlARB");
		_glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)GetOGLProcAddress("glDebugMessageCallbackARB");
	}

	if (_glDebugMessageControl != nullptr && _glDebugMessageCallback != nullptr) {
		/* Enable debug output. As synchronous debug output costs performance, we only enable it with a high debug level. */
		glEnable(GL_DEBUG_OUTPUT);
		if (_debug_driver_level >= 8) glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

		_glDebugMessageCallback(&DebugOutputCallback, nullptr);
		/* Enable all messages on highest debug level.*/
		_glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, _debug_driver_level >= 9 ? GL_TRUE : GL_FALSE);
		/* Get debug messages for errors and undefined/deprecated behaviour. */
		_glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, nullptr, GL_TRUE);
		_glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, GL_DONT_CARE, 0, nullptr, GL_TRUE);
		_glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, GL_DONT_CARE, 0, nullptr, GL_TRUE);
	}
#endif
}

/**
 * Create and initialize the singleton back-end class.
 * @param get_proc Callback to get an OpenGL function from the OS driver.
 * @return nullptr on success, error message otherwise.
 */
/* static */ const char *OpenGLBackend::Create(GetOGLProcAddressProc get_proc)
{
	if (OpenGLBackend::instance != nullptr) OpenGLBackend::Destroy();

	GetOGLProcAddress = get_proc;

	OpenGLBackend::instance = new OpenGLBackend();
	return OpenGLBackend::instance->Init();
}

/**
 * Free resources and destroy singleton back-end class.
 */
/* static */ void OpenGLBackend::Destroy()
{
	delete OpenGLBackend::instance;
	OpenGLBackend::instance = nullptr;
}

/**
 * Construct OpenGL back-end class.
 */
OpenGLBackend::OpenGLBackend()
{
}

/**
 * Free allocated resources.
 */
OpenGLBackend::~OpenGLBackend()
{
	glDeleteTextures(1, &this->vid_texture);
	free(this->vid_buffer);
}

/**
 * Check for the needed OpenGL functionality and allocate all resources.
 * @return Error string or nullptr if successful.
 */
const char *OpenGLBackend::Init()
{
	/* Always query the supported OpenGL version as the current context might have changed. */
	const char *ver = (const char *)glGetString(GL_VERSION);
	const char *vend = (const char *)glGetString(GL_VENDOR);
	const char *renderer = (const char *)glGetString(GL_RENDERER);

	if (ver == nullptr || vend == nullptr || renderer == nullptr) return "OpenGL not supported";

	DEBUG(driver, 1, "OpenGL driver: %s - %s (%s)", vend, renderer, ver);

	const char *minor = strchr(ver, '.');
	_gl_major_ver = atoi(ver);
	_gl_minor_ver = minor != nullptr ? atoi(minor + 1) : 0;

	SetupDebugOutput();

	/* OpenGL 1.3 is the absolute minimum. */
	if (!IsOpenGLVersionAtLeast(1, 3)) return "OpenGL version >= 1.3 required";
	/* Check for non-power-of-two texture support. */
	if (!IsOpenGLVersionAtLeast(2, 0) && !IsOpenGLExtensionSupported("GL_ARB_texture_non_power_of_two")) return "Non-power-of-two textures not supported";

	/* Setup video buffer texture. */
	glGenTextures(1, &this->vid_texture);
	glBindTexture(GL_TEXTURE_2D, this->vid_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	if (glGetError() != GL_NO_ERROR) return "Can't generate video buffer texture";

	/* Prime vertex array with a full-screen quad. */
	static const float vert_array[] = { 1.f, -1.f, 1.f, 1.f, -1.f, -1.f, -1.f, 1.f };
	static const float tex_array[]  = { 1.f,  1.f, 1.f, 0.f,  0.f,  1.f,  0.f, 0.f };

	glVertexPointer(2, GL_FLOAT, 0, vert_array);
	glTexCoordPointer(2, GL_FLOAT, 0, tex_array);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	(void)glGetError(); // Clear errors.

	return nullptr;
}

/**
 * Change the size of the drawing window and allocate matching resources.
 * @param w New width of the window.
 * @param h New height of the window.
 * @param force Recreate resources even if size didn't change.
 * @param False if nothing had to be done, true otherwise.
 */
bool OpenGLBackend::Resize(int w, int h, bool force)
{
	if (!force && _screen.width == w && _screen.height == h && this->vid_buffer != nullptr) return false;

	glViewport(0, 0, w, h);

	/* Re-allocate video buffer texture. */
	free(this->vid_buffer);
	this->vid_buffer = CallocT<uint32>(w * h); // 32bpp

	glBindTexture(GL_TEXTURE_2D, this->vid_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	/* Set new viewport. */
	_screen.height = h;
	_screen.width = w;
	_screen.pitch = w;
	_screen.dst_ptr = this->GetVideoBuffer();

	return true;
}

/**
 * Render video buffer to the screen.
 * @param update_rect Rectangle encompassing the dirty region of the video buffer.
 */
void OpenGLBackend::Paint(Rect update_rect)
{
	assert(this->vid_buffer != nullptr);

	glClear(GL_COLOR_BUFFER_BIT);

	/* Update changed rect of the video buffer texture. */
	glBindTexture(GL_TEXTURE_2D, this->vid_texture);
	if (!IsEmptyRect(update_rect)) {
		glPixelStorei(GL_UNPACK_ROW_LENGTH, _screen.pitch);
		glTexSubImage2D(GL_TEXTURE_2D, 0, update_rect.left, update_rect.top, update_rect.right - update_rect.left, update_rect.bottom - update_rect.top, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, (uint32 *)this->vid_buffer + update_rect.top * _screen.pitch + update_rect.left);
	}

	/* Blit video buffer to screen. */
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

