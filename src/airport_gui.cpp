/* $Id$ */

/** @file airport_gui.cpp The GUI for airports. */

#include "stdafx.h"
#include "openttd.h"
#include "window_gui.h"
#include "gui.h"
#include "station_gui.h"
#include "terraform_gui.h"
#include "command_func.h"
#include "airport.h"
#include "sound_func.h"
#include "window_func.h"
#include "settings_type.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "player_func.h"
#include "order_func.h"
#include "station_type.h"
#include "tilehighlight_func.h"

#include "table/sprites.h"
#include "table/strings.h"

static byte _selected_airport_type;

static void ShowBuildAirportPicker();


void CcBuildAirport(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_1F_SPLAT, tile);
		ResetObjectToPlace();
	}
}

static void PlaceAirport(TileIndex tile)
{
	DoCommandP(tile, _selected_airport_type, _ctrl_pressed, CcBuildAirport, CMD_BUILD_AIRPORT | CMD_NO_WATER | CMD_MSG(STR_A001_CAN_T_BUILD_AIRPORT_HERE));
}


enum {
	ATW_AIRPORT  = 3,
	ATW_DEMOLISH = 4
};


static void BuildAirClick_Airport(Window *w)
{
	if (HandlePlacePushButton(w, ATW_AIRPORT, SPR_CURSOR_AIRPORT, VHM_RECT, PlaceAirport)) ShowBuildAirportPicker();
}

static void BuildAirClick_Demolish(Window *w)
{
	HandlePlacePushButton(w, ATW_DEMOLISH, ANIMCURSOR_DEMOLISH, VHM_RECT, PlaceProc_DemolishArea);
}


typedef void OnButtonClick(Window *w);
static OnButtonClick * const _build_air_button_proc[] = {
	BuildAirClick_Airport,
	BuildAirClick_Demolish,
};

static void BuildAirToolbWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT:
			w->DrawWidgets();
			break;

		case WE_CLICK:
			if (e->we.click.widget - 3 >= 0)
				_build_air_button_proc[e->we.click.widget - 3](w);
			break;

		case WE_KEYPRESS: {
			switch (e->we.keypress.keycode) {
				case '1': BuildAirClick_Airport(w); break;
				case '2': BuildAirClick_Demolish(w); break;
				default: return;
			}
		} break;

		case WE_PLACE_OBJ:
			_place_proc(e->we.place.tile);
			break;

		case WE_PLACE_DRAG:
			VpSelectTilesWithMethod(e->we.place.pt.x, e->we.place.pt.y, e->we.place.select_method);
			break;

		case WE_PLACE_MOUSEUP:
			if (e->we.place.pt.x != -1 && e->we.place.select_proc == DDSP_DEMOLISH_AREA) {
				GUIPlaceProcDragXY(e->we.place.select_proc, e->we.place.starttile, e->we.place.tile);
			}
			break;

		case WE_ABORT_PLACE_OBJ:
			w->RaiseButtons();

			delete FindWindowById(WC_BUILD_STATION, 0);
			break;

		case WE_DESTROY:
			if (_patches.link_terraform_toolbar) DeleteWindowById(WC_SCEN_LAND_GEN, 0);
			break;
	}
}

static const Widget _air_toolbar_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,            STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,    51,     0,    13, STR_A000_AIRPORTS,   STR_018C_WINDOW_TITLE_DRAG_THIS },
{  WWT_STICKYBOX,   RESIZE_NONE,     7,    52,    63,     0,    13, 0x0,                 STR_STICKY_BUTTON },
{     WWT_IMGBTN,   RESIZE_NONE,     7,     0,    41,    14,    35, SPR_IMG_AIRPORT,     STR_A01E_BUILD_AIRPORT },
{     WWT_IMGBTN,   RESIZE_NONE,     7,    42,    63,    14,    35, SPR_IMG_DYNAMITE,    STR_018D_DEMOLISH_BUILDINGS_ETC },
{   WIDGETS_END},
};


static const WindowDesc _air_toolbar_desc = {
	WDP_ALIGN_TBR, 22, 64, 36, 64, 36,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_air_toolbar_widgets,
	BuildAirToolbWndProc
};

