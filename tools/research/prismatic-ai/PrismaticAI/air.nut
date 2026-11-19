/*
 * An air route between two towns: the pair comes from the owner's planner (settings air_a, air_b,
 * set from tools/research/mcp-rhai mcp-plan by_mode.air when the panel starts the bot), so there is
 * one profitability model, not two.
 *
 * - An airport site is chosen per town by the passengers it would actually accept: every buildable
 *   rectangle of the airport's size near the centre is scored with AITile.GetCargoAcceptance over
 *   the airport's catchment radius, best first; the town authority or the land may refuse a site, so
 *   the next one is tried.
 * - The airport type is the biggest the game offers now, from small to large (AT_SMALL first in
 *   1950); a big plane is never sent to a small airport (it would crash: AIEngine.GetPlaneType).
 * - Aircraft: the buildable passenger plane with the best capacity x speed that the airports take.
 * Every step is logged with its tile, so the director moves the camera and the cursor there.
 */

/* Towns where an airport of ours cannot go (the authority refuses, or the town has too many
 * stations and none of ours is adjacent): town id -> the error. The autopilot skips them. */
AIR_BAD_TOWNS <- {};
/* Our airport in a town: town id -> its tile. A new route from that town flies from it (air_reuse). */
AIR_PORTS <- {};

/* Our airport tile in `town` that is still ours, or null. */
function PrismaticAI::OurAirportIn(town) {
	if (!AIR_PORTS.rawin(town)) return null;
	local t = AIR_PORTS[town];
	if (!AIAirport.IsAirportTile(t) || AITile.GetOwner(t) != AICompany.ResolveCompanyID(AICompany.COMPANY_SELF)) {
		delete AIR_PORTS[town];
		return null;
	}
	return t;
}

/* The most levelling one site may cost is the setting air_level_max (AIR_LEVEL_MAX, main.nut); the
 * autopilot's route budget reserves it for both airports (autopilot.nut). */

function PrismaticAI::PassengerCargo() {
	local cargos = AICargoList();
	for (local c = cargos.Begin(); !cargos.IsEnd(); c = cargos.Next()) {
		if (AICargo.HasCargoClass(c, AICargo.CC_PASSENGERS)) return c;
	}
	return -1;
}

function PrismaticAI::AirportType() {
	foreach (t in [AIAirport.AT_LARGE, AIAirport.AT_METROPOLITAN, AIAirport.AT_COMMUTER, AIAirport.AT_SMALL]) {
		if (AIAirport.IsValidAirportType(t)) return t;
	}
	return -1;
}

