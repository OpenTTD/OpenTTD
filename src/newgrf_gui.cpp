/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_gui.cpp GUI to change NewGRF settings. */

#include "stdafx.h"
#include "gui.h"
#include "newgrf.h"
#include "strings_func.h"
#include "window_func.h"
#include "gamelog.h"
#include "settings_type.h"
#include "settings_func.h"
#include "widgets/dropdown_type.h"
#include "network/network.h"
#include "network/network_content.h"
#include "sortlist_type.h"
#include "querystring_gui.h"
#include "core/geometry_func.hpp"
#include "newgrf_text.h"
#include "fileio_func.h"

#include "table/strings.h"
#include "table/sprites.h"

/**
 * Show the first NewGRF error we can find.
 */
void ShowNewGRFError()
{
	for (const GRFConfig *c = _grfconfig; c != NULL; c = c->next) {
		/* We only want to show fatal errors */
		if (c->error == NULL || c->error->severity != STR_NEWGRF_ERROR_MSG_FATAL) continue;

		SetDParam   (0, c->error->custom_message == NULL ? c->error->message : STR_JUST_RAW_STRING);
		SetDParamStr(1, c->error->custom_message);
		SetDParam   (2, STR_JUST_RAW_STRING);
		SetDParamStr(3, c->filename);
		SetDParam   (4, STR_JUST_RAW_STRING);
		SetDParamStr(5, c->error->data);
		for (uint i = 0; i < c->error->num_params; i++) {
			SetDParam(6 + i, c->error->param_value[i]);
		}
		ShowErrorMessage(STR_NEWGRF_ERROR_FATAL_POPUP, INVALID_STRING_ID, WL_CRITICAL);
		break;
	}
}

static void ShowNewGRFInfo(const GRFConfig *c, uint x, uint y, uint right, uint bottom, bool show_params)
{
	if (c->error != NULL) {
		char message[512];
		SetDParamStr(0, c->error->custom_message); // is skipped by built-in messages
		SetDParam   (1, STR_JUST_RAW_STRING);
		SetDParamStr(2, c->filename);
		SetDParam   (3, STR_JUST_RAW_STRING);
		SetDParamStr(4, c->error->data);
		for (uint i = 0; i < c->error->num_params; i++) {
			SetDParam(5 + i, c->error->param_value[i]);
		}
		GetString(message, c->error->custom_message == NULL ? c->error->message : STR_JUST_RAW_STRING, lastof(message));

		SetDParamStr(0, message);
		y = DrawStringMultiLine(x, right, y, bottom, c->error->severity);
	}

	/* Draw filename or not if it is not known (GRF sent over internet) */
	if (c->filename != NULL) {
		SetDParamStr(0, c->filename);
		y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_FILENAME);
	}

	/* Prepare and draw GRF ID */
	char buff[256];
	snprintf(buff, lengthof(buff), "%08X", BSWAP32(c->ident.grfid));
	SetDParamStr(0, buff);
	y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_GRF_ID);

	if ((_settings_client.gui.newgrf_developer_tools || _settings_client.gui.newgrf_show_old_versions) && c->version != 0) {
		SetDParam(0, c->version);
		y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_VERSION);
	}
	if ((_settings_client.gui.newgrf_developer_tools || _settings_client.gui.newgrf_show_old_versions) && c->min_loadable_version != 0) {
		SetDParam(0, c->min_loadable_version);
		y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_MIN_VERSION);
	}

	/* Prepare and draw MD5 sum */
	md5sumToString(buff, lastof(buff), c->ident.md5sum);
	SetDParamStr(0, buff);
	y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_MD5SUM);

	/* Show GRF parameter list */
	if (show_params) {
		if (c->num_params > 0) {
			GRFBuildParamList(buff, c, lastof(buff));
			SetDParam(0, STR_JUST_RAW_STRING);
			SetDParamStr(1, buff);
		} else {
			SetDParam(0, STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY_NONE);
		}
		y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_PARAMETER);

		/* Draw the palette of the NewGRF */
		SetDParamStr(0, (c->palette & GRFP_USE_WINDOWS) ? "Windows" : "DOS");
		y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_PALETTE);
	}

	/* Show flags */
	if (c->status == GCS_NOT_FOUND)       y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_NOT_FOUND);
	if (c->status == GCS_DISABLED)        y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_DISABLED);
	if (HasBit(c->flags, GCF_INVALID))    y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_INCOMPATIBLE);
	if (HasBit(c->flags, GCF_COMPATIBLE)) y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_COMPATIBLE_LOADED);

	/* Draw GRF info if it exists */
	if (!StrEmpty(c->GetDescription())) {
		SetDParam(0, STR_JUST_RAW_STRING);
		SetDParamStr(1, c->GetDescription());
		y = DrawStringMultiLine(x, right, y, bottom, STR_BLACK_STRING);
	} else {
		y = DrawStringMultiLine(x, right, y, bottom, STR_NEWGRF_SETTINGS_NO_INFO);
	}
}


/** Enum referring to the widgets of the NewGRF parameters window */
enum ShowNewGRFParametersWidgets {
	GRFPAR_WIDGET_SHOW_NUMPAR,      ///< #NWID_SELECTION to optionally display #GRFPAR_WIDGET_NUMPAR
	GRFPAR_WIDGET_NUMPAR_DEC,       ///< Button to decrease number of parameters
	GRFPAR_WIDGET_NUMPAR_INC,       ///< Button to increase number of parameters
	GRFPAR_WIDGET_NUMPAR,           ///< Optional number of parameters
	GRFPAR_WIDGET_NUMPAR_TEXT,      ///< Text description
	GRFPAR_WIDGET_BACKGROUND,       ///< Panel to draw the settings on
	GRFPAR_WIDGET_SCROLLBAR,        ///< Scrollbar to scroll through all settings
	GRFPAR_WIDGET_ACCEPT,           ///< Accept button
	GRFPAR_WIDGET_RESET,            ///< Reset button
	GRFPAR_WIDGET_SHOW_DESCRIPTION, ///< #NWID_SELECTION to optionally display parameter descriptions
	GRFPAR_WIDGET_DESCRIPTION,      ///< Multi-line description of a parameter
};

/**
 * Window for setting the parameters of a NewGRF.
 */
struct NewGRFParametersWindow : public Window {
	static GRFParameterInfo dummy_parameter_info; ///< Dummy info in case a newgrf didn't provide info about some parameter.
	GRFConfig *grf_config; ///< Set the parameters of this GRFConfig.
	uint clicked_button;   ///< The row in which a button was clicked or UINT_MAX.
	bool clicked_increase; ///< True if the increase button was clicked, false for the decrease button.
	int timeout;           ///< How long before we unpress the last-pressed button?
	uint clicked_row;      ///< The selected parameter
	int line_height;       ///< Height of a row in the matrix widget.
	Scrollbar *vscroll;
	bool action14present;  ///< True if action14 information is present.

	NewGRFParametersWindow(const WindowDesc *desc, GRFConfig *c) : Window(),
		grf_config(c),
		clicked_button(UINT_MAX),
		timeout(0),
		clicked_row(UINT_MAX)
	{
		this->action14present = (c->num_valid_params != lengthof(c->param) || c->param_info.Length() != 0);

		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(GRFPAR_WIDGET_SCROLLBAR);
		this->GetWidget<NWidgetStacked>(GRFPAR_WIDGET_SHOW_NUMPAR)->SetDisplayedPlane(this->action14present ? SZSP_HORIZONTAL : 0);
		this->GetWidget<NWidgetStacked>(GRFPAR_WIDGET_SHOW_DESCRIPTION)->SetDisplayedPlane(this->action14present ? 0 : SZSP_HORIZONTAL);
		this->FinishInitNested(desc);  // Initializes 'this->line_height' as side effect.

		this->InvalidateData();
	}

