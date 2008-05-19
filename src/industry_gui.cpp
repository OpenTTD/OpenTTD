/* $Id$ */

/** @file industry_gui.cpp GUIs related to industries. */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "industry.h"
#include "town.h"
#include "variables.h"
#include "cheat_func.h"
#include "cargotype.h"
#include "newgrf.h"
#include "newgrf_callbacks.h"
#include "newgrf_industries.h"
#include "newgrf_text.h"
#include "strings_func.h"
#include "map_func.h"
#include "player_func.h"
#include "settings_type.h"
#include "tilehighlight_func.h"
#include "string_func.h"
#include "sortlist_type.h"

#include "table/strings.h"
#include "table/sprites.h"

bool _ignore_restrictions;

enum CargoSuffixType {
	CST_FUND,
	CST_VIEW,
	CST_DIR,
};

/**
 * Gets the string to display after the cargo name (using callback 37)
 * @param cargo the cargo for which the suffix is requested
 * - 00 - first accepted cargo type
 * - 01 - second accepted cargo type
 * - 02 - third accepted cargo type
 * - 03 - first produced cargo type
 * - 04 - second produced cargo type
 * @param cst the cargo suffix type (for which window is it requested)
 * @param ind the industry (NULL if in fund window)
 * @param ind_type the industry type
 * @param indspec the industry spec
 * @return the string to display
 */
