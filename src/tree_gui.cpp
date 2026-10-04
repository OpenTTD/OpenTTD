/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tree_gui.cpp GUIs for building trees. */

#include "stdafx.h"
#include "picker_gui.h"
#include "window_gui.h"
#include "gfx_func.h"
#include "tilehighlight_func.h"
#include "company_func.h"
#include "company_base.h"
#include "command_func.h"
#include "core/random_func.hpp"
#include "sound_func.h"
#include "zoom_func.h"
#include "tree_map.h"
#include "tree_func.h"
#include "tree_cmd.h"

#include "widgets/tree_widget.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"

struct TreePickerSelection {
	TreeClassID sel_class;
	uint16_t sel_type;
};
static TreePickerSelection _tree_gui;

class TreePickerCallbacks : public PickerCallbacksNewGRFClass<TreeClass> {
public:
	TreePickerCallbacks() : PickerCallbacksNewGRFClass<TreeClass>("fav_trees") {}

	GrfSpecFeature GetFeature() const override { return GrfSpecFeature::Trees; }

	StringID GetClassTooltip() const override { return STR_PICKER_STATION_CLASS_TOOLTIP; }
	StringID GetTypeTooltip() const override { return STR_PICKER_STATION_TYPE_TOOLTIP; }
	StringID GetCollectionTooltip() const override { return STR_PICKER_STATION_COLLECTION_TOOLTIP; }

	bool IsActive() const override
	{
		for (const auto &cls : TreeClass::Classes()) {
			for (const auto *spec : cls.Specs()) {
				if (spec != nullptr) return true;
			}
		}
		return false;
	}

	bool HasClassChoice() const override
	{
		return TreeClass::Classes().size() > 1;
	}

	int GetSelectedClass() const override { return _tree_gui.sel_class.base(); }
	void SetSelectedClass(int id) const override { _tree_gui.sel_class = this->GetClassIndex(id); }

	StringID GetClassName(int id) const override
	{
		const auto *tc = GetClass(id);
		if (tc->GetUISpecCount() == 0) return INVALID_STRING_ID;
		return tc->name;
	}

	int GetSelectedType() const override { return _tree_gui.sel_type; }
	void SetSelectedType(int id) const override { _tree_gui.sel_type = id; }

	StringID GetTypeName(int cls_id, int id) const override
	{
		const auto *spec = this->GetSpec(cls_id, id);
		return (spec == nullptr) ? STR_STATION_CLASS_DFLT_STATION : spec->name;
	}

	std::span<const BadgeID> GetTypeBadges(int cls_id, int id) const override
	{
		return {};
		// const auto *spec = this->GetSpec(cls_id, id);
		// if (spec == nullptr) return {};
		// return spec->badges;
	}

	bool IsTypeAvailable(int cls_id, int id) const override
	{
		return this->GetSpec(cls_id, id) != nullptr;
	}

	void DrawType(int x, int y, int cls_id, int id) const override
	{
		const auto *spec = this->GetSpec(cls_id, id);
		if (spec == nullptr) return;

		auto ps = GetTreeSprite(*spec);
		DrawSprite(ps.sprite, ps.pal, x, y + ScaleSpriteTrad(TILE_PIXELS * 3 / 4));
	}

	void FillUsedItems(std::set<PickerItem> &items) override
	{
		for (TileIndex tile : Map::Iterate()) {
			if (!IsTileType(tile, TileType::Trees)) continue;
			const TreeTileSpec &tts = GetTreeTileSpec(GetTreeType(tile));
			items.insert({tts.grf_prop.grfid, tts.grf_prop.local_id, tts.class_index.base(), tts.index});
		}
	}

	static TreePickerCallbacks instance;
};
/* static */ TreePickerCallbacks TreePickerCallbacks::instance;

/**
 * The build trees window.
 */
class BuildTreesWindow : public PickerWindow {
	/** Visual Y offset of tree root from the bottom of the tree type buttons */
	static const int BUTTON_BOTTOM_OFFSET = 7;

	enum PlantingMode : uint8_t {
		PM_NORMAL,
		PM_FOREST_SM,
		PM_FOREST_LG,
	};

	PlantingMode mode = PM_NORMAL; ///< Current mode for planting

