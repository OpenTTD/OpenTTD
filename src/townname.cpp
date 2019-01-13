/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file townname.cpp %Town name generators. */

#include "stdafx.h"
#include "string_func.h"
#include "townname_type.h"
#include "town.h"
#include "strings_func.h"
#include "core/random_func.hpp"
#include "genworld.h"
#include "gfx_layout.h"

#include "table/townname.h"

#include "safeguards.h"


/**
 * Initializes this struct from town data
 * @param t town for which we will be printing name later
 */
TownNameParams::TownNameParams(const Town *t) :
		grfid(t->townnamegrfid), // by default, use supplied data
		type(t->townnametype)
{
	if (t->townnamegrfid != 0 && GetGRFTownName(t->townnamegrfid) == NULL) {
		/* Fallback to english original */
		this->grfid = 0;
		this->type = SPECSTR_TOWNNAME_ENGLISH;
		return;
	}
}


/**
 * Fills buffer with specified town name
 * @param buff buffer start
 * @param par town name parameters
 * @param townnameparts 'encoded' town name
 * @param last end of buffer
 * @return pointer to terminating '\0'
 */
char *GetTownName(char *buff, const TownNameParams *par, uint32 townnameparts, const char *last)
{
	if (par->grfid == 0) {
		int64 args_array[1] = { townnameparts };
		StringParameters tmp_params(args_array);
		return GetStringWithArgs(buff, par->type, &tmp_params, last);
	}

	return GRFTownNameGenerate(buff, par->grfid, par->type, townnameparts, last);
}


/**
 * Fills buffer with town's name
 * @param buff buffer start
 * @param t we want to get name of this town
 * @param last end of buffer
 * @return pointer to terminating '\0'
 */
char *GetTownName(char *buff, const Town *t, const char *last)
{
	TownNameParams par(t);
	return GetTownName(buff, &par, t->townnameparts, last);
}


/**
 * Verifies the town name is valid and unique.
 * @param r random bits
 * @param par town name parameters
 * @param town_names if a name is generated, check its uniqueness with the set
 * @return true iff name is valid and unique
 */
bool VerifyTownName(uint32 r, const TownNameParams *par, TownNames *town_names)
{
	/* reserve space for extra unicode character and terminating '\0' */
	char buf1[(MAX_LENGTH_TOWN_NAME_CHARS + 1) * MAX_CHAR_LENGTH];
	char buf2[(MAX_LENGTH_TOWN_NAME_CHARS + 1) * MAX_CHAR_LENGTH];

	GetTownName(buf1, par, r, lastof(buf1));

	/* Check size and width */
	if (Utf8StringLength(buf1) >= MAX_LENGTH_TOWN_NAME_CHARS) return false;

	if (town_names != NULL) {
		if (town_names->find(buf1) != town_names->end()) return false;
		town_names->insert(buf1);
	} else {
		const Town *t;
		FOR_ALL_TOWNS(t) {
			/* We can't just compare the numbers since
			 * several numbers may map to a single name. */
			const char *buf = t->name;
			if (buf == NULL) {
				GetTownName(buf2, t, lastof(buf2));
				buf = buf2;
			}
			if (strcmp(buf1, buf) == 0) return false;
		}
	}

	return true;
}


/**
 * Generates valid town name.
 * @param townnameparts if a name is generated, it's stored there
 * @param town_names if a name is generated, check its uniqueness with the set
 * @return true iff a name was generated
 */
bool GenerateTownName(uint32 *townnameparts, TownNames *town_names)
{
	TownNameParams par(_settings_game.game_creation.town_name);

	/* This function is called very often without entering the gameloop
	 * inbetween. So reset layout cache to prevent it from growing too big. */
	Layouter::ReduceLineCache();

	/* Do not set i too low, since when we run out of names, we loop
	 * for #tries only one time anyway - then we stop generating more
	 * towns. Do not set it too high either, since looping through all
	 * the other towns may take considerable amount of time (10000 is
	 * too much). */
	for (int i = 1000; i != 0; i--) {
		uint32 r = _generating_world ? Random() : InteractiveRandom();
		if (!VerifyTownName(r, &par, town_names)) continue;

		*townnameparts = r;
		return true;
	}

	return false;
}



/**
 * Generates a number from given seed.
 * @param shift_by number of bits seed is shifted to the right
 * @param max generated number is in interval 0...max-1
 * @param seed seed
 * @return seed transformed to a number from given range
 */
static inline uint32 SeedChance(byte shift_by, int max, uint32 seed)
{
	return (GB(seed, shift_by, 16) * max) >> 16;
}


/**
 * Generates a number from given seed. Uses different algorithm than SeedChance().
 * @param shift_by number of bits seed is shifted to the right
 * @param max generated number is in interval 0...max-1
 * @param seed seed
 * @return seed transformed to a number from given range
 */
