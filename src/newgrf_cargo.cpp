/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_cargo.cpp Implementation of NewGRF cargoes. */

#include "stdafx.h"
#include "debug.h"
#include "newgrf_spritegroup.h"

#include "safeguards.h"

/** Resolver of cargo. */
struct CargoResolverObject : public ResolverObject {
	const CargoSpec *cargospec;

	CargoResolverObject(const CargoSpec *cs, CallbackID callback = CBID_NO_CALLBACK, uint32_t callback_param1 = 0, uint32_t callback_param2 = 0);

	GrfSpecFeature GetFeature() const override;
	uint32_t GetDebugID() const override;
};

GrfSpecFeature CargoResolverObject::GetFeature() const
{
	return GSF_CARGOES;
}

uint32_t CargoResolverObject::GetDebugID() const
{
	return this->cargospec->label;
}

/**
 * Constructor of the cargo resolver.
 * @param cs Cargo being resolved.
 * @param callback Callback ID.
 * @param callback_param1 First parameter (var 10) of the callback.
 * @param callback_param2 Second parameter (var 18) of the callback.
 */
CargoResolverObject::CargoResolverObject(const CargoSpec *cs, CallbackID callback, uint32_t callback_param1, uint32_t callback_param2)
		: ResolverObject(cs->grffile, callback, callback_param1, callback_param2), cargospec(cs)
{
	this->root_spritegroup = cs->group;
}

/**
 * Get the custom sprite for the given cargo type.
 * @param cs Cargo being queried.
 * @return Custom sprite to draw, or \c 0 if not available.
 */
SpriteID GetCustomCargoSprite(const CargoSpec *cs)
{
	CargoResolverObject object(cs);
	const SpriteGroup *group = object.Resolve();
	if (group == nullptr) return 0;

	return group->GetResult();
}


uint16_t GetCargoCallback(CallbackID callback, uint32_t param1, uint32_t param2, const CargoSpec *cs)
{
	CargoResolverObject object(cs, callback, param1, param2);
	return object.ResolveCallback();
}

/**
 * Translate a GRF-local cargo slot/bitnum into a CargoID.
 * @param cargo   GRF-local cargo slot/bitnum.
 * @param grffile Originating GRF file.
 * @param usebit  Defines the meaning of \a cargo for GRF version < 7.
 *                If true, then \a cargo is a bitnum. If false, then \a cargo is a cargoslot.
 *                For GRF version >= 7 \a cargo is always a translated cargo bit.
 * @return CargoID or CT_INVALID if the cargo is not available.
 */
CargoID GetCargoTranslation(uint8_t cargo, const GRFFile *grffile, bool usebit)
{
	/* Pre-version 7 uses the 'climate dependent' ID in callbacks and properties, i.e. cargo is the cargo ID */
	if (grffile->grf_version < 7 && !usebit) {
		if (cargo >= CargoSpec::GetArraySize() || !CargoSpec::Get(cargo)->IsValid()) return CT_INVALID;
		return cargo;
	}

	/* Other cases use (possibly translated) cargobits */

	if (!grffile->cargo_list.empty()) {
		/* ...and the cargo is in bounds, then get the cargo ID for
		 * the label */
		if (cargo < grffile->cargo_list.size()) return GetCargoIDByLabel(grffile->cargo_list[cargo]);
	} else {
		/* Else the cargo value is a 'climate independent' 'bitnum' */
		return GetCargoIDByBitnum(cargo);
	}
	return CT_INVALID;
}
