/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file type_selection_gui.cpp GUI for selecting rail, road or tram types. */

#include "stdafx.h"
#include "rail.h"
#include "road.h"
#include "rail_gui.h"
#include "network/network.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "company_func.h"
#include "newgrf_badge.h"
#include "newgrf_badge_config.h"
#include "newgrf_badge_gui.h"
#include "newgrf_text.h"
#include "group.h"
#include "string_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "timer/timer_game_calendar.h"
#include "dropdown_type.h"
#include "dropdown_func.h"
#include "core/geometry_func.hpp"
#include "zoom_func.h"
#include "querystring_gui.h"
#include "stringfilter_type.h"
#include "hotkeys.h"
#include "widget_type.h"
#include "sortlist_type.h"
#include "newgrf_config.h"
#include "company_base.h"
#include "company_cmd.h"
#include "rail_cmd.h"
#include "road_func.h"
#include "toolbar_gui.h"

#include "widgets/type_selection_widget.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Get the size of a track image in the tack lists.
 * @return the size of the image
 */
static Dimension GetTrackImageCellSize() {
	RailTypes used_railtypes = GetRailTypes(true);
	Dimension d = { 0, 0 };

	/* Get largest icon size, to ensure text is aligned on each menu item. */
	used_railtypes.Reset(_railtypes_hidden_mask);
	for (const auto &rt : _sorted_railtypes) {
		if (!used_railtypes.Test(rt)) continue;
		const RailTypeInfo *rti = GetRailTypeInfo(rt);
		d = maxdim(d, GetSpriteSize(rti->gui_sprites.build_x_rail));
	}

	return d;
}

/**
 * Get the height of a single 'entry' in the track lists.
 * @return the height for the entry
 */
uint GetTrackListHeight()
{
	return std::max<uint>(GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.matrix.Vertical(), GetTrackImageCellSize().height);
}