static inline uint32 SeedModChance(byte shift_by, int max, uint32 seed)
{
	/* This actually gives *MUCH* more even distribution of the values
	 * than SeedChance(), which is absolutely horrible in that. If
	 * you do not believe me, try with i.e. the Czech town names,
	 * compare the words (nicely visible on prefixes) generated by
	 * SeedChance() and SeedModChance(). Do not get discouraged by the
	 * never-use-modulo myths, which hold true only for the linear
	 * congruential generators (and Random() isn't such a generator).
	 * --pasky
	 * TODO: Perhaps we should use it for all the name generators? --pasky */
	return (seed >> shift_by) % max;
}


/**
 * Generates a number from given seed.
 * @param shift_by number of bits seed is shifted to the right
 * @param max generated number is in interval -bias...max-1
 * @param seed seed
 * @param bias minimum value that can be returned
 * @return seed transformed to a number from given range
 */
static inline int32 SeedChanceBias(byte shift_by, int max, uint32 seed, int bias)
{
	return SeedChance(shift_by, max + bias, seed) - bias;
}


/**
 * Replaces a string beginning in 'org' with 'rep'.
 * @param org string to replace, has to be 4 characters long
 * @param rep string to be replaced with, has to be 4 characters long
 * @param buf buffer with string
 */
static void ReplaceWords(const char *org, const char *rep, char *buf)
{
	assert(strlen(org) == 4 && strlen(rep) == 4 && strlen(buf) >= 4);
	if (strncmp(buf, org, 4) == 0) memcpy(buf, rep, 4); // Safe as the string in buf is always more than 4 characters long.
}


/**
 * Replaces english curses and ugly letter combinations by nicer ones.
 * @param buf buffer with town name
 * @param original English (Original) generator was used
 */
static void ReplaceEnglishWords(char *buf, bool original)
{
	ReplaceWords("Cunt", "East", buf);
	ReplaceWords("Slag", "Pits", buf);
	ReplaceWords("Slut", "Edin", buf);
	if (!original) ReplaceWords("Fart", "Boot", buf); // never happens with 'English (Original)'
	ReplaceWords("Drar", "Quar", buf);
	ReplaceWords("Dreh", "Bash", buf);
	ReplaceWords("Frar", "Shor", buf);
	ReplaceWords("Grar", "Aber", buf);
	ReplaceWords("Brar", "Over", buf);
	ReplaceWords("Wrar", original ? "Inve" : "Stan", buf);
}

/**
 * Generates English (Original) town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeEnglishOriginalTownName(char *buf, const char *last, uint32 seed)
{
	char *orig = buf;

	/* optional first segment */
	int i = SeedChanceBias(0, lengthof(_name_original_english_1), seed, 50);
	if (i >= 0) buf = strecpy(buf, _name_original_english_1[i], last);

	/* mandatory middle segments */
	buf = strecpy(buf, _name_original_english_2[SeedChance(4,  lengthof(_name_original_english_2), seed)], last);
	buf = strecpy(buf, _name_original_english_3[SeedChance(7,  lengthof(_name_original_english_3), seed)], last);
	buf = strecpy(buf, _name_original_english_4[SeedChance(10, lengthof(_name_original_english_4), seed)], last);
	buf = strecpy(buf, _name_original_english_5[SeedChance(13, lengthof(_name_original_english_5), seed)], last);

	/* optional last segment */
	i = SeedChanceBias(15, lengthof(_name_original_english_6), seed, 60);
	if (i >= 0) buf = strecpy(buf, _name_original_english_6[i], last);

	/* Ce, Ci => Ke, Ki */
	if (orig[0] == 'C' && (orig[1] == 'e' || orig[1] == 'i')) {
		orig[0] = 'K';
	}

	assert(buf - orig >= 4);
	ReplaceEnglishWords(orig, true);

	return buf;
}


/**
 * Generates English (Additional) town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeEnglishAdditionalTownName(char *buf, const char *last, uint32 seed)
{
	char *orig = buf;

	/* optional first segment */
	int i = SeedChanceBias(0, lengthof(_name_additional_english_prefix), seed, 50);
	if (i >= 0) buf = strecpy(buf, _name_additional_english_prefix[i], last);

	if (SeedChance(3, 20, seed) >= 14) {
		buf = strecpy(buf, _name_additional_english_1a[SeedChance(6, lengthof(_name_additional_english_1a), seed)], last);
	} else {
		buf = strecpy(buf, _name_additional_english_1b1[SeedChance(6, lengthof(_name_additional_english_1b1), seed)], last);
		buf = strecpy(buf, _name_additional_english_1b2[SeedChance(9, lengthof(_name_additional_english_1b2), seed)], last);
		if (SeedChance(11, 20, seed) >= 4) {
			buf = strecpy(buf, _name_additional_english_1b3a[SeedChance(12, lengthof(_name_additional_english_1b3a), seed)], last);
		} else {
			buf = strecpy(buf, _name_additional_english_1b3b[SeedChance(12, lengthof(_name_additional_english_1b3b), seed)], last);
		}
	}

	buf = strecpy(buf, _name_additional_english_2[SeedChance(14, lengthof(_name_additional_english_2), seed)], last);

	/* optional last segment */
	i = SeedChanceBias(15, lengthof(_name_additional_english_3), seed, 60);
	if (i >= 0) buf = strecpy(buf, _name_additional_english_3[i], last);

	assert(buf - orig >= 4);
	ReplaceEnglishWords(orig, false);

	return buf;
}


