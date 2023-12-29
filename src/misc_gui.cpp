/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file misc_gui.cpp GUIs for a number of misc windows. */

#include "stdafx.h"
#include "debug.h"
#include "landscape.h"
#include "error.h"
#include "gui.h"
#include "command_func.h"
#include "company_func.h"
#include "town.h"
#include "string_func.h"
#include "company_base.h"
#include "texteff.hpp"
#include "strings_func.h"
#include "window_func.h"
#include "querystring_gui.h"
#include "core/geometry_func.hpp"
#include "newgrf_debug.h"
#include "zoom_func.h"
#include "viewport_func.h"
#include "landscape_cmd.h"
#include "rev.h"
#include "timer/timer.h"
#include "timer/timer_window.h"

#include "widgets/misc_widget.h"

#include "table/strings.h"

#include <sstream>
#include <iomanip>

#include "safeguards.h"

/** Method to open the OSK. */
enum OskActivation {
	OSKA_DISABLED,           ///< The OSK shall not be activated at all.
	OSKA_DOUBLE_CLICK,       ///< Double click on the edit box opens OSK.
	OSKA_SINGLE_CLICK,       ///< Single click after focus click opens OSK.
	OSKA_IMMEDIATELY,        ///< Focusing click already opens OSK.
};


static const NWidgetPart _nested_land_info_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_LAND_AREA_INFORMATION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_LI_LOCATION), SetMinimalSize(12, 14), SetDataTip(SPR_GOTO_LOCATION, STR_LAND_AREA_INFORMATION_LOCATION_TOOLTIP),
		NWidget(WWT_DEBUGBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_LI_BACKGROUND), EndContainer(),
};

static WindowDesc _land_info_desc(__FILE__, __LINE__,
	WDP_AUTO, nullptr, 0, 0,
	WC_LAND_INFO, WC_NONE,
	0,
	std::begin(_nested_land_info_widgets), std::end(_nested_land_info_widgets)
);

