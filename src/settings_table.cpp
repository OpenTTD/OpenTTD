/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file settings_table.cpp The tables of all the settings as well as the implementation of most of their callbacks.
 */

#include "stdafx.h"
#include "settings_table.h"
#include "currency.h"
#include "screenshot.h"
#include "network/network.h"
#include "network/network_func.h"
#include "network/core/config.h"
#include "pathfinder/pathfinder_type.h"
#include "linkgraph/linkgraphschedule.h"
#include "genworld.h"
#include "train.h"
#include "news_func.h"
#include "window_func.h"
#include "company_func.h"
#include "timer/timer_game_calendar.h"
#if defined(WITH_FREETYPE) || defined(_WIN32) || defined(WITH_COCOA)
#define HAS_TRUETYPE_FONT
#include "fontcache.h"
#endif
#include "textbuf_gui.h"
#include "rail_gui.h"
#include "elrail_func.h"
#include "error.h"
#include "town.h"
#include "video/video_driver.hpp"
#include "sound/sound_driver.hpp"
#include "music/music_driver.hpp"
#include "blitter/factory.hpp"
#include "base_media_base.h"
#include "ai/ai_config.hpp"
#include "ai/ai.hpp"
#include "game/game_config.hpp"
#include "ship.h"
#include "smallmap_gui.h"
#include "roadveh.h"
#include "roadveh_cmd.h"
#include "vehicle_func.h"
#include "viewport_func.h"
#include "void_map.h"
#include "station_func.h"
#include "station_base.h"

#include "table/strings.h"
#include "table/settings.h"

#include "safeguards.h"

SettingTable _company_settings{ _company_settings_table };
SettingTable _currency_settings{ _currency_settings_table };
SettingTable _difficulty_settings{ _difficulty_settings_table };
SettingTable _multimedia_settings{ _multimedia_settings_table };
SettingTable _economy_settings{ _economy_settings_table };
SettingTable _game_settings{ _game_settings_table };
SettingTable _gui_settings{ _gui_settings_table };
SettingTable _linkgraph_settings{ _linkgraph_settings_table };
SettingTable _locale_settings{ _locale_settings_table };
SettingTable _misc_settings{ _misc_settings_table };
SettingTable _network_private_settings{ _network_private_settings_table };
SettingTable _network_secrets_settings{ _network_secrets_settings_table };
SettingTable _network_settings{ _network_settings_table };
SettingTable _news_display_settings{ _news_display_settings_table };
SettingTable _old_gameopt_settings{ _old_gameopt_settings_table };
SettingTable _pathfinding_settings{ _pathfinding_settings_table };
SettingTable _script_settings{ _script_settings_table };
SettingTable _window_settings{ _window_settings_table };
SettingTable _world_settings{ _world_settings_table };
#if defined(_WIN32) && !defined(DEDICATED)
SettingTable _win32_settings{ _win32_settings_table };
#endif /* _WIN32 */


/* Begin - Callback Functions for the various settings. */

/** Switch setting title depending on wallclock setting */
static StringID SettingTitleWallclock(const IntSettingDesc &sd)
{
	return TimerGameEconomy::UsingWallclockUnits(_game_mode == GM_MENU) ? sd.str + 1 : sd.str;
}

/** Switch setting help depending on wallclock setting */
static StringID SettingHelpWallclock(const IntSettingDesc &sd)
{
	return TimerGameEconomy::UsingWallclockUnits(_game_mode == GM_MENU) ? sd.str_help + 1 : sd.str_help;
}

/** Setting values for velocity unit localisation */
static void SettingsValueVelocityUnit(const IntSettingDesc &, uint first_param, int32_t value)
{
	StringID val;
	switch (value) {
		case 0: val = STR_CONFIG_SETTING_LOCALISATION_UNITS_VELOCITY_IMPERIAL; break;
		case 1: val = STR_CONFIG_SETTING_LOCALISATION_UNITS_VELOCITY_METRIC; break;
		case 2: val = STR_CONFIG_SETTING_LOCALISATION_UNITS_VELOCITY_SI; break;
		case 3: val = TimerGameEconomy::UsingWallclockUnits(_game_mode == GM_MENU) ? STR_CONFIG_SETTING_LOCALISATION_UNITS_VELOCITY_GAMEUNITS_SECS : STR_CONFIG_SETTING_LOCALISATION_UNITS_VELOCITY_GAMEUNITS_DAYS; break;
		case 4: val = STR_CONFIG_SETTING_LOCALISATION_UNITS_VELOCITY_KNOTS; break;
		default: NOT_REACHED();
	}
	SetDParam(first_param, val);
}

