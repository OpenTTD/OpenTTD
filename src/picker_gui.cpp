/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file picker_gui.cpp %File for dealing with picker windows */

#include "stdafx.h"
#include "core/backup_type.hpp"
#include "gui.h"
#include "hotkeys.h"
#include "picker_gui.h"
#include "querystring_gui.h"
#include "settings_type.h"
#include "sortlist_type.h"
#include "sound_func.h"
#include "sound_type.h"
#include "stringfilter_type.h"
#include "strings_func.h"
#include "widget_type.h"
#include "window_func.h"
#include "window_gui.h"
#include "window_type.h"
#include "zoom_func.h"

#include "widgets/picker_widget.h"

#include "safeguards.h"

/** Sort classes by id. */
static bool ClassIDSorter(int const &a, int const &b)
{
	return a < b;
}

/** Filter classes by class name. */
static bool ClassTagNameFilter(int const *item, PickerFilterData &filter)
{
	filter.ResetState();
	filter.AddLine(GetString(filter.callbacks->GetClassName(*item)));
	return filter.GetState();
}

/** Sort types by id. */
static bool TypeIDSorter(PickerItem const &a, PickerItem const &b)
{
	int r = a.class_index - b.class_index;
	if (r == 0) r = a.index - b.index;
	return r < 0;
}

/** Filter types by class name. */
static bool TypeTagNameFilter(PickerItem const *item, PickerFilterData &filter)
{
	filter.ResetState();
	filter.AddLine(GetString(filter.callbacks->GetTypeName(item->class_index, item->index)));
	return filter.GetState();
}

static const std::initializer_list<PickerClassList::SortFunction * const> _class_sorter_funcs = { &ClassIDSorter }; ///< Sort functions of the #PickerClassList
static const std::initializer_list<PickerClassList::FilterFunction * const> _class_filter_funcs = { &ClassTagNameFilter }; ///< Filter functions of the #PickerClassList.
static const std::initializer_list<PickerTypeList::SortFunction * const> _type_sorter_funcs = { TypeIDSorter }; ///< Sort functions of the #PickerTypeList.
static const std::initializer_list<PickerTypeList::FilterFunction * const> _type_filter_funcs = { TypeTagNameFilter }; ///< Filter functions of the #PickerTypeList.

PickerWindow::PickerWindow(WindowDesc *desc, Window *parent, int window_number, PickerCallbacks &callbacks) : PickerWindowBase(desc, parent), callbacks(callbacks),
	class_editbox(EDITBOX_MAX_SIZE * MAX_CHAR_LENGTH, EDITBOX_MAX_SIZE),
	type_editbox(EDITBOX_MAX_SIZE * MAX_CHAR_LENGTH, EDITBOX_MAX_SIZE)
{
	this->window_number = window_number;

	/* Init of nested tree is deferred.
	 * PickerWindow::ConstructWindow must be called by the inheriting window. */
}

