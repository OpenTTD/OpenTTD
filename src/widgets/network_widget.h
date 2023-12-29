/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_widget.h Types related to the network widgets. */

#ifndef WIDGETS_NETWORK_WIDGET_H
#define WIDGETS_NETWORK_WIDGET_H

/** Widgets of the #NetworkGameWindow class. */
enum NetworkGameWidgets : WidgetID {
	WID_NG_MAIN,               ///< Main panel.

	WID_NG_CLIENT_LABEL,       ///< Label in front of client name edit box.
	WID_NG_CLIENT,             ///< Panel with editbox to set client name.
	WID_NG_FILTER_LABEL,       ///< Label in front of the filter/search edit box.
	WID_NG_FILTER,             ///< Panel with the edit box to enter the search text.

	WID_NG_NAME,               ///< 'Name' button.
	WID_NG_CLIENTS,            ///< 'Clients' button.
	WID_NG_MAPSIZE,            ///< 'Map size' button.
	WID_NG_DATE,               ///< 'Date' button.
	WID_NG_YEARS,              ///< 'Years' button.
	WID_NG_INFO,               ///< Third button in the game list panel.

	WID_NG_MATRIX,             ///< Panel with list of games.
	WID_NG_SCROLLBAR,          ///< Scrollbar of matrix.

	WID_NG_LASTJOINED_LABEL,   ///< Label "Last joined server:".
	WID_NG_LASTJOINED,         ///< Info about the last joined server.
	WID_NG_LASTJOINED_SPACER,  ///< Spacer after last joined server panel.

	WID_NG_DETAILS,            ///< Panel with game details.
	WID_NG_JOIN,               ///< 'Join game' button.
	WID_NG_REFRESH,            ///< 'Refresh server' button.
	WID_NG_NEWGRF,             ///< 'NewGRF Settings' button.
	WID_NG_NEWGRF_SEL,         ///< Selection 'widget' to hide the NewGRF settings.
	WID_NG_NEWGRF_MISSING,     ///< 'Find missing NewGRF online' button.
	WID_NG_NEWGRF_MISSING_SEL, ///< Selection widget for the above button.

	WID_NG_SEARCH_INTERNET,    ///< 'Search internet server' button.
	WID_NG_SEARCH_LAN,         ///< 'Search LAN server' button.
	WID_NG_ADD,                ///< 'Add server' button.
	WID_NG_START,              ///< 'Start server' button.
	WID_NG_CANCEL,             ///< 'Cancel' button.
};

/** Widgets of the #NetworkStartServerWindow class. */
enum NetworkStartServerWidgets : WidgetID {
	WID_NSS_BACKGROUND,        ///< Background of the window.
	WID_NSS_GAMENAME_LABEL,    ///< Label for the game name.
	WID_NSS_GAMENAME,          ///< Background for editbox to set game name.
	WID_NSS_SETPWD,            ///< 'Set password' button.
	WID_NSS_CONNTYPE_LABEL,    ///< Label for 'connection type'.
	WID_NSS_CONNTYPE_BTN,      ///< 'Connection type' droplist button.
	WID_NSS_CLIENTS_LABEL,     ///< Label for 'max clients'.
	WID_NSS_CLIENTS_BTND,      ///< 'Max clients' downarrow.
	WID_NSS_CLIENTS_TXT,       ///< 'Max clients' text.
	WID_NSS_CLIENTS_BTNU,      ///< 'Max clients' uparrow.
	WID_NSS_COMPANIES_LABEL,   ///< Label for 'max companies'.
	WID_NSS_COMPANIES_BTND,    ///< 'Max companies' downarrow.
	WID_NSS_COMPANIES_TXT,     ///< 'Max companies' text.
	WID_NSS_COMPANIES_BTNU,    ///< 'Max companies' uparrow.

	WID_NSS_GENERATE_GAME,     ///< New game button.
	WID_NSS_LOAD_GAME,         ///< Load game button.
	WID_NSS_PLAY_SCENARIO,     ///< Play scenario button.
	WID_NSS_PLAY_HEIGHTMAP,    ///< Play heightmap button.

	WID_NSS_CANCEL,            ///< 'Cancel' button.
};

/** Widgets of the #NetworkClientListWindow class. */
enum ClientListWidgets : WidgetID {
	WID_CL_PANEL,                      ///< Panel of the window.
	WID_CL_SERVER_SELECTOR,            ///< Selector to hide the server frame.
	WID_CL_SERVER_NAME,                ///< Server name.
	WID_CL_SERVER_NAME_EDIT,           ///< Edit button for server name.
	WID_CL_SERVER_VISIBILITY,          ///< Server visibility.
	WID_CL_SERVER_INVITE_CODE,         ///< Invite code for this server.
	WID_CL_SERVER_CONNECTION_TYPE,     ///< The type of connection the Game Coordinator detected for this server.
	WID_CL_CLIENT_NAME,                ///< Client name.
	WID_CL_CLIENT_NAME_EDIT,           ///< Edit button for client name.
	WID_CL_MATRIX,                     ///< Company/client list.
	WID_CL_SCROLLBAR,                  ///< Scrollbar for company/client list.
	WID_CL_COMPANY_JOIN,               ///< Used for QueryWindow when a company has a password.
	WID_CL_CLIENT_COMPANY_COUNT,       ///< Count of clients and companies.
};

/** Widgets of the #NetworkJoinStatusWindow class. */
enum NetworkJoinStatusWidgets : WidgetID {
	WID_NJS_PROGRESS_BAR,  ///< Simple progress bar.
	WID_NJS_PROGRESS_TEXT, ///< Text explaining what is happening.
	WID_NJS_CANCELOK,      ///< Cancel / OK button.
};

/** Widgets of the #NetworkCompanyPasswordWindow class. */
enum NetworkCompanyPasswordWidgets : WidgetID {
	WID_NCP_BACKGROUND,               ///< Background of the window.
	WID_NCP_LABEL,                    ///< Label in front of the password field.
	WID_NCP_PASSWORD,                 ///< Input field for the password.
	WID_NCP_SAVE_AS_DEFAULT_PASSWORD, ///< Toggle 'button' for saving the current password as default password.
	WID_NCP_WARNING,                  ///< Warning text about password security
	WID_NCP_CANCEL,                   ///< Close the window without changing anything.
	WID_NCP_OK,                       ///< Safe the password etc.
};

/** Widgets of the #NetworkAskRelayWindow class. */
enum NetworkAskRelayWidgets : WidgetID {
	WID_NAR_CAPTION,    ///< Caption of the window.
	WID_NAR_TEXT,       ///< Text in the window.
	WID_NAR_NO,         ///< "No" button.
	WID_NAR_YES_ONCE,   ///< "Yes, once" button.
	WID_NAR_YES_ALWAYS, ///< "Yes, always" button.
};

/** Widgets of the #NetworkAskSurveyWindow class. */
enum NetworkAskSurveyWidgets : WidgetID {
	WID_NAS_CAPTION,    ///< Caption of the window.
	WID_NAS_TEXT,       ///< Text in the window.
	WID_NAS_PREVIEW,    ///< "Preview" button.
	WID_NAS_LINK,       ///< "Details & Privacy" button.
	WID_NAS_NO,         ///< "No" button.
	WID_NAS_YES,        ///< "Yes" button.
};

#endif /* WIDGETS_NETWORK_WIDGET_H */
