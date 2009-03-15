/* $Id$ */

/** @file group_gui.cpp GUI for the group window. */

#include "stdafx.h"
#include "openttd.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "vehicle_gui.h"
#include "vehicle_gui_base.h"
#include "vehicle_base.h"
#include "group.h"
#include "strings_func.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "autoreplace_gui.h"
#include "gfx_func.h"
#include "company_func.h"
#include "widgets/dropdown_type.h"
#include "widgets/dropdown_func.h"
#include "tilehighlight_func.h"

#include "table/strings.h"
#include "table/sprites.h"

typedef GUIList<const Group*> GUIGroupList;

enum GroupListWidgets {
	GRP_WIDGET_CLOSEBOX = 0,
	GRP_WIDGET_CAPTION,
	GRP_WIDGET_STICKY,
	GRP_WIDGET_SORT_BY_ORDER,
	GRP_WIDGET_SORT_BY_DROPDOWN,
	GRP_WIDGET_EMPTY_TOP_RIGHT,
	GRP_WIDGET_LIST_VEHICLE,
	GRP_WIDGET_LIST_VEHICLE_SCROLLBAR,
	GRP_WIDGET_EMPTY2,
	GRP_WIDGET_AVAILABLE_VEHICLES,
	GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN,
	GRP_WIDGET_STOP_ALL,
	GRP_WIDGET_START_ALL,
	GRP_WIDGET_EMPTY_BOTTOM_RIGHT,
	GRP_WIDGET_RESIZE,

	GRP_WIDGET_EMPTY_TOP_LEFT,
	GRP_WIDGET_ALL_VEHICLES,
	GRP_WIDGET_DEFAULT_VEHICLES,
	GRP_WIDGET_LIST_GROUP,
	GRP_WIDGET_LIST_GROUP_SCROLLBAR,
	GRP_WIDGET_CREATE_GROUP,
	GRP_WIDGET_DELETE_GROUP,
	GRP_WIDGET_RENAME_GROUP,
	GRP_WIDGET_EMPTY1,
	GRP_WIDGET_REPLACE_PROTECTION,
};

enum GroupActionListFunction {
	GALF_REPLACE,
	GALF_SERVICE,
	GALF_DEPOT,
	GALF_ADD_SHARED,
	GALF_REMOVE_ALL,
};

/**
 * Update/redraw the group action dropdown
 * @param w   the window the dropdown belongs to
 * @param gid the currently selected group in the window
 */
static void ShowGroupActionDropdown(Window *w, GroupID gid)
{
	DropDownList *list = new DropDownList();

	list->push_back(new DropDownListStringItem(STR_REPLACE_VEHICLES,    GALF_REPLACE, false));
	list->push_back(new DropDownListStringItem(STR_SEND_FOR_SERVICING,  GALF_SERVICE, false));
	list->push_back(new DropDownListStringItem(STR_SEND_TRAIN_TO_DEPOT, GALF_DEPOT,   false));

	if (IsValidGroupID(gid)) {
		list->push_back(new DropDownListStringItem(STR_GROUP_ADD_SHARED_VEHICLE,  GALF_ADD_SHARED, false));
		list->push_back(new DropDownListStringItem(STR_GROUP_REMOVE_ALL_VEHICLES, GALF_REMOVE_ALL, false));
	}

	ShowDropDownList(w, list, 0, GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN);
}