	/**
	 * Get a dummy parameter-info object with default information.
	 * @param nr The param number that should be changed.
	 * @return GRFParameterInfo with dummy information about the given parameter.
	 */
	static GRFParameterInfo *GetDummyParameterInfo(uint nr)
	{
		dummy_parameter_info.param_nr = nr;
		return &dummy_parameter_info;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case GRFPAR_WIDGET_NUMPAR_DEC:
			case GRFPAR_WIDGET_NUMPAR_INC: {
				size->width = size->height = FONT_HEIGHT_NORMAL;
				break;
			}

			case GRFPAR_WIDGET_NUMPAR: {
				SetDParam(0, 999);
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case GRFPAR_WIDGET_BACKGROUND:
				this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;

				resize->width = 1;
				resize->height = this->line_height;
				size->height = GB(this->GetWidget<NWidgetCore>(widget)->widget_data, MAT_ROW_START, MAT_ROW_BITS) * this->line_height;
				break;

			case GRFPAR_WIDGET_DESCRIPTION:
				size->height = max<uint>(size->height, FONT_HEIGHT_NORMAL * 4 + WD_TEXTPANEL_TOP + WD_TEXTPANEL_BOTTOM);
				break;
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case GRFPAR_WIDGET_NUMPAR:
				SetDParam(0, this->vscroll->GetCount());
				break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget == GRFPAR_WIDGET_DESCRIPTION) {
			const GRFParameterInfo *par_info = (this->clicked_row < this->grf_config->param_info.Length()) ? this->grf_config->param_info[this->clicked_row] : NULL;
			if (par_info == NULL) return;
			const char *desc = GetGRFStringFromGRFText(par_info->desc);
			if (desc == NULL) return;
			DrawStringMultiLine(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_TEXTPANEL_TOP, r.bottom - WD_TEXTPANEL_BOTTOM, desc, TC_BLACK);
			return;
		} else if (widget != GRFPAR_WIDGET_BACKGROUND) {
			return;
		}

		bool rtl = _current_text_dir == TD_RTL;
		uint buttons_left = rtl ? r.right - 23 : r.left + 4;
		uint text_left    = r.left + (rtl ? WD_FRAMERECT_LEFT : 28);
		uint text_right   = r.right - (rtl ? 28 : WD_FRAMERECT_RIGHT);

		int y = r.top;
		for (uint i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && i < this->vscroll->GetCount(); i++) {
			GRFParameterInfo *par_info = (i < this->grf_config->param_info.Length()) ? this->grf_config->param_info[i] : NULL;
			if (par_info == NULL) par_info = GetDummyParameterInfo(i);
			uint32 current_value = par_info->GetValue(this->grf_config);
			bool selected = (i == this->clicked_row);

			if (par_info->type == PTYPE_BOOL) {
				DrawFrameRect(buttons_left, y  + 2, buttons_left + 19, y + 10, (current_value != 0) ? COLOUR_GREEN : COLOUR_RED, (current_value != 0) ? FR_LOWERED : FR_NONE);
				SetDParam(2, par_info->GetValue(this->grf_config) == 0 ? STR_CONFIG_SETTING_OFF : STR_CONFIG_SETTING_ON);
			} else if (par_info->type == PTYPE_UINT_ENUM) {
				DrawArrowButtons(buttons_left, y + 2, COLOUR_YELLOW, (this->clicked_button == i) ? 1 + (this->clicked_increase != rtl) : 0, current_value > par_info->min_value, current_value < par_info->max_value);
				SetDParam(2, STR_JUST_INT);
				SetDParam(3, current_value);
				if (par_info->value_names.Contains(current_value)) {
					const char *label = GetGRFStringFromGRFText(par_info->value_names.Find(current_value)->second);
					if (label != NULL) {
						SetDParam(2, STR_JUST_RAW_STRING);
						SetDParamStr(3, label);
					}
				}
			}

			const char *name = GetGRFStringFromGRFText(par_info->name);
			if (name != NULL) {
				SetDParam(0, STR_JUST_RAW_STRING);
				SetDParamStr(1, name);
			} else {
				SetDParam(0, STR_NEWGRF_PARAMETERS_DEFAULT_NAME);
				SetDParam(1, i + 1);
			}

			DrawString(text_left, text_right, y + WD_MATRIX_TOP, STR_NEWGRF_PARAMETERS_SETTING, selected ? TC_WHITE : TC_LIGHT_BLUE);
			y += this->line_height;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case GRFPAR_WIDGET_NUMPAR_DEC:
				if (!this->action14present && this->grf_config->num_params > 0) {
					this->grf_config->num_params--;
					this->InvalidateData();
					SetWindowClassesDirty(WC_GAME_OPTIONS); // Is always the newgrf window
				}
				break;

			case GRFPAR_WIDGET_NUMPAR_INC: {
				GRFConfig *c = this->grf_config;
				if (!this->action14present && c->num_params < c->num_valid_params) {
					c->param[c->num_params++] = 0;
					this->InvalidateData();
					SetWindowClassesDirty(WC_GAME_OPTIONS); // Is always the newgrf window
				}
				break;
			}

			case GRFPAR_WIDGET_BACKGROUND: {
				uint num = this->vscroll->GetScrolledRowFromWidget(pt.y, this, GRFPAR_WIDGET_BACKGROUND);
				if (num >= this->vscroll->GetCount()) break;
				if (this->clicked_row != num) {
					DeleteChildWindows(WC_QUERY_STRING);
					this->clicked_row = num;
				}

				const NWidgetBase *wid = this->GetWidget<NWidgetBase>(GRFPAR_WIDGET_BACKGROUND);
				int x = pt.x - wid->pos_x;
				if (_current_text_dir == TD_RTL) x = wid->current_x - x;
				x -= 4;

				GRFParameterInfo *par_info = (num < this->grf_config->param_info.Length()) ? this->grf_config->param_info[num] : NULL;
				if (par_info == NULL) par_info = GetDummyParameterInfo(num);

				/* One of the arrows is clicked */
				if (IsInsideMM(x, 0, 21)) {
					uint32 val = par_info->GetValue(this->grf_config);
					if (par_info->type == PTYPE_BOOL) {
						val = !val;
					} else {
						if (x >= 10) {
							/* Increase button clicked */
							if (val < par_info->max_value) val++;
							this->clicked_increase = true;
						} else {
							/* Decrease button clicked */
							if (val > par_info->min_value) val--;
							this->clicked_increase = false;
						}
					}
					par_info->SetValue(this->grf_config, val);

					this->clicked_button = num;
					this->timeout = 5;
				} else if (par_info->type == PTYPE_UINT_ENUM && click_count >= 2) {
					/* Display a query box so users can enter a custom value. */
					SetDParam(0, this->grf_config->param[num]);
					ShowQueryString(STR_JUST_INT, STR_CONFIG_SETTING_QUERY_CAPTION, 10, 100, this, CS_NUMERAL, QSF_NONE);
				}

				this->SetDirty();
				break;
			}

			case GRFPAR_WIDGET_RESET:
				this->grf_config->SetParameterDefaults();
				this->InvalidateData();
				SetWindowClassesDirty(WC_GAME_OPTIONS); // Is always the newgrf window
				break;

			case GRFPAR_WIDGET_ACCEPT:
				delete this;
				break;
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (StrEmpty(str)) return;
		int32 value = atoi(str);
		GRFParameterInfo *par_info = ((uint)this->clicked_row < this->grf_config->param_info.Length()) ? this->grf_config->param_info[this->clicked_row] : NULL;
		if (par_info == NULL) par_info = GetDummyParameterInfo(this->clicked_row);
		uint32 val = Clamp<uint32>(value, par_info->min_value, par_info->max_value);
		par_info->SetValue(this->grf_config, val);
		this->SetDirty();
	}

	virtual void OnResize()
	{
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(GRFPAR_WIDGET_BACKGROUND);
		this->vscroll->SetCapacity(nwi->current_y / this->line_height);
		nwi->widget_data = (this->vscroll->GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}

	virtual void OnInvalidateData(int data)
	{
		if (!this->action14present) {
			this->SetWidgetDisabledState(GRFPAR_WIDGET_NUMPAR_DEC, this->grf_config->num_params == 0);
			this->SetWidgetDisabledState(GRFPAR_WIDGET_NUMPAR_INC, this->grf_config->num_params >= this->grf_config->num_valid_params);
		}

		this->vscroll->SetCount(this->action14present ? this->grf_config->num_valid_params : this->grf_config->num_params);
		if (this->clicked_row != UINT_MAX && this->clicked_row >= this->vscroll->GetCount()) {
			this->clicked_row = UINT_MAX;
			DeleteChildWindows(WC_QUERY_STRING);
		}
	}

	virtual void OnTick()
	{
		if (--this->timeout == 0) {
			this->clicked_button = UINT_MAX;
			this->SetDirty();
		}
	}
};
GRFParameterInfo NewGRFParametersWindow::dummy_parameter_info(0);


static const NWidgetPart _nested_newgrf_parameter_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE), SetDataTip(STR_NEWGRF_PARAMETERS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(NWID_SELECTION, INVALID_COLOUR, GRFPAR_WIDGET_SHOW_NUMPAR),
		NWidget(WWT_PANEL, COLOUR_MAUVE), SetResize(1, 0), SetFill(1, 0), SetPIP(4, 0, 4),
			NWidget(NWID_HORIZONTAL), SetPIP(4, 0, 4),
				NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, GRFPAR_WIDGET_NUMPAR_DEC), SetMinimalSize(12, 12), SetDataTip(AWV_DECREASE, STR_NULL),
				NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, GRFPAR_WIDGET_NUMPAR_INC), SetMinimalSize(12, 12), SetDataTip(AWV_INCREASE, STR_NULL),
				NWidget(WWT_TEXT, COLOUR_MAUVE, GRFPAR_WIDGET_NUMPAR), SetResize(1, 0), SetFill(1, 0), SetPadding(0, 0, 0, 4), SetDataTip(STR_NEWGRF_PARAMETERS_NUM_PARAM, STR_NULL),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_MAUVE, GRFPAR_WIDGET_BACKGROUND), SetMinimalSize(188, 182), SetResize(1, 1), SetFill(1, 0), SetDataTip(0x501, STR_NULL), SetScrollbar(GRFPAR_WIDGET_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, GRFPAR_WIDGET_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_SELECTION, INVALID_COLOUR, GRFPAR_WIDGET_SHOW_DESCRIPTION),
		NWidget(WWT_PANEL, COLOUR_MAUVE, GRFPAR_WIDGET_DESCRIPTION), SetResize(1, 0), SetFill(1, 0),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, GRFPAR_WIDGET_ACCEPT), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_NEWGRF_PARAMETERS_CLOSE, STR_NULL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, GRFPAR_WIDGET_RESET), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_NEWGRF_PARAMETERS_RESET, STR_NEWGRF_PARAMETERS_RESET_TOOLTIP),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_MAUVE),
	EndContainer(),
};

