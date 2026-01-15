/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file misc_gui.cpp GUIs for a number of misc windows. */

#include "stdafx.h"
#include "debug.h"
#include "landscape.h"
#include "error.h"
#include "gui.h"
#include "gfx_layout.h"
#include "tilehighlight_func.h"
#include "command_func.h"
#include "company_func.h"
#include "town.h"
#include "string_func.h"
#include "company_base.h"
#include "station_base.h"
#include "waypoint_base.h"
#include "texteff.hpp"
#include "strings_func.h"
#include "window_func.h"
#include "querystring_gui.h"
#include "core/geometry_func.hpp"
#include "newgrf_debug.h"
#include "zoom_func.h"
#include "viewport_func.h"
#include "landscape_cmd.h"
#include "station_cmd.h"
#include "waypoint_cmd.h"
#include "rev.h"
#include "timer/timer.h"
#include "timer/timer_window.h"
#include "pathfinder/water_regions.h"

#include "widgets/misc_widget.h"

#include "table/strings.h"

#include "safeguards.h"

/** Method to open the OSK. */
enum OskActivation : uint8_t {
	OSKA_DISABLED,           ///< The OSK shall not be activated at all.
	OSKA_DOUBLE_CLICK,       ///< Double click on the edit box opens OSK.
	OSKA_SINGLE_CLICK,       ///< Single click after focus click opens OSK.
	OSKA_IMMEDIATELY,        ///< Focusing click already opens OSK.
};


static constexpr std::initializer_list<NWidgetPart> _nested_land_info_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetStringTip(STR_LAND_AREA_INFORMATION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_LI_LOCATION), SetAspect(WidgetDimensions::ASPECT_LOCATION), SetSpriteTip(SPR_GOTO_LOCATION, STR_LAND_AREA_INFORMATION_LOCATION_TOOLTIP),
		NWidget(WWT_DEBUGBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_LI_BACKGROUND), EndContainer(),
};

static WindowDesc _land_info_desc(
	WDP_AUTO, {}, 0, 0,
	WC_LAND_INFO, WC_NONE,
	{},
	_nested_land_info_widgets
);

