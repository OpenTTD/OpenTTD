/*
 * Rail lines: between two towns (the planner's pair, settings rail_a, rail_b) or from a producing
 * industry to an accepting one (fr_*, the bot's own plan). One model, every number derived.
 *
 * Geometry a train can always run (RESEARCH-ONLY knowledge rail-lines-a-train-can-turn-on):
 * - Each station's axis points at the other end; its site is the flat buildable rectangle that
 *   accepts (or produces) the most cargo, the platforms plus their throat.
 * - A terminus with `rail_platforms` platforms (default 2). With two, a two-tile throat merges them
 *   into the single line: a path signal on each platform's exit, a path signal facing the station
 *   on the line's first tile. Trains wait in a platform, never on the single track, so as many
 *   trains as the smaller end has platforms share one track without a passing loop and without a
 *   head-on collision (trains_max). With one platform there is no signal and one train.
 * - The line is an A* path between the two throats (FindRailPath). It never lays a 90-degree turn
 *   (Turns90): the game forbids one with pf.forbid_90_deg (on in this owner's config) and slows a
 *   train on one where it is allowed. A train meeting a forbidden turn cannot follow its own line,
 *   gets lost and earns nothing. The check runs on every tile, the first beyond each throat and the
 *   last into it included, and across a re-plan.
 * - The laid line is walked piece by piece (VerifyRailLine, AIRail.AreTilesConnected) before a
 *   train is bought: a gap becomes a warning with its tile, never a lost train.
 * - The depot stands behind a platform's dead end (trains turn in it and leave inside the signals),
 *   else beside a straight piece of the line, never on a corner.
 * - Platform length and wagons follow from demand (PlatformPlan): the cargo a month, the round
 *   trip at the locomotive's speed, the trains the platforms allow, capped by the game's
 *   vehicle.max_train_length and station.station_spread.
 * Every step is logged with its tile, so the camera director shows it, and a finished line logs
 * one record ("PrismaticAI: rail line <slot>: ...", lines.nut LineRecord) that
 * tools/research/mcp-panel/rhai/rail_lib.rhai reads.
 */

/* Whether three moves in a row (each +-1 along x or +-map width along y) make a 90-degree turn: a
 * corner on the tile between the first two moves and another between the last two, the train leaving
 * opposite to how it came (d3 == -d1). Alternating corners (d3 == d1) are a 45-degree diagonal and
 * fine. The game's rule is follow_track.hpp: with forbid_90_deg a trackdir that crosses the one the
 * train arrives on (TrackdirCrossesTrackdirs) is cut from what it may take. */
function PrismaticAI::Turns90(d1, d2, d3) {
	return d1 != d2 && d2 != d3 && d3 == -d1;
}

function PrismaticAI::RailStepCost(tile) {
	if (!AIMap.IsValidTile(tile)) return -1;
	/* A road tile is crossed only as a level crossing (CanCross), never built over as land:
	 * AITile.IsBuildable counts a half road tile as buildable. */
	if (AIRoad.IsRoadTile(tile)) return -1;
	if (AITile.IsWaterTile(tile) || !AITile.IsBuildable(tile)) return -1;
	local slope = AITile.GetSlope(tile);
	if (slope == AITile.SLOPE_FLAT) return 3;
	/* Track on a slope stands on a foundation (build_on_slopes, on by default), but not on a steep one. */
	if (AITile.IsSteepSlope(slope)) return -1;
	return 7;
}

/* Whether a station exit at `exit`, left by the step `out` (from the station's end toward the exit),
 * leads on by the same rules as FindRailPath's first step: straight ahead, or to either side where the
 * corner piece on the exit fits its slope (PieceFits), is land the line can take or a road it can
 * cross. Without the slope rule a site passed here and the search then stopped after one node
 * ("every reachable tile tried, 1 nodes", OpenTTD seed 31337, New Prunpool). */
function PrismaticAI::ExitLeadsOn(exit, out, straight = false) {
	local map_w = AIMap.GetMapSizeX();
	local perp = (out == 1 || out == -1) ? map_w : 1;
	local slope = AITile.GetSlope(exit);
	/* A throat's exit leads on straight only (see FindRailPath's from_straight). */
	foreach (d in (straight ? [out] : [out, perp, -perp])) {
		local n = exit + d;
		if (d != out && !PrismaticAI.PieceFits(slope, PrismaticAI.PieceBetween(exit, exit - out, n))) continue;
		if (PrismaticAI.RailStepCost(n) >= 0 || PrismaticAI.CanCross(n, d)) return true;
	}
	return false;
}

/* Whether the line can cross the road on `tile` as a level crossing when it enters the tile by the
 * step `d` (+-1 along x, +-map width along y): a plain road tile (no station, depot or existing
 * rail) on flat land, whose road runs only at right angles to the line. The game builds the
 * crossing when AIRail.BuildRail lays the straight piece across it (rail_cmd.cpp, a straight road
 * crossed perpendicularly). */
function PrismaticAI::CanCross(tile, d) {
	if (!AIMap.IsValidTile(tile) || !AIRoad.IsRoadTile(tile)) return false;
	if (AIRoad.IsRoadStationTile(tile) || AIRoad.IsRoadDepotTile(tile) || AIRail.IsRailTile(tile)) return false;
	if (AIRail.IsLevelCrossingTile(tile)) return false;
	if (AITile.GetSlope(tile) != AITile.SLOPE_FLAT) return false;
	local map_w = AIMap.GetMapSizeX();
	local along = (d == 1 || d == -1) ? 1 : map_w;   /* the line's axis */
	local across = (along == 1) ? map_w : 1;          /* the road must run this way only */
	if (AIRoad.AreRoadTilesConnected(tile, tile + along) || AIRoad.AreRoadTilesConnected(tile, tile - along)) return false;
	return AIRoad.AreRoadTilesConnected(tile, tile + across) || AIRoad.AreRoadTilesConnected(tile, tile - across);
}

/* The track piece on a tile that joins the side toward tile `a` and the side toward tile `b`, as
 * the game's TrackBits (X 1, Y 2, upper 4, lower 8, left 16, right 32). Sides: x-1 NE, y+1 SE,
 * x+1 SW, y-1 NW. */
function PrismaticAI::SideOf(d, map_w) {
	return d == -1 ? 0 : (d == map_w ? 1 : (d == 1 ? 2 : 3));
}

function PrismaticAI::PieceBetween(tile, a, b) {
	local map_w = AIMap.GetMapSizeX();
	local p = PrismaticAI.SideOf(a - tile, map_w), q = PrismaticAI.SideOf(b - tile, map_w);
	local lo = min(p, q), hi = max(p, q);
	if (lo == 0 && hi == 2) return 1;
	if (lo == 1 && hi == 3) return 2;
	if (lo == 0 && hi == 3) return 4;
	if (lo == 1 && hi == 2) return 8;
	if (lo == 2 && hi == 3) return 16;
	return 32;
}

/* Whether a track piece fits on a slope, as the game's GetRailFoundation decides with
 * build_on_slopes on (rail_cmd.cpp): any piece on flat land; a straight piece on any slope that is
 * not steep (inclined or levelled foundation); a corner piece only where it needs no foundation or
 * fits a levelled one (_valid_tracks_without_foundation, _valid_tracks_on_leveled_foundation,
 * indexed by the slope bits W 1, S 2, E 4, N 8). A steep slope takes no piece here. */
function PrismaticAI::PieceFits(slope, piece) {
	if (slope == AITile.SLOPE_FLAT) return true;
	if (AITile.IsSteepSlope(slope)) return false;
	if (piece == 1 || piece == 2) return true;
	local without = [63, 32, 4, 1, 16, 0, 2, 8, 8, 2, 0, 16, 1, 4, 32];
	local leveled = [0, 16, 8, 26, 32, 63, 41, 63, 4, 21, 63, 63, 38, 63, 63];
	return ((without[slope] | leveled[slope]) & piece) == piece;
}

/* A* from `from` to `to` over the four neighbours. Costs are in thirds of a straight piece, the game's
 * own measure of travel (vehicle_base.h): a straight piece is 16 steps of TILE_AXIAL_DISTANCE 192
 * (3072), a corner piece 8 steps of 2 * TILE_CORNER_DISTANCE 256 (2048), two thirds as long. So a
 * diagonal (a staircase of corner pieces that alternate, the train running at 45 degrees) costs 2 a
 * piece against 3 for straight track: the same pieces as an L-shaped line, a third less travel. Its
 * entry pays the turn once; a turn back on itself within two tiles (a U) is not taken.
 * `from_end` and `to_end` are the station end tiles beyond `from` and `to`: the line enters `from`
 * from its station and leaves `to` into the other one, so the pieces on both exits are checked
 * like any other. `avoid` (a table of tiles) is left out of the search: a re-plan passes the tile a
 * piece failed on. `from_in` is the move that arrived at `from_end` (null: the station's straight
 * platform, from - from_end); a re-plan passes the move into the last laid tile, so the first new
 * piece cannot make a 90-degree turn with it. `from_straight` and `to_straight` keep the line
 * straight through its first and last tile: beyond a two-platform throat the second platform
 * reaches the line by a sideways move (M2 to M), so a corner on the line's first tile toward that
 * side would make a 90-degree turn on the second platform's route, which the main platform's
 * route does not show (found by an independent review, 2026-11-18). Returns the tiles (from
 * first) or null within the node budget. */
/* The estimate a tile left to go, in the same thirds: 7, between a diagonal piece (6) and a straight
 * one (9). At 6 every staircase toward the goal had the same f = g + h, so the search spread over the
 * whole rectangle between the stations and lines the old search found failed (seed 31337, 2026-11-17). */
H_TILE <- 7;

