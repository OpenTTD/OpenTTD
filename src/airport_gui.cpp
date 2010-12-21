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
#include "sound_func.h"
#include "window_func.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "company_func.h"
#include "tilehighlight_func.h"
#include "company_base.h"
#include "station_type.h"
#include "newgrf_airport.h"
#include "newgrf_callbacks.h"
#include "widgets/dropdown_type.h"
#include "core/geometry_func.hpp"
#include "hotkeys.h"
#include "sprite.h"

#include "table/strings.h"

static AirportClassID _selected_airport_class; ///< the currently visible airport class
static int _selected_airport_index;            ///< the index of the selected airport in the current class or -1
static byte _selected_airport_layout;          ///< selected airport layout number.

static void ShowBuildAirportPicker(Window *parent);

SpriteID GetCustomAirportSprite(const AirportSpec *as, byte layout);

void CcBuildAirport(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	if (result.Failed()) return;

	SndPlayTileFx(SND_1F_SPLAT, tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
}

/**
 * Place an airport.
 * @param tile Position to put the new airport.
 */
static void PlaceAirport(TileIndex tile)
{
	if (_selected_airport_index == -1) return;
	uint32 p2 = _ctrl_pressed;
	SB(p2, 16, 16, INVALID_STATION); // no station to join

	uint32 p1 = AirportClass::Get(_selected_airport_class, _selected_airport_index)->GetIndex();
	p1 |= _selected_airport_layout << 8;
	CommandContainer cmdcont = { tile, p1, p2, CMD_BUILD_AIRPORT | CMD_MSG(STR_ERROR_CAN_T_BUILD_AIRPORT_HERE), CcBuildAirport, "" };
	ShowSelectStationIfNeeded(cmdcont, TileArea(tile, _thd.size.x / TILE_SIZE, _thd.size.y / TILE_SIZE));
}

/** Widget number of the airport build window. */
enum AirportToolbarWidgets {
	ATW_AIRPORT,
	ATW_DEMOLISH,
};


/** Airport build toolbar window handler. */
struct BuildAirToolbarWindow : Window {
	int last_user_action; // Last started user action.

	BuildAirToolbarWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);
		if (_settings_client.gui.link_terraform_toolbar) ShowTerraformToolbar(this);
		this->last_user_action = WIDGET_LIST_END;
	}

	~BuildAirToolbarWindow()
	{
		if (_settings_client.gui.link_terraform_toolbar) DeleteWindowById(WC_SCEN_LAND_GEN, 0, false);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case ATW_AIRPORT:
				if (HandlePlacePushButton(this, ATW_AIRPORT, SPR_CURSOR_AIRPORT, HT_RECT, NULL)) {
					ShowBuildAirportPicker(this);
					this->last_user_action = widget;
				}
				break;

			case ATW_DEMOLISH:
				HandlePlacePushButton(this, ATW_DEMOLISH, ANIMCURSOR_DEMOLISH, HT_RECT, NULL);
				this->last_user_action = widget;
				break;

			default: break;
		}
	}


	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		int num = CheckHotkeyMatch(airtoolbar_hotkeys, keycode, this);
		if (num == -1) return ES_NOT_HANDLED;
		this->OnClick(Point(), num, 1);
		return ES_HANDLED;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		switch (this->last_user_action) {
			case ATW_AIRPORT:
				PlaceAirport(tile);
				break;

			case ATW_DEMOLISH:
				PlaceProc_DemolishArea(tile);
				break;

			default: NOT_REACHED();
		}
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

	static Hotkey<BuildAirToolbarWindow> airtoolbar_hotkeys[];
};

Hotkey<BuildAirToolbarWindow> BuildAirToolbarWindow::airtoolbar_hotkeys[] = {
	Hotkey<BuildAirToolbarWindow>('1', "airport", ATW_AIRPORT),
	Hotkey<BuildAirToolbarWindow>('2', "demolish", ATW_DEMOLISH),
	HOTKEY_LIST_END(BuildAirToolbarWindow)
};
Hotkey<BuildAirToolbarWindow> *_airtoolbar_hotkeys = BuildAirToolbarWindow::airtoolbar_hotkeys;

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

/**
 * Open the build airport toolbar window
 *
 * If the terraform toolbar is linked to the toolbar, that window is also opened.
 *
 * @return newly opened airport toolbar, or NULL if the toolbar could not be opened.
 */
