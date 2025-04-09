/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file settingentry_gui.cpp Definitions of classes for handling display of individual configuration settings. */

#include "stdafx.h"
#include "company_base.h"
#include "company_func.h"
#include "settingentry_gui.h"
#include "settings_gui.h"
#include "settings_internal.h"
#include "stringfilter_type.h"
#include "strings_func.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"


/* == BaseSettingEntry methods == */

/**
 * Initialization of a setting entry
 * @param level      Page nesting level of this entry
 */
void BaseSettingEntry::Init(uint8_t level)
{
	this->level = level;
}

/**
 * Check whether an entry is visible and not folded or filtered away.
 * Note: This does not consider the scrolling range; it might still require scrolling to make the setting really visible.
 * @param item Entry to search for.
 * @return true if entry is visible.
 */
bool BaseSettingEntry::IsVisible(const BaseSettingEntry *item) const
{
	if (this->IsFiltered()) return false;
	return this == item;
}

/**
 * Find setting entry at row \a row_num
 * @param row_num Index of entry to return
 * @param cur_row Current row number
 * @return The requested setting entry or \c nullptr if it not found (folded or filtered)
 */
BaseSettingEntry *BaseSettingEntry::FindEntry(uint row_num, uint *cur_row)
{
	if (this->IsFiltered()) return nullptr;
	if (row_num == *cur_row) return this;
	(*cur_row)++;
	return nullptr;
}

/**
 * Draw a row in the settings panel.
 *
 * The scrollbar uses rows of the page, while the page data structure is a tree of #SettingsPage and #SettingEntry objects.
 * As a result, the drawing routing traverses the tree from top to bottom, counting rows in \a cur_row until it reaches \a first_row.
 * Then it enables drawing rows while traversing until \a max_row is reached, at which point drawing is terminated.
 *
 * The \a parent_last parameter ensures that the vertical lines at the left are
 * only drawn when another entry follows, that it prevents output like
 * \verbatim
 *  |-- setting
 *  |-- (-) - Title
 *  |    |-- setting
 *  |    |-- setting
 * \endverbatim
 * The left-most vertical line is not wanted. It is prevented by setting the
 * appropriate bit in the \a parent_last parameter.
 *
 * @param settings_ptr Pointer to current values of all settings
 * @param left         Left-most position in window/panel to start drawing \a first_row
 * @param right        Right-most x position to draw strings at.
 * @param y            Upper-most position in window/panel to start drawing \a first_row
 * @param first_row    First row number to draw
 * @param max_row      Row-number to stop drawing (the row-number of the row below the last row to draw)
 * @param selected     Selected entry by the user.
 * @param cur_row      Current row number (internal variable)
 * @param parent_last  Last-field booleans of parent page level (page level \e i sets bit \e i to 1 if it is its last field)
 * @return Row number of the next row to draw
 */
uint BaseSettingEntry::Draw(GameSettings *settings_ptr, int left, int right, int y, uint first_row, uint max_row, BaseSettingEntry *selected, uint cur_row, uint parent_last) const
{
	if (this->IsFiltered()) return cur_row;
	if (cur_row >= max_row) return cur_row;

	bool rtl = _current_text_dir == TD_RTL;
	int offset = (rtl ? -(int)_setting_circle_size.width : (int)_setting_circle_size.width) / 2;
	int level_width = rtl ? -WidgetDimensions::scaled.hsep_indent : WidgetDimensions::scaled.hsep_indent;

	int x = rtl ? right : left;
	if (cur_row >= first_row) {
		int colour = GetColourGradient(COLOUR_ORANGE, SHADE_NORMAL);
		y += (cur_row - first_row) * SETTING_HEIGHT; // Compute correct y start position

		/* Draw vertical for parent nesting levels */
		for (uint lvl = 0; lvl < this->level; lvl++) {
			if (!HasBit(parent_last, lvl)) GfxDrawLine(x + offset, y, x + offset, y + SETTING_HEIGHT - 1, colour);
			x += level_width;
		}
		/* draw own |- prefix */
		int halfway_y = y + SETTING_HEIGHT / 2;
		int bottom_y = flags.Test(SettingEntryFlag::LastField) ? halfway_y : y + SETTING_HEIGHT - 1;
		GfxDrawLine(x + offset, y, x + offset, bottom_y, colour);
		/* Small horizontal line from the last vertical line */
		GfxDrawLine(x + offset, halfway_y, x + level_width - (rtl ? -WidgetDimensions::scaled.hsep_normal : WidgetDimensions::scaled.hsep_normal), halfway_y, colour);
		x += level_width;

		this->DrawSetting(settings_ptr, rtl ? left : x, rtl ? x : right, y, this == selected);
	}
	cur_row++;

	return cur_row;
}

/* == SettingEntry methods == */

/**
 * Constructor for a single setting in the 'advanced settings' window
 * @param name Name of the setting in the setting table
 */
SettingEntry::SettingEntry(const char *name)
{
	this->name = name;
	this->setting = nullptr;
}

/**
 * Initialization of a setting entry
 * @param level      Page nesting level of this entry
 */
void SettingEntry::Init(uint8_t level)
{
	BaseSettingEntry::Init(level);
	this->setting = GetSettingFromName(this->name)->AsIntSetting();
}

