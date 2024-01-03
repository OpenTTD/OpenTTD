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

#include "queue.h"
#include "../../tile_type.h"
#include "../../track_type.h"

static const int AYSTAR_DEF_MAX_SEARCH_NODES = 10000; ///< Reference limit for #AyStar::max_search_nodes

/** Return status of #AyStar methods. */
enum AystarStatus {
	AYSTAR_FOUND_END_NODE, ///< An end node was found.
	AYSTAR_EMPTY_OPENLIST, ///< All items are tested, and no path has been found.
	AYSTAR_STILL_BUSY,     ///< Some checking was done, but no path found yet, and there are still items left to try.
	AYSTAR_NO_PATH,        ///< No path to the goal was found.
	AYSTAR_LIMIT_REACHED,  ///< The #AyStar::max_search_nodes limit has been reached, aborting search.
	AYSTAR_DONE,           ///< Not an end-tile, or wrong direction.
};

static const int AYSTAR_INVALID_NODE = -1; ///< Item is not valid (for example, not walkable).

/** Node in the search. */
struct AyStarNode {
	TileIndex tile;
	Trackdir direction;
	uint user_data[2];
};

/** A path of nodes. */
struct PathNode {
	AyStarNode node;
	PathNode *parent; ///< The parent of this item.
};

/**
 * Internal node.
 * @note We do not save the h-value, because it is only needed to calculate the f-value.
 *       h-value should \em always be the distance left to the end-tile.
 */
struct OpenListNode {
	int g;
	PathNode path;
};

bool CheckIgnoreFirstTile(const PathNode *node);

struct AyStar;

/**
 * Check whether the end-tile is found.
 * @param aystar %AyStar search algorithm data.
 * @param current Node to exam one.
 * @note The 2nd parameter should be #OpenListNode, and \em not #AyStarNode. #AyStarNode is
 * part of #OpenListNode and so it could be accessed without any problems.
 * The good part about #OpenListNode is, and how AIs use it, that you can
 * access the parent of the current node, and so check if you, for example
 * don't try to enter the file tile with a 90-degree curve. So please, leave
 * this an #OpenListNode, it works just fine.
 * @return Status of the node:
 *  - #AYSTAR_FOUND_END_NODE : indicates this is the end tile
 *  - #AYSTAR_DONE : indicates this is not the end tile (or direction was wrong)
 */
typedef int32_t AyStar_EndNodeCheck(const AyStar *aystar, const OpenListNode *current);

/**
 * Calculate the G-value for the %AyStar algorithm.
 * @return G value of the node:
 *  - #AYSTAR_INVALID_NODE : indicates an item is not valid (e.g.: unwalkable)
 *  - Any value >= 0 : the g-value for this tile
 */
typedef int32_t AyStar_CalculateG(AyStar *aystar, AyStarNode *current, OpenListNode *parent);

/**
 * Calculate the H-value for the %AyStar algorithm.
 * Mostly, this must return the distance (Manhattan way) between the current point and the end point.
 * @return The h-value for this tile (any value >= 0)
 */
typedef int32_t AyStar_CalculateH(AyStar *aystar, AyStarNode *current, OpenListNode *parent);

/**
 * This function requests the tiles around the current tile and put them in #neighbours.
 * #neighbours is never reset, so if you are not using directions, just leave it alone.
 * @warning Never add more #neighbours than memory allocated for it.
 */
typedef void AyStar_GetNeighbours(AyStar *aystar, OpenListNode *current);

/**
 * If the End Node is found, this function is called.
 * It can do, for example, calculate the route and put that in an array.
 */
typedef void AyStar_FoundEndNode(AyStar *aystar, OpenListNode *current);

/**
 * %AyStar search algorithm struct.
 * Before calling #Init(), fill #CalculateG, #CalculateH, #GetNeighbours, #EndNodeCheck, and #FoundEndNode.
 * If you want to change them after calling #Init(), first call #Free() !
 *
 * The #user_path, #user_target, and #user_data[10] are intended to be used by the user routines. The data not accessed by the #AyStar code itself.
 * The user routines can change any moment they like.
 */
struct AyStar {
/* These fields should be filled before initing the AyStar, but not changed
 * afterwards (except for user_data and user_path)! (free and init again to change them) */

	/* These should point to the application specific routines that do the
	 * actual work */
	AyStar_CalculateG *CalculateG;
	AyStar_CalculateH *CalculateH;
	AyStar_GetNeighbours *GetNeighbours;
	AyStar_EndNodeCheck *EndNodeCheck;
	AyStar_FoundEndNode *FoundEndNode;

	/* These are completely untouched by AyStar, they can be accessed by
	 * the application specific routines to input and output data.
	 * user_path should typically contain data about the resulting path
	 * afterwards, user_target should typically contain information about
	 * what you where looking for, and user_data can contain just about
	 * everything */
	void *user_path;
	void *user_target;
	void *user_data;

	byte loops_per_tick;   ///< How many loops are there called before Main() gives control back to the caller. 0 = until done.
	uint max_path_cost;    ///< If the g-value goes over this number, it stops searching, 0 = infinite.
	uint max_search_nodes; ///< The maximum number of nodes that will be expanded, 0 = infinite.

	/* These should be filled with the neighbours of a tile by
	 * GetNeighbours */
	AyStarNode neighbours[12];
	byte num_neighbours;

	void Init(Hash_HashProc hash, uint num_buckets);

	/* These will contain the methods for manipulating the AyStar. Only
	 * Main() should be called externally */
	void AddStartNode(AyStarNode *start_node, uint g);
	int Main();
	int Loop();
	void Free();
	void Clear();
	void CheckTile(AyStarNode *current, OpenListNode *parent);

protected:
	Hash       closedlist_hash; ///< The actual closed list.
	BinaryHeap openlist_queue;  ///< The open queue.
	Hash       openlist_hash;   ///< An extra hash to speed up the process of looking up an element in the open list.

	void OpenListAdd(PathNode *parent, const AyStarNode *node, int f, int g);
	OpenListNode *OpenListIsInList(const AyStarNode *node);
	OpenListNode *OpenListPop();

	void ClosedListAdd(const PathNode *node);
	PathNode *ClosedListIsInList(const AyStarNode *node);
};

#endif /* AYSTAR_H */
