/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file engine_gui.cpp GUI to show engine related information. */

#include "stdafx.h"
#include "window_gui.h"
#include "engine_base.h"
#include "command_func.h"
#include "strings_func.h"
#include "engine_gui.h"
#include "articulated_vehicles.h"
#include "vehicle_func.h"
#include "company_func.h"
#include "rail.h"
#include "road.h"
#include "settings_type.h"
#include "train.h"
#include "roadveh.h"
#include "ship.h"
#include "aircraft.h"
#include "engine_cmd.h"
#include "zoom_func.h"
#include "dropdown_func.h"

#include "core/geometry_func.hpp"

#include "widgets/engine_widget.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Return the category of an engine.
 * @param engine Engine to examine.
 * @return String describing the category ("road veh", "train". "airplane", or "ship") of the engine.
 */
StringID GetEngineCategoryName(EngineID engine)
{
	const Engine *e = Engine::Get(engine);
	switch (e->type) {
		default: NOT_REACHED();
		case VEH_ROAD:
			return GetRoadTypeInfo(e->u.road.roadtype)->strings.new_engine;
		case VEH_AIRCRAFT:          return STR_ENGINE_PREVIEW_AIRCRAFT;
		case VEH_SHIP:              return STR_ENGINE_PREVIEW_SHIP;
		case VEH_TRAIN:
			return GetRailTypeInfo(e->u.rail.railtype)->strings.new_loco;
	}
}

static constexpr NWidgetPart _nested_engine_preview_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE, WID_EP_CAPTION), SetTextStyle(TC_WHITE),
		NWidget(WWT_SHADEBOX, COLOUR_LIGHT_BLUE),
	EndContainer(),
	NWidget(NWID_VERTICAL),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_EP_SORT_ASCENDING_DESCENDING), SetStringTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
			NWidget(WWT_DROPDOWN, COLOUR_LIGHT_BLUE, WID_EP_SORT_DROPDOWN), SetResize(1, 0), SetFill(1, 0), SetToolTip(STR_TOOLTIP_SORT_CRITERIA),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_TEXTBTN, COLOUR_LIGHT_BLUE, WID_EP_SHOW_HIDDEN_ENGINES),
			NWidget(WWT_DROPDOWN, COLOUR_LIGHT_BLUE, WID_EP_VEH_TYPE_FILTER_DROPDOWN), SetResize(1, 0), SetFill(1, 0), SetToolTip(STR_TOOLTIP_FILTER_CRITERIA),
			NWidget(WWT_IMGBTN, COLOUR_LIGHT_BLUE, WID_EP_CONFIGURE_BADGES), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON), SetResize(0, 0), SetFill(0, 1), SetSpriteTip(SPR_EXTRA_MENU, STR_BADGE_CONFIG_MENU_TOOLTIP),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE),
			NWidget(WWT_EDITBOX, COLOUR_LIGHT_BLUE, WID_EP_FILTER), SetResize(1, 0), SetFill(1, 0), SetPadding(2), SetStringTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
		EndContainer(),
		NWidget(NWID_VERTICAL, NWidContainerFlag{}, WID_EP_BADGE_FILTER),
		EndContainer(),
	EndContainer(),
	/* Vehicle list. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_LIGHT_BLUE, WID_EP_LIST), SetResize(1, 1), SetFill(1, 0), SetMatrixDataTip(1, 0), SetScrollbar(WID_EP_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_LIGHT_BLUE, WID_EP_SCROLLBAR),
	EndContainer(),
	/* Panel with details. */
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, WID_EP_QUESTION), SetMinimalSize(240, 122), SetResize(1, 0), EndContainer(),
	/* Build/rename buttons, resize button. */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_EP_BUILD_SEL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_EP_YES), SetStringTip(STR_QUIT_YES), SetResize(1, 0), SetFill(1, 0),
		EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_EP_NO), SetStringTip(STR_QUIT_NO), SetResize(1, 0), SetFill(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_LIGHT_BLUE),
	EndContainer(),
};

struct EnginePreviewWindow : Window {
	static EnginePreviewWindow *_current_instance;

	int vehicle_space = 0; // The space to show the vehicle image
	GUIEngineList eng_list{};
	GUIEngineList filtered_eng_list{};
	EngineID selected_engine = EngineID::Invalid(); ///< Currently selected engine, or #EngineID::Invalid()
	Scrollbar *vscroll = nullptr;
	VehicleType vehicle_type = VEH_ANY; ///< Type of vehicles shown in the window.
	VehicleType veh_type_filter_criteria = VEH_ANY; ///< Selected vehicle type filter
	bool descending_sort_order = false; ///< Sort direction, @see _engine_sort_direction
	uint8_t sort_criteria = 0; ///< Current sort criterium.

	EnginePreviewWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);

		this->vscroll = this->GetScrollbar(WID_EP_SCROLLBAR);
		assert(this->vscroll != nullptr);

		EnginePreviewWindow::_current_instance = this;

		this->sort_criteria = _engine_sort_last_criteria[this->veh_type_filter_criteria];
		this->descending_sort_order = _engine_sort_last_order[this->veh_type_filter_criteria];

		/* There is no way to recover the window; so disallow closure via DEL; unless SHIFT+DEL */
		this->flags.Set(WindowFlag::Sticky);

		EngineID engine = static_cast<EngineID>(window_number);
		this->AddNewOffert(engine);

		/* Select the first unshaded engine in the list as default when opening the window */
		auto it = std::ranges::find_if(this->eng_list, [](const GUIEngineListItem &item) { return !item.flags.Test(EngineDisplayFlag::Shaded); });
		if (it != this->eng_list.end()) engine = it->engine_id;
		this->selected_engine = engine;
	}

	StringID GetVehTypeFilterLabel(CargoType cargo_type) const
	{
		switch (cargo_type) {
			case VEH_ANY: return STR_ENGINE_PREVIEW_ALL_TYPES;
			case VEH_TRAIN: return STR_ENGINE_PREVIEW_TRAIN_ONLY;
			case VEH_ROAD: return STR_ENGINE_PREVIEW_ROAD_VEHICLE_ONLY;
			case VEH_SHIP: return STR_ENGINE_PREVIEW_SHIP_ONLY;
			case VEH_AIRCRAFT: return STR_ENGINE_PREVIEW_AIRCRAFT_ONLY;
			default: NOT_REACHED();
		}
	}

	DropDownList BuildVehTypeDropDownList() const
	{
		DropDownList list;

		list.push_back(MakeDropDownListStringItem(this->GetVehTypeFilterLabel(VEH_ANY), VEH_ANY));
		list.push_back(MakeDropDownListStringItem(this->GetVehTypeFilterLabel(VEH_TRAIN), VEH_TRAIN));
		list.push_back(MakeDropDownListStringItem(this->GetVehTypeFilterLabel(VEH_ROAD), VEH_ROAD));
		list.push_back(MakeDropDownListStringItem(this->GetVehTypeFilterLabel(VEH_SHIP), VEH_SHIP));
		list.push_back(MakeDropDownListStringItem(this->GetVehTypeFilterLabel(VEH_AIRCRAFT), VEH_AIRCRAFT));

		return list;
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_EP_QUESTION: {
				/* Get size of engine sprite, on loan from depot_gui.cpp */
				EngineImageType image_type = EIT_PREVIEW;
				uint x, y;
				int x_offs, y_offs;

			    EngineID engine = this->selected_engine;
				if(engine == EngineID::Invalid()) engine = static_cast<EngineID>(this->window_number);

				const Engine *e = Engine::Get(engine);
				switch (e->type) {
					default: NOT_REACHED();
					case VEH_TRAIN:    GetTrainSpriteSize(   engine, x, y, x_offs, y_offs, image_type); break;
					case VEH_ROAD:     GetRoadVehSpriteSize( engine, x, y, x_offs, y_offs, image_type); break;
					case VEH_SHIP:     GetShipSpriteSize(    engine, x, y, x_offs, y_offs, image_type); break;
					case VEH_AIRCRAFT: GetAircraftSpriteSize(engine, x, y, x_offs, y_offs, image_type); break;
				}
				this->vehicle_space = std::max<int>(ScaleSpriteTrad(40), y - y_offs);

				size.width = std::max(size.width, x + std::abs(x_offs));
				size.height = GetStringHeight(GetString(STR_ENGINE_PREVIEW_MESSAGE, GetEngineCategoryName(engine)), size.width) + WidgetDimensions::scaled.vsep_wide + GetCharacterHeight(FS_NORMAL) + this->vehicle_space;
				size.height += GetStringHeight(GetEngineInfoString(engine), size.width);
				break;
			}

			case WID_EP_LIST: {
				uint h;
				if (this->veh_type_filter_criteria != VEH_ANY) {
					h = GetEngineListHeight(this->veh_type_filter_criteria);
					this->vehicle_type = this->veh_type_filter_criteria;
				} else {
					h = GetEngineListHeight(VEH_TRAIN);
					if (h > GetEngineListHeight(VEH_ROAD)) {
						if (h > GetEngineListHeight(VEH_SHIP)) {
							if (h > GetEngineListHeight(VEH_AIRCRAFT)) {
								this->vehicle_type = VEH_TRAIN;
							} else {
								h = GetEngineListHeight(VEH_AIRCRAFT);
								this->vehicle_type = VEH_AIRCRAFT;
							}
						} else {
							h = GetEngineListHeight(VEH_SHIP);
							if (h > GetEngineListHeight(VEH_AIRCRAFT)) {
								this->vehicle_type = VEH_SHIP;
							} else {
								h = GetEngineListHeight(VEH_AIRCRAFT);
								this->vehicle_type = VEH_AIRCRAFT;
							}
						}
					} else {
						h = GetEngineListHeight(VEH_ROAD);
						if (h > GetEngineListHeight(VEH_SHIP)) {
							if (h > GetEngineListHeight(VEH_AIRCRAFT)) {
								this->vehicle_type = VEH_ROAD;
							} else {
								h = GetEngineListHeight(VEH_AIRCRAFT);
								this->vehicle_type = VEH_AIRCRAFT;
							}
						} else {
							h = GetEngineListHeight(VEH_SHIP);
							if (h > GetEngineListHeight(VEH_AIRCRAFT)) {
								this->vehicle_type = VEH_SHIP;
							} else {
								h = GetEngineListHeight(VEH_AIRCRAFT);
								this->vehicle_type = VEH_AIRCRAFT;
							}
						}
					}
				}

				VehicleCellSize tcs = GetVehicleImageCellSize(VEH_TRAIN, EIT_PURCHASE);
				VehicleCellSize rcs = GetVehicleImageCellSize(VEH_ROAD, EIT_PURCHASE);
				VehicleCellSize scs = GetVehicleImageCellSize(VEH_SHIP, EIT_PURCHASE);
				VehicleCellSize acs = GetVehicleImageCellSize(VEH_AIRCRAFT, EIT_PURCHASE);

				fill.height = resize.height = h;
				size.height = 3 * resize.height;
				size.width = std::max(size.width, std::max({tcs.extend_left, rcs.extend_left, scs.extend_left, acs.extend_left}) + std::max({tcs.extend_right, rcs.extend_right, scs.extend_right, acs.extend_right}) + 165) + padding.width;
				break;
			}

			case WID_EP_VEH_TYPE_FILTER_DROPDOWN:
				size.width = std::max(size.width, GetDropDownListDimension(this->BuildVehTypeDropDownList()).width + padding.width);
				break;

			case WID_EP_SORT_ASCENDING_DESCENDING: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->GetString());
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}
		}
	}

	void FilterEngineList()
	{
		this->filtered_eng_list.clear();

		if (this->veh_type_filter_criteria == VEH_ANY) {
			this->filtered_eng_list.insert(this->filtered_eng_list.begin(), this->eng_list.begin(), this->eng_list.end());
		} else {
			for (auto it = this->eng_list.begin(); it != this->eng_list.end(); ++it) {
				const Engine *e = Engine::Get((*it).engine_id);
				if(e->type == this->veh_type_filter_criteria) {
					this->filtered_eng_list.push_back((*it));
				}
			}
		}

		_engine_sort_direction = this->descending_sort_order;
		EngList_Sort(this->filtered_eng_list, _engine_sort_functions[this->veh_type_filter_criteria][this->sort_criteria]);

		this->vscroll->SetCount(this->filtered_eng_list.size());
	}

	void UpdateScrollCapacity()
	{
		this->vscroll->SetCapacityFromWidget(this, WID_EP_LIST);
	}

	void AddNewOffert(EngineID &engine)
	{
		assert(this->vscroll != nullptr);

		const Engine *e = Engine::Get(engine);
		EngineDisplayFlags flags;

		/* Ignore folding and variants. */
		if(e->display_flags.Test(EngineDisplayFlag::Shaded)) flags.Set(EngineDisplayFlag::Shaded);
		this->eng_list.emplace_back(engine, e->info.variant_id, flags, 0);

		this->FilterEngineList();
		this->UpdateScrollCapacity();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_EP_QUESTION: {
				int y = DrawStringMultiLine(r, GetString(STR_ENGINE_PREVIEW_MESSAGE, GetEngineCategoryName(this->selected_engine)), TC_FROMSTRING, SA_HOR_CENTER | SA_TOP) + WidgetDimensions::scaled.vsep_wide;

				DrawString(r.left, r.right, y, GetString(STR_ENGINE_NAME, PackEngineNameDParam(this->selected_engine, EngineNameContext::PreviewNews)), TC_BLACK, SA_HOR_CENTER);
				y += GetCharacterHeight(FS_NORMAL);

				DrawVehicleEngine(r.left, r.right, this->width >> 1, y + this->vehicle_space / 2, this->selected_engine, GetEnginePalette(this->selected_engine, _local_company), EIT_PREVIEW);

				y += this->vehicle_space;
				DrawStringMultiLine(r.left, r.right, y, r.bottom, GetEngineInfoString(this->selected_engine), TC_BLACK, SA_CENTER);
				break;
			}

			case WID_EP_LIST:
				DrawEngineList(
					this->vehicle_type,
					r,
					this->filtered_eng_list,
					*this->vscroll,
					this->selected_engine,
					false,
					DEFAULT_GROUP,
					{}
				);
				break;

			case WID_EP_SORT_ASCENDING_DESCENDING:
				this->DrawSortButtonState(WID_EP_SORT_ASCENDING_DESCENDING, this->descending_sort_order ? SBS_DOWN : SBS_UP);
				break;
		}
	}

	void OnDropdownSelect(WidgetID widget, int index, int /*click_result*/) override
	{
		switch (widget) {
			case WID_EP_SORT_DROPDOWN:
				if (this->sort_criteria != index) {
					this->sort_criteria = index;
					_engine_sort_last_criteria[this->veh_type_filter_criteria] = this->sort_criteria;
					this->FilterEngineList();
				}
				break;

			case WID_EP_VEH_TYPE_FILTER_DROPDOWN: // Select a cargo filter criteria
				if (static_cast<int>(this->veh_type_filter_criteria) != index) {
					this->veh_type_filter_criteria = static_cast<VehicleType>(index);
					this->sort_criteria = _engine_sort_last_criteria[this->veh_type_filter_criteria];
					this->descending_sort_order = _engine_sort_last_order[this->veh_type_filter_criteria];
					this->FilterEngineList();
				}
				break;

			default:
				break;
		}
		this->SetDirty();
	}

	void OnResize() override
	{
		if (this->vscroll == nullptr) return;
		this->UpdateScrollCapacity();
	}

	void Close(int data = 0) override
	{
		EnginePreviewWindow::_current_instance = nullptr;
		this->Window::Close(data);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_EP_VEH_TYPE_FILTER_DROPDOWN: // Select cargo filtering criteria dropdown menu
				ShowDropDownList(this, this->BuildVehTypeDropDownList(), this->veh_type_filter_criteria, widget);
				break;

			case WID_EP_SORT_DROPDOWN: // Select sorting criteria dropdown menu
				DisplayVehicleSortDropDown(this, this->veh_type_filter_criteria, this->sort_criteria, WID_EP_SORT_DROPDOWN);
				break;

			case WID_EP_SORT_ASCENDING_DESCENDING:
				this->descending_sort_order ^= true;
				_engine_sort_last_order[this->veh_type_filter_criteria] = this->descending_sort_order;
				this->FilterEngineList();
				this->SetDirty();
				break;

			case WID_EP_LIST: {
				const auto it = this->vscroll->GetScrolledItemFromWidget(this->filtered_eng_list, pt.y, this, WID_EP_LIST);

				if (it == this->filtered_eng_list.end()) break;

				const auto &item = *it;
				if (!item.flags.Test(EngineDisplayFlag::Shaded)) this->selected_engine = item.engine_id;
				this->SetDirty();

				if (click_count <= 1) break;
				[[fallthrough]];
			}

			case WID_EP_YES:
				Command<CMD_WANT_ENGINE_PREVIEW>::Post(this->selected_engine);
				[[fallthrough]];

			case WID_EP_NO:
				if (_shift_pressed) return;
				for (auto it = this->eng_list.begin(); it != this->eng_list.end(); ++it) {
					if ((*it).engine_id == this->selected_engine) {
						this->eng_list.erase(it);
						break;
					}
				}
				if(this->eng_list.size() == 0) {
					this->Close();
				} else {
					this->FilterEngineList();
					if(this->filtered_eng_list.size() == 0) {
						this->selected_engine = this->eng_list[0].engine_id;
					} else {
						this->selected_engine = this->filtered_eng_list[0].engine_id;
					}
					this->SetDirty();
				}
				break;
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_EP_CAPTION:
				return GetString(STR_ENGINE_PREVIEW_CAPTION);

			 case WID_EP_SORT_DROPDOWN:
				 return GetString(std::data(_engine_sort_listing[this->veh_type_filter_criteria])[this->sort_criteria]);

			case WID_EP_VEH_TYPE_FILTER_DROPDOWN:
				return GetString(this->GetVehTypeFilterLabel(this->veh_type_filter_criteria));

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		if (this->selected_engine == EngineID::Invalid()) return;
		if (Engine::Get(this->selected_engine)->preview_company != _local_company) this->Close();
	}
};

