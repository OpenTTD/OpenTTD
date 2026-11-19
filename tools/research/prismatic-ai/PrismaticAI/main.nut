/*
 * Phase 2 strategy: no longer a pure idle skeleton. On start, the AI tries
 * to build one real bus route -- two stops within the town with the
 * largest population, a depot, and 1-3 buses (scaled by the externally
 * controlled `aggressiveness` setting) -- then falls back to the same
 * heartbeat loop as before either way. Deliberately scoped to a
 * single-town route: both stops sit on that town's own, already-connected
 * road network, so no intercity pathfinding is needed -- that remains a
 * separate, larger follow-up (see the bridge repo's README roadmap).
 *
 * Every build step is "try, check the actual return value, log and bail
 * cleanly on failure" -- this intentionally never assumes terrain/placement
 * rules match what's expected; OpenTTD's own API return values are the
 * source of truth.
 */

/* Ticks between two visible build steps, so a watching player sees each one happen. */
PACE <- 20;

/* Every rail line the bot built ({a, b, depot, cargo, wagons, flags, label, railtype, restored}),
 * so KeepLinesServed can put a train back on a line that lost its own. */
RAIL_LINES <- [];


class PrismaticAI extends AIController {
	name = null;
	used_towns = null; /* town id -> true for every town that already has one of our routes */
	routes = 0;
	air_done = ""; /* the planner air pair ("a-b") already tried */
	rail_done = ""; /* the planner rail pair ("a-b") already tried */
	freight_tried = null; /* the planner freight lines ("from-to-cargo") already tried, a table */
	own_built = 0; /* freight lines the bot planned itself and tried (own_lines caps them) */
	selling = null; /* vehicle id -> true while it drives to its depot to be sold */
	resizing = null; /* train id -> true while it drives to its depot to change its wagons (lines.nut) */
	month = -1; /* the game month the line records were last logged in */
	idle = null; /* route key -> heartbeats in a row with almost nothing waiting */
	self_targets = null; /* slot -> {trains, wagons}: the in-bot steward's targets (steward.nut) */
	steward_on = false; /* this heartbeat's Behaviour().self_steward == 1 (steward.nut) */
	auto_air = 0; /* autopilot air routes built (autopilot.nut) */
	air_failed = 0; /* autopilot air pairs that could not be built this game year */
	air_year = 0; /* the game year air_failed counts for */
	air_towns = null; /* town id -> true where an autopilot airport stands */
	air_tried = null; /* "a-b" town pairs the autopilot tried for air */

	constructor() {
		this.name = AICompany.GetName(AICompany.COMPANY_SELF);
		this.used_towns = {};
		this.freight_tried = {};
		this.selling = {};
		this.resizing = {};
		this.idle = {};
		this.self_targets = {};
		this.air_towns = {};
		this.air_tried = {};
	}
}

/* Autopilot floors (Behaviour): the profile of the battle's "solo-max" contestant, which reached
 * second place with the in-bot steward (research/ai-battle/README.md). */
const AUTOPILOT_OWN_LINES = 8;
const AUTOPILOT_MAX_ROUTES = 30;
const AUTOPILOT_CASH_RESERVE = 5000;

/* After the class: intercity.nut adds methods to it. */
require("roadstop.nut");
require("intercity.nut");
require("air.nut");
require("rail.nut");
require("fleet.nut");
require("lines.nut");
require("steward.nut");
require("autopilot.nut");

/* Cheapest buildable road engine that carries a passenger-class cargo. */
function PrismaticAI::PickPassengerEngine() {
	local engines = AIEngineList(AIVehicle.VT_ROAD);
	local best = -1;
	local best_price = -1;

	for (local engine = engines.Begin(); !engines.IsEnd(); engine = engines.Next()) {
		if (!AIEngine.IsBuildable(engine)) continue;
		if (!AICargo.HasCargoClass(AIEngine.GetCargoType(engine), AICargo.CC_PASSENGERS)) continue;

		local price = AIEngine.GetPrice(engine);
		if (best == -1 || price < best_price) {
			best = engine;
			best_price = price;
		}
	}
	return best;
}

/* Towns by current population, largest first: the candidates for a starter route. */
function PrismaticAI::TownsByPopulation() {
	local towns = AITownList();
	towns.Valuate(AITown.GetPopulation);
	towns.Sort(AIList.SORT_BY_VALUE, AIList.SORT_DESCENDING);
	return towns;
}

/*
 * Builds one bus route: two stops, a depot, and 1-3 buses depending on
 * `aggressiveness` (0-100), in the largest town where it fits, trying the
 * `max_towns` largest towns in turn. Returns true if at least one vehicle
 * ended up running, false if no town worked (each failure is logged).
 */
