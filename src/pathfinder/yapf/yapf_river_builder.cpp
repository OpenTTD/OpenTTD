/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file yapf_river_builder.cpp Pathfinder for river building. */

#include "../../stdafx.h"

#include "../../water.h"
#include "../../genworld.h"
#include "yapf.hpp"

#include "../../safeguards.h"

/* River builder pathfinder node. */
struct YapfRiverBuilderNode : CYapfNodeT<CYapfNodeKeyTrackDir, YapfRiverBuilderNode> {};

/* River builder pathfinder node list. */
using RiverBuilderNodeList = NodeList<YapfRiverBuilderNode, 8, 10>;

/* We don't need a follower but YAPF requires one. */
struct RiverBuilderFollower {};

/* We don't need a vehicle but YAPF requires one. */
struct DummyVehicle : Vehicle {};

class YapfRiverBuilder;

/* Types struct required for YAPF components. */
struct RiverBuilderTypes {
	using Tpf = YapfRiverBuilder;
	using TrackFollower = RiverBuilderFollower;
	using NodeList = RiverBuilderNodeList;
	using VehicleType = DummyVehicle;
};

/* River builder pathfinder implementation. */
class YapfRiverBuilder
	: public CYapfBaseT<RiverBuilderTypes>
	, public CYapfSegmentCostCacheNoneT<RiverBuilderTypes>
{
public:
	using Node = RiverBuilderTypes::NodeList::Item;
	using Key = Node::Key;

protected:
	TileIndex end_tile; ///< End tile of the river

	inline YapfRiverBuilder &Yapf()
	{
		return *static_cast<YapfRiverBuilder *>(this);
	}

public:
	YapfRiverBuilder(TileIndex start_tile, TileIndex end_tile)
	{
		this->end_tile = end_tile;

		Node &node = Yapf().CreateNewNode();
		node.Set(nullptr, start_tile, INVALID_TRACKDIR, false);
		Yapf().AddStartupNode(node);
	}

	inline bool PfDetectDestination(Node &n) const
	{
		return n.GetTile() == this->end_tile;
	}

	inline bool PfCalcCost(Node &n, const RiverBuilderFollower *)
	{
		n.cost = n.parent->cost + 1 + RandomRange(_settings_game.game_creation.river_route_random);
		return true;
	}

	inline bool PfCalcEstimate(Node &n)
	{
		n.estimate = n.cost + DistanceManhattan(this->end_tile, n.GetTile());
		assert(n.estimate >= n.parent->estimate);
		return true;
	}

	inline void PfFollowNode(Node &old_node)
	{
		for (DiagDirection d = DIAGDIR_BEGIN; d < DIAGDIR_END; ++d) {
			const TileIndex t = old_node.GetTile() + TileOffsByDiagDir(d);
			if (IsValidTile(t) && RiverFlowsDown(old_node.GetTile(), t)) {
				Node &node = Yapf().CreateNewNode();
				node.Set(&old_node, t, INVALID_TRACKDIR, true);
				Yapf().AddNewNode(node, RiverBuilderFollower{});
			}
		}
	}

	inline char TransportTypeChar() const
	{
		return '~';
	}

	static void BuildRiver(TileIndex start_tile, TileIndex end_tile, TileIndex spring_tile, bool main_river)
	{
		YapfRiverBuilder pf(start_tile, end_tile);
		if (pf.FindPath(nullptr) == false) return; // No path found

		/* First, build the river without worrying about its width. */
		for (Node *node = pf.GetBestNode(); node != nullptr; node = node->parent) {
			TileIndex tile = node->GetTile();
			if (!IsWaterTile(tile)) {
				MakeRiverAndModifyDesertZoneAround(tile);
			}
		}

		/* If the river is a main river, go back along the path to widen it.
		 * Don't make wide rivers if we're using the original landscape generator.
		 */
		if (_settings_game.game_creation.land_generator != LG_ORIGINAL && main_river) {
			const uint long_river_length = _settings_game.game_creation.min_river_length * 4;

			for (Node *node = pf.GetBestNode(); node != nullptr; node = node->parent) {
				const TileIndex center_tile = node->GetTile();

				/* Check if we should widen river depending on how far we are away from the source. */
				uint current_river_length = DistanceManhattan(spring_tile, center_tile);
				uint diameter = std::min(3u, (current_river_length / (long_river_length / 3u)) + 1u);
				if (diameter <= 1) continue;

				for (auto tile : SpiralTileSequence(center_tile, diameter)) {
					RiverMakeWider(tile, center_tile);
				}
			}
		}
	}
};

/**
 * Builds a river from the start tile to the end tile.
 * @param start_tile Start tile of the river.
 * @param end_tile End tile of the river.
 * @param spring_tile spring_tile Tile in which the spring of the river is located.
 * @param main_river Whether it is a main river. Main rivers can get wider than one tile.
 */
void YapfBuildRiver(TileIndex start_tile, TileIndex end_tile, TileIndex spring_tile, bool main_river)
{
	YapfRiverBuilder::BuildRiver(start_tile, end_tile, spring_tile, main_river);
}