	/**
	 * Update the GUI and enable/disable planting to reflect selected options.
	 */
	void UpdateMode()
	{
		this->RaiseButtons();

		// if (this->index_to_plant >= 0) {
		// 	/* SetObjectToPlace may call ResetObjectToPlace which would reset index_to_plant to -1. */
		// 	AutoRestoreBackup backup(this->index_to_plant);

		// 	/* Activate placement */
		// 	SndConfirmBeep();
		// 	SetObjectToPlace(SPR_CURSOR_TREE, PAL_NONE, HT_RECT | HT_DIAGONAL, this->window_class, this->window_number);
		// } else {
		// 	/* Deactivate placement */
		// 	ResetObjectToPlace();
		// }

		// if (this->index_to_plant == TREE_INVALID) {
		// 	this->LowerWidget(WID_BT_TYPE_RANDOM);
		// } else if (this->index_to_plant >= 0) {
		// 	this->LowerWidget(WID_BT_TYPE_BUTTON_FIRST + this->index_to_plant);
		// }

		switch (this->mode) {
			case PM_NORMAL: this->LowerWidget(WID_BT_MODE_NORMAL); break;
			case PM_FOREST_SM: this->LowerWidget(WID_BT_MODE_FOREST_SM); break;
			case PM_FOREST_LG: this->LowerWidget(WID_BT_MODE_FOREST_LG); break;
			default: NOT_REACHED();
		}

		this->SetDirty();
	}

	// /**
	//  * Get the tree type of the currently selected tree.
	//  * @return the current tree type, or \c TREE_INVALID
	//  */
	// TreeType GetSelectedTreeType() const
	// {
	// 	if (this->index_to_plant < 0) return TREE_INVALID;

	// 	const auto treetypes = GetTreeTypes();
	// 	if (static_cast<size_t>(this->index_to_plant) < treetypes.size()) return treetypes[this->index_to_plant];

	// 	return TREE_INVALID;
	// }

	void DoPlantForest(TileIndex tile)
	{
		// this->callbacks.GetSelectedClass();
		// this->callbacks.GetSelectedType();

		// TreeType treetype = this->GetSelectedTreeType();
		// if (treetype == TREE_INVALID) {
		// 	const auto treetypes = GetTreeTypes();
		// 	treetype = treetypes[InteractiveRandomRange(static_cast<uint32_t>(treetypes.size()))];
		// }
		// const uint radius = this->mode == PM_FOREST_LG ? 12 : 5;
		// const uint count = this->mode == PM_FOREST_LG ? 12 : 5;
		// /* Create tropic zones only when the tree type is selected by the user and not picked randomly. */
		// PlaceTreeGroupAroundTile(tile, treetype, radius, count, this->index_to_plant != TREE_INVALID);
	}

public:
	BuildTreesWindow(WindowDesc &desc, Window *parent) : PickerWindow(desc, parent, 0, TreePickerCallbacks::instance)
	{
		this->ConstructWindow();
		// this->CreateNestedTree();
		// ResetObjectToPlace();

		// this->LowerWidget(WID_BT_MODE_NORMAL);
		// /* Show scenario editor tools in editor */
		// if (_game_mode != GameMode::Editor) {
		// 	this->GetWidget<NWidgetStacked>(WID_BT_SE_PANE)->SetDisplayedPlane(SZSP_HORIZONTAL);
		// }
		// this->FinishInitNested(window_number);
	}

	void OnInit() override
	{
		this->PickerWindow::OnInit();

		/* Show scenario editor tools in editor */
		if (_game_mode != GameMode::Editor) {
			this->GetWidget<NWidgetStacked>(WID_BT_SE_PANE)->SetDisplayedPlane(SZSP_HORIZONTAL);
		}
	}

	// void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	// {
	// 	this->UpdateWidgetSize(WidgetID widget, Dimension &size, const Dimension &padding, Dimension &fill, Dimension &resize)
	// }

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BT_TYPE_RANDOM: // tree of random type.
				// this->index_to_plant = this->index_to_plant == TREE_INVALID ? -1 : TREE_INVALID;
				this->UpdateMode();
				break;

			case WID_BT_MANY_RANDOM: // place trees randomly over the landscape
				SndConfirmBeep();
				PlaceTreesRandomly();
				MarkWholeScreenDirty();
				break;

			case WID_BT_MODE_NORMAL:
				this->mode = PM_NORMAL;
				this->UpdateMode();
				break;

			case WID_BT_MODE_FOREST_SM:
				assert(_game_mode == GameMode::Editor);
				this->mode = PM_FOREST_SM;
				this->UpdateMode();
				break;