void PickerWindow::ConstructWindow()
{
	this->CreateNestedTree();

	/* Test if pickers should be active.*/
	bool isActive = this->callbacks.IsActive();

	/* Functionality depends on widgets being present, not window class. */
	this->has_class_picker = isActive && this->GetWidget<NWidgetBase>(WID_PW_CLASS_LIST) != nullptr && this->callbacks.HasClassChoice();
	this->has_type_picker = isActive && this->GetWidget<NWidgetBase>(WID_PW_TYPE_MATRIX) != nullptr;

	if (this->has_class_picker) {
		this->GetWidget<NWidgetCore>(WID_PW_CLASS_LIST)->tool_tip = this->callbacks.GetClassTooltip();

		this->querystrings[WID_PW_CLASS_FILTER] = &this->class_editbox;
	} else {
		if (auto *nwid = this->GetWidget<NWidgetStacked>(WID_PW_CLASS_SEL); nwid != nullptr) {
			/* Check the container orientation. MakeNWidgets adds an additional NWID_VERTICAL container so we check the grand-parent. */
			bool is_vertical = (nwid->parent->parent->type == NWID_VERTICAL);
			nwid->SetDisplayedPlane(is_vertical ? SZSP_HORIZONTAL : SZSP_VERTICAL);
		}
	}

	this->class_editbox.cancel_button = QueryString::ACTION_CLEAR;
	this->class_string_filter.SetFilterTerm(this->class_editbox.text.buf);
	this->class_string_filter.callbacks = &this->callbacks;

	this->classes.SetListing(this->callbacks.class_last_sorting);
	this->classes.SetFiltering(this->callbacks.class_last_filtering);
	this->classes.SetSortFuncs(_class_sorter_funcs);
	this->classes.SetFilterFuncs(_class_filter_funcs);

	if (this->has_type_picker) {
		SetWidgetDisabledState(WID_PW_MODE_ALL, !this->callbacks.HasClassChoice());

		this->GetWidget<NWidgetCore>(WID_PW_TYPE_ITEM)->tool_tip = this->callbacks.GetTypeTooltip();

		auto *matrix = this->GetWidget<NWidgetMatrix>(WID_PW_TYPE_MATRIX);
		matrix->SetScrollbar(this->GetScrollbar(WID_PW_TYPE_SCROLL));

		this->querystrings[WID_PW_TYPE_FILTER] = &this->type_editbox;
	} else {
		if (auto *nwid = this->GetWidget<NWidgetStacked>(WID_PW_TYPE_SEL); nwid != nullptr) {
			/* Check the container orientation. MakeNWidgets adds an additional NWID_VERTICAL container so we check the grand-parent. */
			bool is_vertical = (nwid->parent->parent->type == NWID_VERTICAL);
			nwid->SetDisplayedPlane(is_vertical ? SZSP_HORIZONTAL : SZSP_VERTICAL);
		}
	}

	this->type_editbox.cancel_button = QueryString::ACTION_CLEAR;
	this->type_string_filter.SetFilterTerm(this->type_editbox.text.buf);
	this->type_string_filter.callbacks = &this->callbacks;

	this->types.SetListing(this->callbacks.type_last_sorting);
	this->types.SetFiltering(this->callbacks.type_last_filtering);
	this->types.SetSortFuncs(_type_sorter_funcs);
	this->types.SetFilterFuncs(_type_filter_funcs);

	this->FinishInitNested(this->window_number);

	this->InvalidateData(PFI_CLASS | PFI_TYPE | PFI_POSITION | PFI_VALIDATE);
}

void PickerWindow::Close(int data)
{
	this->callbacks.Close(data);
	this->PickerWindowBase::Close(data);
}

void PickerWindow::UpdateWidgetSize(WidgetID widget, Dimension &size, const Dimension &padding, Dimension &fill, Dimension &resize)
{
	switch (widget) {
		/* Class picker */
		case WID_PW_CLASS_LIST:
			resize.height = GetCharacterHeight(FS_NORMAL) + padding.height;
			size.height = 5 * resize.height;
			break;

		/* Type picker */
		case WID_PW_TYPE_MATRIX:
			/* At least two items wide. */
			size.width += resize.width;
			fill.width = resize.width;
			fill.height = 1;

			/* Resizing in X direction only at blob size, but at pixel level in Y. */
			resize.height = 1;
			break;

		/* Type picker */
		case WID_PW_TYPE_ITEM:
			size.width  = ScaleGUITrad(PREVIEW_WIDTH) + WidgetDimensions::scaled.fullbevel.Horizontal();
			size.height = ScaleGUITrad(PREVIEW_HEIGHT) + WidgetDimensions::scaled.fullbevel.Vertical();
			break;
	}
}

