/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file signs_gui.cpp The GUI for signs. */

#include "stdafx.h"
#include "company_gui.h"
#include "company_func.h"
#include "signs_base.h"
#include "signs_func.h"
#include "debug.h"
#include "command_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "map_func.h"
#include "viewport_func.h"
#include "querystring_gui.h"
#include "sortlist_type.h"
#include "tilehighlight_func.h"
#include "stringfilter_type.h"
#include "string_func.h"
#include "core/geometry_func.hpp"
#include "hotkeys.h"
#include "transparency.h"
#include "gui.h"
#include "signs_cmd.h"
#include "timer/timer.h"
#include "timer/timer_window.h"
#include "dropdown_common_type.h"
#include "dropdown_func.h"

#include "widgets/sign_widget.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "safeguards.h"

struct SignList {
	/**
	 * A GUIList contains signs and uses a StringFilter for filtering.
	 */
	typedef GUIList<const Sign *, std::nullptr_t, StringFilter &> GUISignList;

	GUISignList signs;

	StringFilter string_filter;                                       ///< The match string to be used when the GUIList is (re)-sorted.
	static bool match_case;                                           ///< Should case sensitive matching be used?
	static std::string default_name;                                  ///< Default sign name, used if Sign::name is nullptr.

	/**
	 * Creates a SignList with filtering disabled by default.
	 */
	SignList() : string_filter(&match_case)
	{
	}

	void BuildSignsList()
	{
		if (!this->signs.NeedRebuild()) return;

		Debug(misc, 3, "Building sign list");

		this->signs.clear();
		this->signs.reserve(Sign::GetNumItems());

		for (const Sign *si : Sign::Iterate()) this->signs.push_back(si);

		this->signs.SetFilterState(true);
		this->FilterSignList();
		this->signs.RebuildDone();
	}

	/** Sort signs by their name */
	static bool SignNameSorter(const Sign * const &a, const Sign * const &b)
	{
		/* Signs are very very rarely using the default text, but there can also be
		 * a lot of them. Therefore a worthwhile performance gain can be made by
		 * directly comparing Sign::name instead of going through the string
		 * system for each comparison. */
		const std::string &a_name = a->name.empty() ? SignList::default_name : a->name;
		const std::string &b_name = b->name.empty() ? SignList::default_name : b->name;

		int r = StrNaturalCompare(a_name, b_name); // Sort by name (natural sorting).

		return r != 0 ? r < 0 : (a->index < b->index);
	}

	void SortSignsList()
	{
		if (!this->signs.Sort(&SignNameSorter)) return;
	}

	/** Filter sign list by sign name */
	static bool SignNameFilter(const Sign * const *a, StringFilter &filter)
	{
		/* Same performance benefit as above for sorting. */
		const std::string &a_name = (*a)->name.empty() ? SignList::default_name : (*a)->name;

		filter.ResetState();
		filter.AddLine(a_name);
		return filter.GetState();
	}

	/** Filter sign list excluding OWNER_DEITY */
	static bool OwnerDeityFilter(const Sign * const *a, StringFilter &)
	{
		/* You should never be able to edit signs of owner DEITY */
		return (*a)->owner != OWNER_DEITY;
	}

	/** Filter sign list by owner */
	static bool OwnerVisibilityFilter(const Sign * const *a, StringFilter &)
	{
		assert(!HasBit(_display_opt, DO_SHOW_COMPETITOR_SIGNS));
		/* Hide sign if non-own signs are hidden in the viewport */
		return (*a)->owner == _local_company || (*a)->owner == OWNER_DEITY;
	}

	/** Filter out signs from the sign list that does not match the name filter */
	void FilterSignList()
	{
		this->signs.Filter(&SignNameFilter, this->string_filter);
		if (_game_mode != GM_EDITOR) this->signs.Filter(&OwnerDeityFilter, this->string_filter);
		if (!HasBit(_display_opt, DO_SHOW_COMPETITOR_SIGNS)) {
			this->signs.Filter(&OwnerVisibilityFilter, this->string_filter);
		}
	}
};

bool SignList::match_case = false;
std::string SignList::default_name;

struct SignListWindow : Window, SignList {
	QueryString filter_editbox; ///< Filter editbox;
	int text_offset = 0; ///< Offset of the sign text relative to the left edge of the WID_SIL_LIST widget.
	Scrollbar *vscroll = nullptr;

	SignListWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc), filter_editbox(MAX_LENGTH_SIGN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_SIGN_NAME_CHARS)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_SIL_SCROLLBAR);
		this->FinishInitNested(window_number);
		this->SetWidgetLoweredState(WID_SIL_FILTER_MATCH_CASE_BTN, SignList::match_case);

		/* Initialize the text edit widget */
		this->querystrings[WID_SIL_FILTER_TEXT] = &this->filter_editbox;
		this->filter_editbox.cancel_button = QueryString::ACTION_CLEAR;

		/* Initialize the filtering variables */
		this->SetFilterString("");

		/* Create initial list. */
		this->signs.ForceRebuild();
		this->signs.ForceResort();
		this->BuildSortSignList();
	}

	void OnInit() override
	{
		/* Default sign name, used if Sign::name is nullptr. */
		SignList::default_name = GetString(STR_DEFAULT_SIGN_NAME);
		this->signs.ForceResort();
		this->SortSignsList();
		this->SetDirty();
	}

	/**
	 * This function sets the filter string of the sign list. The contents of
	 * the edit widget is not updated by this function. Depending on if the
	 * new string is zero-length or not the clear button is made
	 * disabled/enabled. The sign list is updated according to the new filter.
	 */
	void SetFilterString(std::string_view new_filter_string)
	{
		/* check if there is a new filter string */
		this->string_filter.SetFilterTerm(new_filter_string);

		/* Rebuild the list of signs */
		this->InvalidateData();
	}

	void OnPaint() override
	{
		if (!this->IsShaded() && this->signs.NeedRebuild()) this->BuildSortSignList();
		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_SIL_LIST: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				uint text_offset_y = (this->resize.step_height - GetCharacterHeight(FS_NORMAL) + 1) / 2;
				/* No signs? */
				if (this->vscroll->GetCount() == 0) {
					DrawString(tr.left, tr.right, tr.top + text_offset_y, STR_STATION_LIST_NONE);
					return;
				}

				Dimension d = GetSpriteSize(SPR_COMPANY_ICON);
				bool rtl = _current_text_dir == TD_RTL;
				int sprite_offset_y = (this->resize.step_height - d.height + 1) / 2;
				uint icon_left = rtl ? tr.right - this->text_offset : tr.left;
				tr = tr.Indent(this->text_offset, rtl);

				/* At least one sign available. */
				auto [first, last] = this->vscroll->GetVisibleRangeIterators(this->signs);
				for (auto it = first; it != last; ++it) {
					const Sign *si = *it;

					if (si->owner != OWNER_NONE) DrawCompanyIcon(si->owner, icon_left, tr.top + sprite_offset_y);

					DrawString(tr.left, tr.right, tr.top + text_offset_y, GetString(STR_SIGN_NAME, si->index), TC_YELLOW);
					tr.top += this->resize.step_height;
				}
				break;
			}
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_SIL_CAPTION) return GetString(STR_SIGN_LIST_CAPTION, this->vscroll->GetCount());

		return this->Window::GetWidgetString(widget, stringid);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_SIL_LIST: {
				auto it = this->vscroll->GetScrolledItemFromWidget(this->signs, pt.y, this, WID_SIL_LIST, WidgetDimensions::scaled.framerect.top);
				if (it == this->signs.end()) return;

				const Sign *si = *it;
				ScrollMainWindowToTile(TileVirtXY(si->x, si->y));
				break;
			}

			case WID_SIL_FILTER_MATCH_CASE_BTN:
				SignList::match_case = !SignList::match_case; // Toggle match case
				this->SetWidgetLoweredState(WID_SIL_FILTER_MATCH_CASE_BTN, SignList::match_case); // Toggle button pushed state
				this->InvalidateData(); // Rebuild the list of signs
				break;
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_SIL_LIST, WidgetDimensions::scaled.framerect.Vertical());
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_SIL_LIST: {
				Dimension spr_dim = GetSpriteSize(SPR_COMPANY_ICON);
				this->text_offset = WidgetDimensions::scaled.frametext.left + spr_dim.width + 2; // 2 pixels space between icon and the sign text.
				fill.height = resize.height = std::max<uint>(GetCharacterHeight(FS_NORMAL), spr_dim.height + 2);
				Dimension d = {(uint)(this->text_offset + WidgetDimensions::scaled.frametext.right), padding.height + 5 * resize.height};
				size = maxdim(size, d);
				break;
			}

			case WID_SIL_CAPTION:
				size = GetStringBoundingBox(GetString(STR_SIGN_LIST_CAPTION, GetParamMaxValue(Sign::GetPoolSize(), 3)));
				size.height += padding.height;
				size.width  += padding.width;
				break;
		}
	}

	void OnEditboxChanged(WidgetID widget) override
	{
		if (widget == WID_SIL_FILTER_TEXT) this->SetFilterString(this->filter_editbox.text.GetText());
	}

	void BuildSortSignList()
	{
		if (this->signs.NeedRebuild()) {
			this->BuildSignsList();
			this->vscroll->SetCount(this->signs.size());
			this->SetWidgetDirty(WID_SIL_CAPTION);
		}
		this->SortSignsList();
	}

	/** Resort the sign listing on a regular interval. */
	const IntervalTimer<TimerWindow> rebuild_interval = {std::chrono::seconds(3), [this](auto) {
		this->BuildSortSignList();
		this->SetDirty();
	}};

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		/* When there is a filter string, we always need to rebuild the list even if
		 * the amount of signs in total is unchanged, as the subset of signs that is
		 * accepted by the filter might has changed. */
		if (data == 0 || data == -1 || !this->string_filter.IsEmpty()) { // New or deleted sign, changed visibility setting or there is a filter string
			/* This needs to be done in command-scope to enforce rebuilding before resorting invalid data */
			this->signs.ForceRebuild();
		} else { // Change of sign contents while there is no filter string
			this->signs.ForceResort();
		}
	}

	/**
	 * Handler for global hotkeys of the SignListWindow.
	 * @param hotkey Hotkey
	 * @return ES_HANDLED if hotkey was accepted.
	 */
	static EventState SignListGlobalHotkeys(int hotkey)
	{
		if (_game_mode == GM_MENU) return ES_NOT_HANDLED;
		Window *w = ShowSignList();
		if (w == nullptr) return ES_NOT_HANDLED;
		return w->OnHotkey(hotkey);
	}

	static inline HotkeyList hotkeys{"signlist", {
		Hotkey('F', "focus_filter_box", WID_SIL_FILTER_TEXT),
	}, SignListGlobalHotkeys};
};

