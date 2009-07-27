/* $Id$ */

/** @file transparency_gui.cpp The transparency GUI. */

#include "stdafx.h"
#include "openttd.h"
#include "window_gui.h"
#include "transparency.h"
#include "sound_func.h"
#include "core/math_func.hpp"

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
	TransparenciesWindow(const WindowDesc *desc, int window_number) : Window()
	{
		this->InitNested(desc, window_number);
	}

	virtual void OnPaint()
	{
		OnInvalidateData(0); // Must be sure that the widgets show the transparency variable changes, also when we use shortcuts.
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case TTW_WIDGET_SIGNS:
			case TTW_WIDGET_TREES:
			case TTW_WIDGET_HOUSES:
			case TTW_WIDGET_INDUSTRIES:
			case TTW_WIDGET_BUILDINGS:
			case TTW_WIDGET_BRIDGES:
			case TTW_WIDGET_STRUCTURES:
			case TTW_WIDGET_CATENARY:
			case TTW_WIDGET_LOADING: {
				uint i = widget - TTW_WIDGET_BEGIN;
				if (HasBit(_transparency_lock, i)) DrawSprite(SPR_LOCK, PAL_NONE, r.left + 1, r.top + 1);
				break;
			}
			case TTW_WIDGET_BUTTONS:
				for (uint i = TTW_WIDGET_BEGIN; i < TTW_WIDGET_END; i++) {
					if (i == TTW_WIDGET_LOADING) continue; // Do not draw button for invisible loading indicators.

					const NWidgetCore *wi = this->nested_array[i];
					DrawFrameRect(wi->pos_x + 1, r.top + 2, wi->pos_x + wi->current_x - 2, r.bottom - 2, COLOUR_PALE_GREEN,
							HasBit(_invisibility_opt, i - TTW_WIDGET_BEGIN) ? FR_LOWERED : FR_NONE);
				}
				break;
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
			uint i;
			for (i = TTW_WIDGET_BEGIN; i < TTW_WIDGET_END; i++) {
				const NWidgetCore *nwid = this->nested_array[i];
				if (IsInsideBS(pt.x, nwid->pos_x, nwid->current_x))
					break;
			}
			if (i == TTW_WIDGET_LOADING || i == TTW_WIDGET_END) return;

			ToggleInvisibility((TransparencyOption)(i - TTW_WIDGET_BEGIN));
			SndPlayFx(SND_15_BEEP);

			/* Redraw whole screen only if transparency is set */
			if (IsTransparencySet((TransparencyOption)(i - TTW_WIDGET_BEGIN))) {
				MarkWholeScreenDirty();
			} else {
				this->InvalidateWidget(TTW_WIDGET_BUTTONS);
			}
		}
	}

	virtual void OnInvalidateData(int data)
	{
		for (uint i = TTW_WIDGET_BEGIN; i < TTW_WIDGET_END; i++) {
			this->SetWidgetLoweredState(i, IsTransparencySet((TransparencyOption)(i - TTW_WIDGET_BEGIN)));
		}
	}
};

static const NWidgetPart _nested_transparency_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, TTW_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, TTW_WIDGET_CAPTION), SetDataTip(STR_TRANSPARENCY_TOOLB, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN, TTW_WIDGET_STICKYBOX),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_SIGNS), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_SIGN, STR_TRANSPARENT_SIGNS_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_TREES), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_PLANTTREES, STR_TRANSPARENT_TREES_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_HOUSES), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_TOWN, STR_TRANSPARENT_HOUSES_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_INDUSTRIES), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_INDUSTRY, STR_TRANSPARENT_INDUSTRIES_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_BUILDINGS), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_COMPANY_LIST, STR_TRANSPARENT_BUILDINGS_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_BRIDGES), SetMinimalSize(43, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BRIDGE, STR_TRANSPARENT_BRIDGES_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_STRUCTURES), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_TRANSMITTER, STR_TRANSPARENT_STRUCTURES_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_CATENARY), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_BUILD_X_ELRAIL, STR_TRANSPARENT_CATENARY_DESC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_LOADING), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_TRAINLIST, STR_TRANSPARENT_LOADING_DESC),
	EndContainer(),
	/* Panel with 'inivisibility' buttons. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, TTW_WIDGET_END), SetMinimalSize(219, 13), SetDataTip(0x0, STR_TRANSPARENT_INVISIBLE_DESC),
	EndContainer(),
};

static const WindowDesc _transparency_desc(
	WDP_ALIGN_TBR, 94, 219, 49, 219, 49,
	WC_TRANSPARENCY_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	NULL, _nested_transparency_widgets, lengthof(_nested_transparency_widgets)
);

void ShowTransparencyToolbar()
{
	AllocateWindowDescFront<TransparenciesWindow>(&_transparency_desc, 0);
}
