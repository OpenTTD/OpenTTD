/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file industry_gui.cpp GUIs related to industries. */

#include "stdafx.h"
#include "openttd.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "industry.h"
#include "town.h"
#include "variables.h"
#include "cheat_type.h"
#include "newgrf.h"
#include "newgrf_industries.h"
#include "newgrf_text.h"
#include "strings_func.h"
#include "company_func.h"
#include "tilehighlight_func.h"
#include "string_func.h"
#include "sortlist_type.h"
#include "widgets/dropdown_func.h"
#include "company_base.h"

#include "table/strings.h"
#include "table/sprites.h"

bool _ignore_restrictions;

/** Cargo suffix type (for which window is it requested) */
enum CargoSuffixType {
	CST_FUND,  ///< Fund-industry window
	CST_VIEW,  ///< View-industry window
	CST_DIR,   ///< Industry-directory window
};

/**
 * Gets the string to display after the cargo name (using callback 37)
 * @param cargo the cargo for which the suffix is requested
 * - 00 - first accepted cargo type
 * - 01 - second accepted cargo type
 * - 02 - third accepted cargo type
 * - 03 - first produced cargo type
 * - 04 - second produced cargo type
 * @param cst the cargo suffix type (for which window is it requested). @see CargoSuffixType
 * @param ind the industry (NULL if in fund window)
 * @param ind_type the industry type
 * @param indspec the industry spec
 * @param suffix is filled with the string to display
 * @param suffix_last lastof(suffix)
 */
static void GetCargoSuffix(uint cargo, CargoSuffixType cst, const Industry *ind, IndustryType ind_type, const IndustrySpec *indspec, char *suffix, const char *suffix_last)
{
	suffix[0] = '\0';
	if (HasBit(indspec->callback_mask, CBM_IND_CARGO_SUFFIX)) {
		uint16 callback = GetIndustryCallback(CBID_INDUSTRY_CARGO_SUFFIX, 0, (cst << 8) | cargo, const_cast<Industry *>(ind), ind_type, (cst != CST_FUND) ? ind->xy : INVALID_TILE);
		if (GB(callback, 0, 8) != 0xFF) {
			PrepareTextRefStackUsage(6);
			GetString(suffix, GetGRFStringID(indspec->grf_prop.grffile->grfid, 0xD000 + callback), suffix_last);
			StopTextRefStackUsage();
		}
	}
}

/**
 * Gets all strings to display after the cargos of industries (using callback 37)
 * @param cb_offset The offset for the cargo used in cb37, 0 for accepted cargos, 3 for produced cargos
 * @param cst the cargo suffix type (for which window is it requested). @see CargoSuffixType
 * @param ind the industry (NULL if in fund window)
 * @param ind_type the industry type
 * @param indspec the industry spec
 * @param cargos array with cargotypes. for CT_INVALID no suffix will be determined
 * @param suffixes is filled with the suffixes
 */
template <typename TC, typename TS>
static inline void GetAllCargoSuffixes(uint cb_offset, CargoSuffixType cst, const Industry *ind, IndustryType ind_type, const IndustrySpec *indspec, const TC &cargos, TS &suffixes)
{
	assert_compile(lengthof(cargos) <= lengthof(suffixes));
	for (uint j = 0; j < lengthof(cargos); j++) {
		if (cargos[j] != CT_INVALID) {
			GetCargoSuffix(cb_offset + j, cst, ind, ind_type, indspec, suffixes[j], lastof(suffixes[j]));
		} else {
			suffixes[j][0] = '\0';
		}
	}
}

/** Names of the widgets of the dynamic place industries gui */
enum DynamicPlaceIndustriesWidgets {
	DPIW_CLOSEBOX = 0,
	DPIW_CAPTION,
	DPIW_MATRIX_WIDGET,
	DPIW_SCROLLBAR,
	DPIW_INFOPANEL,
	DPIW_FUND_WIDGET,
	DPIW_RESIZE_WIDGET,
};

static const NWidgetPart _nested_build_industry_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, DPIW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, DPIW_CAPTION), SetDataTip(STR_FUND_INDUSTRY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_DARK_GREEN, DPIW_MATRIX_WIDGET), SetDataTip(0x801, STR_FUND_INDUSTRY_SELECTION_TOOLTIP), SetResize(1, 1),
		NWidget(WWT_SCROLLBAR, COLOUR_DARK_GREEN, DPIW_SCROLLBAR), SetResize(0, 1),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, DPIW_INFOPANEL), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, COLOUR_DARK_GREEN, DPIW_FUND_WIDGET), SetResize(1, 0), SetDataTip(STR_JUST_STRING, STR_NULL),
		NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN, DPIW_RESIZE_WIDGET),
	EndContainer(),
};

/** Window definition of the dynamic place industries gui */
static const WindowDesc _build_industry_desc(
	WDP_AUTO, WDP_AUTO, 170, 212, 170, 212,
	WC_BUILD_INDUSTRY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_RESIZABLE | WDF_CONSTRUCTION,
	NULL, _nested_build_industry_widgets, lengthof(_nested_build_industry_widgets)
);

/** Build (fund or prospect) a new industry, */
class BuildIndustryWindow : public Window {
	int selected_index;                         ///< index of the element in the matrix
	IndustryType selected_type;                 ///< industry corresponding to the above index
	uint16 callback_timer;                      ///< timer counter for callback eventual verification
	bool timer_enabled;                         ///< timer can be used
	uint16 count;                               ///< How many industries are loaded
	IndustryType index[NUM_INDUSTRYTYPES + 1];  ///< Type of industry, in the order it was loaded
	bool enabled[NUM_INDUSTRYTYPES + 1];        ///< availability state, coming from CBID_INDUSTRY_AVAILABLE (if ever)

	/** The offset for the text in the matrix. */
	static const int MATRIX_TEXT_OFFSET = 17;

