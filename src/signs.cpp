/* $Id$ */

/** @file signs.cpp Handling of signs. */

#include "stdafx.h"
#include "landscape.h"
#include "signs_base.h"
#include "signs_func.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "zoom_func.h"
#include "functions.h"
#include "core/pool_func.hpp"

#include "table/strings.h"

/* Initialize the sign-pool */
SignPool _sign_pool("Sign");
INSTANTIATE_POOL_METHODS(Sign)

Sign::Sign(Owner owner)
{
	this->owner = owner;
}

Sign::~Sign()
{
	free(this->name);

	if (CleaningPool()) return;

	DeleteRenameSignWindow(this->index);
}

/**
 *
 * Update the coordinate of one sign
 * @param si Pointer to the Sign
 *
 */
void UpdateSignVirtCoords(Sign *si)
{
	Point pt = RemapCoords(si->x, si->y, si->z);
	SetDParam(0, si->index);
	si->sign.UpdatePosition(pt.x, pt.y - 6, STR_SIGN_WHITE);
}

/** Update the coordinates of all signs */
void UpdateAllSignVirtCoords()
{
	Sign *si;

	FOR_ALL_SIGNS(si) UpdateSignVirtCoords(si);
}

/**
 *
 * Initialize the signs
 *
 */
void InitializeSigns()
{
	_sign_pool.CleanPool();
}
