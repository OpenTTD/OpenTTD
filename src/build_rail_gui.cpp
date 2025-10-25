/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file build_rail_gui.cpp GUI for building rail tracks. */

#include "stdafx.h"
#include "rail.h"
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

#include "widgets/build_rail_widget.h"

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

static constexpr NWidgetPart _nested_build_rail_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_BR_CAPTION), SetTextStyle(TC_WHITE),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_VERTICAL),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_TEXTBTN, COLOUR_DARK_GREEN, WID_BR_SHOW_HIDDEN_TRACKS),
			NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_GREEN, WID_BR_SORT_ASCENDING_DESCENDING), SetStringTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
			NWidget(WWT_DROPDOWN, COLOUR_DARK_GREEN, WID_BR_SORT_DROPDOWN), SetResize(1, 0), SetFill(1, 0), SetToolTip(STR_TOOLTIP_SORT_CRITERIA),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
				NWidget(WWT_EDITBOX, COLOUR_DARK_GREEN, WID_BR_FILTER), SetResize(1, 0), SetFill(1, 0), SetPadding(2), SetStringTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
			EndContainer(),
			NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_BR_CONFIGURE_BADGES), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON), SetResize(0, 0), SetFill(0, 1), SetSpriteTip(SPR_EXTRA_MENU, STR_BADGE_CONFIG_MENU_TOOLTIP),
		EndContainer(),
		NWidget(NWID_VERTICAL, NWidContainerFlag{}, WID_BR_BADGE_FILTER),
		EndContainer(),
	EndContainer(),
	/* Vehicle list. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_DARK_GREEN, WID_BR_LIST), SetResize(1, 1), SetFill(1, 0), SetMatrixDataTip(1, 0), SetScrollbar(WID_BR_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_BR_SCROLLBAR),
	EndContainer(),
	/* Panel with details. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BR_PANEL), SetMinimalSize(240, 122), SetResize(1, 0), EndContainer(),
	/* Hide, rename and resize buttons. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_GREEN, WID_BR_SHOW_HIDE), SetResize(1, 0), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_GREEN, WID_BR_RENAME), SetResize(1, 0), SetFill(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),
};

bool _track_sort_direction; ///< \c false = descending, \c true = ascending.
uint8_t _track_sort_last_criteria = 0; ///< Last set sort criteria.
bool _track_sort_last_order = false; ///< Last set direction of the sort order.
bool _track_sort_show_hidden_tracks = false; ///< Last set 'show hidden tracks' setting.

typedef bool TrackList_SortTypeFunction(const RailType&, const RailType&);

/**
 * Determines order of tracks by sorting order
 * @param a first track to compare
 * @param b second track to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool TrackNumberSorter(const RailType &a, const RailType &b)
{
	int r = GetRailTypeInfo(a)->sorting_order - GetRailTypeInfo(b)->sorting_order;

	return _track_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of tracks by sorting order
 * @param a first track to compare
 * @param b second track to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool TrackCostSorter(const RailType &a, const RailType &b)
{
	int r = GetRailTypeInfo(a)->cost_multiplier - GetRailTypeInfo(b)->cost_multiplier;

	/* Use sorting_order to sort instead since we want consistent sorting */
	if (r == 0) return TrackNumberSorter(a, b);
	return _track_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of tracks by sorting order
 * @param a first track to compare
 * @param b second track to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool TrackSpeedSorter(const RailType &a, const RailType &b)
{
	int r = GetRailTypeInfo(a)->max_speed - GetRailTypeInfo(b)->max_speed;

	/* Use sorting_order to sort instead since we want consistent sorting */
	if (r == 0) return TrackNumberSorter(a, b);
	return _track_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of tracks by sorting order
 * @param a first track to compare
 * @param b second track to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool TrackMaintenanceCostSorter(const RailType &a, const RailType &b)
{
	int r = GetRailTypeInfo(a)->maintenance_multiplier - GetRailTypeInfo(b)->maintenance_multiplier;

	/* Use sorting_order to sort instead since we want consistent sorting */
	if (r == 0) return TrackNumberSorter(a, b);
	return _track_sort_direction ? r > 0 : r < 0;
}

/* cached values for TrackNameSorter to spare many GetString() calls */
static RailType _last_track[2] = { INVALID_RAILTYPE, INVALID_RAILTYPE };

/**
 * Determines order of tracks by sorting order
 * @param a first track to compare
 * @param b second track to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool TrackNameSorter(const RailType &a, const RailType &b)
{
	static std::string last_name[2] = { {}, {} };

	if (a != _last_track[0]) {
		_last_track[0] = a;
		last_name[0] = GetRailTypeInfo(a)->GetString(RailTypeInfo::Strings::MenuText);
	}

	if (b != _last_track[1]) {
		_last_track[1] = b;
		last_name[1] = GetRailTypeInfo(b)->GetString(RailTypeInfo::Strings::MenuText);
	}

	int r = StrNaturalCompare(last_name[0], last_name[1]); // Sort by name (natural sorting).

	/* Use sorting_order to sort instead since we want consistent sorting */
	if (r == 0) return TrackNumberSorter(a, b);
	return _track_sort_direction ? r > 0 : r < 0;
}

