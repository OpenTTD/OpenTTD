/* $Id$ */

/** @file aystar.h
 * This file has the header for AyStar
 *  AyStar is a fast pathfinding routine and is used for things like
 *  AI_pathfinding and Train_pathfinding.
 *  For more information about AyStar (A* Algorithm), you can look at
 *   http://en.wikipedia.org/wiki/A-star_search_algorithm
 */

#ifndef AYSTAR_H
#define AYSTAR_H

#include "queue.h"

//#define AYSTAR_DEBUG
enum {
	AYSTAR_FOUND_END_NODE,
	AYSTAR_EMPTY_OPENLIST,
	AYSTAR_STILL_BUSY,
	AYSTAR_NO_PATH,
	AYSTAR_LIMIT_REACHED,
	AYSTAR_DONE
};

enum{
	AYSTAR_INVALID_NODE = -1,
};

typedef struct AyStarNode AyStarNode;
struct AyStarNode {
	TileIndex tile;
	int direction;
	uint user_data[2];
};

// The resulting path has nodes looking like this.
typedef struct PathNode PathNode;
struct PathNode {
	AyStarNode node;
	// The parent of this item
	PathNode *parent;
};

// For internal use only
// We do not save the h-value, because it is only needed to calculate the f-value.
//  h-value should _always_ be the distance left to the end-tile.
typedef struct OpenListNode OpenListNode;
struct OpenListNode {
	int g;
	PathNode path;
};

typedef struct AyStar AyStar;
/*
 * This function is called to check if the end-tile is found
 *  return values can be:
 *   AYSTAR_FOUND_END_NODE : indicates this is the end tile
 *   AYSTAR_DONE : indicates this is not the end tile (or direction was wrong)
 */
/*
 * The 2nd parameter should be OpenListNode, and NOT AyStarNode. AyStarNode is
 * part of OpenListNode and so it could be accessed without any problems.
 * The good part about OpenListNode is, and how AIs use it, that you can
 * access the parent of the current node, and so check if you, for example
 * don't try to enter the file tile with a 90-degree curve. So please, leave
 * this an OpenListNode, it works just fine -- TrueLight
 */
typedef int32 AyStar_EndNodeCheck(AyStar *aystar, OpenListNode *current);

/*
 * This function is called to calculate the G-value for AyStar Algorithm.
 *  return values can be:
 *   AYSTAR_INVALID_NODE : indicates an item is not valid (e.g.: unwalkable)
 *   Any value >= 0 : the g-value for this tile
 */
typedef int32 AyStar_CalculateG(AyStar *aystar, AyStarNode *current, OpenListNode *parent);

/*
 * This function is called to calculate the H-value for AyStar Algorithm.
 *  Mostly, this must result the distance (Manhattan way) between the
 *   current point and the end point
 *  return values can be:
 *   Any value >= 0 : the h-value for this tile
 */
typedef int32 AyStar_CalculateH(AyStar *aystar, AyStarNode *current, OpenListNode *parent);

/*
 * This function request the tiles around the current tile and put them in tiles_around
 *  tiles_around is never resetted, so if you are not using directions, just leave it alone.
 * Warning: never add more tiles_around than memory allocated for it.
 */
typedef void AyStar_GetNeighbours(AyStar *aystar, OpenListNode *current);

/*
 * If the End Node is found, this function is called.
 *  It can do, for example, calculate the route and put that in an array
 */
typedef void AyStar_FoundEndNode(AyStar *aystar, OpenListNode *current);

// For internal use, see aystar.c
typedef void AyStar_AddStartNode(AyStar *aystar, AyStarNode *start_node, uint g);
typedef int AyStar_Main(AyStar *aystar);
typedef int AyStar_Loop(AyStar *aystar);
typedef int AyStar_CheckTile(AyStar *aystar, AyStarNode *current, OpenListNode *parent);
typedef void AyStar_Free(AyStar *aystar);
typedef void AyStar_Clear(AyStar *aystar);

struct AyStar {
/* These fields should be filled before initting the AyStar, but not changed
 * afterwards (except for user_data and user_path)! (free and init again to change them) */

	/* These should point to the application specific routines that do the
	 * actual work */
	AyStar_CalculateG *CalculateG;
	AyStar_CalculateH *CalculateH;
	AyStar_GetNeighbours *GetNeighbours;
	AyStar_EndNodeCheck *EndNodeCheck;
	AyStar_FoundEndNode *FoundEndNode;

	/* These are completely untouched by AyStar, they can be accesed by
	 * the application specific routines to input and output data.
	 * user_path should typically contain data about the resulting path
	 * afterwards, user_target should typically contain information about
	 * what where looking for, and user_data can contain just about
	 * everything */
	void *user_path;
	void *user_target;
	uint user_data[10];

	/* How many loops are there called before AyStarMain_Main gives
	 * control back to the caller. 0 = until done */
	byte loops_per_tick;
	/* If the g-value goes over this number, it stops searching
	 *  0 = infinite */
	uint max_path_cost;
	/* The maximum amount of nodes that will be expanded, 0 = infinite */
	uint max_search_nodes;

	/* These should be filled with the neighbours of a tile by
	 * GetNeighbours */
	AyStarNode neighbours[12];
	byte num_neighbours;

	/* These will contain the methods for manipulating the AyStar. Only
	 * main() should be called externally */
	AyStar_AddStartNode *addstart;
	AyStar_Main *main;
	AyStar_Loop *loop;
	AyStar_Free *free;
	AyStar_Clear *clear;
	AyStar_CheckTile *checktile;

	/* These will contain the open and closed lists */

	/* The actual closed list */
	Hash ClosedListHash;
	/* The open queue */
	Queue OpenListQueue;
	/* An extra hash to speed up the process of looking up an element in
	 * the open list */
	Hash OpenListHash;
};


void AyStarMain_AddStartNode(AyStar *aystar, AyStarNode *start_node, uint g);
int AyStarMain_Main(AyStar *aystar);
int AyStarMain_Loop(AyStar *aystar);
int AyStarMain_CheckTile(AyStar *aystar, AyStarNode *current, OpenListNode *parent);
void AyStarMain_Free(AyStar *aystar);
void AyStarMain_Clear(AyStar *aystar);

/* Initialize an AyStar. You should fill all appropriate fields before
 * callling init_AyStar (see the declaration of AyStar for which fields are
 * internal */
void init_AyStar(AyStar *aystar, Hash_HashProc hash, uint num_buckets);


#endif /* AYSTAR_H */
