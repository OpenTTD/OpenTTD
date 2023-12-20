/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file opengl_v.cpp OpenGL video driver support. */

#include "../stdafx.h"

/* Define to disable buffer syncing. Will increase max fast forward FPS but produces artifacts. Mainly useful for performance testing. */
// #define NO_GL_BUFFER_SYNC
/* Define to allow software rendering backends. */
// #define GL_ALLOW_SOFTWARE_RENDERER

#if defined(_WIN32)
#	include <windows.h>
#endif

#define GL_GLEXT_PROTOTYPES
#if defined(__APPLE__)
#	define GL_SILENCE_DEPRECATION
#	include <OpenGL/gl3.h>
#else
#	include <GL/gl.h>
#endif
#include "../3rdparty/opengl/glext.h"

#include "opengl.h"
#include "../core/geometry_func.hpp"
#include "../core/mem_func.hpp"
#include "../core/math_func.hpp"
#include "../core/mem_func.hpp"
#include "../gfx_func.h"
#include "../debug.h"
#include "../blitter/factory.hpp"
#include "../zoom_func.h"

#include "../table/opengl_shader.h"
#include "../table/sprites.h"


#include "../safeguards.h"


/* Define function pointers of all OpenGL functions that we load dynamically. */

#define GL(function) static decltype(&function) _ ## function

GL(glGetString);
GL(glGetIntegerv);
GL(glGetError);
GL(glDebugMessageControl);
GL(glDebugMessageCallback);

GL(glDisable);
GL(glEnable);
GL(glViewport);
GL(glClear);
GL(glClearColor);
GL(glBlendFunc);
GL(glDrawArrays);

GL(glTexImage1D);
GL(glTexImage2D);
GL(glTexParameteri);
GL(glTexSubImage1D);
GL(glTexSubImage2D);
GL(glBindTexture);
GL(glDeleteTextures);
GL(glGenTextures);
GL(glPixelStorei);

GL(glActiveTexture);

GL(glGenBuffers);
GL(glDeleteBuffers);
GL(glBindBuffer);
GL(glBufferData);
GL(glBufferSubData);
GL(glMapBuffer);
GL(glUnmapBuffer);
GL(glClearBufferSubData);

GL(glBufferStorage);
GL(glMapBufferRange);
GL(glClientWaitSync);
GL(glFenceSync);
GL(glDeleteSync);

GL(glGenVertexArrays);
GL(glDeleteVertexArrays);
GL(glBindVertexArray);

GL(glCreateProgram);
GL(glDeleteProgram);
GL(glLinkProgram);
GL(glUseProgram);
GL(glGetProgramiv);
GL(glGetProgramInfoLog);
GL(glCreateShader);
GL(glDeleteShader);
GL(glShaderSource);
GL(glCompileShader);
GL(glAttachShader);
GL(glGetShaderiv);
GL(glGetShaderInfoLog);
GL(glGetUniformLocation);
GL(glUniform1i);
GL(glUniform1f);
GL(glUniform2f);
GL(glUniform4f);

GL(glGetAttribLocation);
GL(glEnableVertexAttribArray);
GL(glDisableVertexAttribArray);
GL(glVertexAttribPointer);
GL(glBindFragDataLocation);

#undef GL


/** A simple 2D vertex with just position and texture. */
struct Simple2DVertex {
	float x, y;
	float u, v;
};

/** Maximum number of cursor sprites to cache. */
static const int MAX_CACHED_CURSORS = 48;

/* static */ OpenGLBackend *OpenGLBackend::instance = nullptr;

GetOGLProcAddressProc GetOGLProcAddress;

/**
 * Find a substring in a string made of space delimited elements. The substring
 * has to match the complete element, partial matches don't count.
 * @param string List of space delimited elements.
 * @param substring Substring to find.
 * @return Pointer to the start of the match or nullptr if the substring is not present.
 */
