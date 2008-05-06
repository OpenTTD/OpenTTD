/* $Id$ */

/** @file aystar.cpp Implementation of A*. */

/*
 * This file has the core function for AyStar
 *  AyStar is a fast pathfinding routine and is used for things like
 *  AI_pathfinding and Train_pathfinding.
 *  For more information about AyStar (A* Algorithm), you can look at
 *    http://en.wikipedia.org/wiki/A-star_search_algorithm
 */

/*
 * Friendly reminder:
 *  Call (AyStar).free() when you are done with Aystar. It reserves a lot of memory
 *  And when not free'd, it can cause system-crashes.
 * Also remember that when you stop an algorithm before it is finished, your
 * should call clear() yourself!
 */

#include "stdafx.h"
#include "openttd.h"
#include "aystar.h"
#include "core/alloc_func.hpp"

int _aystar_stats_open_size;
int _aystar_stats_closed_size;

// This looks in the Hash if a node exists in ClosedList
//  If so, it returns the PathNode, else NULL
static PathNode* AyStarMain_ClosedList_IsInList(AyStar *aystar, const AyStarNode *node)
{
	return (PathNode*)Hash_Get(&aystar->ClosedListHash, node->tile, node->direction);
}

// This adds a node to the ClosedList
//  It makes a copy of the data
static void AyStarMain_ClosedList_Add(AyStar *aystar, const PathNode *node)
{
	// Add a node to the ClosedList
	PathNode *new_node = MallocT<PathNode>(1);
	*new_node = *node;
	Hash_Set(&aystar->ClosedListHash, node->node.tile, node->node.direction, new_node);
}

// Checks if a node is in the OpenList
//   If so, it returns the OpenListNode, else NULL
static OpenListNode *AyStarMain_OpenList_IsInList(AyStar *aystar, const AyStarNode *node)
{
	return (OpenListNode*)Hash_Get(&aystar->OpenListHash, node->tile, node->direction);
}

// Gets the best node from OpenList
//  returns the best node, or NULL of none is found
// Also it deletes the node from the OpenList
static OpenListNode *AyStarMain_OpenList_Pop(AyStar *aystar)
{
	// Return the item the Queue returns.. the best next OpenList item.
	OpenListNode *res = (OpenListNode*)aystar->OpenListQueue.pop(&aystar->OpenListQueue);
	if (res != NULL) {
		Hash_Delete(&aystar->OpenListHash, res->path.node.tile, res->path.node.direction);
	}

	return res;
}

// Adds a node to the OpenList
//  It makes a copy of node, and puts the pointer of parent in the struct
static void AyStarMain_OpenList_Add(AyStar *aystar, PathNode *parent, const AyStarNode *node, int f, int g)
{
	// Add a new Node to the OpenList
	OpenListNode *new_node = MallocT<OpenListNode>(1);
	new_node->g = g;
	new_node->path.parent = parent;
	new_node->path.node = *node;
	Hash_Set(&aystar->OpenListHash, node->tile, node->direction, new_node);

	// Add it to the queue
	aystar->OpenListQueue.push(&aystar->OpenListQueue, new_node, f);
}

/*
 * Checks one tile and calculate his f-value
 *  return values:
 * AYSTAR_DONE : indicates we are done
 */
