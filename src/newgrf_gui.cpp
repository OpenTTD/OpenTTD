/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_gui.cpp GUI to change NewGRF settings. */

#include "stdafx.h"
#include "error.h"
#include "settings_gui.h"
#include "newgrf.h"
#include "strings_func.h"
#include "window_func.h"
#include "gamelog.h"
#include "settings_type.h"
#include "settings_func.h"
#include "widgets/dropdown_type.h"
#include "widgets/dropdown_func.h"
#include "network/network.h"
#include "network/network_content.h"
#include "sortlist_type.h"
#include "stringfilter_type.h"
#include "querystring_gui.h"
#include "core/geometry_func.hpp"
#include "newgrf_text.h"
#include "textfile_gui.h"
#include "tilehighlight_func.h"
#include "fios.h"
#include "timer/timer.h"
#include "timer/timer_window.h"
#include "zoom_func.h"

#include "widgets/newgrf_widget.h"
#include "widgets/misc_widget.h"

#include "table/sprites.h"

#include "safeguards.h"

/**
 * Show the first NewGRF error we can find.
 */
void ShowNewGRFError()
{
	/* Do not show errors when entering the main screen */
	if (_game_mode == GM_MENU) return;

	for (const GRFConfig *c = _grfconfig; c != nullptr; c = c->next) {
		/* Only show Fatal and Error level messages */
		if (!c->error.has_value() || (c->error->severity != STR_NEWGRF_ERROR_MSG_FATAL && c->error->severity != STR_NEWGRF_ERROR_MSG_ERROR)) continue;

		SetDParamStr(0, c->GetName());
		SetDParam   (1, c->error->message != STR_NULL ? c->error->message : STR_JUST_RAW_STRING);
		SetDParamStr(2, c->error->custom_message);
		SetDParamStr(3, c->filename);
		SetDParamStr(4, c->error->data);
		for (uint i = 0; i < c->error->param_value.size(); i++) {
			SetDParam(5 + i, c->error->param_value[i]);
		}
		if (c->error->severity == STR_NEWGRF_ERROR_MSG_FATAL) {
			ShowErrorMessage(STR_NEWGRF_ERROR_FATAL_POPUP, INVALID_STRING_ID, WL_CRITICAL);
		} else {
			ShowErrorMessage(STR_NEWGRF_ERROR_POPUP, INVALID_STRING_ID, WL_ERROR);
		}
		break;
	}
}

static void ShowNewGRFInfo(const GRFConfig *c, const Rect &r, bool show_params)
{
	Rect tr = r.Shrink(WidgetDimensions::scaled.frametext);
	if (c->error.has_value()) {
		SetDParamStr(0, c->error->custom_message); // is skipped by built-in messages
		SetDParamStr(1, c->filename);
		SetDParamStr(2, c->error->data);
		for (uint i = 0; i < c->error->param_value.size(); i++) {
			SetDParam(3 + i, c->error->param_value[i]);
		}

		SetDParamStr(0, GetString(c->error->message != STR_NULL ? c->error->message : STR_JUST_RAW_STRING));
		tr.top = DrawStringMultiLine(tr, c->error->severity);
	}

	/* Draw filename or not if it is not known (GRF sent over internet) */
	if (!c->filename.empty()) {
		SetDParamStr(0, c->filename);
		tr.top = DrawStringMultiLine(tr, STR_NEWGRF_SETTINGS_FILENAME);
	}

	/* Prepare and draw GRF ID */
	SetDParamStr(0, fmt::format("{:08X}", BSWAP32(c->ident.grfid)));
	tr.top = DrawStringMultiLine(tr, STR_NEWGRF_SETTINGS_GRF_ID);

	if ((_settings_client.gui.newgrf_developer_tools || _settings_client.gui.newgrf_show_old_versions) && c->version != 0) {
		SetDParam(0, c->version);
		tr.top = DrawStringMultiLine(tr, STR_NEWGRF_SETTINGS_VERSION);
	}
	if ((_settings_client.gui.newgrf_developer_tools || _settings_client.gui.newgrf_show_old_versions) && c->min_loadable_version != 0) {
		SetDParam(0, c->min_loadable_version);
		tr.top = DrawStringMultiLine(tr, STR_NEWGRF_SETTINGS_MIN_VERSION);
	}

	/* Prepare and draw MD5 sum */
	SetDParamStr(0, FormatArrayAsHex(c->ident.md5sum));
	tr.top = DrawStringMultiLine(tr, STR_NEWGRF_SETTINGS_MD5SUM);

	/* Show GRF parameter list */
	if (show_params) {
		if (c->num_params > 0) {
			SetDParam(0, STR_JUST_RAW_STRING);
			SetDParamStr(1, GRFBuildParamList(c));
		} else {
			SetDParam(0, STR_NEWGRF_SETTINGS_PARAMETER_NONE);
		}
		tr.top = DrawStringMultiLine(tr, STR_NEWGRF_SETTINGS_PARAMETER);

		/* Draw the palette of the NewGRF */
		if (c->palette & GRFP_BLT_32BPP) {
			SetDParam(0, (c->palette & GRFP_USE_WINDOWS) ? STR_NEWGRF_SETTINGS_PALETTE_LEGACY_32BPP : STR_NEWGRF_SETTINGS_PALETTE_DEFAULT_32BPP);
		} else {
			SetDParam(0, (c->palette & GRFP_USE_WINDOWS) ? STR_NEWGRF_SETTINGS_PALETTE_LEGACY : STR_NEWGRF_SETTINGS_PALETTE_DEFAULT);
		}
		tr.top = DrawStringMultiLine(tr, STR_NEWGRF_SETTINGS_PALETTE);
	}

	/* Show flags */
	if (c->status == GCS_NOT_FOUND)       tr.top = DrawStringMultiLine(tr, STR_NEWGRF_SETTINGS_NOT_FOUND);
	if (c->status == GCS_DISABLED)        tr.top = DrawStringMultiLine(tr, STR_NEWGRF_SETTINGS_DISABLED);
	if (HasBit(c->flags, GCF_INVALID))    tr.top = DrawStringMultiLine(tr, STR_NEWGRF_SETTINGS_INCOMPATIBLE);
	if (HasBit(c->flags, GCF_COMPATIBLE)) tr.top = DrawStringMultiLine(tr, STR_NEWGRF_COMPATIBLE_LOADED);

	/* Draw GRF info if it exists */
	if (!StrEmpty(c->GetDescription())) {
		SetDParamStr(0, c->GetDescription());
		tr.top = DrawStringMultiLine(tr, STR_JUST_RAW_STRING, TC_BLACK);
	} else {
		tr.top = DrawStringMultiLine(tr, STR_NEWGRF_SETTINGS_NO_INFO);
	}
}

/**
 * Window for setting the parameters of a NewGRF.
 */
struct NewGRFParametersWindow : public Window {
	static GRFParameterInfo dummy_parameter_info; ///< Dummy info in case a newgrf didn't provide info about some parameter.
	GRFConfig *grf_config; ///< Set the parameters of this GRFConfig.
	uint clicked_button;   ///< The row in which a button was clicked or UINT_MAX.
	bool clicked_increase; ///< True if the increase button was clicked, false for the decrease button.
	bool clicked_dropdown; ///< Whether the dropdown is open.
	bool closing_dropdown; ///< True, if the dropdown list is currently closing.
	uint clicked_row;      ///< The selected parameter
	int line_height;       ///< Height of a row in the matrix widget.
	Scrollbar *vscroll;
	bool action14present;  ///< True if action14 information is present.
	bool editable;         ///< Allow editing parameters.

	NewGRFParametersWindow(WindowDesc *desc, bool is_baseset, GRFConfig *c, bool editable) : Window(desc),
		grf_config(c),
		clicked_button(UINT_MAX),
		clicked_dropdown(false),
		closing_dropdown(false),
		clicked_row(UINT_MAX),
		editable(editable)
	{
		this->action14present = (c->num_valid_params != c->param.size() || !c->param_info.empty());

		this->CreateNestedTree();
		this->GetWidget<NWidgetCore>(WID_NP_CAPTION)->SetDataTip(is_baseset ? STR_BASEGRF_PARAMETERS_CAPTION : STR_NEWGRF_PARAMETERS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS);
		this->vscroll = this->GetScrollbar(WID_NP_SCROLLBAR);
		this->GetWidget<NWidgetStacked>(WID_NP_SHOW_NUMPAR)->SetDisplayedPlane(this->action14present ? SZSP_HORIZONTAL : 0);
		this->GetWidget<NWidgetStacked>(WID_NP_SHOW_DESCRIPTION)->SetDisplayedPlane(this->action14present ? 0 : SZSP_HORIZONTAL);
		this->FinishInitNested();  // Initializes 'this->line_height' as side effect.

		this->SetWidgetDisabledState(WID_NP_RESET, !this->editable);

		this->InvalidateData();
	}

	/**
	 * Get a dummy parameter-info object with default information.
	 * @param nr The param number that should be changed.
	 * @return GRFParameterInfo with dummy information about the given parameter.
	 */
	static GRFParameterInfo &GetDummyParameterInfo(uint nr)
	{
		dummy_parameter_info.param_nr = nr;
		return dummy_parameter_info;
	}

	/**
	 * Test if GRF Parameter Info exists for a given parameter index.
	 * @param nr The param number that should be tested.
	 * @return True iff the parameter info exists.
	 */
	bool HasParameterInfo(uint nr) const
	{
		return nr < this->grf_config->param_info.size() && this->grf_config->param_info[nr].has_value();
	}