const char *FindStringInExtensionList(const char *string, const char *substring)
{
	while (true) {
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
		_glGetIntegerv(GL_NUM_EXTENSIONS, &num_exts);

		for (GLint i = 0; i < num_exts; i++) {
			const char *entry = (const char *)glGetStringi(GL_EXTENSIONS, i);
			if (strcmp(entry, extension) == 0) return true;
		}
	} else {
		/* Old style: A single, space-delimited string for all extensions. */
		return FindStringInExtensionList((const char *)_glGetString(GL_EXTENSIONS), extension) != nullptr;
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

/**
 * Try loading an OpenGL function.
 * @tparam F Type of the function pointer.
 * @param f Reference where to store the function pointer in.
 * @param name Name of the function.
 * @return True if the function could be bound.
 */
template <typename F>
static bool BindGLProc(F &f, const char *name)
{
	f = reinterpret_cast<F>(GetOGLProcAddress(name));
	return f != nullptr;
}

/** Bind basic information functions. */
static bool BindBasicInfoProcs()
{
	if (!BindGLProc(_glGetString, "glGetString")) return false;
	if (!BindGLProc(_glGetIntegerv, "glGetIntegerv")) return false;
	if (!BindGLProc(_glGetError, "glGetError")) return false;

	return true;
}

/** Bind OpenGL 1.0 and 1.1 functions. */
static bool BindBasicOpenGLProcs()
{
	if (!BindGLProc(_glDisable, "glDisable")) return false;
	if (!BindGLProc(_glEnable, "glEnable")) return false;
	if (!BindGLProc(_glViewport, "glViewport")) return false;
	if (!BindGLProc(_glTexImage1D, "glTexImage1D")) return false;
	if (!BindGLProc(_glTexImage2D, "glTexImage2D")) return false;
	if (!BindGLProc(_glTexParameteri, "glTexParameteri")) return false;
	if (!BindGLProc(_glTexSubImage1D, "glTexSubImage1D")) return false;
	if (!BindGLProc(_glTexSubImage2D, "glTexSubImage2D")) return false;
	if (!BindGLProc(_glBindTexture, "glBindTexture")) return false;
	if (!BindGLProc(_glDeleteTextures, "glDeleteTextures")) return false;
	if (!BindGLProc(_glGenTextures, "glGenTextures")) return false;
	if (!BindGLProc(_glPixelStorei, "glPixelStorei")) return false;
	if (!BindGLProc(_glClear, "glClear")) return false;
	if (!BindGLProc(_glClearColor, "glClearColor")) return false;
	if (!BindGLProc(_glBlendFunc, "glBlendFunc")) return false;
	if (!BindGLProc(_glDrawArrays, "glDrawArrays")) return false;

	return true;
}

/** Bind texture-related extension functions. */
static bool BindTextureExtensions()
{
	if (IsOpenGLVersionAtLeast(1, 3)) {
		if (!BindGLProc(_glActiveTexture, "glActiveTexture")) return false;
	} else {
		if (!BindGLProc(_glActiveTexture, "glActiveTextureARB")) return false;
	}

	return true;
}

/** Bind vertex buffer object extension functions. */
static bool BindVBOExtension()
{
	if (IsOpenGLVersionAtLeast(1, 5)) {
		if (!BindGLProc(_glGenBuffers, "glGenBuffers")) return false;
		if (!BindGLProc(_glDeleteBuffers, "glDeleteBuffers")) return false;
		if (!BindGLProc(_glBindBuffer, "glBindBuffer")) return false;
		if (!BindGLProc(_glBufferData, "glBufferData")) return false;
		if (!BindGLProc(_glBufferSubData, "glBufferSubData")) return false;
		if (!BindGLProc(_glMapBuffer, "glMapBuffer")) return false;
		if (!BindGLProc(_glUnmapBuffer, "glUnmapBuffer")) return false;
	} else {
		if (!BindGLProc(_glGenBuffers, "glGenBuffersARB")) return false;
		if (!BindGLProc(_glDeleteBuffers, "glDeleteBuffersARB")) return false;
		if (!BindGLProc(_glBindBuffer, "glBindBufferARB")) return false;
		if (!BindGLProc(_glBufferData, "glBufferDataARB")) return false;
		if (!BindGLProc(_glBufferSubData, "glBufferSubDataARB")) return false;
		if (!BindGLProc(_glMapBuffer, "glMapBufferARB")) return false;
		if (!BindGLProc(_glUnmapBuffer, "glUnmapBufferARB")) return false;
	}

	if (IsOpenGLVersionAtLeast(4, 3) || IsOpenGLExtensionSupported("GL_ARB_clear_buffer_object")) {
		BindGLProc(_glClearBufferSubData, "glClearBufferSubData");
	} else {
		_glClearBufferSubData = nullptr;
	}

	return true;
}

/** Bind vertex array object extension functions. */
static bool BindVBAExtension()
{
	/* The APPLE and ARB variants have different semantics (that don't matter for us).
	 *  Successfully getting pointers to one variant doesn't mean it is supported for
	 *  the current context. Always check the extension strings as well. */
	if (IsOpenGLVersionAtLeast(3, 0) || IsOpenGLExtensionSupported("GL_ARB_vertex_array_object")) {
		if (!BindGLProc(_glGenVertexArrays, "glGenVertexArrays")) return false;
		if (!BindGLProc(_glDeleteVertexArrays, "glDeleteVertexArrays")) return false;
		if (!BindGLProc(_glBindVertexArray, "glBindVertexArray")) return false;
	} else if (IsOpenGLExtensionSupported("GL_APPLE_vertex_array_object")) {
		if (!BindGLProc(_glGenVertexArrays, "glGenVertexArraysAPPLE")) return false;
		if (!BindGLProc(_glDeleteVertexArrays, "glDeleteVertexArraysAPPLE")) return false;
		if (!BindGLProc(_glBindVertexArray, "glBindVertexArrayAPPLE")) return false;
	}

	return true;
}

/** Bind extension functions for shader support. */
static bool BindShaderExtensions()
{
	if (IsOpenGLVersionAtLeast(2, 0)) {
		if (!BindGLProc(_glCreateProgram, "glCreateProgram")) return false;
		if (!BindGLProc(_glDeleteProgram, "glDeleteProgram")) return false;
		if (!BindGLProc(_glLinkProgram, "glLinkProgram")) return false;
		if (!BindGLProc(_glUseProgram, "glUseProgram")) return false;
		if (!BindGLProc(_glGetProgramiv, "glGetProgramiv")) return false;
		if (!BindGLProc(_glGetProgramInfoLog, "glGetProgramInfoLog")) return false;
		if (!BindGLProc(_glCreateShader, "glCreateShader")) return false;
		if (!BindGLProc(_glDeleteShader, "glDeleteShader")) return false;
		if (!BindGLProc(_glShaderSource, "glShaderSource")) return false;
		if (!BindGLProc(_glCompileShader, "glCompileShader")) return false;
		if (!BindGLProc(_glAttachShader, "glAttachShader")) return false;
		if (!BindGLProc(_glGetShaderiv, "glGetShaderiv")) return false;
		if (!BindGLProc(_glGetShaderInfoLog, "glGetShaderInfoLog")) return false;
		if (!BindGLProc(_glGetUniformLocation, "glGetUniformLocation")) return false;
		if (!BindGLProc(_glUniform1i, "glUniform1i")) return false;
		if (!BindGLProc(_glUniform1f, "glUniform1f")) return false;
		if (!BindGLProc(_glUniform2f, "glUniform2f")) return false;
		if (!BindGLProc(_glUniform4f, "glUniform4f")) return false;

		if (!BindGLProc(_glGetAttribLocation, "glGetAttribLocation")) return false;
		if (!BindGLProc(_glEnableVertexAttribArray, "glEnableVertexAttribArray")) return false;
		if (!BindGLProc(_glDisableVertexAttribArray, "glDisableVertexAttribArray")) return false;
		if (!BindGLProc(_glVertexAttribPointer, "glVertexAttribPointer")) return false;
	} else {
		/* In the ARB extension programs and shaders are in the same object space. */
		if (!BindGLProc(_glCreateProgram, "glCreateProgramObjectARB")) return false;
		if (!BindGLProc(_glDeleteProgram, "glDeleteObjectARB")) return false;
		if (!BindGLProc(_glLinkProgram, "glLinkProgramARB")) return false;
		if (!BindGLProc(_glUseProgram, "glUseProgramObjectARB")) return false;
		if (!BindGLProc(_glGetProgramiv, "glGetObjectParameterivARB")) return false;
		if (!BindGLProc(_glGetProgramInfoLog, "glGetInfoLogARB")) return false;
		if (!BindGLProc(_glCreateShader, "glCreateShaderObjectARB")) return false;
		if (!BindGLProc(_glDeleteShader, "glDeleteObjectARB")) return false;
		if (!BindGLProc(_glShaderSource, "glShaderSourceARB")) return false;
		if (!BindGLProc(_glCompileShader, "glCompileShaderARB")) return false;
		if (!BindGLProc(_glAttachShader, "glAttachObjectARB")) return false;
		if (!BindGLProc(_glGetShaderiv, "glGetObjectParameterivARB")) return false;
		if (!BindGLProc(_glGetShaderInfoLog, "glGetInfoLogARB")) return false;
		if (!BindGLProc(_glGetUniformLocation, "glGetUniformLocationARB")) return false;
		if (!BindGLProc(_glUniform1i, "glUniform1iARB")) return false;
		if (!BindGLProc(_glUniform1f, "glUniform1fARB")) return false;
		if (!BindGLProc(_glUniform2f, "glUniform2fARB")) return false;
		if (!BindGLProc(_glUniform4f, "glUniform4fARB")) return false;

		if (!BindGLProc(_glGetAttribLocation, "glGetAttribLocationARB")) return false;
		if (!BindGLProc(_glEnableVertexAttribArray, "glEnableVertexAttribArrayARB")) return false;
		if (!BindGLProc(_glDisableVertexAttribArray, "glDisableVertexAttribArrayARB")) return false;
		if (!BindGLProc(_glVertexAttribPointer, "glVertexAttribPointerARB")) return false;
	}

	/* Bind functions only needed when using GLSL 1.50 shaders. */
	if (IsOpenGLVersionAtLeast(3, 0)) {
		BindGLProc(_glBindFragDataLocation, "glBindFragDataLocation");
	} else if (IsOpenGLExtensionSupported("GL_EXT_gpu_shader4")) {
		BindGLProc(_glBindFragDataLocation, "glBindFragDataLocationEXT");
	} else {
		_glBindFragDataLocation = nullptr;
	}

	return true;
}

/** Bind extension functions for persistent buffer mapping. */
static bool BindPersistentBufferExtensions()
{
	/* Optional functions for persistent buffer mapping. */
	if (IsOpenGLVersionAtLeast(3, 0)) {
		if (!BindGLProc(_glMapBufferRange, "glMapBufferRange")) return false;
	}
	if (IsOpenGLVersionAtLeast(4, 4) || IsOpenGLExtensionSupported("GL_ARB_buffer_storage")) {
		if (!BindGLProc(_glBufferStorage, "glBufferStorage")) return false;
	}
#ifndef NO_GL_BUFFER_SYNC
	if (IsOpenGLVersionAtLeast(3, 2) || IsOpenGLExtensionSupported("GL_ARB_sync")) {
		if (!BindGLProc(_glClientWaitSync, "glClientWaitSync")) return false;
		if (!BindGLProc(_glFenceSync, "glFenceSync")) return false;
		if (!BindGLProc(_glDeleteSync, "glDeleteSync")) return false;
	}
#endif

	return true;
}

/** Callback to receive OpenGL debug messages. */
void APIENTRY DebugOutputCallback([[maybe_unused]] GLenum source, GLenum type, [[maybe_unused]] GLuint id, GLenum severity, [[maybe_unused]] GLsizei length, const GLchar *message, [[maybe_unused]] const void *userParam)
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

	Debug(driver, 6, "OpenGL: {} ({}) - {}", type_str, severity_str, message);
}

/** Enable OpenGL debug messages if supported. */
void SetupDebugOutput()
{
#ifndef NO_DEBUG_MESSAGES
	if (_debug_driver_level < 6) return;

	if (IsOpenGLVersionAtLeast(4, 3)) {
		BindGLProc(_glDebugMessageControl, "glDebugMessageControl");
		BindGLProc(_glDebugMessageCallback, "glDebugMessageCallback");
	} else if (IsOpenGLExtensionSupported("GL_ARB_debug_output")) {
		BindGLProc(_glDebugMessageControl, "glDebugMessageControlARB");
		BindGLProc(_glDebugMessageCallback, "glDebugMessageCallbackARB");
	}

	if (_glDebugMessageControl != nullptr && _glDebugMessageCallback != nullptr) {
		/* Enable debug output. As synchronous debug output costs performance, we only enable it with a high debug level. */
		_glEnable(GL_DEBUG_OUTPUT);
		if (_debug_driver_level >= 8) _glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

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
 * @param screen_res Current display resolution.
 * @return nullptr on success, error message otherwise.
 */
/* static */ const char *OpenGLBackend::Create(GetOGLProcAddressProc get_proc, const Dimension &screen_res)
{
	if (OpenGLBackend::instance != nullptr) OpenGLBackend::Destroy();

	GetOGLProcAddress = get_proc;

	OpenGLBackend::instance = new OpenGLBackend();
	return OpenGLBackend::instance->Init(screen_res);
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
OpenGLBackend::OpenGLBackend() : cursor_cache(MAX_CACHED_CURSORS)
{
}

/**
 * Free allocated resources.
 */
OpenGLBackend::~OpenGLBackend()
{
	if (_glDeleteProgram != nullptr) {
		_glDeleteProgram(this->remap_program);
		_glDeleteProgram(this->vid_program);
		_glDeleteProgram(this->pal_program);
		_glDeleteProgram(this->sprite_program);
	}
	if (_glDeleteVertexArrays != nullptr) _glDeleteVertexArrays(1, &this->vao_quad);
	if (_glDeleteBuffers != nullptr) {
		_glDeleteBuffers(1, &this->vbo_quad);
		_glDeleteBuffers(1, &this->vid_pbo);
		_glDeleteBuffers(1, &this->anim_pbo);
	}
	if (_glDeleteTextures != nullptr) {
		this->InternalClearCursorCache();
		OpenGLSprite::Destroy();

		_glDeleteTextures(1, &this->vid_texture);
		_glDeleteTextures(1, &this->anim_texture);
		_glDeleteTextures(1, &this->pal_texture);
	}
}

/**
 * Check for the needed OpenGL functionality and allocate all resources.
 * @param screen_res Current display resolution.
 * @return Error string or nullptr if successful.
 */
const char *OpenGLBackend::Init(const Dimension &screen_res)
{
	if (!BindBasicInfoProcs()) return "OpenGL not supported";

	/* Always query the supported OpenGL version as the current context might have changed. */
	const char *ver = (const char *)_glGetString(GL_VERSION);
	const char *vend = (const char *)_glGetString(GL_VENDOR);
	const char *renderer = (const char *)_glGetString(GL_RENDERER);

	if (ver == nullptr || vend == nullptr || renderer == nullptr) return "OpenGL not supported";

	Debug(driver, 1, "OpenGL driver: {} - {} ({})", vend, renderer, ver);

#ifndef GL_ALLOW_SOFTWARE_RENDERER
	/* Don't use MESA software rendering backends as they are slower than
	 * just using a non-OpenGL video driver. */
	if (strncmp(renderer, "llvmpipe", 8) == 0 || strncmp(renderer, "softpipe", 8) == 0) return "Software renderer detected, not using OpenGL";
#endif

	const char *minor = strchr(ver, '.');
	_gl_major_ver = atoi(ver);
	_gl_minor_ver = minor != nullptr ? atoi(minor + 1) : 0;

#ifdef _WIN32
	/* Old drivers on Windows (especially if made by Intel) seem to be
	 * unstable, so cull the oldest stuff here.  */
	if (!IsOpenGLVersionAtLeast(3, 2)) return "Need at least OpenGL version 3.2 on Windows";
#endif

	if (!BindBasicOpenGLProcs()) return "Failed to bind basic OpenGL functions.";

	SetupDebugOutput();

	/* OpenGL 1.3 is the absolute minimum. */
	if (!IsOpenGLVersionAtLeast(1, 3)) return "OpenGL version >= 1.3 required";
	/* Check for non-power-of-two texture support. */
	if (!IsOpenGLVersionAtLeast(2, 0) && !IsOpenGLExtensionSupported("GL_ARB_texture_non_power_of_two")) return "Non-power-of-two textures not supported";
	/* Check for single element texture formats. */
	if (!IsOpenGLVersionAtLeast(3, 0) && !IsOpenGLExtensionSupported("GL_ARB_texture_rg")) return "Single element texture formats not supported";
	if (!BindTextureExtensions()) return "Failed to bind texture extension functions";
	/* Check for vertex buffer objects. */
	if (!IsOpenGLVersionAtLeast(1, 5) && !IsOpenGLExtensionSupported("ARB_vertex_buffer_object")) return "Vertex buffer objects not supported";
	if (!BindVBOExtension()) return "Failed to bind VBO extension functions";
	/* Check for pixel buffer objects. */
	if (!IsOpenGLVersionAtLeast(2, 1) && !IsOpenGLExtensionSupported("GL_ARB_pixel_buffer_object")) return "Pixel buffer objects not supported";
	/* Check for vertex array objects. */
	if (!IsOpenGLVersionAtLeast(3, 0) && (!IsOpenGLExtensionSupported("GL_ARB_vertex_array_object") || !IsOpenGLExtensionSupported("GL_APPLE_vertex_array_object"))) return "Vertex array objects not supported";
	if (!BindVBAExtension()) return "Failed to bind VBA extension functions";
	/* Check for shader objects. */
	if (!IsOpenGLVersionAtLeast(2, 0) && (!IsOpenGLExtensionSupported("GL_ARB_shader_objects") || !IsOpenGLExtensionSupported("GL_ARB_fragment_shader") || !IsOpenGLExtensionSupported("GL_ARB_vertex_shader"))) return "No shader support";
	if (!BindShaderExtensions()) return "Failed to bind shader extension functions";
	if (IsOpenGLVersionAtLeast(3, 2) && _glBindFragDataLocation == nullptr) return "OpenGL claims to support version 3.2 but doesn't have glBindFragDataLocation";

	this->persistent_mapping_supported = IsOpenGLVersionAtLeast(3, 0) && (IsOpenGLVersionAtLeast(4, 4) || IsOpenGLExtensionSupported("GL_ARB_buffer_storage"));
#ifndef NO_GL_BUFFER_SYNC
	this->persistent_mapping_supported = this->persistent_mapping_supported && (IsOpenGLVersionAtLeast(3, 2) || IsOpenGLExtensionSupported("GL_ARB_sync"));
#endif

	if (this->persistent_mapping_supported && !BindPersistentBufferExtensions()) {
		Debug(driver, 1, "OpenGL claims to support persistent buffer mapping but doesn't export all functions, not using persistent mapping.");
		this->persistent_mapping_supported = false;
	}
	if (this->persistent_mapping_supported) Debug(driver, 3, "OpenGL: Using persistent buffer mapping");

	/* Check maximum texture size against screen resolution. */
	GLint max_tex_size = 0;
	_glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);
	if (std::max(screen_res.width, screen_res.height) > (uint)max_tex_size) return "Max supported texture size is too small";

	/* Check available texture units. */
	GLint max_tex_units = 0;
	_glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_tex_units);
	if (max_tex_units < 4) return "Not enough simultaneous textures supported";

	Debug(driver, 2, "OpenGL shading language version: {}, texture units = {}", (const char *)_glGetString(GL_SHADING_LANGUAGE_VERSION), (int)max_tex_units);

	if (!this->InitShaders()) return "Failed to initialize shaders";

	/* Setup video buffer texture. */
	_glGenTextures(1, &this->vid_texture);
	_glBindTexture(GL_TEXTURE_2D, this->vid_texture);
	_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	_glBindTexture(GL_TEXTURE_2D, 0);
	if (_glGetError() != GL_NO_ERROR) return "Can't generate video buffer texture";

	/* Setup video buffer texture. */
	_glGenTextures(1, &this->anim_texture);
	_glBindTexture(GL_TEXTURE_2D, this->anim_texture);
	_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	_glBindTexture(GL_TEXTURE_2D, 0);
	if (_glGetError() != GL_NO_ERROR) return "Can't generate animation buffer texture";

	/* Setup palette texture. */
	_glGenTextures(1, &this->pal_texture);
	_glBindTexture(GL_TEXTURE_1D, this->pal_texture);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAX_LEVEL, 0);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	_glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 256, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);
	_glBindTexture(GL_TEXTURE_1D, 0);
	if (_glGetError() != GL_NO_ERROR) return "Can't generate palette lookup texture";

	/* Bind uniforms in rendering shader program. */
	GLint tex_location = _glGetUniformLocation(this->vid_program, "colour_tex");
	GLint palette_location = _glGetUniformLocation(this->vid_program, "palette");
	GLint sprite_location = _glGetUniformLocation(this->vid_program, "sprite");
	GLint screen_location = _glGetUniformLocation(this->vid_program, "screen");
	_glUseProgram(this->vid_program);
	_glUniform1i(tex_location, 0);     // Texture unit 0.
	_glUniform1i(palette_location, 1); // Texture unit 1.
	/* Values that result in no transform. */
	_glUniform4f(sprite_location, 0.0f, 0.0f, 1.0f, 1.0f);
	_glUniform2f(screen_location, 1.0f, 1.0f);

	/* Bind uniforms in palette rendering shader program. */
	tex_location = _glGetUniformLocation(this->pal_program, "colour_tex");
	palette_location = _glGetUniformLocation(this->pal_program, "palette");
	sprite_location = _glGetUniformLocation(this->pal_program, "sprite");
	screen_location = _glGetUniformLocation(this->pal_program, "screen");
	_glUseProgram(this->pal_program);
	_glUniform1i(tex_location, 0);     // Texture unit 0.
	_glUniform1i(palette_location, 1); // Texture unit 1.
	_glUniform4f(sprite_location, 0.0f, 0.0f, 1.0f, 1.0f);
	_glUniform2f(screen_location, 1.0f, 1.0f);

	/* Bind uniforms in remap shader program. */
	tex_location = _glGetUniformLocation(this->remap_program, "colour_tex");
	palette_location = _glGetUniformLocation(this->remap_program, "palette");
	GLint remap_location = _glGetUniformLocation(this->remap_program, "remap_tex");
	this->remap_sprite_loc = _glGetUniformLocation(this->remap_program, "sprite");
	this->remap_screen_loc = _glGetUniformLocation(this->remap_program, "screen");
	this->remap_zoom_loc = _glGetUniformLocation(this->remap_program, "zoom");
	this->remap_rgb_loc = _glGetUniformLocation(this->remap_program, "rgb");
	_glUseProgram(this->remap_program);
	_glUniform1i(tex_location, 0);     // Texture unit 0.
	_glUniform1i(palette_location, 1); // Texture unit 1.
	_glUniform1i(remap_location, 2);   // Texture unit 2.

	/* Bind uniforms in sprite shader program. */
	tex_location = _glGetUniformLocation(this->sprite_program, "colour_tex");
	palette_location = _glGetUniformLocation(this->sprite_program, "palette");
	remap_location = _glGetUniformLocation(this->sprite_program, "remap_tex");
	GLint pal_location = _glGetUniformLocation(this->sprite_program, "pal");
	this->sprite_sprite_loc = _glGetUniformLocation(this->sprite_program, "sprite");
	this->sprite_screen_loc = _glGetUniformLocation(this->sprite_program, "screen");
	this->sprite_zoom_loc = _glGetUniformLocation(this->sprite_program, "zoom");
	this->sprite_rgb_loc = _glGetUniformLocation(this->sprite_program, "rgb");
	this->sprite_crash_loc = _glGetUniformLocation(this->sprite_program, "crash");
	_glUseProgram(this->sprite_program);
	_glUniform1i(tex_location, 0);     // Texture unit 0.
	_glUniform1i(palette_location, 1); // Texture unit 1.
	_glUniform1i(remap_location, 2);   // Texture unit 2.
	_glUniform1i(pal_location, 3);     // Texture unit 3.
	(void)_glGetError(); // Clear errors.

	/* Create pixel buffer object as video buffer storage. */
	_glGenBuffers(1, &this->vid_pbo);
	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->vid_pbo);
	_glGenBuffers(1, &this->anim_pbo);
	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->anim_pbo);
	if (_glGetError() != GL_NO_ERROR) return "Can't allocate pixel buffer for video buffer";

	/* Prime vertex buffer with a full-screen quad and store
	 * the corresponding state in a vertex array object. */
	static const Simple2DVertex vert_array[] = {
		//  x     y    u    v
		{  1.f, -1.f, 1.f, 1.f },
		{  1.f,  1.f, 1.f, 0.f },
		{ -1.f, -1.f, 0.f, 1.f },
		{ -1.f,  1.f, 0.f, 0.f },
	};

	/* Create VAO. */
	_glGenVertexArrays(1, &this->vao_quad);
	_glBindVertexArray(this->vao_quad);

	/* Create and fill VBO. */
	_glGenBuffers(1, &this->vbo_quad);
	_glBindBuffer(GL_ARRAY_BUFFER, this->vbo_quad);
	_glBufferData(GL_ARRAY_BUFFER, sizeof(vert_array), vert_array, GL_STATIC_DRAW);
	if (_glGetError() != GL_NO_ERROR) return "Can't generate VBO for fullscreen quad";

	/* Set vertex state. */
	GLint loc_position = _glGetAttribLocation(this->vid_program, "position");
	GLint colour_position = _glGetAttribLocation(this->vid_program, "colour_uv");
	_glEnableVertexAttribArray(loc_position);
	_glEnableVertexAttribArray(colour_position);
	_glVertexAttribPointer(loc_position, 2, GL_FLOAT, GL_FALSE, sizeof(Simple2DVertex), (GLvoid *)offsetof(Simple2DVertex, x));
	_glVertexAttribPointer(colour_position, 2, GL_FLOAT, GL_FALSE, sizeof(Simple2DVertex), (GLvoid *)offsetof(Simple2DVertex, u));
	_glBindVertexArray(0);

	/* Create resources for sprite rendering. */
	if (!OpenGLSprite::Create()) return "Failed to create sprite rendering resources";

	this->PrepareContext();
	(void)_glGetError(); // Clear errors.

	return nullptr;
}