static const Widget _group_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,             STR_018B_CLOSE_WINDOW},             // GRP_WIDGET_CLOSEBOX
{    WWT_CAPTION,  RESIZE_RIGHT,  COLOUR_GREY,    11,   447,     0,    13, 0x0,                  STR_018C_WINDOW_TITLE_DRAG_THIS},   // GRP_WIDGET_CAPTION
{  WWT_STICKYBOX,     RESIZE_LR,  COLOUR_GREY,   448,   459,     0,    13, 0x0,                  STR_STICKY_BUTTON},                 // GRP_WIDGET_STICKY
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   200,   280,    14,    25, STR_SORT_BY,          STR_SORT_ORDER_TIP},                // GRP_WIDGET_SORT_BY_ORDER
{   WWT_DROPDOWN,   RESIZE_NONE,  COLOUR_GREY,   281,   447,    14,    25, 0x0,                  STR_SORT_CRITERIA_TIP},             // GRP_WIDGET_SORT_BY_DROPDOWN
{      WWT_PANEL,  RESIZE_RIGHT,  COLOUR_GREY,   448,   459,    14,    25, 0x0,                  STR_NULL},                          // GRP_WIDGET_EMPTY_TOP_RIGHT
{     WWT_MATRIX,     RESIZE_RB,  COLOUR_GREY,   200,   447,    26,   181, 0x701,                STR_NULL},                          // GRP_WIDGET_LIST_VEHICLE
{  WWT_SCROLLBAR,    RESIZE_LRB,  COLOUR_GREY,   448,   459,    26,   181, 0x0,                  STR_0190_SCROLL_BAR_SCROLLS_LIST},  // GRP_WIDGET_LIST_VEHICLE_SCROLLBAR
{      WWT_PANEL,     RESIZE_TB,  COLOUR_GREY,   188,   199,   169,   193, 0x0,                  STR_NULL},                          // GRP_WIDGET_EMPTY2
{ WWT_PUSHTXTBTN,     RESIZE_TB,  COLOUR_GREY,   200,   305,   182,   193, 0x0,                  STR_AVAILABLE_ENGINES_TIP},         // GRP_WIDGET_AVAILABLE_VEHICLES
{   WWT_DROPDOWN,     RESIZE_TB,  COLOUR_GREY,   306,   423,   182,   193, STR_MANAGE_LIST,      STR_MANAGE_LIST_TIP},               // GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN
{ WWT_PUSHIMGBTN,     RESIZE_TB,  COLOUR_GREY,   424,   435,   182,   193, SPR_FLAG_VEH_STOPPED, STR_MASS_STOP_LIST_TIP},            // GRP_WIDGET_STOP_ALL
{ WWT_PUSHIMGBTN,     RESIZE_TB,  COLOUR_GREY,   436,   447,   182,   193, SPR_FLAG_VEH_RUNNING, STR_MASS_START_LIST_TIP},           // GRP_WIDGET_START_ALL
{      WWT_PANEL,    RESIZE_RTB,  COLOUR_GREY,   448,   447,   182,   193, 0x0,                  STR_NULL},                          // GRP_WIDGET_EMPTY_BOTTOM_RIGHT
{  WWT_RESIZEBOX,   RESIZE_LRTB,  COLOUR_GREY,   448,   459,   182,   193, 0x0,                  STR_RESIZE_BUTTON},                 // GRP_WIDGET_RESIZE

{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   199,    14,    25, 0x0,                  STR_NULL},                          // GRP_WIDGET_EMPTY_TOP_LEFT
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   199,    26,    38, 0x0,                  STR_NULL},                          // GRP_WIDGET_ALL_VEHICLES
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   199,    39,    51, 0x0,                  STR_NULL},                          // GRP_WIDGET_DEFAULT_VEHICLES
{     WWT_MATRIX, RESIZE_BOTTOM,  COLOUR_GREY,     0,   187,    52,   168, 0x701,                STR_GROUPS_CLICK_ON_GROUP_FOR_TIP}, // GRP_WIDGET_LIST_GROUP
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,  COLOUR_GREY,   188,   199,    52,   168, 0x0,                  STR_0190_SCROLL_BAR_SCROLLS_LIST},  // GRP_WIDGET_LIST_GROUP_SCROLLBAR
{ WWT_PUSHIMGBTN,     RESIZE_TB,  COLOUR_GREY,     0,    23,   169,   193, 0x0,                  STR_GROUP_CREATE_TIP},              // GRP_WIDGET_CREATE_GROUP
{ WWT_PUSHIMGBTN,     RESIZE_TB,  COLOUR_GREY,    24,    47,   169,   193, 0x0,                  STR_GROUP_DELETE_TIP},              // GRP_WIDGET_DELETE_GROUP
{ WWT_PUSHIMGBTN,     RESIZE_TB,  COLOUR_GREY,    48,    71,   169,   193, 0x0,                  STR_GROUP_RENAME_TIP},              // GRP_WIDGET_RENAME_GROUP
{      WWT_PANEL,     RESIZE_TB,  COLOUR_GREY,    72,   163,   169,   193, 0x0,                  STR_NULL},                          // GRP_WIDGET_EMPTY1
{ WWT_PUSHIMGBTN,     RESIZE_TB,  COLOUR_GREY,   164,   187,   169,   193, 0x0,                  STR_GROUP_REPLACE_PROTECTION_TIP},  // GRP_WIDGET_REPLACE_PROTECTION
{   WIDGETS_END},
};


