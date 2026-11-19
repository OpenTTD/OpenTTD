/*
 * Catchment rings (RESEARCH-ONLY; settings catchment_stops, catchment_spacing).
 *
 * Learned from AAAHogEx (station.nut "piece stations"), the LAN battle's winner. Around each of
 * our passenger airports it builds single drive-through road stops on the town's own roads, every
 * `catchment_spacing` tiles within the station spread, and joins them to the airport's station
 * (distant join, on by default). The station then collects the passengers of the whole town, not
 * only those within the airport's small catchment, at the price of a road stop and no vehicle.
 * Each station is ringed once a game year, at most `catchment_stops` stops per station.
 */

/* Whether `t` is a plain straight town road along x (1) or y (2), or 0: no junction, no station,
 * no depot, so that a drive-through stop can stand on it. */
function PrismaticAI::StraightRoadAxis(t) {
	if (!AIRoad.IsRoadTile(t) || AIRoad.IsRoadStationTile(t) || AIRoad.IsRoadDepotTile(t)) return 0;
	if (AITile.GetSlope(t) != AITile.SLOPE_FLAT) return 0;
	local w = AIMap.GetMapSizeX();
	local x = AIRoad.AreRoadTilesConnected(t, t - 1) && AIRoad.AreRoadTilesConnected(t, t + 1);
	local y = AIRoad.AreRoadTilesConnected(t, t - w) && AIRoad.AreRoadTilesConnected(t, t + w);
	local nx = AIRoad.AreRoadTilesConnected(t, t - 1) || AIRoad.AreRoadTilesConnected(t, t + 1);
	local ny = AIRoad.AreRoadTilesConnected(t, t - w) || AIRoad.AreRoadTilesConnected(t, t + w);
	if (x && !ny) return 1;
	if (y && !nx) return 2;
	return 0;
}

/* Ring one station with joined road stops; returns how many were built. */
function PrismaticAI::RingStation(st, b) {
	local spread = AIGameSettings.IsValid("station.station_spread") ? AIGameSettings.GetValue("station.station_spread") : 12;
	local r = max(2, spread / 2 - 1);
	local c = AIStation.GetLocation(st);
	local tiles = AITileList();
	tiles.AddRectangle(AIMap.GetTileIndex(max(1, AIMap.GetTileX(c) - r), max(1, AIMap.GetTileY(c) - r)),
		AIMap.GetTileIndex(min(AIMap.GetMapSizeX() - 2, AIMap.GetTileX(c) + r), min(AIMap.GetMapSizeY() - 2, AIMap.GetTileY(c) + r)));
	tiles.Valuate(AIRoad.IsRoadTile);
	tiles.KeepValue(1);
	tiles.Valuate(AIMap.DistanceManhattan, c);
	tiles.Sort(AIList.SORT_BY_VALUE, AIList.SORT_DESCENDING); /* far first: the most new catchment */
	local placed = [];
	foreach (t, _ in AITileList_StationType(st, AIStation.STATION_BUS_STOP)) placed.push(t);
	local built = 0, w = AIMap.GetMapSizeX();
	AIRoad.SetCurrentRoadType(AIRoad.ROADTYPE_ROAD);
	foreach (t, _ in tiles) {
		if (placed.len() >= b.catchment_stops) break;
		local close = false;
		foreach (p in placed) if (AIMap.DistanceManhattan(p, t) < b.catchment_spacing) { close = true; break; }
		if (close) continue;
		local axis = PrismaticAI.StraightRoadAxis(t);
		if (axis == 0) continue;
		local front = (axis == 1) ? t + 1 : t + w;
		if (AIRoad.BuildDriveThroughRoadStation(t, front, AIRoad.ROADVEHTYPE_BUS, st)) {
			placed.push(t);
			built++;
		}
	}
	if (built > 0) AILog.Info("PrismaticAI: catchment: " + built + " joined stop(s) around station " + AIStation.GetName(st) + " (" + placed.len() + " in its ring)");
	return built;
}

/* Once a game year, ring every passenger airport of ours that has not been ringed this year. */
function PrismaticAI::GrowCatchment(b) {
	if (b.catchment_stops <= 0) return;
	if (AIGameSettings.IsValid("station.distant_join_stations") && AIGameSettings.GetValue("station.distant_join_stations") == 0) return;
	local year = AIDate.GetYear(AIDate.GetCurrentDate());
	if (AICompany.GetBankBalance(AICompany.COMPANY_SELF) < b.cash_reserve + 20000) return;
	foreach (st, _ in AIStationList(AIStation.STATION_AIRPORT)) {
		if (this.ringed.rawin(st) && this.ringed[st] == year) continue;
		this.ringed[st] <- year;
		this.RingStation(st, b);
		return; /* one station per heartbeat: building takes ticks */
	}
}
