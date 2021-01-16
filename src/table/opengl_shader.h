/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file opengl_shader.h OpenGL shader programs. */

/** Vertex shader that just passes colour and tex coords through. */
static const char *_vertex_shader_direct[] = {
	"#version 110\n",
	"attribute vec2 position, colour_uv;",
	"varying vec2 colour_tex_uv;",
	"void main() {",
	"  colour_tex_uv = colour_uv;",
	"  gl_Position = vec4(position, 0.0, 1.0);",
	"}",
};

/** GLSL 1.50 vertex shader that just passes colour and tex coords through. */
static const char *_vertex_shader_direct_150[] = {
	"#version 150\n",
	"in vec2 position, colour_uv;",
	"out vec2 colour_tex_uv;",
	"void main() {",
	"  colour_tex_uv = colour_uv;",
	"  gl_Position = vec4(position, 0.0, 1.0);",
	"}",
};

/** Fragment shader that reads the fragment colour from a 32bpp texture. */
static const char *_frag_shader_direct[] = {
	"#version 110\n",
	"uniform sampler2D colour_tex;",
	"varying vec2 colour_tex_uv;",
	"void main() {",
	"  gl_FragColor = texture2D(colour_tex, colour_tex_uv);",
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