static constexpr std::initializer_list<NWidgetPart> _nested_sign_list_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_SIL_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_PANEL, COLOUR_BROWN, WID_SIL_LIST), SetMinimalSize(WidgetDimensions::unscaled.frametext.Horizontal() + 16 + 255, 0),
								SetResize(1, 1), SetFill(1, 0), SetScrollbar(WID_SIL_SCROLLBAR), EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PANEL, COLOUR_BROWN), SetFill(1, 1),
					NWidget(WWT_EDITBOX, COLOUR_BROWN, WID_SIL_FILTER_TEXT), SetMinimalSize(80, 0), SetResize(1, 0), SetFill(1, 0), SetPadding(2, 2, 2, 2),
							SetStringTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
				EndContainer(),
				NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_SIL_FILTER_MATCH_CASE_BTN), SetStringTip(STR_SIGN_LIST_MATCH_CASE, STR_SIGN_LIST_MATCH_CASE_TOOLTIP),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_SIL_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _sign_list_desc(
	WDP_AUTO, "list_signs", 358, 138,
	WC_SIGN_LIST, WC_NONE,
	{},
	_nested_sign_list_widgets,
	&SignListWindow::hotkeys
);

/**
 * Open the sign list window
 *
 * @return newly opened sign list window, or nullptr if the window could not be opened.
 */
Window *ShowSignList()
{
	return AllocateWindowDescFront<SignListWindow>(_sign_list_desc, 0);
}

/**
 * Actually rename the sign.
 * @param index the sign to rename.
 * @param text  the new name.
 * @param text_colour Colour of the text if the sign is owned by OWNER_DEITY.
 * @return true if the window will already be removed after returning.
 */
static bool RenameSign(SignID index, std::string_view text, Colours text_colour)
{
	bool remove = text.empty();
	Command<Commands::RenameSign>::Post(remove ? STR_ERROR_CAN_T_DELETE_SIGN : STR_ERROR_CAN_T_CHANGE_SIGN_NAME, index, std::string{text}, text_colour);
	return remove;
}

/**
 * Actually move the sign.
 * @param index the sign to move.
 * @param tile on which to move the sign to.
 */
void MoveSign(SignID index, TileIndex tile)
{
	Command<Commands::MoveSign>::Post(STR_ERROR_CAN_T_PLACE_SIGN_HERE, index, tile);
}

struct SignWindow : Window, SignList {
	QueryString name_editbox;
	SignID cur_sign{};
	WidgetID last_user_action = INVALID_WIDGET; ///< Last started user action.
	std::optional<Colours> new_colour; ///< New colour selected by the user. Will be assigned when the OK button is clicked.