class LandInfoWindow : public Window {
	StringList  landinfo_data;    ///< Info lines to show.
	std::string cargo_acceptance; ///< Centered multi-line string for cargo acceptance.

public:
	TileIndex tile;

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_LI_BACKGROUND) return;

		Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		for (size_t i = 0; i < this->landinfo_data.size(); i++) {
			DrawString(ir, this->landinfo_data[i], i == 0 ? TC_LIGHT_BLUE : TC_FROMSTRING, SA_HOR_CENTER);
			ir.top += GetCharacterHeight(FS_NORMAL) + (i == 0 ? WidgetDimensions::scaled.vsep_wide : WidgetDimensions::scaled.vsep_normal);
		}

		if (!this->cargo_acceptance.empty()) {
			SetDParamStr(0, this->cargo_acceptance);
			DrawStringMultiLine(ir, STR_JUST_RAW_STRING, TC_FROMSTRING, SA_CENTER);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != WID_LI_BACKGROUND) return;

		size->height = WidgetDimensions::scaled.frametext.Vertical();
		for (size_t i = 0; i < this->landinfo_data.size(); i++) {
			uint width = GetStringBoundingBox(this->landinfo_data[i]).width + WidgetDimensions::scaled.frametext.Horizontal();
			size->width = std::max(size->width, width);

			size->height += GetCharacterHeight(FS_NORMAL) + (i == 0 ? WidgetDimensions::scaled.vsep_wide : WidgetDimensions::scaled.vsep_normal);
		}

		if (!this->cargo_acceptance.empty()) {
			uint width = GetStringBoundingBox(this->cargo_acceptance).width + WidgetDimensions::scaled.frametext.Horizontal();
			size->width = std::max(size->width, std::min(static_cast<uint>(ScaleGUITrad(300)), width));
			SetDParamStr(0, cargo_acceptance);
			size->height += GetStringHeight(STR_JUST_RAW_STRING, size->width - WidgetDimensions::scaled.frametext.Horizontal());
		}
	}

	LandInfoWindow(Tile tile) : Window(&_land_info_desc), tile(tile)
	{
		this->InitNested();

#if defined(_DEBUG)
#	define LANDINFOD_LEVEL 0
#else
#	define LANDINFOD_LEVEL 1
#endif
		Debug(misc, LANDINFOD_LEVEL, "TILE: 0x{:x} ({},{})", (TileIndex)tile, TileX(tile), TileY(tile));
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
#undef LANDINFOD_LEVEL
	}

	void OnInit() override
	{
		Town *t = ClosestTownFromTile(tile, _settings_game.economy.dist_local_authority);

		/* Because build_date is not set yet in every TileDesc, we make sure it is empty */
		TileDesc td;

		td.build_date = CalendarTime::INVALID_DATE;

		/* Most tiles have only one owner, but
		 *  - drivethrough roadstops can be build on town owned roads (up to 2 owners) and
		 *  - roads can have up to four owners (railroad, road, tram, 3rd-roadtype "highway").
		 */
		td.owner_type[0] = STR_LAND_AREA_INFORMATION_OWNER; // At least one owner is displayed, though it might be "N/A".
		td.owner_type[1] = STR_NULL;       // STR_NULL results in skipping the owner
		td.owner_type[2] = STR_NULL;
		td.owner_type[3] = STR_NULL;
		td.owner[0] = OWNER_NONE;
		td.owner[1] = OWNER_NONE;
		td.owner[2] = OWNER_NONE;
		td.owner[3] = OWNER_NONE;

		td.station_class = STR_NULL;
		td.station_name = STR_NULL;
		td.airport_class = STR_NULL;
		td.airport_name = STR_NULL;
		td.airport_tile_name = STR_NULL;
		td.railtype = STR_NULL;
		td.rail_speed = 0;
		td.roadtype = STR_NULL;
		td.road_speed = 0;
		td.tramtype = STR_NULL;
		td.tram_speed = 0;

		td.grf = nullptr;

		CargoArray acceptance{};
		AddAcceptedCargo(tile, acceptance, nullptr);
		GetTileDesc(tile, &td);

		this->landinfo_data.clear();

		/* Tiletype */
		SetDParam(0, td.dparam);
		this->landinfo_data.push_back(GetString(td.str));

		/* Up to four owners */
		for (uint i = 0; i < 4; i++) {
			if (td.owner_type[i] == STR_NULL) continue;

			SetDParam(0, STR_LAND_AREA_INFORMATION_OWNER_N_A);
			if (td.owner[i] != OWNER_NONE && td.owner[i] != OWNER_WATER) SetDParamsForOwnedBy(td.owner[i], tile);
			this->landinfo_data.push_back(GetString(td.owner_type[i]));
		}

		/* Cost to clear/revenue when cleared */
		StringID str = STR_LAND_AREA_INFORMATION_COST_TO_CLEAR_N_A;
		Company *c = Company::GetIfValid(_local_company);
		if (c != nullptr) {
			assert(_current_company == _local_company);
			CommandCost costclear = Command<CMD_LANDSCAPE_CLEAR>::Do(DC_QUERY_COST, tile);
			if (costclear.Succeeded()) {
				Money cost = costclear.GetCost();
				if (cost < 0) {
					cost = -cost; // Negate negative cost to a positive revenue
					str = STR_LAND_AREA_INFORMATION_REVENUE_WHEN_CLEARED;
				} else {
					str = STR_LAND_AREA_INFORMATION_COST_TO_CLEAR;
				}
				SetDParam(0, cost);
			}
		}
		this->landinfo_data.push_back(GetString(str));

		/* Location */
		std::stringstream tile_ss;
		tile_ss << "0x" << std::setfill('0') << std::setw(4) << std::hex << std::uppercase << tile.base(); // 0x%.4X

		SetDParam(0, TileX(tile));
		SetDParam(1, TileY(tile));
		SetDParam(2, GetTileZ(tile));
		SetDParamStr(3, tile_ss.str());
		this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_LANDINFO_COORDS));

		/* Local authority */
		SetDParam(0, STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY_NONE);
		if (t != nullptr) {
			SetDParam(0, STR_TOWN_NAME);
			SetDParam(1, t->index);
		}
		this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY));

		/* Build date */
		if (td.build_date != CalendarTime::INVALID_DATE) {
			SetDParam(0, td.build_date);
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_BUILD_DATE));
		}

		/* Station class */
		if (td.station_class != STR_NULL) {
			SetDParam(0, td.station_class);
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_STATION_CLASS));
		}

		/* Station type name */
		if (td.station_name != STR_NULL) {
			SetDParam(0, td.station_name);
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_STATION_TYPE));
		}

		/* Airport class */
		if (td.airport_class != STR_NULL) {
			SetDParam(0, td.airport_class);
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_AIRPORT_CLASS));
		}

		/* Airport name */
		if (td.airport_name != STR_NULL) {
			SetDParam(0, td.airport_name);
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_AIRPORT_NAME));
		}

		/* Airport tile name */
		if (td.airport_tile_name != STR_NULL) {
			SetDParam(0, td.airport_tile_name);
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_AIRPORTTILE_NAME));
		}

		/* Rail type name */
		if (td.railtype != STR_NULL) {
			SetDParam(0, td.railtype);
			this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_RAIL_TYPE));
		}

		/* Rail speed limit */
		if (td.rail_speed != 0) {
			SetDParam(0, PackVelocity(td.rail_speed, VEH_TRAIN));
			this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_RAIL_SPEED_LIMIT));
		}

		/* Road type name */
		if (td.roadtype != STR_NULL) {
			SetDParam(0, td.roadtype);
			this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_ROAD_TYPE));
		}

		/* Road speed limit */
		if (td.road_speed != 0) {
			SetDParam(0, PackVelocity(td.road_speed, VEH_ROAD));
			this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_ROAD_SPEED_LIMIT));
		}

		/* Tram type name */
		if (td.tramtype != STR_NULL) {
			SetDParam(0, td.tramtype);
			this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_TRAM_TYPE));
		}

		/* Tram speed limit */
		if (td.tram_speed != 0) {
			SetDParam(0, PackVelocity(td.tram_speed, VEH_ROAD));
			this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_TRAM_SPEED_LIMIT));
		}

		/* NewGRF name */
		if (td.grf != nullptr) {
			SetDParamStr(0, td.grf);
			this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_NEWGRF_NAME));
		}

		/* Cargo acceptance is displayed in a extra multiline */
		std::stringstream line;
		line << GetString(STR_LAND_AREA_INFORMATION_CARGO_ACCEPTED);

		bool found = false;
		for (const CargoSpec *cs : _sorted_cargo_specs) {
			CargoID cid = cs->Index();
			if (acceptance[cid] > 0) {
				/* Add a comma between each item. */
				if (found) line << ", ";
				found = true;

				/* If the accepted value is less than 8, show it in 1/8:ths */
				if (acceptance[cid] < 8) {
					SetDParam(0, acceptance[cid]);
					SetDParam(1, cs->name);
					line << GetString(STR_LAND_AREA_INFORMATION_CARGO_EIGHTS);
				} else {
					line << GetString(cs->name);
				}
			}
		}
		if (found) {
			this->cargo_acceptance = line.str();
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

static const NWidgetPart _nested_about_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_ABOUT_OPENTTD, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY), SetPIP(4, 2, 4),
		NWidget(WWT_LABEL, COLOUR_GREY), SetDataTip(STR_ABOUT_ORIGINAL_COPYRIGHT, STR_NULL),
		NWidget(WWT_LABEL, COLOUR_GREY), SetDataTip(STR_ABOUT_VERSION, STR_NULL),
		NWidget(WWT_FRAME, COLOUR_GREY), SetPadding(0, 5, 1, 5),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_A_SCROLLING_TEXT),
		EndContainer(),
		NWidget(WWT_LABEL, COLOUR_GREY, WID_A_WEBSITE), SetDataTip(STR_JUST_RAW_STRING, STR_NULL),
		NWidget(WWT_LABEL, COLOUR_GREY, WID_A_COPYRIGHT), SetDataTip(STR_ABOUT_COPYRIGHT_OPENTTD, STR_NULL),
	EndContainer(),
};

