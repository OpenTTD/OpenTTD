/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file group_gui.cpp GUI for the group window. */

#include "stdafx.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "vehicle_gui.h"
#include "vehicle_base.h"
#include "string_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "autoreplace_gui.h"
#include "company_func.h"
#include "widgets/dropdown_func.h"
#include "tilehighlight_func.h"
#include "vehicle_gui_base.h"
#include "core/geometry_func.hpp"
#include "core/container_func.hpp"
#include "company_base.h"
#include "company_gui.h"
#include "gui.h"
#include "group_cmd.h"
#include "vehicle_cmd.h"
#include "gfx_func.h"

#include "widgets/group_widget.h"

#include "table/sprites.h"

#include "safeguards.h"

typedef GUIList<const Group*> GUIGroupList;

static const NWidgetPart _nested_group_widgets[] = {
	NWidget(NWID_HORIZONTAL), // Window header
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_GL_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		/* left part */
		NWidget(NWID_VERTICAL, NC_BIGFIRST),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_GL_ALL_VEHICLES), SetFill(1, 0), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_GL_DEFAULT_VEHICLES), SetFill(1, 0), EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_MATRIX, COLOUR_GREY, WID_GL_LIST_GROUP), SetMatrixDataTip(1, 0, STR_GROUPS_CLICK_ON_GROUP_FOR_TOOLTIP),
						SetFill(1, 0), SetResize(0, 1), SetScrollbar(WID_GL_LIST_GROUP_SCROLLBAR),
				NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_GL_LIST_GROUP_SCROLLBAR),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_GL_INFO), SetFill(1, 1), SetMinimalTextLines(3, WidgetDimensions::unscaled.framerect.Vertical()), EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_GL_CREATE_GROUP),
						SetDataTip(SPR_GROUP_CREATE_TRAIN, STR_GROUP_CREATE_TOOLTIP),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_GL_DELETE_GROUP),
						SetDataTip(SPR_GROUP_DELETE_TRAIN, STR_GROUP_DELETE_TOOLTIP),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_GL_RENAME_GROUP),
						SetDataTip(SPR_GROUP_RENAME_TRAIN, STR_GROUP_RENAME_TOOLTIP),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_GL_LIVERY_GROUP),
						SetDataTip(SPR_GROUP_LIVERY_TRAIN, STR_GROUP_LIVERY_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 0), EndContainer(),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_GL_REPLACE_PROTECTION),
						SetDataTip(SPR_GROUP_REPLACE_OFF_TRAIN, STR_GROUP_REPLACE_PROTECTION_TOOLTIP),
			EndContainer(),
		EndContainer(),
		/* right part */
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_VERTICAL),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_GL_GROUP_BY_ORDER), SetFill(1, 1), SetMinimalSize(0, 12), SetDataTip(STR_STATION_VIEW_GROUP, STR_TOOLTIP_GROUP_ORDER),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GL_SORT_BY_ORDER), SetFill(1, 1), SetMinimalSize(0, 12), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
				EndContainer(),
				NWidget(NWID_VERTICAL),
					NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GL_GROUP_BY_DROPDOWN), SetFill(1, 0), SetMinimalSize(0, 12), SetDataTip(0x0, STR_TOOLTIP_GROUP_ORDER),
					NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GL_SORT_BY_DROPDOWN), SetFill(1, 0), SetMinimalSize(0, 12), SetDataTip(0x0, STR_TOOLTIP_SORT_CRITERIA),
				EndContainer(),
				NWidget(NWID_VERTICAL),
					NWidget(WWT_PANEL, COLOUR_GREY), SetMinimalSize(0, 12), SetResize(1, 0), EndContainer(),
					NWidget(NWID_HORIZONTAL),
						NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GL_FILTER_BY_CARGO), SetMinimalSize(0, 12), SetDataTip(STR_JUST_STRING, STR_TOOLTIP_FILTER_CRITERIA),
						NWidget(WWT_PANEL, COLOUR_GREY), SetMinimalSize(0, 12), SetResize(1, 0), EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_MATRIX, COLOUR_GREY, WID_GL_LIST_VEHICLE), SetMinimalSize(248, 0), SetMatrixDataTip(1, 0, STR_NULL), SetResize(1, 1), SetFill(1, 0), SetScrollbar(WID_GL_LIST_VEHICLE_SCROLLBAR),
				NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_GL_LIST_VEHICLE_SCROLLBAR),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY), SetMinimalSize(1, 0), SetFill(1, 1), SetResize(1, 0), EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GL_AVAILABLE_VEHICLES), SetMinimalSize(106, 12),
						SetDataTip(STR_JUST_STRING, STR_VEHICLE_LIST_AVAILABLE_ENGINES_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetMinimalSize(0, 12), SetFill(1, 0), SetResize(1, 0), EndContainer(),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GL_MANAGE_VEHICLES_DROPDOWN), SetMinimalSize(118, 12),
						SetDataTip(STR_VEHICLE_LIST_MANAGE_LIST, STR_VEHICLE_LIST_MANAGE_LIST_TOOLTIP),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_GL_STOP_ALL), SetMinimalSize(12, 12),
						SetDataTip(SPR_FLAG_VEH_STOPPED, STR_VEHICLE_LIST_MASS_STOP_LIST_TOOLTIP),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_GL_START_ALL), SetMinimalSize(12, 12),
						SetDataTip(SPR_FLAG_VEH_RUNNING, STR_VEHICLE_LIST_MASS_START_LIST_TOOLTIP),
				NWidget(WWT_RESIZEBOX, COLOUR_GREY),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

class VehicleGroupWindow : public BaseVehicleListWindow {
private:
	/* Columns in the group list */
	enum ListColumns {
		VGC_FOLD,          ///< Fold / Unfold button.
		VGC_NAME,          ///< Group name.
		VGC_PROTECT,       ///< Autoreplace protect icon.
		VGC_AUTOREPLACE,   ///< Autoreplace active icon.
		VGC_PROFIT,        ///< Profit icon.
		VGC_NUMBER,        ///< Number of vehicles in the group.

		VGC_END
	};

	GroupID group_sel;     ///< Selected group (for drag/drop)
	GroupID group_rename;  ///< Group being renamed, INVALID_GROUP if none
	GroupID group_over;    ///< Group over which a vehicle is dragged, INVALID_GROUP if none
	GroupID group_confirm; ///< Group awaiting delete confirmation
	GUIGroupList groups;   ///< List of groups
	uint tiny_step_height; ///< Step height for the group list
	Scrollbar *group_sb;

	std::vector<int> indents; ///< Indentation levels

	Dimension column_size[VGC_END]; ///< Size of the columns in the group list.

