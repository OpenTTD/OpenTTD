/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file selector_widget.cpp GUI that shows performance graphs. */

#include "stdafx.h"
#include "company_base.h"
#include "company_gui.h"
#include "strings_func.h"
#include "table/sprites.h"
#include "widgets/graph_widget.h"
#include "selector_widget.h"
#include "safeguards.h"
#include <cassert>


/**
 * Create a new NWidgetBase with all the widgets needed by the SelectorWidget widget
 * This function is made to be passed in NWidgetFunction, when creating UI for a parrent window
 * @see NWidgetFunction
 */
std::unique_ptr<NWidgetBase> SelectorWidget::MakeSelectorWidgetUI() {
	static constexpr NWidgetPart widget_ui[] = {
		NWidget(WWT_PANEL, COLOUR_BROWN),
			NWidget(WWT_EDITBOX, COLOUR_BROWN, WID_SELECTOR_EDITBOX), SetFill(1, 0), SetResize(1, 0), SetPadding(2),
			SetDataTip(STR_LIST_FILTER_TOOLTIP, STR_LIST_FILTER_TOOLTIP),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_MATRIX, COLOUR_BROWN, WID_SELECTOR_MATRIX),
					SetScrollbar(WID_SELECTOR_SCROLLBAR), SetResize(1, 1), SetMatrixDataTip(1, 0, STR_NULL), SetFill(1, 1),
				NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_SELECTOR_SCROLLBAR),
			EndContainer(),
			NWidget(NWID_VERTICAL, NC_EQUALSIZE),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SELECTOR_HIDEALL),
						SetDataTip(STR_SELECTOR_WIDGET_DISABLE_ALL, STR_SELECTOR_WIDGET_TOOLTIP_DISABLE_ALL),
						SetResize(1, 0), SetMinimalSize(20, 12), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SELECTOR_SHOWALL),
					SetDataTip(STR_SELECTOR_WIDGET_ENABLE_ALL, STR_SELECTOR_WIDGET_TOOLTIP_ENABLE_ALL),
						SetResize(1, 0), SetMinimalSize(20, 12), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SELECTOR_TOGGLE),
						SetDataTip(STR_SELECTOR_WIDGET_TOGGLE_SELECTED, STR_SELECTOR_WIDGET_TOOLTIP_TOGGLE_SELECTED),
						SetResize(1, 0), SetMinimalSize(20, 12), SetFill(1, 0),
					NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_GRAPH_RESIZE), SetDataTip(RWV_SHOW_BEVEL, STR_TOOLTIP_RESIZE),
						SetResize(0, 0),
				NWidget(NWID_HORIZONTAL),
			EndContainer(),
		EndContainer(),
	};
	return MakeNWidgets({std::begin(widget_ui), std::end(widget_ui)}, nullptr);
}

/**
 * Selector widget initialization function
 * This function is ment to be called after CreateNestedTree() of the window
 *
 * @param w The parrent window of this widget
 *
 * @see SelectorWidget::post_init()
 */
void SelectorWidget::Init(Window* w)
{
	this->w = w;
	this->vscroll = w->GetScrollbar(WID_SELECTOR_SCROLLBAR);
	assert(this->vscroll != nullptr);
	RebuildList();
	w->querystrings[WID_SELECTOR_EDITBOX] = &this->editbox;
	this->vscroll->SetCount(this->filtered_list.size());
	this->vscroll->SetCapacityFromWidget(w, WID_SELECTOR_MATRIX);
}

/**
 * Selector widget profile selection function
 * This function is ment to be called before CreateNestedTree() of the window
 *
 * @param p The "profile" the selector widget should use
 *
 * A selector widget profile is just a set of function pointers /
 * possibly parameters in the future, that determine how the widget works,
 * and help make it more universal.
 *
 * @see SelectorWidget::init()
 */
void SelectorWidget::PreInit(Profile p)
{
	this->profile = p;
}

/**
 * Selector widget's OnClick event handler
 * This function is ment to be called from the parrent's window's OnClick
 * Event handler
 *
 * @param pt The click coordinate
 * @param widget The id of the targeted widget
 * @param click_count The number of clicks
 *
 * All of theese parameters should be passed straight from the parrent's window's OnClick
 * Event handler, without any changes, or conditional checks
 *
 * @see Window::OnClick()
 */
