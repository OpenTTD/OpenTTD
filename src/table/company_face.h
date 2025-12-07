/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file table/company_face.h
 * This file contains all definitions for default company faces.
 */

#ifndef TABLE_COMPANY_FACE_H
#define TABLE_COMPANY_FACE_H

#include "../company_manager_face.h"

/* Definitions for default face variables.
 * Faces are drawn in the listed order, so sprite layers must be ordered
 * according to how the face should be rendered. */

/** Variables of first masculine face. */
static constexpr FaceVar _face_style_1[] = {
	{FaceVarType::Toggle, 3, 3, 1, 2, std::pair{1ULL << 6, 1ULL << 7 | 1ULL << 8}, STR_FACE_MOUSTACHE},
	{FaceVarType::Toggle, 11, 4, 1, 2, std::pair{1ULL << 13, 0ULL}, STR_FACE_GLASSES},
	{FaceVarType::Palette, 2, 5, 2, 3, uint64_t{1ULL << 5}, STR_FACE_EYECOLOUR},
	{FaceVarType::Sprite, 0, 0, 0, 1, SpriteID{0x325}},
	{FaceVarType::Sprite, 7, 7, 2, 4, SpriteID{0x327}, STR_FACE_CHIN},
	{FaceVarType::Sprite, 1, 9, 4, 12, SpriteID{0x32B}, STR_FACE_EYEBROWS},
	{FaceVarType::Sprite, 4, 13, 2, 3, SpriteID{0x367}, STR_FACE_MOUSTACHE},
	{FaceVarType::Sprite, 6, 13, 4, 12, SpriteID{0x35B}, STR_FACE_LIPS},
	{FaceVarType::Sprite, 5, 17, 3, 8, SpriteID{0x349}, STR_FACE_NOSE},
	{FaceVarType::Sprite, 0, 20, 4, 9, SpriteID{0x382}, STR_FACE_HAIR},
	{FaceVarType::Sprite, 9, 26, 2, 4, SpriteID{0x36E}, STR_FACE_COLLAR},
	{FaceVarType::Sprite, 8, 24, 2, 3, SpriteID{0x36B}, STR_FACE_JACKET},
	{FaceVarType::Sprite, 10, 28, 3, 6, SpriteID{0x372}, STR_FACE_TIE},
	{FaceVarType::Sprite, 12, 31, 1, 2, SpriteID{0x347}, STR_FACE_GLASSES},
};

/** Variables of first feminine face. */
static constexpr FaceVar _face_style_2[] = {
	{FaceVarType::Toggle, 9, 3, 1, 2, std::pair{1ULL << 11, 0}, STR_FACE_EARRING},
	{FaceVarType::Toggle, 7, 4, 1, 2, std::pair{1ULL << 12, 0}, STR_FACE_GLASSES},
	{FaceVarType::Palette, 2, 5, 2, 3, uint64_t{1ULL << 5}, STR_FACE_EYECOLOUR},
	{FaceVarType::Sprite, 0, 0, 0, 1, SpriteID{0x326}},
	{FaceVarType::Sprite, 0, 0, 0, 1, SpriteID{0x327}},
	{FaceVarType::Sprite, 1, 9, 4, 16, SpriteID{0x337}, STR_FACE_EYEBROWS},
	{FaceVarType::Sprite, 4, 13, 4, 10, SpriteID{0x351}, STR_FACE_LIPS},
	{FaceVarType::Sprite, 3, 17, 3, 4, SpriteID{0x34C}, STR_FACE_NOSE},
	{FaceVarType::Sprite, 0, 20, 4, 5, SpriteID{0x38B}, STR_FACE_HAIR},
	{FaceVarType::Sprite, 6, 26, 2, 4, SpriteID{0x37B}, STR_FACE_COLLAR},
	{FaceVarType::Sprite, 5, 24, 2, 3, SpriteID{0x378}, STR_FACE_JACKET},
	{FaceVarType::Sprite, 10, 28, 3, 3, SpriteID{0x37F}, STR_FACE_EARRING},
	{FaceVarType::Sprite, 8, 31, 1, 2, SpriteID{0x347}, STR_FACE_GLASSES},
};

