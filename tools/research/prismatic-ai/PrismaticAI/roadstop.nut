/*
 * Road stops and depots that are actually connected to the road.
 *
 * ScriptRoad::BuildRoadStation and BuildRoadDepot (OpenTTD src/script/api/script_road.cpp) take
 * the tile and the tile "exactly in front"; the entrance faces from tile to front
 * (DiagdirBetweenTiles), but neither builds the road piece on the front tile that leads in. A stop
 * or depot built that way can sit next to a road without being reachable. So:
 *
 *  - a stop is first tried as a drive-through stop on a flat, straight piece of existing road
 *    (no junction), its axis along the road: both directions serve it and nothing dead-ends;
 *  - otherwise as a bay (terminal) stop beside the road, then BuildRoad(front, stop);
 *  - a depot is always followed by BuildRoad(front, depot);
 *  - every result is checked with AreRoadTilesConnected, and a piece that did not connect is
 *    demolished and the next site tried.
 */

/* Is `t` a flat road tile that runs straight along one axis, with no junction? Returns the
 * tile in front along that axis, or null. */
function PrismaticAI::StraightRoadFront(t) {
	local w = AIMap.GetMapSizeX();
	if (!AIRoad.IsRoadTile(t) || AIRoad.IsRoadStationTile(t) || AIRoad.IsRoadDepotTile(t)) return null;
	if (AITile.GetSlope(t) != AITile.SLOPE_FLAT) return null;
	local xa = AIRoad.AreRoadTilesConnected(t, t - 1), xb = AIRoad.AreRoadTilesConnected(t, t + 1);
	local ya = AIRoad.AreRoadTilesConnected(t, t - w), yb = AIRoad.AreRoadTilesConnected(t, t + w);
	if (xa && xb && !ya && !yb) return t + 1;
	if (ya && yb && !xa && !xb) return t + w;
	return null;
}

/* An own bus station within `radius` (DistanceMax) of `origin`, and not sharing catchment with `avoid`
 * (7 tiles, DistanceMax) unless that is null:
 * the nearest one's tile, or null. Reusing it keeps one station per place instead of a cluster. */
function PrismaticAI::OwnBusStationNear(origin, radius, avoid) {
	local best = null, best_d = radius + 1;
	local list = AIStationList(AIStation.STATION_BUS_STOP);
	for (local st = list.Begin(); !list.IsEnd(); st = list.Next()) {
		local loc = AIStation.GetLocation(st);
		local d = AIMap.DistanceMax(loc, origin);
		if (d >= best_d) continue;
		if (avoid != null && AIMap.DistanceMax(loc, avoid) < 7) continue;
		best = loc; best_d = d;
	}
	return best;
}

/* Would a new stop at `tile` share catchment with one of our bus stations? A catchment is the square
 * of radius 3 around the stop (DistanceMax, as the game measures it), so two stops need 7 tiles. */
function PrismaticAI::OverlapsOwnBusStation(tile) {
	local list = AIStationList(AIStation.STATION_BUS_STOP);
	for (local st = list.Begin(); !list.IsEnd(); st = list.Next()) {
		if (AIMap.DistanceMax(AIStation.GetLocation(st), tile) < 7) return true;
	}
	return false;
}

/* Is there a road path from tile a to tile b? A bounded breadth-first walk over connected road
 * pieces (stations included), so buses are never bought for stops on two separate networks. */
function PrismaticAI::RoadConnected(a, b, budget) {
	local w = AIMap.GetMapSizeX();
	local seen = {};
	seen[a] <- true;
	local queue = [a];
	local head = 0;
	while (head < queue.len() && budget-- > 0) {
		local t = queue[head++];
		if (t == b) return true;
		foreach (n in [t - 1, t + 1, t - w, t + w]) {
			if (seen.rawin(n) || !AIMap.IsValidTile(n)) continue;
			if (!AIRoad.AreRoadTilesConnected(t, n)) continue;
			seen[n] <- true;
			queue.push(n);
		}
	}
	return false;
}

/* Build a bus stop near `origin`, connected to the road. Logs the survey and the result with the
 * tile, so the director can show the cursor there. Returns the stop tile or null. */
function PrismaticAI::BuildBusStopNear(origin, radius, label) {
	return PrismaticAI.BuildBusStopAway(origin, radius, label, null);
}

/* The same, keeping at least 8 tiles from `avoid` (catchments of radius 3 must not overlap) (the route's other stop) when it is not null.
 * An own bus station within 8 tiles of `origin` is reused first. */