class VehicleGroupWindow : public BaseVehicleListWindow {
private:
	GroupID group_sel;
	VehicleID vehicle_sel;
	GUIGroupList groups;

	/**
	 * (Re)Build the group list.
	 *
	 * @param owner The owner of the window
	 */
	void BuildGroupList(Owner owner)
	{
		if (!this->groups.NeedRebuild()) return;

		this->groups.Clear();

		const Group *g;
		FOR_ALL_GROUPS(g) {
			if (g->owner == owner && g->vehicle_type == this->vehicle_type) {
				*this->groups.Append() = g;
			}
		}

		this->groups.Compact();
		this->groups.RebuildDone();
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

		int r = strcmp(last_name[0], last_name[1]); // sort by name
		if (r == 0) return (*a)->index - (*b)->index;
		return r;
	}

public:
	VehicleGroupWindow(const WindowDesc *desc, WindowNumber window_number) : BaseVehicleListWindow(desc, window_number)
	{
		const Owner owner = (Owner)GB(this->window_number, 0, 8);
		this->vehicle_type = (VehicleType)GB(this->window_number, 11, 5);

		this->owner = owner;
		this->resize.step_width = 1;

		switch (this->vehicle_type) {
			default: NOT_REACHED();
			case VEH_TRAIN:
			case VEH_ROAD:
				this->vscroll2.cap = 9;
				this->vscroll.cap = 6;
				this->resize.step_height = PLY_WND_PRC__SIZE_OF_ROW_SMALL;
				break;
			case VEH_SHIP:
			case VEH_AIRCRAFT:
				this->vscroll2.cap = 9;
				this->vscroll.cap = 4;
				this->resize.step_height = PLY_WND_PRC__SIZE_OF_ROW_BIG;
				break;
		}

		this->widget[GRP_WIDGET_LIST_GROUP].data = (this->vscroll2.cap << 8) + 1;
		this->widget[GRP_WIDGET_LIST_VEHICLE].data = (this->vscroll.cap << 8) + 1;

		switch (this->vehicle_type) {
			default: NOT_REACHED();
			case VEH_TRAIN:    this->sorting = &_sorting.train;    break;
			case VEH_ROAD:     this->sorting = &_sorting.roadveh;  break;
			case VEH_SHIP:     this->sorting = &_sorting.ship;     break;
			case VEH_AIRCRAFT: this->sorting = &_sorting.aircraft; break;
		}

		this->vehicles.SetListing(*this->sorting);
		this->vehicles.ForceRebuild();
		this->vehicles.NeedResort();

		this->groups.ForceRebuild();
		this->groups.NeedResort();

		this->group_sel = ALL_GROUP;
		this->vehicle_sel = INVALID_VEHICLE;

		switch (this->vehicle_type) {
			case VEH_TRAIN:
				this->widget[GRP_WIDGET_LIST_VEHICLE].tooltips = STR_883D_TRAINS_CLICK_ON_TRAIN_FOR;
				this->widget[GRP_WIDGET_AVAILABLE_VEHICLES].data = STR_AVAILABLE_TRAINS;

				this->widget[GRP_WIDGET_CREATE_GROUP].data = SPR_GROUP_CREATE_TRAIN;
				this->widget[GRP_WIDGET_RENAME_GROUP].data = SPR_GROUP_RENAME_TRAIN;
				this->widget[GRP_WIDGET_DELETE_GROUP].data = SPR_GROUP_DELETE_TRAIN;
				break;

			case VEH_ROAD:
				this->widget[GRP_WIDGET_LIST_VEHICLE].tooltips = STR_901A_ROAD_VEHICLES_CLICK_ON;
				this->widget[GRP_WIDGET_AVAILABLE_VEHICLES].data = STR_AVAILABLE_ROAD_VEHICLES;

				this->widget[GRP_WIDGET_CREATE_GROUP].data = SPR_GROUP_CREATE_ROADVEH;
				this->widget[GRP_WIDGET_RENAME_GROUP].data = SPR_GROUP_RENAME_ROADVEH;
				this->widget[GRP_WIDGET_DELETE_GROUP].data = SPR_GROUP_DELETE_ROADVEH;
				break;

			case VEH_SHIP:
				this->widget[GRP_WIDGET_LIST_VEHICLE].tooltips = STR_9823_SHIPS_CLICK_ON_SHIP_FOR;
				this->widget[GRP_WIDGET_AVAILABLE_VEHICLES].data = STR_AVAILABLE_SHIPS;

				this->widget[GRP_WIDGET_CREATE_GROUP].data = SPR_GROUP_CREATE_SHIP;
				this->widget[GRP_WIDGET_RENAME_GROUP].data = SPR_GROUP_RENAME_SHIP;
				this->widget[GRP_WIDGET_DELETE_GROUP].data = SPR_GROUP_DELETE_SHIP;
				break;

			case VEH_AIRCRAFT:
				this->widget[GRP_WIDGET_LIST_VEHICLE].tooltips = STR_A01F_AIRCRAFT_CLICK_ON_AIRCRAFT;
				this->widget[GRP_WIDGET_AVAILABLE_VEHICLES].data = STR_AVAILABLE_AIRCRAFT;

				this->widget[GRP_WIDGET_CREATE_GROUP].data = SPR_GROUP_CREATE_AIRCRAFT;
				this->widget[GRP_WIDGET_RENAME_GROUP].data = SPR_GROUP_RENAME_AIRCRAFT;
				this->widget[GRP_WIDGET_DELETE_GROUP].data = SPR_GROUP_DELETE_AIRCRAFT;
				break;

			default: NOT_REACHED();
		}

		this->FindWindowPlacementAndResize(desc);
		if (this->vehicle_type == VEH_TRAIN) ResizeWindow(this, 65, 0);
	}