static WindowDesc _about_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_GAME_OPTIONS, WC_NONE,
	0,
	std::begin(_nested_about_widgets), std::end(_nested_about_widgets)
);

static const char * const _credits[] = {
	u8"Original design by Chris Sawyer",
	u8"Original graphics by Simon Foster",
	u8"",
	u8"The OpenTTD team (in alphabetical order):",
	u8"  Matthijs Kooijman (blathijs) - Pathfinder-guru, Debian port (since 0.3)",
	u8"  Christoph Elsenhans (frosch) - General coding (since 0.6)",
	u8"  Lo\u00efc Guilloux (glx) - General / Windows Expert (since 0.4.5)",
	u8"  Charles Pigott (LordAro) - General / Correctness police (since 1.9)",
	u8"  Michael Lutz (michi_cc) - Path based signals (since 0.7)",
	u8"  Niels Martin Hansen (nielsm) - Music system, general coding (since 1.9)",
	u8"  Owen Rudge (orudge) - Forum host, OS/2 port (since 0.1)",
	u8"  Peter Nelson (peter1138) - Spiritual descendant from NewGRF gods (since 0.4.5)",
	u8"  Remko Bijker (Rubidium) - Coder and way more (since 0.4.5)",
	u8"  Patric Stout (TrueBrain) - NoProgrammer (since 0.3), sys op",
	u8"  Tyler Trahan (2TallTyler) - General coding (since 13)",
	u8"",
	u8"Inactive Developers:",
	u8"  Grzegorz Duczy\u0144ski (adf88) - General coding (1.7 - 1.8)",
	u8"  Albert Hofkamp (Alberth) - GUI expert (0.7 - 1.9)",
	u8"  Jean-Fran\u00e7ois Claeys (Belugas) - GUI, NewGRF and more (0.4.5 - 1.0)",
	u8"  Bjarni Corfitzen (Bjarni) - MacOSX port, coder and vehicles (0.3 - 0.7)",
	u8"  Victor Fischer (Celestar) - Programming everywhere you need him to (0.3 - 0.6)",
	u8"  Ulf Hermann (fonsinchen) - Cargo Distribution (1.3 - 1.6)",
	u8"  Jaroslav Mazanec (KUDr) - YAPG (Yet Another Pathfinder God) ;) (0.4.5 - 0.6)",
	u8"  Jonathan Coome (Maedhros) - High priest of the NewGRF Temple (0.5 - 0.6)",
	u8"  Attila B\u00e1n (MiHaMiX) - Developer WebTranslator 1 and 2 (0.3 - 0.5)",
	u8"  Ingo von Borstel (planetmaker) - General coding, Support (1.1 - 1.9)",
	u8"  Zden\u011bk Sojka (SmatZ) - Bug finder and fixer (0.6 - 1.3)",
	u8"  Jos\u00e9 Soler (Terkhen) - General coding (1.0 - 1.4)",
	u8"  Christoph Mallon (Tron) - Programmer, code correctness police (0.3 - 0.5)",
	u8"  Thijs Marinussen (Yexo) - AI Framework, General (0.6 - 1.3)",
	u8"  Leif Linse (Zuu) - AI/Game Script (1.2 - 1.6)",
	u8"",
	u8"Retired Developers:",
	u8"  Tam\u00e1s Farag\u00f3 (Darkvater) - Ex-Lead coder (0.3 - 0.5)",
	u8"  Dominik Scherer (dominik81) - Lead programmer, GUI expert (0.3 - 0.3)",
	u8"  Emil Djupfeld (egladil) - MacOSX (0.4.5 - 0.6)",
	u8"  Simon Sasburg (HackyKid) - Many bugfixes (0.4 - 0.4.5)",
	u8"  Ludvig Strigeus (ludde) - Original author of OpenTTD, main coder (0.1 - 0.3)",
	u8"  Cian Duffy (MYOB) - BeOS port / manual writing (0.1 - 0.3)",
	u8"  Petr Baudi\u0161 (pasky) - Many patches, NewGRF support (0.3 - 0.3)",
	u8"  Benedikt Br\u00fcggemeier (skidd13) - Bug fixer and code reworker (0.6 - 0.7)",
	u8"  Serge Paquet (vurlix) - 2nd contributor after ludde (0.1 - 0.3)",
	u8"",
	u8"Special thanks go out to:",
	u8"  Josef Drexler - For his great work on TTDPatch",
	u8"  Marcin Grzegorczyk - Track foundations and for describing TTD internals",
	u8"  Stefan Mei\u00dfner (sign_de) - For his work on the console",
	u8"  Mike Ragsdale - OpenTTD installer",
	u8"  Christian Rosentreter (tokai) - MorphOS / AmigaOS port",
	u8"  Richard Kempton (richK) - additional airports, initial TGP implementation",
	u8"  Alberto Demichelis - Squirrel scripting language \u00a9 2003-2008",
	u8"  L. Peter Deutsch - MD5 implementation \u00a9 1999, 2000, 2002",
	u8"  Michael Blunck - Pre-signals and semaphores \u00a9 2003",
	u8"  George - Canal/Lock graphics \u00a9 2003-2004",
	u8"  Andrew Parkhouse (andythenorth) - River graphics",
	u8"  David Dallaston (Pikka) - Tram tracks",
	u8"  All Translators - Who made OpenTTD a truly international game",
	u8"  Bug Reporters - Without whom OpenTTD would still be full of bugs!",
	u8"",
	u8"",
	u8"And last but not least:",
	u8"  Chris Sawyer - For an amazing game!"
};