function PrismaticAI::FindRailPath(from, to, from_end, to_end, avoid = null, from_in = null, from_straight = false, to_straight = false) {
	local map_w = AIMap.GetMapSizeX();
	local open = AIPriorityQueue();
	local came = {}, cost = {};
	/* A bridge is a jump in the path: its far head `h2` came from the tile before the near head, and
	 * bridge_from[h2] is the near head, bridge_dir[h2] the direction the line keeps beyond it. */
	local bridge_from = {}, bridge_dir = {};
	cost[from] <- 0;
	came[from] <- -1;
	open.Insert(from, H_TILE * AIMap.DistanceManhattan(from, to));
	local budget = 150000, budget0 = budget;
	while (!open.IsEmpty() && budget-- > 0) {
		local cur = open.Pop();
		if (cur == to) {
			local path = [];
			for (local t = to; t != -1; t = came[t]) {
				path.push(t);
				if (bridge_from.rawin(t)) path.push(bridge_from[t]);
			}
			path.reverse();
			return path;
		}
		local prev = came[cur] == -1 ? from_end : came[cur];
		/* The direction the line arrives in: one step, or along the bridge it just left. */
		local in_dir = bridge_dir.rawin(cur) ? bridge_dir[cur] : (cur - prev);
		/* The direction it arrived at `prev` in; equal to in_dir on straight track, at a bridge or at
		 * the start. A corner on `cur` that turns back to it continues a diagonal. */
		local before = in_dir;
		if (!bridge_dir.rawin(cur)) {
			if (came[cur] == -1) {
				/* At `from`: the move into from_end, straight out of a platform unless a re-plan says. */
				if (from_in != null) before = from_in;
			} else if (came.rawin(prev) && came[prev] == -1) {
				/* One beyond `from`: the move into `from` came from from_end. Without this the corner on
				 * `from` was never compared with the next one, and a line could start with a 90-degree
				 * turn no train may take (forbid_90_deg). */
				before = prev - from_end;
			} else if (came.rawin(prev) && !bridge_dir.rawin(prev)) {
				before = prev - came[prev];
			}
		}
		/* A bridge across what blocks the line two tiles ahead (water, a road, rail, a valley): the
		 * near head is the next tile, the far head the first land 3 to 11 tiles on whose bridge the
		 * game accepts (AITestMode; slopes, clearance and length are the game's own rules). */
		local ahead = cur + in_dir;
		if (ahead != to && PrismaticAI.RailStepCost(ahead) >= 0 &&
				PrismaticAI.RailStepCost(ahead + in_dir) < 0 && !PrismaticAI.CanCross(ahead + in_dir, in_dir) && ahead + in_dir != to) {
			for (local k = 2; k <= 10; k++) {
				local h2 = ahead + k * in_dir;
				if (!AIMap.IsValidTile(h2) || h2 == to || PrismaticAI.RailStepCost(h2) < 0) continue;
				if (AIMap.DistanceManhattan(ahead, h2) != k) break; /* wrapped round the map edge */
				local bridge = PrismaticAI.BridgeType(ahead, h2);
				if (bridge == -1) continue;
				local ok = false;
				{
					local test = AITestMode();
					ok = AIBridge.BuildBridge(AIVehicle.VT_RAIL, bridge, ahead, h2);
				}
				if (!ok) continue;
				local c = cost[cur] + 3 * (3 + 8 * (k + 1));
				if (!cost.rawin(h2) || c < cost[h2]) {
					cost[h2] <- c;
					came[h2] <- cur;
					bridge_from[h2] <- ahead;
					bridge_dir[h2] <- in_dir;
					open.Insert(h2, c + H_TILE * AIMap.DistanceManhattan(h2, to));
				}
				break;
			}
		}
		/* A tunnel through the hill ahead: the next tile is a slope rising in the line's direction,
		 * the game finds the far portal (AITunnel.GetOtherTunnelEnd), and the tunnel is taken when it
		 * runs straight on, is at most 15 tiles, the line can go on beyond it, and the game accepts it
		 * (AITestMode). Like a bridge, its two portals sit in a row in the path. */
		if (ahead != to && PrismaticAI.RailStepCost(ahead) >= 0 && AITile.GetSlope(ahead) != AITile.SLOPE_FLAT) {
			local far = AITunnel.GetOtherTunnelEnd(ahead);
			local k = AIMap.IsValidTile(far) ? AIMap.DistanceManhattan(ahead, far) : 0;
			if (k >= 2 && k <= 15 && far == ahead + k * in_dir && far != to &&
					(PrismaticAI.RailStepCost(far + in_dir) >= 0 || far + in_dir == to)) {
				local ok = false;
				{
					local test = AITestMode();
					ok = AITunnel.BuildTunnel(AIVehicle.VT_RAIL, ahead);
				}
				if (ok) {
					local c = cost[cur] + 3 * (3 + 10 * (k + 1));
					if (!cost.rawin(far) || c < cost[far]) {
						cost[far] <- c;
						came[far] <- cur;
						bridge_from[far] <- ahead;
						bridge_dir[far] <- in_dir;
						open.Insert(far, c + H_TILE * AIMap.DistanceManhattan(far, to));
					}
				}
			}
		}
		foreach (next in [cur - 1, cur + 1, cur - map_w, cur + map_w]) {
			if (next == prev) continue;
			if (avoid != null && avoid.rawin(next)) continue;
			/* Straight through a throat's own exit tile, and straight into the far one. */
			if (from_straight && came[cur] == -1 && (next - cur) != in_dir) continue;
			if (to_straight && next == to && (next - cur) != (to_end - to)) continue;
			/* A bridge's far head lets the line go on straight only. */
			if (bridge_dir.rawin(cur) && (next - cur) != in_dir) continue;
			local step = (next == to) ? 1 : PrismaticAI.RailStepCost(next);
			if (step < 0) {
				/* A level crossing: a straight road at right angles, on flat land (CanCross). It costs
				 * more than land, so the line crosses roads only where it must (a station walled in
				 * by its town), and the line passes it straight (no turn on the crossing below). */
				if (!PrismaticAI.CanCross(next, next - cur)) continue;
				step = 12;
			}
			/* No turn on a level crossing: its rail runs straight across the road. */
			if ((next - cur) != in_dir && AIRoad.IsRoadTile(cur)) continue;
			local diagonal = false;
			if ((next - cur) != in_dir) {
				/* A turn: the corner piece on `cur` must fit its slope (PieceFits). Allowing turns
				 * only on flat land left hilly station exits unreachable, and allowing them on any
				 * slope that is not steep built pieces the game refuses (ERR_LAND_SLOPED_WRONG): a
				 * 180-tile coal line failed both ways (OpenTTD research/results duel seed 20208). */
				local s = AITile.GetSlope(cur);
				if (!PrismaticAI.PieceFits(s, PrismaticAI.PieceBetween(cur, prev, next))) continue;
				if (PrismaticAI.Turns90(before, in_dir, next - cur)) continue; /* 90 degrees: two corners turning one way */
				diagonal = before != in_dir && (next - cur) == before;
				/* Entering a turn pays for it; going on along a diagonal does not. */
				if (!diagonal) step += (s == AITile.SLOPE_FLAT) ? 2 : 6;
			}
			/* The piece on the far exit joins this line to the far station. */
			if (next == to && !PrismaticAI.PieceFits(AITile.GetSlope(to), PrismaticAI.PieceBetween(to, cur, to_end))) continue;
			/* Nor a 90-degree turn into the far throat: the corner on `cur` and the one on `to`. */
			if (next == to && PrismaticAI.Turns90(in_dir, next - cur, to_end - to)) continue;
			/* A corner piece on a diagonal is two thirds of a straight one to travel. */
			local c = cost[cur] + (diagonal ? 2 * step : 3 * step);
			if (!cost.rawin(next) || c < cost[next]) {
				cost[next] <- c;
				came[next] <- cur;
				/* Reached on foot now, not over a bridge. */
				if (bridge_from.rawin(next)) { delete bridge_from[next]; delete bridge_dir[next]; }
				/* Weighted A* (H_TILE a tile): far fewer nodes than plain A*, the route in practice a
				 * few percent longer; the search no longer eats months of game time. */
				open.Insert(next, c + H_TILE * AIMap.DistanceManhattan(next, to));
			}
		}
	}
	AILog.Warning("PrismaticAI: no rail path from tile " + from + " to " + to + " (" + AIMap.DistanceManhattan(from, to) + " tiles): "
		+ (budget <= 0 ? "the node budget of " + budget0 + " ran out" : "every reachable tile tried, " + (budget0 - budget) + " nodes"));
	return null;
}

/* 1 when every tile of the w x h rectangle at `t` is flat, else 0 (a valuator for AITileList). */
function PrismaticAI::RectFlat(t, w, h) {
	local mw = AIMap.GetMapSizeX();
	for (local y = 0; y < h; y++) {
		for (local x = 0; x < w; x++) {
			if (AITile.GetSlope(t + y * mw + x) != AITile.SLOPE_FLAT) return 0;
		}
	}
	return 1;
}

/* The company's own train station nearest tile `origin`, at most `reach` tiles away, or null. */
function PrismaticAI::OwnRailStationNear(origin, reach) {
	local list = AIStationList(AIStation.STATION_TRAIN);
	list.Valuate(AIStation.GetDistanceManhattanToTile, origin);
	list.KeepBelowValue(reach + 1);
	list.Sort(AIList.SORT_BY_VALUE, AIList.SORT_ASCENDING);
	return list.IsEmpty() ? null : list.Begin();
}

/* The bounding box of a station's rail tiles: [x0, y0, x1, y1]. */
function PrismaticAI::RailStationBox(st) {
	local b = [AIMap.GetMapSizeX(), AIMap.GetMapSizeY(), 0, 0];
	foreach (t, _ in AITileList_StationType(st, AIStation.STATION_TRAIN)) {
		local x = AIMap.GetTileX(t), y = AIMap.GetTileY(t);
		b[0] = min(b[0], x); b[1] = min(b[1], y); b[2] = max(b[2], x); b[3] = max(b[3], y);
	}
	return b;
}

/* 1 when a w x h platform at tile `t` and the station box `b` together span at most `spread`
 * tiles each way (station.station_spread), so the platform can join that station. */