/* Window definition for the change grf parameters window */
static const WindowDesc _newgrf_parameters_desc(
	WDP_CENTER, 500, 208,
	WC_GRF_PARAMETERS, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_newgrf_parameter_widgets, lengthof(_nested_newgrf_parameter_widgets)
);

void OpenGRFParameterWindow(GRFConfig *c)
{
	DeleteWindowByClass(WC_GRF_PARAMETERS);
	new NewGRFParametersWindow(&_newgrf_parameters_desc, c);
}

static GRFPresetList _grf_preset_list;

class DropDownListPresetItem : public DropDownListItem {
public:
	DropDownListPresetItem(int result) : DropDownListItem(result, false) {}

	virtual ~DropDownListPresetItem() {}

	bool Selectable() const
	{
		return true;
	}

	void Draw(int left, int right, int top, int bottom, bool sel, int bg_colour) const
	{
		DrawString(left + 2, right + 2, top, _grf_preset_list[this->result], sel ? TC_WHITE : TC_BLACK);
	}
};

static void NewGRFConfirmationCallback(Window *w, bool confirmed);

/** Names of the manage newgrfs window widgets. */
enum ShowNewGRFStateWidgets {
	SNGRFS_PRESET_LIST,
	SNGRFS_PRESET_SAVE,
	SNGRFS_PRESET_DELETE,
	SNGRFS_ADD,
	SNGRFS_REMOVE,
	SNGRFS_MOVE_UP,
	SNGRFS_MOVE_DOWN,
	SNGRFS_FILTER,
	SNGRFS_FILE_LIST,
	SNGRFS_SCROLLBAR,
	SNGRFS_AVAIL_LIST,
	SNGRFS_SCROLL2BAR,
	SNGRFS_NEWGRF_INFO_TITLE,
	SNGRFS_NEWGRF_INFO,
	SNGRFS_SET_PARAMETERS,
	SNGRFS_TOGGLE_PALETTE,
	SNGRFS_APPLY_CHANGES,
	SNGRFS_RESCAN_FILES,
	SNGRFS_RESCAN_FILES2,
	SNGRFS_CONTENT_DOWNLOAD,
	SNGRFS_CONTENT_DOWNLOAD2,
	SNGRFS_SHOW_REMOVE, ///< Select active list buttons (0 = normal, 1 = simple layout).
	SNGRFS_SHOW_APPLY,  ///< Select display of the buttons below the 'details'.
};

/**
 * Window for showing NewGRF files
 */
struct NewGRFWindow : public QueryStringBaseWindow {
	typedef GUIList<const GRFConfig *> GUIGRFConfigList;

	static const uint EDITBOX_MAX_SIZE   =  50;
	static const uint EDITBOX_MAX_LENGTH = 300;

	static Listing   last_sorting;   ///< Default sorting of #GUIGRFConfigList.
	static Filtering last_filtering; ///< Default filtering of #GUIGRFConfigList.
	static GUIGRFConfigList::SortFunction   * const sorter_funcs[]; ///< Sort functions of the #GUIGRFConfigList.
	static GUIGRFConfigList::FilterFunction * const filter_funcs[]; ///< Filter functions of the #GUIGRFConfigList.

	GUIGRFConfigList avails;    ///< Available (non-active) grfs.
	const GRFConfig *avail_sel; ///< Currently selected available grf. \c NULL is none is selected.
	int avail_pos;              ///< Index of #avail_sel if existing, else \c -1.

	GRFConfig *actives;         ///< Temporary active grf list to which changes are made.
	GRFConfig *active_sel;      ///< Selected active grf item.

	GRFConfig **orig_list;      ///< List active grfs in the game. Used as initial value, may be updated by the window.
	bool editable;              ///< Is the window editable?
	bool show_params;           ///< Are the grf-parameters shown in the info-panel?
	bool execute;               ///< On pressing 'apply changes' are grf changes applied immediately, or only list is updated.
	int preset;                 ///< Selected preset.

	Scrollbar *vscroll;
	Scrollbar *vscroll2;

	NewGRFWindow(const WindowDesc *desc, bool editable, bool show_params, bool execute, GRFConfig **orig_list) : QueryStringBaseWindow(EDITBOX_MAX_SIZE)
	{
		this->avail_sel   = NULL;
		this->avail_pos   = -1;
		this->active_sel  = NULL;
		this->actives     = NULL;
		this->orig_list   = orig_list;
		this->editable    = editable;
		this->execute     = execute;
		this->show_params = show_params;
		this->preset      = -1;

		CopyGRFConfigList(&this->actives, *orig_list, false);
		GetGRFPresetList(&_grf_preset_list);

		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(SNGRFS_SCROLLBAR);
		this->vscroll2 = this->GetScrollbar(SNGRFS_SCROLL2BAR);

		this->GetWidget<NWidgetStacked>(SNGRFS_SHOW_REMOVE)->SetDisplayedPlane(this->editable ? 0 : 1);
		this->GetWidget<NWidgetStacked>(SNGRFS_SHOW_APPLY)->SetDisplayedPlane(this->editable ? 0 : SZSP_HORIZONTAL);
		this->FinishInitNested(desc);

		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, EDITBOX_MAX_LENGTH);
		this->SetFocusedWidget(SNGRFS_FILTER);

		this->avails.SetListing(this->last_sorting);
		this->avails.SetFiltering(this->last_filtering);
		this->avails.SetSortFuncs(this->sorter_funcs);
		this->avails.SetFilterFuncs(this->filter_funcs);
		this->avails.ForceRebuild();

