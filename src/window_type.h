/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file window_type.h Types related to windows */

#ifndef WINDOW_TYPE_H
#define WINDOW_TYPE_H


/**
 * Window classes
 */
enum WindowClass {
	WC_NONE,                    ///< No window, redirects to WC_MAIN_WINDOW.

	WC_MAIN_WINDOW = WC_NONE,   ///< Main window.
	WC_MAIN_TOOLBAR,            ///< Main toolbar (the long bar at the top..
	WC_STATUS_BAR,              ///< Statusbar (at the bottom of your screen).
	WC_BUILD_TOOLBAR,           ///< Build toolbar.
	WC_SCEN_BUILD_TOOLBAR,      ///< Scenario build toolbar.
	WC_BUILD_TREES,             ///< Build trees toolbar.
	WC_TRANSPARENCY_TOOLBAR,    ///< Transparency toolbar.
	WC_BUILD_SIGNAL,            ///< Build signal toolbar.

	WC_SMALLMAP,                ///< Small map.
	WC_ERRMSG,                  ///< Error message.
	WC_TOOLTIPS,                ///< Tooltip window.
	WC_QUERY_STRING,            ///< Query string window.
	WC_CONFIRM_POPUP_QUERY,     ///< Popup with confirm question.
	WC_SAVELOAD,                ///< Saveload window.
	WC_LAND_INFO,               ///< Land info window.
	WC_DROPDOWN_MENU,           ///< Drop down menu.
	WC_OSK,                     ///< On Screen Keyboard.
	WC_SET_DATE,                ///< Set date.

	WC_AI_SETTINGS,             ///< AI settings.
	WC_GRF_PARAMETERS,          ///< NewGRF parameters.
	WC_NEWGRF_TEXTFILE,         ///< NewGRF textfile.

	WC_TOWN_AUTHORITY,          ///< Town authority.
	WC_VEHICLE_DETAILS,         ///< Vehicle details.
	WC_VEHICLE_REFIT,           ///< Vehicle refit.
	WC_VEHICLE_ORDERS,          ///< Vehicle orders.
	WC_REPLACE_VEHICLE,         ///< Replace vehicle window.
	WC_VEHICLE_TIMETABLE,       ///< Vehicle timetable.
	WC_COMPANY_COLOUR,          ///< Company colour selection.
	WC_COMPANY_MANAGER_FACE,    ///< Alter company face window.
	WC_SELECT_STATION,          ///< Select station (when joining stations).

	WC_NEWS_WINDOW,             ///< News window.
	WC_TOWN_DIRECTORY,          ///< Town directory.
	WC_SUBSIDIES_LIST,          ///< Subsidies list.
	WC_INDUSTRY_DIRECTORY,      ///< Industry directory.
	WC_MESSAGE_HISTORY,         ///< News history list.
	WC_SIGN_LIST,               ///< Sign list.
	WC_AI_LIST,                 ///< AI list.

	WC_STATION_LIST,            ///< Station list.
	WC_TRAINS_LIST,             ///< Trains list.
	WC_ROADVEH_LIST,            ///< Road vehicle list.
	WC_SHIPS_LIST,              ///< Ships list.
	WC_AIRCRAFT_LIST,           ///< Aircraft list.

	WC_TOWN_VIEW,               ///< Town view.
	WC_VEHICLE_VIEW,            ///< Vehicle view.
	WC_STATION_VIEW,            ///< Station view.
	WC_VEHICLE_DEPOT,           ///< Depot view.
	WC_WAYPOINT_VIEW,           ///< Waypoint view.
	WC_INDUSTRY_VIEW,           ///< Industry view.
	WC_COMPANY,                 ///< Company view.

	WC_BUILD_OBJECT,            ///< Build object
	WC_BUILD_VEHICLE,           ///< Build vehicle.
	WC_BUILD_BRIDGE,            ///< Build bridge.
	WC_BUILD_STATION,           ///< Build station.
	WC_BUS_STATION,             ///< Build bus station.
	WC_TRUCK_STATION,           ///< Build truck station.
	WC_BUILD_DEPOT,             ///< Build depot.
	WC_FOUND_TOWN,              ///< Found a town.
	WC_BUILD_INDUSTRY,          ///< Build industry.

	WC_SELECT_GAME,             ///< Select game window.
	WC_SCEN_LAND_GEN,           ///< Landscape generation (in Scenario Editor).
	WC_GENERATE_LANDSCAPE,      ///< Generate landscape (newgame).
	WC_MODAL_PROGRESS,          ///< Progress report of landscape generation.

	WC_NETWORK_WINDOW,          ///< Network window.
	WC_CLIENT_LIST,             ///< Client list.
	WC_CLIENT_LIST_POPUP,       ///< Popup for the client list.
	WC_NETWORK_STATUS_WINDOW,   ///< Network status window.
	WC_SEND_NETWORK_MSG,        ///< Chatbox.
	WC_COMPANY_PASSWORD_WINDOW, ///< Company password query.

	WC_INDUSTRY_CARGOES,        ///< Industry cargoes chain.
	WC_GRAPH_LEGEND,            ///< Legend for graphs.
	WC_FINANCES,                ///< Finances of a company.
	WC_INCOME_GRAPH,            ///< Income graph.
	WC_OPERATING_PROFIT,        ///< Operating profit graph.
	WC_DELIVERED_CARGO,         ///< Delivered cargo graph.
	WC_PERFORMANCE_HISTORY,     ///< Performance history graph.
	WC_COMPANY_VALUE,           ///< Company value graph.
	WC_COMPANY_LEAGUE,          ///< Company league window.
	WC_PAYMENT_RATES,           ///< Payment rates graph.
	WC_PERFORMANCE_DETAIL,      ///< Performance detail window.
	WC_COMPANY_INFRASTRUCTURE,  ///< Company infrastructure overview.

	WC_BUY_COMPANY,             ///< Buyout company (merger).
	WC_ENGINE_PREVIEW,          ///< Engine preview window.

	WC_MUSIC_WINDOW,            ///< Music window.
	WC_MUSIC_TRACK_SELECTION,   ///< Music track selection.
	WC_GAME_OPTIONS,            ///< Game options window.
	WC_CUSTOM_CURRENCY,         ///< Custom currency.
	WC_CHEATS,                  ///< Cheat window.
	WC_EXTRA_VIEW_PORT,         ///< Extra viewport.

	WC_CONSOLE,                 ///< Console.
	WC_BOOTSTRAP,               ///< Bootstrap.
	WC_HIGHSCORE,               ///< Highscore.
	WC_ENDSCREEN,               ///< Endscreen.

	WC_AI_DEBUG,                ///< AI debug window.
	WC_NEWGRF_INSPECT,          ///< NewGRF inspect (debug).
	WC_SPRITE_ALIGNER,          ///< Sprite aligner (debug).

	WC_INVALID = 0xFFFF,        ///< Invalid window.
};

/**
 * Data value for #Window::OnInvalidateData() of windows with class #WC_GAME_OPTIONS.
 */
enum GameOptionsInvalidationData {
	GOID_DEFAULT = 0,
	GOID_NEWGRF_RESCANNED,     ///< NewGRFs were just rescanned.
	GOID_NEWGRF_LIST_EDITED,   ///< List of active NewGRFs is being edited.
	GOID_NEWGRF_PRESET_LOADED, ///< A NewGRF preset was picked.
	GOID_DIFFICULTY_CHANGED,   ///< Difficulty settings were changed.
};

struct Window;

/** Number to differentiate different windows of the same class */
typedef int32 WindowNumber;

#endif /* WINDOW_TYPE_H */