	~VehicleGroupWindow()
	{
		*this->sorting = this->vehicles.GetListing();
	}

	virtual void OnInvalidateData(int data)
	{
		if (data == 0) {
			this->vehicles.ForceRebuild();
			this->groups.ForceRebuild();
		} else {
			this->vehicles.ForceResort();
			this->groups.ForceResort();
		}

		if (!(IsAllGroupID(this->group_sel) || IsDefaultGroupID(this->group_sel) || IsValidGroupID(this->group_sel))) {
			this->group_sel = ALL_GROUP;
			HideDropDownMenu(this);
		}
		this->SetDirty();
	}

	virtual void OnPaint()
	{
		const Owner owner = (Owner)GB(this->window_number, 0, 8);
		int x = this->widget[GRP_WIDGET_LIST_VEHICLE].left + 2;
		int y1 = PLY_WND_PRC__OFFSET_TOP_WIDGET + 2;
		int max;
		int i;

		/* If we select the all vehicles, this->list will contain all vehicles of the owner
		 * else this->list will contain all vehicles which belong to the selected group */
		this->BuildVehicleList(owner, this->group_sel, IsAllGroupID(this->group_sel) ? VLW_STANDARD : VLW_GROUP_LIST);
		this->SortVehicleList();

		this->BuildGroupList(owner);
		this->groups.Sort(&GroupNameSorter);

		SetVScroll2Count(this, this->groups.Length());
		SetVScrollCount(this, this->vehicles.Length());

		/* The drop down menu is out, *but* it may not be used, retract it. */
		if (this->vehicles.Length() == 0 && this->IsWidgetLowered(GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN)) {
			this->RaiseWidget(GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN);
			HideDropDownMenu(this);
		}

		/* Disable all lists management button when the list is empty */
		this->SetWidgetsDisabledState(this->vehicles.Length() == 0 || _local_company != owner,
				GRP_WIDGET_STOP_ALL,
				GRP_WIDGET_START_ALL,
				GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN,
				WIDGET_LIST_END);

		/* Disable the group specific function when we select the default group or all vehicles */
		this->SetWidgetsDisabledState(IsDefaultGroupID(this->group_sel) || IsAllGroupID(this->group_sel) || _local_company != owner,
				GRP_WIDGET_DELETE_GROUP,
				GRP_WIDGET_RENAME_GROUP,
				GRP_WIDGET_REPLACE_PROTECTION,
				WIDGET_LIST_END);

		/* Disable remaining buttons for non-local companies
		 * Needed while changing _local_company, eg. by cheats
		 * All procedures (eg. move vehicle to another group)
		 *  verify, whether you are the owner of the vehicle,
		 *  so it doesn't have to be disabled
		 */
		this->SetWidgetsDisabledState(_local_company != owner,
				GRP_WIDGET_CREATE_GROUP,
				GRP_WIDGET_AVAILABLE_VEHICLES,
				WIDGET_LIST_END);


		/* If selected_group == DEFAULT_GROUP || ALL_GROUP, draw the standard caption
				We list all vehicles or ungrouped vehicles */
		if (IsDefaultGroupID(this->group_sel) || IsAllGroupID(this->group_sel)) {
			SetDParam(0, owner);
			SetDParam(1, this->vehicles.Length());

			switch (this->vehicle_type) {
				case VEH_TRAIN:
					this->widget[GRP_WIDGET_CAPTION].data = STR_881B_TRAINS;
					this->widget[GRP_WIDGET_REPLACE_PROTECTION].data = SPR_GROUP_REPLACE_OFF_TRAIN;
					break;
				case VEH_ROAD:
					this->widget[GRP_WIDGET_CAPTION].data = STR_9001_ROAD_VEHICLES;
					this->widget[GRP_WIDGET_REPLACE_PROTECTION].data = SPR_GROUP_REPLACE_OFF_ROADVEH;
					break;
				case VEH_SHIP:
					this->widget[GRP_WIDGET_CAPTION].data = STR_9805_SHIPS;
					this->widget[GRP_WIDGET_REPLACE_PROTECTION].data = SPR_GROUP_REPLACE_OFF_SHIP;
					break;
				case VEH_AIRCRAFT:
					this->widget[GRP_WIDGET_CAPTION].data =  STR_A009_AIRCRAFT;
					this->widget[GRP_WIDGET_REPLACE_PROTECTION].data = SPR_GROUP_REPLACE_OFF_AIRCRAFT;
					break;
				default: NOT_REACHED();
			}
		} else {
			const Group *g = GetGroup(this->group_sel);

			SetDParam(0, g->index);
			SetDParam(1, g->num_vehicle);

			switch (this->vehicle_type) {
				case VEH_TRAIN:
					this->widget[GRP_WIDGET_CAPTION].data = STR_GROUP_TRAINS_CAPTION;
					this->widget[GRP_WIDGET_REPLACE_PROTECTION].data = (g->replace_protection) ? SPR_GROUP_REPLACE_ON_TRAIN : SPR_GROUP_REPLACE_OFF_TRAIN;
					break;
				case VEH_ROAD:
					this->widget[GRP_WIDGET_CAPTION].data = STR_GROUP_ROADVEH_CAPTION;
					this->widget[GRP_WIDGET_REPLACE_PROTECTION].data = (g->replace_protection) ? SPR_GROUP_REPLACE_ON_ROADVEH : SPR_GROUP_REPLACE_OFF_ROADVEH;
					break;
				case VEH_SHIP:
					this->widget[GRP_WIDGET_CAPTION].data = STR_GROUP_SHIPS_CAPTION;
					this->widget[GRP_WIDGET_REPLACE_PROTECTION].data = (g->replace_protection) ? SPR_GROUP_REPLACE_ON_SHIP : SPR_GROUP_REPLACE_OFF_SHIP;
					break;
				case VEH_AIRCRAFT:
					this->widget[GRP_WIDGET_CAPTION].data = STR_GROUP_AIRCRAFTS_CAPTION;
					this->widget[GRP_WIDGET_REPLACE_PROTECTION].data = (g->replace_protection) ? SPR_GROUP_REPLACE_ON_AIRCRAFT : SPR_GROUP_REPLACE_OFF_AIRCRAFT;
					break;
				default: NOT_REACHED();
			}
		}

		/* Set text of sort by dropdown */
		this->widget[GRP_WIDGET_SORT_BY_DROPDOWN].data = this->vehicle_sorter_names[this->vehicles.SortType()];

		this->DrawWidgets();

		/* Draw Matrix Group
		 * The selected group is drawn in white */
		StringID str_all_veh, str_no_group_veh;

		switch (this->vehicle_type) {
			case VEH_TRAIN:
				str_all_veh = STR_GROUP_ALL_TRAINS;
				str_no_group_veh = STR_GROUP_DEFAULT_TRAINS;
				break;
			case VEH_ROAD:
				str_all_veh = STR_GROUP_ALL_ROADS;
				str_no_group_veh = STR_GROUP_DEFAULT_ROADS;
				break;
			case VEH_SHIP:
				str_all_veh = STR_GROUP_ALL_SHIPS;
				str_no_group_veh = STR_GROUP_DEFAULT_SHIPS;
				break;
			case VEH_AIRCRAFT:
				str_all_veh = STR_GROUP_ALL_AIRCRAFTS;
				str_no_group_veh = STR_GROUP_DEFAULT_AIRCRAFTS;
				break;
			default: NOT_REACHED();
		}
		DrawString(10, y1, str_all_veh, IsAllGroupID(this->group_sel) ? TC_WHITE : TC_BLACK);

		y1 += 13;

		DrawString(10, y1, str_no_group_veh, IsDefaultGroupID(this->group_sel) ? TC_WHITE : TC_BLACK);

		max = min(this->vscroll2.pos + this->vscroll2.cap, this->groups.Length());
		for (i = this->vscroll2.pos ; i < max ; ++i) {
			const Group *g = this->groups[i];

			assert(g->owner == owner);

			y1 += PLY_WND_PRC__SIZE_OF_ROW_TINY;

			/* draw the selected group in white, else we draw it in black */
			SetDParam(0, g->index);
			DrawString(10, y1, STR_GROUP_NAME, (this->group_sel == g->index) ? TC_WHITE : TC_BLACK);

			/* draw the number of vehicles of the group */
			SetDParam(0, g->num_vehicle);
			DrawStringRightAligned(187, y1 + 1, STR_GROUP_TINY_NUM, (this->group_sel == g->index) ? TC_WHITE : TC_BLACK);
		}

		this->DrawSortButtonState(GRP_WIDGET_SORT_BY_ORDER, this->vehicles.IsDescSortOrder() ? SBS_DOWN : SBS_UP);

		this->DrawVehicleListItems(x, this->vehicle_sel);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch(widget) {
			case GRP_WIDGET_SORT_BY_ORDER: // Flip sorting method ascending/descending
				this->vehicles.ToggleSortOrder();
				this->SetDirty();
				break;

			case GRP_WIDGET_SORT_BY_DROPDOWN: // Select sorting criteria dropdown menu
				ShowDropDownMenu(this, this->vehicle_sorter_names, this->vehicles.SortType(),  GRP_WIDGET_SORT_BY_DROPDOWN, 0, (this->vehicle_type == VEH_TRAIN || this->vehicle_type == VEH_ROAD) ? 0 : (1 << 10));
				return;

			case GRP_WIDGET_ALL_VEHICLES: // All vehicles button
				if (!IsAllGroupID(this->group_sel)) {
					this->group_sel = ALL_GROUP;
					this->vehicles.ForceRebuild();
					this->SetDirty();
				}
				break;

			case GRP_WIDGET_DEFAULT_VEHICLES: // Ungrouped vehicles button
				if (!IsDefaultGroupID(this->group_sel)) {
					this->group_sel = DEFAULT_GROUP;
					this->vehicles.ForceRebuild();
					this->SetDirty();
				}
				break;

			case GRP_WIDGET_LIST_GROUP: { // Matrix Group
				uint16 id_g = (pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET - 26) / PLY_WND_PRC__SIZE_OF_ROW_TINY;

				if (id_g >= this->vscroll2.cap) return;

				id_g += this->vscroll2.pos;

				if (id_g >= this->groups.Length()) return;

				this->group_sel = this->groups[id_g]->index;;

				this->vehicles.ForceRebuild();
				this->SetDirty();
				break;
			}

			case GRP_WIDGET_LIST_VEHICLE: { // Matrix Vehicle
				uint32 id_v = (pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET) / (int)this->resize.step_height;
				const Vehicle *v;

				if (id_v >= this->vscroll.cap) return; // click out of bounds

				id_v += this->vscroll.pos;

				if (id_v >= this->vehicles.Length()) return; // click out of list bound

				v = this->vehicles[id_v];

				this->vehicle_sel = v->index;

				if (v->IsValid()) {
					SetObjectToPlaceWnd(v->GetImage(DIR_W), GetVehiclePalette(v), VHM_DRAG, this);
					_cursor.vehchain = true;
				}

				this->SetDirty();
				break;
			}

			case GRP_WIDGET_CREATE_GROUP: // Create a new group
				DoCommandP(0, this->vehicle_type, 0, CMD_CREATE_GROUP | CMD_MSG(STR_GROUP_CAN_T_CREATE));
				break;

			case GRP_WIDGET_DELETE_GROUP: { // Delete the selected group
				GroupID group = this->group_sel;
				this->group_sel = ALL_GROUP;

				DoCommandP(0, group, 0, CMD_DELETE_GROUP | CMD_MSG(STR_GROUP_CAN_T_DELETE));
				break;
			}

			case GRP_WIDGET_RENAME_GROUP: { // Rename the selected roup
				assert(IsValidGroupID(this->group_sel));

				const Group *g = GetGroup(this->group_sel);

				SetDParam(0, g->index);
				ShowQueryString(STR_GROUP_NAME, STR_GROUP_RENAME_CAPTION, MAX_LENGTH_GROUP_NAME_BYTES, MAX_LENGTH_GROUP_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT);
			} break;


			case GRP_WIDGET_AVAILABLE_VEHICLES:
				ShowBuildVehicleWindow(INVALID_TILE, this->vehicle_type);
				break;

			case GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN:
				ShowGroupActionDropdown(this, this->group_sel);
				break;

			case GRP_WIDGET_START_ALL:
			case GRP_WIDGET_STOP_ALL: { // Start/stop all vehicles of the list
				DoCommandP(0, this->group_sel, ((IsAllGroupID(this->group_sel) ? VLW_STANDARD : VLW_GROUP_LIST) & VLW_MASK)
													| (1 << 6)
													| (widget == GRP_WIDGET_START_ALL ? (1 << 5) : 0)
													| this->vehicle_type, CMD_MASS_START_STOP);

				break;
			}

			case GRP_WIDGET_REPLACE_PROTECTION:
				if (IsValidGroupID(this->group_sel)) {
					const Group *g = GetGroup(this->group_sel);

					DoCommandP(0, this->group_sel, !g->replace_protection, CMD_SET_GROUP_REPLACE_PROTECTION);
				}
				break;
		}
	}