static constexpr NWidgetPart _nested_type_selection_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_TS_CAPTION), SetTextStyle(TC_WHITE),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_VERTICAL),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_GREEN, WID_TS_SORT_ASCENDING_DESCENDING), SetStringTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
			NWidget(WWT_DROPDOWN, COLOUR_DARK_GREEN, WID_TS_SORT_DROPDOWN), SetResize(1, 0), SetFill(1, 0), SetToolTip(STR_TOOLTIP_SORT_CRITERIA),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
				NWidget(WWT_EDITBOX, COLOUR_DARK_GREEN, WID_TS_FILTER), SetResize(1, 0), SetFill(1, 0), SetPadding(2), SetStringTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
			EndContainer(),
			NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_TS_CONFIGURE_BADGES), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON), SetResize(0, 0), SetFill(0, 1), SetSpriteTip(SPR_EXTRA_MENU, STR_BADGE_CONFIG_MENU_TOOLTIP),
		EndContainer(),
		NWidget(NWID_VERTICAL, NWidContainerFlag{}, WID_TS_BADGE_FILTER),
		EndContainer(),
	EndContainer(),
	/* Vehicle list. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_DARK_GREEN, WID_TS_LIST), SetResize(1, 1), SetFill(1, 0), SetMatrixDataTip(1, 0), SetScrollbar(WID_TS_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_TS_SCROLLBAR),
	EndContainer(),
	/* Panel with details. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_TS_PANEL), SetMinimalSize(240, 122), SetResize(1, 0), EndContainer(),
	/* Resize button. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetResize(1, 0), SetFill(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),
};

bool _select_type_sort_direction = false; ///< \c false = descending, \c true = ascending.
uint8_t _select_type_sort_last_criteria = 0; ///< Last set sort criteria.
bool _select_type_sort_last_order = false; ///< Last set direction of the sort order.

typedef bool TypeList_SortTypeFunction(const uint8_t&, const uint8_t&);

/**
 * Determines order of rail_types by sorting order
 * @param a first rail_type to compare
 * @param b second rail_type to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool RailTypeNumberSorter(const uint8_t &a, const uint8_t &b)
{
	int r = GetRailTypeInfo((RailType)a)->sorting_order - GetRailTypeInfo((RailType)b)->sorting_order;

	return _select_type_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of rail_types by sorting order
 * @param a first rail_type to compare
 * @param b second rail_type to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool RailTypeCostSorter(const uint8_t &a, const uint8_t &b)
{
	int r = GetRailTypeInfo((RailType)a)->cost_multiplier - GetRailTypeInfo((RailType)b)->cost_multiplier;

	/* Use sorting_order to sort instead since we want consistent sorting */
	if (r == 0) return RailTypeNumberSorter(a, b);
	return _select_type_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of rail_types by sorting order
 * @param a first rail_type to compare
 * @param b second rail_type to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool RailTypeSpeedSorter(const uint8_t &a, const uint8_t &b)
{
	int r = GetRailTypeInfo((RailType)a)->max_speed - GetRailTypeInfo((RailType)b)->max_speed;

	/* Use sorting_order to sort instead since we want consistent sorting */
	if (r == 0) return RailTypeNumberSorter(a, b);
	return _select_type_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of rail_types by sorting order
 * @param a first rail_type to compare
 * @param b second rail_type to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool RailTypeMaintenanceCostSorter(const uint8_t &a, const uint8_t &b)
{
	int r = GetRailTypeInfo((RailType)a)->maintenance_multiplier - GetRailTypeInfo((RailType)b)->maintenance_multiplier;

	/* Use sorting_order to sort instead since we want consistent sorting */
	if (r == 0) return RailTypeNumberSorter(a, b);
	return _select_type_sort_direction ? r > 0 : r < 0;
}

/* cached values for RailTypeNameSorter to spare many GetString() calls */
static uint8_t _last_rail_type[2] = { INVALID_RAILTYPE, INVALID_RAILTYPE };

/**
 * Determines order of rail_types by sorting order
 * @param a first rail_type to compare
 * @param b second rail_type to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool RailTypeNameSorter(const uint8_t &a, const uint8_t &b)
{
	static std::string last_name[2] = { {}, {} };

	if (a != _last_rail_type[0]) {
		_last_rail_type[0] = a;
		last_name[0] = GetString(GetRailTypeInfo((RailType)a)->strings.menu_text);
	}

	if (b != _last_rail_type[1]) {
		_last_rail_type[1] = b;
		last_name[1] = GetString(GetRailTypeInfo((RailType)b)->strings.menu_text);
	}

	int r = StrNaturalCompare(last_name[0], last_name[1]); // Sort by name (natural sorting).

	/* Use sorting_order to sort instead since we want consistent sorting */
	if (r == 0) return RailTypeNumberSorter(a, b);
	return _select_type_sort_direction ? r > 0 : r < 0;
}

/** Sort functions for the rail_type sort criteria. */
TypeList_SortTypeFunction * const _rail_types_sort_functions[] = {
	RailTypeNumberSorter,
	RailTypeCostSorter,
	RailTypeSpeedSorter,
	RailTypeMaintenanceCostSorter,
	RailTypeNameSorter,
};

/** Dropdown menu strings for the rail_type sort criteria. */
const std::initializer_list<const StringID> _rail_type_sort_listing = {
	STR_SORT_BY_RAIL_TYPE_ID,
	STR_SORT_BY_COST,
	STR_SORT_BY_MAX_SPEED,
	STR_SORT_BY_MAINTENANCE_COST,
	STR_SORT_BY_NAME,
};

