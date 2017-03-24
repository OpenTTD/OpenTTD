/* $Id$ */

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
#include "company_base.h"

#include "widgets/group_widget.h"

#include "table/sprites.h"

#include "safeguards.h"

static const int LEVEL_WIDTH = 10; ///< Indenting width of a sub-group in pixels

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
		NWidget(NWID_VERTICAL),
			NWidget(WWT_PANEL, COLOUR_GREY), SetMinimalTextLines(1, WD_DROPDOWNTEXT_TOP + WD_DROPDOWNTEXT_BOTTOM), SetFill(1, 0), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_GL_ALL_VEHICLES), SetFill(1, 0), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_GL_DEFAULT_VEHICLES), SetFill(1, 0), EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_MATRIX, COLOUR_GREY, WID_GL_LIST_GROUP), SetMatrixDataTip(1, 0, STR_GROUPS_CLICK_ON_GROUP_FOR_TOOLTIP),
						SetFill(1, 0), SetResize(0, 1), SetScrollbar(WID_GL_LIST_GROUP_SCROLLBAR),
				NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_GL_LIST_GROUP_SCROLLBAR),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_GL_INFO), SetFill(1, 0), EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_GL_CREATE_GROUP), SetFill(0, 1),
						SetDataTip(SPR_GROUP_CREATE_TRAIN, STR_GROUP_CREATE_TOOLTIP),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_GL_DELETE_GROUP), SetFill(0, 1),
						SetDataTip(SPR_GROUP_DELETE_TRAIN, STR_GROUP_DELETE_TOOLTIP),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_GL_RENAME_GROUP), SetFill(0, 1),
						SetDataTip(SPR_GROUP_RENAME_TRAIN, STR_GROUP_RENAME_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1), EndContainer(),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_GL_REPLACE_PROTECTION), SetFill(0, 1),
						SetDataTip(SPR_GROUP_REPLACE_OFF_TRAIN, STR_GROUP_REPLACE_PROTECTION_TOOLTIP),
			EndContainer(),
		EndContainer(),
		/* right part */
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GL_SORT_BY_ORDER), SetMinimalSize(81, 12), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GL_SORT_BY_DROPDOWN), SetMinimalSize(167, 12), SetDataTip(0x0, STR_TOOLTIP_SORT_CRITERIA),
				NWidget(WWT_PANEL, COLOUR_GREY), SetMinimalSize(12, 12), SetResize(1, 0), EndContainer(),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_MATRIX, COLOUR_GREY, WID_GL_LIST_VEHICLE), SetMinimalSize(248, 0), SetMatrixDataTip(1, 0, STR_NULL), SetResize(1, 1), SetFill(1, 0), SetScrollbar(WID_GL_LIST_VEHICLE_SCROLLBAR),
				NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_GL_LIST_VEHICLE_SCROLLBAR),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY), SetMinimalSize(1, 0), SetFill(1, 1), SetResize(1, 0), EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GL_AVAILABLE_VEHICLES), SetMinimalSize(106, 12), SetFill(0, 1),
						SetDataTip(STR_BLACK_STRING, STR_VEHICLE_LIST_AVAILABLE_ENGINES_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetMinimalSize(0, 12), SetFill(1, 1), SetResize(1, 0), EndContainer(),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GL_MANAGE_VEHICLES_DROPDOWN), SetMinimalSize(118, 12), SetFill(0, 1),
						SetDataTip(STR_VEHICLE_LIST_MANAGE_LIST, STR_VEHICLE_LIST_MANAGE_LIST_TOOLTIP),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_GL_STOP_ALL), SetMinimalSize(12, 12), SetFill(0, 1),
						SetDataTip(SPR_FLAG_VEH_STOPPED, STR_VEHICLE_LIST_MASS_STOP_LIST_TOOLTIP),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_GL_START_ALL), SetMinimalSize(12, 12), SetFill(0, 1),
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
		VGC_NAME,          ///< Group name.
		VGC_PROTECT,       ///< Autoreplace protect icon.
		VGC_AUTOREPLACE,   ///< Autoreplace active icon.
		VGC_PROFIT,        ///< Profit icon.
		VGC_NUMBER,        ///< Number of vehicles in the group.

		VGC_END
	};

	VehicleID vehicle_sel; ///< Selected vehicle
	GroupID group_sel;     ///< Selected group (for drag/drop)
	GroupID group_rename;  ///< Group being renamed, INVALID_GROUP if none
	GroupID group_over;    ///< Group over which a vehicle is dragged, INVALID_GROUP if none
	GroupID group_confirm; ///< Group awaiting delete confirmation
	GUIGroupList groups;   ///< List of groups
	uint tiny_step_height; ///< Step height for the group list
	Scrollbar *group_sb;

	SmallVector<int, 16> indents; ///< Indentation levels

	Dimension column_size[VGC_END]; ///< Size of the columns in the group list.

	void AddParents(GUIGroupList *source, GroupID parent, int indent)
	{
		for (const Group **g = source->Begin(); g != source->End(); g++) {
			if ((*g)->parent == parent) {
				*this->groups.Append() = *g;
				*this->indents.Append() = indent;
				AddParents(source, (*g)->index, indent + 1);
			}
		}
	}

	/** Sort the groups by their name */
	static int CDECL GroupNameSorter(const Group * const *a, const Group * const *b)
	{
		static const Group *last_group[2] = { NULL, NULL };
		static char         last_name[2][64] = { "", "" };

		if (*a != last_group[0]) {
			last_group[0] = *a;
			SetDParam(0, (*a)->index);
			GetString(last_name[0], STR_GROUP_NAME, lastof(last_name[0]));
		}

		if (*b != last_group[1]) {
			last_group[1] = *b;
			SetDParam(0, (*b)->index);
			GetString(last_name[1], STR_GROUP_NAME, lastof(last_name[1]));
		}

		int r = strnatcmp(last_name[0], last_name[1]); // Sort by name (natural sorting).
		if (r == 0) return (*a)->index - (*b)->index;
		return r;
	}

	/**
	 * (Re)Build the group list.
	 *
	 * @param owner The owner of the window
	 */
	void BuildGroupList(Owner owner)
	{
		if (!this->groups.NeedRebuild()) return;

		this->groups.Clear();
		this->indents.Clear();

		GUIGroupList list;

		const Group *g;
		FOR_ALL_GROUPS(g) {
			if (g->owner == owner && g->vehicle_type == this->vli.vtype) {
				*list.Append() = g;
			}
		}

		list.ForceResort();
		list.Sort(&GroupNameSorter);

		AddParents(&list, INVALID_GROUP, 0);

		this->groups.Compact();
		this->groups.RebuildDone();
	}

	/**
	 * Compute tiny_step_height and column_size
	 * @return Total width required for the group list.
	 */
	uint ComputeGroupInfoSize()
	{
		this->column_size[VGC_NAME] = maxdim(GetStringBoundingBox(STR_GROUP_DEFAULT_TRAINS + this->vli.vtype), GetStringBoundingBox(STR_GROUP_ALL_TRAINS + this->vli.vtype));
		this->column_size[VGC_NAME].width = max(170u, this->column_size[VGC_NAME].width);
		this->tiny_step_height = this->column_size[VGC_NAME].height;

		this->column_size[VGC_PROTECT] = GetSpriteSize(SPR_GROUP_REPLACE_PROTECT);
		this->tiny_step_height = max(this->tiny_step_height, this->column_size[VGC_PROTECT].height);

		this->column_size[VGC_AUTOREPLACE] = GetSpriteSize(SPR_GROUP_REPLACE_ACTIVE);
		this->tiny_step_height = max(this->tiny_step_height, this->column_size[VGC_AUTOREPLACE].height);

		this->column_size[VGC_PROFIT].width = 0;
		this->column_size[VGC_PROFIT].height = 0;
		static const SpriteID profit_sprites[] = {SPR_PROFIT_NA, SPR_PROFIT_NEGATIVE, SPR_PROFIT_SOME, SPR_PROFIT_LOT};
		for (uint i = 0; i < lengthof(profit_sprites); i++) {
			Dimension d = GetSpriteSize(profit_sprites[i]);
			this->column_size[VGC_PROFIT] = maxdim(this->column_size[VGC_PROFIT], d);
		}
		this->tiny_step_height = max(this->tiny_step_height, this->column_size[VGC_PROFIT].height);

		SetDParamMaxValue(0, GroupStatistics::Get(this->vli.company, ALL_GROUP, this->vli.vtype).num_vehicle, 3, FS_SMALL);
		this->column_size[VGC_NUMBER] = GetStringBoundingBox(STR_TINY_COMMA);
		this->tiny_step_height = max(this->tiny_step_height, this->column_size[VGC_NUMBER].height);

		this->tiny_step_height += WD_MATRIX_TOP;

		return WD_FRAMERECT_LEFT + 8 +
			this->column_size[VGC_NAME].width + 8 +
			this->column_size[VGC_PROTECT].width + 2 +
			this->column_size[VGC_AUTOREPLACE].width + 2 +
			this->column_size[VGC_PROFIT].width + 2 +
			this->column_size[VGC_NUMBER].width + 2 +
			WD_FRAMERECT_RIGHT;
	}

	/**
	 * Draw a row in the group list.
	 * @param y Top of the row.
	 * @param left Left of the row.
	 * @param right Right of the row.
	 * @param g_id Group to list.
	 * @param indent Indentation level.
	 * @param protection Whether autoreplace protection is set.
	 */
	void DrawGroupInfo(int y, int left, int right, GroupID g_id, int indent = 0, bool protection = false) const
	{
		/* Highlight the group if a vehicle is dragged over it */
		if (g_id == this->group_over) {
			GfxFillRect(left + WD_FRAMERECT_LEFT, y + WD_FRAMERECT_TOP, right - WD_FRAMERECT_RIGHT, y + this->tiny_step_height - WD_FRAMERECT_BOTTOM - WD_MATRIX_TOP, _colour_gradient[COLOUR_GREY][7]);
		}

		if (g_id == NEW_GROUP) return;

		/* draw the selected group in white, else we draw it in black */
		TextColour colour = g_id == this->vli.index ? TC_WHITE : TC_BLACK;
		const GroupStatistics &stats = GroupStatistics::Get(this->vli.company, g_id, this->vli.vtype);
		bool rtl = _current_text_dir == TD_RTL;

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
		int x = rtl ? right - WD_FRAMERECT_RIGHT - 8 - this->column_size[VGC_NAME].width + 1 : left + WD_FRAMERECT_LEFT + 8;
		DrawString(x + indent * LEVEL_WIDTH, x + this->column_size[VGC_NAME].width - 1, y + (this->tiny_step_height - this->column_size[VGC_NAME].height) / 2, str, colour);

		/* draw autoreplace protection */
		x = rtl ? x - 8 - this->column_size[VGC_PROTECT].width : x + 8 + this->column_size[VGC_NAME].width;
		if (protection) DrawSprite(SPR_GROUP_REPLACE_PROTECT, PAL_NONE, x, y + (this->tiny_step_height - this->column_size[VGC_PROTECT].height) / 2);

		/* draw autoreplace status */
		x = rtl ? x - 2 - this->column_size[VGC_AUTOREPLACE].width : x + 2 + this->column_size[VGC_PROTECT].width;
		if (stats.autoreplace_defined) DrawSprite(SPR_GROUP_REPLACE_ACTIVE, stats.autoreplace_finished ? PALETTE_CRASH : PAL_NONE, x, y + (this->tiny_step_height - this->column_size[VGC_AUTOREPLACE].height) / 2);

		/* draw the profit icon */
		x = rtl ? x - 2 - this->column_size[VGC_PROFIT].width : x + 2 + this->column_size[VGC_AUTOREPLACE].width;
		SpriteID spr;
		if (stats.num_profit_vehicle == 0) {
			spr = SPR_PROFIT_NA;
		} else if (stats.profit_last_year < 0) {
			spr = SPR_PROFIT_NEGATIVE;
		} else if (stats.profit_last_year < 10000 * stats.num_profit_vehicle) { // TODO magic number
			spr = SPR_PROFIT_SOME;
		} else {
			spr = SPR_PROFIT_LOT;
		}
		DrawSprite(spr, PAL_NONE, x, y + (this->tiny_step_height - this->column_size[VGC_PROFIT].height) / 2);

		/* draw the number of vehicles of the group */
		x = rtl ? x - 2 - this->column_size[VGC_NUMBER].width : x + 2 + this->column_size[VGC_PROFIT].width;
		SetDParam(0, stats.num_vehicle);
		DrawString(x, x + this->column_size[VGC_NUMBER].width - 1, y + (this->tiny_step_height - this->column_size[VGC_NUMBER].height) / 2, STR_TINY_COMMA, colour, SA_RIGHT | SA_FORCE);
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

		switch (this->vli.vtype) {
			default: NOT_REACHED();
			case VEH_TRAIN:    this->sorting = &_sorting.train;    break;
			case VEH_ROAD:     this->sorting = &_sorting.roadveh;  break;
			case VEH_SHIP:     this->sorting = &_sorting.ship;     break;
			case VEH_AIRCRAFT: this->sorting = &_sorting.aircraft; break;
		}

		this->vli.index = ALL_GROUP;
		this->vehicle_sel = INVALID_VEHICLE;
		this->group_sel = INVALID_GROUP;
		this->group_rename = INVALID_GROUP;
		this->group_over = INVALID_GROUP;

		this->vehicles.SetListing(*this->sorting);
		this->vehicles.ForceRebuild();
		this->vehicles.NeedResort();

		this->BuildVehicleList();
		this->SortVehicleList();

		this->groups.ForceRebuild();
		this->groups.NeedResort();
		this->BuildGroupList(vli.company);

		this->GetWidget<NWidgetCore>(WID_GL_CAPTION)->widget_data = STR_VEHICLE_LIST_TRAIN_CAPTION + this->vli.vtype;
		this->GetWidget<NWidgetCore>(WID_GL_LIST_VEHICLE)->tool_tip = STR_VEHICLE_LIST_TRAIN_LIST_TOOLTIP + this->vli.vtype;

		this->GetWidget<NWidgetCore>(WID_GL_CREATE_GROUP)->widget_data += this->vli.vtype;
		this->GetWidget<NWidgetCore>(WID_GL_RENAME_GROUP)->widget_data += this->vli.vtype;
		this->GetWidget<NWidgetCore>(WID_GL_DELETE_GROUP)->widget_data += this->vli.vtype;
		this->GetWidget<NWidgetCore>(WID_GL_REPLACE_PROTECTION)->widget_data += this->vli.vtype;

		this->FinishInitNested(window_number);
		this->owner = vli.company;
	}

	~VehicleGroupWindow()
	{
		*this->sorting = this->vehicles.GetListing();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_GL_LIST_GROUP: {
				size->width = this->ComputeGroupInfoSize();
				resize->height = this->tiny_step_height;

				/* Minimum height is the height of the list widget minus all and default vehicles... */
				size->height =  4 * GetVehicleListHeight(this->vli.vtype, this->tiny_step_height) - 2 * this->tiny_step_height;

				/* ... minus the buttons at the bottom ... */
				uint max_icon_height = GetSpriteSize(this->GetWidget<NWidgetCore>(WID_GL_CREATE_GROUP)->widget_data).height;
				max_icon_height = max(max_icon_height, GetSpriteSize(this->GetWidget<NWidgetCore>(WID_GL_RENAME_GROUP)->widget_data).height);
				max_icon_height = max(max_icon_height, GetSpriteSize(this->GetWidget<NWidgetCore>(WID_GL_DELETE_GROUP)->widget_data).height);
				max_icon_height = max(max_icon_height, GetSpriteSize(this->GetWidget<NWidgetCore>(WID_GL_REPLACE_PROTECTION)->widget_data).height);

				/* ... minus the height of the group info ... */
				max_icon_height += (FONT_HEIGHT_NORMAL * 3) + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;

				/* Get a multiple of tiny_step_height of that amount */
				size->height = Ceil(size->height - max_icon_height, tiny_step_height);
				break;
			}

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

			case WID_GL_MANAGE_VEHICLES_DROPDOWN: {
				Dimension d = this->GetActionDropdownSize(true, true);
				d.height += padding.height;
				d.width  += padding.width;
				*size = maxdim(*size, d);
				break;
			}

			case WID_GL_INFO: {
				size->height = (FONT_HEIGHT_NORMAL * 3) + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				break;
			}
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (data == 0) {
			/* This needs to be done in command-scope to enforce rebuilding before resorting invalid data */
			this->vehicles.ForceRebuild();
			this->groups.ForceRebuild();
		} else {
			this->vehicles.ForceResort();
			this->groups.ForceResort();
		}

		/* Process ID-invalidation in command-scope as well */
		if (this->group_rename != INVALID_GROUP && !Group::IsValidID(this->group_rename)) {
			DeleteWindowByClass(WC_QUERY_STRING);
			this->group_rename = INVALID_GROUP;
		}

		if (!(IsAllGroupID(this->vli.index) || IsDefaultGroupID(this->vli.index) || Group::IsValidID(this->vli.index))) {
			this->vli.index = ALL_GROUP;
			HideDropDownMenu(this);
		}
		this->SetDirty();
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_GL_AVAILABLE_VEHICLES:
				SetDParam(0, STR_VEHICLE_LIST_AVAILABLE_TRAINS + this->vli.vtype);
				break;

			case WID_GL_CAPTION:
				/* If selected_group == DEFAULT_GROUP || ALL_GROUP, draw the standard caption
				 * We list all vehicles or ungrouped vehicles */
				if (IsDefaultGroupID(this->vli.index) || IsAllGroupID(this->vli.index)) {
					SetDParam(0, STR_COMPANY_NAME);
					SetDParam(1, this->vli.company);
					SetDParam(2, this->vehicles.Length());
					SetDParam(3, this->vehicles.Length());
				} else {
					const Group *g = Group::Get(this->vli.index);

					SetDParam(0, STR_GROUP_NAME);
					SetDParam(1, g->index);
					SetDParam(2, g->statistics.num_vehicle);
					SetDParam(3, g->statistics.num_vehicle);
				}
				break;
		}
	}

	virtual void OnPaint()
	{
		/* If we select the all vehicles, this->list will contain all vehicles of the owner
		 * else this->list will contain all vehicles which belong to the selected group */
		this->BuildVehicleList();
		this->SortVehicleList();

		this->BuildGroupList(this->owner);

		this->group_sb->SetCount(this->groups.Length());
		this->vscroll->SetCount(this->vehicles.Length());

		/* The drop down menu is out, *but* it may not be used, retract it. */
		if (this->vehicles.Length() == 0 && this->IsWidgetLowered(WID_GL_MANAGE_VEHICLES_DROPDOWN)) {
			this->RaiseWidget(WID_GL_MANAGE_VEHICLES_DROPDOWN);
			HideDropDownMenu(this);
		}

		/* Disable all lists management button when the list is empty */
		this->SetWidgetsDisabledState(this->vehicles.Length() == 0 || _local_company != this->vli.company,
				WID_GL_STOP_ALL,
				WID_GL_START_ALL,
				WID_GL_MANAGE_VEHICLES_DROPDOWN,
				WIDGET_LIST_END);

		/* Disable the group specific function when we select the default group or all vehicles */
		this->SetWidgetsDisabledState(IsDefaultGroupID(this->vli.index) || IsAllGroupID(this->vli.index) || _local_company != this->vli.company,
				WID_GL_DELETE_GROUP,
				WID_GL_RENAME_GROUP,
				WID_GL_REPLACE_PROTECTION,
				WIDGET_LIST_END);

		/* Disable remaining buttons for non-local companies
		 * Needed while changing _local_company, eg. by cheats
		 * All procedures (eg. move vehicle to another group)
		 *  verify, whether you are the owner of the vehicle,
		 *  so it doesn't have to be disabled
		 */
		this->SetWidgetsDisabledState(_local_company != this->vli.company,
				WID_GL_CREATE_GROUP,
				WID_GL_AVAILABLE_VEHICLES,
				WIDGET_LIST_END);

		/* If not a default group and the group has replace protection, show an enabled replace sprite. */
		uint16 protect_sprite = SPR_GROUP_REPLACE_OFF_TRAIN;
		if (!IsDefaultGroupID(this->vli.index) && !IsAllGroupID(this->vli.index) && Group::Get(this->vli.index)->replace_protection) protect_sprite = SPR_GROUP_REPLACE_ON_TRAIN;
		this->GetWidget<NWidgetCore>(WID_GL_REPLACE_PROTECTION)->widget_data = protect_sprite + this->vli.vtype;

		/* Set text of sort by dropdown */
		this->GetWidget<NWidgetCore>(WID_GL_SORT_BY_DROPDOWN)->widget_data = this->vehicle_sorter_names[this->vehicles.SortType()];

		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_GL_ALL_VEHICLES:
				DrawGroupInfo(r.top + WD_FRAMERECT_TOP, r.left, r.right, ALL_GROUP);
				break;

			case WID_GL_DEFAULT_VEHICLES:
				DrawGroupInfo(r.top + WD_FRAMERECT_TOP, r.left, r.right, DEFAULT_GROUP);
				break;

			case WID_GL_INFO: {
				Money this_year = 0;
				Money last_year = 0;
				uint32 occupancy = 0;
				uint32 vehicle_count = this->vehicles.Length();

				for (uint i = 0; i < vehicle_count; i++) {
					const Vehicle *v = this->vehicles[i];
					assert(v->owner == this->owner);

					this_year += v->GetDisplayProfitThisYear();
					last_year += v->GetDisplayProfitLastYear();
					occupancy += v->trip_occupancy;
				}

				const int left  = r.left + WD_FRAMERECT_LEFT + 8;
				const int right = r.right - WD_FRAMERECT_RIGHT - 8;

				int y = r.top + WD_FRAMERECT_TOP;
				DrawString(left, right, y, STR_GROUP_PROFIT_THIS_YEAR, TC_BLACK);
				SetDParam(0, this_year);
				DrawString(left, right, y, STR_JUST_CURRENCY_LONG, TC_BLACK, SA_RIGHT);

				y += FONT_HEIGHT_NORMAL;
				DrawString(left, right, y, STR_GROUP_PROFIT_LAST_YEAR, TC_BLACK);
				SetDParam(0, last_year);
				DrawString(left, right, y, STR_JUST_CURRENCY_LONG, TC_BLACK, SA_RIGHT);

				y += FONT_HEIGHT_NORMAL;
				DrawString(left, right, y, STR_GROUP_OCCUPANCY, TC_BLACK);
				if (vehicle_count > 0) {
					SetDParam(0, occupancy / vehicle_count);
					DrawString(left, right, y, STR_GROUP_OCCUPANCY_VALUE, TC_BLACK, SA_RIGHT);
				}

				break;
			}

			case WID_GL_LIST_GROUP: {
				int y1 = r.top + WD_FRAMERECT_TOP;
				int max = min(this->group_sb->GetPosition() + this->group_sb->GetCapacity(), this->groups.Length());
				for (int i = this->group_sb->GetPosition(); i < max; ++i) {
					const Group *g = this->groups[i];

					assert(g->owner == this->owner);

					DrawGroupInfo(y1, r.left, r.right, g->index, this->indents[i], g->replace_protection);

					y1 += this->tiny_step_height;
				}
				if ((uint)this->group_sb->GetPosition() + this->group_sb->GetCapacity() > this->groups.Length()) {
					DrawGroupInfo(y1, r.left, r.right, NEW_GROUP);
				}
				break;
			}

			case WID_GL_SORT_BY_ORDER:
				this->DrawSortButtonState(WID_GL_SORT_BY_ORDER, this->vehicles.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case WID_GL_LIST_VEHICLE:
				if (this->vli.index != ALL_GROUP) {
					/* Mark vehicles which are in sub-groups */
					int y = r.top;
					uint max = min(this->vscroll->GetPosition() + this->vscroll->GetCapacity(), this->vehicles.Length());
					for (uint i = this->vscroll->GetPosition(); i < max; ++i) {
						const Vehicle *v = this->vehicles[i];
						if (v->group_id != this->vli.index) {
							GfxFillRect(r.left + 1, y + 1, r.right - 1, y + this->resize.step_height - 2, _colour_gradient[COLOUR_GREY][3], FILLRECT_CHECKER);
						}
						y += this->resize.step_height;
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
			DoCommandP(0, w->group_confirm, 0, CMD_DELETE_GROUP | CMD_MSG(STR_ERROR_GROUP_CAN_T_DELETE));
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_GL_SORT_BY_ORDER: // Flip sorting method ascending/descending
				this->vehicles.ToggleSortOrder();
				this->SetDirty();
				break;

			case WID_GL_SORT_BY_DROPDOWN: // Select sorting criteria dropdown menu
				ShowDropDownMenu(this, this->vehicle_sorter_names, this->vehicles.SortType(),  WID_GL_SORT_BY_DROPDOWN, 0, (this->vli.vtype == VEH_TRAIN || this->vli.vtype == VEH_ROAD) ? 0 : (1 << 10));
				return;

			case WID_GL_ALL_VEHICLES: // All vehicles button
				if (!IsAllGroupID(this->vli.index)) {
					this->vli.index = ALL_GROUP;
					this->vehicles.ForceRebuild();
					this->SetDirty();
				}
				break;

			case WID_GL_DEFAULT_VEHICLES: // Ungrouped vehicles button
				if (!IsDefaultGroupID(this->vli.index)) {
					this->vli.index = DEFAULT_GROUP;
					this->vehicles.ForceRebuild();
					this->SetDirty();
				}
				break;

			case WID_GL_LIST_GROUP: { // Matrix Group
				uint id_g = this->group_sb->GetScrolledRowFromWidget(pt.y, this, WID_GL_LIST_GROUP, 0, this->tiny_step_height);
				if (id_g >= this->groups.Length()) return;

				this->group_sel = this->vli.index = this->groups[id_g]->index;

				SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_DRAG, this);

				this->vehicles.ForceRebuild();
				this->SetDirty();
				break;
			}

			case WID_GL_LIST_VEHICLE: { // Matrix Vehicle
				uint id_v = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_GL_LIST_VEHICLE);
				if (id_v >= this->vehicles.Length()) return; // click out of list bound

				const Vehicle *v = this->vehicles[id_v];
				if (VehicleClicked(v)) break;

				this->vehicle_sel = v->index;

				SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_DRAG, this);
				SetMouseCursorVehicle(v, EIT_IN_LIST);
				_cursor.vehchain = true;

				this->SetDirty();
				break;
			}

			case WID_GL_CREATE_GROUP: { // Create a new group
				DoCommandP(0, this->vli.vtype, 0, CMD_CREATE_GROUP | CMD_MSG(STR_ERROR_GROUP_CAN_T_CREATE), CcCreateGroup);
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

			case WID_GL_AVAILABLE_VEHICLES:
				ShowBuildVehicleWindow(INVALID_TILE, this->vli.vtype);
				break;

			case WID_GL_MANAGE_VEHICLES_DROPDOWN: {
				DropDownList *list = this->BuildActionDropdownList(true, Group::IsValidID(this->vli.index));
				ShowDropDownList(this, list, 0, WID_GL_MANAGE_VEHICLES_DROPDOWN);
				break;
			}

			case WID_GL_START_ALL:
			case WID_GL_STOP_ALL: { // Start/stop all vehicles of the list
				DoCommandP(0, (1 << 1) | (widget == WID_GL_START_ALL ? (1 << 0) : 0), this->vli.Pack(), CMD_MASS_START_STOP);
				break;
			}

			case WID_GL_REPLACE_PROTECTION: {
				const Group *g = Group::GetIfValid(this->vli.index);
				if (g != NULL) {
					DoCommandP(0, this->vli.index, (g->replace_protection ? 0 : 1) | (_ctrl_pressed << 1), CMD_SET_GROUP_REPLACE_PROTECTION);
				}
				break;
			}
		}
	}

	void OnDragDrop_Group(Point pt, int widget)
	{
		const Group *g = Group::Get(this->group_sel);

		switch (widget) {
			case WID_GL_ALL_VEHICLES: // All vehicles
			case WID_GL_DEFAULT_VEHICLES: // Ungroupd vehicles
				if (g->parent != INVALID_GROUP) {
					DoCommandP(0, this->group_sel | (1 << 16), INVALID_GROUP, CMD_ALTER_GROUP | CMD_MSG(STR_ERROR_GROUP_CAN_T_SET_PARENT));
				}

				this->group_sel = INVALID_GROUP;
				this->group_over = INVALID_GROUP;
				this->SetDirty();
				break;

			case WID_GL_LIST_GROUP: { // Matrix group
				uint id_g = this->group_sb->GetScrolledRowFromWidget(pt.y, this, WID_GL_LIST_GROUP, 0, this->tiny_step_height);
				GroupID new_g = id_g >= this->groups.Length() ? INVALID_GROUP : this->groups[id_g]->index;

				if (this->group_sel != new_g && g->parent != new_g) {
					DoCommandP(0, this->group_sel | (1 << 16), new_g, CMD_ALTER_GROUP | CMD_MSG(STR_ERROR_GROUP_CAN_T_SET_PARENT));
				}

				this->group_sel = INVALID_GROUP;
				this->group_over = INVALID_GROUP;
				this->SetDirty();
				break;
			}
		}
	}

	void OnDragDrop_Vehicle(Point pt, int widget)
	{
		switch (widget) {
			case WID_GL_DEFAULT_VEHICLES: // Ungrouped vehicles
				DoCommandP(0, DEFAULT_GROUP, this->vehicle_sel | (_ctrl_pressed ? 1 << 31 : 0), CMD_ADD_VEHICLE_GROUP | CMD_MSG(STR_ERROR_GROUP_CAN_T_ADD_VEHICLE));

				this->vehicle_sel = INVALID_VEHICLE;
				this->group_over = INVALID_GROUP;

				this->SetDirty();
				break;

			case WID_GL_LIST_GROUP: { // Matrix group
				const VehicleID vindex = this->vehicle_sel;
				this->vehicle_sel = INVALID_VEHICLE;
				this->group_over = INVALID_GROUP;
				this->SetDirty();

				uint id_g = this->group_sb->GetScrolledRowFromWidget(pt.y, this, WID_GL_LIST_GROUP, 0, this->tiny_step_height);
				GroupID new_g = id_g >= this->groups.Length() ? NEW_GROUP : this->groups[id_g]->index;

				DoCommandP(0, new_g, vindex | (_ctrl_pressed ? 1 << 31 : 0), CMD_ADD_VEHICLE_GROUP | CMD_MSG(STR_ERROR_GROUP_CAN_T_ADD_VEHICLE), new_g == NEW_GROUP ? CcAddVehicleNewGroup : NULL);
				break;
			}

			case WID_GL_LIST_VEHICLE: { // Matrix vehicle
				const VehicleID vindex = this->vehicle_sel;
				this->vehicle_sel = INVALID_VEHICLE;
				this->group_over = INVALID_GROUP;
				this->SetDirty();

				uint id_v = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_GL_LIST_VEHICLE);
				if (id_v >= this->vehicles.Length()) return; // click out of list bound

				const Vehicle *v = this->vehicles[id_v];
				if (!VehicleClicked(v) && vindex == v->index) {
					ShowVehicleViewWindow(v);
				}
				break;
			}
		}
	}

	virtual void OnDragDrop(Point pt, int widget)
	{
		if (this->vehicle_sel != INVALID_VEHICLE) OnDragDrop_Vehicle(pt, widget);
		if (this->group_sel != INVALID_GROUP) OnDragDrop_Group(pt, widget);

		_cursor.vehchain = false;
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str != NULL) DoCommandP(0, this->group_rename, 0, CMD_ALTER_GROUP | CMD_MSG(STR_ERROR_GROUP_CAN_T_RENAME), NULL, str);
		this->group_rename = INVALID_GROUP;
	}

	virtual void OnResize()
	{
		this->group_sb->SetCapacityFromWidget(this, WID_GL_LIST_GROUP);
		this->vscroll->SetCapacityFromWidget(this, WID_GL_LIST_VEHICLE);
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case WID_GL_SORT_BY_DROPDOWN:
				this->vehicles.SetSortType(index);
				break;

			case WID_GL_MANAGE_VEHICLES_DROPDOWN:
				assert(this->vehicles.Length() != 0);

				switch (index) {
					case ADI_REPLACE: // Replace window
						ShowReplaceGroupVehicleWindow(this->vli.index, this->vli.vtype);
						break;
					case ADI_SERVICE: // Send for servicing
					case ADI_DEPOT: { // Send to Depots
						DoCommandP(0, DEPOT_MASS_SEND | (index == ADI_SERVICE ? DEPOT_SERVICE : 0U), this->vli.Pack(), GetCmdSendToDepot(this->vli.vtype));
						break;
					}

					case ADI_ADD_SHARED: // Add shared Vehicles
						assert(Group::IsValidID(this->vli.index));

						DoCommandP(0, this->vli.index, this->vli.vtype, CMD_ADD_SHARED_VEHICLE_GROUP | CMD_MSG(STR_ERROR_GROUP_CAN_T_ADD_SHARED_VEHICLE));
						break;
					case ADI_REMOVE_ALL: // Remove all Vehicles from the selected group
						assert(Group::IsValidID(this->vli.index));

						DoCommandP(0, this->vli.index, 0, CMD_REMOVE_ALL_VEHICLES_GROUP | CMD_MSG(STR_ERROR_GROUP_CAN_T_REMOVE_ALL_VEHICLES));
						break;
					default: NOT_REACHED();
				}
				break;

			default: NOT_REACHED();
		}

		this->SetDirty();
	}

	virtual void OnTick()
	{
		if (_pause_mode != PM_UNPAUSED) return;
		if (this->groups.NeedResort() || this->vehicles.NeedResort()) {
			this->SetDirty();
		}
	}

	virtual void OnPlaceObjectAbort()
	{
		/* abort drag & drop */
		this->vehicle_sel = INVALID_VEHICLE;
		this->DirtyHighlightedGroupWidget();
		this->group_over = INVALID_GROUP;
		this->SetWidgetDirty(WID_GL_LIST_VEHICLE);
	}

	virtual void OnMouseDrag(Point pt, int widget)
	{
		if (this->vehicle_sel == INVALID_VEHICLE && this->group_sel == INVALID_GROUP) return;

		/* A vehicle is dragged over... */
		GroupID new_group_over = INVALID_GROUP;
		switch (widget) {
			case WID_GL_DEFAULT_VEHICLES: // ... the 'default' group.
				new_group_over = DEFAULT_GROUP;
				break;

			case WID_GL_LIST_GROUP: { // ... the list of custom groups.
				uint id_g = this->group_sb->GetScrolledRowFromWidget(pt.y, this, WID_GL_LIST_GROUP, 0, this->tiny_step_height);
				new_group_over = id_g >= this->groups.Length() ? NEW_GROUP : this->groups[id_g]->index;
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
};


static WindowDesc _other_group_desc(
	WDP_AUTO, "list_groups", 460, 246,
	WC_INVALID, WC_NONE,
	0,
	_nested_group_widgets, lengthof(_nested_group_widgets)
);

static WindowDesc _train_group_desc(
	WDP_AUTO, "list_groups_train", 525, 246,
	WC_TRAINS_LIST, WC_NONE,
	0,
	_nested_group_widgets, lengthof(_nested_group_widgets)
);

/**
 * Show the group window for the given company and vehicle type.
 * @param company The company to show the window for.
 * @param vehicle_type The type of vehicle to show it for.
 */
void ShowCompanyGroup(CompanyID company, VehicleType vehicle_type)
{
	if (!Company::IsValidID(company)) return;

	WindowNumber num = VehicleListIdentifier(VL_GROUP_LIST, vehicle_type, company).Pack();
	if (vehicle_type == VEH_TRAIN) {
		AllocateWindowDescFront<VehicleGroupWindow>(&_train_group_desc, num);
	} else {
		_other_group_desc.cls = GetWindowClassForVehicleType(vehicle_type);
		AllocateWindowDescFront<VehicleGroupWindow>(&_other_group_desc, num);
	}
}

/**
 * Finds a group list window determined by vehicle type and owner
 * @param vt vehicle type
 * @param owner owner of groups
 * @return pointer to VehicleGroupWindow, NULL if not found
 */
static inline VehicleGroupWindow *FindVehicleGroupWindow(VehicleType vt, Owner owner)
{
	return (VehicleGroupWindow *)FindWindowById(GetWindowClassForVehicleType(vt), VehicleListIdentifier(VL_GROUP_LIST, vt, owner).Pack());
}

/**
 * Opens a 'Rename group' window for newly created group
 * @param success did command succeed?
 * @param tile unused
 * @param p1 vehicle type
 * @param p2 unused
 * @see CmdCreateGroup
 */
void CcCreateGroup(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	if (result.Failed()) return;
	assert(p1 <= VEH_AIRCRAFT);

	VehicleGroupWindow *w = FindVehicleGroupWindow((VehicleType)p1, _current_company);
	if (w != NULL) w->ShowRenameGroupWindow(_new_group_id, true);
}

/**
 * Open rename window after adding a vehicle to a new group via drag and drop.
 * @param success Did command succeed?
 * @param tile Unused.
 * @param p1 Unused.
 * @param p2 Bit 0-19: Vehicle ID.
 */
void CcAddVehicleNewGroup(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	if (result.Failed()) return;
	assert(Vehicle::IsValidID(GB(p2, 0, 20)));

	CcCreateGroup(result, 0, Vehicle::Get(GB(p2, 0, 20))->type, 0);
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
	if (w != NULL) w->UnselectVehicle(v->index);
}
