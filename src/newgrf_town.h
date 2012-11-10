/* $Id$ */

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

/* Currently there is no direct town resolver; we only need to get town
 * variable results from inside stations, house tiles and industries,
 * and to check the town's persistent storage.
 * XXX Remove the functions. */
uint32 TownGetVariable(byte variable, uint32 parameter, bool *available, Town *t, const struct GRFFile *caller_grffile);
void TownStorePSA(Town *t, const struct GRFFile *caller_grffile, uint pos, int32 value);

struct TownScopeResolver : public ScopeResolver {
	Town *t;
	bool readonly;

	TownScopeResolver(ResolverObject *ro, Town *t, bool readonly);

	virtual uint32 GetVariable(byte variable, uint32 parameter, bool *available) const;
	virtual void StorePSA(uint reg, int32 value);
};

struct TownResolverObject : public ResolverObject {
	TownScopeResolver town_scope;

	TownResolverObject(const struct GRFFile *grffile, Town *t, bool readonly);

	/* virtual */ ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0)
	{
		switch (scope) {
			case VSG_SCOPE_SELF: return &town_scope;
			default: return &this->default_scope; // XXX return ResolverObject::GetScope(scope, relative);
		}
	}
};

#endif /* NEWGRF_TOWN_H */
