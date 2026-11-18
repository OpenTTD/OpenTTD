/*
 * Rail lines as desired state (RESEARCH-ONLY): the bot builds a line, the Rhai steward
 * (tools/research/mcp-panel/rhai/rail_steward.rhai) decides what it should run, and the bot makes it
 * so. The bot never decides here; it reconciles.
 *
 * - Every line has a slot: its place in RAIL_LINES, from 1. The steward reads each line's geometry
 *   from the record the bot logs (LineRecord, "PrismaticAI: rail line <slot>: label=..|.."), at
 *   once when the line is built and again on the first heartbeat of every game month, so a steward
 *   started late or a panel whose event buffer rolled over still finds every line.
 * - The steward's targets are this AI's settings line<slot>_trains and line<slot>_wagons (info.nut
 *   declares LINE_SLOTS of each; -1 means no target). They are CONFIG_INGAME, so a change goes
 *   through openttd.ai.set_setting and the owner's approval gate, like every other change.
 * - trains: 0 closes the line (its trains go to the depot and are sold, then its track, stations
 *   and depot are demolished, so it stops costing maintenance); 1..trains_max is the number of
 *   trains (a clone of the first, shared orders; above trains_max the platforms could not hold them).
 * - wagons: wagons per train, at most wagons_fit (what the platform holds at the wagon's real
 *   length). A train with another count is sent to the depot, resized there and started again.
 */

/* The trains of `line`: rail vehicles whose orders name both of its stations (two lines may share
 * a station since a platform joins another's). */
function PrismaticAI::TrainsOf(line) {
	local out = [];
	local st_a = AIStation.GetStationID(line.a), st_b = AIStation.GetStationID(line.b);
	if (!AIStation.IsValidStation(st_a) || !AIStation.IsValidStation(st_b)) return out;
	local list = AIVehicleList_Station(st_a);
	for (local v = list.Begin(); !list.IsEnd(); v = list.Next()) {
		if (AIVehicle.GetVehicleType(v) != AIVehicle.VT_RAIL) continue;
		for (local i = 0; i < AIOrder.GetOrderCount(v); i++) {
			if (AIStation.GetStationID(AIOrder.GetOrderDestination(v, i)) == st_b) { out.push(v); break; }
		}
	}
	return out;
}

/* The wagons a platform of `line.len` tiles holds behind `train`'s locomotive: the train's real
 * length (AIVehicle.GetLength, sixteenths of a tile) shared evenly over its parts, which is exact
 * for the default vehicles (half a tile each) and an estimate for a NewGRF set; ResizeTrain checks
 * the real length of every wagon it adds, so the estimate never makes a train longer than its
 * platform. */
function PrismaticAI::WagonsFit(line, train) {
	local n = AIVehicle.GetNumWagons(train);
	local part = max(1, AIVehicle.GetLength(train) / max(1, n));
	return max(1, line.len * 16 / part - 1);
}

/* One machine-readable record of a line, for the steward (rail_lib.rhai parse_record). */
function PrismaticAI::LineRecord(slot, line) {
	local trains = PrismaticAI.TrainsOf(line);
	local fit = trains.len() > 0 ? PrismaticAI.WagonsFit(line, trains[0]) : max(1, 2 * line.len - 1);
	line.wagons_fit <- fit;
	return "PrismaticAI: rail line " + slot + ": label=" + line.label + "|a=" + line.a + "|b=" + line.b +
		"|cargo=" + line.cargo + "|platforms=" + line.trains_max + "|len=" + line.len + "|depot=" + line.depot +
		"|trains_max=" + line.trains_max + "|wagons_fit=" + fit + "|tiles=" + line.path.len() +
		"|state=" + (line.closed ? "closed" : "open");
}

/* The game month now, to log every line's record once a month. */
function PrismaticAI::MonthKey() {
	local d = AIDate.GetCurrentDate();
	return AIDate.GetYear(d) * 12 + AIDate.GetMonth(d);
}

/* Log every line's record (LineRecord). */
function PrismaticAI::LogLines() {
	foreach (i, line in RAIL_LINES) AILog.Info(PrismaticAI.LineRecord(i + 1, line));
}

/* A setting of this AI, or -1 where info.nut declares none (more lines than slots). */
function PrismaticAI::LineTarget(slot, what) {
	local name = "line" + slot + "_" + what;
	local v = AIController.GetSetting(name);
	return (v == null) ? -1 : v;
}

/* Resize `train`, stopped in its depot, to `want` wagons of the type it has (or the line's best),
 * never longer than the platform. Returns the wagons it has afterwards. */
