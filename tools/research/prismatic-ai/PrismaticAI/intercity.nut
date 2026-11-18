/*
 * Intercity bus routes: the bot picks two large towns a useful distance apart, finds a road path
 * between them (A* over tiles: existing road is cheap, clear land costs more, slopes more still,
 * water and buildings are impassable), builds the missing road tile by tile, logging each tile
 * and pacing the steps so a watching player sees the road grow, then opens a stop in each town and
 * runs buses between them. Longer trips earn more per passenger than a route inside one town.
 */

/* Ticks between two road segments: a road visibly grows instead of appearing at once. */
ROAD_PACE <- 4;

/* Town pairs that failed (no path, or the road could not be finished): never tried again. */
FAILED_PAIRS <- {};

function PrismaticAI::PairKey(a, b) { return a < b ? a * 65536 + b : b * 65536 + a; }

/* The two towns to connect: large, 20 to 80 tiles apart, neither used yet; the best
 * population-to-distance pair among the largest towns. Returns [town_a, town_b] or null. */
function PrismaticAI::PickTownPair(used_towns) {
	local towns = PrismaticAI.TownsByPopulation();
	local cand = [];
	foreach (t, _ in towns) {
		if (cand.len() >= 10) break;
		cand.push(t);
	}
	local best = null;
	local best_score = 0;
	for (local i = 0; i < cand.len(); i++) {
		for (local j = i + 1; j < cand.len(); j++) {
			local a = cand[i], b = cand[j];
			if (used_towns.rawin(a) && used_towns.rawin(b)) continue;
			if (FAILED_PAIRS.rawin(PrismaticAI.PairKey(a, b))) continue;
			local d = AIMap.DistanceManhattan(AITown.GetLocation(a), AITown.GetLocation(b));
			if (d < 20 || d > 80) continue;
			local score = (AITown.GetPopulation(a) + AITown.GetPopulation(b)) * 100 / (d + 10);
			if (score > best_score) {
				best_score = score;
				best = [a, b];
			}
		}
	}
	return best;
}

/* Cost of stepping onto `tile`, or -1 when it cannot carry a road. */
function PrismaticAI::RoadStepCost(tile) {
	if (!AIMap.IsValidTile(tile)) return -1;
	if (AIRoad.IsRoadTile(tile)) return 1;
	if (AITile.IsWaterTile(tile)) return -1;
	if (!AITile.IsBuildable(tile)) return -1;
	/* A road on a slope fails for most slope directions (ERR_LAND_SLOPED_WRONG): go round. */
	if (AITile.GetSlope(tile) != AITile.SLOPE_FLAT) return -1;
	return 3;
}

/* A* from tile `from` to tile `to` over the four neighbours. Returns the path as an array of tiles
 * (from first), or null when none is found within the node budget. */
function PrismaticAI::FindRoadPath(from, to) {
	local map_w = AIMap.GetMapSizeX();
	local open = AIPriorityQueue();
	local came = {};
	local cost = {};
	cost[from] <- 0;
	came[from] <- -1;
	open.Insert(from, AIMap.DistanceManhattan(from, to));
	local budget = 40000;
	while (!open.IsEmpty() && budget-- > 0) {
		local cur = open.Pop();
		if (cur == to) {
			local path = [];
			for (local t = to; t != -1; t = came[t]) path.push(t);
			path.reverse();
			return path;
		}
		foreach (next in [cur - 1, cur + 1, cur - map_w, cur + map_w]) {
			local step = (next == to) ? 1 : PrismaticAI.RoadStepCost(next);
			if (step < 0) continue;
			local c = cost[cur] + step;
			if (!cost.rawin(next) || c < cost[next]) {
				cost[next] <- c;
				came[next] <- cur;
				open.Insert(next, c + AIMap.DistanceManhattan(next, to));
			}
		}
	}
	return null;
}

/* Build the road along `path`, segment by segment, logging every tile that gets new road.
 * Returns the number of tiles built, or -1 when a segment fails. */