/** A negative value has another string (the one after "strval"). */
static void SettingsValueAbsolute(const IntSettingDesc &sd, uint first_param, int32_t value)
{
	SetDParam(first_param, sd.str_val + ((value >= 0) ? 1 : 0));
	SetDParam(first_param + 1, abs(value));
}

/** Service Interval Settings Default Value displays the correct units or as a percentage */
static void ServiceIntervalSettingsValueText(const IntSettingDesc &sd, uint first_param, int32_t value)
{
	VehicleDefaultSettings *vds;
	if (_game_mode == GM_MENU || !Company::IsValidID(_current_company)) {
		vds = &_settings_client.company.vehicle;
	} else {
		vds = &Company::Get(_current_company)->settings.vehicle;
	}

	if (value == 0) {
		SetDParam(first_param, sd.str_val + 3);
	} else if (vds->servint_ispercent) {
		SetDParam(first_param, sd.str_val + 2);
	} else if (TimerGameEconomy::UsingWallclockUnits(_game_mode == GM_MENU)) {
		SetDParam(first_param, sd.str_val + 1);
	} else {
		SetDParam(first_param, sd.str_val);
	}
	SetDParam(first_param + 1, value);
}

/** Reposition the main toolbar as the setting changed. */
static void v_PositionMainToolbar(int32_t)
{
	if (_game_mode != GM_MENU) PositionMainToolbar(nullptr);
}

/** Reposition the statusbar as the setting changed. */
static void v_PositionStatusbar(int32_t)
{
	if (_game_mode != GM_MENU) {
		PositionStatusbar(nullptr);
		PositionNewsMessage(nullptr);
		PositionNetworkChatWindow(nullptr);
	}
}

/**
 * Redraw the smallmap after a colour scheme change.
 */
static void RedrawSmallmap(int32_t)
{
	BuildLandLegend();
	BuildOwnerLegend();
	SetWindowClassesDirty(WC_SMALLMAP);
}

/** Redraw linkgraph links after a colour scheme change. */
static void UpdateLinkgraphColours(int32_t)
{
	BuildLinkStatsLegend();
	MarkWholeScreenDirty();
}

static void StationSpreadChanged(int32_t)
{
	InvalidateWindowData(WC_SELECT_STATION, 0);
	InvalidateWindowData(WC_BUILD_STATION, 0);
}

static void UpdateConsists(int32_t)
{
	for (Train *t : Train::Iterate()) {
		/* Update the consist of all trains so the maximum speed is set correctly. */
		if (t->IsFrontEngine() || t->IsFreeWagon()) t->ConsistChanged(CCF_TRACK);
	}
	InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
}

/**
 * Check and update if needed all vehicle service intervals.
 * @param new_value Contains 0 if service intervals are in time (days or real-world minutes), otherwise intervals use percents.
 */
static void UpdateAllServiceInterval(int32_t new_value)
{
	bool update_vehicles;
	VehicleDefaultSettings *vds;
	if (_game_mode == GM_MENU || !Company::IsValidID(_current_company)) {
		vds = &_settings_client.company.vehicle;
		update_vehicles = false;
	} else {
		vds = &Company::Get(_current_company)->settings.vehicle;
		update_vehicles = true;
	}

	if (new_value != 0) {
		/* Service intervals are in percents. */
		vds->servint_trains   = DEF_SERVINT_PERCENT;
		vds->servint_roadveh  = DEF_SERVINT_PERCENT;
		vds->servint_aircraft = DEF_SERVINT_PERCENT;
		vds->servint_ships    = DEF_SERVINT_PERCENT;
	} else if (TimerGameEconomy::UsingWallclockUnits(_game_mode == GM_MENU)) {
		/* Service intervals are in minutes. */
		vds->servint_trains   = DEF_SERVINT_MINUTES_TRAINS;
		vds->servint_roadveh  = DEF_SERVINT_MINUTES_ROADVEH;
		vds->servint_aircraft = DEF_SERVINT_MINUTES_AIRCRAFT;
		vds->servint_ships    = DEF_SERVINT_MINUTES_SHIPS;
	} else {
		/* Service intervals are in days. */
		vds->servint_trains   = DEF_SERVINT_DAYS_TRAINS;
		vds->servint_roadveh  = DEF_SERVINT_DAYS_ROADVEH;
		vds->servint_aircraft = DEF_SERVINT_DAYS_AIRCRAFT;
		vds->servint_ships    = DEF_SERVINT_DAYS_SHIPS;
	}

	if (update_vehicles) {
		const Company *c = Company::Get(_current_company);
		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->owner == _current_company && v->IsPrimaryVehicle() && !v->ServiceIntervalIsCustom()) {
				v->SetServiceInterval(CompanyServiceInterval(c, v->type));
				v->SetServiceIntervalIsPercent(new_value != 0);
			}
		}
	}

	SetWindowClassesDirty(WC_VEHICLE_DETAILS);
}

