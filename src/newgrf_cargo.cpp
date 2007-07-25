/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "cargotype.h"
#include "newgrf.h"
#include "newgrf_callbacks.h"
#include "newgrf_spritegroup.h"
#include "newgrf_cargo.h"


static uint32 CargoGetRandomBits(const ResolverObject *object)
{
	return 0;
}


static uint32 CargoGetTriggers(const ResolverObject *object)
{
	return 0;
}


static void CargoSetTriggers(const ResolverObject *object, int triggers)
{
	return;
}


static uint32 CargoGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	DEBUG(grf, 1, "Unhandled cargo property 0x%X", variable);

	*available = false;
	return 0;
}


static const SpriteGroup *CargoResolveReal(const ResolverObject *object, const SpriteGroup *group)
{
	/* Cargo action 2s should always have only 1 "loaded" state, but some
	 * times things don't follow the spec... */
	if (group->g.real.num_loaded > 0) return group->g.real.loaded[0];
	if (group->g.real.num_loading > 0) return group->g.real.loading[0];

	return NULL;
}


static void NewCargoResolver(ResolverObject *res, const CargoSpec *cs)
{
	res->GetRandomBits = &CargoGetRandomBits;
	res->GetTriggers   = &CargoGetTriggers;
	res->SetTriggers   = &CargoSetTriggers;
	res->GetVariable   = &CargoGetVariable;
	res->ResolveReal   = &CargoResolveReal;

	res->u.cargo.cs = cs;

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
}


SpriteID GetCustomCargoSprite(const CargoSpec *cs)
{
	const SpriteGroup *group;
	ResolverObject object;

	NewCargoResolver(&object, cs);

	group = Resolve(cs->group, &object);
	if (group == NULL || group->type != SGT_RESULT) return 0;

	return group->g.result.sprite;
}


uint16 GetCargoCallback(CallbackID callback, uint32 param1, uint32 param2, const CargoSpec *cs)
{
	ResolverObject object;
	const SpriteGroup *group;

	NewCargoResolver(&object, cs);
	object.callback = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	group = Resolve(cs->group, &object);
	if (group == NULL || group->type != SGT_CALLBACK) return CALLBACK_FAILED;

	return group->g.callback.result;
}


CargoID GetCargoTranslation(uint8 cargo, const GRFFile *grffile)
{
	/* Pre-version 7 uses the 'climate dependent' ID, i.e. cargo is the cargo ID */
	if (grffile->grf_version < 7) return HASBIT(_cargo_mask, cargo) ? cargo : (CargoID) CT_INVALID;

	/* If the GRF contains a translation table (and the cargo is in bounds)
	 * then get the cargo ID for the label */
	if (cargo < grffile->cargo_max) return GetCargoIDByLabel(grffile->cargo_list[cargo]);

	/* Else the cargo value is a 'climate independent' 'bitnum' */
	return GetCargoIDByBitnum(cargo);
}

uint8 GetReverseCargoTranslation(CargoID cargo, const GRFFile *grffile)
{
	/* Pre-version 7 uses the 'climate dependent' ID, i.e. cargo is the cargo ID */
	if (grffile->grf_version < 7) return cargo;

	const CargoSpec *cs = GetCargo(cargo);

	/* If the GRF contains a translation table (and the cargo is in the table)
	 * then get the cargo ID for the label */
	for (uint i = 0; i < grffile->cargo_max; i++) {
		if (cs->label == grffile->cargo_list[i]) return i;
	}

	/* No matching label was found, so we return the 'climate independent' 'bitnum' */
	return cs->bitnum;;
}
