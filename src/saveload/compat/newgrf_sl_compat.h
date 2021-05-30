/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_sl_compat.h Loading of newgrf chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_NEWGRF_H
#define SAVELOAD_COMPAT_NEWGRF_H

#include "../saveload.h"

/** Original field order for _newgrf_mapping_desc. */
const SaveLoadCompat _newgrf_mapping_sl_compat[] = {
	SLC_VAR("grfid"),
	SLC_VAR("entity_id"),
	SLC_VAR("substitute_id"),
};

/** Original field order for _newgrf_desc. */
const SaveLoadCompat _grfconfig_sl_compat[] = {
	SLC_VAR("filename"),
	SLC_VAR("ident.grfid"),
	SLC_VAR("ident.md5sum"),
	SLC_VAR("version"),
	SLC_VAR("param"),
	SLC_VAR("num_params"),
	SLC_VAR("palette"),
};

#endif /* SAVELOAD_COMPAT_NEWGRF_H */
