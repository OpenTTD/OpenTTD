/* $Id$ */

/** @file industry_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "strings.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "map.h"
#include "gui.h"
#include "window.h"
#include "gfx.h"
#include "command.h"
#include "viewport.h"
#include "industry.h"
#include "town.h"
#include "variables.h"
#include "helpers.hpp"
#include "cargotype.h"
#include "newgrf.h"
#include "newgrf_callbacks.h"
#include "newgrf_industries.h"
#include "newgrf_text.h"
#include "date.h"

extern Industry *CreateNewIndustry(TileIndex tile, IndustryType type);

/**
 * Search callback function for TryBuildIndustry
 * @param tile to test
 * @param data that is passed by the caller.  In this case, the type of industry been tested
 * @return the success (or not) of the operation
 */
static bool SearchTileForIndustry(TileIndex tile, uint32 data)
{
	return CreateNewIndustry(tile, data) != NULL;
}

/**
 * Perform a 9*9 tiles circular search around a tile
 * in order to find a suitable zone to create the desired industry
 * @param tile to start search for
 * @param type of the desired industry
 * @return the success (or not) of the operation
 */
static bool TryBuildIndustry(TileIndex tile, int type)
{
	return CircularTileSearch(tile, 9, SearchTileForIndustry, type);
}
bool _ignore_restrictions;

enum {
	DYNA_INDU_MATRIX_WIDGET = 2,
	DYNA_INDU_INFOPANEL = 4,
	DYNA_INDU_FUND_WIDGET,
	DYNA_INDU_RESIZE_WIDGET,
};

/** Attached struct to the window extended data */
struct fnd_d {
	int index;             ///< index of the element in the matrix
	IndustryType select;   ///< industry corresponding to the above index
	uint16 callback_timer; ///< timer counter for callback eventual verification
	bool timer_enabled;    ///< timer can be used
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(fnd_d));

/** Helper struct holding the available industries for current situation */
static struct IndustryData {
	uint16 count;                               ///< How many industries are loaded
	IndustryType index[NUM_INDUSTRYTYPES + 1];  ///< Type of industry, in the order it was loaded
	StringID text[NUM_INDUSTRYTYPES + 1];       ///< Text coming from CBM_IND_FUND_MORE_TEXT (if ever)
	bool enabled[NUM_INDUSTRYTYPES + 1];        ///< availability state, coming from CBID_INDUSTRY_AVAILABLE (if ever)
} _fund_gui;

