/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file selector_gui.cpp A composable selector widget. */

#include "stdafx.h"
#include "company_base.h"
#include "company_gui.h"
#include "strings_func.h"
#include "table/sprites.h"
#include "selector_gui.h"
#include "widget_type.h"
#include "widgets/selector_widget.h"
#include "zoom_func.h"

#include "safeguards.h"

/**
 * Create a new NWidgetBase with all the widgets needed by the SelectorWidget widget
 * This function is made to be passed in NWidgetFunction, when creating UI for a parent window
 * @see NWidgetFunction
 */
std::unique_ptr<NWidgetBase> SelectorWidget::MakeSelectorWidgetUI()
{
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
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SELECTOR_SHOWALL),
						SetDataTip(STR_SELECTOR_WIDGET_ALL, STR_SELECTOR_WIDGET_TOOLTIP_ALL), SetResize(1, 0), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_SELECTOR_HIDEALL),
						SetDataTip(STR_SELECTOR_WIDGET_NONE, STR_SELECTOR_WIDGET_TOOLTIP_NONE), SetResize(1, 0), SetFill(1, 0),
				NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_SELECTOR_RESIZE), SetDataTip(RWV_SHOW_BEVEL, STR_TOOLTIP_RESIZE), SetResize(0, 0),
			EndContainer(),
		EndContainer(),
	};
	return MakeNWidgets(widget_ui, nullptr);
}

/**
 * Selector widget initialization function
 * This function is meant to be called after CreateNestedTree() of the parent window
 *
 * @param w The parent window of this widget.
 */
void SelectorWidget::Init(Window *w)
{
	this->parent_window = w;
	this->vscroll = w->GetScrollbar(WID_SELECTOR_SCROLLBAR);
	assert(this->vscroll != nullptr);
	RebuildList();
	w->querystrings[WID_SELECTOR_EDITBOX] = &this->editbox;
	this->vscroll->SetCount(this->filtered_list.size());
	this->vscroll->SetCapacityFromWidget(w, WID_SELECTOR_MATRIX);
}

/**
 * Selector widget's OnClick event handler
 * This function is meant to be called from the parent's window's OnClick
 * Event handler
 *
 * @param pt The click coordinate.
 * @param widget The id of the targeted widget.
 * @param click_count The number of clicks.
 *
 * All of these parameters should be passed straight from the parent's window's OnClick
 * Event handler, without any changes, or conditional checks
 *
 * @see Window::OnClick()
 */
void SelectorWidget::OnClick(Point pt, WidgetID widget, [[maybe_unused]]int click_count)
{
	switch (widget) {
		case WID_SELECTOR_HIDEALL:
			for (uint i = 0; i < this->shown.size(); i++) {
				this->shown[i] = false;
			}
			this->OnChanged();
			this->parent_window->InvalidateData(0, true);
			break;

		case WID_SELECTOR_SHOWALL:
			for (uint i = 0; i < this->shown.size(); i++) {
				this->shown[i] = true;
			}
			this->OnChanged();
			this->parent_window->InvalidateData(0, true);
			break;

		case WID_SELECTOR_MATRIX: {
			size_t pos = vscroll->GetScrolledRowFromWidget(pt.y, this->parent_window, widget);

			/* Check if the click was out of range. */
			if (pos >= filtered_list.size()) return;
			int id = this->filtered_list[pos];

			this->shown[id].flip();
			this->OnChanged();
			this->parent_window->InvalidateData(0, true);
			break;
		}
	}
}

void SelectorWidget::OnMouseOver(Point pt, WidgetID widget)
{
	if (widget != WID_SELECTOR_MATRIX) return;

	auto it = vscroll->GetScrolledItemFromWidget(this->filtered_list, pt.y, this->parent_window, widget);
	/* Check if the hover was out of range. */
	if (it == this->filtered_list.end()) return;

	/* Check if the selection actually changed. */
	if (*it == this->selected_id) return;

	this->selected_id = *it;

	this->OnChanged();
	this->parent_window->InvalidateData(0, true);
}