function PrismaticAI::BuildRoadAlong(path) {
	AIRoad.SetCurrentRoadType(AIRoad.ROADTYPE_ROAD);
	local built = 0;
	for (local i = 1; i < path.len(); i++) {
		local a = path[i - 1], b = path[i];
		if (AIRoad.AreRoadTilesConnected(a, b)) continue;
		if (!AIRoad.BuildRoad(a, b)) {
			local err = AIError.GetLastError();
			if (err != AIError.ERR_ALREADY_BUILT) {
				AILog.Warning("PrismaticAI: road stopped at tile " + b + ": " + AIError.GetLastErrorString());
				return -1;
			}
		}
		built++;
		AILog.Info("PrismaticAI: built road at tile " + b);
		AIController.Sleep(ROAD_PACE);
	}
	return built;
}

/* One intercity route: road, a stop in each town, a depot, buses between them.
 * Returns [town_a, town_b] on success, null otherwise. */
function PrismaticAI::BuildIntercityRoute(aggressiveness, used_towns) {
	local pair = PrismaticAI.PickTownPair(used_towns);
	if (pair == null) {
		AILog.Warning("PrismaticAI: no pair of towns 20-80 tiles apart left to connect");
		return null;
	}
	local a = pair[0], b = pair[1];
	local ta = AITown.GetLocation(a), tb = AITown.GetLocation(b);
	AILog.Info("PrismaticAI: survey road from " + AITown.GetName(a) + " at tile " + ta + " to " + AITown.GetName(b));
	AIController.Sleep(PACE);
	local path = PrismaticAI.FindRoadPath(ta, tb);
	if (path == null) {
		AILog.Warning("PrismaticAI: no road path from " + AITown.GetName(a) + " to " + AITown.GetName(b));
		FAILED_PAIRS[PrismaticAI.PairKey(a, b)] <- true;
		return null;
	}
	AILog.Info("PrismaticAI: road plan " + AITown.GetName(a) + " - " + AITown.GetName(b) + ": " + path.len() + " tiles");
	local built = PrismaticAI.BuildRoadAlong(path);
	if (built < 0) {
		FAILED_PAIRS[PrismaticAI.PairKey(a, b)] <- true;
		return null;
	}
	AILog.Info("PrismaticAI: road " + AITown.GetName(a) + " - " + AITown.GetName(b) + " done, " + built + " new tiles");

	local sa = PrismaticAI.BuildBusStopNear(ta, 8, "A in " + AITown.GetName(a));
	if (sa == null) { FAILED_PAIRS[PrismaticAI.PairKey(a, b)] <- true; return null; }
	AIController.Sleep(PACE);
	local sb = PrismaticAI.BuildBusStopNear(tb, 8, "B in " + AITown.GetName(b));
	if (sb == null) { FAILED_PAIRS[PrismaticAI.PairKey(a, b)] <- true; return null; }
	AIController.Sleep(PACE);
	if (!PrismaticAI.RoadConnected(sa, sb, 20000)) {
		AILog.Warning("PrismaticAI: stops at tiles " + sa + " and " + sb + " are not joined by road; no buses bought");
		return null;
	}
	local depot = PrismaticAI.BuildDepotNear(sa, 10);
	if (depot == null) depot = PrismaticAI.BuildDepotNear(sb, 10);
	if (depot == null) {
		AILog.Warning("PrismaticAI: could not build a road depot near either stop");
		return null;
	}
	AIController.Sleep(PACE);
	local engine = PrismaticAI.PickPassengerEngine();
	if (engine == -1) return null;
	/* Start small; fleet management adds buses where passengers pile up (needed = waiting / capacity). */
	local count = 1 + aggressiveness / 50; /* 1..3 */
	local started = 0;
	for (local i = 0; i < count; i++) {
		local v = AIVehicle.BuildVehicle(depot, engine);
		if (!AIVehicle.IsValidVehicle(v)) {
			AILog.Warning("PrismaticAI: failed to build vehicle " + i + ": " + AIError.GetLastErrorString());
			continue;
		}
		AIOrder.AppendOrder(v, sa, AIOrder.OF_NONE);
		AIOrder.AppendOrder(v, sb, AIOrder.OF_NONE);
		AIVehicle.StartStopVehicle(v);
		started++;
		AILog.Info("PrismaticAI: started bus " + v + " at tile " + depot);
		AIController.Sleep(PACE * 2);
	}
	AILog.Info("PrismaticAI: built a bus route in " + AITown.GetName(a) + " with " + started + "/" + count +
		" vehicle(s) (intercity to " + AITown.GetName(b) + ")");
	return started > 0 ? pair : null;
}