function PrismaticAI::FitsSpread(t, w, h, b, spread) {
	local x = AIMap.GetTileX(t), y = AIMap.GetTileY(t);
	return (max(b[2], x + w - 1) - min(b[0], x) < spread && max(b[3], y + h - 1) - min(b[1], y) < spread) ? 1 : 0;
}

/* How a w x h platform at `t` sits against a station box `b` = [x0, y0, x1, y1]: 0 when it lies right
 * beside the station's platforms, parallel to them (the same axis, touching their long side, its span
 * overlapping theirs), so the two form one station with side-by-side platforms; otherwise 100 plus its
 * distance to the box, so a scattered platform is the last resort. */
function PrismaticAI::JoinRank(t, w, h, b) {
	local x = AIMap.GetTileX(t), y = AIMap.GetTileY(t);
	local box_along_x = (b[2] - b[0]) >= (b[3] - b[1]);
	local along_x = w >= h;
	if (along_x == box_along_x) {
		if (along_x && (y == b[1] - 1 || y == b[3] + 1) && x <= b[2] && x + w - 1 >= b[0]) return 0;
		if (!along_x && (x == b[0] - 1 || x == b[2] + 1) && y <= b[3] && y + h - 1 >= b[1]) return 0;
	}
	local dx = max(0, max(b[0] - (x + w - 1), x - b[2])), dy = max(0, max(b[1] - (y + h - 1), y - b[3]));
	return 100 + dx + dy;
}

/* The station a new platform near `origin` joins: the company's own train station there, when the
 * game allows a distant join; null builds a new one. Two lines to one power station share one
 * station with two platforms: on seed 31337 the second coal line built "Slonham Transfer" four
 * tiles from the first line's "Slonham East", and the duplicate audit flagged the pair. */
function PrismaticAI::RailStationToJoin(origin, len) {
	local name = "station.distant_join_stations";
	if (AIGameSettings.IsValid(name) && AIGameSettings.GetValue(name) == 0) return null;
	return PrismaticAI.OwnRailStationNear(origin, 10 + len);
}

/* A terminus near tile `origin` whose axis points at tile `other`: `platforms` platforms of `len`
 * tiles side by side and, with more than one, a two-tile throat that merges them into one line.
 * `site` says what makes a good site: "accept" (cargo accepted, e.g. passengers in a town or coal at
 * a power station) or "produce" (cargo produced into the catchment, at a mine).
 *
 * Geometry, along the move `out` from the platforms' ends toward `other` (one platform is the main
 * one, the other lies `sd` beside it):
 *
 *     platform main  ... end_m | T1m  M   L0 -> the line
 *     platform 2nd   ... end_s | T1s  M2
 *
 * T1m and T1s are straight pieces with a path signal facing out (a train leaves its platform only
 * when it can reserve its whole way into a free platform at the other end); M2 bends the second
 * platform onto M, where it joins the main one; L0 (`exit`) is the first tile of the line and M
 * (`end`) the tile the line search starts from. The bend is a diagonal, never 90 degrees. Without a
 * throat (one platform) `end` is the platform's end and `exit` the tile beyond it.
 *
 * Returns {tile, end, exit, step, platforms, len, platform_rect, throat, backs} or null:
 * `platform_rect` is this station's own platforms ([corner, opposite corner]), `throat` the plain
 * track of its throat (RemoveRailStation removes both and nothing else), `backs` the [depot tile,
 * platform tile] pairs behind each platform's
 * dead end (BuildRailDepotBehind). A company train station already near `origin` gets these
 * platforms (RailStationToJoin) wherever a site fits the station spread; only when none does is a
 * separate station built, with a warning. */
function PrismaticAI::BuildRailStationAt(origin, other, len, cargo, site, label, platforms = 1, hand = 0) {
	local map_w = AIMap.GetMapSizeX();
	local dx = AIMap.GetTileX(other) - AIMap.GetTileX(origin), dy = AIMap.GetTileY(other) - AIMap.GetTileY(origin);
	local along_x = abs(dx) >= abs(dy);
	local axis = along_x ? AIRail.RAILTRACK_NE_SW : AIRail.RAILTRACK_NW_SE;
	local step = along_x ? 1 : map_w;          /* along a platform */
	local side = along_x ? map_w : 1;          /* to the next platform: AIRail.BuildRailStation stacks them so */
	local forward = along_x ? (dx >= 0) : (dy >= 0);
	local out = forward ? step : -step;        /* from the platforms' ends toward `other` */
	/* A double-track terminus (hand +1 at the source, -1 at the far end; DoubleTerminus) has two
	 * more throat tiles, L0 and the junction A1, where the two one-way tracks meet. */
	local throat = (hand != 0) ? 4 : ((platforms > 1) ? 2 : 0);
	local lane = hand * PrismaticAI.QuarterTurn(out);
	/* The site is the platforms and the throat: flat (ERR_FLAT_LAND_REQUIRED for a station, and a
	 * corner on a slope may not fit) and buildable. */
	local w = along_x ? len + throat : platforms, h = along_x ? platforms : len + throat;
	local tiles = AITileList();
	tiles.AddRectangle(AIMap.GetTileIndex(max(2, AIMap.GetTileX(origin) - 10), max(2, AIMap.GetTileY(origin) - 10)),
		AIMap.GetTileIndex(min(AIMap.GetMapSizeX() - 3 - w, AIMap.GetTileX(origin) + 10), min(AIMap.GetMapSizeY() - 3 - h, AIMap.GetTileY(origin) + 10)));
	tiles.Valuate(AITile.IsBuildableRectangle, w, h);
	tiles.KeepValue(1);
	/* What the station will catch: the platforms only, never the throat, which is plain track
	 * (an industry beside the throat but out of the platforms' reach would pass otherwise). */
	local sw = along_x ? len : platforms, sh = along_x ? platforms : len;
	local offset = forward ? 0 : throat * step;
	tiles.Valuate(PrismaticAI.PlatformCoverage, cargo, sw, sh, offset, site == "produce");
	tiles.KeepAboveValue(site == "produce" ? 0 : 7);
	/* Sites that are buildable and catch the cargo but are not flat: the last resort, levelled. */
	local sloped = AITileList();
	sloped.AddList(tiles);
	tiles.Valuate(PrismaticAI.RectFlat, w, h);
	tiles.KeepValue(1);
	sloped.RemoveList(tiles);
	sloped.Valuate(PrismaticAI.RectHeightRange, w, h);
	sloped.KeepBelowValue(RAIL_LEVEL_STEP + 1);
	local join = PrismaticAI.RailStationToJoin(origin, len);
	local attempts = [];
	if (join != null) {
		local spread = AIGameSettings.IsValid("station.station_spread") ? AIGameSettings.GetValue("station.station_spread") : 12;
		local near = AITileList();
		near.AddList(tiles);
		near.Valuate(PrismaticAI.FitsSpread, w, h, PrismaticAI.RailStationBox(join), spread);
		near.KeepValue(1);
		tiles.RemoveList(near);
		/* Side by side first (JoinRank 0): one station with parallel platforms, a transfer yard
		 * for every line of that industry, rather than platforms scattered across the spread. */
		near.Valuate(PrismaticAI.JoinRank, w, h, PrismaticAI.RailStationBox(join));
		near.Sort(AIList.SORT_BY_VALUE, AIList.SORT_ASCENDING);
		attempts.append([near, join]);
	}
	tiles.Sort(AIList.SORT_BY_VALUE, AIList.SORT_DESCENDING);
	attempts.append([tiles, AIStation.STATION_NEW]);
	local tried = 0;
	foreach (a in attempts) {
		local list = a[0], into = a[1];
		if (into == AIStation.STATION_NEW && join != null && !list.IsEmpty()) {
			AILog.Warning("PrismaticAI: no site " + label + " joins " + AIStation.GetName(join) + "; a separate station");
		}
		for (local r = list.Begin(); !list.IsEnd() && tried < 20; r = list.Next()) {
			/* The station's own corner: the throat lies on the `out` side of the platforms. */
			local t = forward ? r : r + throat * step;
			local g = PrismaticAI.TerminusGeometry(t, len, platforms, step, side, out, lane);
			if (g == null) continue;
			tried++;
			AILog.Info("PrismaticAI: survey rail station " + label + " at tile " + t);
			AIController.Sleep(PACE);
			local rank = (into == AIStation.STATION_NEW) ? -1 : PrismaticAI.JoinRank(t, w, h, PrismaticAI.RailStationBox(into));
			if (!AIRail.BuildRailStation(t, axis, platforms, len, into)) {
				AILog.Warning("PrismaticAI: rail station at tile " + t + " refused: " + AIError.GetLastErrorString());
				continue;
			}
			/* The platforms are one rectangle; the throat is plain track. They are removed apart: a
			 * station tile is never demolished, because demolishing one removes every platform of
			 * the station, a joined line's included (station_cmd.cpp RemoveRailStation). */
			local corner = t + (platforms - 1) * side + (len - 1) * step;
			local throat_tiles = [];
			local rec = {tile = t, platform_rect = [t, corner], throat = throat_tiles, step = step};
			if (platforms > 1 && !PrismaticAI.BuildThroat(g, throat_tiles)) {
				AILog.Warning("PrismaticAI: throat of the station at tile " + t + " refused: " + AIError.GetLastErrorString());
				PrismaticAI.RemoveRailStation(rec, len);
				continue;
			}
			AILog.Info("PrismaticAI: built rail station " + label + " at tile " + t + ", " + platforms + " platform(s) of " + len
				+ (into == AIStation.STATION_NEW ? "" : ", platforms of " + AIStation.GetName(into)
					+ (rank == 0 ? " beside its others" : " (apart from its others)")));
			local made = {tile = t, end = g.end, exit = g.exit, step = step, platforms = platforms, len = len,
				platform_rect = [t, corner], throat = throat_tiles, backs = g.backs};
			if (lane != 0) made.dbl <- g.dbl;
			return made;
		}
	}
	/* No flat site: level one. Without this, on OpenTTD seed 260603278 (GA g271.3, slot 3) ten of
	 * the twelve own lines found no flat site at one end, and the company kept 2.1M idle for 15 years
	 * on 3 trains. Only rectangles whose corners span at most RAIL_LEVEL_STEP height steps are
	 * levelled (a bigger step moves corners outside the site, where houses and roads stop it), and
	 * only while the money left after a fourfold margin on the test price still covers the line. */
	sloped.Sort(AIList.SORT_BY_VALUE, AIList.SORT_ASCENDING);
	local levelled = 0;
	for (local r = sloped.Begin(); !sloped.IsEnd() && levelled < 3; r = sloped.Next()) {
		local t = forward ? r : r + throat * step;
		local g = PrismaticAI.TerminusGeometry(t, len, platforms, step, side, out, lane);
		if (g == null) continue;
		local far = AIMap.GetTileIndex(min(AIMap.GetMapSizeX() - 1, AIMap.GetTileX(r) + w),
			min(AIMap.GetMapSizeY() - 1, AIMap.GetTileY(r) + h));
		local cost = 0;
		{
			local test = AITestMode();
			local acc = AIAccounting();
			if (!AITile.LevelTiles(r, far)) cost = 1 << 30;
			else cost = acc.GetCosts();
		}
		local funds = AICompany.GetBankBalance(AICompany.COMPANY_SELF) + AICompany.GetMaxLoanAmount() - AICompany.GetLoanAmount();
		if (cost > RAIL_LEVEL_MAX || funds - 4 * cost < RAIL_LEVEL_KEEP) continue;
		levelled++;
		local real = AIAccounting();
		AILog.Info("PrismaticAI: levelling rail station site " + label + " at tile " + t + " (estimated " + cost + ")");
		AIController.Sleep(PACE);
		if (!AITile.LevelTiles(r, far) || !AIRail.BuildRailStation(t, axis, platforms, len, AIStation.STATION_NEW)) {
			AILog.Warning("PrismaticAI: levelled rail station site at tile " + t + " for " + real.GetCosts() + " and it was still refused: " + AIError.GetLastErrorString());
			continue;
		}
		local corner = t + (platforms - 1) * side + (len - 1) * step;
		local throat_tiles = [];
		local rec = {tile = t, platform_rect = [t, corner], throat = throat_tiles, step = step};
		if (platforms > 1 && !PrismaticAI.BuildThroat(g, throat_tiles)) {
			AILog.Warning("PrismaticAI: throat of the levelled station at tile " + t + " refused: " + AIError.GetLastErrorString());
			PrismaticAI.RemoveRailStation(rec, len);
			continue;
		}
		AILog.Info("PrismaticAI: built rail station " + label + " at tile " + t + " after levelling it for " + real.GetCosts() + ", " + platforms + " platform(s) of " + len);
		local made = {tile = t, end = g.end, exit = g.exit, step = step, platforms = platforms, len = len,
			platform_rect = [t, corner], throat = throat_tiles, backs = g.backs};
		if (lane != 0) made.dbl <- g.dbl;
		return made;
	}
	AILog.Warning("PrismaticAI: no rail station site " + label + " (" + sloped.Count() + " sloped site(s) within " + RAIL_LEVEL_STEP + " height step(s))");
	return null;
}

