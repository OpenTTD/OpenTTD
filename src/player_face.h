/* $Id$ */

/** @file player_face.h Functionality related to the player's face */

#ifndef PLAYER_FACE_H
#define PLAYER_FACE_H

#include "core/random_func.hpp"

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
DECLARE_ENUM_AS_BIT_SET(GenderEthnicity); ///< See GenderRace as a bitset

/** Bitgroups of the PlayerFace variable */
enum PlayerFaceVariable {
	PFV_GENDER,
	PFV_ETHNICITY,
	PFV_GEN_ETHN,
	PFV_HAS_MOUSTACHE,
	PFV_HAS_TIE_EARRING,
	PFV_HAS_GLASSES,
	PFV_EYE_COLOUR,
	PFV_CHEEKS,
	PFV_CHIN,
	PFV_EYEBROWS,
	PFV_MOUSTACHE,
	PFV_LIPS,
	PFV_NOSE,
	PFV_HAIR,
	PFV_JACKET,
	PFV_COLLAR,
	PFV_TIE_EARRING,
	PFV_GLASSES,
	PFV_END
};
DECLARE_POSTFIX_INCREMENT(PlayerFaceVariable);

/** Information about the valid values of PlayerFace bitgroups as well as the sprites to draw */
struct PlayerFaceBitsInfo {
	byte     offset;               ///< Offset in bits into the PlayerFace
	byte     length;               ///< Number of bits used in the PlayerFace
	byte     valid_values[GE_END]; ///< The number of valid values per gender/ethnicity
	SpriteID first_sprite[GE_END]; ///< The first sprite per gender/ethnicity
};

/** Lookup table for indices into the PlayerFace, valid ranges and sprites */
static const PlayerFaceBitsInfo _pf_info[] = {
	/* Index                   off len   WM  WF  BM  BF         WM     WF     BM     BF */
	/* PFV_GENDER          */ {  0, 1, {  2,  2,  2,  2 }, {     0,     0,     0,     0 } }, ///< 0 = male, 1 = female
	/* PFV_ETHNICITY       */ {  1, 2, {  2,  2,  2,  2 }, {     0,     0,     0,     0 } }, ///< 0 = (Western-)Caucasian, 1 = African(-American)/Black
	/* PFV_GEN_ETHN        */ {  0, 3, {  4,  4,  4,  4 }, {     0,     0,     0,     0 } }, ///< Shortcut to get/set gender _and_ ethnicity
	/* PFV_HAS_MOUSTACHE   */ {  3, 1, {  2,  0,  2,  0 }, {     0,     0,     0,     0 } }, ///< Females do not have a moustache
	/* PFV_HAS_TIE_EARRING */ {  3, 1, {  0,  2,  0,  2 }, {     0,     0,     0,     0 } }, ///< Draw the earring for females or not. For males the tie is always drawn.
	/* PFV_HAS_GLASSES     */ {  4, 1, {  2,  2,  2,  2 }, {     0,     0,     0,     0 } }, ///< Whether to draw glasses or not
	/* PFV_EYE_COLOUR      */ {  5, 2, {  3,  3,  1,  1 }, {     0,     0,     0,     0 } }, ///< Palette modification
	/* PFV_CHEEKS          */ {  0, 0, {  1,  1,  1,  1 }, { 0x325, 0x326, 0x390, 0x3B0 } }, ///< Cheeks are only indexed by their gender/ethnicity
	/* PFV_CHIN            */ {  7, 2, {  4,  1,  2,  2 }, { 0x327, 0x327, 0x391, 0x3B1 } },
	/* PFV_EYEBROWS        */ {  9, 4, { 12, 16, 11, 16 }, { 0x32B, 0x337, 0x39A, 0x3B8 } },
	/* PFV_MOUSTACHE       */ { 13, 2, {  3,  0,  3,  0 }, { 0x367,     0, 0x397,     0 } }, ///< Depends on PFV_HAS_MOUSTACHE
	/* PFV_LIPS            */ { 13, 4, { 12, 10,  9,  9 }, { 0x35B, 0x351, 0x3A5, 0x3C8 } }, ///< Depends on !PFV_HAS_MOUSTACHE
	/* PFV_NOSE            */ { 17, 3, {  8,  4,  4,  5 }, { 0x349, 0x34C, 0x393, 0x3B3 } }, ///< Depends on !PFV_HAS_MOUSTACHE
	/* PFV_HAIR            */ { 20, 4, {  9,  5,  5,  4 }, { 0x382, 0x38B, 0x3D4, 0x3D9 } },
	/* PFV_JACKET          */ { 24, 2, {  3,  3,  3,  3 }, { 0x36B, 0x378, 0x36B, 0x378 } },
	/* PFV_COLLAR          */ { 26, 2, {  4,  4,  4,  4 }, { 0x36E, 0x37B, 0x36E, 0x37B } },
	/* PFV_TIE_EARRING     */ { 28, 3, {  6,  3,  6,  3 }, { 0x372, 0x37F, 0x372, 0x3D1 } }, ///< Depends on PFV_HAS_TIE_EARRING
	/* PFV_GLASSES         */ { 31, 1, {  2,  2,  2,  2 }, { 0x347, 0x347, 0x3AE, 0x3AE } }  ///< Depends on PFV_HAS_GLASSES
};
assert_compile(lengthof(_pf_info) == PFV_END);

