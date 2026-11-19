/*
 * Autopilot's own air routes (RESEARCH-ONLY; Behaviour().autopiloted: autopilot=1 and no plan).
 *
 * In the LAN AI battle the gap to the winner opened in the first two quarters
 * (research/ai-battle/README.md "Where we lose"). AAAHogEx ran six aircraft by July 1960 and earned
 * about 100k a quarter, while PrismaticAI held about 210k in cash and earned under 2k. The game
 * pays passengers by distance and time, so a plane between two big towns earns the most per
 * vehicle at the start. The planner hands such a pair over as air_a/air_b. Without a plan, the
 * autopilot picks its own pairs here and builds them first, on borrowed money, with the same
 * BuildAirRoute (air.nut). fleet.nut then adds aircraft where passengers wait, up to its cap.
 *
 * Pair choice: among the AUTOPILOT_AIR_TOWNS most populous towns, the pair with the highest
 * min(population) x distance, distance between AUTOPILOT_AIR_MIN_TILES and AUTOPILOT_AIR_MAX_TILES,
 * where neither town has an airport of ours yet, and the pair was never tried.
 */

const AUTOPILOT_AIR_ROUTES = 10;
const AUTOPILOT_AIR_TOWNS = 25;
/* A town with this many airports of other companies within AUTOPILOT_AIR_NEAR tiles is skipped:
 * its authority refuses the next one, and the battle's winner takes the biggest towns first. */
const AUTOPILOT_FOREIGN_AIRPORTS = 1;
const AUTOPILOT_AIR_NEAR = 16;
const AUTOPILOT_AIR_MIN_TILES = 60;
const AUTOPILOT_AIR_MAX_TILES = 260;
/* Money plus loan headroom an air route needs before it is tried (two airports and two planes). */
const AUTOPILOT_AIR_BUDGET = 60000;
/* After this many pairs that could not be built (no airport site, no money), stop trying. */
const AUTOPILOT_AIR_MAX_FAILS = 6;

/* Airport tiles of other companies within AUTOPILOT_AIR_NEAR tiles of the town centre, counted as
 * distinct stations. */
function PrismaticAI::ForeignAirportsNear(town) {
	local c = AITown.GetLocation(town), r = AUTOPILOT_AIR_NEAR;
	local tiles = AITileList();
	tiles.AddRectangle(AIMap.GetTileIndex(max(1, AIMap.GetTileX(c) - r), max(1, AIMap.GetTileY(c) - r)),
		AIMap.GetTileIndex(min(AIMap.GetMapSizeX() - 2, AIMap.GetTileX(c) + r), min(AIMap.GetMapSizeY() - 2, AIMap.GetTileY(c) + r)));
	tiles.Valuate(AIAirport.IsAirportTile);
	tiles.KeepValue(1);
	local stations = {};
	foreach (t, _ in tiles) {
		if (AITile.GetOwner(t) == AICompany.ResolveCompanyID(AICompany.COMPANY_SELF)) continue;
		stations[AIStation.GetStationID(t)] <- true;
	}
	return stations.len();
}

/* The next town pair for an autopilot air route, or null. */
function PrismaticAI::NextAutopilotAirPair() {
	local towns = AITownList();
	towns.Valuate(AITown.GetPopulation);
	towns.Sort(AIList.SORT_BY_VALUE, AIList.SORT_DESCENDING);
	towns.KeepTop(AUTOPILOT_AIR_TOWNS);
	local list = [];
	foreach (t, pop in towns) {
		if (AIR_BAD_TOWNS.rawin(t)) continue;
		if (!this.air_towns.rawin(t) && PrismaticAI.ForeignAirportsNear(t) >= AUTOPILOT_FOREIGN_AIRPORTS) {
			AIR_BAD_TOWNS[t] <- "foreign airport";
			continue;
		}
		list.push([t, pop]);
	}
	local best = null, best_score = 0;
	for (local i = 0; i < list.len(); i++) {
		if (this.air_towns.rawin(list[i][0])) continue;
		for (local j = i + 1; j < list.len(); j++) {
			if (this.air_towns.rawin(list[j][0])) continue;
			local key = list[i][0] + "-" + list[j][0];
			if (this.air_tried.rawin(key)) continue;
			local dist = AIMap.DistanceManhattan(AITown.GetLocation(list[i][0]), AITown.GetLocation(list[j][0]));
			if (dist < AUTOPILOT_AIR_MIN_TILES || dist > AUTOPILOT_AIR_MAX_TILES) continue;
			local score = min(list[i][1], list[j][1]) * dist;
			if (score > best_score) { best_score = score; best = [list[i][0], list[j][0], key, dist]; }
		}
	}
	return best;
}

/* One autopilot air route per call, while fewer than AUTOPILOT_AIR_ROUTES exist and the money is
 * there. Returns true when it built one. */
function PrismaticAI::AutopilotAir(b) {
	/* Failed pairs are forgiven once a game year: a richer company affords a site it could not before. */
	local year = AIDate.GetYear(AIDate.GetCurrentDate());
	if (year != this.air_year) { this.air_year = year; this.air_failed = 0; }
	if (!b.autopiloted || this.auto_air >= AUTOPILOT_AIR_ROUTES || this.air_failed >= AUTOPILOT_AIR_MAX_FAILS) return false;
	local money = AICompany.GetBankBalance(AICompany.COMPANY_SELF);
	local headroom = AICompany.GetMaxLoanAmount() - AICompany.GetLoanAmount();
	/* The real price of the route: two airports, the planes, and a margin for levelling. Without
	 * this check a route once spent everything on airports and bought no plane. */
	local type = PrismaticAI.AirportType(), pax = PrismaticAI.PassengerCargo();
	if (type == -1 || pax == -1) return false;
	local plane = PrismaticAI.PickPlane(type, pax);
	if (plane == -1) return false;
	local need = max(AUTOPILOT_AIR_BUDGET, 2 * AIAirport.GetPrice(type) + b.air_planes * AIEngine.GetPrice(plane) +
		2 * LEVEL_MAX_COST + b.cash_reserve);
	if (money + headroom < need) return false;
	local pair = this.NextAutopilotAirPair();
	if (pair == null) return false;
	this.air_tried[pair[2]] <- true;
	AILog.Info("PrismaticAI: autopilot air " + (this.auto_air + 1) + "/" + AUTOPILOT_AIR_ROUTES + ": " +
		AITown.GetName(pair[0]) + " - " + AITown.GetName(pair[1]) + ", " + pair[3] + " tiles");
	local loan_before = AICompany.GetLoanAmount();
	AICompany.SetLoanAmount(AICompany.GetMaxLoanAmount());
	if (PrismaticAI.BuildAirRoute(pair[0], pair[1], b.air_planes) > 0) {
		this.air_towns[pair[0]] <- true;
		this.air_towns[pair[1]] <- true;
		this.auto_air++;
		this.routes++;
		return true;
	}
	AICompany.SetLoanAmount(loan_before);
	this.air_failed++;
	AILog.Info("PrismaticAI: autopilot air route not built (" + this.air_failed + " failed); loan back to " + AICompany.GetLoanAmount());
	return false;
}