/* Sets the given setting entry to its default value */
void SettingEntry::ResetAll()
{
	SetSettingValue(this->setting, this->setting->GetDefaultValue());
}

/**
 * Set the button-depressed flags (#SettingsEntryFlag::LeftDepressed and #SettingsEntryFlag::RightDepressed) to a specified value
 * @param new_val New value for the button flags
 * @see SettingEntryFlags
 */
void SettingEntry::SetButtons(SettingEntryFlags new_val)
{
	assert((new_val & SEF_BUTTONS_MASK) == new_val); // Should not touch any flags outside the buttons
	this->flags.Set(SettingEntryFlag::LeftDepressed, new_val.Test(SettingEntryFlag::LeftDepressed));
	this->flags.Set(SettingEntryFlag::RightDepressed, new_val.Test(SettingEntryFlag::RightDepressed));
}

/** Return number of rows needed to display the (filtered) entry */
uint SettingEntry::Length() const
{
	return this->IsFiltered() ? 0 : 1;
}

/**
 * Get the biggest height of the help text(s), if the width is at least \a maxw. Help text gets wrapped if needed.
 * @param maxw Maximal width of a line help text.
 * @return Biggest height needed to display any help text of this node (and its descendants).
 */
uint SettingEntry::GetMaxHelpHeight(int maxw)
{
	return GetStringHeight(this->setting->GetHelp(), maxw);
}

/**
 * Checks whether an entry shall be made visible based on the restriction mode.
 * @param mode The current status of the restriction drop down box.
 * @return true if the entry shall be visible.
 */
bool SettingEntry::IsVisibleByRestrictionMode(RestrictionMode mode) const
{
	/* There shall not be any restriction, i.e. all settings shall be visible. */
	if (mode == RM_ALL) return true;

	const IntSettingDesc *sd = this->setting;

	if (mode == RM_BASIC) return (this->setting->cat & SC_BASIC_LIST) != 0;
	if (mode == RM_ADVANCED) return (this->setting->cat & SC_ADVANCED_LIST) != 0;

	/* Read the current value. */
	const void *object = ResolveObject(&GetGameSettings(), sd);
	int64_t current_value = sd->Read(object);
	int64_t filter_value;

	if (mode == RM_CHANGED_AGAINST_DEFAULT) {
		/* This entry shall only be visible, if the value deviates from its default value. */

		/* Read the default value. */
		filter_value = sd->GetDefaultValue();
	} else {
		assert(mode == RM_CHANGED_AGAINST_NEW);
		/* This entry shall only be visible, if the value deviates from
		 * its value is used when starting a new game. */

		/* Make sure we're not comparing the new game settings against itself. */
		assert(&GetGameSettings() != &_settings_newgame);

		/* Read the new game's value. */
		filter_value = sd->Read(ResolveObject(&_settings_newgame, sd));
	}

	return current_value != filter_value;
}

/**
 * Update the filter state.
 * @param filter Filter
 * @param force_visible Whether to force all items visible, no matter what (due to filter text; not affected by restriction drop down box).
 * @return true if item remains visible
 */
bool SettingEntry::UpdateFilterState(SettingFilter &filter, bool force_visible)
{
	this->flags.Reset(SettingEntryFlag::Filtered);

	bool visible = true;

	const IntSettingDesc *sd = this->setting;
	if (!force_visible && !filter.string.IsEmpty()) {
		/* Process the search text filter for this item. */
		filter.string.ResetState();

		filter.string.AddLine(GetString(sd->GetTitle(), STR_EMPTY));
		filter.string.AddLine(GetString(sd->GetHelp()));

		visible = filter.string.GetState();
	}

	if (visible) {
		if (filter.type != ST_ALL && sd->GetType() != filter.type) {
			filter.type_hides = true;
			visible = false;
		}
		if (!this->IsVisibleByRestrictionMode(filter.mode)) {
			while (filter.min_cat < RM_ALL && (filter.min_cat == filter.mode || !this->IsVisibleByRestrictionMode(filter.min_cat))) filter.min_cat++;
			visible = false;
		}
	}

	if (!visible) this->flags.Set(SettingEntryFlag::Filtered);
	return visible;
}

const void *ResolveObject(const GameSettings *settings_ptr, const IntSettingDesc *sd)
{
	if (sd->flags.Test(SettingFlag::PerCompany)) {
		if (Company::IsValidID(_local_company) && _game_mode != GM_MENU) {
			return &Company::Get(_local_company)->settings;
		}
		return &_settings_client.company;
	}
	return settings_ptr;
}

/**
 * Function to draw setting value (button + text + current value)
 * @param settings_ptr Pointer to current values of all settings
 * @param left         Left-most position in window/panel to start drawing
 * @param right        Right-most position in window/panel to draw
 * @param y            Upper-most position in window/panel to start drawing
 * @param highlight    Highlight entry.
 */