		this->OnInvalidateData(2);
	}

	~NewGRFWindow()
	{
		DeleteWindowByClass(WC_GRF_PARAMETERS);

		if (this->editable && !this->execute) {
			CopyGRFConfigList(this->orig_list, this->actives, true);
			ResetGRFConfig(false);
			ReloadNewGRFData();
		}

		/* Remove the temporary copy of grf-list used in window */
		ClearGRFConfigList(&this->actives);
		_grf_preset_list.Clear();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case SNGRFS_FILE_LIST:
				resize->height = max(12, FONT_HEIGHT_NORMAL + 2);
				size->height = max(size->height, WD_FRAMERECT_TOP + 6 * resize->height + WD_FRAMERECT_BOTTOM);
				break;

			case SNGRFS_AVAIL_LIST:
				resize->height = max(12, FONT_HEIGHT_NORMAL + 2);
				size->height = max(size->height, WD_FRAMERECT_TOP + 8 * resize->height + WD_FRAMERECT_BOTTOM);
				break;

			case SNGRFS_NEWGRF_INFO_TITLE: {
				Dimension dim = GetStringBoundingBox(STR_NEWGRF_SETTINGS_INFO_TITLE);
				size->height = max(size->height, dim.height + WD_FRAMETEXT_TOP + WD_FRAMETEXT_BOTTOM);
				size->width  = max(size->width,  dim.width  + WD_FRAMETEXT_LEFT + WD_FRAMETEXT_RIGHT);
				break;
			}

			case SNGRFS_NEWGRF_INFO:
				size->height = max(size->height, WD_FRAMERECT_TOP + 10 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_BOTTOM + padding.height + 2);
				break;

			case SNGRFS_PRESET_LIST: {
				Dimension d = GetStringBoundingBox(STR_NUM_CUSTOM);
				for (uint i = 0; i < _grf_preset_list.Length(); i++) {
					if (_grf_preset_list[i] != NULL) {
						SetDParamStr(0, _grf_preset_list[i]);
						d = maxdim(d, GetStringBoundingBox(STR_JUST_RAW_STRING));
					}
				}
				d.width += padding.width;
				*size = maxdim(d, *size);
				break;
			}

			case SNGRFS_CONTENT_DOWNLOAD:
			case SNGRFS_CONTENT_DOWNLOAD2: {
				Dimension d = GetStringBoundingBox(STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_BUTTON);
				*size = maxdim(d, GetStringBoundingBox(STR_INTRO_ONLINE_CONTENT));
				size->width  += padding.width;
				size->height += padding.height;
				break;
			}
		}
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, SNGRFS_FILE_LIST);
		this->vscroll2->SetCapacityFromWidget(this, SNGRFS_AVAIL_LIST);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case SNGRFS_PRESET_LIST:
				if (this->preset == -1) {
					SetDParam(0, STR_NUM_CUSTOM);
				} else {
					SetDParam(0, STR_JUST_RAW_STRING);
					SetDParamStr(1, _grf_preset_list[this->preset]);
				}
				break;
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
		if (this->editable) this->DrawEditBox(SNGRFS_FILTER);
	}

	/**
	 * Pick the palette for the sprite of the grf to display.
	 * @param c grf to display.
	 * @return Palette for the sprite.
	 */
	FORCEINLINE PaletteID GetPalette(const GRFConfig *c) const
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

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case SNGRFS_FILE_LIST: {
				GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, 0xD7);

				uint step_height = this->GetWidget<NWidgetBase>(SNGRFS_FILE_LIST)->resize_y;
				uint y = r.top + WD_FRAMERECT_TOP;
				int sprite_offset_y = (step_height - 10) / 2;
				int offset_y = (step_height - FONT_HEIGHT_NORMAL) / 2;

				bool rtl = _current_text_dir == TD_RTL;
				uint text_left    = rtl ? r.left + WD_FRAMERECT_LEFT : r.left + 25;
				uint text_right   = rtl ? r.right - 25 : r.right - WD_FRAMERECT_RIGHT;
				uint square_left  = rtl ? r.right - 15 : r.left + 5;
				uint warning_left = rtl ? r.right - 30 : r.left + 20;

				int i = 0;
				for (const GRFConfig *c = this->actives; c != NULL; c = c->next, i++) {
					if (this->vscroll->IsVisible(i)) {
						const char *text = c->GetName();
						bool h = (this->active_sel == c);
						PaletteID pal = this->GetPalette(c);

						if (h) GfxFillRect(r.left + 1, y, r.right - 1, y + step_height - 1, 156);
						DrawSprite(SPR_SQUARE, pal, square_left, y + sprite_offset_y);
						if (c->error != NULL) DrawSprite(SPR_WARNING_SIGN, 0, warning_left, y + sprite_offset_y);
						uint txtoffset = c->error == NULL ? 0 : 10;
						DrawString(text_left + (rtl ? 0 : txtoffset), text_right - (rtl ? txtoffset : 0), y + offset_y, text, h ? TC_WHITE : TC_ORANGE);
						y += step_height;
					}
				}
				break;
			}

			case SNGRFS_AVAIL_LIST: {
				GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, 0xD7);

				uint step_height = this->GetWidget<NWidgetBase>(SNGRFS_AVAIL_LIST)->resize_y;
				int offset_y = (step_height - FONT_HEIGHT_NORMAL) / 2;
				uint y = r.top + WD_FRAMERECT_TOP;
				uint min_index = this->vscroll2->GetPosition();
				uint max_index = min(min_index + this->vscroll2->GetCapacity(), this->avails.Length());

				for (uint i = min_index; i < max_index; i++) {
					const GRFConfig *c = this->avails[i];
					bool h = (c == this->avail_sel);
					const char *text = c->GetName();

					if (h) GfxFillRect(r.left + 1, y, r.right - 1, y + step_height - 1, 156);
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y + offset_y, text, h ? TC_WHITE : TC_SILVER);
					y += step_height;
				}
				break;
			}

			case SNGRFS_NEWGRF_INFO_TITLE:
				/* Create the nice grayish rectangle at the details top. */
				GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, 157);
				DrawString(r.left, r.right, (r.top + r.bottom - FONT_HEIGHT_NORMAL) / 2, STR_NEWGRF_SETTINGS_INFO_TITLE, TC_FROMSTRING, SA_HOR_CENTER);
				break;

			case SNGRFS_NEWGRF_INFO: {
				const GRFConfig *selected = this->active_sel;
				if (selected == NULL) selected = this->avail_sel;
				if (selected != NULL) {
					ShowNewGRFInfo(selected, r.left + WD_FRAMERECT_LEFT, r.top + WD_FRAMERECT_TOP, r.right - WD_FRAMERECT_RIGHT, r.bottom - WD_FRAMERECT_BOTTOM, this->show_params);
				}
				break;
			}
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case SNGRFS_PRESET_LIST: {
				DropDownList *list = new DropDownList();

				/* Add 'None' option for clearing list */
				list->push_back(new DropDownListStringItem(STR_NONE, -1, false));

				for (uint i = 0; i < _grf_preset_list.Length(); i++) {
					if (_grf_preset_list[i] != NULL) {
						list->push_back(new DropDownListPresetItem(i));
					}
				}

				this->DeleteChildWindows(WC_QUERY_STRING); // Remove the parameter query window
				ShowDropDownList(this, list, this->preset, SNGRFS_PRESET_LIST);
				break;
			}

			case SNGRFS_PRESET_SAVE:
				ShowQueryString(STR_EMPTY, STR_NEWGRF_SETTINGS_PRESET_SAVE_QUERY, 32, 100, this, CS_ALPHANUMERAL, QSF_NONE);
				break;

			case SNGRFS_PRESET_DELETE:
				if (this->preset == -1) return;

				DeleteGRFPresetFromConfig(_grf_preset_list[this->preset]);
				GetGRFPresetList(&_grf_preset_list);
				this->preset = -1;
				this->InvalidateData();
				this->DeleteChildWindows(WC_QUERY_STRING); // Remove the parameter query window
				break;

			case SNGRFS_MOVE_UP: { // Move GRF up
				if (this->active_sel == NULL || !this->editable) break;

				int pos = 0;
				for (GRFConfig **pc = &this->actives; *pc != NULL; pc = &(*pc)->next, pos++) {
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
				this->InvalidateData();
				break;
			}

			case SNGRFS_MOVE_DOWN: { // Move GRF down
				if (this->active_sel == NULL || !this->editable) break;

				int pos = 1; // Start at 1 as we swap the selected newgrf with the next one
				for (GRFConfig **pc = &this->actives; *pc != NULL; pc = &(*pc)->next, pos++) {
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
				this->InvalidateData();
				break;
			}

			case SNGRFS_FILE_LIST: { // Select an active GRF.
				uint i = this->vscroll->GetScrolledRowFromWidget(pt.y, this, SNGRFS_FILE_LIST);

				GRFConfig *c;
				for (c = this->actives; c != NULL && i > 0; c = c->next, i--) {}

				if (this->active_sel != c) DeleteWindowByClass(WC_GRF_PARAMETERS);
				this->active_sel = c;
				this->avail_sel = NULL;
				this->avail_pos = -1;

				this->InvalidateData();
				if (click_count == 1) break;
				/* FALL THROUGH, with double click. */
			}

			case SNGRFS_REMOVE: { // Remove GRF
				if (this->active_sel == NULL || !this->editable) break;
				DeleteWindowByClass(WC_GRF_PARAMETERS);

				/* Choose the next GRF file to be the selected file. */
				GRFConfig *newsel = this->active_sel->next;
				for (GRFConfig **pc = &this->actives; *pc != NULL; pc = &(*pc)->next) {
					GRFConfig *c = *pc;
					/* If the new selection is empty (i.e. we're deleting the last item
					 * in the list, pick the file just before the selected file */
					if (newsel == NULL && c->next == this->active_sel) newsel = c;

					if (c == this->active_sel) {
						*pc = c->next;
						delete c;
						break;
					}
				}

				this->active_sel = newsel;
				this->preset = -1;
				this->avail_pos = -1;
				this->avail_sel = NULL;
				this->avails.ForceRebuild();
				this->InvalidateData(2);
				break;
			}

			case SNGRFS_AVAIL_LIST: { // Select a non-active GRF.
				uint i = this->vscroll2->GetScrolledRowFromWidget(pt.y, this, SNGRFS_AVAIL_LIST);
				this->active_sel = NULL;
				DeleteWindowByClass(WC_GRF_PARAMETERS);
				if (i < this->avails.Length()) {
					this->avail_sel = this->avails[i];
					this->avail_pos = i;
				}
				this->InvalidateData();
				if (click_count == 1) break;
				/* FALL THROUGH, with double click. */
			}

			case SNGRFS_ADD: {
				if (this->avail_sel == NULL || !this->editable || HasBit(this->avail_sel->flags, GCF_INVALID)) break;

				GRFConfig **list;
				/* Find last entry in the list, checking for duplicate grfid on the way */
				for (list = &this->actives; *list != NULL; list = &(*list)->next) {
					if ((*list)->ident.grfid == this->avail_sel->ident.grfid) {
						ShowErrorMessage(STR_NEWGRF_DUPLICATE_GRFID, INVALID_STRING_ID, WL_INFO);
						return;
					}
				}

				GRFConfig *c = new GRFConfig(*this->avail_sel); // Copy GRF details from scanned list.
				c->SetParameterDefaults();
				*list = c; // Append GRF config to configuration list.

				/* Select next (or previous, if last one) item in the list. */
				int new_pos = this->avail_pos + 1;
				if (new_pos >= (int)this->avails.Length()) new_pos = this->avail_pos - 1;
				this->avail_pos = new_pos;
				if (new_pos >= 0) this->avail_sel = this->avails[new_pos];

				this->avails.ForceRebuild();
				this->InvalidateData(2);
				break;
			}

			case SNGRFS_APPLY_CHANGES: // Apply changes made to GRF list
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
				}
				this->DeleteChildWindows(WC_QUERY_STRING); // Remove the parameter query window
				break;

			case SNGRFS_SET_PARAMETERS: { // Edit parameters
				if (this->active_sel == NULL || !this->editable || !this->show_params) break;

				OpenGRFParameterWindow(this->active_sel);
				break;
			}

			case SNGRFS_TOGGLE_PALETTE:
				if (this->active_sel != NULL || !this->editable) {
					this->active_sel->palette ^= GRFP_USE_MASK;
					this->SetDirty();
				}
				break;

			case SNGRFS_CONTENT_DOWNLOAD:
			case SNGRFS_CONTENT_DOWNLOAD2:
				if (!_network_available) {
					ShowErrorMessage(STR_NETWORK_ERROR_NOTAVAILABLE, INVALID_STRING_ID, WL_ERROR);
				} else {
#if defined(ENABLE_NETWORK)
					this->DeleteChildWindows(WC_QUERY_STRING); // Remove the parameter query window

					/* Only show the things in the current list, or everything when nothing's selected */
					ContentVector cv;
					for (const GRFConfig *c = this->actives; c != NULL; c = c->next) {
						if (c->status != GCS_NOT_FOUND && !HasBit(c->flags, GCF_COMPATIBLE)) continue;

						ContentInfo *ci = new ContentInfo();
						ci->type = CONTENT_TYPE_NEWGRF;
						ci->state = ContentInfo::DOES_NOT_EXIST;
						ttd_strlcpy(ci->name, c->GetName(), lengthof(ci->name));
						ci->unique_id = BSWAP32(c->ident.grfid);
						memcpy(ci->md5sum, HasBit(c->flags, GCF_COMPATIBLE) ? c->original_md5sum : c->ident.md5sum, sizeof(ci->md5sum));
						*cv.Append() = ci;
					}
					ShowNetworkContentListWindow(cv.Length() == 0 ? NULL : &cv, CONTENT_TYPE_NEWGRF);
#endif
				}
				break;

			case SNGRFS_RESCAN_FILES:
			case SNGRFS_RESCAN_FILES2:
				TarScanner::DoScan();
				ScanNewGRFFiles();
				this->avail_sel = NULL;
				this->avail_pos = -1;
				this->avails.ForceRebuild();
				this->InvalidateData(1);
				this->DeleteChildWindows(WC_QUERY_STRING); // Remove the parameter query window
				InvalidateWindowClassesData(WC_SAVELOAD);
				break;
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		if (!this->editable) return;

		ClearGRFConfigList(&this->actives);
		this->preset = index;

		if (index != -1) {
			GRFConfig *c = LoadGRFPresetFromConfig(_grf_preset_list[index]);

			this->active_sel = NULL;
			this->actives = c;
			this->avails.ForceRebuild();
		}

		DeleteWindowByClass(WC_GRF_PARAMETERS);
		this->active_sel = NULL;
		this->InvalidateData(3);
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		SaveGRFPresetToConfig(str, this->actives);
		GetGRFPresetList(&_grf_preset_list);

		/* Switch to this preset */
		for (uint i = 0; i < _grf_preset_list.Length(); i++) {
			if (_grf_preset_list[i] != NULL && strcmp(_grf_preset_list[i], str) == 0) {
				this->preset = i;
				break;
			}
		}

		this->InvalidateData();
	}

	/**
	 * Calback to update internal data.
	 *  - 0: (optionally) build availables, update button status.
	 *  - 1: build availables, Add newly found grfs, update button status.
	 *  - 2: (optionally) build availables, Reset preset, + 3
	 *  - 3: (optionally) build availables, Update active scrollbar, update button status.
	 *  - 4: Force a rebuild of the availables, + 2
	 */
	virtual void OnInvalidateData(int data = 0)
	{
		switch (data) {
			default: NOT_REACHED();
			case 0:
				/* Nothing important to do */
				break;

			case 1:
				/* Search the list for items that are now found and mark them as such. */
				for (GRFConfig **l = &this->actives; *l != NULL; l = &(*l)->next) {
					GRFConfig *c = *l;
					bool compatible = HasBit(c->flags, GCF_COMPATIBLE);
					if (c->status != GCS_NOT_FOUND && !compatible) continue;

					const GRFConfig *f = FindGRFConfig(c->ident.grfid, FGCM_EXACT, compatible ? c->original_md5sum : c->ident.md5sum);
					if (f == NULL || HasBit(f->flags, GCF_INVALID)) continue;

					*l = new GRFConfig(*f);
					(*l)->next = c->next;

					if (active_sel == c) active_sel = *l;

					delete c;
				}
				/* FALL THROUGH */
			case 4:
				this->avails.ForceRebuild();
				/* FALL THROUGH */
			case 2:
				this->preset = -1;
				/* FALL THROUGH */
			case 3: {
				int i = 0;
				for (const GRFConfig *c = this->actives; c != NULL; c = c->next, i++) {}

				this->vscroll->SetCapacityFromWidget(this, SNGRFS_FILE_LIST);
				this->vscroll->SetCount(i);

				this->vscroll2->SetCapacityFromWidget(this, SNGRFS_AVAIL_LIST);
				if (this->avail_pos >= 0) this->vscroll2->ScrollTowards(this->avail_pos);
				break;
			}
		}

		this->BuildAvailables();

		this->SetWidgetsDisabledState(!this->editable,
			SNGRFS_PRESET_LIST,
			SNGRFS_APPLY_CHANGES,
			SNGRFS_TOGGLE_PALETTE,
			WIDGET_LIST_END
		);
		this->SetWidgetDisabledState(SNGRFS_ADD, !this->editable || this->avail_sel == NULL || HasBit(this->avail_sel->flags, GCF_INVALID));

		bool disable_all = this->active_sel == NULL || !this->editable;
		this->SetWidgetsDisabledState(disable_all,
			SNGRFS_REMOVE,
			SNGRFS_MOVE_UP,
			SNGRFS_MOVE_DOWN,
			WIDGET_LIST_END
		);
		this->SetWidgetDisabledState(SNGRFS_SET_PARAMETERS, !this->show_params || disable_all);
		this->SetWidgetDisabledState(SNGRFS_TOGGLE_PALETTE, disable_all);

		if (!disable_all) {
			/* All widgets are now enabled, so disable widgets we can't use */
			if (this->active_sel == this->actives)    this->DisableWidget(SNGRFS_MOVE_UP);
			if (this->active_sel->next == NULL)       this->DisableWidget(SNGRFS_MOVE_DOWN);
			if (this->active_sel->IsOpenTTDBaseGRF()) this->DisableWidget(SNGRFS_REMOVE);
		}

		this->SetWidgetDisabledState(SNGRFS_PRESET_DELETE, this->preset == -1);

		bool has_missing = false;
		bool has_compatible = false;
		for (const GRFConfig *c = this->actives; !has_missing && c != NULL; c = c->next) {
			has_missing    |= c->status == GCS_NOT_FOUND;
			has_compatible |= HasBit(c->flags, GCF_COMPATIBLE);
		}
		uint16 widget_data;
		StringID tool_tip;
		if (has_missing || has_compatible) {
			widget_data = STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_BUTTON;
			tool_tip    = STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_TOOLTIP;
		} else {
			widget_data = STR_INTRO_ONLINE_CONTENT;
			tool_tip    = STR_INTRO_TOOLTIP_ONLINE_CONTENT;
		}
		this->GetWidget<NWidgetCore>(SNGRFS_CONTENT_DOWNLOAD)->widget_data  = widget_data;
		this->GetWidget<NWidgetCore>(SNGRFS_CONTENT_DOWNLOAD)->tool_tip     = tool_tip;
		this->GetWidget<NWidgetCore>(SNGRFS_CONTENT_DOWNLOAD2)->widget_data = widget_data;
		this->GetWidget<NWidgetCore>(SNGRFS_CONTENT_DOWNLOAD2)->tool_tip    = tool_tip;

		this->SetWidgetDisabledState(SNGRFS_PRESET_SAVE, has_missing);
	}

	virtual void OnMouseLoop()
	{
		if (this->editable) this->HandleEditBox(SNGRFS_FILTER);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		if (!this->editable) return ES_NOT_HANDLED;

		switch (keycode) {
			case WKC_UP:
				/* scroll up by one */
				if (this->avail_pos > 0) this->avail_pos--;
				break;

			case WKC_DOWN:
				/* scroll down by one */
				if (this->avail_pos < (int)this->avails.Length() - 1) this->avail_pos++;
				break;

			case WKC_PAGEUP:
				/* scroll up a page */
				this->avail_pos = (this->avail_pos < this->vscroll2->GetCapacity()) ? 0 : this->avail_pos - this->vscroll2->GetCapacity();
				break;

			case WKC_PAGEDOWN:
				/* scroll down a page */
				this->avail_pos = min(this->avail_pos + this->vscroll2->GetCapacity(), (int)this->avails.Length() - 1);
				break;

			case WKC_HOME:
				/* jump to beginning */
				this->avail_pos = 0;
				break;

			case WKC_END:
				/* jump to end */
				this->avail_pos = this->avails.Length() - 1;
				break;

			default: {
				/* Handle editbox input */
				EventState state = ES_NOT_HANDLED;
				if (this->HandleEditBoxKey(SNGRFS_FILTER, key, keycode, state) == HEBR_EDITING) {
					this->OnOSKInput(SNGRFS_FILTER);
				}
				return state;
			}
		}

		if (this->avails.Length() == 0) this->avail_pos = -1;
		if (this->avail_pos >= 0) {
			this->avail_sel = this->avails[this->avail_pos];
			this->vscroll2->ScrollTowards(this->avail_pos);
			this->InvalidateData(0);
		}

		return ES_HANDLED;
	}

	virtual void OnOSKInput(int wid)
	{
		if (!this->editable) return;

		this->avails.SetFilterState(!StrEmpty(this->edit_str_buf));
		this->avails.ForceRebuild();
		this->InvalidateData(0);
	}

