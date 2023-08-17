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
#include "strings_internal.h"

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
	if (t->townnamegrfid != 0 && GetGRFTownName(t->townnamegrfid) == nullptr) {
		/* Fallback to english original */
		this->grfid = 0;
		this->type = SPECSTR_TOWNNAME_ENGLISH;
		return;
	}
}


/**
 * Fills builder with specified town name.
 * @param builder       The string builder.
 * @param par           Town name parameters.
 * @param townnameparts 'Encoded' town name.
 */
static void GetTownName(StringBuilder &builder, const TownNameParams *par, uint32_t townnameparts)
{
	if (par->grfid == 0) {
		auto tmp_params = MakeParameters(townnameparts);
		GetStringWithArgs(builder, par->type, tmp_params);
		return;
	}

	GRFTownNameGenerate(builder, par->grfid, par->type, townnameparts);
}

/**
 * Get the town name for the given parameters and parts.
 * @param par Town name parameters.
 * @param townnameparts 'Encoded' town name.
 * @return The town name.
 */
std::string GetTownName(const TownNameParams *par, uint32_t townnameparts)
{
	std::string result;
	StringBuilder builder(result);
	GetTownName(builder, par, townnameparts);
	return result;
}

/**
 * Fills builder with town's name.
 * @param builder String builder.
 * @param t       The town to get the name from.
 */
void GetTownName(StringBuilder &builder, const Town *t)
{
	TownNameParams par(t);
	GetTownName(builder, &par, t->townnameparts);
}

/**
 * Get the name of the given town.
 * @param t The town to get the name for.
 * @return The town name.
 */
std::string GetTownName(const Town *t)
{
	TownNameParams par(t);
	return GetTownName(&par, t->townnameparts);
}


/**
 * Verifies the town name is valid and unique.
 * @param r random bits
 * @param par town name parameters
 * @param town_names if a name is generated, check its uniqueness with the set
 * @return true iff name is valid and unique
 */
bool VerifyTownName(uint32_t r, const TownNameParams *par, TownNames *town_names)
{
	std::string name = GetTownName(par, r);

	/* Check size and width */
	if (Utf8StringLength(name) >= MAX_LENGTH_TOWN_NAME_CHARS) return false;

	if (town_names != nullptr) {
		if (town_names->find(name) != town_names->end()) return false;
		town_names->insert(name);
	} else {
		for (const Town *t : Town::Iterate()) {
			/* We can't just compare the numbers since
			 * several numbers may map to a single name. */
			if (t->name.empty()) {
				if (name == GetTownName(t)) return false;
			} else {
				if (name == t->name) return false;
			}
		}
	}

	return true;
}


/**
 * Generates valid town name.
 * @param randomizer the source of random data for generating the name
 * @param townnameparts if a name is generated, it's stored there
 * @param town_names if a name is generated, check its uniqueness with the set
 * @return true iff a name was generated
 */