void ShowBuildAirToolbar()
{
	if (!IsValidPlayer(_current_player)) return;

	DeleteWindowByClass(WC_BUILD_TOOLBAR);
	Window *w = AllocateWindowDescFront<Window>(&_air_toolbar_desc, TRANSPORT_AIR);
	if (_patches.link_terraform_toolbar) ShowTerraformToolbar(w);
}

class AirportPickerWindow : public PickerWindowBase {

	enum {
		BAW_BOTTOMPANEL = 10,
		BAW_SMALL_AIRPORT,
		BAW_CITY_AIRPORT,
		BAW_HELIPORT,
		BAW_METRO_AIRPORT,
		BAW_STR_INTERNATIONAL_AIRPORT,
		BAW_COMMUTER_AIRPORT,
		BAW_HELIDEPOT,
		BAW_STR_INTERCONTINENTAL_AIRPORT,
		BAW_HELISTATION,
		BAW_LAST_AIRPORT = BAW_HELISTATION,
		BAW_AIRPORT_COUNT = BAW_LAST_AIRPORT - BAW_SMALL_AIRPORT + 1,
		BAW_BTN_DONTHILIGHT = BAW_LAST_AIRPORT + 1,
		BAW_BTN_DOHILIGHT,
	};

public:

	AirportPickerWindow(const WindowDesc *desc) : PickerWindowBase(desc)
	{
		this->SetWidgetLoweredState(BAW_BTN_DONTHILIGHT, !_station_show_coverage);
		this->SetWidgetLoweredState(BAW_BTN_DOHILIGHT, _station_show_coverage);
		this->LowerWidget(_selected_airport_type + BAW_SMALL_AIRPORT);

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		int i; // airport enabling loop
		uint32 avail_airports;
		const AirportFTAClass *airport;

		avail_airports = GetValidAirports();

		this->RaiseWidget(_selected_airport_type + BAW_SMALL_AIRPORT);
		if (!HasBit(avail_airports, 0) && _selected_airport_type == AT_SMALL) _selected_airport_type = AT_LARGE;
		if (!HasBit(avail_airports, 1) && _selected_airport_type == AT_LARGE) _selected_airport_type = AT_SMALL;
		this->LowerWidget(_selected_airport_type + BAW_SMALL_AIRPORT);

		/* 'Country Airport' starts at widget BAW_SMALL_AIRPORT, and if its bit is set, it is
		 * available, so take its opposite value to set the disabled state.
		 * There are 9 buildable airports
		 * XXX TODO : all airports should be held in arrays, with all relevant data.
		 * This should be part of newgrf-airports, i suppose
		 */
		for (i = 0; i < BAW_AIRPORT_COUNT; i++) this->SetWidgetDisabledState(i + BAW_SMALL_AIRPORT, !HasBit(avail_airports, i));

		/* select default the coverage area to 'Off' (16) */
		airport = GetAirport(_selected_airport_type);
		SetTileSelectSize(airport->size_x, airport->size_y);

		int rad = _patches.modified_catchment ? airport->catchment : (uint)CA_UNMODIFIED;

		if (_station_show_coverage) SetTileSelectBigSize(-rad, -rad, 2 * rad, 2 * rad);

		this->DrawWidgets();
		/* strings such as 'Size' and 'Coverage Area' */
		int text_end = DrawStationCoverageAreaText(2, 206, SCT_ALL, rad, false);
		text_end = DrawStationCoverageAreaText(2, text_end + 4, SCT_ALL, rad, true) + 4;
		if (text_end != this->widget[BAW_BOTTOMPANEL].bottom) {
			this->SetDirty();
			ResizeWindowForWidget(this, BAW_BOTTOMPANEL, 0, text_end - this->widget[BAW_BOTTOMPANEL].bottom);
			this->SetDirty();
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case BAW_SMALL_AIRPORT: case BAW_CITY_AIRPORT: case BAW_HELIPORT: case BAW_METRO_AIRPORT:
			case BAW_STR_INTERNATIONAL_AIRPORT: case BAW_COMMUTER_AIRPORT: case BAW_HELIDEPOT:
			case BAW_STR_INTERCONTINENTAL_AIRPORT: case BAW_HELISTATION:
				this->RaiseWidget(_selected_airport_type + BAW_SMALL_AIRPORT);
				_selected_airport_type = widget - BAW_SMALL_AIRPORT;
				this->LowerWidget(_selected_airport_type + BAW_SMALL_AIRPORT);
				SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;

			case BAW_BTN_DONTHILIGHT: case BAW_BTN_DOHILIGHT:
				_station_show_coverage = (widget != BAW_BTN_DONTHILIGHT);
				this->SetWidgetLoweredState(BAW_BTN_DONTHILIGHT, !_station_show_coverage);
				this->SetWidgetLoweredState(BAW_BTN_DOHILIGHT, _station_show_coverage);
				SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;
		}
	}