private:
	/** Sort grfs by name. */
	static int CDECL NameSorter(const GRFConfig * const *a, const GRFConfig * const *b)
	{
		int i = strnatcmp((*a)->GetName(), (*b)->GetName()); // Sort by name (natural sorting).
		if (i != 0) return i;

		i = (*a)->version - (*b)->version;
		if (i != 0) return i;

		return memcmp((*a)->ident.md5sum, (*b)->ident.md5sum, lengthof((*b)->ident.md5sum));
	}

	/** Filter grfs by tags/name */
	static bool CDECL TagNameFilter(const GRFConfig * const *a, const char *filter_string)
	{
		if (strcasestr((*a)->GetName(), filter_string) != NULL) return true;
		if ((*a)->filename != NULL && strcasestr((*a)->filename, filter_string) != NULL) return true;
		if ((*a)->GetDescription() != NULL && strcasestr((*a)->GetDescription(), filter_string) != NULL) return true;
		return false;
	}

	void BuildAvailables()
	{
		if (!this->avails.NeedRebuild()) return;

		this->avails.Clear();

		for (const GRFConfig *c = _all_grfs; c != NULL; c = c->next) {
			bool found = false;
			for (const GRFConfig *grf = this->actives; grf != NULL && !found; grf = grf->next) found = grf->ident.HasGrfIdentifier(c->ident.grfid, c->ident.md5sum);
			if (found) continue;

			if (_settings_client.gui.newgrf_show_old_versions) {
				*this->avails.Append() = c;
			} else {
				const GRFConfig *best = FindGRFConfig(c->ident.grfid, HasBit(c->flags, GCF_INVALID) ? FGCM_NEWEST : FGCM_NEWEST_VALID);
				/*
				 * If the best version is 0, then all NewGRF with this GRF ID
				 * have version 0, so for backward compatability reasons we
				 * want to show them all.
				 * If we are the best version, then we definitely want to
				 * show that NewGRF!.
				 */
				if (best->version == 0 || best->ident.HasGrfIdentifier(c->ident.grfid, c->ident.md5sum)) {
					*this->avails.Append() = c;
				}
			}
		}

		this->avails.Filter(this->edit_str_buf);
		this->avails.Compact();
		this->avails.RebuildDone();
		this->avails.Sort();

		if (this->avail_sel != NULL) {
			this->avail_pos = this->avails.FindIndex(this->avail_sel);
			if (this->avail_pos < 0) this->avail_sel = NULL;
		}

		this->vscroll2->SetCount(this->avails.Length()); // Update the scrollbar
	}
};

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
 * - two column mode, put the #acs and the #avs underneath each other and the #info next to it, or
 * - three column mode, put the #avs, #acs, and #info each in its own column.
 */