bool GenerateTownName(Randomizer &randomizer, uint32_t *townnameparts, TownNames *town_names)
{
	TownNameParams par(_settings_game.game_creation.town_name);

	/* This function is called very often without entering the gameloop
	 * in between. So reset layout cache to prevent it from growing too big. */
	Layouter::ReduceLineCache();

	/* Do not set i too low, since when we run out of names, we loop
	 * for #tries only one time anyway - then we stop generating more
	 * towns. Do not set it too high either, since looping through all
	 * the other towns may take considerable amount of time (10000 is
	 * too much). */
	for (int i = 1000; i != 0; i--) {
		uint32_t r = randomizer.Next();
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
static inline uint32_t SeedChance(byte shift_by, int max, uint32_t seed)
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
static inline uint32_t SeedModChance(byte shift_by, int max, uint32_t seed)
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
static inline int32_t SeedChanceBias(byte shift_by, int max, uint32_t seed, int bias)
{
	return SeedChance(shift_by, max + bias, seed) - bias;
}


/**
 * Replaces a string beginning in 'org' with 'rep'.
 * @param org     string to replace, has to be 4 characters long
 * @param rep     string to be replaced with, has to be 4 characters long
 * @param builder string builder of the town name
 * @param start   the start index within the builder for the town name
 */
static void ReplaceWords(const char *org, const char *rep, StringBuilder &builder, size_t start)
{
	assert(strlen(org) == 4 && strlen(rep) == 4 && builder.CurrentIndex() - start >= 4);
	if (strncmp(&builder[start], org, 4) == 0) memcpy(&builder[start], rep, 4); // Safe as the string in buf is always more than 4 characters long.
}


/**
 * Replaces english curses and ugly letter combinations by nicer ones.
 * @param builder  The builder with the town name
 * @param start    The start index into the builder for the first town name
 * @param original English (Original) generator was used
 */
static void ReplaceEnglishWords(StringBuilder &builder, size_t start, bool original)
{
	ReplaceWords("Cunt", "East", builder, start);
	ReplaceWords("Slag", "Pits", builder, start);
	ReplaceWords("Slut", "Edin", builder, start);
	if (!original) ReplaceWords("Fart", "Boot", builder, start); // never happens with 'English (Original)'
	ReplaceWords("Drar", "Quar", builder, start);
	ReplaceWords("Dreh", "Bash", builder, start);
	ReplaceWords("Frar", "Shor", builder, start);
	ReplaceWords("Grar", "Aber", builder, start);
	ReplaceWords("Brar", "Over", builder, start);
	ReplaceWords("Wrar", original ? "Inve" : "Stan", builder, start);
}

/**
 * Generates English (Original) town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeEnglishOriginalTownName(StringBuilder &builder, uint32_t seed)
{
	size_t start = builder.CurrentIndex();

	/* optional first segment */
	int i = SeedChanceBias(0, lengthof(_name_original_english_1), seed, 50);
	if (i >= 0) builder += _name_original_english_1[i];

	/* mandatory middle segments */
	builder += _name_original_english_2[SeedChance(4,  lengthof(_name_original_english_2), seed)];
	builder += _name_original_english_3[SeedChance(7,  lengthof(_name_original_english_3), seed)];
	builder += _name_original_english_4[SeedChance(10, lengthof(_name_original_english_4), seed)];
	builder += _name_original_english_5[SeedChance(13, lengthof(_name_original_english_5), seed)];

	/* optional last segment */
	i = SeedChanceBias(15, lengthof(_name_original_english_6), seed, 60);
	if (i >= 0) builder += _name_original_english_6[i];

	/* Ce, Ci => Ke, Ki */
	if (builder[start] == 'C' && (builder[start + 1] == 'e' || builder[start + 1] == 'i')) {
		builder[start] = 'K';
	}

	assert(builder.CurrentIndex() - start >= 4);
	ReplaceEnglishWords(builder, start, true);
}


/**
 * Generates English (Additional) town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeEnglishAdditionalTownName(StringBuilder &builder, uint32_t seed)
{
	size_t start = builder.CurrentIndex();

	/* optional first segment */
	int i = SeedChanceBias(0, lengthof(_name_additional_english_prefix), seed, 50);
	if (i >= 0) builder += _name_additional_english_prefix[i];

	if (SeedChance(3, 20, seed) >= 14) {
		builder += _name_additional_english_1a[SeedChance(6, lengthof(_name_additional_english_1a), seed)];
	} else {
		builder += _name_additional_english_1b1[SeedChance(6, lengthof(_name_additional_english_1b1), seed)];
		builder += _name_additional_english_1b2[SeedChance(9, lengthof(_name_additional_english_1b2), seed)];
		if (SeedChance(11, 20, seed) >= 4) {
			builder += _name_additional_english_1b3a[SeedChance(12, lengthof(_name_additional_english_1b3a), seed)];
		} else {
			builder += _name_additional_english_1b3b[SeedChance(12, lengthof(_name_additional_english_1b3b), seed)];
		}
	}

	builder += _name_additional_english_2[SeedChance(14, lengthof(_name_additional_english_2), seed)];

	/* optional last segment */
	i = SeedChanceBias(15, lengthof(_name_additional_english_3), seed, 60);
	if (i >= 0) builder += _name_additional_english_3[i];

	assert(builder.CurrentIndex() - start >= 4);
	ReplaceEnglishWords(builder, start, false);
}