	/**
	 * Get GRF Parameter Info exists for a given parameter index.
	 * If the parameter info does not exist, a dummy parameter-info is returned instead.
	 * @param nr The param number that should be got.
	 * @return Reference to the GRFParameterInfo.
	 */
	GRFParameterInfo &GetParameterInfo(uint nr) const
	{
		return this->HasParameterInfo(nr) ? this->grf_config->param_info[nr].value() : GetDummyParameterInfo(nr);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_NP_NUMPAR_DEC:
			case WID_NP_NUMPAR_INC: {
				size->width  = std::max(SETTING_BUTTON_WIDTH / 2, GetCharacterHeight(FS_NORMAL));
				size->height = std::max(SETTING_BUTTON_HEIGHT, GetCharacterHeight(FS_NORMAL));
				break;
			}

			case WID_NP_NUMPAR: {
				SetDParamMaxValue(0, this->grf_config->param.size());
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case WID_NP_BACKGROUND:
				this->line_height = std::max(SETTING_BUTTON_HEIGHT, GetCharacterHeight(FS_NORMAL)) + padding.height;

				resize->width = 1;
				resize->height = this->line_height;
				size->height = 5 * this->line_height;
				break;

			case WID_NP_DESCRIPTION:
				/* Minimum size of 4 lines. The 500 is the default size of the window. */
				Dimension suggestion = {500U - WidgetDimensions::scaled.frametext.Horizontal(), (uint)GetCharacterHeight(FS_NORMAL) * 4 + WidgetDimensions::scaled.frametext.Vertical()};
				for (const auto &par_info : this->grf_config->param_info) {
					if (!par_info.has_value()) continue;
					const char *desc = GetGRFStringFromGRFText(par_info->desc);
					if (desc == nullptr) continue;
					Dimension d = GetStringMultiLineBoundingBox(desc, suggestion);
					d.height += WidgetDimensions::scaled.frametext.Vertical();
					suggestion = maxdim(d, suggestion);
				}
				size->height = suggestion.height;
				break;
		}
	}

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_NP_NUMPAR:
				SetDParam(0, this->vscroll->GetCount());
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget == WID_NP_DESCRIPTION) {
			if (!this->HasParameterInfo(this->clicked_row)) return;
			const GRFParameterInfo &par_info = this->GetParameterInfo(this->clicked_row);
			const char *desc = GetGRFStringFromGRFText(par_info.desc);
			if (desc == nullptr) return;
			DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.framerect), desc, TC_BLACK);
			return;
		} else if (widget != WID_NP_BACKGROUND) {
			return;
		}

		Rect ir = r.Shrink(WidgetDimensions::scaled.frametext, RectPadding::zero);
		bool rtl = _current_text_dir == TD_RTL;
		uint buttons_left = rtl ? ir.right - SETTING_BUTTON_WIDTH : ir.left;
		Rect tr = ir.Indent(SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_wide, rtl);

		int button_y_offset = (this->line_height - SETTING_BUTTON_HEIGHT) / 2;
		int text_y_offset = (this->line_height - GetCharacterHeight(FS_NORMAL)) / 2;
		for (uint i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && i < this->vscroll->GetCount(); i++) {
			GRFParameterInfo &par_info = this->GetParameterInfo(i);
			uint32_t current_value = par_info.GetValue(this->grf_config);
			bool selected = (i == this->clicked_row);

			if (par_info.type == PTYPE_BOOL) {
				DrawBoolButton(buttons_left, ir.top + button_y_offset, current_value != 0, this->editable);
				SetDParam(2, par_info.GetValue(this->grf_config) == 0 ? STR_CONFIG_SETTING_OFF : STR_CONFIG_SETTING_ON);
			} else if (par_info.type == PTYPE_UINT_ENUM) {
				if (par_info.complete_labels) {
					DrawDropDownButton(buttons_left, ir.top + button_y_offset, COLOUR_YELLOW, this->clicked_row == i && this->clicked_dropdown, this->editable);
				} else {
					DrawArrowButtons(buttons_left, ir.top + button_y_offset, COLOUR_YELLOW, (this->clicked_button == i) ? 1 + (this->clicked_increase != rtl) : 0, this->editable && current_value > par_info.min_value, this->editable && current_value < par_info.max_value);
				}
				SetDParam(2, STR_JUST_INT);
				SetDParam(3, current_value);
				auto it = par_info.value_names.find(current_value);
				if (it != par_info.value_names.end()) {
					const char *label = GetGRFStringFromGRFText(it->second);
					if (label != nullptr) {
						SetDParam(2, STR_JUST_RAW_STRING);
						SetDParamStr(3, label);
					}
				}
			}

			const char *name = GetGRFStringFromGRFText(par_info.name);
			if (name != nullptr) {
				SetDParam(0, STR_JUST_RAW_STRING);
				SetDParamStr(1, name);
			} else {
				SetDParam(0, STR_NEWGRF_PARAMETERS_DEFAULT_NAME);
				SetDParam(1, i + 1);
			}

			DrawString(tr.left, tr.right, ir.top + text_y_offset, STR_NEWGRF_PARAMETERS_SETTING, selected ? TC_WHITE : TC_LIGHT_BLUE);
			ir.top += this->line_height;
		}
	}

	void OnPaint() override
	{
		if (this->closing_dropdown) {
			this->closing_dropdown = false;
			this->clicked_dropdown = false;
		}
		this->DrawWidgets();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_NP_NUMPAR_DEC:
				if (this->editable && !this->action14present && this->grf_config->num_params > 0) {
					this->grf_config->num_params--;
					this->InvalidateData();
					SetWindowDirty(WC_GAME_OPTIONS, WN_GAME_OPTIONS_NEWGRF_STATE);
				}
				break;

			case WID_NP_NUMPAR_INC: {
				GRFConfig *c = this->grf_config;
				if (this->editable && !this->action14present && c->num_params < c->num_valid_params) {
					c->param[c->num_params++] = 0;
					this->InvalidateData();
					SetWindowDirty(WC_GAME_OPTIONS, WN_GAME_OPTIONS_NEWGRF_STATE);
				}
				break;
			}

			case WID_NP_BACKGROUND: {
				if (!this->editable) break;
				uint num = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_NP_BACKGROUND);
				if (num >= this->vscroll->GetCount()) break;

				if (this->clicked_row != num) {
					this->CloseChildWindows(WC_QUERY_STRING);
					this->CloseChildWindows(WC_DROPDOWN_MENU);
					this->clicked_row = num;
					this->clicked_dropdown = false;
				}

				Rect r = this->GetWidget<NWidgetBase>(widget)->GetCurrentRect().Shrink(WidgetDimensions::scaled.frametext, RectPadding::zero);
				int x = pt.x - r.left;
				if (_current_text_dir == TD_RTL) x = r.Width() - 1 - x;

				GRFParameterInfo &par_info = this->GetParameterInfo(num);

				/* One of the arrows is clicked */
				uint32_t old_val = par_info.GetValue(this->grf_config);
				if (par_info.type != PTYPE_BOOL && IsInsideMM(x, 0, SETTING_BUTTON_WIDTH) && par_info.complete_labels) {
					if (this->clicked_dropdown) {
						/* unclick the dropdown */
						this->CloseChildWindows(WC_DROPDOWN_MENU);
						this->clicked_dropdown = false;
						this->closing_dropdown = false;
					} else {
						int rel_y = (pt.y - r.top) % this->line_height;

						Rect wi_rect;
						wi_rect.left = pt.x - (_current_text_dir == TD_RTL ? SETTING_BUTTON_WIDTH - 1 - x : x);;
						wi_rect.right = wi_rect.left + SETTING_BUTTON_WIDTH - 1;
						wi_rect.top = pt.y - rel_y + (this->line_height - SETTING_BUTTON_HEIGHT) / 2;
						wi_rect.bottom = wi_rect.top + SETTING_BUTTON_HEIGHT - 1;

						/* For dropdowns we also have to check the y position thoroughly, the mouse may not above the just opening dropdown */
						if (pt.y >= wi_rect.top && pt.y <= wi_rect.bottom) {
							this->clicked_dropdown = true;
							this->closing_dropdown = false;

							DropDownList list;
							for (uint32_t i = par_info.min_value; i <= par_info.max_value; i++) {
								list.push_back(std::make_unique<DropDownListStringItem>(GetGRFStringFromGRFText(par_info.value_names.find(i)->second), i, false));
							}

							ShowDropDownListAt(this, std::move(list), old_val, WID_NP_SETTING_DROPDOWN, wi_rect, COLOUR_ORANGE);
						}
					}
				} else if (IsInsideMM(x, 0, SETTING_BUTTON_WIDTH)) {
					uint32_t val = old_val;
					if (par_info.type == PTYPE_BOOL) {
						val = !val;
					} else {
						if (x >= SETTING_BUTTON_WIDTH / 2) {
							/* Increase button clicked */
							if (val < par_info.max_value) val++;
							this->clicked_increase = true;
						} else {
							/* Decrease button clicked */
							if (val > par_info.min_value) val--;
							this->clicked_increase = false;
						}
					}
					if (val != old_val) {
						par_info.SetValue(this->grf_config, val);

						this->clicked_button = num;
						this->unclick_timeout.Reset();
					}
				} else if (par_info.type == PTYPE_UINT_ENUM && !par_info.complete_labels && click_count >= 2) {
					/* Display a query box so users can enter a custom value. */
					SetDParam(0, old_val);
					ShowQueryString(STR_JUST_INT, STR_CONFIG_SETTING_QUERY_CAPTION, 10, this, CS_NUMERAL, QSF_NONE);
				}
				this->SetDirty();
				break;
			}

			case WID_NP_RESET:
				if (!this->editable) break;
				this->grf_config->SetParameterDefaults();
				this->InvalidateData();
				SetWindowDirty(WC_GAME_OPTIONS, WN_GAME_OPTIONS_NEWGRF_STATE);
				break;

			case WID_NP_ACCEPT:
				this->Close();
				break;
		}
	}

	void OnQueryTextFinished(char *str) override
	{
		if (StrEmpty(str)) return;
		int32_t value = atoi(str);
		GRFParameterInfo &par_info = this->GetParameterInfo(this->clicked_row);
		uint32_t val = Clamp<uint32_t>(value, par_info.min_value, par_info.max_value);
		par_info.SetValue(this->grf_config, val);
		this->SetDirty();
	}

	void OnDropdownSelect(WidgetID widget, int index) override
	{
		if (widget != WID_NP_SETTING_DROPDOWN) return;
		assert(this->clicked_dropdown);
		GRFParameterInfo &par_info = this->GetParameterInfo(this->clicked_row);
		par_info.SetValue(this->grf_config, index);
		this->SetDirty();
	}

	void OnDropdownClose(Point, WidgetID widget, int, bool) override
	{
		if (widget != WID_NP_SETTING_DROPDOWN) return;
		/* We cannot raise the dropdown button just yet. OnClick needs some hint, whether
		 * the same dropdown button was clicked again, and then not open the dropdown again.
		 * So, we only remember that it was closed, and process it on the next OnPaint, which is
		 * after OnClick. */
		assert(this->clicked_dropdown);
		this->closing_dropdown = true;
		this->SetDirty();
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_NP_BACKGROUND);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		if (!this->action14present) {
			this->SetWidgetDisabledState(WID_NP_NUMPAR_DEC, !this->editable || this->grf_config->num_params == 0);
			this->SetWidgetDisabledState(WID_NP_NUMPAR_INC, !this->editable || this->grf_config->num_params >= this->grf_config->num_valid_params);
		}

		this->vscroll->SetCount(this->action14present ? this->grf_config->num_valid_params : this->grf_config->num_params);
		if (this->clicked_row != UINT_MAX && this->clicked_row >= this->vscroll->GetCount()) {
			this->clicked_row = UINT_MAX;
			this->CloseChildWindows(WC_QUERY_STRING);
		}
	}

	/** When reset, unclick the button after a small timeout. */
	TimeoutTimer<TimerWindow> unclick_timeout = {std::chrono::milliseconds(150), [this]() {
		this->clicked_button = UINT_MAX;
		this->SetDirty();
	}};
};
GRFParameterInfo NewGRFParametersWindow::dummy_parameter_info(0);


static const NWidgetPart _nested_newgrf_parameter_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE, WID_NP_CAPTION),
		NWidget(WWT_DEFSIZEBOX, COLOUR_MAUVE),
	EndContainer(),
	NWidget(NWID_SELECTION, INVALID_COLOUR, WID_NP_SHOW_NUMPAR),
		NWidget(WWT_PANEL, COLOUR_MAUVE), SetResize(1, 0), SetFill(1, 0), SetPIP(4, 0, 4),
			NWidget(NWID_HORIZONTAL), SetPIP(4, 0, 4),
				NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_NP_NUMPAR_DEC), SetMinimalSize(12, 12), SetDataTip(AWV_DECREASE, STR_NULL),
				NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_NP_NUMPAR_INC), SetMinimalSize(12, 12), SetDataTip(AWV_INCREASE, STR_NULL),
				NWidget(WWT_TEXT, COLOUR_MAUVE, WID_NP_NUMPAR), SetResize(1, 0), SetFill(1, 0), SetPadding(0, 0, 0, 4), SetDataTip(STR_NEWGRF_PARAMETERS_NUM_PARAM, STR_NULL),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_MAUVE, WID_NP_BACKGROUND), SetMinimalSize(188, 182), SetResize(1, 1), SetFill(1, 0), SetMatrixDataTip(1, 0, STR_NULL), SetScrollbar(WID_NP_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_NP_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_SELECTION, INVALID_COLOUR, WID_NP_SHOW_DESCRIPTION),
		NWidget(WWT_PANEL, COLOUR_MAUVE, WID_NP_DESCRIPTION), SetResize(1, 0), SetFill(1, 0),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_NP_ACCEPT), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_NEWGRF_PARAMETERS_CLOSE, STR_NULL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_NP_RESET), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_NEWGRF_PARAMETERS_RESET, STR_NEWGRF_PARAMETERS_RESET_TOOLTIP),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_MAUVE),
	EndContainer(),
};

/** Window definition for the change grf parameters window */
static WindowDesc _newgrf_parameters_desc(__FILE__, __LINE__,
	WDP_CENTER, "settings_newgrf_config", 500, 208,
	WC_GRF_PARAMETERS, WC_NONE,
	0,
	std::begin(_nested_newgrf_parameter_widgets), std::end(_nested_newgrf_parameter_widgets)
);

void OpenGRFParameterWindow(bool is_baseset, GRFConfig *c, bool editable)
{
	CloseWindowByClass(WC_GRF_PARAMETERS);
	new NewGRFParametersWindow(&_newgrf_parameters_desc, is_baseset, c, editable);
}

/** Window for displaying the textfile of a NewGRF. */
struct NewGRFTextfileWindow : public TextfileWindow {
	const GRFConfig *grf_config; ///< View the textfile of this GRFConfig.