	void AddChildren(GUIGroupList &source, GroupID parent, int indent)
	{
		for (const Group *g : source) {
			if (g->parent != parent) continue;
			this->groups.push_back(g);
			this->indents.push_back(indent);
			if (g->folded) {
				/* Test if this group has children at all. If not, the folded flag should be cleared to avoid lingering unfold buttons in the list. */
				bool has_children = std::any_of(source.begin(), source.end(), [g](const Group *child){ return child->parent == g->index; });
				Group::Get(g->index)->folded = has_children;
			} else {
				AddChildren(source, g->index, indent + 1);
			}
		}
	}

	/**
	 * (Re)Build the group list.
	 *
	 * @param owner The owner of the window
	 */
	void BuildGroupList(Owner owner)
	{
		if (!this->groups.NeedRebuild()) return;

		this->groups.clear();
		this->indents.clear();

		GUIGroupList list;

		for (const Group *g : Group::Iterate()) {
			if (g->owner == owner && g->vehicle_type == this->vli.vtype) {
				list.push_back(g);
			}
		}

		list.ForceResort();

		/* Sort the groups by their name */
		const Group *last_group[2] = { nullptr, nullptr };
		std::string last_name[2] = { {}, {} };
		list.Sort([&](const Group * const &a, const Group * const &b) {
			if (a != last_group[0]) {
				last_group[0] = a;
				SetDParam(0, a->index);
				last_name[0] = GetString(STR_GROUP_NAME);
			}

			if (b != last_group[1]) {
				last_group[1] = b;
				SetDParam(0, b->index);
				last_name[1] = GetString(STR_GROUP_NAME);
			}

			int r = StrNaturalCompare(last_name[0], last_name[1]); // Sort by name (natural sorting).
			if (r == 0) return a->index < b->index;
			return r < 0;
		});

		AddChildren(list, INVALID_GROUP, 0);

		this->groups.shrink_to_fit();
		this->groups.RebuildDone();
	}

	/**
	 * Compute tiny_step_height and column_size
	 * @return Total width required for the group list.
	 */
	uint ComputeGroupInfoSize()
	{
		this->column_size[VGC_FOLD] = maxdim(GetSpriteSize(SPR_CIRCLE_FOLDED), GetSpriteSize(SPR_CIRCLE_UNFOLDED));
		this->tiny_step_height = this->column_size[VGC_FOLD].height;

		this->column_size[VGC_NAME] = maxdim(GetStringBoundingBox(STR_GROUP_DEFAULT_TRAINS + this->vli.vtype), GetStringBoundingBox(STR_GROUP_ALL_TRAINS + this->vli.vtype));
		this->column_size[VGC_NAME].width = std::max(170u, this->column_size[VGC_NAME].width) + WidgetDimensions::scaled.hsep_indent;
		this->tiny_step_height = std::max(this->tiny_step_height, this->column_size[VGC_NAME].height);

		this->column_size[VGC_PROTECT] = GetSpriteSize(SPR_GROUP_REPLACE_PROTECT);
		this->tiny_step_height = std::max(this->tiny_step_height, this->column_size[VGC_PROTECT].height);

		this->column_size[VGC_AUTOREPLACE] = GetSpriteSize(SPR_GROUP_REPLACE_ACTIVE);
		this->tiny_step_height = std::max(this->tiny_step_height, this->column_size[VGC_AUTOREPLACE].height);

		this->column_size[VGC_PROFIT].width = 0;
		this->column_size[VGC_PROFIT].height = 0;
		static const SpriteID profit_sprites[] = {SPR_PROFIT_NA, SPR_PROFIT_NEGATIVE, SPR_PROFIT_SOME, SPR_PROFIT_LOT};
		for (uint i = 0; i < lengthof(profit_sprites); i++) {
			Dimension d = GetSpriteSize(profit_sprites[i]);
			this->column_size[VGC_PROFIT] = maxdim(this->column_size[VGC_PROFIT], d);
		}
		this->tiny_step_height = std::max(this->tiny_step_height, this->column_size[VGC_PROFIT].height);

		int num_vehicle = GetGroupNumVehicle(this->vli.company, ALL_GROUP, this->vli.vtype);
		SetDParamMaxValue(0, num_vehicle, 3, FS_SMALL);
		SetDParamMaxValue(1, num_vehicle, 3, FS_SMALL);
		this->column_size[VGC_NUMBER] = GetStringBoundingBox(STR_GROUP_COUNT_WITH_SUBGROUP);
		this->tiny_step_height = std::max(this->tiny_step_height, this->column_size[VGC_NUMBER].height);

		this->tiny_step_height += WidgetDimensions::scaled.framerect.Vertical();

		return WidgetDimensions::scaled.framerect.left +
			this->column_size[VGC_FOLD].width + WidgetDimensions::scaled.hsep_normal +
			this->column_size[VGC_NAME].width + WidgetDimensions::scaled.hsep_wide +
			this->column_size[VGC_PROTECT].width + WidgetDimensions::scaled.hsep_normal +
			this->column_size[VGC_AUTOREPLACE].width + WidgetDimensions::scaled.hsep_normal +
			this->column_size[VGC_PROFIT].width + WidgetDimensions::scaled.hsep_normal +
			this->column_size[VGC_NUMBER].width +
			WidgetDimensions::scaled.framerect.right;
	}

