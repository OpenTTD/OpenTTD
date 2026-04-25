/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file window_type.h Types related to windows. */

#ifndef WINDOW_TYPE_H
#define WINDOW_TYPE_H

#include "core/convertible_through_base.hpp"
#include "core/enum_type.hpp"

/**
 * Widget ID.
 * Even though the ID is signed, actual IDs must be non-negative.
 * Negative IDs are used for special cases, like denoting 'no widget'.
 */
using WidgetID = int;

/** An invalid widget index. */
static constexpr WidgetID INVALID_WIDGET = -1;

/** Window numbers for GameOptions windows. */
enum class GameOptionsWindowNumber : uint8_t {
	AI, ///< AI settings.
	GS, ///< GS settings.
	About, ///< About window.
	NewGRFState, ///< NewGRF settings.
	GameOptions, ///< Game options.
	GameSettings, ///< Game settings.
};

/** Window numbers for QueryString windows. */
enum class QueryStringWindowNumber : uint8_t {
	Default, ///< Query string.
	Sign, ///< Query string for signs.
};

/** Window numbers for PopupQuery windows. */
enum class ConfirmPopupQueryWindowNumber : uint8_t {
	Default, ///< Query popup confirm.
	Bootstrap, ///< Query popup confirm for bootstrap.
};

/** Window numbers for network windows. */
enum class NetworkWindowNumber : uint8_t {
	Game, ///< Network game window.
	ContentList, ///< Network content list.
	StartServer, ///< Network start server.
};

/** Window number for network status windows. */
enum class NetworkStatusWindowNumber : uint8_t {
	Join, ///< Network join status.
	ContentDownload, ///< Network content download status.
};

/** %Window classes. */
enum class WindowClass : uint16_t {
	None, ///< No window, redirects to WindowClass::MainWindow.

	/**
	 * Main window; %Window numbers:
	 *   - 0 = #MainWidgets
	 */
	MainWindow = WindowClass::None,

	/**
	 * Main toolbar (the long bar at the top); %Window numbers:
	 *   - 0 = #ToolbarNormalWidgets
	 *   - 0 = #ToolbarEditorWidgets
	 */
	MainToolbar,

	/**
	 * Statusbar (at the bottom of your screen); %Window numbers:
	 *   - 0 = #StatusbarWidgets
	 */
	Statusbar,

	/**
	 * Build toolbar; %Window numbers:
	 *   - #TRANSPORT_RAIL = #RailToolbarWidgets
	 *   - #TRANSPORT_AIR = #AirportToolbarWidgets
	 *   - #TRANSPORT_WATER = #DockToolbarWidgets
	 *   - #TRANSPORT_ROAD = #RoadToolbarWidgets
	 */
	BuildToolbar,

	/**
	 * Scenario build toolbar; %Window numbers:
	 *   - #TRANSPORT_WATER = #DockToolbarWidgets
	 *   - #TRANSPORT_ROAD = #RoadToolbarWidgets
	 */
	ScenarioBuildToolbar,

	/**
	 * Build trees toolbar; %Window numbers:
	 *   - 0 = #BuildTreesWidgets
	 */
	BuildTrees,

	/**
	 * Transparency toolbar; %Window numbers:
	 *   - 0 = #TransparencyToolbarWidgets
	 */
	TransparencyToolbar,

	/**
	 * Build signal toolbar; %Window numbers:
	 *   - #TRANSPORT_RAIL = #BuildSignalWidgets
	 */
	BuildSignal,

	/**
	 * Small map; %Window numbers:
	 *   - 0 = #SmallMapWidgets
	 */
	SmallMap,

	/**
	 * Error message; %Window numbers:
	 *   - 0 = #ErrorMessageWidgets
	 */
	ErrorMessage,

	/**
	 * Tooltip window; %Window numbers:
	 *   - 0 = #ToolTipsWidgets
	 */
	ToolTips,

	/**
	 * Query string window; %Window numbers:
	 *   - #QueryStringWindowNumber::Default = #QueryStringWidgets
	 *   - #QueryStringWindowNumber::Sign = #QueryEditSignWidgets
	 */
	QueryString,

	/**
	 * Popup with confirm question; %Window numbers:
	 *   - #ConfirmPopupQueryWindowNumber::Default = #QueryWidgets
	 *   - #ConfirmPopupQueryWindowNumber::Bootstrap = #BootstrapAskForDownloadWidgets
	 */
	ConfirmPopupQuery,

	/**
	 * Popup with a set of buttons, designed to ask the user a question
	 *  from a GameScript. %Window numbers:
	 *   - uniqueid = #GoalQuestionWidgets
	 */
	GoalQuestion,