void PickerWindow::DrawWidget(const Rect &r, WidgetID widget) const
{
	switch (widget) {
		/* Class picker */
		case WID_PW_CLASS_LIST: {
			Rect ir = r.Shrink(WidgetDimensions::scaled.matrix);
			const int selected = this->callbacks.GetSelectedClass();
			const auto vscroll = this->GetScrollbar(WID_PW_CLASS_SCROLL);
			auto [first, last] = vscroll->GetVisibleRangeIterators(this->classes);
			for (auto it = first; it != last; ++it) {
				DrawString(ir, this->callbacks.GetClassName(*it), *it == selected ? TC_WHITE : TC_BLACK);
				ir.top += this->resize.step_height;
			}
			break;
		}

		/* Type picker */
		case WID_PW_TYPE_ITEM: {
			assert(this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->GetCurrentElement() < static_cast<int>(this->types.size()));
			const auto &item = this->types[this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->GetCurrentElement()];

			DrawPixelInfo tmp_dpi;
			Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
			if (FillDrawPixelInfo(&tmp_dpi, ir)) {
				AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
				int x = (ir.Width()  - ScaleSpriteTrad(PREVIEW_WIDTH)) / 2 + ScaleSpriteTrad(PREVIEW_LEFT);
				int y = (ir.Height() + ScaleSpriteTrad(PREVIEW_HEIGHT)) / 2 - ScaleSpriteTrad(PREVIEW_BOTTOM);

				this->callbacks.DrawType(x, y, item.class_index, item.index);
			}

			if (!this->callbacks.IsTypeAvailable(item.class_index, item.index)) {
				GfxFillRect(ir, GetColourGradient(COLOUR_GREY, SHADE_DARKER), FILLRECT_CHECKER);
			}
			break;
		}

		case WID_PW_TYPE_NAME:
			DrawString(r, this->callbacks.GetTypeName(this->callbacks.GetSelectedClass(), this->callbacks.GetSelectedType()), TC_ORANGE, SA_CENTER);
			break;
	}
}

void PickerWindow::OnResize()
{
	if (this->has_class_picker) {
		this->GetScrollbar(WID_PW_CLASS_SCROLL)->SetCapacityFromWidget(this, WID_PW_CLASS_LIST);
	}
}

void PickerWindow::OnClick(Point pt, WidgetID widget, int)
{
	switch (widget) {
		/* Class Picker */
		case WID_PW_CLASS_LIST: {
			const auto vscroll = this->GetWidget<NWidgetScrollbar>(WID_PW_CLASS_SCROLL);
			auto it = vscroll->GetScrolledItemFromWidget(this->classes, pt.y, this, WID_PW_CLASS_LIST);
			if (it == this->classes.end()) return;

			if (this->callbacks.GetSelectedClass() != *it || HasBit(this->callbacks.mode, PFM_ALL)) {
				ClrBit(this->callbacks.mode, PFM_ALL); // Disable showing all.
				SetWidgetLoweredState(WID_PW_MODE_ALL, false);
				this->callbacks.SetSelectedClass(*it);
				this->InvalidateData(PFI_TYPE | PFI_POSITION | PFI_VALIDATE);
			}
			if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
			CloseWindowById(WC_SELECT_STATION, 0);
			break;
		}

		case WID_PW_MODE_ALL:
			ToggleBit(this->callbacks.mode, widget - WID_PW_MODE_ALL);
			SetWidgetLoweredState(widget, HasBit(this->callbacks.mode, widget - WID_PW_MODE_ALL));
			this->InvalidateData(PFI_TYPE | PFI_POSITION);
			break;

		/* Type Picker */
		case WID_PW_TYPE_ITEM: {
			int sel = this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->GetCurrentElement();
			assert(sel < (int)this->types.size());
			const auto &item = this->types[sel];
			if (this->callbacks.IsTypeAvailable(item.class_index, item.index)) {
				this->callbacks.SetSelectedClass(item.class_index);
				this->callbacks.SetSelectedType(item.index);
				this->InvalidateData(PFI_POSITION);
			}
			if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
			CloseWindowById(WC_SELECT_STATION, 0);
			break;
		}
	}
}

void PickerWindow::OnInvalidateData(int data, bool gui_scope)
{
	if (!gui_scope) return;

	if ((data & PFI_CLASS) != 0) this->classes.ForceRebuild();
	if ((data & PFI_TYPE) != 0) this->types.ForceRebuild();

	this->BuildPickerClassList();
	if ((data & PFI_VALIDATE) != 0) this->EnsureSelectedClassIsValid();
	if ((data & PFI_POSITION) != 0) this->EnsureSelectedClassIsVisible();

	this->BuildPickerTypeList();
	if ((data & PFI_VALIDATE) != 0) this->EnsureSelectedTypeIsValid();
	if ((data & PFI_POSITION) != 0) this->EnsureSelectedTypeIsVisible();

	if (this->has_type_picker) {
		SetWidgetLoweredState(WID_PW_MODE_ALL, HasBit(this->callbacks.mode, PFM_ALL));
	}
}

