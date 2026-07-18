/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file misc_sl.cpp Saving and loading of things that didn't fit anywhere else. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/misc_sl_compat.h"

#include "../timer/timer_game_calendar.h"
#include "../timer/timer_game_economy.h"
#include "../zoom_func.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../viewport_func.h"
#include "../gfx_func.h"
#include "../core/random_func.hpp"
#include "../fios.h"
#include "../timer/timer.h"
#include "../timer/timer_game_tick.h"

#include "../safeguards.h"

extern TileIndex _cur_tileloop_tile;
extern uint16_t _disaster_delay;
extern uint8_t _trees_tick_ctr;

/** @{
 * Keep track of current game position. */
int _saved_scrollpos_x;
int _saved_scrollpos_y;
ZoomLevel _saved_scrollpos_zoom;
/** @} */

void SaveViewportBeforeSaveGame()
{
	/* Don't use GetMainWindow() in case the window does not exist. */
	const Window *w = FindWindowById(WindowClass::MainWindow, 0);
	if (w == nullptr || w->viewport == nullptr) {
		/* Ensure saved position is clearly invalid. */
		_saved_scrollpos_x = INT_MAX;
		_saved_scrollpos_y = INT_MAX;
		_saved_scrollpos_zoom = ZoomLevel::End;
	} else {
		_saved_scrollpos_x = w->viewport->scrollpos_x;
		_saved_scrollpos_y = w->viewport->scrollpos_y;
		_saved_scrollpos_zoom = w->viewport->zoom;
	}
}

void ResetViewportAfterLoadGame()
{
	if (_saved_scrollpos_x == INT_MAX && _saved_scrollpos_y == INT_MAX) return;

	Window *w = GetMainWindow();

	w->viewport->scrollpos_x = _saved_scrollpos_x;
	w->viewport->scrollpos_y = _saved_scrollpos_y;
	w->viewport->dest_scrollpos_x = _saved_scrollpos_x;
	w->viewport->dest_scrollpos_y = _saved_scrollpos_y;

	Viewport &vp = *w->viewport;
	vp.zoom = std::min(_saved_scrollpos_zoom, ZoomLevel::Max);
	vp.virtual_width = ScaleByZoom(vp.width, vp.zoom);
	vp.virtual_height = ScaleByZoom(vp.height, vp.zoom);

	/* If zoom_max is ZoomLevel::Min then the setting has not been loaded yet, therefore all levels are allowed. */
	if (_settings_client.gui.zoom_max != ZoomLevel::Min) {
		/* Ensure zoom level is allowed */
		while (vp.zoom < _settings_client.gui.zoom_min) DoZoomInOutWindow(ZOOM_OUT, w);
		while (vp.zoom > _settings_client.gui.zoom_max) DoZoomInOutWindow(ZOOM_IN, w);
	}

	DoZoomInOutWindow(ZOOM_NONE, w); // update button status
	MarkWholeScreenDirty();
}

uint8_t _age_cargo_skip_counter; ///< Skip aging of cargo? Used before savegame version 162.
extern TimeoutTimer<TimerGameTick> _new_competitor_timeout;