class LandInfoWindow : public Window {
	StringList landinfo_data{}; ///< Info lines to show.
	std::string cargo_acceptance{}; ///< Centered multi-line string for cargo acceptance.

public:
	TileIndex tile = INVALID_TILE;

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_LI_BACKGROUND) return;

		Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		for (size_t i = 0; i < this->landinfo_data.size(); i++) {
			DrawString(ir, this->landinfo_data[i], i == 0 ? TC_LIGHT_BLUE : TC_FROMSTRING, SA_HOR_CENTER);
			ir.top += GetCharacterHeight(FS_NORMAL) + (i == 0 ? WidgetDimensions::scaled.vsep_wide : WidgetDimensions::scaled.vsep_normal);
		}

		if (!this->cargo_acceptance.empty()) {
			DrawStringMultiLine(ir, GetString(STR_JUST_RAW_STRING, this->cargo_acceptance), TC_FROMSTRING, SA_CENTER);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_LI_BACKGROUND) return;

		size.height = WidgetDimensions::scaled.frametext.Vertical();
		for (size_t i = 0; i < this->landinfo_data.size(); i++) {
			uint width = GetStringBoundingBox(this->landinfo_data[i]).width + WidgetDimensions::scaled.frametext.Horizontal();
			size.width = std::max(size.width, width);

			size.height += GetCharacterHeight(FS_NORMAL) + (i == 0 ? WidgetDimensions::scaled.vsep_wide : WidgetDimensions::scaled.vsep_normal);
		}

		if (!this->cargo_acceptance.empty()) {
			uint width = GetStringBoundingBox(this->cargo_acceptance).width + WidgetDimensions::scaled.frametext.Horizontal();
			size.width = std::max(size.width, std::min(static_cast<uint>(ScaleGUITrad(300)), width));
			size.height += GetStringHeight(GetString(STR_JUST_RAW_STRING, this->cargo_acceptance), size.width - WidgetDimensions::scaled.frametext.Horizontal());
		}
	}

	LandInfoWindow(Tile tile) : Window(_land_info_desc), tile(tile)
	{
		this->InitNested();

#if defined(_DEBUG)
#	define LANDINFOD_LEVEL 0
#else
#	define LANDINFOD_LEVEL 1
#endif
		Debug(misc, LANDINFOD_LEVEL, "TILE: {0} (0x{0:x}) ({1},{2})", (TileIndex)tile, TileX(tile), TileY(tile));
		Debug(misc, LANDINFOD_LEVEL, "type   = 0x{:x}", tile.type());
		Debug(misc, LANDINFOD_LEVEL, "height = 0x{:x}", tile.height());
		Debug(misc, LANDINFOD_LEVEL, "m1     = 0x{:x}", tile.m1());
		Debug(misc, LANDINFOD_LEVEL, "m2     = 0x{:x}", tile.m2());
		Debug(misc, LANDINFOD_LEVEL, "m3     = 0x{:x}", tile.m3());
		Debug(misc, LANDINFOD_LEVEL, "m4     = 0x{:x}", tile.m4());
		Debug(misc, LANDINFOD_LEVEL, "m5     = 0x{:x}", tile.m5());
		Debug(misc, LANDINFOD_LEVEL, "m6     = 0x{:x}", tile.m6());
		Debug(misc, LANDINFOD_LEVEL, "m7     = 0x{:x}", tile.m7());
		Debug(misc, LANDINFOD_LEVEL, "m8     = 0x{:x}", tile.m8());

		PrintWaterRegionDebugInfo(tile);
#undef LANDINFOD_LEVEL
	}

	void OnInit() override
	{
		Town *t = ClosestTownFromTile(tile, _settings_game.economy.dist_local_authority);

		TileDesc td{};
		td.owner_type[0] = STR_LAND_AREA_INFORMATION_OWNER; // At least one owner is displayed, though it might be "N/A".

		CargoArray acceptance{};
		AddAcceptedCargo(tile, acceptance, nullptr);
		GetTileDesc(tile, td);

		this->landinfo_data.clear();

		/* Tiletype */
		this->landinfo_data.push_back(GetString(td.str, td.dparam));

		/* Up to four owners */
		for (uint i = 0; i < 4; i++) {
			if (td.owner_type[i] == STR_NULL) continue;

			if (td.owner[i] == OWNER_NONE || td.owner[i] == OWNER_WATER) {
				this->landinfo_data.push_back(GetString(td.owner_type[i], STR_LAND_AREA_INFORMATION_OWNER_N_A, std::monostate{}));
			} else {
				auto params = GetParamsForOwnedBy(td.owner[i], tile);
				this->landinfo_data.push_back(GetStringWithArgs(td.owner_type[i], params));
			}
		}

		/* Cost to clear/revenue when cleared */
		Company *c = Company::GetIfValid(_local_company);
		if (c != nullptr) {
			assert(_current_company == _local_company);
			CommandCost costclear = Command<Commands::LandscapeClear>::Do(DoCommandFlag::QueryCost, tile);
			if (costclear.Succeeded()) {
				Money cost = costclear.GetCost();
				StringID str;
				if (cost < 0) {
					cost = -cost; // Negate negative cost to a positive revenue
					str = STR_LAND_AREA_INFORMATION_REVENUE_WHEN_CLEARED;
				} else {
					str = STR_LAND_AREA_INFORMATION_COST_TO_CLEAR;
				}
				this->landinfo_data.push_back(GetString(str, cost));
			} else {
				this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_COST_TO_CLEAR_N_A));
			}
		} else {
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_COST_TO_CLEAR_N_A));
		}

		/* Location */
		this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_LANDINFO_COORDS, TileX(tile), TileY(tile), GetTileZ(tile)));

		/* Tile index */
		this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_LANDINFO_INDEX, tile, tile));

		/* Local authority */
		if (t == nullptr) {
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY, STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY_NONE, std::monostate{}));
		} else {
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY, STR_TOWN_NAME, t->index));
		}

		/* Build date */
		if (td.build_date != CalendarTime::INVALID_DATE) {
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_BUILD_DATE, td.build_date));
		}

		/* Station class */
		if (td.station_class != STR_NULL) {
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_STATION_CLASS, td.station_class));
		}

		/* Station type name */
		if (td.station_name != STR_NULL) {
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_STATION_TYPE, td.station_name));
		}

		/* Airport class */
		if (td.airport_class != STR_NULL) {
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_AIRPORT_CLASS, td.airport_class));
		}

		/* Airport name */
		if (td.airport_name != STR_NULL) {
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_AIRPORT_NAME, td.airport_name));
		}

		/* Airport tile name */
		if (td.airport_tile_name != STR_NULL) {
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_AIRPORTTILE_NAME, td.airport_tile_name));
		}

		/* Rail type name */
		if (td.railtype != STR_NULL) {
			this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_RAIL_TYPE, td.railtype));
		}

		/* Rail speed limit */
		if (td.rail_speed != 0) {
			this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_RAIL_SPEED_LIMIT, PackVelocity(td.rail_speed, VEH_TRAIN)));
		}

		/* Road type name */
		if (td.roadtype != STR_NULL) {
			this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_ROAD_TYPE, td.roadtype));
		}

		/* Road speed limit */
		if (td.road_speed != 0) {
			this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_ROAD_SPEED_LIMIT, PackVelocity(td.road_speed, VEH_ROAD)));
		}

		/* Tram type name */
		if (td.tramtype != STR_NULL) {
			this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_TRAM_TYPE, td.tramtype));
		}

		/* Tram speed limit */
		if (td.tram_speed != 0) {
			this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_TRAM_SPEED_LIMIT, PackVelocity(td.tram_speed, VEH_ROAD)));
		}

		/* Tile protection status */
		if (td.town_can_upgrade.has_value()) {
			this->landinfo_data.push_back(GetString(td.town_can_upgrade.value() ? STR_LAND_AREA_INFORMATION_TOWN_CAN_UPGRADE : STR_LAND_AREA_INFORMATION_TOWN_CANNOT_UPGRADE));
		}

		/* NewGRF name */
		if (td.grf.has_value()) {
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_NEWGRF_NAME, std::move(*td.grf)));
		}

		/* Cargo acceptance is displayed in a extra multiline */
		auto line = BuildCargoAcceptanceString(acceptance, STR_LAND_AREA_INFORMATION_CARGO_ACCEPTED);
		if (line.has_value()) {
			this->cargo_acceptance = std::move(*line);
		} else {
			this->cargo_acceptance.clear();
		}
	}

	bool IsNewGRFInspectable() const override
	{
		return ::IsNewGRFInspectable(GetGrfSpecFeature(this->tile), this->tile.base());
	}

	void ShowNewGRFInspectWindow() const override
	{
		::ShowNewGRFInspectWindow(GetGrfSpecFeature(this->tile), this->tile.base());
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_LI_LOCATION:
				if (_ctrl_pressed) {
					ShowExtraViewportWindow(this->tile);
				} else {
					ScrollMainWindowToTile(this->tile);
				}
				break;
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;

		/* ReInit, "debug" sprite might have changed */
		if (data == 1) this->ReInit();
	}
};

/**
 * Show land information window.
 * @param tile The tile to show information about.
 */
void ShowLandInfo(TileIndex tile)
{
	CloseWindowById(WC_LAND_INFO, 0);
	new LandInfoWindow(tile);
}

static constexpr std::initializer_list<NWidgetPart> _nested_about_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetStringTip(STR_ABOUT_OPENTTD, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY), SetPIP(4, 2, 4),
		NWidget(WWT_LABEL, INVALID_COLOUR), SetStringTip(STR_ABOUT_ORIGINAL_COPYRIGHT),
		NWidget(WWT_LABEL, INVALID_COLOUR), SetStringTip(STR_ABOUT_VERSION),
		NWidget(WWT_FRAME, COLOUR_GREY), SetPadding(0, 5, 1, 5),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_A_SCROLLING_TEXT),
		EndContainer(),
		NWidget(WWT_LABEL, INVALID_COLOUR, WID_A_WEBSITE),
		NWidget(WWT_LABEL, INVALID_COLOUR, WID_A_COPYRIGHT),
	EndContainer(),
};