/**
 * Gets the player's face bits for the given player face variable
 * @param pf  the face to extract the bits from
 * @param pfv the face variable to get the data of
 * @param ge  the gender and ethnicity of the face
 * @pre _pf_info[pfv].valid_values[ge] != 0
 * @return the requested bits
 */
static inline uint GetPlayerFaceBits(PlayerFace pf, PlayerFaceVariable pfv, GenderEthnicity ge)
{
	assert(_pf_info[pfv].valid_values[ge] != 0);

	return GB(pf, _pf_info[pfv].offset, _pf_info[pfv].length);
}

/**
 * Sets the player's face bits for the given player face variable
 * @param pf  the face to write the bits to
 * @param pfv the face variable to write the data of
 * @param ge  the gender and ethnicity of the face
 * @param val the new value
 * @pre val < _pf_info[pfv].valid_values[ge]
 */
static inline void SetPlayerFaceBits(PlayerFace &pf, PlayerFaceVariable pfv, GenderEthnicity ge, uint val)
{
	assert(val < _pf_info[pfv].valid_values[ge]);

	SB(pf, _pf_info[pfv].offset, _pf_info[pfv].length, val);
}

/**
 * Increase/Decrease the player face variable by the given amount.
 * If the new value greater than the max value for this variable it will be set to 0.
 * Or is it negativ (< 0) it will be set to max value.
 *
 * @param pf     the player face to write the bits to
 * @param pfv    the player face variable to write the data of
 * @param ge     the gender and ethnicity of the player face
 * @param amount the amount which change the value
 *
 * @pre 0 <= val < _pf_info[pfv].valid_values[ge]
 */
static inline void IncreasePlayerFaceBits(PlayerFace &pf, PlayerFaceVariable pfv, GenderEthnicity ge, int8 amount)
{
	int8 val = GetPlayerFaceBits(pf, pfv, ge) + amount; // the new value for the pfv

	/* scales the new value to the correct scope */
	if (val >= _pf_info[pfv].valid_values[ge]) {
		val = 0;
	} else if (val < 0) {
		val = _pf_info[pfv].valid_values[ge] - 1;
	}

	SetPlayerFaceBits(pf, pfv, ge, val); // save the new value
}

/**
 * Checks whether the player bits have a valid range
 * @param pf  the face to extract the bits from
 * @param pfv the face variable to get the data of
 * @param ge  the gender and ethnicity of the face
 * @return true if and only if the bits are valid
 */
static inline bool ArePlayerFaceBitsValid(PlayerFace pf, PlayerFaceVariable pfv, GenderEthnicity ge)
{
	return GB(pf, _pf_info[pfv].offset, _pf_info[pfv].length) < _pf_info[pfv].valid_values[ge];
}

