#include "stdafx.h"
#include "ttd.h"

#include "table/namegen.h"

static inline uint32 SeedChance(int shift_by, int max, uint32 seed)
{
	return ((uint16)(seed >> shift_by) * max) >> 16;
}

static inline int32 SeedChanceBias(int shift_by, int max, uint32 seed, int bias)
{
	return SeedChance(shift_by, max + bias, seed) - bias;
}

static void ReplaceWords(const char *org, const char *rep, char *buf) 
{
	if (strncmp(buf, org, 4) == 0) strncpy(buf, rep, 4);
}

static byte MakeEnglishOriginalTownName(byte *buf, uint32 seed)
{
	int i;

	//null terminates the string for strcat
	strcpy(buf, "");

	// optional first segment
	i = SeedChanceBias(0, lengthof(name_original_english_1), seed, 50);
	if (i >= 0)
		strcat(buf,name_original_english_1[i]);

	//mandatory middle segments
	strcat(buf, name_original_english_2[SeedChance(4,  lengthof(name_original_english_2), seed)]);
	strcat(buf, name_original_english_3[SeedChance(7,  lengthof(name_original_english_3), seed)]);
	strcat(buf, name_original_english_4[SeedChance(10, lengthof(name_original_english_4), seed)]);
	strcat(buf, name_original_english_5[SeedChance(13, lengthof(name_original_english_5), seed)]);

	//optional last segment
	i = SeedChanceBias(15, lengthof(name_original_english_6), seed, 60);
	if (i >= 0)
		strcat(buf, name_original_english_6[i]);

	if (buf[0] == 'C' && (buf[1] == 'e' || buf[1] == 'i'))
		buf[0] = 'K';

	ReplaceWords("Cunt", "East", buf);
	ReplaceWords("Slag", "Pits", buf);
	ReplaceWords("Slut", "Edin", buf);
	//ReplaceWords("Fart", "Boot", buf);
	ReplaceWords("Drar", "Quar", buf);
	ReplaceWords("Dreh", "Bash", buf);
	ReplaceWords("Frar", "Shor", buf);
	ReplaceWords("Grar", "Aber", buf);
	ReplaceWords("Brar", "Over", buf);
	ReplaceWords("Wrar", "Inve", buf);

	return 0;
}


static byte MakeEnglishAdditionalTownName(byte *buf, uint32 seed)
{
	int i;

	//null terminates the string for strcat
	strcpy(buf, "");

	// optional first segment
	i = SeedChanceBias(0, lengthof(name_additional_english_prefix), seed, 50);
	if (i >= 0)
		strcat(buf,name_additional_english_prefix[i]);

	if (SeedChance(3, 20, seed) >= 14) {
		strcat(buf, name_additional_english_1a[SeedChance(6, lengthof(name_additional_english_1a), seed)]);
	} else {
		strcat(buf, name_additional_english_1b1[SeedChance(6, lengthof(name_additional_english_1b1), seed)]);
		strcat(buf, name_additional_english_1b2[SeedChance(9, lengthof(name_additional_english_1b2), seed)]);
		if (SeedChance(11, 20, seed) >= 4) {
			strcat(buf, name_additional_english_1b3a[SeedChance(12, lengthof(name_additional_english_1b3a), seed)]);
		} else {
			strcat(buf, name_additional_english_1b3b[SeedChance(12, lengthof(name_additional_english_1b3b), seed)]);
		}
	}

	strcat(buf, name_additional_english_2[SeedChance(14, lengthof(name_additional_english_2), seed)]);

	//optional last segment
	i = SeedChanceBias(15, lengthof(name_additional_english_3), seed, 60);
	if (i >= 0)
		strcat(buf, name_additional_english_3[i]);

	ReplaceWords("Cunt", "East", buf);
	ReplaceWords("Slag", "Pits", buf);
	ReplaceWords("Slut", "Edin", buf);
	ReplaceWords("Fart", "Boot", buf);
	ReplaceWords("Drar", "Quar", buf);
	ReplaceWords("Dreh", "Bash", buf);
	ReplaceWords("Frar", "Shor", buf);
	ReplaceWords("Grar", "Aber", buf);
	ReplaceWords("Brar", "Over", buf);
	ReplaceWords("Wrar", "Stan", buf);

	return 0;
}