function PrismaticAI::ResizeTrain(line, train, want) {
	local depot = AIVehicle.GetLocation(train);
	local wagon = AIVehicle.GetNumWagons(train) > 1 ? AIVehicle.GetWagonEngineType(train, 1) : PrismaticAI.PickRailEngines(line.railtype, line.cargo).wagon;
	while (AIVehicle.GetNumWagons(train) - 1 > want) AIVehicle.SellWagon(train, AIVehicle.GetNumWagons(train) - 1);
	while (AIVehicle.GetNumWagons(train) - 1 < want && wagon != -1 && AIEngine.IsBuildable(wagon)) {
		local w = AIVehicle.BuildVehicle(depot, wagon);
		if (!AIVehicle.IsValidVehicle(w)) break;
		if (AIEngine.GetCargoType(wagon) != line.cargo) AIVehicle.RefitVehicle(w, line.cargo);
		AIVehicle.MoveWagon(w, 0, train, AIVehicle.GetNumWagons(train) - 1);
		if (AIVehicle.GetLength(train) > line.len * 16) { AIVehicle.SellWagon(train, AIVehicle.GetNumWagons(train) - 1); break; }
	}
	return AIVehicle.GetNumWagons(train) - 1;
}

/* Close `line`: every train to the depot and sold (this.selling, ManageFleet sells it there); once
 * none is left, its track, stations and depot are demolished. Returns true when it is gone. */
function PrismaticAI::CloseLine(slot, line) {
	local trains = PrismaticAI.TrainsOf(line);
	foreach (t in trains) {
		if (this.selling.rawin(t)) continue;
		if (AIVehicle.SendVehicleToDepot(t)) {
			this.selling[t] <- true;
			AILog.Info("PrismaticAI: rail line " + slot + " closing: train " + t + " to the depot to be sold");
		}
	}
	if (trains.len() > 0) return false;
	local n = 0;
	foreach (t in line.path) if (AITile.DemolishTile(t)) n++;
	foreach (t in line.built) if (AITile.DemolishTile(t)) n++;
	AITile.DemolishTile(line.depot);
	line.closed = true;
	AILog.Info("PrismaticAI: rail line " + slot + " closed: " + line.label + ", " + n + " tiles cleared");
	AILog.Info(PrismaticAI.LineRecord(slot, line));
	return true;
}

/* Make every line what its targets say (see the head of this file). */
function PrismaticAI::ReconcileLines(reserve) {
	foreach (i, line in RAIL_LINES) {
		if (line.closed) continue;
		local slot = i + 1;
		local want_trains = PrismaticAI.LineTarget(slot, "trains");
		local want_wagons = PrismaticAI.LineTarget(slot, "wagons");
		if (want_trains == 0) { this.CloseLine(slot, line); continue; }
		local trains = PrismaticAI.TrainsOf(line);
		if (want_wagons > 0) {
			if (!("wagons_fit" in line)) PrismaticAI.LineRecord(slot, line);
			want_wagons = min(want_wagons, line.wagons_fit);
			foreach (t in trains) {
				if (this.selling.rawin(t)) continue;
				local have = AIVehicle.GetNumWagons(t) - 1;
				if (have == want_wagons) { if (this.resizing.rawin(t)) delete this.resizing[t]; continue; }
				if (AIVehicle.IsStoppedInDepot(t)) {
					local got = PrismaticAI.ResizeTrain(line, t, want_wagons);
					AIVehicle.StartStopVehicle(t);
					if (this.resizing.rawin(t)) delete this.resizing[t];
					AILog.Info("PrismaticAI: rail line " + slot + ": train " + t + " resized from " + have + " to " + got + " wagons");
				} else if (!this.resizing.rawin(t) && AIVehicle.SendVehicleToDepot(t)) {
					this.resizing[t] <- true;
					AILog.Info("PrismaticAI: rail line " + slot + ": train " + t + " to the depot for " + want_wagons + " wagons (has " + have + ")");
				}
			}
		}
		if (want_trains > 0) {
			want_trains = min(want_trains, line.trains_max);
			local running = 0;
			foreach (t in trains) if (!this.selling.rawin(t)) running++;
			if (running < want_trains && running > 0 && AICompany.GetBankBalance(AICompany.COMPANY_SELF) >= reserve) {
				local nv = AIVehicle.CloneVehicle(line.depot, trains[0], true);
				if (AIVehicle.IsValidVehicle(nv)) {
					AIVehicle.StartStopVehicle(nv);
					AILog.Info("PrismaticAI: rail line " + slot + ": added train " + nv + " (" + (running + 1) + " of " + want_trains + ")");
				}
			} else if (running > want_trains) {
				local newest = -1;
				foreach (t in trains) if (!this.selling.rawin(t) && (newest == -1 || AIVehicle.GetAge(t) < AIVehicle.GetAge(newest))) newest = t;
				if (newest != -1 && AIVehicle.SendVehicleToDepot(newest)) {
					this.selling[newest] <- true;
					AILog.Info("PrismaticAI: rail line " + slot + ": train " + newest + " to the depot to be sold (" + running + " for " + want_trains + ")");
				}
			}
		}
	}
}