int AyStarMain_CheckTile(AyStar *aystar, AyStarNode *current, OpenListNode *parent)
{
	int new_f, new_g, new_h;
	PathNode *closedlist_parent;
	OpenListNode *check;

	// Check the new node against the ClosedList
	if (AyStarMain_ClosedList_IsInList(aystar, current) != NULL) return AYSTAR_DONE;

	// Calculate the G-value for this node
	new_g = aystar->CalculateG(aystar, current, parent);
	// If the value was INVALID_NODE, we don't do anything with this node
	if (new_g == AYSTAR_INVALID_NODE) return AYSTAR_DONE;

	// There should not be given any other error-code..
	assert(new_g >= 0);
	// Add the parent g-value to the new g-value
	new_g += parent->g;
	if (aystar->max_path_cost != 0 && (uint)new_g > aystar->max_path_cost) return AYSTAR_DONE;

	// Calculate the h-value
	new_h = aystar->CalculateH(aystar, current, parent);
	// There should not be given any error-code..
	assert(new_h >= 0);

	// The f-value if g + h
	new_f = new_g + new_h;

	// Get the pointer to the parent in the ClosedList (the currentone is to a copy of the one in the OpenList)
	closedlist_parent = AyStarMain_ClosedList_IsInList(aystar, &parent->path.node);

	// Check if this item is already in the OpenList
	check = AyStarMain_OpenList_IsInList(aystar, current);
	if (check != NULL) {
		uint i;
		// Yes, check if this g value is lower..
		if (new_g > check->g) return AYSTAR_DONE;
		aystar->OpenListQueue.del(&aystar->OpenListQueue, check, 0);
		// It is lower, so change it to this item
		check->g = new_g;
		check->path.parent = closedlist_parent;
		/* Copy user data, will probably have changed */
		for (i = 0; i < lengthof(current->user_data); i++) {
			check->path.node.user_data[i] = current->user_data[i];
		}
		// Readd him in the OpenListQueue
		aystar->OpenListQueue.push(&aystar->OpenListQueue, check, new_f);
	} else {
		// A new node, add him to the OpenList
		AyStarMain_OpenList_Add(aystar, closedlist_parent, current, new_f, new_g);
	}

	return AYSTAR_DONE;
}

/*
 * This function is the core of AyStar. It handles one item and checks
 *  his neighbour items. If they are valid, they are added to be checked too.
 *  return values:
 *   AYSTAR_EMPTY_OPENLIST : indicates all items are tested, and no path
 *    has been found.
 *   AYSTAR_LIMIT_REACHED : Indicates that the max_nodes limit has been
 *    reached.
 *   AYSTAR_FOUND_END_NODE : indicates we found the end. Path_found now is true, and in path is the path found.
 *   AYSTAR_STILL_BUSY : indicates we have done this tile, did not found the path yet, and have items left to try.
 */
int AyStarMain_Loop(AyStar *aystar)
{
	int i, r;

	// Get the best node from OpenList
	OpenListNode *current = AyStarMain_OpenList_Pop(aystar);
	// If empty, drop an error
	if (current == NULL) return AYSTAR_EMPTY_OPENLIST;

	// Check for end node and if found, return that code
	if (aystar->EndNodeCheck(aystar, current) == AYSTAR_FOUND_END_NODE) {
		if (aystar->FoundEndNode != NULL)
			aystar->FoundEndNode(aystar, current);
		free(current);
		return AYSTAR_FOUND_END_NODE;
	}

	// Add the node to the ClosedList
	AyStarMain_ClosedList_Add(aystar, &current->path);

	// Load the neighbours
	aystar->GetNeighbours(aystar, current);

	// Go through all neighbours
	for (i = 0; i < aystar->num_neighbours; i++) {
		// Check and add them to the OpenList if needed
		r = aystar->checktile(aystar, &aystar->neighbours[i], current);
	}

	// Free the node
	free(current);

	if (aystar->max_search_nodes != 0 && Hash_Size(&aystar->ClosedListHash) >= aystar->max_search_nodes) {
		/* We've expanded enough nodes */
		return AYSTAR_LIMIT_REACHED;
	} else {
		// Return that we are still busy
		return AYSTAR_STILL_BUSY;
	}
}

/*
 * This function frees the memory it allocated
 */
void AyStarMain_Free(AyStar *aystar)
{
	aystar->OpenListQueue.free(&aystar->OpenListQueue, false);
	/* 2nd argument above is false, below is true, to free the values only
	 * once */
	delete_Hash(&aystar->OpenListHash, true);
	delete_Hash(&aystar->ClosedListHash, true);
#ifdef AYSTAR_DEBUG
	printf("[AyStar] Memory free'd\n");
#endif
}

/*
 * This function make the memory go back to zero
 *  This function should be called when you are using the same instance again.
 */