	NewGRFTextfileWindow(TextfileType file_type, const GRFConfig *c) : TextfileWindow(file_type), grf_config(c)
	{
		auto textfile = this->grf_config->GetTextfile(file_type);
		this->LoadTextfile(textfile.value(), NEWGRF_DIR);
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_TF_CAPTION) {
			SetDParam(0, STR_CONTENT_TYPE_NEWGRF);
			SetDParamStr(1, this->grf_config->GetName());
		}
	}
};

void ShowNewGRFTextfileWindow(TextfileType file_type, const GRFConfig *c)
{
	CloseWindowById(WC_TEXTFILE, file_type);
	new NewGRFTextfileWindow(file_type, c);
}

typedef std::map<uint32_t, const GRFConfig *> GrfIdMap; ///< Map of grfid to the grf config.

/**
 * Add all grf configs from \a c into the map.
 * @param c Grf list to add.
 * @param grfid_map Map to add them to.
 */
static void FillGrfidMap(const GRFConfig *c, GrfIdMap *grfid_map)
{
	while (c != nullptr) {
		std::pair<uint32_t, const GRFConfig *> p(c->ident.grfid, c);
		grfid_map->insert(p);
		c = c->next;
	}
}

static void NewGRFConfirmationCallback(Window *w, bool confirmed);
static void ShowSavePresetWindow(const char *initial_text);

/**
 * Window for showing NewGRF files
 */
struct NewGRFWindow : public Window, NewGRFScanCallback {
	typedef GUIList<const GRFConfig *, std::nullptr_t, StringFilter &> GUIGRFConfigList;

	static const uint EDITBOX_MAX_SIZE   =  50;

	static Listing   last_sorting;   ///< Default sorting of #GUIGRFConfigList.
	static Filtering last_filtering; ///< Default filtering of #GUIGRFConfigList.
	static GUIGRFConfigList::SortFunction   * const sorter_funcs[]; ///< Sort functions of the #GUIGRFConfigList.
	static GUIGRFConfigList::FilterFunction * const filter_funcs[]; ///< Filter functions of the #GUIGRFConfigList.

	GUIGRFConfigList avails;    ///< Available (non-active) grfs.
	const GRFConfig *avail_sel; ///< Currently selected available grf. \c nullptr is none is selected.
	int avail_pos;              ///< Index of #avail_sel if existing, else \c -1.
	StringFilter string_filter; ///< Filter for available grf.
	QueryString filter_editbox; ///< Filter editbox;

	StringList grf_presets;     ///< List of known NewGRF presets.

	GRFConfig *actives;         ///< Temporary active grf list to which changes are made.
	GRFConfig *active_sel;      ///< Selected active grf item.

	GRFConfig **orig_list;      ///< List active grfs in the game. Used as initial value, may be updated by the window.
	bool editable;              ///< Is the window editable?
	bool show_params;           ///< Are the grf-parameters shown in the info-panel?
	bool execute;               ///< On pressing 'apply changes' are grf changes applied immediately, or only list is updated.
	int preset;                 ///< Selected preset or \c -1 if none selected.
	int active_over;            ///< Active GRF item over which another one is dragged, \c -1 if none.
	bool modified;              ///< The list of active NewGRFs has been modified since the last time they got saved.

	Scrollbar *vscroll;
	Scrollbar *vscroll2;

	NewGRFWindow(WindowDesc *desc, bool editable, bool show_params, bool execute, GRFConfig **orig_list) : Window(desc), filter_editbox(EDITBOX_MAX_SIZE)
	{
		this->avail_sel   = nullptr;
		this->avail_pos   = -1;
		this->active_sel  = nullptr;
		this->actives     = nullptr;
		this->orig_list   = orig_list;
		this->editable    = editable;
		this->execute     = execute;
		this->show_params = show_params;
		this->preset      = -1;
		this->active_over = -1;

		CopyGRFConfigList(&this->actives, *orig_list, false);
		this->grf_presets = GetGRFPresetList();

		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_NS_SCROLLBAR);
		this->vscroll2 = this->GetScrollbar(WID_NS_SCROLL2BAR);

		this->GetWidget<NWidgetStacked>(WID_NS_SHOW_REMOVE)->SetDisplayedPlane(this->editable ? 0 : 1);
		this->GetWidget<NWidgetStacked>(WID_NS_SHOW_APPLY)->SetDisplayedPlane(this->editable ? 0 : this->show_params ? 1 : SZSP_HORIZONTAL);
		this->FinishInitNested(WN_GAME_OPTIONS_NEWGRF_STATE);

		this->querystrings[WID_NS_FILTER] = &this->filter_editbox;
		this->filter_editbox.cancel_button = QueryString::ACTION_CLEAR;
		if (editable) {
			this->SetFocusedWidget(WID_NS_FILTER);
		} else {
			this->DisableWidget(WID_NS_FILTER);
		}

		this->avails.SetListing(this->last_sorting);
		this->avails.SetFiltering(this->last_filtering);
		this->avails.SetSortFuncs(this->sorter_funcs);
		this->avails.SetFilterFuncs(this->filter_funcs);
		this->avails.ForceRebuild();

		this->OnInvalidateData(GOID_NEWGRF_CURRENT_LOADED);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		CloseWindowByClass(WC_GRF_PARAMETERS);
		CloseWindowByClass(WC_TEXTFILE);
		CloseWindowByClass(WC_SAVE_PRESET);

		if (this->editable && this->modified && !this->execute && !_exit_game) {
			CopyGRFConfigList(this->orig_list, this->actives, true);
			ResetGRFConfig(false);
			ReloadNewGRFData();
		}