void SettingEntry::DrawSetting(GameSettings *settings_ptr, int left, int right, int y, bool highlight) const
{
	const IntSettingDesc *sd = this->setting;
	int state = (this->flags & SEF_BUTTONS_MASK).base();

	bool rtl = _current_text_dir == TD_RTL;
	uint buttons_left = rtl ? right + 1 - SETTING_BUTTON_WIDTH : left;
	uint text_left  = left + (rtl ? 0 : SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_wide);
	uint text_right = right - (rtl ? SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_wide : 0);
	uint button_y = y + (SETTING_HEIGHT - SETTING_BUTTON_HEIGHT) / 2;

	/* We do not allow changes of some items when we are a client in a networkgame */
	bool editable = sd->IsEditable();

	auto [min_val, max_val] = sd->GetRange();
	int32_t value = sd->Read(ResolveObject(settings_ptr, sd));
	if (sd->IsBoolSetting()) {
		/* Draw checkbox for boolean-value either on/off */
		DrawBoolButton(buttons_left, button_y, value != 0, editable);
	} else if (sd->flags.Test(SettingFlag::GuiDropdown)) {
		/* Draw [v] button for settings of an enum-type */
		DrawDropDownButton(buttons_left, button_y, COLOUR_YELLOW, state != 0, editable);
	} else {
		/* Draw [<][>] boxes for settings of an integer-type */
		DrawArrowButtons(buttons_left, button_y, COLOUR_YELLOW, state,
				editable && value != (sd->flags.Test(SettingFlag::GuiZeroIsSpecial) ? 0 : min_val), editable && static_cast<uint32_t>(value) != max_val);
	}
	auto [param1, param2] = sd->GetValueParams(value);
	DrawString(text_left, text_right, y + (SETTING_HEIGHT - GetCharacterHeight(FS_NORMAL)) / 2, GetString(sd->GetTitle(), STR_CONFIG_SETTING_VALUE, param1, param2), highlight ? TC_WHITE : TC_LIGHT_BLUE);
}

/* == SettingsContainer methods == */

/**
 * Initialization of an entire setting page
 * @param level Nesting level of this page (internal variable, do not provide a value for it when calling)
 */
void SettingsContainer::Init(uint8_t level)
{
	for (auto &it : this->entries) {
		it->Init(level);
	}
}

/** Resets all settings to their default values */
void SettingsContainer::ResetAll()
{
	for (auto settings_entry : this->entries) {
		settings_entry->ResetAll();
	}
}

/** Recursively close all folds of sub-pages */
void SettingsContainer::FoldAll()
{
	for (auto &it : this->entries) {
		it->FoldAll();
	}
}

/** Recursively open all folds of sub-pages */
void SettingsContainer::UnFoldAll()
{
	for (auto &it : this->entries) {
		it->UnFoldAll();
	}
}

/**
 * Recursively accumulate the folding state of the tree.
 * @param[in,out] all_folded Set to false, if one entry is not folded.
 * @param[in,out] all_unfolded Set to false, if one entry is folded.
 */
void SettingsContainer::GetFoldingState(bool &all_folded, bool &all_unfolded) const
{
	for (auto &it : this->entries) {
		it->GetFoldingState(all_folded, all_unfolded);
	}
}

/**
 * Update the filter state.
 * @param filter Filter
 * @param force_visible Whether to force all items visible, no matter what
 * @return true if item remains visible
 */
bool SettingsContainer::UpdateFilterState(SettingFilter &filter, bool force_visible)
{
	bool visible = false;
	bool first_visible = true;
	for (EntryVector::reverse_iterator it = this->entries.rbegin(); it != this->entries.rend(); ++it) {
		visible |= (*it)->UpdateFilterState(filter, force_visible);
		(*it)->SetLastField(first_visible);
		if (visible && first_visible) first_visible = false;
	}
	return visible;
}


/**
 * Check whether an entry is visible and not folded or filtered away.
 * Note: This does not consider the scrolling range; it might still require scrolling to make the setting really visible.
 * @param item Entry to search for.
 * @return true if entry is visible.
 */
bool SettingsContainer::IsVisible(const BaseSettingEntry *item) const
{
	for (const auto &it : this->entries) {
		if (it->IsVisible(item)) return true;
	}
	return false;
}

/** Return number of rows needed to display the whole page */
uint SettingsContainer::Length() const
{
	uint length = 0;
	for (const auto &it : this->entries) {
		length += it->Length();
	}
	return length;
}

/**
 * Find the setting entry at row number \a row_num
 * @param row_num Index of entry to return
 * @param cur_row Variable used for keeping track of the current row number. Should point to memory initialized to \c 0 when first called.
 * @return The requested setting entry or \c nullptr if it does not exist
 */
BaseSettingEntry *SettingsContainer::FindEntry(uint row_num, uint *cur_row)
{
	BaseSettingEntry *pe = nullptr;
	for (const auto &it : this->entries) {
		pe = it->FindEntry(row_num, cur_row);
		if (pe != nullptr) {
			break;
		}
	}
	return pe;
}

/**
 * Get the biggest height of the help texts, if the width is at least \a maxw. Help text gets wrapped if needed.
 * @param maxw Maximal width of a line help text.
 * @return Biggest height needed to display any help text of this (sub-)tree.
 */
uint SettingsContainer::GetMaxHelpHeight(int maxw)
{
	uint biggest = 0;
	for (const auto &it : this->entries) {
		biggest = std::max(biggest, it->GetMaxHelpHeight(maxw));
	}
	return biggest;
}