	/**
	 * Saveload window; %Window numbers:
	 *   - 0 = #SaveLoadWidgets
	 */
	SaveLoad,

	/**
	 * Land info window; %Window numbers:
	 *   - 0 = #LandInfoWidgets
	 */
	LandInfo,

	/**
	 * Drop down menu; %Window numbers:
	 *   - 0 = #DropdownMenuWidgets
	 */
	DropdownMenu,

	/**
	 * On Screen Keyboard; %Window numbers:
	 *   - 0 = #OnScreenKeyboardWidgets
	 */
	OnScreenKeyboard,

	/**
	 * Set date; %Window numbers:
	 *   - #VehicleID = #SetDateWidgets
	 */
	SetDate,


	/**
	 * Script settings; %Window numbers:
	 *   - 0 = #ScriptSettingsWidgets
	 */
	ScriptSettings,

	/**
	 * NewGRF parameters; %Window numbers:
	 *   - 0 = #NewGRFParametersWidgets
	 */
	NewGRFParameters,

	/**
	 * textfile; %Window numbers:
	 *   - 0 = #TextfileWidgets
	 */
	Textfile,


	/**
	 * Town authority; %Window numbers:
	 *   - #TownID = #TownAuthorityWidgets
	 */
	TownAuthority,

	/**
	 * Vehicle details; %Window numbers:
	 *   - #VehicleID = #VehicleDetailsWidgets
	 */
	VehicleDetails,

	/**
	 * Vehicle refit; %Window numbers:
	 *   - #VehicleID = #VehicleRefitWidgets
	 */
	VehicleRefit,

	/**
	 * Vehicle orders; %Window numbers:
	 *   - #VehicleID = #OrderWidgets
	 */
	VehicleOrders,

	/**
	 * Replace vehicle window; %Window numbers:
	 *   - #VehicleType = #ReplaceVehicleWidgets
	 */
	ReplaceVehicle,

	/**
	 * Vehicle timetable; %Window numbers:
	 *   - #VehicleID = #VehicleTimetableWidgets
	 */
	VehicleTimetable,

	/**
	 * Company colour selection; %Window numbers:
	 *   - #CompanyID = #SelectCompanyLiveryWidgets
	 */
	CompanyLivery,

	/**
	 * Alter company face window; %Window numbers:
	 *   - #CompanyID = #SelectCompanyManagerFaceWidgets
	 */
	CompanyManagerFace,

	/**
	 * Select station (when joining stations); %Window numbers:
	 *   - 0 = #JoinStationWidgets
	 */
	JoinStation,

	/**
	 * News window; %Window numbers:
	 *   - 0 = #NewsWidgets
	 */
	News,

	/**
	 * Town directory; %Window numbers:
	 *   - 0 = #TownDirectoryWidgets
	 */
	TownDirectory,

	/**
	 * Subsidies list; %Window numbers:
	 *   - 0 = #SubsidyListWidgets
	 */
	SubsidyList,

	/**
	 * Industry directory; %Window numbers:
	 *   - 0 = #IndustryDirectoryWidgets
	 */
	IndustryDirectory,

	/**
	 * News history list; %Window numbers:
	 *   - 0 = #MessageHistoryWidgets
	 */
	MessageHistory,

	/**
	 * Sign list; %Window numbers:
	 *   - 0 = #SignListWidgets
	 */
	SignList,

	/**
	 * Scripts list; %Window numbers:
	 *   - 0 = #ScriptListWidgets
	 */
	ScriptList,

	/**
	 * Goals list; %Window numbers:
	 *   - 0 ; #GoalListWidgets
	 */
	GoalList,

	/**
	 * Story book; %Window numbers:
	 *   - CompanyID = #StoryBookWidgets
	 */
	StoryBook,

	/**
	 * Station list; %Window numbers:
	 *   - #CompanyID = #StationListWidgets
	 */
	StationList,

	/**
	 * Trains list; %Window numbers:
	 *   - Packed value = #GroupListWidgets / #VehicleListWidgets
	 */
	TrainList,

	/**
	 * Road vehicle list; %Window numbers:
	 *   - Packed value = #GroupListWidgets / #VehicleListWidgets
	 */
	RoadVehicleList,

	/**
	 * Ships list; %Window numbers:
	 *   - Packed value = #GroupListWidgets / #VehicleListWidgets
	 */
	ShipList,

	/**
	 * Aircraft list; %Window numbers:
	 *   - Packed value = #GroupListWidgets / #VehicleListWidgets
	 */
	AircraftList,


	/**
	 * Town view; %Window numbers:
	 *   - #TownID = #TownViewWidgets
	 */
	TownView,