/** Sort functions for the track sort criteria. */
TrackList_SortTypeFunction * const _tracks_sort_functions[] = {
	TrackNumberSorter,
	TrackCostSorter,
	TrackSpeedSorter,
	TrackMaintenanceCostSorter,
	TrackNameSorter,
};

/** Dropdown menu strings for the track sort criteria. */
const std::initializer_list<const StringID> _track_sort_listing = {
	STR_SORT_BY_RAIL_TYPE_ID,
	STR_SORT_BY_COST,
	STR_SORT_BY_MAX_SPEED,
	STR_SORT_BY_MAINTENANCE_COST,
	STR_SORT_BY_NAME,
};

/**
 * Draw the details of a rail track type at a given location.
 * @param left,right,y location where to draw the info
 * @param track_number the track of which to draw the info of
 * @return y after drawing all the text
 */
int DrawTrackInfo(int left, int right, int y, RailType track_number)
{
	const RailTypeInfo *rti = GetRailTypeInfo(track_number);

	DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST, RailBuildCost(track_number)));
	y += GetCharacterHeight(FS_NORMAL);

	if (rti->max_speed != 0) {
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_SPEED, PackVelocity(rti->max_speed, VEH_TRAIN)));
		y += GetCharacterHeight(FS_NORMAL);
	}

	DrawString(left, right, y, GetString(TimerGameEconomy::UsingWallclockUnits() ? STR_PURCHASE_INFO_MAINTENANCE_PERIOD : STR_PURCHASE_INFO_MAINTENANCE_YEAR, RailMaintenanceCost(track_number, 1, 1)));
	y += GetCharacterHeight(FS_NORMAL);

	y = DrawBadgeNameList({left, y, right, INT16_MAX}, rti->badges, static_cast<GrfSpecFeature>(GSF_RAILTYPES));

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

static void DrawTrackBadgeColumn(const Rect &r, int column_group, const GUIBadgeClasses &badge_classes, const RailTypeInfo *rti, PaletteID remap)
{
	DrawBadgeColumn(r, column_group, badge_classes, rti->badges, static_cast<GrfSpecFeature>(GSF_RAILTYPES), rti->introduction_date, remap);
}

/**
 * Engine drawing loop
 * @param r The Rect of the list
 * @param track_list What tracks to draw
 * @param sb Scrollbar of list.
 * @param selected_id what track to highlight as selected, if any
 * @param selected_group the group to list the tracks of
 */