static WindowDesc _about_desc(
	WDP_CENTER, {}, 0, 0,
	WC_GAME_OPTIONS, WC_NONE,
	{},
	_nested_about_widgets
);

static const std::initializer_list<const std::string_view> _credits = {
	"Original design by Chris Sawyer",
	"Original graphics by Simon Foster",
	"",
	"The OpenTTD team (in alphabetical order):",
	"  Matthijs Kooijman (blathijs) - Pathfinder-guru, Debian port (since 0.3)",
	"  Christoph Elsenhans (frosch) - General coding (since 0.6)",
	"  Lo\u00efc Guilloux (glx) - General / Windows Expert (since 0.4.5)",
	"  Koen Bussemaker (Kuhnovic) - General / Ship pathfinder (since 14)",
	"  Charles Pigott (LordAro) - General / Correctness police (since 1.9)",
	"  Michael Lutz (michi_cc) - Path based signals (since 0.7)",
	"  Niels Martin Hansen (nielsm) - Music system, general coding (since 1.9)",
	"  Owen Rudge (orudge) - Forum host, OS/2 port (since 0.1)",
	"  Peter Nelson (peter1138) - Spiritual descendant from NewGRF gods (since 0.4.5)",
	"  Remko Bijker (Rubidium) - Coder and way more (since 0.4.5)",
	"  Patric Stout (TrueBrain) - NoProgrammer (since 0.3), sys op",
	"  Tyler Trahan (2TallTyler) - General / Time Lord (since 13)",
	"  Richard Wheeler (zephyris) - Precision pixel production (since 15)",
	"",
	"Inactive Developers:",
	"  Grzegorz Duczy\u0144ski (adf88) - General coding (1.7 - 1.8)",
	"  Albert Hofkamp (Alberth) - GUI expert (0.7 - 1.9)",
	"  Jean-Fran\u00e7ois Claeys (Belugas) - GUI, NewGRF and more (0.4.5 - 1.0)",
	"  Bjarni Corfitzen (Bjarni) - MacOSX port, coder and vehicles (0.3 - 0.7)",
	"  Victor Fischer (Celestar) - Programming everywhere you need him to (0.3 - 0.6)",
	"  Ulf Hermann (fonsinchen) - Cargo Distribution (1.3 - 1.6)",
	"  Jaroslav Mazanec (KUDr) - YAPG (Yet Another Pathfinder God) ;) (0.4.5 - 0.6)",
	"  Jonathan Coome (Maedhros) - High priest of the NewGRF Temple (0.5 - 0.6)",
	"  Attila B\u00e1n (MiHaMiX) - Developer WebTranslator 1 and 2 (0.3 - 0.5)",
	"  Ingo von Borstel (planetmaker) - General coding, Support (1.1 - 1.9)",
	"  Zden\u011bk Sojka (SmatZ) - Bug finder and fixer (0.6 - 1.3)",
	"  Jos\u00e9 Soler (Terkhen) - General coding (1.0 - 1.4)",
	"  Christoph Mallon (Tron) - Programmer, code correctness police (0.3 - 0.5)",
	"  Thijs Marinussen (Yexo) - AI Framework, General (0.6 - 1.3)",
	"  Leif Linse (Zuu) - AI/Game Script (1.2 - 1.6)",
	"",
	"Retired Developers:",
	"  Tam\u00e1s Farag\u00f3 (Darkvater) - Ex-Lead coder (0.3 - 0.5)",
	"  Dominik Scherer (dominik81) - Lead programmer, GUI expert (0.3 - 0.3)",
	"  Emil Djupfeld (egladil) - MacOSX (0.4.5 - 0.6)",
	"  Simon Sasburg (HackyKid) - Many bugfixes (0.4 - 0.4.5)",
	"  Ludvig Strigeus (ludde) - Original author of OpenTTD, main coder (0.1 - 0.3)",
	"  Cian Duffy (MYOB) - BeOS port / manual writing (0.1 - 0.3)",
	"  Petr Baudi\u0161 (pasky) - Many patches, NewGRF support (0.3 - 0.3)",
	"  Benedikt Br\u00fcggemeier (skidd13) - Bug fixer and code reworker (0.6 - 0.7)",
	"  Serge Paquet (vurlix) - 2nd contributor after ludde (0.1 - 0.3)",
	"",
	"Special thanks go out to:",
	"  Josef Drexler - For his great work on TTDPatch",
	"  Marcin Grzegorczyk - Track foundations and for describing TTD internals",
	"  Stefan Mei\u00dfner (sign_de) - For his work on the console",
	"  Mike Ragsdale - OpenTTD installer",
	"  Christian Rosentreter (tokai) - MorphOS / AmigaOS port",
	"  Richard Kempton (richK) - additional airports, initial TGP implementation",
	"  Alberto Demichelis - Squirrel scripting language \u00a9 2003-2008",
	"  L. Peter Deutsch - MD5 implementation \u00a9 1999, 2000, 2002",
	"  Michael Blunck - Pre-signals and semaphores \u00a9 2003",
	"  George - Canal/Lock graphics \u00a9 2003-2004",
	"  Andrew Parkhouse (andythenorth) - River graphics",
	"  David Dallaston (Pikka) - Tram tracks",
	"  All Translators - Who made OpenTTD a truly international game",
	"  Bug Reporters - Without whom OpenTTD would still be full of bugs!",
	"",
	"",
	"And last but not least:",
	"  Chris Sawyer - For an amazing game!"
};

struct AboutWindow : public Window {
	int text_position = 0; ///< The top of the scrolling text
	int line_height = 0; ///< The height of a single line
	static const int num_visible_lines = 19; ///< The number of lines visible simultaneously

