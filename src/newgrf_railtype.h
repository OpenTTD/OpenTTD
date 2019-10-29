/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_railtype.h NewGRF handling of rail types. */

#ifndef NEWGRF_RAILTYPE_H
#define NEWGRF_RAILTYPE_H

#include "rail.h"
#include "newgrf_commons.h"
#include "newgrf_spritegroup.h"

/** Resolver for the railtype scope. */
struct RailTypeScopeResolver : public ScopeResolver {
	TileIndex tile;      ///< Tracktile. For track on a bridge this is the southern bridgehead.
	TileContext context; ///< Are we resolving sprites for the upper halftile, or on a bridge?

	/**
	 * Constructor of the railtype scope resolvers.
	 * @param ro Surrounding resolver.
	 * @param tile %Tile containing the track. For track on a bridge this is the southern bridgehead.
	 * @param context Are we resolving sprites for the upper halftile, or on a bridge?
	 */
	RailTypeScopeResolver(ResolverObject &ro, TileIndex tile, TileContext context)
		: ScopeResolver(ro), tile(tile), context(context)
	{
	}

	uint32 GetRandomBits() const override;
	uint32 GetVariable(byte variable, uint32 parameter, bool *available) const override;
};

/** Resolver object for rail types. */
struct RailTypeResolverObject : public ResolverObject {
	RailTypeScopeResolver railtype_scope; ///< Resolver for the railtype scope.

	RailTypeResolverObject(const RailtypeInfo *rti, TileIndex tile, TileContext context, RailTypeSpriteGroup rtsg, uint32 param1 = 0, uint32 param2 = 0);

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0) override
	{
		switch (scope) {
			case VSG_SCOPE_SELF: return &this->railtype_scope;
			default:             return ResolverObject::GetScope(scope, relative);
		}
	}

	const SpriteGroup *ResolveReal(const RealSpriteGroup *group) const override;
};

SpriteID GetCustomRailSprite(const RailtypeInfo *rti, TileIndex tile, RailTypeSpriteGroup rtsg, TileContext context = TCX_NORMAL, uint *num_results = nullptr);
SpriteID GetCustomSignalSprite(const RailtypeInfo *rti, TileIndex tile, SignalType type, SignalVariant var, SignalState state, bool gui = false);

uint8 GetReverseRailTypeTranslation(RailType railtype, const GRFFile *grffile);

#endif /* NEWGRF_RAILTYPE_H */
