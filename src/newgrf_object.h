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
#include "newgrf_spritegroup.h"
#include "newgrf_town.h"
#include "economy_func.h"
#include "date_type.h"
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
	OBJECT_FLAG_ANIM_RANDOM_BITS   = 1 << 12, ///< Object wants random bits in "next animation frame" callback.
	OBJECT_FLAG_SCALE_BY_WATER     = 1 << 13, ///< Object count is roughly scaled by water amount at edges.
};
DECLARE_ENUM_AS_BIT_SET(ObjectFlags)

void ResetObjects();

/** Class IDs for objects. */
enum ObjectClassID {
	OBJECT_CLASS_BEGIN   =    0, ///< The lowest valid value
	OBJECT_CLASS_MAX     = 0xFF, ///< Maximum number of classes.
	INVALID_OBJECT_CLASS = 0xFF, ///< Class for the less fortunate.
};
/** Allow incrementing of ObjectClassID variables */
DECLARE_POSTFIX_INCREMENT(ObjectClassID)

/** An object that isn't use for transport, industries or houses.
 * @note If you change this struct, adopt the initialization of
 * default objects in table/object_land.h
 */
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
	uint8 generate_amount;        ///< Number of objects which are attempted to be generated per 256^2 map during world generation.
	bool enabled;                 ///< Is this spec enabled?

	/**
	 * Get the cost for building a structure of this type.
	 * @return The cost for building.
	 */
	Money GetBuildCost() const { return GetPrice(PR_BUILD_OBJECT, this->build_cost_multiplier, this->grf_prop.grffile, 0); }

	/**
	 * Get the cost for clearing a structure of this type.
	 * @return The cost for clearing.
	 */
	Money GetClearCost() const { return GetPrice(PR_CLEAR_OBJECT, this->clear_cost_multiplier, this->grf_prop.grffile, 0); }

	bool IsEverAvailable() const;
	bool WasEverAvailable() const;
	bool IsAvailable() const;
	uint Index() const;

	static const ObjectSpec *Get(ObjectType index);
	static const ObjectSpec *GetByTile(TileIndex tile);
};

/** Object scope resolver. */
struct ObjectScopeResolver : public ScopeResolver {
	struct Object *obj; ///< The object the callback is ran for.
	TileIndex tile;     ///< The tile related to the object.
	uint8 view;         ///< The view of the object.

	/**
	 * Constructor of an object scope resolver.
	 * @param ro Surrounding resolver.
	 * @param obj Object being resolved.
	 * @param tile %Tile of the object.
	 * @param view View of the object.
	 */
	ObjectScopeResolver(ResolverObject &ro, Object *obj, TileIndex tile, uint8 view = 0)
		: ScopeResolver(ro), obj(obj), tile(tile), view(view)
	{
	}

	/* virtual */ uint32 GetRandomBits() const;
	/* virtual */ uint32 GetVariable(byte variable, uint32 parameter, bool *available) const;
};

/** A resolver object to be used with feature 0F spritegroups. */
struct ObjectResolverObject : public ResolverObject {
	ObjectScopeResolver object_scope; ///< The object scope resolver.
	TownScopeResolver *town_scope;    ///< The town scope resolver (created on the first call).

	ObjectResolverObject(const ObjectSpec *spec, Object *o, TileIndex tile, uint8 view = 0,
			CallbackID callback = CBID_NO_CALLBACK, uint32 param1 = 0, uint32 param2 = 0);
	~ObjectResolverObject();

	/* virtual */ ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0)
	{
		switch (scope) {
			case VSG_SCOPE_SELF:
				return &this->object_scope;

			case VSG_SCOPE_PARENT: {
				TownScopeResolver *tsr = this->GetTown();
				if (tsr != NULL) return tsr;
				FALLTHROUGH;
			}

			default:
				return ResolverObject::GetScope(scope, relative);
		}
	}

private:
	TownScopeResolver *GetTown();
};

/** Struct containing information relating to object classes. */
typedef NewGRFClass<ObjectSpec, ObjectClassID, OBJECT_CLASS_MAX> ObjectClass;

/** Mapping of purchase for objects. */
static const CargoID CT_PURCHASE_OBJECT = 1;

uint16 GetObjectCallback(CallbackID callback, uint32 param1, uint32 param2, const ObjectSpec *spec, Object *o, TileIndex tile, uint8 view = 0);

void DrawNewObjectTile(TileInfo *ti, const ObjectSpec *spec);
void DrawNewObjectTileInGUI(int x, int y, const ObjectSpec *spec, uint8 view);
void AnimateNewObjectTile(TileIndex tile);
void TriggerObjectTileAnimation(Object *o, TileIndex tile, ObjectAnimationTrigger trigger, const ObjectSpec *spec);
void TriggerObjectAnimation(Object *o, ObjectAnimationTrigger trigger, const ObjectSpec *spec);

#endif /* NEWGRF_OBJECT_H */