static void BuildDynamicIndustryWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE: {
			IndustryType ind;
			const IndustrySpec *indsp;

			/* Shorten the window to the equivalant of the additionnal purchase
			 * info coming from the callback.  SO it will only be available to tis full
			 * height when newindistries are loaded */
			if (!_loaded_newgrf_features.has_newindustries) {
				w->widget[DYNA_INDU_INFOPANEL].bottom -= 44;
				w->widget[DYNA_INDU_FUND_WIDGET].bottom -= 44;
				w->widget[DYNA_INDU_FUND_WIDGET].top -= 44;
				w->widget[DYNA_INDU_RESIZE_WIDGET].bottom -= 44;
				w->widget[DYNA_INDU_RESIZE_WIDGET].top -= 44;
				w->resize.height = w->height -= 44;
			}

			WP(w, fnd_d).timer_enabled = _loaded_newgrf_features.has_newindustries;

			/* Initilialize structures */
			memset(&_fund_gui.index, 0xFF, NUM_INDUSTRYTYPES);
			memset(&_fund_gui.text, STR_NULL, NUM_INDUSTRYTYPES);
			memset(&_fund_gui.enabled, false, NUM_INDUSTRYTYPES);
			_fund_gui.count = 0;

			w->vscroll.cap = 8; // rows in grid, same in scroller
			w->resize.step_height = 13;

			if (_game_mode == GM_EDITOR) { // give room for the Many Random "button"
				_fund_gui.index[_fund_gui.count] = INVALID_INDUSTRYTYPE;
				_fund_gui.count++;
				WP(w, fnd_d).timer_enabled = false;
			}

			/* Fill the _fund_gui structure with industries.
			 * The tests performed after the enabled allow to load the industries
			 * In the same way they are inserted by grf (if any)
			 */
			for (ind = 0; ind < NUM_INDUSTRYTYPES; ind++) {
				indsp = GetIndustrySpec(ind);
				if (indsp->enabled){
					/* Rule is that editor mode loads all industries.
					 * In game mode, all non raw industries are loaded too
					 * and raw ones are loaded only when setting allows it */
					if (_game_mode != GM_EDITOR && indsp->IsRawIndustry() && _patches.raw_industry_construction == 0) continue;
					_fund_gui.index[_fund_gui.count] = ind;
					_fund_gui.enabled[_fund_gui.count] = (_game_mode == GM_EDITOR) || CheckIfCallBackAllowsAvailability(ind, IACT_USERCREATION);
					_fund_gui.count++;
				}
			}

			/* first indutry type is selected.
			 * I'll be damned if there are none available ;) */
			WP(w, fnd_d).index = 0;
			WP(w, fnd_d).select = _fund_gui.index[0];
			WP(w, fnd_d).callback_timer = DAY_TICKS;
		} break;

		case WE_PAINT: {
			const IndustrySpec *indsp = (WP(w, fnd_d).select == INVALID_INDUSTRYTYPE) ? NULL : GetIndustrySpec(WP(w, fnd_d).select);
			int x_str = w->widget[DYNA_INDU_INFOPANEL].left + 3;
			int y_str = w->widget[DYNA_INDU_INFOPANEL].top + 3;
			const Widget *wi = &w->widget[DYNA_INDU_INFOPANEL];
			int max_width = wi->right - wi->left - 4;

			/* Raw industries might be prospected. Show this fact by changing the string
			 * In Editor, you just build, while ingame, or you fund or you prospect */
			if (_game_mode == GM_EDITOR) {
				/* We've chosen many random industries but no industries have been specified */
				if (indsp == NULL) _fund_gui.enabled[WP(w, fnd_d).index] = _opt.diff.number_industries != 0;
				w->widget[DYNA_INDU_FUND_WIDGET].data = STR_BUILD_NEW_INDUSTRY;
			} else {
				w->widget[DYNA_INDU_FUND_WIDGET].data = (_patches.raw_industry_construction == 2 && indsp->IsRawIndustry()) ? STR_PROSPECT_NEW_INDUSTRY : STR_FUND_NEW_INDUSTRY;
			}
			SetWindowWidgetDisabledState(w, DYNA_INDU_FUND_WIDGET, !_fund_gui.enabled[WP(w, fnd_d).index]);

			SetVScrollCount(w, _fund_gui.count);

			DrawWindowWidgets(w);

			/* and now with the matrix painting */
			for (byte i = 0; i < w->vscroll.cap && ((i + w->vscroll.pos) < _fund_gui.count); i++) {
				int offset = i * 13;
				int x = 3;
				int y = 16;
				bool selected = WP(w, fnd_d).index == i + w->vscroll.pos;

				if (_fund_gui.index[i + w->vscroll.pos] == INVALID_INDUSTRYTYPE) {
					DrawStringTruncated(20, y + offset, STR_MANY_RANDOM_INDUSTRIES, selected ? TC_WHITE : TC_ORANGE, max_width - 25);
					continue;
				}
				const IndustrySpec *indsp = GetIndustrySpec(_fund_gui.index[i + w->vscroll.pos]);

				/* Draw the name of the industry in white is selected, otherwise, in orange */
				DrawStringTruncated(20, y + offset, indsp->name, selected ? TC_WHITE : TC_ORANGE, max_width - 25);
				GfxFillRect(x,     y + 1 + offset,  x + 10, y + 7 + offset, selected ? 15 : 0);
				GfxFillRect(x + 1, y + 2 + offset,  x +  9, y + 6 + offset, indsp->map_colour);
			}

			if (WP(w, fnd_d).select == INVALID_INDUSTRYTYPE) {
				DrawStringMultiLine(x_str, y_str, STR_RANDOM_INDUSTRIES_TIP, max_width, wi->bottom - wi->top - 40);
				break;
			}

			if (_game_mode != GM_EDITOR) {
				SetDParam(0, indsp->GetConstructionCost());
				DrawStringTruncated(x_str, y_str, STR_482F_COST, TC_FROMSTRING, max_width);
				y_str += 11;
			}

			/* Draw the accepted cargos, if any. Otherwhise, will print "Nothing" */
			StringID str = STR_4827_REQUIRES;
			byte p = 0;
			SetDParam(0, STR_00D0_NOTHING);
			for (byte j = 0; j < lengthof(indsp->accepts_cargo); j++) {
				if (indsp->accepts_cargo[j] == CT_INVALID) continue;
				if (p > 0) str++;
				SetDParam(p++, GetCargo(indsp->accepts_cargo[j])->name);
			}
			DrawStringTruncated(x_str, y_str, str, TC_FROMSTRING, max_width);
			y_str += 11;

			/* Draw the produced cargos, if any. Otherwhise, will print "Nothing" */
			str = STR_4827_PRODUCES;
			p = 0;
			SetDParam(0, STR_00D0_NOTHING);
			for (byte j = 0; j < lengthof(indsp->produced_cargo); j++) {
				if (indsp->produced_cargo[j] == CT_INVALID) continue;
				if (p > 0) str++;
				SetDParam(p++, GetCargo(indsp->produced_cargo[j])->name);
			}
			DrawStringTruncated(x_str, y_str, str, TC_FROMSTRING, max_width);
			y_str += 11;

			/* Get the additional purchase info text, if it has not already been */
			if (_fund_gui.text[WP(w, fnd_d).index] == STR_NULL) {   // Have i been called already?
				if (HasBit(indsp->callback_flags, CBM_IND_FUND_MORE_TEXT)) {          // No. Can it be called?
					uint16 callback_res = GetIndustryCallback(CBID_INDUSTRY_FUND_MORE_TEXT, 0, 0, NULL, WP(w, fnd_d).select, INVALID_TILE);
					if (callback_res != CALLBACK_FAILED) {  // Did it failed?
						StringID newtxt = GetGRFStringID(indsp->grf_prop.grffile->grfid, 0xD000 + callback_res);  // No. here's the new string
						_fund_gui.text[WP(w, fnd_d).index] = newtxt;   // Store it for further usage
					}
				}
			}

			/* Draw the Additional purchase text, provided by newgrf callback, if any.
			 * Otherwhise, will print Nothing */
			str = _fund_gui.text[WP(w, fnd_d).index];
			if (str != STR_NULL && str != STR_UNDEFINED) {
				SetDParam(0, str);
				DrawStringMultiLine(x_str, y_str, STR_JUST_STRING, max_width, wi->bottom - wi->top - 40);
			}
		} break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case DYNA_INDU_MATRIX_WIDGET: {
					const IndustrySpec *indsp;
					int y = (e->we.click.pt.y - w->widget[DYNA_INDU_MATRIX_WIDGET].top) / 13 + w->vscroll.pos ;

					if (y >= 0 && y < _fund_gui.count) { // Is it within the boundaries of available data?
						WP(w, fnd_d).index = y;
						WP(w, fnd_d).select = _fund_gui.index[WP(w, fnd_d).index];
						indsp = (WP(w, fnd_d).select == INVALID_INDUSTRYTYPE) ? NULL : GetIndustrySpec(WP(w, fnd_d).select);

						SetWindowDirty(w);

						if ((_game_mode != GM_EDITOR && _patches.raw_industry_construction == 2 && indsp != NULL && indsp->IsRawIndustry()) ||
								WP(w, fnd_d).select == INVALID_INDUSTRYTYPE) {
							/* Reset the button state if going to prospecting or "build many industries" */
							RaiseWindowButtons(w);
							ResetObjectToPlace();
						}
					}
				} break;

				case DYNA_INDU_FUND_WIDGET: {
					if (WP(w, fnd_d).select == INVALID_INDUSTRYTYPE) {
						HandleButtonClick(w, DYNA_INDU_FUND_WIDGET);

						if (GetNumTowns() == 0) {
							ShowErrorMessage(STR_0286_MUST_BUILD_TOWN_FIRST, STR_CAN_T_GENERATE_INDUSTRIES, 0, 0);
						} else {
							extern void GenerateIndustries();
							_generating_world = true;
							GenerateIndustries();
							_generating_world = false;
						}
					} else if (_game_mode != GM_EDITOR && _patches.raw_industry_construction == 2 && GetIndustrySpec(WP(w, fnd_d).select)->IsRawIndustry()) {
						DoCommandP(0, WP(w, fnd_d).select, 0, NULL, CMD_BUILD_INDUSTRY | CMD_MSG(STR_4830_CAN_T_CONSTRUCT_THIS_INDUSTRY));
						HandleButtonClick(w, DYNA_INDU_FUND_WIDGET);
					} else {
						HandlePlacePushButton(w, DYNA_INDU_FUND_WIDGET, SPR_CURSOR_INDUSTRY, VHM_RECT, NULL);
					}
				} break;
			}
			break;

		case WE_RESIZE: {
			/* Adjust the number of items in the matrix depending of the rezise */
			w->vscroll.cap  += e->we.sizing.diff.y / (int)w->resize.step_height;
			w->widget[DYNA_INDU_MATRIX_WIDGET].data = (w->vscroll.cap << 8) + 1;
		} break;

		case WE_PLACE_OBJ: {
			bool success = true;
			/* We do not need to protect ourselves against "Random Many Industries" in this mode */
			const IndustrySpec *indsp = GetIndustrySpec(WP(w, fnd_d).select);

			if (_game_mode == GM_EDITOR) {
				/* Show error if no town exists at all */
				if (GetNumTowns() == 0) {
					SetDParam(0, indsp->name);
					ShowErrorMessage(STR_0286_MUST_BUILD_TOWN_FIRST, STR_0285_CAN_T_BUILD_HERE, e->we.place.pt.x, e->we.place.pt.y);
					return;
				}

				_current_player = OWNER_NONE;
				_generating_world = true;
				_ignore_restrictions = true;
				success = TryBuildIndustry(e->we.place.tile, WP(w, fnd_d).select);
				if (!success) {
					SetDParam(0, indsp->name);
					ShowErrorMessage(_error_message, STR_0285_CAN_T_BUILD_HERE, e->we.place.pt.x, e->we.place.pt.y);
				}

				_ignore_restrictions = false;
				_generating_world = false;
			} else {
				success = DoCommandP(e->we.place.tile, WP(w, fnd_d).select, InteractiveRandomRange(indsp->num_table), NULL, CMD_BUILD_INDUSTRY | CMD_MSG(STR_4830_CAN_T_CONSTRUCT_THIS_INDUSTRY));
			}

			/* If an industry has been built, just reset the cursor and the system */
			if (success) ResetObjectToPlace();
		} break;

		case WE_TICK:
			if (!WP(w, fnd_d).timer_enabled) break;
			if (--WP(w, fnd_d).callback_timer == 0) {
				/* We have just passed another day.
				 * See if we need to update availability of currently selected industry */
				WP(w, fnd_d).callback_timer = DAY_TICKS;  //restart counter

				const IndustrySpec *indsp = GetIndustrySpec(WP(w, fnd_d).select);

				if (indsp->enabled) {
					bool call_back_result = CheckIfCallBackAllowsAvailability(WP(w, fnd_d).select, IACT_USERCREATION);

					/* Only if result does match the previous state would it require a redraw. */
					if (call_back_result != _fund_gui.enabled[WP(w, fnd_d).index]) {
						_fund_gui.enabled[WP(w, fnd_d).index] = call_back_result;
						SetWindowDirty(w);
					}
				}
			}
			break;

		case WE_TIMEOUT:
		case WE_ABORT_PLACE_OBJ:
			RaiseWindowButtons(w);
			break;
	}
}