/**
 * Generates Austrian town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeAustrianTownName(StringBuilder &builder, uint32_t seed)
{
	/* Bad, Maria, Gross, ... */
	int i = SeedChanceBias(0, lengthof(_name_austrian_a1), seed, 15);
	if (i >= 0) builder += _name_austrian_a1[i];

	int j = 0;

	i = SeedChance(4, 6, seed);
	if (i >= 4) {
		/* Kaisers-kirchen */
		builder += _name_austrian_a2[SeedChance( 7, lengthof(_name_austrian_a2), seed)];
		builder += _name_austrian_a3[SeedChance(13, lengthof(_name_austrian_a3), seed)];
	} else if (i >= 2) {
		/* St. Johann */
		builder += _name_austrian_a5[SeedChance( 7, lengthof(_name_austrian_a5), seed)];
		builder += _name_austrian_a6[SeedChance( 9, lengthof(_name_austrian_a6), seed)];
		j = 1; // More likely to have a " an der " or " am "
	} else {
		/* Zell */
		builder += _name_austrian_a4[SeedChance( 7, lengthof(_name_austrian_a4), seed)];
	}

	i = SeedChance(1, 6, seed);
	if (i >= 4 - j) {
		/* an der Donau (rivers) */
		builder += _name_austrian_f1[SeedChance(4, lengthof(_name_austrian_f1), seed)];
		builder += _name_austrian_f2[SeedChance(5, lengthof(_name_austrian_f2), seed)];
	} else if (i >= 2 - j) {
		/* am Dachstein (mountains) */
		builder += _name_austrian_b1[SeedChance(4, lengthof(_name_austrian_b1), seed)];
		builder += _name_austrian_b2[SeedChance(5, lengthof(_name_austrian_b2), seed)];
	}
}


/**
 * Generates German town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeGermanTownName(StringBuilder &builder, uint32_t seed)
{
	uint seed_derivative = SeedChance(7, 28, seed);

	/* optional prefix */
	if (seed_derivative == 12 || seed_derivative == 19) {
		uint i = SeedChance(2, lengthof(_name_german_pre), seed);
		builder += _name_german_pre[i];
	}

	/* mandatory middle segments including option of hardcoded name */
	uint i = SeedChance(3, lengthof(_name_german_real) + lengthof(_name_german_1), seed);
	if (i < lengthof(_name_german_real)) {
		builder += _name_german_real[i];
	} else {
		builder += _name_german_1[i - lengthof(_name_german_real)];

		i = SeedChance(5, lengthof(_name_german_2), seed);
		builder += _name_german_2[i];
	}

	/* optional suffix */
	if (seed_derivative == 24) {
		i = SeedChance(9, lengthof(_name_german_4_an_der) + lengthof(_name_german_4_am), seed);
		if (i < lengthof(_name_german_4_an_der)) {
			builder += _name_german_3_an_der[0];
			builder += _name_german_4_an_der[i];
		} else {
			builder += _name_german_3_am[0];
			builder += _name_german_4_am[i - lengthof(_name_german_4_an_der)];
		}
	}
}


