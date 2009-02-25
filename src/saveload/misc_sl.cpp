/* $Id$ */

/** @file misc_sl.cpp Saving and loading of things that didn't fit anywhere else */

#include "../stdafx.h"
#include "../date_func.h"
#include "../variables.h"
#include "../core/random_func.hpp"
#include "../openttd.h"
#include "../zoom_func.h"
#include "../vehicle_func.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../viewport_func.h"
#include "../gfx_func.h"
#include "../company_base.h"
#include "../town.h"

#include "saveload.h"

extern TileIndex _cur_tileloop_tile;

/* Keep track of current game position */
int _saved_scrollpos_x;
int _saved_scrollpos_y;

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
	vp->zoom = min(_saved_scrollpos_zoom, ZOOM_LVL_MAX);
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
	    SLEG_VAR(_vehicle_id_ctr_day,     SLE_UINT16),
	    SLEG_VAR(_age_cargo_skip_counter, SLE_UINT8),
	SLE_CONDNULL(1, 0, 45),
	SLEG_CONDVAR(_cur_tileloop_tile,      SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLEG_CONDVAR(_cur_tileloop_tile,      SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLEG_VAR(_disaster_delay,         SLE_UINT16),
	    SLEG_VAR(_station_tick_ctr,       SLE_UINT16),
	    SLEG_VAR(_random.state[0],        SLE_UINT32),
	    SLEG_VAR(_random.state[1],        SLE_UINT32),
	SLEG_CONDVAR(_cur_town_ctr,           SLE_FILE_U8  | SLE_VAR_U32,  0, 9),
	SLEG_CONDVAR(_cur_town_ctr,           SLE_UINT32,                 10, SL_MAX_VERSION),
	    SLEG_VAR(_cur_company_tick_index, SLE_FILE_U8  | SLE_VAR_U32),
	SLEG_CONDVAR(_next_competitor_start,  SLE_FILE_U16 | SLE_VAR_U32,  0, 108),
	SLEG_CONDVAR(_next_competitor_start,  SLE_UINT32,                109, SL_MAX_VERSION),
	    SLEG_VAR(_trees_tick_ctr,         SLE_UINT8),
	SLEG_CONDVAR(_pause_game,             SLE_UINT8,                   4, SL_MAX_VERSION),
	SLEG_CONDVAR(_cur_town_iter,          SLE_UINT32,                 11, SL_MAX_VERSION),
	    SLEG_END()
};

/* Save load date related variables as well as persistent tick counters
 * XXX: currently some unrelated stuff is just put here */
static void SaveLoad_DATE()
{
	SlGlobList(_date_desc);
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
	{ 'DATE', SaveLoad_DATE, SaveLoad_DATE, CH_RIFF},
	{ 'VIEW', SaveLoad_VIEW, SaveLoad_VIEW, CH_RIFF | CH_LAST},
};