Window *ShowBuildAirToolbar()
{
	if (!Company::IsValidID(_local_company)) return NULL;

	DeleteWindowByClass(WC_BUILD_TOOLBAR);
	return AllocateWindowDescFront<BuildAirToolbarWindow>(&_air_toolbar_desc, TRANSPORT_AIR);
}

EventState AirportToolbarGlobalHotkeys(uint16 key, uint16 keycode)
{
	int num = CheckHotkeyMatch<BuildAirToolbarWindow>(_airtoolbar_hotkeys, keycode, NULL, true);
	if (num == -1) return ES_NOT_HANDLED;
	Window *w = ShowBuildAirToolbar();
	if (w == NULL) return ES_NOT_HANDLED;
	return w->OnKeyPress(key, keycode);
}

/** Airport widgets in the airport picker window. */
enum AirportPickerWidgets {
	BAIRW_CLASS_DROPDOWN,
	BAIRW_AIRPORT_LIST,
	BAIRW_SCROLLBAR,
	BAIRW_LAYOUT_NUM,
	BAIRW_LAYOUT_DECREASE,
	BAIRW_LAYOUT_INCREASE,
	BAIRW_AIRPORT_SPRITE,
	BAIRW_EXTRA_TEXT,
	BAIRW_BOTTOMPANEL,
	BAIRW_COVERAGE_LABEL,
	BAIRW_BTN_DONTHILIGHT,
	BAIRW_BTN_DOHILIGHT,
};

class BuildAirportWindow : public PickerWindowBase {
	SpriteID preview_sprite; ///< Cached airport preview sprite.
	int line_height;
	Scrollbar *vscroll;

	/** Build a dropdown list of available airport classes */
	static DropDownList *BuildAirportClassDropDown()
	{
		DropDownList *list = new DropDownList();

		for (uint i = 0; i < AirportClass::GetCount(); i++) {
			list->push_back(new DropDownListStringItem(AirportClass::GetName((AirportClassID)i), i, false));
		}

		return list;
	}

public:
	BuildAirportWindow(const WindowDesc *desc, Window *parent) : PickerWindowBase(parent)
	{
		this->CreateNestedTree(desc);

		this->vscroll = this->GetScrollbar(BAIRW_SCROLLBAR);
		this->vscroll->SetCapacity(5);
		this->vscroll->SetPosition(0);

		this->FinishInitNested(desc, TRANSPORT_AIR);

		this->SetWidgetLoweredState(BAIRW_BTN_DONTHILIGHT, !_settings_client.gui.station_show_coverage);
		this->SetWidgetLoweredState(BAIRW_BTN_DOHILIGHT, _settings_client.gui.station_show_coverage);
		this->OnInvalidateData();

		this->vscroll->SetCount(AirportClass::GetCount(_selected_airport_class));
		this->SelectFirstAvailableAirport(true);
	}