void OpenGLBackend::PrepareContext()
{
	_glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	_glDisable(GL_DEPTH_TEST);
	/* Enable alpha blending using the src alpha factor. */
	_glEnable(GL_BLEND);
	_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

std::string OpenGLBackend::GetDriverName()
{
	std::string res{};
	/* Skipping GL_VENDOR as it tends to be "obvious" from the renderer and version data, and just makes the string pointlessly longer */
	res += reinterpret_cast<const char *>(_glGetString(GL_RENDERER));
	res += ", ";
	res += reinterpret_cast<const char *>(_glGetString(GL_VERSION));
	return res;
}

/**
 * Check a shader for compilation errors and log them if necessary.
 * @param shader Shader to check.
 * @return True if the shader is valid.
 */
static bool VerifyShader(GLuint shader)
{
	static ReusableBuffer<char> log_buf;

	GLint result = GL_FALSE;
	_glGetShaderiv(shader, GL_COMPILE_STATUS, &result);

	/* Output log if there is one. */
	GLint log_len = 0;
	_glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
	if (log_len > 0) {
		_glGetShaderInfoLog(shader, log_len, nullptr, log_buf.Allocate(log_len));
		Debug(driver, result != GL_TRUE ? 0 : 2, "{}", log_buf.GetBuffer()); // Always print on failure.
	}

	return result == GL_TRUE;
}

/**
 * Check a program for link errors and log them if necessary.
 * @param program Program to check.
 * @return True if the program is valid.
 */
static bool VerifyProgram(GLuint program)
{
	static ReusableBuffer<char> log_buf;

	GLint result = GL_FALSE;
	_glGetProgramiv(program, GL_LINK_STATUS, &result);

	/* Output log if there is one. */
	GLint log_len = 0;
	_glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
	if (log_len > 0) {
		_glGetProgramInfoLog(program, log_len, nullptr, log_buf.Allocate(log_len));
		Debug(driver, result != GL_TRUE ? 0 : 2, "{}", log_buf.GetBuffer()); // Always print on failure.
	}

	return result == GL_TRUE;
}

/**
 * Create all needed shader programs.
 * @return True if successful, false otherwise.
 */
bool OpenGLBackend::InitShaders()
{
	const char *ver = (const char *)_glGetString(GL_SHADING_LANGUAGE_VERSION);
	if (ver == nullptr) return false;

	int glsl_major  = ver[0] - '0';
	int glsl_minor = ver[2] - '0';

	bool glsl_150 = (IsOpenGLVersionAtLeast(3, 2) || glsl_major > 1 || (glsl_major == 1 && glsl_minor >= 5)) && _glBindFragDataLocation != nullptr;

	/* Create vertex shader. */
	GLuint vert_shader = _glCreateShader(GL_VERTEX_SHADER);
	_glShaderSource(vert_shader, glsl_150 ? lengthof(_vertex_shader_sprite_150) : lengthof(_vertex_shader_sprite), glsl_150 ? _vertex_shader_sprite_150 : _vertex_shader_sprite, nullptr);
	_glCompileShader(vert_shader);
	if (!VerifyShader(vert_shader)) return false;

	/* Create fragment shader for plain RGBA. */
	GLuint frag_shader_rgb = _glCreateShader(GL_FRAGMENT_SHADER);
	_glShaderSource(frag_shader_rgb, glsl_150 ? lengthof(_frag_shader_direct_150) : lengthof(_frag_shader_direct), glsl_150 ? _frag_shader_direct_150 : _frag_shader_direct, nullptr);
	_glCompileShader(frag_shader_rgb);
	if (!VerifyShader(frag_shader_rgb)) return false;

	/* Create fragment shader for paletted only. */
	GLuint frag_shader_pal = _glCreateShader(GL_FRAGMENT_SHADER);
	_glShaderSource(frag_shader_pal, glsl_150 ? lengthof(_frag_shader_palette_150) : lengthof(_frag_shader_palette), glsl_150 ? _frag_shader_palette_150 : _frag_shader_palette, nullptr);
	_glCompileShader(frag_shader_pal);
	if (!VerifyShader(frag_shader_pal)) return false;

	/* Sprite remap fragment shader. */
	GLuint remap_shader = _glCreateShader(GL_FRAGMENT_SHADER);
	_glShaderSource(remap_shader, glsl_150 ? lengthof(_frag_shader_rgb_mask_blend_150) : lengthof(_frag_shader_rgb_mask_blend), glsl_150 ? _frag_shader_rgb_mask_blend_150 : _frag_shader_rgb_mask_blend, nullptr);
	_glCompileShader(remap_shader);
	if (!VerifyShader(remap_shader)) return false;

	/* Sprite fragment shader. */
	GLuint sprite_shader = _glCreateShader(GL_FRAGMENT_SHADER);
	_glShaderSource(sprite_shader, glsl_150 ? lengthof(_frag_shader_sprite_blend_150) : lengthof(_frag_shader_sprite_blend), glsl_150 ? _frag_shader_sprite_blend_150 : _frag_shader_sprite_blend, nullptr);
	_glCompileShader(sprite_shader);
	if (!VerifyShader(sprite_shader)) return false;

	/* Link shaders to program. */
	this->vid_program = _glCreateProgram();
	_glAttachShader(this->vid_program, vert_shader);
	_glAttachShader(this->vid_program, frag_shader_rgb);

	this->pal_program = _glCreateProgram();
	_glAttachShader(this->pal_program, vert_shader);
	_glAttachShader(this->pal_program, frag_shader_pal);

	this->remap_program = _glCreateProgram();
	_glAttachShader(this->remap_program, vert_shader);
	_glAttachShader(this->remap_program, remap_shader);

	this->sprite_program = _glCreateProgram();
	_glAttachShader(this->sprite_program, vert_shader);
	_glAttachShader(this->sprite_program, sprite_shader);

	if (glsl_150) {
		/* Bind fragment shader outputs. */
		_glBindFragDataLocation(this->vid_program, 0, "colour");
		_glBindFragDataLocation(this->pal_program, 0, "colour");
		_glBindFragDataLocation(this->remap_program, 0, "colour");
		_glBindFragDataLocation(this->sprite_program, 0, "colour");
	}

	_glLinkProgram(this->vid_program);
	if (!VerifyProgram(this->vid_program)) return false;

	_glLinkProgram(this->pal_program);
	if (!VerifyProgram(this->pal_program)) return false;

	_glLinkProgram(this->remap_program);
	if (!VerifyProgram(this->remap_program)) return false;

	_glLinkProgram(this->sprite_program);
	if (!VerifyProgram(this->sprite_program)) return false;

	_glDeleteShader(vert_shader);
	_glDeleteShader(frag_shader_rgb);
	_glDeleteShader(frag_shader_pal);
	_glDeleteShader(remap_shader);
	_glDeleteShader(sprite_shader);

	return true;
}

/**
 * Clear the bound pixel buffer to a specific value.
 * @param len Length of the buffer.
 * @param data Value to set.
 * @tparam T Pixel type.
 */
template <class T>
static void ClearPixelBuffer(size_t len, T data)
{
	T *buf = reinterpret_cast<T *>(_glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_WRITE));
	for (size_t i = 0; i < len; i++) {
		*buf++ = data;
	}
	_glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
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
	if (!force && _screen.width == w && _screen.height == h) return false;

	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	int pitch = Align(w, 4);
	size_t line_pixel_count = static_cast<size_t>(pitch) * h;

	_glViewport(0, 0, w, h);

	_glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch);

	this->vid_buffer = nullptr;
	if (this->persistent_mapping_supported) {
		_glDeleteBuffers(1, &this->vid_pbo);
		_glGenBuffers(1, &this->vid_pbo);
		_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->vid_pbo);
		_glBufferStorage(GL_PIXEL_UNPACK_BUFFER, line_pixel_count * bpp / 8, nullptr, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_CLIENT_STORAGE_BIT);
	} else {
		/* Re-allocate video buffer texture and backing store. */
		_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->vid_pbo);
		_glBufferData(GL_PIXEL_UNPACK_BUFFER, line_pixel_count * bpp / 8, nullptr, GL_DYNAMIC_DRAW);
	}

	if (bpp == 32) {
		/* Initialize backing store alpha to opaque for 32bpp modes. */
		Colour black(0, 0, 0);
		if (_glClearBufferSubData != nullptr) {
			_glClearBufferSubData(GL_PIXEL_UNPACK_BUFFER, GL_RGBA8, 0, line_pixel_count * bpp / 8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, &black.data);
		} else {
			ClearPixelBuffer<uint32_t>(line_pixel_count, black.data);
		}
	} else if (bpp == 8) {
		if (_glClearBufferSubData != nullptr) {
			byte b = 0;
			_glClearBufferSubData(GL_PIXEL_UNPACK_BUFFER, GL_R8, 0, line_pixel_count, GL_RED, GL_UNSIGNED_BYTE, &b);
		} else {
			ClearPixelBuffer<byte>(line_pixel_count, 0);
		}
	}

	_glActiveTexture(GL_TEXTURE0);
	_glBindTexture(GL_TEXTURE_2D, this->vid_texture);
	if (bpp == 8) {
		_glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
	} else {
		_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);
	}
	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	/* Does this blitter need a separate animation buffer? */
	if (BlitterFactory::GetCurrentBlitter()->NeedsAnimationBuffer()) {
		this->anim_buffer = nullptr;
		if (this->persistent_mapping_supported) {
			_glDeleteBuffers(1, &this->anim_pbo);
			_glGenBuffers(1, &this->anim_pbo);
			_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->anim_pbo);
			_glBufferStorage(GL_PIXEL_UNPACK_BUFFER, line_pixel_count, nullptr, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_CLIENT_STORAGE_BIT);
		} else {
			_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->anim_pbo);
			_glBufferData(GL_PIXEL_UNPACK_BUFFER, line_pixel_count, nullptr, GL_DYNAMIC_DRAW);
		}

		/* Initialize buffer as 0 == no remap. */
		if (_glClearBufferSubData != nullptr) {
			byte b = 0;
			_glClearBufferSubData(GL_PIXEL_UNPACK_BUFFER, GL_R8, 0, line_pixel_count, GL_RED, GL_UNSIGNED_BYTE, &b);
		} else {
			ClearPixelBuffer<byte>(line_pixel_count, 0);
		}

		_glBindTexture(GL_TEXTURE_2D, this->anim_texture);
		_glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
		_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	} else {
		if (this->anim_buffer != nullptr) {
			_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->anim_pbo);
			_glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			this->anim_buffer = nullptr;
		}

		/* Allocate dummy texture that always reads as 0 == no remap. */
		uint dummy = 0;
		_glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		_glBindTexture(GL_TEXTURE_2D, this->anim_texture);
		_glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &dummy);
	}

	_glBindTexture(GL_TEXTURE_2D, 0);

	/* Set new viewport. */
	_screen.height = h;
	_screen.width = w;
	_screen.pitch = pitch;
	_screen.dst_ptr = nullptr;

	/* Update screen size in remap shader program. */
	_glUseProgram(this->remap_program);
	_glUniform2f(this->remap_screen_loc, (float)_screen.width, (float)_screen.height);

	_glClear(GL_COLOR_BUFFER_BIT);

	return true;
}

