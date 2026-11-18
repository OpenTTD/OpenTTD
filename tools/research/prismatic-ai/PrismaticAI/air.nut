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
		AILog.Warning("PrismaticAI: airport at tile " + t + " refused: " + AIError.GetLastErrorString());
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
	local pa = PrismaticAI.BuildAirportNear(a, type, pax);
	if (pa == null) return 0;
	local pb = PrismaticAI.BuildAirportNear(b, type, pax);
	if (pb == null) return 0;
	local engine = PrismaticAI.PickPlane(type, pax);
	if (engine == -1) { AILog.Warning("PrismaticAI: no plane fits these airports yet"); return 0; }
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
		AIOrder.AppendOrder(v, first, AIOrder.OF_NONE);
		AIOrder.AppendOrder(v, second, AIOrder.OF_NONE);
		AIVehicle.StartStopVehicle(v);
		running++;
		AILog.Info("PrismaticAI: plane " + running + " (" + AIEngine.GetName(engine) + ") flies " + AITown.GetName(a) + " - " + AITown.GetName(b));
		AIController.Sleep(PACE);
	}
	AILog.Info("PrismaticAI: built an air route " + AITown.GetName(a) + " - " + AITown.GetName(b) + " with " + running + " plane(s)");
	return running;
}
