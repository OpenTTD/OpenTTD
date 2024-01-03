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
#include "../spriteloader/spriteloader.hpp"
#include "../misc/lrucache.hpp"

typedef void (*OGLProc)();
typedef OGLProc (*GetOGLProcAddressProc)(const char *proc);

bool IsOpenGLVersionAtLeast(byte major, byte minor);
const char *FindStringInExtensionList(const char *string, const char *substring);

class OpenGLSprite;

/** Platform-independent back-end class for OpenGL video drivers. */
class OpenGLBackend : public ZeroedMemoryAllocator, SpriteEncoder {
private:
	static OpenGLBackend *instance; ///< Singleton instance pointer.

	bool persistent_mapping_supported; ///< Persistent pixel buffer mapping supported.
	GLsync sync_vid_mapping;           ///< Sync object for the persistently mapped video buffer.
	GLsync sync_anim_mapping;          ///< Sync object for the persistently mapped animation buffer.

	void *vid_buffer;   ///< Pointer to the mapped video buffer.
	GLuint vid_pbo;     ///< Pixel buffer object storing the memory used for the video driver to draw to.
	GLuint vid_texture; ///< Texture handle for the video buffer texture.
	GLuint vid_program; ///< Shader program for rendering a RGBA video buffer.
	GLuint pal_program; ///< Shader program for rendering a paletted video buffer.
	GLuint vao_quad;    ///< Vertex array object storing the rendering state for the fullscreen quad.
	GLuint vbo_quad;    ///< Vertex buffer with a fullscreen quad.
	GLuint pal_texture; ///< Palette lookup texture.

	void *anim_buffer;   ///< Pointer to the mapped animation buffer.
	GLuint anim_pbo;     ///< Pixel buffer object storing the memory used for the animation buffer.
	GLuint anim_texture; ///< Texture handle for the animation buffer texture.

	GLuint remap_program;    ///< Shader program for blending and rendering a RGBA + remap texture.
	GLint  remap_sprite_loc; ///< Uniform location for sprite parameters.
	GLint  remap_screen_loc; ///< Uniform location for screen size.
	GLint  remap_zoom_loc;   ///< Uniform location for sprite zoom.
	GLint  remap_rgb_loc;    ///< Uniform location for RGB mode flag.

	GLuint sprite_program;    ///< Shader program for blending and rendering a sprite to the video buffer.
	GLint  sprite_sprite_loc; ///< Uniform location for sprite parameters.
	GLint  sprite_screen_loc; ///< Uniform location for screen size.
	GLint  sprite_zoom_loc;   ///< Uniform location for sprite zoom.
	GLint  sprite_rgb_loc;    ///< Uniform location for RGB mode flag.
	GLint  sprite_crash_loc;  ///< Uniform location for crash remap mode flag.

	LRUCache<SpriteID, Sprite> cursor_cache;   ///< Cache of encoded cursor sprites.
	PaletteID last_sprite_pal = (PaletteID)-1; ///< Last uploaded remap palette.
	bool clear_cursor_cache = false;           ///< A clear of the cursor cache is pending.

	Point cursor_pos;                    ///< Cursor position
	bool cursor_in_window;               ///< Cursor inside this window
	PalSpriteID cursor_sprite_seq[16];   ///< Current image of cursor
	Point cursor_sprite_pos[16];         ///< Relative position of individual cursor sprites
	uint cursor_sprite_count;            ///< Number of cursor sprites to draw

	OpenGLBackend();
	~OpenGLBackend();

	const char *Init(const Dimension &screen_res);
	bool InitShaders();

	void InternalClearCursorCache();

	void RenderOglSprite(OpenGLSprite *gl_sprite, PaletteID pal, int x, int y, ZoomLevel zoom);

public:
	/** Get singleton instance of this class. */
	static inline OpenGLBackend *Get()
	{
		return OpenGLBackend::instance;
	}
	static const char *Create(GetOGLProcAddressProc get_proc, const Dimension &screen_res);
	static void Destroy();

	void PrepareContext();

	std::string GetDriverName();

	void UpdatePalette(const Colour *pal, uint first, uint length);
	bool Resize(int w, int h, bool force = false);
	void Paint();

	void DrawMouseCursor();
	void PopulateCursorCache();
	void ClearCursorCache();

	void *GetVideoBuffer();
	uint8_t *GetAnimBuffer();
	void ReleaseVideoBuffer(const Rect &update_rect);
	void ReleaseAnimBuffer(const Rect &update_rect);

	/* SpriteEncoder */

	bool Is32BppSupported() override { return true; }
	uint GetSpriteAlignment() override { return 1u << (ZOOM_LVL_END - 1); }
	Sprite *Encode(const SpriteLoader::SpriteCollection &sprite, AllocatorProc *allocator) override;
};


/** Class that encapsulates a RGBA texture together with a paletted remap texture. */
class OpenGLSprite {
private:
	/** Enum of all used OpenGL texture objects. */
	enum Texture {
		TEX_RGBA,    ///< RGBA texture part.
		TEX_REMAP,   ///< Remap texture part.
		NUM_TEX
	};

	Dimension dim;
	GLuint tex[NUM_TEX]; ///< The texture objects.

	static GLuint dummy_tex[NUM_TEX]; ///< 1x1 dummy textures to substitute for unused sprite components.

	static GLuint pal_identity; ///< Identity texture mapping.
	static GLuint pal_tex;      ///< Texture for palette remap.
	static GLuint pal_pbo;      ///< Pixel buffer object for remap upload.

	static bool Create();
	static void Destroy();

	bool BindTextures();

public:
	OpenGLSprite(uint width, uint height, uint levels, SpriteColourComponent components);
	~OpenGLSprite();

	void Update(uint width, uint height, uint level, const SpriteLoader::CommonPixel *data);
	Dimension GetSize(ZoomLevel level) const;

	friend class OpenGLBackend;
};

#endif /* VIDEO_OPENGL_H */