void DrawTrackList(const Rect &r, const GUIList<RailType, std::nullptr_t, std::nullptr_t> &track_list, const Scrollbar &sb, RailType selected_id, const GUIBadgeClasses &badge_classes)
{
	auto [first, last] = sb.GetVisibleRangeIterators(track_list);

	bool rtl = _current_text_dir == TD_RTL;
	int step_size = GetTrackListHeight();
	int sprite_width = GetTrackImageCellSize().width;
	int circle_width = std::max(GetScaledSpriteSize(SPR_CIRCLE_FOLDED).width, GetScaledSpriteSize(SPR_CIRCLE_UNFOLDED).width);

	auto badge_column_widths = badge_classes.GetColumnWidths();

	Rect ir = r.WithHeight(step_size).Shrink(WidgetDimensions::scaled.matrix, RectPadding::zero);

	const int text_row_height = ir.Shrink(WidgetDimensions::scaled.matrix).Height();
	const int normal_text_y_offset = (text_row_height - GetCharacterHeight(FS_NORMAL)) / 2;

	const Company *c = Company::Get(_local_company);

	for (auto it = first; it != last; ++it) {
		const auto &item = *it;
		const RailTypeInfo *rti = GetRailTypeInfo(item);

		Rect textr = ir.Shrink(WidgetDimensions::scaled.matrix);
		Rect tr = ir.Indent(circle_width + WidgetDimensions::scaled.hsep_normal, rtl);

		const PaletteID pal = PAL_NONE;

		if (badge_column_widths.size() >= 1 && badge_column_widths[0] > 0) {
			Rect br = tr.WithWidth(badge_column_widths[0], rtl);
			DrawTrackBadgeColumn(br, 0, badge_classes, rti, pal);
			tr = tr.Indent(badge_column_widths[0], rtl);
		}

		DrawSpriteIgnorePadding(rti->gui_sprites.build_x_rail, pal, tr.WithWidth(sprite_width, rtl), SA_CENTER);

		tr = tr.Indent(sprite_width + WidgetDimensions::scaled.hsep_wide, rtl);

		if (badge_column_widths.size() >= 2 && badge_column_widths[1] > 0) {
			Rect br = tr.WithWidth(badge_column_widths[1], rtl);
			DrawTrackBadgeColumn(br, 1, badge_classes, rti, pal);
			tr = tr.Indent(badge_column_widths[1], rtl);
		}

		if (badge_column_widths.size() >= 3 && badge_column_widths[2] > 0) {
			Rect br = tr.WithWidth(badge_column_widths[2], !rtl).Indent(WidgetDimensions::scaled.hsep_wide, rtl);
			DrawTrackBadgeColumn(br, 2, badge_classes, rti, pal);
			tr = tr.Indent(badge_column_widths[2], !rtl);
		}

		bool hidden = c->hidden_railtypes.Test(item) ;
		TextColour tc = (item == selected_id) ? TC_WHITE : ((hidden) ? (TC_GREY | TC_FORCED | TC_NO_SHADE) : TC_BLACK);

		DrawString(tr.left, tr.right, textr.top + normal_text_y_offset, rti->GetString(RailTypeInfo::Strings::MenuText), tc);

		ir = ir.Translate(0, step_size);
	}
}

/** GUI for building rail tracks. */
struct BuildRailWindow : Window {
	bool descending_sort_order = false; ///< Sort direction, @see _tracks_sort_direction
	uint8_t sort_criteria = 0; ///< Current sort criterium.
	bool show_hidden_tracks = false; ///< State of the 'show hidden engines' button.
	int details_height = 4; ///< Minimal needed height of the details panels, in text lines (found so far).
	RailType sel_track = INVALID_RAILTYPE; ///< Currently selected engine, or #INVALID_RAILTYPE
	RailType rename_track = INVALID_RAILTYPE; ///< Engine being renamed.
	GUIList<RailType, std::nullptr_t, std::nullptr_t> track_list{};
	Scrollbar *vscroll = nullptr;
	GUIBadgeClasses badge_classes{};

	static constexpr int BADGE_COLUMNS = 3; ///< Number of columns available for badges (0 = left of image, 1 = between image and name, 2 = after name)

	StringFilter string_filter{}; ///< Filter for track name
	QueryString track_editbox; ///< Filter editbox

	std::pair<WidgetID, WidgetID> badge_filters{}; ///< First and last widgets IDs of badge filters.
	BadgeFilterChoices badge_filter_choices{};