static const SaveLoad _date_desc[] = {
	SLEG_CONDVAR("date", TimerGameCalendar::date, VarFileType::U16 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::BigDates),
	SLEG_CONDVAR("date", TimerGameCalendar::date, VarTypes::I32, SaveLoadVersion::BigDates, SaveLoadVersion::MaxVersion),
	    SLEG_VAR("date_fract",             TimerGameCalendar::date_fract,             VarTypes::U16),
	SLEG_CONDVAR("tick_counter", TimerGameTick::counter, VarFileType::U16 | VarMemType::U64, SaveLoadVersion::MinVersion, SaveLoadVersion::U64TickCounter),
	SLEG_CONDVAR("tick_counter", TimerGameTick::counter, VarTypes::U64, SaveLoadVersion::U64TickCounter, SaveLoadVersion::MaxVersion),
	SLEG_CONDVAR("economy_date", TimerGameEconomy::date, VarTypes::I32, SaveLoadVersion::EconomyDate, SaveLoadVersion::MaxVersion),
	SLEG_CONDVAR("economy_date_fract", TimerGameEconomy::date_fract, VarTypes::U16, SaveLoadVersion::EconomyDate, SaveLoadVersion::MaxVersion),
	SLEG_CONDVAR("days_since_last_month", TimerGameEconomy::days_since_last_month, VarTypes::U32, SaveLoadVersion::IndustryAcceptedHistory, SaveLoadVersion::MaxVersion),
	SLEG_CONDVAR("calendar_sub_date_fract", TimerGameCalendar::sub_date_fract, VarTypes::U16, SaveLoadVersion::CalendarSubDateFract, SaveLoadVersion::MaxVersion),
	SLEG_CONDVAR("age_cargo_skip_counter", _age_cargo_skip_counter, VarTypes::U8, SaveLoadVersion::MinVersion, SaveLoadVersion::NewGRFCustomCargoAging),
	SLEG_CONDVAR("cur_tileloop_tile", _cur_tileloop_tile, VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
	SLEG_CONDVAR("cur_tileloop_tile", _cur_tileloop_tile, VarTypes::U32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),
	    SLEG_VAR("next_disaster_start",         _disaster_delay,         VarTypes::U16),
	    SLEG_VAR("random_state[0]",        _random.state[0],        VarTypes::U32),
	    SLEG_VAR("random_state[1]",        _random.state[1],        VarTypes::U32),
	    SLEG_VAR("company_tick_counter", _cur_company_tick_index, VarFileType::U8  | VarMemType::U32),
	    SLEG_VAR("trees_tick_counter",     _trees_tick_ctr,         VarTypes::U8),
	SLEG_CONDVAR("pause_mode", _pause_mode, VarTypes::U8, SaveLoadVersion::TownTolerancePauseMode, SaveLoadVersion::MaxVersion),
	SLEG_CONDSSTR("id", _game_session_stats.savegame_id, VarTypes::STR, SaveLoadVersion::SavegameId, SaveLoadVersion::MaxVersion),
	/* For older savegames, we load the current value as the "period"; afterload will set the "fired" and "elapsed". */
	SLEG_CONDVAR("next_competitor_start", _new_competitor_timeout.period.value, VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::NextCompetitorStartOverflow),
	SLEG_CONDVAR("next_competitor_start", _new_competitor_timeout.period.value, VarTypes::U32, SaveLoadVersion::NextCompetitorStartOverflow, SaveLoadVersion::AIStartDate),
	SLEG_CONDVAR("competitors_interval", _new_competitor_timeout.period.value, VarTypes::U32, SaveLoadVersion::AIStartDate, SaveLoadVersion::MaxVersion),
	SLEG_CONDVAR("competitors_interval_elapsed", _new_competitor_timeout.storage.elapsed, VarTypes::U32, SaveLoadVersion::AIStartDate, SaveLoadVersion::MaxVersion),
	SLEG_CONDVAR("competitors_interval_fired", _new_competitor_timeout.fired, VarTypes::BOOL, SaveLoadVersion::AIStartDate, SaveLoadVersion::MaxVersion),
};

static const SaveLoad _date_check_desc[] = {
	SLEG_CONDVAR("date", _load_check_data.current_date, VarFileType::U16 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::BigDates),
	SLEG_CONDVAR("date", _load_check_data.current_date, VarTypes::I32, SaveLoadVersion::BigDates, SaveLoadVersion::MaxVersion),
};

/**
 * Save load date related variables as well as persistent tick counters.
 * @note currently some unrelated stuff is just put here.
 */
struct DATEChunkHandler : ChunkHandler {
	DATEChunkHandler() : ChunkHandler("DATE", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_date_desc);

		SlSetArrayIndex(0);
		SlGlobList(_date_desc);
	}

	void LoadCommon(const SaveLoadTable &slt, const SaveLoadCompatTable &slct) const
	{
		const std::vector<SaveLoad> oslt = SlCompatTableHeader(slt, slct);

		if (!IsSavegameVersionBefore(SaveLoadVersion::RiffToArray) && SlIterateArray() == -1) return;
		SlGlobList(oslt);
		if (!IsSavegameVersionBefore(SaveLoadVersion::RiffToArray) && SlIterateArray() != -1) SlErrorCorrupt("Too many DATE entries");
	}

	void Load() const override
	{
		this->LoadCommon(_date_desc, _date_sl_compat);
	}


	void LoadCheck(size_t) const override
	{
		this->LoadCommon(_date_check_desc, _date_check_sl_compat);

		if (IsSavegameVersionBefore(SaveLoadVersion::BigDates)) {
			_load_check_data.current_date += CalendarTime::DAYS_TILL_ORIGINAL_BASE_YEAR;
		}
	}
};

static const SaveLoad _view_desc[] = {
	SLEG_CONDVAR("x", _saved_scrollpos_x, VarFileType::I16 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
	SLEG_CONDVAR("x", _saved_scrollpos_x, VarTypes::I32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),
	SLEG_CONDVAR("y", _saved_scrollpos_y, VarFileType::I16 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
	SLEG_CONDVAR("y", _saved_scrollpos_y, VarTypes::I32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),
	    SLEG_VAR("zoom", _saved_scrollpos_zoom, VarFileType::U8 | VarMemType::I8),
};

struct VIEWChunkHandler : ChunkHandler {
	VIEWChunkHandler() : ChunkHandler("VIEW", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_view_desc);

		SlSetArrayIndex(0);
		SlGlobList(_view_desc);
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_view_desc, _view_sl_compat);

		if (!IsSavegameVersionBefore(SaveLoadVersion::RiffToArray) && SlIterateArray() == -1) return;
		SlGlobList(slt);
		if (!IsSavegameVersionBefore(SaveLoadVersion::RiffToArray) && SlIterateArray() != -1) SlErrorCorrupt("Too many DATE entries");
	}
};

static const DATEChunkHandler DATE;
static const VIEWChunkHandler VIEW;
static const ChunkHandlerRef misc_chunk_handlers[] = {
	DATE,
	VIEW,
};

extern const ChunkHandlerTable _misc_chunk_handlers(misc_chunk_handlers);