	virtual void OnDragDrop(Point pt, int widget)
	{
		switch (widget) {
			case GRP_WIDGET_ALL_VEHICLES: // All vehicles
			case GRP_WIDGET_DEFAULT_VEHICLES: // Ungrouped vehicles
				DoCommandP(0, DEFAULT_GROUP, this->vehicle_sel, CMD_ADD_VEHICLE_GROUP | CMD_MSG(STR_GROUP_CAN_T_ADD_VEHICLE));

				this->vehicle_sel = INVALID_VEHICLE;

				this->SetDirty();

				break;

			case GRP_WIDGET_LIST_GROUP: { // Maxtrix group
				uint16 id_g = (pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET - 26) / PLY_WND_PRC__SIZE_OF_ROW_TINY;
				const VehicleID vindex = this->vehicle_sel;

				this->vehicle_sel = INVALID_VEHICLE;

				this->SetDirty();

				if (id_g >= this->vscroll2.cap) return;

				id_g += this->vscroll2.pos;

				if (id_g >= this->groups.Length()) return;

				DoCommandP(0, this->groups[id_g]->index, vindex, CMD_ADD_VEHICLE_GROUP | CMD_MSG(STR_GROUP_CAN_T_ADD_VEHICLE));

				break;
			}

			case GRP_WIDGET_LIST_VEHICLE: { // Maxtrix vehicle
				uint32 id_v = (pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET) / (int)this->resize.step_height;
				const Vehicle *v;
				const VehicleID vindex = this->vehicle_sel;

				this->vehicle_sel = INVALID_VEHICLE;

				this->SetDirty();

				if (id_v >= this->vscroll.cap) return; // click out of bounds

				id_v += this->vscroll.pos;

				if (id_v >= this->vehicles.Length()) return; // click out of list bound

				v = this->vehicles[id_v];

				if (vindex == v->index) {
					ShowVehicleViewWindow(v);
				}

				break;
			}
		}
		_cursor.vehchain = false;
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		DoCommandP(0, this->group_sel, 0, CMD_RENAME_GROUP | CMD_MSG(STR_GROUP_CAN_T_RENAME), NULL, str);
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		this->vscroll2.cap += delta.y / PLY_WND_PRC__SIZE_OF_ROW_TINY;
		this->vscroll.cap += delta.y / (int)this->resize.step_height;

		this->widget[GRP_WIDGET_LIST_GROUP].data = (this->vscroll2.cap << 8) + 1;
		this->widget[GRP_WIDGET_LIST_VEHICLE].data = (this->vscroll.cap << 8) + 1;
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case GRP_WIDGET_SORT_BY_DROPDOWN:
				this->vehicles.SetSortType(index);
				break;

			case GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN:
				assert(this->vehicles.Length() != 0);

				switch (index) {
					case GALF_REPLACE: // Replace window
						ShowReplaceGroupVehicleWindow(this->group_sel, this->vehicle_type);
						break;
					case GALF_SERVICE: // Send for servicing
						DoCommandP(0, this->group_sel, ((IsAllGroupID(this->group_sel) ? VLW_STANDARD : VLW_GROUP_LIST) & VLW_MASK)
									| DEPOT_MASS_SEND
									| DEPOT_SERVICE, GetCmdSendToDepot(this->vehicle_type));
						break;
					case GALF_DEPOT: // Send to Depots
						DoCommandP(0, this->group_sel, ((IsAllGroupID(this->group_sel) ? VLW_STANDARD : VLW_GROUP_LIST) & VLW_MASK)
									| DEPOT_MASS_SEND, GetCmdSendToDepot(this->vehicle_type));
						break;
					case GALF_ADD_SHARED: // Add shared Vehicles
						assert(IsValidGroupID(this->group_sel));

						DoCommandP(0, this->group_sel, this->vehicle_type, CMD_ADD_SHARED_VEHICLE_GROUP | CMD_MSG(STR_GROUP_CAN_T_ADD_SHARED_VEHICLE));
						break;
					case GALF_REMOVE_ALL: // Remove all Vehicles from the selected group
						assert(IsValidGroupID(this->group_sel));

						DoCommandP(0, this->group_sel, this->vehicle_type, CMD_REMOVE_ALL_VEHICLES_GROUP | CMD_MSG(STR_GROUP_CAN_T_REMOVE_ALL_VEHICLES));
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
		if (_pause_game != 0) return;
		if (this->groups.NeedResort() || this->vehicles.NeedResort()) {
			this->SetDirty();
		}
	}

	virtual void OnPlaceObjectAbort()
	{
		/* abort drag & drop */
		this->vehicle_sel = INVALID_VEHICLE;
		this->InvalidateWidget(GRP_WIDGET_LIST_VEHICLE);
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


static WindowDesc _group_desc(
	WDP_AUTO, WDP_AUTO, 460, 194, 460, 246,
	WC_INVALID, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_group_widgets
);

void ShowCompanyGroup(CompanyID company, VehicleType vehicle_type)
{
	if (!IsValidCompanyID(company)) return;

	_group_desc.cls = GetWindowClassForVehicleType(vehicle_type);
	WindowNumber num = (vehicle_type << 11) | VLW_GROUP_LIST | company;
	AllocateWindowDescFront<VehicleGroupWindow>(&_group_desc, num);
}

/**
 * Removes the highlight of a vehicle in a group window
 * @param *v Vehicle to remove all highlights from
 */
void DeleteGroupHighlightOfVehicle(const Vehicle *v)
{
	VehicleGroupWindow *w;

	/* If we haven't got any vehicles on the mouse pointer, we haven't got any highlighted in any group windows either
	 * If that is the case, we can skip looping though the windows and save time
	 */
	if (_special_mouse_mode != WSM_DRAGDROP) return;

	VehicleType vehicle_type = v->type;
	w = dynamic_cast<VehicleGroupWindow *>(FindWindowById(GetWindowClassForVehicleType(vehicle_type), (vehicle_type << 11) | VLW_GROUP_LIST | v->owner));
	if (w != NULL) w->UnselectVehicle(v->index);
}

