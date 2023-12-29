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
#include "settings_type.h"

#include "widgets/transparency_widget.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"

TransparencyOptionBits _transparency_opt;  ///< The bits that should be transparent.
TransparencyOptionBits _transparency_lock; ///< Prevent these bits from flipping with X.
TransparencyOptionBits _invisibility_opt;  ///< The bits that should be invisible.
byte _display_opt; ///< What do we want to draw/do?

class TransparenciesWindow : public Window
{
public:
	TransparenciesWindow(WindowDesc *desc, int window_number) : Window(desc)
	{
		this->InitNested(window_number);
	}

	void OnPaint() override
	{
		this->OnInvalidateData(0); // Must be sure that the widgets show the transparency variable changes, also when we use shortcuts.
		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_TT_SIGNS:
			case WID_TT_TREES:
			case WID_TT_HOUSES:
			case WID_TT_INDUSTRIES:
			case WID_TT_BUILDINGS:
			case WID_TT_BRIDGES:
			case WID_TT_STRUCTURES:
			case WID_TT_CATENARY:
			case WID_TT_TEXT: {
				int i = widget - WID_TT_BEGIN;
				if (HasBit(_transparency_lock, i)) DrawSprite(SPR_LOCK, PAL_NONE, r.left + WidgetDimensions::scaled.fullbevel.left, r.top + WidgetDimensions::scaled.fullbevel.top);
				break;
			}
			case WID_TT_BUTTONS: {
				const Rect fr = r.Shrink(WidgetDimensions::scaled.framerect);
				for (WidgetID i = WID_TT_BEGIN; i < WID_TT_END; i++) {
					if (i == WID_TT_TEXT) continue; // Loading and cost/income text has no invisibility button.

					const Rect wr = this->GetWidget<NWidgetBase>(i)->GetCurrentRect().Shrink(WidgetDimensions::scaled.fullbevel);
					DrawFrameRect(wr.left, fr.top, wr.right, fr.bottom, COLOUR_PALE_GREEN,
							HasBit(_invisibility_opt, i - WID_TT_BEGIN) ? FR_LOWERED : FR_NONE);
				}
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget >= WID_TT_BEGIN && widget < WID_TT_END) {
			if (_ctrl_pressed) {
				/* toggle the bit of the transparencies lock variable */
				ToggleTransparencyLock((TransparencyOption)(widget - WID_TT_BEGIN));
				this->SetDirty();
			} else {
				/* toggle the bit of the transparencies variable and play a sound */
				ToggleTransparency((TransparencyOption)(widget - WID_TT_BEGIN));
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				MarkWholeScreenDirty();
			}
		} else if (widget == WID_TT_BUTTONS) {
			uint i;
			for (i = WID_TT_BEGIN; i < WID_TT_END; i++) {
				const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(i);
				if (IsInsideBS(pt.x, nwid->pos_x, nwid->current_x)) {
					break;
				}
			}
			if (i == WID_TT_TEXT|| i == WID_TT_END) return;

			ToggleInvisibility((TransparencyOption)(i - WID_TT_BEGIN));
			if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);

			/* Redraw whole screen only if transparency is set */
			if (IsTransparencySet((TransparencyOption)(i - WID_TT_BEGIN))) {
				MarkWholeScreenDirty();
			} else {
				this->SetWidgetDirty(WID_TT_BUTTONS);
			}
		}
	}

	Point OnInitialPosition([[maybe_unused]] int16_t sm_width, [[maybe_unused]] int16_t sm_height, [[maybe_unused]] int window_number) override
	{
		Point pt = GetToolbarAlignedWindowPosition(sm_width);
		pt.y += 2 * (sm_height - this->GetWidget<NWidgetBase>(WID_TT_BUTTONS)->current_y);
		return pt;
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		for (WidgetID i = WID_TT_BEGIN; i < WID_TT_END; i++) {
			this->SetWidgetLoweredState(i, IsTransparencySet((TransparencyOption)(i - WID_TT_BEGIN)));
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
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_TT_SIGNS), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_SIGN, STR_TRANSPARENT_SIGNS_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_TT_TREES), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_PLANTTREES, STR_TRANSPARENT_TREES_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_TT_HOUSES), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_TOWN, STR_TRANSPARENT_HOUSES_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_TT_INDUSTRIES), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_INDUSTRY, STR_TRANSPARENT_INDUSTRIES_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_TT_BUILDINGS), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_COMPANY_LIST, STR_TRANSPARENT_BUILDINGS_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_TT_BRIDGES), SetMinimalSize(43, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BRIDGE, STR_TRANSPARENT_BRIDGES_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_TT_STRUCTURES), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_TRANSMITTER, STR_TRANSPARENT_STRUCTURES_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_TT_CATENARY), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_BUILD_X_ELRAIL, STR_TRANSPARENT_CATENARY_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_TT_TEXT), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_TRAINLIST, STR_TRANSPARENT_TEXT_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetFill(1, 1), EndContainer(),
	EndContainer(),
	/* Panel with 'invisibility' buttons. */
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_TT_BUTTONS), SetMinimalSize(219, 13), SetDataTip(0x0, STR_TRANSPARENT_INVISIBLE_TOOLTIP),
	EndContainer(),
};

static WindowDesc _transparency_desc(__FILE__, __LINE__,
	WDP_MANUAL, "toolbar_transparency", 0, 0,
	WC_TRANSPARENCY_TOOLBAR, WC_NONE,
	0,
	std::begin(_nested_transparency_widgets), std::end(_nested_transparency_widgets)
);

/**
 * Show the transparency toolbar.
 */
void ShowTransparencyToolbar()
{
	AllocateWindowDescFront<TransparenciesWindow>(&_transparency_desc, 0);
}