/**
 * Determines order of road_types by sorting order
 * @param a first road_type to compare
 * @param b second road_type to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool RoadTypeNumberSorter(const uint8_t &a, const uint8_t &b)
{
	int r = GetRoadTypeInfo((RoadType)a)->sorting_order - GetRoadTypeInfo((RoadType)b)->sorting_order;

	return _select_type_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of road_types by sorting order
 * @param a first road_type to compare
 * @param b second road_type to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool RoadTypeCostSorter(const uint8_t &a, const uint8_t &b)
{
	int r = GetRoadTypeInfo((RoadType)a)->cost_multiplier - GetRoadTypeInfo((RoadType)b)->cost_multiplier;

	/* Use sorting_order to sort instead since we want consistent sorting */
	if (r == 0) return RoadTypeNumberSorter(a, b);
	return _select_type_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of road_types by sorting order
 * @param a first road_type to compare
 * @param b second road_type to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool RoadTypeSpeedSorter(const uint8_t &a, const uint8_t &b)
{
	int r = GetRoadTypeInfo((RoadType)a)->max_speed - GetRoadTypeInfo((RoadType)b)->max_speed;

	/* Use sorting_order to sort instead since we want consistent sorting */
	if (r == 0) return RoadTypeNumberSorter(a, b);
	return _select_type_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of road_types by sorting order
 * @param a first road_type to compare
 * @param b second road_type to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool RoadTypeMaintenanceCostSorter(const uint8_t &a, const uint8_t &b)
{
	int r = GetRoadTypeInfo((RoadType)a)->maintenance_multiplier - GetRoadTypeInfo((RoadType)b)->maintenance_multiplier;

	/* Use sorting_order to sort instead since we want consistent sorting */
	if (r == 0) return RoadTypeNumberSorter(a, b);
	return _select_type_sort_direction ? r > 0 : r < 0;
}

/* cached values for RoadTypeNameSorter to spare many GetString() calls */
static uint8_t _last_road_type[2] = { INVALID_ROADTYPE, INVALID_ROADTYPE };

/**
 * Determines order of road_types by sorting order
 * @param a first road_type to compare
 * @param b second road_type to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool RoadTypeNameSorter(const uint8_t &a, const uint8_t &b)
{
	static std::string last_name[2] = { {}, {} };

	if (a != _last_road_type[0]) {
		_last_road_type[0] = a;
		last_name[0] = GetString(GetRoadTypeInfo((RoadType)a)->strings.menu_text);
	}

	if (b != _last_road_type[1]) {
		_last_road_type[1] = b;
		last_name[1] = GetString(GetRoadTypeInfo((RoadType)b)->strings.menu_text);
	}

	int r = StrNaturalCompare(last_name[0], last_name[1]); // Sort by name (natural sorting).

	/* Use sorting_order to sort instead since we want consistent sorting */
	if (r == 0) return RoadTypeNumberSorter(a, b);
	return _select_type_sort_direction ? r > 0 : r < 0;
}

/** Sort functions for the road_type sort criteria. */
TypeList_SortTypeFunction * const _road_types_sort_functions[] = {
	RoadTypeNumberSorter,
	RoadTypeCostSorter,
	RoadTypeSpeedSorter,
	RoadTypeMaintenanceCostSorter,
	RoadTypeNameSorter,
};

/** Dropdown menu strings for the road_type sort criteria. */
const std::initializer_list<const StringID> _road_type_sort_listing = {
	STR_SORT_BY_ROAD_TYPE_ID,
	STR_SORT_BY_COST,
	STR_SORT_BY_MAX_SPEED,
	STR_SORT_BY_MAINTENANCE_COST,
	STR_SORT_BY_NAME,
};

/**
 * Draw the details of a rail type at a given location.
 * @param left,right,y location where to draw the info
 * @param type the rail type of which to draw the info of
 * @return y after drawing all the text
 */
int DrawRailTypeInfo(int left, int right, int y, RailType type)
{
	const RailTypeInfo *rti = GetRailTypeInfo(type);

	DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST, RailBuildCost(type)));
	y += GetCharacterHeight(FS_NORMAL);

	if (rti->max_speed != 0) {
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_SPEED, PackVelocity(rti->max_speed, VEH_TRAIN)));
		y += GetCharacterHeight(FS_NORMAL);
	}

	DrawString(left, right, y, GetString(TimerGameEconomy::UsingWallclockUnits() ? STR_PURCHASE_INFO_MAINTENANCE_PERIOD : STR_PURCHASE_INFO_MAINTENANCE_YEAR, RailMaintenanceCost(type, 1, 1)));
	y += GetCharacterHeight(FS_NORMAL);

	y = DrawBadgeNameList({left, y, right, INT16_MAX}, rti->badges, GSF_RAILTYPES);

	/* The NewGRF's name which the track comes from */
	const GRFFile *const *file = rti->grffile;
	const GRFConfig *config = GetGRFConfig(file == nullptr ? 0 : (*file) == nullptr ? 0 : (*file)->grfid);
	if (_settings_client.gui.show_newgrf_name && config != nullptr)
	{
		DrawString(left, right, y, config->GetName(), TC_BLACK);
		y += GetCharacterHeight(FS_NORMAL);
	}

	return y;
}

