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
	SaveLoad::Variable<VarFileType::U16>("date", SLE_GLOBAL_ADDRESS(TimerGameCalendar::date), SaveLoadVersion::MinVersion, SaveLoadVersion::BigDates),
	SaveLoad::Variable<VarFileType::I32>("date", SLE_GLOBAL_ADDRESS(TimerGameCalendar::date), SaveLoadVersion::BigDates),
	SaveLoad::Variable<VarFileType::U16>("date_fract", SLE_GLOBAL_ADDRESS(TimerGameCalendar::date_fract)),
	SaveLoad::Variable<VarFileType::U16>("tick_counter", SLE_GLOBAL_ADDRESS(TimerGameTick::counter), SaveLoadVersion::MinVersion, SaveLoadVersion::U64TickCounter),
	SaveLoad::Variable<VarFileType::U64>("tick_counter", SLE_GLOBAL_ADDRESS(TimerGameTick::counter), SaveLoadVersion::U64TickCounter),
	SaveLoad::Variable<VarFileType::I32>("economy_date", SLE_GLOBAL_ADDRESS(TimerGameEconomy::date), SaveLoadVersion::EconomyDate),
	SaveLoad::Variable<VarFileType::U16>("economy_date_fract", SLE_GLOBAL_ADDRESS(TimerGameEconomy::date_fract), SaveLoadVersion::EconomyDate),
	SaveLoad::Variable<VarFileType::U32>("days_since_last_month", SLE_GLOBAL_ADDRESS(TimerGameEconomy::days_since_last_month), SaveLoadVersion::IndustryAcceptedHistory),
	SaveLoad::Variable<VarFileType::U16>("calendar_sub_date_fract", SLE_GLOBAL_ADDRESS(TimerGameCalendar::sub_date_fract), SaveLoadVersion::CalendarSubDateFract),
	SaveLoad::Variable<VarFileType::U8>("age_cargo_skip_counter", SLE_GLOBAL_ADDRESS(_age_cargo_skip_counter), SaveLoadVersion::MinVersion, SaveLoadVersion::NewGRFCustomCargoAging),
	SaveLoad::Variable<VarFileType::U16>("cur_tileloop_tile", SLE_GLOBAL_ADDRESS(_cur_tileloop_tile), SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
	SaveLoad::Variable<VarFileType::U32>("cur_tileloop_tile", SLE_GLOBAL_ADDRESS(_cur_tileloop_tile), SaveLoadVersion::MultipleRoadStops),
	SaveLoad::Variable<VarFileType::U16>("next_disaster_start", SLE_GLOBAL_ADDRESS(_disaster_delay)),
	SaveLoad::Variable<VarFileType::U32>("random_state[0]", SLE_GLOBAL_ADDRESS(_random.state[0])),
	SaveLoad::Variable<VarFileType::U32>("random_state[1]", SLE_GLOBAL_ADDRESS(_random.state[1])),
	SaveLoad::Variable<VarFileType::U8>("company_tick_counter", SLE_GLOBAL_ADDRESS(_cur_company_tick_index)),
	SaveLoad::Variable<VarFileType::U8>("trees_tick_counter", SLE_GLOBAL_ADDRESS(_trees_tick_ctr)),
	SaveLoad::Variable<VarFileType::U8>("pause_mode", SLE_GLOBAL_ADDRESS(_pause_mode), SaveLoadVersion::TownTolerancePauseMode),
	SaveLoad::String("id", SLE_GLOBAL_ADDRESS(_game_session_stats.savegame_id), {}, SaveLoadVersion::SavegameId),
	/* For older savegames, we load the current value as the "period"; afterload will set the "fired" and "elapsed". */
	SaveLoad::Variable<VarFileType::U16>("next_competitor_start", SLE_GLOBAL_ADDRESS(_new_competitor_timeout.period.value), SaveLoadVersion::MinVersion, SaveLoadVersion::NextCompetitorStartOverflow),
	SaveLoad::Variable<VarFileType::U32>("next_competitor_start", SLE_GLOBAL_ADDRESS(_new_competitor_timeout.period.value), SaveLoadVersion::NextCompetitorStartOverflow, SaveLoadVersion::AIStartDate),
	SaveLoad::Variable<VarFileType::U32>("competitors_interval", SLE_GLOBAL_ADDRESS(_new_competitor_timeout.period.value), SaveLoadVersion::AIStartDate),
	SaveLoad::Variable<VarFileType::U32>("competitors_interval_elapsed", SLE_GLOBAL_ADDRESS(_new_competitor_timeout.storage.elapsed), SaveLoadVersion::AIStartDate),
	SaveLoad::Variable<VarFileType::Bool>("competitors_interval_fired", SLE_GLOBAL_ADDRESS(_new_competitor_timeout.fired), SaveLoadVersion::AIStartDate),
};

static const SaveLoad _date_check_desc[] = {
	SaveLoad::Variable<VarFileType::U16>("date", SLE_GLOBAL_ADDRESS(_load_check_data.current_date), SaveLoadVersion::MinVersion, SaveLoadVersion::BigDates),
	SaveLoad::Variable<VarFileType::I32>("date", SLE_GLOBAL_ADDRESS(_load_check_data.current_date), SaveLoadVersion::BigDates),
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
	SaveLoad::Variable<VarFileType::I16>("x", SLE_GLOBAL_ADDRESS(_saved_scrollpos_x), SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
	SaveLoad::Variable<VarFileType::I32>("x", SLE_GLOBAL_ADDRESS(_saved_scrollpos_x), SaveLoadVersion::MultipleRoadStops),
	SaveLoad::Variable<VarFileType::I16>("y", SLE_GLOBAL_ADDRESS(_saved_scrollpos_y), SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
	SaveLoad::Variable<VarFileType::I32>("y", SLE_GLOBAL_ADDRESS(_saved_scrollpos_y), SaveLoadVersion::MultipleRoadStops),
	SaveLoad::Variable<VarFileType::U8>("zoom", SLE_GLOBAL_ADDRESS(_saved_scrollpos_zoom)),
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