struct AboutWindow : public Window {
	int text_position;                       ///< The top of the scrolling text
	int line_height;                         ///< The height of a single line
	static const int num_visible_lines = 19; ///< The number of lines visible simultaneously

	AboutWindow() : Window(&_about_desc)
	{
		this->InitNested(WN_GAME_OPTIONS_ABOUT);

		this->text_position = this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->pos_y + this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->current_y;
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_A_WEBSITE) SetDParamStr(0, "Website: https://www.openttd.org");
		if (widget == WID_A_COPYRIGHT) SetDParamStr(0, _openttd_revision_year);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != WID_A_SCROLLING_TEXT) return;

		this->line_height = GetCharacterHeight(FS_NORMAL);

		Dimension d;
		d.height = this->line_height * num_visible_lines;

		d.width = 0;
		for (uint i = 0; i < lengthof(_credits); i++) {
			d.width = std::max(d.width, GetStringBoundingBox(_credits[i]).width);
		}
		*size = maxdim(*size, d);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_A_SCROLLING_TEXT) return;

		int y = this->text_position;

		/* Show all scrolling _credits */
		for (uint i = 0; i < lengthof(_credits); i++) {
			if (y >= r.top + 7 && y < r.bottom - this->line_height) {
				DrawString(r.left, r.right, y, _credits[i], TC_BLACK, SA_LEFT | SA_FORCE);
			}
			y += this->line_height;
		}
	}

	/**
	 * Scroll the text in the about window slow.
	 *
	 * The interval of 2100ms is chosen to maintain parity: 2100 / GetCharacterHeight(FS_NORMAL) = 150ms.
	 */
	IntervalTimer<TimerWindow> scroll_interval = {std::chrono::milliseconds(2100) / GetCharacterHeight(FS_NORMAL), [this](uint count) {
		this->text_position -= count;
		/* If the last text has scrolled start a new from the start */
		if (this->text_position < (int)(this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->pos_y - lengthof(_credits) * this->line_height)) {
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
	SetDParam(0, cost);
	ShowErrorMessage(msg, INVALID_STRING_ID, WL_INFO, x, y);
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
	SetDParam(0, cost);
	AddTextEffect(msg, pt.x, pt.y, Ticks::DAY_TICKS, TE_RISING);
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

	SetDParam(0, transfer);
	if (income == 0) {
		AddTextEffect(STR_FEEDER, pt.x, pt.y, Ticks::DAY_TICKS, TE_RISING);
	} else {
		StringID msg = STR_FEEDER_COST;
		if (income < 0) {
			income = -income;
			msg = STR_FEEDER_INCOME;
		}
		SetDParam(1, income);
		AddTextEffect(msg, pt.x, pt.y, Ticks::DAY_TICKS, TE_RISING);
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

	SetDParam(0, percent);
	return AddTextEffect(string, pt.x, pt.y, 0, TE_STATIC);
}

/**
 * Update vehicle loading indicators.
 * @param te_id   TextEffectID to be updated.
 * @param string  String which is printed.
 */
void UpdateFillingPercent(TextEffectID te_id, uint8_t percent, StringID string)
{
	assert(string != STR_NULL);

	SetDParam(0, percent);
	UpdateTextEffect(te_id, string);
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

static const NWidgetPart _nested_tooltips_widgets[] = {
	NWidget(WWT_EMPTY, INVALID_COLOUR, WID_TT_BACKGROUND),
};

static WindowDesc _tool_tips_desc(__FILE__, __LINE__,
	WDP_MANUAL, nullptr, 0, 0, // Coordinates and sizes are not used,
	WC_TOOLTIPS, WC_NONE,
	WDF_NO_FOCUS | WDF_NO_CLOSE,
	std::begin(_nested_tooltips_widgets), std::end(_nested_tooltips_widgets)
);

/** Window for displaying a tooltip. */
struct TooltipsWindow : public Window
{
	StringID string_id;               ///< String to display as tooltip.
	std::vector<StringParameterBackup> params; ///< The string parameters.
	TooltipCloseCondition close_cond; ///< Condition for closing the window.

	TooltipsWindow(Window *parent, StringID str, uint paramcount, TooltipCloseCondition close_tooltip) : Window(&_tool_tips_desc)
	{
		this->parent = parent;
		this->string_id = str;
		CopyOutDParam(this->params, paramcount);
		this->close_cond = close_tooltip;

		this->InitNested();

		CLRBITS(this->flags, WF_WHITE_BORDER);
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

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != WID_TT_BACKGROUND) return;
		CopyInDParam(this->params);

		size->width  = std::min<uint>(GetStringBoundingBox(this->string_id).width, ScaleGUITrad(194));
		size->height = GetStringHeight(this->string_id, size->width);

		/* Increase slightly to have some space around the box. */
		size->width  += WidgetDimensions::scaled.framerect.Horizontal()  + WidgetDimensions::scaled.fullbevel.Horizontal();
		size->height += WidgetDimensions::scaled.framerect.Vertical()    + WidgetDimensions::scaled.fullbevel.Vertical();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_TT_BACKGROUND) return;
		GfxFillRect(r, PC_BLACK);
		GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), PC_LIGHT_YELLOW);

		CopyInDParam(this->params);
		DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.framerect).Shrink(WidgetDimensions::scaled.fullbevel), this->string_id, TC_BLACK, SA_CENTER);
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
 * @param str String to be displayed
 * @param close_tooltip the condition under which the tooltip closes
 * @param paramcount number of params to deal with
 */
