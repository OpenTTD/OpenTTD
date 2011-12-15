/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_widget.h Types related to the network widgets. */

#ifndef WIDGETS_NETWORK_WIDGET_H
#define WIDGETS_NETWORK_WIDGET_H

/** Widgets of the WC_NETWORK_WINDOW (WC_NETWORK_WINDOW is also used in NetworkContentListWindowWidgets, NetworkStartServerWidgets, and NetworkLobbyWindowWidgets). */
enum NetworkGameWindowWidgets {
	NGWW_MAIN,          ///< Main panel

	NGWW_CONNECTION,    ///< Label in front of connection droplist
	NGWW_CONN_BTN,      ///< 'Connection' droplist button
	NGWW_CLIENT_LABEL,  ///< Label in front of client name edit box
	NGWW_CLIENT,        ///< Panel with editbox to set client name

	NGWW_HEADER,        ///< Header container of the matrix
	NGWW_NAME,          ///< 'Name' button
	NGWW_CLIENTS,       ///< 'Clients' button
	NGWW_MAPSIZE,       ///< 'Map size' button
	NGWW_DATE,          ///< 'Date' button
	NGWW_YEARS,         ///< 'Years' button
	NGWW_INFO,          ///< Third button in the game list panel

	NGWW_MATRIX,        ///< Panel with list of games
	NGWW_SCROLLBAR,     ///< Scrollbar of matrix

	NGWW_LASTJOINED_LABEL, ///< Label "Last joined server:"
	NGWW_LASTJOINED,    ///< Info about the last joined server
	NGWW_LASTJOINED_SPACER, ///< Spacer after last joined server panel

	NGWW_DETAILS,       ///< Panel with game details
	NGWW_DETAILS_SPACER, ///< Spacer for game actual details
	NGWW_JOIN,          ///< 'Join game' button
	NGWW_REFRESH,       ///< 'Refresh server' button
	NGWW_NEWGRF,        ///< 'NewGRF Settings' button
	NGWW_NEWGRF_SEL,    ///< Selection 'widget' to hide the NewGRF settings
	NGWW_NEWGRF_MISSING,     ///< 'Find missing NewGRF online' button
	NGWW_NEWGRF_MISSING_SEL, ///< Selection widget for the above button

	NGWW_FIND,          ///< 'Find server' button
	NGWW_ADD,           ///< 'Add server' button
	NGWW_START,         ///< 'Start server' button
	NGWW_CANCEL,        ///< 'Cancel' button
};

/** Widgets of the WC_NETWORK_WINDOW (WC_NETWORK_WINDOW is also used in NetworkContentListWindowWidgets, NetworkGameWindowWidgets, and NetworkLobbyWindowWidgets). */
enum NetworkStartServerWidgets {
	NSSW_BACKGROUND,
	NSSW_GAMENAME_LABEL,
	NSSW_GAMENAME,          ///< Background for editbox to set game name
	NSSW_SETPWD,            ///< 'Set password' button
	NSSW_CONNTYPE_LABEL,
	NSSW_CONNTYPE_BTN,      ///< 'Connection type' droplist button
	NSSW_CLIENTS_LABEL,
	NSSW_CLIENTS_BTND,      ///< 'Max clients' downarrow
	NSSW_CLIENTS_TXT,       ///< 'Max clients' text
	NSSW_CLIENTS_BTNU,      ///< 'Max clients' uparrow
	NSSW_COMPANIES_LABEL,
	NSSW_COMPANIES_BTND,    ///< 'Max companies' downarrow
	NSSW_COMPANIES_TXT,     ///< 'Max companies' text
	NSSW_COMPANIES_BTNU,    ///< 'Max companies' uparrow
	NSSW_SPECTATORS_LABEL,
	NSSW_SPECTATORS_BTND,   ///< 'Max spectators' downarrow
	NSSW_SPECTATORS_TXT,    ///< 'Max spectators' text
	NSSW_SPECTATORS_BTNU,   ///< 'Max spectators' uparrow

	NSSW_LANGUAGE_LABEL,
	NSSW_LANGUAGE_BTN,      ///< 'Language spoken' droplist button

	NSSW_GENERATE_GAME,     ///< New game button
	NSSW_LOAD_GAME,         ///< Load game button
	NSSW_PLAY_SCENARIO,     ///< Play scenario button
	NSSW_PLAY_HEIGHTMAP,    ///< Play heightmap button

	NSSW_CANCEL,            ///< 'Cancel' button
};

/** Widgets of the WC_NETWORK_WINDOW (WC_NETWORK_WINDOW is also used in NetworkContentListWindowWidgets, NetworkGameWindowWidgets, and NetworkStartServerWidgets). */
enum NetworkLobbyWindowWidgets {
	NLWW_BACKGROUND, ///< Background panel
	NLWW_TEXT,       ///< Heading text
	NLWW_HEADER,     ///< Header above list of companies
	NLWW_MATRIX,     ///< List of companies
	NLWW_SCROLLBAR,  ///< Scroll bar
	NLWW_DETAILS,    ///< Company details
	NLWW_JOIN,       ///< 'Join company' button
	NLWW_NEW,        ///< 'New company' button
	NLWW_SPECTATE,   ///< 'Spectate game' button
	NLWW_REFRESH,    ///< 'Refresh server' button
	NLWW_CANCEL,     ///< 'Cancel' button
};

/** Widgets of the WC_CLIENT_LIST. */
enum ClientListWidgets {
	CLW_PANEL,
};

/** Widgets of the WC_CLIENT_LIST_POPUP. */
enum ClientListPopupWidgets {
	CLPW_PANEL,
};

/** Widgets of the WC_NETWORK_STATUS_WINDOW (WC_NETWORK_STATUS_WINDOW is also used in NetworkContentDownloadStatusWindowWidgets). */
enum NetworkJoinStatusWidgets {
	NJSW_BACKGROUND, ///< Background
	NJSW_CANCELOK,   ///< Cancel/OK button
};

/** Widgets of the WC_COMPANY_PASSWORD_WINDOW. */
enum NetworkCompanyPasswordWindowWidgets {
	NCPWW_BACKGROUND,               ///< The background of the interface
	NCPWW_LABEL,                    ///< Label in front of the password field
	NCPWW_PASSWORD,                 ///< Input field for the password
	NCPWW_SAVE_AS_DEFAULT_PASSWORD, ///< Toggle 'button' for saving the current password as default password
	NCPWW_CANCEL,                   ///< Close the window without changing anything
	NCPWW_OK,                       ///< Safe the password etc.
};


#endif /* WIDGETS_NETWORK_WIDGET_H */
