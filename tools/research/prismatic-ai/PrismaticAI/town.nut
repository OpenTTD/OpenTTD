/* Towns as the authority sees them, over TOWN_MODEL (town_model.nut, generated from the engine
 * sources by tools/research/town-model/derive.py; research/mechanisms/town-authority.md). No
 * threshold here is a literal: each comes from the model, so an engine change reaches the bot by
 * re-running the generator.
 *
 * An AI sees only the rating's band (AITown.GetRating; GetDetailedRating is for game scripts), so
 * these work from the band's lowest value, which is up to 200 points below the real rating. Two
 * decisions built on that bound lost in replays (.ai knowledge record
 * levelling-an-airport-site-clears-trees-and-costs-town-rating); use these for logs and for
 * decisions that have been A/B-tested. */

/* The band entry of TOWN_MODEL.bands for the rating AITown.GetRating gives, or null. */
function PrismaticAI::TownBand(town) {
	local e = AITown.GetRating(town, AICompany.COMPANY_SELF);
	foreach (b in TOWN_MODEL.bands) if (b.enum == e) return b;
	return null;
}

/* The lowest rating the company can have in `town`: its band's lowest value (RATING_INITIAL when
 * it has no rating there yet). */
function PrismaticAI::TownRatingFloor(town) {
	local b = PrismaticAI.TownBand(town);
	return b == null ? TOWN_MODEL.rating.RATING_INITIAL : b.lowest;
}

/* Whether the authority surely allows a new station after a rating change of `delta` (negative for
 * cleared trees: TOWN_MODEL.rating.RATING_TREE_DOWN_STEP each). True with a permissive council. */
function PrismaticAI::TownSurelyAllowsStation(town, delta) {
	if (AIGameSettings.IsValid("difficulty.town_council_tolerance") &&
			AIGameSettings.GetValue("difficulty.town_council_tolerance") == 3) return true;
	return PrismaticAI.TownRatingFloor(town) + delta > TOWN_MODEL.new_station_above;
}

/* Tree tiles to plant to lift a rating of `rating` above `target`, or -1 when planting cannot
 * (planting stops raising it at RATING_TREE_MAXIMUM). */
function PrismaticAI::TownTreesToGetAbove(rating, target) {
	if (rating > target) return 0;
	if (target >= TOWN_MODEL.rating.RATING_TREE_MAXIMUM) return -1;
	local step = TOWN_MODEL.rating.RATING_TREE_UP_STEP;
	return (target + 1 - rating + step - 1) / step;
}

/* One log line on how `town` sees the company: band, floor and whether a new station is allowed. */
function PrismaticAI::TownReport(town) {
	local b = PrismaticAI.TownBand(town);
	return AITown.GetName(town) + ": rating " + (b == null ? "?" : b.name) + " (at least " +
		PrismaticAI.TownRatingFloor(town) + "), new station " +
		(PrismaticAI.TownSurelyAllowsStation(town, 0) ? "allowed" : "maybe refused") + " (model " + TOWN_MODEL.source_rev + ")";
}
