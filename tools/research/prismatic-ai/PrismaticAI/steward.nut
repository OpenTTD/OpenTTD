/*
 * The in-bot steward (RESEARCH-ONLY, opt-in: setting self_steward=1, default 0).
 *
 * The bot never decides what a rail line runs: the Rhai steward does
 * (tools/research/mcp-panel/rhai/rail_steward.rhai with rail_lib.rhai) and hands its targets over as
 * the settings line<slot>_trains / line<slot>_wagons (lines.nut). A bot started without the panel
 * has no steward, so every line keeps the one train it was built with, and the money its lines earn
 * piles up unspent. In the LAN AI battle (research/ai-battle/README.md) PrismaticAI solo-max grew to
 * about 20 vehicles by its second year, then fell to 11 while its bank balance rose to millions.
 *
 * With self_steward=1 this file applies the same policy as rail_lib.rhai::target, inside the bot,
 * to every line that has no external target (setting -1). An external target always wins, so a bot
 * the panel steers behaves exactly as before. Rules (the policy of rail_lib.rhai::policy):
 *   - close: the oldest train is judge_after_days old and the line lost money last year and this year
 *   - lengthen: cargo waiting >= grow_loads trainloads and the trains are shorter than the platform
 *   - add a train: as above with full-length trains, while trains < trains_max
 *   - shrink: more than one train, waiting < shrink_loads trainloads, and the newest train is old
 *     enough to judge and lost money last year
 * One change per line per game month, like the Rhai steward's rounds.
 */

const STEWARD_JUDGE_AFTER_DAYS = 400;
const STEWARD_GROW_LOADS = 1.0;
const STEWARD_SHRINK_LOADS = 0.25;
/* Beyond own_lines, a further own line needs this many line budgets in the bank (not on loan). */

/* No more own lines than this however rich the bot is (the planner's own cap is 16 handed lines). */
const STEWARD_OWN_LINES_MAX = 32;

/* What `line`'s trains carry and earn: {trains, capacity, wagons, profit_last, profit_this, oldest,
 * newest, newest_profit_last}. Trains on their way to be sold do not count. */
function PrismaticAI::LineStats(line) {
	local s = { trains = 0, capacity = 0, wagons = 0, profit_last = 0, profit_this = 0, oldest = 0,
		newest = -1, newest_age = 0, newest_profit_last = 0 };
	foreach (t in PrismaticAI.TrainsOf(line)) {
		if (this.selling.rawin(t)) continue;
		s.trains++;
		s.capacity += max(0, AIVehicle.GetCapacity(t, line.cargo));
		s.wagons = max(s.wagons, AIVehicle.GetNumWagons(t) - 1);
		s.profit_last += AIVehicle.GetProfitLastYear(t);
		s.profit_this += AIVehicle.GetProfitThisYear(t);
		local age = AIVehicle.GetAge(t);
		s.oldest = max(s.oldest, age);
		if (s.newest == -1 || age < s.newest_age) {
			s.newest = t;
			s.newest_age = age;
			s.newest_profit_last = AIVehicle.GetProfitLastYear(t);
		}
	}
	return s;
}

/* The most of the line's cargo waiting at either of its stations. */
function PrismaticAI::LineWaiting(line) {
	local w = 0;
	foreach (tile in [line.a, line.b]) {
		local st = AIStation.GetStationID(tile);
		if (AIStation.IsValidStation(st)) w = max(w, AIStation.GetCargoWaiting(st, line.cargo));
	}
	return w;
}

/* The target for `slot` (lines.nut reads this instead of the setting alone): the external steward's
 * setting when it set one, else the in-bot steward's own target when self_steward is on, else -1. */
function PrismaticAI::EffectiveTarget(slot, what) {
	local external = PrismaticAI.LineTarget(slot, what);
	if (external >= 0 || !this.steward_on) return external;
	if (!this.self_targets.rawin(slot)) return -1;
	return this.self_targets[slot][what];
}

/* One round of the in-bot steward: at most one change per line, logged like the Rhai steward's. */
function PrismaticAI::SelfSteward() {
	foreach (i, line in RAIL_LINES) {
		if (line.closed) continue;
		local slot = i + 1;
		if (PrismaticAI.LineTarget(slot, "trains") >= 0 || PrismaticAI.LineTarget(slot, "wagons") >= 0) continue;
		if (!("wagons_fit" in line)) PrismaticAI.LineRecord(slot, line);
		local s = this.LineStats(line);
		if (s.trains == 0) continue;
		local cur = this.self_targets.rawin(slot) ? this.self_targets[slot] : { trains = -1, wagons = -1 };
		local want = { trains = cur.trains, wagons = cur.wagons };
		local why = null;
		if (s.oldest >= STEWARD_JUDGE_AFTER_DAYS && s.profit_last < 0 && s.profit_this < 0) {
			want.trains = 0;
			why = "close: lost " + s.profit_last + " last year and " + s.profit_this + " this year";
		} else if (s.capacity > 0) {
			local waiting = PrismaticAI.LineWaiting(line);
			local loads = waiting.tofloat() / s.capacity.tofloat();
			local fit = ("wagons_fit" in line) ? line.wagons_fit : s.wagons;
			if (loads >= STEWARD_GROW_LOADS && s.wagons < fit) {
				local per_wagon = max(1, s.capacity / max(1, s.trains * s.wagons));
				local more = max(1, min(fit - s.wagons, (waiting + per_wagon - 1) / per_wagon));
				want.wagons = s.wagons + more;
				why = "lengthen: " + waiting + " waiting is " + loads + " trainloads; " + s.wagons + " -> " + want.wagons + " wagons (platform holds " + fit + ")";
			} else if (loads >= STEWARD_GROW_LOADS && s.trains < line.trains_max) {
				want.trains = s.trains + 1;
				why = "add_train: " + waiting + " waiting with full-length trains; " + s.trains + " -> " + want.trains;
			} else if (s.trains > 1 && loads < STEWARD_SHRINK_LOADS && s.newest_age >= STEWARD_JUDGE_AFTER_DAYS && s.newest_profit_last < 0) {
				want.trains = s.trains - 1;
				why = "shrink: " + waiting + " waiting and train " + s.newest + " lost " + s.newest_profit_last;
			}
		}
		if (why == null || (want.trains == cur.trains && want.wagons == cur.wagons)) continue;
		this.self_targets[slot] <- want;
		AILog.Info("PrismaticAI: self-steward line " + slot + " (" + line.label + "): " + why);
	}
}

/* How many own lines the bot may plan: own_lines, or up to STEWARD_OWN_LINES_MAX with self_steward
 * once the bank (not the loan) holds `reinvest_budgets` line budgets. */
function PrismaticAI::OwnLinesCap(b, money) {
	if (b.self_steward == 1 && this.own_built >= b.own_lines && money >= b.reinvest_budgets * b.line_budget) {
		return STEWARD_OWN_LINES_MAX;
	}
	return b.own_lines;
}
