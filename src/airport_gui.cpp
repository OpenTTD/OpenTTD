/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file airport_gui.cpp The GUI for airports. */

#include "stdafx.h"
#include "window_gui.h"
#include "station_gui.h"
#include "terraform_gui.h"
#include "airport.h"
#include "sound_func.h"
#include "window_func.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "company_func.h"
#include "tilehighlight_func.h"
#include "company_base.h"
#include "station_type.h"
#include "newgrf_airport.h"
#include "widgets/dropdown_type.h"
#include "core/geometry_func.hpp"

#include "table/sprites.h"
#include "table/strings.h"

static AirportClassID _selected_airport_class; ///< the currently visible airport class
static int _selected_airport_index;            ///< the index of the selected airport in the current class or -1

static void ShowBuildAirportPicker(Window *parent);


void CcBuildAirport(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	if (result.Failed()) return;

	SndPlayTileFx(SND_1F_SPLAT, tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
}

static void PlaceAirport(TileIndex tile)
{
	if (_selected_airport_index == -1) return;
	uint32 p2 = _ctrl_pressed;
	SB(p2, 16, 16, INVALID_STATION); // no station to join

	uint32 p1 = GetAirportSpecFromClass(_selected_airport_class, _selected_airport_index)->GetIndex();
	CommandContainer cmdcont = { tile, p1, p2, CMD_BUILD_AIRPORT | CMD_MSG(STR_ERROR_CAN_T_BUILD_AIRPORT_HERE), CcBuildAirport, "" };
	ShowSelectStationIfNeeded(cmdcont, TileArea(tile, _thd.size.x / TILE_SIZE, _thd.size.y / TILE_SIZE));
}

/** Widget number of the airport build window. */
enum {
	ATW_AIRPORT,
	ATW_DEMOLISH,
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
	BuildAirToolbarWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);
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

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		if (!IsInsideBS(widget, ATW_AIRPORT, lengthof(_build_air_button_proc))) return;

		_build_air_button_proc[widget - ATW_AIRPORT](this);
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

		DeleteWindowById(WC_BUILD_STATION, TRANSPORT_AIR);
		DeleteWindowById(WC_SELECT_STATION, 0);
	}
};

static const NWidgetPart _nested_air_toolbar_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_TOOLBAR_AIRCRAFT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, ATW_AIRPORT), SetFill(0, 1), SetMinimalSize(42, 22), SetDataTip(SPR_IMG_AIRPORT, STR_TOOLBAR_AIRCRAFT_BUILD_AIRPORT_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetMinimalSize(4, 22), SetFill(1, 1), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, ATW_DEMOLISH), SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
	EndContainer(),
};

static const WindowDesc _air_toolbar_desc(
	WDP_ALIGN_TOOLBAR, 0, 0,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_air_toolbar_widgets, lengthof(_nested_air_toolbar_widgets)
);

void ShowBuildAirToolbar()
{
	if (!Company::IsValidID(_local_company)) return;

	DeleteWindowByClass(WC_BUILD_TOOLBAR);
	AllocateWindowDescFront<BuildAirToolbarWindow>(&_air_toolbar_desc, TRANSPORT_AIR);
}

/** Airport widgets in the airport picker window. */
enum AirportPickerWidgets {
	BAIRW_CLASS_DROPDOWN,
	BAIRW_AIRPORT_LIST,
	BAIRW_SCROLLBAR,
	BAIRW_BOTTOMPANEL,
	BAIRW_COVERAGE_LABEL,
	BAIRW_BTN_DONTHILIGHT,
	BAIRW_BTN_DOHILIGHT,
};

class BuildAirportWindow : public PickerWindowBase {
	int line_height;

	/** Build a dropdown list of available airport classes */
	static DropDownList *BuildAirportClassDropDown()
	{
		DropDownList *list = new DropDownList();

		for (uint i = 0; i < GetNumAirportClasses(); i++) {
			list->push_back(new DropDownListStringItem(GetAirportClassName((AirportClassID)i), i, false));
		}

		return list;
	}

public:
	BuildAirportWindow(const WindowDesc *desc, Window *parent) : PickerWindowBase(parent)
	{
		this->vscroll.SetCapacity(5);
		this->vscroll.SetPosition(0);
		this->InitNested(desc, TRANSPORT_AIR);

		this->SetWidgetLoweredState(BAIRW_BTN_DONTHILIGHT, !_settings_client.gui.station_show_coverage);
		this->SetWidgetLoweredState(BAIRW_BTN_DOHILIGHT, _settings_client.gui.station_show_coverage);
		this->OnInvalidateData();

		this->vscroll.SetCount(GetNumAirportsInClass(_selected_airport_class));
		this->SelectFirstAvailableAirport(true);
	}

