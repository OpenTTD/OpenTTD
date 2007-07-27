/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "functions.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "sound.h"
#include "variables.h"

enum TransparencyToolbarWidgets{
	/* Widgets not toggled when pressing the X key */
	TTW_WIDGET_SIGNS = 3,    ///< Make signs background transparent

	/* Widgets toggled when pressing the X key */
	TTW_WIDGET_TREES,        ///< Make trees transparent
	TTW_WIDGET_HOUSES,       ///< Make houses transparent
	TTW_WIDGET_INDUSTRIES,   ///< Make Industries transparent
	TTW_WIDGET_BUILDINGS,    ///< Make player buildings and structures transparent
	TTW_WIDGET_BRIDGES,      ///< Make bridges transparent
	TTW_WIDGET_STRUCTURES,   ///< Make unmovable structures transparent
	TTW_WIDGET_LOADING,      ///< Make loading indicators transperent
	TTW_WIDGET_END,          ///< End of toggle buttons
};

/** Toggle the bits of the transparencies variable
 * when clicking on a widget, and play a sound
 * @param widget been clicked.
 */
static void Transparent_Click(byte widget)
{
	TOGGLEBIT(_transparent_opt, widget);
	SndPlayFx(SND_15_BEEP);
}

static void TransparencyToolbWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT:
			/* must be sure that the widgets show the transparency variable changes
			 * also when we use shortcuts */
			for (uint i = TTW_WIDGET_SIGNS; i < TTW_WIDGET_END; i++) {
				SetWindowWidgetLoweredState(w, i, HASBIT(_transparent_opt, i - TTW_WIDGET_SIGNS));
			}
			DrawWindowWidgets(w);
			break;

		case WE_CLICK:
			if (e->we.click.widget >= TTW_WIDGET_SIGNS) {
				Transparent_Click(e->we.click.widget - TTW_WIDGET_SIGNS);
				MarkWholeScreenDirty();
			}
			break;
	}
}

static const Widget _transparency_widgets[] = {
{ WWT_CLOSEBOX,   RESIZE_NONE,  7,   0,  10,   0,  13, STR_00C5,                 STR_018B_CLOSE_WINDOW},
{  WWT_CAPTION,   RESIZE_NONE,  7,  11, 184,   0,  13, STR_TRANSPARENCY_TOOLB,   STR_018C_WINDOW_TITLE_DRAG_THIS},
{WWT_STICKYBOX,   RESIZE_NONE,  7, 185, 196,   0,  13, STR_NULL,                 STR_STICKY_BUTTON},

/* transparency widgets:
 * transparent signs, trees, houses, industries, player's buildings, bridges, unmovable structures and loading indicators */
{   WWT_IMGBTN,   RESIZE_NONE,  7,   0,  21,  14,  35, SPR_IMG_SIGN,         STR_TRANSPARENT_SIGNS_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7,  22,  43,  14,  35, SPR_IMG_PLANTTREES,   STR_TRANSPARENT_TREES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7,  44,  65,  14,  35, SPR_IMG_TOWN,         STR_TRANSPARENT_HOUSES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7,  66,  87,  14,  35, SPR_IMG_INDUSTRY,     STR_TRANSPARENT_INDUSTRIES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7,  88, 109,  14,  35, SPR_IMG_COMPANY_LIST, STR_TRANSPARENT_BUILDINGS_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7, 110, 152,  14,  35, SPR_IMG_BRIDGE,       STR_TRANSPARENT_BRIDGES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7, 153, 174,  14,  35, SPR_IMG_TRANSMITTER,  STR_TRANSPARENT_STRUCTURES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7, 175, 196,  14,  35, SPR_IMG_TRAINLIST,    STR_TRANSPARENT_LOADING_DESC},

{   WIDGETS_END},
};

static const WindowDesc _transparency_desc = {
	WDP_ALIGN_TBR, 58+36, 197, 36, 197, 36,
	WC_TRANSPARENCY_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_transparency_widgets,
	TransparencyToolbWndProc
};

void ShowTransparencyToolbar(void)
{
	AllocateWindowDescFront(&_transparency_desc, 0);
}
