/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
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
#include "tree_cmd.h"

#include "widgets/tree_widget.h"

#include "table/sprites.h"
#include "table/strings.h"
#include "table/tree_land.h"

#include "safeguards.h"

void PlaceTreesRandomly();
uint PlaceTreeGroupAroundTile(TileIndex tile, TreeType treetype, uint radius, uint count, bool set_zone);

/** Tree Sprites with their palettes */
const PalSpriteID tree_sprites[] = {
	{ 1621, PAL_NONE }, { 1635, PAL_NONE }, { 1656, PAL_NONE }, { 1579, PAL_NONE },
	{ 1607, PAL_NONE }, { 1593, PAL_NONE }, { 1614, PAL_NONE }, { 1586, PAL_NONE },
	{ 1663, PAL_NONE }, { 1677, PAL_NONE }, { 1691, PAL_NONE }, { 1705, PAL_NONE },
	{ 1711, PAL_NONE }, { 1746, PAL_NONE }, { 1753, PAL_NONE }, { 1732, PAL_NONE },
	{ 1739, PAL_NONE }, { 1718, PAL_NONE }, { 1725, PAL_NONE }, { 1760, PAL_NONE },
	{ 1838, PAL_NONE }, { 1844, PAL_NONE }, { 1866, PAL_NONE }, { 1871, PAL_NONE },
	{ 1899, PAL_NONE }, { 1935, PAL_NONE }, { 1928, PAL_NONE }, { 1915, PAL_NONE },
	{ 1887, PAL_NONE }, { 1908, PAL_NONE }, { 1824, PAL_NONE }, { 1943, PAL_NONE },
	{ 1950, PAL_NONE }, { 1957, PALETTE_TO_GREEN }, { 1964, PALETTE_TO_RED },        { 1971, PAL_NONE },
	{ 1978, PAL_NONE }, { 1985, PALETTE_TO_RED, },  { 1992, PALETTE_TO_PALE_GREEN }, { 1999, PALETTE_TO_YELLOW }, { 2006, PALETTE_TO_RED }
};

/**
 * Calculate the maximum size of all tree sprites
 * @return Dimension of the largest tree sprite
 */
