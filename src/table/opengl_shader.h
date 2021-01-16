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
	"void main() {",
	"  gl_TexCoord[0] = gl_MultiTexCoord0;",
	"  gl_Position = gl_Vertex;",
	"}",
};

/** Fragment shader that reads the fragment colour from a 32bpp texture. */
static const char *_frag_shader_direct[] = {
	"#version 110\n",
	"uniform sampler2D colour_tex;",
	"void main() {",
	"  gl_FragColor = texture2D(colour_tex, gl_TexCoord[0].st);",
	"}",
};