	BuildRailWindow(WindowDesc &desc) : Window(desc), track_editbox(MAX_LENGTH_RAIL_TRACK_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_RAIL_TRACK_NAME_CHARS)
	{
		this->window_number = 0;

		this->sort_criteria         = _track_sort_last_criteria;
		this->descending_sort_order = _track_sort_last_order;
		this->show_hidden_tracks   = _track_sort_show_hidden_tracks;

		this->CreateNestedTree();

		this->vscroll = this->GetScrollbar(WID_BR_SCROLLBAR);

		NWidgetCore *widget = this->GetWidget<NWidgetCore>(WID_BR_LIST);
		widget->SetToolTip(STR_RAIL_TYPE_LIST_TOOLTIP);

		widget = this->GetWidget<NWidgetCore>(WID_BR_SHOW_HIDE);
		widget->SetToolTip(STR_RAIL_TYPE_HIDE_SHOW_TOGGLE_TOOLTIP);

		widget = this->GetWidget<NWidgetCore>(WID_BR_RENAME);
		widget->SetStringTip(STR_RAIL_TYPE_RENAME_BUTTON, STR_RAIL_TYPE_RENAME_TOOLTIP);

		widget = this->GetWidget<NWidgetCore>(WID_BR_SHOW_HIDDEN_TRACKS);
		widget->SetStringTip(STR_SHOW_HIDDEN_RAIL_TYPES, STR_SHOW_HIDDEN_RAIL_TYPES_TOOLTIP);
		widget->SetLowered(this->show_hidden_tracks);

		this->FinishInitNested(0);

		this->querystrings[WID_BR_FILTER] = &this->track_editbox;
		this->track_editbox.cancel_button = QueryString::ACTION_CLEAR;

		this->owner = _local_company;

		this->track_list.ForceRebuild();
	}

	void SelectTrack(RailType track)
	{
		this->sel_track = track;
	}

	void OnInit() override
	{
		this->badge_classes = GUIBadgeClasses(static_cast<GrfSpecFeature>(GSF_RAILTYPES));

		auto container = this->GetWidget<NWidgetContainer>(WID_BR_BADGE_FILTER);
		this->badge_filters = AddBadgeDropdownFilters(*container, WID_BR_BADGE_FILTER, COLOUR_DARK_GREEN, static_cast<GrfSpecFeature>(GSF_RAILTYPES));

		this->widget_lookup.clear();
		this->nested_root->FillWidgetLookup(this->widget_lookup);
	}

	/** Filter by name and NewGRF extra text */
	bool FilterByText(const RailTypeInfo *rti)
	{
		/* Do not filter if the filter text box is empty */
		if (this->string_filter.IsEmpty()) return true;

		/* Filter engine name */
		this->string_filter.ResetState();
		this->string_filter.AddLine(rti->GetString(RailTypeInfo::Strings::Name));
		this->string_filter.AddLine(rti->GetString(RailTypeInfo::Strings::MenuText));

		/* Filter NewGRF extra text */
		//auto text = GetNewGRFAdditionalText(e->index);
		//if (text) this->string_filter.AddLine(*text);

		return this->string_filter.GetState();
	}

	/* Generate the list of tracks */
	void GenerateBuildList()
	{
		if (!this->track_list.NeedRebuild()) return;

		this->track_list.clear();

		RailTypes used_railtypes = GetRailTypes(true);
		RailType sel_id = INVALID_RAILTYPE;

		BadgeTextFilter btf(this->string_filter, GSF_RAILTYPES);
		BadgeDropdownFilter bdf(this->badge_filter_choices);

		const Company *c = Company::Get(_local_company);

		for (const auto &rt : _sorted_railtypes) {
			/* If it's not used ever, don't show it to the user. */
			if (!used_railtypes.Test(rt)) continue;

			if (!this->show_hidden_tracks && c->hidden_railtypes.Test(rt)) continue;

			const RailTypeInfo *rti = GetRailTypeInfo(rt);
			if (!bdf.Filter(rti->badges)) continue;

			/* Filter by name or NewGRF extra text */
			if (!FilterByText(rti) && !btf.Filter(rti->badges)) continue;

			this->track_list.emplace_back(rt);

			if (rt == this->sel_track) sel_id = this->sel_track;
		}

		this->SelectTrack(sel_id);

		_track_sort_direction = this->descending_sort_order;
		std::sort(this->track_list.begin(), this->track_list.end(), _tracks_sort_functions[this->sort_criteria]);

		this->track_list.RebuildDone();
	}