static const Widget _build_dynamic_industry_widgets[] = {
{   WWT_CLOSEBOX,    RESIZE_NONE,    7,     0,    10,     0,    13, STR_00C5,                       STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_RIGHT,    7,    11,   169,     0,    13, STR_0314_FUND_NEW_INDUSTRY,     STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX,      RESIZE_RB,    7,     0,   157,    14,   118, 0x801,                          STR_INDUSTRY_SELECTION_HINT},
{  WWT_SCROLLBAR,     RESIZE_LRB,    7,   158,   169,    14,   118, 0x0,                            STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_RTB,    7,     0,   169,   119,   199, 0x0,                            STR_NULL},
{    WWT_TEXTBTN,     RESIZE_RTB,    7,     0,   157,   200,   211, STR_FUND_NEW_INDUSTRY,          STR_NULL},
{  WWT_RESIZEBOX,    RESIZE_LRTB,    7,   158,   169,   200,   211, 0x0,                            STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _build_industry_dynamic_desc = {
	WDP_AUTO, WDP_AUTO, 170, 212, 170, 212,
	WC_BUILD_INDUSTRY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_RESIZABLE,
	_build_dynamic_industry_widgets,
	BuildDynamicIndustryWndProc,
};

void ShowBuildIndustryWindow()
{
	if (_game_mode != GM_EDITOR && !IsValidPlayer(_current_player)) return;
	AllocateWindowDescFront(&_build_industry_dynamic_desc, 0);
}

static void UpdateIndustryProduction(Industry *i);

static inline bool isProductionMinimum(const Industry *i, int pt)
{
	return i->production_rate[pt] == 0;
}

static inline bool isProductionMaximum(const Industry *i, int pt)
{
	return i->production_rate[pt] >= 255;
}

static inline bool IsProductionAlterable(const Industry *i)
{
	return ((_game_mode == GM_EDITOR || _cheats.setup_prod.value) &&
			(i->accepts_cargo[0] == CT_INVALID || i->accepts_cargo[0] == CT_VALUABLES));
}

/** Information to store about the industry window */
struct indview_d : public vp_d {
	byte editbox_line;        ///< The line clicked to open the edit box
	byte clicked_line;        ///< The line of the button that has been clicked
	byte clicked_button;      ///< The button that has been clicked (to raise)
	byte production_offset_y; ///< The offset of the production texts/buttons
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(indview_d));


static void IndustryViewWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE: {
		/* Count the number of lines that we need to resize the GUI with */
		const Industry *i = GetIndustry(w->window_number);
		const IndustrySpec *ind = GetIndustrySpec(i->type);
		int lines = -3;
		bool first = true;
		bool has_accept = false;

		if (HasBit(ind->callback_flags, CBM_IND_PRODUCTION_CARGO_ARRIVAL) || HasBit(ind->callback_flags, CBM_IND_PRODUCTION_256_TICKS)) {
			for (byte j = 0; j < lengthof(i->accepts_cargo); j++) {
				if (i->accepts_cargo[j] == CT_INVALID) continue;
				has_accept = true;
				if (first) {
					lines++;
					first = false;
				}
				lines++;
			}
		} else {
			for (byte j = 0; j < lengthof(i->accepts_cargo); j++) {
				if (i->accepts_cargo[j] == CT_INVALID) continue;
				has_accept = true;
				lines++;
				break;
			}
		}

		first = true;
		for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
			if (i->produced_cargo[j] == CT_INVALID) continue;
			if (first) {
				if (has_accept) lines++;
				lines++;
				first = false;
			}
			lines++;
		}

		if (HasBit(ind->callback_flags, CBM_IND_WINDOW_MORE_TEXT)) lines += 2;

		for (byte j = 5; j <= 7; j++) {
			if (j != 5) w->widget[j].top += lines * 10;
			w->widget[j].bottom += lines * 10;
		}
		w->height += lines * 10;
	} break;

	case WE_PAINT: {
		Industry *i = GetIndustry(w->window_number);
		const IndustrySpec *ind = GetIndustrySpec(i->type);
		int y = 111;
		bool first = true;
		bool has_accept = false;

		SetDParam(0, w->window_number);
		DrawWindowWidgets(w);

		if (HasBit(ind->callback_flags, CBM_IND_PRODUCTION_CARGO_ARRIVAL) || HasBit(ind->callback_flags, CBM_IND_PRODUCTION_256_TICKS)) {
			for (byte j = 0; j < lengthof(i->accepts_cargo); j++) {
				if (i->accepts_cargo[j] == CT_INVALID) continue;
				has_accept = true;
				if (first) {
					DrawString(2, y, STR_INDUSTRY_WINDOW_WAITING_FOR_PROCESSING, TC_FROMSTRING);
					y += 10;
					first = false;
				}
				SetDParam(0, i->accepts_cargo[j]);
				SetDParam(1, i->incoming_cargo_waiting[j]);
				DrawString(4, y, STR_INDUSTRY_WINDOW_WAITING_STOCKPILE_CARGO, TC_FROMSTRING);
				y += 10;
			}
		} else {
			StringID str = STR_4827_REQUIRES;
			byte p = 0;
			for (byte j = 0; j < lengthof(i->accepts_cargo); j++) {
				if (i->accepts_cargo[j] == CT_INVALID) continue;
				has_accept = true;
				if (p > 0) str++;
				SetDParam(p++, GetCargo(i->accepts_cargo[j])->name);
			}
			if (has_accept) {
				DrawString(2, y, str, TC_FROMSTRING);
				y += 10;
			}
		}

		first = true;
		for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
			if (i->produced_cargo[j] == CT_INVALID) continue;
			if (first) {
				if (has_accept) y += 10;
				DrawString(2, y, STR_482A_PRODUCTION_LAST_MONTH, TC_FROMSTRING);
				y += 10;
				WP(w, indview_d).production_offset_y = y;
				first = false;
			}

			SetDParam(0, i->produced_cargo[j]);
			SetDParam(1, i->last_month_production[j]);

			SetDParam(2, i->last_month_pct_transported[j] * 100 >> 8);
			DrawString(4 + (IsProductionAlterable(i) ? 30 : 0), y, STR_482B_TRANSPORTED, TC_FROMSTRING);
			/* Let's put out those buttons.. */
			if (IsProductionAlterable(i)) {
				DrawArrowButtons(5, y, 3, (WP(w, indview_d).clicked_line == j + 1) ? WP(w, indview_d).clicked_button : 0,
						!isProductionMinimum(i, j), !isProductionMaximum(i, j));
			}
			y += 10;
		}

		/* Get the extra message for the GUI */
		if (HasBit(ind->callback_flags, CBM_IND_WINDOW_MORE_TEXT)) {
			uint16 callback_res = GetIndustryCallback(CBID_INDUSTRY_WINDOW_MORE_TEXT, 0, 0, i, i->type, i->xy);
			if (callback_res != CALLBACK_FAILED) {
				StringID message = GetGRFStringID(ind->grf_prop.grffile->grfid, 0xD000 + callback_res);
				if (message != STR_NULL && message != STR_UNDEFINED) {
					y += 10;

					PrepareTextRefStackUsage(6);
					DrawString(2, y, message, TC_FROMSTRING);
					StopTextRefStackUsage();
				}
			}
		}

		DrawWindowViewport(w);
	} break;

	case WE_CLICK: {
		Industry *i;

		switch (e->we.click.widget) {
		case 5: {
			int line, x;

			i = GetIndustry(w->window_number);

			/* We should work if needed.. */
			if (!IsProductionAlterable(i)) return;
			x = e->we.click.pt.x;
			line = (e->we.click.pt.y - WP(w, indview_d).production_offset_y) / 10;
			if (e->we.click.pt.y >= WP(w, indview_d).production_offset_y && IsInsideMM(line, 0, 2) && i->produced_cargo[line] != CT_INVALID) {
				if (IsInsideMM(x, 5, 25) ) {
					/* Clicked buttons, decrease or increase production */
					if (x < 15) {
						if (isProductionMinimum(i, line)) return;
						i->production_rate[line] = max(i->production_rate[line] / 2, 0);
					} else {
						/* a zero production industry is unlikely to give anything but zero, so push it a little bit */
						int new_prod = i->production_rate[line] == 0 ? 1 : i->production_rate[line] * 2;
						if (isProductionMaximum(i, line)) return;
						i->production_rate[line] = minu(new_prod, 255);
					}

					UpdateIndustryProduction(i);
					SetWindowDirty(w);
					w->flags4 |= 5 << WF_TIMEOUT_SHL;
					WP(w, indview_d).clicked_line = line + 1;
					WP(w, indview_d).clicked_button = (x < 15 ? 1 : 2);
				} else if (IsInsideMM(x, 34, 160)) {
					/* clicked the text */
					WP(w, indview_d).editbox_line = line;
					SetDParam(0, i->production_rate[line] * 8);
					ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_CONFIG_GAME_PRODUCTION, 10, 100, w, CS_ALPHANUMERAL);
				}
			}
		} break;
		case 6:
			i = GetIndustry(w->window_number);
			ScrollMainWindowToTile(i->xy + TileDiffXY(1, 1));
		} break;

		}
		break;
	case WE_TIMEOUT:
		WP(w, indview_d).clicked_line = 0;
		WP(w, indview_d).clicked_button = 0;
		SetWindowDirty(w);
		break;

	case WE_ON_EDIT_TEXT:
		if (e->we.edittext.str[0] != '\0') {
			Industry* i = GetIndustry(w->window_number);
			int line = WP(w, indview_d).editbox_line;

			i->production_rate[line] = ClampU(atoi(e->we.edittext.str), 0, 255);
			UpdateIndustryProduction(i);
			SetWindowDirty(w);
		}
	}
}