static byte MakeAustrianTownName(byte *buf, uint32 seed)
{
	int i, j = 0;
	strcpy(buf, "");

	// Bad, Maria, Gross, ...
	i = SeedChanceBias(0, lengthof(name_austrian_a1), seed, 15);
	if (i >= 0) strcat(buf, name_austrian_a1[i]);

	i = SeedChance(4, 6, seed);
	if (i >= 4) {
		// Kaisers-kirchen
		strcat(buf, name_austrian_a2[SeedChance( 7, lengthof(name_austrian_a2), seed)]);
		strcat(buf, name_austrian_a3[SeedChance(13, lengthof(name_austrian_a3), seed)]);
	} else if (i >= 2) {
		// St. Johann
		strcat(buf, name_austrian_a5[SeedChance( 7, lengthof(name_austrian_a5), seed)]);
		strcat(buf, name_austrian_a6[SeedChance( 9, lengthof(name_austrian_a6), seed)]);
		j = 1; // More likely to have a " an der " or " am "
	} else {
		// Zell
		strcat(buf, name_austrian_a4[SeedChance( 7, lengthof(name_austrian_a4), seed)]);
	}

	i = SeedChance(1, 6, seed);
	if (i >= 4 - j) {
		// an der Donau (rivers)
		strcat(buf, name_austrian_f1[SeedChance(4, lengthof(name_austrian_f1), seed)]);
		strcat(buf, name_austrian_f2[SeedChance(5, lengthof(name_austrian_f2), seed)]);
	} else if (i >= 2 - j) {
		// am Dachstein (mountains)
		strcat(buf, name_austrian_b1[SeedChance(4, lengthof(name_austrian_b1), seed)]);
		strcat(buf, name_austrian_b2[SeedChance(5, lengthof(name_austrian_b2), seed)]);
	}

	return 0;
}

static byte MakeGermanTownName(byte *buf, uint32 seed)
{
	uint i;
	uint seed_derivative;

	//null terminates the string for strcat
	strcpy(buf, "");

	seed_derivative = SeedChance(7, 28, seed);

	//optional prefix
	if (seed_derivative == 12 || seed_derivative == 19) {
		i = SeedChance(2, lengthof(name_german_pre), seed);
		strcat(buf,name_german_pre[i]);
	}

	// mandatory middle segments including option of hardcoded name
	i = SeedChance(3, lengthof(name_german_hardcoded) + lengthof(name_german_1), seed);
	if (i < lengthof(name_german_hardcoded)) {
		strcat(buf,name_german_hardcoded[i]);
	} else {
		strcat(buf, name_german_1[i - lengthof(name_german_hardcoded)]);

		i = SeedChance(5, lengthof(name_german_2), seed);
		strcat(buf, name_german_2[i]);
	}

	// optional suffix
	if (seed_derivative == 24) {
		i = SeedChance(9,
			lengthof(name_german_4_an_der) + lengthof(name_german_4_am), seed);
		if (i < lengthof(name_german_4_an_der)) {
			strcat(buf, name_german_3_an_der[0]);
			strcat(buf, name_german_4_an_der[i]);
		} else {
			strcat(buf, name_german_3_am[0]);
			strcat(buf, name_german_4_am[i - lengthof(name_german_4_an_der)]);
		}
	}
	return 0;
}

static byte MakeSpanishTownName(byte *buf, uint32 seed)
{
	strcpy(buf, name_spanish_1[SeedChance(0, lengthof(name_spanish_1), seed)]);
	return 0;
}

static byte MakeFrenchTownName(byte *buf, uint32 seed)
{
	strcpy(buf, name_french_1[SeedChance(0, lengthof(name_french_1), seed)]);
	return 0;
}

static byte MakeSillyTownName(byte *buf, uint32 seed)
{
	strcpy(buf, name_silly_1[SeedChance( 0, lengthof(name_silly_1), seed)]);
	strcat(buf, name_silly_2[SeedChance(16, lengthof(name_silly_2), seed)]);
	return 0;
}

static byte MakeSwedishTownName(byte *buf, uint32 seed)
{
	int i;

	//null terminates the string for strcat
	strcpy(buf, "");

	// optional first segment
	i = SeedChanceBias(0, lengthof(name_swedish_1), seed, 50);
	if (i >= 0)
		strcat(buf, name_swedish_1[i]);

	// mandatory middle segments including option of hardcoded name
	if (SeedChance(4, 5, seed) >= 3) {
		strcat(buf, name_swedish_2[SeedChance( 7, lengthof(name_swedish_2), seed)]);
	} else {
		strcat(buf, name_swedish_2a[SeedChance( 7, lengthof(name_swedish_2a), seed)]);
		strcat(buf, name_swedish_2b[SeedChance(10, lengthof(name_swedish_2b), seed)]);
		strcat(buf, name_swedish_2c[SeedChance(13, lengthof(name_swedish_2c), seed)]);
	}

	strcat(buf, name_swedish_3[SeedChance(16, lengthof(name_swedish_3), seed)]);

	return 0;
}