	virtual void OnTick()
	{
		CheckRedrawStationCoverage(this);
	}
};

static const Widget _build_airport_picker_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   147,     0,    13, STR_3001_AIRPORT_SELECTION,       STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   147,    14,    52, 0x0,                              STR_NULL},
{      WWT_LABEL,   RESIZE_NONE,     7,     0,   147,    14,    27, STR_SMALL_AIRPORTS,               STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   147,    53,    89, 0x0,                              STR_NULL},
{      WWT_LABEL,   RESIZE_NONE,     7,     0,   147,    52,    65, STR_LARGE_AIRPORTS,               STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   147,    90,   127, 0x0,                              STR_NULL},
{      WWT_LABEL,   RESIZE_NONE,     7,     0,   147,    90,   103, STR_HUB_AIRPORTS,                 STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   147,   128,   177, 0x0,                              STR_NULL},
{      WWT_LABEL,   RESIZE_NONE,     7,     0,   147,   128,   141, STR_HELIPORTS,                    STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   147,   178,   239, 0x0,                              STR_NULL}, // bottom general box
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   145,    27,    38, STR_SMALL_AIRPORT,                STR_3058_SELECT_SIZE_TYPE_OF_AIRPORT},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   145,    65,    76, STR_CITY_AIRPORT,                 STR_3058_SELECT_SIZE_TYPE_OF_AIRPORT},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   145,   141,   152, STR_HELIPORT,                     STR_3058_SELECT_SIZE_TYPE_OF_AIRPORT},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   145,    77,    88, STR_METRO_AIRPORT ,               STR_3058_SELECT_SIZE_TYPE_OF_AIRPORT},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   145,   103,   114, STR_INTERNATIONAL_AIRPORT,        STR_3058_SELECT_SIZE_TYPE_OF_AIRPORT},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   145,    39,    50, STR_COMMUTER_AIRPORT,             STR_3058_SELECT_SIZE_TYPE_OF_AIRPORT},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   145,   165,   176, STR_HELIDEPOT,                    STR_3058_SELECT_SIZE_TYPE_OF_AIRPORT},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   145,   115,   126, STR_INTERCONTINENTAL_AIRPORT,     STR_3058_SELECT_SIZE_TYPE_OF_AIRPORT},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   145,   153,   164, STR_HELISTATION,                  STR_3058_SELECT_SIZE_TYPE_OF_AIRPORT},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    14,    73,   191,   202, STR_02DB_OFF,                     STR_3065_DON_T_HIGHLIGHT_COVERAGE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    74,   133,   191,   202, STR_02DA_ON,                      STR_3064_HIGHLIGHT_COVERAGE_AREA},
{      WWT_LABEL,   RESIZE_NONE,     7,     0,   147,   178,   191, STR_3066_COVERAGE_AREA_HIGHLIGHT, STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _build_airport_desc = {
	WDP_AUTO, WDP_AUTO, 148, 240, 148, 240,
	WC_BUILD_STATION, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_airport_picker_widgets,
	NULL
};

static void ShowBuildAirportPicker()
{
	new AirportPickerWindow(&_build_airport_desc);
}

void InitializeAirportGui()
{
	_selected_airport_type = AT_SMALL;
}