	void SetupArrays()
	{
		this->count = 0;

		for (uint i = 0; i < lengthof(this->index); i++) {
			this->index[i]   = INVALID_INDUSTRYTYPE;
			this->enabled[i] = false;
		}

		if (_game_mode == GM_EDITOR) { // give room for the Many Random "button"
			this->index[this->count] = INVALID_INDUSTRYTYPE;
			this->count++;
			this->timer_enabled = false;
		}
		/* Fill the arrays with industries.
		 * The tests performed after the enabled allow to load the industries
		 * In the same way they are inserted by grf (if any)
		 */
		for (IndustryType ind = 0; ind < NUM_INDUSTRYTYPES; ind++) {
			const IndustrySpec *indsp = GetIndustrySpec(ind);
			if (indsp->enabled) {
				/* Rule is that editor mode loads all industries.
				 * In game mode, all non raw industries are loaded too
				 * and raw ones are loaded only when setting allows it */
				if (_game_mode != GM_EDITOR && indsp->IsRawIndustry() && _settings_game.construction.raw_industry_construction == 0) {
					/* Unselect if the industry is no longer in the list */
					if (this->selected_type == ind) this->selected_index = -1;
					continue;
				}
				this->index[this->count] = ind;
				this->enabled[this->count] = (_game_mode == GM_EDITOR) || CheckIfCallBackAllowsAvailability(ind, IACT_USERCREATION);
				/* Keep the selection to the correct line */
				if (this->selected_type == ind) this->selected_index = this->count;
				this->count++;
			}
		}

		/* first indutry type is selected if the current selection is invalid.
		 * I'll be damned if there are none available ;) */
		if (this->selected_index == -1) {
			this->selected_index = 0;
			this->selected_type = this->index[0];
		}

		this->vscroll.SetCount(this->count);
	}

public:
	BuildIndustryWindow() : Window()
	{
		this->timer_enabled = _loaded_newgrf_features.has_newindustries;

		this->selected_index = -1;
		this->selected_type = INVALID_INDUSTRYTYPE;

		/* Initialize arrays */
		this->SetupArrays();

		this->callback_timer = DAY_TICKS;

		this->InitNested(&_build_industry_desc, 0);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		switch (widget) {
			case DPIW_MATRIX_WIDGET: {
				Dimension d = GetStringBoundingBox(STR_FUND_INDUSTRY_MANY_RANDOM_INDUSTRIES);
				for (byte i = 0; i < this->count; i++) {
					if (this->index[i] == INVALID_INDUSTRYTYPE) continue;
					d = maxdim(d, GetStringBoundingBox(GetIndustrySpec(this->index[i])->name));
				}
				resize->height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;
				d.width += MATRIX_TEXT_OFFSET + padding.width;
				d.height = 5 * resize->height;
				*size = maxdim(*size, d);
				break;
			}

			case DPIW_INFOPANEL: {
				/* Extra line for cost outside of editor + extra lines for 'extra' information for NewGRFs. */
				int height = 2 + (_game_mode == GM_EDITOR ? 0 : 1) + (_loaded_newgrf_features.has_newindustries ? 4 : 0);
				Dimension d = {0, 0};
				for (byte i = 0; i < this->count; i++) {
					if (this->index[i] == INVALID_INDUSTRYTYPE) continue;

					const IndustrySpec *indsp = GetIndustrySpec(this->index[i]);

					char cargo_suffix[3][512];
					GetAllCargoSuffixes(0, CST_FUND, NULL, this->selected_type, indsp, indsp->accepts_cargo, cargo_suffix);
					StringID str = STR_INDUSTRY_VIEW_REQUIRES_CARGO;
					byte p = 0;
					SetDParam(0, STR_JUST_NOTHING);
					SetDParamStr(1, "");
					for (byte j = 0; j < lengthof(indsp->accepts_cargo); j++) {
						if (indsp->accepts_cargo[j] == CT_INVALID) continue;
						if (p > 0) str++;
						SetDParam(p++, CargoSpec::Get(indsp->accepts_cargo[j])->name);
						SetDParamStr(p++, cargo_suffix[j]);
					}
					d = maxdim(d, GetStringBoundingBox(str));

					/* Draw the produced cargos, if any. Otherwhise, will print "Nothing" */
					GetAllCargoSuffixes(3, CST_FUND, NULL, this->selected_type, indsp, indsp->produced_cargo, cargo_suffix);
					str = STR_INDUSTRY_VIEW_PRODUCES_CARGO;
					p = 0;
					SetDParam(0, STR_JUST_NOTHING);
					SetDParamStr(1, "");
					for (byte j = 0; j < lengthof(indsp->produced_cargo); j++) {
						if (indsp->produced_cargo[j] == CT_INVALID) continue;
						if (p > 0) str++;
						SetDParam(p++, CargoSpec::Get(indsp->produced_cargo[j])->name);
						SetDParamStr(p++, cargo_suffix[j]);
					}
					d = maxdim(d, GetStringBoundingBox(str));
				}

				/* Set it to something more sane :) */
				size->height = height * FONT_HEIGHT_NORMAL + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				size->width  = d.width + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
			} break;

			case DPIW_FUND_WIDGET: {
				Dimension d = GetStringBoundingBox(STR_FUND_INDUSTRY_BUILD_NEW_INDUSTRY);
				d = maxdim(d, GetStringBoundingBox(STR_FUND_INDUSTRY_PROSPECT_NEW_INDUSTRY));
				d = maxdim(d, GetStringBoundingBox(STR_FUND_INDUSTRY_FUND_NEW_INDUSTRY));
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case DPIW_FUND_WIDGET:
				/* Raw industries might be prospected. Show this fact by changing the string
				 * In Editor, you just build, while ingame, or you fund or you prospect */
				if (_game_mode == GM_EDITOR) {
					/* We've chosen many random industries but no industries have been specified */
					SetDParam(0, STR_FUND_INDUSTRY_BUILD_NEW_INDUSTRY);
				} else {
					const IndustrySpec *indsp = GetIndustrySpec(this->index[this->selected_index]);
					SetDParam(0, (_settings_game.construction.raw_industry_construction == 2 && indsp->IsRawIndustry()) ? STR_FUND_INDUSTRY_PROSPECT_NEW_INDUSTRY : STR_FUND_INDUSTRY_FUND_NEW_INDUSTRY);
				}
				break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case DPIW_MATRIX_WIDGET:
				for (byte i = 0; i < this->vscroll.GetCapacity() && i + this->vscroll.GetPosition() < this->count; i++) {
					int x = r.left + WD_MATRIX_LEFT;
					int y = r.top + WD_MATRIX_TOP + i * this->resize.step_height;
					bool selected = this->selected_index == i + this->vscroll.GetPosition();

					if (this->index[i + this->vscroll.GetPosition()] == INVALID_INDUSTRYTYPE) {
						DrawString(x + MATRIX_TEXT_OFFSET, r.right - WD_MATRIX_RIGHT, y, STR_FUND_INDUSTRY_MANY_RANDOM_INDUSTRIES, selected ? TC_WHITE : TC_ORANGE);
						continue;
					}
					const IndustrySpec *indsp = GetIndustrySpec(this->index[i + this->vscroll.GetPosition()]);

					/* Draw the name of the industry in white is selected, otherwise, in orange */
					DrawString(x + MATRIX_TEXT_OFFSET, r.right - WD_MATRIX_RIGHT, y, indsp->name, selected ? TC_WHITE : TC_ORANGE);
					GfxFillRect(x,     y + 1,  x + 10, y + 7, selected ? 15 : 0);
					GfxFillRect(x + 1, y + 2,  x +  9, y + 6, indsp->map_colour);
				}
				break;

			case DPIW_INFOPANEL: {
				int y      = r.top    + WD_FRAMERECT_TOP;
				int bottom = r.bottom - WD_FRAMERECT_BOTTOM;
				int left   = r.left   + WD_FRAMERECT_LEFT;
				int right  = r.right  - WD_FRAMERECT_RIGHT;

				if (this->selected_type == INVALID_INDUSTRYTYPE) {
					DrawStringMultiLine(left, right, y,  bottom, STR_FUND_INDUSTRY_MANY_RANDOM_INDUSTRIES_TOOLTIP);
					break;
				}

				const IndustrySpec *indsp = GetIndustrySpec(this->selected_type);

				if (_game_mode != GM_EDITOR) {
					SetDParam(0, indsp->GetConstructionCost());
					DrawString(left, right, y, STR_FUND_INDUSTRY_INDUSTRY_BUILD_COST);
					y += FONT_HEIGHT_NORMAL;
				}

				/* Draw the accepted cargos, if any. Otherwhise, will print "Nothing" */
				char cargo_suffix[3][512];
				GetAllCargoSuffixes(0, CST_FUND, NULL, this->selected_type, indsp, indsp->accepts_cargo, cargo_suffix);
				StringID str = STR_INDUSTRY_VIEW_REQUIRES_CARGO;
				byte p = 0;
				SetDParam(0, STR_JUST_NOTHING);
				SetDParamStr(1, "");
				for (byte j = 0; j < lengthof(indsp->accepts_cargo); j++) {
					if (indsp->accepts_cargo[j] == CT_INVALID) continue;
					if (p > 0) str++;
					SetDParam(p++, CargoSpec::Get(indsp->accepts_cargo[j])->name);
					SetDParamStr(p++, cargo_suffix[j]);
				}
				DrawString(left, right, y, str);
				y += FONT_HEIGHT_NORMAL;

				/* Draw the produced cargos, if any. Otherwhise, will print "Nothing" */
				GetAllCargoSuffixes(3, CST_FUND, NULL, this->selected_type, indsp, indsp->produced_cargo, cargo_suffix);
				str = STR_INDUSTRY_VIEW_PRODUCES_CARGO;
				p = 0;
				SetDParam(0, STR_JUST_NOTHING);
				SetDParamStr(1, "");
				for (byte j = 0; j < lengthof(indsp->produced_cargo); j++) {
					if (indsp->produced_cargo[j] == CT_INVALID) continue;
					if (p > 0) str++;
					SetDParam(p++, CargoSpec::Get(indsp->produced_cargo[j])->name);
					SetDParamStr(p++, cargo_suffix[j]);
				}
				DrawString(left, right, y, str);
				y += FONT_HEIGHT_NORMAL;

				/* Get the additional purchase info text, if it has not already been queried. */
				str = STR_NULL;
				if (HasBit(indsp->callback_mask, CBM_IND_FUND_MORE_TEXT)) {
					uint16 callback_res = GetIndustryCallback(CBID_INDUSTRY_FUND_MORE_TEXT, 0, 0, NULL, this->selected_type, INVALID_TILE);
					if (callback_res != CALLBACK_FAILED) {  // Did it fail?
						str = GetGRFStringID(indsp->grf_prop.grffile->grfid, 0xD000 + callback_res);  // No. here's the new string
					}
				}

				/* Draw the Additional purchase text, provided by newgrf callback, if any.
				 * Otherwhise, will print Nothing */
				if (str != STR_NULL && str != STR_UNDEFINED) {
					SetDParam(0, str);
					DrawStringMultiLine(left, right, y, bottom, STR_JUST_STRING);
				}
			} break;
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void OnDoubleClick(Point pt, int widget)
	{
		if (widget != DPIW_MATRIX_WIDGET) return;
		this->OnClick(pt, DPIW_FUND_WIDGET);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case DPIW_MATRIX_WIDGET: {
				const IndustrySpec *indsp;
				int y = (pt.y - this->GetWidget<NWidgetBase>(DPIW_MATRIX_WIDGET)->pos_y) / this->resize.step_height + this->vscroll.GetPosition() ;

				if (y >= 0 && y < count) { // Is it within the boundaries of available data?
					this->selected_index = y;
					this->selected_type = this->index[y];
					indsp = (this->selected_type == INVALID_INDUSTRYTYPE) ? NULL : GetIndustrySpec(this->selected_type);

					this->SetDirty();

					if ((_game_mode != GM_EDITOR && _settings_game.construction.raw_industry_construction == 2 && indsp != NULL && indsp->IsRawIndustry()) ||
							this->selected_type == INVALID_INDUSTRYTYPE) {
						/* Reset the button state if going to prospecting or "build many industries" */
						this->RaiseButtons();
						ResetObjectToPlace();
					}

					this->SetWidgetDisabledState(DPIW_FUND_WIDGET, !this->enabled[this->selected_index]);
				}
			} break;

			case DPIW_FUND_WIDGET: {
				if (this->selected_type == INVALID_INDUSTRYTYPE) {
					this->HandleButtonClick(DPIW_FUND_WIDGET);

					if (Town::GetNumItems() == 0) {
						ShowErrorMessage(STR_ERROR_CAN_T_GENERATE_INDUSTRIES, STR_ERROR_MUST_FOUND_TOWN_FIRST, 0, 0);
					} else {
						extern void GenerateIndustries();
						_generating_world = true;
						GenerateIndustries();
						_generating_world = false;
					}
				} else if (_game_mode != GM_EDITOR && _settings_game.construction.raw_industry_construction == 2 && GetIndustrySpec(this->selected_type)->IsRawIndustry()) {
					DoCommandP(0, this->selected_type, InteractiveRandom(), CMD_BUILD_INDUSTRY | CMD_MSG(STR_ERROR_CAN_T_CONSTRUCT_THIS_INDUSTRY));
					this->HandleButtonClick(DPIW_FUND_WIDGET);
				} else {
					HandlePlacePushButton(this, DPIW_FUND_WIDGET, SPR_CURSOR_INDUSTRY, HT_RECT, NULL);
				}
			} break;
		}
	}

	virtual void OnResize()
	{
		/* Adjust the number of items in the matrix depending of the resize */
		this->vscroll.SetCapacity(this->GetWidget<NWidgetBase>(DPIW_MATRIX_WIDGET)->current_y / this->resize.step_height);
		this->GetWidget<NWidgetCore>(DPIW_MATRIX_WIDGET)->widget_data = (this->vscroll.GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		bool success = true;
		/* We do not need to protect ourselves against "Random Many Industries" in this mode */
		const IndustrySpec *indsp = GetIndustrySpec(this->selected_type);
		uint32 seed = InteractiveRandom();

		if (_game_mode == GM_EDITOR) {
			/* Show error if no town exists at all */
			if (Town::GetNumItems() == 0) {
				SetDParam(0, indsp->name);
				ShowErrorMessage(STR_ERROR_CAN_T_BUILD_HERE, STR_ERROR_MUST_FOUND_TOWN_FIRST, pt.x, pt.y);
				return;
			}

			_current_company = OWNER_NONE;
			_generating_world = true;
			_ignore_restrictions = true;
			success = DoCommandP(tile, (InteractiveRandomRange(indsp->num_table) << 8) | this->selected_type, seed, CMD_BUILD_INDUSTRY | CMD_MSG(STR_ERROR_CAN_T_CONSTRUCT_THIS_INDUSTRY));
			if (!success) {
				SetDParam(0, indsp->name);
				ShowErrorMessage(STR_ERROR_CAN_T_BUILD_HERE, _error_message, pt.x, pt.y);
			}

			_ignore_restrictions = false;
			_generating_world = false;
		} else {
			success = DoCommandP(tile, (InteractiveRandomRange(indsp->num_table) << 8) | this->selected_type, seed, CMD_BUILD_INDUSTRY | CMD_MSG(STR_ERROR_CAN_T_CONSTRUCT_THIS_INDUSTRY));
		}

		/* If an industry has been built, just reset the cursor and the system */
		if (success && !_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
	}

	virtual void OnTick()
	{
		if (_pause_mode != PM_UNPAUSED) return;
		if (!this->timer_enabled) return;
		if (--this->callback_timer == 0) {
			/* We have just passed another day.
			 * See if we need to update availability of currently selected industry */
			this->callback_timer = DAY_TICKS; // restart counter

			const IndustrySpec *indsp = GetIndustrySpec(this->selected_type);

			if (indsp->enabled) {
				bool call_back_result = CheckIfCallBackAllowsAvailability(this->selected_type, IACT_USERCREATION);

				/* Only if result does match the previous state would it require a redraw. */
				if (call_back_result != this->enabled[this->selected_index]) {
					this->enabled[this->selected_index] = call_back_result;
					this->SetWidgetDisabledState(DPIW_FUND_WIDGET, !this->enabled[this->selected_index]);
					this->SetDirty();
				}
			}
		}
	}

	virtual void OnTimeout()
	{
		this->RaiseButtons();
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseButtons();
	}

	virtual void OnInvalidateData(int data = 0)
	{
		this->SetupArrays();

		const IndustrySpec *indsp = (this->selected_type == INVALID_INDUSTRYTYPE) ? NULL : GetIndustrySpec(this->selected_type);
		if (indsp == NULL) this->enabled[this->selected_index] = _settings_game.difficulty.number_industries != 0;
		this->SetWidgetDisabledState(DPIW_FUND_WIDGET, !this->enabled[this->selected_index]);
	}
};

void ShowBuildIndustryWindow()
{
	if (_game_mode != GM_EDITOR && !Company::IsValidID(_local_company)) return;
	if (BringWindowToFrontById(WC_BUILD_INDUSTRY, 0)) return;
	new BuildIndustryWindow();
}

static void UpdateIndustryProduction(Industry *i);

static inline bool IsProductionMinimum(const Industry *i, int pt)
{
	return i->production_rate[pt] == 0;
}

static inline bool IsProductionMaximum(const Industry *i, int pt)
{
	return i->production_rate[pt] >= 255;
}

static inline bool IsProductionAlterable(const Industry *i)
{
	return ((_game_mode == GM_EDITOR || _cheats.setup_prod.value) &&
			(i->accepts_cargo[0] == CT_INVALID || i->accepts_cargo[0] == CT_VALUABLES));
}

/** Names of the widgets of the view industry gui */
enum IndustryViewWidgets {
	IVW_CLOSEBOX = 0,
	IVW_CAPTION,
	IVW_STICKY,
	IVW_BACKGROUND,
	IVW_INSET,
	IVW_VIEWPORT,
	IVW_INFO,
	IVW_GOTO,
	IVW_SPACER,
	IVW_RESIZE,
};

class IndustryViewWindow : public Window
{
	byte editbox_line;        ///< The line clicked to open the edit box
	byte clicked_line;        ///< The line of the button that has been clicked
	byte clicked_button;      ///< The button that has been clicked (to raise)
	byte production_offset_y; ///< The offset of the production texts/buttons
	int info_height;          ///< Height needed for the #IVW_INFO panel

public:
	IndustryViewWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->flags4 |= WF_DISABLE_VP_SCROLL;
		this->editbox_line = 0;
		this->clicked_line = 0;
		this->clicked_button = 0;
		this->info_height = WD_FRAMERECT_TOP + 2 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM + 1; // Info panel has at least two lines text.

		this->InitNested(desc, window_number);
		NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(IVW_VIEWPORT);
		nvp->InitializeViewport(this, Industry::Get(window_number)->xy + TileDiffXY(1, 1), ZOOM_LVL_INDUSTRY);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(IVW_INFO);
		uint expected = this->DrawInfo(nwi->pos_x, nwi->pos_x + nwi->current_x - 1, nwi->pos_y) - nwi->pos_y;
		if (expected > nwi->current_y - 1) {
			this->info_height = expected + 1;
			this->ReInit();
			return;
		}
	}

	/** Draw the text in the #IVW_INFO panel.
	 * @param left  Left edge of the panel.
	 * @param right Right edge of the panel.
	 * @param top   Top edge of the panel.
	 * @return Expected position of the bottom edge of the panel.
	 */
	int DrawInfo(uint left, uint right, uint top)
	{
		Industry *i = Industry::Get(this->window_number);
		const IndustrySpec *ind = GetIndustrySpec(i->type);
		int y = top + WD_FRAMERECT_TOP;
		bool first = true;
		bool has_accept = false;
		char cargo_suffix[3][512];

		if (HasBit(ind->callback_mask, CBM_IND_PRODUCTION_CARGO_ARRIVAL) || HasBit(ind->callback_mask, CBM_IND_PRODUCTION_256_TICKS)) {
			GetAllCargoSuffixes(0, CST_VIEW, i, i->type, ind, i->accepts_cargo, cargo_suffix);
			for (byte j = 0; j < lengthof(i->accepts_cargo); j++) {
				if (i->accepts_cargo[j] == CT_INVALID) continue;
				has_accept = true;
				if (first) {
					DrawString(left + WD_FRAMERECT_LEFT, right - WD_FRAMERECT_RIGHT, y, STR_INDUSTRY_VIEW_WAITING_FOR_PROCESSING);
					y += FONT_HEIGHT_NORMAL;
					first = false;
				}
				SetDParam(0, i->accepts_cargo[j]);
				SetDParam(1, i->incoming_cargo_waiting[j]);
				SetDParamStr(2, cargo_suffix[j]);
				DrawString(left + WD_FRAMETEXT_LEFT, right - WD_FRAMERECT_RIGHT, y, STR_INDUSTRY_VIEW_WAITING_STOCKPILE_CARGO);
				y += FONT_HEIGHT_NORMAL;
			}
		} else {
			GetAllCargoSuffixes(0, CST_VIEW, i, i->type, ind, i->accepts_cargo, cargo_suffix);
			StringID str = STR_INDUSTRY_VIEW_REQUIRES_CARGO;
			byte p = 0;
			for (byte j = 0; j < lengthof(i->accepts_cargo); j++) {
				if (i->accepts_cargo[j] == CT_INVALID) continue;
				has_accept = true;
				if (p > 0) str++;
				SetDParam(p++, CargoSpec::Get(i->accepts_cargo[j])->name);
				SetDParamStr(p++, cargo_suffix[j]);
			}
			if (has_accept) {
				DrawString(left + WD_FRAMERECT_LEFT, right - WD_FRAMERECT_RIGHT, y, str);
				y += FONT_HEIGHT_NORMAL;
			}
		}

		GetAllCargoSuffixes(3, CST_VIEW, i, i->type, ind, i->produced_cargo, cargo_suffix);
		first = true;
		for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
			if (i->produced_cargo[j] == CT_INVALID) continue;
			if (first) {
				if (has_accept) y += WD_PAR_VSEP_WIDE;
				DrawString(left + WD_FRAMERECT_LEFT, right - WD_FRAMERECT_RIGHT, y, STR_INDUSTRY_VIEW_PRODUCTION_LAST_MONTH_TITLE);
				y += FONT_HEIGHT_NORMAL;
				this->production_offset_y = y;
				first = false;
			}

			SetDParam(0, i->produced_cargo[j]);
			SetDParam(1, i->last_month_production[j]);
			SetDParamStr(2, cargo_suffix[j]);
			SetDParam(3, ToPercent8(i->last_month_pct_transported[j]));
			uint x = left + WD_FRAMETEXT_LEFT + (IsProductionAlterable(i) ? 30 : 0);
			DrawString(x, right - WD_FRAMERECT_RIGHT, y, STR_INDUSTRY_VIEW_TRANSPORTED);
			/* Let's put out those buttons.. */
			if (IsProductionAlterable(i)) {
				DrawArrowButtons(left + WD_FRAMETEXT_LEFT, y, COLOUR_YELLOW, (this->clicked_line == j + 1) ? this->clicked_button : 0,
						!IsProductionMinimum(i, j), !IsProductionMaximum(i, j));
			}
			y += FONT_HEIGHT_NORMAL;
		}

		/* Get the extra message for the GUI */
		if (HasBit(ind->callback_mask, CBM_IND_WINDOW_MORE_TEXT)) {
			uint16 callback_res = GetIndustryCallback(CBID_INDUSTRY_WINDOW_MORE_TEXT, 0, 0, i, i->type, i->xy);
			if (callback_res != CALLBACK_FAILED) {
				StringID message = GetGRFStringID(ind->grf_prop.grffile->grfid, 0xD000 + callback_res);
				if (message != STR_NULL && message != STR_UNDEFINED) {
					y += WD_PAR_VSEP_WIDE;

					PrepareTextRefStackUsage(6);
					/* Use all the available space left from where we stand up to the
					 * end of the window. We ALSO enlarge the window if needed, so we
					 * can 'go' wild with the bottom of the window. */
					y = DrawStringMultiLine(left + WD_FRAMERECT_LEFT, right - WD_FRAMERECT_RIGHT, y, UINT16_MAX, message);
					StopTextRefStackUsage();
				}
			}
		}
		return y + WD_FRAMERECT_BOTTOM;
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget== IVW_CAPTION) SetDParam(0, this->window_number);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		if (widget == IVW_INFO) size->height = this->info_height;
	}

	virtual void OnClick(Point pt, int widget)
	{
		Industry *i;

		switch (widget) {
			case IVW_INFO: {
				i = Industry::Get(this->window_number);

				/* We should work if needed.. */
				if (!IsProductionAlterable(i)) return;
				uint x = pt.x;
				int line = (pt.y - this->production_offset_y) / FONT_HEIGHT_NORMAL;
				if (pt.y >= this->production_offset_y && IsInsideMM(line, 0, 2) && i->produced_cargo[line] != CT_INVALID) {
					NWidgetCore *nwi = this->GetWidget<NWidgetCore>(widget);
					uint left = nwi->pos_x + WD_FRAMETEXT_LEFT;
					uint right = nwi->pos_x + nwi->current_x - 1 - WD_FRAMERECT_RIGHT;
					if (IsInsideMM(x, left, left + 20) ) {
						/* Clicked buttons, decrease or increase production */
						if (x < left + 10) {
							if (IsProductionMinimum(i, line)) return;
							i->production_rate[line] = max(i->production_rate[line] / 2, 0);
						} else {
							/* a zero production industry is unlikely to give anything but zero, so push it a little bit */
							int new_prod = i->production_rate[line] == 0 ? 1 : i->production_rate[line] * 2;
							if (IsProductionMaximum(i, line)) return;
							i->production_rate[line] = minu(new_prod, 255);
						}

						UpdateIndustryProduction(i);
						this->SetDirty();
						this->flags4 |= WF_TIMEOUT_BEGIN;
						this->clicked_line = line + 1;
						this->clicked_button = (x < left + 10 ? 1 : 2);
					} else if (IsInsideMM(x, left + 30, right)) {
						/* clicked the text */
						this->editbox_line = line;
						SetDParam(0, i->production_rate[line] * 8);
						ShowQueryString(STR_JUST_INT, STR_CONFIG_GAME_PRODUCTION, 10, 100, this, CS_ALPHANUMERAL, QSF_NONE);
					}
				}
			} break;

			case IVW_GOTO:
				i = Industry::Get(this->window_number);
				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(i->xy + TileDiffXY(1, 1));
				} else {
					ScrollMainWindowToTile(i->xy + TileDiffXY(1, 1));
				}
				break;
		}
	}

