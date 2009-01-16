/* $Id$ */

/**
 * A Road Pathfinder.
 *  This road pathfinder tries to find a buildable / existing route for
 *  road vehicles. You can changes the costs below using for example
 *  roadpf.cost.turn = 30. Note that it's not allowed to change the cost
 *  between consecutive calls to FindPath. You can change the cost before
 *  the first call to FindPath and after FindPath has returned an actual
 *  route. To use only existing roads, set cost.no_existing_road to
 *  cost.max_cost.
 */
class Road
{
	_aystar_class = import("graph.aystar", "", 4);
	_max_cost = null;              ///< The maximum cost for a route.
	_cost_tile = null;             ///< The cost for a single tile.
	_cost_no_existing_road = null; ///< The cost that is added to _cost_tile if no road exists yet.
	_cost_turn = null;             ///< The cost that is added to _cost_tile if the direction changes.
	_cost_slope = null;            ///< The extra cost if a road tile is sloped.
	_cost_bridge_per_tile = null;  ///< The cost per tile of a new bridge, this is added to _cost_tile.
	_cost_tunnel_per_tile = null;  ///< The cost per tile of a new tunnel, this is added to _cost_tile.
	_cost_coast = null;            ///< The extra cost for a coast tile.
	_pathfinder = null;            ///< A reference to the used AyStar object.
	_max_bridge_length = null;     ///< The maximum length of a bridge that will be build.
	_max_tunnel_length = null;     ///< The maximum length of a tunnel that will be build.

	cost = null;                   ///< Used to change the costs.
	_running = null;

	constructor()
	{
		this._max_cost = 10000000;
		this._cost_tile = 100;
		this._cost_no_existing_road = 40;
		this._cost_turn = 100;
		this._cost_slope = 200;
		this._cost_bridge_per_tile = 150;
		this._cost_tunnel_per_tile = 120;
		this._cost_coast = 20;
		this._max_bridge_length = 10;
		this._max_tunnel_length = 20;
		this._pathfinder = this._aystar_class(this._Cost, this._Estimate, this._Neighbours, this._CheckDirection, this, this, this, this);

		this.cost = this.Cost(this);
		this._running = false;
	}

	/**
	 * Initialize a path search between sources and goals.
	 * @param sources The source tiles.
	 * @param goals The target tiles.
	 * @see AyStar::InitializePath()
	 */
	function InitializePath(sources, goals) {
		local nsources = [];

		foreach (node in sources) {
			nsources.push([node, 0xFF]);
		}
		this._pathfinder.InitializePath(nsources, goals);
	}

	/**
	 * Try to find the path as indicated with InitializePath with the lowest cost.
	 * @param iterations After how many iterations it should abort for a moment.
	 *  This value should either be -1 for infinite, or > 0. Any other value
	 *  aborts immediatly and will never find a path.
	 * @return A route if one was found, or false if the amount of iterations was
	 *  reached, or null if no path was found.
	 *  You can call this function over and over as long as it returns false,
	 *  which is an indication it is not yet done looking for a route.
	 * @see AyStar::FindPath()
	 */
	function FindPath(iterations);
};

class Road.Cost
{
	_main = null;

	function _set(idx, val)
	{
		if (this._main._running) throw("You are not allowed to change parameters of a running pathfinder.");

		switch (idx) {
			case "max_cost":          this._main._max_cost = val; break;
			case "tile":              this._main._cost_tile = val; break;
			case "no_existing_road":  this._main._cost_no_existing_road = val; break;
			case "turn":              this._main._cost_turn = val; break;
			case "slope":             this._main._cost_slope = val; break;
			case "bridge_per_tile":   this._main._cost_bridge_per_tile = val; break;
			case "tunnel_per_tile":   this._main._cost_tunnel_per_tile = val; break;
			case "coast":             this._main._cost_coast = val; break;
			case "max_bridge_length": this._main._max_bridge_length = val; break;
			case "max_tunnel_length": this._main._max_tunnel_length = val; break;
			default: throw("the index '" + idx + "' does not exist");
		}

		return val;
	}

	function _get(idx)
	{
		switch (idx) {
			case "max_cost":          return this._main._max_cost;
			case "tile":              return this._main._cost_tile;
			case "no_existing_road":  return this._main._cost_no_existing_road;
			case "turn":              return this._main._cost_turn;
			case "slope":             return this._main._cost_slope;
			case "bridge_per_tile":   return this._main._cost_bridge_per_tile;
			case "tunnel_per_tile":   return this._main._cost_tunnel_per_tile;
			case "coast":             return this._main._cost_coast;
			case "max_bridge_length": return this._main._max_bridge_length;
			case "max_tunnel_length": return this._main._max_tunnel_length;
			default: throw("the index '" + idx + "' does not exist");
		}
	}

