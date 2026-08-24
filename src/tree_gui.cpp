/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tree_gui.cpp GUIs for building trees. */

#include "stdafx.h"
#include "window_gui.h"
#include "gfx_func.h"
#include "tilehighlight_func.h"
#include "company_func.h"
#include "company_base.h"
#include "command_func.h"
#include "core/random_func.hpp"
#include "sound_func.h"
#include "strings_func.h"
#include "zoom_func.h"
#include "tree_map.h"
#include "tree_func.h"
#include "tree_cmd.h"

#include "widgets/tree_widget.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"

/**
 * Calculate the maximum size of all tree sprites
 * @return Dimension of the largest tree sprite
 */
static Dimension GetMaxTreeSpriteSize()
{
	Dimension size{};
	Point offset{};

	for (const TreeType &treetype : GetTreeTypes()) {
		Dimension this_size = GetSpriteSize(GetTreeSprite(treetype).sprite, &offset);
		size.width = std::max<int>(size.width, 2 * std::max<int>(this_size.width, -offset.x));
		size.height = std::max<int>(size.height, std::max<int>(this_size.height, -offset.y));
	}

	return size;
}


/**
 * The build trees window.
 */
class BuildTreesWindow : public Window
{
	/** Visual Y offset of tree root from the bottom of the tree type buttons */
	static const int BUTTON_BOTTOM_OFFSET = 7;

	enum PlantingMode : uint8_t {
		PM_NORMAL,
		PM_FOREST_SM,
		PM_FOREST_LG,
	};

	int index_to_plant = -1; ///< Index of active trees to plant, \c -1 for a random tree.
	PlantingMode mode = PM_NORMAL; ///< Current mode for planting

	/**
	 * Update the GUI and enable/disable planting to reflect selected options.
	 */
	void UpdateMode()
	{
		this->RaiseButtons();

		if (this->index_to_plant >= 0) {
			/* SetObjectToPlace may call ResetObjectToPlace which would reset index_to_plant to -1. */
			AutoRestoreBackup backup(this->index_to_plant);

			/* Activate placement */
			SndConfirmBeep();
			SetObjectToPlace(SPR_CURSOR_TREE, PAL_NONE, HT_RECT | HT_DIAGONAL, this->window_class, this->window_number);
		} else {
			/* Deactivate placement */
			ResetObjectToPlace();
		}

		if (this->index_to_plant == TREE_INVALID) {
			this->LowerWidget(WID_BT_TYPE_RANDOM);
		} else if (this->index_to_plant >= 0) {
			this->LowerWidget(WID_BT_TYPE_BUTTON_FIRST + this->index_to_plant);
		}

		switch (this->mode) {
			case PM_NORMAL: this->LowerWidget(WID_BT_MODE_NORMAL); break;
			case PM_FOREST_SM: this->LowerWidget(WID_BT_MODE_FOREST_SM); break;
			case PM_FOREST_LG: this->LowerWidget(WID_BT_MODE_FOREST_LG); break;
			default: NOT_REACHED();
		}

		this->SetDirty();
	}

	/**
	 * Get the tree type of the currently selected tree.
	 * @return the current tree type, or \c TREE_INVALID
	 */
	TreeType GetSelectedTreeType() const
	{
		if (this->index_to_plant < 0) return TREE_INVALID;

		const auto treetypes = GetTreeTypes();
		if (static_cast<size_t>(this->index_to_plant) < treetypes.size()) return treetypes[this->index_to_plant];

		return TREE_INVALID;
	}

	void DoPlantForest(TileIndex tile)
	{
		TreeType treetype = this->GetSelectedTreeType();
		if (treetype == TREE_INVALID) {
			const auto treetypes = GetTreeTypes();
			treetype = treetypes[InteractiveRandomRange(static_cast<uint32_t>(treetypes.size()))];
		}
		const uint radius = this->mode == PM_FOREST_LG ? 12 : 5;
		const uint count = this->mode == PM_FOREST_LG ? 12 : 5;
		/* Create tropic zones only when the tree type is selected by the user and not picked randomly. */
		PlaceTreeGroupAroundTile(tile, treetype, radius, count, this->index_to_plant != TREE_INVALID);
	}

public:
	BuildTreesWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		ResetObjectToPlace();