/* Build an airport near `town`; returns its tile or null. */
function PrismaticAI::BuildAirportNear(town, type, pax) {
	local w = AIAirport.GetAirportWidth(type), h = AIAirport.GetAirportHeight(type);
	local rad = AIAirport.GetAirportCoverageRadius(type);
	local c = AITown.GetLocation(town);
	local tiles = AITileList();
	tiles.AddRectangle(AIMap.GetTileIndex(max(1, AIMap.GetTileX(c) - 14), max(1, AIMap.GetTileY(c) - 14)),
		AIMap.GetTileIndex(min(AIMap.GetMapSizeX() - 2, AIMap.GetTileX(c) + 14), min(AIMap.GetMapSizeY() - 2, AIMap.GetTileY(c) + 14)));
	tiles.Valuate(AITile.IsBuildableRectangle, w, h);
	tiles.KeepValue(1);
	tiles.Valuate(AITile.GetCargoAcceptance, pax, w, h, rad);
	tiles.KeepAboveValue(9);
	tiles.Sort(AIList.SORT_BY_VALUE, AIList.SORT_DESCENDING);
	local tried = 0;
	for (local t = tiles.Begin(); !tiles.IsEnd() && tried < 25; t = tiles.Next()) {
		tried++;
		AILog.Info("PrismaticAI: survey airport in " + AITown.GetName(town) + " at tile " + t + " (passengers accepted " + tiles.GetValue(t) + ")");
		AIController.Sleep(PACE);
		if (AIAirport.BuildAirport(t, type, AIStation.STATION_NEW)) {
			AILog.Info("PrismaticAI: built airport in " + AITown.GetName(town) + " at tile " + t);
			return t;
		}
		/* Most refusals in the battle were slopes (ERR_FLAT_LAND_REQUIRED): level the rectangle to the
		 * height of its first tile and try once more, which a buildable rectangle usually allows. */
		if (AIError.GetLastError() == AIError.ERR_FLAT_LAND_REQUIRED) {
			local far = AIMap.GetTileIndex(AIMap.GetTileX(t) + w - 1, AIMap.GetTileY(t) + h - 1);
			/* Only when levelling is cheap: in a battle, levelling plus two airports once left no money
			 * for a plane, and the company sat on two empty airports for good. */
			local cost = 0;
			{
				local test = AITestMode();
				local acc = AIAccounting();
				AITile.LevelTiles(t, far);
				cost = acc.GetCosts();
			}
			if (cost <= AIR_LEVEL_MAX && AITile.LevelTiles(t, far) && AIAirport.BuildAirport(t, type, AIStation.STATION_NEW)) {
				AILog.Info("PrismaticAI: built airport in " + AITown.GetName(town) + " at tile " + t + " after levelling it");
				return t;
			}
			/* Say why: a test pass that succeeded leaves ERR_NONE as the last error. */
			if (cost > AIR_LEVEL_MAX) {
				AILog.Warning("PrismaticAI: airport at tile " + t + " refused: levelling costs " + cost + " (air_level_max " + AIR_LEVEL_MAX + ")");
				continue;
			}
		}
		local err = AIError.GetLastError();
		/* The town allows no further station: join one of ours next to the site instead. In a battle
		 * this refused 365 sites in one game. */
		if (err == AIStation.ERR_STATION_TOO_MANY_STATIONS_IN_TOWN &&
				AIAirport.BuildAirport(t, type, AIStation.STATION_JOIN_ADJACENT)) {
			AILog.Info("PrismaticAI: built airport in " + AITown.GetName(town) + " at tile " + t + ", joined to our adjacent station");
			return t;
		}
		AILog.Warning("PrismaticAI: airport at tile " + t + " refused: " + AIError.GetLastErrorString());
		/* The authority's answer is the same for every site of this town: stop surveying it. */
		if (err == AIError.ERR_LOCAL_AUTHORITY_REFUSES) {
			AIR_BAD_TOWNS[town] <- "authority";
			return null;
		}
		/* "Too many stations" is the authority refusing another airport (STR_ERROR_LOCAL_AUTHORITY_
		 * REFUSES_AIRPORT): the same for every site, so stop here as well. In the battle it cost up
		 * to 125 refused surveys a game, mostly in towns where AAAHogEx flew first. */
		if (err == AIStation.ERR_STATION_TOO_MANY_STATIONS_IN_TOWN) {
			AIR_BAD_TOWNS[town] <- "stations";
			return null;
		}
	}
	AILog.Warning("PrismaticAI: no airport site in " + AITown.GetName(town) + " after " + tried + " tries");
	return null;
}

/* The buildable passenger plane with the best capacity x speed that `type` airports take. */
function PrismaticAI::PickPlane(type, pax) {
	local small = (type == AIAirport.AT_SMALL || type == AIAirport.AT_COMMUTER);
	local best = -1, best_value = -1;
	local engines = AIEngineList(AIVehicle.VT_AIR);
	for (local e = engines.Begin(); !engines.IsEnd(); e = engines.Next()) {
		if (!AIEngine.IsBuildable(e) || (!AIEngine.CanRefitCargo(e, pax) && AIEngine.GetCargoType(e) != pax)) continue;
		if (small && AIEngine.GetPlaneType(e) == AIAirport.PT_BIG_PLANE) continue;
		local v = AIEngine.GetCapacity(e) * AIEngine.GetMaxSpeed(e);
		if (v > best_value) { best_value = v; best = e; }
	}
	return best;
}

