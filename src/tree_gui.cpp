/* $Id$ */

/** @file tree_gui.cpp GUIs for building trees. */

#include "stdafx.h"
#include "openttd.h"
#include "window_gui.h"
#include "gfx_func.h"
#include "tilehighlight_func.h"
#include "player_func.h"
#include "command_func.h"
#include "sound_func.h"
#include "settings_type.h"

#include "table/sprites.h"
#include "table/strings.h"
#include "table/tree_land.h"

void PlaceTreesRandomly();

class BuildTreesWindow : public Window
{
	uint16 base;
	uint16 count;
	int tree_to_plant;

	enum BuildTreesWidgets {
		BTW_CLOSE,
		BTW_CAPTION,
		BTW_BACKGROUND,
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

public:
	BuildTreesWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		if (_game_mode != GM_EDITOR) {
			this->HideWidget(BTW_MANY_RANDOM);
			int offset = this->widget[BTW_MANY_RANDOM].bottom - this->widget[BTW_MANY_RANDOM].top;
			this->height -= offset;
			this->widget[BTW_BACKGROUND].bottom -= offset;
		}
		ResetObjectToPlace();
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
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

		this->DrawWidgets();

		int i = this->base = _tree_base_by_landscape[_settings.game_creation.landscape];
		int count = this->count = _tree_count_by_landscape[_settings.game_creation.landscape];

		int x = 18;
		int y = 54;
		do {
			DrawSprite(tree_sprites[i].sprite, tree_sprites[i].pal, x, y);
			x += 35;
			if (!(++i & 3)) {
				x -= 35 * 4;
				y += 47;
			}
		} while (--count);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case BTW_TYPE_11: case BTW_TYPE_12: case BTW_TYPE_13: case BTW_TYPE_14:
			case BTW_TYPE_21: case BTW_TYPE_22: case BTW_TYPE_23: case BTW_TYPE_24:
			case BTW_TYPE_31: case BTW_TYPE_32: case BTW_TYPE_33: case BTW_TYPE_34:
				if (widget - BTW_TYPE_11 >= this->count) break;

				if (HandlePlacePushButton(this, widget, SPR_CURSOR_TREE, VHM_RECT, NULL)) {
					this->tree_to_plant = this->base + widget - BTW_TYPE_11;
				}
				break;

			case BTW_TYPE_RANDOM: // tree of random type.
				if (HandlePlacePushButton(this, BTW_TYPE_RANDOM, SPR_CURSOR_TREE, VHM_RECT, NULL)) {
					this->tree_to_plant = -1;
				}
				break;

			case BTW_MANY_RANDOM: // place trees randomly over the landscape
				this->LowerWidget(BTW_MANY_RANDOM);
				this->flags4 |= 5 << WF_TIMEOUT_SHL;
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
			DoCommandP(end_tile, this->tree_to_plant, start_tile, NULL,
				CMD_PLANT_TREE | CMD_MSG(STR_2805_CAN_T_PLANT_TREE_HERE));
		}
	}

	virtual void OnTimeout()
	{
		this->RaiseWidget(BTW_MANY_RANDOM);
		this->InvalidateWidget(BTW_MANY_RANDOM);
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseButtons();
	}
};

static const Widget _build_trees_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,              STR_018B_CLOSE_WINDOW},              // BTW_CLOSE
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   142,     0,    13, STR_2802_TREES,        STR_018C_WINDOW_TITLE_DRAG_THIS},    // BTW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   142,    14,   183, 0x0,                   STR_NULL},                           // BTW_BACKGROUND
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT}, // BTW_TYPE_11
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT}, // BTW_TYPE_12
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT}, // BTW_TYPE_13
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT}, // BTW_TYPE_14
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT}, // BTW_TYPE_21
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT}, // BTW_TYPE_22
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT}, // BTW_TYPE_23
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT}, // BTW_TYPE_24
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT}, // BTW_TYPE_31
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT}, // BTW_TYPE_32
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT}, // BTW_TYPE_33
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT}, // BTW_TYPE_34
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   140,   157,   168, STR_TREES_RANDOM_TYPE, STR_TREES_RANDOM_TYPE_TIP},          // BTW_TYPE_RANDOM
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   140,   170,   181, STR_028A_RANDOM_TREES, STR_028B_PLANT_TREES_RANDOMLY_OVER}, // BTW_MANY_RANDOM
{    WIDGETS_END},
};

static const WindowDesc _build_trees_desc = {
	WDP_AUTO, WDP_AUTO, 143, 184, 143, 184,
	WC_BUILD_TREES, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_trees_widgets,
};

void ShowBuildTreesToolbar()
{
	if (_game_mode != GM_EDITOR && !IsValidPlayer(_current_player)) return;
	AllocateWindowDescFront<BuildTreesWindow>(&_build_trees_desc, 0);
}