class NWidgetNewGRFDisplay : public NWidgetContainer {
public:
	static const uint INTER_LIST_SPACING;      ///< Empty vertical space between both lists in the 2 column mode.
	static const uint INTER_COLUMN_SPACING;    ///< Empty horizontal space between two columns.
	static const uint MAX_EXTRA_INFO_WIDTH;    ///< Maximal additional width given to the panel.
	static const uint MIN_EXTRA_FOR_3_COLUMNS; ///< Minimal additional width needed before switching to 3 columns.

	NWidgetBase *avs; ///< Widget with the available grfs list and buttons.
	NWidgetBase *acs; ///< Widget with the active grfs list and buttons.
	NWidgetBase *inf; ///< Info panel.
	bool editable;    ///< Editable status of the parent NewGRF window (if \c false, drop all widgets that make the window editable).

	NWidgetNewGRFDisplay(NWidgetBase *avs, NWidgetBase *acs, NWidgetBase *inf) : NWidgetContainer(NWID_HORIZONTAL)
	{
		this->avs = avs;
		this->acs = acs;
		this->inf = inf;

		this->Add(this->avs);
		this->Add(this->acs);
		this->Add(this->inf);

		this->editable = true; // Temporary setting, 'real' value is set in SetupSmallestSize().
	}

	virtual void SetupSmallestSize(Window *w, bool init_array)
	{
		/* Copy state flag from the window. */
		assert(dynamic_cast<NewGRFWindow *>(w) != NULL);
		NewGRFWindow *ngw = (NewGRFWindow *)w;
		this->editable = ngw->editable;

		this->avs->SetupSmallestSize(w, init_array);
		this->acs->SetupSmallestSize(w, init_array);
		this->inf->SetupSmallestSize(w, init_array);

		uint min_avs_width = this->avs->smallest_x + this->avs->padding_left + this->avs->padding_right;
		uint min_acs_width = this->acs->smallest_x + this->acs->padding_left + this->acs->padding_right;
		uint min_inf_width = this->inf->smallest_x + this->inf->padding_left + this->inf->padding_right;

		uint min_avs_height = this->avs->smallest_y + this->avs->padding_top + this->avs->padding_bottom;
		uint min_acs_height = this->acs->smallest_y + this->acs->padding_top + this->acs->padding_bottom;
		uint min_inf_height = this->inf->smallest_y + this->inf->padding_top + this->inf->padding_bottom;

		/* Smallest window is in two column mode. */
		this->smallest_x = max(min_avs_width, min_acs_width) + INTER_COLUMN_SPACING + min_inf_width;
		this->smallest_y = max(min_inf_height, min_acs_height + INTER_LIST_SPACING + min_avs_height);

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
	}