static bool CanUpdateServiceInterval(VehicleType, int32_t &new_value)
{
	VehicleDefaultSettings *vds;
	if (_game_mode == GM_MENU || !Company::IsValidID(_current_company)) {
		vds = &_settings_client.company.vehicle;
	} else {
		vds = &Company::Get(_current_company)->settings.vehicle;
	}

	/* Test if the interval is valid */
	int32_t interval = GetServiceIntervalClamped(new_value, vds->servint_ispercent);
	return interval == new_value;
}

static void UpdateServiceInterval(VehicleType type, int32_t new_value)
{
	if (_game_mode != GM_MENU && Company::IsValidID(_current_company)) {
		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->owner == _current_company && v->type == type && v->IsPrimaryVehicle() && !v->ServiceIntervalIsCustom()) {
				v->SetServiceInterval(new_value);
			}
		}
	}

	SetWindowClassesDirty(WC_VEHICLE_DETAILS);
}

/**
 * Checks if the service intervals in the settings are specified as percentages and corrects the default value accordingly.
 * @param new_value Contains the service interval's default value in days, or 50 (default in percentage).
 */
static int32_t GetDefaultServiceInterval(VehicleType type)
{
	VehicleDefaultSettings *vds;
	if (_game_mode == GM_MENU || !Company::IsValidID(_current_company)) {
		vds = &_settings_client.company.vehicle;
	} else {
		vds = &Company::Get(_current_company)->settings.vehicle;
	}

	int32_t new_value;
	if (vds->servint_ispercent) {
		new_value = DEF_SERVINT_PERCENT;
	} else if (TimerGameEconomy::UsingWallclockUnits(_game_mode == GM_MENU)) {
		switch (type) {
			case VEH_TRAIN:    new_value = DEF_SERVINT_MINUTES_TRAINS; break;
			case VEH_ROAD:     new_value = DEF_SERVINT_MINUTES_ROADVEH; break;
			case VEH_AIRCRAFT: new_value = DEF_SERVINT_MINUTES_AIRCRAFT; break;
			case VEH_SHIP:     new_value = DEF_SERVINT_MINUTES_SHIPS; break;
			default: NOT_REACHED();
		}
	} else {
		switch (type) {
			case VEH_TRAIN:    new_value = DEF_SERVINT_DAYS_TRAINS; break;
			case VEH_ROAD:     new_value = DEF_SERVINT_DAYS_ROADVEH; break;
			case VEH_AIRCRAFT: new_value = DEF_SERVINT_DAYS_AIRCRAFT; break;
			case VEH_SHIP:     new_value = DEF_SERVINT_DAYS_SHIPS; break;
			default: NOT_REACHED();
		}
	}

	return new_value;
}

static void TrainAccelerationModelChanged(int32_t)
{
	for (Train *t : Train::Iterate()) {
		if (t->IsFrontEngine()) {
			t->tcache.cached_max_curve_speed = t->GetCurveSpeedLimit();
			t->UpdateAcceleration();
		}
	}

	/* These windows show acceleration values only when realistic acceleration is on. They must be redrawn after a setting change. */
	SetWindowClassesDirty(WC_ENGINE_PREVIEW);
	InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
	SetWindowClassesDirty(WC_VEHICLE_DETAILS);
}

/**
 * This function updates the train acceleration cache after a steepness change.
 */
static void TrainSlopeSteepnessChanged(int32_t)
{
	for (Train *t : Train::Iterate()) {
		if (t->IsFrontEngine()) t->CargoChanged();
	}
}

/**
 * This function updates realistic acceleration caches when the setting "Road vehicle acceleration model" is set.
 */
