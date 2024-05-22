/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file signs.cpp Handling of signs. */

#include "stdafx.h"
#include "landscape.h"
#include "company_func.h"
#include "signs_base.h"
#include "signs_func.h"
#include "strings_func.h"
#include "core/pool_func.hpp"
#include "viewport_kdtree.h"

#include "table/strings.h"

#include "safeguards.h"

/** Initialize the sign-pool */
SignPool _sign_pool("Sign");
INSTANTIATE_POOL_METHODS(Sign)

/**
 * Creates a new sign
 */
Sign::Sign(Owner owner)
{
	this->owner = owner;
}

/** Destroy the sign */
Sign::~Sign()
{
	if (CleaningPool()) return;

	DeleteRenameSignWindow(this->index);
}

/**
 * Update the coordinate of one sign
 */
void Sign::UpdateVirtCoord()
{
	Point pt = RemapCoords(this->x, this->y, this->z);

	if (this->sign.kdtree_valid) _viewport_sign_kdtree.Remove(ViewportSignKdtreeItem::MakeSign(this->index));

	SetDParam(0, this->index);
	this->sign.UpdatePosition(pt.x, pt.y - 6 * ZOOM_BASE, STR_WHITE_SIGN);

	_viewport_sign_kdtree.Insert(ViewportSignKdtreeItem::MakeSign(this->index));
}

/** Update the coordinates of all signs */
void UpdateAllSignVirtCoords()
{
	for (Sign *si : Sign::Iterate()) {
		si->UpdateVirtCoord();
	}
}

/**
 * Check if the current company can rename a given sign.
 * @param *si The sign in question.
 * @return true if the sign can be renamed, else false.
 */
bool CompanyCanRenameSign(const Sign *si)
{
	if (si->owner == OWNER_DEITY && _current_company != OWNER_DEITY && _game_mode != GM_EDITOR) return false;
	return true;
}