/**
 * Selector widget's OnInvalidateData event handler
 * This function is meant to be called from the parent's window's OnInvalidateData
 * Event handler. This function will do nothing when runnning ouside of the gui scope.
 *
 * @param data Information about the changed data.
 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
 *
 * All of these parameters should be passed straight from the parent's window's OnInvalidateData
 * Event handler, without any changes, or conditional checks
 *
 * @see Window::OnInvalidateData()
 */
void SelectorWidget::OnInvalidateData([[maybe_unused]] int data, bool gui_scope)
{
	if (!gui_scope) return;
	RebuildList();
	this->vscroll->SetCount(this->filtered_list.size());
	this->vscroll->SetCapacityFromWidget(parent_window, WID_SELECTOR_MATRIX);

	/* This does not assume that the IDs must be contiguous. */
	auto it = std::find(std::begin(this->filtered_list), std::end(this->filtered_list), this->selected_id);

	if (it != std::end(this->filtered_list)) {
		int selected_pos = std::distance(std::begin(this->filtered_list), it);
		this->vscroll->ScrollTowards(selected_pos);
	}
}

/**
 * Selector widget's UpdateWidgetSize method
 * This function is meant to be called from the parent's window's UpdateWidgetSize method
 *
 * @param widget  Widget number.
 * @param[In,out] size Size of the widget.
 * @param padding Recommended amount of space between the widget content and the widget edge.
 * @param[In,out] fill Fill step of the widget.
 * @param[In,out] resize Resize step of the widget.
 *
 * All of these parameters should be passed straight from the parent's window's UpdateWidgetSize
 * method, without any changes, or conditional checks.
 *
 * @see Window::UpdateWidgetSize()
 */
void SelectorWidget::UpdateWidgetSize(WidgetID widget, Dimension &size, const Dimension &padding, Dimension &fill, Dimension &resize)
{
	if (widget != WID_SELECTOR_MATRIX) return;

	const int min_rows = 11; ///< The minimal number of rows shown
	const int min_width = ScaleGUITrad(100); ///< The minimal width of the widget


	this->row_height = GetCharacterHeight(FS_NORMAL) + padding.height;

	size.height = this->row_height * min_rows;
	size.width = min_width;
	resize.width = 1;
	resize.height = this->row_height;
	fill.width = 1;
	fill.height = this->row_height;
}

/**
 * Selector widget's OnResize event handler
 * This function is meant to be called from the parent's window's OnResize
 * Event handler
 *
 * All of these parameters should be passed straight from the parent's window's OnResize
 * Event handler, without any changes, or conditional checks
 *
 * @see Window::OnResize()
 */
void SelectorWidget::OnResize()
{
	this->vscroll->SetCapacityFromWidget(parent_window, WID_SELECTOR_MATRIX);
}

/**
 * Selector widget's OnEditboxChanged event handler
 * This function is meant to be called from the parent's window's OnEditboxChanged
 * Event handler
 *
 * @param wid The widget id of the editbox.
 *
 * All of these parameters should be passed straight from the parent's window's OnEditboxChanged
 * Event handler, without any changes, or conditional checks
 *
 * @see Window::OnEditboxChanged()
 */
void SelectorWidget::OnEditboxChanged(WidgetID wid)
{
	if (wid != WID_SELECTOR_EDITBOX) return;

	this->string_filter.SetFilterTerm(this->editbox.text.buf);
	this->parent_window->InvalidateData(0);
	this->vscroll->SetCount(this->filtered_list.size());
}