	AboutWindow() : Window(_about_desc)
	{
		this->InitNested(WN_GAME_OPTIONS_ABOUT);

		this->text_position = this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->pos_y + this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->current_y;
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_A_WEBSITE) return "Website: https://www.openttd.org";
		if (widget == WID_A_COPYRIGHT) return GetString(STR_ABOUT_COPYRIGHT_OPENTTD, _openttd_revision_year);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_A_SCROLLING_TEXT) return;

		this->line_height = GetCharacterHeight(FS_NORMAL);

		Dimension d;
		d.height = this->line_height * num_visible_lines;

		d.width = 0;
		for (const auto &str : _credits) {
			d.width = std::max(d.width, GetStringBoundingBox(str).width);
		}
		size = maxdim(size, d);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_A_SCROLLING_TEXT) return;

		int y = this->text_position;

		/* Show all scrolling _credits */
		for (const auto &str : _credits) {
			if (y >= r.top + 7 && y < r.bottom - this->line_height) {
				DrawString(r.left, r.right, y, str, TC_BLACK, SA_LEFT | SA_FORCE);
			}
			y += this->line_height;
		}
	}

	/**
	 * Scroll the text in the about window slow.
	 *
	 * The interval of 2100ms is chosen to maintain parity: 2100 / GetCharacterHeight(FS_NORMAL) = 150ms.
	 */
	const IntervalTimer<TimerWindow> scroll_interval = {std::chrono::milliseconds(2100) / GetCharacterHeight(FS_NORMAL), [this](uint count) {
		this->text_position -= count;
		/* If the last text has scrolled start a new from the start */
		if (this->text_position < (int)(this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->pos_y - std::size(_credits) * this->line_height)) {
			this->text_position = this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->pos_y + this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->current_y;
		}
		this->SetWidgetDirty(WID_A_SCROLLING_TEXT);
	}};
};

void ShowAboutWindow()
{
	CloseWindowByClass(WC_GAME_OPTIONS);
	new AboutWindow();
}

/**
 * Display estimated costs.
 * @param cost Estimated cost (or income if negative).
 * @param x    X position of the notification window.
 * @param y    Y position of the notification window.
 */
void ShowEstimatedCostOrIncome(Money cost, int x, int y)
{
	StringID msg = STR_MESSAGE_ESTIMATED_COST;

	if (cost < 0) {
		cost = -cost;
		msg = STR_MESSAGE_ESTIMATED_INCOME;
	}
	ShowErrorMessage(GetEncodedString(msg, cost), {}, WL_INFO, x, y);
}

/**
 * Display animated income or costs on the map. Does nothing if cost is zero.
 * @param x    World X position of the animation location.
 * @param y    World Y position of the animation location.
 * @param z    World Z position of the animation location.
 * @param cost Estimated cost (or income if negative).
 */
void ShowCostOrIncomeAnimation(int x, int y, int z, Money cost)
{
	if (cost == 0) {
		return;
	}
	Point pt = RemapCoords(x, y, z);
	StringID msg = STR_INCOME_FLOAT_COST;

	if (cost < 0) {
		cost = -cost;
		msg = STR_INCOME_FLOAT_INCOME;
	}
	AddTextEffect(GetEncodedString(msg, cost), pt.x, pt.y, Ticks::DAY_TICKS, TE_RISING);
}

/**
 * Display animated feeder income.
 * @param x        World X position of the animation location.
 * @param y        World Y position of the animation location.
 * @param z        World Z position of the animation location.
 * @param transfer Estimated feeder income.
 * @param income   Real income from goods being delivered to their final destination.
 */
void ShowFeederIncomeAnimation(int x, int y, int z, Money transfer, Money income)
{
	Point pt = RemapCoords(x, y, z);

	if (income == 0) {
		AddTextEffect(GetEncodedString(STR_FEEDER, transfer), pt.x, pt.y, Ticks::DAY_TICKS, TE_RISING);
	} else {
		StringID msg = STR_FEEDER_COST;
		if (income < 0) {
			income = -income;
			msg = STR_FEEDER_INCOME;
		}
		AddTextEffect(GetEncodedString(msg, transfer, income), pt.x, pt.y, Ticks::DAY_TICKS, TE_RISING);
	}
}

/**
 * Display vehicle loading indicators.
 * @param x       World X position of the animation location.
 * @param y       World Y position of the animation location.
 * @param z       World Z position of the animation location.
 * @param percent Estimated feeder income.
 * @param string  String which is drawn on the map.
 * @return        TextEffectID to be used for future updates of the loading indicators.
 */
TextEffectID ShowFillingPercent(int x, int y, int z, uint8_t percent, StringID string)
{
	Point pt = RemapCoords(x, y, z);

	assert(string != STR_NULL);

	return AddTextEffect(GetEncodedString(string, percent), pt.x, pt.y, 0, TE_STATIC);
}

/**
 * Update vehicle loading indicators.
 * @param te_id   TextEffectID to be updated.
 * @param string  String which is printed.
 */
void UpdateFillingPercent(TextEffectID te_id, uint8_t percent, StringID string)
{
	assert(string != STR_NULL);

	UpdateTextEffect(te_id, GetEncodedString(string, percent));
}

/**
 * Hide vehicle loading indicators.
 * @param *te_id TextEffectID which is supposed to be hidden.
 */
void HideFillingPercent(TextEffectID *te_id)
{
	if (*te_id == INVALID_TE_ID) return;

	RemoveTextEffect(*te_id);
	*te_id = INVALID_TE_ID;
}

static constexpr std::initializer_list<NWidgetPart> _nested_tooltips_widgets = {
	NWidget(WWT_EMPTY, INVALID_COLOUR, WID_TT_BACKGROUND),
};

static WindowDesc _tool_tips_desc(
	WDP_MANUAL, {}, 0, 0, // Coordinates and sizes are not used,
	WC_TOOLTIPS, WC_NONE,
	{WindowDefaultFlag::NoFocus, WindowDefaultFlag::NoClose},
	_nested_tooltips_widgets
);

/** Window for displaying a tooltip. */
struct TooltipsWindow : public Window
{
	EncodedString text{}; ///< String to display as tooltip.
	TooltipCloseCondition close_cond{}; ///< Condition for closing the window.

