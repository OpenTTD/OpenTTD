/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file progress.cpp Functions for modal progress windows. */

#include "stdafx.h"
#include "progress.h"

#include "safeguards.h"

/** Are we in a modal progress or not? */
bool _in_modal_progress = false;

/**
 * Set the modal progress state.
 * @param state The new state; are we modal or not?
 */
void SetModalProgress(bool state)
{
	_in_modal_progress = state;
}
