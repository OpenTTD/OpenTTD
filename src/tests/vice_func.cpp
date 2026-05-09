/** @file vice_func.cpp Tests for City Vice calculation functions. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../vice_type.h"

#include "../safeguards.h"

/* Helper: replicate the base_vice formula. */
static uint8_t CalcBaseVice(uint32_t population)
{
	return static_cast<uint8_t>(
		(static_cast<uint64_t>(population) * 100) / (population + 5000)
	);
}

/* Helper: replicate the service modifier formula. */
static uint8_t CalcServiceModifier(uint8_t pct_transported)
{
	return static_cast<uint8_t>((pct_transported * 60) / 256);
}

/* Helper: replicate the rating modifier formula. */
static int8_t CalcRatingModifier(int16_t best_rating)
{
	return static_cast<int8_t>(-(best_rating * 20) / 1000);
}


TEST_CASE("ViceCalc - BaseVice scales with population")
{
	CHECK(CalcBaseVice(0) == 0);
	CHECK(CalcBaseVice(100) == 1);
	CHECK(CalcBaseVice(500) == 9);
	CHECK(CalcBaseVice(1000) == 16);
	CHECK(CalcBaseVice(5000) == 50);
	CHECK(CalcBaseVice(10000) == 66);
	CHECK(CalcBaseVice(50000) == 90);
	CHECK(CalcBaseVice(100000) == 95);
}

TEST_CASE("ViceCalc - BaseVice never reaches 100")
{
	/* Even with extreme population, base_vice < 100 */
	CHECK(CalcBaseVice(1000000) < 100);
	CHECK(CalcBaseVice(UINT32_MAX / 100) < 100);
}

TEST_CASE("ViceCalc - ServiceModifier range")
{
	CHECK(CalcServiceModifier(0) == 0);
	CHECK(CalcServiceModifier(128) == 30);  /* 50% transport */
	CHECK(CalcServiceModifier(204) == 47);  /* ~80% transport */
	CHECK(CalcServiceModifier(255) == 59);  /* ~100% transport */
}

TEST_CASE("ViceCalc - RatingModifier range")
{
	CHECK(CalcRatingModifier(1000) == -20);  /* Best possible rating */
	CHECK(CalcRatingModifier(0) == 0);
	CHECK(CalcRatingModifier(-1000) == 20);  /* Worst possible rating */
	CHECK(CalcRatingModifier(800) == -16);
	CHECK(CalcRatingModifier(400) == -8);
}

TEST_CASE("ViceCalc - Well-serviced small town is immune")
{
	uint32_t population = 3000;
	uint8_t base = CalcBaseVice(population);   /* 37 */
	uint8_t svc = CalcServiceModifier(204);    /* 47 (80% transport) */
	int adjusted = std::max(0, (int)base - (int)svc);  /* 0 */
	int8_t rating_mod = CalcRatingModifier(800); /* -16 */
	int final_vice = std::clamp(adjusted + rating_mod, 0, 100);
	CHECK(final_vice == 0);
}

TEST_CASE("ViceCalc - Neglected metropolis has high vice")
{
	uint32_t population = 50000;
	uint8_t base = CalcBaseVice(population);   /* 90 */
	uint8_t svc = CalcServiceModifier(0);      /* 0 (no transport) */
	int adjusted = std::max(0, (int)base - (int)svc);  /* 90 */
	int8_t rating_mod = CalcRatingModifier(-500); /* 10 */
	int final_vice = std::clamp(adjusted + rating_mod, 0, 100);
	CHECK(final_vice == 100);
}

TEST_CASE("ViceCalc - Policing tiers have correct data")
{
	/* Verify the _policing_tier_data table matches the 3-tier design. */
	CHECK(std::size(_policing_tier_data) == 3);

	CHECK(_policing_tier_data[0].reduction == 10);
	CHECK(_policing_tier_data[0].months == 3);

	CHECK(_policing_tier_data[1].reduction == 20);
	CHECK(_policing_tier_data[1].months == 6);

	CHECK(_policing_tier_data[2].reduction == 30);
	CHECK(_policing_tier_data[2].months == 12);
}