			case WID_BT_MODE_FOREST_LG:
				assert(_game_mode == GameMode::Editor);
				this->mode = PM_FOREST_LG;
				this->UpdateMode();
				break;

			default:
				this->PickerWindow::OnClick(pt, widget, click_count);
				break;
		}
	}

	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		this->PickerWindow::OnInvalidateData(data, gui_scope);
		if (!gui_scope) return;
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		if (_game_mode != GameMode::Editor && this->mode == PM_NORMAL) {
			VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_PLANT_TREES);
		} else {
			VpStartDragging(DDSP_PLANT_TREES);
		}
	}

	void OnPlaceDrag(ViewportPlaceMethod select_method, [[maybe_unused]] ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt) override
	{
		if (_game_mode != GameMode::Editor && this->mode == PM_NORMAL) {
			VpSelectTilesWithMethod(pt.x, pt.y, select_method);
		} else {
			TileIndex tile = TileVirtXY(pt.x, pt.y);

			// if (this->mode == PM_NORMAL) {
			// 	Command<Commands::PlantTree>::Post(tile, tile, this->GetSelectedTreeType(), false);
			// } else {
			// 	this->DoPlantForest(tile);
			// }
		}
	}

	void OnPlaceMouseUp([[maybe_unused]] ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt, TileIndex start_tile, TileIndex end_tile) override
	{
		// if (_game_mode != GameMode::Editor && this->mode == PM_NORMAL && pt.x != -1 && select_proc == DDSP_PLANT_TREES) {
		// 	Command<Commands::PlantTree>::Post(STR_ERROR_CAN_T_PLANT_TREE_HERE, end_tile, start_tile, this->GetSelectedTreeType(), _ctrl_pressed);
		// }
	}

	void OnPlaceObjectAbort() override
	{
		// this->index_to_plant = -1;
		this->UpdateMode();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_build_trees_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::DarkGreen),
		NWidget(WWT_CAPTION, Colours::DarkGreen), SetStringTip(STR_PLANT_TREE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::DarkGreen),
		NWidget(WWT_DEFSIZEBOX, Colours::DarkGreen),
		NWidget(WWT_STICKYBOX, Colours::DarkGreen),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidgetFunction(MakePickerClassWidgets),
			NWidget(WWT_PANEL, Colours::DarkGreen),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_picker, 0), SetPadding(WidgetDimensions::unscaled.picker),
					NWidget(WWT_TEXTBTN, Colours::Grey, WID_BT_TYPE_RANDOM), SetStringTip(STR_TREES_RANDOM_TYPE, STR_TREES_RANDOM_TYPE_TOOLTIP),
					NWidget(NWID_SELECTION, Colours::Invalid, WID_BT_SE_PANE),
						NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_picker, 0),
							NWidget(WWT_TEXTBTN, Colours::Grey, WID_BT_MODE_NORMAL), SetFill(1, 0), SetStringTip(STR_TREES_MODE_NORMAL_BUTTON, STR_TREES_MODE_NORMAL_TOOLTIP),
							NWidget(WWT_TEXTBTN, Colours::Grey, WID_BT_MODE_FOREST_SM), SetFill(1, 0), SetStringTip(STR_TREES_MODE_FOREST_SM_BUTTON, STR_TREES_MODE_FOREST_SM_TOOLTIP),
							NWidget(WWT_TEXTBTN, Colours::Grey, WID_BT_MODE_FOREST_LG), SetFill(1, 0), SetStringTip(STR_TREES_MODE_FOREST_LG_BUTTON, STR_TREES_MODE_FOREST_LG_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BT_MANY_RANDOM), SetStringTip(STR_TREES_RANDOM_TREES_BUTTON, STR_TREES_RANDOM_TREES_TOOLTIP),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidgetFunction(MakePickerTypeWidgets),
	EndContainer(),
};

/** Window definition for the tree building window. */
static WindowDesc _build_trees_desc(
	WindowPosition::Automatic, "build_tree", 0, 0,
	WindowClass::BuildTrees, WindowClass::None,
	WindowDefaultFlag::Construction,
	_nested_build_trees_widgets
);

void ShowBuildTreesToolbar()
{
	if (_game_mode != GameMode::Editor && !Company::IsValidID(_local_company)) return;
	new BuildTreesWindow(_build_trees_desc, nullptr);
}
