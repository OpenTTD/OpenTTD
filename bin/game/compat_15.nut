/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/* This file contains code to downgrade the API from 16 to 15. */

/*
 * These window/widget related enumerations have never been stable, but breaking anything using them
 * is a really big ask when restructuring code. If in the future things are removed from the underlying
 * enumerations, feel free to remove them from this compatibility script.
 */
GSWindow.WN_GAME_OPTIONS_AI <- GSWindow.GameOptionsWindowNumber.AI;
GSWindow.WN_GAME_OPTIONS_GS <- GSWindow.GameOptionsWindowNumber.GS;
GSWindow.WN_GAME_OPTIONS_ABOUT <- GSWindow.GameOptionsWindowNumber.About;
GSWindow.WN_GAME_OPTIONS_NEWGRF_STATE <- GSWindow.GameOptionsWindowNumber.NewGRFState;
GSWindow.WN_GAME_OPTIONS_GAME_OPTIONS <- GSWindow.GameOptionsWindowNumber.GameOptions;
GSWindow.WN_GAME_OPTIONS_GAME_SETTINGS <- GSWindow.GameOptionsWindowNumber.GameSettings;
GSWindow.WN_QUERY_STRING <- GSWindow.QueryStringWindowNumber.Default;
GSWindow.WN_QUERY_STRING_SIGN <- GSWindow.QueryStringWindowNumber.Sign;
GSWindow.WN_CONFIRM_POPUP_QUERY <- GSWindow.ConfirmPopupQueryWindowNumber.Default;
GSWindow.WN_CONFIRM_POPUP_QUERY_BOOTSTRAP <- GSWindow.ConfirmPopupQueryWindowNumber.Bootstrap;
GSWindow.WN_NETWORK_WINDOW_GAME <- GSWindow.NetworkWindowNumber.Game;
GSWindow.WN_NETWORK_WINDOW_CONTENT_LIST <- GSWindow.NetworkWindowNumber.ContentList;
GSWindow.WN_NETWORK_WINDOW_START <- GSWindow.NetworkWindowNumber.StartServer;
GSWindow.WN_NETWORK_STATUS_WINDOW_JOIN <- GSWindow.NetworkStatusWindowNumber.Join;
GSWindow.WN_NETWORK_STATUS_WINDOW_CONTENT_DOWNLOAD <- GSWindow.NetworkStatusWindowNumber.ContentDownload;