static byte MakeDutchTownName(byte *buf, uint32 seed)
{
	int i;

	//null terminates the string for strcat
	strcpy(buf, "");

	// optional first segment
	i = SeedChanceBias(0, lengthof(name_dutch_1), seed, 50);
	if (i >= 0)
		strcat(buf, name_dutch_1[i]);

	// mandatory middle segments including option of hardcoded name
	if (SeedChance(6, 9, seed) > 4) {
		strcat(buf, name_dutch_2[SeedChance( 9, lengthof(name_dutch_2), seed)]);
	} else {
		strcat(buf, name_dutch_3[SeedChance( 9, lengthof(name_dutch_3), seed)]);
		strcat(buf, name_dutch_4[SeedChance(12, lengthof(name_dutch_4), seed)]);
	}
	strcat(buf, name_dutch_5[SeedChance(15, lengthof(name_dutch_5), seed)]);

	return 0;
}

static byte MakeFinnishTownName(byte *buf, uint32 seed)
{
	//null terminates the string for strcat
	strcpy(buf, "");

	// Select randomly if town name should consists of one or two parts.
	if (SeedChance(0, 15, seed) >= 10) {
		strcat(buf, name_finnish_1[SeedChance( 2, lengthof(name_finnish_1), seed)]);
	} else {
		strcat(buf, name_finnish_2a[SeedChance( 2, lengthof(name_finnish_2a), seed)]);
		strcat(buf, name_finnish_2b[SeedChance(10, lengthof(name_finnish_2b), seed)]);
	}

	return 0;
}

static byte MakePolishTownName(byte *buf, uint32 seed)
{
	uint i;
	uint j;

	//null terminates the string for strcat
	strcpy(buf, "");

	// optional first segment
	i = SeedChance(0,
		lengthof(name_polish_2_o) + lengthof(name_polish_2_m) +
		lengthof(name_polish_2_f) + lengthof(name_polish_2_n),
		seed);
	j = SeedChance(2, 20, seed);


	if (i < lengthof(name_polish_2_o)) {
		strcat(buf, name_polish_2_o[SeedChance(3, lengthof(name_polish_2_o), seed)]);
	} else if (i < lengthof(name_polish_2_m) + lengthof(name_polish_2_o)) {
		if (j < 4)
			strcat(buf, name_polish_1_m[SeedChance(5, lengthof(name_polish_1_m), seed)]);

		strcat(buf, name_polish_2_m[SeedChance(7, lengthof(name_polish_2_m), seed)]);

		if (j >= 4 && j < 16)
			strcat(buf, name_polish_3_m[SeedChance(10, lengthof(name_polish_3_m), seed)]);
	} else if (i < lengthof(name_polish_2_f) + lengthof(name_polish_2_m) + lengthof(name_polish_2_o)) {
		if (j < 4)
			strcat(buf, name_polish_1_f[SeedChance(5, lengthof(name_polish_1_f), seed)]);

		strcat(buf, name_polish_2_f[SeedChance(7, lengthof(name_polish_2_f), seed)]);

		if (j >= 4 && j < 16)
			strcat(buf, name_polish_3_f[SeedChance(10, lengthof(name_polish_3_f), seed)]);
	} else {
		if (j < 4)
			strcat(buf, name_polish_1_n[SeedChance(5, lengthof(name_polish_1_n), seed)]);

		strcat(buf, name_polish_2_n[SeedChance(7, lengthof(name_polish_2_n), seed)]);

		if (j >= 4 && j < 16)
			strcat(buf, name_polish_3_n[SeedChance(10, lengthof(name_polish_3_n), seed)]);
	}
	return 0;
}

static byte MakeCzechTownName(byte *buf, uint32 seed)
{
	strcpy(buf, name_czech_1[SeedChance(0, lengthof(name_czech_1), seed)]);
	return 0;
}

static byte MakeRomanianTownName(byte *buf, uint32 seed)
{
	strcpy(buf, name_romanian_1[SeedChance(0, lengthof(name_romanian_1), seed)]);
	return 0;
}