	/**
	 * Draw a row in the group list.
	 * @param y Top of the row.
	 * @param left Left of the row.
	 * @param right Right of the row.
	 * @param g_id Group to list.
	 * @param indent Indentation level.
	 * @param protection Whether autoreplace protection is set.
	 * @param has_children Whether the group has children and should have a fold / unfold button.
	 */
	void DrawGroupInfo(int y, int left, int right, GroupID g_id, int indent = 0, bool protection = false, bool has_children = false) const
	{
		/* Highlight the group if a vehicle is dragged over it */
		if (g_id == this->group_over) {
			GfxFillRect(left + WidgetDimensions::scaled.bevel.left, y + WidgetDimensions::scaled.framerect.top, right - WidgetDimensions::scaled.bevel.right, y + this->tiny_step_height - 1 - WidgetDimensions::scaled.framerect.bottom, _colour_gradient[COLOUR_GREY][7]);
		}

		if (g_id == NEW_GROUP) return;

		/* draw the selected group in white, else we draw it in black */
		TextColour colour = g_id == this->vli.index ? TC_WHITE : TC_BLACK;
		const GroupStatistics &stats = GroupStatistics::Get(this->vli.company, g_id, this->vli.vtype);
		bool rtl = _current_text_dir == TD_RTL;

		/* draw fold / unfold button */
		int x = rtl ? right - WidgetDimensions::scaled.framerect.right - this->column_size[VGC_FOLD].width + 1 : left + WidgetDimensions::scaled.framerect.left;
		if (has_children) {
			DrawSprite(Group::Get(g_id)->folded ? SPR_CIRCLE_FOLDED : SPR_CIRCLE_UNFOLDED, PAL_NONE, rtl ? x - indent : x + indent, y + (this->tiny_step_height - this->column_size[VGC_FOLD].height) / 2);
		}

		/* draw group name */
		StringID str;
		if (IsAllGroupID(g_id)) {
			str = STR_GROUP_ALL_TRAINS + this->vli.vtype;
		} else if (IsDefaultGroupID(g_id)) {
			str = STR_GROUP_DEFAULT_TRAINS + this->vli.vtype;
		} else {
			SetDParam(0, g_id);
			str = STR_GROUP_NAME;
		}
		x = rtl ? x - WidgetDimensions::scaled.hsep_normal - this->column_size[VGC_NAME].width : x + WidgetDimensions::scaled.hsep_normal + this->column_size[VGC_FOLD].width;
		DrawString(x + (rtl ? 0 : indent), x + this->column_size[VGC_NAME].width - 1 - (rtl ? indent : 0), y + (this->tiny_step_height - this->column_size[VGC_NAME].height) / 2, str, colour);

		/* draw autoreplace protection */
		x = rtl ? x - WidgetDimensions::scaled.hsep_wide - this->column_size[VGC_PROTECT].width : x + WidgetDimensions::scaled.hsep_wide + this->column_size[VGC_NAME].width;
		if (protection) DrawSprite(SPR_GROUP_REPLACE_PROTECT, PAL_NONE, x, y + (this->tiny_step_height - this->column_size[VGC_PROTECT].height) / 2);

		/* draw autoreplace status */
		x = rtl ? x - WidgetDimensions::scaled.hsep_normal - this->column_size[VGC_AUTOREPLACE].width : x + WidgetDimensions::scaled.hsep_normal + this->column_size[VGC_PROTECT].width;
		if (stats.autoreplace_defined) DrawSprite(SPR_GROUP_REPLACE_ACTIVE, stats.autoreplace_finished ? PALETTE_CRASH : PAL_NONE, x, y + (this->tiny_step_height - this->column_size[VGC_AUTOREPLACE].height) / 2);

		/* draw the profit icon */
		x = rtl ? x - WidgetDimensions::scaled.hsep_normal - this->column_size[VGC_PROFIT].width : x + WidgetDimensions::scaled.hsep_normal + this->column_size[VGC_AUTOREPLACE].width;
		SpriteID spr;
		uint num_vehicle_min_age = GetGroupNumVehicleMinAge(this->vli.company, g_id, this->vli.vtype);
		Money profit_last_year_min_age = GetGroupProfitLastYearMinAge(this->vli.company, g_id, this->vli.vtype);
		if (num_vehicle_min_age == 0) {
			spr = SPR_PROFIT_NA;
		} else if (profit_last_year_min_age < 0) {
			spr = SPR_PROFIT_NEGATIVE;
		} else if (profit_last_year_min_age < VEHICLE_PROFIT_THRESHOLD * num_vehicle_min_age) {
			spr = SPR_PROFIT_SOME;
		} else {
			spr = SPR_PROFIT_LOT;
		}
		DrawSprite(spr, PAL_NONE, x, y + (this->tiny_step_height - this->column_size[VGC_PROFIT].height) / 2);

		/* draw the number of vehicles of the group */
		x = rtl ? x - WidgetDimensions::scaled.hsep_normal - this->column_size[VGC_NUMBER].width : x + WidgetDimensions::scaled.hsep_normal + this->column_size[VGC_PROFIT].width;
		int num_vehicle_with_subgroups = GetGroupNumVehicle(this->vli.company, g_id, this->vli.vtype);
		int num_vehicle = GroupStatistics::Get(this->vli.company, g_id, this->vli.vtype).num_vehicle;
		if (IsAllGroupID(g_id) || IsDefaultGroupID(g_id) || num_vehicle_with_subgroups == num_vehicle) {
			SetDParam(0, num_vehicle);
			DrawString(x, x + this->column_size[VGC_NUMBER].width - 1, y + (this->tiny_step_height - this->column_size[VGC_NUMBER].height) / 2, STR_JUST_COMMA, colour, SA_RIGHT | SA_FORCE, false, FS_SMALL);
		} else {
			SetDParam(0, num_vehicle);
			SetDParam(1, num_vehicle_with_subgroups - num_vehicle);
			DrawString(x, x + this->column_size[VGC_NUMBER].width - 1, y + (this->tiny_step_height - this->column_size[VGC_NUMBER].height) / 2, STR_GROUP_COUNT_WITH_SUBGROUP, colour, SA_RIGHT | SA_FORCE);
		}
	}

	/**
	 * Mark the widget containing the currently highlighted group as dirty.
	 */
	void DirtyHighlightedGroupWidget()
	{
		if (this->group_over == INVALID_GROUP) return;

		if (IsAllGroupID(this->group_over)) {
			this->SetWidgetDirty(WID_GL_ALL_VEHICLES);
		} else if (IsDefaultGroupID(this->group_over)) {
			this->SetWidgetDirty(WID_GL_DEFAULT_VEHICLES);
		} else {
			this->SetWidgetDirty(WID_GL_LIST_GROUP);
		}
	}

public:
	VehicleGroupWindow(WindowDesc *desc, WindowNumber window_number) : BaseVehicleListWindow(desc, window_number)
	{
		this->CreateNestedTree();

		this->vscroll = this->GetScrollbar(WID_GL_LIST_VEHICLE_SCROLLBAR);
		this->group_sb = this->GetScrollbar(WID_GL_LIST_GROUP_SCROLLBAR);

		this->vli.index = ALL_GROUP;
		this->group_sel = INVALID_GROUP;
		this->group_rename = INVALID_GROUP;
		this->group_over = INVALID_GROUP;

		this->groups.ForceRebuild();
		this->groups.NeedResort();
		this->BuildGroupList(vli.company);
		this->group_sb->SetCount(this->groups.size());

		this->GetWidget<NWidgetCore>(WID_GL_CAPTION)->widget_data = STR_VEHICLE_LIST_TRAIN_CAPTION + this->vli.vtype;
		this->GetWidget<NWidgetCore>(WID_GL_LIST_VEHICLE)->tool_tip = STR_VEHICLE_LIST_TRAIN_LIST_TOOLTIP + this->vli.vtype;

		this->GetWidget<NWidgetCore>(WID_GL_CREATE_GROUP)->widget_data += this->vli.vtype;
		this->GetWidget<NWidgetCore>(WID_GL_RENAME_GROUP)->widget_data += this->vli.vtype;
		this->GetWidget<NWidgetCore>(WID_GL_DELETE_GROUP)->widget_data += this->vli.vtype;
		this->GetWidget<NWidgetCore>(WID_GL_LIVERY_GROUP)->widget_data += this->vli.vtype;
		this->GetWidget<NWidgetCore>(WID_GL_REPLACE_PROTECTION)->widget_data += this->vli.vtype;

		this->FinishInitNested(window_number);
		this->owner = vli.company;

		this->BuildVehicleList();
		this->SortVehicleList();
	}

