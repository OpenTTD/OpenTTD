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
#include "oldpool_func.h"

#include "table/strings.h"

/* Initialize the sign-pool */
DEFINE_OLD_POOL_GENERIC(Sign, Sign)

Sign::Sign(Owner owner)
{
	this->owner = owner;
}

Sign::~Sign()
{
	free(this->name);

	if (CleaningPool()) return;

	DeleteRenameSignWindow(this->index);
	this->owner = INVALID_OWNER;
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
	UpdateViewportSignPos(&si->sign, pt.x, pt.y - 6, STR_2806);
}

/** Update the coordinates of all signs */
void UpdateAllSignVirtCoords()
{
	Sign *si;

	FOR_ALL_SIGNS(si) UpdateSignVirtCoords(si);
}

/**
 * Marks the region of a sign as dirty.
 *
 * This function marks the sign in all viewports as dirty for repaint.
 *
 * @param si Pointer to the Sign
 * @ingroup dirty
 */
void MarkSignDirty(Sign *si)
{
	/* We use ZOOM_LVL_MAX here, as every viewport can have an other zoom,
	 *  and there is no way for us to know which is the biggest. So make the
	 *  biggest area dirty, and we are safe for sure. */
	MarkAllViewportsDirty(
		si->sign.left - 6,
		si->sign.top  - 3,
		si->sign.left + ScaleByZoom(si->sign.width_1 + 12, ZOOM_LVL_MAX),
		si->sign.top  + ScaleByZoom(12, ZOOM_LVL_MAX));
}

/**
 *
 * Initialize the signs
 *
 */
void InitializeSigns()
{
	_Sign_pool.CleanPool();
	_Sign_pool.AddBlockToPool();
}