function PrismaticAI::BuildBusStopAway(origin, radius, label, avoid) {
	local reuse = PrismaticAI.OwnBusStationNear(origin, 6, avoid);
	if (reuse != null) {
		AILog.Info("PrismaticAI: reuse stop " + label + " at tile " + reuse + " (" + AIStation.GetName(AIStation.GetStationID(reuse)) + ")");
		return reuse;
	}
	local w = AIMap.GetMapSizeX(), h = AIMap.GetMapSizeY();
	local ox = AIMap.GetTileX(origin), oy = AIMap.GetTileY(origin);

	/* Pass 1: drive-through on a straight road, nearest rings first. */
	for (local r = 0; r <= radius; r++) {
		for (local dy = -r; dy <= r; dy++) {
			for (local dx = -r; dx <= r; dx++) {
				if (abs(dx) != r && abs(dy) != r) continue;
				local x = ox + dx, y = oy + dy;
				if (x < 2 || y < 2 || x >= w - 2 || y >= h - 2) continue;
				local t = AIMap.GetTileIndex(x, y);
				local front = PrismaticAI.StraightRoadFront(t);
				if (front == null) continue;
				if (PrismaticAI.OverlapsOwnBusStation(t)) continue;
				AILog.Info("PrismaticAI: survey stop " + label + " at tile " + t + " (drive-through, along the road)");
				AIController.Sleep(PACE);
				if (AIRoad.BuildDriveThroughRoadStation(t, front, AIRoad.ROADVEHTYPE_BUS, AIStation.STATION_JOIN_ADJACENT)) {
					AILog.Info("PrismaticAI: built stop " + label + " at tile " + t + " (drive-through)");
					return t;
				}
			}
		}
	}

	/* Pass 2: a bay stop beside the road, then the road piece that leads in; at most 12 tries, a town
	 * where every site fails is not worth a game month of attempts. */
	local tries = 0;
	for (local r = 0; r <= radius; r++) {
		for (local dy = -r; dy <= r; dy++) {
			for (local dx = -r; dx <= r; dx++) {
				if (abs(dx) != r && abs(dy) != r) continue;
				local x = ox + dx, y = oy + dy;
				if (x < 2 || y < 2 || x >= w - 2 || y >= h - 2) continue;
				local front = AIMap.GetTileIndex(x, y);
				if (!AIRoad.IsRoadTile(front) || AIRoad.IsRoadStationTile(front) || AIRoad.IsRoadDepotTile(front)) continue;
				foreach (t in [front - 1, front + 1, front - w, front + w]) {
					if (!AIMap.IsValidTile(t) || AIRoad.IsRoadTile(t) || !AITile.IsBuildable(t)) continue;
					if (AITile.GetSlope(t) != AITile.SLOPE_FLAT) continue;
					if (PrismaticAI.OverlapsOwnBusStation(t)) continue;
					if (++tries > 12) { AILog.Warning("PrismaticAI: no stop " + label + " after 12 tries"); return null; }
					AILog.Info("PrismaticAI: survey stop " + label + " at tile " + t + " (bay, facing tile " + front + ")");
					AIController.Sleep(PACE);
					if (!AIRoad.BuildRoadStation(t, front, AIRoad.ROADVEHTYPE_BUS, AIStation.STATION_JOIN_ADJACENT)) {
						local why = AIError.GetLastErrorString();
						AILog.Warning("PrismaticAI: stop at tile " + t + " refused: " + why);
						/* The town will not have us: no other site in it will do better now. */
						if (AIError.GetLastError() == AIError.ERR_LOCAL_AUTHORITY_REFUSES) return null;
						continue;
					}
					AIRoad.BuildRoad(front, t);
					if (AIRoad.AreRoadTilesConnected(front, t)) {
						AILog.Info("PrismaticAI: built stop " + label + " at tile " + t + " (bay, connected)");
						return t;
					}
					AILog.Warning("PrismaticAI: stop at tile " + t + " did not connect; removing it");
					AITile.DemolishTile(t);
				}
			}
		}
	}
	AILog.Warning("PrismaticAI: no connected site for stop " + label);
	return null;
}

/* Build a road depot near `origin` whose entrance is connected to the road. Returns its tile or
 * null. */
function PrismaticAI::BuildDepotNear(origin, radius) {
	/* An own road depot within the radius and on the same road network is reused. */
	local depots = AIDepotList(AITile.TRANSPORT_ROAD);
	depots.Valuate(AIMap.DistanceManhattan, origin);
	depots.KeepBelowValue(radius + 1);
	depots.Sort(AIList.SORT_BY_VALUE, AIList.SORT_ASCENDING);
	for (local d = depots.Begin(); !depots.IsEnd(); d = depots.Next()) {
		if (PrismaticAI.RoadConnected(d, origin, 3000)) {
			AILog.Info("PrismaticAI: reuse depot at tile " + d);
			return d;
		}
	}
	local w = AIMap.GetMapSizeX(), h = AIMap.GetMapSizeY();
	local ox = AIMap.GetTileX(origin), oy = AIMap.GetTileY(origin);
	for (local r = 0; r <= radius; r++) {
		for (local dy = -r; dy <= r; dy++) {
			for (local dx = -r; dx <= r; dx++) {
				if (abs(dx) != r && abs(dy) != r) continue;
				local x = ox + dx, y = oy + dy;
				if (x < 2 || y < 2 || x >= w - 2 || y >= h - 2) continue;
				local front = AIMap.GetTileIndex(x, y);
				if (!AIRoad.IsRoadTile(front) || AIRoad.IsRoadStationTile(front) || AIRoad.IsRoadDepotTile(front)) continue;
				foreach (t in [front - 1, front + 1, front - w, front + w]) {
					if (!AIMap.IsValidTile(t) || AIRoad.IsRoadTile(t) || !AITile.IsBuildable(t)) continue;
					if (AITile.GetSlope(t) != AITile.SLOPE_FLAT) continue;
					if (!AIRoad.BuildRoadDepot(t, front)) continue;
					AIRoad.BuildRoad(front, t);
					if (AIRoad.AreRoadTilesConnected(front, t)) {
						/* Logged here only, where it really happened; a reused depot says "reuse". */
						AILog.Info("PrismaticAI: built depot at tile " + t);
						return t;
					}
					AILog.Warning("PrismaticAI: depot at tile " + t + " did not connect; removing it");
					AITile.DemolishTile(t);
				}
			}
		}
	}
	return null;
}