	~VehicleGroupWindow()
	{
		*this->sorting = this->vehgroups.GetListing();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_GL_LIST_GROUP:
				size->width = this->ComputeGroupInfoSize();
				resize->height = this->tiny_step_height;
				fill->height = this->tiny_step_height;
				break;

			case WID_GL_ALL_VEHICLES:
			case WID_GL_DEFAULT_VEHICLES:
				size->width = this->ComputeGroupInfoSize();
				size->height = this->tiny_step_height;
				break;

			case WID_GL_SORT_BY_ORDER: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case WID_GL_LIST_VEHICLE:
				this->ComputeGroupInfoSize();
				resize->height = GetVehicleListHeight(this->vli.vtype, this->tiny_step_height);
				size->height = 4 * resize->height;
				break;

			case WID_GL_GROUP_BY_DROPDOWN:
				size->width = GetStringListWidth(this->vehicle_group_by_names) + padding.width;
				break;

			case WID_GL_SORT_BY_DROPDOWN:
				size->width = GetStringListWidth(this->vehicle_group_none_sorter_names);
				size->width = std::max(size->width, GetStringListWidth(this->vehicle_group_shared_orders_sorter_names));
				size->width += padding.width;
				break;

			case WID_GL_FILTER_BY_CARGO:
				size->width = std::max(size->width, GetDropDownListDimension(this->BuildCargoDropDownList(true)).width + padding.width);
				break;

			case WID_GL_MANAGE_VEHICLES_DROPDOWN: {
				Dimension d = this->GetActionDropdownSize(true, true, true);
				d.height += padding.height;
				d.width  += padding.width;
				*size = maxdim(*size, d);
				break;
			}
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (data == 0) {
			/* This needs to be done in command-scope to enforce rebuilding before resorting invalid data */
			this->vehgroups.ForceRebuild();
			this->groups.ForceRebuild();
		} else {
			this->vehgroups.ForceResort();
			this->groups.ForceResort();
		}

		/* Process ID-invalidation in command-scope as well */
		if (this->group_rename != INVALID_GROUP && !Group::IsValidID(this->group_rename)) {
			CloseWindowByClass(WC_QUERY_STRING);
			this->group_rename = INVALID_GROUP;
		}

		if (!(IsAllGroupID(this->vli.index) || IsDefaultGroupID(this->vli.index) || Group::IsValidID(this->vli.index))) {
			this->vli.index = ALL_GROUP;
			this->CloseChildWindows(WC_DROPDOWN_MENU);
		}
		this->SetDirty();
	}

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_GL_FILTER_BY_CARGO:
				SetDParam(0, this->GetCargoFilterLabel(this->cargo_filter_criteria));
				break;

			case WID_GL_AVAILABLE_VEHICLES:
				SetDParam(0, STR_VEHICLE_LIST_AVAILABLE_TRAINS + this->vli.vtype);
				break;