	virtual void OnTimeout()
	{
		this->clicked_line = 0;
		this->clicked_button = 0;
		this->SetDirty();
	}

	virtual void OnResize()
	{
		if (this->viewport != NULL) {
			NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(IVW_VIEWPORT);
			nvp->UpdateViewportCoordinates(this);
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (StrEmpty(str)) return;

		Industry *i = Industry::Get(this->window_number);
		int line = this->editbox_line;

		i->production_rate[line] = ClampU(atoi(str) / 8, 0, 255);
		UpdateIndustryProduction(i);
		this->SetDirty();
	}
};

static void UpdateIndustryProduction(Industry *i)
{
	for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
		if (i->produced_cargo[j] != CT_INVALID) {
			i->last_month_production[j] = 8 * i->production_rate[j];
		}
	}
}

/** Widget definition of the view industy gui */
static const NWidgetPart _nested_industry_view_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_CREAM, IVW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_CREAM, IVW_CAPTION), SetMinimalSize(237, 14), SetDataTip(STR_INDUSTRY_VIEW_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetResize(1, 0),
		NWidget(WWT_STICKYBOX, COLOUR_CREAM, IVW_STICKY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_CREAM, IVW_BACKGROUND),
		NWidget(WWT_INSET, COLOUR_CREAM, IVW_INSET), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, IVW_VIEWPORT), SetMinimalSize(254, 86), SetFill(true, false), SetPadding(1, 1, 1, 1), SetResize(1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_CREAM, IVW_INFO), SetMinimalSize(260, 2), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_CREAM, IVW_GOTO), SetMinimalSize(130, 12), SetDataTip(STR_BUTTON_LOCATION, STR_INDUSTRY_VIEW_LOCATION_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_CREAM, IVW_SPACER), /*SetMinimalSize(118, 12),*/ SetResize(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_CREAM, IVW_RESIZE),
	EndContainer(),
};

