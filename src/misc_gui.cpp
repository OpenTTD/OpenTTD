/* $Id$ */

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

#include "widgets/misc_widget.h"

#include "table/strings.h"

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
		NWidget(WWT_DEBUGBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_LI_BACKGROUND), EndContainer(),
};

static WindowDesc _land_info_desc(
	WDP_AUTO, "land_info", 0, 0,
	WC_LAND_INFO, WC_NONE,
	0,
	_nested_land_info_widgets, lengthof(_nested_land_info_widgets)
);

class LandInfoWindow : public Window {
	enum LandInfoLines {
		LAND_INFO_CENTERED_LINES   = 32,                       ///< Up to 32 centered lines (arbitrary limit)
		LAND_INFO_MULTICENTER_LINE = LAND_INFO_CENTERED_LINES, ///< One multicenter line
		LAND_INFO_LINE_END,
	};

	static const uint LAND_INFO_LINE_BUFF_SIZE = 512;

public:
	char landinfo_data[LAND_INFO_LINE_END][LAND_INFO_LINE_BUFF_SIZE];
	TileIndex tile;

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != WID_LI_BACKGROUND) return;

		uint y = r.top + WD_TEXTPANEL_TOP;
		for (uint i = 0; i < LAND_INFO_CENTERED_LINES; i++) {
			if (StrEmpty(this->landinfo_data[i])) break;

			DrawString(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, y, this->landinfo_data[i], i == 0 ? TC_LIGHT_BLUE : TC_FROMSTRING, SA_HOR_CENTER);
			y += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
			if (i == 0) y += 4;
		}

		if (!StrEmpty(this->landinfo_data[LAND_INFO_MULTICENTER_LINE])) {
			SetDParamStr(0, this->landinfo_data[LAND_INFO_MULTICENTER_LINE]);
			DrawStringMultiLine(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, y, r.bottom - WD_TEXTPANEL_BOTTOM, STR_JUST_RAW_STRING, TC_FROMSTRING, SA_CENTER);
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != WID_LI_BACKGROUND) return;

		size->height = WD_TEXTPANEL_TOP + WD_TEXTPANEL_BOTTOM;
		for (uint i = 0; i < LAND_INFO_CENTERED_LINES; i++) {
			if (StrEmpty(this->landinfo_data[i])) break;

			uint width = GetStringBoundingBox(this->landinfo_data[i]).width + WD_FRAMETEXT_LEFT + WD_FRAMETEXT_RIGHT;
			size->width = max(size->width, width);

			size->height += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
			if (i == 0) size->height += 4;
		}

		if (!StrEmpty(this->landinfo_data[LAND_INFO_MULTICENTER_LINE])) {
			uint width = GetStringBoundingBox(this->landinfo_data[LAND_INFO_MULTICENTER_LINE]).width + WD_FRAMETEXT_LEFT + WD_FRAMETEXT_RIGHT;
			size->width = max(size->width, min(300u, width));
			SetDParamStr(0, this->landinfo_data[LAND_INFO_MULTICENTER_LINE]);
			size->height += GetStringHeight(STR_JUST_RAW_STRING, size->width - WD_FRAMETEXT_LEFT - WD_FRAMETEXT_RIGHT);
		}
	}

	LandInfoWindow(TileIndex tile) : Window(&_land_info_desc), tile(tile)
	{
		this->InitNested();

#if defined(_DEBUG)
#	define LANDINFOD_LEVEL 0
#else
#	define LANDINFOD_LEVEL 1
#endif
		DEBUG(misc, LANDINFOD_LEVEL, "TILE: %#x (%i,%i)", tile, TileX(tile), TileY(tile));
		DEBUG(misc, LANDINFOD_LEVEL, "type   = %#x", _m[tile].type);
		DEBUG(misc, LANDINFOD_LEVEL, "height = %#x", _m[tile].height);
		DEBUG(misc, LANDINFOD_LEVEL, "m1     = %#x", _m[tile].m1);
		DEBUG(misc, LANDINFOD_LEVEL, "m2     = %#x", _m[tile].m2);
		DEBUG(misc, LANDINFOD_LEVEL, "m3     = %#x", _m[tile].m3);
		DEBUG(misc, LANDINFOD_LEVEL, "m4     = %#x", _m[tile].m4);
		DEBUG(misc, LANDINFOD_LEVEL, "m5     = %#x", _m[tile].m5);
		DEBUG(misc, LANDINFOD_LEVEL, "m6     = %#x", _me[tile].m6);
		DEBUG(misc, LANDINFOD_LEVEL, "m7     = %#x", _me[tile].m7);
#undef LANDINFOD_LEVEL
	}

	virtual void OnInit()
	{
		Town *t = ClosestTownFromTile(tile, _settings_game.economy.dist_local_authority);

		/* Because build_date is not set yet in every TileDesc, we make sure it is empty */
		TileDesc td;

		td.build_date = INVALID_DATE;

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
		td.rail_speed = 0;
		td.road_speed = 0;

		td.grf = NULL;

		CargoArray acceptance;
		AddAcceptedCargo(tile, acceptance, NULL);
		GetTileDesc(tile, &td);

		uint line_nr = 0;

		/* Tiletype */
		SetDParam(0, td.dparam[0]);
		GetString(this->landinfo_data[line_nr], td.str, lastof(this->landinfo_data[line_nr]));
		line_nr++;

		/* Up to four owners */
		for (uint i = 0; i < 4; i++) {
			if (td.owner_type[i] == STR_NULL) continue;

			SetDParam(0, STR_LAND_AREA_INFORMATION_OWNER_N_A);
			if (td.owner[i] != OWNER_NONE && td.owner[i] != OWNER_WATER) GetNameOfOwner(td.owner[i], tile);
			GetString(this->landinfo_data[line_nr], td.owner_type[i], lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Cost to clear/revenue when cleared */
		StringID str = STR_LAND_AREA_INFORMATION_COST_TO_CLEAR_N_A;
		Company *c = Company::GetIfValid(_local_company);
		if (c != NULL) {
			Money old_money = c->money;
			c->money = INT64_MAX;
			assert(_current_company == _local_company);
			CommandCost costclear = DoCommand(tile, 0, 0, DC_NONE, CMD_LANDSCAPE_CLEAR);
			c->money = old_money;
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
		GetString(this->landinfo_data[line_nr], str, lastof(this->landinfo_data[line_nr]));
		line_nr++;

		/* Location */
		char tmp[16];
		seprintf(tmp, lastof(tmp), "0x%.4X", tile);
		SetDParam(0, TileX(tile));
		SetDParam(1, TileY(tile));
		SetDParam(2, GetTileZ(tile));
		SetDParamStr(3, tmp);
		GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_LANDINFO_COORDS, lastof(this->landinfo_data[line_nr]));
		line_nr++;

		/* Local authority */
		SetDParam(0, STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY_NONE);
		if (t != NULL) {
			SetDParam(0, STR_TOWN_NAME);
			SetDParam(1, t->index);
		}
		GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY, lastof(this->landinfo_data[line_nr]));
		line_nr++;

		/* Build date */
		if (td.build_date != INVALID_DATE) {
			SetDParam(0, td.build_date);
			GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_BUILD_DATE, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Station class */
		if (td.station_class != STR_NULL) {
			SetDParam(0, td.station_class);
			GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_STATION_CLASS, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Station type name */
		if (td.station_name != STR_NULL) {
			SetDParam(0, td.station_name);
			GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_STATION_TYPE, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Airport class */
		if (td.airport_class != STR_NULL) {
			SetDParam(0, td.airport_class);
			GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_AIRPORT_CLASS, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Airport name */
		if (td.airport_name != STR_NULL) {
			SetDParam(0, td.airport_name);
			GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_AIRPORT_NAME, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Airport tile name */
		if (td.airport_tile_name != STR_NULL) {
			SetDParam(0, td.airport_tile_name);
			GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_AIRPORTTILE_NAME, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Rail speed limit */
		if (td.rail_speed != 0) {
			SetDParam(0, td.rail_speed);
			GetString(this->landinfo_data[line_nr], STR_LANG_AREA_INFORMATION_RAIL_SPEED_LIMIT, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* Road speed limit */
		if (td.road_speed != 0) {
			SetDParam(0, td.road_speed);
			GetString(this->landinfo_data[line_nr], STR_LANG_AREA_INFORMATION_ROAD_SPEED_LIMIT, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		/* NewGRF name */
		if (td.grf != NULL) {
			SetDParamStr(0, td.grf);
			GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_NEWGRF_NAME, lastof(this->landinfo_data[line_nr]));
			line_nr++;
		}

		assert(line_nr < LAND_INFO_CENTERED_LINES);

		/* Mark last line empty */
		this->landinfo_data[line_nr][0] = '\0';

		/* Cargo acceptance is displayed in a extra multiline */
		char *strp = GetString(this->landinfo_data[LAND_INFO_MULTICENTER_LINE], STR_LAND_AREA_INFORMATION_CARGO_ACCEPTED, lastof(this->landinfo_data[LAND_INFO_MULTICENTER_LINE]));
		bool found = false;

		for (CargoID i = 0; i < NUM_CARGO; ++i) {
			if (acceptance[i] > 0) {
				/* Add a comma between each item. */
				if (found) {
					*strp++ = ',';
					*strp++ = ' ';
				}
				found = true;

				/* If the accepted value is less than 8, show it in 1/8:ths */
				if (acceptance[i] < 8) {
					SetDParam(0, acceptance[i]);
					SetDParam(1, CargoSpec::Get(i)->name);
					strp = GetString(strp, STR_LAND_AREA_INFORMATION_CARGO_EIGHTS, lastof(this->landinfo_data[LAND_INFO_MULTICENTER_LINE]));
				} else {
					strp = GetString(strp, CargoSpec::Get(i)->name, lastof(this->landinfo_data[LAND_INFO_MULTICENTER_LINE]));
				}
			}
		}
		if (!found) this->landinfo_data[LAND_INFO_MULTICENTER_LINE][0] = '\0';
	}

	virtual bool IsNewGRFInspectable() const
	{
		return ::IsNewGRFInspectable(GetGrfSpecFeature(this->tile), this->tile);
	}

	virtual void ShowNewGRFInspectWindow() const
	{
		::ShowNewGRFInspectWindow(GetGrfSpecFeature(this->tile), this->tile);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		switch (data) {
			case 1:
				/* ReInit, "debug" sprite might have changed */
				this->ReInit();
				break;
		}
	}
};

/**
 * Show land information window.
 * @param tile The tile to show information about.
 */
void ShowLandInfo(TileIndex tile)
{
	DeleteWindowById(WC_LAND_INFO, 0);
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
		NWidget(WWT_LABEL, COLOUR_GREY, WID_A_WEBSITE), SetDataTip(STR_BLACK_RAW_STRING, STR_NULL),
		NWidget(WWT_LABEL, COLOUR_GREY), SetDataTip(STR_ABOUT_COPYRIGHT_OPENTTD, STR_NULL),
	EndContainer(),
};

static WindowDesc _about_desc(
	WDP_CENTER, NULL, 0, 0,
	WC_GAME_OPTIONS, WC_NONE,
	0,
	_nested_about_widgets, lengthof(_nested_about_widgets)
);

static const char * const _credits[] = {
	"Original design by Chris Sawyer",
	"Original graphics by Simon Foster",
	"",
	"The OpenTTD team (in alphabetical order):",
	"  Albert Hofkamp (Alberth) - GUI expert (since 0.7)",
	"  Matthijs Kooijman (blathijs) - Pathfinder-guru, Debian port (since 0.3)",
	"  Ulf Hermann (fonsinchen) - Cargo Distribution (since 1.3)",
	"  Christoph Elsenhans (frosch) - General coding (since 0.6)",
	"  Lo\xC3\xAF""c Guilloux (glx) - General / Windows Expert (since 0.4.5)",
	"  Michael Lutz (michi_cc) - Path based signals (since 0.7)",
	"  Owen Rudge (orudge) - Forum host, OS/2 port (since 0.1)",
	"  Peter Nelson (peter1138) - Spiritual descendant from NewGRF gods (since 0.4.5)",
	"  Ingo von Borstel (planetmaker) - General, Support (since 1.1)",
	"  Remko Bijker (Rubidium) - Lead coder and way more (since 0.4.5)",
	"  Jos\xC3\xA9 Soler (Terkhen) - General coding (since 1.0)",
	"  Leif Linse (Zuu) - AI/Game Script (since 1.2)",
	"",
	"Inactive Developers:",
	"  Jean-Fran\xC3\xA7ois Claeys (Belugas) - GUI, NewGRF and more (0.4.5 - 1.0)",
	"  Bjarni Corfitzen (Bjarni) - MacOSX port, coder and vehicles (0.3 - 0.7)",
	"  Victor Fischer (Celestar) - Programming everywhere you need him to (0.3 - 0.6)",
	"  Jaroslav Mazanec (KUDr) - YAPG (Yet Another Pathfinder God) ;) (0.4.5 - 0.6)",
	"  Jonathan Coome (Maedhros) - High priest of the NewGRF Temple (0.5 - 0.6)",
	"  Attila B\xC3\xA1n (MiHaMiX) - Developer WebTranslator 1 and 2 (0.3 - 0.5)",
	"  Zden\xC4\x9Bk Sojka (SmatZ) - Bug finder and fixer (0.6 - 1.3)",
	"  Christoph Mallon (Tron) - Programmer, code correctness police (0.3 - 0.5)",
	"  Patric Stout (TrueBrain) - NoAI, NoGo, Network (0.3 - 1.2), sys op (active)",
	"  Thijs Marinussen (Yexo) - AI Framework, General (0.6 - 1.3)",
	"",
	"Retired Developers:",
	"  Tam\xC3\xA1s Farag\xC3\xB3 (Darkvater) - Ex-Lead coder (0.3 - 0.5)",
	"  Dominik Scherer (dominik81) - Lead programmer, GUI expert (0.3 - 0.3)",
	"  Emil Djupfeld (egladil) - MacOSX (0.4.5 - 0.6)",
	"  Simon Sasburg (HackyKid) - Many bugfixes (0.4 - 0.4.5)",
	"  Ludvig Strigeus (ludde) - Original author of OpenTTD, main coder (0.1 - 0.3)",
	"  Cian Duffy (MYOB) - BeOS port / manual writing (0.1 - 0.3)",
	"  Petr Baudi\xC5\xA1 (pasky) - Many patches, NewGRF support (0.3 - 0.3)",
	"  Benedikt Br\xC3\xBCggemeier (skidd13) - Bug fixer and code reworker (0.6 - 0.7)",
	"  Serge Paquet (vurlix) - 2nd contributor after ludde (0.1 - 0.3)",
	"",
	"Special thanks go out to:",
	"  Josef Drexler - For his great work on TTDPatch",
	"  Marcin Grzegorczyk - Track foundations and for describing TTD internals",
	"  Stefan Mei\xC3\x9Fner (sign_de) - For his work on the console",
	"  Mike Ragsdale - OpenTTD installer",
	"  Christian Rosentreter (tokai) - MorphOS / AmigaOS port",
	"  Richard Kempton (richK) - additional airports, initial TGP implementation",
	"",
	"  Alberto Demichelis - Squirrel scripting language \xC2\xA9 2003-2008",
	"  L. Peter Deutsch - MD5 implementation \xC2\xA9 1999, 2000, 2002",
	"  Michael Blunck - Pre-signals and semaphores \xC2\xA9 2003",
	"  George - Canal/Lock graphics \xC2\xA9 2003-2004",
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
	int text_position;                       ///< The top of the scrolling text
	byte counter;                            ///< Used to scroll the text every 5 ticks
	int line_height;                         ///< The height of a single line
	static const int num_visible_lines = 19; ///< The number of lines visible simultaneously

	AboutWindow() : Window(&_about_desc)
	{
		this->InitNested(WN_GAME_OPTIONS_ABOUT);

		this->counter = 5;
		this->text_position = this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->pos_y + this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->current_y;
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == WID_A_WEBSITE) SetDParamStr(0, "Website: http://www.openttd.org");
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != WID_A_SCROLLING_TEXT) return;

		this->line_height = FONT_HEIGHT_NORMAL;

		Dimension d;
		d.height = this->line_height * num_visible_lines;

		d.width = 0;
		for (uint i = 0; i < lengthof(_credits); i++) {
			d.width = max(d.width, GetStringBoundingBox(_credits[i]).width);
		}
		*size = maxdim(*size, d);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
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

	virtual void OnTick()
	{
		if (--this->counter == 0) {
			this->counter = 5;
			this->text_position--;
			/* If the last text has scrolled start a new from the start */
			if (this->text_position < (int)(this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->pos_y - lengthof(_credits) * this->line_height)) {
				this->text_position = this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->pos_y + this->GetWidget<NWidgetBase>(WID_A_SCROLLING_TEXT)->current_y;
			}
			this->SetDirty();
		}
	}
};

void ShowAboutWindow()
{
	DeleteWindowByClass(WC_GAME_OPTIONS);
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
 * Display animated income or costs on the map.
 * @param x    World X position of the animation location.
 * @param y    World Y position of the animation location.
 * @param z    World Z position of the animation location.
 * @param cost Estimated cost (or income if negative).
 */
void ShowCostOrIncomeAnimation(int x, int y, int z, Money cost)
{
	Point pt = RemapCoords(x, y, z);
	StringID msg = STR_INCOME_FLOAT_COST;

	if (cost < 0) {
		cost = -cost;
		msg = STR_INCOME_FLOAT_INCOME;
	}
	SetDParam(0, cost);
	AddTextEffect(msg, pt.x, pt.y, DAY_TICKS, TE_RISING);
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
		AddTextEffect(STR_FEEDER, pt.x, pt.y, DAY_TICKS, TE_RISING);
	} else {
		StringID msg = STR_FEEDER_COST;
		if (income < 0) {
			income = -income;
			msg = STR_FEEDER_INCOME;
		}
		SetDParam(1, income);
		AddTextEffect(msg, pt.x, pt.y, DAY_TICKS, TE_RISING);
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
TextEffectID ShowFillingPercent(int x, int y, int z, uint8 percent, StringID string)
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
void UpdateFillingPercent(TextEffectID te_id, uint8 percent, StringID string)
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
	NWidget(WWT_PANEL, COLOUR_GREY, WID_TT_BACKGROUND), SetMinimalSize(200, 32), EndContainer(),
};

static WindowDesc _tool_tips_desc(
	WDP_MANUAL, NULL, 0, 0, // Coordinates and sizes are not used,
	WC_TOOLTIPS, WC_NONE,
	WDF_NO_FOCUS,
	_nested_tooltips_widgets, lengthof(_nested_tooltips_widgets)
);

/** Window for displaying a tooltip. */
struct TooltipsWindow : public Window
{
	StringID string_id;               ///< String to display as tooltip.
	byte paramcount;                  ///< Number of string parameters in #string_id.
	uint64 params[5];                 ///< The string parameters.
	TooltipCloseCondition close_cond; ///< Condition for closing the window.

	TooltipsWindow(Window *parent, StringID str, uint paramcount, const uint64 params[], TooltipCloseCondition close_tooltip) : Window(&_tool_tips_desc)
	{
		this->parent = parent;
		this->string_id = str;
		assert_compile(sizeof(this->params[0]) == sizeof(params[0]));
		assert(paramcount <= lengthof(this->params));
		memcpy(this->params, params, sizeof(this->params[0]) * paramcount);
		this->paramcount = paramcount;
		this->close_cond = close_tooltip;

		this->InitNested();

		CLRBITS(this->flags, WF_WHITE_BORDER);
	}

	virtual Point OnInitialPosition(int16 sm_width, int16 sm_height, int window_number)
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
		pt.y = Clamp(_cursor.pos.y + _cursor.total_size.y + _cursor.total_offs.y + 5, scr_top, scr_bot);
		if (pt.y + sm_height > scr_bot) pt.y = min(_cursor.pos.y + _cursor.total_offs.y - 5, scr_bot) - sm_height;
		pt.x = sm_width >= _screen.width ? 0 : Clamp(_cursor.pos.x - (sm_width >> 1), 0, _screen.width - sm_width);

		return pt;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		/* There is only one widget. */
		for (uint i = 0; i != this->paramcount; i++) SetDParam(i, this->params[i]);

		size->width  = min(GetStringBoundingBox(this->string_id).width, ScaleGUITrad(194));
		size->height = GetStringHeight(this->string_id, size->width);

		/* Increase slightly to have some space around the box. */
		size->width  += 2 + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
		size->height += 2 + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		/* There is only one widget. */
		GfxFillRect(r.left, r.top, r.right, r.bottom, PC_BLACK);
		GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, PC_LIGHT_YELLOW);

		for (uint arg = 0; arg < this->paramcount; arg++) {
			SetDParam(arg, this->params[arg]);
		}
		DrawStringMultiLine(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, r.bottom - WD_FRAMERECT_BOTTOM, this->string_id, TC_FROMSTRING, SA_CENTER);
	}

	virtual void OnMouseLoop()
	{
		/* Always close tooltips when the cursor is not in our window. */
		if (!_cursor.in_window) {
			delete this;
			return;
		}

		/* We can show tooltips while dragging tools. These are shown as long as
		 * we are dragging the tool. Normal tooltips work with hover or rmb. */
		switch (this->close_cond) {
			case TCC_RIGHT_CLICK: if (!_right_button_down) delete this; break;
			case TCC_LEFT_CLICK: if (!_left_button_down) delete this; break;
			case TCC_HOVER: if (!_mouse_hovering) delete this; break;
		}
	}
};

/**
 * Shows a tooltip
 * @param parent The window this tooltip is related to.
 * @param str String to be displayed
 * @param paramcount number of params to deal with
 * @param params (optional) up to 5 pieces of additional information that may be added to a tooltip
 * @param use_left_mouse_button close the tooltip when the left (true) or right (false) mouse button is released
 */
void GuiShowTooltips(Window *parent, StringID str, uint paramcount, const uint64 params[], TooltipCloseCondition close_tooltip)
{
	DeleteWindowById(WC_TOOLTIPS, 0);

	if (str == STR_NULL) return;

	new TooltipsWindow(parent, str, paramcount, params, close_tooltip);
}

void QueryString::HandleEditBox(Window *w, int wid)
{
	if (w->IsWidgetGloballyFocused(wid) && this->text.HandleCaret()) {
		w->SetWidgetDirty(wid);

		/* For the OSK also invalidate the parent window */
		if (w->window_class == WC_OSK) w->InvalidateData();
	}
}

void QueryString::DrawEditBox(const Window *w, int wid) const
{
	const NWidgetLeaf *wi = w->GetWidget<NWidgetLeaf>(wid);

	assert((wi->type & WWT_MASK) == WWT_EDITBOX);

	bool rtl = _current_text_dir == TD_RTL;
	Dimension sprite_size = GetSpriteSize(rtl ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT);
	int clearbtn_width = sprite_size.width + WD_IMGBTN_LEFT + WD_IMGBTN_RIGHT;

	int clearbtn_left  = wi->pos_x + (rtl ? 0 : wi->current_x - clearbtn_width);
	int clearbtn_right = wi->pos_x + (rtl ? clearbtn_width : wi->current_x) - 1;
	int left   = wi->pos_x + (rtl ? clearbtn_width : 0);
	int right  = wi->pos_x + (rtl ? wi->current_x : wi->current_x - clearbtn_width) - 1;

	int top    = wi->pos_y;
	int bottom = wi->pos_y + wi->current_y - 1;

	DrawFrameRect(clearbtn_left, top, clearbtn_right, bottom, wi->colour, wi->IsLowered() ? FR_LOWERED : FR_NONE);
	DrawSprite(rtl ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT, PAL_NONE, clearbtn_left + WD_IMGBTN_LEFT + (wi->IsLowered() ? 1 : 0), (top + bottom - sprite_size.height) / 2 + (wi->IsLowered() ? 1 : 0));
	if (this->text.bytes == 1) GfxFillRect(clearbtn_left + 1, top + 1, clearbtn_right - 1, bottom - 1, _colour_gradient[wi->colour & 0xF][2], FILLRECT_CHECKER);

	DrawFrameRect(left, top, right, bottom, wi->colour, FR_LOWERED | FR_DARKENED);
	GfxFillRect(left + 1, top + 1, right - 1, bottom - 1, PC_BLACK);

	/* Limit the drawing of the string inside the widget boundaries */
	DrawPixelInfo dpi;
	if (!FillDrawPixelInfo(&dpi, left + WD_FRAMERECT_LEFT, top + WD_FRAMERECT_TOP, right - left - WD_FRAMERECT_RIGHT, bottom - top - WD_FRAMERECT_BOTTOM)) return;

	DrawPixelInfo *old_dpi = _cur_dpi;
	_cur_dpi = &dpi;

	/* We will take the current widget length as maximum width, with a small
	 * space reserved at the end for the caret to show */
	const Textbuf *tb = &this->text;
	int delta = min(0, (right - left) - tb->pixels - 10);

	if (tb->caretxoffs + delta < 0) delta = -tb->caretxoffs;

	/* If we have a marked area, draw a background highlight. */
	if (tb->marklength != 0) GfxFillRect(delta + tb->markxoffs, 0, delta + tb->markxoffs + tb->marklength - 1, bottom - top, PC_GREY);

	DrawString(delta, tb->pixels, 0, tb->buf, TC_YELLOW);
	bool focussed = w->IsWidgetGloballyFocused(wid) || IsOSKOpenedFor(w, wid);
	if (focussed && tb->caret) {
		int caret_width = GetStringBoundingBox("_").width;
		DrawString(tb->caretxoffs + delta, tb->caretxoffs + delta + caret_width, 0, "_", TC_WHITE);
	}

	_cur_dpi = old_dpi;
}

/**
 * Get the current caret position.
 * @param w Window the edit box is in.
 * @param wid Widget index.
 * @return Top-left location of the caret, relative to the window.
 */
Point QueryString::GetCaretPosition(const Window *w, int wid) const
{
	const NWidgetLeaf *wi = w->GetWidget<NWidgetLeaf>(wid);

	assert((wi->type & WWT_MASK) == WWT_EDITBOX);

	bool rtl = _current_text_dir == TD_RTL;
	Dimension sprite_size = GetSpriteSize(rtl ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT);
	int clearbtn_width = sprite_size.width + WD_IMGBTN_LEFT + WD_IMGBTN_RIGHT;

	int left   = wi->pos_x + (rtl ? clearbtn_width : 0);
	int right  = wi->pos_x + (rtl ? wi->current_x : wi->current_x - clearbtn_width) - 1;

	/* Clamp caret position to be inside out current width. */
	const Textbuf *tb = &this->text;
	int delta = min(0, (right - left) - tb->pixels - 10);
	if (tb->caretxoffs + delta < 0) delta = -tb->caretxoffs;

	Point pt = {left + WD_FRAMERECT_LEFT + tb->caretxoffs + delta, (int)wi->pos_y + WD_FRAMERECT_TOP};
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
Rect QueryString::GetBoundingRect(const Window *w, int wid, const char *from, const char *to) const
{
	const NWidgetLeaf *wi = w->GetWidget<NWidgetLeaf>(wid);

	assert((wi->type & WWT_MASK) == WWT_EDITBOX);

	bool rtl = _current_text_dir == TD_RTL;
	Dimension sprite_size = GetSpriteSize(rtl ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT);
	int clearbtn_width = sprite_size.width + WD_IMGBTN_LEFT + WD_IMGBTN_RIGHT;

	int left   = wi->pos_x + (rtl ? clearbtn_width : 0);
	int right  = wi->pos_x + (rtl ? wi->current_x : wi->current_x - clearbtn_width) - 1;

	int top    = wi->pos_y + WD_FRAMERECT_TOP;
	int bottom = wi->pos_y + wi->current_y - 1 - WD_FRAMERECT_BOTTOM;

	/* Clamp caret position to be inside our current width. */
	const Textbuf *tb = &this->text;
	int delta = min(0, (right - left) - tb->pixels - 10);
	if (tb->caretxoffs + delta < 0) delta = -tb->caretxoffs;

	/* Get location of first and last character. */
	Point p1 = GetCharPosInString(tb->buf, from, FS_NORMAL);
	Point p2 = from != to ? GetCharPosInString(tb->buf, to, FS_NORMAL) : p1;

	Rect r = { Clamp(left + p1.x + delta + WD_FRAMERECT_LEFT, left, right), top, Clamp(left + p2.x + delta + WD_FRAMERECT_LEFT, left, right - WD_FRAMERECT_RIGHT), bottom };

	return r;
}

/**
 * Get the character that is rendered at a position.
 * @param w Window the edit box is in.
 * @param wid Widget index.
 * @param pt Position to test.
 * @return Pointer to the character at the position or NULL if no character is at the position.
 */
const char *QueryString::GetCharAtPosition(const Window *w, int wid, const Point &pt) const
{
	const NWidgetLeaf *wi = w->GetWidget<NWidgetLeaf>(wid);

	assert((wi->type & WWT_MASK) == WWT_EDITBOX);

	bool rtl = _current_text_dir == TD_RTL;
	Dimension sprite_size = GetSpriteSize(rtl ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT);
	int clearbtn_width = sprite_size.width + WD_IMGBTN_LEFT + WD_IMGBTN_RIGHT;

	int left   = wi->pos_x + (rtl ? clearbtn_width : 0);
	int right  = wi->pos_x + (rtl ? wi->current_x : wi->current_x - clearbtn_width) - 1;

	int top    = wi->pos_y + WD_FRAMERECT_TOP;
	int bottom = wi->pos_y + wi->current_y - 1 - WD_FRAMERECT_BOTTOM;

	if (!IsInsideMM(pt.y, top, bottom)) return NULL;

	/* Clamp caret position to be inside our current width. */
	const Textbuf *tb = &this->text;
	int delta = min(0, (right - left) - tb->pixels - 10);
	if (tb->caretxoffs + delta < 0) delta = -tb->caretxoffs;

	return ::GetCharAtPosition(tb->buf, pt.x - delta - left);
}

void QueryString::ClickEditBox(Window *w, Point pt, int wid, int click_count, bool focus_changed)
{
	const NWidgetLeaf *wi = w->GetWidget<NWidgetLeaf>(wid);

	assert((wi->type & WWT_MASK) == WWT_EDITBOX);

	bool rtl = _current_text_dir == TD_RTL;
	int clearbtn_width = GetSpriteSize(rtl ? SPR_IMG_DELETE_RIGHT : SPR_IMG_DELETE_LEFT).width;

	int clearbtn_left  = wi->pos_x + (rtl ? 0 : wi->current_x - clearbtn_width);

	if (IsInsideBS(pt.x, clearbtn_left, clearbtn_width)) {
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

	QueryStringWindow(StringID str, StringID caption, uint max_bytes, uint max_chars, WindowDesc *desc, Window *parent, CharSetFilter afilter, QueryStringFlags flags) :
			Window(desc), editbox(max_bytes, max_chars)
	{
		char *last_of = &this->editbox.text.buf[this->editbox.text.max_bytes - 1];
		GetString(this->editbox.text.buf, str, last_of);
		str_validate(this->editbox.text.buf, last_of, SVS_NONE);

		/* Make sure the name isn't too long for the text buffer in the number of
		 * characters (not bytes). max_chars also counts the '\0' characters. */
		while (Utf8StringLength(this->editbox.text.buf) + 1 > this->editbox.text.max_chars) {
			*Utf8PrevChar(this->editbox.text.buf + strlen(this->editbox.text.buf)) = '\0';
		}

		this->editbox.text.UpdateSize();

		if ((flags & QSF_ACCEPT_UNCHANGED) == 0) this->editbox.orig = stredup(this->editbox.text.buf);

		this->querystrings[WID_QS_TEXT] = &this->editbox;
		this->editbox.caption = caption;
		this->editbox.cancel_button = WID_QS_CANCEL;
		this->editbox.ok_button = WID_QS_OK;
		this->editbox.text.afilter = afilter;
		this->flags = flags;

		this->InitNested(WN_QUERY_STRING);

		this->parent = parent;

		this->SetFocusedWidget(WID_QS_TEXT);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget == WID_QS_DEFAULT && (this->flags & QSF_ENABLE_DEFAULT) == 0) {
			/* We don't want this widget to show! */
			fill->width = 0;
			resize->width = 0;
			size->width = 0;
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == WID_QS_CAPTION) SetDParam(0, this->editbox.caption);
	}

	void OnOk()
	{
		if (this->editbox.orig == NULL || strcmp(this->editbox.text.buf, this->editbox.orig) != 0) {
			/* If the parent is NULL, the editbox is handled by general function
			 * HandleOnEditText */
			if (this->parent != NULL) {
				this->parent->OnQueryTextFinished(this->editbox.text.buf);
			} else {
				HandleOnEditText(this->editbox.text.buf);
			}
			this->editbox.handled = true;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_QS_DEFAULT:
				this->editbox.text.DeleteAll();
				/* FALL THROUGH */
			case WID_QS_OK:
				this->OnOk();
				/* FALL THROUGH */
			case WID_QS_CANCEL:
				delete this;
				break;
		}
	}

	~QueryStringWindow()
	{
		if (!this->editbox.handled && this->parent != NULL) {
			Window *parent = this->parent;
			this->parent = NULL; // so parent doesn't try to delete us again
			parent->OnQueryTextFinished(NULL);
		}
	}
};

static const NWidgetPart _nested_query_string_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_QS_CAPTION), SetDataTip(STR_WHITE_STRING, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(WWT_EDITBOX, COLOUR_GREY, WID_QS_TEXT), SetMinimalSize(256, 12), SetFill(1, 1), SetPadding(2, 2, 2, 2),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_QS_DEFAULT), SetMinimalSize(87, 12), SetFill(1, 1), SetDataTip(STR_BUTTON_DEFAULT, STR_NULL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_QS_CANCEL), SetMinimalSize(86, 12), SetFill(1, 1), SetDataTip(STR_BUTTON_CANCEL, STR_NULL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_QS_OK), SetMinimalSize(87, 12), SetFill(1, 1), SetDataTip(STR_BUTTON_OK, STR_NULL),
	EndContainer(),
};

static WindowDesc _query_string_desc(
	WDP_CENTER, "query_string", 0, 0,
	WC_QUERY_STRING, WC_NONE,
	0,
	_nested_query_string_widgets, lengthof(_nested_query_string_widgets)
);

/**
 * Show a query popup window with a textbox in it.
 * @param str StringID for the text shown in the textbox
 * @param caption StringID of text shown in caption of querywindow
 * @param maxsize maximum size in bytes or characters (including terminating '\0') depending on flags
 * @param parent pointer to a Window that will handle the events (ok/cancel) of this
 *        window. If NULL, results are handled by global function HandleOnEditText
 * @param afilter filters out unwanted character input
 * @param flags various flags, @see QueryStringFlags
 */
void ShowQueryString(StringID str, StringID caption, uint maxsize, Window *parent, CharSetFilter afilter, QueryStringFlags flags)
{
	DeleteWindowByClass(WC_QUERY_STRING);
	new QueryStringWindow(str, caption, ((flags & QSF_LEN_IN_CHARS) ? MAX_CHAR_LENGTH : 1) * maxsize, maxsize, &_query_string_desc, parent, afilter, flags);
}

/**
 * Window used for asking the user a YES/NO question.
 */
struct QueryWindow : public Window {
	QueryCallbackProc *proc; ///< callback function executed on closing of popup. Window* points to parent, bool is true if 'yes' clicked, false otherwise
	uint64 params[10];       ///< local copy of _decode_parameters
	StringID message;        ///< message shown for query window
	StringID caption;        ///< title of window

	QueryWindow(WindowDesc *desc, StringID caption, StringID message, Window *parent, QueryCallbackProc *callback) : Window(desc)
	{
		/* Create a backup of the variadic arguments to strings because it will be
		 * overridden pretty often. We will copy these back for drawing */
		CopyOutDParam(this->params, 0, lengthof(this->params));
		this->caption = caption;
		this->message = message;
		this->proc    = callback;

		this->InitNested(WN_CONFIRM_POPUP_QUERY);

		this->parent = parent;
		this->left = parent->left + (parent->width / 2) - (this->width / 2);
		this->top = parent->top + (parent->height / 2) - (this->height / 2);
	}

	~QueryWindow()
	{
		if (this->proc != NULL) this->proc(this->parent, false);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_Q_CAPTION:
				CopyInDParam(1, this->params, lengthof(this->params));
				SetDParam(0, this->caption);
				break;

			case WID_Q_TEXT:
				CopyInDParam(0, this->params, lengthof(this->params));
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != WID_Q_TEXT) return;

		Dimension d = GetStringMultiLineBoundingBox(this->message, *size);
		d.width += WD_FRAMETEXT_LEFT + WD_FRAMETEXT_RIGHT;
		d.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
		*size = d;
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != WID_Q_TEXT) return;

		DrawStringMultiLine(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, r.top + WD_FRAMERECT_TOP, r.bottom - WD_FRAMERECT_BOTTOM,
				this->message, TC_FROMSTRING, SA_CENTER);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_Q_YES: {
				/* in the Generate New World window, clicking 'Yes' causes
				 * DeleteNonVitalWindows() to be called - we shouldn't be in a window then */
				QueryCallbackProc *proc = this->proc;
				Window *parent = this->parent;
				/* Prevent the destructor calling the callback function */
				this->proc = NULL;
				delete this;
				if (proc != NULL) {
					proc(parent, true);
					proc = NULL;
				}
				break;
			}
			case WID_Q_NO:
				delete this;
				break;
		}
	}

	virtual EventState OnKeyPress(WChar key, uint16 keycode)
	{
		/* ESC closes the window, Enter confirms the action */
		switch (keycode) {
			case WKC_RETURN:
			case WKC_NUM_ENTER:
				if (this->proc != NULL) {
					this->proc(this->parent, true);
					this->proc = NULL;
				}
				/* FALL THROUGH */
			case WKC_ESC:
				delete this;
				return ES_HANDLED;
		}
		return ES_NOT_HANDLED;
	}
};

static const NWidgetPart _nested_query_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_RED),
		NWidget(WWT_CAPTION, COLOUR_RED, WID_Q_CAPTION), SetDataTip(STR_JUST_STRING, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_RED), SetPIP(8, 15, 8),
		NWidget(WWT_TEXT, COLOUR_RED, WID_Q_TEXT), SetMinimalSize(200, 12),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(20, 29, 20),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_Q_NO), SetMinimalSize(71, 12), SetFill(1, 1), SetDataTip(STR_QUIT_NO, STR_NULL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_Q_YES), SetMinimalSize(71, 12), SetFill(1, 1), SetDataTip(STR_QUIT_YES, STR_NULL),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _query_desc(
	WDP_CENTER, NULL, 0, 0,
	WC_CONFIRM_POPUP_QUERY, WC_NONE,
	WDF_MODAL,
	_nested_query_widgets, lengthof(_nested_query_widgets)
);

/**
 * Show a modal confirmation window with standard 'yes' and 'no' buttons
 * The window is aligned to the centre of its parent.
 * @param caption string shown as window caption
 * @param message string that will be shown for the window
 * @param parent pointer to parent window, if this pointer is NULL the parent becomes
 * the main window WC_MAIN_WINDOW
 * @param callback callback function pointer to set in the window descriptor
 */
void ShowQuery(StringID caption, StringID message, Window *parent, QueryCallbackProc *callback)
{
	if (parent == NULL) parent = FindWindowById(WC_MAIN_WINDOW, 0);

	const Window *w;
	FOR_ALL_WINDOWS_FROM_BACK(w) {
		if (w->window_class != WC_CONFIRM_POPUP_QUERY) continue;

		const QueryWindow *qw = (const QueryWindow *)w;
		if (qw->parent != parent || qw->proc != callback) continue;

		delete qw;
		break;
	}

	new QueryWindow(&_query_desc, caption, message, parent, callback);
}