	constructor(main)
	{
		this._main = main;
	}
};

function Road::FindPath(iterations)
{
	local test_mode = AITestMode();
	local ret = this._pathfinder.FindPath(iterations);
	this._running = (ret == false) ? true : false;
	return ret;
}

function Road::_GetBridgeNumSlopes(end_a, end_b)
{
	local slopes = 0;
	local direction = (end_b - end_a) / AIMap.DistanceManhattan(end_a, end_b);
	local slope = AITile.GetSlope(end_a);
	if (!((slope == AITile.SLOPE_NE && direction == 1) || (slope == AITile.SLOPE_SE && direction == -AIMap.GetMapSizeX()) ||
		(slope == AITile.SLOPE_SW && direction == -1) || (slope == AITile.SLOPE_NW && direction == AIMap.GetMapSizeX()) ||
		 slope == AITile.SLOPE_N || slope == AITile.SLOPE_E || slope == AITile.SLOPE_S || slope == AITile.SLOPE_W)) {
		slopes++;
	}

	local slope = AITile.GetSlope(end_b);
	direction = -direction;
	if (!((slope == AITile.SLOPE_NE && direction == 1) || (slope == AITile.SLOPE_SE && direction == -AIMap.GetMapSizeX()) ||
		(slope == AITile.SLOPE_SW && direction == -1) || (slope == AITile.SLOPE_NW && direction == AIMap.GetMapSizeX()) ||
		 slope == AITile.SLOPE_N || slope == AITile.SLOPE_E || slope == AITile.SLOPE_S || slope == AITile.SLOPE_W)) {
		slopes++;
	}
	return slopes;
}

function Road::_Cost(path, new_tile, new_direction, self)
{
	/* path == null means this is the first node of a path, so the cost is 0. */
	if (path == null) return 0;

	local prev_tile = path.GetTile();

	/* If the new tile is a bridge / tunnel tile, check whether we came from the other
	 * end of the bridge / tunnel or if we just entered the bridge / tunnel. */
	if (AIBridge.IsBridgeTile(new_tile)) {
		if (AIBridge.GetOtherBridgeEnd(new_tile) != prev_tile) return path.GetCost() + self._cost_tile;
		return path.GetCost() + AIMap.DistanceManhattan(new_tile, prev_tile) * self._cost_tile + self._GetBridgeNumSlopes(new_tile, prev_tile) * self._cost_slope;
	}
	if (AITunnel.IsTunnelTile(new_tile)) {
		if (AITunnel.GetOtherTunnelEnd(new_tile) != prev_tile) return path.GetCost() + self._cost_tile;
		return path.GetCost() + AIMap.DistanceManhattan(new_tile, prev_tile) * self._cost_tile;
	}

	/* If the two tiles are more then 1 tile apart, the pathfinder wants a bridge or tunnel
	 * to be build. It isn't an existing bridge / tunnel, as that case is already handled. */
	if (AIMap.DistanceManhattan(new_tile, prev_tile) > 1) {
		/* Check if we should build a bridge or a tunnel. */
		if (AITunnel.GetOtherTunnelEnd(new_tile) == prev_tile) {
			return path.GetCost() + AIMap.DistanceManhattan(new_tile, prev_tile) * (self._cost_tile + self._cost_tunnel_per_tile);
		} else {
			return path.GetCost() + AIMap.DistanceManhattan(new_tile, prev_tile) * (self._cost_tile + self._cost_bridge_per_tile) + self._GetBridgeNumSlopes(new_tile, prev_tile) * self._cost_slope;
		}
	}

	/* Check for a turn. We do this by substracting the TileID of the current node from
	 * the TileID of the previous node and comparing that to the difference between the
	 * previous node and the node before that. */
	local cost = self._cost_tile;
	if (path.GetParent() != null && (prev_tile - path.GetParent().GetTile()) != (new_tile - prev_tile) &&
		AIMap.DistanceManhattan(path.GetParent().GetTile(), prev_tile) == 1) {
		cost += self._cost_turn;
	}

	/* Check if the new tile is a coast tile. */
	if (AITile.IsCoastTile(new_tile)) {
		cost += self._cost_coast;
	}

	/* Check if the last tile was sloped. */
	if (path.GetParent() != null && !AIBridge.IsBridgeTile(prev_tile) && !AITunnel.IsTunnelTile(prev_tile) &&
	    self._IsSlopedRoad(path.GetParent().GetTile(), prev_tile, new_tile)) {
		cost += self._cost_slope;
	}

	if (!AIRoad.AreRoadTilesConnected(prev_tile, new_tile)) {
		cost += self._cost_no_existing_road;
	}

	return path.GetCost() + cost;
}