static StringID GetCargoSuffix(uint cargo, CargoSuffixType cst, Industry *ind, IndustryType ind_type, const IndustrySpec *indspec)
{
	if (HasBit(indspec->callback_flags, CBM_IND_CARGO_SUFFIX)) {
		uint16 callback = GetIndustryCallback(CBID_INDUSTRY_CARGO_SUFFIX, 0, (cst << 8) | cargo, ind, ind_type, (cst != CST_FUND) ? ind->xy : INVALID_TILE);
		if (GB(callback, 0, 8) != 0xFF) return GetGRFStringID(indspec->grf_prop.grffile->grfid, 0xD000 + callback);
	}
	return STR_EMPTY;
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

/** Widget definition of the dynamic place industries gui */
static const Widget _build_industry_widgets[] = {
{   WWT_CLOSEBOX,    RESIZE_NONE,    7,     0,    10,     0,    13, STR_00C5,                       STR_018B_CLOSE_WINDOW},            // DPIW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_RIGHT,    7,    11,   169,     0,    13, STR_0314_FUND_NEW_INDUSTRY,     STR_018C_WINDOW_TITLE_DRAG_THIS},  // DPIW_CAPTION
{     WWT_MATRIX,      RESIZE_RB,    7,     0,   157,    14,   118, 0x801,                          STR_INDUSTRY_SELECTION_HINT},      // DPIW_MATRIX_WIDGET
{  WWT_SCROLLBAR,     RESIZE_LRB,    7,   158,   169,    14,   118, 0x0,                            STR_0190_SCROLL_BAR_SCROLLS_LIST}, // DPIW_SCROLLBAR
{      WWT_PANEL,     RESIZE_RTB,    7,     0,   169,   119,   199, 0x0,                            STR_NULL},                         // DPIW_INFOPANEL
{    WWT_TEXTBTN,     RESIZE_RTB,    7,     0,   157,   200,   211, STR_FUND_NEW_INDUSTRY,          STR_NULL},                         // DPIW_FUND_WIDGET
{  WWT_RESIZEBOX,    RESIZE_LRTB,    7,   158,   169,   200,   211, 0x0,                            STR_RESIZE_BUTTON},                // DPIW_RESIZE_WIDGET
{   WIDGETS_END},
};

/** Window definition of the dynamic place industries gui */
static const WindowDesc _build_industry_desc = {
	WDP_AUTO, WDP_AUTO, 170, 212, 170, 212,
	WC_BUILD_INDUSTRY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_RESIZABLE,
	_build_industry_widgets,
};

class BuildIndustryWindow : public Window {
	int selected_index;                         ///< index of the element in the matrix
	IndustryType selected_type;                 ///< industry corresponding to the above index
	uint16 callback_timer;                      ///< timer counter for callback eventual verification
	bool timer_enabled;                         ///< timer can be used
	uint16 count;                               ///< How many industries are loaded
	IndustryType index[NUM_INDUSTRYTYPES + 1];  ///< Type of industry, in the order it was loaded
	StringID text[NUM_INDUSTRYTYPES + 1];       ///< Text coming from CBM_IND_FUND_MORE_TEXT (if ever)
	bool enabled[NUM_INDUSTRYTYPES + 1];        ///< availability state, coming from CBID_INDUSTRY_AVAILABLE (if ever)

	void SetupArrays()
	{
		IndustryType ind;
		const IndustrySpec *indsp;

		this->count = 0;

		for (uint i = 0; i < lengthof(this->index); i++) {
			this->index[i]   = INVALID_INDUSTRYTYPE;
			this->text[i]    = STR_NULL;
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
		for (ind = 0; ind < NUM_INDUSTRYTYPES; ind++) {
			indsp = GetIndustrySpec(ind);
			if (indsp->enabled){
				/* Rule is that editor mode loads all industries.
				 * In game mode, all non raw industries are loaded too
				 * and raw ones are loaded only when setting allows it */
				if (_game_mode != GM_EDITOR && indsp->IsRawIndustry() && _patches.raw_industry_construction == 0) {
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
	}

public:
	BuildIndustryWindow() : Window(&_build_industry_desc)
	{
		/* Shorten the window to the equivalant of the additionnal purchase
		 * info coming from the callback.  SO it will only be available to its full
		 * height when newindistries are loaded */
		if (!_loaded_newgrf_features.has_newindustries) {
			this->widget[DPIW_INFOPANEL].bottom -= 44;
			this->widget[DPIW_FUND_WIDGET].bottom -= 44;
			this->widget[DPIW_FUND_WIDGET].top -= 44;
			this->widget[DPIW_RESIZE_WIDGET].bottom -= 44;
			this->widget[DPIW_RESIZE_WIDGET].top -= 44;
			this->resize.height = this->height -= 44;
		}

		this->timer_enabled = _loaded_newgrf_features.has_newindustries;

		this->vscroll.cap = 8; // rows in grid, same in scroller
		this->resize.step_height = 13;

		this->selected_index = -1;
		this->selected_type = INVALID_INDUSTRYTYPE;

		/* Initialize arrays */
		this->SetupArrays();

		this->callback_timer = DAY_TICKS;

		this->FindWindowPlacementAndResize(&_build_industry_desc);
	}

	virtual void OnPaint()
	{
		const IndustrySpec *indsp = (this->selected_type == INVALID_INDUSTRYTYPE) ? NULL : GetIndustrySpec(this->selected_type);
		int x_str = this->widget[DPIW_INFOPANEL].left + 3;
		int y_str = this->widget[DPIW_INFOPANEL].top + 3;
		const Widget *wi = &this->widget[DPIW_INFOPANEL];
		int max_width = wi->right - wi->left - 4;

		/* Raw industries might be prospected. Show this fact by changing the string
		 * In Editor, you just build, while ingame, or you fund or you prospect */
		if (_game_mode == GM_EDITOR) {
			/* We've chosen many random industries but no industries have been specified */
			if (indsp == NULL) this->enabled[this->selected_index] = _opt.diff.number_industries != 0;
			this->widget[DPIW_FUND_WIDGET].data = STR_BUILD_NEW_INDUSTRY;
		} else {
			this->widget[DPIW_FUND_WIDGET].data = (_patches.raw_industry_construction == 2 && indsp->IsRawIndustry()) ? STR_PROSPECT_NEW_INDUSTRY : STR_FUND_NEW_INDUSTRY;
		}
		this->SetWidgetDisabledState(DPIW_FUND_WIDGET, !this->enabled[this->selected_index]);

		SetVScrollCount(this, this->count);

		this->DrawWidgets();

		/* and now with the matrix painting */
		for (byte i = 0; i < this->vscroll.cap && ((i + this->vscroll.pos) < this->count); i++) {
			int offset = i * 13;
			int x = 3;
			int y = 16;
			bool selected = this->selected_index == i + this->vscroll.pos;

			if (this->index[i + this->vscroll.pos] == INVALID_INDUSTRYTYPE) {
				DrawStringTruncated(20, y + offset, STR_MANY_RANDOM_INDUSTRIES, selected ? TC_WHITE : TC_ORANGE, max_width - 25);
				continue;
			}
			const IndustrySpec *indsp = GetIndustrySpec(this->index[i + this->vscroll.pos]);

			/* Draw the name of the industry in white is selected, otherwise, in orange */
			DrawStringTruncated(20, y + offset, indsp->name, selected ? TC_WHITE : TC_ORANGE, max_width - 25);
			GfxFillRect(x,     y + 1 + offset,  x + 10, y + 7 + offset, selected ? 15 : 0);
			GfxFillRect(x + 1, y + 2 + offset,  x +  9, y + 6 + offset, indsp->map_colour);
		}

		if (this->selected_type == INVALID_INDUSTRYTYPE) {
			DrawStringMultiLine(x_str, y_str, STR_RANDOM_INDUSTRIES_TIP, max_width, wi->bottom - wi->top - 40);
			return;
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
		SetDParam(1, STR_EMPTY);
		for (byte j = 0; j < lengthof(indsp->accepts_cargo); j++) {
			if (indsp->accepts_cargo[j] == CT_INVALID) continue;
			if (p > 0) str++;
			SetDParam(p++, GetCargo(indsp->accepts_cargo[j])->name);
			SetDParam(p++, GetCargoSuffix(j, CST_FUND, NULL, this->selected_type, indsp));
		}
		DrawStringTruncated(x_str, y_str, str, TC_FROMSTRING, max_width);
		y_str += 11;

		/* Draw the produced cargos, if any. Otherwhise, will print "Nothing" */
		str = STR_4827_PRODUCES;
		p = 0;
		SetDParam(0, STR_00D0_NOTHING);
		SetDParam(1, STR_EMPTY);
		for (byte j = 0; j < lengthof(indsp->produced_cargo); j++) {
			if (indsp->produced_cargo[j] == CT_INVALID) continue;
			if (p > 0) str++;
			SetDParam(p++, GetCargo(indsp->produced_cargo[j])->name);
			SetDParam(p++, GetCargoSuffix(j + 3, CST_FUND, NULL, this->selected_type, indsp));
		}
		DrawStringTruncated(x_str, y_str, str, TC_FROMSTRING, max_width);
		y_str += 11;

		/* Get the additional purchase info text, if it has not already been */
		if (this->text[this->selected_index] == STR_NULL) {   // Have i been called already?
			if (HasBit(indsp->callback_flags, CBM_IND_FUND_MORE_TEXT)) {          // No. Can it be called?
				uint16 callback_res = GetIndustryCallback(CBID_INDUSTRY_FUND_MORE_TEXT, 0, 0, NULL, this->selected_type, INVALID_TILE);
				if (callback_res != CALLBACK_FAILED) {  // Did it failed?
					StringID newtxt = GetGRFStringID(indsp->grf_prop.grffile->grfid, 0xD000 + callback_res);  // No. here's the new string
					this->text[this->selected_index] = newtxt;   // Store it for further usage
				}
			}
		}

		/* Draw the Additional purchase text, provided by newgrf callback, if any.
		 * Otherwhise, will print Nothing */
		str = this->text[this->selected_index];
		if (str != STR_NULL && str != STR_UNDEFINED) {
			SetDParam(0, str);
			DrawStringMultiLine(x_str, y_str, STR_JUST_STRING, max_width, wi->bottom - wi->top - 40);
		}
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
				int y = (pt.y - this->widget[DPIW_MATRIX_WIDGET].top) / 13 + this->vscroll.pos ;

				if (y >= 0 && y < count) { // Is it within the boundaries of available data?
					this->selected_index = y;
					this->selected_type = this->index[y];
					indsp = (this->selected_type == INVALID_INDUSTRYTYPE) ? NULL : GetIndustrySpec(this->selected_type);

					this->SetDirty();

					if ((_game_mode != GM_EDITOR && _patches.raw_industry_construction == 2 && indsp != NULL && indsp->IsRawIndustry()) ||
							this->selected_type == INVALID_INDUSTRYTYPE) {
						/* Reset the button state if going to prospecting or "build many industries" */
						this->RaiseButtons();
						ResetObjectToPlace();
					}
				}
			} break;

			case DPIW_FUND_WIDGET: {
				if (this->selected_type == INVALID_INDUSTRYTYPE) {
					this->HandleButtonClick(DPIW_FUND_WIDGET);

					if (GetNumTowns() == 0) {
						ShowErrorMessage(STR_0286_MUST_BUILD_TOWN_FIRST, STR_CAN_T_GENERATE_INDUSTRIES, 0, 0);
					} else {
						extern void GenerateIndustries();
						_generating_world = true;
						GenerateIndustries();
						_generating_world = false;
					}
				} else if (_game_mode != GM_EDITOR && _patches.raw_industry_construction == 2 && GetIndustrySpec(this->selected_type)->IsRawIndustry()) {
					DoCommandP(0, this->selected_type, InteractiveRandom(), NULL, CMD_BUILD_INDUSTRY | CMD_MSG(STR_4830_CAN_T_CONSTRUCT_THIS_INDUSTRY));
					this->HandleButtonClick(DPIW_FUND_WIDGET);
				} else {
					HandlePlacePushButton(this, DPIW_FUND_WIDGET, SPR_CURSOR_INDUSTRY, VHM_RECT, NULL);
				}
			} break;
		}
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		/* Adjust the number of items in the matrix depending of the rezise */
		this->vscroll.cap  += delta.y / (int)this->resize.step_height;
		this->widget[DPIW_MATRIX_WIDGET].data = (this->vscroll.cap << 8) + 1;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		bool success = true;
		/* We do not need to protect ourselves against "Random Many Industries" in this mode */
		const IndustrySpec *indsp = GetIndustrySpec(this->selected_type);
		uint32 seed = InteractiveRandom();

		if (_game_mode == GM_EDITOR) {
			/* Show error if no town exists at all */
			if (GetNumTowns() == 0) {
				SetDParam(0, indsp->name);
				ShowErrorMessage(STR_0286_MUST_BUILD_TOWN_FIRST, STR_0285_CAN_T_BUILD_HERE, pt.x, pt.y);
				return;
			}

			_current_player = OWNER_NONE;
			_generating_world = true;
			_ignore_restrictions = true;
			success = DoCommandP(tile, (InteractiveRandomRange(indsp->num_table) << 16) | this->selected_type, seed, NULL, CMD_BUILD_INDUSTRY | CMD_MSG(STR_4830_CAN_T_CONSTRUCT_THIS_INDUSTRY));
			if (!success) {
				SetDParam(0, indsp->name);
				ShowErrorMessage(_error_message, STR_0285_CAN_T_BUILD_HERE, pt.x, pt.y);
			}

			_ignore_restrictions = false;
			_generating_world = false;
		} else {
			success = DoCommandP(tile, (InteractiveRandomRange(indsp->num_table) << 16) | this->selected_type, seed, NULL, CMD_BUILD_INDUSTRY | CMD_MSG(STR_4830_CAN_T_CONSTRUCT_THIS_INDUSTRY));
		}

		/* If an industry has been built, just reset the cursor and the system */
		if (success) ResetObjectToPlace();
	}

	virtual void OnTick()
	{
		if (_pause_game != 0) return;
		if (!this->timer_enabled) return;
		if (--this->callback_timer == 0) {
			/* We have just passed another day.
			 * See if we need to update availability of currently selected industry */
			this->callback_timer = DAY_TICKS;  //restart counter

			const IndustrySpec *indsp = GetIndustrySpec(this->selected_type);

			if (indsp->enabled) {
				bool call_back_result = CheckIfCallBackAllowsAvailability(this->selected_type, IACT_USERCREATION);

				/* Only if result does match the previous state would it require a redraw. */
				if (call_back_result != this->enabled[this->selected_index]) {
					this->enabled[this->selected_index] = call_back_result;
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
		this->SetDirty();
	}
};

void ShowBuildIndustryWindow()
{
	if (_game_mode != GM_EDITOR && !IsValidPlayer(_current_player)) return;
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

public:
	IndustryViewWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->flags4 |= WF_DISABLE_VP_SCROLL;
		this->editbox_line = 0;
		this->clicked_line = 0;
		this->clicked_button = 0;
		InitializeWindowViewport(this, 3, 17, 254, 86, GetIndustry(window_number)->xy + TileDiffXY(1, 1), ZOOM_LVL_INDUSTRY);
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		Industry *i = GetIndustry(this->window_number);
		const IndustrySpec *ind = GetIndustrySpec(i->type);
		int y = this->widget[IVW_INFO].top + 1;
		bool first = true;
		bool has_accept = false;

		SetDParam(0, this->window_number);
		this->DrawWidgets();

		if (HasBit(ind->callback_flags, CBM_IND_PRODUCTION_CARGO_ARRIVAL) || HasBit(ind->callback_flags, CBM_IND_PRODUCTION_256_TICKS)) {
			for (byte j = 0; j < lengthof(i->accepts_cargo); j++) {
				if (i->accepts_cargo[j] == CT_INVALID) continue;
				has_accept = true;
				if (first) {
					DrawStringTruncated(2, y, STR_INDUSTRY_WINDOW_WAITING_FOR_PROCESSING, TC_FROMSTRING, this->widget[IVW_INFO].right - 2);
					y += 10;
					first = false;
				}
				SetDParam(0, i->accepts_cargo[j]);
				SetDParam(1, i->incoming_cargo_waiting[j]);
				SetDParam(2, GetCargoSuffix(j, CST_VIEW, i, i->type, ind));
				DrawStringTruncated(4, y, STR_INDUSTRY_WINDOW_WAITING_STOCKPILE_CARGO, TC_FROMSTRING, this->widget[IVW_INFO].right - 4);
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
				SetDParam(p++, GetCargoSuffix(j, CST_VIEW, i, i->type, ind));
			}
			if (has_accept) {
				DrawStringTruncated(2, y, str, TC_FROMSTRING, this->widget[IVW_INFO].right - 2);
				y += 10;
			}
		}

		first = true;
		for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
			if (i->produced_cargo[j] == CT_INVALID) continue;
			if (first) {
				if (has_accept) y += 10;
				DrawStringTruncated(2, y, STR_482A_PRODUCTION_LAST_MONTH, TC_FROMSTRING, this->widget[IVW_INFO].right - 2);
				y += 10;
				this->production_offset_y = y;
				first = false;
			}

			SetDParam(0, i->produced_cargo[j]);
			SetDParam(1, i->last_month_production[j]);
			SetDParam(2, GetCargoSuffix(j + 3, CST_VIEW, i, i->type, ind));

			SetDParam(3, i->last_month_pct_transported[j] * 100 >> 8);
			uint x = 4 + (IsProductionAlterable(i) ? 30 : 0);
			DrawStringTruncated(x, y, STR_482B_TRANSPORTED, TC_FROMSTRING, this->widget[IVW_INFO].right - x);
			/* Let's put out those buttons.. */
			if (IsProductionAlterable(i)) {
				DrawArrowButtons(5, y, 3, (this->clicked_line == j + 1) ? this->clicked_button : 0,
						!IsProductionMinimum(i, j), !IsProductionMaximum(i, j));
			}
			y += 10;
		}

		/* Get the extra message for the GUI */
		if (HasBit(ind->callback_flags, CBM_IND_WINDOW_MORE_TEXT)) {
			uint16 callback_res = GetIndustryCallback(CBID_INDUSTRY_WINDOW_MORE_TEXT, 0, 0, i, i->type, i->xy);
			if (callback_res != CALLBACK_FAILED) {
				StringID message = GetGRFStringID(ind->grf_prop.grffile->grfid, 0xD000 + callback_res);
				if (message != STR_NULL && message != STR_UNDEFINED) {
					const Widget *wi = &this->widget[IVW_INFO];
					y += 10;

					PrepareTextRefStackUsage(6);
					/* Use all the available space left from where we stand up to the end of the window */
					y += DrawStringMultiLine(2, y, message, wi->right - wi->left - 4, -1);
					StopTextRefStackUsage();
				}
			}
		}

		if (y > this->widget[IVW_INFO].bottom) {
			this->SetDirty();
			ResizeWindowForWidget(this, IVW_INFO, 0, y - this->widget[IVW_INFO].top);
			this->SetDirty();
			return;
		}

		this->DrawViewport();
	}

	virtual void OnClick(Point pt, int widget)
	{
		Industry *i;

		switch (widget) {
			case IVW_INFO: {
				int line, x;

				i = GetIndustry(this->window_number);

				/* We should work if needed.. */
				if (!IsProductionAlterable(i)) return;
				x = pt.x;
				line = (pt.y - this->production_offset_y) / 10;
				if (pt.y >= this->production_offset_y && IsInsideMM(line, 0, 2) && i->produced_cargo[line] != CT_INVALID) {
					if (IsInsideMM(x, 5, 25) ) {
						/* Clicked buttons, decrease or increase production */
						if (x < 15) {
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
						this->flags4 |= 5 << WF_TIMEOUT_SHL;
						this->clicked_line = line + 1;
						this->clicked_button = (x < 15 ? 1 : 2);
					} else if (IsInsideMM(x, 34, 160)) {
						/* clicked the text */
						this->editbox_line = line;
						SetDParam(0, i->production_rate[line] * 8);
						ShowQueryString(STR_CONFIG_PATCHES_INT32, STR_CONFIG_GAME_PRODUCTION, 10, 100, this, CS_ALPHANUMERAL);
					}
				}
			} break;

			case IVW_GOTO:
				i = GetIndustry(this->window_number);
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

	virtual void OnResize(Point new_size, Point delta)
	{
		this->viewport->width            += delta.x;
		this->viewport->height           += delta.y;
		this->viewport->virtual_width    += delta.x;
		this->viewport->virtual_height   += delta.y;
		this->viewport->dest_scrollpos_x -= delta.x;
		this->viewport->dest_scrollpos_y -= delta.y;
		UpdateViewportPosition(this);
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (StrEmpty(str)) return;

		Industry* i = GetIndustry(this->window_number);
		int line = this->editbox_line;

		i->production_rate[line] = ClampU(atoi(str), 0, 255);
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
static const Widget _industry_view_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     9,     0,    10,     0,    13, STR_00C5,          STR_018B_CLOSE_WINDOW},            // IVW_CLOSEBOX
{    WWT_CAPTION,  RESIZE_RIGHT,     9,    11,   247,     0,    13, STR_4801,          STR_018C_WINDOW_TITLE_DRAG_THIS},  // IVW_CAPTION
{  WWT_STICKYBOX,     RESIZE_LR,     9,   248,   259,     0,    13, 0x0,               STR_STICKY_BUTTON},                // IVW_STICKY
{      WWT_PANEL,     RESIZE_RB,     9,     0,   259,    14,   105, 0x0,               STR_NULL},                         // IVW_BACKGROUND
{      WWT_INSET,     RESIZE_RB,     9,     2,   257,    16,   103, 0x0,               STR_NULL},                         // IVW_VIEWPORT
{      WWT_PANEL,    RESIZE_RTB,     9,     0,   259,   106,   107, 0x0,               STR_NULL},                         // IVW_INFO
{ WWT_PUSHTXTBTN,     RESIZE_TB,     9,     0,   129,   108,   119, STR_00E4_LOCATION, STR_482C_CENTER_THE_MAIN_VIEW_ON}, // IVW_GOTO
{      WWT_PANEL,    RESIZE_RTB,     9,   130,   247,   108,   119, 0x0,               STR_NULL},                         // IVW_SPACER
{  WWT_RESIZEBOX,   RESIZE_LRTB,     9,   248,   259,   108,   119, 0x0,               STR_RESIZE_BUTTON},                // IVW_RESIZE
{   WIDGETS_END},
};

/** Window definition of the view industy gui */
static const WindowDesc _industry_view_desc = {
	WDP_AUTO, WDP_AUTO, 260, 120, 260, 120,
	WC_INDUSTRY_VIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_industry_view_widgets,
};

void ShowIndustryViewWindow(int industry)
{
	AllocateWindowDescFront<IndustryViewWindow>(&_industry_view_desc, industry);
}

/** Names of the widgets of the industry directory gui */
enum IndustryDirectoryWidgets {
	IDW_CLOSEBOX = 0,
	IDW_CAPTION,
	IDW_STICKY,
	IDW_SORTBYNAME,
	IDW_SORTBYTYPE,
	IDW_SORTBYPROD,
	IDW_SORTBYTRANSPORT,
	IDW_SPACER,
	IDW_INDUSTRY_LIST,
	IDW_SCROLLBAR,
	IDW_RESIZE,
};

/** Widget definition of the industy directory gui */
static const Widget _industry_directory_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    13,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW},             // IDW_CLOSEBOX
{    WWT_CAPTION,  RESIZE_RIGHT,    13,    11,   495,     0,    13, STR_INDUSTRYDIR_CAPTION, STR_018C_WINDOW_TITLE_DRAG_THIS},   // IDW_CAPTION
{  WWT_STICKYBOX,     RESIZE_LR,    13,   496,   507,     0,    13, 0x0,                     STR_STICKY_BUTTON},                 // IDW_STICKY
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,     0,   100,    14,    25, STR_SORT_BY_NAME,        STR_SORT_ORDER_TIP},                // IDW_SORTBYNAME
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,   101,   200,    14,    25, STR_SORT_BY_TYPE,        STR_SORT_ORDER_TIP},                // IDW_SORTBYTYPE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,   201,   300,    14,    25, STR_SORT_BY_PRODUCTION,  STR_SORT_ORDER_TIP},                // IDW_SORTBYPROD
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    13,   301,   400,    14,    25, STR_SORT_BY_TRANSPORTED, STR_SORT_ORDER_TIP},                // IDW_SORTBYTRANSPORT
{      WWT_PANEL,  RESIZE_RIGHT,    13,   401,   495,    14,    25, 0x0,                     STR_NULL},                          // IDW_SPACER
{      WWT_PANEL,     RESIZE_RB,    13,     0,   495,    26,   189, 0x0,                     STR_200A_TOWN_NAMES_CLICK_ON_NAME}, // IDW_INDUSRTY_LIST
{  WWT_SCROLLBAR,    RESIZE_LRB,    13,   496,   507,    14,   177, 0x0,                     STR_0190_SCROLL_BAR_SCROLLS_LIST},  // IDW_SCROLLBAR
{  WWT_RESIZEBOX,   RESIZE_LRTB,    13,   496,   507,   178,   189, 0x0,                     STR_RESIZE_BUTTON},                 // IDW_RESIZE
{   WIDGETS_END},
};

static char _bufcache[96];
static const Industry* _last_industry;
static int _internal_sort_order;

static int CDECL GeneralIndustrySorter(const void *a, const void *b)
{
	const Industry* i = *(const Industry**)a;
	const Industry* j = *(const Industry**)b;
	int r;

	switch (_internal_sort_order >> 1) {
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

	if (_internal_sort_order & 1) r = -r;
	return r;
}

typedef GUIList<const Industry*> GUIIndustryList;

/**
 * Rebuild industries list if the VL_REBUILD flag is set
 *
 * @param sl pointer to industry list
 */
static void BuildIndustriesList(GUIIndustryList *sl)
{
	uint n = 0;
	const Industry *i;

	if (!(sl->flags & VL_REBUILD)) return;

	/* Create array for sorting */
	const Industry **industry_sort = MallocT<const Industry*>(GetMaxIndustryIndex() + 1);

	DEBUG(misc, 3, "Building industry list");

	FOR_ALL_INDUSTRIES(i) industry_sort[n++] = i;

	free((void*)sl->sort_list);
	sl->sort_list = MallocT<const Industry*>(n);
	sl->list_length = n;

	for (uint i = 0; i < n; ++i) sl->sort_list[i] = industry_sort[i];

	sl->flags &= ~VL_REBUILD;
	sl->flags |= VL_RESORT;
	free((void*)industry_sort);
}


/**
 * Sort industry list if the VL_RESORT flag is set
 *
 * @param sl pointer to industry list
 */
static void SortIndustriesList(GUIIndustryList *sl)
{
	if (!(sl->flags & VL_RESORT)) return;

	_internal_sort_order = (sl->sort_type << 1) | (sl->flags & VL_DESC);
	_last_industry = NULL; // used for "cache" in namesorting
	qsort((void*)sl->sort_list, sl->list_length, sizeof(sl->sort_list[0]), &GeneralIndustrySorter);

	sl->flags &= ~VL_RESORT;
}

/**
 * The list of industries.
 */
struct IndustryDirectoryWindow : public Window, public GUIIndustryList {
	static Listing industry_sort;

	IndustryDirectoryWindow(const WindowDesc *desc, WindowNumber number) : Window(desc, number)
	{
		this->vscroll.cap = 16;
		this->resize.height = this->height - 6 * 10; // minimum 10 items
		this->resize.step_height = 10;
		this->FindWindowPlacementAndResize(desc);

		this->sort_list = NULL;
		this->flags = VL_REBUILD;
		this->sort_type = industry_sort.criteria;
		if (industry_sort.order) this->flags |= VL_DESC;
	}

	virtual void OnPaint()
	{
		BuildIndustriesList(this);
		SortIndustriesList(this);

		SetVScrollCount(this, this->list_length);

		this->DrawWidgets();
		this->DrawSortButtonState(IDW_SORTBYNAME + this->sort_type, this->flags & VL_DESC ? SBS_DOWN : SBS_UP);

		int max = min(this->vscroll.pos + this->vscroll.cap, this->list_length);
		int y = 28; // start of the list-widget

		for (int n = this->vscroll.pos; n < max; ++n) {
			const Industry* i = this->sort_list[n];
			const IndustrySpec *indsp = GetIndustrySpec(i->type);
			byte p = 0;

			/* Industry name */
			SetDParam(p++, i->index);

			/* Industry productions */
			for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
				if (i->produced_cargo[j] == CT_INVALID) continue;
				SetDParam(p++, i->produced_cargo[j]);
				SetDParam(p++, i->last_month_production[j]);
				SetDParam(p++, GetCargoSuffix(j + 3, CST_DIR, (Industry*)i, i->type, indsp));
			}

			/* Transported productions */
			for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
				if (i->produced_cargo[j] == CT_INVALID) continue;
				SetDParam(p++, i->last_month_pct_transported[j] * 100 >> 8);
			}

			/* Drawing the right string */
			StringID str = STR_INDUSTRYDIR_ITEM_NOPROD;
			if (p != 1) str = (p == 5) ? STR_INDUSTRYDIR_ITEM : STR_INDUSTRYDIR_ITEM_TWO;
			DrawStringTruncated(4, y, str, TC_FROMSTRING, this->widget[IDW_INDUSTRY_LIST].right - 4);

			y += 10;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case IDW_SORTBYNAME:
			case IDW_SORTBYTYPE:
			case IDW_SORTBYPROD:
			case IDW_SORTBYTRANSPORT:
				if (this->sort_type == (widget - IDW_SORTBYNAME)) {
					this->flags ^= VL_DESC;
				} else {
					this->sort_type = widget - IDW_SORTBYNAME;
					this->flags &= ~VL_DESC;
				}
				industry_sort.criteria = this->sort_type;
				industry_sort.order = HasBit(this->flags, 0);
				this->flags |= VL_RESORT;
				this->SetDirty();
				break;

			case IDW_INDUSTRY_LIST: {
				int y = (pt.y - 28) / 10;
				uint16 p;

				if (!IsInsideMM(y, 0, this->vscroll.cap)) return;
				p = y + this->vscroll.pos;
				if (p < this->list_length) {
					if (_ctrl_pressed) {
						ShowExtraViewPortWindow(this->sort_list[p]->xy);
					} else {
						ScrollMainWindowToTile(this->sort_list[p]->xy);
					}
				}
			} break;
		}
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		this->vscroll.cap += delta.y / 10;
	}

	virtual void OnInvalidateData(int data)
	{
		this->flags |= (data == 0 ? VL_REBUILD : VL_RESORT);
		this->InvalidateWidget(IDW_INDUSTRY_LIST);
	}
};

Listing IndustryDirectoryWindow::industry_sort = {0, 0};

/** Window definition of the industy directory gui */
static const WindowDesc _industry_directory_desc = {
	WDP_AUTO, WDP_AUTO, 508, 190, 508, 190,
	WC_INDUSTRY_DIRECTORY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_industry_directory_widgets,
};

void ShowIndustryDirectory()
{
	AllocateWindowDescFront<IndustryDirectoryWindow>(&_industry_directory_desc, 0);
}