/* Initialise static variables */
EnginePreviewWindow* EnginePreviewWindow::_current_instance = nullptr;

static WindowDesc _engine_preview_desc(
	WDP_CENTER, {}, 0, 0,
	WC_ENGINE_PREVIEW, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_engine_preview_widgets
);


void ShowEnginePreviewWindow(EngineID engine)
{
	if(EnginePreviewWindow::_current_instance == nullptr) {
		AllocateWindowDescFront<EnginePreviewWindow>(_engine_preview_desc, engine);
	} else {
		EnginePreviewWindow::_current_instance->AddNewOffert(engine);
		EnginePreviewWindow::_current_instance->SetDirty();
	}
}

/**
 * Get the capacity of an engine with articulated parts.
 * @param engine The engine to get the capacity of.
 * @return The capacity.
 */
uint GetTotalCapacityOfArticulatedParts(EngineID engine)
{
	CargoArray cap = GetCapacityOfArticulatedParts(engine);
	return cap.GetSum<uint>();
}

/**
 * Get preview running cost string for an engine.
 * @param e Engine.
 * @returns Formatted string of running cost.
 */
static std::string GetPreviewRunningCostString(const Engine &e)
{
	return GetString(TimerGameEconomy::UsingWallclockUnits() ? STR_ENGINE_PREVIEW_RUNCOST_PERIOD : STR_ENGINE_PREVIEW_RUNCOST_YEAR, e.GetRunningCost());
}

