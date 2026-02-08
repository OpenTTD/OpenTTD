/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file picker_gui.h Functions/types etc. related to the picker GUI. */

#ifndef PICKER_GUI_H
#define PICKER_GUI_H

#include "newgrf_badge.h"
#include "newgrf_badge_gui.h"
#include "querystring_gui.h"
#include "sortlist_type.h"
#include "stringfilter_type.h"
#include "strings_type.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_window.h"
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
	explicit PickerCallbacks(const std::string &ini_group);
	virtual ~PickerCallbacks();

	/** @copydoc Window::Close */
	virtual void Close([[maybe_unused]] int data) { }

	/**
	 * NewGRF feature this picker is for.
	 * @return The associated NewGRF feature.
	 */
	virtual GrfSpecFeature GetFeature() const = 0;
	/**
	 * Should picker class/type selection be enabled?
	 * @return \c true when to show the class/type picker.
	 */
	virtual bool IsActive() const = 0;
	/**
	 * Are there multiple classes to chose from?
	 * @return \c true when there are multiple classes for this picker.
	 */
	virtual bool HasClassChoice() const = 0;

	/* Class callbacks */
	/**
	 * Get the tooltip string for the class list.
	 * @return StringID without parameters for the tooltip.
	 */
	virtual StringID GetClassTooltip() const = 0;
	/**
	 * Get the number of classes.
	 * @return Number of classes.
	 * @note Used only to estimate space requirements.
	 */
	virtual int GetClassCount() const = 0;
	/**
	 * Get the index of the selected class.
	 * @return The previously selected class.
	 */
	virtual int GetSelectedClass() const = 0;
	/**
	 * Set the selected class.
	 * @param id The new selected class.
	 */
	virtual void SetSelectedClass(int id) const = 0;
	/**
	 * Get the name of a class.
	 * @param id The class to get the name for.
	 * @return StringID without parameters.
	 */
	virtual StringID GetClassName(int id) const = 0;

	/* Type callbacks */
	/**
	 * Get the tooltip string for the type grid.
	 * @return StringID without parameters.
	 */
	virtual StringID GetTypeTooltip() const = 0;
	/**
	 * Get the number of types in a class.
	 * @param cls_id The class' identifier.
	 * @return The number of types.
	 * @note Used only to estimate space requirements.
	 */
	virtual int GetTypeCount(int cls_id) const = 0;
	/**
	 * Get the selected type.
	 * @return Previously selected type.
	 */
	virtual int GetSelectedType() const = 0;
	/**
	 * Set the selected type.
	 * @param id The new type.
	 */
	virtual void SetSelectedType(int id) const = 0;
	/**
	 * Get data about an item.
	 * @param cls_id The chosen class.
	 * @param id The chosen type within the class.
	 * @return The metadata about the item.
	 */
	virtual PickerItem GetPickerItem(int cls_id, int id) const = 0;
	/**
	 * Get the item name of a type.
	 * @param cls_id The chosen class.
	 * @param id The chosen type within the class.
	 * @return The name of the item.
	 */
	virtual StringID GetTypeName(int cls_id, int id) const = 0;
	/**
	 * Get the item's badges of a type.
	 * @param cls_id The chosen class.
	 * @param id The chosen type within the class.
	 * @return The badge IDs.
	 */
	virtual std::span<const BadgeID> GetTypeBadges(int cls_id, int id) const = 0;
	/**
	 * Test if an item is currently buildable.
	 * @param cls_id The chosen class.
	 * @param id The chosen type within the class.
	 * @return \c true if the combination is buildable.
	 */
	virtual bool IsTypeAvailable(int cls_id, int id) const = 0;
	/**
	 * Draw preview image of an item.
	 * @param x The X-position for the sprite to draw.
	 * @param y The Y-position for the sprite to draw.
	 * @param cls_id The chosen class.
	 * @param id The chosen type within the class.
	 */
	virtual void DrawType(int x, int y, int cls_id, int id) const = 0;

	/* Collection Callbacks */
	/**
	 * Get the tooltip string for the collection list.
	 * @return StringID without parameters.
	 */
	virtual StringID GetCollectionTooltip() const = 0;

	/**
	 * Fill a set with all items that are used by the current player.
	 * @param items The set to fill.
	 */
	virtual void FillUsedItems(std::set<PickerItem> &items) = 0;
	/**
	 * Update link between grfid/localidx and class_index/index in saved items.
	 * @param src Mapping of name to set of PickerItems with only grfid and localidx set.
	 * @return Mapping of name to fully populated PickerItems for loaded NewGRFs and 'src' items when the NewGRF is not loaded.
	 */
	virtual std::map<std::string, std::set<PickerItem>> UpdateSavedItems(const std::map<std::string, std::set<PickerItem>> &src) = 0;
	/**
	 * Initialize the list of active collections for sorting purposes.
	 * @param collections The map of collections to check.
	 * @return The set of collections with inactive items.
	 */
	inline std::set<std::string> InitializeInactiveCollections(const std::map<std::string, std::set<PickerItem>> collections)
	{
		std::set<std::string> inactive;

		for (const auto &collection : collections) {
			if ((collection.second.size() == 1 && collection.second.contains({})) || collection.first == "") continue;
			for (const PickerItem &item : collection.second) {
				if (item.class_index == -1 || item.index == -1) {
					inactive.emplace(collection.first);
					break;
				}
				if (GetTypeName(item.class_index, item.index) == INVALID_STRING_ID) {
					inactive.emplace(collection.first);
					break;
				}
			}
		}
		return inactive;
	}

	Listing class_last_sorting = { false, 0 }; ///< Default sorting of #PickerClassList.
	Filtering class_last_filtering = { false, 0 }; ///< Default filtering of #PickerClassList.

	Listing type_last_sorting = { false, 0 }; ///< Default sorting of #PickerTypeList.
	Filtering type_last_filtering = { false, 0 }; ///< Default filtering of #PickerTypeList.

	Listing collection_last_sorting = { false, 0 }; ///< Default sorting of #PickerCollectionList.

	const std::string ini_group; ///< Ini Group for saving favourites.
	uint8_t mode = 0; ///< Bitmask of \c PickerFilterModes.
	bool rename_collection = false;      ///< Are we renaming a collection?
	std::string sel_collection;          ///< Currently selected collection of saved items.
	std::string edit_collection;         ///< Collection to rename or delete.
	std::set<std::string> rm_collections; ///< Set of removed or renamed collections for updating ini file.

	int preview_height = 0; ///< Previously adjusted height.

	std::set<PickerItem> used; ///< Set of items used in the current game by the current company.
	std::map<std::string, std::set<PickerItem>> saved; ///< Set of saved collections of items.
};

