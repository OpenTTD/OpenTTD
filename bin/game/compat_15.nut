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

GSWindow.WC_NONE <- GSWindow.WindowClass.None;
GSWindow.WC_MAIN_WINDOW <- GSWindow.WindowClass.MainWindow;
GSWindow.WC_MAIN_TOOLBAR <- GSWindow.WindowClass.MainToolbar;
GSWindow.WC_STATUS_BAR <- GSWindow.WindowClass.Statusbar;
GSWindow.WC_BUILD_TOOLBAR <- GSWindow.WindowClass.BuildToolbar;
GSWindow.WC_SCEN_BUILD_TOOLBAR <- GSWindow.WindowClass.ScenarioBuildToolbar;
GSWindow.WC_BUILD_TREES <- GSWindow.WindowClass.BuildTrees;
GSWindow.WC_TRANSPARENCY_TOOLBAR <- GSWindow.WindowClass.TransparencyToolbar;
GSWindow.WC_BUILD_SIGNAL <- GSWindow.WindowClass.BuildSignal;
GSWindow.WC_SMALLMAP <- GSWindow.WindowClass.SmallMap;
GSWindow.WC_ERRMSG <- GSWindow.WindowClass.ErrorMessage;
GSWindow.WC_TOOLTIPS <- GSWindow.WindowClass.ToolTips;
GSWindow.WC_QUERY_STRING <- GSWindow.WindowClass.QueryString;
GSWindow.WC_CONFIRM_POPUP_QUERY <- GSWindow.WindowClass.ConfirmPopupQuery;
GSWindow.WC_GOAL_QUESTION <- GSWindow.WindowClass.GoalQuestion;
GSWindow.WC_SAVELOAD <- GSWindow.WindowClass.SaveLoad;
GSWindow.WC_LAND_INFO <- GSWindow.WindowClass.LandInfo;
GSWindow.WC_DROPDOWN_MENU <- GSWindow.WindowClass.DropdownMenu;
GSWindow.WC_OSK <- GSWindow.WindowClass.OnScreenKeyboard;
GSWindow.WC_SET_DATE <- GSWindow.WindowClass.SetDate;
GSWindow.WC_SCRIPT_SETTINGS <- GSWindow.WindowClass.ScriptSettings;
GSWindow.WC_GRF_PARAMETERS <- GSWindow.WindowClass.NewGRFParameters;
GSWindow.WC_TEXTFILE <- GSWindow.WindowClass.Textfile;
GSWindow.WC_TOWN_AUTHORITY <- GSWindow.WindowClass.TownAuthority;
GSWindow.WC_VEHICLE_DETAILS <- GSWindow.WindowClass.VehicleDetails;
GSWindow.WC_VEHICLE_REFIT <- GSWindow.WindowClass.VehicleRefit;
GSWindow.WC_VEHICLE_ORDERS <- GSWindow.WindowClass.VehicleOrders;
GSWindow.WC_REPLACE_VEHICLE <- GSWindow.WindowClass.ReplaceVehicle;
GSWindow.WC_VEHICLE_TIMETABLE <- GSWindow.WindowClass.VehicleTimetable;
GSWindow.WC_COMPANY_COLOUR <- GSWindow.WindowClass.CompanyLivery;
GSWindow.WC_COMPANY_MANAGER_FACE <- GSWindow.WindowClass.CompanyManagerFace;
GSWindow.WC_SELECT_STATION <- GSWindow.WindowClass.JoinStation;
GSWindow.WC_NEWS_WINDOW <- GSWindow.WindowClass.News;
GSWindow.WC_TOWN_DIRECTORY <- GSWindow.WindowClass.TownDirectory;
GSWindow.WC_SUBSIDIES_LIST <- GSWindow.WindowClass.SubsidyList;
GSWindow.WC_INDUSTRY_DIRECTORY <- GSWindow.WindowClass.IndustryDirectory;
GSWindow.WC_MESSAGE_HISTORY <- GSWindow.WindowClass.MessageHistory;
GSWindow.WC_SIGN_LIST <- GSWindow.WindowClass.SignList;
GSWindow.WC_SCRIPT_LIST <- GSWindow.WindowClass.ScriptList;
GSWindow.WC_GOALS_LIST <- GSWindow.WindowClass.GoalList;
GSWindow.WC_STORY_BOOK <- GSWindow.WindowClass.StoryBook;
GSWindow.WC_STATION_LIST <- GSWindow.WindowClass.StationList;
GSWindow.WC_TRAINS_LIST <- GSWindow.WindowClass.TrainList;
GSWindow.WC_ROADVEH_LIST <- GSWindow.WindowClass.RoadVehicleList;
GSWindow.WC_SHIPS_LIST <- GSWindow.WindowClass.ShipList;
GSWindow.WC_AIRCRAFT_LIST <- GSWindow.WindowClass.AircraftList;
GSWindow.WC_TOWN_VIEW <- GSWindow.WindowClass.TownView;
GSWindow.WC_VEHICLE_VIEW <- GSWindow.WindowClass.VehicleView;
GSWindow.WC_STATION_VIEW <- GSWindow.WindowClass.StationView;
GSWindow.WC_VEHICLE_DEPOT <- GSWindow.WindowClass.VehicleDepot;
GSWindow.WC_WAYPOINT_VIEW <- GSWindow.WindowClass.WaypointView;
GSWindow.WC_INDUSTRY_VIEW <- GSWindow.WindowClass.IndustryView;
GSWindow.WC_COMPANY <- GSWindow.WindowClass.Company;
GSWindow.WC_BUILD_OBJECT <- GSWindow.WindowClass.BuildObject;
GSWindow.WC_BUILD_HOUSE <- GSWindow.WindowClass.BuildHouse;
GSWindow.WC_BUILD_VEHICLE <- GSWindow.WindowClass.BuildVehicle;
GSWindow.WC_BUILD_BRIDGE <- GSWindow.WindowClass.BuildBridge;
GSWindow.WC_BUILD_STATION <- GSWindow.WindowClass.BuildStation;
GSWindow.WC_BUS_STATION <- GSWindow.WindowClass.BuildBusStation;
GSWindow.WC_TRUCK_STATION <- GSWindow.WindowClass.BuildTruckStation;
GSWindow.WC_BUILD_DEPOT <- GSWindow.WindowClass.BuildDepot;
GSWindow.WC_BUILD_WAYPOINT <- GSWindow.WindowClass.BuildWaypoint;
GSWindow.WC_FOUND_TOWN <- GSWindow.WindowClass.FoundTown;
GSWindow.WC_BUILD_INDUSTRY <- GSWindow.WindowClass.BuildIndustry;
GSWindow.WC_SELECT_GAME <- GSWindow.WindowClass.SelectGame;
GSWindow.WC_SCEN_LAND_GEN <- GSWindow.WindowClass.ScenarioGenerateLandscape;
GSWindow.WC_GENERATE_LANDSCAPE <- GSWindow.WindowClass.GenerateLandscape;
GSWindow.WC_MODAL_PROGRESS <- GSWindow.WindowClass.ModalProgress;
GSWindow.WC_NETWORK_WINDOW <- GSWindow.WindowClass.Network;
GSWindow.WC_CLIENT_LIST <- GSWindow.WindowClass.NetworkClientList;
GSWindow.WC_NETWORK_STATUS_WINDOW <- GSWindow.WindowClass.NetworkStatus;
GSWindow.WC_NETWORK_ASK_RELAY <- GSWindow.WindowClass.NetworkAskRelay;
GSWindow.WC_NETWORK_ASK_SURVEY <- GSWindow.WindowClass.NetworkAskSurvey;
GSWindow.WC_SEND_NETWORK_MSG <- GSWindow.WindowClass.NetworkChat;
GSWindow.WC_INDUSTRY_CARGOES <- GSWindow.WindowClass.IndustryCargoes;
GSWindow.WC_GRAPH_LEGEND <- GSWindow.WindowClass.GraphLegend;
GSWindow.WC_FINANCES <- GSWindow.WindowClass.Finances;
GSWindow.WC_INCOME_GRAPH <- GSWindow.WindowClass.IncomeGraph;
GSWindow.WC_OPERATING_PROFIT <- GSWindow.WindowClass.OperatingProfitGraph;
GSWindow.WC_DELIVERED_CARGO <- GSWindow.WindowClass.DeliveredCargoGraph;
GSWindow.WC_PERFORMANCE_HISTORY <- GSWindow.WindowClass.PerformanceGraph;
GSWindow.WC_COMPANY_VALUE <- GSWindow.WindowClass.CompanyValueGraph;
GSWindow.WC_COMPANY_LEAGUE <- GSWindow.WindowClass.CompanyLeague;
GSWindow.WC_PAYMENT_RATES <- GSWindow.WindowClass.CargoPaymentRatesGraph;
GSWindow.WC_PERFORMANCE_DETAIL <- GSWindow.WindowClass.PerformanceDetail;
GSWindow.WC_INDUSTRY_PRODUCTION <- GSWindow.WindowClass.IndustryProductionGraph;
GSWindow.WC_TOWN_CARGO_GRAPH <- GSWindow.WindowClass.TownCargoGraph;
GSWindow.WC_COMPANY_INFRASTRUCTURE <- GSWindow.WindowClass.CompanyInfrastructure;
GSWindow.WC_BUY_COMPANY <- GSWindow.WindowClass.BuyCompany;
GSWindow.WC_ENGINE_PREVIEW <- GSWindow.WindowClass.EnginePreview;
GSWindow.WC_MUSIC_WINDOW <- GSWindow.WindowClass.Music;
GSWindow.WC_MUSIC_TRACK_SELECTION <- GSWindow.WindowClass.MusicTrackSelection;
GSWindow.WC_GAME_OPTIONS <- GSWindow.WindowClass.GameOptions;
GSWindow.WC_CUSTOM_CURRENCY <- GSWindow.WindowClass.CustomCurrenty;
GSWindow.WC_CHEATS <- GSWindow.WindowClass.Cheat;
GSWindow.WC_EXTRA_VIEWPORT <- GSWindow.WindowClass.ExtraViewport;
GSWindow.WC_CONSOLE <- GSWindow.WindowClass.Console;
GSWindow.WC_BOOTSTRAP <- GSWindow.WindowClass.Bootstrap;
GSWindow.WC_HIGHSCORE <- GSWindow.WindowClass.Highscore;
GSWindow.WC_ENDSCREEN <- GSWindow.WindowClass.Endscreen;
GSWindow.WC_SCRIPT_DEBUG <- GSWindow.WindowClass.ScriptDebug;
GSWindow.WC_NEWGRF_INSPECT <- GSWindow.WindowClass.NewGRFInspect;
GSWindow.WC_SPRITE_ALIGNER <- GSWindow.WindowClass.SpriteAligner;
GSWindow.WC_LINKGRAPH_LEGEND <- GSWindow.WindowClass.LinkGraphLegend;
GSWindow.WC_SAVE_PRESET <- GSWindow.WindowClass.SavePreset;
GSWindow.WC_FRAMERATE_DISPLAY <- GSWindow.WindowClass.FramerateDisplay;
GSWindow.WC_FRAMETIME_GRAPH <- GSWindow.WindowClass.FrametimeGraph;
GSWindow.WC_SCREENSHOT <- GSWindow.WindowClass.Screenshot;
GSWindow.WC_HELPWIN <- GSWindow.WindowClass.Help;
GSWindow.WC_INVALID <- GSWindow.WindowClass.Invalid;
