/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_object.h Functions related to NewGRF objects. */

#ifndef NEWGRF_OBJECT_H
#define NEWGRF_OBJECT_H

#include "newgrf_callbacks.h"
#include "economy_func.h"
#include "tile_cmd.h"
#include "object_type.h"
#include "newgrf_animation_type.h"
#include "newgrf_class.h"
#include "newgrf_commons.h"

/** Various object behaviours. */
enum ObjectFlags {
	OBJECT_FLAG_NONE               =       0, ///< Just nothing.
	OBJECT_FLAG_ONLY_IN_SCENEDIT   = 1 <<  0, ///< Object can only be constructed in the scenario editor.
	OBJECT_FLAG_CANNOT_REMOVE      = 1 <<  1, ///< Object can not be removed.
	OBJECT_FLAG_AUTOREMOVE         = 1 <<  2, ///< Object get automatically removed (like "owned land").
	OBJECT_FLAG_BUILT_ON_WATER     = 1 <<  3, ///< Object can be built on water (not required).
	OBJECT_FLAG_CLEAR_INCOME       = 1 <<  4, ///< When object is cleared a positive income is generated instead of a cost.
	OBJECT_FLAG_HAS_NO_FOUNDATION  = 1 <<  5, ///< Do not display foundations when on a slope.
	OBJECT_FLAG_ANIMATION          = 1 <<  6, ///< Object has animated tiles.
	OBJECT_FLAG_ONLY_IN_GAME       = 1 <<  7, ///< Object can only be built in game.
	OBJECT_FLAG_2CC_COLOUR         = 1 <<  8, ///< Object wants 2CC colour mapping.
	OBJECT_FLAG_NOT_ON_LAND        = 1 <<  9, ///< Object can not be on land, implicitly sets #OBJECT_FLAG_BUILT_ON_WATER.
	OBJECT_FLAG_DRAW_WATER         = 1 << 10, ///< Object wants to be drawn on water.
	OBJECT_FLAG_ALLOW_UNDER_BRIDGE = 1 << 11, ///< Object can built under a bridge.
	OBJECT_FLAG_ANIM_RANDOM_BITS   = 1 << 12, ///< Object wants random bits in "next animation frame" callback
};
DECLARE_ENUM_AS_BIT_SET(ObjectFlags)

void ResetObjects();

/** Class IDs for objects. */
enum ObjectClassID {
	OBJECT_CLASS_BEGIN   =    0, ///< The lowest valid value
	OBJECT_CLASS_MAX     =   32, ///< Maximum number of classes.
	INVALID_OBJECT_CLASS = 0xFF, ///< Class for the less fortunate.
};
/** Allow incrementing of ObjectClassID variables */
DECLARE_POSTFIX_INCREMENT(ObjectClassID)

/** An object that isn't use for transport, industries or houses. */
struct ObjectSpec {
	/* 2 because of the "normal" and "buy" sprite stacks. */
	GRFFilePropsBase<2> grf_prop; ///< Properties related the the grf file
	ObjectClassID cls_id;         ///< The class to which this spec belongs.
	StringID name;                ///< The name for this object.

	uint8 climate;                ///< In which climates is this object available?
	uint8 size;                   ///< The size of this objects; low nibble for X, high nibble for Y.
	uint8 build_cost_multiplier;  ///< Build cost multiplier per tile.
	uint8 clear_cost_multiplier;  ///< Clear cost multiplier per tile.
	Date introduction_date;       ///< From when can this object be built.
	Date end_of_life_date;        ///< When can't this object be built anymore.
	ObjectFlags flags;            ///< Flags/settings related to the object.
	AnimationInfo animation;      ///< Information about the animation.
	uint16 callback_mask;         ///< Bitmask of requested/allowed callbacks.
	uint8 height;                 ///< The height of this structure, in heightlevels; max MAX_TILE_HEIGHT.
	uint8 views;                  ///< The number of views.
	bool enabled;                 ///< Is this spec enabled?

	/**
	 * Get the cost for building a structure of this type.
	 * @return The cost for building.
	 */
	Money GetBuildCost() const { return (_price[PR_BUILD_OBJECT] * this->build_cost_multiplier); }

	/**
	 * Get the cost for clearing a structure of this type.
	 * @return The cost for clearing.
	 */
	Money GetClearCost() const { return (_price[PR_CLEAR_OBJECT] * this->clear_cost_multiplier); }

	bool IsAvailable() const;
	uint Index() const;

	static const ObjectSpec *Get(ObjectType index);
	static const ObjectSpec *GetByTile(TileIndex tile);
};

/** Struct containing information relating to station classes. */
typedef NewGRFClass<ObjectSpec, ObjectClassID, OBJECT_CLASS_MAX> ObjectClass;

/** Mapping of purchase for objects. */
static const CargoID CT_PURCHASE_OBJECT = 1;

uint16 GetObjectCallback(CallbackID callback, uint32 param1, uint32 param2, const ObjectSpec *spec, const Object *o, TileIndex tile, uint8 view = 0);

void DrawNewObjectTile(TileInfo *ti, const ObjectSpec *spec);
void DrawNewObjectTileInGUI(int x, int y, const ObjectSpec *spec, uint8 view);
void AnimateNewObjectTile(TileIndex tile);
void TriggerObjectTileAnimation(const Object *o, TileIndex tile, ObjectAnimationTrigger trigger, const ObjectSpec *spec);
void TriggerObjectAnimation(const Object *o, ObjectAnimationTrigger trigger, const ObjectSpec *spec);

#endif /* NEWGRF_OBJECT_H */
