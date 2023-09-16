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
#include "table/sprites.h"
#include "company_type.h"

/** The gender/race combinations that we have faces for */
enum GenderEthnicity {
	GENDER_FEMALE    = 0, ///< This bit set means a female, otherwise male
	ETHNICITY_BLACK  = 1, ///< This bit set means black, otherwise white

	GE_WM = 0,                                         ///< A male of Caucasian origin (white)
	GE_WF = 1 << GENDER_FEMALE,                        ///< A female of Caucasian origin (white)
	GE_BM = 1 << ETHNICITY_BLACK,                      ///< A male of African origin (black)
	GE_BF = 1 << ETHNICITY_BLACK | 1 << GENDER_FEMALE, ///< A female of African origin (black)
	GE_END,
};
DECLARE_ENUM_AS_BIT_SET(GenderEthnicity) ///< See GenderRace as a bitset

/** Bitgroups of the CompanyManagerFace variable */
enum CompanyManagerFaceVariable {
	CMFV_GENDER,
	CMFV_ETHNICITY,
	CMFV_GEN_ETHN,
	CMFV_HAS_MOUSTACHE,
	CMFV_HAS_TIE_EARRING,
	CMFV_HAS_GLASSES,
	CMFV_EYE_COLOUR,
	CMFV_CHEEKS,
	CMFV_CHIN,
	CMFV_EYEBROWS,
	CMFV_MOUSTACHE,
	CMFV_LIPS,
	CMFV_NOSE,
	CMFV_HAIR,
	CMFV_JACKET,
	CMFV_COLLAR,
	CMFV_TIE_EARRING,
	CMFV_GLASSES,
	CMFV_END,
};
DECLARE_POSTFIX_INCREMENT(CompanyManagerFaceVariable)

/** Information about the valid values of CompanyManagerFace bitgroups as well as the sprites to draw */
struct CompanyManagerFaceBitsInfo {
	byte     offset;               ///< Offset in bits into the CompanyManagerFace
	byte     length;               ///< Number of bits used in the CompanyManagerFace
	byte     valid_values[GE_END]; ///< The number of valid values per gender/ethnicity
	SpriteID first_sprite[GE_END]; ///< The first sprite per gender/ethnicity
};

/** Lookup table for indices into the CompanyManagerFace, valid ranges and sprites */
static const CompanyManagerFaceBitsInfo _cmf_info[] = {
	/* Index                   off len   WM  WF  BM  BF         WM     WF     BM     BF
	 * CMFV_GENDER          */ {  0, 1, {  2,  2,  2,  2 }, {     0,     0,     0,     0 } }, ///< 0 = male, 1 = female
	/* CMFV_ETHNICITY       */ {  1, 2, {  2,  2,  2,  2 }, {     0,     0,     0,     0 } }, ///< 0 = (Western-)Caucasian, 1 = African(-American)/Black
	/* CMFV_GEN_ETHN        */ {  0, 3, {  4,  4,  4,  4 }, {     0,     0,     0,     0 } }, ///< Shortcut to get/set gender _and_ ethnicity
	/* CMFV_HAS_MOUSTACHE   */ {  3, 1, {  2,  0,  2,  0 }, {     0,     0,     0,     0 } }, ///< Females do not have a moustache
	/* CMFV_HAS_TIE_EARRING */ {  3, 1, {  0,  2,  0,  2 }, {     0,     0,     0,     0 } }, ///< Draw the earring for females or not. For males the tie is always drawn.
	/* CMFV_HAS_GLASSES     */ {  4, 1, {  2,  2,  2,  2 }, {     0,     0,     0,     0 } }, ///< Whether to draw glasses or not
	/* CMFV_EYE_COLOUR      */ {  5, 2, {  3,  3,  1,  1 }, {     0,     0,     0,     0 } }, ///< Palette modification
	/* CMFV_CHEEKS          */ {  0, 0, {  1,  1,  1,  1 }, { 0x325, 0x326, 0x390, 0x3B0 } }, ///< Cheeks are only indexed by their gender/ethnicity
	/* CMFV_CHIN            */ {  7, 2, {  4,  1,  2,  2 }, { 0x327, 0x327, 0x391, 0x3B1 } },
	/* CMFV_EYEBROWS        */ {  9, 4, { 12, 16, 11, 16 }, { 0x32B, 0x337, 0x39A, 0x3B8 } },
	/* CMFV_MOUSTACHE       */ { 13, 2, {  3,  0,  3,  0 }, { 0x367,     0, 0x397,     0 } }, ///< Depends on CMFV_HAS_MOUSTACHE
	/* CMFV_LIPS            */ { 13, 4, { 12, 10,  9,  9 }, { 0x35B, 0x351, 0x3A5, 0x3C8 } }, ///< Depends on !CMFV_HAS_MOUSTACHE
	/* CMFV_NOSE            */ { 17, 3, {  8,  4,  4,  5 }, { 0x349, 0x34C, 0x393, 0x3B3 } }, ///< Depends on !CMFV_HAS_MOUSTACHE
	/* CMFV_HAIR            */ { 20, 4, {  9,  5,  5,  5 }, { 0x382, 0x38B, 0x3D4, 0x3D9 } },
	/* CMFV_JACKET          */ { 24, 2, {  3,  3,  3,  3 }, { 0x36B, 0x378, 0x36B, 0x378 } },
	/* CMFV_COLLAR          */ { 26, 2, {  4,  4,  4,  4 }, { 0x36E, 0x37B, 0x36E, 0x37B } },
	/* CMFV_TIE_EARRING     */ { 28, 3, {  6,  3,  6,  3 }, { 0x372, 0x37F, 0x372, 0x3D1 } }, ///< Depends on CMFV_HAS_TIE_EARRING
	/* CMFV_GLASSES         */ { 31, 1, {  2,  2,  2,  2 }, { 0x347, 0x347, 0x3AE, 0x3AE } }  ///< Depends on CMFV_HAS_GLASSES
};
/** Make sure the table's size is right. */
static_assert(lengthof(_cmf_info) == CMFV_END);