/**
 * Generates Austrian town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeAustrianTownName(char *buf, const char *last, uint32 seed)
{
	/* Bad, Maria, Gross, ... */
	int i = SeedChanceBias(0, lengthof(_name_austrian_a1), seed, 15);
	if (i >= 0) buf = strecpy(buf, _name_austrian_a1[i], last);

	int j = 0;

	i = SeedChance(4, 6, seed);
	if (i >= 4) {
		/* Kaisers-kirchen */
		buf = strecpy(buf, _name_austrian_a2[SeedChance( 7, lengthof(_name_austrian_a2), seed)], last);
		buf = strecpy(buf, _name_austrian_a3[SeedChance(13, lengthof(_name_austrian_a3), seed)], last);
	} else if (i >= 2) {
		/* St. Johann */
		buf = strecpy(buf, _name_austrian_a5[SeedChance( 7, lengthof(_name_austrian_a5), seed)], last);
		buf = strecpy(buf, _name_austrian_a6[SeedChance( 9, lengthof(_name_austrian_a6), seed)], last);
		j = 1; // More likely to have a " an der " or " am "
	} else {
		/* Zell */
		buf = strecpy(buf, _name_austrian_a4[SeedChance( 7, lengthof(_name_austrian_a4), seed)], last);
	}

	i = SeedChance(1, 6, seed);
	if (i >= 4 - j) {
		/* an der Donau (rivers) */
		buf = strecpy(buf, _name_austrian_f1[SeedChance(4, lengthof(_name_austrian_f1), seed)], last);
		buf = strecpy(buf, _name_austrian_f2[SeedChance(5, lengthof(_name_austrian_f2), seed)], last);
	} else if (i >= 2 - j) {
		/* am Dachstein (mountains) */
		buf = strecpy(buf, _name_austrian_b1[SeedChance(4, lengthof(_name_austrian_b1), seed)], last);
		buf = strecpy(buf, _name_austrian_b2[SeedChance(5, lengthof(_name_austrian_b2), seed)], last);
	}

	return buf;
}


/**
 * Generates German town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeGermanTownName(char *buf, const char *last, uint32 seed)
{
	uint seed_derivative = SeedChance(7, 28, seed);

	/* optional prefix */
	if (seed_derivative == 12 || seed_derivative == 19) {
		uint i = SeedChance(2, lengthof(_name_german_pre), seed);
		buf = strecpy(buf, _name_german_pre[i], last);
	}

	/* mandatory middle segments including option of hardcoded name */
	uint i = SeedChance(3, lengthof(_name_german_real) + lengthof(_name_german_1), seed);
	if (i < lengthof(_name_german_real)) {
		buf = strecpy(buf, _name_german_real[i], last);
	} else {
		buf = strecpy(buf, _name_german_1[i - lengthof(_name_german_real)], last);

		i = SeedChance(5, lengthof(_name_german_2), seed);
		buf = strecpy(buf, _name_german_2[i], last);
	}

	/* optional suffix */
	if (seed_derivative == 24) {
		i = SeedChance(9, lengthof(_name_german_4_an_der) + lengthof(_name_german_4_am), seed);
		if (i < lengthof(_name_german_4_an_der)) {
			buf = strecpy(buf, _name_german_3_an_der[0], last);
			buf = strecpy(buf, _name_german_4_an_der[i], last);
		} else {
			buf = strecpy(buf, _name_german_3_am[0], last);
			buf = strecpy(buf, _name_german_4_am[i - lengthof(_name_german_4_an_der)], last);
		}
	}

	return buf;
}


