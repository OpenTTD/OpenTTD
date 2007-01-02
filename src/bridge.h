/* $Id$ */

/** @file bridge.h Header file for bridges */

#ifndef BRIDGE_H
#define BRIDGE_H

enum {
	MAX_BRIDGES = 13
};

/** Struct containing information about a single bridge type
 */
typedef struct Bridge {
	Year avail_year;     ///< the year in which the bridge becomes available
	byte min_length;     ///< the minimum length of the bridge (not counting start and end tile)
	byte max_length;     ///< the maximum length of the bridge (not counting start and end tile)
	uint16 price;        ///< the relative price of the bridge
	uint16 speed;        ///< maximum travel speed
	PalSpriteID sprite;  ///< the sprite which is used in the GUI (possibly with a recolor sprite)
	StringID material;   ///< the string that contains the bridge description
	PalSpriteID **sprite_table; ///< table of sprites for drawing the bridge
	byte flags;          ///< bit 0 set: disable drawing of far pillars.
} Bridge;

extern const Bridge orig_bridge[MAX_BRIDGES];
extern Bridge _bridge[MAX_BRIDGES];

uint GetBridgeFoundation(Slope tileh, Axis axis);
uint SetSpeedLimitOnBridge(Vehicle *);

#endif /* BRIDGE_H */
