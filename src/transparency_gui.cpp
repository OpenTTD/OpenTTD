/* $Id$ */

/** @file transparency_gui.cpp The transparency GUI. */

#include "stdafx.h"
#include "openttd.h"
#include "gui.h"
#include "window_gui.h"
#include "variables.h"
#include "transparency.h"
#include "sound_func.h"

#include "table/sprites.h"
#include "table/strings.h"

TransparencyOptionBits _transparency_opt;
TransparencyOptionBits _transparency_lock;
TransparencyOptionBits _invisibility_opt;

class TransparenciesWindow : public Window
{
	enum TransparencyToolbarWidgets{
		TTW_WIDGET_SIGNS = 3,    ///< Make signs background transparent
		TTW_WIDGET_TREES,        ///< Make trees transparent
		TTW_WIDGET_HOUSES,       ///< Make houses transparent
		TTW_WIDGET_INDUSTRIES,   ///< Make Industries transparent
		TTW_WIDGET_BUILDINGS,    ///< Make player buildings and structures transparent
		TTW_WIDGET_BRIDGES,      ///< Make bridges transparent
		TTW_WIDGET_STRUCTURES,   ///< Make unmovable structures transparent
		TTW_WIDGET_CATENARY,     ///< Make catenary transparent
		TTW_WIDGET_LOADING,      ///< Make loading indicators transparent
		TTW_WIDGET_END,          ///< End of toggle buttons

		/* Panel with buttons for invisibility */
		TTW_BUTTONS = 12,        ///< Panel with 'invisibility' buttons
	};

public:
	TransparenciesWindow(const WindowDesc *desc, void *data, int window_number) : Window(desc, data, window_number)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		/* must be sure that the widgets show the transparency variable changes
		 * also when we use shortcuts */
		for (uint i = TTW_WIDGET_SIGNS; i < TTW_WIDGET_END; i++) {
			this->SetWidgetLoweredState(i, IsTransparencySet((TransparencyOption)(i - TTW_WIDGET_SIGNS)));
		}

		DrawWindowWidgets(this);
		for (uint i = TO_SIGNS; i < TO_END; i++) {
			if (HasBit(_transparency_lock, i)) DrawSprite(SPR_LOCK, PAL_NONE, this->widget[TTW_WIDGET_SIGNS + i].left + 1, this->widget[TTW_WIDGET_SIGNS + i].top + 1);
		}

		/* Do not draw button for invisible loading indicators */
		for (uint i = 0; i < 8; i++) {
			if (i < TTW_WIDGET_BRIDGES - TTW_WIDGET_SIGNS) {
				DrawFrameRect(i * 22, 38, i * 22 + 19, 46, true, HasBit(_invisibility_opt, i) ? FR_LOWERED : FR_NONE);
			} else if (i == TTW_WIDGET_BRIDGES - TTW_WIDGET_SIGNS) {
				DrawFrameRect(i * 22, 38, i * 22 + 41, 46, true, HasBit(_invisibility_opt, i) ? FR_LOWERED : FR_NONE);
			} else { // i > TTW_WIDGET_BRIDGES - TTW_WIDGET_SIGNS
				DrawFrameRect((i + 1) * 22, 38, (i + 1) * 22 + 19, 46, true, HasBit(_invisibility_opt, i) ? FR_LOWERED : FR_NONE);
			}
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget >= TTW_WIDGET_SIGNS && widget < TTW_WIDGET_END) {
			if (_ctrl_pressed) {
				/* toggle the bit of the transparencies lock variable */
				ToggleTransparencyLock((TransparencyOption)(widget - TTW_WIDGET_SIGNS));
				this->SetDirty();
			} else {
				/* toggle the bit of the transparencies variable and play a sound */
				ToggleTransparency((TransparencyOption)(widget - TTW_WIDGET_SIGNS));
				SndPlayFx(SND_15_BEEP);
				MarkWholeScreenDirty();
			}
		} else if (widget == TTW_BUTTONS) {
			uint x = pt.x / 22;

			if (x > TTW_WIDGET_BRIDGES - TTW_WIDGET_SIGNS) x--;
			if (x > TTW_WIDGET_CATENARY - TTW_WIDGET_SIGNS) return;

			ToggleInvisibility((TransparencyOption)x);
			SndPlayFx(SND_15_BEEP);

			/* Redraw whole screen only if transparency is set */
			if (IsTransparencySet((TransparencyOption)x)) {
				MarkWholeScreenDirty();
			} else {
				this->InvalidateWidget(TTW_BUTTONS);
			}
		}
	}
};

static const Widget _transparency_widgets[] = {
{ WWT_CLOSEBOX,   RESIZE_NONE,  7,   0,  10,   0,  13, STR_00C5,                 STR_018B_CLOSE_WINDOW},
{  WWT_CAPTION,   RESIZE_NONE,  7,  11, 206,   0,  13, STR_TRANSPARENCY_TOOLB,   STR_018C_WINDOW_TITLE_DRAG_THIS},
{WWT_STICKYBOX,   RESIZE_NONE,  7, 207, 218,   0,  13, STR_NULL,                 STR_STICKY_BUTTON},

/* transparency widgets:
 * transparent signs, trees, houses, industries, player's buildings, bridges, unmovable structures, catenary and loading indicators */
{   WWT_IMGBTN,   RESIZE_NONE,  7,   0,  21,  14,  35, SPR_IMG_SIGN,         STR_TRANSPARENT_SIGNS_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7,  22,  43,  14,  35, SPR_IMG_PLANTTREES,   STR_TRANSPARENT_TREES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7,  44,  65,  14,  35, SPR_IMG_TOWN,         STR_TRANSPARENT_HOUSES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7,  66,  87,  14,  35, SPR_IMG_INDUSTRY,     STR_TRANSPARENT_INDUSTRIES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7,  88, 109,  14,  35, SPR_IMG_COMPANY_LIST, STR_TRANSPARENT_BUILDINGS_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7, 110, 152,  14,  35, SPR_IMG_BRIDGE,       STR_TRANSPARENT_BRIDGES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7, 153, 174,  14,  35, SPR_IMG_TRANSMITTER,  STR_TRANSPARENT_STRUCTURES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7, 175, 196,  14,  35, SPR_BUILD_X_ELRAIL,   STR_TRANSPARENT_CATENARY_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  7, 197, 218,  14,  35, SPR_IMG_TRAINLIST,    STR_TRANSPARENT_LOADING_DESC},

{    WWT_PANEL,   RESIZE_NONE,  7,   0, 218,  36,  48, 0x0,                  STR_TRANSPARENT_INVISIBLE_DESC},

{   WIDGETS_END},
};

static const WindowDesc _transparency_desc = {
	WDP_ALIGN_TBR, 58+36, 219, 49, 219, 49,
	WC_TRANSPARENCY_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_transparency_widgets,
	NULL
};

void ShowTransparencyToolbar(void)
{
	AllocateWindowDescFront<TransparenciesWindow>(&_transparency_desc, 0);
}