/** Helper for PickerCallbacks when the class system is based on NewGRFClass. */
template <typename T>
class PickerCallbacksNewGRFClass : public PickerCallbacks {
public:
	explicit PickerCallbacksNewGRFClass(const std::string &ini_group) : PickerCallbacks(ini_group) {}

	inline typename T::index_type GetClassIndex(int cls_id) const { return static_cast<typename T::index_type>(cls_id); }
	inline const T *GetClass(int cls_id) const { return T::Get(this->GetClassIndex(cls_id)); }
	inline const typename T::spec_type *GetSpec(int cls_id, int id) const { return this->GetClass(cls_id)->GetSpec(id); }

	bool HasClassChoice() const override { return T::GetUIClassCount() > 1; }

	int GetClassCount() const override { return T::GetClassCount(); }
	int GetTypeCount(int cls_id) const override { return this->GetClass(cls_id)->GetSpecCount(); }

	PickerItem GetPickerItem(const typename T::spec_type *spec, int cls_id = -1, int id = -1) const
	{
		if (spec == nullptr) return {0, 0, cls_id, id};
		return {spec->grf_prop.grfid, spec->grf_prop.local_id, spec->class_index, spec->index};
	}

	PickerItem GetPickerItem(int cls_id, int id) const override
	{
		return GetPickerItem(GetClass(cls_id)->GetSpec(id), cls_id, id);
	}

	std::map<std::string, std::set<PickerItem>> UpdateSavedItems(const std::map<std::string, std::set<PickerItem>> &src) override
	{
		if (src.empty()) return {};

		std::map<std::string, std::set<PickerItem>> dst;
		for (auto it = src.begin(); it != src.end(); it++) {
			if (it->second.empty() || (it->second.size() == 1 && it->second.contains({}))) {
				dst[it->first];
				continue;
			}

			for (const auto &item : it->second) {
				const auto *spec = T::GetByGrf(item.grfid, item.local_id);
				if (spec == nullptr) {
					dst[it->first].emplace(item.grfid, item.local_id, -1, -1);
				} else {
					dst[it->first].emplace(GetPickerItem(spec));
				}
			}
		}
		return dst;
	}
};

struct PickerFilterData : StringFilter {
	const PickerCallbacks *callbacks; ///< Callbacks for filter functions to access to callbacks.
	std::optional<BadgeTextFilter> btf;
	std::optional<BadgeDropdownFilter> bdf;
};