void SelectorWidget::OnClick(Point pt, WidgetID widget, int click_count)
{
	if (widget == WID_SELECTOR_HIDEALL){
		for (uint i = 0; i < this->shown.size(); i++) {
			this->shown[i] = false;
		}
	} else if (widget == WID_SELECTOR_SHOWALL) {
		for (uint i = 0; i < this->shown.size(); i++) {
			this->shown[i] = true;
		}
	} else if (widget == WID_SELECTOR_TOGGLE) {
		if (this->selected_id.has_value()) {
			this->shown[this->selected_id.value()].flip();
		}
	} else if (widget == WID_SELECTOR_MATRIX) {
		const uint selected_row = vscroll->GetScrolledRowFromWidget(pt.y, w, widget);
		if (selected_row >= this->filtered_list.size()) {
			return;
		}
		selected_id = this->filtered_list[selected_row];
		if (click_count > 1) {
			if (this->selected_id.has_value()) {
				this->shown[this->selected_id.value()].flip();
			}
		}
	}
	w->InvalidateData(0, true);
}

/**
 * Selector widget's OnInvalidateData event handler
 * This function is ment to be called from the parrent's window's OnInvalidateData
 * Event handler
 *
 * @param data Information about the changed data.
 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
 *
 * All of theese parameters should be passed straight from the parrent's window's OnInvalidateData
 * Event handler, without any changes, or conditional checks
 *
 * @see Window::OnInvalidateData()
 */
void SelectorWidget::OnInvalidateData([[maybe_unused]] int data, bool gui_scope)
{
	if (!gui_scope) return;
	RebuildList();
	this->vscroll->SetCapacityFromWidget(w, WID_SELECTOR_MATRIX);
	if (this->selected_id.has_value()) {
		const uint to_select_id = this->selected_id.value();
		int selected_pos = -1;
		for (uint i = 0; i < this->filtered_list.size(); i++) {
			if (this->filtered_list[i] == to_select_id) {
				selected_pos = i;
				break;
			}
		}
		if (selected_pos != -1) {
			const int cap = this->vscroll->GetCapacity();
			const int pos = this->vscroll->GetPosition();
			if (selected_pos < pos) {
				vscroll->SetPosition(selected_pos);
			} else if (selected_pos > pos + cap) {
				vscroll->SetPosition(selected_pos - cap + 1);
			}
		}
	}
	w->SetDirty();
}

/**
 * Selector widget's UpdateWidgetSize method
 * This function is ment to be called from the parrent's window's UpdateWidgetSize method
 *
 * @param widget  Widget number.
 * @param[in,out] size Size of the widget.
 * @param padding Recommended amount of space between the widget content and the widget edge.
 * @param[in,out] fill Fill step of the widget.
 * @param[in,out] resize Resize step of the widget.
 *
 * All of theese parameters should be passed straight from the parrent's window's UpdateWidgetSize
 * method, without any changes, or conditional checks.
 *
 * @see Window::UpdateWidgetSize()
 */
void SelectorWidget::UpdateWidgetSize(WidgetID widget, Dimension &size, const Dimension &padding, Dimension &fill, Dimension &resize)
{
	if (widget != WID_SELECTOR_MATRIX) return;
	this->row_height = GetCharacterHeight(FS_NORMAL) + padding.height;
	size.height = this->row_height * 7;
	size.width = 300;
	resize.width = 1;
	resize.height = this->row_height;
	fill.width = 1;
	fill.height = this->row_height;
}

/**
 * Selector widget's OnResize event handler
 * This function is ment to be called from the parrent's window's OnResize
 * Event handler
 *
 * All of theese parameters should be passed straight from the parrent's window's OnResize
 * Event handler, without any changes, or conditional checks
 *
 * @see Window::OnResize()
 */
void SelectorWidget::OnResize()
{
	this->vscroll->SetCapacityFromWidget(w, WID_SELECTOR_MATRIX);
}