			case WID_GL_CAPTION:
				/* If selected_group == DEFAULT_GROUP || ALL_GROUP, draw the standard caption
				 * We list all vehicles or ungrouped vehicles */
				if (IsDefaultGroupID(this->vli.index) || IsAllGroupID(this->vli.index)) {
					SetDParam(0, STR_COMPANY_NAME);
					SetDParam(1, this->vli.company);
					SetDParam(2, this->vehicles.size());
					SetDParam(3, this->vehicles.size());
				} else {
					uint num_vehicle = GetGroupNumVehicle(this->vli.company, this->vli.index, this->vli.vtype);

					SetDParam(0, STR_GROUP_NAME);
					SetDParam(1, this->vli.index);
					SetDParam(2, num_vehicle);
					SetDParam(3, num_vehicle);
				}
				break;
		}
	}

	void OnPaint() override
	{
		/* If we select the all vehicles, this->list will contain all vehicles of the owner
		 * else this->list will contain all vehicles which belong to the selected group */
		this->BuildVehicleList();
		this->SortVehicleList();

		this->BuildGroupList(this->owner);

		this->group_sb->SetCount(this->groups.size());
		this->vscroll->SetCount(this->vehgroups.size());

		/* The drop down menu is out, *but* it may not be used, retract it. */
		if (this->vehicles.empty() && this->IsWidgetLowered(WID_GL_MANAGE_VEHICLES_DROPDOWN)) {
			this->RaiseWidget(WID_GL_MANAGE_VEHICLES_DROPDOWN);
			this->CloseChildWindows(WC_DROPDOWN_MENU);
		}

		/* Disable all lists management button when the list is empty */
		this->SetWidgetsDisabledState(this->vehicles.empty() || _local_company != this->vli.company,
				WID_GL_STOP_ALL,
				WID_GL_START_ALL,
				WID_GL_MANAGE_VEHICLES_DROPDOWN);

		/* Disable the group specific function when we select the default group or all vehicles */
		this->SetWidgetsDisabledState(IsDefaultGroupID(this->vli.index) || IsAllGroupID(this->vli.index) || _local_company != this->vli.company,
				WID_GL_DELETE_GROUP,
				WID_GL_RENAME_GROUP,
				WID_GL_LIVERY_GROUP,
				WID_GL_REPLACE_PROTECTION);

		/* Disable remaining buttons for non-local companies
		 * Needed while changing _local_company, eg. by cheats
		 * All procedures (eg. move vehicle to another group)
		 *  verify, whether you are the owner of the vehicle,
		 *  so it doesn't have to be disabled
		 */
		this->SetWidgetsDisabledState(_local_company != this->vli.company,
				WID_GL_CREATE_GROUP,
				WID_GL_AVAILABLE_VEHICLES);

		/* If not a default group and the group has replace protection, show an enabled replace sprite. */
		uint16_t protect_sprite = SPR_GROUP_REPLACE_OFF_TRAIN;
		if (!IsDefaultGroupID(this->vli.index) && !IsAllGroupID(this->vli.index) && HasBit(Group::Get(this->vli.index)->flags, GroupFlags::GF_REPLACE_PROTECTION)) protect_sprite = SPR_GROUP_REPLACE_ON_TRAIN;
		this->GetWidget<NWidgetCore>(WID_GL_REPLACE_PROTECTION)->widget_data = protect_sprite + this->vli.vtype;

		/* Set text of "group by" dropdown widget. */
		this->GetWidget<NWidgetCore>(WID_GL_GROUP_BY_DROPDOWN)->widget_data = this->vehicle_group_by_names[this->grouping];

		/* Set text of "sort by" dropdown widget. */
		this->GetWidget<NWidgetCore>(WID_GL_SORT_BY_DROPDOWN)->widget_data = this->GetVehicleSorterNames()[this->vehgroups.SortType()];

		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_GL_ALL_VEHICLES:
				DrawGroupInfo(r.top, r.left, r.right, ALL_GROUP);
				break;

			case WID_GL_DEFAULT_VEHICLES:
				DrawGroupInfo(r.top, r.left, r.right, DEFAULT_GROUP);
				break;

			case WID_GL_INFO: {
				Money this_year = 0;
				Money last_year = 0;
				uint64_t occupancy = 0;

				for (const Vehicle * const v : this->vehicles) {
					assert(v->owner == this->owner);

					this_year += v->GetDisplayProfitThisYear();
					last_year += v->GetDisplayProfitLastYear();
					occupancy += v->trip_occupancy;
				}

				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);

				DrawString(tr, STR_GROUP_PROFIT_THIS_YEAR, TC_BLACK);
				SetDParam(0, this_year);
				DrawString(tr, STR_JUST_CURRENCY_LONG, TC_BLACK, SA_RIGHT);

				tr.top += GetCharacterHeight(FS_NORMAL);
				DrawString(tr, STR_GROUP_PROFIT_LAST_YEAR, TC_BLACK);
				SetDParam(0, last_year);
				DrawString(tr, STR_JUST_CURRENCY_LONG, TC_BLACK, SA_RIGHT);

				tr.top += GetCharacterHeight(FS_NORMAL);
				DrawString(tr, STR_GROUP_OCCUPANCY, TC_BLACK);
				const size_t vehicle_count = this->vehicles.size();
				if (vehicle_count > 0) {
					SetDParam(0, occupancy / vehicle_count);
					DrawString(tr, STR_GROUP_OCCUPANCY_VALUE, TC_BLACK, SA_RIGHT);
				}

				break;
			}

			case WID_GL_LIST_GROUP: {
				int y1 = r.top;
				size_t max = std::min<size_t>(this->group_sb->GetPosition() + this->group_sb->GetCapacity(), this->groups.size());
				for (size_t i = this->group_sb->GetPosition(); i < max; ++i) {
					const Group *g = this->groups[i];

					assert(g->owner == this->owner);

					DrawGroupInfo(y1, r.left, r.right, g->index, this->indents[i] * WidgetDimensions::scaled.hsep_indent, HasBit(g->flags, GroupFlags::GF_REPLACE_PROTECTION), g->folded || (i + 1 < this->groups.size() && indents[i + 1] > this->indents[i]));

					y1 += this->tiny_step_height;
				}
				if ((uint)this->group_sb->GetPosition() + this->group_sb->GetCapacity() > this->groups.size()) {
					DrawGroupInfo(y1, r.left, r.right, NEW_GROUP);
				}
				break;
			}

			case WID_GL_SORT_BY_ORDER:
				this->DrawSortButtonState(WID_GL_SORT_BY_ORDER, this->vehgroups.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case WID_GL_LIST_VEHICLE:
				if (this->vli.index != ALL_GROUP && this->grouping == GB_NONE) {
					/* Mark vehicles which are in sub-groups (only if we are not using shared order coalescing) */
					Rect mr = r.WithHeight(this->resize.step_height);
					size_t max = std::min<size_t>(this->vscroll->GetPosition() + this->vscroll->GetCapacity(), this->vehgroups.size());
					for (size_t i = this->vscroll->GetPosition(); i < max; ++i) {
						const Vehicle *v = this->vehgroups[i].GetSingleVehicle();
						if (v->group_id != this->vli.index) {
							GfxFillRect(mr.Shrink(WidgetDimensions::scaled.bevel), _colour_gradient[COLOUR_GREY][3], FILLRECT_CHECKER);
						}
						mr = mr.Translate(0, this->resize.step_height);
					}
				}

				this->DrawVehicleListItems(this->vehicle_sel, this->resize.step_height, r);
				break;
		}
	}

	static void DeleteGroupCallback(Window *win, bool confirmed)
	{
		if (confirmed) {
			VehicleGroupWindow *w = (VehicleGroupWindow*)win;
			w->vli.index = ALL_GROUP;
			Command<CMD_DELETE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_DELETE, w->group_confirm);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_GL_SORT_BY_ORDER: // Flip sorting method ascending/descending
				this->vehgroups.ToggleSortOrder();
				this->SetDirty();
				break;

			case WID_GL_GROUP_BY_DROPDOWN: // Select grouping option dropdown menu
				ShowDropDownMenu(this, this->vehicle_group_by_names, this->grouping, WID_GL_GROUP_BY_DROPDOWN, 0, 0);
				return;

			case WID_GL_SORT_BY_DROPDOWN: // Select sorting criteria dropdown menu
				ShowDropDownMenu(this, this->GetVehicleSorterNames(), this->vehgroups.SortType(),  WID_GL_SORT_BY_DROPDOWN, 0, (this->vli.vtype == VEH_TRAIN || this->vli.vtype == VEH_ROAD) ? 0 : (1 << 10));
				return;

			case WID_GL_FILTER_BY_CARGO: // Select filtering criteria dropdown menu
				ShowDropDownList(this, this->BuildCargoDropDownList(false), this->cargo_filter_criteria, widget);
				break;

			case WID_GL_ALL_VEHICLES: // All vehicles button
				if (!IsAllGroupID(this->vli.index)) {
					this->vli.index = ALL_GROUP;
					this->vehgroups.ForceRebuild();
					this->SetDirty();
				}
				break;

			case WID_GL_DEFAULT_VEHICLES: // Ungrouped vehicles button
				if (!IsDefaultGroupID(this->vli.index)) {
					this->vli.index = DEFAULT_GROUP;
					this->vehgroups.ForceRebuild();
					this->SetDirty();
				}
				break;

			case WID_GL_LIST_GROUP: { // Matrix Group
				auto it = this->group_sb->GetScrolledItemFromWidget(this->groups, pt.y, this, WID_GL_LIST_GROUP);
				if (it == this->groups.end()) return;

				size_t id_g = it - this->groups.begin();
				if ((*it)->folded || (id_g + 1 < this->groups.size() && this->indents[id_g + 1] > this->indents[id_g])) {
					/* The group has children, check if the user clicked the fold / unfold button. */
					NWidgetCore *group_display = this->GetWidget<NWidgetCore>(widget);
					int x = _current_text_dir == TD_RTL ?
							group_display->pos_x + group_display->current_x - WidgetDimensions::scaled.framerect.right - this->indents[id_g] * WidgetDimensions::scaled.hsep_indent - this->column_size[VGC_FOLD].width :
							group_display->pos_x + WidgetDimensions::scaled.framerect.left + this->indents[id_g] * WidgetDimensions::scaled.hsep_indent;
					if (click_count > 1 || (pt.x >= x && pt.x < (int)(x + this->column_size[VGC_FOLD].width))) {

						GroupID g = this->vli.index;
						if (!IsAllGroupID(g) && !IsDefaultGroupID(g)) {
							do {
								g = Group::Get(g)->parent;
								if (g == groups[id_g]->index) {
									this->vli.index = g;
									break;
								}
							} while (g != INVALID_GROUP);
						}

						Group::Get(groups[id_g]->index)->folded = !groups[id_g]->folded;
						this->groups.ForceRebuild();

						this->SetDirty();
						break;
					}
				}

				this->group_sel = this->vli.index = this->groups[id_g]->index;

				SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_DRAG, this);

				this->vehgroups.ForceRebuild();
				this->SetDirty();
				break;
			}

			case WID_GL_LIST_VEHICLE: { // Matrix Vehicle
				auto it = this->vscroll->GetScrolledItemFromWidget(this->vehgroups, pt.y, this, WID_GL_LIST_VEHICLE);
				if (it == this->vehgroups.end()) return; // click out of list bound

				const GUIVehicleGroup &vehgroup = *it;

				const Vehicle *v = nullptr;

				switch (this->grouping) {
					case GB_NONE: {
						const Vehicle *v2 = vehgroup.GetSingleVehicle();
						if (VehicleClicked(v2)) break;
						v = v2;
						break;
					}

					case GB_SHARED_ORDERS: {
						assert(vehgroup.NumVehicles() > 0);
						v = vehgroup.vehicles_begin[0];
						/*
						 * No VehicleClicked(v) support for now, because don't want
						 * to enable any contextual actions except perhaps clicking/ctrl-clicking to clone orders.
						 */
						break;
					}

					default:
						NOT_REACHED();
				}
				if (v) {
					if (_ctrl_pressed && this->grouping == GB_SHARED_ORDERS) {
						ShowOrdersWindow(v);
					} else {
						this->vehicle_sel = v->index;

						if (_ctrl_pressed && this->grouping == GB_NONE) {
							/*
							 * It only makes sense to select a group if not using shared orders
							 * since two vehicles sharing orders can be from different groups.
							 */
							this->SelectGroup(v->group_id);
						}

						SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_DRAG, this);
						SetMouseCursorVehicle(v, EIT_IN_LIST);
						_cursor.vehchain = true;

						this->SetDirty();
					}
				}

				break;
			}

			case WID_GL_CREATE_GROUP: { // Create a new group
				Command<CMD_CREATE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_CREATE, CcCreateGroup, this->vli.vtype, this->vli.index);
				break;
			}

			case WID_GL_DELETE_GROUP: { // Delete the selected group
				this->group_confirm = this->vli.index;
				ShowQuery(STR_QUERY_GROUP_DELETE_CAPTION, STR_GROUP_DELETE_QUERY_TEXT, this, DeleteGroupCallback);
				break;
			}

			case WID_GL_RENAME_GROUP: // Rename the selected roup
				this->ShowRenameGroupWindow(this->vli.index, false);
				break;

			case WID_GL_LIVERY_GROUP: // Set group livery
				ShowCompanyLiveryWindow(this->owner, this->vli.index);
				break;

			case WID_GL_AVAILABLE_VEHICLES:
				ShowBuildVehicleWindow(INVALID_TILE, this->vli.vtype);
				break;

			case WID_GL_MANAGE_VEHICLES_DROPDOWN: {
				ShowDropDownList(this, this->BuildActionDropdownList(true, Group::IsValidID(this->vli.index), IsDefaultGroupID(this->vli.index)), 0, WID_GL_MANAGE_VEHICLES_DROPDOWN);
				break;
			}

			case WID_GL_START_ALL:
			case WID_GL_STOP_ALL: { // Start/stop all vehicles of the list
				Command<CMD_MASS_START_STOP>::Post(0, widget == WID_GL_START_ALL, true, this->vli);
				break;
			}

			case WID_GL_REPLACE_PROTECTION: {
				const Group *g = Group::GetIfValid(this->vli.index);
				if (g != nullptr) {
					Command<CMD_SET_GROUP_FLAG>::Post(this->vli.index, GroupFlags::GF_REPLACE_PROTECTION, !HasBit(g->flags, GroupFlags::GF_REPLACE_PROTECTION), _ctrl_pressed);
				}
				break;
			}
		}
	}

	void OnDragDrop_Group(Point pt, WidgetID widget)
	{
		const Group *g = Group::Get(this->group_sel);

		switch (widget) {
			case WID_GL_ALL_VEHICLES: // All vehicles
			case WID_GL_DEFAULT_VEHICLES: // Ungrouped vehicles
				if (g->parent != INVALID_GROUP) {
					Command<CMD_ALTER_GROUP>::Post(STR_ERROR_GROUP_CAN_T_SET_PARENT, AlterGroupMode::SetParent, this->group_sel, INVALID_GROUP, {});
				}

				this->group_sel = INVALID_GROUP;
				this->group_over = INVALID_GROUP;
				this->SetDirty();
				break;

			case WID_GL_LIST_GROUP: { // Matrix group
				auto it = this->group_sb->GetScrolledItemFromWidget(this->groups, pt.y, this, WID_GL_LIST_GROUP);
				GroupID new_g = it == this->groups.end() ? INVALID_GROUP : (*it)->index;

				if (this->group_sel != new_g && g->parent != new_g) {
					Command<CMD_ALTER_GROUP>::Post(STR_ERROR_GROUP_CAN_T_SET_PARENT, AlterGroupMode::SetParent, this->group_sel, new_g, {});
				}

				this->group_sel = INVALID_GROUP;
				this->group_over = INVALID_GROUP;
				this->SetDirty();
				break;
			}
		}
	}

	void OnDragDrop_Vehicle(Point pt, WidgetID widget)
	{
		switch (widget) {
			case WID_GL_DEFAULT_VEHICLES: // Ungrouped vehicles
				Command<CMD_ADD_VEHICLE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_ADD_VEHICLE, DEFAULT_GROUP, this->vehicle_sel, _ctrl_pressed || this->grouping == GB_SHARED_ORDERS, VehicleListIdentifier{});

				this->vehicle_sel = INVALID_VEHICLE;
				this->group_over = INVALID_GROUP;

				this->SetDirty();
				break;

			case WID_GL_LIST_GROUP: { // Matrix group
				const VehicleID vindex = this->vehicle_sel;
				this->vehicle_sel = INVALID_VEHICLE;
				this->group_over = INVALID_GROUP;
				this->SetDirty();

				auto it = this->group_sb->GetScrolledItemFromWidget(this->groups, pt.y, this, WID_GL_LIST_GROUP);
				GroupID new_g = it == this->groups.end() ? NEW_GROUP : (*it)->index;

				Command<CMD_ADD_VEHICLE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_ADD_VEHICLE, new_g == NEW_GROUP ? CcAddVehicleNewGroup : nullptr, new_g, vindex, _ctrl_pressed || this->grouping == GB_SHARED_ORDERS, VehicleListIdentifier{});
				break;
			}

			case WID_GL_LIST_VEHICLE: { // Matrix vehicle
				const VehicleID vindex = this->vehicle_sel;
				this->vehicle_sel = INVALID_VEHICLE;
				this->group_over = INVALID_GROUP;
				this->SetDirty();

				auto it = this->vscroll->GetScrolledItemFromWidget(this->vehgroups, pt.y, this, WID_GL_LIST_VEHICLE);
				if (it == this->vehgroups.end()) return; // click out of list bound

				const GUIVehicleGroup &vehgroup = *it;
				switch (this->grouping) {
					case GB_NONE: {
						const Vehicle *v = vehgroup.GetSingleVehicle();
						if (!VehicleClicked(v) && vindex == v->index) {
							ShowVehicleViewWindow(v);
						}
						break;
					}

					case GB_SHARED_ORDERS: {
						if (!VehicleClicked(vehgroup)) {
							const Vehicle *v = vehgroup.vehicles_begin[0];
							if (vindex == v->index) {
								if (vehgroup.NumVehicles() == 1) {
									ShowVehicleViewWindow(v);
								} else {
									ShowVehicleListWindow(v);
								}
							}
						}
						break;
					}

					default:
						NOT_REACHED();
				}
				break;
			}
		}
	}

	void OnDragDrop(Point pt, WidgetID widget) override
	{
		if (this->vehicle_sel != INVALID_VEHICLE) OnDragDrop_Vehicle(pt, widget);
		if (this->group_sel != INVALID_GROUP) OnDragDrop_Group(pt, widget);

		_cursor.vehchain = false;
	}

	void OnQueryTextFinished(char *str) override
	{
		if (str != nullptr) Command<CMD_ALTER_GROUP>::Post(STR_ERROR_GROUP_CAN_T_RENAME, AlterGroupMode::Rename, this->group_rename, 0, str);
		this->group_rename = INVALID_GROUP;
	}

	void OnResize() override
	{
		this->group_sb->SetCapacityFromWidget(this, WID_GL_LIST_GROUP);
		this->vscroll->SetCapacityFromWidget(this, WID_GL_LIST_VEHICLE);
	}

	void OnDropdownSelect(WidgetID widget, int index) override
	{
		switch (widget) {
			case WID_GL_GROUP_BY_DROPDOWN:
				this->UpdateVehicleGroupBy(static_cast<GroupBy>(index));
				break;

			case WID_GL_SORT_BY_DROPDOWN:
				this->vehgroups.SetSortType(index);
				break;

			case WID_GL_FILTER_BY_CARGO: // Select a cargo filter criteria
				this->SetCargoFilter(index);
				break;

			case WID_GL_MANAGE_VEHICLES_DROPDOWN:
				assert(!this->vehicles.empty());

				switch (index) {
					case ADI_REPLACE: // Replace window
						ShowReplaceGroupVehicleWindow(this->vli.index, this->vli.vtype);
						break;
					case ADI_SERVICE: // Send for servicing
					case ADI_DEPOT: { // Send to Depots
						Command<CMD_SEND_VEHICLE_TO_DEPOT>::Post(GetCmdSendToDepotMsg(this->vli.vtype), 0, DepotCommand::MassSend | (index == ADI_SERVICE ? DepotCommand::Service : DepotCommand::None), this->vli);
						break;
					}

					case ADI_CREATE_GROUP: // Create group
						Command<CMD_ADD_VEHICLE_GROUP>::Post(CcAddVehicleNewGroup, NEW_GROUP, INVALID_VEHICLE, false, this->vli);
						break;

					case ADI_ADD_SHARED: // Add shared Vehicles
						assert(Group::IsValidID(this->vli.index));

						Command<CMD_ADD_SHARED_VEHICLE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_ADD_SHARED_VEHICLE, this->vli.index, this->vli.vtype);
						break;
					case ADI_REMOVE_ALL: // Remove all Vehicles from the selected group
						assert(Group::IsValidID(this->vli.index));

						Command<CMD_REMOVE_ALL_VEHICLES_GROUP>::Post(STR_ERROR_GROUP_CAN_T_REMOVE_ALL_VEHICLES, this->vli.index);
						break;
					default: NOT_REACHED();
				}
				break;

			default: NOT_REACHED();
		}

		this->SetDirty();
	}

	void OnGameTick() override
	{
		if (this->groups.NeedResort() || this->vehgroups.NeedResort()) {
			this->SetDirty();
		}
	}

	void OnPlaceObjectAbort() override
	{
		/* abort drag & drop */
		this->vehicle_sel = INVALID_VEHICLE;
		this->DirtyHighlightedGroupWidget();
		this->group_sel = INVALID_GROUP;
		this->group_over = INVALID_GROUP;
		this->SetWidgetDirty(WID_GL_LIST_VEHICLE);
	}

	void OnMouseDrag(Point pt, WidgetID widget) override
	{
		if (this->vehicle_sel == INVALID_VEHICLE && this->group_sel == INVALID_GROUP) return;

		/* A vehicle is dragged over... */
		GroupID new_group_over = INVALID_GROUP;
		switch (widget) {
			case WID_GL_DEFAULT_VEHICLES: // ... the 'default' group.
				new_group_over = DEFAULT_GROUP;
				break;

			case WID_GL_LIST_GROUP: { // ... the list of custom groups.
				auto it = this->group_sb->GetScrolledItemFromWidget(this->groups, pt.y, this, WID_GL_LIST_GROUP);
				new_group_over = it == this->groups.end() ? NEW_GROUP : (*it)->index;
				break;
			}
		}

		/* Do not highlight when dragging over the current group */
		if (this->vehicle_sel != INVALID_VEHICLE) {
			if (Vehicle::Get(vehicle_sel)->group_id == new_group_over) new_group_over = INVALID_GROUP;
		} else if (this->group_sel != INVALID_GROUP) {
			if (this->group_sel == new_group_over || Group::Get(this->group_sel)->parent == new_group_over) new_group_over = INVALID_GROUP;
		}

		/* Mark widgets as dirty if the group changed. */
		if (new_group_over != this->group_over) {
			this->DirtyHighlightedGroupWidget();
			this->group_over = new_group_over;
			this->DirtyHighlightedGroupWidget();
		}
	}

	void ShowRenameGroupWindow(GroupID group, bool empty)
	{
		assert(Group::IsValidID(group));
		this->group_rename = group;
		/* Show empty query for new groups */
		StringID str = STR_EMPTY;
		if (!empty) {
			SetDParam(0, group);
			str = STR_GROUP_NAME;
		}
		ShowQueryString(str, STR_GROUP_RENAME_CAPTION, MAX_LENGTH_GROUP_NAME_CHARS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT | QSF_LEN_IN_CHARS);
	}

	/**
	 * Tests whether a given vehicle is selected in the window, and unselects it if necessary.
	 * Called when the vehicle is deleted.
	 * @param vehicle Vehicle that is going to be deleted
	 */
	void UnselectVehicle(VehicleID vehicle)
	{
		if (this->vehicle_sel == vehicle) ResetObjectToPlace();
	}

	/**
	 * Selects the specified group in the list
	 *
	 * @param g_id The ID of the group to be selected
	 */
	void SelectGroup(const GroupID g_id)
	{
		if (g_id == INVALID_GROUP || g_id == this->vli.index) return;

		this->vli.index = g_id;
		if (g_id != ALL_GROUP && g_id != DEFAULT_GROUP) {
			const Group *g = Group::Get(g_id);
			int id_g = find_index(this->groups, g);
			// The group's branch is maybe collapsed, so try to expand it
			if (id_g == -1) {
				for (auto pg = Group::GetIfValid(g->parent); pg != nullptr; pg = Group::GetIfValid(pg->parent)) {
					pg->folded = false;
				}
				this->groups.ForceRebuild();
				this->BuildGroupList(this->owner);
				this->group_sb->SetCount(this->groups.size());
				id_g = find_index(this->groups, g);
			}
			this->group_sb->ScrollTowards(id_g);
		}
		this->vehgroups.ForceRebuild();
		this->SetDirty();
	}

};