function Road::_Estimate(cur_tile, cur_direction, goal_tiles, self)
{
	local min_cost = self._max_cost;
	/* As estimate we multiply the lowest possible cost for a single tile with
	 * with the minimum number of tiles we need to traverse. */
	foreach (tile in goal_tiles) {
		min_cost = min(AIMap.DistanceManhattan(cur_tile, tile) * self._cost_tile, min_cost);
	}
	return min_cost;
}

function Road::_Neighbours(path, cur_node, self)
{
	/* self._max_cost is the maximum path cost, if we go over it, the path isn't valid. */
	if (path.GetCost() >= self._max_cost) return [];
	local tiles = [];

	/* Check if the current tile is part of a bridge or tunnel. */
	if ((AIBridge.IsBridgeTile(cur_node) || AITunnel.IsTunnelTile(cur_node)) &&
	     AITile.HasTransportType(cur_node, AITile.TRANSPORT_ROAD)) {
		local other_end = AIBridge.IsBridgeTile(cur_node) ? AIBridge.GetOtherBridgeEnd(cur_node) : AITunnel.GetOtherTunnelEnd(cur_node);
		local next_tile = cur_node + (cur_node - other_end) / AIMap.DistanceManhattan(cur_node, other_end);
		if (AIRoad.AreRoadTilesConnected(cur_node, next_tile) || AITile.IsBuildable(next_tile) || AIRoad.IsRoadTile(next_tile)) {
			tiles.push([next_tile, self._GetDirection(cur_node, next_tile, false)]);
		}
		/* The other end of the bridge / tunnel is a neighbour. */
		tiles.push([other_end, self._GetDirection(next_tile, cur_node, true) << 4]);
	} else if (path.GetParent() != null && AIMap.DistanceManhattan(cur_node, path.GetParent().GetTile()) > 1) {
		local other_end = path.GetParent().GetTile();
		local next_tile = cur_node + (cur_node - other_end) / AIMap.DistanceManhattan(cur_node, other_end);
		if (AIRoad.AreRoadTilesConnected(cur_node, next_tile) || AIRoad.BuildRoad(cur_node, next_tile)) {
			tiles.push([next_tile, self._GetDirection(cur_node, next_tile, false)]);
		}
	} else {
		local offsets = [AIMap.GetTileIndex(0, 1), AIMap.GetTileIndex(0, -1),
		                 AIMap.GetTileIndex(1, 0), AIMap.GetTileIndex(-1, 0)];
		/* Check all tiles adjacent to the current tile. */
		foreach (offset in offsets) {
			local next_tile = cur_node + offset;
			/* We add them to the to the neighbours-list if one of the following applies:
			 * 1) There already is a connections between the current tile and the next tile.
			 * 2) We can build a road to the next tile.
			 * 3) The next tile is the entrance of a tunnel / bridge in the correct direction. */
			if (AIRoad.AreRoadTilesConnected(cur_node, next_tile)) {
				tiles.push([next_tile, self._GetDirection(cur_node, next_tile, false)]);
			} else if ((AITile.IsBuildable(next_tile) || AIRoad.IsRoadTile(next_tile)) &&
					(path.GetParent() == null || AIRoad.CanBuildConnectedRoadPartsHere(cur_node, path.GetParent().GetTile(), next_tile)) &&
					AIRoad.BuildRoad(cur_node, next_tile)) {
				tiles.push([next_tile, self._GetDirection(cur_node, next_tile, false)]);
			} else if (self._CheckTunnelBridge(cur_node, next_tile)) {
				tiles.push([next_tile, self._GetDirection(cur_node, next_tile, false)]);
			}
		}
		if (path.GetParent() != null) {
			local bridges = self._GetTunnelsBridges(path.GetParent().GetTile(), cur_node, self._GetDirection(path.GetParent().GetTile(), cur_node, true) << 4);
			foreach (tile in bridges) {
				tiles.push(tile);
			}
		}
	}
	return tiles;
}

function Road::_CheckDirection(tile, existing_direction, new_direction, self)
{
	return false;
}

