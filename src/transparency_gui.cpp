/* $Id$ */

/** @file transparency_gui.cpp The transparency GUI. */

#include "stdafx.h"
#include "openttd.h"
#include "window_gui.h"
#include "transparency.h"
#include "sound_func.h"

#include "table/sprites.h"
#include "table/strings.h"

TransparencyOptionBits _transparency_opt;
TransparencyOptionBits _transparency_lock;
TransparencyOptionBits _invisibility_opt;

/** Widget numbers of the transparency window. */
enum TransparencyToolbarWidgets {
	TTW_WIDGET_CLOSEBOX,                 ///< Closebox.
	TTW_WIDGET_CAPTION,                  ///< Titlebar caption.
	TTW_WIDGET_STICKYBOX,                ///< Stickybox.

	/* Button row. */
	TTW_WIDGET_BEGIN,                    ///< First toggle button.
	TTW_WIDGET_SIGNS = TTW_WIDGET_BEGIN, ///< Signs background transparency toggle button.
	TTW_WIDGET_TREES,                    ///< Trees transparency toggle button.
	TTW_WIDGET_HOUSES,                   ///< Houses transparency toggle button.
	TTW_WIDGET_INDUSTRIES,               ///< industries transparency toggle button.
	TTW_WIDGET_BUILDINGS,                ///< Company buildings and structures transparency toggle button.
	TTW_WIDGET_BRIDGES,                  ///< Bridges transparency toggle button.
	TTW_WIDGET_STRUCTURES,               ///< Unmovable structures transparency toggle button.
	TTW_WIDGET_CATENARY,                 ///< Catenary transparency toggle button.
	TTW_WIDGET_LOADING,                  ///< Loading indicators transparency toggle button.
	TTW_WIDGET_END,                      ///< End of toggle buttons.

	/* Panel with buttons for invisibility */
	TTW_WIDGET_BUTTONS = TTW_WIDGET_END, ///< Panel with 'invisibility' buttons.
};

class TransparenciesWindow : public Window
{
public:
	TransparenciesWindow(const WindowDesc *desc, int window_number) : Window(desc, window_number)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		/* must be sure that the widgets show the transparency variable changes
		 * also when we use shortcuts */
		for (uint i = TTW_WIDGET_BEGIN; i < TTW_WIDGET_END; i++) {
			this->SetWidgetLoweredState(i, IsTransparencySet((TransparencyOption)(i - TTW_WIDGET_BEGIN)));
		}

		this->DrawWidgets();
		for (uint i = TO_SIGNS; i < TO_END; i++) {
			if (HasBit(_transparency_lock, i)) DrawSprite(SPR_LOCK, PAL_NONE, this->widget[TTW_WIDGET_BEGIN + i].left + 1, this->widget[TTW_WIDGET_BEGIN + i].top + 1);
		}

		/* Do not draw button for invisible loading indicators */
		for (uint i = TTW_WIDGET_BEGIN; i <= TTW_WIDGET_CATENARY; i++) {
			const Widget *wi = &this->widget[i];
			DrawFrameRect(wi->left + 1, 38, wi->right - 1, 46, COLOUR_PALE_GREEN, HasBit(_invisibility_opt, i - TTW_WIDGET_BEGIN) ? FR_LOWERED : FR_NONE);
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget >= TTW_WIDGET_BEGIN && widget < TTW_WIDGET_END) {
			if (_ctrl_pressed) {
				/* toggle the bit of the transparencies lock variable */
				ToggleTransparencyLock((TransparencyOption)(widget - TTW_WIDGET_BEGIN));
				this->SetDirty();
			} else {
				/* toggle the bit of the transparencies variable and play a sound */
				ToggleTransparency((TransparencyOption)(widget - TTW_WIDGET_BEGIN));
				SndPlayFx(SND_15_BEEP);
				MarkWholeScreenDirty();
			}
		} else if (widget == TTW_WIDGET_BUTTONS) {
			uint x = pt.x / 22;

			if (x > TTW_WIDGET_BRIDGES - TTW_WIDGET_BEGIN) x--;
			if (x > TTW_WIDGET_CATENARY - TTW_WIDGET_BEGIN) return;

			ToggleInvisibility((TransparencyOption)x);
			SndPlayFx(SND_15_BEEP);

			/* Redraw whole screen only if transparency is set */
			if (IsTransparencySet((TransparencyOption)x)) {
				MarkWholeScreenDirty();
			} else {
				this->InvalidateWidget(TTW_WIDGET_BUTTONS);
			}
		}
	}
};

