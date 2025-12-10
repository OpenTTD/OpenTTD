/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
	uint8_t position; ///< Position in UI.
	uint8_t offset; ///< Offset in bits into the CompanyManagerFace
	uint8_t length; ///< Number of bits used in the CompanyManagerFace
	uint8_t valid_values; ///< The number of valid values
	std::variant<SpriteID, uint64_t, std::pair<uint64_t, uint64_t>> data; ///< The first sprite
	StringID name = STR_NULL;

	/**
	 * Gets the company manager's face bits.
	 * @param cmf The face to extract the bits from.
	 * @return the requested bits
	 */
	inline uint GetBits(const CompanyManagerFace &cmf) const
	{
		return GB(cmf.bits, this->offset, this->length);
	}

	/**
	 * Sets the company manager's face bits.
	 * @param cmf The face to write the bits to.
	 * @param val The new value.
	 */
	inline void SetBits(CompanyManagerFace &cmf, uint val) const
	{
		SB(cmf.bits, this->offset, this->length, val);
	}

	/**
	 * Increase/Decrease the company manager's face variable by the given amount.
	 * The value wraps around to stay in the valid range.
	 * @param cmf The face to write the bits to.
	 * @param amount the amount to change the value
	 */
	inline void ChangeBits(CompanyManagerFace &cmf, int8_t amount) const
	{
		int8_t val = this->GetBits(cmf) + amount; // the new value for the cmfv

		/* scales the new value to the correct scope */
		while (val < 0) {
			val += this->valid_values;
		}
		val %= this->valid_values;

		this->SetBits(cmf, val); // save the new value
	}

	/**
	 * Checks whether the company manager's face bits have a valid range
	 * @param cmf The face to check.
	 * @return true if and only if the bits are valid
	 */
	inline bool IsValid(const CompanyManagerFace &cmf) const
	{
		return GB(cmf.bits, this->offset, this->length) < this->valid_values;
	}

	/**
	 * Scales a company manager's face bits variable to the correct scope
	 * @param vars The face variables of the face style.
	 * @pre val < (1U << length), i.e. val has a value of 0..2^(bits used for this variable)-1
	 * @return the scaled value
	 */
	inline uint ScaleBits(uint val) const
	{
		assert(val < (1U << this->length));
		return (val * this->valid_values) >> this->length;
	}

	/**
	 * Gets the sprite to draw.
	 * @param cmf The face to extract the data from
	 * @pre vars[var].type == FaceVarType::Sprite.
	 * @return sprite to draw
	 */
	inline SpriteID GetSprite(const CompanyManagerFace &cmf) const
	{
		assert(this->type == FaceVarType::Sprite);
		return std::get<SpriteID>(this->data) + this->GetBits(cmf);
	}
};

using FaceVars = std::span<const FaceVar>;

struct FaceSpec {
	std::string label;
	std::variant<FaceVars, std::vector<FaceVar>> face_vars;

	inline FaceVars GetFaceVars() const
	{
		struct visitor {
			FaceVars operator()(FaceVars vars) const { return vars; }
			FaceVars operator()(const std::vector<FaceVar> &vars) const { return vars; }
		};
		return std::visit(visitor{}, this->face_vars);
	}
};

void ResetFaces();
uint GetNumCompanyManagerFaceStyles();
std::optional<uint> FindCompanyManagerFaceLabel(std::string_view label);
const FaceSpec *GetCompanyManagerFaceSpec(uint style_index);
FaceVars GetCompanyManagerFaceVars(uint style_index);

/**
 * Get a bitmask of currently active face variables.
 * Face variables can be inactive due to toggles in the face variables.
 * @param cmf The company manager face.
 * @param vars The face variables of the face.
 * @return Currently active face variables for the face.
 */
inline uint64_t GetActiveFaceVars(const CompanyManagerFace &cmf, FaceVars vars)
{
	uint64_t active_vars = (1ULL << std::size(vars)) - 1ULL;

	for (const auto &info : vars) {
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
inline void ScaleAllCompanyManagerFaceBits(CompanyManagerFace &cmf, FaceVars vars)
{
	for (auto var : SetBitIterator(GetActiveFaceVars(cmf, vars))) {
		vars[var].ChangeBits(cmf, 0);
	}
}

/**
 * Make a random new face without changing the face style.
 * @param cmf The company manager's face to write the bits to
 * @param vars The face variables.
 * @param randomizer The source of random to use for creating the manager face
 */
inline void RandomiseCompanyManagerFaceBits(CompanyManagerFace &cmf, FaceVars vars, Randomizer &randomizer)
{
	cmf.bits = randomizer.Next();
	ScaleAllCompanyManagerFaceBits(cmf, vars);
}

void SetCompanyManagerFaceStyle(CompanyManagerFace &cmf, uint style);
void RandomiseCompanyManagerFace(CompanyManagerFace &cmf, Randomizer &randomizer);
uint32_t MaskCompanyManagerFaceBits(const CompanyManagerFace &cmf, FaceVars vars);
std::string FormatCompanyManagerFaceCode(const CompanyManagerFace &cmf);
std::optional<CompanyManagerFace> ParseCompanyManagerFaceCode(std::string_view str);

void DrawCompanyManagerFace(const CompanyManagerFace &cmf, Colours colour, const Rect &r);

#endif /* COMPANY_MANAGER_FACE_H */
