/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file transparency_gui.cpp The transparency GUI. */

#include "stdafx.h"
#include "window_gui.h"
#include "transparency.h"
#include "sound_func.h"

#include "table/sprites.h"
#include "table/strings.h"

TransparencyOptionBits _transparency_opt;  ///< The bits that should be transparent.
TransparencyOptionBits _transparency_lock; ///< Prevent these bits from flipping with X.
TransparencyOptionBits _invisibility_opt;  ///< The bits that should be invisible.
byte _display_opt; ///< What do we want to draw/do?

/** Widget numbers of the transparency window. */
enum TransparencyToolbarWidgets {
	/* Button row. */
	TTW_WIDGET_BEGIN,                    ///< First toggle button.
	TTW_WIDGET_SIGNS = TTW_WIDGET_BEGIN, ///< Signs background transparency toggle button.
	TTW_WIDGET_TREES,                    ///< Trees transparency toggle button.
	TTW_WIDGET_HOUSES,                   ///< Houses transparency toggle button.
	TTW_WIDGET_INDUSTRIES,               ///< industries transparency toggle button.
	TTW_WIDGET_BUILDINGS,                ///< Company buildings and structures transparency toggle button.
	TTW_WIDGET_BRIDGES,                  ///< Bridges transparency toggle button.
	TTW_WIDGET_STRUCTURES,               ///< Object structure transparency toggle button.
	TTW_WIDGET_CATENARY,                 ///< Catenary transparency toggle button.
	TTW_WIDGET_LOADING,                  ///< Loading indicators transparency toggle button.
	TTW_WIDGET_END,                      ///< End of toggle buttons.

	/* Panel with buttons for invisibility */
	TTW_WIDGET_BUTTONS,                  ///< Panel with 'invisibility' buttons.
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
		this->OnInvalidateData(0); // Must be sure that the widgets show the transparency variable changes, also when we use shortcuts.
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

					const NWidgetBase *wi = this->GetWidget<NWidgetBase>(i);
					DrawFrameRect(wi->pos_x + 1, r.top + 2, wi->pos_x + wi->current_x - 2, r.bottom - 2, COLOUR_PALE_GREEN,
							HasBit(_invisibility_opt, i - TTW_WIDGET_BEGIN) ? FR_LOWERED : FR_NONE);
				}
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
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
				const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(i);
				if (IsInsideBS(pt.x, nwid->pos_x, nwid->current_x)) {
					break;
				}
			}
			if (i == TTW_WIDGET_LOADING || i == TTW_WIDGET_END) return;

			ToggleInvisibility((TransparencyOption)(i - TTW_WIDGET_BEGIN));
			SndPlayFx(SND_15_BEEP);

			/* Redraw whole screen only if transparency is set */
			if (IsTransparencySet((TransparencyOption)(i - TTW_WIDGET_BEGIN))) {
				MarkWholeScreenDirty();
			} else {
				this->SetWidgetDirty(TTW_WIDGET_BUTTONS);
			}
		}
	}

	virtual Point OnInitialPosition(const WindowDesc *desc, int16 sm_width, int16 sm_height, int window_number)
	{
		Point pt = GetToolbarAlignedWindowPosition(sm_width);
		pt.y += 2 * (sm_height - this->GetWidget<NWidgetBase>(TTW_WIDGET_BUTTONS)->current_y);
		return pt;
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		for (uint i = TTW_WIDGET_BEGIN; i < TTW_WIDGET_END; i++) {
			this->SetWidgetLoweredState(i, IsTransparencySet((TransparencyOption)(i - TTW_WIDGET_BEGIN)));
		}
	}
};

static const NWidgetPart _nested_transparency_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_TRANSPARENCY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_SIGNS), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_SIGN, STR_TRANSPARENT_SIGNS_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_TREES), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_PLANTTREES, STR_TRANSPARENT_TREES_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_HOUSES), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_TOWN, STR_TRANSPARENT_HOUSES_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_INDUSTRIES), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_INDUSTRY, STR_TRANSPARENT_INDUSTRIES_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_BUILDINGS), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_COMPANY_LIST, STR_TRANSPARENT_BUILDINGS_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_BRIDGES), SetMinimalSize(43, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BRIDGE, STR_TRANSPARENT_BRIDGES_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_STRUCTURES), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_TRANSMITTER, STR_TRANSPARENT_STRUCTURES_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_CATENARY), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_BUILD_X_ELRAIL, STR_TRANSPARENT_CATENARY_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_WIDGET_LOADING), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_TRAINLIST, STR_TRANSPARENT_LOADING_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetFill(1, 1), EndContainer(),
	EndContainer(),
	/* Panel with 'inivisibility' buttons. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, TTW_WIDGET_BUTTONS), SetMinimalSize(219, 13), SetDataTip(0x0, STR_TRANSPARENT_INVISIBLE_TOOLTIP),
	EndContainer(),
};

static const WindowDesc _transparency_desc(
	WDP_MANUAL, 0, 0,
	WC_TRANSPARENCY_TOOLBAR, WC_NONE,
	0,
	_nested_transparency_widgets, lengthof(_nested_transparency_widgets)
);

/**
 * Show the transparency toolbar.
 */
void ShowTransparencyToolbar()
{
	AllocateWindowDescFront<TransparenciesWindow>(&_transparency_desc, 0);
}