		this->Window::Close();
	}

	~NewGRFWindow()
	{
		/* Remove the temporary copy of grf-list used in window */
		ClearGRFConfigList(&this->actives);
	}

	/**
	 * Test whether the currently active set of NewGRFs can be upgraded with the available NewGRFs.
	 * @return Whether an upgrade is possible.
	 */
	bool CanUpgradeCurrent()
	{
		GrfIdMap grfid_map;
		FillGrfidMap(this->actives, &grfid_map);

		for (const GRFConfig *a = _all_grfs; a != nullptr; a = a->next) {
			GrfIdMap::const_iterator iter = grfid_map.find(a->ident.grfid);
			if (iter != grfid_map.end() && a->version > iter->second->version) return true;
		}
		return false;
	}

	/** Upgrade the currently active set of NewGRFs. */
	void UpgradeCurrent()
	{
		GrfIdMap grfid_map;
		FillGrfidMap(this->actives, &grfid_map);

		for (const GRFConfig *a = _all_grfs; a != nullptr; a = a->next) {
			GrfIdMap::iterator iter = grfid_map.find(a->ident.grfid);
			if (iter == grfid_map.end() || iter->second->version >= a->version) continue;

			GRFConfig **c = &this->actives;
			while (*c != iter->second) c = &(*c)->next;
			GRFConfig *d = new GRFConfig(*a);
			d->next = (*c)->next;
			if (d->IsCompatible((*c)->version)) {
				d->CopyParams(**c);
			} else {
				d->SetParameterDefaults();
			}
			if (this->active_sel == *c) {
				CloseWindowByClass(WC_GRF_PARAMETERS);
				CloseWindowByClass(WC_TEXTFILE);
				this->active_sel = nullptr;
			}
			delete *c;
			*c = d;
			iter->second = d;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_NS_FILE_LIST:
			{
				Dimension d = maxdim(GetScaledSpriteSize(SPR_SQUARE), GetScaledSpriteSize(SPR_WARNING_SIGN));
				resize->height = std::max<uint>(d.height + 2U, GetCharacterHeight(FS_NORMAL));
				size->height = std::max(size->height, padding.height + 6 * resize->height);
				break;
			}

			case WID_NS_AVAIL_LIST:
			{
				Dimension d = maxdim(GetScaledSpriteSize(SPR_SQUARE), GetScaledSpriteSize(SPR_WARNING_SIGN));
				resize->height = std::max<uint>(d.height + 2U, GetCharacterHeight(FS_NORMAL));
				size->height = std::max(size->height, padding.height + 8 * resize->height);
				break;
			}

			case WID_NS_NEWGRF_INFO_TITLE: {
				Dimension dim = GetStringBoundingBox(STR_NEWGRF_SETTINGS_INFO_TITLE);
				size->height = std::max(size->height, dim.height + WidgetDimensions::scaled.frametext.Vertical());
				size->width  = std::max(size->width,  dim.width  + WidgetDimensions::scaled.frametext.Horizontal());
				break;
			}

			case WID_NS_NEWGRF_INFO:
				size->height = std::max<uint>(size->height, WidgetDimensions::scaled.framerect.Vertical() + 10 * GetCharacterHeight(FS_NORMAL));
				break;

			case WID_NS_PRESET_LIST: {
				Dimension d = GetStringBoundingBox(STR_NUM_CUSTOM);
				for (const auto &i : this->grf_presets) {
					SetDParamStr(0, i);
					d = maxdim(d, GetStringBoundingBox(STR_JUST_RAW_STRING));
				}
				d.width += padding.width;
				*size = maxdim(d, *size);
				break;
			}

			case WID_NS_CONTENT_DOWNLOAD:
			case WID_NS_CONTENT_DOWNLOAD2: {
				Dimension d = GetStringBoundingBox(STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_BUTTON);
				*size = maxdim(d, GetStringBoundingBox(STR_INTRO_ONLINE_CONTENT));
				size->width  += padding.width;
				size->height += padding.height;
				break;
			}
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_NS_FILE_LIST);
		this->vscroll2->SetCapacityFromWidget(this, WID_NS_AVAIL_LIST);
	}

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_NS_PRESET_LIST:
				if (this->preset == -1) {
					SetDParam(0, STR_NUM_CUSTOM);
				} else {
					SetDParam(0, STR_JUST_RAW_STRING);
					SetDParamStr(1, this->grf_presets[this->preset]);
				}
				break;
		}
	}

	/**
	 * Pick the palette for the sprite of the grf to display.
	 * @param c grf to display.
	 * @return Palette for the sprite.
	 */
	inline PaletteID GetPalette(const GRFConfig *c) const
	{
		PaletteID pal;

		/* Pick a colour */
		switch (c->status) {
			case GCS_NOT_FOUND:
			case GCS_DISABLED:
				pal = PALETTE_TO_RED;
				break;
			case GCS_ACTIVATED:
				pal = PALETTE_TO_GREEN;
				break;
			default:
				pal = PALETTE_TO_BLUE;
				break;
		}

		/* Do not show a "not-failure" colour when it actually failed to load */
		if (pal != PALETTE_TO_RED) {
			if (HasBit(c->flags, GCF_STATIC)) {
				pal = PALETTE_TO_GREY;
			} else if (HasBit(c->flags, GCF_COMPATIBLE)) {
				pal = PALETTE_TO_ORANGE;
			}
		}

		return pal;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_NS_FILE_LIST: {
				const Rect br = r.Shrink(WidgetDimensions::scaled.bevel);
				GfxFillRect(br, PC_BLACK);

				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				uint step_height = this->GetWidget<NWidgetBase>(WID_NS_FILE_LIST)->resize_y;
				Dimension square = GetSpriteSize(SPR_SQUARE);
				Dimension warning = GetSpriteSize(SPR_WARNING_SIGN);
				int square_offset_y = (step_height - square.height) / 2;
				int warning_offset_y = (step_height - warning.height) / 2;
				int offset_y = (step_height - GetCharacterHeight(FS_NORMAL)) / 2;

				bool rtl = _current_text_dir == TD_RTL;
				uint text_left    = rtl ? tr.left : tr.left + square.width + 13;
				uint text_right   = rtl ? tr.right - square.width - 13 : tr.right;
				uint square_left  = rtl ? tr.right - square.width - 3 : tr.left + 3;
				uint warning_left = rtl ? tr.right - square.width - warning.width - 8 : tr.left + square.width + 8;

				int i = 0;
				for (const GRFConfig *c = this->actives; c != nullptr; c = c->next, i++) {
					if (this->vscroll->IsVisible(i)) {
						const char *text = c->GetName();
						bool h = (this->active_sel == c);
						PaletteID pal = this->GetPalette(c);

						if (h) {
							GfxFillRect(br.left, tr.top, br.right, tr.top + step_height - 1, PC_DARK_BLUE);
						} else if (i == this->active_over) {
							/* Get index of current selection. */
							int active_sel_pos = 0;
							for (GRFConfig *c = this->actives; c != nullptr && c != this->active_sel; c = c->next, active_sel_pos++) {}
							if (active_sel_pos != this->active_over) {
								uint top = this->active_over < active_sel_pos ? tr.top + 1 : tr.top + step_height - 2;
								GfxFillRect(tr.left, top - 1, tr.right, top + 1, PC_GREY);
							}
						}
						DrawSprite(SPR_SQUARE, pal, square_left, tr.top + square_offset_y);
						if (c->error.has_value()) DrawSprite(SPR_WARNING_SIGN, 0, warning_left, tr.top + warning_offset_y);
						uint txtoffset = !c->error.has_value() ? 0 : warning.width;
						DrawString(text_left + (rtl ? 0 : txtoffset), text_right - (rtl ? txtoffset : 0), tr.top + offset_y, text, h ? TC_WHITE : TC_ORANGE);
						tr.top += step_height;
					}
				}
				if (i == this->active_over && this->vscroll->IsVisible(i)) { // Highlight is after the last GRF entry.
					GfxFillRect(tr.left, tr.top, tr.right, tr.top + 2, PC_GREY);
				}
				break;
			}

			case WID_NS_AVAIL_LIST: {
				const Rect br = r.Shrink(WidgetDimensions::scaled.bevel);
				GfxFillRect(br, this->active_over == -2 ? PC_DARK_GREY : PC_BLACK);

				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				uint step_height = this->GetWidget<NWidgetBase>(WID_NS_AVAIL_LIST)->resize_y;
				int offset_y = (step_height - GetCharacterHeight(FS_NORMAL)) / 2;
				uint min_index = this->vscroll2->GetPosition();
				uint max_index = std::min(min_index + this->vscroll2->GetCapacity(), (uint)this->avails.size());

				for (uint i = min_index; i < max_index; i++) {
					const GRFConfig *c = this->avails[i];
					bool h = (c == this->avail_sel);
					const char *text = c->GetName();

					if (h) GfxFillRect(br.left, tr.top, br.right, tr.top + step_height - 1, PC_DARK_BLUE);
					DrawString(tr.left, tr.right, tr.top + offset_y, text, h ? TC_WHITE : TC_SILVER);
					tr.top += step_height;
				}
				break;
			}

			case WID_NS_NEWGRF_INFO_TITLE: {
				/* Create the nice grayish rectangle at the details top. */
				GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), PC_DARK_BLUE);
				DrawString(r.left, r.right, CenterBounds(r.top, r.bottom, GetCharacterHeight(FS_NORMAL)), STR_NEWGRF_SETTINGS_INFO_TITLE, TC_FROMSTRING, SA_HOR_CENTER);
				break;
			}

			case WID_NS_NEWGRF_INFO: {
				const GRFConfig *selected = this->active_sel;
				if (selected == nullptr) selected = this->avail_sel;
				if (selected != nullptr) {
					ShowNewGRFInfo(selected, r, this->show_params);
				}
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget >= WID_NS_NEWGRF_TEXTFILE && widget < WID_NS_NEWGRF_TEXTFILE + TFT_CONTENT_END) {
			if (this->active_sel == nullptr && this->avail_sel == nullptr) return;

			ShowNewGRFTextfileWindow((TextfileType)(widget - WID_NS_NEWGRF_TEXTFILE), this->active_sel != nullptr ? this->active_sel : this->avail_sel);
			return;
		}

		switch (widget) {
			case WID_NS_PRESET_LIST: {
				DropDownList list;

				/* Add 'None' option for clearing list */
				list.push_back(std::make_unique<DropDownListStringItem>(STR_NONE, -1, false));

				for (uint i = 0; i < this->grf_presets.size(); i++) {
					list.push_back(std::make_unique<DropDownListStringItem>(this->grf_presets[i], i, false));
				}

				this->CloseChildWindows(WC_QUERY_STRING); // Remove the parameter query window
				ShowDropDownList(this, std::move(list), this->preset, WID_NS_PRESET_LIST);
				break;
			}

			case WID_NS_OPEN_URL: {
				const GRFConfig *c = (this->avail_sel == nullptr) ? this->active_sel : this->avail_sel;

				OpenBrowser(c->GetURL());
				break;
			}

			case WID_NS_PRESET_SAVE:
				ShowSavePresetWindow((this->preset == -1) ? nullptr : this->grf_presets[this->preset].c_str());
				break;

			case WID_NS_PRESET_DELETE:
				if (this->preset == -1) return;

				DeleteGRFPresetFromConfig(this->grf_presets[this->preset].c_str());
				this->grf_presets = GetGRFPresetList();
				this->preset = -1;
				this->InvalidateData();
				this->CloseChildWindows(WC_QUERY_STRING); // Remove the parameter query window
				break;

			case WID_NS_MOVE_UP: { // Move GRF up
				if (this->active_sel == nullptr || !this->editable) break;

				int pos = 0;
				for (GRFConfig **pc = &this->actives; *pc != nullptr; pc = &(*pc)->next, pos++) {
					GRFConfig *c = *pc;
					if (c->next == this->active_sel) {
						c->next = this->active_sel->next;
						this->active_sel->next = c;
						*pc = this->active_sel;
						break;
					}
				}
				this->vscroll->ScrollTowards(pos);
				this->preset = -1;
				this->InvalidateData(GOID_NEWGRF_LIST_EDITED);
				break;
			}

			case WID_NS_MOVE_DOWN: { // Move GRF down
				if (this->active_sel == nullptr || !this->editable) break;

				int pos = 1; // Start at 1 as we swap the selected newgrf with the next one
				for (GRFConfig **pc = &this->actives; *pc != nullptr; pc = &(*pc)->next, pos++) {
					GRFConfig *c = *pc;
					if (c == this->active_sel) {
						*pc = c->next;
						c->next = c->next->next;
						(*pc)->next = c;
						break;
					}
				}
				this->vscroll->ScrollTowards(pos);
				this->preset = -1;
				this->InvalidateData(GOID_NEWGRF_LIST_EDITED);
				break;
			}

			case WID_NS_FILE_LIST: { // Select an active GRF.
				ResetObjectToPlace();

				uint i = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_NS_FILE_LIST);

				GRFConfig *c;
				for (c = this->actives; c != nullptr && i > 0; c = c->next, i--) {}

				if (this->active_sel != c) {
					CloseWindowByClass(WC_GRF_PARAMETERS);
					CloseWindowByClass(WC_TEXTFILE);
				}
				this->active_sel = c;
				this->avail_sel = nullptr;
				this->avail_pos = -1;

				this->InvalidateData();
				if (click_count == 1) {
					if (this->editable && this->active_sel != nullptr) SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_DRAG, this);
					break;
				}
				/* With double click, continue */
				FALLTHROUGH;
			}

			case WID_NS_REMOVE: { // Remove GRF
				if (this->active_sel == nullptr || !this->editable) break;
				CloseWindowByClass(WC_GRF_PARAMETERS);
				CloseWindowByClass(WC_TEXTFILE);

				/* Choose the next GRF file to be the selected file. */
				GRFConfig *newsel = this->active_sel->next;
				for (GRFConfig **pc = &this->actives; *pc != nullptr; pc = &(*pc)->next) {
					GRFConfig *c = *pc;
					/* If the new selection is empty (i.e. we're deleting the last item
					 * in the list, pick the file just before the selected file */
					if (newsel == nullptr && c->next == this->active_sel) newsel = c;

					if (c == this->active_sel) {
						if (newsel == c) newsel = nullptr;

						*pc = c->next;
						delete c;
						break;
					}
				}

				this->active_sel = newsel;
				this->preset = -1;
				this->avail_pos = -1;
				this->avail_sel = nullptr;
				this->avails.ForceRebuild();
				this->InvalidateData(GOID_NEWGRF_LIST_EDITED);
				break;
			}

			case WID_NS_UPGRADE: { // Upgrade GRF.
				if (!this->editable || this->actives == nullptr) break;
				UpgradeCurrent();
				this->InvalidateData(GOID_NEWGRF_LIST_EDITED);
				break;
			}

			case WID_NS_AVAIL_LIST: { // Select a non-active GRF.
				ResetObjectToPlace();

				auto it = this->vscroll2->GetScrolledItemFromWidget(this->avails, pt.y, this, WID_NS_AVAIL_LIST);
				this->active_sel = nullptr;
				CloseWindowByClass(WC_GRF_PARAMETERS);
				if (it != this->avails.end()) {
					if (this->avail_sel != *it) CloseWindowByClass(WC_TEXTFILE);
					this->avail_sel = *it;
					this->avail_pos = it - this->avails.begin();
				}
				this->InvalidateData();
				if (click_count == 1) {
					if (this->editable && this->avail_sel != nullptr && !HasBit(this->avail_sel->flags, GCF_INVALID)) SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_DRAG, this);
					break;
				}
				/* With double click, continue */
				FALLTHROUGH;
			}

			case WID_NS_ADD:
				if (this->avail_sel == nullptr || !this->editable || HasBit(this->avail_sel->flags, GCF_INVALID)) break;

				this->AddGRFToActive();
				break;

			case WID_NS_APPLY_CHANGES: // Apply changes made to GRF list
				if (!this->editable) break;
				if (this->execute) {
					ShowQuery(
						STR_NEWGRF_POPUP_CAUTION_CAPTION,
						STR_NEWGRF_CONFIRMATION_TEXT,
						this,
						NewGRFConfirmationCallback
					);
				} else {
					CopyGRFConfigList(this->orig_list, this->actives, true);
					ResetGRFConfig(false);
					ReloadNewGRFData();
					this->InvalidateData(GOID_NEWGRF_CHANGES_APPLIED);
				}
				this->CloseChildWindows(WC_QUERY_STRING); // Remove the parameter query window
				break;

			case WID_NS_VIEW_PARAMETERS:
			case WID_NS_SET_PARAMETERS: { // Edit parameters
				if (this->active_sel == nullptr || !this->show_params || this->active_sel->num_valid_params == 0) break;

				OpenGRFParameterWindow(false, this->active_sel, this->editable);
				this->InvalidateData(GOID_NEWGRF_CHANGES_MADE);
				break;
			}

			case WID_NS_TOGGLE_PALETTE:
				if (this->active_sel != nullptr && this->editable) {
					this->active_sel->palette ^= GRFP_USE_MASK;
					this->SetDirty();
					this->InvalidateData(GOID_NEWGRF_CHANGES_MADE);
				}
				break;

			case WID_NS_CONTENT_DOWNLOAD:
			case WID_NS_CONTENT_DOWNLOAD2:
				if (!_network_available) {
					ShowErrorMessage(STR_NETWORK_ERROR_NOTAVAILABLE, INVALID_STRING_ID, WL_ERROR);
				} else {
					this->CloseChildWindows(WC_QUERY_STRING); // Remove the parameter query window

					ShowMissingContentWindow(this->actives);
				}
				break;

			case WID_NS_RESCAN_FILES:
			case WID_NS_RESCAN_FILES2:
				RequestNewGRFScan(this);
				break;
		}
	}

	void OnNewGRFsScanned() override
	{
		if (this->active_sel == nullptr) CloseWindowByClass(WC_TEXTFILE);
		this->avail_sel = nullptr;
		this->avail_pos = -1;
		this->avails.ForceRebuild();
		this->CloseChildWindows(WC_QUERY_STRING); // Remove the parameter query window
	}

	void OnDropdownSelect(WidgetID widget, int index) override
	{
		if (widget != WID_NS_PRESET_LIST) return;
		if (!this->editable) return;

		ClearGRFConfigList(&this->actives);
		this->preset = index;

		if (index != -1) {
			this->actives = LoadGRFPresetFromConfig(this->grf_presets[index].c_str());
		}
		this->avails.ForceRebuild();

		ResetObjectToPlace();
		CloseWindowByClass(WC_GRF_PARAMETERS);
		CloseWindowByClass(WC_TEXTFILE);
		this->active_sel = nullptr;
		this->InvalidateData(GOID_NEWGRF_CHANGES_MADE);
	}

	void OnQueryTextFinished(char *str) override
	{
		if (str == nullptr) return;

		SaveGRFPresetToConfig(str, this->actives);
		this->grf_presets = GetGRFPresetList();

		/* Switch to this preset */
		for (uint i = 0; i < this->grf_presets.size(); i++) {
			if (this->grf_presets[i] == str) {
				this->preset = i;
				break;
			}
		}

		this->InvalidateData();
	}

	/**
	 * Updates the scroll bars for the active and inactive NewGRF lists.
	 */
	void UpdateScrollBars()
	{
		/* Update scrollbars */
		int i = 0;
		for (const GRFConfig *c = this->actives; c != nullptr; c = c->next, i++) {}

		this->vscroll->SetCount(i + 1); // Reserve empty space for drag and drop handling.

		if (this->avail_pos >= 0) this->vscroll2->ScrollTowards(this->avail_pos);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data. @see GameOptionsInvalidationData
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		switch (data) {
			default:
				/* Nothing important to do */
				break;

			case GOID_NEWGRF_RESCANNED:
				/* Search the list for items that are now found and mark them as such. */
				for (GRFConfig **l = &this->actives; *l != nullptr; l = &(*l)->next) {
					GRFConfig *c = *l;
					bool compatible = HasBit(c->flags, GCF_COMPATIBLE);
					if (c->status != GCS_NOT_FOUND && !compatible) continue;

					const GRFConfig *f = FindGRFConfig(c->ident.grfid, FGCM_EXACT, compatible ? &c->original_md5sum : &c->ident.md5sum);
					if (f == nullptr || HasBit(f->flags, GCF_INVALID)) continue;

					*l = new GRFConfig(*f);
					(*l)->next = c->next;

					if (this->active_sel == c) this->active_sel = *l;

					delete c;
				}

				this->avails.ForceRebuild();
				FALLTHROUGH;

			case GOID_NEWGRF_CURRENT_LOADED:
				this->modified = false;
				UpdateScrollBars();
				break;

			case GOID_NEWGRF_LIST_EDITED:
				this->preset = -1;
				FALLTHROUGH;

			case GOID_NEWGRF_CHANGES_MADE:
				UpdateScrollBars();

				/* Changes have been made to the list of active NewGRFs */
				this->modified = true;

				break;

			case GOID_NEWGRF_CHANGES_APPLIED:
				/* No changes have been made to the list of active NewGRFs since the last time the changes got applied */
				this->modified = false;
				break;
		}

		this->BuildAvailables();

		this->SetWidgetDisabledState(WID_NS_APPLY_CHANGES, !((this->editable && this->modified) || _settings_client.gui.newgrf_developer_tools));
		this->SetWidgetsDisabledState(!this->editable,
			WID_NS_PRESET_LIST,
			WID_NS_TOGGLE_PALETTE
		);
		this->SetWidgetDisabledState(WID_NS_ADD, !this->editable || this->avail_sel == nullptr || HasBit(this->avail_sel->flags, GCF_INVALID));
		this->SetWidgetDisabledState(WID_NS_UPGRADE, !this->editable || this->actives == nullptr || !this->CanUpgradeCurrent());

		bool disable_all = this->active_sel == nullptr || !this->editable;
		this->SetWidgetsDisabledState(disable_all,
			WID_NS_REMOVE,
			WID_NS_MOVE_UP,
			WID_NS_MOVE_DOWN
		);

		const GRFConfig *selected_config = (this->avail_sel == nullptr) ? this->active_sel : this->avail_sel;
		for (TextfileType tft = TFT_CONTENT_BEGIN; tft < TFT_CONTENT_END; tft++) {
			this->SetWidgetDisabledState(WID_NS_NEWGRF_TEXTFILE + tft, selected_config == nullptr || !selected_config->GetTextfile(tft).has_value());
		}
		this->SetWidgetDisabledState(WID_NS_OPEN_URL, selected_config == nullptr || StrEmpty(selected_config->GetURL()));

		this->SetWidgetDisabledState(WID_NS_SET_PARAMETERS, !this->show_params || this->active_sel == nullptr || this->active_sel->num_valid_params == 0);
		this->SetWidgetDisabledState(WID_NS_VIEW_PARAMETERS, !this->show_params || this->active_sel == nullptr || this->active_sel->num_valid_params == 0);
		this->SetWidgetDisabledState(WID_NS_TOGGLE_PALETTE, disable_all ||
				(!(_settings_client.gui.newgrf_developer_tools || _settings_client.gui.scenario_developer) && ((selected_config->palette & GRFP_GRF_MASK) != GRFP_GRF_UNSET)));

		if (!disable_all) {
			/* All widgets are now enabled, so disable widgets we can't use */
			if (this->active_sel == this->actives)    this->DisableWidget(WID_NS_MOVE_UP);
			if (this->active_sel->next == nullptr)       this->DisableWidget(WID_NS_MOVE_DOWN);
		}

		this->SetWidgetDisabledState(WID_NS_PRESET_DELETE, this->preset == -1);

		bool has_missing = false;
		bool has_compatible = false;
		for (const GRFConfig *c = this->actives; !has_missing && c != nullptr; c = c->next) {
			has_missing    |= c->status == GCS_NOT_FOUND;
			has_compatible |= HasBit(c->flags, GCF_COMPATIBLE);
		}
		uint32_t widget_data;
		StringID tool_tip;
		if (has_missing || has_compatible) {
			widget_data = STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_BUTTON;
			tool_tip    = STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_TOOLTIP;
		} else {
			widget_data = STR_INTRO_ONLINE_CONTENT;
			tool_tip    = STR_INTRO_TOOLTIP_ONLINE_CONTENT;
		}
		this->GetWidget<NWidgetCore>(WID_NS_CONTENT_DOWNLOAD)->widget_data  = widget_data;
		this->GetWidget<NWidgetCore>(WID_NS_CONTENT_DOWNLOAD)->tool_tip     = tool_tip;
		this->GetWidget<NWidgetCore>(WID_NS_CONTENT_DOWNLOAD2)->widget_data = widget_data;
		this->GetWidget<NWidgetCore>(WID_NS_CONTENT_DOWNLOAD2)->tool_tip    = tool_tip;

		this->SetWidgetDisabledState(WID_NS_PRESET_SAVE, has_missing);
	}

	EventState OnKeyPress([[maybe_unused]] char32_t key, uint16_t keycode) override
	{
		if (!this->editable) return ES_NOT_HANDLED;

		if (this->vscroll2->UpdateListPositionOnKeyPress(this->avail_pos, keycode) == ES_NOT_HANDLED) return ES_NOT_HANDLED;

		if (this->avail_pos >= 0) {
			this->active_sel = nullptr;
			CloseWindowByClass(WC_GRF_PARAMETERS);
			if (this->avail_sel != this->avails[this->avail_pos]) CloseWindowByClass(WC_TEXTFILE);
			this->avail_sel = this->avails[this->avail_pos];
			this->vscroll2->ScrollTowards(this->avail_pos);
			this->InvalidateData(0);
		}

		return ES_HANDLED;
	}

	void OnEditboxChanged(WidgetID widget) override
	{
		if (!this->editable) return;

		if (widget == WID_NS_FILTER) {
			string_filter.SetFilterTerm(this->filter_editbox.text.buf);
			this->avails.SetFilterState(!string_filter.IsEmpty());
			this->avails.ForceRebuild();
			this->InvalidateData(0);
		}
	}

	void OnDragDrop(Point pt, WidgetID widget) override
	{
		if (!this->editable) return;

		if (widget == WID_NS_FILE_LIST) {
			if (this->active_sel != nullptr) {
				/* Get pointer to the selected file in the active list. */
				int from_pos = 0;
				GRFConfig **from_prev;
				for (from_prev = &this->actives; *from_prev != this->active_sel; from_prev = &(*from_prev)->next, from_pos++) {}

				/* Gets the drag-and-drop destination offset. Ignore the last dummy line. */
				int to_pos = std::min(this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_NS_FILE_LIST), this->vscroll->GetCount() - 2);
				if (to_pos != from_pos) { // Don't move NewGRF file over itself.
					/* Get pointer to destination position. */
					GRFConfig **to_prev = &this->actives;
					for (int i = from_pos < to_pos ? -1 : 0; *to_prev != nullptr && i < to_pos; to_prev = &(*to_prev)->next, i++) {}

					/* Detach NewGRF file from its original position. */
					*from_prev = this->active_sel->next;

					/* Attach NewGRF file to its new position. */
					this->active_sel->next = *to_prev;
					*to_prev = this->active_sel;

					this->vscroll->ScrollTowards(to_pos);
					this->preset = -1;
					this->InvalidateData();
				}
			} else if (this->avail_sel != nullptr) {
				int to_pos = std::min(this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_NS_FILE_LIST), this->vscroll->GetCount() - 1);
				this->AddGRFToActive(to_pos);
			}
		} else if (widget == WID_NS_AVAIL_LIST && this->active_sel != nullptr) {
			/* Remove active NewGRF file by dragging it over available list. */
			Point dummy = {-1, -1};
			this->OnClick(dummy, WID_NS_REMOVE, 1);
		}

		ResetObjectToPlace();

		if (this->active_over != -1) {
			/* End of drag-and-drop, hide dragged destination highlight. */
			this->SetWidgetDirty(this->active_over == -2 ? WID_NS_AVAIL_LIST : WID_NS_FILE_LIST);
			this->active_over = -1;
		}
	}

	void OnMouseDrag(Point pt, WidgetID widget) override
	{
		if (!this->editable) return;

		if (widget == WID_NS_FILE_LIST && (this->active_sel != nullptr || this->avail_sel != nullptr)) {
			/* An NewGRF file is dragged over the active list. */
			int to_pos = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_NS_FILE_LIST);
			/* Skip the last dummy line if the source is from the active list. */
			to_pos = std::min(to_pos, this->vscroll->GetCount() - (this->active_sel != nullptr ? 2 : 1));

			if (to_pos != this->active_over) {
				this->active_over = to_pos;
				this->SetWidgetDirty(WID_NS_FILE_LIST);
			}
		} else if (widget == WID_NS_AVAIL_LIST && this->active_sel != nullptr) {
			this->active_over = -2;
			this->SetWidgetDirty(WID_NS_AVAIL_LIST);
		} else if (this->active_over != -1) {
			this->SetWidgetDirty(this->active_over == -2 ? WID_NS_AVAIL_LIST : WID_NS_FILE_LIST);
			this->active_over = -1;
		}
	}

