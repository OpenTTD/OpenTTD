/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file engine_sl_compat.h Loading for engine chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_ENGINE_H
#define SAVELOAD_COMPAT_ENGINE_H

#include "../saveload.h"

/** Original field order for _engine_desc. */
const SaveLoadCompat _engine_sl_compat[] = {
	SLC_VAR("intro_date"),
	SLC_VAR("age"),
	SLC_VAR("reliability"),
	SLC_VAR("reliability_spd_dec"),
	SLC_VAR("reliability_start"),
	SLC_VAR("reliability_max"),
	SLC_VAR("reliability_final"),
	SLC_VAR("duration_phase_1"),
	SLC_VAR("duration_phase_2"),
	SLC_VAR("duration_phase_3"),
	SLC_NULL(1, SL_MIN_VERSION, SLV_121),
	SLC_VAR("flags"),
	SLC_NULL(1, SL_MIN_VERSION, SLV_179),
	SLC_VAR("preview_asked"),
	SLC_VAR("preview_company"),
	SLC_VAR("preview_wait"),
	SLC_NULL(1, SL_MIN_VERSION,  SLV_45),
	SLC_VAR("company_avail"),
	SLC_VAR("company_hidden"),
	SLC_VAR("name"),
	SLC_NULL(16, SLV_2, SLV_144),
};

/** Original field order for _engine_id_mapping_desc. */
const SaveLoadCompat _engine_id_mapping_sl_compat[] = {
	SLC_VAR("grfid"),
	SLC_VAR("internal_id"),
	SLC_VAR("type"),
	SLC_VAR("substitute_id"),
};

#endif /* SAVELOAD_COMPAT_ENGINE_H */