	/**
	 * Vehicle view; %Window numbers:
	 *   - #VehicleID = #VehicleViewWidgets
	 */
	VehicleView,

	/**
	 * Station view; %Window numbers:
	 *   - #StationID = #StationViewWidgets
	 */
	StationView,

	/**
	 * Depot view; %Window numbers:
	 *   - #TileIndex = #DepotWidgets
	 */
	VehicleDepot,

	/**
	 * Waypoint view; %Window numbers:
	 *   - #WaypointID = #WaypointWidgets
	 */
	WaypointView,

	/**
	 * Industry view; %Window numbers:
	 *   - #IndustryID = #IndustryViewWidgets
	 */
	IndustryView,

	/**
	 * Company view; %Window numbers:
	 *   - #CompanyID = #CompanyWidgets
	 */
	Company,


	/**
	 * Build object; %Window numbers:
	 *   - 0 = #BuildObjectWidgets
	 */
	BuildObject,

	/**
	 * Build house; %Window numbers:
	 *   - 0 = #BuildHouseWidgets
	 */
	BuildHouse,

	/**
	 * Build vehicle; %Window numbers:
	 *   - #VehicleType = #BuildVehicleWidgets
	 *   - #TileIndex = #BuildVehicleWidgets
	 */
	BuildVehicle,

	/**
	 * Build bridge; %Window numbers:
	 *   - #TransportType = #BuildBridgeSelectionWidgets
	 */
	BuildBridge,

	/**
	 * Build station; %Window numbers:
	 *   - #TRANSPORT_AIR = #AirportPickerWidgets
	 *   - #TRANSPORT_WATER = #DockToolbarWidgets
	 *   - #TRANSPORT_RAIL = #BuildRailStationWidgets
	 */
	BuildStation,

	/**
	 * Build bus station; %Window numbers:
	 *   - #TRANSPORT_ROAD = #BuildRoadStationWidgets
	 */
	BuildBusStation,

	/**
	 * Build truck station; %Window numbers:
	 *   - #TRANSPORT_ROAD = #BuildRoadStationWidgets
	 */
	BuildTruckStation,

	/**
	 * Build depot; %Window numbers:
	 *   - #TRANSPORT_WATER = #BuildDockDepotWidgets
	 *   - #TRANSPORT_RAIL = #BuildRailDepotWidgets
	 *   - #TRANSPORT_ROAD = #BuildRoadDepotWidgets
	 */
	BuildDepot,

	/**
	 * Build waypoint; %Window numbers:
	 *   - #TRANSPORT_RAIL = #BuildRailWaypointWidgets
	 */
	BuildWaypoint,

	/**
	 * Found a town; %Window numbers:
	 *   - 0 = #TownFoundingWidgets
	 */
	FoundTown,

	/**
	 * Build industry; %Window numbers:
	 *   - 0 = #DynamicPlaceIndustriesWidgets
	 */
	BuildIndustry,


	/**
	 * Select game window; %Window numbers:
	 *   - 0 = #SelectGameIntroWidgets
	 */
	SelectGame,

	/**
	 * Landscape generation (in Scenario Editor); %Window numbers:
	 *   - 0 = #TerraformToolbarWidgets
	 *   - 0 = #EditorTerraformToolbarWidgets
	 */
	ScenarioGenerateLandscape,

	/**
	 * Generate landscape (newgame); %Window numbers:
	 *   - GLWM_SCENARIO = #CreateScenarioWidgets
	 *   - #GenerateLandscapeWindowMode = #GenerateLandscapeWidgets
	 */
	GenerateLandscape,

	/**
	 * Progress report of landscape generation; %Window numbers:
	 *   - 0 = #GenerationProgressWidgets
	 *   - 1 = #ScanProgressWidgets
	 */
	ModalProgress,


	/**
	 * Network window; %Window numbers:
	 *   - #NetworkWindowNumber::Game = #NetworkGameWidgets
	 *   - #NetworkWindowNumber::ContentList = #NetworkContentListWidgets
	 *   - #NetworkWindowNumber::StartServer = #NetworkStartServerWidgets
	 */
	Network,

	/**
	 * Client list; %Window numbers:
	 *   - 0 = #ClientListWidgets
	 */
	NetworkClientList,

	/**
	 * Network status window; %Window numbers:
	 *   - #NetworkStatusWindowNumber::Join = #NetworkJoinStatusWidgets
	 *   - #NetworkStatusWindowNumber::ContentDownload = #NetworkContentDownloadStatusWidgets
	 */
	NetworkStatus,