/**
 * Generates Latin-American town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeSpanishTownName(char *buf, const char *last, uint32 seed)
{
	return strecpy(buf, _name_spanish_real[SeedChance(0, lengthof(_name_spanish_real), seed)], last);
}


/**
 * Generates French town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeFrenchTownName(char *buf, const char *last, uint32 seed)
{
	return strecpy(buf, _name_french_real[SeedChance(0, lengthof(_name_french_real), seed)], last);
}


/**
 * Generates Silly town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeSillyTownName(char *buf, const char *last, uint32 seed)
{
	buf = strecpy(buf, _name_silly_1[SeedChance( 0, lengthof(_name_silly_1), seed)], last);
	buf = strecpy(buf, _name_silly_2[SeedChance(16, lengthof(_name_silly_2), seed)], last);

	return buf;
}


/**
 * Generates Swedish town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeSwedishTownName(char *buf, const char *last, uint32 seed)
{
	/* optional first segment */
	int i = SeedChanceBias(0, lengthof(_name_swedish_1), seed, 50);
	if (i >= 0) buf = strecpy(buf, _name_swedish_1[i], last);

	/* mandatory middle segments including option of hardcoded name */
	if (SeedChance(4, 5, seed) >= 3) {
		buf = strecpy(buf, _name_swedish_2[SeedChance( 7, lengthof(_name_swedish_2), seed)], last);
	} else {
		buf = strecpy(buf, _name_swedish_2a[SeedChance( 7, lengthof(_name_swedish_2a), seed)], last);
		buf = strecpy(buf, _name_swedish_2b[SeedChance(10, lengthof(_name_swedish_2b), seed)], last);
		buf = strecpy(buf, _name_swedish_2c[SeedChance(13, lengthof(_name_swedish_2c), seed)], last);
	}

	buf = strecpy(buf, _name_swedish_3[SeedChance(16, lengthof(_name_swedish_3), seed)], last);

	return buf;
}


/**
 * Generates Dutch town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeDutchTownName(char *buf, const char *last, uint32 seed)
{
	/* optional first segment */
	int i = SeedChanceBias(0, lengthof(_name_dutch_1), seed, 50);
	if (i >= 0) buf = strecpy(buf, _name_dutch_1[i], last);

	/* mandatory middle segments including option of hardcoded name */
	if (SeedChance(6, 9, seed) > 4) {
		buf = strecpy(buf, _name_dutch_2[SeedChance( 9, lengthof(_name_dutch_2), seed)], last);
	} else {
		buf = strecpy(buf, _name_dutch_3[SeedChance( 9, lengthof(_name_dutch_3), seed)], last);
		buf = strecpy(buf, _name_dutch_4[SeedChance(12, lengthof(_name_dutch_4), seed)], last);
	}

	buf = strecpy(buf, _name_dutch_5[SeedChance(15, lengthof(_name_dutch_5), seed)], last);

	return buf;
}


/**
 * Generates Finnish town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeFinnishTownName(char *buf, const char *last, uint32 seed)
{
	char *orig = buf;

	/* Select randomly if town name should consists of one or two parts. */
	if (SeedChance(0, 15, seed) >= 10) {
		return strecpy(buf, _name_finnish_real[SeedChance(2, lengthof(_name_finnish_real), seed)], last);
	}

	if (SeedChance(0, 15, seed) >= 5) {
		/* A two-part name by combining one of _name_finnish_1 + "la"/"lä"
		 * The reason for not having the contents of _name_finnish_{1,2} in the same table is
		 * that the ones in _name_finnish_2 are not good for this purpose. */
		uint sel = SeedChance( 0, lengthof(_name_finnish_1), seed);
		buf = strecpy(buf, _name_finnish_1[sel], last);
		char *end = buf - 1;
		assert(end >= orig);
		if (*end == 'i') *end = 'e';
		if (strstr(orig, "a") != NULL || strstr(orig, "o") != NULL || strstr(orig, "u") != NULL ||
				strstr(orig, "A") != NULL || strstr(orig, "O") != NULL || strstr(orig, "U")  != NULL) {
			buf = strecpy(buf, "la", last);
		} else {
			buf = strecpy(buf, "l\xC3\xA4", last);
		}
		return buf;
	}

	/* A two-part name by combining one of _name_finnish_{1,2} + _name_finnish_3.
	 * Why aren't _name_finnish_{1,2} just one table? See above. */
	uint sel = SeedChance(2, lengthof(_name_finnish_1) + lengthof(_name_finnish_2), seed);
	if (sel >= lengthof(_name_finnish_1)) {
		buf = strecpy(buf, _name_finnish_2[sel - lengthof(_name_finnish_1)], last);
	} else {
		buf = strecpy(buf, _name_finnish_1[sel], last);
	}

	buf = strecpy(buf, _name_finnish_3[SeedChance(10, lengthof(_name_finnish_3), seed)], last);

	return buf;
}