/**
 * Draw the details of a road type at a given location.
 * @param left,right,y location where to draw the info
 * @param type the road type of which to draw the info of
 * @return y after drawing all the text
 */
int DrawRoadTypeInfo(int left, int right, int y, RoadType type)
{
	const RoadTypeInfo *rti = GetRoadTypeInfo(type);

	DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST, RoadBuildCost(type)));
	y += GetCharacterHeight(FS_NORMAL);

	if (rti->max_speed != 0) {
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_SPEED, PackVelocity(rti->max_speed, VEH_TRAIN)));
		y += GetCharacterHeight(FS_NORMAL);
	}

	DrawString(left, right, y, GetString(TimerGameEconomy::UsingWallclockUnits() ? STR_PURCHASE_INFO_MAINTENANCE_PERIOD : STR_PURCHASE_INFO_MAINTENANCE_YEAR, RoadMaintenanceCost(type, 1, 1)));
	y += GetCharacterHeight(FS_NORMAL);

	y = DrawBadgeNameList({left, y, right, INT16_MAX}, rti->badges, GSF_ROADTYPES);

	/* The NewGRF's name which the track comes from */
	const GRFFile *const *file = rti->grffile;
	const GRFConfig *config = GetGRFConfig(file == nullptr ? 0 : (*file) == nullptr ? 0 : (*file)->grfid);
	if (_settings_client.gui.show_newgrf_name && config != nullptr)
	{
		DrawString(left, right, y, config->GetName(), TC_BLACK);
		y += GetCharacterHeight(FS_NORMAL);
	}

	return y;
}


static void DrawTypeBadgeColumn(const Rect &r, int column_group, const GUIBadgeClasses &badge_classes, const std::vector<BadgeID> &badges, TimerGameCalendar::Date introduction_date, PaletteID remap)
{
	DrawBadgeColumn(r, column_group, badge_classes, badges, static_cast<GrfSpecFeature>(GSF_RAILTYPES), introduction_date, remap);
}

/**
 * Rail, road or tram type drawing loop
 * @param r The Rect of the list
 * @param type_list What types to draw
 * @param sb Scrollbar of list.
 * @param selected_id what types to highlight as selected, if any
 * @param badge_classes list of badges to draw.
 * @param feature whether to draw rail type or road/tram type.
 */