/**
 * Draw a row in the settings panel.
 *
 * @param settings_ptr Pointer to current values of all settings
 * @param left         Left-most position in window/panel to start drawing \a first_row
 * @param right        Right-most x position to draw strings at.
 * @param y            Upper-most position in window/panel to start drawing \a first_row
 * @param first_row    First row number to draw
 * @param max_row      Row-number to stop drawing (the row-number of the row below the last row to draw)
 * @param selected     Selected entry by the user.
 * @param cur_row      Current row number (internal variable)
 * @param parent_last  Last-field booleans of parent page level (page level \e i sets bit \e i to 1 if it is its last field)
 * @return Row number of the next row to draw
 */
uint SettingsContainer::Draw(GameSettings *settings_ptr, int left, int right, int y, uint first_row, uint max_row, BaseSettingEntry *selected, uint cur_row, uint parent_last) const
{
	for (const auto &it : this->entries) {
		cur_row = it->Draw(settings_ptr, left, right, y, first_row, max_row, selected, cur_row, parent_last);
		if (cur_row >= max_row) break;
	}
	return cur_row;
}

/* == SettingsPage methods == */

/**
 * Constructor for a sub-page in the 'advanced settings' window
 * @param title Title of the sub-page
 */
SettingsPage::SettingsPage(StringID title)
{
	this->title = title;
	this->folded = true;
}

/**
 * Initialization of an entire setting page
 * @param level Nesting level of this page (internal variable, do not provide a value for it when calling)
 */
void SettingsPage::Init(uint8_t level)
{
	BaseSettingEntry::Init(level);
	SettingsContainer::Init(level + 1);
}

/** Resets all settings to their default values */
void SettingsPage::ResetAll()
{
	for (auto settings_entry : this->entries) {
		settings_entry->ResetAll();
	}
}

/** Recursively close all (filtered) folds of sub-pages */
void SettingsPage::FoldAll()
{
	if (this->IsFiltered()) return;
	this->folded = true;

	SettingsContainer::FoldAll();
}

/** Recursively open all (filtered) folds of sub-pages */
void SettingsPage::UnFoldAll()
{
	if (this->IsFiltered()) return;
	this->folded = false;

	SettingsContainer::UnFoldAll();
}

/**
 * Recursively accumulate the folding state of the (filtered) tree.
 * @param[in,out] all_folded Set to false, if one entry is not folded.
 * @param[in,out] all_unfolded Set to false, if one entry is folded.
 */
void SettingsPage::GetFoldingState(bool &all_folded, bool &all_unfolded) const
{
	if (this->IsFiltered()) return;

	if (this->folded) {
		all_unfolded = false;
	} else {
		all_folded = false;
	}

	SettingsContainer::GetFoldingState(all_folded, all_unfolded);
}

/**
 * Update the filter state.
 * @param filter Filter
 * @param force_visible Whether to force all items visible, no matter what (due to filter text; not affected by restriction drop down box).
 * @return true if item remains visible
 */
bool SettingsPage::UpdateFilterState(SettingFilter &filter, bool force_visible)
{
	if (!force_visible && !filter.string.IsEmpty()) {
		filter.string.ResetState();
		filter.string.AddLine(GetString(this->title));
		force_visible = filter.string.GetState();
	}

	bool visible = SettingsContainer::UpdateFilterState(filter, force_visible);
	this->flags.Set(SettingEntryFlag::Filtered, !visible);
	return visible;
}

/**
 * Check whether an entry is visible and not folded or filtered away.
 * Note: This does not consider the scrolling range; it might still require scrolling to make the setting really visible.
 * @param item Entry to search for.
 * @return true if entry is visible.
 */
bool SettingsPage::IsVisible(const BaseSettingEntry *item) const
{
	if (this->IsFiltered()) return false;
	if (this == item) return true;
	if (this->folded) return false;

	return SettingsContainer::IsVisible(item);
}

/** Return number of rows needed to display the (filtered) entry */
uint SettingsPage::Length() const
{
	if (this->IsFiltered()) return 0;
	if (this->folded) return 1; // Only displaying the title

	return 1 + SettingsContainer::Length();
}

/**
 * Find setting entry at row \a row_num
 * @param row_num Index of entry to return
 * @param cur_row Current row number
 * @return The requested setting entry or \c nullptr if it not found (folded or filtered)
 */
BaseSettingEntry *SettingsPage::FindEntry(uint row_num, uint *cur_row)
{
	if (this->IsFiltered()) return nullptr;
	if (row_num == *cur_row) return this;
	(*cur_row)++;
	if (this->folded) return nullptr;

	return SettingsContainer::FindEntry(row_num, cur_row);
}

/**
 * Draw a row in the settings panel.
 *
 * @param settings_ptr Pointer to current values of all settings
 * @param left         Left-most position in window/panel to start drawing \a first_row
 * @param right        Right-most x position to draw strings at.
 * @param y            Upper-most position in window/panel to start drawing \a first_row
 * @param first_row    First row number to draw
 * @param max_row      Row-number to stop drawing (the row-number of the row below the last row to draw)
 * @param selected     Selected entry by the user.
 * @param cur_row      Current row number (internal variable)
 * @param parent_last  Last-field booleans of parent page level (page level \e i sets bit \e i to 1 if it is its last field)
 * @return Row number of the next row to draw
 */