static void RoadVehAccelerationModelChanged(int32_t)
{
	if (_settings_game.vehicle.roadveh_acceleration_model != AM_ORIGINAL) {
		for (RoadVehicle *rv : RoadVehicle::Iterate()) {
			if (rv->IsFrontEngine()) {
				rv->CargoChanged();
			}
		}
	}

	/* These windows show acceleration values only when realistic acceleration is on. They must be redrawn after a setting change. */
	SetWindowClassesDirty(WC_ENGINE_PREVIEW);
	InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
	SetWindowClassesDirty(WC_VEHICLE_DETAILS);
}

/**
 * This function updates the road vehicle acceleration cache after a steepness change.
 */
static void RoadVehSlopeSteepnessChanged(int32_t)
{
	for (RoadVehicle *rv : RoadVehicle::Iterate()) {
		if (rv->IsFrontEngine()) rv->CargoChanged();
	}
}

static void TownFoundingChanged(int32_t)
{
	if (_game_mode != GM_EDITOR && _settings_game.economy.found_town == TF_FORBIDDEN) {
		CloseWindowById(WC_FOUND_TOWN, 0);
	} else {
		InvalidateWindowData(WC_FOUND_TOWN, 0);
	}
}

static void ZoomMinMaxChanged(int32_t)
{
	ConstrainAllViewportsZoom();
	GfxClearSpriteCache();
	InvalidateWindowClassesData(WC_SPRITE_ALIGNER);
	if (AdjustGUIZoom(false)) {
		ReInitAllWindows(true);
	}
}

static void SpriteZoomMinChanged(int32_t)
{
	GfxClearSpriteCache();
	/* Force all sprites to redraw at the new chosen zoom level */
	MarkWholeScreenDirty();
}

/**
 * Update any possible saveload window and delete any newgrf dialogue as
 * its widget parts might change. Reinit all windows as it allows access to the
 * newgrf debug button.
 */
static void InvalidateNewGRFChangeWindows(int32_t)
{
	InvalidateWindowClassesData(WC_SAVELOAD);
	CloseWindowByClass(WC_GAME_OPTIONS);
	ReInitAllWindows(false);
}

static void InvalidateCompanyLiveryWindow(int32_t)
{
	InvalidateWindowClassesData(WC_COMPANY_COLOUR, -1);
	ResetVehicleColourMap();
}

static void DifficultyNoiseChange(int32_t)
{
	if (_game_mode == GM_NORMAL) {
		UpdateAirportsNoise();
		if (_settings_game.economy.station_noise_level) {
			InvalidateWindowClassesData(WC_TOWN_VIEW, 0);
		}
	}
}

static void MaxNoAIsChange(int32_t)
{
	if (GetGameSettings().difficulty.max_no_competitors != 0 &&
			AI::GetInfoList()->empty() &&
			(!_networking || _network_server)) {
		ShowErrorMessage(STR_WARNING_NO_SUITABLE_AI, INVALID_STRING_ID, WL_CRITICAL);
	}

	InvalidateWindowClassesData(WC_GAME_OPTIONS, 0);
}

/**
 * Check whether the road side may be changed.
 * @return true if the road side may be changed.
 */
static bool CheckRoadSide(int32_t &)
{
	return _game_mode == GM_MENU || !RoadVehiclesAreBuilt();
}

/**
 * Conversion callback for _gameopt_settings_game.landscape
 * It converts (or try) between old values and the new ones,
 * without losing initial setting of the user
 * @param value that was read from config file
 * @return the "hopefully" converted value
 */
static size_t ConvertLandscape(const char *value)
{
	/* try with the old values */
	static std::vector<std::string> _old_landscape_values{"normal", "hilly", "desert", "candy"};
	return OneOfManySettingDesc::ParseSingleValue(value, strlen(value), _old_landscape_values);
}

