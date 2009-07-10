/* $Id$ */

/** @file airport_gui.cpp The GUI for airports. */

#include "stdafx.h"
#include "window_gui.h"
#include "station_gui.h"
#include "terraform_gui.h"
#include "airport.h"
#include "sound_func.h"
#include "window_func.h"
#include "strings_func.h"
#include "settings_type.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "company_func.h"
#include "station_type.h"
#include "tilehighlight_func.h"
#include "company_base.h"

#include "table/sprites.h"
#include "table/strings.h"

static byte _selected_airport_type;

static void ShowBuildAirportPicker(Window *parent);


void CcBuildAirport(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_1F_SPLAT, tile);
		if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
	}
}

static void PlaceAirport(TileIndex tile)
{
	uint32 p2 = _ctrl_pressed;
	SB(p2, 16, 16, INVALID_STATION); // no station to join

	CommandContainer cmdcont = { tile, _selected_airport_type, p2, CMD_BUILD_AIRPORT | CMD_MSG(STR_ERROR_CAN_T_BUILD_AIRPORT_HERE), CcBuildAirport, "" };
	ShowSelectStationIfNeeded(cmdcont, _thd.size.x / TILE_SIZE, _thd.size.y / TILE_SIZE);
}

/** Widget number of the airport build window. */
enum {
	ATW_CLOSEBOX,
	ATW_CAPTION,
	ATW_STICKYBOX,
	ATW_AIRPORT,
	ATW_DEMOLISH
};


static void BuildAirClick_Airport(Window *w)
{
	if (HandlePlacePushButton(w, ATW_AIRPORT, SPR_CURSOR_AIRPORT, HT_RECT, PlaceAirport)) ShowBuildAirportPicker(w);
}

static void BuildAirClick_Demolish(Window *w)
{
	HandlePlacePushButton(w, ATW_DEMOLISH, ANIMCURSOR_DEMOLISH, HT_RECT, PlaceProc_DemolishArea);
}


typedef void OnButtonClick(Window *w);
static OnButtonClick * const _build_air_button_proc[] = {
	BuildAirClick_Airport,
	BuildAirClick_Demolish,
};

struct BuildAirToolbarWindow : Window {
	BuildAirToolbarWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->FindWindowPlacementAndResize(desc);
		if (_settings_client.gui.link_terraform_toolbar) ShowTerraformToolbar(this);
	}

	~BuildAirToolbarWindow()
	{
		if (_settings_client.gui.link_terraform_toolbar) DeleteWindowById(WC_SCEN_LAND_GEN, 0, false);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget - ATW_AIRPORT >= 0) {
			_build_air_button_proc[widget - ATW_AIRPORT](this);
		}
	}


	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		switch (keycode) {
			case '1': BuildAirClick_Airport(this); break;
			case '2': BuildAirClick_Demolish(this); break;
			default: return ES_NOT_HANDLED;
		}
		return ES_HANDLED;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		_place_proc(tile);
	}

	virtual void OnPlaceDrag(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt)
	{
		VpSelectTilesWithMethod(pt.x, pt.y, select_method);
	}

	virtual void OnPlaceMouseUp(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt, TileIndex start_tile, TileIndex end_tile)
	{
		if (pt.x != -1 && select_proc == DDSP_DEMOLISH_AREA) {
			GUIPlaceProcDragXY(select_proc, start_tile, end_tile);
		}
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseButtons();

		DeleteWindowById(WC_BUILD_STATION, 0);
		DeleteWindowById(WC_SELECT_STATION, 0);
	}
};

static const Widget _air_toolbar_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,   COLOUR_DARK_GREEN,     0,    10,     0,    13, STR_BLACK_CROSS,              STR_TOOLTIP_CLOSE_WINDOW },
{    WWT_CAPTION,   RESIZE_NONE,   COLOUR_DARK_GREEN,    11,    51,     0,    13, STR_TOOLBAR_AIRCRAFT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS },
{  WWT_STICKYBOX,   RESIZE_NONE,   COLOUR_DARK_GREEN,    52,    63,     0,    13, 0x0,                          STR_STICKY_BUTTON },
{     WWT_IMGBTN,   RESIZE_NONE,   COLOUR_DARK_GREEN,     0,    41,    14,    35, SPR_IMG_AIRPORT,              STR_TOOLBAR_AIRCRAFT_BUILD_AIRPORT_TOOLTIP },
{     WWT_IMGBTN,   RESIZE_NONE,   COLOUR_DARK_GREEN,    42,    63,    14,    35, SPR_IMG_DYNAMITE,             STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC },
{   WIDGETS_END},
};