/**
 * Generates Polish town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakePolishTownName(char *buf, const char *last, uint32 seed)
{
	/* optional first segment */
	uint i = SeedChance(0,
			lengthof(_name_polish_2_o) + lengthof(_name_polish_2_m) +
			lengthof(_name_polish_2_f) + lengthof(_name_polish_2_n),
			seed);
	uint j = SeedChance(2, 20, seed);


	if (i < lengthof(_name_polish_2_o)) {
		return strecpy(buf, _name_polish_2_o[SeedChance(3, lengthof(_name_polish_2_o), seed)], last);
	}

	if (i < lengthof(_name_polish_2_m) + lengthof(_name_polish_2_o)) {
		if (j < 4) {
			buf = strecpy(buf, _name_polish_1_m[SeedChance(5, lengthof(_name_polish_1_m), seed)], last);
		}

		buf = strecpy(buf, _name_polish_2_m[SeedChance(7, lengthof(_name_polish_2_m), seed)], last);

		if (j >= 4 && j < 16) {
			buf = strecpy(buf, _name_polish_3_m[SeedChance(10, lengthof(_name_polish_3_m), seed)], last);
		}

		return buf;
	}

	if (i < lengthof(_name_polish_2_f) + lengthof(_name_polish_2_m) + lengthof(_name_polish_2_o)) {
		if (j < 4) {
			buf = strecpy(buf, _name_polish_1_f[SeedChance(5, lengthof(_name_polish_1_f), seed)], last);
		}

		buf = strecpy(buf, _name_polish_2_f[SeedChance(7, lengthof(_name_polish_2_f), seed)], last);

		if (j >= 4 && j < 16) {
			buf = strecpy(buf, _name_polish_3_f[SeedChance(10, lengthof(_name_polish_3_f), seed)], last);
		}

		return buf;
	}

	if (j < 4) {
		buf = strecpy(buf, _name_polish_1_n[SeedChance(5, lengthof(_name_polish_1_n), seed)], last);
	}

	buf = strecpy(buf, _name_polish_2_n[SeedChance(7, lengthof(_name_polish_2_n), seed)], last);

	if (j >= 4 && j < 16) {
		buf = strecpy(buf, _name_polish_3_n[SeedChance(10, lengthof(_name_polish_3_n), seed)], last);
	}

	return buf;
}