static std::string GetTrainEngineInfoString(const Engine &e)
{
	std::stringstream res;

	res << GetString(STR_ENGINE_PREVIEW_COST_WEIGHT, e.GetCost(), e.GetDisplayWeight());
	res << '\n';

	if (_settings_game.vehicle.train_acceleration_model != AM_ORIGINAL && GetRailTypeInfo(e.u.rail.railtype)->acceleration_type != 2) {
		res << GetString(STR_ENGINE_PREVIEW_SPEED_POWER_MAX_TE, PackVelocity(e.GetDisplayMaxSpeed(), e.type), e.GetPower(), e.GetDisplayMaxTractiveEffort());
		res << '\n';
	} else {
		res << GetString(STR_ENGINE_PREVIEW_SPEED_POWER, PackVelocity(e.GetDisplayMaxSpeed(), e.type), e.GetPower());
		res << '\n';
	}

	res << GetPreviewRunningCostString(e);
	res << '\n';

	uint capacity = GetTotalCapacityOfArticulatedParts(e.index);
	res << GetString(STR_ENGINE_PREVIEW_CAPACITY, capacity == 0 ? INVALID_CARGO : e.GetDefaultCargoType(), capacity);

	return res.str();
}

static std::string GetAircraftEngineInfoString(const Engine &e)
{
	std::stringstream res;

	res << GetString(STR_ENGINE_PREVIEW_COST_MAX_SPEED, e.GetCost(), PackVelocity(e.GetDisplayMaxSpeed(), e.type));
	res << '\n';

	if (uint16_t range = e.GetRange(); range > 0) {
		res << GetString(STR_ENGINE_PREVIEW_TYPE_RANGE, e.GetAircraftTypeText(), range);
		res << '\n';
	} else {
		res << GetString(STR_ENGINE_PREVIEW_TYPE, e.GetAircraftTypeText());
		res << '\n';
	}

	res << GetPreviewRunningCostString(e);
	res << '\n';

	CargoType cargo = e.GetDefaultCargoType();
	uint16_t mail_capacity;
	uint capacity = e.GetDisplayDefaultCapacity(&mail_capacity);
	if (mail_capacity > 0) {
		res << GetString(STR_ENGINE_PREVIEW_CAPACITY_2, cargo, capacity, GetCargoTypeByLabel(CT_MAIL), mail_capacity);
	} else {
		res << GetString(STR_ENGINE_PREVIEW_CAPACITY, cargo, capacity);
	}

	return res.str();
}