static const NWidgetPart _nested_air_toolbar_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, ATW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, ATW_CAPTION), SetMinimalSize(41, 14), SetDataTip(STR_TOOLBAR_AIRCRAFT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN, ATW_STICKYBOX),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, ATW_AIRPORT), SetMinimalSize(42, 22), SetDataTip(SPR_IMG_AIRPORT, STR_TOOLBAR_AIRCRAFT_BUILD_AIRPORT_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, ATW_DEMOLISH), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
	EndContainer(),
};

static const WindowDesc _air_toolbar_desc(
	WDP_ALIGN_TBR, 22, 64, 36, 64, 36,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_CONSTRUCTION,
	_air_toolbar_widgets, _nested_air_toolbar_widgets, lengthof(_nested_air_toolbar_widgets)
);

void ShowBuildAirToolbar()
{
	if (!Company::IsValidID(_local_company)) return;

	DeleteWindowByClass(WC_BUILD_TOOLBAR);
	AllocateWindowDescFront<BuildAirToolbarWindow>(&_air_toolbar_desc, TRANSPORT_AIR);
}

/** Airport widgets in the airport picker window. */
enum AirportPickerWidgets {
	BAW_CLOSEBOX,
	BAW_CAPTION,
	/* Panels and labels. */
	BAW_SMALL_AIRPORTS_PANEL,
	BAW_SMALL_AIRPORTS_LABEL,
	BAW_LARGE_AIRPORTS_PANEL,
	BAW_LARGE_AIRPORTS_LABEL,
	BAW_HUB_AIRPORTS_PANEL,
	BAW_HUB_AIRPORTS_LABEL,
	BAW_HELIPORTS_PANEL,
	BAW_HELIPORTS_LABEL,
	BAW_BOTTOMPANEL,
	/* Airport selection buttons. */
	BAW_SMALL_AIRPORT,
	BAW_CITY_AIRPORT,
	BAW_HELIPORT,
	BAW_METRO_AIRPORT,
	BAW_INTERNATIONAL_AIRPORT,
	BAW_COMMUTER_AIRPORT,
	BAW_HELIDEPOT,
	BAW_INTERCONTINENTAL_AIRPORT,
	BAW_HELISTATION,
	/* Coverage. */
	BAW_BTN_DONTHILIGHT,
	BAW_BTN_DOHILIGHT,
	BAW_COVERAGE_LABEL,

	BAW_LAST_AIRPORT = BAW_HELISTATION,
	BAW_AIRPORT_COUNT = BAW_LAST_AIRPORT - BAW_SMALL_AIRPORT + 1,
};

class AirportPickerWindow : public PickerWindowBase {
public:
	AirportPickerWindow(const WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->SetWidgetLoweredState(BAW_BTN_DONTHILIGHT, !_settings_client.gui.station_show_coverage);
		this->SetWidgetLoweredState(BAW_BTN_DOHILIGHT, _settings_client.gui.station_show_coverage);
		this->OnInvalidateData();
		this->SelectOtherAirport(_selected_airport_type);

		if (_settings_game.economy.station_noise_level) {
			ResizeWindowForWidget(this, BAW_BOTTOMPANEL, 0, 10);
		}

		this->FindWindowPlacementAndResize(desc);
	}

	virtual ~AirportPickerWindow()
	{
		DeleteWindowById(WC_SELECT_STATION, 0);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		const AirportFTAClass *airport = GetAirport(_selected_airport_type);
		int rad = _settings_game.station.modified_catchment ? airport->catchment : (uint)CA_UNMODIFIED;

		uint16 top = this->widget[BAW_BTN_DOHILIGHT].bottom + WD_PAR_VSEP_NORMAL;
		/* only show the station (airport) noise, if the noise option is activated */
		if (_settings_game.economy.station_noise_level) {
			/* show the noise of the selected airport */
			SetDParam(0, airport->noise_level);
			DrawString(WD_FRAMERECT_LEFT, this->width - WD_FRAMERECT_RIGHT, top, STR_STATION_NOISE);
			top += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
		}

		/* strings such as 'Size' and 'Coverage Area' */
		top = DrawStationCoverageAreaText(WD_FRAMERECT_LEFT, top, SCT_ALL, rad, false) + WD_PAR_VSEP_NORMAL;
		top = DrawStationCoverageAreaText(WD_FRAMERECT_LEFT, top, SCT_ALL, rad, true) + WD_PAR_VSEP_NORMAL;
		if (top != this->widget[BAW_BOTTOMPANEL].bottom) {
			this->SetDirty();
			ResizeWindowForWidget(this, BAW_BOTTOMPANEL, 0, top - this->widget[BAW_BOTTOMPANEL].bottom);
			this->SetDirty();
		}
	}