	virtual void AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool rtl)
	{
		this->StoreSizePosition(sizing, x, y, given_width, given_height);

		uint min_avs_width = this->avs->smallest_x + this->avs->padding_left + this->avs->padding_right;
		uint min_acs_width = this->acs->smallest_x + this->acs->padding_left + this->acs->padding_right;
		uint min_inf_width = this->inf->smallest_x + this->inf->padding_left + this->inf->padding_right;

		uint min_list_width = max(min_avs_width, min_acs_width); // Smallest width of the lists such that they have equal width (incl padding).
		uint avs_extra_width = min_list_width - min_avs_width;   // Additional width needed for avs to reach min_list_width.
		uint acs_extra_width = min_list_width - min_acs_width;   // Additional width needed for acs to reach min_list_width.

		/* Use 2 or 3 colmuns? */
		uint min_three_columns = min_avs_width + min_acs_width + min_inf_width + 2 * INTER_COLUMN_SPACING;
		uint min_two_columns   = min_list_width + min_inf_width + INTER_COLUMN_SPACING;
		bool use_three_columns = this->editable && (min_three_columns + MIN_EXTRA_FOR_3_COLUMNS <= given_width);

		/* Info panel is a seperate column in both modes. Compute its width first. */
		uint extra_width, inf_width;
		if (use_three_columns) {
			extra_width = given_width - min_three_columns;
			inf_width = min(MAX_EXTRA_INFO_WIDTH, extra_width / 2);
		} else {
			extra_width = given_width - min_two_columns;
			inf_width = min(MAX_EXTRA_INFO_WIDTH, extra_width / 2);
		}
		inf_width = ComputeMaxSize(this->inf->smallest_x, this->inf->smallest_x + inf_width, this->inf->GetHorizontalStepSize(sizing));
		extra_width -= inf_width - this->inf->smallest_x;

		uint inf_height = ComputeMaxSize(this->inf->smallest_y, given_height, this->inf->GetVerticalStepSize(sizing));

		if (use_three_columns) {
			/* Three column display, first make both lists equally wide, then divide whatever is left between both lists.
			 * Only keep track of what avs gets, all other space goes to acs. */
			uint avs_width = min(avs_extra_width, extra_width);
			extra_width -= avs_width;
			extra_width -= min(acs_extra_width, extra_width);
			avs_width += extra_width / 2;

			avs_width = ComputeMaxSize(this->avs->smallest_x, this->avs->smallest_x + avs_width, this->avs->GetHorizontalStepSize(sizing));

			uint acs_width = given_width - // Remaining space, including horizontal padding.
					inf_width - this->inf->padding_left - this->inf->padding_right -
					avs_width - this->avs->padding_left - this->avs->padding_right - 2 * INTER_COLUMN_SPACING;
			acs_width = ComputeMaxSize(min_acs_width, acs_width, this->acs->GetHorizontalStepSize(sizing)) -
					this->acs->padding_left - this->acs->padding_right;

			uint avs_height = ComputeMaxSize(this->avs->smallest_y, given_height, this->avs->GetVerticalStepSize(sizing));
			uint acs_height = ComputeMaxSize(this->acs->smallest_y, given_height, this->acs->GetVerticalStepSize(sizing));

			/* Assign size and position to the childs. */
			if (rtl) {
				x += this->inf->padding_left;
				this->inf->AssignSizePosition(sizing, x, y + this->inf->padding_top, inf_width, inf_height, rtl);
				x += inf_width + this->inf->padding_right + INTER_COLUMN_SPACING;
			} else {
				x += this->avs->padding_left;
				this->avs->AssignSizePosition(sizing, x, y + this->avs->padding_top, avs_width, avs_height, rtl);
				x += avs_width + this->avs->padding_right + INTER_COLUMN_SPACING;
			}

			x += this->acs->padding_left;
			this->acs->AssignSizePosition(sizing, x, y + this->acs->padding_top, acs_width, acs_height, rtl);
			x += acs_width + this->acs->padding_right + INTER_COLUMN_SPACING;

			if (rtl) {
				x += this->avs->padding_left;
				this->avs->AssignSizePosition(sizing, x, y + this->avs->padding_top, avs_width, avs_height, rtl);
			} else {
				x += this->inf->padding_left;
				this->inf->AssignSizePosition(sizing, x, y + this->inf->padding_top, inf_width, inf_height, rtl);
			}
		} else {
			/* Two columns, all space in extra_width goes to both lists. Since the lists are underneath each other,
			 * the column is min_list_width wide at least. */
			uint avs_width = ComputeMaxSize(this->avs->smallest_x, this->avs->smallest_x + avs_extra_width + extra_width,
					this->avs->GetHorizontalStepSize(sizing));
			uint acs_width = ComputeMaxSize(this->acs->smallest_x, this->acs->smallest_x + acs_extra_width + extra_width,
					this->acs->GetHorizontalStepSize(sizing));

			uint min_avs_height = (!this->editable) ? 0 : this->avs->smallest_y + this->avs->padding_top + this->avs->padding_bottom + INTER_LIST_SPACING;
			uint min_acs_height = this->acs->smallest_y + this->acs->padding_top + this->acs->padding_bottom;
			uint extra_height = given_height - min_acs_height - min_avs_height;

			uint avs_height = ComputeMaxSize(this->avs->smallest_y, this->avs->smallest_y + extra_height / 2, this->avs->GetVerticalStepSize(sizing));
			if (this->editable) extra_height -= avs_height - this->avs->smallest_y;
			uint acs_height = ComputeMaxSize(this->acs->smallest_y, this->acs->smallest_y + extra_height, this->acs->GetVerticalStepSize(sizing));

			/* Assign size and position to the childs. */
			if (rtl) {
				x += this->inf->padding_left;
				this->inf->AssignSizePosition(sizing, x, y + this->inf->padding_top, inf_width, inf_height, rtl);
				x += inf_width + this->inf->padding_right + INTER_COLUMN_SPACING;

				uint ypos = y + this->acs->padding_top;
				this->acs->AssignSizePosition(sizing, x + this->acs->padding_left, ypos, acs_width, acs_height, rtl);
				if (this->editable) {
					ypos += acs_height + this->acs->padding_bottom + INTER_LIST_SPACING + this->avs->padding_top;
					this->avs->AssignSizePosition(sizing, x + this->avs->padding_left, ypos, avs_width, avs_height, rtl);
				} else {
					this->avs->AssignSizePosition(sizing, 0, 0, this->avs->smallest_x, this->avs->smallest_y, rtl);
				}
			} else {
				uint ypos = y + this->acs->padding_top;
				this->acs->AssignSizePosition(sizing, x + this->acs->padding_left, ypos, acs_width, acs_height, rtl);
				if (this->editable) {
					ypos += acs_height + this->acs->padding_bottom + INTER_LIST_SPACING + this->avs->padding_top;
					this->avs->AssignSizePosition(sizing, x + this->avs->padding_left, ypos, avs_width, avs_height, rtl);
				} else {
					this->avs->AssignSizePosition(sizing, 0, 0, this->avs->smallest_x, this->avs->smallest_y, rtl);
				}
				uint dx = this->acs->current_x + this->acs->padding_left + this->acs->padding_right;
				if (this->editable) {
					dx = max(dx, this->avs->current_x + this->avs->padding_left + this->avs->padding_right);
				}
				x += dx + INTER_COLUMN_SPACING + this->inf->padding_left;
				this->inf->AssignSizePosition(sizing, x, y + this->inf->padding_top, inf_width, inf_height, rtl);
			}
		}
	}

	virtual NWidgetCore *GetWidgetFromPos(int x, int y)
	{
		if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return NULL;

		NWidgetCore *nw = (this->editable) ? this->avs->GetWidgetFromPos(x, y) : NULL;
		if (nw == NULL) nw = this->acs->GetWidgetFromPos(x, y);
		if (nw == NULL) nw = this->inf->GetWidgetFromPos(x, y);
		return nw;
	}

	virtual void Draw(const Window *w)
	{
		if (this->editable) this->avs->Draw(w);
		this->acs->Draw(w);
		this->inf->Draw(w);
	}
};

const uint NWidgetNewGRFDisplay::INTER_LIST_SPACING      = WD_RESIZEBOX_WIDTH + 1;
const uint NWidgetNewGRFDisplay::INTER_COLUMN_SPACING    = WD_RESIZEBOX_WIDTH;
const uint NWidgetNewGRFDisplay::MAX_EXTRA_INFO_WIDTH    = 150;
const uint NWidgetNewGRFDisplay::MIN_EXTRA_FOR_3_COLUMNS = 50;