function PrismaticAI::BuildStarterRoute(aggressiveness, used_towns) {
	AIRoad.SetCurrentRoadType(AIRoad.ROADTYPE_ROAD);

	local towns = PrismaticAI.TownsByPopulation();
	if (towns.IsEmpty()) {
		AILog.Warning("PrismaticAI: no towns on the map, staying idle");
		return -1;
	}
	local max_towns = 10;
	local tried = 0;
	for (local town = towns.Begin(); !towns.IsEnd() && tried < max_towns; town = towns.Next()) {
		if (used_towns.rawin(town)) continue;
		tried++;
		if (PrismaticAI.BuildRouteInTown(town, aggressiveness)) return town;
	}
	AILog.Warning("PrismaticAI: no new route in the " + tried + " largest unused town(s)");
	return -1;
}

/* One attempt at a bus route in `town`. See BuildStarterRoute. */
function PrismaticAI::BuildRouteInTown(town, aggressiveness) {
	local center = AITown.GetLocation(town);
	local stop_a = PrismaticAI.BuildBusStopNear(center, 15, "A in " + AITown.GetName(town));
	if (stop_a == null) return false;
	AIController.Sleep(PACE);
	/* Stop B in the same town, at least 8 tiles from A, so the two catchments do not overlap (center + 20 used to land in the next town). */
	local stop_b = PrismaticAI.BuildBusStopAway(center, 12, "B in " + AITown.GetName(town), stop_a);
	if (stop_b == null || stop_b == stop_a) {
		AILog.Warning("PrismaticAI: could not find a distinct site for stop B in " + AITown.GetName(town));
		return false;
	}
	if (!PrismaticAI.RoadConnected(stop_a, stop_b, 4000)) {
		AILog.Warning("PrismaticAI: stops at tiles " + stop_a + " and " + stop_b + " are on separate road networks; no buses bought");
		return false;
	}
	AIController.Sleep(PACE);

	local depot = PrismaticAI.BuildDepotNear(stop_a, 10);
	if (depot == null) depot = PrismaticAI.BuildDepotNear(stop_b, 10);
	if (depot == null) {
		AILog.Warning("PrismaticAI: could not build a road depot near either stop");
		return false;
	}
	AIController.Sleep(PACE);

	local engine = PrismaticAI.PickPassengerEngine();
	if (engine == -1) {
		AILog.Warning("PrismaticAI: no buildable passenger road vehicle available");
		return false;
	}

	local vehicle_count = 1 + aggressiveness / 67; /* start small: 1, or 2 when aggressive; fleet management grows it */
	local built = 0;
	for (local i = 0; i < vehicle_count; i++) {
		local vehicle = AIVehicle.BuildVehicle(depot, engine);
		if (!AIVehicle.IsValidVehicle(vehicle)) {
			AILog.Warning("PrismaticAI: failed to build vehicle " + i + ": " + AIError.GetLastErrorString());
			continue;
		}
		AIOrder.AppendOrder(vehicle, stop_a, AIOrder.OF_NONE);
		AIOrder.AppendOrder(vehicle, stop_b, AIOrder.OF_NONE);
		AIVehicle.StartStopVehicle(vehicle);
		built++;
		AILog.Info("PrismaticAI: started bus " + vehicle + " at tile " + depot);
		AIController.Sleep(PACE);
	}

	AILog.Info("PrismaticAI: built a bus route in " + AITown.GetName(town) +
		" with " + built + "/" + vehicle_count + " vehicle(s) (aggressiveness=" + aggressiveness + ")");
	return built > 0;
}

/* Append " (AI)" to the company's name once the game has given it one (not "Unnamed"); names are
 * unique, so a taken one gets a number. Returns true when the name says so. */
function PrismaticAI::DiscloseName() {
	local name = AICompany.GetName(AICompany.COMPANY_SELF);
	if (name == null || name.find("(AI") != null) return true;
	if (name.find("Unnamed") != null) return false;
	foreach (i, suffix in [" (AI)", " (AI 2)", " (AI 3)"]) {
		if (AICompany.SetName(name + suffix)) {
			AILog.Info("PrismaticAI: the company says it is an AI: " + name + suffix);
			return true;
		}
	}
	return false;
}

