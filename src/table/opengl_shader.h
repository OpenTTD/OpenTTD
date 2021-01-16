/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file opengl_shader.h OpenGL shader programs. */

/** Vertex shader that positions a sprite on screen.. */
static const char *_vertex_shader_sprite[] = {
	"#version 110\n",
	"uniform vec4 sprite;",
	"uniform vec2 screen;",
	"attribute vec2 position, colour_uv;",
	"varying vec2 colour_tex_uv;",
	"void main() {",
	"  vec2 size = sprite.zw / screen.xy;",
	"  vec2 offset = ((2.0 * sprite.xy + sprite.zw) / screen.xy - 1.0) * vec2(1.0, -1.0);",
	"  colour_tex_uv = colour_uv;",
	"  gl_Position = vec4(position * size + offset, 0.0, 1.0);",
	"}",
};

/** GLSL 1.50 vertex shader that positions a sprite on screen. */
static const char *_vertex_shader_sprite_150[] = {
	"#version 150\n",
	"uniform vec4 sprite;",
	"uniform vec2 screen;",
	"in vec2 position, colour_uv;",
	"out vec2 colour_tex_uv;",
	"void main() {",
	"  vec2 size = sprite.zw / screen.xy;",
	"  vec2 offset = ((2.0 * sprite.xy + sprite.zw) / screen.xy - 1.0) * vec2(1.0, -1.0);",
	"  colour_tex_uv = colour_uv;",
	"  gl_Position = vec4(position * size + offset, 0.0, 1.0);",
	"}",
};

/** Fragment shader that reads the fragment colour from a 32bpp texture. */
static const char *_frag_shader_direct[] = {
	"#version 110\n",
	"uniform sampler2D colour_tex;",
	"varying vec2 colour_tex_uv;",
	"void main() {",
	"  gl_FragData[0] = texture2D(colour_tex, colour_tex_uv);",
	"}",
};

/** GLSL 1.50 fragment shader that reads the fragment colour from a 32bpp texture. */
static const char *_frag_shader_direct_150[] = {
	"#version 150\n",
	"uniform sampler2D colour_tex;",
	"in vec2 colour_tex_uv;",
	"out vec4 colour;",
	"void main() {",
	"  colour = texture(colour_tex, colour_tex_uv);",
	"}",
};

/** Fragment shader that performs a palette lookup to read the colour from an 8bpp texture. */
static const char *_frag_shader_palette[] = {
	"#version 110\n",
	"uniform sampler2D colour_tex;",
	"uniform sampler1D palette;",
	"varying vec2 colour_tex_uv;",
	"void main() {",
	"  float idx = texture2D(colour_tex, colour_tex_uv).r;",
	"  gl_FragData[0] = texture1D(palette, idx);",
	"}",
};

/** GLSL 1.50 fragment shader that performs a palette lookup to read the colour from an 8bpp texture. */
static const char *_frag_shader_palette_150[] = {
	"#version 150\n",
	"uniform sampler2D colour_tex;",
	"uniform sampler1D palette;",
	"in vec2 colour_tex_uv;",
	"out vec4 colour;",
	"void main() {",
	"  float idx = texture(colour_tex, colour_tex_uv).r;",
	"  colour = texture(palette, idx);",
	"}",
};


/** Fragment shader function for remap brightness modulation. */
static const char *_frag_shader_remap_func = \
	"float max3(vec3 v) {"
	"  return max(max(v.x, v.y), v.z);"
	"}"
	""
	"vec3 adj_brightness(vec3 colour, float brightness) {"
	"  vec3 adj = colour * (brightness > 0.0 ? brightness / 0.5 : 1.0);"
	"  vec3 ob_vec = clamp(adj - 1.0, 0.0, 1.0);"
	"  float ob = (ob_vec.r + ob_vec.g + ob_vec.b) / 2.0;"
	""
	"  return clamp(adj + ob * (1.0 - adj), 0.0, 1.0);"
	"}";

/** Fragment shader that performs a palette lookup to read the colour from an 8bpp texture. */
static const char *_frag_shader_rgb_mask_blend[] = {
	"#version 110\n",
	"#extension GL_ATI_shader_texture_lod: enable\n",
	"#extension GL_ARB_shader_texture_lod: enable\n",
	"uniform sampler2D colour_tex;",
	"uniform sampler1D palette;",
	"uniform sampler2D remap_tex;",
	"uniform bool rgb;",
	"uniform float zoom;",
	"varying vec2 colour_tex_uv;",
	"",
	_frag_shader_remap_func,
	"",
	"void main() {",
	"  float idx = texture2DLod(remap_tex, colour_tex_uv, zoom).r;",
	"  vec4 remap_col = texture1D(palette, idx);",
	"  vec4 rgb_col = texture2DLod(colour_tex, colour_tex_uv, zoom);",
	"",
	"  gl_FragData[0].a = rgb ? rgb_col.a : remap_col.a;",
	"  gl_FragData[0].rgb = idx > 0.0 ? adj_brightness(remap_col.rgb, max3(rgb_col.rgb)) : rgb_col.rgb;",
	"}",
};

/** GLSL 1.50 fragment shader that performs a palette lookup to read the colour from an 8bpp texture. */
static const char *_frag_shader_rgb_mask_blend_150[] = {
	"#version 150\n",
	"uniform sampler2D colour_tex;",
	"uniform sampler1D palette;",
	"uniform sampler2D remap_tex;",
	"uniform float zoom;",
	"uniform bool rgb;",
	"in vec2 colour_tex_uv;",
	"out vec4 colour;",
	"",
	_frag_shader_remap_func,
	"",
	"void main() {",
	"  float idx = textureLod(remap_tex, colour_tex_uv, zoom).r;",
	"  vec4 remap_col = texture(palette, idx);",
	"  vec4 rgb_col = textureLod(colour_tex, colour_tex_uv, zoom);",
	"",
	"  colour.a = rgb ? rgb_col.a : remap_col.a;",
	"  colour.rgb = idx > 0.0 ? adj_brightness(remap_col.rgb, max3(rgb_col.rgb)) : rgb_col.rgb;",
	"}",
};
