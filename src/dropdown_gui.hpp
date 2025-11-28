/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dropdown_gui.h Custom base types for dropdown window. */

#include "dropdown_type.h"
#include "newgrf.h"
#include "newgrf_badge_type.h"
#include "rail.h"
#include "road.h"
#include "newgrf_badge_gui.h"
#include "strings_type.h"
#include "strings_func.h"
#include "timer/timer.h"
#include "timer/timer_window.h"
#include "sound_func.h"
#include "zoom_func.h"
#include "window_func.h"
#include "querystring_gui.h"
#include "stringfilter_type.h"

/** Drop-down menu window */
struct DropdownWindow : Window {
public:
	IntervalTimer<TimerWindow> scroll_interval; ///< Rate limit how fast scrolling happens.
private:
	WidgetID parent_button{}; ///< Parent widget number where the window is dropped from.
	Rect wi_rect{}; ///< Rect of the button that opened the dropdown.
	DropDownList list{}; ///< List with dropdown menu items.
	int selected_result = 0; ///< Result value of the selected item in the list.
	int selected_click_result = -1; ///< Click result value, from the OnClick handler of the selected item.
	uint8_t click_delay = 0; ///< Timer to delay selection.
	bool drag_mode = true;
	DropDownOptions options; ///< Options for this drop down menu.
	int scrolling = 0; ///< If non-zero, auto-scroll the item list (one time).
	Point position{}; ///< Position of the topleft corner of the window.

	Scrollbar *vscroll = nullptr;
	QueryString editbox; ///< Filter editbox.
	StringFilter string_filter{}; ///< Filter for type name.

	bool last_shift_state; ///< Whether the shift button was pressed during last frame.
	bool last_ctrl_state; ///< Whether the ctrl button was pressed during last frame.

	bool has_subdropdown_open = false;
	Colours window_colour;

	GUIBadgeClasses badge_classes{};
	static constexpr int BADGE_COLUMNS = 1; ///< Number of columns available for badges (0 = at the end)
	std::pair<WidgetID, WidgetID> badge_filters{}; ///< First and last widgets IDs of badge filters.
	BadgeFilterChoices badge_filter_choices{};

	Dimension items_dim{}; ///< Calculated cropped and padded dimension for the items widget

public:
	DropdownWindow(int window_id, Window *parent, DropDownList &&list, int selected, WidgetID button, const Rect wi_rect, Colours wi_colour, DropDownOptions options, bool has_sorter);
	DropdownWindow(int window_id, Window *parent, DropDownList &&list, int selected, WidgetID button, const Rect wi_rect, Colours wi_colour, DropDownOptions options);

	void OnInit() override;
	void Close(int data = 0) override;
	void OnFocusLost(bool closing) override;
	void UpdateWidgetSize(WidgetID widget, Dimension &size, const Dimension &padding, Dimension &fill, Dimension &resize) override;
	Point OnInitialPosition(int16_t sm_width, int16_t sm_height, int window_number) override;
	void DrawWidget(const Rect &r, WidgetID widget) const override;
	void OnClick(Point pt, WidgetID widget, int click_count) override;
	std::string GetWidgetString(WidgetID widget, StringID stringid) const override;
	void OnDropdownSelect(WidgetID widget, int index, int click_result) override;
	void OnDropdownClose(Point pt, WidgetID widget, int index, int click_result, bool instant_close) override;
	void OnMouseLoop() override;
	void OnEditboxChanged(WidgetID wid) override;

	void FitAvailableHeight(Dimension &desired, const Dimension &list, uint available_height);
	void UpdateSizeAndPosition();
	bool GetDropdownItem(int &result, int &click_result);
	void ShowSubDropdownList(WidgetID widget, DropDownList &&list, int sub_dropdown_id = 1, DropDownOptions options = {}, int selected_result = -1);
	void ReplaceList(DropDownList &&list, std::optional<int> selected_result);

	virtual StringID GetSortCriteriaString() const;
	virtual bool IsSortOrderInverted() const;
	virtual DropDownList GetDropdownList(const BadgeFilterChoices &badge_filter_choices, const StringFilter &string_filter) const;
	virtual GrfSpecFeature GetGrfSpecFeature() const;
	virtual DropDownList GetSortDropdownList() const;

	/**
	 * Sets the sort criteria for dropdown's content.
	 * @param sort_criteria New sort criteria.
	 */
	virtual void SetSortCriteria([[maybe_unused]] int sort_criteria) {}

	/**
	 * Sets whether the sort order is inverted.
	 * @param sort_order_inverted Iff true the sort order will be set to inverted.
	 */
	virtual void SetSortOrderInverted([[maybe_unused]] bool sort_order_inverted) {}
};

class RailTypeDropdownWindow : public DropdownWindow {
public:
	/** @see DropdownWindow::DropdownWindow. */
	RailTypeDropdownWindow(int window_id, Window *parent, DropDownList &&list, int selected, WidgetID button, const Rect wi_rect, Colours wi_colour, DropDownOptions options) : DropdownWindow(window_id, parent, std::move(list), selected, button, wi_rect, wi_colour, options, true)
	{
		this->FinishInitNested(window_id);
		this->flags.Reset(WindowFlag::WhiteBorder);
	}