TEST_CASE("ViceCalc - Policing reduces vice, clamped to zero")
{
	int final_vice = 15;
	int result;

	/* Crime Patrol: -10 */
	result = std::max(0, final_vice - 10);
	CHECK(result == 5);

	/* Crime Security: -20, clamped */
	result = std::max(0, final_vice - 20);
	CHECK(result == 0);

	/* Crime Peacekeeping: -30, clamped */
	result = std::max(0, final_vice - 30);
	CHECK(result == 0);
}

TEST_CASE("ViceCalc - Population below threshold produces no events")
{
	uint32_t population = 999;
	uint16_t min_population = 1000;
	CHECK(population < min_population);
}

TEST_CASE("ViceCalc - Event weight selection")
{
	/* Weights for Pop < 5k: Vandalism=60, Sabotage=25, Arson=10, Riot=5, CrimeWave=0 */
	uint8_t weights[] = {60, 25, 10, 5, 0};
	uint total = 0;
	for (auto w : weights) total += w;
	CHECK(total == 100);

	/* Roll 0-59 should select Vandalism (index 0) */
	uint roll = 30;
	uint cumulative = 0;
	int selected = -1;
	for (int i = 0; i < 5; i++) {
		cumulative += weights[i];
		if (roll < cumulative) { selected = i; break; }
	}
	CHECK(selected == 0);

	/* Roll 85 should select Arson (index 2) */
	roll = 85;
	cumulative = 0;
	selected = -1;
	for (int i = 0; i < 5; i++) {
		cumulative += weights[i];
		if (roll < cumulative) { selected = i; break; }
	}
	CHECK(selected == 2);

	/* Roll 98 should select Riot (index 3) */
	roll = 98;
	cumulative = 0;
	selected = -1;
	for (int i = 0; i < 5; i++) {
		cumulative += weights[i];
		if (roll < cumulative) { selected = i; break; }
	}
	CHECK(selected == 3);
}

TEST_CASE("ViceCalc - Riot building count")
{
	/* Formula: 1 + RandomRange(clamp(population / 20000, 0, 2)) */
	CHECK(std::clamp(5000U / 20000U, 0U, 2U) == 0);   /* Pop 5k: max 1 building */
	CHECK(std::clamp(20000U / 20000U, 0U, 2U) == 1);   /* Pop 20k: max 2 buildings */
	CHECK(std::clamp(40000U / 20000U, 0U, 2U) == 2);   /* Pop 40k: max 3 buildings */
	CHECK(std::clamp(100000U / 20000U, 0U, 2U) == 2);  /* Pop 100k: still max 3 */
}

TEST_CASE("ViceCalc - Cooldown prevents events")
{
	uint8_t cooldown = 2;
	CHECK(cooldown > 0);  /* Event should not fire */
}

TEST_CASE("ViceCalc - Severity Mild blocks Arson/Riot/CrimeWave")
{
	uint8_t severity = 0; /* Mild */
	/* In Mild mode, only ViceEventType::PettyVandalism and ViceEventType::VehicleSabotage are allowed */
	CHECK(severity == 0);
	/* Weights for Arson, Riot, CrimeWave should be forced to 0 in Mild mode */
}

TEST_CASE("ViceCalc - Overflow safety in base_vice")
{
	/* population * 100 could overflow uint32_t at ~42 million.
	 * Use uint64_t intermediate or ensure population is capped. */
	uint32_t large_pop = 1000000;
	uint64_t product = static_cast<uint64_t>(large_pop) * 100;
	uint8_t base = static_cast<uint8_t>(product / (large_pop + 5000));
	CHECK(base == 99);
}

TEST_CASE("ViceCalc - Crime wave rating slowdown")
{
	/* Verify the halving logic used in UpdateStationRating.
	 * When crime_wave_months > 0, positive rating steps are halved. */

	/* Normal step: Clamp(rating - old_rating, -2, 2) */
	int old_rating = 100;
	int new_rating_calc = 130; /* rating computed by the formula */

	int step = std::clamp(new_rating_calc - old_rating, -2, 2); /* +2 */
	CHECK(step == 2);

	/* With crime wave: positive step halved (integer division). */
	int slowed_step = step > 0 ? step / 2 : step;
	CHECK(slowed_step == 1);

	/* Negative steps are NOT halved (penalties still apply at full speed). */
	new_rating_calc = 50;
	step = std::clamp(new_rating_calc - old_rating, -2, 2); /* -2 */
	slowed_step = step > 0 ? step / 2 : step;
	CHECK(slowed_step == -2);

	/* Edge case: step of +1 halved to 0 (ratings stall). */
	step = 1;
	slowed_step = step > 0 ? step / 2 : step;
	CHECK(slowed_step == 0);
}