	/**
	 * Network ask relay window; %Window numbers:
	 *   - 0 - #NetworkAskRelayWidgets
	 */
	NetworkAskRelay,

	/**
	 * Network ask survey window; %Window numbers:
	 *  - 0 - #NetworkAskSurveyWidgets
	 */
	NetworkAskSurvey,

	/**
	 * Chatbox; %Window numbers:
	 *   - #NetworkChatDestinationType = #NetWorkChatWidgets
	 */
	NetworkChat,

	/**
	 * Industry cargoes chain; %Window numbers:
	 *   - 0 = #IndustryCargoesWidgets
	 */
	IndustryCargoes,

	/**
	 * Legend for graphs; %Window numbers:
	 *   - 0 = #GraphLegendWidgets
	 */
	GraphLegend,

	/**
	 * Finances of a company; %Window numbers:
	 *   - #CompanyID = #CompanyWidgets
	 */
	Finances,

	/**
	 * Income graph; %Window numbers:
	 *   - 0 = #CompanyValueWidgets
	 */
	IncomeGraph,

	/**
	 * Operating profit graph; %Window numbers:
	 *   - 0 = #CompanyValueWidgets
	 */
	OperatingProfitGraph,

	/**
	 * Delivered cargo graph; %Window numbers:
	 *   - 0 = #CompanyValueWidgets
	 */
	DeliveredCargoGraph,

	/**
	 * Performance history graph; %Window numbers:
	 *   - 0 = #PerformanceHistoryGraphWidgets
	 */
	PerformanceGraph,

	/**
	 * Company value graph; %Window numbers:
	 *   - 0 = #CompanyValueWidgets
	 */
	CompanyValueGraph,

	/**
	 * Company league window; %Window numbers:
	 *   - 0 = #CompanyLeagueWidgets
	 */
	CompanyLeague,

	/**
	 * Payment rates graph; %Window numbers:
	 *   - 0 = #CargoPaymentRatesWidgets
	 */
	CargoPaymentRatesGraph,

	/**
	 * Performance detail window; %Window numbers:
	 *   - 0 = #PerformanceRatingDetailsWidgets
	 */
	PerformanceDetail,

	/**
	 * Industry production history graph; %Window numbers:
	 *   - #IndustryID = #IndustryProductionGraphWidgets
	 */
	IndustryProductionGraph,

	/**
	 * Town cargo history graph; %Window numbers:
	 *   - #TownID = #GraphWidgets
	 */
	TownCargoGraph,

	/**
	 * Company infrastructure overview; %Window numbers:
	 *   - #CompanyID = #CompanyInfrastructureWidgets
	 */
	CompanyInfrastructure,


	/**
	 * Buyout company (merger); %Window numbers:
	 *   - #CompanyID = #BuyCompanyWidgets
	 */
	BuyCompany,

	/**
	 * Engine preview window; %Window numbers:
	 *   - 0 = #EnginePreviewWidgets
	 */
	EnginePreview,


	/**
	 * Music window; %Window numbers:
	 *   - 0 = #MusicWidgets
	 */
	Music,

	/**
	 * Music track selection; %Window numbers:
	 *   - 0 = MusicTrackSelectionWidgets
	 */
	MusicTrackSelection,

	/**
	 * Game options window; %Window numbers:
	 *   - #GameOptionsWindowNumber::AI = #AIConfigWidgets
	 *   - #GameOptionsWindowNumber::GS = #GSConfigWidgets
	 *   - #GameOptionsWindowNumber::About = #AboutWidgets
	 *   - #GameOptionsWindowNumber::NewGRFState = #NewGRFStateWidgets
	 *   - #GameOptionsWindowNumber::GameOptions = #GameOptionsWidgets
	 *   - #GameOptionsWindowNumber::GameSettings = #GameSettingsWidgets
	 */
	GameOptions,

	/**
	 * Custom currency; %Window numbers:
	 *   - 0 = #CustomCurrencyWidgets
	 */
	CustomCurrenty,

	/**
	 * Cheat window; %Window numbers:
	 *   - 0 = #CheatWidgets
	 */
	Cheat,

	/**
	 * Extra viewport; %Window numbers:
	 *   - Ascending value = #ExtraViewportWidgets
	 */
	ExtraViewport,


	/**
	 * Console; %Window numbers:
	 *   - 0 = #ConsoleWidgets
	 */
	Console,

	/**
	 * Bootstrap; %Window numbers:
	 *   - 0 = #BootstrapBackgroundWidgets
	 */
	Bootstrap,

	/**
	 * Highscore; %Window numbers:
	 *   - 0 = #HighscoreWidgets
	 */
	Highscore,