void DrawTypeList(const Rect &r, const GUIList<uint8_t, std::nullptr_t, std::nullptr_t> &type_list, const Scrollbar &sb, uint8_t selected_id, const GUIBadgeClasses &badge_classes, GrfSpecFeature feature)
{
	auto [first, last] = sb.GetVisibleRangeIterators(type_list);

	bool rtl = _current_text_dir == TD_RTL;
	int step_size = GetTrackListHeight();
	int sprite_width = GetTrackImageCellSize().width;
	int circle_width = std::max(GetScaledSpriteSize(SPR_CIRCLE_FOLDED).width, GetScaledSpriteSize(SPR_CIRCLE_UNFOLDED).width);

	auto badge_column_widths = badge_classes.GetColumnWidths();

	Rect ir = r.WithHeight(step_size).Shrink(WidgetDimensions::scaled.matrix, RectPadding::zero);

	const int text_row_height = ir.Shrink(WidgetDimensions::scaled.matrix).Height();
	const int normal_text_y_offset = (text_row_height - GetCharacterHeight(FS_NORMAL)) / 2;

	for (auto it = first; it != last; ++it) {
		const auto &item = *it;

		const std::vector<BadgeID> *badges;
		TimerGameCalendar::Date introduction_date;
		StringID menu_text;
		SpriteID build_x;

		if (feature == GSF_RAILTYPES) {
			const RailTypeInfo *rti = GetRailTypeInfo((RailType)item);
			introduction_date = rti->introduction_date;
			badges = &rti->badges;
			menu_text = rti->strings.menu_text;
			build_x = rti->gui_sprites.build_x_rail;
		} else {
			const RoadTypeInfo *rti = GetRoadTypeInfo((RoadType)item);
			introduction_date = rti->introduction_date;
			badges = &rti->badges;
			menu_text = rti->strings.menu_text;
			build_x = rti->gui_sprites.build_x_road;
		}

		Rect textr = ir.Shrink(WidgetDimensions::scaled.matrix);
		Rect tr = ir.Indent(circle_width + WidgetDimensions::scaled.hsep_normal, rtl);

		const PaletteID pal = PAL_NONE;

		if (badge_column_widths.size() >= 1 && badge_column_widths[0] > 0) {
			Rect br = tr.WithWidth(badge_column_widths[0], rtl);
			DrawTypeBadgeColumn(br, 0, badge_classes, *badges, introduction_date, pal);
			tr = tr.Indent(badge_column_widths[0], rtl);
		}

		DrawSpriteIgnorePadding(build_x, pal, tr.WithWidth(sprite_width, rtl), SA_CENTER);

		tr = tr.Indent(sprite_width + WidgetDimensions::scaled.hsep_wide, rtl);

		if (badge_column_widths.size() >= 2 && badge_column_widths[1] > 0) {
			Rect br = tr.WithWidth(badge_column_widths[1], rtl);
			DrawTypeBadgeColumn(br, 1, badge_classes, *badges, introduction_date, pal);
			tr = tr.Indent(badge_column_widths[1], rtl);
		}

		if (badge_column_widths.size() >= 3 && badge_column_widths[2] > 0) {
			Rect br = tr.WithWidth(badge_column_widths[2], !rtl).Indent(WidgetDimensions::scaled.hsep_wide, rtl);
			DrawTypeBadgeColumn(br, 2, badge_classes, *badges, introduction_date, pal);
			tr = tr.Indent(badge_column_widths[2], !rtl);
		}

		TextColour tc = (item == selected_id) ? TC_WHITE : TC_BLACK;

		DrawString(tr.left, tr.right, textr.top + normal_text_y_offset, GetString(menu_text), tc);

		ir = ir.Translate(0, step_size);
	}
}

/** GUI for selecting rail, road or tram types. */
struct TypeSelectionWindow : Window {
	bool descending_sort_order = false; ///< Sort direction.
	uint8_t sort_criteria = 0; ///< Current sort criterium.
	int details_height = 4; ///< Minimal needed height of the details panels, in text lines (found so far).
	uint8_t selected_type = INVALID_RAILTYPE; ///< Currently selected engine, or #INVALID_RAILTYPE
	GUIList<uint8_t, std::nullptr_t, std::nullptr_t> type_list{};
	Scrollbar *vscroll = nullptr;
	GUIBadgeClasses badge_classes{};
	GrfSpecFeature feature;

	static constexpr int BADGE_COLUMNS = 3; ///< Number of columns available for badges (0 = left of image, 1 = between image and name, 2 = after name)

	StringFilter string_filter{}; ///< Filter for track name
	QueryString track_editbox; ///< Filter editbox

	std::pair<WidgetID, WidgetID> badge_filters{}; ///< First and last widgets IDs of badge filters.
	BadgeFilterChoices badge_filter_choices{};

	TypeSelectionWindow(WindowDesc &desc, GrfSpecFeature feature) : Window(desc), feature(feature), track_editbox(32 * MAX_CHAR_LENGTH, 32)
	{
		this->window_number = feature;

		this->sort_criteria = _select_type_sort_last_criteria;
		this->descending_sort_order = _select_type_sort_last_order;

		this->CreateNestedTree();

		this->vscroll = this->GetScrollbar(WID_TS_SCROLLBAR);

		NWidgetCore *widget = this->GetWidget<NWidgetCore>(WID_TS_LIST);

		switch (feature) {
			case GSF_RAILTYPES: widget->SetToolTip(STR_RAIL_TYPE_LIST_TOOLTIP); break;
			case GSF_ROADTYPES: widget->SetToolTip(STR_ROAD_TYPE_LIST_TOOLTIP); break;
			case GSF_TRAMTYPES: widget->SetToolTip(STR_TRAM_TYPE_LIST_TOOLTIP); break;
			default: NOT_REACHED();
		}

		this->FinishInitNested(0);

		this->querystrings[WID_TS_FILTER] = &this->track_editbox;
		this->track_editbox.cancel_button = QueryString::ACTION_CLEAR;

		this->owner = _local_company;

		this->type_list.ForceRebuild();
	}

