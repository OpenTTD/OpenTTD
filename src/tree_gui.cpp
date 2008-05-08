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

struct tree_d {
	uint16 base;
	uint16 count;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(tree_d));

static int _tree_to_plant;
void PlaceTreesRandomly();

static const PalSpriteID _tree_sprites[] = {
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

static void BuildTreesWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE:
			ResetObjectToPlace();
			break;

		case WE_PAINT: {
			DrawWindowWidgets(w);

			int i = WP(w, tree_d).base = _tree_base_by_landscape[_opt.landscape];
			int count = WP(w, tree_d).count = _tree_count_by_landscape[_opt.landscape];

			int x = 18;
			int y = 54;
			do {
				DrawSprite(_tree_sprites[i].sprite, _tree_sprites[i].pal, x, y);
				x += 35;
				if (!(++i & 3)) {
					x -= 35 * 4;
					y += 47;
				}
			} while (--count);
		} break;

		case WE_CLICK: {
			int wid = e->we.click.widget;

			switch (wid) {
				case 0:
					ResetObjectToPlace();
					break;

				case 3: case 4: case 5: case 6:
				case 7: case 8: case 9: case 10:
				case 11:case 12: case 13: case 14:
					if (wid - 3 >= WP(w, tree_d).count) break;

					if (HandlePlacePushButton(w, wid, SPR_CURSOR_TREE, VHM_RECT, NULL)) {
						_tree_to_plant = WP(w, tree_d).base + wid - 3;
					}
					break;

				case 15: // tree of random type.
					if (HandlePlacePushButton(w, 15, SPR_CURSOR_TREE, VHM_RECT, NULL)) {
						_tree_to_plant = -1;
					}
					break;

				case 16: // place trees randomly over the landscape
					w->LowerWidget(16);
					w->flags4 |= 5 << WF_TIMEOUT_SHL;
					SndPlayFx(SND_15_BEEP);
					PlaceTreesRandomly();
					MarkWholeScreenDirty();
					break;
			}
		} break;

		case WE_PLACE_OBJ:
			VpStartPlaceSizing(e->we.place.tile, VPM_X_AND_Y_LIMITED, DDSP_PLANT_TREES);
			VpSetPlaceSizingLimit(20);
			break;

		case WE_PLACE_DRAG:
			VpSelectTilesWithMethod(e->we.place.pt.x, e->we.place.pt.y, e->we.place.select_method);
			return;

		case WE_PLACE_MOUSEUP:
			if (e->we.place.pt.x != -1 && e->we.place.select_proc == DDSP_PLANT_TREES) {
				DoCommandP(e->we.place.tile, _tree_to_plant, e->we.place.starttile, NULL,
					CMD_PLANT_TREE | CMD_MSG(STR_2805_CAN_T_PLANT_TREE_HERE));
			}
			break;

		case WE_TIMEOUT:
			w->RaiseWidget(16);
			break;

		case WE_ABORT_PLACE_OBJ:
			w->RaiseButtons();
			break;
	}
}

static const Widget _build_trees_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,              STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   142,     0,    13, STR_2802_TREES,        STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   142,    14,   170, 0x0,                   STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   140,   157,   168, STR_TREES_RANDOM_TYPE, STR_TREES_RANDOM_TYPE_TIP},
{    WIDGETS_END},
};

static const WindowDesc _build_trees_desc = {
	497, 22, 143, 171, 143, 171,
	WC_BUILD_TREES, WC_SCEN_LAND_GEN,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_trees_widgets,
	BuildTreesWndProc
};

static const Widget _build_trees_scen_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,              STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   142,     0,    13, STR_2802_TREES,        STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   142,    14,   183, 0x0,                   STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,    16,    61, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,    63,   108, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,    35,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    37,    70,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,    72,   105,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{      WWT_PANEL,   RESIZE_NONE,    14,   107,   140,   110,   155, 0x0,                   STR_280D_SELECT_TREE_TYPE_TO_PLANT},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   140,   157,   168, STR_TREES_RANDOM_TYPE, STR_TREES_RANDOM_TYPE_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   140,   170,   181, STR_028A_RANDOM_TREES, STR_028B_PLANT_TREES_RANDOMLY_OVER},
{    WIDGETS_END},
};

static const WindowDesc _build_trees_scen_desc = {
	WDP_AUTO, WDP_AUTO, 143, 184, 143, 184,
	WC_BUILD_TREES, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_trees_scen_widgets,
	BuildTreesWndProc
};


void ShowBuildTreesToolbar()
{
	if (!IsValidPlayer(_current_player)) return;
	AllocateWindowDescFront<Window>(&_build_trees_desc, 0);
}

void ShowBuildTreesScenToolbar()
{
	AllocateWindowDescFront<Window>(&_build_trees_scen_desc, 0);
}