/* Repay the loan down to what keeps `keep` in the bank, in the game's loan steps. */
function PrismaticAI::RepaySurplus(keep) {
	local loan = AICompany.GetLoanAmount();
	if (loan <= 0) return;
	local surplus = AICompany.GetBankBalance(AICompany.COMPANY_SELF) - keep;
	local step = AICompany.GetLoanInterval();
	local repay = (surplus / step) * step;
	if (repay <= 0) return;
	local target = max(0, loan - repay);
	if (AICompany.SetLoanAmount(target)) {
		AILog.Info("PrismaticAI: repaid " + (loan - target) + " of the loan (now " + target + ")");
	}
}

/* The owner's behaviour knobs, read again at every heartbeat: all are CONFIG_INGAME, so a change made
 * while the AI runs (the settings GUI, or an MCP tool that sets them) takes effect without a restart. */
function PrismaticAI::Behaviour() {
	local b = {
		aggressiveness = AIController.GetSetting("aggressiveness"),
		heartbeat = AIController.GetSetting("heartbeat_seconds"),
		max_routes = AIController.GetSetting("max_routes"),
		cash_reserve = AIController.GetSetting("cash_reserve"),
		intercity = AIController.GetSetting("intercity"),
		air_a = AIController.GetSetting("air_a"),
		air_b = AIController.GetSetting("air_b"),
		air_planes = AIController.GetSetting("air_planes"),
		rail_a = AIController.GetSetting("rail_a"),
		rail_b = AIController.GetSetting("rail_b"),
		rail_wagons = AIController.GetSetting("rail_wagons"),
		rail_len = AIController.GetSetting("rail_len"),
		rail_platforms = AIController.GetSetting("rail_platforms"),
		rail_after = AIController.GetSetting("rail_after"),
		fr_from = AIController.GetSetting("fr_from"),
		fr_to = AIController.GetSetting("fr_to"),
		fr_cargo = AIController.GetSetting("fr_cargo"),
		freight = PrismaticAI.FreightLines(),
		line_budget = AIController.GetSetting("line_budget"),
		own_lines = AIController.GetSetting("own_lines"),
		self_steward = AIController.GetSetting("self_steward"),
		autopilot = AIController.GetSetting("autopilot"),
	};
	/* Autopilot (default on): a bot nobody gave a plan (no planner line, air or rail pair) grows on
	 * its own instead of stopping at a few buses. In the LAN AI battle the unplanned default ended
	 * last in every match (research/ai-battle/README.md). It runs the in-bot steward, plans its own
	 * freight lines, connects towns and keeps a small reserve. A plan from the panel switches it off,
	 * and autopilot=0 restores the unplanned bot that duels used as their rival before 2026-11-19. */
	b.planned <- b.freight.len() > 0 || (b.air_a >= 0 && b.air_b >= 0) || (b.rail_a >= 0 && b.rail_b >= 0);
	b.autopiloted <- b.autopilot == 1 && !b.planned;
	if (b.autopiloted) {
		b.self_steward = 1;
		b.own_lines = max(b.own_lines, AUTOPILOT_OWN_LINES);
		b.max_routes = max(b.max_routes, AUTOPILOT_MAX_ROUTES);
		b.intercity = 1;
		b.cash_reserve = min(b.cash_reserve, AUTOPILOT_CASH_RESERVE);
	}
	return b;
}

/* The planner's freight rail lines in its order: fr_* then fr2_* to fr16_* (as info.nut declares
 * them), those set. */
function PrismaticAI::FreightLines() {
	local out = [];
	for (local i = 1; i <= 16; i++) {
		local p = (i == 1) ? "fr_" : ("fr" + i + "_");
		local line = {from = AIController.GetSetting(p + "from"), to = AIController.GetSetting(p + "to"), cargo = AIController.GetSetting(p + "cargo")};
		if (line.from >= 0 && line.to >= 0 && line.cargo >= 0) out.push(line);
	}
	return out;
}

