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
	/* Cargo action 2s should always have only 1 "loaded" state */
	if (group->g.real.num_loaded == 0) return NULL;

	return group->g.real.loaded[0];
}


static void NewCargoResolver(ResolverObject *res, const CargoSpec *cs)
{
	res->GetRandomBits = &CargoGetRandomBits;
	res->GetTriggers   = &CargoGetTriggers;
	res->SetTriggers   = &CargoSetTriggers;
	res->GetVariable   = &CargoGetVariable;
	res->ResolveReal   = &CargoResolveReal;

	res->u.cargo.cs = cs;

	res->callback        = 0;
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


uint16 GetCargoCallback(uint16 callback, uint32 param1, uint32 param2, const CargoSpec *cs)
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