using PickerClassList = GUIList<int, std::nullptr_t, PickerFilterData &>; ///< GUIList holding classes to display.
using PickerTypeList = GUIList<PickerItem, std::nullptr_t, PickerFilterData &>; ///< GUIList holding classes/types to display.
using PickerCollectionList = GUIList<std::string, std::nullptr_t, PickerFilterData &>; ///< GUIList holding collections to display.

class PickerWindow : public PickerWindowBase {
public:
	enum PickerFilterModes : uint8_t {
		PFM_ALL = 0, ///< Show all classes.
		PFM_USED = 1, ///< Show used types.
		PFM_SAVED = 2, ///< Show saved types.
	};

	/** The things of a picker that can be invalidated. */
	enum class PickerInvalidation : uint8_t {
		Class, ///< Refresh the class list.
		Type, ///< Refresh the type list.
		Collection, ///< Refresh the collection list.
		Position, ///< Update scroll positions.
		Validate, ///< Validate selected item.
		Filter, ///< Update filter state.
	};
	using PickerInvalidations = EnumBitSet<PickerInvalidation, uint8_t>;

	static constexpr PickerInvalidations PICKER_INVALIDATION_ALL{PickerInvalidation::Class, PickerInvalidation::Type, PickerInvalidation::Position, PickerInvalidation::Validate};

	static constexpr int PREVIEW_WIDTH = 64; ///< Width of each preview button.
	static constexpr int PREVIEW_HEIGHT = 48; ///< Height of each preview button.
	static constexpr int PREVIEW_LEFT = 31; ///< Offset from left edge to draw preview.
	static constexpr int PREVIEW_BOTTOM = 31; ///< Offset from bottom edge to draw preview.

	static constexpr int STEP_PREVIEW_HEIGHT = 16; ///< Step for decreasing or increase preview button height.
	static constexpr int MAX_PREVIEW_HEIGHT = PREVIEW_HEIGHT * 3; ///< Maximum height of each preview button.

	static constexpr uint EDITBOX_MAX_SIZE = 16; ///< The maximum number of characters for the filter edit box.

	bool has_class_picker = false; ///< Set if this window has a class picker 'component'.
	bool has_type_picker = false; ///< Set if this window has a type picker 'component'.
	bool has_collection_picker = false; ///< Set if this window has a collection picker 'component'.
	int preview_height = 0; ///< Height of preview images.
	std::set<std::string> inactive; ///< Set of collections with inactive items.

	PickerWindow(WindowDesc &desc, Window *parent, int window_number, PickerCallbacks &callbacks);
	void OnInit() override;
	void Close(int data = 0) override;
	void UpdateWidgetSize(WidgetID widget, Dimension &size, const Dimension &padding, Dimension &fill, Dimension &resize) override;
	std::string GetWidgetString(WidgetID widget, StringID stringid) const override;
	DropDownList BuildCollectionDropDownList();
	void DrawWidget(const Rect &r, WidgetID widget) const override;
	void OnDropdownSelect(WidgetID widget, int index, int click_result) override;
	void OnResize() override;
	void static DeletePickerCollectionCallback(Window *win, bool confirmed);
	void OnClick(Point pt, WidgetID widget, int click_count) override;
	void OnQueryTextFinished(std::optional<std::string> str) override;
	void OnInvalidateData(int data = 0, bool gui_scope = true) override;
	EventState OnHotkey(int hotkey) override;
	void OnEditboxChanged(WidgetID wid) override;

	/** Enum referring to the Hotkeys in the picker window */
	enum PickerClassWindowHotkeys : int32_t {
		PCWHK_FOCUS_FILTER_BOX, ///< Focus the edit box for editing the filter string
	};

	void InvalidateData(PickerInvalidations data) { this->Window::InvalidateData(data.base()); }

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

	void RefreshUsedTypeList();
	void BuildPickerTypeList();
	void EnsureSelectedTypeIsValid();
	void EnsureSelectedTypeIsVisible();

	PickerCollectionList collections; ///< List of collections.

	void BuildPickerCollectionList();

	GUIBadgeClasses badge_classes;
	std::pair<WidgetID, WidgetID> badge_filters{};
	BadgeFilterChoices badge_filter_choices{};

	const IntervalTimer<TimerGameCalendar> yearly_interval = {{TimerGameCalendar::Trigger::Year, TimerGameCalendar::Priority::None}, [this](auto) {
		this->SetDirty();
	}};

	const IntervalTimer<TimerWindow> refresh_interval = {std::chrono::seconds(3), [this](auto) {
		RefreshUsedTypeList();
	}};
};

class NWidgetBase;
std::unique_ptr<NWidgetBase> MakePickerClassWidgets();
std::unique_ptr<NWidgetBase> MakePickerTypeWidgets();

#endif /* PICKER_GUI_H */