	TooltipsWindow(Window *parent, EncodedString &&text, TooltipCloseCondition close_tooltip) : Window(_tool_tips_desc), text(std::move(text))
	{
		this->parent = parent;
		this->close_cond = close_tooltip;

		this->InitNested();

		this->flags.Reset(WindowFlag::WhiteBorder);
	}

	Point OnInitialPosition([[maybe_unused]] int16_t sm_width, [[maybe_unused]] int16_t sm_height, [[maybe_unused]] int window_number) override
	{
		/* Find the free screen space between the main toolbar at the top, and the statusbar at the bottom.
		 * Add a fixed distance 2 so the tooltip floats free from both bars.
		 */
		int scr_top = GetMainViewTop() + 2;
		int scr_bot = GetMainViewBottom() - 2;

		Point pt;

		/* Correctly position the tooltip position, watch out for window and cursor size
		 * Clamp value to below main toolbar and above statusbar. If tooltip would
		 * go below window, flip it so it is shown above the cursor */
		pt.y = SoftClamp(_cursor.pos.y + _cursor.total_size.y + _cursor.total_offs.y + 5, scr_top, scr_bot);
		if (pt.y + sm_height > scr_bot) pt.y = std::min(_cursor.pos.y + _cursor.total_offs.y - 5, scr_bot) - sm_height;
		pt.x = sm_width >= _screen.width ? 0 : SoftClamp(_cursor.pos.x - (sm_width >> 1), 0, _screen.width - sm_width);

		return pt;
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_TT_BACKGROUND) return;

		auto str = this->text.GetDecodedString();
		size.width  = std::min<uint>(GetStringBoundingBox(str).width, ScaleGUITrad(194));
		size.height = GetStringHeight(str, size.width);

		/* Increase slightly to have some space around the box. */
		size.width  += WidgetDimensions::scaled.framerect.Horizontal()  + WidgetDimensions::scaled.fullbevel.Horizontal();
		size.height += WidgetDimensions::scaled.framerect.Vertical()    + WidgetDimensions::scaled.fullbevel.Vertical();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_TT_BACKGROUND) return;
		GfxFillRect(r, PC_BLACK);
		GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), PC_LIGHT_YELLOW);

		DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.framerect).Shrink(WidgetDimensions::scaled.fullbevel), this->text.GetDecodedString(), TC_BLACK, SA_CENTER);
	}

	void OnMouseLoop() override
	{
		/* Always close tooltips when the cursor is not in our window. */
		if (!_cursor.in_window) {
			this->Close();
			return;
		}

		/* We can show tooltips while dragging tools. These are shown as long as
		 * we are dragging the tool. Normal tooltips work with hover or rmb. */
		switch (this->close_cond) {
			case TCC_RIGHT_CLICK: if (!_right_button_down) this->Close(); break;
			case TCC_HOVER: if (!_mouse_hovering) this->Close(); break;
			case TCC_NONE: break;

			case TCC_EXIT_VIEWPORT: {
				Window *w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);
				if (w == nullptr || IsPtInWindowViewport(w, _cursor.pos.x, _cursor.pos.y) == nullptr) this->Close();
				break;
			}
		}
	}
};

/**
 * Shows a tooltip
 * @param parent The window this tooltip is related to.
 * @param text String to be displayed. May include encoded parameters.
 * @param close_tooltip the condition under which the tooltip closes
 */
void GuiShowTooltips(Window *parent, EncodedString &&text, TooltipCloseCondition close_tooltip)
{
	CloseWindowById(WC_TOOLTIPS, 0);

	if (text.empty() || !_cursor.in_window) return;

	new TooltipsWindow(parent, std::move(text), close_tooltip);
}

void QueryString::HandleEditBox(Window *w, WidgetID wid)
{
	if (w->IsWidgetGloballyFocused(wid) && this->text.HandleCaret()) {
		w->SetWidgetDirty(wid);

		/* For the OSK also invalidate the parent window */
		if (w->window_class == WC_OSK) w->InvalidateData();
	}
}

static int GetCaretWidth()
{
	return GetCharacterWidth(FS_NORMAL, '_');
}

/**
 * Reposition edit text box rect based on textbuf length can caret position.
 * @param r Initial rect of edit text box.
 * @param tb The Textbuf being processed.
 * @return Updated rect.
 */
static Rect ScrollEditBoxTextRect(Rect r, const Textbuf &tb)
{
	const int linewidth = tb.pixels + GetCaretWidth();
	const int boxwidth = r.Width();
	if (linewidth <= boxwidth) return r;

	/* Extend to cover whole string. This is left-aligned, adjusted by caret position. */
	r = r.WithWidth(linewidth, false);

	/* Slide so that the caret is at the centre unless limited by bounds of the line, i.e. near either end. */
	return r.Translate(-std::clamp(tb.caretxoffs - (boxwidth / 2), 0, linewidth - boxwidth), 0);
}