/**
 * Generates Latin-American town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeSpanishTownName(StringBuilder &builder, uint32_t seed)
{
	builder += _name_spanish_real[SeedChance(0, lengthof(_name_spanish_real), seed)];
}


/**
 * Generates French town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeFrenchTownName(StringBuilder &builder, uint32_t seed)
{
	builder += _name_french_real[SeedChance(0, lengthof(_name_french_real), seed)];
}


/**
 * Generates Silly town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeSillyTownName(StringBuilder &builder, uint32_t seed)
{
	builder += _name_silly_1[SeedChance( 0, lengthof(_name_silly_1), seed)];
	builder += _name_silly_2[SeedChance(16, lengthof(_name_silly_2), seed)];
}


/**
 * Generates Swedish town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeSwedishTownName(StringBuilder &builder, uint32_t seed)
{
	/* optional first segment */
	int i = SeedChanceBias(0, lengthof(_name_swedish_1), seed, 50);
	if (i >= 0) builder += _name_swedish_1[i];

	/* mandatory middle segments including option of hardcoded name */
	if (SeedChance(4, 5, seed) >= 3) {
		builder += _name_swedish_2[SeedChance( 7, lengthof(_name_swedish_2), seed)];
	} else {
		builder += _name_swedish_2a[SeedChance( 7, lengthof(_name_swedish_2a), seed)];
		builder += _name_swedish_2b[SeedChance(10, lengthof(_name_swedish_2b), seed)];
		builder += _name_swedish_2c[SeedChance(13, lengthof(_name_swedish_2c), seed)];
	}

	builder += _name_swedish_3[SeedChance(16, lengthof(_name_swedish_3), seed)];
}


/**
 * Generates Dutch town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeDutchTownName(StringBuilder &builder, uint32_t seed)
{
	/* optional first segment */
	int i = SeedChanceBias(0, lengthof(_name_dutch_1), seed, 50);
	if (i >= 0) builder += _name_dutch_1[i];

	/* mandatory middle segments including option of hardcoded name */
	if (SeedChance(6, 9, seed) > 4) {
		builder += _name_dutch_2[SeedChance( 9, lengthof(_name_dutch_2), seed)];
	} else {
		builder += _name_dutch_3[SeedChance( 9, lengthof(_name_dutch_3), seed)];
		builder += _name_dutch_4[SeedChance(12, lengthof(_name_dutch_4), seed)];
	}

	builder += _name_dutch_5[SeedChance(15, lengthof(_name_dutch_5), seed)];
}


/**
 * Generates Finnish town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeFinnishTownName(StringBuilder &builder, uint32_t seed)
{
	size_t start = builder.CurrentIndex();

	/* Select randomly if town name should consists of one or two parts. */
	if (SeedChance(0, 15, seed) >= 10) {
		builder += _name_finnish_real[SeedChance(2, lengthof(_name_finnish_real), seed)];
		return;
	}

	if (SeedChance(0, 15, seed) >= 5) {
		/* A two-part name by combining one of _name_finnish_1 + "la"/"lä"
		 * The reason for not having the contents of _name_finnish_{1,2} in the same table is
		 * that the ones in _name_finnish_2 are not good for this purpose. */
		uint sel = SeedChance( 0, lengthof(_name_finnish_1), seed);
		builder += _name_finnish_1[sel];
		size_t last = builder.CurrentIndex() - 1;
		if (builder[last] == 'i') builder[last] = 'e';

		std::string_view view(&builder[start], builder.CurrentIndex() - start);
		if (view.find_first_of("aouAOU") != std::string_view::npos) {
			builder += "la";
		} else {
			builder += u8"l\u00e4";
		}
		return;
	}

	/* A two-part name by combining one of _name_finnish_{1,2} + _name_finnish_3.
	 * Why aren't _name_finnish_{1,2} just one table? See above. */
	uint sel = SeedChance(2, lengthof(_name_finnish_1) + lengthof(_name_finnish_2), seed);
	if (sel >= lengthof(_name_finnish_1)) {
		builder += _name_finnish_2[sel - lengthof(_name_finnish_1)];
	} else {
		builder += _name_finnish_1[sel];
	}

	builder += _name_finnish_3[SeedChance(10, lengthof(_name_finnish_3), seed)];
}


