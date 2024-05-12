/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file aystar.cpp Implementation of A*.
 *
 * This file has the core function for %AyStar.
 * %AyStar is a fast path finding routine and is used for things like AI path finding and Train path finding.
 * For more information about %AyStar (A* Algorithm), you can look at
 * <A HREF='http://en.wikipedia.org/wiki/A-star_search_algorithm'>http://en.wikipedia.org/wiki/A-star_search_algorithm</A>.
 */

#include "../../stdafx.h"
#include "aystar.h"

#include "../../safeguards.h"

/**
 * Adds a node to the open list.
 * It makes a copy of node, and puts the pointer of parent in the struct.
 */
void AyStar::OpenListAdd(PathNode *parent, const AyStarNode *node, int f, int g)
{
	/* Add a new Node to the OpenList */
	PathNode *new_node = this->nodes.CreateNewNode();
	new_node->Set(parent, node->m_tile, node->m_td, true);
	new_node->m_estimate = f;
	new_node->m_cost = g;
	this->nodes.InsertOpenNode(*new_node);
}

/**
 * Checks one tile and calculate its f-value
 */
void AyStar::CheckTile(AyStarNode *current, PathNode *parent)
{
	/* Check the new node against the ClosedList */
	if (this->nodes.FindClosedNode(*current) != nullptr) return;

	/* Calculate the G-value for this node */
	int new_g = this->CalculateG(this, current, parent);
	/* If the value was INVALID_NODE, we don't do anything with this node */
	if (new_g == AYSTAR_INVALID_NODE) return;

	/* There should not be given any other error-code.. */
	assert(new_g >= 0);
	/* Add the parent g-value to the new g-value */
	new_g += parent->m_cost;
	if (this->max_path_cost != 0 && new_g > this->max_path_cost) return;

	/* Calculate the h-value */
	int new_h = this->CalculateH(this, current, parent);
	/* There should not be given any error-code.. */
	assert(new_h >= 0);

	/* The f-value if g + h */
	int new_f = new_g + new_h;

	/* Get the pointer to the parent in the ClosedList (the current one is to a copy of the one in the OpenList) */
	PathNode *closedlist_parent = this->nodes.FindClosedNode(parent->m_key);

	/* Check if this item is already in the OpenList */
	PathNode *check = this->nodes.FindOpenNode(*current);
	if (check != nullptr) {
		/* Yes, check if this g value is lower.. */
		if (new_g > check->m_cost) return;
		this->nodes.PopOpenNode(check->m_key);
		/* It is lower, so change it to this item */
		check->m_estimate = new_f;
		check->m_cost = new_g;
		check->m_parent = closedlist_parent;
		/* Re-add it in the openlist_queue. */
		this->nodes.InsertOpenNode(*check);
	} else {
		/* A new node, add it to the OpenList */
		this->OpenListAdd(closedlist_parent, current, new_f, new_g);
	}
}

/**
 * This function is the core of %AyStar. It handles one item and checks
 * its neighbour items. If they are valid, they are added to be checked too.
 * @return Possible values:
 *  - #AYSTAR_EMPTY_OPENLIST : indicates all items are tested, and no path has been found.
 *  - #AYSTAR_LIMIT_REACHED : Indicates that the max_search_nodes limit has been reached.
 *  - #AYSTAR_FOUND_END_NODE : indicates we found the end. Path_found now is true, and in path is the path found.
 *  - #AYSTAR_STILL_BUSY : indicates we have done this tile, did not found the path yet, and have items left to try.
 */
int AyStar::Loop()
{
	/* Get the best node from OpenList */
	PathNode *current = this->nodes.PopBestOpenNode();
	/* If empty, drop an error */
	if (current == nullptr) return AYSTAR_EMPTY_OPENLIST;

	/* Check for end node and if found, return that code */
	if (this->EndNodeCheck(this, current) == AYSTAR_FOUND_END_NODE && current->m_parent != nullptr) {
		if (this->FoundEndNode != nullptr) {
			this->FoundEndNode(this, current);
		}
		return AYSTAR_FOUND_END_NODE;
	}

	/* Add the node to the ClosedList */
	this->nodes.InsertClosedNode(*current);

	/* Load the neighbours */
	this->GetNeighbours(this, current);

	/* Go through all neighbours */
	for (int i = 0; i < this->num_neighbours; i++) {
		/* Check and add them to the OpenList if needed */
		this->CheckTile(&this->neighbours[i], current);
	}

	if (this->max_search_nodes != 0 && this->nodes.ClosedCount() >= this->max_search_nodes) {
		/* We've expanded enough nodes */
		return AYSTAR_LIMIT_REACHED;
	} else {
		/* Return that we are still busy */
		return AYSTAR_STILL_BUSY;
	}
}

/**
 * This is the function you call to run AyStar.
 * @return Possible values:
 *  - #AYSTAR_FOUND_END_NODE : indicates we found an end node.
 *  - #AYSTAR_NO_PATH : indicates that there was no path found.
 *  - #AYSTAR_STILL_BUSY : indicates we have done some checked, that we did not found the path yet, and that we still have items left to try.
 * @note When the algorithm is done (when the return value is not #AYSTAR_STILL_BUSY) #Clear() is called automatically.
 *       When you stop the algorithm halfway, you should call #Clear() yourself!
 */
int AyStar::Main()
{
	int r, i = 0;
	/* Loop through the OpenList
	 *  Quit if result is no AYSTAR_STILL_BUSY or is more than loops_per_tick */
	while ((r = this->Loop()) == AYSTAR_STILL_BUSY && (this->loops_per_tick == 0 || ++i < this->loops_per_tick)) { }
#ifdef AYSTAR_DEBUG
	switch (r) {
		case AYSTAR_FOUND_END_NODE: Debug(misc, 0, "[AyStar] Found path!"); break;
		case AYSTAR_EMPTY_OPENLIST: Debug(misc, 0, "[AyStar] OpenList run dry, no path found"); break;
		case AYSTAR_LIMIT_REACHED:  Debug(misc, 0, "[AyStar] Exceeded search_nodes, no path found"); break;
		default: break;
	}
#endif

	switch (r) {
		case AYSTAR_FOUND_END_NODE: return AYSTAR_FOUND_END_NODE;
		case AYSTAR_EMPTY_OPENLIST:
		case AYSTAR_LIMIT_REACHED:  return AYSTAR_NO_PATH;
		default:                    return AYSTAR_STILL_BUSY;
	}
}

/**
 * Adds a node from where to start an algorithm. Multiple nodes can be added
 * if wanted. You should make sure that #Clear() is called before adding nodes
 * if the #AyStar has been used before (though the normal main loop calls
 * #Clear() automatically when the algorithm finishes.
 * @param start_node Node to start with.
 * @param g the cost for starting with this node.
 */
void AyStar::AddStartNode(AyStarNode *start_node, int g)
{
#ifdef AYSTAR_DEBUG
	Debug(misc, 0, "[AyStar] Starting A* Algorithm from node ({}, {}, {})\n",
		TileX(start_node->tile), TileY(start_node->tile), start_node->direction);
#endif
	this->OpenListAdd(nullptr, start_node, 0, g);
}