void QueryString::DrawEditBox(const Window *w, WidgetID wid) const
{
	const NWidgetLeaf *wi = w->GetWidget<NWidgetLeaf>(wid);

	assert((wi->type & WWT_MASK) == WWT_EDITBOX);

	bool rtl = _current_text_dir == TD_RTL;
	Dimension sprite_size = GetScaledSpriteSize(rtl ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT);
	int clearbtn_width = sprite_size.width + WidgetDimensions::scaled.imgbtn.Horizontal();

	Rect r = wi->GetCurrentRect();
	Rect cr = r.WithWidth(clearbtn_width, !rtl);
	Rect fr = r.Indent(clearbtn_width, !rtl);

	DrawFrameRect(cr, wi->colour, wi->IsLowered() ? FrameFlag::Lowered : FrameFlags{});
	DrawSpriteIgnorePadding(rtl ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT, PAL_NONE, cr, SA_CENTER);
	if (this->text.GetText().empty()) GfxFillRect(cr.Shrink(WidgetDimensions::scaled.bevel), GetColourGradient(wi->colour, SHADE_DARKER), FILLRECT_CHECKER);

	DrawFrameRect(fr, wi->colour, {FrameFlag::Lowered, FrameFlag::Darkened});
	GfxFillRect(fr.Shrink(WidgetDimensions::scaled.bevel), PC_BLACK);

	fr = fr.Shrink(WidgetDimensions::scaled.framerect);
	/* Limit the drawing of the string inside the widget boundaries */
	DrawPixelInfo dpi;
	if (!FillDrawPixelInfo(&dpi, fr)) return;
	/* Keep coordinates relative to the window. */
	dpi.left += fr.left;
	dpi.top += fr.top;

	AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);

	/* We will take the current widget length as maximum width, with a small
	 * space reserved at the end for the caret to show */
	const Textbuf *tb = &this->text;
	fr = ScrollEditBoxTextRect(fr, *tb);

	/* If we have a marked area, draw a background highlight. */
	if (tb->marklength != 0) GfxFillRect(fr.left + tb->markxoffs, fr.top, fr.left + tb->markxoffs + tb->marklength - 1, fr.bottom, PC_GREY);

	DrawString(fr.left, fr.right, CentreBounds(fr.top, fr.bottom, GetCharacterHeight(FS_NORMAL)), tb->GetText(), TC_YELLOW);
	bool focussed = w->IsWidgetGloballyFocused(wid) || IsOSKOpenedFor(w, wid);
	if (focussed && tb->caret) {
		int caret_width = GetCaretWidth();
		if (rtl) {
			DrawString(fr.right - tb->pixels + tb->caretxoffs - caret_width, fr.right - tb->pixels + tb->caretxoffs, CentreBounds(fr.top, fr.bottom, GetCharacterHeight(FS_NORMAL)), "_", TC_WHITE);
		} else {
			DrawString(fr.left + tb->caretxoffs, fr.left + tb->caretxoffs + caret_width, CentreBounds(fr.top, fr.bottom, GetCharacterHeight(FS_NORMAL)), "_", TC_WHITE);
		}
	}
}

/**
 * Get the current caret position.
 * @param w Window the edit box is in.
 * @param wid Widget index.
 * @return Top-left location of the caret, relative to the window.
 */
Point QueryString::GetCaretPosition(const Window *w, WidgetID wid) const
{
	const NWidgetLeaf *wi = w->GetWidget<NWidgetLeaf>(wid);

	assert((wi->type & WWT_MASK) == WWT_EDITBOX);

	bool rtl = _current_text_dir == TD_RTL;
	Dimension sprite_size = GetScaledSpriteSize(rtl ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT);
	int clearbtn_width = sprite_size.width + WidgetDimensions::scaled.imgbtn.Horizontal();

	Rect r = wi->GetCurrentRect().Indent(clearbtn_width, !rtl).Shrink(WidgetDimensions::scaled.framerect);

	/* Clamp caret position to be inside out current width. */
	const Textbuf *tb = &this->text;
	r = ScrollEditBoxTextRect(r, *tb);

	Point pt = {r.left + tb->caretxoffs, r.top};
	return pt;
}

/**
 * Get the bounding rectangle for a range of the query string.
 * @param w Window the edit box is in.
 * @param wid Widget index.
 * @param from Start of the string range.
 * @param to End of the string range.
 * @return Rectangle encompassing the string range, relative to the window.
 */
Rect QueryString::GetBoundingRect(const Window *w, WidgetID wid, size_t from, size_t to) const
{
	const NWidgetLeaf *wi = w->GetWidget<NWidgetLeaf>(wid);

	assert((wi->type & WWT_MASK) == WWT_EDITBOX);

	bool rtl = _current_text_dir == TD_RTL;
	Dimension sprite_size = GetScaledSpriteSize(rtl ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT);
	int clearbtn_width = sprite_size.width + WidgetDimensions::scaled.imgbtn.Horizontal();

	Rect r = wi->GetCurrentRect().Indent(clearbtn_width, !rtl).Shrink(WidgetDimensions::scaled.framerect);

	/* Clamp caret position to be inside our current width. */
	const Textbuf *tb = &this->text;
	r = ScrollEditBoxTextRect(r, *tb);

	/* Get location of first and last character. */
	const auto p1 = GetCharPosInString(tb->GetText(), from, FS_NORMAL);
	const auto p2 = from != to ? GetCharPosInString(tb->GetText(), to, FS_NORMAL) : p1;

	return r.WithX(Clamp(r.left + p1.left, r.left, r.right), Clamp(r.left + p2.right, r.left, r.right));
}

/**
 * Get the character that is rendered at a position.
 * @param w Window the edit box is in.
 * @param wid Widget index.
 * @param pt Position to test.
 * @return Index of the character position or -1 if no character is at the position.
 */
ptrdiff_t QueryString::GetCharAtPosition(const Window *w, WidgetID wid, const Point &pt) const
{
	const NWidgetLeaf *wi = w->GetWidget<NWidgetLeaf>(wid);

	assert((wi->type & WWT_MASK) == WWT_EDITBOX);

	bool rtl = _current_text_dir == TD_RTL;
	Dimension sprite_size = GetScaledSpriteSize(rtl ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT);
	int clearbtn_width = sprite_size.width + WidgetDimensions::scaled.imgbtn.Horizontal();

	Rect r = wi->GetCurrentRect().Indent(clearbtn_width, !rtl).Shrink(WidgetDimensions::scaled.framerect);

	if (!IsInsideMM(pt.y, r.top, r.bottom)) return -1;

	/* Clamp caret position to be inside our current width. */
	const Textbuf *tb = &this->text;
	r = ScrollEditBoxTextRect(r, *tb);

	return ::GetCharAtPosition(tb->GetText(), pt.x - r.left);
}