	DropDownList BuildBadgeConfigurationList() const
	{
		static const auto separators = {STR_BADGE_CONFIG_PREVIEW, STR_BADGE_CONFIG_NAME};
		return BuildBadgeClassConfigurationList(this->badge_classes, BADGE_COLUMNS, separators);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BR_SORT_ASCENDING_DESCENDING:
				this->descending_sort_order ^= true;
				_track_sort_last_order = this->descending_sort_order;
				this->track_list.ForceRebuild();
				this->SetDirty();
				break;

			case WID_BR_SHOW_HIDDEN_TRACKS:
				this->show_hidden_tracks ^= true;
				_track_sort_show_hidden_tracks = this->show_hidden_tracks;
				this->track_list.ForceRebuild();
				this->SetWidgetLoweredState(widget, this->show_hidden_tracks);
				this->SetDirty();
				break;

			case WID_BR_LIST: {
				RailType rt = INVALID_RAILTYPE;
				const auto it = this->vscroll->GetScrolledItemFromWidget(this->track_list, pt.y, this, WID_BR_LIST);
				if (it != this->track_list.end()) {
					rt = *it;
				}
				this->SelectTrack(rt);
				this->SetDirty();
				if (_ctrl_pressed) {
					this->OnClick(pt, WID_BR_SHOW_HIDE, 1);
				} else if (rt != INVALID_RAILTYPE) {
					ShowBuildRailToolbar(rt);
				} else {
					CloseWindowByClass(WC_BUILD_TOOLBAR);
				}
				break;
			}

			case WID_BR_SORT_DROPDOWN: // Select sorting criteria dropdown menu
				ShowDropDownMenu(this, _track_sort_listing, this->sort_criteria, WID_BR_SORT_DROPDOWN, 0, 0);
				break;

			case WID_BR_CONFIGURE_BADGES:
				if (this->badge_classes.GetClasses().empty()) break;
				ShowDropDownList(this, this->BuildBadgeConfigurationList(), -1, widget, 0, false, true);
				break;

			case WID_BR_SHOW_HIDE: {
				if (this->sel_track == INVALID_RAILTYPE) break;
				const Company *c = Company::Get(_local_company);
				Command<CMD_SET_RAIL_TYPE_COMPANY_HIDDEN>::Post(this->sel_track, !c->hidden_railtypes.Test(this->sel_track));
				break;
			}

			case WID_BR_RENAME: {
				if (this->sel_track != INVALID_RAILTYPE) {
					this->rename_track = this->sel_track;
					std::string str = GetRailTypeInfo(this->rename_track)->GetString(RailTypeInfo::Strings::Name);
					if (str.size() > MAX_LENGTH_RAIL_TRACK_NAME_CHARS) str.resize(MAX_LENGTH_RAIL_TRACK_NAME_CHARS); ///< Prevent overflow
					ShowQueryString(str, STR_QUERY_RENAME_RAIL_TYPE_CAPTION, MAX_LENGTH_RAIL_TRACK_NAME_CHARS, this, CS_ALPHANUMERAL, {QueryStringFlag::EnableDefault, QueryStringFlag::LengthIsInChars});
				}
				break;
			}

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
		this->track_list.ForceRebuild();
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_BR_CAPTION:
				return GetString(STR_RAIL_TYPE_LIST_AVAILABLE_TRACKS);

			case WID_BR_SORT_DROPDOWN:
				return GetString(std::data(_track_sort_listing)[this->sort_criteria]);

			case WID_BR_SHOW_HIDE: {
				const Company *c = Company::Get(_local_company);
				if (c->hidden_railtypes.Test(this->sel_track)) {
					return GetString(STR_RAIL_TYPE_SHOW_TOGGLE_BUTTON);
				}
				return GetString(STR_RAIL_TYPE_HIDE_TOGGLE_BUTTON);
			}

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
			case WID_BR_LIST:
				fill.height = resize.height = GetTrackListHeight();
				size.height = 3 * resize.height;
				size.width = std::max(size.width, this->badge_classes.GetTotalColumnsWidth() + GetTrackImageCellSize().width + 165) + padding.width;
				break;

			case WID_BR_PANEL:
				size.height = GetCharacterHeight(FS_NORMAL) * this->details_height + padding.height;
				break;

			case WID_BR_SORT_ASCENDING_DESCENDING: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->GetString());
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_BR_CONFIGURE_BADGES:
				/* Hide the configuration button if no configurable badges are present. */
				if (this->badge_classes.GetClasses().empty()) size = {0, 0};
				break;