/* The height span of the corners of the w x h rectangle at `t` (a valuator for AITileList). */
function PrismaticAI::RectHeightRange(t, w, h) {
	local lo = 255, hi = 0;
	for (local dx = 0; dx <= w; dx++) {
		for (local dy = 0; dy <= h; dy++) {
			local c = AIMap.GetTileIndex(min(AIMap.GetMapSizeX() - 1, AIMap.GetTileX(t) + dx),
				min(AIMap.GetMapSizeY() - 1, AIMap.GetTileY(t) + dy));
			local z = AITile.GetCornerHeight(c, AITile.CORNER_N);
			if (z < lo) lo = z;
			if (z > hi) hi = z;
		}
	}
	return hi - lo;
}

/* What a terminus whose site starts at `r` catches with its platforms (sw x sh tiles, `offset` from
 * `r`): the cargo produced into the catchment at a mine, accepted at a town or factory (a valuator). */
function PrismaticAI::PlatformCoverage(r, cargo, sw, sh, offset, produce) {
	return produce ? AITile.GetCargoProduction(r + offset, cargo, sw, sh, 4) : AITile.GetCargoAcceptance(r + offset, cargo, sw, sh, 4);
}

/* The tiles of a terminus whose station corner is `t` (see BuildRailStationAt), with the main
 * platform chosen so that its exit leads on: {end, exit, throat, backs}, or null when neither choice
 * has an exit the line search can leave (ExitLeadsOn, RailStepCost). */
function PrismaticAI::TerminusGeometry(t, len, platforms, step, side, out, lane = 0) {
	local forward = (out == step);
	local rows = [];
	for (local p = 0; p < platforms; p++) rows.push(t + p * side);
	foreach (k, main in rows) {
		local end_m = forward ? main + (len - 1) * step : main;
		local g = {end = end_m, exit = end_m + out, throat = null, backs = []};
		if (platforms > 1) {
			/* Two platforms: the other row lies sd beside the main one. */
			local sec = rows[platforms - 1 - k];
			/* Double track: the second platform lies on the lane side, where the second track leaves. */
			if (lane != 0 && sec - main != lane) continue;
			local end_s = end_m + (sec - main);
			g.throat = {end_m = end_m, end_s = end_s, t1m = end_m + out, t1s = end_s + out,
				m = end_m + 2 * out, m2 = end_s + 2 * out, l0 = end_m + 3 * out};
			g.end = g.throat.m;
			g.exit = g.throat.l0;
		}
		if (lane != 0) {
			g.dbl <- PrismaticAI.DoubleTerminus(g.throat, out, lane);
			g.end = g.dbl.a1;
			g.exit = g.dbl.a2;
			/* Both tracks leave straight, and the second must lead on as well as the first. */
			if (PrismaticAI.RailStepCost(g.dbl.b2) < 0 || !PrismaticAI.ExitLeadsOn(g.dbl.b2, out, true)) continue;
		}
		if (PrismaticAI.RailStepCost(g.exit) < 0) continue;
		/* The exit must lead on: a station between houses whose exit faces a road junction is a dead
		 * end the line can never leave (OpenTTD seed 20208, Ganingbury). */
		if (!PrismaticAI.ExitLeadsOn(g.exit, out, platforms > 1)) continue;
		foreach (row in rows) {
			local far = forward ? row : row + (len - 1) * step;
			g.backs.push([far - out, far]);
		}
		return g;
	}
	return null;
}

/* Lay a terminus throat (TerminusGeometry): both platforms straight out onto T1m and T1s, a path
 * signal on each facing away from the platforms, the second platform bent onto the main row at M.
 * Every tile laid is added to `built` (the throat's own track). Returns false when the game
 * refuses a piece. */
function PrismaticAI::BuildThroat(g, built) {
	local th = g.throat;
	local pieces = [[th.end_m, th.t1m, th.m], [th.t1m, th.m, th.l0], [th.end_s, th.t1s, th.m2],
		[th.t1s, th.m2, th.m], [th.m2, th.m, th.l0]];
	foreach (pc in pieces) {
		if (!AIRail.BuildRail(pc[0], pc[1], pc[2])) return false;
		built.push(pc[1]);
	}
	if ("dbl" in g) {
		local d = g.dbl;
		/* A1 takes two pieces; each tile is recorded once, as soon as it holds track. */
		foreach (i, pc in [[d.m, d.l0, d.a1], [d.l0, d.a1, d.a2], [d.l0, d.a1, d.s1], [d.a1, d.s1, d.b2]]) {
			if (!AIRail.BuildRail(pc[0], pc[1], pc[2])) return false;
			if (i != 2) built.push(pc[1]);
		}
		AILog.Info("PrismaticAI: built the double-track junction at tile " + d.a1 + ", tracks leave at tiles " + d.a2 + " and " + d.b2);
	}
	/* AIRail.BuildSignal(tile, front) signals the trains that come from `front` (script_rail.cpp
	 * _possible_trackdirs): here those entering a platform. On a double track they face the trains
	 * leaving instead, so a train arriving at full platforms waits on its own track, at the last
	 * one-way signal before the junction, and never on M, the tile every departure needs. */
	local dbl = "dbl" in g;
	if (!AIRail.BuildSignal(th.t1m, dbl ? th.end_m : th.m, AIRail.SIGNALTYPE_PBS)) return false;
	if (!AIRail.BuildSignal(th.t1s, dbl ? th.end_s : th.m2, AIRail.SIGNALTYPE_PBS)) return false;
	AILog.Info("PrismaticAI: built the throat of two platforms at tile " + th.m + ", path signals at tiles " + th.t1m + " and " + th.t1s);
	return true;
}

/* The rail bridge for a span from head `a` to head `b`: the cheapest one faster than 100 km/h, else
 * the fastest there is; -1 when none is that long. */
function PrismaticAI::BridgeType(a, b) {
	local len = AIMap.DistanceManhattan(a, b) + 1;
	local list = AIBridgeList_Length(len);
	if (list.IsEmpty()) return -1;
	list.Valuate(AIBridge.GetMaxSpeed);
	local fast = AIList();
	fast.AddList(list);
	fast.KeepAboveValue(100);
	if (!fast.IsEmpty()) {
		fast.Valuate(AIBridge.GetPrice, len);
		fast.Sort(AIList.SORT_BY_VALUE, AIList.SORT_ASCENDING);
		return fast.Begin();
	}
	list.Sort(AIList.SORT_BY_VALUE, AIList.SORT_DESCENDING);
	return list.Begin();
}

/* Lays `path` piece by piece between the station end tiles `a_end` and `b_end`. Returns the tiles
 * actually laid, or null. A piece the game refuses (a house, a rival's track or a vehicle took the
 * tile since the search, ERR_AREA_NOT_CLEAR) is not the end of the line: from the last laid tile the
 * search runs again around the refused one, the last piece is relaid toward the new route, and the
 * rest follows (at most `replans` times). On seed 31337 a 212-tile coal line was dropped this way. */