/**
 * Update the stored palette.
 * @param pal Palette array with at least 256 elements.
 * @param first First entry to update.
 * @param length Number of entries to update.
 */
void OpenGLBackend::UpdatePalette(const Colour *pal, uint first, uint length)
{
	assert(first + length <= 256);

	_glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	_glActiveTexture(GL_TEXTURE1);
	_glBindTexture(GL_TEXTURE_1D, this->pal_texture);
	_glTexSubImage1D(GL_TEXTURE_1D, 0, first, length, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pal + first);
}

/**
 * Render video buffer to the screen.
 */
void OpenGLBackend::Paint()
{
	_glClear(GL_COLOR_BUFFER_BIT);

	_glDisable(GL_BLEND);

	/* Blit video buffer to screen. */
	_glActiveTexture(GL_TEXTURE0);
	_glBindTexture(GL_TEXTURE_2D, this->vid_texture);
	_glActiveTexture(GL_TEXTURE1);
	_glBindTexture(GL_TEXTURE_1D, this->pal_texture);
	/* Is the blitter relying on a separate animation buffer? */
	if (BlitterFactory::GetCurrentBlitter()->NeedsAnimationBuffer()) {
		_glActiveTexture(GL_TEXTURE2);
		_glBindTexture(GL_TEXTURE_2D, this->anim_texture);
		_glUseProgram(this->remap_program);
		_glUniform4f(this->remap_sprite_loc, 0.0f, 0.0f, 1.0f, 1.0f);
		_glUniform2f(this->remap_screen_loc, 1.0f, 1.0f);
		_glUniform1f(this->remap_zoom_loc, 0);
		_glUniform1i(this->remap_rgb_loc, 1);
	} else {
		_glUseProgram(BlitterFactory::GetCurrentBlitter()->GetScreenDepth() == 8 ? this->pal_program : this->vid_program);
	}
	_glBindVertexArray(this->vao_quad);
	_glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	_glEnable(GL_BLEND);
}

