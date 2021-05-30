/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file labelmaps_sl_compat.h Loading of labelmaps chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_LABELMAPS_H
#define SAVELOAD_COMPAT_LABELMAPS_H

#include "../saveload.h"

/** Original field order for _label_object_desc. */
const SaveLoadCompat _label_object_sl_compat[] = {
	SLC_VAR("label"),
};

#endif /* SAVELOAD_COMPAT_LABELMAPS_H */