	void SelectOtherAirport(byte airport_id)
	{
		this->RaiseWidget(_selected_airport_type + BAW_SMALL_AIRPORT);
		_selected_airport_type = airport_id;
		this->LowerWidget(airport_id + BAW_SMALL_AIRPORT);

		const AirportFTAClass *airport = GetAirport(airport_id);
		SetTileSelectSize(airport->size_x, airport->size_y);

		int rad = _settings_game.station.modified_catchment ? airport->catchment : (uint)CA_UNMODIFIED;
		if (_settings_client.gui.station_show_coverage) SetTileSelectBigSize(-rad, -rad, 2 * rad, 2 * rad);

		this->SetDirty();
	}

	virtual void OnInvalidateData(int data = 0)
	{
		if (!GetAirport(_selected_airport_type)->IsAvailable()) {
			for (int i = 0; i < BAW_AIRPORT_COUNT; i++) {
				if (GetAirport(i)->IsAvailable()) {
					this->SelectOtherAirport(i);
					break;
				}
			}
		}
		for (int i = 0; i < BAW_AIRPORT_COUNT; i++) {
			this->SetWidgetDisabledState(i + BAW_SMALL_AIRPORT, !GetAirport(i)->IsAvailable());
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case BAW_SMALL_AIRPORT: case BAW_CITY_AIRPORT: case BAW_HELIPORT: case BAW_METRO_AIRPORT:
			case BAW_INTERNATIONAL_AIRPORT: case BAW_COMMUTER_AIRPORT: case BAW_HELIDEPOT:
			case BAW_INTERCONTINENTAL_AIRPORT: case BAW_HELISTATION:
				this->SelectOtherAirport(widget - BAW_SMALL_AIRPORT);
				SndPlayFx(SND_15_BEEP);
				DeleteWindowById(WC_SELECT_STATION, 0);
				break;

			case BAW_BTN_DONTHILIGHT: case BAW_BTN_DOHILIGHT:
				_settings_client.gui.station_show_coverage = (widget != BAW_BTN_DONTHILIGHT);
				this->SetWidgetLoweredState(BAW_BTN_DONTHILIGHT, !_settings_client.gui.station_show_coverage);
				this->SetWidgetLoweredState(BAW_BTN_DOHILIGHT, _settings_client.gui.station_show_coverage);
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
{   WWT_CLOSEBOX,   RESIZE_NONE,   COLOUR_DARK_GREEN,   0,    10,     0,    13, STR_BLACK_CROSS,                   STR_TOOLTIP_CLOSE_WINDOW},             // BAW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,   COLOUR_DARK_GREEN,  11,   147,     0,    13, STR_STATION_BUILD_AIRPORT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},   // BAW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,   COLOUR_DARK_GREEN,   0,   147,    14,    52, 0x0,                               STR_NULL},                             // BAW_SMALL_AIRPORTS_PANEL
{      WWT_LABEL,   RESIZE_NONE,   COLOUR_DARK_GREEN,   0,   147,    14,    27, STR_SMALL_AIRPORTS,                STR_NULL},                             // BAW_SMALL_AIRPORTS_LABEL
{      WWT_PANEL,   RESIZE_NONE,   COLOUR_DARK_GREEN,   0,   147,    53,    91, 0x0,                               STR_NULL},                             // BAW_LARGE_AIRPORTS_PANEL
{      WWT_LABEL,   RESIZE_NONE,   COLOUR_DARK_GREEN,   0,   147,    53,    66, STR_LARGE_AIRPORTS,                STR_NULL},                             // BAW_LARGE_AIRPORTS_LABEL
{      WWT_PANEL,   RESIZE_NONE,   COLOUR_DARK_GREEN,   0,   147,    92,   130, 0x0,                               STR_NULL},                             // BAW_HUB_AIRPORTS_PANEL
{      WWT_LABEL,   RESIZE_NONE,   COLOUR_DARK_GREEN,   0,   147,    92,   105, STR_HUB_AIRPORTS,                  STR_NULL},                             // BAW_HUB_AIRPORTS_LABEL
{      WWT_PANEL,   RESIZE_NONE,   COLOUR_DARK_GREEN,   0,   147,   131,   181, 0x0,                               STR_NULL},                             // BAW_HELIPORTS_PANEL
{      WWT_LABEL,   RESIZE_NONE,   COLOUR_DARK_GREEN,   0,   147,   131,   144, STR_HELIPORTS,                     STR_NULL},                             // BAW_HELIPORTS_LABEL
{      WWT_PANEL,   RESIZE_NONE,   COLOUR_DARK_GREEN,   0,   147,   182,   244, 0x0,                               STR_NULL},                             // BAW_BOTTOMPANEL
{    WWT_TEXTBTN,   RESIZE_NONE,   COLOUR_GREY,         2,   145,    28,    39, STR_SMALL_AIRPORT,                 STR_STATION_BUILD_AIRPORT_TOOLTIP},    // BAW_SMALL_AIRPORT
{    WWT_TEXTBTN,   RESIZE_NONE,   COLOUR_GREY,         2,   145,    67,    78, STR_CITY_AIRPORT,                  STR_STATION_BUILD_AIRPORT_TOOLTIP},    // BAW_CITY_AIRPORT
{    WWT_TEXTBTN,   RESIZE_NONE,   COLOUR_GREY,         2,   145,   145,   156, STR_HELIPORT,                      STR_STATION_BUILD_AIRPORT_TOOLTIP},    // BAW_HELIPORT
{    WWT_TEXTBTN,   RESIZE_NONE,   COLOUR_GREY,         2,   145,    79,    90, STR_METRO_AIRPORT ,                STR_STATION_BUILD_AIRPORT_TOOLTIP},    // BAW_METRO_AIRPORT
{    WWT_TEXTBTN,   RESIZE_NONE,   COLOUR_GREY,         2,   145,   106,   117, STR_INTERNATIONAL_AIRPORT,         STR_STATION_BUILD_AIRPORT_TOOLTIP},    // BAW_INTERNATIONAL_AIRPORT
{    WWT_TEXTBTN,   RESIZE_NONE,   COLOUR_GREY,         2,   145,    40,    51, STR_COMMUTER_AIRPORT,              STR_STATION_BUILD_AIRPORT_TOOLTIP},    // BAW_COMMUTER_AIRPORT
{    WWT_TEXTBTN,   RESIZE_NONE,   COLOUR_GREY,         2,   145,   169,   180, STR_HELIDEPOT,                     STR_STATION_BUILD_AIRPORT_TOOLTIP},    // BAW_HELIDEPOT
{    WWT_TEXTBTN,   RESIZE_NONE,   COLOUR_GREY,         2,   145,   118,   129, STR_INTERCONTINENTAL_AIRPORT,      STR_STATION_BUILD_AIRPORT_TOOLTIP},    // BAW_INTERCONTINENTAL_AIRPORT
{    WWT_TEXTBTN,   RESIZE_NONE,   COLOUR_GREY,         2,   145,   157,   168, STR_HELISTATION,                   STR_STATION_BUILD_AIRPORT_TOOLTIP},    // BAW_HELISTATION
{    WWT_TEXTBTN,   RESIZE_NONE,   COLOUR_GREY,        14,    73,   196,   207, STR_STATION_BUILD_COVERAGE_OFF,    STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP}, // BAW_BTN_DONTHILIGHT
{    WWT_TEXTBTN,   RESIZE_NONE,   COLOUR_GREY,        74,   133,   196,   207, STR_STATION_BUILD_COVERAGE_ON,     STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP},  // BAW_BTN_DOHILIGHT
{      WWT_LABEL,   RESIZE_NONE,   COLOUR_DARK_GREEN,   0,   147,   182,   195, STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL},                                // BAW_COVERAGE_LABEL
{   WIDGETS_END},
};

static const NWidgetPart _nested_build_airport_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, BAW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, BAW_CAPTION), SetMinimalSize(137, 14), SetDataTip(STR_STATION_BUILD_AIRPORT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	/* Small airports. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BAW_SMALL_AIRPORTS_PANEL),
		NWidget(WWT_LABEL, COLOUR_DARK_GREEN, BAW_SMALL_AIRPORTS_LABEL), SetMinimalSize(148, 14), SetDataTip(STR_SMALL_AIRPORTS, STR_NULL),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BAW_SMALL_AIRPORT), SetMinimalSize(144, 12),
									SetDataTip(STR_SMALL_AIRPORT, STR_STATION_BUILD_AIRPORT_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BAW_COMMUTER_AIRPORT), SetMinimalSize(144, 12),
									SetDataTip(STR_COMMUTER_AIRPORT, STR_STATION_BUILD_AIRPORT_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(1, 0),
		EndContainer(),
	EndContainer(),
	/* Large airports. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BAW_LARGE_AIRPORTS_PANEL),
		NWidget(WWT_LABEL, COLOUR_DARK_GREEN, BAW_LARGE_AIRPORTS_LABEL), SetMinimalSize(148, 14), SetDataTip(STR_LARGE_AIRPORTS, STR_NULL),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BAW_CITY_AIRPORT), SetMinimalSize(144, 12),
									SetDataTip(STR_CITY_AIRPORT, STR_STATION_BUILD_AIRPORT_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BAW_METRO_AIRPORT), SetMinimalSize(144, 12),
									SetDataTip(STR_METRO_AIRPORT, STR_STATION_BUILD_AIRPORT_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(1, 0),
		EndContainer(),
	EndContainer(),
	/* Hub airports. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BAW_HUB_AIRPORTS_PANEL),
		NWidget(WWT_LABEL, COLOUR_DARK_GREEN, BAW_HUB_AIRPORTS_LABEL), SetMinimalSize(148, 14), SetDataTip(STR_HUB_AIRPORTS, STR_NULL),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BAW_INTERNATIONAL_AIRPORT), SetMinimalSize(144, 12),
									SetDataTip(STR_INTERNATIONAL_AIRPORT, STR_STATION_BUILD_AIRPORT_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BAW_INTERCONTINENTAL_AIRPORT), SetMinimalSize(144, 12),
									SetDataTip(STR_INTERCONTINENTAL_AIRPORT, STR_STATION_BUILD_AIRPORT_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
		EndContainer(),
	EndContainer(),
	/* Heliports. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BAW_HELIPORTS_PANEL),
		NWidget(WWT_LABEL, COLOUR_DARK_GREEN, BAW_HELIPORTS_LABEL), SetMinimalSize(148, 14), SetDataTip(STR_HELIPORTS, STR_NULL),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BAW_HELIPORT), SetMinimalSize(144, 12),
									SetDataTip(STR_HELIPORT, STR_STATION_BUILD_AIRPORT_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BAW_HELISTATION), SetMinimalSize(144, 12),
									SetDataTip(STR_HELISTATION, STR_STATION_BUILD_AIRPORT_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BAW_HELIDEPOT), SetMinimalSize(144, 12),
									SetDataTip(STR_HELIDEPOT, STR_STATION_BUILD_AIRPORT_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
		EndContainer(),
	EndContainer(),
	/* Bottom panel. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BAW_BOTTOMPANEL),
		NWidget(WWT_LABEL, COLOUR_DARK_GREEN, BAW_COVERAGE_LABEL), SetMinimalSize(148, 14), SetDataTip(STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(14, 0),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, BAW_BTN_DONTHILIGHT), SetMinimalSize(60, 12), SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, BAW_BTN_DOHILIGHT), SetMinimalSize(60, 12), SetDataTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
			NWidget(NWID_SPACER), SetMinimalSize(14, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 37),
	EndContainer(),
};

static const WindowDesc _build_airport_desc(
	WDP_AUTO, WDP_AUTO, 148, 245, 148, 245,
	WC_BUILD_STATION, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_CONSTRUCTION,
	_build_airport_picker_widgets, _nested_build_airport_widgets, lengthof(_nested_build_airport_widgets)
);

static void ShowBuildAirportPicker(Window *parent)
{
	new AirportPickerWindow(&_build_airport_desc, parent);
}

void InitializeAirportGui()
{
	_selected_airport_type = AT_SMALL;
}