/**
 * Selector widget's OnEditboxChanged event handler
 * This function is ment to be called from the parrent's window's OnEditboxChanged
 * Event handler
 *
 * @param wid The widget id of the editbox
 *
 * All of theese parameters should be passed straight from the parrent's window's OnEditboxChanged
 * Event handler, without any changes, or conditional checks
 *
 * @see Window::OnEditboxChanged()
 */
void SelectorWidget::OnEditboxChanged(WidgetID wid)
{
	if (wid != WID_SELECTOR_EDITBOX) {
		return;
	}
	this->string_filter.SetFilterTerm(this->editbox.text.buf);
	this->w->InvalidateData(0);
	this->vscroll->SetCount(this->filtered_list.size());
}

/**
 * Selector widget's DrawWidget method
 * This function is ment to be called from the parrent's window's DrawWidget
 * method
 *
 * @param widget  Widget number.
 * @param[in,out] size Size of the widget.
 * @param padding Recommended amount of space between the widget content and the widget edge.
 * @param[in,out] fill Fill step of the widget.
 * @param[in,out] resize Resize step of the widget.
 *
 * All of theese parameters should be passed straight from the parrent's window's DrawWidget
 * Event handler, without any changes, or conditional checks
 *
 * This function calls a profile defined function this->profile.DrawSection(),
 * that actually draws the fields
 *
 * @see Window::DrawWidget()
 * @see SelectorWidget::Profile
 */
void SelectorWidget::DrawWidget(const Rect &r, WidgetID widget)
{
	if (widget != WID_SELECTOR_MATRIX) {
		return;
	}
	Rect line = r.WithHeight(row_height);

	const bool rtl = _current_text_dir == TD_RTL;

	auto [first, last] = this->vscroll->GetVisibleRangeIterators(filtered_list);

	for (auto it = first; it != last; ++it) {
		const Rect ir = line.Shrink(WidgetDimensions::scaled.framerect);

		if (!this->shown[*it]) {
			DrawFrameRect(ir, COLOUR_BROWN, FR_LOWERED);
		}
		this->profile.DrawSection(this, *it, ir.Indent(WidgetDimensions::scaled.fullbevel.Horizontal(), rtl));

		line = line.Translate(0, row_height);
	}
}

/**
 * This function is one of the possible functions ment to be used as part of the DrawWidget::Profile
 * It draws "section" (line) of the scrollable list
 * @param wid The current SelectionWidget object pointer
 * @param id the "id" of the item to be drawn, can mean different things depending on the profile
 * e.g when this widget displays companies, the id is a unique and valid CompanyID
 * @param r the rectangle where the item is to be drawn
 * @see SelectorWidget::DrawSection()
 * @see SelectorWidget::DrawWidget()
 * @see SelectorWidget::Profile
 */
void DrawSectionCompany(SelectorWidget *wid, int id, const Rect &r)
{
	const CompanyID cid = (CompanyID)id;
	assert(Company::IsValidID(cid));

	const bool rtl = _current_text_dir == TD_RTL;
	const Dimension d = GetSpriteSize(SPR_COMPANY_ICON);

	DrawCompanyIcon(cid, rtl ? r.right - d.width : r.left, CenterBounds(r.top, r.bottom, d.height));

	const Rect text = r.Indent(d.width + WidgetDimensions::scaled.hsep_normal, rtl);

	SetDParam(0, cid);
	if (wid->selected_id.value_or(INVALID_OWNER) == cid) {
		DrawString(text.left, text.right, CenterBounds(text.top, text.bottom, GetCharacterHeight(FS_NORMAL)), STR_COMPANY_NAME,  TC_WHITE);
	} else {
		DrawString(text.left, text.right, CenterBounds(text.top, text.bottom, GetCharacterHeight(FS_NORMAL)), STR_COMPANY_NAME,  TC_BLACK);
	}
}

/**
 * This function is one of the possible functions ment to be used as part of the DrawWidget::Profile
 * It draws "section" (line) of the scrollable list
 * @param wid The current SelectionWidget object pointer
 * @param id the "id" of the item to be drawn, can mean different things depending on the profile
 * e.g when this widget displays companies, the id is a unique and valid CompanyID
 * @param r the rectangle where the item is to be drawn
 * @see SelectorWidget::DrawSection()
 * @see SelectorWidget::DrawWidget()
 * @see SelectorWidget::Profile
 */