/**
 * Draw mouse cursor on screen.
 */
void OpenGLBackend::DrawMouseCursor()
{
	if (!this->cursor_in_window) return;

	/* Draw cursor on screen */
	_cur_dpi = &_screen;
	for (uint i = 0; i < this->cursor_sprite_count; ++i) {
		SpriteID sprite = this->cursor_sprite_seq[i].sprite;

		/* Sprites are cached by PopulateCursorCache(). */
		if (this->cursor_cache.Contains(sprite)) {
			Sprite *spr = this->cursor_cache.Get(sprite);

			this->RenderOglSprite((OpenGLSprite *)spr->data, this->cursor_sprite_seq[i].pal,
					this->cursor_pos.x + this->cursor_sprite_pos[i].x + UnScaleByZoom(spr->x_offs, ZOOM_LVL_GUI),
					this->cursor_pos.y + this->cursor_sprite_pos[i].y + UnScaleByZoom(spr->y_offs, ZOOM_LVL_GUI),
					ZOOM_LVL_GUI);
		}
	}
}

void OpenGLBackend::PopulateCursorCache()
{
	static_assert(lengthof(_cursor.sprite_seq) == lengthof(this->cursor_sprite_seq));
	static_assert(lengthof(_cursor.sprite_pos) == lengthof(this->cursor_sprite_pos));

	if (this->clear_cursor_cache) {
		/* We have a pending cursor cache clear to do first. */
		this->clear_cursor_cache = false;
		this->last_sprite_pal = (PaletteID)-1;

		this->InternalClearCursorCache();
	}

	this->cursor_pos = _cursor.pos;
	this->cursor_sprite_count = _cursor.sprite_count;
	this->cursor_in_window = _cursor.in_window;

	for (uint i = 0; i < _cursor.sprite_count; ++i) {
		this->cursor_sprite_seq[i] = _cursor.sprite_seq[i];
		this->cursor_sprite_pos[i] = _cursor.sprite_pos[i];
		SpriteID sprite = _cursor.sprite_seq[i].sprite;

		if (!this->cursor_cache.Contains(sprite)) {
			Sprite *old = this->cursor_cache.Insert(sprite, (Sprite *)GetRawSprite(sprite, SpriteType::Normal, &SimpleSpriteAlloc, this));
			if (old != nullptr) {
				OpenGLSprite *gl_sprite = (OpenGLSprite *)old->data;
				gl_sprite->~OpenGLSprite();
				free(old);
			}
		}
	}
}