/** Window definition of the view industy gui */
static const WindowDesc _industry_view_desc(
	WDP_AUTO, WDP_AUTO, 260, 120, 260, 120,
	WC_INDUSTRY_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	NULL, _nested_industry_view_widgets, lengthof(_nested_industry_view_widgets)
);

void ShowIndustryViewWindow(int industry)
{
	AllocateWindowDescFront<IndustryViewWindow>(&_industry_view_desc, industry);
}

/** Names of the widgets of the industry directory gui */
enum IndustryDirectoryWidgets {
	IDW_CLOSEBOX = 0,
	IDW_CAPTION,
	IDW_STICKY,
	IDW_DROPDOWN_ORDER,
	IDW_DROPDOWN_CRITERIA,
	IDW_SPACER,
	IDW_INDUSTRY_LIST,
	IDW_SCROLLBAR,
	IDW_RESIZE,
};

/** Widget definition of the industy directory gui */
static const NWidgetPart _nested_industry_directory_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN, IDW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_BROWN, IDW_CAPTION), SetDataTip(STR_INDUSTRY_DIRECTORY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN, IDW_STICKY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, COLOUR_BROWN, IDW_DROPDOWN_ORDER), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
				NWidget(WWT_DROPDOWN, COLOUR_BROWN, IDW_DROPDOWN_CRITERIA), SetDataTip(STR_JUST_STRING, STR_TOOLTIP_SORT_CRITERIAP),
				NWidget(WWT_PANEL, COLOUR_BROWN, IDW_SPACER), SetResize(1, 0),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_BROWN, IDW_INDUSTRY_LIST), SetDataTip(0x0, STR_INDUSTRY_DIRECTORY_LIST_CAPTION), SetResize(1, 1),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_SCROLLBAR, COLOUR_BROWN, IDW_SCROLLBAR), SetResize(0, 1),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN, IDW_RESIZE),
		EndContainer(),
	EndContainer(),
};

