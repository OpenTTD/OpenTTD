/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_town.h Functions to handle the town part of NewGRF towns. */

#ifndef NEWGRF_TOWN_H
#define NEWGRF_TOWN_H

#include "town_type.h"
#include "newgrf_spritegroup.h"

/**
 * Scope resolver for a town.
 * @note Currently there is no direct town resolver; we only need to get town
 *       variable results from inside stations, house tiles and industries,
 *       and to check the town's persistent storage.
 */
struct TownScopeResolver : public ScopeResolver {
	Town *t;       ///< %Town of the scope.
	bool readonly; ///< When set, persistent storage of the town is read-only,

	/**
	 * Resolver of a town scope.
	 * @param ro Surrounding resolver.
	 * @param t %Town of the scope.
	 * @param readonly Scope may change persistent storage of the town.
	 */
	TownScopeResolver(ResolverObject &ro, Town *t, bool readonly)
		: ScopeResolver(ro), t(t), readonly(readonly)
	{
	}

	uint32_t GetVariable(byte variable, [[maybe_unused]] uint32_t parameter, bool *available) const override;
	void StorePSA(uint reg, int32_t value) override;
};

/** Resolver of town properties. */
struct TownResolverObject : public ResolverObject {
	TownScopeResolver town_scope; ///< Scope resolver specific for towns.

	TownResolverObject(const struct GRFFile *grffile, Town *t, bool readonly);

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0) override
	{
		switch (scope) {
			case VSG_SCOPE_SELF: return &town_scope;
			default: return ResolverObject::GetScope(scope, relative);
		}
	}
};

#endif /* NEWGRF_TOWN_H */