/**
 * Generates Czech town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeCzechTownName(char *buf, const char *last, uint32 seed)
{
	/* 1:3 chance to use a real name. */
	if (SeedModChance(0, 4, seed) == 0) {
		return strecpy(buf, _name_czech_real[SeedModChance(4, lengthof(_name_czech_real), seed)], last);
	}

	const char *orig = buf;

	/* Probability of prefixes/suffixes
	 * 0..11 prefix, 12..13 prefix+suffix, 14..17 suffix, 18..31 nothing */
	int prob_tails = SeedModChance(2, 32, seed);
	bool do_prefix = prob_tails < 12;
	bool do_suffix = prob_tails > 11 && prob_tails < 17;
	bool dynamic_subst;

	/* IDs of the respective parts */
	int prefix = 0, ending = 0, suffix = 0;
	uint postfix = 0;
	uint stem;

	/* The select criteria. */
	CzechGender gender;
	CzechChoose choose;
	CzechAllow allow;

	if (do_prefix) prefix = SeedModChance(5, lengthof(_name_czech_adj) * 12, seed) / 12;
	if (do_suffix) suffix = SeedModChance(7, lengthof(_name_czech_suffix), seed);
	/* 3:1 chance 3:1 to use dynamic substantive */
	stem = SeedModChance(9,
			lengthof(_name_czech_subst_full) + 3 * lengthof(_name_czech_subst_stem),
			seed);
	if (stem < lengthof(_name_czech_subst_full)) {
		/* That was easy! */
		dynamic_subst = false;
		gender = _name_czech_subst_full[stem].gender;
		choose = _name_czech_subst_full[stem].choose;
		allow = _name_czech_subst_full[stem].allow;
	} else {
		uint map[lengthof(_name_czech_subst_ending)];
		int ending_start = -1, ending_stop = -1;

		/* Load the substantive */
		dynamic_subst = true;
		stem -= lengthof(_name_czech_subst_full);
		stem %= lengthof(_name_czech_subst_stem);
		gender = _name_czech_subst_stem[stem].gender;
		choose = _name_czech_subst_stem[stem].choose;
		allow = _name_czech_subst_stem[stem].allow;

		/* Load the postfix (1:1 chance that a postfix will be inserted) */
		postfix = SeedModChance(14, lengthof(_name_czech_subst_postfix) * 2, seed);

		if (choose & CZC_POSTFIX) {
			/* Always get a real postfix. */
			postfix %= lengthof(_name_czech_subst_postfix);
		}
		if (choose & CZC_NOPOSTFIX) {
			/* Always drop a postfix. */
			postfix += lengthof(_name_czech_subst_postfix);
		}
		if (postfix < lengthof(_name_czech_subst_postfix)) {
			choose |= CZC_POSTFIX;
		} else {
			choose |= CZC_NOPOSTFIX;
		}

		/* Localize the array segment containing a good gender */
		for (ending = 0; ending < (int)lengthof(_name_czech_subst_ending); ending++) {
			const CzechNameSubst *e = &_name_czech_subst_ending[ending];

			if (gender == CZG_FREE ||
					(gender == CZG_NFREE && e->gender != CZG_SNEUT && e->gender != CZG_PNEUT) ||
					 gender == e->gender) {
				if (ending_start < 0) {
					ending_start = ending;
				}
			} else if (ending_start >= 0) {
				ending_stop = ending - 1;
				break;
			}
		}
		if (ending_stop < 0) {
			/* Whoa. All the endings matched. */
			ending_stop = ending - 1;
		}

		/* Make a sequential map of the items with good mask */
		size_t i = 0;
		for (ending = ending_start; ending <= ending_stop; ending++) {
			const CzechNameSubst *e = &_name_czech_subst_ending[ending];

			if ((e->choose & choose) == choose && (e->allow & allow) != 0) {
				map[i++] = ending;
			}
		}
		assert(i > 0);

		/* Load the ending */
		ending = map[SeedModChance(16, (int)i, seed)];
		/* Override possible CZG_*FREE; this must be a real gender,
		 * otherwise we get overflow when modifying the adjectivum. */
		gender = _name_czech_subst_ending[ending].gender;
		assert(gender != CZG_FREE && gender != CZG_NFREE);
	}

	if (do_prefix && (_name_czech_adj[prefix].choose & choose) != choose) {
		/* Throw away non-matching prefix. */
		do_prefix = false;
	}

	/* Now finally construct the name */
	if (do_prefix) {
		CzechPattern pattern = _name_czech_adj[prefix].pattern;

		buf = strecpy(buf, _name_czech_adj[prefix].name, last);

		char *endpos = buf - 1;
		/* Find the first character in a UTF-8 sequence */
		while (GB(*endpos, 6, 2) == 2) endpos--;

		if (gender == CZG_SMASC && pattern == CZP_PRIVL) {
			assert(endpos >= orig + 2);
			/* -ovX -> -uv */
			*(endpos - 2) = 'u';
			assert(*(endpos - 1) == 'v');
			*endpos = '\0';
		} else {
			assert(endpos >= orig);
			endpos = strecpy(endpos, _name_czech_patmod[gender][pattern], last);
		}

		buf = strecpy(endpos, " ", last);
	}

	if (dynamic_subst) {
		buf = strecpy(buf, _name_czech_subst_stem[stem].name, last);
		if (postfix < lengthof(_name_czech_subst_postfix)) {
			const char *poststr = _name_czech_subst_postfix[postfix];
			const char *endstr = _name_czech_subst_ending[ending].name;

			size_t postlen = strlen(poststr);
			size_t endlen = strlen(endstr);
			assert(postlen > 0 && endlen > 0);

			/* Kill the "avava" and "Jananna"-like cases */
			if (postlen < 2 || postlen > endlen ||
					((poststr[1] != 'v' || poststr[1] != endstr[1]) &&
					poststr[2] != endstr[1])) {
				buf = strecpy(buf, poststr, last);

				/* k-i -> c-i, h-i -> z-i */
				if (endstr[0] == 'i') {
					switch (*(buf - 1)) {
						case 'k': *(buf - 1) = 'c'; break;
						case 'h': *(buf - 1) = 'z'; break;
						default: break;
					}
				}
			}
		}
		buf = strecpy(buf, _name_czech_subst_ending[ending].name, last);
	} else {
		buf = strecpy(buf, _name_czech_subst_full[stem].name, last);
	}

	if (do_suffix) {
		buf = strecpy(buf, " ", last);
		buf = strecpy(buf, _name_czech_suffix[suffix], last);
	}

	return buf;
}


/**
 * Generates Romanian town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeRomanianTownName(char *buf, const char *last, uint32 seed)
{
	return strecpy(buf, _name_romanian_real[SeedChance(0, lengthof(_name_romanian_real), seed)], last);
}


/**
 * Generates Slovak town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeSlovakTownName(char *buf, const char *last, uint32 seed)
{
	return strecpy(buf, _name_slovak_real[SeedChance(0, lengthof(_name_slovak_real), seed)], last);
}


/**
 * Generates Norwegian town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeNorwegianTownName(char *buf, const char *last, uint32 seed)
{
	/* Use first 4 bit from seed to decide whether or not this town should
	 * have a real name 3/16 chance.  Bit 0-3 */
	if (SeedChance(0, 15, seed) < 3) {
		/* Use 7bit for the realname table index.  Bit 4-10 */
		return strecpy(buf, _name_norwegian_real[SeedChance(4, lengthof(_name_norwegian_real), seed)], last);
	}

	/* Use 7bit for the first fake part.  Bit 4-10 */
	buf = strecpy(buf, _name_norwegian_1[SeedChance(4, lengthof(_name_norwegian_1), seed)], last);
	/* Use 7bit for the last fake part.  Bit 11-17 */
	buf = strecpy(buf, _name_norwegian_2[SeedChance(11, lengthof(_name_norwegian_2), seed)], last);

	return buf;
}


