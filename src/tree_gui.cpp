/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tree_gui.cpp GUIs for building trees. */

#include "stdafx.h"
#include "openttd.h"
#include "window_gui.h"
#include "gfx_func.h"
#include "tilehighlight_func.h"
#include "company_func.h"
#include "company_base.h"
#include "command_func.h"
#include "sound_func.h"
#include "tree_map.h"

#include "table/sprites.h"
#include "table/strings.h"
#include "table/tree_land.h"

void PlaceTreesRandomly();

/** Widget definitions for the build trees window. */
enum BuildTreesWidgets {
	BTW_TYPE_11,
	BTW_TYPE_12,
	BTW_TYPE_13,
	BTW_TYPE_14,
	BTW_TYPE_21,
	BTW_TYPE_22,
	BTW_TYPE_23,
	BTW_TYPE_24,
	BTW_TYPE_31,
	BTW_TYPE_32,
	BTW_TYPE_33,
	BTW_TYPE_34,
	BTW_TYPE_RANDOM,
	BTW_MANY_RANDOM,
};

/**
 * The build trees window.
 */
class BuildTreesWindow : public Window
{
	uint16 base;        ///< Base tree number used for drawing the window.
	uint16 count;       ///< Number of different trees available.
	TreeType tree_to_plant; ///< Tree number to plant, \c TREE_INVALID for a random tree.

public:
	BuildTreesWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);
		ResetObjectToPlace();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != BTW_MANY_RANDOM) return;

		if (_game_mode != GM_EDITOR) {
			size->width = 0;
			size->height = 0;
		}
	}

	virtual void OnPaint()
	{
		this->OnInvalidateData(0);
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		static const PalSpriteID tree_sprites[] = {
			{ 0x655, PAL_NONE }, { 0x663, PAL_NONE }, { 0x678, PAL_NONE }, { 0x62B, PAL_NONE },
			{ 0x647, PAL_NONE }, { 0x639, PAL_NONE }, { 0x64E, PAL_NONE }, { 0x632, PAL_NONE },
			{ 0x67F, PAL_NONE }, { 0x68D, PAL_NONE }, { 0x69B, PAL_NONE }, { 0x6A9, PAL_NONE },
			{ 0x6AF, PAL_NONE }, { 0x6D2, PAL_NONE }, { 0x6D9, PAL_NONE }, { 0x6C4, PAL_NONE },
			{ 0x6CB, PAL_NONE }, { 0x6B6, PAL_NONE }, { 0x6BD, PAL_NONE }, { 0x6E0, PAL_NONE },
			{ 0x72E, PAL_NONE }, { 0x734, PAL_NONE }, { 0x74A, PAL_NONE }, { 0x74F, PAL_NONE },
			{ 0x76B, PAL_NONE }, { 0x78F, PAL_NONE }, { 0x788, PAL_NONE }, { 0x77B, PAL_NONE },
			{ 0x75F, PAL_NONE }, { 0x774, PAL_NONE }, { 0x720, PAL_NONE }, { 0x797, PAL_NONE },
			{ 0x79E, PAL_NONE }, { 0x7A5, PALETTE_TO_GREEN }, { 0x7AC, PALETTE_TO_RED }, { 0x7B3, PAL_NONE },
			{ 0x7BA, PAL_NONE }, { 0x7C1, PALETTE_TO_RED, }, { 0x7C8, PALETTE_TO_PALE_GREEN }, { 0x7CF, PALETTE_TO_YELLOW }, { 0x7D6, PALETTE_TO_RED }
		};

		if (widget < BTW_TYPE_11 || widget > BTW_TYPE_34 || widget - BTW_TYPE_11 >= this->count) return;

		int i = this->base + widget - BTW_TYPE_11;
		DrawSprite(tree_sprites[i].sprite, tree_sprites[i].pal, (r.left + r.right) / 2, r.bottom - 7);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case BTW_TYPE_11: case BTW_TYPE_12: case BTW_TYPE_13: case BTW_TYPE_14:
			case BTW_TYPE_21: case BTW_TYPE_22: case BTW_TYPE_23: case BTW_TYPE_24:
			case BTW_TYPE_31: case BTW_TYPE_32: case BTW_TYPE_33: case BTW_TYPE_34:
				if (widget - BTW_TYPE_11 >= this->count) break;

				if (HandlePlacePushButton(this, widget, SPR_CURSOR_TREE, HT_RECT)) {
					this->tree_to_plant = (TreeType)(this->base + widget - BTW_TYPE_11);
				}
				break;

			case BTW_TYPE_RANDOM: // tree of random type.
				if (HandlePlacePushButton(this, BTW_TYPE_RANDOM, SPR_CURSOR_TREE, HT_RECT)) {
					this->tree_to_plant = TREE_INVALID;
				}
				break;

			case BTW_MANY_RANDOM: // place trees randomly over the landscape
				this->LowerWidget(BTW_MANY_RANDOM);
				this->flags4 |= WF_TIMEOUT_BEGIN;
				SndPlayFx(SND_15_BEEP);
				PlaceTreesRandomly();
				MarkWholeScreenDirty();
				break;
		}
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		VpStartPlaceSizing(tile, VPM_X_AND_Y_LIMITED, DDSP_PLANT_TREES);
		VpSetPlaceSizingLimit(20);
	}

	virtual void OnPlaceDrag(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt)
	{
		VpSelectTilesWithMethod(pt.x, pt.y, select_method);
	}

	virtual void OnPlaceMouseUp(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt, TileIndex start_tile, TileIndex end_tile)
	{
		if (pt.x != -1 && select_proc == DDSP_PLANT_TREES) {
			DoCommandP(end_tile, this->tree_to_plant, start_tile,
				CMD_PLANT_TREE | CMD_MSG(STR_ERROR_CAN_T_PLANT_TREE_HERE));
		}
	}

	virtual void OnInvalidateData(int data = 0)
	{
		this->base  = _tree_base_by_landscape[_settings_game.game_creation.landscape];
		this->count = _tree_count_by_landscape[_settings_game.game_creation.landscape];
	}

	virtual void OnTimeout()
	{
		this->RaiseWidget(BTW_MANY_RANDOM);
		this->SetWidgetDirty(BTW_MANY_RANDOM);
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseButtons();
	}
};

