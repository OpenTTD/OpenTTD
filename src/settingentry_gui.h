/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file settingentry_gui.h Declarations of classes for handling display of individual configuration settings. */

#ifndef SETTINGENTRY_GUI_H
#define SETTINGENTRY_GUI_H

#include "core/enum_type.hpp"
#include "settings_internal.h"
#include "stringfilter_type.h"

extern Dimension _setting_circle_size;
extern int SETTING_HEIGHT;

/**
 * Flags for #SettingEntry
 * @note The #SEF_BUTTONS_MASK matches expectations of the formal parameter 'state' of #DrawArrowButtons
 */
enum class SettingEntryFlag : uint8_t {
	LeftDepressed, ///< Of a numeric setting entry, the left button is depressed
	RightDepressed, ///< Of a numeric setting entry, the right button is depressed
	LastField, ///< This entry is the last one in a (sub-)page
	Filtered, ///< Entry is hidden by the string filter
};
using SettingEntryFlags = EnumBitSet<SettingEntryFlag, uint8_t>;

static constexpr SettingEntryFlags SEF_BUTTONS_MASK = {SettingEntryFlag::LeftDepressed, SettingEntryFlag::RightDepressed}; ///< Mask for button flags

/** How the list of advanced settings is filtered. */
enum RestrictionMode : uint8_t {
	RM_BASIC,                            ///< Display settings associated to the "basic" list.
	RM_ADVANCED,                         ///< Display settings associated to the "advanced" list.
	RM_ALL,                              ///< List all settings regardless of the default/newgame/... values.
	RM_CHANGED_AGAINST_DEFAULT,          ///< Show only settings which are different compared to default values.
	RM_CHANGED_AGAINST_NEW,              ///< Show only settings which are different compared to the user's new game setting values.
	RM_END,                              ///< End for iteration.
};
DECLARE_INCREMENT_DECREMENT_OPERATORS(RestrictionMode)

/** Filter for settings list. */
struct SettingFilter {
	StringFilter string;     ///< Filter string.
	RestrictionMode min_cat; ///< Minimum category needed to display all filtered strings (#RM_BASIC, #RM_ADVANCED, or #RM_ALL).
	bool type_hides;         ///< Whether the type hides filtered strings.
	RestrictionMode mode;    ///< Filter based on category.
	SettingType type;        ///< Filter based on type.
};

/** Data structure describing a single setting in a tab */
struct BaseSettingEntry {
	SettingEntryFlags flags; ///< Flags of the setting entry. @see SettingEntryFlags
	uint8_t level; ///< Nesting level of this setting entry

	BaseSettingEntry() : flags(), level(0) {}
	virtual ~BaseSettingEntry() = default;

	virtual void Init(uint8_t level = 0);
	virtual void FoldAll() {}
	virtual void UnFoldAll() {}
	virtual void ResetAll() = 0;

	/**
	 * Set whether this is the last visible entry of the parent node.
	 * @param last_field Value to set
	 */
	void SetLastField(bool last_field) { this->flags.Set(SettingEntryFlag::LastField, last_field); }

	virtual uint Length() const = 0;
	virtual void GetFoldingState([[maybe_unused]] bool &all_folded, [[maybe_unused]] bool &all_unfolded) const {}
	virtual bool IsVisible(const BaseSettingEntry *item) const;
	virtual BaseSettingEntry *FindEntry(uint row, uint *cur_row);
	virtual uint GetMaxHelpHeight([[maybe_unused]] int maxw) { return 0; }

	/**
	 * Check whether an entry is hidden due to filters
	 * @return true if hidden.
	 */
	bool IsFiltered() const { return this->flags.Test(SettingEntryFlag::Filtered); }

	virtual bool UpdateFilterState(SettingFilter &filter, bool force_visible) = 0;

	virtual uint Draw(GameSettings *settings_ptr, int left, int right, int y, uint first_row, uint max_row, BaseSettingEntry *selected, uint cur_row = 0, uint parent_last = 0) const;

protected:
	virtual void DrawSetting(GameSettings *settings_ptr, int left, int right, int y, bool highlight) const = 0;
};

/** Standard setting */
struct SettingEntry : BaseSettingEntry {
	const char *name;              ///< Name of the setting
	const IntSettingDesc *setting; ///< Setting description of the setting

	SettingEntry(const char *name);

	void Init(uint8_t level = 0) override;
	void ResetAll() override;
	uint Length() const override;
	uint GetMaxHelpHeight(int maxw) override;
	bool UpdateFilterState(SettingFilter &filter, bool force_visible) override;

	void SetButtons(SettingEntryFlags new_val);

protected:
	void DrawSetting(GameSettings *settings_ptr, int left, int right, int y, bool highlight) const override;

private:
	bool IsVisibleByRestrictionMode(RestrictionMode mode) const;
};

/** Containers for BaseSettingEntry */
struct SettingsContainer {
	typedef std::vector<BaseSettingEntry*> EntryVector;
	EntryVector entries; ///< Settings on this page

	template <typename T>
	T *Add(T *item)
	{
		this->entries.push_back(item);
		return item;
	}

	void Init(uint8_t level = 0);
	void ResetAll();
	void FoldAll();
	void UnFoldAll();

	uint Length() const;
	void GetFoldingState(bool &all_folded, bool &all_unfolded) const;
	bool IsVisible(const BaseSettingEntry *item) const;
	BaseSettingEntry *FindEntry(uint row, uint *cur_row);
	uint GetMaxHelpHeight(int maxw);

	bool UpdateFilterState(SettingFilter &filter, bool force_visible);

	uint Draw(GameSettings *settings_ptr, int left, int right, int y, uint first_row, uint max_row, BaseSettingEntry *selected, uint cur_row = 0, uint parent_last = 0) const;
};

/** Data structure describing one page of settings in the settings window. */
struct SettingsPage : BaseSettingEntry, SettingsContainer {
	StringID title;     ///< Title of the sub-page
	bool folded;        ///< Sub-page is folded (not visible except for its title)

	SettingsPage(StringID title);

	void Init(uint8_t level = 0) override;
	void ResetAll() override;
	void FoldAll() override;
	void UnFoldAll() override;

	uint Length() const override;
	void GetFoldingState(bool &all_folded, bool &all_unfolded) const override;
	bool IsVisible(const BaseSettingEntry *item) const override;
	BaseSettingEntry *FindEntry(uint row, uint *cur_row) override;
	uint GetMaxHelpHeight(int maxw) override { return SettingsContainer::GetMaxHelpHeight(maxw); }

	bool UpdateFilterState(SettingFilter &filter, bool force_visible) override;

	uint Draw(GameSettings *settings_ptr, int left, int right, int y, uint first_row, uint max_row, BaseSettingEntry *selected, uint cur_row = 0, uint parent_last = 0) const override;

protected:
	void DrawSetting(GameSettings *settings_ptr, int left, int right, int y, bool highlight) const override;
};

SettingsContainer &GetSettingsTree();
const void *ResolveObject(const GameSettings *settings_ptr, const IntSettingDesc *sd);

#endif /* SETTINGENTRY_GUI_H */

