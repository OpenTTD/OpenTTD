/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cheat.cpp Handling (loading/saving/initializing) of cheats. */

#include "stdafx.h"
#include "cheat_type.h"

Cheats _cheats;

void InitializeCheats()
{
	memset(&_cheats, 0, sizeof(Cheats));
}

/**
 * Return true if any cheat has been used, false otherwise
 * @return has a cheat been used?
 */
bool CheatHasBeenUsed()
{
	/* Cannot use lengthof because _cheats is of type Cheats, not Cheat */
	const Cheat *cht = (Cheat*)&_cheats;
	const Cheat *cht_last = &cht[sizeof(_cheats) / sizeof(Cheat)];

	for (; cht != cht_last; cht++) {
		if (cht->been_used) return true;
	}

	return false;
}