static const NWidgetPart _nested_newgrf_actives_widgets[] = {
	/* Left side, presets. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXT, COLOUR_MAUVE), SetDataTip(STR_NEWGRF_SETTINGS_SELECT_PRESET, STR_NULL),
				SetPadding(0, WD_FRAMETEXT_RIGHT, 0, 0),
		NWidget(WWT_DROPDOWN, COLOUR_YELLOW, SNGRFS_PRESET_LIST), SetFill(1, 0), SetResize(1, 0),
				SetDataTip(STR_JUST_STRING, STR_NEWGRF_SETTINGS_PRESET_LIST_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_PRESET_SAVE), SetFill(1, 0), SetResize(1, 0),
				SetDataTip(STR_NEWGRF_SETTINGS_PRESET_SAVE, STR_NEWGRF_SETTINGS_PRESET_SAVE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_PRESET_DELETE), SetFill(1, 0), SetResize(1, 0),
				SetDataTip(STR_NEWGRF_SETTINGS_PRESET_DELETE, STR_NEWGRF_SETTINGS_PRESET_DELETE_TOOLTIP),
	EndContainer(),

	NWidget(NWID_SPACER), SetMinimalSize(0, WD_RESIZEBOX_WIDTH), SetResize(1, 0), SetFill(1, 0),
	NWidget(WWT_PANEL, COLOUR_MAUVE),
		NWidget(WWT_LABEL, COLOUR_MAUVE), SetDataTip(STR_NEWGRF_SETTINGS_ACTIVE_LIST, STR_NULL),
				SetFill(1, 0), SetResize(1, 0), SetPadding(3, WD_FRAMETEXT_RIGHT, 0, WD_FRAMETEXT_LEFT),
		/* Left side, active grfs. */
		NWidget(NWID_HORIZONTAL), SetPadding(0, 2, 0, 2),
			NWidget(WWT_PANEL, COLOUR_MAUVE),
				NWidget(WWT_INSET, COLOUR_MAUVE, SNGRFS_FILE_LIST), SetMinimalSize(100, 1), SetPadding(2, 2, 2, 2),
						SetFill(1, 1), SetResize(1, 1), SetScrollbar(SNGRFS_SCROLLBAR),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, SNGRFS_SCROLLBAR),
		EndContainer(),
		/* Buttons. */
		NWidget(NWID_SELECTION, INVALID_COLOUR, SNGRFS_SHOW_REMOVE),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPadding(2, 2, 2, 2), SetPIP(0, WD_RESIZEBOX_WIDTH, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_REMOVE), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_NEWGRF_SETTINGS_REMOVE, STR_NEWGRF_SETTINGS_REMOVE_TOOLTIP),
				NWidget(NWID_VERTICAL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_MOVE_UP), SetFill(1, 0), SetResize(1, 0),
							SetDataTip(STR_NEWGRF_SETTINGS_MOVEUP, STR_NEWGRF_SETTINGS_MOVEUP_TOOLTIP),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_MOVE_DOWN), SetFill(1, 0), SetResize(1, 0),
							SetDataTip(STR_NEWGRF_SETTINGS_MOVEDOWN, STR_NEWGRF_SETTINGS_MOVEDOWN_TOOLTIP),
				EndContainer(),
			EndContainer(),

			NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPadding(2, 2, 2, 2),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_RESCAN_FILES2), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_NEWGRF_SETTINGS_RESCAN_FILES, STR_NEWGRF_SETTINGS_RESCAN_FILES_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_CONTENT_DOWNLOAD2), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const NWidgetPart _nested_newgrf_availables_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_MAUVE),
		NWidget(WWT_LABEL, COLOUR_MAUVE), SetDataTip(STR_NEWGRF_SETTINGS_INACTIVE_LIST, STR_NULL),
				SetFill(1, 0), SetResize(1, 0), SetPadding(3, WD_FRAMETEXT_RIGHT, 0, WD_FRAMETEXT_LEFT),
		/* Left side, available grfs, filter edit box. */
		NWidget(NWID_HORIZONTAL), SetPadding(WD_TEXTPANEL_TOP, 0, WD_TEXTPANEL_BOTTOM, 0),
				SetPIP(WD_FRAMETEXT_LEFT, WD_FRAMETEXT_RIGHT, WD_FRAMETEXT_RIGHT),
			NWidget(WWT_TEXT, COLOUR_MAUVE), SetFill(0, 1), SetDataTip(STR_NEWGRF_FILTER_TITLE, STR_NULL),
			NWidget(WWT_EDITBOX, COLOUR_MAUVE, SNGRFS_FILTER), SetFill(1, 0), SetMinimalSize(50, 12), SetResize(1, 0),
					SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
		EndContainer(),
		/* Left side, available grfs. */
		NWidget(NWID_HORIZONTAL), SetPadding(0, 2, 0, 2),
			NWidget(WWT_PANEL, COLOUR_MAUVE),
				NWidget(WWT_INSET, COLOUR_MAUVE, SNGRFS_AVAIL_LIST), SetMinimalSize(100, 1), SetPadding(2, 2, 2, 2),
						SetFill(1, 1), SetResize(1, 1), SetScrollbar(SNGRFS_SCROLL2BAR),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, SNGRFS_SCROLL2BAR),
		EndContainer(),
		/* Left side, available grfs, buttons. */
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPadding(2, 2, 2, 2), SetPIP(0, WD_RESIZEBOX_WIDTH, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_ADD), SetFill(1, 0), SetResize(1, 0),
					SetDataTip(STR_NEWGRF_SETTINGS_ADD, STR_NEWGRF_SETTINGS_ADD_FILE_TOOLTIP),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_RESCAN_FILES), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_NEWGRF_SETTINGS_RESCAN_FILES, STR_NEWGRF_SETTINGS_RESCAN_FILES_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_CONTENT_DOWNLOAD), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const NWidgetPart _nested_newgrf_infopanel_widgets[] = {
	/* Right side, info panel. */
	NWidget(WWT_PANEL, COLOUR_MAUVE), SetPadding(0, 0, 2, 0),
		NWidget(WWT_EMPTY, COLOUR_MAUVE, SNGRFS_NEWGRF_INFO_TITLE), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_EMPTY, COLOUR_MAUVE, SNGRFS_NEWGRF_INFO), SetFill(1, 1), SetResize(1, 1), SetMinimalSize(150, 100),
	EndContainer(),
	NWidget(NWID_SELECTION, INVALID_COLOUR, SNGRFS_SHOW_APPLY),
		/* Right side, buttons. */
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WD_RESIZEBOX_WIDTH, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_SET_PARAMETERS), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_NEWGRF_SETTINGS_SET_PARAMETERS, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_TOGGLE_PALETTE), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_NEWGRF_SETTINGS_TOGGLE_PALETTE, STR_NEWGRF_SETTINGS_TOGGLE_PALETTE_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, SNGRFS_APPLY_CHANGES), SetFill(1, 0), SetResize(1, 0),
					SetDataTip(STR_NEWGRF_SETTINGS_APPLY_CHANGES, STR_NULL),
		EndContainer(),
	EndContainer(),
};

/** Construct nested container widget for managing the lists and the info panel of the NewGRF GUI. */
NWidgetBase* NewGRFDisplay(int *biggest_index)
{
	NWidgetBase *avs = MakeNWidgets(_nested_newgrf_availables_widgets, lengthof(_nested_newgrf_availables_widgets), biggest_index, NULL);

	int biggest2;
	NWidgetBase *acs = MakeNWidgets(_nested_newgrf_actives_widgets, lengthof(_nested_newgrf_actives_widgets), &biggest2, NULL);
	*biggest_index = max(*biggest_index, biggest2);

	NWidgetBase *inf = MakeNWidgets(_nested_newgrf_infopanel_widgets, lengthof(_nested_newgrf_infopanel_widgets), &biggest2, NULL);
	*biggest_index = max(*biggest_index, biggest2);

	return new NWidgetNewGRFDisplay(avs, acs, inf);
}

/* Widget definition of the manage newgrfs window */
static const NWidgetPart _nested_newgrf_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE), SetDataTip(STR_NEWGRF_SETTINGS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE),
		NWidgetFunction(NewGRFDisplay), SetPadding(WD_RESIZEBOX_WIDTH, WD_RESIZEBOX_WIDTH, 2, WD_RESIZEBOX_WIDTH),
		/* Resize button. */
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_RESIZEBOX, COLOUR_MAUVE),
		EndContainer(),
	EndContainer(),
};

/* Window definition of the manage newgrfs window */
static const WindowDesc _newgrf_desc(
	WDP_CENTER, 300, 263,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_newgrf_widgets, lengthof(_nested_newgrf_widgets)
);

/**
 * Callback function for the newgrf 'apply changes' confirmation window
 * @param w Window which is calling this callback
 * @param confirmed boolean value, true when yes was clicked, false otherwise
 */
static void NewGRFConfirmationCallback(Window *w, bool confirmed)
{
	if (confirmed) {
		DeleteWindowByClass(WC_GRF_PARAMETERS);
		NewGRFWindow *nw = dynamic_cast<NewGRFWindow*>(w);

		GamelogStartAction(GLAT_GRF);
		GamelogGRFUpdate(_grfconfig, nw->actives); // log GRF changes
		CopyGRFConfigList(nw->orig_list, nw->actives, false);
		ReloadNewGRFData();
		GamelogStopAction();

		/* Show new, updated list */
		GRFConfig *c;
		int i = 0;
		for (c = nw->actives; c != NULL && c != nw->active_sel; c = c->next, i++) {}
		CopyGRFConfigList(&nw->actives, *nw->orig_list, false);
		for (c = nw->actives; c != NULL && i > 0; c = c->next, i--) {}
		nw->active_sel = c;
		nw->avails.ForceRebuild();

		w->InvalidateData();

		ReInitAllWindows();
		DeleteWindowByClass(WC_BUILD_OBJECT);
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
	DeleteWindowByClass(WC_GAME_OPTIONS);
	new NewGRFWindow(&_newgrf_desc, editable, show_params, exec_changes, config);
}