/** Variables of second masculine face. */
static constexpr FaceVar _face_style_3[] = {
	{FaceVarType::Toggle, 2, 3, 1, 2, std::pair{1ULL << 5, 1ULL << 6 | 1ULL << 7}, STR_FACE_MOUSTACHE},
	{FaceVarType::Toggle, 10, 4, 1, 2, std::pair{1ULL << 12, 0ULL}, STR_FACE_GLASSES},
	{FaceVarType::Sprite, 0, 0, 0, 1, SpriteID{0x390}},
	{FaceVarType::Sprite, 6, 7, 2, 2, SpriteID{0x391}, STR_FACE_CHIN},
	{FaceVarType::Sprite, 1, 9, 4, 11, SpriteID{0x39A}, STR_FACE_EYEBROWS},
	{FaceVarType::Sprite, 3, 13, 2, 3, SpriteID{0x397}, STR_FACE_MOUSTACHE},
	{FaceVarType::Sprite, 5, 13, 4, 9, SpriteID{0x3A5}, STR_FACE_LIPS},
	{FaceVarType::Sprite, 4, 17, 3, 4, SpriteID{0x393}, STR_FACE_NOSE},
	{FaceVarType::Sprite, 0, 20, 4, 5, SpriteID{0x3D4}, STR_FACE_HAIR},
	{FaceVarType::Sprite, 8, 26, 2, 4, SpriteID{0x36E}, STR_FACE_COLLAR},
	{FaceVarType::Sprite, 7, 24, 2, 3, SpriteID{0x36B}, STR_FACE_JACKET},
	{FaceVarType::Sprite, 9, 28, 3, 6, SpriteID{0x372}, STR_FACE_TIE},
	{FaceVarType::Sprite, 11, 31, 1, 2, SpriteID{0x3AE}, STR_FACE_GLASSES},
};

/** Variables of second feminine face. */
static constexpr FaceVar _face_style_4[] = {
	{FaceVarType::Toggle, 9, 3, 1, 2, std::pair{1ULL << 10, 0ULL}, STR_FACE_EARRING},
	{FaceVarType::Toggle, 7, 4, 1, 2, std::pair{1ULL << 11, 0ULL}, STR_FACE_GLASSES},
	{FaceVarType::Sprite, 0, 0, 0, 1, SpriteID{0x3B0}},
	{FaceVarType::Sprite, 4, 7, 2, 2, SpriteID{0x3B1}, STR_FACE_CHIN},
	{FaceVarType::Sprite, 1, 9, 4, 16, SpriteID{0x3B8}, STR_FACE_EYEBROWS},
	{FaceVarType::Sprite, 3, 13, 4, 9, SpriteID{0x3C8}, STR_FACE_LIPS},
	{FaceVarType::Sprite, 2, 17, 3, 5, SpriteID{0x3B3}, STR_FACE_NOSE},
	{FaceVarType::Sprite, 0, 20, 4, 5, SpriteID{0x3D9}, STR_FACE_HAIR},
	{FaceVarType::Sprite, 6, 26, 2, 4, SpriteID{0x37B}, STR_FACE_COLLAR},
	{FaceVarType::Sprite, 5, 24, 2, 3, SpriteID{0x378}, STR_FACE_JACKET},
	{FaceVarType::Sprite, 10, 28, 3, 3, SpriteID{0x3D1}, STR_FACE_EARRING},
	{FaceVarType::Sprite, 8, 31, 1, 2, SpriteID{0x3AE}, STR_FACE_GLASSES},
};

/** Original face styles. */
static FaceSpec _original_faces[] = {
	{"default/face1", _face_style_1},
	{"default/face2", _face_style_2},
	{"default/face3", _face_style_3},
	{"default/face4", _face_style_4},
};

#endif /* TABLE_COMPANY_FACE_H */