	virtual ~BuildAirportWindow()
	{
		DeleteWindowById(WC_SELECT_STATION, 0);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case BAIRW_CLASS_DROPDOWN:
				SetDParam(0, AirportClass::GetName(_selected_airport_class));
				break;

			case BAIRW_LAYOUT_NUM:
				SetDParam(0, STR_EMPTY);
				if (_selected_airport_index != -1) {
					const AirportSpec *as = AirportClass::Get(_selected_airport_class, _selected_airport_index);
					StringID string = GetAirportTextCallback(as, _selected_airport_layout, CBID_AIRPORT_LAYOUT_NAME);
					if (string != STR_UNDEFINED) {
						SetDParam(0, string);
					} else if (as->num_table > 1) {
						SetDParam(0, STR_STATION_BUILD_AIRPORT_LAYOUT_NAME);
						SetDParam(1, _selected_airport_layout + 1);
					}
				}
				break;

			default: break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case BAIRW_CLASS_DROPDOWN: {
				Dimension d = {0, 0};
				for (uint i = 0; i < AirportClass::GetCount(); i++) {
					SetDParam(0, AirportClass::GetName((AirportClassID)i));
					d = maxdim(d, GetStringBoundingBox(STR_BLACK_STRING));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case BAIRW_AIRPORT_LIST: {
				for (int i = 0; i < NUM_AIRPORTS; i++) {
					const AirportSpec *as = AirportSpec::Get(i);
					if (!as->enabled) continue;

					size->width = max(size->width, GetStringBoundingBox(as->name).width);
				}

				this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;
				size->height = this->vscroll->GetCapacity() * this->line_height;
				break;
			}

			case BAIRW_AIRPORT_SPRITE:
				for (int i = 0; i < NUM_AIRPORTS; i++) {
					const AirportSpec *as = AirportSpec::Get(i);
					if (!as->enabled) continue;
					for (byte layout = 0; layout < as->num_table; layout++) {
						SpriteID sprite = GetCustomAirportSprite(as, layout);
						if (sprite != 0) {
							Dimension d = GetSpriteSize(sprite);
							d.width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
							d.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
							*size = maxdim(d, *size);
						}
					}
				}
				break;

			case BAIRW_EXTRA_TEXT:
				for (int i = NEW_AIRPORT_OFFSET; i < NUM_AIRPORTS; i++) {
					const AirportSpec *as = AirportSpec::Get(i);
					if (!as->enabled) continue;
					for (byte layout = 0; layout < as->num_table; layout++) {
						StringID string = GetAirportTextCallback(as, layout, CBID_AIRPORT_ADDITIONAL_TEXT);
						if (string == STR_UNDEFINED) continue;

						/* STR_BLACK_STRING is used to start the string with {BLACK} */
						SetDParam(0, string);
						Dimension d = GetStringMultiLineBoundingBox(STR_BLACK_STRING, *size);
						*size = maxdim(d, *size);
					}
				}
				break;

			default: break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case BAIRW_AIRPORT_LIST: {
				int y = r.top;
				for (uint i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && i < AirportClass::GetCount(_selected_airport_class); i++) {
					const AirportSpec *as = AirportClass::Get(_selected_airport_class, i);
					if (!as->IsAvailable()) {
						GfxFillRect(r.left + 1, y + 1, r.right - 1, y + this->line_height - 2, 0, FILLRECT_CHECKER);
					}
					DrawString(r.left + WD_MATRIX_LEFT, r.right + WD_MATRIX_RIGHT, y + WD_MATRIX_TOP, as->name, ((int)i == _selected_airport_index) ? TC_WHITE : TC_BLACK);
					y += this->line_height;
				}
				break;
			}

			case BAIRW_AIRPORT_SPRITE:
				if (this->preview_sprite != 0) {
					Dimension d = GetSpriteSize(this->preview_sprite);
					DrawSprite(this->preview_sprite, COMPANY_SPRITE_COLOUR(_local_company), (r.left + r.right - d.width) / 2, (r.top + r.bottom - d.height) / 2);
				}
				break;

			case BAIRW_EXTRA_TEXT:
				if (_selected_airport_index != -1) {
					const AirportSpec *as = AirportClass::Get(_selected_airport_class, _selected_airport_index);
					StringID string = GetAirportTextCallback(as, _selected_airport_layout, CBID_AIRPORT_ADDITIONAL_TEXT);
					if (string != STR_UNDEFINED) {
						SetDParam(0, string);
						DrawStringMultiLine(r.left, r.right, r.top, r.bottom, STR_BLACK_STRING);
					}
				}
				break;
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
			const AirportSpec *as = AirportClass::Get(_selected_airport_class, _selected_airport_index);
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
		_selected_airport_layout = 0;

		if (_selected_airport_index != -1) {
			const AirportSpec *as = AirportClass::Get(_selected_airport_class, _selected_airport_index);
			this->preview_sprite = GetCustomAirportSprite(as, _selected_airport_layout);
		}

		this->UpdateSelectSize();
		this->SetDirty();
	}

	void UpdateSelectSize()
	{
		if (_selected_airport_index == -1) {
			SetTileSelectSize(1, 1);
			this->DisableWidget(BAIRW_LAYOUT_DECREASE);
			this->DisableWidget(BAIRW_LAYOUT_INCREASE);
		} else {
			const AirportSpec *as = AirportClass::Get(_selected_airport_class, _selected_airport_index);
			int w = as->size_x;
			int h = as->size_y;
			Direction rotation = as->rotation[_selected_airport_layout];
			if (rotation == DIR_E || rotation == DIR_W) Swap(w, h);
			SetTileSelectSize(w, h);

			this->SetWidgetDisabledState(BAIRW_LAYOUT_DECREASE, _selected_airport_layout == 0);
			this->SetWidgetDisabledState(BAIRW_LAYOUT_INCREASE, _selected_airport_layout + 1 >= as->num_table);

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
				int num_clicked = this->vscroll->GetPosition() + (pt.y - this->nested_array[widget]->pos_y) / this->line_height;
				if (num_clicked >= this->vscroll->GetCount()) break;
				const AirportSpec *as = AirportClass::Get(_selected_airport_class, num_clicked);
				if (as->IsAvailable()) this->SelectOtherAirport(num_clicked);
				break;
			}

			case BAIRW_BTN_DONTHILIGHT: case BAIRW_BTN_DOHILIGHT:
				_settings_client.gui.station_show_coverage = (widget != BAIRW_BTN_DONTHILIGHT);
				this->SetWidgetLoweredState(BAIRW_BTN_DONTHILIGHT, !_settings_client.gui.station_show_coverage);
				this->SetWidgetLoweredState(BAIRW_BTN_DOHILIGHT, _settings_client.gui.station_show_coverage);
				this->SetDirty();
				SndPlayFx(SND_15_BEEP);
				this->UpdateSelectSize();
				break;

			case BAIRW_LAYOUT_DECREASE:
				_selected_airport_layout--;
				this->UpdateSelectSize();
				this->SetDirty();
				break;

			case BAIRW_LAYOUT_INCREASE:
				_selected_airport_layout++;
				this->UpdateSelectSize();
				this->SetDirty();
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
		for (uint i = 0; i < AirportClass::GetCount(_selected_airport_class); i++) {
			const AirportSpec *as = AirportClass::Get(_selected_airport_class, i);
			if (as->IsAvailable()) {
				this->SelectOtherAirport(i);
				return;
			}
		}
		if (change_class) {
			/* If that fails, select the first available airport
			 * from a random class. */
			for (AirportClassID j = APC_BEGIN; j < APC_MAX; j++) {
				for (uint i = 0; i < AirportClass::GetCount(j); i++) {
					const AirportSpec *as = AirportClass::Get(j, i);
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
		this->vscroll->SetCount(AirportClass::GetCount(_selected_airport_class));
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
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetFill(1, 0), SetPIP(2, 0, 2),
		NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_AIRPORT_CLASS_LABEL, STR_NULL), SetFill(1, 0),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, BAIRW_CLASS_DROPDOWN), SetFill(1, 0), SetDataTip(STR_BLACK_STRING, STR_NULL),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_MATRIX, COLOUR_GREY, BAIRW_AIRPORT_LIST), SetFill(1, 0), SetDataTip(0x501, STR_NULL), SetScrollbar(BAIRW_SCROLLBAR),
			NWidget(NWID_VSCROLLBAR, COLOUR_GREY, BAIRW_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, BAIRW_LAYOUT_DECREASE), SetMinimalSize(12, 0),SetDataTip(AWV_DECREASE, STR_NULL),
			NWidget(WWT_LABEL, COLOUR_GREY, BAIRW_LAYOUT_NUM), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_BLACK_STRING, STR_NULL),
			NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, BAIRW_LAYOUT_INCREASE), SetMinimalSize(12, 0), SetDataTip(AWV_INCREASE, STR_NULL),
		EndContainer(),
		NWidget(WWT_EMPTY, COLOUR_DARK_GREEN, BAIRW_AIRPORT_SPRITE), SetFill(1, 0),
		NWidget(WWT_EMPTY, COLOUR_DARK_GREEN, BAIRW_EXTRA_TEXT), SetFill(1, 0), SetMinimalSize(150, 0),
	EndContainer(),
	/* Bottom panel. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BAIRW_BOTTOMPANEL), SetPIP(2, 2, 2),
		NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL), SetFill(1, 0),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(14, 0), SetFill(1, 0),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BAIRW_BTN_DONTHILIGHT), SetMinimalSize(60, 12), SetFill(1, 0),
											SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BAIRW_BTN_DOHILIGHT), SetMinimalSize(60, 12), SetFill(1, 0),
											SetDataTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(14, 0), SetFill(1, 0),
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
