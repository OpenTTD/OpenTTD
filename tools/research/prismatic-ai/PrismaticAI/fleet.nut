/*
 * Fleet management, every heartbeat: capacity follows demand instead of a count fixed at build time.
 *
 * - A route is the set of stations in a vehicle's orders. For each route, the cargo waiting at its
 *   stations (AIStation.GetCargoWaiting) is compared with one vehicle's capacity: when more than 1.5
 *   loads wait and money is above the reserve, one more vehicle is cloned with shared orders, up to
 *   a cap per type (road 8, air 4). Trains are not added here: a rail line's trains and wagons are
 *   the Rhai steward's targets, which lines.nut ReconcileLines carries out up to the line's
 *   trains_max, the trains its two-platform termini hold without a meeting on the single track.
 * - Too many vehicles: on a two-stop route the game's link graph gives the planned monthly flow
 *   each way (GetCargoPlannedVia); needed = max(1, ceil(flow / one vehicle's monthly capacity)), and
 *   the newest vehicle above that is sold. Without link data yet, a route with more than one vehicle
 *   where under a quarter of a load waits for 6 heartbeats in a row sells its newest.
 * - A vehicle older than two years that lost money last year and this year is sent to its depot
 *   and sold once it stands there.
 * Every action is logged with its station tile, so the camera can show it.
 */

function PrismaticAI::RouteKey(v) {
	local ids = [];
	for (local i = 0; i < AIOrder.GetOrderCount(v); i++) {
		local st = AIStation.GetStationID(AIOrder.GetOrderDestination(v, i));
		if (AIStation.IsValidStation(st)) ids.push(st);
	}
	ids.sort();
	local k = "";
	foreach (s in ids) k += s + ",";
	return k;
}

function PrismaticAI::DepotFor(v) {
	local type = AIVehicle.GetVehicleType(v);
	local first = AIOrder.GetOrderDestination(v, 0);
	if (type == AIVehicle.VT_AIR) return AIAirport.GetHangarOfAirport(first);
	if (type != AIVehicle.VT_ROAD) return -1;
	local depots = AIDepotList(AITile.TRANSPORT_ROAD);
	depots.Valuate(AIMap.DistanceManhattan, first);
	depots.KeepBelowValue(16);
	depots.Sort(AIList.SORT_BY_VALUE, AIList.SORT_ASCENDING);
	return depots.IsEmpty() ? -1 : depots.Begin();
}

/* Whether the route's vehicles that are old enough to judge (a year) earned money last year. Little
 * waiting cargo on such a route means it is served well, not over-served: aircraft pick up every
 * passenger, and the "nothing waits" rule sold a pair of planes earning about 67k a year each, until
 * the air routes were gone (LAN AI battle, research/ai-battle/README.md). */
function PrismaticAI::RouteEarns(k) {
	local profit = 0, judged = 0;
	local all = AIVehicleList();
	for (local u = all.Begin(); !all.IsEnd(); u = all.Next()) {
		if (AIVehicle.GetAge(u) < 365 || PrismaticAI.RouteKey(u) != k) continue;
		profit += AIVehicle.GetProfitLastYear(u);
		judged++;
	}
	return judged == 0 || profit > 0;
}

/* Surplus to put to work (steward on): money above this many reserves, and above the setting
 * invest_min, funds more vehicles. */
const INVEST_SURPLUS_RESERVES = 20;

/* Put idle money to work: one more vehicle on the route whose judged vehicles (a year old) earned
 * the most per vehicle last year, while that is above a third of the vehicle's price and the route
 * is under its cap. The winner of the LAN AI battle grew its fleet tenfold while PrismaticAI sat on
 * millions with ~30 vehicles; waiting cargo alone never called for more. Road and air only: rail
 * lines grow through the steward's targets (lines.nut, steward.nut). */