function PrismaticAI::BuildRailAlong(path, a_end, b_end, replans = 2, to_straight = false, from_straight = false) {
	local n = path.len();
	local corners = 0;
	for (local i = 0; i < n; i++) {
		/* Two portals in a row that are not neighbours and a tunnel joins: the tunnel FindRailPath
		 * planned (the game digs from the first to the far end it finds itself). */
		if (i + 1 < n && AIMap.DistanceManhattan(path[i], path[i + 1]) > 1 &&
				AITile.GetSlope(path[i]) != AITile.SLOPE_FLAT && AITunnel.GetOtherTunnelEnd(path[i]) == path[i + 1]) {
			if (!AITunnel.BuildTunnel(AIVehicle.VT_RAIL, path[i])) {
				AILog.Warning("PrismaticAI: rail tunnel from tile " + path[i] + " to " + path[i + 1] + " failed: " + AIError.GetLastErrorString());
				return null;
			}
			AILog.Info("PrismaticAI: built rail tunnel from tile " + path[i] + " to " + path[i + 1]);
			i++;
			continue;
		}
		/* Two heads in a row that are not neighbours: the bridge FindRailPath planned between them. */
		if (i + 1 < n && AIMap.DistanceManhattan(path[i], path[i + 1]) > 1) {
			local bridge = PrismaticAI.BridgeType(path[i], path[i + 1]);
			if (bridge == -1 || !AIBridge.BuildBridge(AIVehicle.VT_RAIL, bridge, path[i], path[i + 1])) {
				AILog.Warning("PrismaticAI: rail bridge from tile " + path[i] + " to " + path[i + 1] + " failed: " + AIError.GetLastErrorString());
				return null;
			}
			AILog.Info("PrismaticAI: built rail bridge " + AIBridge.GetName(bridge, AIVehicle.VT_RAIL) + " from tile " + path[i] + " to " + path[i + 1]);
			i++;
			continue;
		}
		local prev = (i == 0) ? a_end : path[i - 1];
		local next = (i == n - 1) ? b_end : path[i + 1];
		if (next - path[i] != path[i] - prev) corners++;
		if (!AIRail.BuildRail(prev, path[i], next)) {
			AILog.Warning("PrismaticAI: rail at tile " + path[i] + " failed: " + AIError.GetLastErrorString());
			if (replans <= 0 || i < 1) return null;
			local f = path[i - 1], e = (i == 1) ? a_end : path[i - 2], g = path[i];
			local avoid = {};
			avoid[g] <- 1;
			/* The move that arrived at e, so the new route's first corner is checked against e's
			 * (Turns90): the old search started as if out of a straight platform. */
			local from_in = null;
			if (i >= 3) from_in = PrismaticAI.UnitStep(path[i - 3], path[i - 2]);
			else if (i == 2) from_in = PrismaticAI.UnitStep(a_end, path[0]);
			/* Re-planning from the throat's own exit tile (f is path[0]) keeps it straight too. */
			local rest = PrismaticAI.FindRailPath(f, path[n - 1], e, b_end, avoid, from_in, from_straight && i == 1, to_straight);
			if (rest == null || rest.len() < 2) return null;
			/* The piece on f still points at g; it is relaid toward the new route's first step. */
			AIRail.RemoveRail(e, f, g);
			AILog.Info("PrismaticAI: re-planned from tile " + f + " around tile " + g + ", " + rest.len() + " tiles to go");
			local tail = PrismaticAI.BuildRailAlong(rest, e, b_end, replans - 1, to_straight, from_straight && i == 1);
			if (tail == null) return null;
			local laid = path.slice(0, i - 1);
			laid.extend(tail);
			return laid;
		}
		AILog.Info("PrismaticAI: built rail at tile " + path[i]);
		if (i % 4 == 0) AIController.Sleep(ROAD_PACE);
	}
	AILog.Info("PrismaticAI: line laid, " + n + " tiles, " + corners + " corner pieces (diagonal track and turns)");
	return path;
}

/* The unit move from tile a toward tile b along one axis (a bridge or tunnel jump counts as its
 * direction): +-1 or +-map width. */
function PrismaticAI::UnitStep(a, b) {
	local k = max(1, AIMap.DistanceManhattan(a, b));
	return (b - a) / k;
}

/* Walk a laid line from station end `a_end` through `path` to `b_end` and check that every plain
 * piece joins its neighbours (AIRail.AreTilesConnected, the game's own track bits); bridge and
 * tunnel heads stand as the game built them. Returns -1 when the line is whole, else the first tile
 * where it is broken. A broken line is one where trains get lost: found here, the missing piece is
 * laid once more, and a line that stays broken gets no train. */
function PrismaticAI::VerifyRailLine(path, a_end, b_end) {
	local n = path.len();
	for (local i = 0; i < n; i++) {
		local prev = (i == 0) ? a_end : path[i - 1];
		local next = (i == n - 1) ? b_end : path[i + 1];
		if (AIMap.DistanceManhattan(prev, path[i]) != 1 || AIMap.DistanceManhattan(path[i], next) != 1) continue;
		if (!AIRail.IsRailTile(path[i])) continue; /* a bridge or tunnel head */
		if (AIRail.AreTilesConnected(prev, path[i], next)) continue;
		if (AIRail.BuildRail(prev, path[i], next) && AIRail.AreTilesConnected(prev, path[i], next)) {
			AILog.Info("PrismaticAI: relaid the missing piece at tile " + path[i]);
			continue;
		}
		return path[i];
	}
	return -1;
}

/* A depot behind the dead end of one of a station's platforms (`st.backs`, [depot tile, platform
 * tile] pairs from BuildRailStationAt): trains reach it through their own platform, turn in it, and
 * leave inside the station's signals, so a depot never lets a train onto the single track past
 * them. Returns its tile or null. */
function PrismaticAI::BuildRailDepotBehind(st) {
	if (!("backs" in st)) return null;
	foreach (b in st.backs) {
		if (PrismaticAI.RailStepCost(b[0]) < 0) continue;
		if (AIRail.BuildRailDepot(b[0], b[1])) {
			AILog.Info("PrismaticAI: built rail depot at tile " + b[0] + " behind a platform");
			return b[0];
		}
	}
	return null;
}

/* A depot beside the line near its start, joined to the line both ways, only where the line runs
 * straight: on a corner the two joining pieces meet the line's own corner at 90 degrees, and with
 * forbid_90_deg a train leaving the depot could take neither way. Returns its tile or null. */
function PrismaticAI::BuildRailDepotBeside(path) {
	local map_w = AIMap.GetMapSizeX();
	for (local k = 1; k < min(path.len() - 1, 12); k++) {
		local p = path[k];
		if (AIMap.DistanceManhattan(path[k - 1], p) != 1 || AIMap.DistanceManhattan(p, path[k + 1]) != 1) continue;
		if (p - path[k - 1] != path[k + 1] - p) continue; /* not straight here */
		foreach (cand in [p - 1, p + 1, p - map_w, p + map_w]) {
			if (cand == path[k - 1] || cand == path[k + 1] || PrismaticAI.RailStepCost(cand) < 0) continue;
			if (!AIRail.BuildRailDepot(cand, p)) continue;
			if (AIRail.BuildRail(cand, p, path[k + 1]) && AIRail.BuildRail(cand, p, path[k - 1])) {
				AILog.Info("PrismaticAI: built rail depot at tile " + cand);
				return cand;
			}
			AITile.DemolishTile(cand);
		}
	}
	return null;
}

/* Removes what a station record built: its own platforms (AIRail.RemoveRailStationTileRectangle,
 * never DemolishTile, which would take every platform of a joined station) and its throat. A record
 * without `platform_rect` (from before the throat) demolishes its platform tiles: the step along the platform comes from the
 * record, since derived from end - tile it was 0 for a station facing the other way and the cleanup
 * after a failed line demolished one tile of three (OpenTTD seed 31337, 2026-11-17). */
function PrismaticAI::RemoveRailStation(st, len) {
	if ("platform_rect" in st) {
		/* Only this station's own platforms (keep_rail false: their track goes too), never the
		 * joined station's others, then the throat's plain track. */
		AIRail.RemoveRailStationTileRectangle(st.platform_rect[0], st.platform_rect[1], false);
		foreach (t in st.throat) AITile.DemolishTile(t);
		return;
	}
	local step = ("step" in st) ? st.step : (st.end - st.tile) / max(1, len - 1);
	for (local i = 0; i < len; i++) AITile.DemolishTile(st.tile + i * step);
}

/* The line between two termini, its depot and one train for `cargo`. The first stop loads with
 * `first_flags` (full load at a mine; none for passengers). Returns 1 when a train runs, else 0. A
 * finished line logs its record (lines.nut LineRecord): its geometry for the Rhai steward, which
 * sets trains up to trains_max and wagons up to wagons_fit. */