static byte MakeSlovakTownName(byte *buf, uint32 seed)
{
	strcpy(buf, name_slovakish_1[SeedChance(0, lengthof(name_slovakish_1), seed)]);
	return 0;
}

static byte MakeNorwegianTownName(byte *buf, uint32 seed)
{
	strcpy(buf, "");

	// Use first 4 bit from seed to decide whether or not this town should
	// have a real name 3/16 chance.  Bit 0-3
	if (SeedChance(0, 15, seed) < 3) {
		// Use 7bit for the realname table index.  Bit 4-10
		strcat(buf, name_norwegian_real[SeedChance(4, lengthof(name_norwegian_real), seed)]);
	} else {
		// Use 7bit for the first fake part.  Bit 4-10
		strcat(buf, name_norwegian_1[SeedChance(4, lengthof(name_norwegian_1), seed)]);
		// Use 7bit for the last fake part.  Bit 11-17
		strcat(buf, name_norwegian_2[SeedChance(11, lengthof(name_norwegian_2), seed)]);
	}

	return 0;
}

static byte MakeHungarianTownName(byte *buf, uint32 seed)
{
	uint i;

	//null terminates the string for strcat
	strcpy(buf, "");

	if (SeedChance(12, 15, seed) < 3) {
		strcat(buf, name_hungarian_real[SeedChance(0, lengthof(name_hungarian_real), seed)]);
	} else {
		// optional first segment
		i = SeedChance(3, lengthof(name_hungarian_1) * 3, seed);
		if (i < lengthof(name_hungarian_1))
			strcat(buf, name_hungarian_1[i]);

		// mandatory middle segments
		strcat(buf, name_hungarian_2[SeedChance(3, lengthof(name_hungarian_2), seed)]);
		strcat(buf, name_hungarian_3[SeedChance(6, lengthof(name_hungarian_3), seed)]);

		// optional last segment
		i = SeedChance(10, lengthof(name_hungarian_4) * 3, seed);
		if (i < lengthof(name_hungarian_4)) {
			strcat(buf, name_hungarian_4[i]);
		}
	}

	return 0;
}

static byte MakeSwissTownName(byte *buf, uint32 seed)
{
	strcpy(buf, name_swiss_real[SeedChance(0, lengthof(name_swiss_real), seed)]);
	return 0;
}

TownNameGenerator * const _town_name_generators[] =
{
	MakeEnglishOriginalTownName,
	MakeFrenchTownName,
	MakeGermanTownName,
	MakeEnglishAdditionalTownName,
	MakeSpanishTownName,
	MakeSillyTownName,
	MakeSwedishTownName,
	MakeDutchTownName,
	MakeFinnishTownName,
	MakePolishTownName,
	MakeSlovakTownName,
	MakeNorwegianTownName,
	MakeHungarianTownName,
	MakeAustrianTownName,
	MakeRomanianTownName,
	MakeCzechTownName,
	MakeSwissTownName,
};

// DO WE NEED THIS ANY MORE?
#define FIXNUM(x, y, z) (((((x) << 16) / (y)) + 1) << z)

uint32 GetOldTownName(uint32 townnameparts, byte old_town_name_type)
{
	switch (old_town_name_type) {
		case 0: case 3: /* English, American */
			/*	Already OK */
			return townnameparts;

		case 1: /* French */
			/*	For some reason 86 needs to be subtracted from townnameparts
			*	0000 0000 0000 0000 0000 0000 1111 1111 */
			return FIXNUM(townnameparts - 86, lengthof(name_french_1), 0);

		case 2: /* German */
			DEBUG(misc, 0) ("German Townnames are buggy... (%d)", townnameparts);
			return townnameparts;

		case 4: /* Latin-American */
			/*	0000 0000 0000 0000 0000 0000 1111 1111 */
			return FIXNUM(townnameparts, lengthof(name_spanish_1), 0);

		case 5: /* Silly */
			/*	NUM_SILLY_1	-	lower 16 bits
			*	NUM_SILLY_2	-	upper 16 bits without leading 1 (first 8 bytes)
			*	1000 0000 2222 2222 0000 0000 1111 1111 */
			return FIXNUM(townnameparts, lengthof(name_silly_1), 0) | FIXNUM(((townnameparts >> 16)&0xFF), lengthof(name_silly_2), 16);
	}
	return 0;
}