static Dimension GetMaxTreeSpriteSize()
{
	const uint16_t base = _tree_base_by_landscape[_settings_game.game_creation.landscape];
	const uint16_t count = _tree_count_by_landscape[_settings_game.game_creation.landscape];

	Dimension size, this_size;
	Point offset;
	/* Avoid to use it uninitialized */
	size.width = ScaleGUITrad(32); // default width - WD_FRAMERECT_LEFT
	size.height = ScaleGUITrad(39); // default height - BUTTON_BOTTOM_OFFSET
	offset.x = 0;
	offset.y = 0;

	for (int i = base; i < base + count; i++) {
		if (i >= (int)lengthof(tree_sprites)) return size;
		this_size = GetSpriteSize(tree_sprites[i].sprite, &offset);
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

	enum PlantingMode {
		PM_NORMAL,
		PM_FOREST_SM,
		PM_FOREST_LG,
	};

	int tree_to_plant;  ///< Tree number to plant, \c TREE_INVALID for a random tree.
	PlantingMode mode;  ///< Current mode for planting

	/**
	 * Update the GUI and enable/disable planting to reflect selected options.
	 */
	void UpdateMode()
	{
		this->RaiseButtons();

		const int current_tree = this->tree_to_plant;

		if (this->tree_to_plant >= 0) {
			/* Activate placement */
			if (_settings_client.sound.confirm) SndPlayFx(SND_15_BEEP);
			SetObjectToPlace(SPR_CURSOR_TREE, PAL_NONE, HT_RECT | HT_DIAGONAL, this->window_class, this->window_number);
			this->tree_to_plant = current_tree; // SetObjectToPlace may call ResetObjectToPlace which may reset tree_to_plant to -1
		} else {
			/* Deactivate placement */
			ResetObjectToPlace();
		}

		if (this->tree_to_plant == TREE_INVALID) {
			this->LowerWidget(WID_BT_TYPE_RANDOM);
		} else if (this->tree_to_plant >= 0) {
			this->LowerWidget(WID_BT_TYPE_BUTTON_FIRST + this->tree_to_plant);
		}

		switch (this->mode) {
			case PM_NORMAL: this->LowerWidget(WID_BT_MODE_NORMAL); break;
			case PM_FOREST_SM: this->LowerWidget(WID_BT_MODE_FOREST_SM); break;
			case PM_FOREST_LG: this->LowerWidget(WID_BT_MODE_FOREST_LG); break;
			default: NOT_REACHED();
		}

		this->SetDirty();
	}

	void DoPlantForest(TileIndex tile)
	{
		TreeType treetype = (TreeType)this->tree_to_plant;
		if (this->tree_to_plant == TREE_INVALID) {
			treetype = (TreeType)(InteractiveRandomRange(_tree_count_by_landscape[_settings_game.game_creation.landscape]) + _tree_base_by_landscape[_settings_game.game_creation.landscape]);
		}
		const uint radius = this->mode == PM_FOREST_LG ? 12 : 5;
		const uint count = this->mode == PM_FOREST_LG ? 12 : 5;
		// Create tropic zones only when the tree type is selected by the user and not picked randomly.
		PlaceTreeGroupAroundTile(tile, treetype, radius, count, this->tree_to_plant != TREE_INVALID);
	}

public:
	BuildTreesWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc), tree_to_plant(-1), mode(PM_NORMAL)
	{
		this->CreateNestedTree();
		ResetObjectToPlace();

		this->LowerWidget(WID_BT_MODE_NORMAL);
		/* Show scenario editor tools in editor */
		if (_game_mode != GM_EDITOR) {
			this->GetWidget<NWidgetStacked>(WID_BT_SE_PANE)->SetDisplayedPlane(SZSP_HORIZONTAL);
		}
		this->FinishInitNested(window_number);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget >= WID_BT_TYPE_BUTTON_FIRST) {
			/* Ensure tree type buttons are sized after the largest tree type */
			Dimension d = GetMaxTreeSpriteSize();
			size->width = d.width + padding.width;
			size->height = d.height + padding.height + ScaleGUITrad(BUTTON_BOTTOM_OFFSET); // we need some more space
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget >= WID_BT_TYPE_BUTTON_FIRST) {
			const int index = widget - WID_BT_TYPE_BUTTON_FIRST;
			/* Trees "grow" in the centre on the bottom line of the buttons */
			DrawSprite(tree_sprites[index].sprite, tree_sprites[index].pal, CenterBounds(r.left, r.right, 0), r.bottom - ScaleGUITrad(BUTTON_BOTTOM_OFFSET));
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BT_TYPE_RANDOM: // tree of random type.
				this->tree_to_plant = this->tree_to_plant == TREE_INVALID ? -1 : TREE_INVALID;
				this->UpdateMode();
				break;

			case WID_BT_MANY_RANDOM: // place trees randomly over the landscape
				if (_settings_client.sound.confirm) SndPlayFx(SND_15_BEEP);
				PlaceTreesRandomly();
				MarkWholeScreenDirty();
				break;

			case WID_BT_MODE_NORMAL:
				this->mode = PM_NORMAL;
				this->UpdateMode();
				break;

			case WID_BT_MODE_FOREST_SM:
				assert(_game_mode == GM_EDITOR);
				this->mode = PM_FOREST_SM;
				this->UpdateMode();
				break;

			case WID_BT_MODE_FOREST_LG:
				assert(_game_mode == GM_EDITOR);
				this->mode = PM_FOREST_LG;
				this->UpdateMode();
				break;

			default:
				if (widget >= WID_BT_TYPE_BUTTON_FIRST) {
					const int index = widget - WID_BT_TYPE_BUTTON_FIRST;
					this->tree_to_plant = this->tree_to_plant == index ? -1 : index;
					this->UpdateMode();
				}
				break;
		}
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		if (_game_mode != GM_EDITOR && this->mode == PM_NORMAL) {
			VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_PLANT_TREES);
		} else {
			VpStartDragging(DDSP_PLANT_TREES);
		}
	}

	void OnPlaceDrag(ViewportPlaceMethod select_method, [[maybe_unused]] ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt) override
	{
		if (_game_mode != GM_EDITOR && this->mode == PM_NORMAL) {
			VpSelectTilesWithMethod(pt.x, pt.y, select_method);
		} else {
			TileIndex tile = TileVirtXY(pt.x, pt.y);

			if (this->mode == PM_NORMAL) {
				Command<CMD_PLANT_TREE>::Post(tile, tile, this->tree_to_plant, false);
			} else {
				this->DoPlantForest(tile);
			}
		}
	}

	void OnPlaceMouseUp([[maybe_unused]] ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt, TileIndex start_tile, TileIndex end_tile) override
	{
		if (_game_mode != GM_EDITOR && this->mode == PM_NORMAL && pt.x != -1 && select_proc == DDSP_PLANT_TREES) {
			Command<CMD_PLANT_TREE>::Post(STR_ERROR_CAN_T_PLANT_TREE_HERE, end_tile, start_tile, this->tree_to_plant, _ctrl_pressed);
		}
	}

	void OnPlaceObjectAbort() override
	{
		this->tree_to_plant = -1;
		this->UpdateMode();
	}
};

