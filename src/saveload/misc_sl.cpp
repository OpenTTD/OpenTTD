/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file misc_sl.cpp Saving and loading of things that didn't fit anywhere else */

#include "../stdafx.h"
#include "../date_func.h"
#include "../zoom_func.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../viewport_func.h"
#include "../gfx_func.h"
#include "../core/random_func.hpp"
#include "../fios.h"

#include "saveload.h"

#include "../safeguards.h"

extern TileIndex _cur_tileloop_tile;
extern uint16 _disaster_delay;
extern byte _trees_tick_ctr;

/* Keep track of current game position */
int _saved_scrollpos_x;
int _saved_scrollpos_y;
ZoomLevelByte _saved_scrollpos_zoom;

void SaveViewportBeforeSaveGame()
{
	const Window *w = FindWindowById(WC_MAIN_WINDOW, 0);

	if (w != nullptr) {
		_saved_scrollpos_x = w->viewport->scrollpos_x;
		_saved_scrollpos_y = w->viewport->scrollpos_y;
		_saved_scrollpos_zoom = w->viewport->zoom;
	}
}

void ResetViewportAfterLoadGame()
{
	Window *w = FindWindowById(WC_MAIN_WINDOW, 0);

	w->viewport->scrollpos_x = _saved_scrollpos_x;
	w->viewport->scrollpos_y = _saved_scrollpos_y;
	w->viewport->dest_scrollpos_x = _saved_scrollpos_x;
	w->viewport->dest_scrollpos_y = _saved_scrollpos_y;

	ViewPort *vp = w->viewport;
	vp->zoom = (ZoomLevel)min(_saved_scrollpos_zoom, ZOOM_LVL_MAX);
	vp->virtual_width = ScaleByZoom(vp->width, vp->zoom);
	vp->virtual_height = ScaleByZoom(vp->height, vp->zoom);

	/* If zoom_max is ZOOM_LVL_MIN then the setting has not been loaded yet, therefore all levels are allowed. */
	if (_settings_client.gui.zoom_max != ZOOM_LVL_MIN) {
		/* Ensure zoom level is allowed */
		while (vp->zoom < _settings_client.gui.zoom_min) DoZoomInOutWindow(ZOOM_OUT, w);
		while (vp->zoom > _settings_client.gui.zoom_max) DoZoomInOutWindow(ZOOM_IN, w);
	}

	DoZoomInOutWindow(ZOOM_NONE, w); // update button status
	MarkWholeScreenDirty();
}

byte _age_cargo_skip_counter; ///< Skip aging of cargo? Used before savegame version 162.

static const SaveLoadGlobVarList _date_desc[] = {
	SLEG_CONDVAR(_date,                   SLE_FILE_U16 | SLE_VAR_I32,  SL_MIN_VERSION,  SLV_31),
	SLEG_CONDVAR(_date,                   SLE_INT32,                  SLV_31, SL_MAX_VERSION),
	    SLEG_VAR(_date_fract,             SLE_UINT16),
	    SLEG_VAR(_tick_counter,           SLE_UINT16),
	SLE_CONDNULL(2, SL_MIN_VERSION, SLV_157), // _vehicle_id_ctr_day
	SLEG_CONDVAR(_age_cargo_skip_counter, SLE_UINT8,                   SL_MIN_VERSION, SLV_162),
	SLE_CONDNULL(1, SL_MIN_VERSION, SLV_46),
	SLEG_CONDVAR(_cur_tileloop_tile,      SLE_FILE_U16 | SLE_VAR_U32,  SL_MIN_VERSION, SLV_6),
	SLEG_CONDVAR(_cur_tileloop_tile,      SLE_UINT32,                  SLV_6, SL_MAX_VERSION),
	    SLEG_VAR(_disaster_delay,         SLE_UINT16),
	SLE_CONDNULL(2, SL_MIN_VERSION, SLV_120),
	    SLEG_VAR(_random.state[0],        SLE_UINT32),
	    SLEG_VAR(_random.state[1],        SLE_UINT32),
	SLE_CONDNULL(1,  SL_MIN_VERSION,  SLV_10),
	SLE_CONDNULL(4, SLV_10, SLV_120),
	    SLEG_VAR(_cur_company_tick_index, SLE_FILE_U8  | SLE_VAR_U32),
	SLEG_CONDVAR(_next_competitor_start,  SLE_FILE_U16 | SLE_VAR_U32,  SL_MIN_VERSION, SLV_109),
	SLEG_CONDVAR(_next_competitor_start,  SLE_UINT32,                SLV_109, SL_MAX_VERSION),
	    SLEG_VAR(_trees_tick_ctr,         SLE_UINT8),
	SLEG_CONDVAR(_pause_mode,             SLE_UINT8,                   SLV_4, SL_MAX_VERSION),
	SLE_CONDNULL(4, SLV_11, SLV_120),
	    SLEG_END()
};