private:
	/** Sort grfs by name. */
	static bool NameSorter(const GRFConfig * const &a, const GRFConfig * const &b)
	{
		int i = StrNaturalCompare(a->GetName(), b->GetName(), true); // Sort by name (natural sorting).
		if (i != 0) return i < 0;

		i = a->version - b->version;
		if (i != 0) return i < 0;

		return a->ident.md5sum < b->ident.md5sum;
	}

	/** Filter grfs by tags/name */
	static bool CDECL TagNameFilter(const GRFConfig * const *a, StringFilter &filter)
	{
		filter.ResetState();
		filter.AddLine((*a)->GetName());
		filter.AddLine((*a)->filename);
		filter.AddLine((*a)->GetDescription());
		return filter.GetState();;
	}

	void BuildAvailables()
	{
		if (!this->avails.NeedRebuild()) return;

		this->avails.clear();

		for (const GRFConfig *c = _all_grfs; c != nullptr; c = c->next) {
			bool found = false;
			for (const GRFConfig *grf = this->actives; grf != nullptr && !found; grf = grf->next) found = grf->ident.HasGrfIdentifier(c->ident.grfid, &c->ident.md5sum);
			if (found) continue;

			if (_settings_client.gui.newgrf_show_old_versions) {
				this->avails.push_back(c);
			} else {
				const GRFConfig *best = FindGRFConfig(c->ident.grfid, HasBit(c->flags, GCF_INVALID) ? FGCM_NEWEST : FGCM_NEWEST_VALID);
				/* Never triggers; FindGRFConfig returns either c, or a newer version of c. */
				assert(best != nullptr);

				/*
				 * If the best version is 0, then all NewGRF with this GRF ID
				 * have version 0, so for backward compatibility reasons we
				 * want to show them all.
				 * If we are the best version, then we definitely want to
				 * show that NewGRF!.
				 */
				if (best->version == 0 || best->ident.HasGrfIdentifier(c->ident.grfid, &c->ident.md5sum)) {
					this->avails.push_back(c);
				}
			}
		}

		this->avails.Filter(this->string_filter);
		this->avails.shrink_to_fit();
		this->avails.RebuildDone();
		this->avails.Sort();

		if (this->avail_sel != nullptr) {
			this->avail_pos = find_index(this->avails, this->avail_sel);
			if (this->avail_pos == -1) {
				this->avail_sel = nullptr;
			}
		}

		this->vscroll2->SetCount(this->avails.size()); // Update the scrollbar
	}

	/**
	 * Insert a GRF into the active list.
	 * @param ins_pos Insert GRF at this position.
	 * @return True if the GRF was successfully added.
	 */
	bool AddGRFToActive(int ins_pos = -1)
	{
		if (this->avail_sel == nullptr || !this->editable || HasBit(this->avail_sel->flags, GCF_INVALID)) return false;

		CloseWindowByClass(WC_TEXTFILE);

		uint count = 0;
		GRFConfig **entry = nullptr;
		GRFConfig **list;
		/* Find last entry in the list, checking for duplicate grfid on the way */
		for (list = &this->actives; *list != nullptr; list = &(*list)->next, ins_pos--) {
			if (ins_pos == 0) entry = list; // Insert position? Save.
			if ((*list)->ident.grfid == this->avail_sel->ident.grfid) {
				ShowErrorMessage(STR_NEWGRF_DUPLICATE_GRFID, INVALID_STRING_ID, WL_INFO);
				return false;
			}
			if (!HasBit((*list)->flags, GCF_STATIC)) count++;
		}
		if (entry == nullptr) entry = list;
		if (count >= NETWORK_MAX_GRF_COUNT) {
			ShowErrorMessage(STR_NEWGRF_TOO_MANY_NEWGRFS, INVALID_STRING_ID, WL_INFO);
			return false;
		}

		GRFConfig *c = new GRFConfig(*this->avail_sel); // Copy GRF details from scanned list.
		c->SetParameterDefaults();

		/* Insert GRF config to configuration list. */
		c->next = *entry;
		*entry = c;

		/* Select next (or previous, if last one) item in the list. */
		int new_pos = this->avail_pos + 1;
		if (new_pos >= (int)this->avails.size()) new_pos = this->avail_pos - 1;
		this->avail_pos = new_pos;
		if (new_pos >= 0) this->avail_sel = this->avails[new_pos];

		this->avails.ForceRebuild();
		this->InvalidateData(GOID_NEWGRF_LIST_EDITED);
		return true;
	}
};