/**
 * Scales a player face bits variable to the correct scope
 * @param pfv the face variable to write the data of
 * @param ge  the gender and ethnicity of the face
 * @param val the to value to scale
 * @pre val < (1U << _pf_info[pfv].length), i.e. val has a value of 0..2^(bits used for this variable)-1
 * @return the scaled value
 */
static inline uint ScalePlayerFaceValue(PlayerFaceVariable pfv, GenderEthnicity ge, uint val)
{
	assert(val < (1U << _pf_info[pfv].length));

	return (val * _pf_info[pfv].valid_values[ge]) >> _pf_info[pfv].length;
}

/**
 * Scales all player face bits to the correct scope
 *
 * @param pf the player face to write the bits to
 */
static inline void ScaleAllPlayerFaceBits(PlayerFace &pf)
{
	IncreasePlayerFaceBits(pf, PFV_ETHNICITY, GE_WM, 0); // scales the ethnicity

	GenderEthnicity ge = (GenderEthnicity)GB(pf, _pf_info[PFV_GEN_ETHN].offset, _pf_info[PFV_GEN_ETHN].length); // gender & ethnicity of the face

	/* Is a male face with moustache. Need to reduce CPU load in the loop. */
	bool is_moust_male = !HasBit(ge, GENDER_FEMALE) && GetPlayerFaceBits(pf, PFV_HAS_MOUSTACHE, ge) != 0;

	for (PlayerFaceVariable pfv = PFV_EYE_COLOUR; pfv < PFV_END; pfv++) { // scales all other variables

		/* The moustache variable will be scaled only if it is a male face with has a moustache */
		if (pfv != PFV_MOUSTACHE || is_moust_male) {
			IncreasePlayerFaceBits(pf, pfv, ge, 0);
		}
	}
}

/**
 * Make a random new face.
 * If it is for the advanced player face window then the new face have the same gender
 * and ethnicity as the old one, else the gender is equal and the ethnicity is random.
 *
 * @param pf  the player face to write the bits to
 * @param ge  the gender and ethnicity of the old player face
 * @param adv if it for the advanced player face window
 *
 * @pre scale 'ge' to a valid gender/ethnicity combination
 */
static inline void RandomPlayerFaceBits(PlayerFace &pf, GenderEthnicity ge, bool adv)
{
	pf = InteractiveRandom(); // random all player face bits

	/* scale ge: 0 == GE_WM, 1 == GE_WF, 2 == GE_BM, 3 == GE_BF (and maybe in future: ...) */
	ge = (GenderEthnicity)((uint)ge % GE_END);

	/* set the gender (and ethnicity) for the new player face */
	if (adv) {
		SetPlayerFaceBits(pf, PFV_GEN_ETHN, ge, ge);
	} else {
		SetPlayerFaceBits(pf, PFV_GENDER, ge, HasBit(ge, GENDER_FEMALE));
	}

	/* scales all player face bits to the correct scope */
	ScaleAllPlayerFaceBits(pf);
}

/**
 * Gets the sprite to draw for the given player face variable
 * @param pf  the face to extract the data from
 * @param pfv the face variable to get the sprite of
 * @param ge  the gender and ethnicity of the face
 * @pre _pf_info[pfv].valid_values[ge] != 0
 * @return sprite to draw
 */
static inline SpriteID GetPlayerFaceSprite(PlayerFace pf, PlayerFaceVariable pfv, GenderEthnicity ge)
{
	assert(_pf_info[pfv].valid_values[ge] != 0);

	return _pf_info[pfv].first_sprite[ge] + GB(pf, _pf_info[pfv].offset, _pf_info[pfv].length);
}

void DrawPlayerFace(PlayerFace face, int color, int x, int y);
PlayerFace ConvertFromOldPlayerFace(uint32 face);
bool IsValidPlayerFace(PlayerFace pf);
void DrawFaceStringLabel(const Window *w, byte widget_index, StringID str, uint8 val, bool is_bool_widget);

#endif /* PLAYER_FACE_H */