uint SettingsPage::Draw(GameSettings *settings_ptr, int left, int right, int y, uint first_row, uint max_row, BaseSettingEntry *selected, uint cur_row, uint parent_last) const
{
	if (this->IsFiltered()) return cur_row;
	if (cur_row >= max_row) return cur_row;

	cur_row = BaseSettingEntry::Draw(settings_ptr, left, right, y, first_row, max_row, selected, cur_row, parent_last);

	if (!this->folded) {
		if (this->flags.Test(SettingEntryFlag::LastField)) {
			assert(this->level < 8 * sizeof(parent_last));
			SetBit(parent_last, this->level); // Add own last-field state
		}

		cur_row = SettingsContainer::Draw(settings_ptr, left, right, y, first_row, max_row, selected, cur_row, parent_last);
	}

	return cur_row;
}

/**
 * Function to draw setting value (button + text + current value)
 * @param left         Left-most position in window/panel to start drawing
 * @param right        Right-most position in window/panel to draw
 * @param y            Upper-most position in window/panel to start drawing
 */
void SettingsPage::DrawSetting(GameSettings *, int left, int right, int y, bool) const
{
	bool rtl = _current_text_dir == TD_RTL;
	DrawSprite((this->folded ? SPR_CIRCLE_FOLDED : SPR_CIRCLE_UNFOLDED), PAL_NONE, rtl ? right - _setting_circle_size.width : left, y + (SETTING_HEIGHT - _setting_circle_size.height) / 2);
	DrawString(rtl ? left : left + _setting_circle_size.width + WidgetDimensions::scaled.hsep_normal, rtl ? right - _setting_circle_size.width - WidgetDimensions::scaled.hsep_normal : right, y + (SETTING_HEIGHT - GetCharacterHeight(FS_NORMAL)) / 2, this->title, TC_ORANGE);
}