function PrismaticAI::BuildRailLineAndTrain(sa, sb, len, cargo, wagons, first_flags, label) {
	AILog.Info("PrismaticAI: survey rail line from tile " + sa.exit + " to tile " + sb.exit);
	local throat_a = ("platforms" in sa) && sa.platforms > 1, throat_b = ("platforms" in sb) && sb.platforms > 1;
	local path = PrismaticAI.FindRailPath(sa.exit, sb.exit, sa.end, sb.end, null, null, throat_a, throat_b);
	if (path == null) {
		AILog.Warning("PrismaticAI: no rail path between the stations; removing them");
		PrismaticAI.RemoveRailStation(sa, len); PrismaticAI.RemoveRailStation(sb, len);
		return 0;
	}
	/* What was actually laid: a re-plan around a refused piece changes the route. */
	local laid = PrismaticAI.BuildRailAlong(path, sa.end, sb.end, 2, throat_b, throat_a);
	if (laid == null) return 0;
	local broken = PrismaticAI.VerifyRailLine(laid, sa.end, sb.end);
	if (broken != -1) {
		AILog.Warning("PrismaticAI: rail line " + label + " is broken at tile " + broken + "; no train on a line it cannot run");
		return 0;
	}
	AILog.Info("PrismaticAI: rail line " + label + " done, " + laid.len() + " tiles");
	local depot = PrismaticAI.BuildRailDepotBehind(sa);
	if (depot == null) depot = PrismaticAI.BuildRailDepotBehind(sb);
	if (depot == null) depot = PrismaticAI.BuildRailDepotBeside(laid);
	if (depot == null) { AILog.Warning("PrismaticAI: no rail depot behind a platform or beside the line"); return 0; }
	local platforms = min(("platforms" in sa) ? sa.platforms : 1, ("platforms" in sb) ? sb.platforms : 1);
	/* Everything the line owns (its stations' own platforms and throats, its track, its depot), so
	 * closing it (lines.nut CloseLine) clears it to the last tile and nothing of another line. */
	local line = {a = sa.tile, b = sb.tile, depot = depot, cargo = cargo, wagons = wagons, flags = first_flags,
		label = label, railtype = AIRail.GetCurrentRailType(), restored = 0, len = len, trains_max = platforms,
		path = laid, stations = [sa, sb], closed = false};
	local train = PrismaticAI.BuildTrain(line);
	if (train == -1) return 0;
	RAIL_LINES.push(line);
	AILog.Info("PrismaticAI: built a rail route " + label + " with a train of " + AIVehicle.GetNumWagons(train) + " (" + AIEngine.GetName(AIVehicle.GetEngineType(train)) + ")");
	AILog.Info(PrismaticAI.LineRecord(RAIL_LINES.len(), line));
	return 1;
}

/*
 * Double track (RESEARCH-ONLY, opt-in: setting rail_double=1, the bot's own freight lines only).
 * After AAAHogEx, whose rail earns 2-3M a year by 1970 while a single-track line here holds two
 * trains: one track each way, one-way path signals every RAIL_SIGNAL_SPACING tiles, so a line holds
 * as many trains as it has signal blocks and the in-bot steward adds them while cargo waits.
 *
 * Each terminus is the two-platform throat (BuildThroat) and two more tiles, L0 and the junction A1,
 * where the two tracks part (`lane` is the side the second track leaves on):
 *
 *     platform main | T1m  M   L0  A1  A2 -> track A
 *     platform 2nd  | T1s  M2          S1  B2 -> track B
 *
 * A1 carries a straight piece (track A) and a corner toward S1, S1 a corner back onto the axis. L0 is
 * straight, so neither platform's route turns 90 degrees into either track (Turns90: M2 to M, then
 * two straight moves, then A1 to S1). Seen from the source, the second track leaves on one side,
 * and at the far end on the same side of the map (hand +1 and -1 in BuildRailStationAt), so track A
 * runs A2 to the far A2 and track B B2 to the far B2 without crossing.
 *
 * Track A carries the trains from the source, track B the trains back: one-way path signals facing
 * that way (PlaceLaneSignals) make the other way a dead end to the pathfinder. A train leaves its
 * platform at the throat's path signal, which faces departures on a double track (BuildThroat),
 * shares M-L0-A1 with arriving trains only, and an arriving train waits at the last one-way signal
 * before the junction until it can reserve its way into a free platform. Both tracks come from the same A*
 * search (FindRailPath); the second runs with the first's tiles left out, and both are walked
 * (VerifyRailLine, VerifyJunction) before a train is bought. When the second track cannot be found,
 * laid or walked, the junction's spur is taken out again and the line runs single track with
 * trains_max = platforms, as rail_double=0 builds it.
 */

/* Tiles between the one-way path signals of a double track: one block holds a train as long as
 * the longest the game allows by default (vehicle.max_train_length 7). */
const RAIL_SIGNAL_SPACING = 7;

/* The move a quarter turn from `d` (+-1 along x, +-map width along y), the same way for every d. */
function PrismaticAI::QuarterTurn(d) {
	local map_w = AIMap.GetMapSizeX();
	if (d == 1) return map_w;
	if (d == map_w) return -1;
	if (d == -1) return -map_w;
	return 1;
}

/* The junction tiles of a double-track terminus from its throat (TerminusGeometry), the move `out`
 * toward the other end and the side `lane` the second track leaves on (see above). */
function PrismaticAI::DoubleTerminus(th, out, lane) {
	local a1 = th.l0 + out, s1 = a1 + lane;
	return {m = th.m, l0 = th.l0, a1 = a1, a2 = a1 + out, s1 = s1, b2 = s1 + out, out = out, lane = lane};
}

/* The pieces of a junction (`d`, DoubleTerminus): the first two carry track A, the last two join
 * track B. Returns the first tile whose piece is missing, or -1. */
function PrismaticAI::VerifyJunction(d, track_b) {
	local pieces = [[d.m, d.l0, d.a1], [d.l0, d.a1, d.a2]];
	if (track_b) pieces.extend([[d.l0, d.a1, d.s1], [d.a1, d.s1, d.b2]]);
	foreach (pc in pieces) {
		if (!AIRail.AreTilesConnected(pc[0], pc[1], pc[2])) return pc[1];
	}
	return -1;
}

/* One-way path signals along a laid track `path` (between the junction tiles `a_end` and `b_end`)
 * for trains that run from path[0] toward its end (`forward`) or back: on the first tile they enter,
 * then every RAIL_SIGNAL_SPACING tiles, and on the last tile before the far junction, so a train
 * waits for a platform there and never on the junction. A tile that takes no signal (a bridge or
 * tunnel head, a level crossing) passes the signal on to the next one. Returns the signals built. */
function PrismaticAI::PlaceLaneSignals(path, a_end, b_end, forward) {
	local n = path.len(), placed = 0, last = -RAIL_SIGNAL_SPACING;
	for (local k = 0; k < n; k++) {
		if (k - last < RAIL_SIGNAL_SPACING && k != n - 1) continue;
		local i = forward ? k : n - 1 - k;
		local p = path[i];
		local front = forward ? ((i == n - 1) ? b_end : path[i + 1]) : ((i == 0) ? a_end : path[i - 1]);
		local back = forward ? ((i == 0) ? a_end : path[i - 1]) : ((i == n - 1) ? b_end : path[i + 1]);
		if (AIMap.DistanceManhattan(p, front) != 1 || AIMap.DistanceManhattan(p, back) != 1) continue;
		if (!AIRail.IsRailTile(p) || AIRail.IsLevelCrossingTile(p)) continue;
		/* The tile the trains come from: BuildSignal signals trains arriving from its second tile. */
		if (AIRail.BuildSignal(p, back, AIRail.SIGNALTYPE_PBS_ONEWAY)) {
			placed++;
			last = k;
		}
	}
	return placed;
}

/* Take out a junction's spur onto track B (A1 to S1 and S1 itself): the line runs single track. */
function PrismaticAI::RemoveDoubleSpur(d) {
	AIRail.RemoveRail(d.l0, d.a1, d.s1);
	AIRail.RemoveRail(d.a1, d.s1, d.b2);
}

/* The double-track line between two termini built with a junction (BuildRailStationAt with hand),
 * its depot and one train as long as the platform; the steward adds the rest (steward.nut). Returns
 * 1 when a train runs, else 0. The line's record says trains_max: the platforms plus the signal
 * blocks of the shorter track, half of what both tracks and all four platforms hold, so a full
 * line still has room to move (a train waits for a platform on its own track, never on the other). */