void GuiShowTooltips(Window *parent, StringID str, TooltipCloseCondition close_tooltip, uint paramcount)
{
	CloseWindowById(WC_TOOLTIPS, 0);

	if (str == STR_NULL || !_cursor.in_window) return;

	new TooltipsWindow(parent, str, paramcount, close_tooltip);
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

	DrawFrameRect(cr, wi->colour, wi->IsLowered() ? FR_LOWERED : FR_NONE);
	DrawSpriteIgnorePadding(rtl ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT, PAL_NONE, cr, SA_CENTER);
	if (this->text.bytes == 1) GfxFillRect(cr.Shrink(WidgetDimensions::scaled.bevel), _colour_gradient[wi->colour & 0xF][2], FILLRECT_CHECKER);

	DrawFrameRect(fr, wi->colour, FR_LOWERED | FR_DARKENED);
	GfxFillRect(fr.Shrink(WidgetDimensions::scaled.bevel), PC_BLACK);

	fr = fr.Shrink(WidgetDimensions::scaled.framerect);
	/* Limit the drawing of the string inside the widget boundaries */
	DrawPixelInfo dpi;
	if (!FillDrawPixelInfo(&dpi, fr)) return;

	AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);

	/* We will take the current widget length as maximum width, with a small
	 * space reserved at the end for the caret to show */
	const Textbuf *tb = &this->text;
	int delta = std::min(0, (fr.right - fr.left) - tb->pixels - GetCaretWidth());

	if (tb->caretxoffs + delta < 0) delta = -tb->caretxoffs;

	/* If we have a marked area, draw a background highlight. */
	if (tb->marklength != 0) GfxFillRect(delta + tb->markxoffs, 0, delta + tb->markxoffs + tb->marklength - 1, fr.bottom - fr.top, PC_GREY);

	DrawString(delta, tb->pixels, 0, tb->buf, TC_YELLOW);
	bool focussed = w->IsWidgetGloballyFocused(wid) || IsOSKOpenedFor(w, wid);
	if (focussed && tb->caret) {
		int caret_width = GetStringBoundingBox("_").width;
		DrawString(tb->caretxoffs + delta, tb->caretxoffs + delta + caret_width, 0, "_", TC_WHITE);
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
	int delta = std::min(0, (r.right - r.left) - tb->pixels - GetCaretWidth());
	if (tb->caretxoffs + delta < 0) delta = -tb->caretxoffs;

	Point pt = {r.left + tb->caretxoffs + delta, r.top};
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
Rect QueryString::GetBoundingRect(const Window *w, WidgetID wid, const char *from, const char *to) const
{
	const NWidgetLeaf *wi = w->GetWidget<NWidgetLeaf>(wid);

	assert((wi->type & WWT_MASK) == WWT_EDITBOX);

	bool rtl = _current_text_dir == TD_RTL;
	Dimension sprite_size = GetScaledSpriteSize(rtl ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT);
	int clearbtn_width = sprite_size.width + WidgetDimensions::scaled.imgbtn.Horizontal();

	Rect r = wi->GetCurrentRect().Indent(clearbtn_width, !rtl).Shrink(WidgetDimensions::scaled.framerect);

	/* Clamp caret position to be inside our current width. */
	const Textbuf *tb = &this->text;
	int delta = std::min(0, r.Width() - tb->pixels - GetCaretWidth());
	if (tb->caretxoffs + delta < 0) delta = -tb->caretxoffs;

	/* Get location of first and last character. */
	Point p1 = GetCharPosInString(tb->buf, from, FS_NORMAL);
	Point p2 = from != to ? GetCharPosInString(tb->buf, to, FS_NORMAL) : p1;

	return { Clamp(r.left + p1.x + delta, r.left, r.right), r.top, Clamp(r.left + p2.x + delta, r.left, r.right), r.bottom };
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
	int delta = std::min(0, r.Width() - tb->pixels - GetCaretWidth());
	if (tb->caretxoffs + delta < 0) delta = -tb->caretxoffs;

	return ::GetCharAtPosition(tb->buf, pt.x - delta - r.left);
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
		if (this->text.bytes > 1) {
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
	QueryString editbox;    ///< Editbox.
	QueryStringFlags flags; ///< Flags controlling behaviour of the window.
	Dimension warning_size; ///< How much space to use for the warning text

	QueryStringWindow(StringID str, StringID caption, uint max_bytes, uint max_chars, WindowDesc *desc, Window *parent, CharSetFilter afilter, QueryStringFlags flags) :
			Window(desc), editbox(max_bytes, max_chars)
	{
		this->editbox.text.Assign(str);

		if ((flags & QSF_ACCEPT_UNCHANGED) == 0) this->editbox.orig = this->editbox.text.buf;

		this->querystrings[WID_QS_TEXT] = &this->editbox;
		this->editbox.caption = caption;
		this->editbox.cancel_button = WID_QS_CANCEL;
		this->editbox.ok_button = WID_QS_OK;
		this->editbox.text.afilter = afilter;
		this->flags = flags;

		this->InitNested(WN_QUERY_STRING);
		this->UpdateWarningStringSize();

		this->parent = parent;

		this->SetFocusedWidget(WID_QS_TEXT);
	}

	void UpdateWarningStringSize()
	{
		if (this->flags & QSF_PASSWORD) {
			assert(this->nested_root->smallest_x > 0);
			this->warning_size.width = this->nested_root->current_x - WidgetDimensions::scaled.frametext.Horizontal() - WidgetDimensions::scaled.framerect.Horizontal();
			this->warning_size.height = GetStringHeight(STR_WARNING_PASSWORD_SECURITY, this->warning_size.width);
			this->warning_size.height += WidgetDimensions::scaled.frametext.Vertical() + WidgetDimensions::scaled.framerect.Vertical();
		} else {
			this->warning_size = Dimension{ 0, 0 };
		}

		this->ReInit();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget == WID_QS_DEFAULT && (this->flags & QSF_ENABLE_DEFAULT) == 0) {
			/* We don't want this widget to show! */
			fill->width = 0;
			resize->width = 0;
			size->width = 0;
		}

		if (widget == WID_QS_WARNING) {
			*size = this->warning_size;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_QS_WARNING) return;

		if (this->flags & QSF_PASSWORD) {
			DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.framerect).Shrink(WidgetDimensions::scaled.frametext),
				STR_WARNING_PASSWORD_SECURITY, TC_FROMSTRING, SA_CENTER);
		}
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_QS_CAPTION) SetDParam(0, this->editbox.caption);
	}

	void OnOk()
	{
		if (!this->editbox.orig.has_value() || this->editbox.text.buf != this->editbox.orig) {
			assert(this->parent != nullptr);

			this->parent->OnQueryTextFinished(this->editbox.text.buf);
			this->editbox.handled = true;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_QS_DEFAULT:
				this->editbox.text.DeleteAll();
				FALLTHROUGH;

			case WID_QS_OK:
				this->OnOk();
				FALLTHROUGH;

			case WID_QS_CANCEL:
				this->Close();
				break;
		}
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		if (!this->editbox.handled && this->parent != nullptr) {
			Window *parent = this->parent;
			this->parent = nullptr; // so parent doesn't try to close us again
			parent->OnQueryTextFinished(nullptr);
		}
		this->Window::Close();
	}
};