/**
 * Show the content list window with all missing grfs from the given list.
 * @param list The list of grfs to check for missing / not exactly matching ones.
 */
void ShowMissingContentWindow(const GRFConfig *list)
{
	/* Only show the things in the current list, or everything when nothing's selected */
	ContentVector cv;
	for (const GRFConfig *c = list; c != nullptr; c = c->next) {
		if (c->status != GCS_NOT_FOUND && !HasBit(c->flags, GCF_COMPATIBLE)) continue;

		ContentInfo *ci = new ContentInfo();
		ci->type = CONTENT_TYPE_NEWGRF;
		ci->state = ContentInfo::DOES_NOT_EXIST;
		ci->name = c->GetName();
		ci->unique_id = BSWAP32(c->ident.grfid);
		ci->md5sum = HasBit(c->flags, GCF_COMPATIBLE) ? c->original_md5sum : c->ident.md5sum;
		cv.push_back(ci);
	}
	ShowNetworkContentListWindow(cv.empty() ? nullptr : &cv, CONTENT_TYPE_NEWGRF);
}

Listing NewGRFWindow::last_sorting     = {false, 0};
Filtering NewGRFWindow::last_filtering = {false, 0};

NewGRFWindow::GUIGRFConfigList::SortFunction * const NewGRFWindow::sorter_funcs[] = {
	&NameSorter,
};

NewGRFWindow::GUIGRFConfigList::FilterFunction * const NewGRFWindow::filter_funcs[] = {
	&TagNameFilter,
};

/**
 * Custom nested widget container for the NewGRF gui.
 * Depending on the space in the gui, it uses either
 * - two column mode, put the #acs and the #avs underneath each other and the #inf next to it, or
 * - three column mode, put the #avs, #acs, and #inf each in its own column.
 */
class NWidgetNewGRFDisplay : public NWidgetBase {
public:
	static const uint MAX_EXTRA_INFO_WIDTH;    ///< Maximal additional width given to the panel.
	static const uint MIN_EXTRA_FOR_3_COLUMNS; ///< Minimal additional width needed before switching to 3 columns.

	std::unique_ptr<NWidgetBase> avs; ///< Widget with the available grfs list and buttons.
	std::unique_ptr<NWidgetBase> acs; ///< Widget with the active grfs list and buttons.
	std::unique_ptr<NWidgetBase> inf; ///< Info panel.
	bool editable;    ///< Editable status of the parent NewGRF window (if \c false, drop all widgets that make the window editable).

	NWidgetNewGRFDisplay(std::unique_ptr<NWidgetBase> &&avs, std::unique_ptr<NWidgetBase> &&acs, std::unique_ptr<NWidgetBase> &&inf) : NWidgetBase(NWID_CUSTOM)
		, avs(std::move(avs))
		, acs(std::move(acs))
		, inf(std::move(inf))
		, editable(true) // Temporary setting, 'real' value is set in SetupSmallestSize().
	{
	}

	void SetupSmallestSize(Window *w) override
	{
		/* Copy state flag from the window. */
		assert(dynamic_cast<NewGRFWindow *>(w) != nullptr);
		NewGRFWindow *ngw = (NewGRFWindow *)w;
		this->editable = ngw->editable;

		this->avs->SetupSmallestSize(w);
		this->acs->SetupSmallestSize(w);
		this->inf->SetupSmallestSize(w);

		uint min_avs_width = this->avs->smallest_x + this->avs->padding.Horizontal();
		uint min_acs_width = this->acs->smallest_x + this->acs->padding.Horizontal();
		uint min_inf_width = this->inf->smallest_x + this->inf->padding.Horizontal();

		uint min_avs_height = this->avs->smallest_y + this->avs->padding.Vertical();
		uint min_acs_height = this->acs->smallest_y + this->acs->padding.Vertical();
		uint min_inf_height = this->inf->smallest_y + this->inf->padding.Vertical();

		/* Smallest window is in two column mode. */
		this->smallest_x = std::max(min_avs_width, min_acs_width) + WidgetDimensions::scaled.hsep_wide + min_inf_width;
		this->smallest_y = std::max(min_inf_height, min_acs_height + WidgetDimensions::scaled.vsep_wide + min_avs_height);

		/* Filling. */
		this->fill_x = LeastCommonMultiple(this->avs->fill_x, this->acs->fill_x);
		if (this->inf->fill_x > 0 && (this->fill_x == 0 || this->fill_x > this->inf->fill_x)) this->fill_x = this->inf->fill_x;

		this->fill_y = this->avs->fill_y;
		if (this->acs->fill_y > 0 && (this->fill_y == 0 || this->fill_y > this->acs->fill_y)) this->fill_y = this->acs->fill_y;
		this->fill_y = LeastCommonMultiple(this->fill_y, this->inf->fill_y);

		/* Resizing. */
		this->resize_x = LeastCommonMultiple(this->avs->resize_x, this->acs->resize_x);
		if (this->inf->resize_x > 0 && (this->resize_x == 0 || this->resize_x > this->inf->resize_x)) this->resize_x = this->inf->resize_x;

		this->resize_y = this->avs->resize_y;
		if (this->acs->resize_y > 0 && (this->resize_y == 0 || this->resize_y > this->acs->resize_y)) this->resize_y = this->acs->resize_y;
		this->resize_y = LeastCommonMultiple(this->resize_y, this->inf->resize_y);

		/* Make sure the height suits the 3 column (resp. not-editable) format; the 2 column format can easily fill space between the lists */
		this->smallest_y = ComputeMaxSize(min_acs_height, this->smallest_y + this->resize_y - 1, this->resize_y);
	}

	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override
	{
		this->StoreSizePosition(sizing, x, y, given_width, given_height);

		uint min_avs_width = this->avs->smallest_x + this->avs->padding.Horizontal();
		uint min_acs_width = this->acs->smallest_x + this->acs->padding.Horizontal();
		uint min_inf_width = this->inf->smallest_x + this->inf->padding.Horizontal();

		uint min_list_width = std::max(min_avs_width, min_acs_width); // Smallest width of the lists such that they have equal width (incl padding).
		uint avs_extra_width = min_list_width - min_avs_width;   // Additional width needed for avs to reach min_list_width.
		uint acs_extra_width = min_list_width - min_acs_width;   // Additional width needed for acs to reach min_list_width.

		/* Use 2 or 3 columns? */
		uint min_three_columns = min_avs_width + min_acs_width + min_inf_width + 2 * WidgetDimensions::scaled.hsep_wide;
		uint min_two_columns   = min_list_width + min_inf_width + WidgetDimensions::scaled.hsep_wide;
		bool use_three_columns = this->editable && (min_three_columns + ScaleGUITrad(MIN_EXTRA_FOR_3_COLUMNS) <= given_width);

		/* Info panel is a separate column in both modes. Compute its width first. */
		uint extra_width, inf_width;
		if (use_three_columns) {
			extra_width = given_width - min_three_columns;
			inf_width = std::min<uint>(ScaleGUITrad(MAX_EXTRA_INFO_WIDTH), extra_width / 2);
		} else {
			extra_width = given_width - min_two_columns;
			inf_width = std::min<uint>(ScaleGUITrad(MAX_EXTRA_INFO_WIDTH), extra_width / 2);
		}
		inf_width = ComputeMaxSize(this->inf->smallest_x, this->inf->smallest_x + inf_width, this->inf->GetHorizontalStepSize(sizing));
		extra_width -= inf_width - this->inf->smallest_x;

		uint inf_height = ComputeMaxSize(this->inf->smallest_y, given_height, this->inf->GetVerticalStepSize(sizing));

		if (use_three_columns) {
			/* Three column display, first make both lists equally wide, then divide whatever is left between both lists.
			 * Only keep track of what avs gets, all other space goes to acs. */
			uint avs_width = std::min(avs_extra_width, extra_width);
			extra_width -= avs_width;
			extra_width -= std::min(acs_extra_width, extra_width);
			avs_width += extra_width / 2;

			avs_width = ComputeMaxSize(this->avs->smallest_x, this->avs->smallest_x + avs_width, this->avs->GetHorizontalStepSize(sizing));

			uint acs_width = given_width - // Remaining space, including horizontal padding.
					inf_width - this->inf->padding.Horizontal() -
					avs_width - this->avs->padding.Horizontal() - 2 * WidgetDimensions::scaled.hsep_wide;
			acs_width = ComputeMaxSize(min_acs_width, acs_width, this->acs->GetHorizontalStepSize(sizing)) -
					this->acs->padding.Horizontal();

			/* Never use fill_y on these; the minimal size is chosen, so that the 3 column view looks nice */
			uint avs_height = ComputeMaxSize(this->avs->smallest_y, given_height, this->avs->resize_y);
			uint acs_height = ComputeMaxSize(this->acs->smallest_y, given_height, this->acs->resize_y);

			/* Assign size and position to the children. */
			if (rtl) {
				x += this->inf->padding.left;
				this->inf->AssignSizePosition(sizing, x, y + this->inf->padding.top, inf_width, inf_height, rtl);
				x += inf_width + this->inf->padding.right + WidgetDimensions::scaled.hsep_wide;
			} else {
				x += this->avs->padding.left;
				this->avs->AssignSizePosition(sizing, x, y + this->avs->padding.top, avs_width, avs_height, rtl);
				x += avs_width + this->avs->padding.right + WidgetDimensions::scaled.hsep_wide;
			}

			x += this->acs->padding.left;
			this->acs->AssignSizePosition(sizing, x, y + this->acs->padding.top, acs_width, acs_height, rtl);
			x += acs_width + this->acs->padding.right + WidgetDimensions::scaled.hsep_wide;

			if (rtl) {
				x += this->avs->padding.left;
				this->avs->AssignSizePosition(sizing, x, y + this->avs->padding.top, avs_width, avs_height, rtl);
			} else {
				x += this->inf->padding.left;
				this->inf->AssignSizePosition(sizing, x, y + this->inf->padding.top, inf_width, inf_height, rtl);
			}
		} else {
			/* Two columns, all space in extra_width goes to both lists. Since the lists are underneath each other,
			 * the column is min_list_width wide at least. */
			uint avs_width = ComputeMaxSize(this->avs->smallest_x, this->avs->smallest_x + avs_extra_width + extra_width,
					this->avs->GetHorizontalStepSize(sizing));
			uint acs_width = ComputeMaxSize(this->acs->smallest_x, this->acs->smallest_x + acs_extra_width + extra_width,
					this->acs->GetHorizontalStepSize(sizing));

			uint min_avs_height = (!this->editable) ? 0 : this->avs->smallest_y + this->avs->padding.Vertical() + WidgetDimensions::scaled.vsep_wide;
			uint min_acs_height = this->acs->smallest_y + this->acs->padding.Vertical();
			uint extra_height = given_height - min_acs_height - min_avs_height;

			/* Never use fill_y on these; instead use WidgetDimensions::scaled.vsep_wide as filler */
			uint avs_height = ComputeMaxSize(this->avs->smallest_y, this->avs->smallest_y + extra_height / 2, this->avs->resize_y);
			if (this->editable) extra_height -= avs_height - this->avs->smallest_y;
			uint acs_height = ComputeMaxSize(this->acs->smallest_y, this->acs->smallest_y + extra_height, this->acs->resize_y);

			/* Assign size and position to the children. */
			if (rtl) {
				x += this->inf->padding.left;
				this->inf->AssignSizePosition(sizing, x, y + this->inf->padding.top, inf_width, inf_height, rtl);
				x += inf_width + this->inf->padding.right + WidgetDimensions::scaled.hsep_wide;

				this->acs->AssignSizePosition(sizing, x + this->acs->padding.left, y + this->acs->padding.top, acs_width, acs_height, rtl);
				if (this->editable) {
					this->avs->AssignSizePosition(sizing, x + this->avs->padding.left, y + given_height - avs_height - this->avs->padding.bottom, avs_width, avs_height, rtl);
				} else {
					this->avs->AssignSizePosition(sizing, 0, 0, this->avs->smallest_x, this->avs->smallest_y, rtl);
				}
			} else {
				this->acs->AssignSizePosition(sizing, x + this->acs->padding.left, y + this->acs->padding.top, acs_width, acs_height, rtl);
				if (this->editable) {
					this->avs->AssignSizePosition(sizing, x + this->avs->padding.left, y + given_height - avs_height - this->avs->padding.bottom, avs_width, avs_height, rtl);
				} else {
					this->avs->AssignSizePosition(sizing, 0, 0, this->avs->smallest_x, this->avs->smallest_y, rtl);
				}
				uint dx = this->acs->current_x + this->acs->padding.Horizontal();
				if (this->editable) {
					dx = std::max(dx, this->avs->current_x + this->avs->padding.Horizontal());
				}
				x += dx + WidgetDimensions::scaled.hsep_wide + this->inf->padding.left;
				this->inf->AssignSizePosition(sizing, x, y + this->inf->padding.top, inf_width, inf_height, rtl);
			}
		}
	}