typedef GUIList<const Industry*> GUIIndustryList;


/**
 * The list of industries.
 */
class IndustryDirectoryWindow : public Window {
protected:
	/* Runtime saved values */
	static Listing last_sorting;
	static const Industry *last_industry;

	/* Constants for sorting stations */
	static const StringID sorter_names[];
	static GUIIndustryList::SortFunction * const sorter_funcs[];

	GUIIndustryList industries;

	/** (Re)Build industries list */
	void BuildSortIndustriesList()
	{
		if (this->industries.NeedRebuild()) {
			this->industries.Clear();

			const Industry *i;
			FOR_ALL_INDUSTRIES(i) {
				*this->industries.Append() = i;
			}

			this->industries.Compact();
			this->industries.RebuildDone();
			this->vscroll.SetCount(this->industries.Length()); // Update scrollbar as well.
		}

		if (!this->industries.Sort()) return;
		IndustryDirectoryWindow::last_industry = NULL; // Reset name sorter sort cache
		this->SetWidgetDirty(IDW_INDUSTRY_LIST); // Set the modified widget dirty
	}

	/**
	 * Returns percents of cargo transported if industry produces this cargo, else -1
	 *
	 * @param i industry to check
	 * @param id cargo slot
	 * @return percents of cargo transported, or -1 if industry doesn't use this cargo slot
	 */
	static inline int GetCargoTransportedPercentsIfValid(const Industry *i, uint id)
	{
		assert(id < lengthof(i->produced_cargo));

		if (i->produced_cargo[id] == CT_INVALID) return 101;
		return ToPercent8(i->last_month_pct_transported[id]);
	}