		this->LowerWidget(WID_BT_MODE_NORMAL);
		/* Show scenario editor tools in editor */
		if (_game_mode != GameMode::Editor) {
			this->GetWidget<NWidgetStacked>(WID_BT_SE_PANE)->SetDisplayedPlane(SZSP_HORIZONTAL);
		}
		this->FinishInitNested(window_number);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget >= WID_BT_TYPE_BUTTON_FIRST) {
			/* Ensure tree type buttons are sized after the largest tree type */
			Dimension d = GetMaxTreeSpriteSize();
			size.width = d.width + padding.width;
			size.height = d.height + padding.height + ScaleGUITrad(BUTTON_BOTTOM_OFFSET); // we need some more space
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget >= WID_BT_TYPE_BUTTON_FIRST) {
			auto tree_types = GetTreeTypes();

			size_t index = widget - WID_BT_TYPE_BUTTON_FIRST;
			if (index >= tree_types.size()) return;

			auto ps = GetTreeSprite(tree_types[index]);

			/* Trees "grow" in the centre on the bottom line of the buttons */
			DrawSprite(ps.sprite, ps.pal, CentreBounds(r.left, r.right, 0), r.bottom - ScaleGUITrad(BUTTON_BOTTOM_OFFSET));
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BT_TYPE_RANDOM: // tree of random type.
				this->index_to_plant = this->index_to_plant == TREE_INVALID ? -1 : TREE_INVALID;
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
				if (widget >= WID_BT_TYPE_BUTTON_FIRST) {
					const int index = widget - WID_BT_TYPE_BUTTON_FIRST;
					this->index_to_plant = this->index_to_plant == index ? -1 : index;
					this->UpdateMode();
				}
				break;
		}
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

			if (this->mode == PM_NORMAL) {
				Command<Commands::PlantTree>::Post(tile, tile, this->GetSelectedTreeType(), false);
			} else {
				this->DoPlantForest(tile);
			}
		}
	}

	void OnPlaceMouseUp([[maybe_unused]] ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt, TileIndex start_tile, TileIndex end_tile) override
	{
		if (_game_mode != GameMode::Editor && this->mode == PM_NORMAL && pt.x != -1 && select_proc == DDSP_PLANT_TREES) {
			Command<Commands::PlantTree>::Post(STR_ERROR_CAN_T_PLANT_TREE_HERE, end_tile, start_tile, this->GetSelectedTreeType(), _ctrl_pressed);
		}
	}

	void OnPlaceObjectAbort() override
	{
		this->index_to_plant = -1;
		this->UpdateMode();
	}
};

/**
 * Make widgets for the current available tree types.
 * This does not use a NWID_MATRIX or WWT_MATRIX control as those are more difficult to
 * get producing the correct result than dynamically building the widgets is.
 * @copydoc NWidgetFunctionType
 */
static std::unique_ptr<NWidgetBase> MakeTreeTypeButtons()
{
	auto treetypes = GetTreeTypes();
	uint32_t num_treetypes = std::size(treetypes);

	/* Toyland has 9 tree types, which look better in 3x3 than 4x3 */
	const int num_columns = num_treetypes == 9 ? 3 : 4;
	const int num_rows = CeilDiv(num_treetypes, num_columns);

	auto vstack = std::make_unique<NWidgetVertical>(NWidContainerFlag::EqualSize);
	vstack->SetPIP(0, 1, 0);

	uint32_t index = 0;
	for (int row = 0; row < num_rows; row++) {
		auto hstack = std::make_unique<NWidgetHorizontal>(NWidContainerFlag::EqualSize);
		hstack->SetPIP(0, 1, 0);
		for (int col = 0; col < num_columns; col++) {
			if (index >= num_treetypes) break;
			auto button = std::make_unique<NWidgetBackground>(WWT_PANEL, Colours::Grey, WID_BT_TYPE_BUTTON_FIRST + index);
			button->SetToolTip(STR_PLANT_TREE_TOOLTIP);
			hstack->Add(std::move(button));
			++index;
		}
		vstack->Add(std::move(hstack));
	}

	return vstack;
}

static constexpr std::initializer_list<NWidgetPart> _nested_build_trees_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::DarkGreen),
		NWidget(WWT_CAPTION, Colours::DarkGreen), SetStringTip(STR_PLANT_TREE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::DarkGreen),
		NWidget(WWT_STICKYBOX, Colours::DarkGreen),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen),
		NWidget(NWID_VERTICAL), SetPIP(0, 1, 0), SetPadding(2),
			NWidgetFunction(MakeTreeTypeButtons),
			NWidget(WWT_TEXTBTN, Colours::Grey, WID_BT_TYPE_RANDOM), SetStringTip(STR_TREES_RANDOM_TYPE, STR_TREES_RANDOM_TYPE_TOOLTIP),
			NWidget(NWID_SELECTION, Colours::Invalid, WID_BT_SE_PANE),
				NWidget(NWID_VERTICAL), SetPIP(0, 1, 0),
					NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
						NWidget(WWT_TEXTBTN, Colours::Grey, WID_BT_MODE_NORMAL), SetFill(1, 0), SetStringTip(STR_TREES_MODE_NORMAL_BUTTON, STR_TREES_MODE_NORMAL_TOOLTIP),
						NWidget(WWT_TEXTBTN, Colours::Grey, WID_BT_MODE_FOREST_SM), SetFill(1, 0), SetStringTip(STR_TREES_MODE_FOREST_SM_BUTTON, STR_TREES_MODE_FOREST_SM_TOOLTIP),
						NWidget(WWT_TEXTBTN, Colours::Grey, WID_BT_MODE_FOREST_LG), SetFill(1, 0), SetStringTip(STR_TREES_MODE_FOREST_LG_BUTTON, STR_TREES_MODE_FOREST_LG_TOOLTIP),
					EndContainer(),
					NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_BT_MANY_RANDOM), SetStringTip(STR_TREES_RANDOM_TREES_BUTTON, STR_TREES_RANDOM_TREES_TOOLTIP),
				EndContainer(),
			EndContainer(),
		EndContainer(),
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
	AllocateWindowDescFront<BuildTreesWindow>(_build_trees_desc, 0);
}