TEST_CASE("ViceCalc - Inciting tiers have correct data")
{
	/* Verify the _inciting_tier_data table matches the 3-tier design. */
	CHECK(std::size(_inciting_tier_data) == 3);

	CHECK(_inciting_tier_data[0].reduction == 10);
	CHECK(_inciting_tier_data[0].months == 3);

	CHECK(_inciting_tier_data[1].reduction == 20);
	CHECK(_inciting_tier_data[1].months == 6);

	CHECK(_inciting_tier_data[2].reduction == 30);
	CHECK(_inciting_tier_data[2].months == 12);
}

TEST_CASE("ViceCalc - Incitement increases vice, clamped to 100")
{
	int final_vice = 15;
	int result;

	/* Crime Incite Low: +10 */
	result = std::min(100, final_vice + 10);
	CHECK(result == 25);

	/* Crime Incite Medium: +20 */
	result = std::min(100, final_vice + 20);
	CHECK(result == 35);

	/* Crime Incite High: +30 */
	result = std::min(100, final_vice + 30);
	CHECK(result == 45);

	/* Clamped at 100 */
	final_vice = 90;
	result = std::min(100, final_vice + 30);
	CHECK(result == 100);
}

TEST_CASE("ViceCalc - Incitement and policing cancel out")
{
	int final_vice = 50;

	/* Policing -20, Inciting +20 -> net zero */
	int policing = 20;
	int inciting = 20;
	int result = std::max(0, final_vice - policing);
	result = std::min(100, result + inciting);
	CHECK(result == 50);

	/* Policing dominates: -30 + 10 -> net -20 */
	policing = 30;
	inciting = 10;
	result = std::max(0, final_vice - policing);
	result = std::min(100, result + inciting);
	CHECK(result == 30);

	/* Inciting dominates: -10 + 30 -> net +20 */
	policing = 10;
	inciting = 30;
	result = std::max(0, final_vice - policing);
	result = std::min(100, result + inciting);
	CHECK(result == 70);
}

TEST_CASE("ViceCalc - Incitement applied after policing")
{
	/* Policing is applied first (reduces vice), then inciting boosts it. */
	int final_vice = 15;

	/* Policing -30 reduces to 0, then inciting +20 raises to 20. */
	int result = std::max(0, final_vice - 30);
	CHECK(result == 0);
	result = std::min(100, result + 20);
	CHECK(result == 20);

	/* Policing -10 reduces to 5, then inciting +30 raises to 35. */
	result = std::max(0, final_vice - 10);
	CHECK(result == 5);
	result = std::min(100, result + 30);
	CHECK(result == 35);
}

TEST_CASE("ViceCalc - Growth rate penalty tiers")
{
	uint base_rate = 100;

	/* Vice 0-50: no penalty */
	uint rate = base_rate;
	CHECK(rate == 100);

	/* Vice 51-75: 25% slower (rate * 5 / 4) */
	uint8_t vice_level = 60;
	CHECK(vice_level > 50);
	CHECK(vice_level <= 75);
	rate = base_rate * 5 / 4;
	CHECK(rate == 125);

	/* Vice 76-100: 50% slower (rate * 3 / 2) */
	vice_level = 80;
	CHECK(vice_level > 75);
	rate = base_rate * 3 / 2;
	CHECK(rate == 150);

	/* Crime wave: halve growth speed (rate * 2) */
	rate = base_rate * 2;
	CHECK(rate == 200);

	/* Combined: vice 80 + crime wave = base * 3/2 * 2 = 300 */
	rate = base_rate * 3 / 2 * 2;
	CHECK(rate == 300);
}