EventState PickerWindow::OnHotkey(int hotkey)
{
	switch (hotkey) {
		case PCWHK_FOCUS_FILTER_BOX:
			/* Cycle between the two edit boxes. */
			if (this->has_type_picker && (this->nested_focus == nullptr || this->nested_focus->index != WID_PW_TYPE_FILTER)) {
				this->SetFocusedWidget(WID_PW_TYPE_FILTER);
			} else if (this->has_class_picker && (this->nested_focus == nullptr || this->nested_focus->index != WID_PW_CLASS_FILTER)) {
				this->SetFocusedWidget(WID_PW_CLASS_FILTER);
			}
			SetFocusedWindow(this);
			return ES_HANDLED;

		default:
			return ES_NOT_HANDLED;
	}
}

void PickerWindow::OnEditboxChanged(WidgetID wid)
{
	switch (wid) {
		case WID_PW_CLASS_FILTER:
			this->class_string_filter.SetFilterTerm(this->class_editbox.text.buf);
			this->classes.SetFilterState(!class_string_filter.IsEmpty());
			this->InvalidateData(PFI_CLASS);
			break;

		case WID_PW_TYPE_FILTER:
			this->type_string_filter.SetFilterTerm(this->type_editbox.text.buf);
			this->types.SetFilterState(!type_string_filter.IsEmpty());
			this->InvalidateData(PFI_TYPE);
			break;

		default:
			break;
	}
}

/** Builds the filter list of classes. */
void PickerWindow::BuildPickerClassList()
{
	if (!this->classes.NeedRebuild()) return;

	int count = this->callbacks.GetClassCount();

	this->classes.clear();
	this->classes.reserve(count);

	for (int i = 0; i < count; i++) {
		if (this->callbacks.GetClassName(i) == INVALID_STRING_ID) continue;
		this->classes.emplace_back(i);
	}

	this->classes.Filter(this->class_string_filter);
	this->classes.RebuildDone();
	this->classes.Sort();

	if (!this->has_class_picker) return;
	this->GetScrollbar(WID_PW_CLASS_SCROLL)->SetCount(this->classes.size());
}

void PickerWindow::EnsureSelectedClassIsValid()
{
	int class_index = this->callbacks.GetSelectedClass();
	if (std::binary_search(std::begin(this->classes), std::end(this->classes), class_index)) return;

	class_index = this->classes.front();
	this->callbacks.SetSelectedClass(class_index);
}

void PickerWindow::EnsureSelectedClassIsVisible()
{
	if (!this->has_class_picker) return;
	if (this->classes.empty()) return;

	auto it = std::find(std::begin(this->classes), std::end(this->classes), this->callbacks.GetSelectedClass());
	if (it == std::end(this->classes)) return;

	int pos = static_cast<int>(std::distance(std::begin(this->classes), it));
	this->GetScrollbar(WID_PW_CLASS_SCROLL)->ScrollTowards(pos);
}

/** Builds the filter list of types. */
void PickerWindow::BuildPickerTypeList()
{
	if (!this->types.NeedRebuild()) return;

	this->types.clear();
	bool show_all = HasBit(this->callbacks.mode, PFM_ALL);
	int cls_id = this->callbacks.GetSelectedClass();

	if (show_all) {
		/* Reserve enough space for everything. */
		int total = 0;
		for (int class_index : this->classes) total += this->callbacks.GetTypeCount(class_index);
		this->types.reserve(total);
		/* Add types in all classes. */
		for (int class_index : this->classes) {
			int count = this->callbacks.GetTypeCount(class_index);
			for (int i = 0; i < count; i++) {
				if (this->callbacks.GetTypeName(class_index, i) == INVALID_STRING_ID) continue;
				this->types.emplace_back(this->callbacks.GetPickerItem(class_index, i));
			}
		}
	} else {
		/* Add types in only the selected class. */
		if (cls_id >= 0 && cls_id < this->callbacks.GetClassCount()) {
			int count = this->callbacks.GetTypeCount(cls_id);
			this->types.reserve(count);
			for (int i = 0; i < count; i++) {
				if (this->callbacks.GetTypeName(cls_id, i) == INVALID_STRING_ID) continue;
				this->types.emplace_back(this->callbacks.GetPickerItem(cls_id, i));
			}
		}
	}

	this->types.Filter(this->type_string_filter);
	this->types.RebuildDone();
	this->types.Sort();

	if (!this->has_type_picker) return;
	this->GetWidget<NWidgetMatrix>(WID_PW_TYPE_MATRIX)->SetCount(static_cast<int>(this->types.size()));
}

