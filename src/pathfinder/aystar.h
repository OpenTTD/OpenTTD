/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file aystar.h
 * This file has the header for %AyStar.
 * %AyStar is a fast path finding routine and is used for things like AI path finding and Train path finding.
 * For more information about AyStar (A* Algorithm), you can look at
 * <A HREF='http://en.wikipedia.org/wiki/A-star_search_algorithm'>http://en.wikipedia.org/wiki/A-star_search_algorithm</A>.
 */

#ifndef AYSTAR_H
#define AYSTAR_H

#include "yapf/nodelist.hpp"
#include "yapf/yapf_node.hpp"

static const int AYSTAR_DEF_MAX_SEARCH_NODES = 10000; ///< Reference limit for #AyStar::max_search_nodes

/** Return status of #AyStar methods. */
enum class AyStarStatus : uint8_t {
	FoundEndNode, ///< An end node was found.
	EmptyOpenList, ///< All items are tested, and no path has been found.
	StillBusy, ///< Some checking was done, but no path found yet, and there are still items left to try.
	NoPath, ///< No path to the goal was found.
	LimitReached, ///< The AYSTAR_DEF_MAX_SEARCH_NODES limit has been reached, aborting search.
	Done, ///< Not an end-tile, or wrong direction.
};

static const int AYSTAR_INVALID_NODE = -1; ///< Item is not valid (for example, not walkable).

using AyStarNode = CYapfNodeKeyTrackDir;

struct PathNode : CYapfNodeT<AyStarNode, PathNode> {
};

/**
 * %AyStar search algorithm struct.
 */
class AyStar {
protected:
	/**
	 * Calculate the G-value for the %AyStar algorithm.
	 * @return G value of the node:
	 *  - #AYSTAR_INVALID_NODE : indicates an item is not valid (e.g.: unwalkable)
	 *  - Any value >= 0 : the g-value for this tile
	 */
	virtual int32_t CalculateG(const AyStarNode &current, const PathNode &parent) const = 0;

	/**
	 * Calculate the H-value for the %AyStar algorithm.
	 * Mostly, this must return the distance (Manhattan way) between the current point and the end point.
	 * @return The h-value for this tile (any value >= 0)
	 */
	virtual int32_t CalculateH(const AyStarNode &current, const PathNode &parent) const = 0;

	/**
	 * This function requests the tiles around the current tile.
	 * #neighbours is never reset, so if you are not using directions, just leave it alone.
	 */
	virtual void GetNeighbours(const PathNode &current, std::vector<AyStarNode> &neighours) const = 0;

	 /**
	 * Check whether the end-tile is found.
	 * @param current Node to exam.
	 * @return Status of the node:
	 *  - #AyStarStatus::FoundEndNode : indicates this is the end tile
	 *  - #AyStarStatus::Done : indicates this is not the end tile (or direction was wrong)
	 */
	virtual AyStarStatus EndNodeCheck(const PathNode &current) const = 0;

	/**
	 * If the End Node is found, this function is called.
	 * It can do, for example, calculate the route and put that in an array.
	 */
	virtual void FoundEndNode(const PathNode &current) = 0;

	void AddStartNode(AyStarNode *start_node, int g);

	AyStarStatus Main();

public:
	virtual ~AyStar() = default;

private:
	NodeList<PathNode, 8, 10> nodes;
	mutable std::vector<AyStarNode> neighbours;

	AyStarStatus Loop();
	void OpenListAdd(PathNode *parent, const AyStarNode *node, int f, int g);
	void CheckTile(AyStarNode *current, PathNode *parent);
};

#endif /* AYSTAR_H */