/**
 * Clear all cached cursor sprites.
 */
void OpenGLBackend::InternalClearCursorCache()
{
	Sprite *sp;
	while ((sp = this->cursor_cache.Pop()) != nullptr) {
		OpenGLSprite *sprite = (OpenGLSprite *)sp->data;
		sprite->~OpenGLSprite();
		free(sp);
	}
}

/**
 * Queue a request for cursor cache clear.
 */
void OpenGLBackend::ClearCursorCache()
{
	/* If the game loop is threaded, this function might be called
	 * from the game thread. As we can call OpenGL functions only
	 * on the main thread, just set a flag that is handled the next
	 * time we prepare the cursor cache for drawing. */
	this->clear_cursor_cache = true;
}

/**
 * Get a pointer to the memory for the video driver to draw to.
 * @return Pointer to draw on.
 */
void *OpenGLBackend::GetVideoBuffer()
{
#ifndef NO_GL_BUFFER_SYNC
	if (this->sync_vid_mapping != nullptr) _glClientWaitSync(this->sync_vid_mapping, GL_SYNC_FLUSH_COMMANDS_BIT, 100000000); // 100ms timeout.
#endif

	if (!this->persistent_mapping_supported) {
		assert(this->vid_buffer == nullptr);
		_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->vid_pbo);
		this->vid_buffer = _glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_WRITE);
	} else if (this->vid_buffer == nullptr) {
		_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->vid_pbo);
		this->vid_buffer = _glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, _screen.pitch * _screen.height * BlitterFactory::GetCurrentBlitter()->GetScreenDepth() / 8, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
	}

	return this->vid_buffer;
}