static WindowDesc _other_group_desc(__FILE__, __LINE__,
	WDP_AUTO, "list_groups", 460, 246,
	WC_INVALID, WC_NONE,
	0,
	std::begin(_nested_group_widgets), std::end(_nested_group_widgets)
);

static WindowDesc _train_group_desc(__FILE__, __LINE__,
	WDP_AUTO, "list_groups_train", 525, 246,
	WC_TRAINS_LIST, WC_NONE,
	0,
	std::begin(_nested_group_widgets), std::end(_nested_group_widgets)
);

/**
 * Show the group window for the given company and vehicle type.
 * @param company The company to show the window for.
 * @param vehicle_type The type of vehicle to show it for.
 * @param group The group to be selected. Defaults to INVALID_GROUP.
 * @param need_existing_window Whether the existing window is needed. Defaults to false.
 */
void ShowCompanyGroup(CompanyID company, VehicleType vehicle_type, GroupID group = INVALID_GROUP, bool need_existing_window = false)
{
	if (!Company::IsValidID(company)) return;

	const WindowNumber num = VehicleListIdentifier(VL_GROUP_LIST, vehicle_type, company).Pack();
	VehicleGroupWindow *w;
	if (vehicle_type == VEH_TRAIN) {
		w = AllocateWindowDescFront<VehicleGroupWindow>(&_train_group_desc, num, need_existing_window);
	} else {
		_other_group_desc.cls = GetWindowClassForVehicleType(vehicle_type);
		w = AllocateWindowDescFront<VehicleGroupWindow>(&_other_group_desc, num, need_existing_window);
	}
	if (w != nullptr) w->SelectGroup(group);
}