function PrismaticAI::Start() {
	local b = PrismaticAI.Behaviour();
	AILog.Info("PrismaticAI starting for " + this.name + " (aggressiveness=" + b.aggressiveness +
		", max_routes=" + b.max_routes + ", cash_reserve=" + b.cash_reserve + ", heartbeat_seconds=" + b.heartbeat +
		", air_a=" + b.air_a + ", air_b=" + b.air_b + ", air_planes=" + b.air_planes +
		", rail_a=" + b.rail_a + ", rail_b=" + b.rail_b + ", rail_wagons=" + b.rail_wagons +
		", rail_len=" + b.rail_len + ", rail_platforms=" + b.rail_platforms + ")");

	this.Sleep(10); /* let the world/company finish settling before touching it */
	/* Old vehicles are renewed by the game itself, six months before their maximum age, when the
	 * company has 30000 to spare: in a 21-year duel the bot's trains fell from five to three. */
	AICompany.SetAutoRenewStatus(true);
	AICompany.SetAutoRenewMonths(-6);
	AICompany.SetAutoRenewMoney(30000);

	local last = "";
	local disclosed = false;
	while (true) {
		b = PrismaticAI.Behaviour();
		/* A real game against people: the company says it is an AI (research/real-game.md). The game
		 * names a company after its first station's town, so this waits for that name. */
		if (!disclosed && AIController.GetSetting("disclose_name") == 1) disclosed = PrismaticAI.DiscloseName();
		this.steward_on = b.self_steward == 1;
		local knobs = "aggressiveness=" + b.aggressiveness + " max_routes=" + b.max_routes + " cash_reserve=" + b.cash_reserve +
			" intercity=" + b.intercity + " self_steward=" + b.self_steward + " own_lines=" + b.own_lines +
			(b.autopiloted ? " autopilot=engaged" : "");
		if (knobs != last) {
			AILog.Info("PrismaticAI behaviour: " + knobs);
			last = knobs;
		}

		local money = AICompany.GetBankBalance(AICompany.COMPANY_SELF);
		/* The planner's air route first: by the game's payment formula the long route between big
		 * towns earns the most per passenger. Tried once per pair the owner gives. */
		local air_key = b.air_a + "-" + b.air_b;
		if (b.air_a >= 0 && b.air_b >= 0 && this.air_done != air_key && money > b.cash_reserve) {
			this.air_done = air_key;
			if (PrismaticAI.BuildAirRoute(b.air_a, b.air_b, b.air_planes) > 0) this.routes++;
			money = AICompany.GetBankBalance(AICompany.COMPANY_SELF);
		}
		/* Without a plan, the autopilot's own air routes come first: the gap to the winner opens in the
		 * first quarters, and a plane between two big towns earns the most per vehicle (autopilot.nut). */
		if (this.AutopilotAir(b)) money = AICompany.GetBankBalance(AICompany.COMPANY_SELF);
		/* The portfolio's freight first: by the game's payment rates, coal and wood by rail pay most.
		 * The planner's first line is built at once; each next one (fr2_* to fr16_*) once the bank
		 * balance plus the loan headroom reach line_budget, one line per heartbeat, each tried once. */
		local first = true;
		foreach (line in b.freight) {
			local fr_key = line.from + "-" + line.to + "-" + line.cargo;
			if (this.freight_tried.rawin(fr_key)) { first = false; continue; }
			local headroom = AICompany.GetMaxLoanAmount() - AICompany.GetLoanAmount();
			if (!first && money + headroom < b.line_budget) break;
			this.freight_tried[fr_key] <- true;
			local loan_before = AICompany.GetLoanAmount();
			AICompany.SetLoanAmount(AICompany.GetMaxLoanAmount());
			if (!first) AILog.Info("PrismaticAI: money " + money + " and loan headroom " + headroom + " reach " + b.line_budget + ": the planner's next freight line");
			if (PrismaticAI.BuildFreightRailRoute(line.from, line.to, line.cargo, PrismaticAI.RailShape(b)) > 0) {
				this.routes++;
			} else {
				AICompany.SetLoanAmount(loan_before);
				AILog.Info("PrismaticAI: no freight route built; loan back to " + AICompany.GetLoanAmount());
			}
			money = AICompany.GetBankBalance(AICompany.COMPANY_SELF);
			break;
		}
		local rail_key = b.rail_a + "-" + b.rail_b;
		/* The passenger pair waits for the freight lines that score more (rail_after, from the panel):
		 * each line is built on borrowed money, so the best first. */
		local tried = 0;
		foreach (line in b.freight) if (this.freight_tried.rawin(line.from + "-" + line.to + "-" + line.cargo)) tried++;
		local pair_due = tried >= min(b.rail_after, b.freight.len());
		if (b.rail_a >= 0 && b.rail_b >= 0 && this.rail_done != rail_key && pair_due) {
			this.rail_done = rail_key;
			/* A line pays back over years: borrow up to the limit to build it, as the planner's budget
			 * assumes; when nothing gets built, give the money back so it does not sit there at interest. */
			local loan_before = AICompany.GetLoanAmount();
			AICompany.SetLoanAmount(AICompany.GetMaxLoanAmount());
			if (PrismaticAI.BuildRailRoute(b.rail_a, b.rail_b, PrismaticAI.RailShape(b)) > 0) {
				this.routes++;
			} else {
				AICompany.SetLoanAmount(loan_before);
				AILog.Info("PrismaticAI: no rail route built; loan back to " + AICompany.GetLoanAmount());
			}
			money = AICompany.GetBankBalance(AICompany.COMPANY_SELF);
		}
		/* The bot's own plan: once every freight line the planner handed over was tried and its
		 * passenger pair is done, money no longer waits for a new plan from outside. With the script
		 * budget it was meant to have, the bot built all fourteen planned lines by month 24 on OpenTTD
		 * seed 31337 and then held 529983 idle at month 36 (research/results duel-20261117T112500).
		 * One line per heartbeat, when money plus loan headroom reach line_budget, at most own_lines. */
		local handed_done = true;
		foreach (line in b.freight) if (!this.freight_tried.rawin(line.from + "-" + line.to + "-" + line.cargo)) handed_done = false;
		local pair_open = b.rail_a >= 0 && b.rail_b >= 0 && this.rail_done != rail_key;
		local headroom_own = AICompany.GetMaxLoanAmount() - AICompany.GetLoanAmount();
		local own_cap = this.OwnLinesCap(b, money);
		if (handed_done && !pair_open && this.own_built < own_cap && money + headroom_own >= b.line_budget) {
			local own = PrismaticAI.NextOwnFreightLine(this.freight_tried);
			if (own != null) {
				this.freight_tried[own.from + "-" + own.to + "-" + own.cargo] <- true;
				this.own_built++;
				AILog.Info("PrismaticAI: own plan " + this.own_built + "/" + own_cap + ": " + AICargo.GetCargoLabel(own.cargo) + " " +
					AIIndustry.GetName(own.from) + " - " + AIIndustry.GetName(own.to) + ", " + own.distance + " tiles, " +
					own.production + " a month, score " + own.score);
				local loan_before = AICompany.GetLoanAmount();
				AICompany.SetLoanAmount(AICompany.GetMaxLoanAmount());
				if (PrismaticAI.BuildFreightRailRoute(own.from, own.to, own.cargo, PrismaticAI.RailShape(b)) > 0) {
					this.routes++;
				} else {
					AICompany.SetLoanAmount(loan_before);
					AILog.Info("PrismaticAI: no freight route built; loan back to " + AICompany.GetLoanAmount());
				}
				money = AICompany.GetBankBalance(AICompany.COMPANY_SELF);
			}
		}
		if (this.routes < b.max_routes && money > b.cash_reserve) {
			local pair = (b.intercity == 1) ? PrismaticAI.BuildIntercityRoute(b.aggressiveness, this.used_towns) : null;
			if (pair != null) {
				this.used_towns[pair[0]] <- true;
				this.used_towns[pair[1]] <- true;
				this.routes++;
				AILog.Info("PrismaticAI: route " + this.routes + "/" + b.max_routes + " in " + AITown.GetName(pair[0]));
			} else {
				local town = PrismaticAI.BuildStarterRoute(b.aggressiveness, this.used_towns);
				if (town != -1) {
					this.used_towns[town] <- true;
					this.routes++;
					AILog.Info("PrismaticAI: route " + this.routes + "/" + b.max_routes + " in " + AITown.GetName(town));
				}
			}
		}

		/* Capacity follows demand: add where cargo piles up, sell what keeps losing money. */
		this.ManageFleet(b.cash_reserve);
		/* Every rail line keeps a train: one lost at a level crossing or sold is replaced. */
		PrismaticAI.KeepLinesServed(b.cash_reserve);
		/* Each line runs what the steward's targets say (lines.nut), and its record is logged again
		 * once a game month so a steward started late still finds it. */
		/* With self_steward, the bot sets its own line targets once a game month (steward.nut). */
		if (b.self_steward == 1 && PrismaticAI.MonthKey() != this.month) this.SelfSteward();
		this.ReconcileLines(b.cash_reserve);
		if (PrismaticAI.MonthKey() != this.month) { this.month = PrismaticAI.MonthKey(); PrismaticAI.LogLines(); }
		/* Borrowed money that sits idle only costs interest: repay what is above twice the reserve. */
		PrismaticAI.RepaySurplus(2 * b.cash_reserve);

		AILog.Info("PrismaticAI heartbeat: date=" + AIDate.GetCurrentDate() +
			" money=" + AICompany.GetBankBalance(AICompany.COMPANY_SELF) +
			" vehicles=" + AIVehicleList().Count() + " routes=" + this.routes + "/" + b.max_routes +
			" " + knobs);

		local ticks = b.heartbeat * 74 / 30; // ~heartbeat_seconds of ticks (74 ticks/sec baseline)
		this.Sleep(ticks > 0 ? ticks : 1);
	}
}
