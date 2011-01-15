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
#include "../openttd.h"
#include "../zoom_func.h"
#include "../vehicle_func.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../viewport_func.h"
#include "../gfx_func.h"
#include "../core/random_func.hpp"
#include "../fios.h"

#include "saveload.h"

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

	if (w != NULL) {
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

	DoZoomInOutWindow(ZOOM_NONE, w); // update button status
	MarkWholeScreenDirty();
}


static const SaveLoadGlobVarList _date_desc[] = {
	SLEG_CONDVAR(_date,                   SLE_FILE_U16 | SLE_VAR_I32,  0,  30),
	SLEG_CONDVAR(_date,                   SLE_INT32,                  31, SL_MAX_VERSION),
	    SLEG_VAR(_date_fract,             SLE_UINT16),
	    SLEG_VAR(_tick_counter,           SLE_UINT16),
	SLE_CONDNULL(2, 0, 156), // _vehicle_id_ctr_day
	    SLEG_VAR(_age_cargo_skip_counter, SLE_UINT8),
	SLE_CONDNULL(1, 0, 45),
	SLEG_CONDVAR(_cur_tileloop_tile,      SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLEG_CONDVAR(_cur_tileloop_tile,      SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLEG_VAR(_disaster_delay,         SLE_UINT16),
	SLE_CONDNULL(2, 0, 119),
	    SLEG_VAR(_random.state[0],        SLE_UINT32),
	    SLEG_VAR(_random.state[1],        SLE_UINT32),
	SLE_CONDNULL(1,  0,   9),
	SLE_CONDNULL(4, 10, 119),
	    SLEG_VAR(_cur_company_tick_index, SLE_FILE_U8  | SLE_VAR_U32),
	SLEG_CONDVAR(_next_competitor_start,  SLE_FILE_U16 | SLE_VAR_U32,  0, 108),
	SLEG_CONDVAR(_next_competitor_start,  SLE_UINT32,                109, SL_MAX_VERSION),
	    SLEG_VAR(_trees_tick_ctr,         SLE_UINT8),
	SLEG_CONDVAR(_pause_mode,             SLE_UINT8,                   4, SL_MAX_VERSION),
	SLE_CONDNULL(4, 11, 119),
	    SLEG_END()
};

static const SaveLoadGlobVarList _date_check_desc[] = {
	SLEG_CONDVAR(_load_check_data.current_date,  SLE_FILE_U16 | SLE_VAR_I32,  0,  30),
	SLEG_CONDVAR(_load_check_data.current_date,  SLE_INT32,                  31, SL_MAX_VERSION),
	    SLE_NULL(2),                       // _date_fract
	    SLE_NULL(2),                       // _tick_counter
	SLE_CONDNULL(2, 0, 156),               // _vehicle_id_ctr_day
	    SLE_NULL(1),                       // _age_cargo_skip_counter
	SLE_CONDNULL(1, 0, 45),
	SLE_CONDNULL(2, 0, 5),                 // _cur_tileloop_tile
	SLE_CONDNULL(4, 6, SL_MAX_VERSION),    // _cur_tileloop_tile
	    SLE_NULL(2),                       // _disaster_delay
	SLE_CONDNULL(2, 0, 119),
	    SLE_NULL(4),                       // _random.state[0]
	    SLE_NULL(4),                       // _random.state[1]
	SLE_CONDNULL(1,  0,   9),
	SLE_CONDNULL(4, 10, 119),
	    SLE_NULL(1),                       // _cur_company_tick_index
	SLE_CONDNULL(2, 0, 108),               // _next_competitor_start
	SLE_CONDNULL(4, 109, SL_MAX_VERSION),  // _next_competitor_start
	    SLE_NULL(1),                       // _trees_tick_ctr
	SLE_CONDNULL(1, 4, SL_MAX_VERSION),    // _pause_mode
	SLE_CONDNULL(4, 11, 119),
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
	if (IsSavegameVersionBefore(31)) {
		_load_check_data.current_date += DAYS_TILL_ORIGINAL_BASE_YEAR;
	}
}


static const SaveLoadGlobVarList _view_desc[] = {
	SLEG_CONDVAR(_saved_scrollpos_x,    SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLEG_CONDVAR(_saved_scrollpos_x,    SLE_INT32,                  6, SL_MAX_VERSION),
	SLEG_CONDVAR(_saved_scrollpos_y,    SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLEG_CONDVAR(_saved_scrollpos_y,    SLE_INT32,                  6, SL_MAX_VERSION),
	    SLEG_VAR(_saved_scrollpos_zoom, SLE_UINT8),
	    SLEG_END()
};

static void SaveLoad_VIEW()
{
	SlGlobList(_view_desc);
}

extern const ChunkHandler _misc_chunk_handlers[] = {
	{ 'DATE', SaveLoad_DATE, SaveLoad_DATE, NULL, Check_DATE, CH_RIFF},
	{ 'VIEW', SaveLoad_VIEW, SaveLoad_VIEW, NULL, NULL,       CH_RIFF | CH_LAST},
};