/**
 * Show the group window for the given vehicle.
 * @param v The vehicle to show the window for.
 */
void ShowCompanyGroupForVehicle(const Vehicle *v)
{
	ShowCompanyGroup(v->owner, v->type, v->group_id, true);
}

/**
 * Finds a group list window determined by vehicle type and owner
 * @param vt vehicle type
 * @param owner owner of groups
 * @return pointer to VehicleGroupWindow, nullptr if not found
 */
static inline VehicleGroupWindow *FindVehicleGroupWindow(VehicleType vt, Owner owner)
{
	return (VehicleGroupWindow *)FindWindowById(GetWindowClassForVehicleType(vt), VehicleListIdentifier(VL_GROUP_LIST, vt, owner).Pack());
}

/**
 * Opens a 'Rename group' window for newly created group.
 * @param veh_type Vehicle type.
 */
static void CcCreateGroup(GroupID gid, VehicleType veh_type)
{
	VehicleGroupWindow *w = FindVehicleGroupWindow(veh_type, _current_company);
	if (w != nullptr) w->ShowRenameGroupWindow(gid, true);
}

/**
 * Opens a 'Rename group' window for newly created group.
 * @param result Did command succeed?
 * @param new_group ID of the created group.
 * @param vt Vehicle type.
 * @see CmdCreateGroup
 */