static std::string GetRoadVehEngineInfoString(const Engine &e)
{
	std::stringstream res;

	if (_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL) {
		res << GetString(STR_ENGINE_PREVIEW_COST_MAX_SPEED, e.GetCost(), PackVelocity(e.GetDisplayMaxSpeed(), e.type));
		res << '\n';
	} else {
		res << GetString(STR_ENGINE_PREVIEW_COST_WEIGHT, e.GetCost(), e.GetDisplayWeight());
		res << '\n';
		res << GetString(STR_ENGINE_PREVIEW_SPEED_POWER_MAX_TE, PackVelocity(e.GetDisplayMaxSpeed(), e.type), e.GetPower(), e.GetDisplayMaxTractiveEffort());
		res << '\n';
	}

	res << GetPreviewRunningCostString(e);
	res << '\n';

	uint capacity = GetTotalCapacityOfArticulatedParts(e.index);
	res << GetString(STR_ENGINE_PREVIEW_CAPACITY, capacity == 0 ? INVALID_CARGO : e.GetDefaultCargoType(), capacity);

	return res.str();
}

static std::string GetShipEngineInfoString(const Engine &e)
{
	std::stringstream res;

	res << GetString(STR_ENGINE_PREVIEW_COST_MAX_SPEED, e.GetCost(), PackVelocity(e.GetDisplayMaxSpeed(), e.type));
	res << '\n';

	res << GetPreviewRunningCostString(e);
	res << '\n';

	res << GetString(STR_ENGINE_PREVIEW_CAPACITY, e.GetDefaultCargoType(), e.GetDisplayDefaultCapacity());

	return res.str();
}