static const NWidgetPart _nested_build_trees_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_PLANT_TREE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_PANEL, COLOUR_GREY, BTW_TYPE_11), SetMinimalSize(34, 46), SetDataTip(0x0, STR_PLANT_TREE_TOOLTIP),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(1, 0),
					NWidget(WWT_PANEL, COLOUR_GREY, BTW_TYPE_12), SetMinimalSize(34, 46), SetDataTip(0x0, STR_PLANT_TREE_TOOLTIP),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(1, 0),
					NWidget(WWT_PANEL, COLOUR_GREY, BTW_TYPE_13), SetMinimalSize(34, 46), SetDataTip(0x0, STR_PLANT_TREE_TOOLTIP),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(1, 0),
					NWidget(WWT_PANEL, COLOUR_GREY, BTW_TYPE_14), SetMinimalSize(34, 46), SetDataTip(0x0, STR_PLANT_TREE_TOOLTIP),
					EndContainer(),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_PANEL, COLOUR_GREY, BTW_TYPE_21), SetMinimalSize(34, 46), SetDataTip(0x0, STR_PLANT_TREE_TOOLTIP),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(1, 0),
					NWidget(WWT_PANEL, COLOUR_GREY, BTW_TYPE_22), SetMinimalSize(34, 46), SetDataTip(0x0, STR_PLANT_TREE_TOOLTIP),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(1, 0),
					NWidget(WWT_PANEL, COLOUR_GREY, BTW_TYPE_23), SetMinimalSize(34, 46), SetDataTip(0x0, STR_PLANT_TREE_TOOLTIP),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(1, 0),
					NWidget(WWT_PANEL, COLOUR_GREY, BTW_TYPE_24), SetMinimalSize(34, 46), SetDataTip(0x0, STR_PLANT_TREE_TOOLTIP),
					EndContainer(),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_PANEL, COLOUR_GREY, BTW_TYPE_31), SetMinimalSize(34, 46), SetDataTip(0x0, STR_PLANT_TREE_TOOLTIP),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(1, 0),
					NWidget(WWT_PANEL, COLOUR_GREY, BTW_TYPE_32), SetMinimalSize(34, 46), SetDataTip(0x0, STR_PLANT_TREE_TOOLTIP),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(1, 0),
					NWidget(WWT_PANEL, COLOUR_GREY, BTW_TYPE_33), SetMinimalSize(34, 46), SetDataTip(0x0, STR_PLANT_TREE_TOOLTIP),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(1, 0),
					NWidget(WWT_PANEL, COLOUR_GREY, BTW_TYPE_34), SetMinimalSize(34, 46), SetDataTip(0x0, STR_PLANT_TREE_TOOLTIP),
					EndContainer(),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BTW_TYPE_RANDOM), SetMinimalSize(139, 12), SetDataTip(STR_TREES_RANDOM_TYPE, STR_TREES_RANDOM_TYPE_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, BTW_MANY_RANDOM), SetMinimalSize(139, 12), SetDataTip(STR_TREES_RANDOM_TREES_BUTTON, STR_TREES_RANDOM_TREES_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _build_trees_desc(
	WDP_AUTO, 0, 0,
	WC_BUILD_TREES, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_build_trees_widgets, lengthof(_nested_build_trees_widgets)
);

void ShowBuildTreesToolbar()
{
	if (_game_mode != GM_EDITOR && !Company::IsValidID(_local_company)) return;
	AllocateWindowDescFront<BuildTreesWindow>(&_build_trees_desc, 0);
}