static const NWidgetPart _nested_query_string_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_QS_CAPTION), SetDataTip(STR_JUST_STRING, STR_NULL), SetTextStyle(TC_WHITE),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(WWT_EDITBOX, COLOUR_GREY, WID_QS_TEXT), SetMinimalSize(256, 12), SetFill(1, 1), SetPadding(2, 2, 2, 2),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_QS_WARNING), EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_QS_DEFAULT), SetMinimalSize(87, 12), SetFill(1, 1), SetDataTip(STR_BUTTON_DEFAULT, STR_NULL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_QS_CANCEL), SetMinimalSize(86, 12), SetFill(1, 1), SetDataTip(STR_BUTTON_CANCEL, STR_NULL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_QS_OK), SetMinimalSize(87, 12), SetFill(1, 1), SetDataTip(STR_BUTTON_OK, STR_NULL),
	EndContainer(),
};

static WindowDesc _query_string_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_QUERY_STRING, WC_NONE,
	0,
	std::begin(_nested_query_string_widgets), std::end(_nested_query_string_widgets)
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
void ShowQueryString(StringID str, StringID caption, uint maxsize, Window *parent, CharSetFilter afilter, QueryStringFlags flags)
{
	assert(parent != nullptr);

	CloseWindowByClass(WC_QUERY_STRING);
	new QueryStringWindow(str, caption, ((flags & QSF_LEN_IN_CHARS) ? MAX_CHAR_LENGTH : 1) * maxsize, maxsize, &_query_string_desc, parent, afilter, flags);
}