/** Construct settings tree */
SettingsContainer &GetSettingsTree()
{
	static SettingsContainer *main = nullptr;

	if (main == nullptr)
	{
		/* Build up the dynamic settings-array only once per OpenTTD session */
		main = new SettingsContainer();

		SettingsPage *localisation = main->Add(new SettingsPage(STR_CONFIG_SETTING_LOCALISATION));
		{
			localisation->Add(new SettingEntry("locale.units_velocity"));
			localisation->Add(new SettingEntry("locale.units_velocity_nautical"));
			localisation->Add(new SettingEntry("locale.units_power"));
			localisation->Add(new SettingEntry("locale.units_weight"));
			localisation->Add(new SettingEntry("locale.units_volume"));
			localisation->Add(new SettingEntry("locale.units_force"));
			localisation->Add(new SettingEntry("locale.units_height"));
			localisation->Add(new SettingEntry("gui.date_format_in_default_names"));
		}

		SettingsPage *graphics = main->Add(new SettingsPage(STR_CONFIG_SETTING_GRAPHICS));
		{
			graphics->Add(new SettingEntry("gui.zoom_min"));
			graphics->Add(new SettingEntry("gui.zoom_max"));
			graphics->Add(new SettingEntry("gui.sprite_zoom_min"));
			graphics->Add(new SettingEntry("gui.smallmap_land_colour"));
			graphics->Add(new SettingEntry("gui.linkgraph_colours"));
			graphics->Add(new SettingEntry("gui.graph_line_thickness"));
		}

		SettingsPage *sound = main->Add(new SettingsPage(STR_CONFIG_SETTING_SOUND));
		{
			sound->Add(new SettingEntry("sound.click_beep"));
			sound->Add(new SettingEntry("sound.confirm"));
			sound->Add(new SettingEntry("sound.news_ticker"));
			sound->Add(new SettingEntry("sound.news_full"));
			sound->Add(new SettingEntry("sound.new_year"));
			sound->Add(new SettingEntry("sound.disaster"));
			sound->Add(new SettingEntry("sound.vehicle"));
			sound->Add(new SettingEntry("sound.ambient"));
		}

		SettingsPage *interface = main->Add(new SettingsPage(STR_CONFIG_SETTING_INTERFACE));
		{
			SettingsPage *general = interface->Add(new SettingsPage(STR_CONFIG_SETTING_INTERFACE_GENERAL));
			{
				general->Add(new SettingEntry("gui.osk_activation"));
				general->Add(new SettingEntry("gui.hover_delay_ms"));
				general->Add(new SettingEntry("gui.errmsg_duration"));
				general->Add(new SettingEntry("gui.window_snap_radius"));
				general->Add(new SettingEntry("gui.window_soft_limit"));
				general->Add(new SettingEntry("gui.right_click_wnd_close"));
			}

			SettingsPage *viewports = interface->Add(new SettingsPage(STR_CONFIG_SETTING_INTERFACE_VIEWPORTS));
			{
				viewports->Add(new SettingEntry("gui.auto_scrolling"));
				viewports->Add(new SettingEntry("gui.scroll_mode"));
				viewports->Add(new SettingEntry("gui.smooth_scroll"));
				/* While the horizontal scrollwheel scrolling is written as general code, only
				 *  the cocoa (OSX) driver generates input for it.
				 *  Since it's also able to completely disable the scrollwheel will we display it on all platforms anyway */
				viewports->Add(new SettingEntry("gui.scrollwheel_scrolling"));
				viewports->Add(new SettingEntry("gui.scrollwheel_multiplier"));
#ifdef __APPLE__
				/* We might need to emulate a right mouse button on mac */
				viewports->Add(new SettingEntry("gui.right_mouse_btn_emulation"));
#endif
				viewports->Add(new SettingEntry("gui.population_in_label"));
				viewports->Add(new SettingEntry("gui.liveries"));
				viewports->Add(new SettingEntry("construction.train_signal_side"));
				viewports->Add(new SettingEntry("gui.measure_tooltip"));
				viewports->Add(new SettingEntry("gui.loading_indicators"));
				viewports->Add(new SettingEntry("gui.show_track_reservation"));
			}

			SettingsPage *construction = interface->Add(new SettingsPage(STR_CONFIG_SETTING_INTERFACE_CONSTRUCTION));
			{
				construction->Add(new SettingEntry("gui.link_terraform_toolbar"));
				construction->Add(new SettingEntry("gui.persistent_buildingtools"));
				construction->Add(new SettingEntry("gui.default_rail_type"));
				construction->Add(new SettingEntry("gui.semaphore_build_before"));
				construction->Add(new SettingEntry("gui.signal_gui_mode"));
				construction->Add(new SettingEntry("gui.cycle_signal_types"));
				construction->Add(new SettingEntry("gui.drag_signals_fixed_distance"));
				construction->Add(new SettingEntry("gui.auto_remove_signals"));
			}

			interface->Add(new SettingEntry("gui.toolbar_pos"));
			interface->Add(new SettingEntry("gui.statusbar_pos"));
			interface->Add(new SettingEntry("gui.prefer_teamchat"));
			interface->Add(new SettingEntry("gui.advanced_vehicle_list"));
			interface->Add(new SettingEntry("gui.timetable_mode"));
			interface->Add(new SettingEntry("gui.timetable_arrival_departure"));
			interface->Add(new SettingEntry("gui.show_newgrf_name"));
			interface->Add(new SettingEntry("gui.show_cargo_in_vehicle_lists"));
		}

		SettingsPage *advisors = main->Add(new SettingsPage(STR_CONFIG_SETTING_ADVISORS));
		{
			advisors->Add(new SettingEntry("gui.coloured_news_year"));
			advisors->Add(new SettingEntry("news_display.general"));
			advisors->Add(new SettingEntry("news_display.new_vehicles"));
			advisors->Add(new SettingEntry("news_display.accident"));
			advisors->Add(new SettingEntry("news_display.accident_other"));
			advisors->Add(new SettingEntry("news_display.company_info"));
			advisors->Add(new SettingEntry("news_display.acceptance"));
			advisors->Add(new SettingEntry("news_display.arrival_player"));
			advisors->Add(new SettingEntry("news_display.arrival_other"));
			advisors->Add(new SettingEntry("news_display.advice"));
			advisors->Add(new SettingEntry("gui.order_review_system"));
			advisors->Add(new SettingEntry("gui.vehicle_income_warn"));
			advisors->Add(new SettingEntry("gui.lost_vehicle_warn"));
			advisors->Add(new SettingEntry("gui.old_vehicle_warn"));
			advisors->Add(new SettingEntry("gui.show_finances"));
			advisors->Add(new SettingEntry("news_display.economy"));
			advisors->Add(new SettingEntry("news_display.subsidies"));
			advisors->Add(new SettingEntry("news_display.open"));
			advisors->Add(new SettingEntry("news_display.close"));
			advisors->Add(new SettingEntry("news_display.production_player"));
			advisors->Add(new SettingEntry("news_display.production_other"));
			advisors->Add(new SettingEntry("news_display.production_nobody"));
		}

		SettingsPage *company = main->Add(new SettingsPage(STR_CONFIG_SETTING_COMPANY));
		{
			company->Add(new SettingEntry("gui.starting_colour"));
			company->Add(new SettingEntry("gui.starting_colour_secondary"));
			company->Add(new SettingEntry("company.engine_renew"));
			company->Add(new SettingEntry("company.engine_renew_months"));
			company->Add(new SettingEntry("company.engine_renew_money"));
			company->Add(new SettingEntry("vehicle.servint_ispercent"));
			company->Add(new SettingEntry("vehicle.servint_trains"));
			company->Add(new SettingEntry("vehicle.servint_roadveh"));
			company->Add(new SettingEntry("vehicle.servint_ships"));
			company->Add(new SettingEntry("vehicle.servint_aircraft"));
		}

		SettingsPage *accounting = main->Add(new SettingsPage(STR_CONFIG_SETTING_ACCOUNTING));
		{
			accounting->Add(new SettingEntry("difficulty.infinite_money"));
			accounting->Add(new SettingEntry("economy.inflation"));
			accounting->Add(new SettingEntry("difficulty.initial_interest"));
			accounting->Add(new SettingEntry("difficulty.max_loan"));
			accounting->Add(new SettingEntry("difficulty.subsidy_multiplier"));
			accounting->Add(new SettingEntry("difficulty.subsidy_duration"));
			accounting->Add(new SettingEntry("economy.feeder_payment_share"));
			accounting->Add(new SettingEntry("economy.infrastructure_maintenance"));
			accounting->Add(new SettingEntry("difficulty.vehicle_costs"));
			accounting->Add(new SettingEntry("difficulty.construction_cost"));
		}

		SettingsPage *vehicles = main->Add(new SettingsPage(STR_CONFIG_SETTING_VEHICLES));
		{
			SettingsPage *physics = vehicles->Add(new SettingsPage(STR_CONFIG_SETTING_VEHICLES_PHYSICS));
			{
				physics->Add(new SettingEntry("vehicle.train_acceleration_model"));
				physics->Add(new SettingEntry("vehicle.train_slope_steepness"));
				physics->Add(new SettingEntry("vehicle.wagon_speed_limits"));
				physics->Add(new SettingEntry("vehicle.freight_trains"));
				physics->Add(new SettingEntry("vehicle.roadveh_acceleration_model"));
				physics->Add(new SettingEntry("vehicle.roadveh_slope_steepness"));
				physics->Add(new SettingEntry("vehicle.smoke_amount"));
				physics->Add(new SettingEntry("vehicle.plane_speed"));
			}

			SettingsPage *routing = vehicles->Add(new SettingsPage(STR_CONFIG_SETTING_VEHICLES_ROUTING));
			{
				routing->Add(new SettingEntry("vehicle.road_side"));
				routing->Add(new SettingEntry("difficulty.line_reverse_mode"));
				routing->Add(new SettingEntry("pf.reverse_at_signals"));
				routing->Add(new SettingEntry("pf.forbid_90_deg"));
			}

			SettingsPage *orders = vehicles->Add(new SettingsPage(STR_CONFIG_SETTING_VEHICLES_ORDERS));
			{
				orders->Add(new SettingEntry("gui.new_nonstop"));
				orders->Add(new SettingEntry("gui.quick_goto"));
				orders->Add(new SettingEntry("gui.stop_location"));
			}
		}

		SettingsPage *limitations = main->Add(new SettingsPage(STR_CONFIG_SETTING_LIMITATIONS));
		{
			limitations->Add(new SettingEntry("construction.command_pause_level"));
			limitations->Add(new SettingEntry("construction.autoslope"));
			limitations->Add(new SettingEntry("construction.extra_dynamite"));
			limitations->Add(new SettingEntry("construction.map_height_limit"));
			limitations->Add(new SettingEntry("construction.max_bridge_length"));
			limitations->Add(new SettingEntry("construction.max_bridge_height"));
			limitations->Add(new SettingEntry("construction.max_tunnel_length"));
			limitations->Add(new SettingEntry("station.never_expire_airports"));
			limitations->Add(new SettingEntry("vehicle.never_expire_vehicles"));
			limitations->Add(new SettingEntry("vehicle.max_trains"));
			limitations->Add(new SettingEntry("vehicle.max_roadveh"));
			limitations->Add(new SettingEntry("vehicle.max_aircraft"));
			limitations->Add(new SettingEntry("vehicle.max_ships"));
			limitations->Add(new SettingEntry("vehicle.max_train_length"));
			limitations->Add(new SettingEntry("station.station_spread"));
			limitations->Add(new SettingEntry("station.distant_join_stations"));
			limitations->Add(new SettingEntry("station.modified_catchment"));
			limitations->Add(new SettingEntry("construction.road_stop_on_town_road"));
			limitations->Add(new SettingEntry("construction.road_stop_on_competitor_road"));
			limitations->Add(new SettingEntry("construction.crossing_with_competitor"));
			limitations->Add(new SettingEntry("vehicle.disable_elrails"));
			limitations->Add(new SettingEntry("order.station_length_loading_penalty"));
		}

		SettingsPage *disasters = main->Add(new SettingsPage(STR_CONFIG_SETTING_ACCIDENTS));
		{
			disasters->Add(new SettingEntry("difficulty.disasters"));
			disasters->Add(new SettingEntry("difficulty.economy"));
			disasters->Add(new SettingEntry("vehicle.plane_crashes"));
			disasters->Add(new SettingEntry("difficulty.vehicle_breakdowns"));
			disasters->Add(new SettingEntry("order.no_servicing_if_no_breakdowns"));
			disasters->Add(new SettingEntry("order.serviceathelipad"));
		}

		SettingsPage *genworld = main->Add(new SettingsPage(STR_CONFIG_SETTING_GENWORLD));
		{
			genworld->Add(new SettingEntry("game_creation.landscape"));
			genworld->Add(new SettingEntry("game_creation.land_generator"));
			genworld->Add(new SettingEntry("difficulty.terrain_type"));
			genworld->Add(new SettingEntry("game_creation.tgen_smoothness"));
			genworld->Add(new SettingEntry("game_creation.variety"));
			genworld->Add(new SettingEntry("game_creation.snow_coverage"));
			genworld->Add(new SettingEntry("game_creation.snow_line_height"));
			genworld->Add(new SettingEntry("game_creation.desert_coverage"));
			genworld->Add(new SettingEntry("game_creation.amount_of_rivers"));
		}

		SettingsPage *environment = main->Add(new SettingsPage(STR_CONFIG_SETTING_ENVIRONMENT));
		{
			SettingsPage *time = environment->Add(new SettingsPage(STR_CONFIG_SETTING_ENVIRONMENT_TIME));
			{
				time->Add(new SettingEntry("economy.timekeeping_units"));
				time->Add(new SettingEntry("economy.minutes_per_calendar_year"));
				time->Add(new SettingEntry("game_creation.ending_year"));
				time->Add(new SettingEntry("gui.pause_on_newgame"));
				time->Add(new SettingEntry("gui.fast_forward_speed_limit"));
			}

			SettingsPage *authorities = environment->Add(new SettingsPage(STR_CONFIG_SETTING_ENVIRONMENT_AUTHORITIES));
			{
				authorities->Add(new SettingEntry("difficulty.town_council_tolerance"));
				authorities->Add(new SettingEntry("economy.bribe"));
				authorities->Add(new SettingEntry("economy.exclusive_rights"));
				authorities->Add(new SettingEntry("economy.fund_roads"));
				authorities->Add(new SettingEntry("economy.fund_buildings"));
				authorities->Add(new SettingEntry("economy.station_noise_level"));
			}

			SettingsPage *towns = environment->Add(new SettingsPage(STR_CONFIG_SETTING_ENVIRONMENT_TOWNS));
			{
				towns->Add(new SettingEntry("economy.town_cargo_scale"));
				towns->Add(new SettingEntry("economy.town_growth_rate"));
				towns->Add(new SettingEntry("economy.allow_town_roads"));
				towns->Add(new SettingEntry("economy.allow_town_level_crossings"));
				towns->Add(new SettingEntry("economy.found_town"));
				towns->Add(new SettingEntry("economy.place_houses"));
				towns->Add(new SettingEntry("economy.town_layout"));
				towns->Add(new SettingEntry("economy.larger_towns"));
				towns->Add(new SettingEntry("economy.initial_city_size"));
				towns->Add(new SettingEntry("economy.town_cargogen_mode"));
			}

			SettingsPage *industries = environment->Add(new SettingsPage(STR_CONFIG_SETTING_ENVIRONMENT_INDUSTRIES));
			{
				industries->Add(new SettingEntry("economy.industry_cargo_scale"));
				industries->Add(new SettingEntry("difficulty.industry_density"));
				industries->Add(new SettingEntry("construction.raw_industry_construction"));
				industries->Add(new SettingEntry("construction.industry_platform"));
				industries->Add(new SettingEntry("economy.multiple_industry_per_town"));
				industries->Add(new SettingEntry("game_creation.oil_refinery_limit"));
				industries->Add(new SettingEntry("economy.type"));
				industries->Add(new SettingEntry("station.serve_neutral_industries"));
			}

			SettingsPage *cdist = environment->Add(new SettingsPage(STR_CONFIG_SETTING_ENVIRONMENT_CARGODIST));
			{
				cdist->Add(new SettingEntry("linkgraph.recalc_time"));
				cdist->Add(new SettingEntry("linkgraph.recalc_interval"));
				cdist->Add(new SettingEntry("linkgraph.distribution_pax"));
				cdist->Add(new SettingEntry("linkgraph.distribution_mail"));
				cdist->Add(new SettingEntry("linkgraph.distribution_armoured"));
				cdist->Add(new SettingEntry("linkgraph.distribution_default"));
				cdist->Add(new SettingEntry("linkgraph.accuracy"));
				cdist->Add(new SettingEntry("linkgraph.demand_distance"));
				cdist->Add(new SettingEntry("linkgraph.demand_size"));
				cdist->Add(new SettingEntry("linkgraph.short_path_saturation"));
			}

			SettingsPage *trees = environment->Add(new SettingsPage(STR_CONFIG_SETTING_ENVIRONMENT_TREES));
			{
				trees->Add(new SettingEntry("game_creation.tree_placer"));
				trees->Add(new SettingEntry("construction.extra_tree_placement"));
			}
		}

		SettingsPage *ai = main->Add(new SettingsPage(STR_CONFIG_SETTING_AI));
		{
			SettingsPage *npc = ai->Add(new SettingsPage(STR_CONFIG_SETTING_AI_NPC));
			{
				npc->Add(new SettingEntry("script.script_max_opcode_till_suspend"));
				npc->Add(new SettingEntry("script.script_max_memory_megabytes"));
				npc->Add(new SettingEntry("difficulty.competitor_speed"));
				npc->Add(new SettingEntry("ai.ai_in_multiplayer"));
				npc->Add(new SettingEntry("ai.ai_disable_veh_train"));
				npc->Add(new SettingEntry("ai.ai_disable_veh_roadveh"));
				npc->Add(new SettingEntry("ai.ai_disable_veh_aircraft"));
				npc->Add(new SettingEntry("ai.ai_disable_veh_ship"));
			}

			ai->Add(new SettingEntry("economy.give_money"));
		}

		SettingsPage *network = main->Add(new SettingsPage(STR_CONFIG_SETTING_NETWORK));
		{
			network->Add(new SettingEntry("network.use_relay_service"));
		}

		main->Init();
	}
	return *main;
}