void QueryString::ClickEditBox(Window *w, Point pt, WidgetID wid, int click_count, bool focus_changed)
{
	const NWidgetLeaf *wi = w->GetWidget<NWidgetLeaf>(wid);

	assert((wi->type & WWT_MASK) == WWT_EDITBOX);

	bool rtl = _current_text_dir == TD_RTL;
	Dimension sprite_size = GetScaledSpriteSize(rtl ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT);
	int clearbtn_width = sprite_size.width + WidgetDimensions::scaled.imgbtn.Horizontal();

	Rect cr = wi->GetCurrentRect().WithWidth(clearbtn_width, !rtl);

	if (IsInsideMM(pt.x, cr.left, cr.right)) {
		if (!this->text.GetText().empty()) {
			this->text.DeleteAll();
			w->HandleButtonClick(wid);
			w->OnEditboxChanged(wid);
		}
		return;
	}

	if (w->window_class != WC_OSK && _settings_client.gui.osk_activation != OSKA_DISABLED &&
		(!focus_changed || _settings_client.gui.osk_activation == OSKA_IMMEDIATELY) &&
		(click_count == 2 || _settings_client.gui.osk_activation != OSKA_DOUBLE_CLICK)) {
		/* Open the OSK window */
		ShowOnScreenKeyboard(w, wid);
	}
}

/** Class for the string query window. */
struct QueryStringWindow : public Window
{
	QueryString editbox; ///< Editbox.
	QueryStringFlags flags{}; ///< Flags controlling behaviour of the window.

	WidgetID last_user_action = INVALID_WIDGET; ///< Last started user action.

	QueryStringWindow(std::string_view str, StringID caption, uint max_bytes, uint max_chars, WindowDesc &desc, Window *parent, CharSetFilter afilter, QueryStringFlags flags) :
			Window(desc), editbox(max_bytes, max_chars)
	{
		this->editbox.text.Assign(str);

		if (!flags.Test(QueryStringFlag::AcceptUnchanged)) this->editbox.orig = this->editbox.text.GetText();

		this->querystrings[WID_QS_TEXT] = &this->editbox;
		this->editbox.caption = caption;
		this->editbox.cancel_button = WID_QS_CANCEL;
		this->editbox.ok_button = WID_QS_OK;
		this->editbox.text.afilter = afilter;
		this->flags = flags;

		this->CreateNestedTree();
		this->GetWidget<NWidgetStacked>(WID_QS_DEFAULT_SEL)->SetDisplayedPlane((this->flags.Test(QueryStringFlag::EnableDefault)) ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_QS_MOVE_SEL)->SetDisplayedPlane((this->flags.Test(QueryStringFlag::EnableMove)) ? 0 : SZSP_NONE);
		this->FinishInitNested(WN_QUERY_STRING);

		this->parent = parent;

		this->SetFocusedWidget(WID_QS_TEXT);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_QS_CAPTION) return GetString(this->editbox.caption);

		return this->Window::GetWidgetString(widget, stringid);
	}

	void OnOk()
	{
		if (!this->editbox.orig.has_value() || this->editbox.text.GetText() != this->editbox.orig) {
			assert(this->parent != nullptr);

			this->parent->OnQueryTextFinished(std::string{this->editbox.text.GetText()});
			this->editbox.handled = true;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_QS_DEFAULT:
				this->editbox.text.DeleteAll();
				[[fallthrough]];

			case WID_QS_OK:
				this->OnOk();
				[[fallthrough]];

			case WID_QS_CANCEL:
				this->Close();
				break;

			case WID_QS_MOVE:
				this->last_user_action = widget;

				if (Station::IsExpected(Station::Get(this->parent->window_number))) {
					/* this is a station */
					SetViewportStationRect(Station::Get(this->parent->window_number), !this->IsWidgetLowered(WID_QS_MOVE));
				} else {
					/* this is a waypoint */
					SetViewportWaypointRect(Waypoint::Get(this->parent->window_number), !this->IsWidgetLowered(WID_QS_MOVE));
				}

				HandlePlacePushButton(this, WID_QS_MOVE, SPR_CURSOR_SIGN, HT_RECT);
				break;
		}
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		switch (this->last_user_action) {
			case WID_QS_MOVE: // Move name button
				if (Station::IsExpected(Station::Get(this->parent->window_number))) {
					/* this is a station */
					Command<Commands::MoveStationName>::Post(STR_ERROR_CAN_T_MOVE_STATION_NAME, CcMoveStationName, this->parent->window_number, tile);
				} else {
					/* this is a waypoint */
					Command<Commands::MoveWaypointNAme>::Post(STR_ERROR_CAN_T_MOVE_WAYPOINT_NAME, CcMoveWaypointName, this->parent->window_number, tile);
				}
				break;

			default: NOT_REACHED();
		}
	}

	void OnPlaceObjectAbort() override
	{
		if (Station::IsExpected(Station::Get(this->parent->window_number))) {
			/* this is a station */
			SetViewportStationRect(Station::Get(this->parent->window_number), false);
		} else {
			/* this is a waypoint */
			SetViewportWaypointRect(Waypoint::Get(this->parent->window_number), false);
		}

		this->RaiseButtons();
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		if (this->parent != nullptr) {
			if (this->parent->window_class == WC_STATION_VIEW) SetViewportStationRect(Station::Get(this->parent->window_number), false);
			if (this->parent->window_class == WC_WAYPOINT_VIEW) SetViewportWaypointRect(Waypoint::Get(this->parent->window_number), false);

			if (!this->editbox.handled) {
				Window *parent = this->parent;
				this->parent = nullptr; // so parent doesn't try to close us again
				parent->OnQueryTextFinished(std::nullopt);
			}
		}

		this->Window::Close();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_query_string_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_QS_CAPTION), SetTextStyle(TC_WHITE),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(WWT_EDITBOX, COLOUR_GREY, WID_QS_TEXT), SetMinimalSize(256, 0), SetFill(1, 0), SetPadding(2, 2, 2, 2),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_QS_DEFAULT_SEL),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_QS_DEFAULT), SetMinimalSize(65, 12), SetFill(1, 1), SetStringTip(STR_BUTTON_DEFAULT),
		EndContainer(),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_QS_CANCEL), SetMinimalSize(65, 12), SetFill(1, 1), SetStringTip(STR_BUTTON_CANCEL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_QS_OK), SetMinimalSize(65, 12), SetFill(1, 1), SetStringTip(STR_BUTTON_OK),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_QS_MOVE_SEL),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_QS_MOVE), SetMinimalSize(65, 12), SetFill(1, 1), SetStringTip(STR_BUTTON_MOVE),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _query_string_desc(
	WDP_CENTER, {}, 0, 0,
	WC_QUERY_STRING, WC_NONE,
	{},
	_nested_query_string_widgets
);