void PickerWindow::EnsureSelectedTypeIsValid()
{
	int class_index = this->callbacks.GetSelectedClass();
	int index = this->callbacks.GetSelectedType();
	if (std::any_of(std::begin(this->types), std::end(this->types), [class_index, index](const auto &item) { return item.class_index == class_index && item.index == index; })) return;

	class_index = this->types.front().class_index;
	index = this->types.front().index;
	this->callbacks.SetSelectedClass(class_index);
	this->callbacks.SetSelectedType(index);
}

void PickerWindow::EnsureSelectedTypeIsVisible()
{
	if (!this->has_type_picker) return;
	if (this->types.empty()) {
		this->GetWidget<NWidgetMatrix>(WID_PW_TYPE_MATRIX)->SetClicked(-1);
		return;
	}

	int class_index = this->callbacks.GetSelectedClass();
	int index = this->callbacks.GetSelectedType();

	auto it = std::find_if(std::begin(this->types), std::end(this->types), [class_index, index](const auto &item) { return item.class_index == class_index && item.index == index; });
	if (it == std::end(this->types)) return;

	int pos = static_cast<int>(std::distance(std::begin(this->types), it));
	this->GetWidget<NWidgetMatrix>(WID_PW_TYPE_MATRIX)->SetClicked(pos);
}

/** Create nested widgets for the class picker widgets. */
std::unique_ptr<NWidgetBase> MakePickerClassWidgets()
{
	static constexpr NWidgetPart picker_class_widgets[] = {
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_PW_CLASS_SEL),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
					NWidget(WWT_EDITBOX, COLOUR_DARK_GREEN, WID_PW_CLASS_FILTER), SetMinimalSize(144, 0), SetPadding(2), SetFill(1, 0), SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
				EndContainer(),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
						NWidget(WWT_MATRIX, COLOUR_GREY, WID_PW_CLASS_LIST), SetFill(1, 1), SetResize(1, 1), SetPadding(WidgetDimensions::unscaled.picker),
								SetMatrixDataTip(1, 0, STR_NULL), SetScrollbar(WID_PW_CLASS_SCROLL),
					EndContainer(),
					NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_PW_CLASS_SCROLL),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	};

	return MakeNWidgets(std::begin(picker_class_widgets), std::end(picker_class_widgets), nullptr);
}

/** Create nested widgets for the type picker widgets. */
std::unique_ptr<NWidgetBase> MakePickerTypeWidgets()
{
	static constexpr NWidgetPart picker_type_widgets[] = {
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_PW_TYPE_SEL),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
					NWidget(WWT_EDITBOX, COLOUR_DARK_GREEN, WID_PW_TYPE_FILTER), SetPadding(2), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_TEXTBTN, COLOUR_DARK_GREEN, WID_PW_MODE_ALL), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_PICKER_MODE_ALL, STR_PICKER_MODE_ALL_TOOLTIP),
				EndContainer(),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetScrollbar(WID_PW_TYPE_SCROLL),
						NWidget(NWID_MATRIX, COLOUR_DARK_GREEN, WID_PW_TYPE_MATRIX), SetPIP(0, 2, 0), SetPadding(WidgetDimensions::unscaled.picker),
							NWidget(WWT_PANEL, COLOUR_GREY, WID_PW_TYPE_ITEM), SetScrollbar(WID_PW_TYPE_SCROLL),
							EndContainer(),
						EndContainer(),
					EndContainer(),
					NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_PW_TYPE_SCROLL),
				EndContainer(),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
						NWidget(WWT_EMPTY, INVALID_COLOUR, WID_PW_TYPE_NAME), SetPadding(WidgetDimensions::unscaled.framerect), SetResize(1, 0), SetFill(1, 0), SetMinimalTextLines(1, 0),
					EndContainer(),
					NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN, WID_PW_TYPE_RESIZE),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	};

	return MakeNWidgets(std::begin(picker_type_widgets), std::end(picker_type_widgets), nullptr);
}