/**
 * Window used for asking the user a YES/NO question.
 */
struct QueryWindow : public Window {
	QueryCallbackProc *proc; ///< callback function executed on closing of popup. Window* points to parent, bool is true if 'yes' clicked, false otherwise
	std::vector<StringParameterBackup> params; ///< local copy of #_global_string_params
	StringID message;        ///< message shown for query window

	QueryWindow(WindowDesc *desc, StringID caption, StringID message, Window *parent, QueryCallbackProc *callback) : Window(desc)
	{
		/* Create a backup of the variadic arguments to strings because it will be
		 * overridden pretty often. We will copy these back for drawing */
		CopyOutDParam(this->params, 10);
		this->message = message;
		this->proc    = callback;
		this->parent  = parent;

		this->CreateNestedTree();
		this->GetWidget<NWidgetCore>(WID_Q_CAPTION)->SetDataTip(caption, STR_NULL);
		this->FinishInitNested(WN_CONFIRM_POPUP_QUERY);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		if (this->proc != nullptr) this->proc(this->parent, false);
		this->Window::Close();
	}

	void FindWindowPlacementAndResize([[maybe_unused]] int def_width, [[maybe_unused]] int def_height) override
	{
		/* Position query window over the calling window, ensuring it's within screen bounds. */
		this->left = SoftClamp(parent->left + (parent->width / 2) - (this->width / 2), 0, _screen.width - this->width);
		this->top = SoftClamp(parent->top + (parent->height / 2) - (this->height / 2), 0, _screen.height - this->height);
		this->SetDirty();
	}

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_Q_CAPTION:
			case WID_Q_TEXT:
				CopyInDParam(this->params);
				break;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != WID_Q_TEXT) return;

		Dimension d = GetStringMultiLineBoundingBox(this->message, *size);
		d.width += WidgetDimensions::scaled.frametext.Horizontal();
		d.height += WidgetDimensions::scaled.framerect.Vertical();
		*size = d;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_Q_TEXT) return;

		DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.frametext, WidgetDimensions::scaled.framerect),
				this->message, TC_FROMSTRING, SA_CENTER);
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
				FALLTHROUGH;

			case WKC_ESC:
				this->Close();
				return ES_HANDLED;
		}
		return ES_NOT_HANDLED;
	}
};