/**
 * Generates Polish town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakePolishTownName(StringBuilder &builder, uint32_t seed)
{
	/* optional first segment */
	uint i = SeedChance(0,
			lengthof(_name_polish_2_o) + lengthof(_name_polish_2_m) +
			lengthof(_name_polish_2_f) + lengthof(_name_polish_2_n),
			seed);
	uint j = SeedChance(2, 20, seed);


	if (i < lengthof(_name_polish_2_o)) {
		builder += _name_polish_2_o[SeedChance(3, lengthof(_name_polish_2_o), seed)];
		return;
	}

	if (i < lengthof(_name_polish_2_m) + lengthof(_name_polish_2_o)) {
		if (j < 4) {
			builder += _name_polish_1_m[SeedChance(5, lengthof(_name_polish_1_m), seed)];
		}

		builder += _name_polish_2_m[SeedChance(7, lengthof(_name_polish_2_m), seed)];

		if (j >= 4 && j < 16) {
			builder += _name_polish_3_m[SeedChance(10, lengthof(_name_polish_3_m), seed)];
		}

		return;
	}

	if (i < lengthof(_name_polish_2_f) + lengthof(_name_polish_2_m) + lengthof(_name_polish_2_o)) {
		if (j < 4) {
			builder += _name_polish_1_f[SeedChance(5, lengthof(_name_polish_1_f), seed)];
		}

		builder += _name_polish_2_f[SeedChance(7, lengthof(_name_polish_2_f), seed)];

		if (j >= 4 && j < 16) {
			builder += _name_polish_3_f[SeedChance(10, lengthof(_name_polish_3_f), seed)];
		}

		return;
	}

	if (j < 4) {
		builder += _name_polish_1_n[SeedChance(5, lengthof(_name_polish_1_n), seed)];
	}

	builder += _name_polish_2_n[SeedChance(7, lengthof(_name_polish_2_n), seed)];

	if (j >= 4 && j < 16) {
		builder += _name_polish_3_n[SeedChance(10, lengthof(_name_polish_3_n), seed)];
	}

	return;
}


/**
 * Generates Czech town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeCzechTownName(StringBuilder &builder, uint32_t seed)
{
	/* 1:3 chance to use a real name. */
	if (SeedModChance(0, 4, seed) == 0) {
		builder += _name_czech_real[SeedModChance(4, lengthof(_name_czech_real), seed)];
		return;
	}

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

		builder += _name_czech_adj[prefix].name;

		size_t endpos = builder.CurrentIndex() - 1;
		/* Find the first character in a UTF-8 sequence */
		while (GB(builder[endpos], 6, 2) == 2) endpos--;
		builder.RemoveElementsFromBack(builder.CurrentIndex() - endpos);

		if (gender == CZG_SMASC && pattern == CZP_PRIVL) {
			/* -ovX -> -uv */
			builder[endpos - 2] = 'u';
		} else {
			builder += _name_czech_patmod[gender][pattern];
		}

		builder += ' ';
	}

	if (dynamic_subst) {
		builder += _name_czech_subst_stem[stem].name;
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
				builder += poststr;

				/* k-i -> c-i, h-i -> z-i */
				if (endstr[0] == 'i') {
					size_t last = builder.CurrentIndex() - 1;
					switch (builder[last]) {
						case 'k': builder[last] = 'c'; break;
						case 'h': builder[last] = 'z'; break;
						default: break;
					}
				}
			}
		}
		builder += _name_czech_subst_ending[ending].name;
	} else {
		builder += _name_czech_subst_full[stem].name;
	}

	if (do_suffix) {
		builder += " ";
		builder += _name_czech_suffix[suffix];
	}
}


/**
 * Generates Romanian town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeRomanianTownName(StringBuilder &builder, uint32_t seed)
{
	builder += _name_romanian_real[SeedChance(0, lengthof(_name_romanian_real), seed)];
}


/**
 * Generates Slovak town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeSlovakTownName(StringBuilder &builder, uint32_t seed)
{
	builder += _name_slovak_real[SeedChance(0, lengthof(_name_slovak_real), seed)];
}


/**
 * Generates Norwegian town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeNorwegianTownName(StringBuilder &builder, uint32_t seed)
{
	/* Use first 4 bit from seed to decide whether or not this town should
	 * have a real name 3/16 chance.  Bit 0-3 */
	if (SeedChance(0, 15, seed) < 3) {
		/* Use 7bit for the realname table index.  Bit 4-10 */
		builder += _name_norwegian_real[SeedChance(4, lengthof(_name_norwegian_real), seed)];
		return;
	}

	/* Use 7bit for the first fake part.  Bit 4-10 */
	builder += _name_norwegian_1[SeedChance(4, lengthof(_name_norwegian_1), seed)];
	/* Use 7bit for the last fake part.  Bit 11-17 */
	builder += _name_norwegian_2[SeedChance(11, lengthof(_name_norwegian_2), seed)];
}


