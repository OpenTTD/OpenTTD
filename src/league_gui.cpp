/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file league_gui.cpp GUI for league tables. */

#include "stdafx.h"
#include "league_gui.h"

#include "company_base.h"
#include "company_gui.h"
#include "gui.h"
#include "industry.h"
#include "league_base.h"
#include "sortlist_type.h"
#include "story_base.h"
#include "strings_func.h"
#include "tile_map.h"
#include "town.h"
#include "viewport_func.h"
#include "window_gui.h"
#include "widgets/league_widget.h"
#include "table/strings.h"
#include "table/sprites.h"

#include "safeguards.h"


static const StringID _performance_titles[] = {
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_ENGINEER,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_ENGINEER,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TRAFFIC_MANAGER,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TRAFFIC_MANAGER,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TRANSPORT_COORDINATOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TRANSPORT_COORDINATOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_ROUTE_SUPERVISOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_ROUTE_SUPERVISOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_DIRECTOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_DIRECTOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_CHIEF_EXECUTIVE,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_CHIEF_EXECUTIVE,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_CHAIRMAN,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_CHAIRMAN,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_PRESIDENT,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TYCOON,
};

static inline StringID GetPerformanceTitleFromValue(uint value)
{
	return _performance_titles[std::min(value, 1000u) >> 6];
}

class PerformanceLeagueWindow : public Window {
private:
	GUIList<const Company *> companies;
	uint ordinal_width; ///< The width of the ordinal number
	uint text_width;    ///< The width of the actual text
	int line_height;    ///< Height of the text lines
	Dimension icon;     ///< Dimension of the company icon.

	/**
	 * (Re)Build the company league list
	 */
	void BuildCompanyList()
	{
		if (!this->companies.NeedRebuild()) return;

		this->companies.clear();

		for (const Company *c : Company::Iterate()) {
			this->companies.push_back(c);
		}

		this->companies.shrink_to_fit();
		this->companies.RebuildDone();
	}

	/** Sort the company league by performance history */
	static bool PerformanceSorter(const Company * const &c1, const Company * const &c2)
	{
		return c2->old_economy[0].performance_history < c1->old_economy[0].performance_history;
	}

public:
	PerformanceLeagueWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);
		this->companies.ForceRebuild();
		this->companies.NeedResort();
	}

	void OnPaint() override
	{
		this->BuildCompanyList();
		this->companies.Sort(&PerformanceSorter);

		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_PLT_BACKGROUND) return;

		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		int icon_y_offset = (this->line_height - this->icon.height) / 2;
		int text_y_offset = (this->line_height - GetCharacterHeight(FS_NORMAL)) / 2;

		bool rtl = _current_text_dir == TD_RTL;
		Rect ordinal = ir.WithWidth(this->ordinal_width, rtl);
		uint icon_left = ir.Indent(rtl ? this->text_width : this->ordinal_width, rtl).left;
		Rect text    = ir.WithWidth(this->text_width, !rtl);

		for (uint i = 0; i != this->companies.size(); i++) {
			const Company *c = this->companies[i];
			DrawString(ordinal.left, ordinal.right, ir.top + text_y_offset, i + STR_ORDINAL_NUMBER_1ST, i == 0 ? TC_WHITE : TC_YELLOW);

			DrawCompanyIcon(c->index, icon_left, ir.top + icon_y_offset);

			SetDParam(0, c->index);
			SetDParam(1, c->index);
			SetDParam(2, GetPerformanceTitleFromValue(c->old_economy[0].performance_history));
			DrawString(text.left, text.right, ir.top + text_y_offset, STR_COMPANY_LEAGUE_COMPANY_NAME);
			ir.top += this->line_height;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != WID_PLT_BACKGROUND) return;

		this->ordinal_width = 0;
		for (uint i = 0; i < MAX_COMPANIES; i++) {
			this->ordinal_width = std::max(this->ordinal_width, GetStringBoundingBox(STR_ORDINAL_NUMBER_1ST + i).width);
		}
		this->ordinal_width += WidgetDimensions::scaled.hsep_wide; // Keep some extra spacing

		uint widest_width = 0;
		uint widest_title = 0;
		for (uint i = 0; i < lengthof(_performance_titles); i++) {
			uint width = GetStringBoundingBox(_performance_titles[i]).width;
			if (width > widest_width) {
				widest_title = i;
				widest_width = width;
			}
		}

		this->icon = GetSpriteSize(SPR_COMPANY_ICON);
		this->line_height = std::max<int>(this->icon.height + WidgetDimensions::scaled.vsep_normal, GetCharacterHeight(FS_NORMAL));

		for (const Company *c : Company::Iterate()) {
			SetDParam(0, c->index);
			SetDParam(1, c->index);
			SetDParam(2, _performance_titles[widest_title]);
			widest_width = std::max(widest_width, GetStringBoundingBox(STR_COMPANY_LEAGUE_COMPANY_NAME).width);
		}

		this->text_width = widest_width + WidgetDimensions::scaled.hsep_indent * 3; // Keep some extra spacing

		size->width = WidgetDimensions::scaled.framerect.Horizontal() + this->ordinal_width + this->icon.width + this->text_width + WidgetDimensions::scaled.hsep_wide;
		size->height = this->line_height * MAX_COMPANIES + WidgetDimensions::scaled.framerect.Vertical();
	}

	void OnGameTick() override
	{
		if (this->companies.NeedResort()) {
			this->SetDirty();
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (data == 0) {
			/* This needs to be done in command-scope to enforce rebuilding before resorting invalid data */
			this->companies.ForceRebuild();
		} else {
			this->companies.ForceResort();
		}
	}
};