function PrismaticAI::BuildDoubleRailLineAndTrain(sa, sb, len, cargo, first_flags, label) {
	local da = sa.dbl, db = sb.dbl;
	AILog.Info("PrismaticAI: survey double-track rail line from tile " + sa.exit + " to tile " + sb.exit);
	/* Track A keeps clear of where track B leaves each junction. */
	local avoid = {};
	foreach (d in [da, db]) {
		for (local k = 0; k < 3; k++) avoid[d.b2 + k * d.out] <- 1;
	}
	local path = PrismaticAI.FindRailPath(sa.exit, sb.exit, sa.end, sb.end, avoid, null, true, true);
	if (path == null) {
		AILog.Warning("PrismaticAI: no rail path between the stations; removing them");
		PrismaticAI.RemoveRailStation(sa, len); PrismaticAI.RemoveRailStation(sb, len);
		/* -1: nothing of the line is left, so the caller may try it single track (straight exits
		 * and the second track's reserved tiles are what a hilly map most often refuses). */
		return -1;
	}
	local acc_a = AIAccounting();
	local laid = PrismaticAI.BuildRailAlong(path, sa.end, sb.end, 2, true, true);
	local cost_a = acc_a.GetCosts();
	if (laid == null) return 0;
	local broken = PrismaticAI.VerifyRailLine(laid, sa.end, sb.end);
	if (broken == -1) broken = PrismaticAI.VerifyJunction(da, false);
	if (broken == -1) broken = PrismaticAI.VerifyJunction(db, false);
	if (broken != -1) {
		AILog.Warning("PrismaticAI: rail line " + label + " is broken at tile " + broken + "; no train on a line it cannot run");
		return 0;
	}
	/* Track B only when, after it, the money still buys the train: track B costs about what track A
	 * did. On OpenTTD seed 1516196607 (company 4) both tracks took the whole loan, the locomotive
	 * was refused for ERR_NOT_ENOUGH_CASH, and the company went bankrupt in 1961 with two buses. */
	local engines = PrismaticAI.PickRailEngines(AIRail.GetCurrentRailType(), cargo);
	local train_price = (engines.loco == -1) ? 0 : AIEngine.GetPrice(engines.loco) +
		(engines.wagon == -1 ? 0 : 2 * len * AIEngine.GetPrice(engines.wagon));
	local funds = AICompany.GetBankBalance(AICompany.COMPANY_SELF) + AICompany.GetMaxLoanAmount() - AICompany.GetLoanAmount();
	local afford_b = funds >= cost_a + train_price;
	if (!afford_b) AILog.Warning("PrismaticAI: no second track for " + label + ": it would cost about " + cost_a +
		" and leave less than the train's " + train_price + " (funds " + funds + ")");
	local taken = {};
	foreach (t in laid) taken[t] <- 1;
	local laid2 = null;
	local path2 = afford_b ? PrismaticAI.FindRailPath(da.b2, db.b2, da.s1, db.s1, taken, da.lane, true, true) : null;
	if (path2 != null) laid2 = PrismaticAI.BuildRailAlong(path2, da.s1, db.s1, 2, true, true);
	if (laid2 != null) {
		broken = PrismaticAI.VerifyRailLine(laid2, da.s1, db.s1);
		if (broken == -1) broken = PrismaticAI.VerifyJunction(da, true);
		if (broken == -1) broken = PrismaticAI.VerifyJunction(db, true);
		if (broken != -1) {
			AILog.Warning("PrismaticAI: second track of " + label + " is broken at tile " + broken);
			laid2 = null;
		}
	}
	local platforms = min(sa.platforms, sb.platforms);
	local trains_max = platforms;
	if (laid2 != null) {
		local sig_a = PrismaticAI.PlaceLaneSignals(laid, sa.end, sb.end, true);
		local sig_b = PrismaticAI.PlaceLaneSignals(laid2, da.s1, db.s1, false);
		if (sig_a == 0 || sig_b == 0) {
			AILog.Warning("PrismaticAI: no one-way signal on a track of " + label + " (" + sig_a + ", " + sig_b + "); no train on it");
			return 0;
		}
		trains_max = platforms + min(sig_a, sig_b);
		AILog.Info("PrismaticAI: double track " + label + ": " + laid.len() + " and " + laid2.len() + " tiles, " + sig_a + " and " +
			sig_b + " one-way path signals, trains_max " + trains_max);
	} else {
		PrismaticAI.RemoveDoubleSpur(da);
		PrismaticAI.RemoveDoubleSpur(db);
		AILog.Warning("PrismaticAI: no second track for " + label + "; it runs single track, " + platforms + " trains");
	}
	AILog.Info("PrismaticAI: rail line " + label + " done, " + laid.len() + " tiles");
	local depot = PrismaticAI.BuildRailDepotBehind(sa);
	if (depot == null) depot = PrismaticAI.BuildRailDepotBehind(sb);
	if (depot == null) depot = PrismaticAI.BuildRailDepotBeside(laid);
	if (depot == null) { AILog.Warning("PrismaticAI: no rail depot behind a platform or beside the line"); return 0; }
	/* wagons 0: every train as long as the platform. */
	local line = {a = sa.tile, b = sb.tile, depot = depot, cargo = cargo, wagons = 0, flags = first_flags,
		label = label, railtype = AIRail.GetCurrentRailType(), restored = 0, len = len, trains_max = trains_max,
		path = laid, stations = [sa, sb], closed = false, platforms = platforms};
	if (laid2 != null) line.path2 <- laid2;
	local train = PrismaticAI.BuildTrain(line);
	if (train == -1) return 0;
	RAIL_LINES.push(line);
	AILog.Info("PrismaticAI: built a rail route " + label + " with a train of " + AIVehicle.GetNumWagons(train) + " (" + AIEngine.GetName(AIVehicle.GetEngineType(train)) + ")");
	AILog.Info(PrismaticAI.LineRecord(RAIL_LINES.len(), line));
	return 1;
}

/* The engines a line runs: the fastest buildable locomotive for `railtype` (ties: the more powerful)
 * and the wagon of the largest capacity that carries or refits to `cargo`. {loco, wagon}, -1 where
 * there is none. */
function PrismaticAI::PickRailEngines(railtype, cargo) {
	local loco = -1, wagon = -1;
	local engines = AIEngineList(AIVehicle.VT_RAIL);
	for (local e = engines.Begin(); !engines.IsEnd(); e = engines.Next()) {
		if (!AIEngine.IsBuildable(e) || !AIEngine.CanRunOnRail(e, railtype)) continue;
		if (AIEngine.IsWagon(e)) {
			if ((AIEngine.GetCargoType(e) == cargo || AIEngine.CanRefitCargo(e, cargo)) &&
					(wagon == -1 || AIEngine.GetCapacity(e) > AIEngine.GetCapacity(wagon))) wagon = e;
		} else if (loco == -1 || AIEngine.GetMaxSpeed(e) > AIEngine.GetMaxSpeed(loco) ||
				(AIEngine.GetMaxSpeed(e) == AIEngine.GetMaxSpeed(loco) && AIEngine.GetPower(e) > AIEngine.GetPower(loco))) {
			loco = e;
		}
	}
	return {loco = loco, wagon = wagon};
}

/* A train for `line` ({a, b, depot, cargo, wagons, flags, railtype, len}): PickRailEngines' engines,
 * `wagons` wagons (0: as many as fit), never longer than the platforms (`len` tiles, sixteen
 * sixteenths a tile, AIVehicle.GetLength), orders to both stations (the first with the line's load
 * flags), started. Returns the train or -1. */
function PrismaticAI::BuildTrain(line) {
	local e = PrismaticAI.PickRailEngines(line.railtype, line.cargo);
	if (e.loco == -1 || e.wagon == -1) { AILog.Warning("PrismaticAI: no locomotive or wagon for " + AICargo.GetCargoLabel(line.cargo)); return -1; }
	local train = AIVehicle.BuildVehicle(line.depot, e.loco);
	if (!AIVehicle.IsValidVehicle(train)) { AILog.Warning("PrismaticAI: locomotive not bought: " + AIError.GetLastErrorString()); return -1; }
	local max_len = (("len" in line) ? line.len : 3) * 16;
	for (local i = 0; line.wagons <= 0 || i < line.wagons; i++) {
		if (!PrismaticAI.AddWagon(train, line.depot, e.wagon, line.cargo, max_len)) break;
	}
	/* A train that carries nothing earns nothing: a NewGRF may refuse every wagon or refit. */
	if (AIVehicle.GetCapacity(train, line.cargo) <= 0) {
		AILog.Warning("PrismaticAI: no wagon of " + AIEngine.GetName(e.wagon) + " would carry " + AICargo.GetCargoLabel(line.cargo) + " behind " + AIEngine.GetName(e.loco) + "; the locomotive is sold");
		AIVehicle.SellVehicle(train);
		return -1;
	}
	AIOrder.AppendOrder(train, line.a, line.flags);
	AIOrder.AppendOrder(train, line.b, AIOrder.OF_NONE);
	AIVehicle.StartStopVehicle(train);
	return train;
}

/* Buy one `wagon` in `depot`, refit it to `cargo` and couple it behind `train`, keeping the train
 * within `max_len` sixteenths of a tile. A wagon the game will not refit or couple (a NewGRF rule)
 * is sold again, never left in the depot. Returns true when the train has one more wagon. */
function PrismaticAI::AddWagon(train, depot, wagon, cargo, max_len) {
	local w = AIVehicle.BuildVehicle(depot, wagon);
	if (!AIVehicle.IsValidVehicle(w)) return false;
	if (AIEngine.GetCargoType(wagon) != cargo && !AIVehicle.RefitVehicle(w, cargo)) { AIVehicle.SellVehicle(w); return false; }
	if (!AIVehicle.MoveWagon(w, 0, train, AIVehicle.GetNumWagons(train) - 1)) { AIVehicle.SellVehicle(w); return false; }
	if (AIVehicle.GetLength(train) > max_len) {
		/* One too many for the platform: a train longer than its platform blocks the throat. */
		AIVehicle.SellWagon(train, AIVehicle.GetNumWagons(train) - 1);
		return false;
	}
	return true;
}

/* Platform length and wagons for a line of `dist` tiles carrying `supply` units of `cargo` a month,
 * on `platforms` platforms at each end. Every number is derived:
 * - the engines PickRailEngines would buy, their speed (0.036 tiles a day per km/h, .ai knowledge
 *   cargo-payment-and-vehicle-speed) and the wagon's capacity;
 * - the round trip, both ways plus a week at the stations;
 * - the trains the line may run (trains_max = platforms) and the load each must take per trip to
 *   move the month's supply: wagons = ceil(supply * trip_days / 30 / trains / capacity);
 * - the platform: the locomotive and those wagons at half a tile each (the default vehicle length;
 *   BuildTrain checks the real one), at least 2 tiles, at most the game's vehicle.max_train_length
 *   and station.station_spread.
 * `override_len` and `override_wagons` (the settings rail_len, rail_wagons) win when above 0.
 * Returns {len, wagons, trip_days, why}. */
function PrismaticAI::PlatformPlan(cargo, supply, dist, platforms, override_len, override_wagons) {
	local e = PrismaticAI.PickRailEngines(AIRail.GetCurrentRailType(), cargo);
	local kmh = (e.loco == -1) ? 100 : max(1, AIEngine.GetMaxSpeed(e.loco));
	local cap = (e.wagon == -1) ? 30 : max(1, AIEngine.GetCapacity(e.wagon));
	local trip_days = (2.0 * dist / (kmh * 0.036)).tointeger() + 7;
	local cap_len = 7;
	if (AIGameSettings.IsValid("vehicle.max_train_length")) cap_len = AIGameSettings.GetValue("vehicle.max_train_length");
	if (AIGameSettings.IsValid("station.station_spread")) cap_len = min(cap_len, AIGameSettings.GetValue("station.station_spread"));
	local trains = max(1, platforms);
	local need = (supply * trip_days + 30 * trains * cap - 1) / (30 * trains * cap);
	local wagons = max(1, need);
	local len = max(2, min(cap_len, (1 + wagons + 1) / 2));
	/* Never more wagons than the platform takes at half a tile each. */
	wagons = min(wagons, 2 * len - 1);
	if (override_len > 0) len = min(cap_len, override_len);
	if (override_wagons > 0) wagons = override_wagons;
	local why = "supply " + supply + "/month, " + dist + " tiles, trip " + trip_days + " days at " + kmh + " km/h, " + trains +
		" train(s) of " + wagons + " x " + cap + " -> platform " + len + " tiles (max " + cap_len + ")";
	return {len = len, wagons = (override_wagons > 0) ? override_wagons : 0, wagons_planned = wagons, trip_days = trip_days, why = why};
}