/**
 * Generates Hungarian town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeHungarianTownName(StringBuilder &builder, uint32_t seed)
{
	if (SeedChance(12, 15, seed) < 3) {
		builder += _name_hungarian_real[SeedChance(0, lengthof(_name_hungarian_real), seed)];
		return;
	}

	/* optional first segment */
	uint i = SeedChance(3, lengthof(_name_hungarian_1) * 3, seed);
	if (i < lengthof(_name_hungarian_1)) builder += _name_hungarian_1[i];

	/* mandatory middle segments */
	builder += _name_hungarian_2[SeedChance(3, lengthof(_name_hungarian_2), seed)];
	builder += _name_hungarian_3[SeedChance(6, lengthof(_name_hungarian_3), seed)];

	/* optional last segment */
	i = SeedChance(10, lengthof(_name_hungarian_4) * 3, seed);
	if (i < lengthof(_name_hungarian_4)) {
		builder += _name_hungarian_4[i];
	}
}


/**
 * Generates Swiss town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeSwissTownName(StringBuilder &builder, uint32_t seed)
{
	builder += _name_swiss_real[SeedChance(0, lengthof(_name_swiss_real), seed)];
}


/**
 * Generates Danish town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeDanishTownName(StringBuilder &builder, uint32_t seed)
{
	/* optional first segment */
	int i = SeedChanceBias(0, lengthof(_name_danish_1), seed, 50);
	if (i >= 0) builder += _name_danish_1[i];

	/* middle segments removed as this algorithm seems to create much more realistic names */
	builder += _name_danish_2[SeedChance( 7, lengthof(_name_danish_2), seed)];
	builder += _name_danish_3[SeedChance(16, lengthof(_name_danish_3), seed)];
}


/**
 * Generates Turkish town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeTurkishTownName(StringBuilder &builder, uint32_t seed)
{
	uint i = SeedModChance(0, 5, seed);

	switch (i) {
		case 0:
			builder += _name_turkish_prefix[SeedModChance( 2, lengthof(_name_turkish_prefix), seed)];

			/* middle segment */
			builder += _name_turkish_middle[SeedModChance( 4, lengthof(_name_turkish_middle), seed)];

			/* optional suffix */
			if (SeedModChance(0, 7, seed) == 0) {
				builder += _name_turkish_suffix[SeedModChance( 10, lengthof(_name_turkish_suffix), seed)];
			}
			break;

		case 1: case 2:
			builder += _name_turkish_prefix[SeedModChance( 2, lengthof(_name_turkish_prefix), seed)];
			builder += _name_turkish_suffix[SeedModChance( 4, lengthof(_name_turkish_suffix), seed)];
			break;

		default:
			builder += _name_turkish_real[SeedModChance( 4, lengthof(_name_turkish_real), seed)];
			break;
	}
}


