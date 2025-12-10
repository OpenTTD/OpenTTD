/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file opengl.h OpenGL video driver support. */

#ifndef VIDEO_OPENGL_H
#define VIDEO_OPENGL_H

#include "../core/geometry_type.hpp"
#include "../gfx_type.h"
#include "../spriteloader/spriteloader.hpp"
#include "../misc/lrucache.hpp"

typedef void (*OGLProc)();
typedef OGLProc (*GetOGLProcAddressProc)(const char *proc);

bool IsOpenGLVersionAtLeast(uint8_t major, uint8_t minor);
bool HasStringInExtensionList(std::string_view string, std::string_view substring);

class OpenGLSprite;

using OpenGLSpriteLRUCache = LRUCache<SpriteID, std::unique_ptr<OpenGLSprite>>;

/** Platform-independent back-end class for OpenGL video drivers. */
class OpenGLBackend : public SpriteEncoder {
private:
	static OpenGLBackend *instance; ///< Singleton instance pointer.

	bool persistent_mapping_supported = false; ///< Persistent pixel buffer mapping supported.
	GLsync sync_vid_mapping{}; ///< Sync object for the persistently mapped video buffer.
	GLsync sync_anim_mapping{}; ///< Sync object for the persistently mapped animation buffer.

	void *vid_buffer = nullptr; ///< Pointer to the mapped video buffer.
	GLuint vid_pbo = 0; ///< Pixel buffer object storing the memory used for the video driver to draw to.
	GLuint vid_texture = 0; ///< Texture handle for the video buffer texture.
	GLuint vid_program = 0; ///< Shader program for rendering a RGBA video buffer.
	GLuint pal_program = 0; ///< Shader program for rendering a paletted video buffer.
	GLuint vao_quad = 0; ///< Vertex array object storing the rendering state for the fullscreen quad.
	GLuint vbo_quad = 0; ///< Vertex buffer with a fullscreen quad.
	GLuint pal_texture = 0; ///< Palette lookup texture.

	void *anim_buffer = nullptr; ///< Pointer to the mapped animation buffer.
	GLuint anim_pbo = 0; ///< Pixel buffer object storing the memory used for the animation buffer.
	GLuint anim_texture = 0; ///< Texture handle for the animation buffer texture.

	GLuint remap_program = 0; ///< Shader program for blending and rendering a RGBA + remap texture.
	GLint  remap_sprite_loc = 0; ///< Uniform location for sprite parameters.
	GLint  remap_screen_loc = 0; ///< Uniform location for screen size.
	GLint  remap_zoom_loc = 0; ///< Uniform location for sprite zoom.
	GLint  remap_rgb_loc = 0; ///< Uniform location for RGB mode flag.

	GLuint sprite_program = 0; ///< Shader program for blending and rendering a sprite to the video buffer.
	GLint  sprite_sprite_loc = 0; ///< Uniform location for sprite parameters.
	GLint  sprite_screen_loc = 0; ///< Uniform location for screen size.
	GLint  sprite_zoom_loc = 0; ///< Uniform location for sprite zoom.
	GLint  sprite_rgb_loc = 0; ///< Uniform location for RGB mode flag.
	GLint  sprite_crash_loc = 0; ///< Uniform location for crash remap mode flag.

	OpenGLSpriteLRUCache cursor_cache; ///< Cache of encoded cursor sprites.
	PaletteID last_sprite_pal = (PaletteID)-1; ///< Last uploaded remap palette.
	bool clear_cursor_cache = false; ///< A clear of the cursor cache is pending.

	Point cursor_pos{}; ///< Cursor position
	bool cursor_in_window = false; ///< Cursor inside this window
	std::vector<CursorSprite> cursor_sprites{}; ///< Sprites comprising cursor

	OpenGLBackend();
	~OpenGLBackend();

	std::optional<std::string_view> Init(const Dimension &screen_res);
	bool InitShaders();

	void InternalClearCursorCache();

	void RenderOglSprite(const OpenGLSprite *gl_sprite, PaletteID pal, int x, int y, ZoomLevel zoom);

public:
	/** Get singleton instance of this class. */
	static inline OpenGLBackend *Get()
	{
		return OpenGLBackend::instance;
	}
	static std::optional<std::string_view> Create(GetOGLProcAddressProc get_proc, const Dimension &screen_res);
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
	uint GetSpriteAlignment() override { return 1u << to_underlying(ZoomLevel::Max); }
	Sprite *Encode(SpriteType sprite_type, const SpriteLoader::SpriteCollection &sprite, SpriteAllocator &allocator) override;
};


/** Class that encapsulates a RGBA texture together with a paletted remap texture. */
class OpenGLSprite {
private:
	/** Enum of all used OpenGL texture objects. */
	enum Texture : uint8_t {
		TEX_RGBA,    ///< RGBA texture part.
		TEX_REMAP,   ///< Remap texture part.
		NUM_TEX
	};

	Dimension dim{};
	std::array<GLuint, NUM_TEX> tex{}; ///< The texture objects.
	int16_t x_offs = 0;  ///< Number of pixels to shift the sprite to the right.
	int16_t y_offs = 0;  ///< Number of pixels to shift the sprite downwards.

	static std::array<GLuint, NUM_TEX> dummy_tex; ///< 1x1 dummy textures to substitute for unused sprite components.

	static GLuint pal_identity; ///< Identity texture mapping.
	static GLuint pal_tex;      ///< Texture for palette remap.
	static GLuint pal_pbo;      ///< Pixel buffer object for remap upload.

	static bool Create();
	static void Destroy();

	bool BindTextures() const;

public:
	OpenGLSprite(SpriteType sprite_type, const SpriteLoader::SpriteCollection &sprite);

	/* No support for moving/copying the textures is implemented. */
	OpenGLSprite(const OpenGLSprite&) = delete;
	OpenGLSprite(OpenGLSprite&&) = delete;
	OpenGLSprite& operator=(const OpenGLSprite&) = delete;
	OpenGLSprite& operator=(OpenGLSprite&&) = delete;
	~OpenGLSprite();

	void Update(uint width, uint height, uint level, const SpriteLoader::CommonPixel *data);
	Dimension GetSize(ZoomLevel level) const;

	friend class OpenGLBackend;
};

#endif /* VIDEO_OPENGL_H */