static const NWidgetPart _nested_performance_league_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_COMPANY_LEAGUE_TABLE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_PLT_BACKGROUND), SetMinimalSize(400, 0), SetMinimalTextLines(15, WidgetDimensions::unscaled.framerect.Vertical()),
	EndContainer(),
};

static WindowDesc _performance_league_desc(__FILE__, __LINE__,
	WDP_AUTO, "performance_league", 0, 0,
	WC_COMPANY_LEAGUE, WC_NONE,
	0,
	std::begin(_nested_performance_league_widgets), std::end(_nested_performance_league_widgets)
);

void ShowPerformanceLeagueTable()
{
	AllocateWindowDescFront<PerformanceLeagueWindow>(&_performance_league_desc, 0);
}

static void HandleLinkClick(Link link)
{
	TileIndex xy;
	switch (link.type) {
		case LT_NONE: return;

		case LT_TILE:
			if (!IsValidTile(link.target)) return;
			xy = link.target;
			break;

		case LT_INDUSTRY:
			if (!Industry::IsValidID(link.target)) return;
			xy = Industry::Get(link.target)->location.tile;
			break;

		case LT_TOWN:
			if (!Town::IsValidID(link.target)) return;
			xy = Town::Get(link.target)->xy;
			break;

		case LT_COMPANY:
			ShowCompany((CompanyID)link.target);
			return;

		case LT_STORY_PAGE: {
			if (!StoryPage::IsValidID(link.target)) return;
			CompanyID story_company = StoryPage::Get(link.target)->company;
			ShowStoryBook(story_company, link.target);
			return;
		}

		default: NOT_REACHED();
	}

	if (_ctrl_pressed) {
		ShowExtraViewportWindow(xy);
	} else {
		ScrollMainWindowToTile(xy);
	}
}


class ScriptLeagueWindow : public Window {
private:
	LeagueTableID table;
	std::vector<std::pair<uint, const LeagueTableElement *>> rows;
	uint rank_width;     ///< The width of the rank ordinal
	uint text_width;     ///< The width of the actual text
	uint score_width;    ///< The width of the score text
	uint header_height;  ///< Height of the table header
	int line_height;     ///< Height of the text lines
	Dimension icon_size; ///< Dimenion of the company icon.
	std::string title;