/**
 * Gets the company manager's face bits for the given company manager's face variable
 * @param cmf  the face to extract the bits from
 * @param cmfv the face variable to get the data of
 * @param ge   the gender and ethnicity of the face
 * @pre _cmf_info[cmfv].valid_values[ge] != 0
 * @return the requested bits
 */
static inline uint GetCompanyManagerFaceBits(CompanyManagerFace cmf, CompanyManagerFaceVariable cmfv, [[maybe_unused]] GenderEthnicity ge)
{
	assert(_cmf_info[cmfv].valid_values[ge] != 0);

	return GB(cmf, _cmf_info[cmfv].offset, _cmf_info[cmfv].length);
}

/**
 * Sets the company manager's face bits for the given company manager's face variable
 * @param cmf  the face to write the bits to
 * @param cmfv the face variable to write the data of
 * @param ge   the gender and ethnicity of the face
 * @param val  the new value
 * @pre val < _cmf_info[cmfv].valid_values[ge]
 */
static inline void SetCompanyManagerFaceBits(CompanyManagerFace &cmf, CompanyManagerFaceVariable cmfv, [[maybe_unused]] GenderEthnicity ge, uint val)
{
	assert(val < _cmf_info[cmfv].valid_values[ge]);

	SB(cmf, _cmf_info[cmfv].offset, _cmf_info[cmfv].length, val);
}

/**
 * Increase/Decrease the company manager's face variable by the given amount.
 * If the new value greater than the max value for this variable it will be set to 0.
 * Or is it negative (< 0) it will be set to max value.
 *
 * @param cmf    the company manager face to write the bits to
 * @param cmfv   the company manager face variable to write the data of
 * @param ge     the gender and ethnicity of the company manager's face
 * @param amount the amount which change the value
 *
 * @pre 0 <= val < _cmf_info[cmfv].valid_values[ge]
 */
static inline void IncreaseCompanyManagerFaceBits(CompanyManagerFace &cmf, CompanyManagerFaceVariable cmfv, GenderEthnicity ge, int8_t amount)
{
	int8_t val = GetCompanyManagerFaceBits(cmf, cmfv, ge) + amount; // the new value for the cmfv

	/* scales the new value to the correct scope */
	if (val >= _cmf_info[cmfv].valid_values[ge]) {
		val = 0;
	} else if (val < 0) {
		val = _cmf_info[cmfv].valid_values[ge] - 1;
	}

	SetCompanyManagerFaceBits(cmf, cmfv, ge, val); // save the new value
}

/**
 * Checks whether the company manager's face bits have a valid range
 * @param cmf  the face to extract the bits from
 * @param cmfv the face variable to get the data of
 * @param ge   the gender and ethnicity of the face
 * @return true if and only if the bits are valid
 */
