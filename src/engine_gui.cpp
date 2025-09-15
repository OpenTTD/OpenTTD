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
#include "stringfilter_type.h"
#include "hotkeys.h"
#include "querystring_gui.h"

#include "core/geometry_func.hpp"

#include "widgets/engine_widget.h"

#include "table/strings.h"

#include "safeguards.h"

#include "dropdown_common_type.h"

using DropDownListMoverItem = DropDownMover<DropDownString<DropDownListItem>>;

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
			assert(e->u.rail.railtypes.Any());
			return GetRailTypeInfo(e->u.rail.railtypes.GetNthSetBit(0).value())->strings.new_loco;
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
	/* Accept and decline buttons, resize button. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_EP_YES), SetStringTip(STR_QUIT_YES), SetResize(1, 0), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_EP_NO), SetStringTip(STR_QUIT_NO), SetResize(1, 0), SetFill(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_LIGHT_BLUE),
	EndContainer(),
};

static const int BADGE_NEXT_VEH_TYPE = 0xF;
static const int BADGE_PREVIOUS_VEH_TYPE = 0xE;
static const int BADGE_VEH_TYPE_WIDGET_INDEX = -2;

struct EnginePreviewWindow : Window {
	static EnginePreviewWindow *_current_instance;

	int vehicle_space = 0; // The space to show the vehicle image
	GUIEngineList eng_list{}; ///< List of engines offered for testing
	GUIEngineList filtered_eng_list{}; ///< List of currently visible engines
	EngineID selected_engine = EngineID::Invalid(); ///< Currently selected engine, or #EngineID::Invalid()
	Scrollbar *vscroll = nullptr;
	VehicleType vehicle_type = VEH_ANY; ///< Type of vehicles currently shown in the window.
	VehicleType veh_type_filter_criteria = VEH_ANY; ///< Selected vehicle type filter
	bool descending_sort_order = false; ///< Sort direction, @see _engine_sort_direction
	uint8_t sort_criteria = 0; ///< Current sort criterium.

	StringFilter string_filter{}; ///< Filter for vehicle name
	QueryString vehicle_editbox; ///< Filter editbox

	std::array<GUIBadgeClasses, VEH_COMPANY_END> badge_classes{}; ///< Individual object for each vehicle type.
	VehicleType badge_veh_type = VEH_BEGIN; ///< Vehicle type of currently shown badge configuration dropdown.
	std::pair<WidgetID, WidgetID> badge_filters{}; ///< First and last widgets IDs of badge filters.
	BadgeFilterChoices badge_filter_choices{};

	EnginePreviewWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc), vehicle_editbox(MAX_LENGTH_VEHICLE_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_VEHICLE_NAME_CHARS)
	{
		this->UpdateWindowsVehicleType();
		this->InitNested(window_number);

		this->vscroll = this->GetScrollbar(WID_EP_SCROLLBAR);
		assert(this->vscroll != nullptr);

		this->querystrings[WID_EP_FILTER] = &this->vehicle_editbox;
		this->vehicle_editbox.cancel_button = QueryString::ACTION_CLEAR;

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

	void OnInit() override
	{
		this->badge_classes[VEH_TRAIN] = GUIBadgeClasses(static_cast<GrfSpecFeature>(GSF_TRAINS));
		this->badge_classes[VEH_ROAD] = GUIBadgeClasses(static_cast<GrfSpecFeature>(GSF_ROADVEHICLES));
		this->badge_classes[VEH_SHIP] = GUIBadgeClasses(static_cast<GrfSpecFeature>(GSF_SHIPS));
		this->badge_classes[VEH_AIRCRAFT] = GUIBadgeClasses(static_cast<GrfSpecFeature>(GSF_AIRCRAFT));

		if (this->vscroll != nullptr) this->FilterEngineList();

		auto container = this->GetWidget<NWidgetContainer>(WID_EP_BADGE_FILTER);

		if (this->veh_type_filter_criteria == VEH_ANY) {
			this->badge_filters = AddBadgeDropdownFilters(*container, WID_EP_BADGE_FILTER, COLOUR_LIGHT_BLUE, GSF_TRAINS);
			for (GrfSpecFeature feature : {GSF_ROADVEHICLES, GSF_SHIPS, GSF_AIRCRAFT}) {
				std::pair<WidgetID, WidgetID> tmp = AddBadgeDropdownFilters(*container, this->badge_filters.second, COLOUR_LIGHT_BLUE, feature, false);
				this->badge_filters.first = std::min(this->badge_filters.first, tmp.first);
				this->badge_filters.second = std::max(this->badge_filters.second, tmp.second);
			}
		} else {
			this->badge_filters = AddBadgeDropdownFilters(*container, WID_EP_BADGE_FILTER, COLOUR_LIGHT_BLUE, static_cast<GrfSpecFeature>(GSF_TRAINS + this->veh_type_filter_criteria));
		}

		this->widget_lookup.clear();
		this->nested_root->FillWidgetLookup(this->widget_lookup);
	}

	/**
	 * Converts VehicleType into a corresponding filter label.
	 * @param vehicle_type type of vehicle to convert into the label.
	 * @returns ID of a string containing the filter label.
	 */
	StringID GetVehTypeFilterLabel(VehicleType vehicle_type) const
	{
		switch (vehicle_type) {
			case VEH_ANY: return STR_ENGINE_PREVIEW_ALL_TYPES;
			case VEH_TRAIN: return STR_ENGINE_PREVIEW_TRAIN_ONLY;
			case VEH_ROAD: return STR_ENGINE_PREVIEW_ROAD_VEHICLE_ONLY;
			case VEH_SHIP: return STR_ENGINE_PREVIEW_SHIP_ONLY;
			case VEH_AIRCRAFT: return STR_ENGINE_PREVIEW_AIRCRAFT_ONLY;
			default: NOT_REACHED();
		}
	}

	/**
	 * Builds widgets for vehicle type filter dropdown.
	 * @returns list containing widgets for vehicle type filter dropdown.
	 */
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

	/**
	 * Builds widgets for badge configuration dropdown.
	 * @returns list containing widgets for badge configuration dropdown.
	 */
	DropDownList BuildBadgeConfigurationList()
	{
		static const auto separators = {STR_BADGE_CONFIG_PREVIEW, STR_BADGE_CONFIG_NAME};
		if (this->veh_type_filter_criteria == VEH_ANY) {
			DropDownList out = BuildBadgeClassConfigurationList(this->badge_classes[this->badge_veh_type], BADGE_COLUMNS, separators);
			out.push_back(MakeDropDownListDividerItem());
			out.push_back(std::make_unique<DropDownListMoverItem>(BADGE_NEXT_VEH_TYPE, BADGE_PREVIOUS_VEH_TYPE, COLOUR_YELLOW, GetString(this->GetVehTypeFilterLabel(this->badge_veh_type)), BADGE_VEH_TYPE_WIDGET_INDEX));
			return out;
		}
		return BuildBadgeClassConfigurationList(this->badge_classes[this->veh_type_filter_criteria], BADGE_COLUMNS, separators);
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
				size.height += GetCharacterHeight(FS_NORMAL); ///< Space for NewGRF name
				break;
			}

			case WID_EP_LIST: {
				VehicleCellSize tcs = GetVehicleImageCellSize(VEH_TRAIN, EIT_PURCHASE);
				VehicleCellSize rcs = GetVehicleImageCellSize(VEH_ROAD, EIT_PURCHASE);
				VehicleCellSize scs = GetVehicleImageCellSize(VEH_SHIP, EIT_PURCHASE);
				VehicleCellSize acs = GetVehicleImageCellSize(VEH_AIRCRAFT, EIT_PURCHASE);

				fill.height = resize.height = GetEngineListHeight(this->vehicle_type);
				size.height = 3 * resize.height;
				auto badges_width = std::max({this->badge_classes[VEH_TRAIN].GetTotalColumnsWidth(), this->badge_classes[VEH_ROAD].GetTotalColumnsWidth(), this->badge_classes[VEH_SHIP].GetTotalColumnsWidth(), this->badge_classes[VEH_AIRCRAFT].GetTotalColumnsWidth()});
				size.width = std::max(size.width, badges_width + std::max({tcs.extend_left, rcs.extend_left, scs.extend_left, acs.extend_left}) + std::max({tcs.extend_right, rcs.extend_right, scs.extend_right, acs.extend_right}) + 165) + padding.width;
				break;
			}

			case WID_EP_VEH_TYPE_FILTER_DROPDOWN:
				size.width = std::max(size.width, GetDropDownListDimension(this->BuildVehTypeDropDownList()).width + padding.width);
				break;

			case WID_EP_CONFIGURE_BADGES:
				/* Hide the configuration button if no configurable badges are present. */
				if (this->veh_type_filter_criteria == VEH_ANY) {
					for (VehicleType veh_type : {VEH_TRAIN, VEH_ROAD, VEH_SHIP, VEH_AIRCRAFT}) {
						if (!this->badge_classes[veh_type].GetClasses().empty()) return;
					}
				} else {
					if (!this->badge_classes[this->veh_type_filter_criteria].GetClasses().empty()) return;
				}
				size = {0, 0};
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

	/** Filter by name and NewGRF extra text */
	bool FilterByText(const Engine *e)
	{
		/* Do not filter if the filter text box is empty */
		if (this->string_filter.IsEmpty()) return true;

		/* Filter engine name */
		this->string_filter.ResetState();
		this->string_filter.AddLine(GetString(STR_ENGINE_NAME, PackEngineNameDParam(e->index, EngineNameContext::PurchaseList)));

		/* Filter NewGRF extra text */
		auto text = GetNewGRFAdditionalText(e->index);
		if (text) this->string_filter.AddLine(*text);

		return this->string_filter.GetState();
	}

	/** Check if conversion from VehicleType to GrfSpecFeature can be done with addition */
	static_assert((GSF_TRAINS + VEH_TRAIN) == GSF_TRAINS);
	static_assert((GSF_TRAINS + VEH_ROAD) == GSF_ROADVEHICLES);
	static_assert((GSF_TRAINS + VEH_SHIP) == GSF_SHIPS);
	static_assert((GSF_TRAINS + VEH_AIRCRAFT) == GSF_AIRCRAFT);

	/**
	 * Determines which engines should be currently visible.
	 * Stores the output in the filtered_eng_list.
	 */
	void FilterEngineList()
	{
		this->filtered_eng_list.clear();

		BadgeDropdownFilter bdf(this->badge_filter_choices);

		for (auto it = this->eng_list.begin(); it != this->eng_list.end(); ++it) {
			const Engine *e = Engine::Get((*it).engine_id);

			if (this->veh_type_filter_criteria != VEH_ANY) {
				if(e->type != this->veh_type_filter_criteria) continue;
			}
			if (!bdf.Filter(e->badges)) continue;

			/* Filter by name or NewGRF extra text */
			BadgeTextFilter btf(this->string_filter, GSF_TRAINS+e->type);
			if(!FilterByText(e) && !btf.Filter(e->badges)) continue;

			this->filtered_eng_list.push_back((*it));
		}

		_engine_sort_direction = this->descending_sort_order;
		EngList_Sort(this->filtered_eng_list, _engine_sort_functions[this->veh_type_filter_criteria][this->sort_criteria]);

		this->vscroll->SetCount(this->filtered_eng_list.size());
	}

	/** Udates capacity of engine list scrollbar. */
	void UpdateScrollCapacity()
	{
		this->vscroll->SetCapacityFromWidget(this, WID_EP_LIST);
	}

	/**
	 * Appends an engine offered for a preview to the end of the engine list.
	 * @param engine ID of the engine offered for the preview.
	 */
	void AddNewOffert(EngineID &engine)
	{
		assert(this->vscroll != nullptr);

		const Engine *e = Engine::Get(engine);
		EngineDisplayFlags flags;

		/* Ignore folding and variants. */
		if(e->display_flags.Test(EngineDisplayFlag::Shaded)) flags.Set(EngineDisplayFlag::Shaded);
		this->eng_list.emplace_back(engine, e->info.variant_id, flags, 0);

		this->UpdateScrollCapacity();
		this->ReInit();
		this->GetWidget<NWidgetLeaf>(WID_EP_CONFIGURE_BADGES)->SetupSmallestSize(this);
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

				/* The NewGRF's name which the vehicle comes from */
				const Engine *e = Engine::Get(this->selected_engine);
				const GRFConfig *config = GetGRFConfig(e->GetGRFID());
				if (_settings_client.gui.show_newgrf_name && config != nullptr) {
					DrawString(r.left, r.right, y, config->GetName(), TC_YELLOW, SA_HOR_CENTER);
					y += GetCharacterHeight(FS_NORMAL);
				}

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
					{},
					&this->badge_classes
				);
				break;

			case WID_EP_SORT_ASCENDING_DESCENDING:
				this->DrawSortButtonState(WID_EP_SORT_ASCENDING_DESCENDING, this->descending_sort_order ? SBS_DOWN : SBS_UP);
				break;
		}
	}

	/** Determines which type of vehicles is currently being shown in the window. */
	void UpdateWindowsVehicleType()
	{
		if (this->veh_type_filter_criteria != VEH_ANY) {
			/* Only vehicles of one type are visible */
			this->vehicle_type = this->veh_type_filter_criteria;
			return;
		}

		/**
		 * Vehicles of multiple types are visible.
		 * Choose type with the biggest row height,
		 * as draw engine list function does not support multiple types.
		 */

		uint max_h = GetEngineListHeight(VEH_BEGIN);
		this->vehicle_type = VEH_BEGIN;
		VehicleType next_type = VEH_BEGIN;
		++next_type;
		while (next_type < VEH_COMPANY_END) {
			if (max_h < GetEngineListHeight(next_type)) {
				max_h = GetEngineListHeight(next_type);
				this->vehicle_type = next_type;
			}
			++next_type;
		}
	}

	void OnDropdownSelect(WidgetID widget, int index, int click_result) override
	{
		switch (widget) {
			case WID_EP_SORT_DROPDOWN:
				if (this->sort_criteria != index) {
					this->sort_criteria = index;
					_engine_sort_last_criteria[this->veh_type_filter_criteria] = this->sort_criteria;
					this->FilterEngineList();
				}
				break;

			case WID_EP_VEH_TYPE_FILTER_DROPDOWN: // Select a vehicle type filter criteria
				if (static_cast<int>(this->veh_type_filter_criteria) != index) {
					this->veh_type_filter_criteria = static_cast<VehicleType>(index);
					this->sort_criteria = _engine_sort_last_criteria[this->veh_type_filter_criteria];
					this->descending_sort_order = _engine_sort_last_order[this->veh_type_filter_criteria];
					this->UpdateWindowsVehicleType();
					this->GetWidget<NWidgetLeaf>(WID_EP_LIST)->SetupSmallestSize(this);
					this->ReInit();
					this->FilterEngineList();
				}
				break;

			case WID_EP_CONFIGURE_BADGES: {
				bool reopen = true;

				if (index == BADGE_VEH_TYPE_WIDGET_INDEX && click_result == BADGE_PREVIOUS_VEH_TYPE) {
					if (this->badge_veh_type == VEH_BEGIN) this->badge_veh_type = VEH_COMPANY_END;
					this->badge_veh_type--;
				} else if (index == BADGE_VEH_TYPE_WIDGET_INDEX && click_result == BADGE_NEXT_VEH_TYPE) {
					this->badge_veh_type++;
					if (this->badge_veh_type == VEH_COMPANY_END) this->badge_veh_type = VEH_BEGIN;
				} else {
					reopen = HandleBadgeConfigurationDropDownClick(static_cast<GrfSpecFeature>(GSF_TRAINS + this->badge_veh_type), BADGE_COLUMNS, index, click_result, this->badge_filter_choices);
				}

				this->ReInit();

				if (reopen) {
					ReplaceDropDownList(this, this->BuildBadgeConfigurationList(), -1);
				} else {
					this->CloseChildWindows(WC_DROPDOWN_MENU);
				}

				/* We need to refresh if a filter is removed. */
				this->FilterEngineList();
				break;
			}

			default:
				if (IsInsideMM(widget, this->badge_filters.first, this->badge_filters.second)) {
					if (index < 0) {
						ResetBadgeFilter(this->badge_filter_choices, this->GetWidget<NWidgetBadgeFilter>(widget)->GetBadgeClassID());
					} else {
						SetBadgeFilter(this->badge_filter_choices, BadgeID(index));
					}
					this->FilterEngineList();
				}
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
			case WID_EP_VEH_TYPE_FILTER_DROPDOWN: // Select vehicle type filtering criteria dropdown menu
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

			case WID_EP_CONFIGURE_BADGES:
				if (this->veh_type_filter_criteria == VEH_ANY) {
					for (VehicleType veh_type : {VEH_TRAIN, VEH_ROAD, VEH_SHIP, VEH_AIRCRAFT}) {
						if (!this->badge_classes[veh_type].GetClasses().empty()) {
							ShowDropDownList(this, this->BuildBadgeConfigurationList(), -1, widget, 0, false, true);
							return;
						}
					}
				} else {
					if (this->badge_classes[this->veh_type_filter_criteria].GetClasses().empty()) return;
				}
				ShowDropDownList(this, this->BuildBadgeConfigurationList(), -1, widget, 0, false, true);
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

			default:
				if (IsInsideMM(widget, this->badge_filters.first, this->badge_filters.second)) {
					ShowDropDownList(this, this->GetWidget<NWidgetBadgeFilter>(widget)->GetDropDownList(), -1, widget, 0, false);
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
				if (IsInsideMM(widget, this->badge_filters.first, this->badge_filters.second)) {
					return this->GetWidget<NWidgetBadgeFilter>(widget)->GetStringParameter(this->badge_filter_choices);
				}

				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		if (this->selected_engine == EngineID::Invalid()) return;
		if (Engine::Get(this->selected_engine)->preview_company != _local_company) this->Close();
	}

	void OnEditboxChanged(WidgetID wid) override
	{
		if (wid == WID_EP_FILTER) {
			this->string_filter.SetFilterTerm(this->vehicle_editbox.text.GetText());
			this->FilterEngineList();
			this->SetDirty();
		}
	}

	static inline HotkeyList hotkeys{"enginepreview", {
		Hotkey('F', "focus_filter_box", WID_EP_FILTER),
	}};
};

/* Initialise static variables */
EnginePreviewWindow* EnginePreviewWindow::_current_instance = nullptr;

static WindowDesc _engine_preview_desc(
	WDP_CENTER, {}, 0, 0,
	WC_ENGINE_PREVIEW, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_engine_preview_widgets,
	&EnginePreviewWindow::hotkeys
);

/**
 * Shows new EnginePreviewWindow if none is visible.
 * Otherwise, adds a new offer to the existing window.
 * @param engine The engine offered for testing.
 */
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

	if (e.u.rail.railtypes.Count() > 1) {
		std::string railtypes{};
		std::string_view list_separator = GetListSeparator();

		for (const auto &rt : _sorted_railtypes) {
			if (!e.u.rail.railtypes.Test(rt)) continue;

			if (!railtypes.empty()) railtypes += list_separator;
			AppendStringInPlace(railtypes, GetRailTypeInfo(rt)->strings.name);
		}
		res << GetString(STR_ENGINE_PREVIEW_RAILTYPES, railtypes);
		res << '\n';
	}

	bool is_maglev = true;
	for (RailType rt : e.u.rail.railtypes) {
		is_maglev &= GetRailTypeInfo(rt)->acceleration_type == VehicleAccelerationModel::Maglev;
	}

	if (_settings_game.vehicle.train_acceleration_model != AM_ORIGINAL && !is_maglev) {
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

