/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
	GUIList<const Company *> companies{};
	uint rank_width = 0; ///< The width of the rank
	uint text_width = 0; ///< The width of the actual text
	int line_height = 0; ///< Height of the text lines
	Dimension icon{}; ///< Dimension of the company icon.

	/**
	 * (Re)Build the company league list
	 */
	void BuildCompanyList()
	{
		if (!this->companies.NeedRebuild()) return;

		this->companies.clear();
		this->companies.reserve(Company::GetNumItems());

		for (const Company *c : Company::Iterate()) {
			this->companies.push_back(c);
		}

		this->companies.RebuildDone();
	}

	/** Sort the company league by performance history */
	static bool PerformanceSorter(const Company * const &c1, const Company * const &c2)
	{
		return c2->old_economy[0].performance_history < c1->old_economy[0].performance_history;
	}

public:
	PerformanceLeagueWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
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
		Rect rank_rect = ir.WithWidth(this->rank_width, rtl);
		Rect icon_rect = ir.Indent(this->rank_width + WidgetDimensions::scaled.hsep_wide, rtl).WithWidth(this->icon.width, rtl);
		Rect text_rect = ir.Indent(this->rank_width + this->icon.width + WidgetDimensions::scaled.hsep_wide * 2, rtl);

		for (uint i = 0; i != this->companies.size(); i++) {
			const Company *c = this->companies[i];
			DrawString(rank_rect.left, rank_rect.right, ir.top + text_y_offset, GetString(STR_COMPANY_LEAGUE_COMPANY_RANK, i + 1));

			DrawCompanyIcon(c->index, icon_rect.left, ir.top + icon_y_offset);

			DrawString(text_rect.left, text_rect.right, ir.top + text_y_offset, GetString(STR_COMPANY_LEAGUE_COMPANY_NAME, c->index, c->index, GetPerformanceTitleFromValue(c->old_economy[0].performance_history)));
			ir.top += this->line_height;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_PLT_BACKGROUND) return;

		this->rank_width = 0;
		for (uint i = 0; i < MAX_COMPANIES; i++) {
			this->rank_width = std::max(this->rank_width, GetStringBoundingBox(GetString(STR_COMPANY_LEAGUE_COMPANY_RANK, i + 1)).width);
		}

		uint widest_width = 0;
		StringID widest_title = STR_NULL;
		for (auto title : _performance_titles) {
			uint width = GetStringBoundingBox(title).width;
			if (width > widest_width) {
				widest_title = title;
				widest_width = width;
			}
		}

		this->icon = GetSpriteSize(SPR_COMPANY_ICON);
		this->line_height = std::max<int>(this->icon.height + WidgetDimensions::scaled.vsep_normal, GetCharacterHeight(FS_NORMAL));

		for (const Company *c : Company::Iterate()) {
			widest_width = std::max(widest_width, GetStringBoundingBox(GetString(STR_COMPANY_LEAGUE_COMPANY_NAME, c->index, c->index, widest_title)).width);
		}

		this->text_width = widest_width + WidgetDimensions::scaled.hsep_indent * 3; // Keep some extra spacing

		size.width = WidgetDimensions::scaled.framerect.Horizontal() + this->rank_width + WidgetDimensions::scaled.hsep_wide + this->icon.width + WidgetDimensions::scaled.hsep_wide + this->text_width;
		size.height = this->line_height * MAX_COMPANIES + WidgetDimensions::scaled.framerect.Vertical();
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

static constexpr std::initializer_list<NWidgetPart> _nested_performance_league_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_COMPANY_LEAGUE_TABLE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_PLT_BACKGROUND), SetMinimalSize(400, 0), SetMinimalTextLines(15, WidgetDimensions::unscaled.framerect.Vertical()),
	EndContainer(),
};

static WindowDesc _performance_league_desc(
	WDP_AUTO, "performance_league", 0, 0,
	WC_COMPANY_LEAGUE, WC_NONE,
	{},
	_nested_performance_league_widgets
);

void ShowPerformanceLeagueTable()
{
	AllocateWindowDescFront<PerformanceLeagueWindow>(_performance_league_desc, 0);
}