/**
 * Make widgets for the current available tree types.
 * This does not use a NWID_MATRIX or WWT_MATRIX control as those are more difficult to
 * get producing the correct result than dynamically building the widgets is.
 * @see NWidgetFunctionType
 */
static std::unique_ptr<NWidgetBase> MakeTreeTypeButtons()
{
	const byte type_base = _tree_base_by_landscape[_settings_game.game_creation.landscape];
	const byte type_count = _tree_count_by_landscape[_settings_game.game_creation.landscape];

	/* Toyland has 9 tree types, which look better in 3x3 than 4x3 */
	const int num_columns = type_count == 9 ? 3 : 4;
	const int num_rows = CeilDiv(type_count, num_columns);
	byte cur_type = type_base;

	auto vstack = std::make_unique<NWidgetVertical>(NC_EQUALSIZE);
	vstack->SetPIP(0, 1, 0);

	for (int row = 0; row < num_rows; row++) {
		auto hstack = std::make_unique<NWidgetHorizontal>(NC_EQUALSIZE);
		hstack->SetPIP(0, 1, 0);
		for (int col = 0; col < num_columns; col++) {
			if (cur_type > type_base + type_count) break;
			auto button = std::make_unique<NWidgetBackground>(WWT_PANEL, COLOUR_GREY, WID_BT_TYPE_BUTTON_FIRST + cur_type);
			button->SetDataTip(0x0, STR_PLANT_TREE_TOOLTIP);
			hstack->Add(std::move(button));
			cur_type++;
		}
		vstack->Add(std::move(hstack));
	}

	return vstack;
}

static const NWidgetPart _nested_build_trees_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_PLANT_TREE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_VERTICAL), SetPadding(2),
			NWidgetFunction(MakeTreeTypeButtons),
			NWidget(NWID_SPACER), SetMinimalSize(0, 1),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BT_TYPE_RANDOM), SetDataTip(STR_TREES_RANDOM_TYPE, STR_TREES_RANDOM_TYPE_TOOLTIP),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BT_SE_PANE),
				NWidget(NWID_VERTICAL),
					NWidget(NWID_SPACER), SetMinimalSize(0, 1),
					NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BT_MODE_NORMAL), SetFill(1, 0), SetDataTip(STR_TREES_MODE_NORMAL_BUTTON, STR_TREES_MODE_NORMAL_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BT_MODE_FOREST_SM), SetFill(1, 0), SetDataTip(STR_TREES_MODE_FOREST_SM_BUTTON, STR_TREES_MODE_FOREST_SM_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BT_MODE_FOREST_LG), SetFill(1, 0), SetDataTip(STR_TREES_MODE_FOREST_LG_BUTTON, STR_TREES_MODE_FOREST_LG_TOOLTIP),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(0, 1),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BT_MANY_RANDOM), SetDataTip(STR_TREES_RANDOM_TREES_BUTTON, STR_TREES_RANDOM_TREES_TOOLTIP),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _build_trees_desc(__FILE__, __LINE__,
	WDP_AUTO, "build_tree", 0, 0,
	WC_BUILD_TREES, WC_NONE,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_trees_widgets), std::end(_nested_build_trees_widgets)
);

void ShowBuildTreesToolbar()
{
	if (_game_mode != GM_EDITOR && !Company::IsValidID(_local_company)) return;
	AllocateWindowDescFront<BuildTreesWindow>(&_build_trees_desc, 0);
}