/**
 * Get a multi-line string with some technical data, describing the engine.
 * @param engine Engine to describe.
 * @return String describing the engine.
 * @post \c DParam array is set up for printing the string.
 */
std::string GetEngineInfoString(EngineID engine)
{
	const Engine &e = *Engine::Get(engine);

	switch (e.type) {
		case VEH_TRAIN:
			return GetTrainEngineInfoString(e);

		case VEH_ROAD:
			return GetRoadVehEngineInfoString(e);

		case VEH_SHIP:
			return GetShipEngineInfoString(e);

		case VEH_AIRCRAFT:
			return GetAircraftEngineInfoString(e);

		default: NOT_REACHED();
	}
}

/**
 * Draw an engine.
 * @param left   Minimum horizontal position to use for drawing the engine
 * @param right  Maximum horizontal position to use for drawing the engine
 * @param preferred_x Horizontal position to use for drawing the engine.
 * @param y      Vertical position to use for drawing the engine.
 * @param engine Engine to draw.
 * @param pal    Palette to use for drawing.
 */
void DrawVehicleEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal, EngineImageType image_type)
{
	const Engine *e = Engine::Get(engine);

	switch (e->type) {
		case VEH_TRAIN:
			DrawTrainEngine(left, right, preferred_x, y, engine, pal, image_type);
			break;

		case VEH_ROAD:
			DrawRoadVehEngine(left, right, preferred_x, y, engine, pal, image_type);
			break;

		case VEH_SHIP:
			DrawShipEngine(left, right, preferred_x, y, engine, pal, image_type);
			break;

		case VEH_AIRCRAFT:
			DrawAircraftEngine(left, right, preferred_x, y, engine, pal, image_type);
			break;

		default: NOT_REACHED();
	}
}

/**
 * Sort all items using quick sort and given 'CompareItems' function
 * @param el list to be sorted
 * @param compare function for evaluation of the quicksort
 */
void EngList_Sort(GUIEngineList &el, EngList_SortTypeFunction compare)
{
	if (el.size() < 2) return;
	std::sort(el.begin(), el.end(), compare);
}

/**
 * Sort selected range of items (on indices @ <begin, begin+num_items-1>)
 * @param el list to be sorted
 * @param compare function for evaluation of the quicksort
 * @param begin start of sorting
 * @param num_items count of items to be sorted
 */
void EngList_SortPartial(GUIEngineList &el, EngList_SortTypeFunction compare, size_t begin, size_t num_items)
{
	if (num_items < 2) return;
	assert(begin < el.size());
	assert(begin + num_items <= el.size());
	std::sort(el.begin() + begin, el.begin() + begin + num_items, compare);
}