static const NWidgetPart _nested_query_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_RED),
		NWidget(WWT_CAPTION, COLOUR_RED, WID_Q_CAPTION), // The caption's string is set in the constructor
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_RED),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_TEXT, COLOUR_RED, WID_Q_TEXT), SetMinimalSize(200, 12),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(WidgetDimensions::unscaled.hsep_indent, WidgetDimensions::unscaled.hsep_indent, WidgetDimensions::unscaled.hsep_indent),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_Q_NO), SetMinimalSize(71, 12), SetFill(1, 1), SetDataTip(STR_QUIT_NO, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_Q_YES), SetMinimalSize(71, 12), SetFill(1, 1), SetDataTip(STR_QUIT_YES, STR_NULL),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _query_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_CONFIRM_POPUP_QUERY, WC_NONE,
	WDF_MODAL,
	std::begin(_nested_query_widgets), std::end(_nested_query_widgets)
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
void ShowQuery(StringID caption, StringID message, Window *parent, QueryCallbackProc *callback, bool focus)
{
	if (parent == nullptr) parent = GetMainWindow();

	for (Window *w : Window::Iterate()) {
		if (w->window_class != WC_CONFIRM_POPUP_QUERY) continue;

		QueryWindow *qw = dynamic_cast<QueryWindow *>(w);
		if (qw->parent != parent || qw->proc != callback) continue;

		qw->Close();
		break;
	}

	QueryWindow *q = new QueryWindow(&_query_desc, caption, message, parent, callback);
	if (focus) SetFocusedWindow(q);
}