	virtual ~BuildAirportWindow()
	{
		DeleteWindowById(WC_SELECT_STATION, 0);
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget != BAIRW_CLASS_DROPDOWN) return;

		SetDParam(0, GetAirportClassName(_selected_airport_class));
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case BAIRW_CLASS_DROPDOWN: {
				Dimension d = {0, 0};
				for (uint i = 0; i < GetNumAirportClasses(); i++) {
					SetDParam(0, GetAirportClassName((AirportClassID)i));
					d = maxdim(d, GetStringBoundingBox(STR_BLACK_STRING));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
			} break;

			case BAIRW_AIRPORT_LIST: {
				for (int i = 0; i < NUM_AIRPORTS; i++) {
					const AirportSpec *as = AirportSpec::Get(i);
					if (!as->enabled) continue;

					size->width = max(size->width, GetStringBoundingBox(as->name).width);
				}

				this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;
				size->height = this->vscroll.GetCapacity() * this->line_height;
			} break;

			default: break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != BAIRW_AIRPORT_LIST) return;

		int y = r.top;
		for (uint i = this->vscroll.GetPosition(); this->vscroll.IsVisible(i) && i < GetNumAirportsInClass(_selected_airport_class); i++) {
			const AirportSpec *as = GetAirportSpecFromClass(_selected_airport_class, i);
			if (!as->IsAvailable()) {
				GfxFillRect(r.left + 1, y + 1, r.right - 1, y + this->line_height - 2, 0, FILLRECT_CHECKER);
			}
			DrawString(r.left + WD_MATRIX_LEFT, r.right + WD_MATRIX_RIGHT, y + WD_MATRIX_TOP, as->name, ((int)i == _selected_airport_index) ? TC_WHITE : TC_BLACK);
			y += this->line_height;
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		uint16 top = this->GetWidget<NWidgetBase>(BAIRW_BTN_DOHILIGHT)->pos_y + this->GetWidget<NWidgetBase>(BAIRW_BTN_DOHILIGHT)->current_y + WD_PAR_VSEP_NORMAL;
		NWidgetBase *panel_nwi = this->GetWidget<NWidgetBase>(BAIRW_BOTTOMPANEL);

		int right = panel_nwi->pos_x +  panel_nwi->current_x;
		int bottom = panel_nwi->pos_y +  panel_nwi->current_y;

		if (_selected_airport_index != -1) {
			const AirportSpec *as = GetAirportSpecFromClass(_selected_airport_class, _selected_airport_index);
			int rad = _settings_game.station.modified_catchment ? as->catchment : (uint)CA_UNMODIFIED;

			/* only show the station (airport) noise, if the noise option is activated */
			if (_settings_game.economy.station_noise_level) {
				/* show the noise of the selected airport */
				SetDParam(0, as->noise_level);
				DrawString(panel_nwi->pos_x + WD_FRAMERECT_LEFT, right - WD_FRAMERECT_RIGHT, top, STR_STATION_BUILD_NOISE);
				top += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
			}

			/* strings such as 'Size' and 'Coverage Area' */
			top = DrawStationCoverageAreaText(panel_nwi->pos_x + WD_FRAMERECT_LEFT, right - WD_FRAMERECT_RIGHT, top, SCT_ALL, rad, false) + WD_PAR_VSEP_NORMAL;
			top = DrawStationCoverageAreaText(panel_nwi->pos_x + WD_FRAMERECT_LEFT, right - WD_FRAMERECT_RIGHT, top, SCT_ALL, rad, true) + WD_PAR_VSEP_NORMAL;
		}

		/* Resize background if the text is not equally long as the window. */
		if (top > bottom || (top < bottom && panel_nwi->current_y > panel_nwi->smallest_y)) {
			ResizeWindow(this, 0, top - bottom);
		}
	}

	void SelectOtherAirport(int airport_index)
	{
		_selected_airport_index = airport_index;

		this->UpdateSelectSize();
		this->SetDirty();
	}