function Road::_GetDirection(from, to, is_bridge)
{
	if (!is_bridge && AITile.GetSlope(to) == AITile.SLOPE_FLAT) return 0xFF;
	if (from - to == 1) return 1;
	if (from - to == -1) return 2;
	if (from - to == AIMap.GetMapSizeX()) return 4;
	if (from - to == -AIMap.GetMapSizeX()) return 8;
}

/**
 * Get a list of all bridges and tunnels that can be build from the
 * current tile. Bridges will only be build starting on non-flat tiles
 * for performance reasons. Tunnels will only be build if no terraforming
 * is needed on both ends.
 */
function Road::_GetTunnelsBridges(last_node, cur_node, bridge_dir)
{
	local slope = AITile.GetSlope(cur_node);
	if (slope == AITile.SLOPE_FLAT) return [];
	local tiles = [];

	for (local i = 2; i < this._max_bridge_length; i++) {
		local bridge_list = AIBridgeList_Length(i + 1);
		local target = cur_node + i * (cur_node - last_node);
		if (!bridge_list.IsEmpty() && AIBridge.BuildBridge(AIVehicle.VT_ROAD, bridge_list.Begin(), cur_node, target)) {
			tiles.push([target, bridge_dir]);
		}
	}

	if (slope != AITile.SLOPE_SW && slope != AITile.SLOPE_NW && slope != AITile.SLOPE_SE && slope != AITile.SLOPE_NE) return tiles;
	local other_tunnel_end = AITunnel.GetOtherTunnelEnd(cur_node);
	if (!AIMap.IsValidTile(other_tunnel_end)) return tiles;

	local tunnel_length = AIMap.DistanceManhattan(cur_node, other_tunnel_end);
	local prev_tile = cur_node + (cur_node - other_tunnel_end) / tunnel_length;
	if (AITunnel.GetOtherTunnelEnd(other_tunnel_end) == cur_node && tunnel_length >= 2 &&
			prev_tile == last_node && tunnel_length < _max_tunnel_length && AITunnel.BuildTunnel(AIVehicle.VT_ROAD, cur_node)) {
		tiles.push([other_tunnel_end, bridge_dir]);
	}
	return tiles;
}

function Road::_IsSlopedRoad(start, middle, end)
{
	local NW = 0; //Set to true if we want to build a road to / from the north-west
	local NE = 0; //Set to true if we want to build a road to / from the north-east
	local SW = 0; //Set to true if we want to build a road to / from the south-west
	local SE = 0; //Set to true if we want to build a road to / from the south-east

	if (middle - AIMap.GetMapSizeX() == start || middle - AIMap.GetMapSizeX() == end) NW = 1;
	if (middle - 1 == start || middle - 1 == end) NE = 1;
	if (middle + AIMap.GetMapSizeX() == start || middle + AIMap.GetMapSizeX() == end) SE = 1;
	if (middle + 1 == start || middle + 1 == end) SW = 1;

	/* If there is a turn in the current tile, it can't be sloped. */
	if ((NW || SE) && (NE || SW)) return false;

	local slope = AITile.GetSlope(middle);
	/* A road on a steep slope is always sloped. */
	if (AITile.IsSteepSlope(slope)) return true;

	/* If only one corner is raised, the road is sloped. */
	if (slope == AITile.SLOPE_N || slope == AITile.SLOPE_W) return true;
	if (slope == AITile.SLOPE_S || slope == AITile.SLOPE_E) return true;

	if (NW && (slope == AITile.SLOPE_NW || slope == AITile.SLOPE_SE)) return true;
	if (NE && (slope == AITile.SLOPE_NE || slope == AITile.SLOPE_SW)) return true;

	return false;
}

function Road::_CheckTunnelBridge(current_tile, new_tile)
{
	if (!AIBridge.IsBridgeTile(new_tile) && !AITunnel.IsTunnelTile(new_tile)) return false;
	local dir = new_tile - current_tile;
	local other_end = AIBridge.IsBridgeTile(new_tile) ? AIBridge.GetOtherBridgeEnd(new_tile) : AITunnel.GetOtherTunnelEnd(new_tile);
	local dir2 = other_end - new_tile;
	if ((dir < 0 && dir2 > 0) || (dir > 0 && dir2 < 0)) return false;
	dir = abs(dir);
	dir2 = abs(dir2);
	if ((dir >= AIMap.GetMapSizeX() && dir2 < AIMap.GetMapSizeX()) ||
	    (dir < AIMap.GetMapSizeX() && dir2 >= AIMap.GetMapSizeX())) return false;

	return true;
}