static const Widget _transparency_widgets[] = {
{ WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_DARK_GREEN,   0,  10,   0,  13, STR_BLACK_CROSS,        STR_TOOLTIP_CLOSE_WINDOW},
{  WWT_CAPTION,   RESIZE_NONE,  COLOUR_DARK_GREEN,  11, 206,   0,  13, STR_TRANSPARENCY_TOOLB, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},
{WWT_STICKYBOX,   RESIZE_NONE,  COLOUR_DARK_GREEN, 207, 218,   0,  13, STR_NULL,               STR_STICKY_BUTTON},

/* transparency widgets:
 * transparent signs, trees, houses, industries, company's buildings, bridges, unmovable structures, catenary and loading indicators */
{   WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,   0,  21,  14,  35, SPR_IMG_SIGN,           STR_TRANSPARENT_SIGNS_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,  22,  43,  14,  35, SPR_IMG_PLANTTREES,     STR_TRANSPARENT_TREES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,  44,  65,  14,  35, SPR_IMG_TOWN,           STR_TRANSPARENT_HOUSES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,  66,  87,  14,  35, SPR_IMG_INDUSTRY,       STR_TRANSPARENT_INDUSTRIES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,  88, 109,  14,  35, SPR_IMG_COMPANY_LIST,   STR_TRANSPARENT_BUILDINGS_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN, 110, 152,  14,  35, SPR_IMG_BRIDGE,         STR_TRANSPARENT_BRIDGES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN, 153, 174,  14,  35, SPR_IMG_TRANSMITTER,    STR_TRANSPARENT_STRUCTURES_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN, 175, 196,  14,  35, SPR_BUILD_X_ELRAIL,     STR_TRANSPARENT_CATENARY_DESC},
{   WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN, 197, 218,  14,  35, SPR_IMG_TRAINLIST,      STR_TRANSPARENT_LOADING_DESC},

{    WWT_PANEL,   RESIZE_NONE,  COLOUR_DARK_GREEN,   0, 218,  36,  48, 0x0,                    STR_TRANSPARENT_INVISIBLE_DESC},

{   WIDGETS_END},
};

static const NWidgetPart _nested_transparency_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, TTW_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, TTW_WIDGET_CAPTION), SetDataTip(STR_TRANSPARENCY_TOOLB, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN, TTW_WIDGET_STICKYBOX),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_SIGNS), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_SIGN, STR_TRANSPARENT_SIGNS_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_TREES), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_PLANTTREES, STR_TRANSPARENT_TREES_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_HOUSES), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TOWN, STR_TRANSPARENT_HOUSES_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_INDUSTRIES), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_INDUSTRY, STR_TRANSPARENT_INDUSTRIES_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_BUILDINGS), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_COMPANY_LIST, STR_TRANSPARENT_BUILDINGS_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_BRIDGES), SetMinimalSize(43, 22), SetDataTip(SPR_IMG_BRIDGE, STR_TRANSPARENT_BRIDGES_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_STRUCTURES), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRANSMITTER, STR_TRANSPARENT_STRUCTURES_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_CATENARY), SetMinimalSize(22, 22), SetDataTip(SPR_BUILD_X_ELRAIL, STR_TRANSPARENT_CATENARY_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_LOADING), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRAINLIST, STR_TRANSPARENT_LOADING_DESC),
	EndContainer(),
	/* Panel with 'inivisibility' buttons. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, TTW_WIDGET_END), SetMinimalSize(219, 13), SetDataTip(0x0, STR_TRANSPARENT_INVISIBLE_DESC),
	EndContainer(),
};

static const WindowDesc _transparency_desc(
	WDP_ALIGN_TBR, 94, 219, 49, 219, 49,
	WC_TRANSPARENCY_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_transparency_widgets, _nested_transparency_widgets, lengthof(_nested_transparency_widgets)
);

void ShowTransparencyToolbar()
{
	AllocateWindowDescFront<TransparenciesWindow>(&_transparency_desc, 0);
}