function PrismaticAI::InvestSurplus(b) {
	local money = AICompany.GetBankBalance(AICompany.COMPANY_SELF);
	/* With invest_borrow the unused loan counts too (AAAHogEx plans against balance + unused loan). */
	local usable = money + (b.invest_borrow == 1 ? AICompany.GetMaxLoanAmount() - AICompany.GetLoanAmount() : 0);
	if (usable < max(b.invest_min, INVEST_SURPLUS_RESERVES * b.cash_reserve)) return;
	local routes = {};
	local all = AIVehicleList();
	for (local v = all.Begin(); !all.IsEnd(); v = all.Next()) {
		if (this.selling.rawin(v)) continue;
		local type = AIVehicle.GetVehicleType(v);
		if (type != AIVehicle.VT_ROAD && type != AIVehicle.VT_AIR) continue;
		local k = PrismaticAI.RouteKey(v);
		if (k == "") continue;
		if (!routes.rawin(k)) routes[k] <- {sample = v, count = 0, profit = 0, judged = 0, type = type};
		local r = routes[k];
		r.count++;
		if (AIVehicle.GetAge(v) >= 365) { r.profit += AIVehicle.GetProfitLastYear(v); r.judged++; }
	}
	local best = null, best_ppv = 0;
	foreach (k, r in routes) {
		if (r.judged == 0 || r.count >= ((r.type == AIVehicle.VT_AIR) ? b.invest_air_cap : b.invest_road_cap)) continue;
		/* At most as many unjudged vehicles (under a year old) as judged ones: the profit per vehicle
		 * counts only the judged, so it does not fall as vehicles are added, and without this GA
		 * g276.brainSafe1 (OpenTTD seed 1878134858, slot 0) put 50 buses on one route at 1810 a year
		 * each and sold 96 vehicles over the game as each new one lost money. */
		if (r.count - r.judged >= r.judged) continue;
		local ppv = r.profit / r.judged;
		if (ppv * b.invest_ppv_div < AIEngine.GetPrice(AIVehicle.GetEngineType(r.sample))) continue;
		if (ppv > best_ppv) { best_ppv = ppv; best = r; }
	}
	if (best == null) return;
	local depot = PrismaticAI.DepotFor(best.sample);
	if (depot == -1) return;
	local price = AIEngine.GetPrice(AIVehicle.GetEngineType(best.sample));
	if (money < price + b.cash_reserve) {
		if (b.invest_borrow != 1) return;
		local step = AICompany.GetLoanInterval();
		local want = AICompany.GetLoanAmount() + ((price + b.cash_reserve - money) / step + 1) * step;
		if (want > AICompany.GetMaxLoanAmount() || !AICompany.SetLoanAmount(want)) return;
	}
	local nv = AIVehicle.CloneVehicle(depot, best.sample, true);
	if (AIVehicle.IsValidVehicle(nv)) {
		AIVehicle.StartStopVehicle(nv);
		AILog.Info("PrismaticAI: invest: " + best_ppv + " a year per vehicle on route " + PrismaticAI.RouteKey(best.sample) +
			"; added vehicle " + nv + " (" + (best.count + 1) + " on the route, money " + money + ")");
	}
}