	void FillWidgetLookup(WidgetLookup &widget_lookup) override
	{
		this->avs->FillWidgetLookup(widget_lookup);
		this->acs->FillWidgetLookup(widget_lookup);
		this->inf->FillWidgetLookup(widget_lookup);
	}

	NWidgetCore *GetWidgetFromPos(int x, int y) override
	{
		if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return nullptr;

		NWidgetCore *nw = (this->editable) ? this->avs->GetWidgetFromPos(x, y) : nullptr;
		if (nw == nullptr) nw = this->acs->GetWidgetFromPos(x, y);
		if (nw == nullptr) nw = this->inf->GetWidgetFromPos(x, y);
		return nw;
	}

	void Draw(const Window *w) override
	{
		if (this->editable) this->avs->Draw(w);
		this->acs->Draw(w);
		this->inf->Draw(w);
	}
};

const uint NWidgetNewGRFDisplay::MAX_EXTRA_INFO_WIDTH    = 150;
const uint NWidgetNewGRFDisplay::MIN_EXTRA_FOR_3_COLUMNS = 50;

static const NWidgetPart _nested_newgrf_actives_widgets[] = {
	NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0),
		/* Left side, presets. */
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXT, COLOUR_MAUVE), SetDataTip(STR_NEWGRF_SETTINGS_SELECT_PRESET, STR_NULL),
						SetPadding(0, WidgetDimensions::unscaled.hsep_wide, 0, 0),
				NWidget(WWT_DROPDOWN, COLOUR_YELLOW, WID_NS_PRESET_LIST), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_JUST_STRING1, STR_NEWGRF_SETTINGS_PRESET_LIST_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_PRESET_SAVE), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_NEWGRF_SETTINGS_PRESET_SAVE, STR_NEWGRF_SETTINGS_PRESET_SAVE_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_PRESET_DELETE), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_NEWGRF_SETTINGS_PRESET_DELETE, STR_NEWGRF_SETTINGS_PRESET_DELETE_TOOLTIP),
			EndContainer(),
		EndContainer(),

		NWidget(WWT_FRAME, COLOUR_MAUVE), SetDataTip(STR_NEWGRF_SETTINGS_ACTIVE_LIST, STR_NULL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0),
			/* Left side, active grfs. */
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PANEL, COLOUR_MAUVE),
					NWidget(WWT_INSET, COLOUR_MAUVE, WID_NS_FILE_LIST), SetMinimalSize(100, 1), SetPadding(2),
							SetFill(1, 1), SetResize(1, 1), SetScrollbar(WID_NS_SCROLLBAR), SetDataTip(STR_NULL, STR_NEWGRF_SETTINGS_FILE_TOOLTIP),
					EndContainer(),
				EndContainer(),
				NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_NS_SCROLLBAR),
			EndContainer(),

			/* Buttons. */
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_NS_SHOW_REMOVE),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_REMOVE), SetFill(1, 0), SetResize(1, 0),
							SetDataTip(STR_NEWGRF_SETTINGS_REMOVE, STR_NEWGRF_SETTINGS_REMOVE_TOOLTIP),
					NWidget(NWID_VERTICAL),
						NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_MOVE_UP), SetFill(1, 0), SetResize(1, 0),
								SetDataTip(STR_NEWGRF_SETTINGS_MOVEUP, STR_NEWGRF_SETTINGS_MOVEUP_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_MOVE_DOWN), SetFill(1, 0), SetResize(1, 0),
								SetDataTip(STR_NEWGRF_SETTINGS_MOVEDOWN, STR_NEWGRF_SETTINGS_MOVEDOWN_TOOLTIP),
					EndContainer(),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_UPGRADE), SetFill(1, 0), SetResize(1, 0),
							SetDataTip(STR_NEWGRF_SETTINGS_UPGRADE, STR_NEWGRF_SETTINGS_UPGRADE_TOOLTIP),
				EndContainer(),

				NWidget(NWID_VERTICAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_RESCAN_FILES2), SetFill(1, 0), SetResize(1, 0),
							SetDataTip(STR_NEWGRF_SETTINGS_RESCAN_FILES, STR_NEWGRF_SETTINGS_RESCAN_FILES_TOOLTIP),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_CONTENT_DOWNLOAD2), SetFill(1, 0), SetResize(1, 0),
							SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const NWidgetPart _nested_newgrf_availables_widgets[] = {
	NWidget(WWT_FRAME, COLOUR_MAUVE), SetDataTip(STR_NEWGRF_SETTINGS_INACTIVE_LIST, STR_NULL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0),
		/* Left side, available grfs, filter edit box. */
		NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
			NWidget(WWT_TEXT, COLOUR_MAUVE), SetFill(0, 1), SetDataTip(STR_NEWGRF_FILTER_TITLE, STR_NULL),
			NWidget(WWT_EDITBOX, COLOUR_MAUVE, WID_NS_FILTER), SetFill(1, 0), SetMinimalSize(50, 12), SetResize(1, 0),
					SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
		EndContainer(),

		/* Left side, available grfs. */
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_MAUVE),
				NWidget(WWT_INSET, COLOUR_MAUVE, WID_NS_AVAIL_LIST), SetMinimalSize(100, 1), SetPadding(2),
						SetFill(1, 1), SetResize(1, 1), SetScrollbar(WID_NS_SCROLL2BAR),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_NS_SCROLL2BAR),
		EndContainer(),

		/* Left side, available grfs, buttons. */
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_ADD), SetFill(1, 0), SetResize(1, 0),
					SetDataTip(STR_NEWGRF_SETTINGS_ADD, STR_NEWGRF_SETTINGS_ADD_FILE_TOOLTIP),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_RESCAN_FILES), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_NEWGRF_SETTINGS_RESCAN_FILES, STR_NEWGRF_SETTINGS_RESCAN_FILES_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_CONTENT_DOWNLOAD), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const NWidgetPart _nested_newgrf_infopanel_widgets[] = {
	NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0),
		/* Right side, info panel. */
		NWidget(WWT_PANEL, COLOUR_MAUVE),
			NWidget(WWT_EMPTY, COLOUR_MAUVE, WID_NS_NEWGRF_INFO_TITLE), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_EMPTY, COLOUR_MAUVE, WID_NS_NEWGRF_INFO), SetFill(1, 1), SetResize(1, 1), SetMinimalSize(150, 100),
		EndContainer(),

		/* Right side, info buttons. */
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_OPEN_URL), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_CONTENT_OPEN_URL, STR_CONTENT_OPEN_URL_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_NEWGRF_TEXTFILE + TFT_README), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_TEXTFILE_VIEW_README, STR_TEXTFILE_VIEW_README_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_NEWGRF_TEXTFILE + TFT_CHANGELOG), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_TEXTFILE_VIEW_CHANGELOG, STR_TEXTFILE_VIEW_CHANGELOG_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_NEWGRF_TEXTFILE + TFT_LICENSE), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_TEXTFILE_VIEW_LICENCE, STR_TEXTFILE_VIEW_LICENCE_TOOLTIP),
			EndContainer(),
		EndContainer(),

		/* Right side, config buttons. */
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_NS_SHOW_APPLY),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(NWID_VERTICAL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_SET_PARAMETERS), SetFill(1, 0), SetResize(1, 0),
							SetDataTip(STR_NEWGRF_SETTINGS_SET_PARAMETERS, STR_NULL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_TOGGLE_PALETTE), SetFill(1, 0), SetResize(1, 0),
							SetDataTip(STR_NEWGRF_SETTINGS_TOGGLE_PALETTE, STR_NEWGRF_SETTINGS_TOGGLE_PALETTE_TOOLTIP),
				EndContainer(),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_APPLY_CHANGES), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_NEWGRF_SETTINGS_APPLY_CHANGES, STR_NULL),
			EndContainer(),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_NS_VIEW_PARAMETERS), SetFill(1, 0), SetResize(1, 0),
					SetDataTip(STR_NEWGRF_SETTINGS_SHOW_PARAMETERS, STR_NULL),
		EndContainer(),
	EndContainer(),
};

/** Construct nested container widget for managing the lists and the info panel of the NewGRF GUI. */
std::unique_ptr<NWidgetBase> NewGRFDisplay()
{
	std::unique_ptr<NWidgetBase> avs = MakeNWidgets(std::begin(_nested_newgrf_availables_widgets), std::end(_nested_newgrf_availables_widgets), nullptr);
	std::unique_ptr<NWidgetBase> acs = MakeNWidgets(std::begin(_nested_newgrf_actives_widgets), std::end(_nested_newgrf_actives_widgets), nullptr);
	std::unique_ptr<NWidgetBase> inf = MakeNWidgets(std::begin(_nested_newgrf_infopanel_widgets), std::end(_nested_newgrf_infopanel_widgets), nullptr);

	return std::make_unique<NWidgetNewGRFDisplay>(std::move(avs), std::move(acs), std::move(inf));
}