	void SetSortCriteria(int new_sort_criteria) override;
	StringID GetSortCriteriaString() const override;
	void SetSortOrderInverted(bool is_sort_order_inverted) override;
	bool IsSortOrderInverted() const override { return _railtypes_invert_sort_order; }
	DropDownList GetDropdownList(const BadgeFilterChoices &badge_filter_choices, const StringFilter &string_filter) const override;
	GrfSpecFeature GetGrfSpecFeature() const override { return GSF_RAILTYPES; }
	DropDownList GetSortDropdownList() const override;
};

class RoadTypeDropdownWindow : public DropdownWindow {
public:
	/** @see DropdownWindow::DropdownWindow. */
	RoadTypeDropdownWindow(int window_id, Window *parent, DropDownList &&list, int selected, WidgetID button, const Rect wi_rect, Colours wi_colour, DropDownOptions options) : DropdownWindow(window_id, parent, std::move(list), selected, button, wi_rect, wi_colour, options, true)
	{
		this->FinishInitNested(window_id);
		this->flags.Reset(WindowFlag::WhiteBorder);
	}

	void SetSortCriteria(int new_sort_criteria) override;
	StringID GetSortCriteriaString() const override;
	void SetSortOrderInverted(bool is_sort_order_inverted) override;
	bool IsSortOrderInverted() const override { return _roadtypes_invert_sort_order; }
	DropDownList GetDropdownList(const BadgeFilterChoices &badge_filter_choices, const StringFilter &string_filter) const override;
	GrfSpecFeature GetGrfSpecFeature() const override { return GSF_ROADTYPES; }
	DropDownList GetSortDropdownList() const override;
};

class TramTypeDropdownWindow : public DropdownWindow {
public:
	/** @see DropdownWindow::DropdownWindow. */
	TramTypeDropdownWindow(int window_id, Window *parent, DropDownList &&list, int selected, WidgetID button, const Rect wi_rect, Colours wi_colour, DropDownOptions options) : DropdownWindow(window_id, parent, std::move(list), selected, button, wi_rect, wi_colour, options, true)
	{
		this->FinishInitNested(window_id);
		this->flags.Reset(WindowFlag::WhiteBorder);
	}

	void SetSortCriteria(int new_sort_criteria) override;
	StringID GetSortCriteriaString() const override;
	void SetSortOrderInverted(bool is_sort_order_inverted) override;
	bool IsSortOrderInverted() const override { return _tramtypes_invert_sort_order; }
	DropDownList GetDropdownList(const BadgeFilterChoices &badge_filter_choices, const StringFilter &string_filter) const override;
	GrfSpecFeature GetGrfSpecFeature() const override { return GSF_TRAMTYPES; }
	DropDownList GetSortDropdownList() const override;
};

/**
 * Concept specifying that the provided type can br used as a dropdown window.
 * @tparam TDropdownWindow Type to check.
 */
template <typename TDropdownWindow>
concept DropdownWindowType = std::derived_from<TDropdownWindow, DropdownWindow> && requires (int window_id, Window *parent, DropDownList &&list, int selected, WidgetID button, const Rect wi_rect, Colours wi_colour, DropDownOptions options) {
	{ TDropdownWindow(window_id, parent, std::move(list), selected, button, wi_rect, wi_colour, options) };
};

/**
 * Show a dropdown list of a custom dropdown window class.
 * @tparam TDropdownWindow Class for new dropdown.
 * @param w Parent window for the list.
 * @param list Prepopulated DropDownList.
 * @param selected The initially selected list item.
 * @param button The widget within the parent window that is used to determine the list's location.
 * @param width Override the minimum width determined by the selected widget and list contents.
 * @param options Dropdown options for this menu.
 */
template <DropdownWindowType TDropdownWindow>
void ShowCustomDropdownList(Window *w, DropDownList &&list, int selected, WidgetID button, uint width = 0, DropDownOptions options = {})
{
	/* Handle the beep of the player's click. */
	SndClickBeep();

	/* Our parent's button widget is used to determine where to place the drop
	 * down list window. */
	NWidgetCore *nwi = w->GetWidget<NWidgetCore>(button);
	Rect wi_rect      = nwi->GetCurrentRect();
	Colours wi_colour = nwi->colour;

	if ((nwi->type & WWT_MASK) == NWID_BUTTON_DROPDOWN) {
		nwi->disp_flags.Set(NWidgetDisplayFlag::DropdownActive);
	} else {
		nwi->SetLowered(true);
	}
	nwi->SetDirty(w);

	if (width != 0) {
		if (_current_text_dir == TD_RTL) {
			wi_rect.left = wi_rect.right + 1 - ScaleGUITrad(width);
		} else {
			wi_rect.right = wi_rect.left + ScaleGUITrad(width) - 1;
		}
	}

	CloseWindowByClass(WC_DROPDOWN_MENU);
	new TDropdownWindow(0, w, std::move(list), selected, button, wi_rect, wi_colour, options);
}

/**
 * Replaces content of the dropdown with new one.
 * @tparam T_DropdownWindow Class of the dropdown.
 * @param parent Parent window of the dropdown.
 * @param list New constent for the dropdown.
 * @param selected_result Optional id of the selected item.
 */
template <DropdownWindowType TDropdownWindow>
void ReplaceDropDownList(Window *parent, DropDownList &&list, std::optional<int> selected_result)
{
	TDropdownWindow *ddw = dynamic_cast<TDropdownWindow *>(parent->FindChildWindow(WC_DROPDOWN_MENU));
	if (ddw != nullptr) ddw->ReplaceList(std::move(list), selected_result);
}