/**
 * Get a pointer to the memory for the separate animation buffer.
 * @return Pointer to draw on.
 */
uint8_t *OpenGLBackend::GetAnimBuffer()
{
	if (this->anim_pbo == 0) return nullptr;

#ifndef NO_GL_BUFFER_SYNC
	if (this->sync_anim_mapping != nullptr) _glClientWaitSync(this->sync_anim_mapping, GL_SYNC_FLUSH_COMMANDS_BIT, 100000000); // 100ms timeout.
#endif

	if (!this->persistent_mapping_supported) {
		_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->anim_pbo);
		this->anim_buffer = _glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_WRITE);
	} else if (this->anim_buffer == nullptr) {
		_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->anim_pbo);
		this->anim_buffer = _glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, static_cast<GLsizeiptr>(_screen.pitch) * _screen.height, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
	}

	return (uint8_t *)this->anim_buffer;
}

/**
 * Update video buffer texture after the video buffer was filled.
 * @param update_rect Rectangle encompassing the dirty region of the video buffer.
 */
void OpenGLBackend::ReleaseVideoBuffer(const Rect &update_rect)
{
	assert(this->vid_pbo != 0);

	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->vid_pbo);
	if (!this->persistent_mapping_supported) {
		_glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
		this->vid_buffer = nullptr;
	}

#ifndef NO_GL_BUFFER_SYNC
	if (this->persistent_mapping_supported) {
		_glDeleteSync(this->sync_vid_mapping);
		this->sync_vid_mapping = nullptr;
	}
#endif

	/* Update changed rect of the video buffer texture. */
	if (!IsEmptyRect(update_rect)) {
		_glActiveTexture(GL_TEXTURE0);
		_glBindTexture(GL_TEXTURE_2D, this->vid_texture);
		_glPixelStorei(GL_UNPACK_ROW_LENGTH, _screen.pitch);
		if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() == 8) {
			_glTexSubImage2D(GL_TEXTURE_2D, 0, update_rect.left, update_rect.top, update_rect.right - update_rect.left, update_rect.bottom - update_rect.top, GL_RED, GL_UNSIGNED_BYTE, (GLvoid*)(size_t)(update_rect.top * _screen.pitch + update_rect.left));
		} else {
			_glTexSubImage2D(GL_TEXTURE_2D, 0, update_rect.left, update_rect.top, update_rect.right - update_rect.left, update_rect.bottom - update_rect.top, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, (GLvoid*)(size_t)(update_rect.top * _screen.pitch * 4 + update_rect.left * 4));
		}

#ifndef NO_GL_BUFFER_SYNC
		if (this->persistent_mapping_supported) this->sync_vid_mapping = _glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#endif
	}
}

/**
 * Update animation buffer texture after the animation buffer was filled.
 * @param update_rect Rectangle encompassing the dirty region of the animation buffer.
 */
void OpenGLBackend::ReleaseAnimBuffer(const Rect &update_rect)
{
	if (this->anim_pbo == 0) return;

	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->anim_pbo);
	if (!this->persistent_mapping_supported) {
		_glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
		this->anim_buffer = nullptr;
	}

#ifndef NO_GL_BUFFER_SYNC
	if (this->persistent_mapping_supported) {
		_glDeleteSync(this->sync_anim_mapping);
		this->sync_anim_mapping = nullptr;
	}
#endif

	/* Update changed rect of the video buffer texture. */
	if (update_rect.left != update_rect.right) {
		_glActiveTexture(GL_TEXTURE0);
		_glBindTexture(GL_TEXTURE_2D, this->anim_texture);
		_glPixelStorei(GL_UNPACK_ROW_LENGTH, _screen.pitch);
		_glTexSubImage2D(GL_TEXTURE_2D, 0, update_rect.left, update_rect.top, update_rect.right - update_rect.left, update_rect.bottom - update_rect.top, GL_RED, GL_UNSIGNED_BYTE, (GLvoid *)(size_t)(update_rect.top * _screen.pitch + update_rect.left));

#ifndef NO_GL_BUFFER_SYNC
		if (this->persistent_mapping_supported) this->sync_anim_mapping = _glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#endif
	}
}

/* virtual */ Sprite *OpenGLBackend::Encode(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator)
{
	/* Allocate and construct sprite data. */
	Sprite *dest_sprite = (Sprite *)allocator(sizeof(*dest_sprite) + sizeof(OpenGLSprite));

	OpenGLSprite *gl_sprite = (OpenGLSprite *)dest_sprite->data;
	new (gl_sprite) OpenGLSprite(sprite[ZOOM_LVL_NORMAL].width, sprite[ZOOM_LVL_NORMAL].height, sprite[ZOOM_LVL_NORMAL].type == SpriteType::Font ? 1 : ZOOM_LVL_END, sprite[ZOOM_LVL_NORMAL].colours);

	/* Upload texture data. */
	for (int i = 0; i < (sprite[ZOOM_LVL_NORMAL].type == SpriteType::Font ? 1 : ZOOM_LVL_END); i++) {
		gl_sprite->Update(sprite[i].width, sprite[i].height, i, sprite[i].data);
	}

	dest_sprite->height = sprite[ZOOM_LVL_NORMAL].height;
	dest_sprite->width  = sprite[ZOOM_LVL_NORMAL].width;
	dest_sprite->x_offs = sprite[ZOOM_LVL_NORMAL].x_offs;
	dest_sprite->y_offs = sprite[ZOOM_LVL_NORMAL].y_offs;

	return dest_sprite;
}

/**
 * Render a sprite to the back buffer.
 * @param gl_sprite Sprite to render.
 * @param x X position of the sprite.
 * @param y Y position of the sprite.
 * @param zoom Zoom level to use.
 */
void OpenGLBackend::RenderOglSprite(OpenGLSprite *gl_sprite, PaletteID pal, int x, int y, ZoomLevel zoom)
{
	/* Set textures. */
	bool rgb = gl_sprite->BindTextures();
	_glActiveTexture(GL_TEXTURE0 + 1);
	_glBindTexture(GL_TEXTURE_1D, this->pal_texture);

	/* Set palette remap. */
	_glActiveTexture(GL_TEXTURE0 + 3);
	if (pal != PAL_NONE) {
		_glBindTexture(GL_TEXTURE_1D, OpenGLSprite::pal_tex);
		if (pal != this->last_sprite_pal) {
			/* Different remap palette in use, update texture. */
			_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, OpenGLSprite::pal_pbo);
			_glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

			_glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, 256, GetNonSprite(GB(pal, 0, PALETTE_WIDTH), SpriteType::Recolour) + 1);
			_glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 256, GL_RED, GL_UNSIGNED_BYTE, nullptr);

			_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

			this->last_sprite_pal = pal;
		}
	} else {
		_glBindTexture(GL_TEXTURE_1D, OpenGLSprite::pal_identity);
	}

	/* Set up shader program. */
	Dimension dim = gl_sprite->GetSize(zoom);
	_glUseProgram(this->sprite_program);
	_glUniform4f(this->sprite_sprite_loc, (float)x, (float)y, (float)dim.width, (float)dim.height);
	_glUniform1f(this->sprite_zoom_loc, (float)zoom);
	_glUniform2f(this->sprite_screen_loc, (float)_screen.width, (float)_screen.height);
	_glUniform1i(this->sprite_rgb_loc, rgb ? 1 : 0);
	_glUniform1i(this->sprite_crash_loc, pal == PALETTE_CRASH ? 1 : 0);

	_glBindVertexArray(this->vao_quad);
	_glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