/**
 * Selector widget's DrawWidget method
 * This function is meant to be called from the parent's window's DrawWidget
 * method
 *
 * @param widget  Widget number.
 * @param[In,out] size Size of the widget.
 * @param padding Recommended amount of space between the widget content and the widget edge.
 * @param[In,out] fill Fill step of the widget.
 * @param[In,out] resize Resize step of the widget.
 *
 * All of these parameters should be passed straight from the parent's window's DrawWidget
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
	if (widget != WID_SELECTOR_MATRIX) return;

	Rect line = r.WithHeight(row_height);

	auto [first, last] = this->vscroll->GetVisibleRangeIterators(filtered_list);

	for (auto it = first; it != last; ++it) {
		const Rect ir = line.Shrink(WidgetDimensions::scaled.framerect);

		if (this->shown[*it]) {
			DrawFrameRect(line, COLOUR_BROWN, FR_LOWERED);
		}
		this->DrawSection(*it, ir.Shrink(WidgetDimensions::scaled.matrix));

		line = line.Translate(0, row_height);
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
void SelectorWidget::RebuildList()
{
	this->list.clear();
	this->filtered_list.clear();
	this->PopulateList();
	uint32_t max = *std::max_element(std::begin(this->list), std::end(this->list));

	this->shown.reserve(max);
	for (size_t i = this->shown.size(); i <= max ; i++) {
		this->shown.push_back(true);
	}
}

/* virtual */ void CargoSelectorWidget::PopulateList()
{
	for (const CargoSpec *cargo : _sorted_standard_cargo_specs){
		this->list.push_back(cargo->Index());
		if (this->string_filter.IsEmpty()) {
			this->filtered_list.push_back(cargo->Index());
			continue;
		}
		this->string_filter.ResetState();

		SetDParam(0, cargo->name);
		this->string_filter.AddLine(GetString(STR_JUST_STRING));

		if (this->string_filter.GetState()) {
			this->filtered_list.push_back(cargo->Index());
		}
	}
}

/* virtual */ void CargoSelectorWidget::DrawSection(uint id, const Rect &r)
{
	const CargoID cargo_id = static_cast<CargoID>(id); ///< CargoID of the current row's cargo
	const CargoSpec *cargo = CargoSpec::Get(id); ///< CargoSpec of the current row's cargo

	const bool rtl = _current_text_dir == TD_RTL;

	int legend_height = GetCharacterHeight(FS_SMALL);
	int legend_width = legend_height * 9 / 6;

	Rect cargo_swatch = r.WithWidth(legend_width, rtl);
	cargo_swatch.top    = CenterBounds(r.top, r.bottom, legend_height) - 1;
	cargo_swatch.bottom = cargo_swatch.top + legend_height;

	/* Cargo-colour box with outline. */
	GfxFillRect(cargo_swatch, PC_BLACK);
	GfxFillRect(cargo_swatch.Shrink(WidgetDimensions::scaled.bevel), cargo->legend_colour);

	/* Cargo name. */
	SetDParam(0, cargo->name);
	const Rect text = r.Indent(legend_width + WidgetDimensions::scaled.hsep_wide, rtl);

	const TextColour colour = (this->selected_id.value_or(INVALID_OWNER) == cargo_id) ? TC_WHITE : TC_BLACK;
	DrawString(text.left, text.right, CenterBounds(text.top, text.bottom, GetCharacterHeight(FS_NORMAL)), STR_JUST_STRING, colour);
}


/* virtual */ void CompanySelectorWidget::PopulateList()
{
	for (const Company *c: Company::Iterate()) {
		this->list.push_back(c->index);
		if (this->string_filter.IsEmpty()) {
			this->filtered_list.push_back(c->index);
			continue;
		}
		this->string_filter.ResetState();

		SetDParam(0, c->index);
		this->string_filter.AddLine(GetString(STR_COMPANY_NAME));

		if (this->string_filter.GetState()) {
			this->filtered_list.push_back(c->index);
		}
	}
}

/* virtual */ void CompanySelectorWidget::DrawSection(uint id, const Rect &r)
{
	const CompanyID cid = static_cast<CompanyID>(id);
	assert(Company::IsValidID(cid));

	const bool rtl = _current_text_dir == TD_RTL;
	const Dimension icon_size = GetSpriteSize(SPR_COMPANY_ICON);

	DrawCompanyIcon(cid, rtl ? r.right - icon_size.width : r.left, CenterBounds(r.top, r.bottom, icon_size.height));

	const Rect text = r.Indent(icon_size.width + WidgetDimensions::scaled.hsep_wide, rtl);

	SetDParam(0, cid);

	const TextColour colour = (this->selected_id.value_or(INVALID_OWNER) == cid) ? TC_WHITE : TC_BLACK;
	DrawString(text.left, text.right, CenterBounds(text.top, text.bottom, GetCharacterHeight(FS_NORMAL)), STR_COMPANY_NAME, colour);
}
