/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "landscape.h"
#include "debug.h"
#include "newgrf.h"
#include "newgrf_callbacks.h"
#include "newgrf_commons.h"
#include "newgrf_spritegroup.h"
#include "newgrf_generic.h"
#include "tile_map.h"
#include <list>


struct GenericCallback {
	const GRFFile *file;
	const SpriteGroup *group;

	GenericCallback(const GRFFile *file, const SpriteGroup *group) :
		file(file),
		group(group)
	{ }
};

typedef std::list<GenericCallback> GenericCallbackList;

static GenericCallbackList _gcl[GSF_END];


/**
 * Reset all generic feature callback sprite groups.
 */
void ResetGenericCallbacks()
{
	for (uint8 feature = 0; feature < lengthof(_gcl); feature++) {
		_gcl[feature].clear();
	}
}


/**
 * Add a generic feature callback sprite group to the appropriate feature list.
 * @param feature
 * @param file
 * @param group
 */
void AddGenericCallback(uint8 feature, const GRFFile *file, const SpriteGroup *group)
{
	if (feature >= lengthof(_gcl)) {
		grfmsg(5, "AddGenericCallback: Unsupported feature 0x%02X", feature);
		return;
	}

	/* Generic feature callbacks are evaluated in reverse (i.e. the last group
	 * to be added is evaluated first, etc) thus we push the group to the
	 * beginning of the list so a standard iterator will do the right thing. */
	_gcl[feature].push_front(GenericCallback(file, group));
}


static uint32 GenericCallbackGetRandomBits(const ResolverObject *object)
{
	return 0;
}


static uint32 GenericCallbackGetTriggers(const ResolverObject *object)
{
	return 0;
}


static void GenericCallbackSetTriggers(const ResolverObject *object, int triggers)
{
	return;
}


static uint32 GenericCallbackGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	switch (variable) {
		case 0x40: return object->u.generic.cargo_type;

		case 0x80: return object->u.generic.cargo_type;
		case 0x81: return object->u.generic.cargo_type;
		case 0x82: return object->u.generic.default_selection;
		case 0x83: return object->u.generic.src_industry;
		case 0x84: return object->u.generic.dst_industry;
		case 0x85: return object->u.generic.distance;
		case 0x86: return object->u.generic.event;
		case 0x87: return object->u.generic.count;
		case 0x88: return object->u.generic.station_size;

		default: break;
	}

	DEBUG(grf, 1, "Unhandled generic feature property 0x%02X", variable);

	*available = false;
	return 0;
}


static const SpriteGroup *GenericCallbackResolveReal(const ResolverObject *object, const SpriteGroup *group)
{
	if (group->g.real.num_loaded == 0) return NULL;

	return group->g.real.loaded[0];
}


static inline void NewGenericResolver(ResolverObject *res)
{
	res->GetRandomBits = &GenericCallbackGetRandomBits;
	res->GetTriggers   = &GenericCallbackGetTriggers;
	res->SetTriggers   = &GenericCallbackSetTriggers;
	res->GetVariable   = &GenericCallbackGetVariable;
	res->ResolveReal   = &GenericCallbackResolveReal;

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
}


/** Follow a generic feature callback list and return the first successful
 * answer
 * @param feature GRF Feature of callback
 * @param object  pre-populated resolver object
 * @param file    address of GRFFile object if file reference is needed, NULL is valid
 * @return callback value if successful or CALLBACK_FAILED
 */
static uint16 GetGenericCallbackResult(uint8 feature, ResolverObject *object, const GRFFile **file)
{
	assert(feature < lengthof(_gcl));

	/* Test each feature callback sprite group. */
	for (GenericCallbackList::const_iterator it = _gcl[feature].begin(); it != _gcl[feature].end(); ++it) {
		const SpriteGroup *group = it->group;
		group = Resolve(group, object);
		if (group == NULL || group->type != SGT_CALLBACK) continue;

		/* Return NewGRF file if necessary */
		if (file != NULL) *file = it->file;

		return group->g.callback.result;
	}

	/* No callback returned a valid result, so we've failed. */
	return CALLBACK_FAILED;
}


/**
 * 'Execute' an AI purchase selection callback
 */
uint16 GetAiPurchaseCallbackResult(uint8 feature, CargoID cargo_type, uint8 default_selection, IndustryType src_industry, IndustryType dst_industry, uint8 distance, AIConstructionEvent event, uint8 count, uint8 station_size, const GRFFile **file)
{
	ResolverObject object;

	NewGenericResolver(&object);

	object.callback = CBID_GENERIC_AI_PURCHASE_SELECTION;
	object.u.generic.cargo_type        = cargo_type;
	object.u.generic.default_selection = default_selection;
	object.u.generic.src_industry      = src_industry;
	object.u.generic.dst_industry      = dst_industry;
	object.u.generic.distance          = distance;
	object.u.generic.event             = event;
	object.u.generic.count             = count;
	object.u.generic.station_size      = station_size;

	return GetGenericCallbackResult(feature, &object, file);
}