	/**
	 * Endscreen; %Window numbers:
	 *   - 0 = #HighscoreWidgets
	 */
	Endscreen,


	/**
	 * Script debug window; %Window numbers:
	 *   - Ascending value = #ScriptDebugWidgets
	 */
	ScriptDebug,

	/**
	 * NewGRF inspect (debug); %Window numbers:
	 *   - Packed value = #NewGRFInspectWidgets
	 */
	NewGRFInspect,

	/**
	 * Sprite aligner (debug); %Window numbers:
	 *   - 0 = #SpriteAlignerWidgets
	 */
	SpriteAligner,

	/**
	 * Linkgraph legend; %Window numbers:
	 *   - 0 = #LinkGraphWidgets
	 */
	LinkGraphLegend,

	/**
	 * Save preset; %Window numbers:
	 *   - 0 = #SavePresetWidgets
	 */
	SavePreset,

	/**
	 * Framerate display; %Window numbers:
	 *   - 0 = #FramerateDisplayWidgets
	 */
	FramerateDisplay,

	/**
	 * Frame time graph; %Window numbers:
	 *   - 0 = #FrametimeGraphWindowWidgets
	 */
	FrametimeGraph,

	/**
	 * Screenshot window; %Window numbers:
	 *   - 0 = #ScreenshotWidgets
	 */
	Screenshot,

	/**
	 * Help and manuals window; %Window numbers:
	 *   - 0 = #HelpWindowWidgets
	 */
	Help,

	Invalid = 0xFFFF, ///< Invalid window.
};

/** Data value for #Window::OnInvalidateData() of windows with class ::WindowClass::GameOptions. */
enum GameOptionsInvalidationData : uint8_t {
	GOID_DEFAULT = 0,
	GOID_NEWGRF_RESCANNED,       ///< NewGRFs were just rescanned.
	GOID_NEWGRF_CURRENT_LOADED,  ///< The current list of active NewGRF has been loaded.
	GOID_NEWGRF_LIST_EDITED,     ///< List of active NewGRFs is being edited.
	GOID_NEWGRF_CHANGES_MADE,    ///< Changes have been made to a given NewGRF either through the palette or its parameters.
};

struct Window;

/**
 * Number to differentiate different windows of the same class. This number generally
 * implicitly passes some information, e.g. the TileIndex or Company associated with
 * the window. To ease this use, the window number is lenient with what it accepts and
 * broad with what it returns.
 *
 * Anything that converts into a number and ConvertibleThroughBase types will be accepted.
 * When it's being used it returns int32_t or any other type when that's specifically
 * requested, e.g. `VehicleType type = window_number` or `GetEngineListHeight(window_number)`
 * in which the returned value will be a `VehicleType`.
 */
struct WindowNumber {
private:
	int32_t value = 0; ///< The identifier of the window.
public:
	/** Create a WindowNumber 0. */
	WindowNumber() = default;

	/**
	 * Create a WindowNumber with the given value.
	 * @param value The new window number.
	 */
	WindowNumber(int32_t value) : value(value) {}
	/** @copydoc WindowNumber(int32_t) */
	WindowNumber(ConvertibleThroughBase auto value) : value(value.base()) {}
	/** @copydoc WindowNumber(int32_t) */
	template <typename T> requires is_scoped_enum_v<T>
	WindowNumber(T value) : value(to_underlying(value)) {}

	/**
	 * Automatically convert to int32_t.
	 * @return The window number.
	 */
	operator int32_t() const { return value; }

	/**
	 * Automatically convert to any other type that might be requested.
	 * @return The window number in the requested type.
	 */
	template <typename T> requires (std::is_enum_v<T> || std::is_class_v<T>)
	operator T() const { return static_cast<T>(value); };

	/**
	 * Compare the right hand side against our window number.
	 * @param rhs The other value to compare to.
	 * @return \c true iff the underlying window number matches the underlying value.
	 */
	constexpr bool operator==(const std::integral auto &rhs) const { return this->value == static_cast<int32_t>(rhs); }
	/** @copydoc operator== */
	constexpr bool operator==(const ConvertibleThroughBase auto &rhs) const { return this->value == static_cast<int32_t>(rhs.base()); }
	/** @copydoc operator== */
	template <typename T> requires is_scoped_enum_v<T>
	constexpr bool operator==(const T &rhs) const { return this->value == static_cast<int32_t>(to_underlying(rhs)); }
};

/** State of handling an event. */
enum EventState : uint8_t {
	ES_HANDLED,     ///< The passed event is handled.
	ES_NOT_HANDLED, ///< The passed event is not handled.
};

#endif /* WINDOW_TYPE_H */