function PrismaticAI::ManageFleet(reserve) {
	local routes = {};
	local list = AIVehicleList();
	/* Planes per air route that are not on their way to be sold. With air_keep_last on (AIR_KEEP_LAST,
	 * main.nut) the last one is never sold: on
	 * OpenTTD seed 725554694 (company 2) every plane lost money in 1964-66, the seller took the last
	 * of each route, and with no plane left to clone the company never flew again and ended among
	 * the weakest AIs. The default, which kept one plane on its first route, grew it back to 12
	 * planes at 23k a year each. */
	local flying = {};
	for (local v = list.Begin(); !list.IsEnd(); v = list.Next()) {
		if (AIVehicle.GetVehicleType(v) != AIVehicle.VT_AIR || this.selling.rawin(v)) continue;
		local k = PrismaticAI.RouteKey(v);
		flying[k] <- (flying.rawin(k) ? flying[k] : 0) + 1;
	}
	for (local v = list.Begin(); !list.IsEnd(); v = list.Next()) {
		/* Selling: two years old, losing money last year and this year. */
		if (this.selling.rawin(v)) {
			if (AIVehicle.IsStoppedInDepot(v)) {
				AILog.Info("PrismaticAI: fleet: sold unprofitable vehicle " + v + " at tile " + AIVehicle.GetLocation(v));
				AIVehicle.SellVehicle(v);
				delete this.selling[v];
			}
			continue;
		}
		if (AIVehicle.GetAge(v) > 730 && AIVehicle.GetProfitLastYear(v) < 0 && AIVehicle.GetProfitThisYear(v) < 0) {
			local air = AIVehicle.GetVehicleType(v) == AIVehicle.VT_AIR;
			if (air && AIR_KEEP_LAST == 1 && flying[PrismaticAI.RouteKey(v)] <= 1) continue; /* the route's last plane */
			if (AIVehicle.SendVehicleToDepot(v)) {
				if (air) flying[PrismaticAI.RouteKey(v)]--;
				this.selling[v] <- true;
				AILog.Info("PrismaticAI: fleet: vehicle " + v + " lost money two years running; sending it to be sold");
			}
			continue;
		}
		local k = PrismaticAI.RouteKey(v);
		if (k == "") continue;
		if (!routes.rawin(k)) routes[k] <- {sample = v, count = 0, newest = v};
		routes[k].count++;
		if (AIVehicle.GetAge(v) < AIVehicle.GetAge(routes[k].newest)) routes[k].newest = v;
	}
	foreach (k, r in routes) {
		local v = r.sample;
		local type = AIVehicle.GetVehicleType(v);
		if (type == AIVehicle.VT_RAIL) continue;
		/* Planes per air route: 4, or 8 with the steward on (autopilot): in the battle the winner ran
		 * 10-14 planes on its first routes by 1962 and its air profit funded everything else. */
		local cap_limit = (type == AIVehicle.VT_AIR) ? (this.steward_on ? this.air_cap : 4) : 8;
		if (r.count >= cap_limit) continue;
		local cargo = AIEngine.GetCargoType(AIVehicle.GetEngineType(v));
		local cap = AIVehicle.GetCapacity(v, cargo);
		if (cap <= 0) continue;
		/* What waits for this route: at each stop, the cargo bound for the route's next stop
		 * (GetCargoWaitingVia). Under cargodist much of a station's cargo waits for other next hops
		 * and would never board here (openttd-a0's link audit caught the over-adding). */
		local waiting = 0, where = -1;
		local n_orders = AIOrder.GetOrderCount(v);
		for (local i = 0; i < n_orders; i++) {
			local tile = AIOrder.GetOrderDestination(v, i);
			local st = AIStation.GetStationID(tile);
			local next = AIStation.GetStationID(AIOrder.GetOrderDestination(v, (i + 1) % n_orders));
			if (!AIStation.IsValidStation(st) || !AIStation.IsValidStation(next)) continue;
			local w = AIStation.GetCargoWaitingVia(st, next, cargo);
			if (w > waiting) { waiting = w; where = tile; }
		}
		/* Too many by the game's own link graph: on a two-stop route the planned monthly flow each way
		 * (AIStation.GetCargoPlannedVia) needs max(1, ceil(flow / monthly capacity of one vehicle))
		 * vehicles; the monthly capacity is the vehicle's capacity times its round trips a month
		 * (0.036 tiles a day per km/h, .ai knowledge cargo-payment-and-vehicle-speed). Judged once the
		 * newest vehicle is 120 days old, so the link graph has measured the route. */
		local stops = [];
		for (local i = 0; i < AIOrder.GetOrderCount(v); i++) {
			local st = AIStation.GetStationID(AIOrder.GetOrderDestination(v, i));
			if (!AIStation.IsValidStation(st)) continue;
			local seen = false;
			foreach (x in stops) if (x == st) seen = true; /* no Array.find in this Squirrel */
			if (!seen) stops.push(st);
		}
		if (stops.len() == 2 && r.count > 1 && type != AIVehicle.VT_RAIL && AIVehicle.GetAge(r.newest) > 120) {
			local flow = max(AIStation.GetCargoPlannedVia(stops[0], stops[1], cargo), AIStation.GetCargoPlannedVia(stops[1], stops[0], cargo));
			local tiles = AIMap.DistanceManhattan(AIStation.GetLocation(stops[0]), AIStation.GetLocation(stops[1]));
			local kmh = max(1, AIEngine.GetMaxSpeed(AIVehicle.GetEngineType(v)));
			local round_days = 2 * tiles * 1000 / (kmh * 36) + 4;
			local monthly = max(1, cap * 30 / max(1, round_days));
			local needed = max(1, (flow + monthly - 1) / monthly);
			if (flow > 0 && r.count > needed) {
				if (AIVehicle.SendVehicleToDepot(r.newest)) {
					this.selling[r.newest] <- true;
					AILog.Info("PrismaticAI: fleet: link flow " + flow + "/month needs " + needed + " vehicle(s) of " + monthly +
						"/month, route has " + r.count + "; selling vehicle " + r.newest);
				}
				continue;
			}
		}
		/* Too many by waiting cargo, where the link graph has no answer yet: almost nothing waits at any
		 * stop for 6 heartbeats in a row and the route has more than one vehicle. */
		if (waiting * 4 < cap && r.count > 1 && !PrismaticAI.RouteEarns(k)) {
			local n = this.idle.rawin(k) ? this.idle[k] + 1 : 1;
			this.idle[k] <- n;
			if (n >= 6) {
				this.idle[k] <- 0;
				local newest = -1, newest_age = 1 << 30;
				local all = AIVehicleList();
				for (local u = all.Begin(); !all.IsEnd(); u = all.Next()) {
					if (PrismaticAI.RouteKey(u) == k && AIVehicle.GetAge(u) < newest_age && !this.selling.rawin(u)) { newest = u; newest_age = AIVehicle.GetAge(u); }
				}
				if (newest != -1 && AIVehicle.SendVehicleToDepot(newest)) {
					this.selling[newest] <- true;
					AILog.Info("PrismaticAI: fleet: " + r.count + " vehicles on a route where " + waiting + " wait (capacity " + cap + "); selling vehicle " + newest);
				}
			}
			continue;
		}
		this.idle[k] <- 0;
		/* Air, learned from AAAHogEx: a plane more while both airports have passengers waiting, not
		 * only when one end piles up 1.5 loads (it ran twice our planes on the same routes). */
		local both_ok = false;
		if (type == AIVehicle.VT_AIR && this.air_add_waiting > 0 && stops.len() == 2) {
			local w0 = AIStation.GetCargoWaiting(stops[0], cargo), w1 = AIStation.GetCargoWaiting(stops[1], cargo);
			both_ok = min(w0, w1) > this.air_add_waiting;
			if (both_ok) { waiting = min(w0, w1); where = AIStation.GetLocation(stops[0]); }
		}
		if (!both_ok && waiting * 2 <= cap * 3) continue;
		/* And never above what the link's planned flow needs plus one, once the link graph knows it. */
		if (stops.len() == 2 && !both_ok) {
			local flow = max(AIStation.GetCargoPlannedVia(stops[0], stops[1], cargo), AIStation.GetCargoPlannedVia(stops[1], stops[0], cargo));
			local tiles = AIMap.DistanceManhattan(AIStation.GetLocation(stops[0]), AIStation.GetLocation(stops[1]));
			local kmh = max(1, AIEngine.GetMaxSpeed(AIVehicle.GetEngineType(v)));
			local monthly = max(1, cap * 30 / max(1, 2 * tiles * 1000 / (kmh * 36) + 4));
			if (flow > 0 && r.count >= (flow + monthly - 1) / monthly + 1) continue;
		}
		/* Never a vehicle more on a route whose year-old vehicles lost money last year, whatever the
		 * waiting cargo says. With air_full_load and a low air_add_waiting, GA g276.brainSafe1 (OpenTTD
		 * seed 1878134858, slot 0) put 17 planes on 3 routes that lost 42.7k a year between them, and
		 * ended last. */
		if (!PrismaticAI.RouteEarns(k)) continue;
		if (AICompany.GetBankBalance(AICompany.COMPANY_SELF) < reserve) return;
		local depot = PrismaticAI.DepotFor(v);
		if (depot == -1) continue;
		local nv = AIVehicle.CloneVehicle(depot, v, true);
		if (AIVehicle.IsValidVehicle(nv)) {
			AIVehicle.StartStopVehicle(nv);
			AILog.Info("PrismaticAI: fleet: " + waiting + " " + AICargo.GetCargoLabel(cargo) + " waiting at tile " + where +
				" for a capacity of " + cap + "; added vehicle " + nv + " (" + (r.count + 1) + " on the route)");
		}
	}
}