void CcCreateGroup(Commands, const CommandCost &result, GroupID new_group, VehicleType vt, GroupID)
{
	if (result.Failed()) return;

	assert(vt <= VEH_AIRCRAFT);
	CcCreateGroup(new_group, vt);
}

/**
 * Open rename window after adding a vehicle to a new group via drag and drop.
 * @param result Did command succeed?
 * @param new_group ID of the created group.
 */
void CcAddVehicleNewGroup(Commands, const CommandCost &result, GroupID new_group, GroupID, VehicleID, bool, const VehicleListIdentifier &)
{
	if (result.Failed()) return;

	const Group *g = Group::Get(new_group);
	CcCreateGroup(new_group, g->vehicle_type);
}

/**
 * Removes the highlight of a vehicle in a group window
 * @param *v Vehicle to remove all highlights from
 */
void DeleteGroupHighlightOfVehicle(const Vehicle *v)
{
	/* If we haven't got any vehicles on the mouse pointer, we haven't got any highlighted in any group windows either
	 * If that is the case, we can skip looping though the windows and save time
	 */
	if (_special_mouse_mode != WSM_DRAGDROP) return;

	VehicleGroupWindow *w = FindVehicleGroupWindow(v->type, v->owner);
	if (w != nullptr) w->UnselectVehicle(v->index);
}