	/**
	 * Returns value representing industry's transported cargo
	 *  percentage for industry sorting
	 *
	 * @param i industry to check
	 * @return value used for sorting
	 */
	static int GetCargoTransportedSortValue(const Industry *i)
	{
		int p1 = GetCargoTransportedPercentsIfValid(i, 0);
		int p2 = GetCargoTransportedPercentsIfValid(i, 1);

		if (p1 > p2) Swap(p1, p2); // lower value has higher priority

		return (p1 << 8) + p2;
	}

	/** Sort industries by name */
	static int CDECL IndustryNameSorter(const Industry * const *a, const Industry * const *b)
	{
		static char buf_cache[96];
		static char buf[96];

		SetDParam(0, (*a)->town->index);
		GetString(buf, STR_TOWN_NAME, lastof(buf));

		if (*b != last_industry) {
			last_industry = *b;
			SetDParam(0, (*b)->town->index);
			GetString(buf_cache, STR_TOWN_NAME, lastof(buf_cache));
		}

		return strcmp(buf, buf_cache);
	}

	/** Sort industries by type and name */
	static int CDECL IndustryTypeSorter(const Industry * const *a, const Industry * const *b)
	{
		int r = (*a)->type - (*b)->type;
		return (r == 0) ? IndustryNameSorter(a, b) : r;
	}

