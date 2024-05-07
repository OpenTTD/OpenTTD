/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file picker_gui.h Functions/types etc. related to the picker GUI. */

#ifndef PICKER_GUI_H
#define PICKER_GUI_H

#include "querystring_gui.h"
#include "sortlist_type.h"
#include "stringfilter_type.h"
#include "strings_type.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"
#include "window_gui.h"
#include "window_type.h"

struct PickerItem {
	uint32_t grfid;
	uint16_t local_id;
	int class_index;
	int index;

	inline auto operator<=>(const PickerItem &other) const
	{
		if (auto cmp = this->grfid <=> other.grfid; cmp != 0) return cmp;
		return this->local_id <=> other.local_id;
	}
};

/** Class for PickerClassWindow to collect information and retain state. */
class PickerCallbacks {
public:
	virtual ~PickerCallbacks() {}
	virtual void Close(int) { }

	/** Should picker class/type selection be enabled? */
	virtual bool IsActive() const = 0;
	/** Are there multiple classes to chose from? */
	virtual bool HasClassChoice() const = 0;

	/* Class callbacks */
	/** Get the tooltip string for the class list. */
	virtual StringID GetClassTooltip() const = 0;
	/** Get the number of classes. @note Used only to estimate space requirements. */
	virtual int GetClassCount() const = 0;
	/** Get the index of the selected class. */
	virtual int GetSelectedClass() const = 0;
	/** Set the selected class. */
	virtual void SetSelectedClass(int id) const = 0;
	/** Get the name of a class. */
	virtual StringID GetClassName(int id) const = 0;

	/* Type callbacks */
	/** Get the tooltip string for the type grid. */
	virtual StringID GetTypeTooltip() const = 0;
	/** Get the number of types in a class. @note Used only to estimate space requirements. */
	virtual int GetTypeCount(int cls_id) const = 0;

	/** Get the selected type. */
	virtual int GetSelectedType() const = 0;
	/** Set the selected type. */
	virtual void SetSelectedType(int id) const = 0;
	/** Get data about an item. */
	virtual PickerItem GetPickerItem(int cls_id, int id) const = 0;
	/** Get the item of a type. */
	virtual StringID GetTypeName(int cls_id, int id) const = 0;
	/** Test if an item is currently buildable. */
	virtual bool IsTypeAvailable(int cls_id, int id) const = 0;
	/** Draw preview image of an item. */
	virtual void DrawType(int x, int y, int cls_id, int id) const = 0;

	Listing class_last_sorting = { false, 0 }; ///< Default sorting of #PickerClassList.
	Filtering class_last_filtering = { false, 0 }; ///< Default filtering of #PickerClassList.

	Listing type_last_sorting = { false, 0 }; ///< Default sorting of #PickerTypeList.
	Filtering type_last_filtering = { false, 0 }; ///< Default filtering of #PickerTypeList.
};

/** Helper for PickerCallbacks when the class system is based on NewGRFClass. */
template <typename T>
class PickerCallbacksNewGRFClass : public PickerCallbacks {
public:
	inline typename T::index_type GetClassIndex(int cls_id) const { return static_cast<typename T::index_type>(cls_id); }
	inline const T *GetClass(int cls_id) const { return T::Get(this->GetClassIndex(cls_id)); }
	inline const typename T::spec_type *GetSpec(int cls_id, int id) const { return this->GetClass(cls_id)->GetSpec(id); }

	bool HasClassChoice() const override { return T::GetUIClassCount() > 1; }

	int GetClassCount() const override { return T::GetClassCount(); }
	int GetTypeCount(int cls_id) const override { return this->GetClass(cls_id)->GetSpecCount(); }

	PickerItem GetPickerItem(const typename T::spec_type *spec, int cls_id = -1, int id = -1) const
	{
		if (spec == nullptr) return {0, 0, cls_id, id};
		return {spec->grf_prop.grffile == nullptr ? 0 : spec->grf_prop.grffile->grfid, spec->grf_prop.local_id, spec->class_index, spec->index};
	}

	PickerItem GetPickerItem(int cls_id, int id) const override
	{
		return GetPickerItem(GetClass(cls_id)->GetSpec(id), cls_id, id);
	}
};

struct PickerFilterData : StringFilter {
	const PickerCallbacks *callbacks; ///< Callbacks for filter functions to access to callbacks.
};

using PickerClassList = GUIList<int, std::nullptr_t, PickerFilterData &>; ///< GUIList holding classes to display.
using PickerTypeList = GUIList<PickerItem, std::nullptr_t, PickerFilterData &>; ///< GUIList holding classes/types to display.

class PickerWindow : public PickerWindowBase {
public:
	enum PickerFilterInvalidation {
		PFI_CLASS = 1U << 0, ///< Refresh the class list.
		PFI_TYPE = 1U << 1, ///< Refresh the type list.
		PFI_POSITION = 1U << 2, ///< Update scroll positions.
		PFI_VALIDATE = 1U << 3, ///< Validate selected item.
	};

	static const int PREVIEW_WIDTH = 64; ///< Width of each preview button.
	static const int PREVIEW_HEIGHT = 48; ///< Height of each preview button.
	static const int PREVIEW_LEFT = 31; ///< Offset from left edge to draw preview.
	static const int PREVIEW_BOTTOM = 31; ///< Offset from bottom edge to draw preview.

	static const uint EDITBOX_MAX_SIZE = 16; ///< The maximum number of characters for the filter edit box.

	bool has_class_picker = false; ///< Set if this window has a class picker 'component'.
	bool has_type_picker = false; ///< Set if this window has a type picker 'component'.

	PickerWindow(WindowDesc *desc, Window *parent, int window_number, PickerCallbacks &callbacks);
	void Close(int data = 0) override;
	void UpdateWidgetSize(WidgetID widget, Dimension &size, const Dimension &padding, Dimension &fill, Dimension &resize) override;
	void DrawWidget(const Rect &r, WidgetID widget) const override;
	void OnResize() override;
	void OnClick(Point pt, WidgetID widget, int click_count) override;
	void OnInvalidateData(int data = 0, bool gui_scope = true) override;
	EventState OnHotkey(int hotkey) override;
	void OnEditboxChanged(WidgetID wid) override;

	/** Enum referring to the Hotkeys in the picker window */
	enum PickerClassWindowHotkeys {
		PCWHK_FOCUS_FILTER_BOX, ///< Focus the edit box for editing the filter string
	};

protected:
	void ConstructWindow();

	PickerCallbacks &callbacks;

private:
	PickerClassList classes; ///< List of classes.
	PickerFilterData class_string_filter;
	QueryString class_editbox; ///< Filter editbox.

	void BuildPickerClassList();
	void EnsureSelectedClassIsValid();
	void EnsureSelectedClassIsVisible();

	PickerTypeList types; ///< List of types.
	PickerFilterData type_string_filter;
	QueryString type_editbox; ///< Filter editbox

	void BuildPickerTypeList();
	void EnsureSelectedTypeIsValid();
	void EnsureSelectedTypeIsVisible();

	IntervalTimer<TimerGameCalendar> yearly_interval = {{TimerGameCalendar::YEAR, TimerGameCalendar::Priority::NONE}, [this](auto) {
		this->SetDirty();
	}};
};

class NWidgetBase;
std::unique_ptr<NWidgetBase> MakePickerClassWidgets();
std::unique_ptr<NWidgetBase> MakePickerTypeWidgets();

#endif /* PICKER_GUI_H */