/* Remove an airport of ours that no vehicle serves (a route that was not completed). */
function PrismaticAI::RemoveIdleAirport(tile) {
	local st = AIStation.GetStationID(tile);
	if (!AIStation.IsValidStation(st) || AIVehicleList_Station(st).Count() > 0) return;
	if (AIAirport.RemoveAirport(tile)) AILog.Info("PrismaticAI: removed unused airport at tile " + tile);
}

/* An air route between towns a and b with up to `planes` aircraft. Returns the number running. */
function PrismaticAI::BuildAirRoute(a, b, planes) {
	if (!AITown.IsValidTown(a) || !AITown.IsValidTown(b) || a == b) {
		AILog.Warning("PrismaticAI: air route needs two different towns (air_a=" + a + ", air_b=" + b + ")");
		return 0;
	}
	local pax = PrismaticAI.PassengerCargo();
	local type = PrismaticAI.AirportType();
	if (pax == -1 || type == -1) { AILog.Warning("PrismaticAI: no passengers or no airport type available yet"); return 0; }
	AILog.Info("PrismaticAI: planner pair by air: " + AITown.GetName(a) + " - " + AITown.GetName(b) +
		" (" + AIMap.DistanceManhattan(AITown.GetLocation(a), AITown.GetLocation(b)) + " tiles)");
	/* A town that has our airport flies from it: a hub serves several routes (air_reuse). */
	local pa = PrismaticAI.OurAirportIn(a);
	if (pa == null) pa = PrismaticAI.BuildAirportNear(a, type, pax);
	if (pa == null) return 0;
	local pb = PrismaticAI.OurAirportIn(b);
	if (pb == null) pb = PrismaticAI.BuildAirportNear(b, type, pax);
	/* A half-built route is an airport that costs upkeep and carries nobody: in a battle the
	 * autopilot left dozens. Take the first airport down again when the second one fails. */
	if (pb == null) { PrismaticAI.RemoveIdleAirport(pa); return 0; }
	AIR_PORTS[a] <- pa;
	AIR_PORTS[b] <- pb;
	/* A reused airport may be smaller than what is built now: the plane must fit both. */
	foreach (t in [pa, pb]) {
		local at = AIAirport.GetAirportType(t);
		if (at == AIAirport.AT_SMALL || at == AIAirport.AT_COMMUTER) type = at;
	}
	local engine = PrismaticAI.PickPlane(type, pax);
	if (engine == -1) {
		AILog.Warning("PrismaticAI: no plane fits these airports yet");
		PrismaticAI.RemoveIdleAirport(pa);
		PrismaticAI.RemoveIdleAirport(pb);
		return 0;
	}
	local hangar = AIAirport.GetHangarOfAirport(pa);
	local running = 0;
	for (local i = 0; i < planes; i++) {
		local v = AIVehicle.BuildVehicle(hangar, engine);
		if (!AIVehicle.IsValidVehicle(v)) {
			AILog.Warning("PrismaticAI: plane " + i + " not bought: " + AIError.GetLastErrorString());
			break;
		}
		if (AIEngine.GetCargoType(engine) != pax) AIVehicle.RefitVehicle(v, pax);
		/* Alternate the first stop so the planes spread over the route. */
		local first = (i % 2 == 0) ? pa : pb, second = (i % 2 == 0) ? pb : pa;
		local flags = (PRISMATIC_FULL_LOAD == 1) ? AIOrder.OF_FULL_LOAD_ANY : AIOrder.OF_NONE;
		AIOrder.AppendOrder(v, first, flags);
		AIOrder.AppendOrder(v, second, flags);
		AIVehicle.StartStopVehicle(v);
		running++;
		AILog.Info("PrismaticAI: plane " + running + " (" + AIEngine.GetName(engine) + ") flies " + AITown.GetName(a) + " - " + AITown.GetName(b));
		AIController.Sleep(PACE);
	}
	AILog.Info("PrismaticAI: built an air route " + AITown.GetName(a) + " - " + AITown.GetName(b) + " with " + running + " plane(s)");
	/* No plane bought: the airports would only cost upkeep. */
	if (running == 0) {
		PrismaticAI.RemoveIdleAirport(pa);
		PrismaticAI.RemoveIdleAirport(pb);
	}
	return running;
}