/* static */ GLuint OpenGLSprite::dummy_tex[] = { 0, 0 };
/* static */ GLuint OpenGLSprite::pal_identity = 0;
/* static */ GLuint OpenGLSprite::pal_tex = 0;
/* static */ GLuint OpenGLSprite::pal_pbo = 0;

/**
 * Create all common resources for sprite rendering.
 * @return True if no error occurred.
 */
/* static */ bool OpenGLSprite::Create()
{
	_glGenTextures(NUM_TEX, OpenGLSprite::dummy_tex);

	for (int t = TEX_RGBA; t < NUM_TEX; t++) {
		_glBindTexture(GL_TEXTURE_2D, OpenGLSprite::dummy_tex[t]);

		_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	_glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	/* Load dummy RGBA texture. */
	const Colour rgb_pixel(0, 0, 0);
	_glBindTexture(GL_TEXTURE_2D, OpenGLSprite::dummy_tex[TEX_RGBA]);
	_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, &rgb_pixel);

	/* Load dummy remap texture. */
	const uint pal = 0;
	_glBindTexture(GL_TEXTURE_2D, OpenGLSprite::dummy_tex[TEX_REMAP]);
	_glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &pal);

	/* Create palette remap textures. */
	std::array<uint8_t, 256> identity_pal;
	std::iota(std::begin(identity_pal), std::end(identity_pal), 0);

	/* Permanent texture for identity remap. */
	_glGenTextures(1, &OpenGLSprite::pal_identity);
	_glBindTexture(GL_TEXTURE_1D, OpenGLSprite::pal_identity);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAX_LEVEL, 0);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	_glTexImage1D(GL_TEXTURE_1D, 0, GL_R8, 256, 0, GL_RED, GL_UNSIGNED_BYTE, identity_pal.data());

	/* Dynamically updated texture for remaps. */
	_glGenTextures(1, &OpenGLSprite::pal_tex);
	_glBindTexture(GL_TEXTURE_1D, OpenGLSprite::pal_tex);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAX_LEVEL, 0);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	_glTexImage1D(GL_TEXTURE_1D, 0, GL_R8, 256, 0, GL_RED, GL_UNSIGNED_BYTE, identity_pal.data());

	/* Pixel buffer for remap updates. */
	_glGenBuffers(1, &OpenGLSprite::pal_pbo);
	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, OpenGLSprite::pal_pbo);
	_glBufferData(GL_PIXEL_UNPACK_BUFFER, 256, identity_pal.data(), GL_DYNAMIC_DRAW);
	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	return _glGetError() == GL_NO_ERROR;
}

/** Free all common resources for sprite rendering. */
/* static */ void OpenGLSprite::Destroy()
{
	_glDeleteTextures(NUM_TEX, OpenGLSprite::dummy_tex);
	_glDeleteTextures(1, &OpenGLSprite::pal_identity);
	_glDeleteTextures(1, &OpenGLSprite::pal_tex);
	if (_glDeleteBuffers != nullptr) _glDeleteBuffers(1, &OpenGLSprite::pal_pbo);
}

/**
 * Create an OpenGL sprite with a palette remap part.
 * @param width Width of the top-level texture.
 * @param height Height of the top-level texture.
 * @param levels Number of mip-map levels.
 * @param components Indicates which sprite components are used.
 */
OpenGLSprite::OpenGLSprite(uint width, uint height, uint levels, SpriteColourComponent components)
{
	assert(levels > 0);
	(void)_glGetError();

	this->dim.width = width;
	this->dim.height = height;

	MemSetT(this->tex, 0, NUM_TEX);
	_glActiveTexture(GL_TEXTURE0);
	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	for (int t = TEX_RGBA; t < NUM_TEX; t++) {
		/* Sprite component present? */
		if (t == TEX_RGBA && components == SCC_PAL) continue;
		if (t == TEX_REMAP && (components & SCC_PAL) != SCC_PAL) continue;

		/* Allocate texture. */
		_glGenTextures(1, &this->tex[t]);
		_glBindTexture(GL_TEXTURE_2D, this->tex[t]);

		_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels - 1);
		_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		/* Set size. */
		for (uint i = 0, w = width, h = height; i < levels; i++, w /= 2, h /= 2) {
			assert(w * h != 0);
			if (t == TEX_REMAP) {
				_glTexImage2D(GL_TEXTURE_2D, i, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
			} else {
				_glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);
			}
		}
	}

	assert(_glGetError() == GL_NO_ERROR);
}

OpenGLSprite::~OpenGLSprite()
{
	_glDeleteTextures(NUM_TEX, this->tex);
}

/**
 * Update a single mip-map level with new pixel data.
 * @param width Width of the level.
 * @param height Height of the level.
 * @param level Mip-map level.
 * @param data New pixel data.
 */
void OpenGLSprite::Update(uint width, uint height, uint level, const SpriteLoader::CommonPixel * data)
{
	static ReusableBuffer<Colour> buf_rgba;
	static ReusableBuffer<uint8_t> buf_pal;

	_glActiveTexture(GL_TEXTURE0);
	_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	_glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	if (this->tex[TEX_RGBA] != 0) {
		/* Unpack pixel data */
		size_t size = static_cast<size_t>(width) * height;
		Colour *rgba = buf_rgba.Allocate(size);
		for (size_t i = 0; i < size; i++) {
			rgba[i].r = data[i].r;
			rgba[i].g = data[i].g;
			rgba[i].b = data[i].b;
			rgba[i].a = data[i].a;
		}

		_glBindTexture(GL_TEXTURE_2D, this->tex[TEX_RGBA]);
		_glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, rgba);
	}

	if (this->tex[TEX_REMAP] != 0) {
		/* Unpack and align pixel data. */
		size_t pitch = Align(width, 4);

		uint8_t *pal = buf_pal.Allocate(pitch * height);
		const SpriteLoader::CommonPixel *row = data;
		for (uint y = 0; y < height; y++, pal += pitch, row += width) {
			for (uint x = 0; x < width; x++) {
				pal[x] = row[x].m;
			}
		}

		_glBindTexture(GL_TEXTURE_2D, this->tex[TEX_REMAP]);
		_glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, buf_pal.GetBuffer());
	}

	assert(_glGetError() == GL_NO_ERROR);
}

/**
 * Query the sprite size at a certain zoom level.
 * @param level The zoom level to query.
 * @return Sprite size at the given zoom level.
 */
inline Dimension OpenGLSprite::GetSize(ZoomLevel level) const
{
	Dimension sd = { (uint)UnScaleByZoomLower(this->dim.width, level), (uint)UnScaleByZoomLower(this->dim.height, level) };
	return sd;
}

/**
 * Bind textures for rendering this sprite.
 * @return True if the sprite has RGBA data.
 */
bool OpenGLSprite::BindTextures()
{
	_glActiveTexture(GL_TEXTURE0);
	_glBindTexture(GL_TEXTURE_2D, this->tex[TEX_RGBA] != 0 ? this->tex[TEX_RGBA] : OpenGLSprite::dummy_tex[TEX_RGBA]);
	_glActiveTexture(GL_TEXTURE0 + 2);
	_glBindTexture(GL_TEXTURE_2D, this->tex[TEX_REMAP] != 0 ? this->tex[TEX_REMAP] : OpenGLSprite::dummy_tex[TEX_REMAP]);

	return this->tex[TEX_RGBA] != 0;
}
