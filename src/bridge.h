/* $Id$ */

/** @file bridge.h Header file for bridges */

#ifndef BRIDGE_H
#define BRIDGE_H

enum {
	MAX_BRIDGES = 13
};

/** Struct containing information about a single bridge type
 */
struct Bridge {
	Year avail_year;     ///< the year in which the bridge becomes available
	byte min_length;     ///< the minimum length of the bridge (not counting start and end tile)
	byte max_length;     ///< the maximum length of the bridge (not counting start and end tile)
	uint16 price;        ///< the relative price of the bridge
	uint16 speed;        ///< maximum travel speed
	SpriteID sprite;     ///< the sprite which is used in the GUI
	SpriteID pal;        ///< the palette which is used in the GUI
	StringID material;   ///< the string that contains the bridge description
	PalSpriteID **sprite_table; ///< table of sprites for drawing the bridge
	byte flags;          ///< bit 0 set: disable drawing of far pillars.
};

extern const Bridge orig_bridge[MAX_BRIDGES];
extern Bridge _bridge[MAX_BRIDGES];

Foundation GetBridgeFoundation(Slope tileh, Axis axis);

static inline const Bridge *GetBridge(uint i)
{
	assert(i < lengthof(_bridge));
	return &_bridge[i];
}

void DrawBridgeMiddle(const TileInfo *ti);

bool CheckBridge_Stuff(byte bridge_type, uint bridge_len);
uint32 GetBridgeLength(TileIndex begin, TileIndex end);
int CalcBridgeLenCostFactor(int x);

#endif /* BRIDGE_H */