/**
 * Generates Hungarian town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeHungarianTownName(char *buf, const char *last, uint32 seed)
{
	if (SeedChance(12, 15, seed) < 3) {
		return strecpy(buf, _name_hungarian_real[SeedChance(0, lengthof(_name_hungarian_real), seed)], last);
	}

	/* optional first segment */
	uint i = SeedChance(3, lengthof(_name_hungarian_1) * 3, seed);
	if (i < lengthof(_name_hungarian_1)) buf = strecpy(buf, _name_hungarian_1[i], last);

	/* mandatory middle segments */
	buf = strecpy(buf, _name_hungarian_2[SeedChance(3, lengthof(_name_hungarian_2), seed)], last);
	buf = strecpy(buf, _name_hungarian_3[SeedChance(6, lengthof(_name_hungarian_3), seed)], last);

	/* optional last segment */
	i = SeedChance(10, lengthof(_name_hungarian_4) * 3, seed);
	if (i < lengthof(_name_hungarian_4)) {
		buf = strecpy(buf, _name_hungarian_4[i], last);
	}

	return buf;
}


/**
 * Generates Swiss town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeSwissTownName(char *buf, const char *last, uint32 seed)
{
	return strecpy(buf, _name_swiss_real[SeedChance(0, lengthof(_name_swiss_real), seed)], last);
}


/**
 * Generates Danish town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeDanishTownName(char *buf, const char *last, uint32 seed)
{
	/* optional first segment */
	int i = SeedChanceBias(0, lengthof(_name_danish_1), seed, 50);
	if (i >= 0) buf = strecpy(buf, _name_danish_1[i], last);

	/* middle segments removed as this algorithm seems to create much more realistic names */
	buf = strecpy(buf, _name_danish_2[SeedChance( 7, lengthof(_name_danish_2), seed)], last);
	buf = strecpy(buf, _name_danish_3[SeedChance(16, lengthof(_name_danish_3), seed)], last);

	return buf;
}


/**
 * Generates Turkish town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeTurkishTownName(char *buf, const char *last, uint32 seed)
{
	uint i = SeedModChance(0, 5, seed);

	switch (i) {
		case 0:
			buf = strecpy(buf, _name_turkish_prefix[SeedModChance( 2, lengthof(_name_turkish_prefix), seed)], last);

			/* middle segment */
			buf = strecpy(buf, _name_turkish_middle[SeedModChance( 4, lengthof(_name_turkish_middle), seed)], last);

			/* optional suffix */
			if (SeedModChance(0, 7, seed) == 0) {
				buf = strecpy(buf, _name_turkish_suffix[SeedModChance( 10, lengthof(_name_turkish_suffix), seed)], last);
			}
			break;

		case 1: case 2:
			buf = strecpy(buf, _name_turkish_prefix[SeedModChance( 2, lengthof(_name_turkish_prefix), seed)], last);
			buf = strecpy(buf, _name_turkish_suffix[SeedModChance( 4, lengthof(_name_turkish_suffix), seed)], last);
			break;

		default:
			buf = strecpy(buf, _name_turkish_real[SeedModChance( 4, lengthof(_name_turkish_real), seed)], last);
			break;
	}

	return buf;
}


/**
 * Generates Italian town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeItalianTownName(char *buf, const char *last, uint32 seed)
{
	if (SeedModChance(0, 6, seed) == 0) { // real city names
		return strecpy(buf, _name_italian_real[SeedModChance(4, lengthof(_name_italian_real), seed)], last);
	}

	static const char * const mascul_femin_italian[] = {
		"o",
		"a",
	};

	if (SeedModChance(0, 8, seed) == 0) { // prefix
		buf = strecpy(buf, _name_italian_pref[SeedModChance(11, lengthof(_name_italian_pref), seed)], last);
	}

	uint i = SeedChance(0, 2, seed);
	if (i == 0) { // masculine form
		buf = strecpy(buf, _name_italian_1m[SeedModChance(4, lengthof(_name_italian_1m), seed)], last);
	} else { // feminine form
		buf = strecpy(buf, _name_italian_1f[SeedModChance(4, lengthof(_name_italian_1f), seed)], last);
	}

	if (SeedModChance(3, 3, seed) == 0) {
		buf = strecpy(buf, _name_italian_2[SeedModChance(11, lengthof(_name_italian_2), seed)], last);
		buf = strecpy(buf, mascul_femin_italian[i], last);
	} else {
		buf = strecpy(buf, _name_italian_2i[SeedModChance(16, lengthof(_name_italian_2i), seed)], last);
	}

	if (SeedModChance(15, 4, seed) == 0) {
		if (SeedModChance(5, 2, seed) == 0) { // generic suffix
			buf = strecpy(buf, _name_italian_3[SeedModChance(4, lengthof(_name_italian_3), seed)], last);
		} else { // river name suffix
			buf = strecpy(buf, _name_italian_river1[SeedModChance(4, lengthof(_name_italian_river1), seed)], last);
			buf = strecpy(buf, _name_italian_river2[SeedModChance(16, lengthof(_name_italian_river2), seed)], last);
		}
	}

	return buf;
}


/**
 * Generates Catalan town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 * @param last end of buffer
 */