void AyStarMain_Clear(AyStar *aystar)
{
	// Clean the Queue, but not the elements within. That will be done by
	// the hash.
	aystar->OpenListQueue.clear(&aystar->OpenListQueue, false);
	// Clean the hashes
	clear_Hash(&aystar->OpenListHash, true);
	clear_Hash(&aystar->ClosedListHash, true);

#ifdef AYSTAR_DEBUG
	printf("[AyStar] Cleared AyStar\n");
#endif
}

/*
 * This is the function you call to run AyStar.
 *  return values:
 *   AYSTAR_FOUND_END_NODE : indicates we found an end node.
 *   AYSTAR_NO_PATH : indicates that there was no path found.
 *   AYSTAR_STILL_BUSY : indicates we have done some checked, that we did not found the path yet, and that we still have items left to try.
 * When the algorithm is done (when the return value is not AYSTAR_STILL_BUSY)
 * aystar->clear() is called. Note that when you stop the algorithm halfway,
 * you should still call clear() yourself!
 */
int AyStarMain_Main(AyStar *aystar) {
	int r, i = 0;
	// Loop through the OpenList
	//  Quit if result is no AYSTAR_STILL_BUSY or is more than loops_per_tick
	while ((r = aystar->loop(aystar)) == AYSTAR_STILL_BUSY && (aystar->loops_per_tick == 0 || ++i < aystar->loops_per_tick)) { }
#ifdef AYSTAR_DEBUG
	switch (r) {
		case AYSTAR_FOUND_END_NODE: printf("[AyStar] Found path!\n"); break;
		case AYSTAR_EMPTY_OPENLIST: printf("[AyStar] OpenList run dry, no path found\n"); break;
		case AYSTAR_LIMIT_REACHED:  printf("[AyStar] Exceeded search_nodes, no path found\n"); break;
		default: break;
	}
#endif
	if (r != AYSTAR_STILL_BUSY) {
		/* We're done, clean up */
		_aystar_stats_open_size = aystar->OpenListHash.size;
		_aystar_stats_closed_size = aystar->ClosedListHash.size;
		aystar->clear(aystar);
	}

	switch (r) {
		case AYSTAR_FOUND_END_NODE: return AYSTAR_FOUND_END_NODE;
		case AYSTAR_EMPTY_OPENLIST:
		case AYSTAR_LIMIT_REACHED:  return AYSTAR_NO_PATH;
		default:                    return AYSTAR_STILL_BUSY;
	}
}

/*
 * Adds a node from where to start an algorithm. Multiple nodes can be added
 * if wanted. You should make sure that clear() is called before adding nodes
 * if the AyStar has been used before (though the normal main loop calls
 * clear() automatically when the algorithm finishes
 * g is the cost for starting with this node.
 */
void AyStarMain_AddStartNode(AyStar *aystar, AyStarNode *start_node, uint g)
{
#ifdef AYSTAR_DEBUG
	printf("[AyStar] Starting A* Algorithm from node (%d, %d, %d)\n",
		TileX(start_node->tile), TileY(start_node->tile), start_node->direction);
#endif
	AyStarMain_OpenList_Add(aystar, NULL, start_node, 0, g);
}

void init_AyStar(AyStar *aystar, Hash_HashProc hash, uint num_buckets)
{
	// Allocated the Hash for the OpenList and ClosedList
	init_Hash(&aystar->OpenListHash, hash, num_buckets);
	init_Hash(&aystar->ClosedListHash, hash, num_buckets);

	// Set up our sorting queue
	//  BinaryHeap allocates a block of 1024 nodes
	//  When thatone gets full it reserves an otherone, till this number
	//  That is why it can stay this high
	init_BinaryHeap(&aystar->OpenListQueue, 102400);

	aystar->addstart  = AyStarMain_AddStartNode;
	aystar->main      = AyStarMain_Main;
	aystar->loop      = AyStarMain_Loop;
	aystar->free      = AyStarMain_Free;
	aystar->clear     = AyStarMain_Clear;
	aystar->checktile = AyStarMain_CheckTile;
}