	void SelectTrack(uint8_t type)
	{
		this->selected_type = type;
	}

	void OnInit() override
	{
		this->badge_classes = GUIBadgeClasses(this->feature);

		auto container = this->GetWidget<NWidgetContainer>(WID_TS_BADGE_FILTER);
		this->badge_filters = AddBadgeDropdownFilters(*container, WID_TS_BADGE_FILTER, COLOUR_DARK_GREEN, this->feature);

		this->widget_lookup.clear();
		this->nested_root->FillWidgetLookup(this->widget_lookup);
	}

	/** Filter by name and NewGRF extra text */
	template<typename TypeInfo>
	bool FilterByText(const TypeInfo *ti)
	{
		/* Do not filter if the filter text box is empty */
		if (this->string_filter.IsEmpty()) return true;

		/* Filter engine name */
		this->string_filter.ResetState();
		this->string_filter.AddLine(GetString(ti->strings.name));
		this->string_filter.AddLine(GetString(ti->strings.menu_text));

		return this->string_filter.GetState();
	}

	/* Generate the list of tracks */
	void GenerateBuildList()
	{
		if (!this->type_list.NeedRebuild()) return;

		this->type_list.clear();

		uint8_t sel_id = INVALID_RAILTYPE;
		BadgeTextFilter btf(this->string_filter, this->feature);
		BadgeDropdownFilter bdf(this->badge_filter_choices);

		const Company *c = Company::Get(_local_company);

		if (this->feature == GSF_RAILTYPES) {
			RailTypes used_railtypes = GetRailTypes(true);
			for (const auto &rt : _sorted_railtypes) {
				/* If it isn't ever used or isn't available, don't show it to the user. */
				if (!used_railtypes.Test(rt)) continue;
				if (!c->avail_railtypes.Test(rt)) continue;
				if (!ValParamRailType(rt)) continue;

				const RailTypeInfo *rti = GetRailTypeInfo(rt);
				if (!bdf.Filter(rti->badges)) continue;

				/* Filter by name or NewGRF extra text */
				if (!FilterByText<RailTypeInfo>(rti) && !btf.Filter(rti->badges)) continue;

				this->type_list.emplace_back(rt);

				if (rt == this->selected_type) sel_id = this->selected_type;
			}
		} else {
			RoadTypes used_roadtypes = GetRoadTypes(true);

			/* Filter listed road types to match feature */
			if (this->feature == GSF_TRAMTYPES) used_roadtypes.Reset(GetMaskForRoadTramType(RTT_ROAD));
			else used_roadtypes.Reset(GetMaskForRoadTramType(RTT_TRAM));

			for (const auto &rt : _sorted_roadtypes) {
				/* If it isn't ever used or isn't available, don't show it to the user. */
				if (!used_roadtypes.Test(rt)) continue;
				if (!c->avail_roadtypes.Test(rt)) continue;
				if (!ValParamRoadType(rt)) continue;

				const RoadTypeInfo *rti = GetRoadTypeInfo(rt);
				if (!bdf.Filter(rti->badges)) continue;

				/* Filter by name or NewGRF extra text */
				if (!FilterByText<RoadTypeInfo>(rti) && !btf.Filter(rti->badges)) continue;

				this->type_list.emplace_back(rt);

				if (rt == this->selected_type) sel_id = this->selected_type;
			}
		}

		this->SelectTrack(sel_id);

		_select_type_sort_direction = this->descending_sort_order;

		if (this->feature == GSF_RAILTYPES) {
			std::sort(this->type_list.begin(), this->type_list.end(), _rail_types_sort_functions[this->sort_criteria]);
		} else {
			std::sort(this->type_list.begin(), this->type_list.end(), _road_types_sort_functions[this->sort_criteria]);
		}

		this->type_list.RebuildDone();
	}