static bool CheckFreeformEdges(int32_t &new_value)
{
	if (_game_mode == GM_MENU) return true;
	if (new_value != 0) {
		for (Ship *s : Ship::Iterate()) {
			/* Check if there is a ship on the northern border. */
			if (TileX(s->tile) == 0 || TileY(s->tile) == 0) {
				ShowErrorMessage(STR_CONFIG_SETTING_EDGES_NOT_EMPTY, INVALID_STRING_ID, WL_ERROR);
				return false;
			}
		}
		for (const BaseStation *st : BaseStation::Iterate()) {
			/* Check if there is a non-deleted buoy on the northern border. */
			if (st->IsInUse() && (TileX(st->xy) == 0 || TileY(st->xy) == 0)) {
				ShowErrorMessage(STR_CONFIG_SETTING_EDGES_NOT_EMPTY, INVALID_STRING_ID, WL_ERROR);
				return false;
			}
		}
	} else {
		for (uint i = 0; i < Map::MaxX(); i++) {
			if (TileHeight(TileXY(i, 1)) != 0) {
				ShowErrorMessage(STR_CONFIG_SETTING_EDGES_NOT_WATER, INVALID_STRING_ID, WL_ERROR);
				return false;
			}
		}
		for (uint i = 1; i < Map::MaxX(); i++) {
			if (!IsTileType(TileXY(i, Map::MaxY() - 1), MP_WATER) || TileHeight(TileXY(1, Map::MaxY())) != 0) {
				ShowErrorMessage(STR_CONFIG_SETTING_EDGES_NOT_WATER, INVALID_STRING_ID, WL_ERROR);
				return false;
			}
		}
		for (uint i = 0; i < Map::MaxY(); i++) {
			if (TileHeight(TileXY(1, i)) != 0) {
				ShowErrorMessage(STR_CONFIG_SETTING_EDGES_NOT_WATER, INVALID_STRING_ID, WL_ERROR);
				return false;
			}
		}
		for (uint i = 1; i < Map::MaxY(); i++) {
			if (!IsTileType(TileXY(Map::MaxX() - 1, i), MP_WATER) || TileHeight(TileXY(Map::MaxX(), i)) != 0) {
				ShowErrorMessage(STR_CONFIG_SETTING_EDGES_NOT_WATER, INVALID_STRING_ID, WL_ERROR);
				return false;
			}
		}
	}
	return true;
}

static void UpdateFreeformEdges(int32_t new_value)
{
	if (_game_mode == GM_MENU) return;

	if (new_value != 0) {
		for (uint x = 0; x < Map::SizeX(); x++) MakeVoid(TileXY(x, 0));
		for (uint y = 0; y < Map::SizeY(); y++) MakeVoid(TileXY(0, y));
	} else {
		/* Make tiles at the border water again. */
		for (uint i = 0; i < Map::MaxX(); i++) {
			SetTileHeight(TileXY(i, 0), 0);
			SetTileType(TileXY(i, 0), MP_WATER);
		}
		for (uint i = 0; i < Map::MaxY(); i++) {
			SetTileHeight(TileXY(0, i), 0);
			SetTileType(TileXY(0, i), MP_WATER);
		}
	}
	MarkWholeScreenDirty();
}

/**
 * Changing the setting "allow multiple NewGRF sets" is not allowed
 * if there are vehicles.
 */
static bool CheckDynamicEngines(int32_t &)
{
	if (_game_mode == GM_MENU) return true;

	if (!EngineOverrideManager::ResetToCurrentNewGRFConfig()) {
		ShowErrorMessage(STR_CONFIG_SETTING_DYNAMIC_ENGINES_EXISTING_VEHICLES, INVALID_STRING_ID, WL_ERROR);
		return false;
	}

	return true;
}

static bool CheckMaxHeightLevel(int32_t &new_value)
{
	if (_game_mode == GM_NORMAL) return false;
	if (_game_mode != GM_EDITOR) return true;

	/* Check if at least one mountain on the map is higher than the new value.
	 * If yes, disallow the change. */
	for (TileIndex t = 0; t < Map::Size(); t++) {
		if ((int32_t)TileHeight(t) > new_value) {
			ShowErrorMessage(STR_CONFIG_SETTING_TOO_HIGH_MOUNTAIN, INVALID_STRING_ID, WL_ERROR);
			/* Return old, unchanged value */
			return false;
		}
	}

	return true;
}

static void StationCatchmentChanged(int32_t)
{
	Station::RecomputeCatchmentForAll();
	MarkWholeScreenDirty();
}

static void MaxVehiclesChanged(int32_t)
{
	InvalidateWindowClassesData(WC_BUILD_TOOLBAR);
	MarkWholeScreenDirty();
}

static void InvalidateShipPathCache(int32_t)
{
	for (Ship *s : Ship::Iterate()) {
		s->path.clear();
	}
}