static void DrawSectionCargo(SelectorWidget *wid, int id, const Rect &r)
{
	const CargoID cargo_id = (CargoID)id; ///< Id of the current row's cargo
	const CargoSpec *cargo = CargoSpec::Get(id); ///< cargo spec of the current row's cargo

	const bool rtl = _current_text_dir == TD_RTL;

	const float cargo_rect_scale_factor = 0.85f;
	const int legend_width = r.Height() * 9 / 6;

	/* Make a rectangle to display the legend color */
	const Rect cargo_colour = r.Shrink(0, r.Height() * (1.0 - cargo_rect_scale_factor) / 2.0)
		.WithWidth(legend_width * cargo_rect_scale_factor, rtl);

	/* Cargo-colour box with outline */
	GfxFillRect(cargo_colour, PC_BLACK);
	GfxFillRect(cargo_colour.Shrink(WidgetDimensions::scaled.bevel), cargo->legend_colour);

	/* Cargo name */
	SetDParam(0, cargo->name);
	const Rect text = r.Shrink(legend_width + WidgetDimensions::scaled.hsep_normal, rtl);
	if (wid->selected_id.value_or(INVALID_OWNER) == cargo_id) {
		DrawString(text.left, text.right, CenterBounds(text.top, text.bottom, GetCharacterHeight(FS_NORMAL)), STR_JUST_STRING,  TC_WHITE);
	} else {
		DrawString(text.left, text.right, CenterBounds(text.top, text.bottom, GetCharacterHeight(FS_NORMAL)), STR_JUST_STRING,  TC_BLACK);
	}
};



/**
 * This function is one of the possible functions ment to be used as part of the DrawWidget::Profile
 * It repopulates the list that the widget uses to keep track of different selectable items
 * @param wid The current SelectionWidget object pointer
 * @see SelectorWidget::RebuildList()
 * @see SelectorWidget::Profile
 */
static void RebuildListCompany(SelectorWidget *wid) {
	for (const Company *c: Company::Iterate()) {
		wid->list.push_back(c->index);
		if (wid->string_filter.IsEmpty()) {
			wid->filtered_list.push_back(c->index);
			continue;
		}
		wid->string_filter.ResetState();

		SetDParam(0, c->index);
		wid->string_filter.AddLine(GetString(STR_COMPANY_NAME));

		if (wid->string_filter.GetState()) {
			wid->filtered_list.push_back(c->index);
		}
	}
}

/**
 * This function is one of the possible functions ment to be used as part of the DrawWidget::Profile
 * It repopulates the list that the widget uses to keep track of different selectable items
 * @param wid The current SelectionWidget object pointer
 * @see SelectorWidget::RebuildList()
 * @see SelectorWidget::Profile
 */
void RebuildListCargo(SelectorWidget *wid) {
	for (const CargoSpec *cargo : _sorted_standard_cargo_specs){
		wid->list.push_back(cargo->Index());
		if (wid->string_filter.IsEmpty()) {
			wid->filtered_list.push_back(cargo->Index());
			continue;
		}
		wid->string_filter.ResetState();

		SetDParam(0, cargo->Index());
		wid->string_filter.AddLine(GetString(STR_JUST_CARGO));

		if (wid->string_filter.GetState()) {
			wid->filtered_list.push_back(cargo->Index());
		}
	}
}

/**
 * A function that updates and rebuilds the list of selectable items
 * This function calls a profile defined function this->profile.RebuildList(),
 * that actually rebuilds the list of items.
 *
 * It is also manages the shown items and the filtered_list lists
 *
 * @see SelectorWidget::Profile
 */
void SelectorWidget::RebuildList() {
	this->list.clear();
	this->filtered_list.clear();
	this->profile.RebuildList(this);
	uint32_t max = 0;
	for (const uint32_t id : list) {
		if (id > max) {
			max = id;
		}
	}
	for (size_t i = this->shown.size(); i < max + 1; i++) {
		shown.push_back(true);
	}
}

const SelectorWidget::Profile SelectorWidget::company_selector_profile = {DrawSectionCompany, RebuildListCompany};
const SelectorWidget::Profile SelectorWidget::cargo_selector_profile = {DrawSectionCargo, RebuildListCargo};