	DropDownList BuildBadgeConfigurationList() const
	{
		static const auto separators = {STR_BADGE_CONFIG_PREVIEW, STR_BADGE_CONFIG_NAME};
		return BuildBadgeClassConfigurationList(this->badge_classes, BADGE_COLUMNS, separators);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_TS_SORT_ASCENDING_DESCENDING:
				this->descending_sort_order ^= true;
				_select_type_sort_last_order = this->descending_sort_order;
				this->type_list.ForceRebuild();
				this->SetDirty();
				break;

			case WID_TS_LIST: {
				uint8_t type = INVALID_RAILTYPE;
				const auto it = this->vscroll->GetScrolledItemFromWidget(this->type_list, pt.y, this, WID_TS_LIST);
				if (it != this->type_list.end()) {
					type = *it;
				}
				this->SelectTrack(type);
				this->SetDirty();
				if (type != INVALID_RAILTYPE) {
					SetLastBuiltType(this->feature, type);
					ShowBuildToolbar(this->feature, type);
				} else {
					CloseWindowByClass(WC_BUILD_TOOLBAR);
				}
				break;
			}

			case WID_TS_SORT_DROPDOWN: // Select sorting criteria dropdown menu
				if (this->feature == GSF_RAILTYPES) ShowDropDownMenu(this, _rail_type_sort_listing, this->sort_criteria, WID_TS_SORT_DROPDOWN, 0, 0);
				else ShowDropDownMenu(this, _road_type_sort_listing, this->sort_criteria, WID_TS_SORT_DROPDOWN, 0, 0);
				break;

			case WID_TS_CONFIGURE_BADGES:
				if (this->badge_classes.GetClasses().empty()) break;
				ShowDropDownList(this, this->BuildBadgeConfigurationList(), -1, widget, 0, false, true);
				break;

			default:
				if (IsInsideMM(widget, this->badge_filters.first, this->badge_filters.second)) {
					ShowDropDownList(this, this->GetWidget<NWidgetBadgeFilter>(widget)->GetDropDownList(), -1, widget, 0, false);
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
		this->type_list.ForceRebuild();
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_TS_CAPTION:
				switch (this->feature) {
					case GSF_RAILTYPES: return GetString(STR_RAIL_TYPE_LIST_AVAILABLE_TYPES);
					case GSF_ROADTYPES: return GetString(STR_ROAD_TYPE_LIST_AVAILABLE_TYPES);
					case GSF_TRAMTYPES: return GetString(STR_TRAM_TYPE_LIST_AVAILABLE_TYPES);
					default: NOT_REACHED();
				}

			case WID_TS_SORT_DROPDOWN:
				if (this->feature == GSF_RAILTYPES) return GetString(std::data(_rail_type_sort_listing)[this->sort_criteria]);
				return GetString(std::data(_road_type_sort_listing)[this->sort_criteria]);

			default:
				if (IsInsideMM(widget, this->badge_filters.first, this->badge_filters.second)) {
					return this->GetWidget<NWidgetBadgeFilter>(widget)->GetStringParameter(this->badge_filter_choices);
				}

				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_TS_LIST:
				fill.height = resize.height = GetTrackListHeight();
				size.height = 3 * resize.height;
				size.width = std::max(size.width, this->badge_classes.GetTotalColumnsWidth() + GetTrackImageCellSize().width + 165) + padding.width;
				break;

			case WID_TS_PANEL:
				size.height = GetCharacterHeight(FS_NORMAL) * this->details_height + padding.height;
				break;

			case WID_TS_SORT_ASCENDING_DESCENDING: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->GetString());
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_TS_CONFIGURE_BADGES:
				/* Hide the configuration button if no configurable badges are present. */
				if (this->badge_classes.GetClasses().empty()) size = {0, 0};
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_TS_LIST:
				DrawTypeList(
					r,
					this->type_list,
					*this->vscroll,
					this->selected_type,
					this->badge_classes,
					this->feature
				);
				break;

			case WID_TS_SORT_ASCENDING_DESCENDING:
				this->DrawSortButtonState(WID_TS_SORT_ASCENDING_DESCENDING, this->descending_sort_order ? SBS_DOWN : SBS_UP);
				break;
		}
	}

	void OnPaint() override
	{
		this->GenerateBuildList();
		this->vscroll->SetCount(this->type_list.size());

		this->DrawWidgets();

		if (!this->IsShaded()) {
			int needed_height = this->details_height;
			/* Draw details panels. */
			if (this->selected_type != INVALID_RAILTYPE) {
				const Rect r = this->GetWidget<NWidgetBase>(WID_TS_PANEL)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);

				int text_end;
				if (this->feature == GSF_RAILTYPES) text_end = DrawRailTypeInfo(r.left, r.right, r.top, (RailType)this->selected_type);
				else text_end = DrawRoadTypeInfo(r.left, r.right, r.top, (RoadType)this->selected_type);

				needed_height = std::max(needed_height, (text_end - r.top) / GetCharacterHeight(FS_NORMAL));
			}
			if (needed_height != this->details_height) { // Details window are not high enough, enlarge them.
				int resize = needed_height - this->details_height;
				this->details_height = needed_height;
				this->ReInit(0, resize * GetCharacterHeight(FS_NORMAL));
				return;
			}
		}
	}

	void OnDropdownSelect(WidgetID widget, int index, int click_result) override
	{
		switch (widget) {
			case WID_TS_SORT_DROPDOWN:
				if (this->sort_criteria != index) {
					this->sort_criteria = index;
					_select_type_sort_last_criteria = this->sort_criteria;
					this->type_list.ForceRebuild();
				}
				break;

			case WID_TS_CONFIGURE_BADGES: {
				bool reopen = HandleBadgeConfigurationDropDownClick(this->feature, BADGE_COLUMNS, index, click_result, this->badge_filter_choices);

				this->ReInit();

				if (reopen) {
					ReplaceDropDownList(this, this->BuildBadgeConfigurationList(), -1);
				} else {
					this->CloseChildWindows(WC_DROPDOWN_MENU);
				}

				/* We need to refresh if a filter is removed. */
				this->type_list.ForceRebuild();
				break;
			}

			default:
				if (IsInsideMM(widget, this->badge_filters.first, this->badge_filters.second)) {
					if (index < 0) {
						ResetBadgeFilter(this->badge_filter_choices, this->GetWidget<NWidgetBadgeFilter>(widget)->GetBadgeClassID());
					} else {
						SetBadgeFilter(this->badge_filter_choices, BadgeID(index));
					}
					this->type_list.ForceRebuild();
				}
				break;
		}
		this->SetDirty();
	}

	Point OnInitialPosition([[maybe_unused]] int16_t sm_width, [[maybe_unused]] int16_t sm_height, [[maybe_unused]] int window_number) override
	{
		Point pt = GetToolbarAlignedWindowPosition(sm_width);
		Window *w_tb, *w_lg;
		w_tb = FindWindowByClass(WC_BUILD_TOOLBAR);
		w_lg = FindWindowByClass(WC_SCEN_LAND_GEN);

		if (w_tb != nullptr && w_tb->top == pt.y) {
			pt.y = w_tb->top + w_tb->height;
		}
		if (w_lg != nullptr && !_settings_client.gui.link_terraform_toolbar) {
			pt.x = w_lg->left - sm_width;
		}

		return pt;
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_TS_LIST);
	}

	void OnEditboxChanged(WidgetID wid) override
	{
		if (wid == WID_TS_FILTER) {
			this->string_filter.SetFilterTerm(this->track_editbox.text.GetText());
			this->InvalidateData();
		}
	}

	static inline HotkeyList hotkeys{"typeselection", {
		Hotkey('F', "focus_filter_box", WID_TS_FILTER),
	}};
};

static WindowDesc _type_selection_desc(
	WDP_ALIGN_TOOLBAR, "type_selection", 240, 268,
	WC_TYPE_SELECTION, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_type_selection_widgets,
	&TypeSelectionWindow::hotkeys
);

void ShowTypeSelectionWindow(GrfSpecFeature feature)
{
	assert(feature == GSF_RAILTYPES || feature == GSF_ROADTYPES || feature == GSF_TRAMTYPES);
	CloseWindowByClass(WC_TYPE_SELECTION);
	new TypeSelectionWindow(_type_selection_desc, feature);
}