	void UpdateSelectSize()
	{
		if (_selected_airport_index == -1) {
			SetTileSelectSize(1, 1);
		} else {
			const AirportSpec *as = GetAirportSpecFromClass(_selected_airport_class, _selected_airport_index);
			SetTileSelectSize(as->size_x, as->size_y);

			int rad = _settings_game.station.modified_catchment ? as->catchment : (uint)CA_UNMODIFIED;
			if (_settings_client.gui.station_show_coverage) SetTileSelectBigSize(-rad, -rad, 2 * rad, 2 * rad);
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case BAIRW_CLASS_DROPDOWN:
				ShowDropDownList(this, BuildAirportClassDropDown(), _selected_airport_class, BAIRW_CLASS_DROPDOWN);
				break;

			case BAIRW_AIRPORT_LIST: {
				int num_clicked = this->vscroll.GetPosition() + (pt.y - this->nested_array[widget]->pos_y) / this->line_height;
				if (num_clicked >= this->vscroll.GetCount()) break;
				const AirportSpec *as = GetAirportSpecFromClass(_selected_airport_class, num_clicked);
				if (as->IsAvailable()) this->SelectOtherAirport(num_clicked);
			} break;

			case BAIRW_BTN_DONTHILIGHT: case BAIRW_BTN_DOHILIGHT:
				_settings_client.gui.station_show_coverage = (widget != BAIRW_BTN_DONTHILIGHT);
				this->SetWidgetLoweredState(BAIRW_BTN_DONTHILIGHT, !_settings_client.gui.station_show_coverage);
				this->SetWidgetLoweredState(BAIRW_BTN_DOHILIGHT, _settings_client.gui.station_show_coverage);
				this->SetDirty();
				SndPlayFx(SND_15_BEEP);
				this->UpdateSelectSize();
				break;
		}
	}

	/**
	 * Select the first available airport.
	 * @param change_class If true, change the class if no airport in the current
	 *   class is available.
	 */
	void SelectFirstAvailableAirport(bool change_class)
	{
		/* First try to select an airport in the selected class. */
		for (uint i = 0; i < GetNumAirportsInClass(_selected_airport_class); i++) {
			const AirportSpec *as = GetAirportSpecFromClass(_selected_airport_class, i);
			if (as->IsAvailable()) {
				this->SelectOtherAirport(i);
				return;
			}
		}
		if (change_class) {
			/* If that fails, select the first available airport
			 * from a random class. */
			for (AirportClassID j = APC_BEGIN; j < APC_MAX; j++) {
				for (uint i = 0; i < GetNumAirportsInClass(j); i++) {
					const AirportSpec *as = GetAirportSpecFromClass(j, i);
					if (as->IsAvailable()) {
						_selected_airport_class = j;
						this->SelectOtherAirport(i);
						return;
					}
				}
			}
		}
		/* If all airports are unavailable, select nothing. */
		this->SelectOtherAirport(-1);
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		assert(widget == BAIRW_CLASS_DROPDOWN);
		_selected_airport_class = (AirportClassID)index;
		this->vscroll.SetCount(GetNumAirportsInClass(_selected_airport_class));
		this->SelectFirstAvailableAirport(false);
	}

	virtual void OnTick()
	{
		CheckRedrawStationCoverage(this);
	}
};

static const NWidgetPart _nested_build_airport_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_AIRPORT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetFill(true, false), SetPIP(2, 0, 2),
		NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_AIRPORT_CLASS_LABEL, STR_NULL), SetFill(true, false),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, BAIRW_CLASS_DROPDOWN), SetFill(true, false), SetDataTip(STR_BLACK_STRING, STR_NULL),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_MATRIX, COLOUR_GREY, BAIRW_AIRPORT_LIST), SetFill(true, false), SetDataTip(0x501, STR_NULL),
			NWidget(WWT_SCROLLBAR, COLOUR_GREY, BAIRW_SCROLLBAR),
		EndContainer(),
	EndContainer(),
	/* Bottom panel. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BAIRW_BOTTOMPANEL), SetPIP(2, 2, 2),
		NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL), SetFill(true, false),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(14, 0), SetFill(true, false),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BAIRW_BTN_DONTHILIGHT), SetMinimalSize(60, 12), SetFill(1, 0),
											SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BAIRW_BTN_DOHILIGHT), SetMinimalSize(60, 12), SetFill(1, 0),
											SetDataTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(14, 0), SetFill(true, false),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 10), SetResize(0, 1), SetFill(1, 0),
	EndContainer(),
};

static const WindowDesc _build_airport_desc(
	WDP_AUTO, 0, 0,
	WC_BUILD_STATION, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_build_airport_widgets, lengthof(_nested_build_airport_widgets)
);

static void ShowBuildAirportPicker(Window *parent)
{
	new BuildAirportWindow(&_build_airport_desc, parent);
}

void InitializeAirportGui()
{
	_selected_airport_class = APC_BEGIN;
}