static inline bool AreCompanyManagerFaceBitsValid(CompanyManagerFace cmf, CompanyManagerFaceVariable cmfv, GenderEthnicity ge)
{
	return GB(cmf, _cmf_info[cmfv].offset, _cmf_info[cmfv].length) < _cmf_info[cmfv].valid_values[ge];
}

/**
 * Scales a company manager's face bits variable to the correct scope
 * @param cmfv the face variable to write the data of
 * @param ge  the gender and ethnicity of the face
 * @param val the to value to scale
 * @pre val < (1U << _cmf_info[cmfv].length), i.e. val has a value of 0..2^(bits used for this variable)-1
 * @return the scaled value
 */
static inline uint ScaleCompanyManagerFaceValue(CompanyManagerFaceVariable cmfv, GenderEthnicity ge, uint val)
{
	assert(val < (1U << _cmf_info[cmfv].length));

	return (val * _cmf_info[cmfv].valid_values[ge]) >> _cmf_info[cmfv].length;
}

/**
 * Scales all company manager's face bits to the correct scope
 *
 * @param cmf the company manager's face to write the bits to
 */
static inline void ScaleAllCompanyManagerFaceBits(CompanyManagerFace &cmf)
{
	IncreaseCompanyManagerFaceBits(cmf, CMFV_ETHNICITY, GE_WM, 0); // scales the ethnicity

	GenderEthnicity ge = (GenderEthnicity)GB(cmf, _cmf_info[CMFV_GEN_ETHN].offset, _cmf_info[CMFV_GEN_ETHN].length); // gender & ethnicity of the face

	/* Is a male face with moustache. Need to reduce CPU load in the loop. */
	bool is_moust_male = !HasBit(ge, GENDER_FEMALE) && GetCompanyManagerFaceBits(cmf, CMFV_HAS_MOUSTACHE, ge) != 0;

	for (CompanyManagerFaceVariable cmfv = CMFV_EYE_COLOUR; cmfv < CMFV_END; cmfv++) { // scales all other variables

		/* The moustache variable will be scaled only if it is a male face with has a moustache */
		if (cmfv != CMFV_MOUSTACHE || is_moust_male) {
			IncreaseCompanyManagerFaceBits(cmf, cmfv, ge, 0);
		}
	}
}

/**
 * Make a random new face.
 * If it is for the advanced company manager's face window then the new face have the same gender
 * and ethnicity as the old one, else the gender is equal and the ethnicity is random.
 *
 * @param cmf the company manager's face to write the bits to
 * @param ge  the gender and ethnicity of the old company manager's face
 * @param adv if it for the advanced company manager's face window
 * @param randomizer the source of random to use for creating the manager face
 *
 * @pre scale 'ge' to a valid gender/ethnicity combination
 */
static inline void RandomCompanyManagerFaceBits(CompanyManagerFace &cmf, GenderEthnicity ge, bool adv, Randomizer &randomizer)
{
	cmf = randomizer.Next(); // random all company manager's face bits

	/* scale ge: 0 == GE_WM, 1 == GE_WF, 2 == GE_BM, 3 == GE_BF (and maybe in future: ...) */
	ge = (GenderEthnicity)((uint)ge % GE_END);

	/* set the gender (and ethnicity) for the new company manager's face */
	if (adv) {
		SetCompanyManagerFaceBits(cmf, CMFV_GEN_ETHN, ge, ge);
	} else {
		SetCompanyManagerFaceBits(cmf, CMFV_GENDER, ge, HasBit(ge, GENDER_FEMALE));
	}

	/* scales all company manager's face bits to the correct scope */
	ScaleAllCompanyManagerFaceBits(cmf);
}

/**
 * Gets the sprite to draw for the given company manager's face variable
 * @param cmf  the face to extract the data from
 * @param cmfv the face variable to get the sprite of
 * @param ge   the gender and ethnicity of the face
 * @pre _cmf_info[cmfv].valid_values[ge] != 0
 * @return sprite to draw
 */
static inline SpriteID GetCompanyManagerFaceSprite(CompanyManagerFace cmf, CompanyManagerFaceVariable cmfv, GenderEthnicity ge)
{
	assert(_cmf_info[cmfv].valid_values[ge] != 0);

	return _cmf_info[cmfv].first_sprite[ge] + GB(cmf, _cmf_info[cmfv].offset, _cmf_info[cmfv].length);
}

void DrawCompanyManagerFace(CompanyManagerFace face, int colour, const Rect &r);

#endif /* COMPANY_MANAGER_FACE_H */