static char *MakeCatalanTownName(char *buf, const char *last, uint32 seed)
{
	if (SeedModChance(0, 3, seed) == 0) { // real city names
		return strecpy(buf, _name_catalan_real[SeedModChance(4, lengthof(_name_catalan_real), seed)], last);
	}

	if (SeedModChance(0, 2, seed) == 0) { // prefix
		buf = strecpy(buf, _name_catalan_pref[SeedModChance(11, lengthof(_name_catalan_pref), seed)], last);
	}

	uint i = SeedChance(0, 2, seed);
	if (i == 0) { // masculine form
		buf = strecpy(buf, _name_catalan_1m[SeedModChance(4, lengthof(_name_catalan_1m), seed)], last);
		buf = strecpy(buf, _name_catalan_2m[SeedModChance(11, lengthof(_name_catalan_2m), seed)], last);
	} else { // feminine form
		buf = strecpy(buf, _name_catalan_1f[SeedModChance(4, lengthof(_name_catalan_1f), seed)], last);
		buf = strecpy(buf, _name_catalan_2f[SeedModChance(11, lengthof(_name_catalan_2f), seed)], last);
	}

	if (SeedModChance(15, 5, seed) == 0) {
		if (SeedModChance(5, 2, seed) == 0) { // generic suffix
			buf = strecpy(buf, _name_catalan_3[SeedModChance(4, lengthof(_name_catalan_3), seed)], last);
		} else { // river name suffix
			buf = strecpy(buf, _name_catalan_river1[SeedModChance(4, lengthof(_name_catalan_river1), seed)], last);
		}
	}

	return buf;
}


/**
 * Type for all town name generator functions.
 * @param buf  The buffer to write the name to.
 * @param last The last element of the buffer.
 * @param seed The seed of the town name.
 * @return The end of the filled buffer.
 */
typedef char *TownNameGenerator(char *buf, const char *last, uint32 seed);

/** Contains pointer to generator and minimum buffer size (not incl. terminating '\0') */
struct TownNameGeneratorParams {
	byte min; ///< minimum number of characters that need to be printed for generator to work correctly
	TownNameGenerator *proc; ///< generator itself
};

/** Town name generators */
static const TownNameGeneratorParams _town_name_generators[] = {
	{  4, MakeEnglishOriginalTownName},  // replaces first 4 characters of name
	{  0, MakeFrenchTownName},
	{  0, MakeGermanTownName},
	{  4, MakeEnglishAdditionalTownName}, // replaces first 4 characters of name
	{  0, MakeSpanishTownName},
	{  0, MakeSillyTownName},
	{  0, MakeSwedishTownName},
	{  0, MakeDutchTownName},
	{  8, MakeFinnishTownName}, // _name_finnish_1
	{  0, MakePolishTownName},
	{  0, MakeSlovakTownName},
	{  0, MakeNorwegianTownName},
	{  0, MakeHungarianTownName},
	{  0, MakeAustrianTownName},
	{  0, MakeRomanianTownName},
	{ 28, MakeCzechTownName}, // _name_czech_adj + _name_czech_patmod + 1 + _name_czech_subst_stem + _name_czech_subst_postfix
	{  0, MakeSwissTownName},
	{  0, MakeDanishTownName},
	{  0, MakeTurkishTownName},
	{  0, MakeItalianTownName},
	{  0, MakeCatalanTownName},
};


/**
 * Generates town name from given seed.
 * @param buf output buffer
 * @param last end of buffer
 * @param lang town name language
 * @param seed generation seed
 * @return last character ('/0')
 */
char *GenerateTownNameString(char *buf, const char *last, size_t lang, uint32 seed)
{
	assert(lang < lengthof(_town_name_generators));

	/* Some generators need at least 9 bytes in buffer.  English generators need 5 for
	 * string replacing, others use constructions like strlen(buf)-3 and so on.
	 * Finnish generator needs to fit all strings from _name_finnish_1.
	 * Czech generator needs to fit almost whole town name...
	 * These would break. Using another temporary buffer results in ~40% slower code,
	 * so use it only when really needed. */
	const TownNameGeneratorParams *par = &_town_name_generators[lang];
	if (last >= buf + par->min) return par->proc(buf, last, seed);

	char *buffer = AllocaM(char, par->min + 1);
	par->proc(buffer, buffer + par->min, seed);

	return strecpy(buf, buffer, last);
}