/**
 * Show a query popup window with a textbox in it.
 * @param str StringID for the text shown in the textbox
 * @param caption StringID of text shown in caption of querywindow
 * @param maxsize maximum size in bytes or characters (including terminating '\0') depending on flags
 * @param parent pointer to a Window that will handle the events (ok/cancel) of this window.
 * @param afilter filters out unwanted character input
 * @param flags various flags, @see QueryStringFlags
 */
void ShowQueryString(std::string_view str, StringID caption, uint maxsize, Window *parent, CharSetFilter afilter, QueryStringFlags flags)
{
	assert(parent != nullptr);

	CloseWindowByClass(WC_QUERY_STRING);
	new QueryStringWindow(str, caption, (flags.Test(QueryStringFlag::LengthIsInChars) ? MAX_CHAR_LENGTH : 1) * maxsize, maxsize, _query_string_desc, parent, afilter, flags);
}

/**
 * Updates default text value of query strign window.
 * @param str String for the default text shown in the textbox.
 */
void UpdateQueryStringDefault(std::string_view str)
{
	QueryStringWindow *w = dynamic_cast<QueryStringWindow *>(FindWindowByClass(WC_QUERY_STRING));
	if (w != nullptr) w->editbox.orig = str;
}

/**
 * Window used for asking the user a YES/NO question.
 */
struct QueryWindow : public Window {
	QueryCallbackProc *proc = nullptr; ///< callback function executed on closing of popup. Window* points to parent, bool is true if 'yes' clicked, false otherwise
	EncodedString caption{}; ///< caption for query window.
	EncodedString message{}; ///< message for query window.

	QueryWindow(WindowDesc &desc, EncodedString &&caption, EncodedString &&message, Window *parent, QueryCallbackProc *callback)
		: Window(desc), proc(callback), caption(std::move(caption)), message(std::move(message))
	{
		this->parent = parent;

		this->CreateNestedTree();
		this->FinishInitNested(WN_CONFIRM_POPUP_QUERY);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		if (this->proc != nullptr) this->proc(this->parent, false);
		this->Window::Close();
	}

	void FindWindowPlacementAndResize(int, int, bool) override
	{
		/* Position query window over the calling window, ensuring it's within screen bounds. */
		this->left = SoftClamp(parent->left + (parent->width / 2) - (this->width / 2), 0, _screen.width - this->width);
		this->top = SoftClamp(parent->top + (parent->height / 2) - (this->height / 2), 0, _screen.height - this->height);
		this->SetDirty();
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_Q_CAPTION:
				return this->caption.GetDecodedString();

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_Q_TEXT) return;

		size = GetStringMultiLineBoundingBox(this->message.GetDecodedString(), size);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_Q_TEXT) return;

		DrawStringMultiLine(r, this->message.GetDecodedString(), TC_FROMSTRING, SA_CENTER);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_Q_YES: {
				/* in the Generate New World window, clicking 'Yes' causes
				 * CloseNonVitalWindows() to be called - we shouldn't be in a window then */
				QueryCallbackProc *proc = this->proc;
				Window *parent = this->parent;
				/* Prevent the destructor calling the callback function */
				this->proc = nullptr;
				this->Close();
				if (proc != nullptr) {
					proc(parent, true);
					proc = nullptr;
				}
				break;
			}
			case WID_Q_NO:
				this->Close();
				break;
		}
	}

	EventState OnKeyPress([[maybe_unused]] char32_t key, uint16_t keycode) override
	{
		/* ESC closes the window, Enter confirms the action */
		switch (keycode) {
			case WKC_RETURN:
			case WKC_NUM_ENTER:
				if (this->proc != nullptr) {
					this->proc(this->parent, true);
					this->proc = nullptr;
				}
				[[fallthrough]];

			case WKC_ESC:
				this->Close();
				return ES_HANDLED;
		}
		return ES_NOT_HANDLED;
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_query_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_RED),
		NWidget(WWT_CAPTION, COLOUR_RED, WID_Q_CAPTION),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_RED),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_TEXT, INVALID_COLOUR, WID_Q_TEXT), SetMinimalSize(200, 12),
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPIP(WidgetDimensions::unscaled.hsep_indent, WidgetDimensions::unscaled.hsep_indent, WidgetDimensions::unscaled.hsep_indent),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_Q_NO), SetMinimalSize(71, 12), SetFill(1, 1), SetStringTip(STR_QUIT_NO),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_Q_YES), SetMinimalSize(71, 12), SetFill(1, 1), SetStringTip(STR_QUIT_YES),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _query_desc(
	WDP_CENTER, {}, 0, 0,
	WC_CONFIRM_POPUP_QUERY, WC_NONE,
	WindowDefaultFlag::Modal,
	_nested_query_widgets
);

/**
 * Show a confirmation window with standard 'yes' and 'no' buttons
 * The window is aligned to the centre of its parent.
 * @param caption string shown as window caption
 * @param message string that will be shown for the window
 * @param parent pointer to parent window, if this pointer is nullptr the parent becomes
 * the main window WC_MAIN_WINDOW
 * @param callback callback function pointer to set in the window descriptor
 * @param focus whether the window should be focussed (by default false)
 */
void ShowQuery(EncodedString &&caption, EncodedString &&message, Window *parent, QueryCallbackProc *callback, bool focus)
{
	if (parent == nullptr) parent = GetMainWindow();

	for (Window *w : Window::Iterate()) {
		if (w->window_class != WC_CONFIRM_POPUP_QUERY) continue;

		QueryWindow *qw = dynamic_cast<QueryWindow *>(w);
		assert(qw != nullptr);
		if (qw->parent != parent || qw->proc != callback) continue;

		qw->Close();
		break;
	}

	QueryWindow *q = new QueryWindow(_query_desc, std::move(caption), std::move(message), parent, callback);
	if (focus) SetFocusedWindow(q);
}