	SignWindow(WindowDesc &desc, const Sign *si) : Window(desc), name_editbox(MAX_LENGTH_SIGN_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_SIGN_NAME_CHARS)
	{
		this->querystrings[WID_QES_TEXT] = &this->name_editbox;
		this->name_editbox.caption = STR_EDIT_SIGN_CAPTION;
		this->name_editbox.cancel_button = WID_QES_CANCEL;
		this->name_editbox.ok_button = WID_QES_OK;

		this->InitNested(WN_QUERY_STRING_SIGN);

		if (_game_mode != GameMode::GM_EDITOR) {
			this->GetWidget<NWidgetStacked>(WID_QES_COLOUR_PANE)->SetDisplayedPlane(SZSP_VERTICAL);
			this->ReInit();
		}

		UpdateSignEditWindow(si);
		this->SetFocusedWidget(WID_QES_TEXT);
	}

	void UpdateSignEditWindow(const Sign *si)
	{
		/* Display an empty string when the sign hasn't been edited yet */
		if (!si->name.empty()) {
			this->name_editbox.text.Assign(GetString(STR_SIGN_NAME, si->index));
		} else {
			this->name_editbox.text.DeleteAll();
		}

		this->cur_sign = si->index;
		this->new_colour.reset();

		this->SetWidgetDirty(WID_QES_TEXT);
		this->SetWidgetDirty(WID_QES_COLOUR);
		this->SetFocusedWidget(WID_QES_TEXT);
	}

	/**
	 * Returns a pointer to the (alphabetically) previous or next sign of the current sign.
	 * @param next false if the previous sign is wanted, true if the next sign is wanted
	 * @return pointer to the previous/next sign
	 */
	const Sign *PrevNextSign(bool next)
	{
		/* Rebuild the sign list */
		this->signs.ForceRebuild();
		this->signs.NeedResort();
		this->BuildSignsList();
		this->SortSignsList();

		/* Search through the list for the current sign, excluding
		 * - the first sign if we want the previous sign or
		 * - the last sign if we want the next sign */
		size_t end = this->signs.size() - (next ? 1 : 0);
		for (uint i = next ? 0 : 1; i < end; i++) {
			if (this->cur_sign == this->signs[i]->index) {
				/* We've found the current sign, so return the sign before/after it */
				return this->signs[i + (next ? 1 : -1)];
			}
		}
		/* If we haven't found the current sign by now, return the last/first sign */
		return next ? this->signs.front() : this->signs.back();
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_QES_CAPTION:
				return GetString(this->name_editbox.caption);

			case WID_QES_COLOUR:
				return GetString(STR_COLOUR_DARK_BLUE + this->new_colour.value_or(Sign::Get(this->cur_sign)->text_colour));

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, const Dimension &padding, Dimension &fill, Dimension &resize) override
	{
		if (widget == WID_QES_COLOUR) {
			const Dimension square_size = GetSpriteSize(SPR_SQUARE);
			const uint string_padding = square_size.width + WidgetDimensions::scaled.hsep_normal + padding.width;
			for (Colours colour = COLOUR_BEGIN; colour != COLOUR_END; ++colour) {
				size.width = std::max(size.width, GetStringBoundingBox(STR_COLOUR_DARK_BLUE + colour).width + string_padding);
			}
			size.width = std::max(size.width, GetStringBoundingBox(STR_COLOUR_DEFAULT).width + string_padding);
			return;
		}

		Window::UpdateWidgetSize(widget, size, padding, fill, resize);
	}