	/** Sort industries by production and name */
	static int CDECL IndustryProductionSorter(const Industry * const *a, const Industry * const *b)
	{
		int r = 0;

		if ((*a)->produced_cargo[0] == CT_INVALID) {
			if ((*b)->produced_cargo[0] != CT_INVALID) return -1;
		} else {
			if ((*b)->produced_cargo[0] == CT_INVALID) return 1;

			r = ((*a)->last_month_production[0] + (*a)->last_month_production[1]) -
			    ((*b)->last_month_production[0] + (*b)->last_month_production[1]);
		}

		return (r == 0) ? IndustryNameSorter(a, b) : r;
	}

	/** Sort industries by transported cargo and name */
	static int CDECL IndustryTransportedCargoSorter(const Industry * const *a, const Industry * const *b)
	{
		int r = GetCargoTransportedSortValue(*a) - GetCargoTransportedSortValue(*b);
		return (r == 0) ? IndustryNameSorter(a, b) : r;
	}

	/**
	 * Get the StringID to draw and set the appropriate DParams.
	 * @param i the industry to get the StringID of.
	 * @return the StringID.
	 */
	StringID GetIndustryString(const Industry *i) const
	{
		const IndustrySpec *indsp = GetIndustrySpec(i->type);
		byte p = 0;

		/* Industry name */
		SetDParam(p++, i->index);

		char cargo_suffix[lengthof(i->produced_cargo)][512];
		GetAllCargoSuffixes(3, CST_DIR, i, i->type, indsp, i->produced_cargo, cargo_suffix);

		/* Industry productions */
		for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
			if (i->produced_cargo[j] == CT_INVALID) continue;
			SetDParam(p++, i->produced_cargo[j]);
			SetDParam(p++, i->last_month_production[j]);
			SetDParamStr(p++, cargo_suffix[j]);
		}