static const SaveLoadGlobVarList _date_check_desc[] = {
	SLEG_CONDVAR(_load_check_data.current_date,  SLE_FILE_U16 | SLE_VAR_I32,  SL_MIN_VERSION,  SLV_31),
	SLEG_CONDVAR(_load_check_data.current_date,  SLE_INT32,                  SLV_31, SL_MAX_VERSION),
	    SLE_NULL(2),                       // _date_fract
	    SLE_NULL(2),                       // _tick_counter
	SLE_CONDNULL(2, SL_MIN_VERSION, SLV_157),               // _vehicle_id_ctr_day
	SLE_CONDNULL(1, SL_MIN_VERSION, SLV_162),               // _age_cargo_skip_counter
	SLE_CONDNULL(1, SL_MIN_VERSION, SLV_46),
	SLE_CONDNULL(2, SL_MIN_VERSION, SLV_6),                 // _cur_tileloop_tile
	SLE_CONDNULL(4, SLV_6, SL_MAX_VERSION),    // _cur_tileloop_tile
	    SLE_NULL(2),                       // _disaster_delay
	SLE_CONDNULL(2, SL_MIN_VERSION, SLV_120),
	    SLE_NULL(4),                       // _random.state[0]
	    SLE_NULL(4),                       // _random.state[1]
	SLE_CONDNULL(1,  SL_MIN_VERSION,  SLV_10),
	SLE_CONDNULL(4, SLV_10, SLV_120),
	    SLE_NULL(1),                       // _cur_company_tick_index
	SLE_CONDNULL(2, SL_MIN_VERSION, SLV_109),               // _next_competitor_start
	SLE_CONDNULL(4, SLV_109, SL_MAX_VERSION),  // _next_competitor_start
	    SLE_NULL(1),                       // _trees_tick_ctr
	SLE_CONDNULL(1, SLV_4, SL_MAX_VERSION),    // _pause_mode
	SLE_CONDNULL(4, SLV_11, SLV_120),
	    SLEG_END()
};

/* Save load date related variables as well as persistent tick counters
 * XXX: currently some unrelated stuff is just put here */
static void SaveLoad_DATE()
{
	SlGlobList(_date_desc);
}

static void Check_DATE()
{
	SlGlobList(_date_check_desc);
	if (IsSavegameVersionBefore(SLV_31)) {
		_load_check_data.current_date += DAYS_TILL_ORIGINAL_BASE_YEAR;
	}
}


static const SaveLoadGlobVarList _view_desc[] = {
	SLEG_CONDVAR(_saved_scrollpos_x,    SLE_FILE_I16 | SLE_VAR_I32, SL_MIN_VERSION, SLV_6),
	SLEG_CONDVAR(_saved_scrollpos_x,    SLE_INT32,                  SLV_6, SL_MAX_VERSION),
	SLEG_CONDVAR(_saved_scrollpos_y,    SLE_FILE_I16 | SLE_VAR_I32, SL_MIN_VERSION, SLV_6),
	SLEG_CONDVAR(_saved_scrollpos_y,    SLE_INT32,                  SLV_6, SL_MAX_VERSION),
	    SLEG_VAR(_saved_scrollpos_zoom, SLE_UINT8),
	    SLEG_END()
};

static void SaveLoad_VIEW()
{
	SlGlobList(_view_desc);
}

extern const ChunkHandler _misc_chunk_handlers[] = {
	{ 'DATE', SaveLoad_DATE, SaveLoad_DATE, nullptr, Check_DATE, CH_RIFF},
	{ 'VIEW', SaveLoad_VIEW, SaveLoad_VIEW, nullptr, nullptr,       CH_RIFF | CH_LAST},
};