/* Widget definition of the manage newgrfs window */
static const NWidgetPart _nested_newgrf_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE), SetDataTip(STR_NEWGRF_SETTINGS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_MAUVE),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE),
		NWidgetFunction(NewGRFDisplay), SetPadding(WidgetDimensions::unscaled.sparse_resize),
		/* Resize button. */
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_RESIZEBOX, COLOUR_MAUVE), SetDataTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

/* Window definition of the manage newgrfs window */
static WindowDesc _newgrf_desc(__FILE__, __LINE__,
	WDP_CENTER, "settings_newgrf", 300, 263,
	WC_GAME_OPTIONS, WC_NONE,
	0,
	std::begin(_nested_newgrf_widgets), std::end(_nested_newgrf_widgets)
);

/**
 * Callback function for the newgrf 'apply changes' confirmation window
 * @param w Window which is calling this callback
 * @param confirmed boolean value, true when yes was clicked, false otherwise
 */
static void NewGRFConfirmationCallback(Window *w, bool confirmed)
{
	if (confirmed) {
		CloseWindowByClass(WC_GRF_PARAMETERS);
		CloseWindowByClass(WC_TEXTFILE);
		NewGRFWindow *nw = dynamic_cast<NewGRFWindow*>(w);

		_gamelog.StartAction(GLAT_GRF);
		_gamelog.GRFUpdate(_grfconfig, nw->actives); // log GRF changes
		CopyGRFConfigList(nw->orig_list, nw->actives, false);
		ReloadNewGRFData();
		_gamelog.StopAction();

		/* Show new, updated list */
		GRFConfig *c;
		int i = 0;
		for (c = nw->actives; c != nullptr && c != nw->active_sel; c = c->next, i++) {}
		CopyGRFConfigList(&nw->actives, *nw->orig_list, false);
		for (c = nw->actives; c != nullptr && i > 0; c = c->next, i--) {}
		nw->active_sel = c;
		nw->avails.ForceRebuild();
		nw->modified = false;

		w->InvalidateData();

		ReInitAllWindows(false);
		CloseWindowByClass(WC_BUILD_OBJECT);
	}
}



/**
 * Setup the NewGRF gui
 * @param editable allow the user to make changes to the grfconfig in the window
 * @param show_params show information about what parameters are set for the grf files
 * @param exec_changes if changes are made to the list (editable is true), apply these
 *        changes immediately or only update the list
 * @param config pointer to a linked-list of grfconfig's that will be shown
 */
void ShowNewGRFSettings(bool editable, bool show_params, bool exec_changes, GRFConfig **config)
{
	CloseWindowByClass(WC_GAME_OPTIONS);
	new NewGRFWindow(&_newgrf_desc, editable, show_params, exec_changes, config);
}

/** Widget parts of the save preset window. */
static const NWidgetPart _nested_save_preset_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_SAVE_PRESET_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_INSET, COLOUR_GREY, WID_SVP_PRESET_LIST), SetPadding(2, 1, 2, 2),
					SetDataTip(0x0, STR_SAVE_PRESET_LIST_TOOLTIP), SetResize(1, 10), SetScrollbar(WID_SVP_SCROLLBAR), EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_SVP_SCROLLBAR),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(WWT_EDITBOX, COLOUR_GREY, WID_SVP_EDITBOX), SetPadding(2, 2, 2, 2), SetFill(1, 0), SetResize(1, 0),
				SetDataTip(STR_SAVE_PRESET_TITLE, STR_SAVE_PRESET_EDITBOX_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SVP_CANCEL), SetDataTip(STR_SAVE_PRESET_CANCEL, STR_SAVE_PRESET_CANCEL_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SVP_SAVE), SetDataTip(STR_SAVE_PRESET_SAVE, STR_SAVE_PRESET_SAVE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

/** Window description of the preset save window. */
static WindowDesc _save_preset_desc(__FILE__, __LINE__,
	WDP_CENTER, "save_preset", 140, 110,
	WC_SAVE_PRESET, WC_GAME_OPTIONS,
	WDF_MODAL,
	std::begin(_nested_save_preset_widgets), std::end(_nested_save_preset_widgets)
);

/** Class for the save preset window. */
struct SavePresetWindow : public Window {
	QueryString presetname_editbox; ///< Edit box of the save preset.
	StringList presets; ///< Available presets.
	Scrollbar *vscroll; ///< Pointer to the scrollbar widget.
	int selected; ///< Selected entry in the preset list, or \c -1 if none selected.

	/**
	 * Constructor of the save preset window.
	 * @param initial_text Initial text to display in the edit box, or \c nullptr.
	 */
	SavePresetWindow(const char *initial_text) : Window(&_save_preset_desc), presetname_editbox(32)
	{
		this->presets = GetGRFPresetList();
		this->selected = -1;
		if (initial_text != nullptr) {
			for (uint i = 0; i < this->presets.size(); i++) {
				if (this->presets[i] == initial_text) {
					this->selected = i;
					break;
				}
			}
		}

		this->querystrings[WID_SVP_EDITBOX] = &this->presetname_editbox;
		this->presetname_editbox.ok_button = WID_SVP_SAVE;
		this->presetname_editbox.cancel_button = WID_SVP_CANCEL;

		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_SVP_SCROLLBAR);
		this->FinishInitNested(0);

		this->vscroll->SetCount(this->presets.size());
		this->SetFocusedWidget(WID_SVP_EDITBOX);
		if (initial_text != nullptr) this->presetname_editbox.text.Assign(initial_text);
	}

	~SavePresetWindow()
	{
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_SVP_PRESET_LIST: {
				resize->height = GetCharacterHeight(FS_NORMAL);
				size->height = 0;
				for (uint i = 0; i < this->presets.size(); i++) {
					Dimension d = GetStringBoundingBox(this->presets[i]);
					size->width = std::max(size->width, d.width + padding.width);
					resize->height = std::max(resize->height, d.height);
				}
				size->height = ClampU((uint)this->presets.size(), 5, 20) * resize->height + padding.height;
				break;
			}
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_SVP_PRESET_LIST: {
				const Rect br = r.Shrink(WidgetDimensions::scaled.bevel);
				GfxFillRect(br, PC_BLACK);

				uint step_height = this->GetWidget<NWidgetBase>(WID_SVP_PRESET_LIST)->resize_y;
				int offset_y = (step_height - GetCharacterHeight(FS_NORMAL)) / 2;
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				uint min_index = this->vscroll->GetPosition();
				uint max_index = std::min(min_index + this->vscroll->GetCapacity(), (uint)this->presets.size());

				for (uint i = min_index; i < max_index; i++) {
					if ((int)i == this->selected) GfxFillRect(br.left, tr.top, br.right, tr.top + step_height - 1, PC_DARK_BLUE);

					DrawString(tr.left, tr.right, tr.top + offset_y, this->presets[i], ((int)i == this->selected) ? TC_WHITE : TC_SILVER);
					tr.top += step_height;
				}
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_SVP_PRESET_LIST: {
				auto it = this->vscroll->GetScrolledItemFromWidget(this->presets, pt.y, this, WID_SVP_PRESET_LIST);
				if (it != this->presets.end()) {
					this->selected = it - this->presets.begin();
					this->presetname_editbox.text.Assign(*it);
					this->SetWidgetDirty(WID_SVP_PRESET_LIST);
					this->SetWidgetDirty(WID_SVP_EDITBOX);
				}
				break;
			}

			case WID_SVP_CANCEL:
				this->Close();
				break;

			case WID_SVP_SAVE: {
				Window *w = FindWindowById(WC_GAME_OPTIONS, WN_GAME_OPTIONS_NEWGRF_STATE);
				if (w != nullptr && !StrEmpty(this->presetname_editbox.text.buf)) w->OnQueryTextFinished(this->presetname_editbox.text.buf);
				this->Close();
				break;
			}
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_SVP_PRESET_LIST);
	}
};

/**
 * Open the window for saving a preset.
 * @param initial_text Initial text to display in the edit box, or \c nullptr.
 */
static void ShowSavePresetWindow(const char *initial_text)
{
	CloseWindowByClass(WC_SAVE_PRESET);
	new SavePresetWindow(initial_text);
}

/** Widgets for the progress window. */
static const NWidgetPart _nested_scan_progress_widgets[] = {
	NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_NEWGRF_SCAN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_LABEL, INVALID_COLOUR), SetDataTip(STR_NEWGRF_SCAN_MESSAGE, STR_NULL), SetFill(1, 0),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_SP_PROGRESS_BAR), SetFill(1, 0),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_SP_PROGRESS_TEXT), SetFill(1, 0), SetMinimalSize(400, 0),
		EndContainer(),
	EndContainer(),
};

/** Description of the widgets and other settings of the window. */
static WindowDesc _scan_progress_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_MODAL_PROGRESS, WC_NONE,
	0,
	std::begin(_nested_scan_progress_widgets), std::end(_nested_scan_progress_widgets)
);

/** Window for showing the progress of NewGRF scanning. */
struct ScanProgressWindow : public Window {
	std::string last_name; ///< The name of the last 'seen' NewGRF.
	int scanned;           ///< The number of NewGRFs that we have seen.

	/** Create the window. */
	ScanProgressWindow() : Window(&_scan_progress_desc), scanned(0)
	{
		this->InitNested(1);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_SP_PROGRESS_BAR: {
				SetDParamMaxValue(0, 100);
				*size = GetStringBoundingBox(STR_GENERATION_PROGRESS);
				/* We need some spacing for the 'border' */
				size->height += WidgetDimensions::scaled.frametext.Horizontal();
				size->width  += WidgetDimensions::scaled.frametext.Vertical();
				break;
			}

			case WID_SP_PROGRESS_TEXT:
				SetDParamMaxDigits(0, 4);
				SetDParamMaxDigits(1, 4);
				/* We really don't know the width. We could determine it by scanning the NewGRFs,
				 * but this is the status window for scanning them... */
				size->width = std::max<uint>(size->width, GetStringBoundingBox(STR_NEWGRF_SCAN_STATUS).width + padding.width);
				size->height = GetCharacterHeight(FS_NORMAL) * 2 + WidgetDimensions::scaled.vsep_normal;
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_SP_PROGRESS_BAR: {
				/* Draw the % complete with a bar and a text */
				DrawFrameRect(r, COLOUR_GREY, FR_BORDERONLY | FR_LOWERED);
				Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
				uint percent = scanned * 100 / std::max(1U, _settings_client.gui.last_newgrf_count);
				DrawFrameRect(ir.WithWidth(ir.Width() * percent / 100, false), COLOUR_MAUVE, FR_NONE);
				SetDParam(0, percent);
				DrawString(ir.left, ir.right, CenterBounds(ir.top, ir.bottom, GetCharacterHeight(FS_NORMAL)), STR_GENERATION_PROGRESS, TC_FROMSTRING, SA_HOR_CENTER);
				break;
			}

			case WID_SP_PROGRESS_TEXT:
				SetDParam(0, this->scanned);
				SetDParam(1, _settings_client.gui.last_newgrf_count);
				DrawString(r.left, r.right, r.top, STR_NEWGRF_SCAN_STATUS, TC_FROMSTRING, SA_HOR_CENTER);

				DrawString(r.left, r.right, r.top + GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal, this->last_name, TC_BLACK, SA_HOR_CENTER);
				break;
		}
	}

	/**
	 * Update the NewGRF scan status.
	 * @param num  The number of NewGRFs scanned so far.
	 * @param name The name of the last scanned NewGRF.
	 */
	void UpdateNewGRFScanStatus(uint num, const char *name)
	{
		if (name == nullptr) {
			this->last_name = GetString(STR_NEWGRF_SCAN_ARCHIVES);
		} else {
			this->last_name = name;
		}
		this->scanned = num;
		if (num > _settings_client.gui.last_newgrf_count) _settings_client.gui.last_newgrf_count = num;

		this->SetDirty();
	}
};

/**
 * Update the NewGRF scan status.
 * @param num  The number of NewGRFs scanned so far.
 * @param name The name of the last scanned NewGRF.
 */
void UpdateNewGRFScanStatus(uint num, const char *name)
{
	ScanProgressWindow *w  = dynamic_cast<ScanProgressWindow *>(FindWindowByClass(WC_MODAL_PROGRESS));
	if (w == nullptr) w = new ScanProgressWindow();
	w->UpdateNewGRFScanStatus(num, name);
}