	void ShowColourDropDownMenu()
	{
		DropDownList list;
		for (Colours colour = COLOUR_BEGIN; colour != COLOUR_END; ++colour) {
			list.emplace_back(MakeDropDownListIconItem(SPR_SQUARE, GetColourPalette(colour), STR_COLOUR_DARK_BLUE + colour, colour));
		}
		const int selected = this->new_colour.value_or(Sign::Get(this->cur_sign)->text_colour);
		ShowDropDownList(this, std::move(list), selected, WID_QES_COLOUR);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_QES_COLOUR: {
				ShowColourDropDownMenu();
				break;
			}

			case WID_QES_LOCATION: {
				const Sign *si = Sign::Get(this->cur_sign);
				TileIndex tile = TileVirtXY(si->x, si->y);
				if (_ctrl_pressed) {
					ShowExtraViewportWindow(tile);
				} else {
					ScrollMainWindowToTile(tile);
				}
				break;
			}

			case WID_QES_PREVIOUS:
			case WID_QES_NEXT: {
				const Sign *si = this->PrevNextSign(widget == WID_QES_NEXT);

				/* Rebuild the sign list */
				this->signs.ForceRebuild();
				this->signs.NeedResort();
				this->BuildSignsList();
				this->SortSignsList();

				/* Scroll to sign and reopen window */
				ScrollMainWindowToTile(TileVirtXY(si->x, si->y));
				UpdateSignEditWindow(si);
				break;
			}

			case WID_QES_OK:
				if (RenameSign(this->cur_sign, this->name_editbox.text.GetText(), this->new_colour.value_or(INVALID_COLOUR))) break;
				[[fallthrough]];

			case WID_QES_CANCEL:
				this->Close();
				break;

			case WID_QES_DELETE:
				/* Only need to set the buffer to null, the rest is handled as the OK button */
				RenameSign(this->cur_sign, "", INVALID_COLOUR);
				/* don't delete this, we are deleted in Sign::~Sign() -> DeleteRenameSignWindow() */
				break;

			case WID_QES_MOVE:
				HandlePlacePushButton(this, WID_QES_MOVE, SPR_CURSOR_SIGN, HT_RECT);
				this->last_user_action = widget;
				break;
		}
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		switch (this->last_user_action) {
			case WID_QES_MOVE: // Place sign button
				RenameSign(this->cur_sign, this->name_editbox.text.GetText(), this->new_colour.value_or(INVALID_COLOUR));
				MoveSign(this->cur_sign, tile);
				this->Close();
				break;

			default: NOT_REACHED();
		}
	}

	void OnPlaceObjectAbort() override
	{
		this->RaiseButtons();
	}

	void OnDropdownSelect(WidgetID widget, int index, int) override
	{
		if (widget == WID_QES_COLOUR) this->new_colour = static_cast<Colours>(index);
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_query_sign_edit_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_QES_CAPTION), SetTextStyle(TC_WHITE),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_QES_LOCATION), SetAspect(WidgetDimensions::ASPECT_LOCATION), SetSpriteTip(SPR_GOTO_LOCATION, STR_EDIT_SIGN_LOCATION_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(WWT_EDITBOX, COLOUR_GREY, WID_QES_TEXT), SetMinimalSize(256, 0), SetStringTip(STR_EDIT_SIGN_SIGN_OSKTITLE), SetPadding(2, 2, 2, 2),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_QES_OK), SetMinimalSize(61, 12), SetStringTip(STR_BUTTON_OK),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_QES_CANCEL), SetMinimalSize(60, 12), SetStringTip(STR_BUTTON_CANCEL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_QES_DELETE), SetMinimalSize(60, 12), SetStringTip(STR_TOWN_VIEW_DELETE_BUTTON),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_QES_MOVE), SetMinimalSize(60, 12), SetStringTip(STR_BUTTON_MOVE),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_QES_COLOUR_PANE),
			NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_QES_COLOUR), SetMinimalSize(60, 12), SetToolTip(STR_EDIT_SIGN_TEXT_COLOUR_TOOLTIP),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1), EndContainer(),
		NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, WID_QES_PREVIOUS), SetMinimalSize(11, 12), SetArrowWidgetTypeTip(AWV_DECREASE, STR_EDIT_SIGN_PREVIOUS_SIGN_TOOLTIP),
		NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, WID_QES_NEXT), SetMinimalSize(11, 12), SetArrowWidgetTypeTip(AWV_INCREASE, STR_EDIT_SIGN_NEXT_SIGN_TOOLTIP),
	EndContainer(),
};

static WindowDesc _query_sign_edit_desc(
	WDP_CENTER, {}, 0, 0,
	WC_QUERY_STRING, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_query_sign_edit_widgets
);

/**
 * Handle clicking on a sign.
 * @param si The sign that was clicked on.
 */
void HandleClickOnSign(const Sign *si)
{
	/* If we can't edit the sign, don't even open the rename GUI. */
	if (!CompanyCanEditSign(si)) return;

	if (_ctrl_pressed && (si->owner == _local_company || (si->owner == OWNER_DEITY && _game_mode == GM_EDITOR))) {
		RenameSign(si->index, "", INVALID_COLOUR);
		return;
	}

	ShowRenameSignWindow(si);
}

/**
 * Show the window to change the text of a sign.
 * @param si The sign to show the window for.
 */
void ShowRenameSignWindow(const Sign *si)
{
	/* Delete all other edit windows */
	CloseWindowByClass(WC_QUERY_STRING);

	new SignWindow(_query_sign_edit_desc, si);
}

/**
 * Close the sign window associated with the given sign.
 * @param sign The sign to close the window for.
 */
void DeleteRenameSignWindow(SignID sign)
{
	SignWindow *w = dynamic_cast<SignWindow *>(FindWindowById(WC_QUERY_STRING, WN_QUERY_STRING_SIGN));

	if (w != nullptr && w->cur_sign == sign) w->Close();
}