static void UpdateIndustryProduction(Industry *i)
{
	for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
		if (i->produced_cargo[j] != CT_INVALID) {
			i->last_month_production[j] = 8 * i->production_rate[j];
		}
	}
}

static const Widget _industry_view_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     9,     0,    10,     0,    13, STR_00C5,          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     9,    11,   247,     0,    13, STR_4801,          STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,     9,   248,   259,     0,    13, 0x0,               STR_STICKY_BUTTON},
{      WWT_PANEL,   RESIZE_NONE,     9,     0,   259,    14,   105, 0x0,               STR_NULL},
{      WWT_INSET,   RESIZE_NONE,     9,     2,   257,    16,   103, 0x0,               STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,     9,     0,   259,   106,   147, 0x0,               STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     9,     0,   129,   148,   159, STR_00E4_LOCATION, STR_482C_CENTER_THE_MAIN_VIEW_ON},
{      WWT_PANEL,   RESIZE_NONE,     9,   130,   259,   148,   159, 0x0,               STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _industry_view_desc = {
	WDP_AUTO, WDP_AUTO, 260, 160, 260, 160,
	WC_INDUSTRY_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_industry_view_widgets,
	IndustryViewWndProc
};

void ShowIndustryViewWindow(int industry)
{
	Window *w = AllocateWindowDescFront(&_industry_view_desc, industry);

	if (w != NULL) {
		w->flags4 |= WF_DISABLE_VP_SCROLL;
		WP(w, indview_d).editbox_line = 0;
		WP(w, indview_d).clicked_line = 0;
		WP(w, indview_d).clicked_button = 0;
		AssignWindowViewport(w, 3, 17, 0xFE, 0x56, GetIndustry(w->window_number)->xy + TileDiffXY(1, 1), ZOOM_LVL_INDUSTRY);
	}
}

enum {
	DIRECTORY_INDU_SORTBYNAME = 3,
	DIRECTORY_INDU_SORTBYTYPE,
	DIRECTORY_INDU_SORTBYPROD,
	DIRECTORY_INDU_SORTBYTRANSPORT,
	DIRECTORY_INDU_SHOWINDU = 8,
};

static const Widget _industry_directory_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    13,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    13,    11,   495,     0,    13, STR_INDUSTRYDIR_CAPTION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    13,   496,   507,     0,    13, 0x0,                     STR_STICKY_BUTTON},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,     0,   100,    14,    25, STR_SORT_BY_NAME,        STR_SORT_ORDER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,   101,   200,    14,    25, STR_SORT_BY_TYPE,        STR_SORT_ORDER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,   201,   300,    14,    25, STR_SORT_BY_PRODUCTION,  STR_SORT_ORDER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,   301,   400,    14,    25, STR_SORT_BY_TRANSPORTED, STR_SORT_ORDER_TIP},
{      WWT_PANEL,   RESIZE_NONE,    13,   401,   495,    14,    25, 0x0,                     STR_NULL},
{      WWT_PANEL, RESIZE_BOTTOM,    13,     0,   495,    26,   189, 0x0,                     STR_200A_TOWN_NAMES_CLICK_ON_NAME},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    13,   496,   507,    14,   177, 0x0,                     STR_0190_SCROLL_BAR_SCROLLS_LIST},
{  WWT_RESIZEBOX,     RESIZE_TB,    13,   496,   507,   178,   189, 0x0,                     STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static uint _num_industry_sort;

static char _bufcache[96];
static const Industry* _last_industry;

static byte _industry_sort_order;

static int CDECL GeneralIndustrySorter(const void *a, const void *b)
{
	const Industry* i = *(const Industry**)a;
	const Industry* j = *(const Industry**)b;
	int r;

	switch (_industry_sort_order >> 1) {
		default: NOT_REACHED();
		case 0: /* Sort by Name (handled later) */
			r = 0;
			break;

		case 1: /* Sort by Type */
			r = i->type - j->type;
			break;

		case 2: /* Sort by Production */
			if (i->produced_cargo[0] == CT_INVALID) {
				r = (j->produced_cargo[0] == CT_INVALID ? 0 : -1);
			} else {
				if (j->produced_cargo[0] == CT_INVALID) {
					r = 1;
				} else {
					r =
						(i->last_month_production[0] + i->last_month_production[1]) -
						(j->last_month_production[0] + j->last_month_production[1]);
				}
			}
			break;

		case 3: /* Sort by transported fraction */
			if (i->produced_cargo[0] == CT_INVALID) {
				r = (j->produced_cargo[0] == CT_INVALID ? 0 : -1);
			} else {
				if (j->produced_cargo[0] == CT_INVALID) {
					r = 1;
				} else {
					int pi;
					int pj;

					pi = i->last_month_pct_transported[0] * 100 >> 8;
					if (i->produced_cargo[1] != CT_INVALID) {
						int p = i->last_month_pct_transported[1] * 100 >> 8;
						if (p < pi) pi = p;
					}

					pj = j->last_month_pct_transported[0] * 100 >> 8;
					if (j->produced_cargo[1] != CT_INVALID) {
						int p = j->last_month_pct_transported[1] * 100 >> 8;
						if (p < pj) pj = p;
					}

					r = pi - pj;
				}
			}
			break;
	}

	/* default to string sorting if they are otherwise equal */
	if (r == 0) {
		char buf1[96];

		SetDParam(0, i->town->index);
		GetString(buf1, STR_TOWN, lastof(buf1));

		if (j != _last_industry) {
			_last_industry = j;
			SetDParam(0, j->town->index);
			GetString(_bufcache, STR_TOWN, lastof(_bufcache));
		}
		r = strcmp(buf1, _bufcache);
	}

	if (_industry_sort_order & 1) r = -r;
	return r;
}

/**
 * Makes a sorted industry list.
 * When there are no industries, the list has to be made. This so when one
 * starts a new game without industries after playing a game with industries
 * the list is not populated with invalid industries from the previous game.
 */
static void MakeSortedIndustryList()
{
	const Industry* i;
	int n = 0;

	/* Create array for sorting */
	_industry_sort = ReallocT(_industry_sort, GetMaxIndustryIndex() + 1);
	if (_industry_sort == NULL) error("Could not allocate memory for the industry-sorting-list");

	/* Don't attempt a sort if there are no industries */
	if (GetNumIndustries() != 0) {
		FOR_ALL_INDUSTRIES(i) _industry_sort[n++] = i;
		qsort((void*)_industry_sort, n, sizeof(_industry_sort[0]), GeneralIndustrySorter);
	}

	_num_industry_sort = n;
	_last_industry = NULL; // used for "cache"

	DEBUG(misc, 3, "Resorting industries list");
}


static void IndustryDirectoryWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		int n;
		uint p;
		static const uint16 _indicator_positions[4] = {88, 187, 284, 387};

		if (_industry_sort_dirty) {
			_industry_sort_dirty = false;
			MakeSortedIndustryList();
		}

		SetVScrollCount(w, _num_industry_sort);

		DrawWindowWidgets(w);
		DoDrawString(_industry_sort_order & 1 ? DOWNARROW : UPARROW, _indicator_positions[_industry_sort_order >> 1], 15, TC_BLACK);

		p = w->vscroll.pos;
		n = 0;

		while (p < _num_industry_sort) {
			const Industry* i = _industry_sort[p];

			SetDParam(0, i->index);
			if (i->produced_cargo[0] != CT_INVALID) {
				SetDParam(1, i->produced_cargo[0]);
				SetDParam(2, i->last_month_production[0]);

				if (i->produced_cargo[1] != CT_INVALID) {
					SetDParam(3, i->produced_cargo[1]);
					SetDParam(4, i->last_month_production[1]);
					SetDParam(5, i->last_month_pct_transported[0] * 100 >> 8);
					SetDParam(6, i->last_month_pct_transported[1] * 100 >> 8);
					DrawString(4, 28 + n * 10, STR_INDUSTRYDIR_ITEM_TWO, TC_FROMSTRING);
				} else {
					SetDParam(3, i->last_month_pct_transported[0] * 100 >> 8);
					DrawString(4, 28 + n * 10, STR_INDUSTRYDIR_ITEM, TC_FROMSTRING);
				}
			} else {
				DrawString(4, 28 + n * 10, STR_INDUSTRYDIR_ITEM_NOPROD, TC_FROMSTRING);
			}
			p++;
			if (++n == w->vscroll.cap) break;
		}
	} break;

	case WE_CLICK:
		switch (e->we.click.widget) {
			case DIRECTORY_INDU_SORTBYNAME: {
				_industry_sort_order = _industry_sort_order == 0 ? 1 : 0;
				_industry_sort_dirty = true;
				SetWindowDirty(w);
			} break;

			case DIRECTORY_INDU_SORTBYTYPE: {
				_industry_sort_order = _industry_sort_order == 2 ? 3 : 2;
				_industry_sort_dirty = true;
				SetWindowDirty(w);
			} break;

			case DIRECTORY_INDU_SORTBYPROD: {
				_industry_sort_order = _industry_sort_order == 4 ? 5 : 4;
				_industry_sort_dirty = true;
				SetWindowDirty(w);
			} break;

			case DIRECTORY_INDU_SORTBYTRANSPORT: {
				_industry_sort_order = _industry_sort_order == 6 ? 7 : 6;
				_industry_sort_dirty = true;
				SetWindowDirty(w);
			} break;

			case DIRECTORY_INDU_SHOWINDU: {
				int y = (e->we.click.pt.y - 28) / 10;
				uint16 p;

				if (!IsInsideMM(y, 0, w->vscroll.cap)) return;
				p = y + w->vscroll.pos;
				if (p < _num_industry_sort) {
					ScrollMainWindowToTile(_industry_sort[p]->xy);
				}
			} break;
		}
		break;

	case WE_4:
		SetWindowDirty(w);
		break;

	case WE_RESIZE:
		w->vscroll.cap += e->we.sizing.diff.y / 10;
		break;
	}
}


/* Industry List */
static const WindowDesc _industry_directory_desc = {
	WDP_AUTO, WDP_AUTO, 508, 190, 508, 190,
	WC_INDUSTRY_DIRECTORY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_industry_directory_widgets,
	IndustryDirectoryWndProc
};


void ShowIndustryDirectory()
{
	Window *w = AllocateWindowDescFront(&_industry_directory_desc, 0);

	if (w != NULL) {
		w->vscroll.cap = 16;
		w->resize.height = w->height - 6 * 10; // minimum 10 items
		w->resize.step_height = 10;
		SetWindowDirty(w);
	}
}