/**
 * Generates Italian town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeItalianTownName(StringBuilder &builder, uint32_t seed)
{
	if (SeedModChance(0, 6, seed) == 0) { // real city names
		builder += _name_italian_real[SeedModChance(4, lengthof(_name_italian_real), seed)];
		return;
	}

	static const char * const mascul_femin_italian[] = {
		"o",
		"a",
	};

	if (SeedModChance(0, 8, seed) == 0) { // prefix
		builder += _name_italian_pref[SeedModChance(11, lengthof(_name_italian_pref), seed)];
	}

	uint i = SeedChance(0, 2, seed);
	if (i == 0) { // masculine form
		builder += _name_italian_1m[SeedModChance(4, lengthof(_name_italian_1m), seed)];
	} else { // feminine form
		builder += _name_italian_1f[SeedModChance(4, lengthof(_name_italian_1f), seed)];
	}

	if (SeedModChance(3, 3, seed) == 0) {
		builder += _name_italian_2[SeedModChance(11, lengthof(_name_italian_2), seed)];
		builder += mascul_femin_italian[i];
	} else {
		builder += _name_italian_2i[SeedModChance(16, lengthof(_name_italian_2i), seed)];
	}

	if (SeedModChance(15, 4, seed) == 0) {
		if (SeedModChance(5, 2, seed) == 0) { // generic suffix
			builder += _name_italian_3[SeedModChance(4, lengthof(_name_italian_3), seed)];
		} else { // river name suffix
			builder += _name_italian_river1[SeedModChance(4, lengthof(_name_italian_river1), seed)];
			builder += _name_italian_river2[SeedModChance(16, lengthof(_name_italian_river2), seed)];
		}
	}
}


/**
 * Generates Catalan town name from given seed.
 * @param builder string builder
 * @param seed town name seed
 */
static void MakeCatalanTownName(StringBuilder &builder, uint32_t seed)
{
	if (SeedModChance(0, 3, seed) == 0) { // real city names
		builder += _name_catalan_real[SeedModChance(4, lengthof(_name_catalan_real), seed)];
		return;
	}

	if (SeedModChance(0, 2, seed) == 0) { // prefix
		builder += _name_catalan_pref[SeedModChance(11, lengthof(_name_catalan_pref), seed)];
	}

	uint i = SeedChance(0, 2, seed);
	if (i == 0) { // masculine form
		builder += _name_catalan_1m[SeedModChance(4, lengthof(_name_catalan_1m), seed)];
		builder += _name_catalan_2m[SeedModChance(11, lengthof(_name_catalan_2m), seed)];
	} else { // feminine form
		builder += _name_catalan_1f[SeedModChance(4, lengthof(_name_catalan_1f), seed)];
		builder += _name_catalan_2f[SeedModChance(11, lengthof(_name_catalan_2f), seed)];
	}

	if (SeedModChance(15, 5, seed) == 0) {
		if (SeedModChance(5, 2, seed) == 0) { // generic suffix
			builder += _name_catalan_3[SeedModChance(4, lengthof(_name_catalan_3), seed)];
		} else { // river name suffix
			builder += _name_catalan_river1[SeedModChance(4, lengthof(_name_catalan_river1), seed)];
		}
	}
}


/**
 * Type for all town name generator functions.
 * @param builder The builder to write the name to.
 * @param seed The seed of the town name.
 */
typedef void TownNameGenerator(StringBuilder &builder, uint32_t seed);

/** Town name generators */
static TownNameGenerator *_town_name_generators[] = {
	MakeEnglishOriginalTownName,  // replaces first 4 characters of name
	MakeFrenchTownName,
	MakeGermanTownName,
	MakeEnglishAdditionalTownName, // replaces first 4 characters of name
	MakeSpanishTownName,
	MakeSillyTownName,
	MakeSwedishTownName,
	MakeDutchTownName,
	MakeFinnishTownName, // _name_finnish_1
	MakePolishTownName,
	MakeSlovakTownName,
	MakeNorwegianTownName,
	MakeHungarianTownName,
	MakeAustrianTownName,
	MakeRomanianTownName,
	MakeCzechTownName, // _name_czech_adj + _name_czech_patmod + 1 + _name_czech_subst_stem + _name_czech_subst_postfix
	MakeSwissTownName,
	MakeDanishTownName,
	MakeTurkishTownName,
	MakeItalianTownName,
	MakeCatalanTownName,
};


/**
 * Generates town name from given seed.
 * @param builder string builder to write to
 * @param lang    town name language
 * @param seed    generation seed
 */
void GenerateTownNameString(StringBuilder &builder, size_t lang, uint32_t seed)
{
	assert(lang < lengthof(_town_name_generators));
	return _town_name_generators[lang](builder, seed);
}