		/* Transported productions */
		for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
			if (i->produced_cargo[j] == CT_INVALID) continue;
			SetDParam(p++, ToPercent8(i->last_month_pct_transported[j]));
		}

		/* Drawing the right string */
		switch (p) {
			case 1:  return STR_INDUSTRY_DIRECTORY_ITEM_NOPROD;
			case 5:  return STR_INDUSTRY_DIRECTORY_ITEM;
			default: return STR_INDUSTRY_DIRECTORY_ITEM_TWO;
		}
	}

public:
	IndustryDirectoryWindow(const WindowDesc *desc, WindowNumber number) : Window()
	{
		this->industries.SetListing(this->last_sorting);
		this->industries.SetSortFuncs(IndustryDirectoryWindow::sorter_funcs);
		this->industries.ForceRebuild();
		this->BuildSortIndustriesList();

		this->InitNested(desc, 0);
		this->vscroll.SetCapacity(this->GetWidget<NWidgetBase>(IDW_INDUSTRY_LIST)->current_y / this->resize.step_height);
	}

	~IndustryDirectoryWindow()
	{
		this->last_sorting = this->industries.GetListing();
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == IDW_DROPDOWN_CRITERIA) SetDParam(0, IndustryDirectoryWindow::sorter_names[this->industries.SortType()]);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case IDW_DROPDOWN_ORDER:
				this->DrawSortButtonState(widget, this->industries.IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case IDW_INDUSTRY_LIST: {
				int n = 0;
				int y = r.top + WD_FRAMERECT_TOP;
				if (this->industries.Length() == 0) {
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_INDUSTRY_DIRECTORY_NONE);
					break;
				}
				for (uint i = this->vscroll.GetPosition(); i < this->industries.Length(); i++) {
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, this->GetIndustryString(this->industries[i]));

					y += this->resize.step_height;
					if (++n == this->vscroll.GetCapacity()) break; // max number of industries in 1 window
				}
			} break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		switch (widget) {
			case IDW_DROPDOWN_ORDER: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + WD_SORTBUTTON_ARROW_WIDTH * 2; // Doubled since the word is centered, also looks nice.
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case IDW_DROPDOWN_CRITERIA: {
				Dimension d = {0, 0};
				for (uint i = 0; IndustryDirectoryWindow::sorter_names[i] != INVALID_STRING_ID; i++) {
					d = maxdim(d, GetStringBoundingBox(IndustryDirectoryWindow::sorter_names[i]));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case IDW_INDUSTRY_LIST: {
				Dimension d = GetStringBoundingBox(STR_INDUSTRY_DIRECTORY_NONE);
				for (uint i = 0; i < this->industries.Length(); i++) {
					d = maxdim(d, GetStringBoundingBox(this->GetIndustryString(this->industries[i])));
				}
				resize->height = d.height;
				d.width += padding.width + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				d.height += padding.height + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				*size = maxdim(*size, d);
				break;
			}
		}
	}


	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case IDW_DROPDOWN_ORDER:
				this->industries.ToggleSortOrder();
				this->SetDirty();
				break;

			case IDW_DROPDOWN_CRITERIA:
				ShowDropDownMenu(this, IndustryDirectoryWindow::sorter_names, this->industries.SortType(), IDW_DROPDOWN_CRITERIA, 0, 0);
				break;

			case IDW_INDUSTRY_LIST: {
				int y = (pt.y - this->GetWidget<NWidgetBase>(widget)->pos_y - WD_FRAMERECT_TOP) / this->resize.step_height;
				uint16 p;

				if (!IsInsideMM(y, 0, this->vscroll.GetCapacity())) return;
				p = y + this->vscroll.GetPosition();
				if (p < this->industries.Length()) {
					if (_ctrl_pressed) {
						ShowExtraViewPortWindow(this->industries[p]->xy);
					} else {
						ScrollMainWindowToTile(this->industries[p]->xy);
					}
				}
			} break;
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		if (this->industries.SortType() != index) {
			this->industries.SetSortType(index);
			this->BuildSortIndustriesList();
		}
	}

	virtual void OnResize()
	{
		this->vscroll.SetCapacity(this->GetWidget<NWidgetBase>(IDW_INDUSTRY_LIST)->current_y / this->resize.step_height);
	}

	virtual void OnHundredthTick()
	{
		this->industries.ForceResort();
		this->BuildSortIndustriesList();
	}

	virtual void OnInvalidateData(int data)
	{
		if (data == 0) {
			this->industries.ForceRebuild();
		} else {
			this->industries.ForceResort();
		}
		this->BuildSortIndustriesList();
	}
};

Listing IndustryDirectoryWindow::last_sorting = {false, 0};
const Industry *IndustryDirectoryWindow::last_industry = NULL;

/* Availible station sorting functions */
GUIIndustryList::SortFunction * const IndustryDirectoryWindow::sorter_funcs[] = {
	&IndustryNameSorter,
	&IndustryTypeSorter,
	&IndustryProductionSorter,
	&IndustryTransportedCargoSorter
};

/* Names of the sorting functions */
const StringID IndustryDirectoryWindow::sorter_names[] = {
	STR_SORT_BY_NAME,
	STR_SORT_BY_TYPE,
	STR_SORT_BY_PRODUCTION,
	STR_SORT_BY_TRANSPORTED,
	INVALID_STRING_ID
};


/** Window definition of the industy directory gui */
static const WindowDesc _industry_directory_desc(
	WDP_AUTO, WDP_AUTO, 428, 190, 428, 190,
	WC_INDUSTRY_DIRECTORY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	NULL, _nested_industry_directory_widgets, lengthof(_nested_industry_directory_widgets)
);

void ShowIndustryDirectory()
{
	AllocateWindowDescFront<IndustryDirectoryWindow>(&_industry_directory_desc, 0);
}
