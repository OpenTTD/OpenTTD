/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_roadtype.h NewGRF handling of road types. */

#ifndef NEWGRF_ROADTYPE_H
#define NEWGRF_ROADTYPE_H

#include "road.h"
#include "newgrf_commons.h"
#include "newgrf_spritegroup.h"

/** Resolver for the railtype scope. */
struct RoadTypeScopeResolver : public ScopeResolver {
	TileIndex tile;      ///< Tracktile. For track on a bridge this is the southern bridgehead.
	TileContext context; ///< Are we resolving sprites for the upper halftile, or on a bridge?

	RoadTypeScopeResolver(ResolverObject &ro, TileIndex tile, TileContext context);

	/* virtual */ uint32 GetRandomBits() const;
	/* virtual */ uint32 GetVariable(byte variable, uint32 parameter, bool *available) const;
};

/** Resolver object for road types. */
struct RoadTypeResolverObject : public ResolverObject {
	RoadTypeScopeResolver roadtype_scope; ///< Resolver for the roadtype scope.

	RoadTypeResolverObject(const RoadtypeInfo *rti, TileIndex tile, TileContext context, RoadTypeSpriteGroup rtsg, uint32 param1 = 0, uint32 param2 = 0);

	/* virtual */ ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0)
	{
		switch (scope) {
			case VSG_SCOPE_SELF: return &this->roadtype_scope;
			default:             return ResolverObject::GetScope(scope, relative);
		}
	}

	/* virtual */ const SpriteGroup *ResolveReal(const RealSpriteGroup *group) const;
};

SpriteID GetCustomRoadSprite(const RoadtypeInfo *rti, TileIndex tile, RoadTypeSpriteGroup rtsg, TileContext context = TCX_NORMAL, uint *num_results = NULL);

uint8 GetReverseRoadTypeTranslation(RoadTypeIdentifier rtid, const GRFFile *grffile);

#endif /* NEWGRF_ROADTYPE_H */