/* Every rail line the bot built keeps a train: one that was lost (a crash at a level crossing, a
 * sale) is replaced in the line's depot, at most twice per line, so a line that keeps losing its
 * train for money reasons is not rebuilt forever. Old trains are renewed by the company's own
 * autorenew (main.nut). */
function PrismaticAI::KeepLinesServed(reserve) {
	foreach (i, line in RAIL_LINES) {
		if (line.restored >= 2 || line.closed || PrismaticAI.LineTarget(i + 1, "trains") == 0) continue;
		/* A train of this line: a rail vehicle with orders to both of its stations (two lines may
		 * share a station since a second platform joins it). */
		local served = false;
		local st_b = AIStation.GetStationID(line.b);
		local list = AIVehicleList_Station(AIStation.GetStationID(line.a));
		for (local v = list.Begin(); !list.IsEnd() && !served; v = list.Next()) {
			if (AIVehicle.GetVehicleType(v) != AIVehicle.VT_RAIL) continue;
			for (local i = 0; i < AIOrder.GetOrderCount(v); i++) {
				if (AIStation.GetStationID(AIOrder.GetOrderDestination(v, i)) == st_b) { served = true; break; }
			}
		}
		if (served) continue;
		if (AICompany.GetBankBalance(AICompany.COMPANY_SELF) < reserve) return;
		local train = PrismaticAI.BuildTrain(line);
		if (train == -1) continue;
		line.restored++;
		AILog.Info("PrismaticAI: line " + line.label + " had no train; a new one runs (" + AIEngine.GetName(AIVehicle.GetEngineType(train)) +
			", replacement " + line.restored + " of 2)");
	}
}

function PrismaticAI::UseFirstRailType() {
	local types = AIRailTypeList();
	if (types.IsEmpty()) { AILog.Warning("PrismaticAI: no rail type available"); return false; }
	AIRail.SetCurrentRailType(types.Begin());
	return true;
}

/* The rail settings a route is built with, from Behaviour(): platforms per terminus and the
 * overrides of the derived platform length and wagons (0: derive). */
function PrismaticAI::RailShape(b, own = false) {
	/* rail_double applies to the bot's own freight lines only (main.nut passes own = true there). */
	return {platforms = max(1, b.rail_platforms), len = b.rail_len, wagons = b.rail_wagons,
		double = own && b.rail_double == 1};
}

function PrismaticAI::BuildRailRoute(a, b, shape) {
	if (!AITown.IsValidTown(a) || !AITown.IsValidTown(b) || a == b) {
		AILog.Warning("PrismaticAI: rail route needs two different towns (rail_a=" + a + ", rail_b=" + b + ")");
		return 0;
	}
	if (!PrismaticAI.UseFirstRailType()) return 0;
	local pax = PrismaticAI.PassengerCargo();
	local dist = AIMap.DistanceManhattan(AITown.GetLocation(a), AITown.GetLocation(b));
	AILog.Info("PrismaticAI: planner pair by rail: " + AITown.GetName(a) + " - " + AITown.GetName(b) + " (" + dist + " tiles)");
	/* A station near the centre catches about a third of what a town produces; the smaller town
	 * bounds what flows both ways. */
	local supply = min(AITown.GetLastMonthProduction(a, pax), AITown.GetLastMonthProduction(b, pax)) / 3;
	local plan = PrismaticAI.PlatformPlan(pax, supply, dist, shape.platforms, shape.len, shape.wagons);
	AILog.Info("PrismaticAI: rail plan " + AITown.GetName(a) + " - " + AITown.GetName(b) + ": " + plan.why);
	local sa = PrismaticAI.BuildRailStationAt(AITown.GetLocation(a), AITown.GetLocation(b), plan.len, pax, "accept",
		"in " + AITown.GetName(a) + " (axis toward " + AITown.GetName(b) + ")", shape.platforms);
	if (sa == null) return 0;
	local sb = PrismaticAI.BuildRailStationAt(AITown.GetLocation(b), AITown.GetLocation(a), plan.len, pax, "accept",
		"in " + AITown.GetName(b) + " (axis toward " + AITown.GetName(a) + ")", shape.platforms);
	if (sb == null) { PrismaticAI.RemoveRailStation(sa, plan.len); return 0; }
	return PrismaticAI.BuildRailLineAndTrain(sa, sb, plan.len, pax, plan.wagons, AIOrder.OF_NONE, AITown.GetName(a) + " - " + AITown.GetName(b));
}

/* Freight by rail: from the producing industry `p` to the accepting industry `q` for `cargo`, the
 * planner's portfolio pair (settings fr_from, fr_to, fr_cargo) or the bot's own. The train waits
 * for a full load at the producer. */
function PrismaticAI::BuildFreightRailRoute(p, q, cargo, shape) {
	if (!AIIndustry.IsValidIndustry(p) || !AIIndustry.IsValidIndustry(q) || p == q || !AICargo.IsValidCargo(cargo)) {
		AILog.Warning("PrismaticAI: freight route needs two industries and a cargo (fr_from=" + p + ", fr_to=" + q + ", fr_cargo=" + cargo + ")");
		return 0;
	}
	if (!PrismaticAI.UseFirstRailType()) return 0;
	local pl = AIIndustry.GetLocation(p), ql = AIIndustry.GetLocation(q);
	local label = AICargo.GetCargoLabel(cargo) + " " + AIIndustry.GetName(p) + " - " + AIIndustry.GetName(q);
	local dist = AIMap.DistanceManhattan(pl, ql);
	AILog.Info("PrismaticAI: planner freight by rail: " + label + " (" + dist + " tiles)");
	/* What a served station gets of the production: the station rating, about 70% once trains call
	 * regularly. */
	local supply = AIIndustry.GetLastMonthProduction(p, cargo) * 70 / 100;
	local double = ("double" in shape) && shape.double;
	/* Double track: two platforms at each end (the junction joins two), trains as long as the platform. */
	local platforms = double ? 2 : shape.platforms;
	local plan = PrismaticAI.PlatformPlan(cargo, supply, dist, platforms, shape.len, shape.wagons);
	AILog.Info("PrismaticAI: rail plan " + label + ": " + plan.why + (double ? ", double track" : ""));
	local sa = PrismaticAI.BuildRailStationAt(pl, ql, plan.len, cargo, "produce", "at " + AIIndustry.GetName(p), platforms, double ? 1 : 0);
	if (sa == null) return 0;
	local sb = PrismaticAI.BuildRailStationAt(ql, pl, plan.len, cargo, "accept", "at " + AIIndustry.GetName(q), platforms, double ? -1 : 0);
	if (sb == null) { PrismaticAI.RemoveRailStation(sa, plan.len); return 0; }
	local built = double ? PrismaticAI.BuildDoubleRailLineAndTrain(sa, sb, plan.len, cargo, AIOrder.OF_FULL_LOAD_ANY, label)
		: PrismaticAI.BuildRailLineAndTrain(sa, sb, plan.len, cargo, plan.wagons, AIOrder.OF_FULL_LOAD_ANY, label);
	/* No double-track path: the same line single track. On a hilly map (OpenTTD seed 1807087905)
	 * every double-track search stopped after 5 nodes and the bot ended last with a few buses, while
	 * the single-track line on the same pair was built and ran 7 trains. */
	if (built == -1) {
		AILog.Info("PrismaticAI: no double-track path for " + label + "; trying it single track");
		shape = clone shape;
		shape.double = false;
		return PrismaticAI.BuildFreightRailRoute(p, q, cargo, shape);
	}
	/* The industries it serves, so a saturated line can get a twin (steward.nut TwinLines). */
	if (built > 0) { RAIL_LINES.top().from <- p; RAIL_LINES.top().to <- q; }
	return built;
}

/* The bot's own next freight line: the best untried pair of a producing industry nobody serves yet
 * (no cargo transported last month, no station of ours near it) and an industry accepting that cargo
 * 100 to 260 tiles away, the distance the planner's lines run. Scored by what the game pays for the
 * cargo (AICargo.GetCargoIncome) on that distance and transit time, per day of a round trip, times
 * last month's production. `tried` holds the "from-to-cargo" keys already attempted. Returns
 * {from, to, cargo, distance, production, score} or null. */
function PrismaticAI::NextOwnFreightLine(tried) {
	local best = null, best_score = 0;
	/* Transit time at the speed of a typical early steam engine: about 100 km/h, some 3.7 tiles a day. */
	local tiles_a_day = 3.7;
	foreach (p, _ in AIIndustryList()) {
		if (PrismaticAI.OwnRailStationNear(AIIndustry.GetLocation(p), 8) != null) continue;
		foreach (c, _ in AICargoList_IndustryProducing(p)) {
			if (!AICargo.IsFreight(c)) continue;
			local prod = AIIndustry.GetLastMonthProduction(p, c);
			if (prod < 40 || AIIndustry.GetLastMonthTransported(p, c) > 0) continue;
			local accs = AIIndustryList_CargoAccepting(c);
			accs.Valuate(AIIndustry.GetDistanceManhattanToTile, AIIndustry.GetLocation(p));
			accs.KeepBetweenValue(99, 261);
			foreach (q, dist in accs) {
				if (tried.rawin(p + "-" + q + "-" + c)) continue;
				local days = (dist / tiles_a_day).tointeger() + 1;
				local income = AICargo.GetCargoIncome(c, dist, days);
				local score = (prod * income / (2 * days + 10)).tointeger();
				if (score > best_score) {
					best_score = score;
					best = {from = p, to = q, cargo = c, distance = dist, production = prod, score = score};
				}
			}
		}
	}
	return best;
}