static void HandleLinkClick(Link link)
{
	TileIndex xy;
	switch (link.type) {
		case LT_NONE: return;

		case LT_TILE:
			if (!IsValidTile(link.target)) return;
			xy = TileIndex{link.target};
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
			ShowStoryBook(story_company, static_cast<StoryPageID>(link.target));
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
	LeagueTableID table{};
	std::vector<std::pair<uint, const LeagueTableElement *>> rows{};
	uint rank_width = 0; ///< The width of the rank ordinal
	uint text_width = 0; ///< The width of the actual text
	uint score_width = 0; ///< The width of the score text
	uint header_height = 0; ///< Height of the table header
	int line_height = 0; ///< Height of the text lines
	Dimension icon_size{}; ///< Dimension of the company icon.
	EncodedString title{};

	/**
	 * Rebuild the company league list
	 */
	void BuildTable()
	{
		this->rows.clear();
		this->title = {};

		const LeagueTable *lt = LeagueTable::GetIfValid(this->table);
		if (lt == nullptr) return;

		/* We store title in the window class so we can safely reference the string later */
		this->title = lt->title;

		std::vector<const LeagueTableElement *> elements;
		for (const LeagueTableElement *lte : LeagueTableElement::Iterate()) {
			if (lte->table != this->table) continue;
			elements.push_back(lte);
		}
		std::ranges::sort(elements, [](const LeagueTableElement *a, const LeagueTableElement *b) { return a->rating > b->rating; });

		/* Calculate rank, companies with the same rating share the ranks */
		uint rank = 0;
		for (uint i = 0; i != elements.size(); i++) {
			const LeagueTableElement *lte = elements[i];
			if (i > 0 && elements[i - 1]->rating != lte->rating) rank = i;
			this->rows.emplace_back(rank, lte);
		}
	}

public:
	ScriptLeagueWindow(WindowDesc &desc, WindowNumber table) : Window(desc), table(table)
	{
		this->BuildTable();
		this->InitNested(table);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget != WID_SLT_CAPTION) return this->Window::GetWidgetString(widget, stringid);

		return this->title.GetDecodedString();
	}

	void OnPaint() override
	{
		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_SLT_BACKGROUND) return;

		const LeagueTable *lt = LeagueTable::GetIfValid(this->table);
		if (lt == nullptr) return;

		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);

		if (!lt->header.empty()) {
			ir.top = DrawStringMultiLine(ir, lt->header.GetDecodedString(), TC_BLACK) + WidgetDimensions::scaled.vsep_wide;
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

		for (const auto &[rank, lte] : this->rows) {
			DrawString(rank_rect.left, rank_rect.right, ir.top + text_y_offset, GetString(STR_COMPANY_LEAGUE_COMPANY_RANK, rank + 1));
			if (this->icon_size.width > 0 && lte->company != CompanyID::Invalid()) DrawCompanyIcon(lte->company, icon_rect.left, ir.top + icon_y_offset);
			DrawString(text_rect.left, text_rect.right, ir.top + text_y_offset, lte->text.GetDecodedString(), TC_BLACK);
			DrawString(score_rect.left, score_rect.right, ir.top + text_y_offset, lte->score.GetDecodedString(), TC_BLACK, SA_RIGHT | SA_FORCE);
			ir.top += this->line_height;
		}

		if (!lt->footer.empty()) {
			ir.top += WidgetDimensions::scaled.vsep_wide;
			ir.top = DrawStringMultiLine(ir, lt->footer.GetDecodedString(), TC_BLACK);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_SLT_BACKGROUND) return;

		const LeagueTable *lt = LeagueTable::GetIfValid(this->table);
		if (lt == nullptr) return;

		this->icon_size = GetSpriteSize(SPR_COMPANY_ICON);
		this->line_height = std::max<int>(this->icon_size.height + WidgetDimensions::scaled.fullbevel.Vertical(), GetCharacterHeight(FS_NORMAL));

		/* Calculate maximum width of every column */
		this->rank_width = this->text_width = this->score_width = 0;
		bool show_icon_column = false;
		for (const auto &[rank, lte] : this->rows) {
			this->rank_width = std::max(this->rank_width, GetStringBoundingBox(GetString(STR_COMPANY_LEAGUE_COMPANY_RANK, rank + 1)).width);
			this->text_width = std::max(this->text_width, GetStringBoundingBox(lte->text.GetDecodedString()).width);
			this->score_width = std::max(this->score_width, GetStringBoundingBox(lte->score.GetDecodedString()).width);
			if (lte->company != CompanyID::Invalid()) show_icon_column = true;
		}

		if (!show_icon_column) {
			this->icon_size.width = 0;
		} else {
			this->icon_size.width += WidgetDimensions::scaled.hsep_wide;
		}

		uint non_text_width = this->rank_width + this->icon_size.width + this->score_width + WidgetDimensions::scaled.framerect.Horizontal() + WidgetDimensions::scaled.hsep_wide * 2;
		size.width = std::max(size.width, non_text_width + this->text_width);
		uint used_height = this->line_height * std::max<uint>(3u, static_cast<uint>(this->rows.size())) + WidgetDimensions::scaled.framerect.Vertical();

		/* Adjust text_width to fill any space left over if the preset minimal width is larger than our calculated width. */
		this->text_width = size.width - non_text_width;

		if (!lt->header.empty()) {
			this->header_height = GetStringHeight(lt->header.GetDecodedString(), size.width - WidgetDimensions::scaled.framerect.Horizontal()) + WidgetDimensions::scaled.vsep_wide;
		} else {
			this->header_height = 0;
		}
		used_height += this->header_height;

		if (!lt->footer.empty()) {
			used_height += GetStringHeight(lt->footer.GetDecodedString(), size.width - WidgetDimensions::scaled.framerect.Horizontal()) + WidgetDimensions::scaled.vsep_wide;
		}

		size.height = std::max(size.height, used_height);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget != WID_SLT_BACKGROUND) return;

		auto *wid = this->GetWidget<NWidgetResizeBase>(WID_SLT_BACKGROUND);
		int index = (pt.y - WidgetDimensions::scaled.framerect.top - wid->pos_y - this->header_height) / this->line_height;
		if (index >= 0 && static_cast<uint>(index) < this->rows.size()) {
			const LeagueTableElement *lte = this->rows[index].second;
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

static constexpr std::initializer_list<NWidgetPart> _nested_script_league_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_SLT_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_SLT_BACKGROUND), SetMinimalSize(400, 0), SetMinimalTextLines(15, WidgetDimensions::unscaled.framerect.Vertical()),
	EndContainer(),
};

static WindowDesc _script_league_desc(
	WDP_AUTO, "script_league", 0, 0,
	WC_COMPANY_LEAGUE, WC_NONE,
	{},
	_nested_script_league_widgets
);

void ShowScriptLeagueTable(LeagueTableID table)
{
	if (!LeagueTable::IsValidID(table)) return;
	AllocateWindowDescFront<ScriptLeagueWindow>(_script_league_desc, table);
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