			case WID_BR_SHOW_HIDE:
				size = GetStringBoundingBox(STR_BUY_VEHICLE_TRAIN_HIDE_TOGGLE_BUTTON);
				size = maxdim(size, GetStringBoundingBox(STR_BUY_VEHICLE_TRAIN_SHOW_TOGGLE_BUTTON));
				size.width += padding.width;
				size.height += padding.height;
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_BR_LIST:
				DrawTrackList(
					r,
					this->track_list,
					*this->vscroll,
					this->sel_track,
					this->badge_classes
				);
				break;

			case WID_BR_SORT_ASCENDING_DESCENDING:
				this->DrawSortButtonState(WID_BR_SORT_ASCENDING_DESCENDING, this->descending_sort_order ? SBS_DOWN : SBS_UP);
				break;
		}
	}

	void OnPaint() override
	{
		this->GenerateBuildList();
		this->vscroll->SetCount(this->track_list.size());

		this->SetWidgetDisabledState(WID_BR_SHOW_HIDE, this->sel_track == INVALID_RAILTYPE);

		/* Disable renaming tracks in network games if you are not the server. */
		this->SetWidgetDisabledState(WID_BR_RENAME, this->sel_track == INVALID_RAILTYPE || (_networking && !_network_server));

		this->DrawWidgets();

		if (!this->IsShaded()) {
			int needed_height = this->details_height;
			/* Draw details panels. */
			if (this->sel_track != INVALID_RAILTYPE) {
				const Rect r = this->GetWidget<NWidgetBase>(WID_BR_PANEL)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
				int text_end = DrawTrackInfo(r.left, r.right, r.top, this->sel_track);
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

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value()) return;

		Command<CMD_RENAME_RAIL_TYPE>::Post(STR_ERROR_CAN_T_RENAME_RAIL_TYPE, this->rename_track, *str);
	}

	void OnDropdownSelect(WidgetID widget, int index, int click_result) override
	{
		switch (widget) {
			case WID_BR_SORT_DROPDOWN:
				if (this->sort_criteria != index) {
					this->sort_criteria = index;
					_track_sort_last_criteria = this->sort_criteria;
					this->track_list.ForceRebuild();
				}
				break;

			case WID_BR_CONFIGURE_BADGES: {
				bool reopen = HandleBadgeConfigurationDropDownClick(static_cast<GrfSpecFeature>(GSF_RAILTYPES), BADGE_COLUMNS, index, click_result, this->badge_filter_choices);

				this->ReInit();

				if (reopen) {
					ReplaceDropDownList(this, this->BuildBadgeConfigurationList(), -1);
				} else {
					this->CloseChildWindows(WC_DROPDOWN_MENU);
				}

				/* We need to refresh if a filter is removed. */
				this->track_list.ForceRebuild();
				break;
			}

			default:
				if (IsInsideMM(widget, this->badge_filters.first, this->badge_filters.second)) {
					if (index < 0) {
						ResetBadgeFilter(this->badge_filter_choices, this->GetWidget<NWidgetBadgeFilter>(widget)->GetBadgeClassID());
					} else {
						SetBadgeFilter(this->badge_filter_choices, BadgeID(index));
					}
					this->track_list.ForceRebuild();
				}
				break;
		}
		this->SetDirty();
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_BR_LIST);
	}

	void OnEditboxChanged(WidgetID wid) override
	{
		if (wid == WID_BR_FILTER) {
			this->string_filter.SetFilterTerm(this->track_editbox.text.GetText());
			this->InvalidateData();
		}
	}

	static inline HotkeyList hotkeys{"buildrail", {
		Hotkey('F', "focus_filter_box", WID_BR_FILTER),
	}};
};

static WindowDesc _build_rail_desc(
	WDP_ALIGN_TOOLBAR, "build_rail", 240, 268,
	WC_BUILD_RAIL, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_build_rail_widgets,
	&BuildRailWindow::hotkeys
);

void ShowBuildRailWindow()
{
	CloseWindowById(WC_BUILD_RAIL, 0);

	new BuildRailWindow(_build_rail_desc);
}
