/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file company_manager_face.h Functionality related to the company manager's face */

#ifndef COMPANY_MANAGER_FACE_H
#define COMPANY_MANAGER_FACE_H

#include "core/random_func.hpp"
#include "core/bitmath_func.hpp"
#include "strings_type.h"
#include "company_type.h"
#include "gfx_type.h"

#include "table/strings.h"

enum class FaceVarType : uint8_t {
	Sprite,
	Palette,
	Toggle,
};

/** Information about the valid values of CompanyManagerFace bitgroups as well as the sprites to draw */
struct FaceVar {
	FaceVarType type;
	uint8_t offset; ///< Offset in bits into the CompanyManagerFace
	uint8_t length; ///< Number of bits used in the CompanyManagerFace
	uint8_t valid_values; ///< The number of valid values
	std::variant<SpriteID, uint64_t, std::pair<uint64_t, uint64_t>> data; ///< The first sprite
	StringID name = STR_NULL;
};

using FaceStyle = std::span<const FaceVar>;

struct FaceSpec {
	std::string label;
	std::variant<FaceStyle, std::vector<FaceVar>> face_vars;

	inline FaceStyle GetFaceStyle() const
	{
		struct visitor {
			FaceStyle operator()(FaceStyle vars) const { return vars; }
			FaceStyle operator()(const std::vector<FaceVar> &vars) const { return vars; }
		};
		return std::visit(visitor{}, this->face_vars);
	}
};

void ResetFaces();
uint GetNumCompanyManagerFaceStyles();
std::optional<uint> FindCompanyManagerFaceLabel(std::string_view label);
const FaceSpec *GetCompanyManagerFaceSpec(uint style_index);
FaceStyle GetCompanyManagerFaceStyle(uint style_index);

/**
 * Gets the company manager's face bits for the given company manager's face variable
 * @param cmf  the face to extract the bits from
 * @param cmfv the face variable to get the data of
 * @param ge   the gender and ethnicity of the face
 * @pre _cmf_info[cmfv].valid_values[ge] != 0
 * @return the requested bits
 */
inline uint GetCompanyManagerFaceBits(CompanyManagerFace cmf, FaceStyle style, uint8_t var)
{
	assert(var < std::size(style));
	return GB(cmf.bits, style[var].offset, style[var].length);
}

/**
 * Sets the company manager's face bits for the given company manager's face variable
 * @param cmf  the face to write the bits to
 * @param cmfv the face variable to write the data of
 * @param ge   the gender and ethnicity of the face
 * @param val  the new value
 * @pre val < _cmf_info[cmfv].valid_values[ge]
 */
inline void SetCompanyManagerFaceBits(CompanyManagerFace &cmf, FaceStyle style, uint8_t var, uint val)
{
	assert(var < std::size(style));
	SB(cmf.bits, style[var].offset, style[var].length, val);
}

/**
 * Increase/Decrease the company manager's face variable by the given amount.
 * The value wraps around to stay in the valid range.
 *
 * @param cmf    the company manager face to write the bits to
 * @param cmfv   the company manager face variable to write the data of
 * @param ge     the gender and ethnicity of the company manager's face
 * @param amount the amount which change the value
 *
 * @pre 0 <= val < _cmf_info[cmfv].valid_values[ge]
 */
inline void IncreaseCompanyManagerFaceBits(CompanyManagerFace &cmf, FaceStyle style, uint8_t var, int8_t amount)
{
	int8_t val = GetCompanyManagerFaceBits(cmf, style, var) + amount; // the new value for the cmfv

	/* scales the new value to the correct scope */
	while (val < 0) {
		val += style[var].valid_values;
	}
	val %= style[var].valid_values;

	SetCompanyManagerFaceBits(cmf, style, var, val); // save the new value
}

/**
 * Checks whether the company manager's face bits have a valid range
 * @param cmf  the face to extract the bits from
 * @param cmfv the face variable to get the data of
 * @param ge   the gender and ethnicity of the face
 * @return true if and only if the bits are valid
 */
inline bool AreCompanyManagerFaceBitsValid(CompanyManagerFace cmf, FaceStyle style, uint8_t var)
{
	assert(var < std::size(style));
	return GB(cmf.bits, style[var].offset, style[var].length) < style[var].valid_values;
}

/**
 * Scales a company manager's face bits variable to the correct scope
 * @param cmfv the face variable to write the data of
 * @param ge  the gender and ethnicity of the face
 * @param val the to value to scale
 * @pre val < (1U << _cmf_info[cmfv].length), i.e. val has a value of 0..2^(bits used for this variable)-1
 * @return the scaled value
 */
inline uint ScaleCompanyManagerFaceValue(FaceStyle style, uint8_t var, uint val)
{
	assert(var < std::size(style));
	assert(val < (1U << style[var].length));

	return (val * style[var].valid_values) >> style[var].length;
}

inline uint64_t GetActiveFaceVars(const CompanyManagerFace &cmf, FaceStyle style)
{
	uint64_t active_vars = (1ULL << std::size(style)) - 1ULL;

	for (const auto &info : style) {
		if (info.type != FaceVarType::Toggle) continue;
		const auto &[off, on] = std::get<std::pair<uint64_t, uint64_t>>(info.data);
		active_vars &= ~(HasBit(cmf.bits, info.offset) ? on : off);
	}

	return active_vars;
}

/**
 * Scales all company manager's face bits to the correct scope
 *
 * @param cmf the company manager's face to write the bits to
 */
inline void ScaleAllCompanyManagerFaceBits(CompanyManagerFace &cmf, FaceStyle style)
{
	for (auto var : SetBitIterator(GetActiveFaceVars(cmf, style))) {
		IncreaseCompanyManagerFaceBits(cmf, style, var, 0);
	}
}

/**
 * Make a random new face.
 * If it is for the advanced company manager's face window then the new face have the same gender
 * and ethnicity as the old one, else the gender is equal and the ethnicity is random.
 *
 * @param cmf the company manager's face to write the bits to
 * @param style the face style.
 * @param randomizer the source of random to use for creating the manager face
 *
 * @pre scale 'ge' to a valid gender/ethnicity combination
 */
inline void RandomCompanyManagerFaceBits(CompanyManagerFace &cmf, FaceStyle style, Randomizer &randomizer)
{
	cmf.bits = randomizer.Next(); // random all company manager's face bits

	/* scales all company manager's face bits to the correct scope */
	ScaleAllCompanyManagerFaceBits(cmf, style);
}

/**
 * Gets the sprite to draw for the given company manager's face variable
 * @param cmf  the face to extract the data from
 * @param style the face style.
 * @param var the face variable to get the sprite of.
 * @pre _cmf_info[cmfv].type == CompanyManagerFaceBitsType::Sprite
 * @return sprite to draw
 */
inline SpriteID GetCompanyManagerFaceSprite(const CompanyManagerFace &cmf, FaceStyle style, uint8_t var)
{
	assert(var < std::size(style));
	assert(style[var].type == FaceVarType::Sprite);

	return std::get<SpriteID>(style[var].data) + GB(cmf.bits, style[var].offset, style[var].length);
}

uint32_t MaskCompanyManagerFaceBits(const CompanyManagerFace &cmf, FaceStyle style);
std::string FormatCompanyManagerFaceString(const CompanyManagerFace &cmf);
std::optional<CompanyManagerFace> ParseCompanyManagerFaceString(std::string_view str);

void DrawCompanyManagerFace(const CompanyManagerFace &cmf, Colours colour, const Rect &r);

#endif /* COMPANY_MANAGER_FACE_H */