/**
 * Replace a passwords that are a literal asterisk with an empty string.
 * @param newval The new string value for this password field.
 * @return Always true.
 */
static bool ReplaceAsteriskWithEmptyPassword(std::string &newval)
{
	if (newval.compare("*") == 0) newval.clear();
	return true;
}

/** Update the game info, and send it to the clients when we are running as a server. */
static void UpdateClientConfigValues()
{
	NetworkServerUpdateGameInfo();

	if (_network_server) {
		NetworkServerSendConfigUpdate();
		SetWindowClassesDirty(WC_CLIENT_LIST);
	}
}

/**
 * Callback for when the player changes the timekeeping units.
 * @param Unused.
 */
static void ChangeTimekeepingUnits(int32_t)
{
	/* If service intervals are in time units (calendar days or real-world minutes), reset them to the correct defaults. */
	if (!_settings_client.company.vehicle.servint_ispercent) {
		UpdateAllServiceInterval(0);
	}

	/* If we are using calendar timekeeping, "minutes per year" must be default. */
	if (!TimerGameEconomy::UsingWallclockUnits(true)) {
		_settings_newgame.economy.minutes_per_calendar_year = CalendarTime::DEF_MINUTES_PER_YEAR;
	}

	InvalidateWindowClassesData(WC_GAME_OPTIONS, 0);

	/* It is possible to change these units in Scenario Editor. We must set the economy date appropriately. */
	if (_game_mode == GM_EDITOR) {
		TimerGameEconomy::Date new_economy_date;
		TimerGameEconomy::DateFract new_economy_date_fract;

		if (TimerGameEconomy::UsingWallclockUnits()) {
			/* If the new mode is wallclock units, set the economy year back to 1. */
			new_economy_date = TimerGameEconomy::ConvertYMDToDate(1, 0, 1);
			new_economy_date_fract = 0;
		} else {
			/* If the new mode is calendar units, sync the economy year with the calendar year. */
			new_economy_date = TimerGameCalendar::date.base();
			new_economy_date_fract = TimerGameCalendar::date_fract;
		}

		/* If you open a savegame as a scenario, there may already be link graphs and/or vehicles. These use economy date. */
		LinkGraphSchedule::instance.ShiftDates(new_economy_date - TimerGameEconomy::date);
		for (auto v : Vehicle::Iterate()) v->ShiftDates(new_economy_date - TimerGameEconomy::date);

		/* Only change the date after changing cached values above. */
		TimerGameEconomy::SetDate(new_economy_date, new_economy_date_fract);
	}
}

/**
 * Callback after the player changes the minutes per year.
 * @param new_value The intended new value of the setting, used for clamping.
 */
static void ChangeMinutesPerYear(int32_t new_value)
{
	/* We don't allow setting Minutes Per Year below default, unless it's to 0 for frozen calendar time. */
	if (new_value < CalendarTime::DEF_MINUTES_PER_YEAR) {
		int clamped;

		/* If the new value is 1, we're probably at 0 and trying to increase the value, so we should jump up to default. */
		if (new_value == 1) {
			clamped = CalendarTime::DEF_MINUTES_PER_YEAR;
		} else {
			clamped = CalendarTime::FROZEN_MINUTES_PER_YEAR;
		}

		/* Override the setting with the clamped value. */
		if (_game_mode == GM_MENU) {
			_settings_newgame.economy.minutes_per_calendar_year = clamped;
		} else {
			_settings_game.economy.minutes_per_calendar_year = clamped;
		}
	}

	/* If the setting value is not the default, force the game to use wallclock timekeeping units.
	 * This can only happen in the menu, since the pre_cb ensures this setting can only be changed there, or if we're already using wallclock units.
	 */
	if (_game_mode == GM_MENU && (_settings_newgame.economy.minutes_per_calendar_year != CalendarTime::DEF_MINUTES_PER_YEAR)) {
		_settings_newgame.economy.timekeeping_units = TKU_WALLCLOCK;
		InvalidateWindowClassesData(WC_GAME_OPTIONS, 0);
	}
}

/**
 * Pre-callback check when trying to change the timetable mode. This is locked to Seconds when using wallclock units.
 * @param Unused.
 * @return True if we allow changing the timetable mode.
 */
static bool CanChangeTimetableMode(int32_t &)
{
	return !TimerGameEconomy::UsingWallclockUnits();
}

/* End - Callback Functions */