	/**
	 * Rebuild the company league list
	 */
	void BuildTable()
	{
		this->rows.clear();
		this->title = std::string{};
		auto lt = LeagueTable::GetIfValid(this->table);
		if (lt == nullptr) return;

		/* We store title in the window class so we can safely reference the string later */
		this->title = lt->title;

		std::vector<const LeagueTableElement *> elements;
		for (LeagueTableElement *lte : LeagueTableElement::Iterate()) {
			if (lte->table == this->table) {
				elements.push_back(lte);
			}
		}
		std::sort(elements.begin(), elements.end(), [](auto a, auto b) { return a->rating > b->rating; });

		/* Calculate rank, companies with the same rating share the ranks */
		uint rank = 0;
		for (uint i = 0; i != elements.size(); i++) {
			auto *lte = elements[i];
			if (i > 0 && elements[i - 1]->rating != lte->rating) rank = i;
			this->rows.emplace_back(std::make_pair(rank, lte));
		}
	}

public:
	ScriptLeagueWindow(WindowDesc *desc, LeagueTableID table) : Window(desc)
	{
		this->table = table;
		this->BuildTable();
		this->InitNested(table);
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget != WID_SLT_CAPTION) return;
		SetDParamStr(0, this->title);
	}

	void OnPaint() override
	{
		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_SLT_BACKGROUND) return;

		auto lt = LeagueTable::GetIfValid(this->table);
		if (lt == nullptr) return;

		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);

		if (!lt->header.empty()) {
			SetDParamStr(0, lt->header);
			ir.top = DrawStringMultiLine(ir.left, ir.right, ir.top, UINT16_MAX, STR_JUST_RAW_STRING, TC_BLACK) + WidgetDimensions::scaled.vsep_wide;
		}

		int icon_y_offset = (this->line_height - this->icon_size.height) / 2;
		int text_y_offset = (this->line_height - GetCharacterHeight(FS_NORMAL)) / 2;

		/* Calculate positions.of the columns */
		bool rtl = _current_text_dir == TD_RTL;
		int spacer = WidgetDimensions::scaled.hsep_wide;
		Rect rank_rect = ir.WithWidth(this->rank_width, rtl);
		Rect icon_rect = ir.Indent(this->rank_width + (rtl ? 0 : spacer), rtl).WithWidth(this->icon_size.width, rtl);
		Rect text_rect = ir.Indent(this->rank_width + spacer + this->icon_size.width, rtl).WithWidth(this->text_width, rtl);
		Rect score_rect = ir.Indent(this->rank_width + 2 * spacer + this->icon_size.width + this->text_width, rtl).WithWidth(this->score_width, rtl);

		for (auto [rank, lte] : this->rows) {
			DrawString(rank_rect.left, rank_rect.right, ir.top + text_y_offset, rank + STR_ORDINAL_NUMBER_1ST, rank == 0 ? TC_WHITE : TC_YELLOW);
			if (this->icon_size.width > 0 && lte->company != INVALID_COMPANY) DrawCompanyIcon(lte->company, icon_rect.left, ir.top + icon_y_offset);
			SetDParamStr(0, lte->text);
			DrawString(text_rect.left, text_rect.right, ir.top + text_y_offset, STR_JUST_RAW_STRING, TC_BLACK);
			SetDParamStr(0, lte->score);
			DrawString(score_rect.left, score_rect.right, ir.top + text_y_offset, STR_JUST_RAW_STRING, TC_BLACK, SA_RIGHT);
			ir.top += this->line_height;
		}

		if (!lt->footer.empty()) {
			ir.top += WidgetDimensions::scaled.vsep_wide;
			SetDParamStr(0, lt->footer);
			ir.top = DrawStringMultiLine(ir.left, ir.right, ir.top, UINT16_MAX, STR_JUST_RAW_STRING, TC_BLACK);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != WID_SLT_BACKGROUND) return;

		auto lt = LeagueTable::GetIfValid(this->table);
		if (lt == nullptr) return;

		this->icon_size = GetSpriteSize(SPR_COMPANY_ICON);
		this->line_height = std::max<int>(this->icon_size.height + WidgetDimensions::scaled.fullbevel.Vertical(), GetCharacterHeight(FS_NORMAL));

		/* Calculate maximum width of every column */
		this->rank_width = this->text_width = this->score_width = 0;
		bool show_icon_column = false;
		for (auto [rank, lte] : this->rows) {
			this->rank_width = std::max(this->rank_width, GetStringBoundingBox(STR_ORDINAL_NUMBER_1ST + rank).width);
			SetDParamStr(0, lte->text);
			this->text_width = std::max(this->text_width, GetStringBoundingBox(STR_JUST_RAW_STRING).width);
			SetDParamStr(0, lte->score);
			this->score_width = std::max(this->score_width, GetStringBoundingBox(STR_JUST_RAW_STRING).width);
			if (lte->company != INVALID_COMPANY) show_icon_column = true;
		}

		if (!show_icon_column) this->icon_size.width = 0;
		else this->icon_size.width += WidgetDimensions::scaled.hsep_wide;

		size->width = this->rank_width + this->icon_size.width + this->text_width + this->score_width + WidgetDimensions::scaled.framerect.Horizontal() + WidgetDimensions::scaled.hsep_wide * 2;
		size->height = this->line_height * std::max<uint>(3u, (unsigned)this->rows.size()) + WidgetDimensions::scaled.framerect.Vertical();

		if (!lt->header.empty()) {
			SetDParamStr(0, lt->header);
			this->header_height = GetStringHeight(STR_JUST_RAW_STRING, size->width - WidgetDimensions::scaled.framerect.Horizontal()) + WidgetDimensions::scaled.vsep_wide;
			size->height += header_height;
		} else this->header_height = 0;

		if (!lt->footer.empty()) {
			SetDParamStr(0, lt->footer);
			size->height += GetStringHeight(STR_JUST_RAW_STRING, size->width - WidgetDimensions::scaled.framerect.Horizontal()) + WidgetDimensions::scaled.vsep_wide;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget != WID_SLT_BACKGROUND) return;

		auto *wid = this->GetWidget<NWidgetResizeBase>(WID_SLT_BACKGROUND);
		int index = (pt.y - WidgetDimensions::scaled.framerect.top - wid->pos_y - this->header_height) / this->line_height;
		if (index >= 0 && (uint)index < this->rows.size()) {
			auto lte = this->rows[index].second;
			HandleLinkClick(lte->link);
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		this->BuildTable();
		this->ReInit();
	}
};

static const NWidgetPart _nested_script_league_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_SLT_CAPTION), SetDataTip(STR_JUST_RAW_STRING, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_SLT_BACKGROUND), SetMinimalSize(400, 0), SetMinimalTextLines(15, WidgetDimensions::scaled.framerect.Vertical()),
	EndContainer(),
};

static WindowDesc _script_league_desc(__FILE__, __LINE__,
	WDP_AUTO, "script_league", 0, 0,
	WC_COMPANY_LEAGUE, WC_NONE,
	0,
	std::begin(_nested_script_league_widgets), std::end(_nested_script_league_widgets)
);

void ShowScriptLeagueTable(LeagueTableID table)
{
	if (!LeagueTable::IsValidID(table)) return;
	AllocateWindowDescFront<ScriptLeagueWindow>(&_script_league_desc, table);
}

void ShowFirstLeagueTable()
{
	auto it = LeagueTable::Iterate();
	if (!it.empty()) {
		ShowScriptLeagueTable((*it.begin())->index);
	} else {
		ShowPerformanceLeagueTable();
	}
}
