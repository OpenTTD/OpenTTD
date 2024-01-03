/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file linkgraph_gui.cpp Implementation of linkgraph overlay GUI. */

#include "../stdafx.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../company_base.h"
#include "../company_gui.h"
#include "../timer/timer_game_tick.h"
#include "../timer/timer_game_calendar.h"
#include "../viewport_func.h"
#include "../zoom_func.h"
#include "../smallmap_gui.h"
#include "../core/geometry_func.hpp"
#include "../widgets/link_graph_legend_widget.h"
#include "../strings_func.h"
#include "linkgraph_gui.h"

#include "table/strings.h"

#include "../safeguards.h"

/**
 * Colours for the various "load" states of links. Ordered from "unused" to
 * "overloaded".
 */
const uint8_t LinkGraphOverlay::LINK_COLOURS[][12] = {
{
	0x0f, 0xd1, 0xd0, 0x57,
	0x55, 0x53, 0xbf, 0xbd,
	0xba, 0xb9, 0xb7, 0xb5
},
{
	0x0f, 0xd1, 0xd0, 0x57,
	0x55, 0x53, 0x96, 0x95,
	0x94, 0x93, 0x92, 0x91
},
{
	0x0f, 0x0b, 0x09, 0x07,
	0x05, 0x03, 0xbf, 0xbd,
	0xba, 0xb9, 0xb7, 0xb5
},
{
	0x0f, 0x0b, 0x0a, 0x09,
	0x08, 0x07, 0x06, 0x05,
	0x04, 0x03, 0x02, 0x01
}
};

/**
 * Get a DPI for the widget we will be drawing to.
 * @param dpi DrawPixelInfo to fill with the desired dimensions.
 */
void LinkGraphOverlay::GetWidgetDpi(DrawPixelInfo *dpi) const
{
	const NWidgetBase *wi = this->window->GetWidget<NWidgetBase>(this->widget_id);
	dpi->left = dpi->top = 0;
	dpi->width = wi->current_x;
	dpi->height = wi->current_y;
}

/**
 * Rebuild the cache and recalculate which links and stations to be shown.
 */
void LinkGraphOverlay::RebuildCache()
{
	this->cached_links.clear();
	this->cached_stations.clear();
	if (this->company_mask == 0) return;

	DrawPixelInfo dpi;
	this->GetWidgetDpi(&dpi);

	for (const Station *sta : Station::Iterate()) {
		if (sta->rect.IsEmpty()) continue;

		Point pta = this->GetStationMiddle(sta);

		StationID from = sta->index;
		StationLinkMap &seen_links = this->cached_links[from];

		uint supply = 0;
		for (CargoID c : SetCargoBitIterator(this->cargo_mask)) {
			if (!CargoSpec::Get(c)->IsValid()) continue;
			if (!LinkGraph::IsValidID(sta->goods[c].link_graph)) continue;
			const LinkGraph &lg = *LinkGraph::Get(sta->goods[c].link_graph);

			ConstNode &from_node = lg[sta->goods[c].node];
			supply += lg.Monthly(from_node.supply);
			for (const Edge &edge : from_node.edges) {
				StationID to = lg[edge.dest_node].station;
				assert(from != to);
				if (!Station::IsValidID(to) || seen_links.find(to) != seen_links.end()) {
					continue;
				}
				const Station *stb = Station::Get(to);
				assert(sta != stb);

				/* Show links between stations of selected companies or "neutral" ones like oilrigs. */
				if (stb->owner != OWNER_NONE && sta->owner != OWNER_NONE && !HasBit(this->company_mask, stb->owner)) continue;
				if (stb->rect.IsEmpty()) continue;

				if (!this->IsLinkVisible(pta, this->GetStationMiddle(stb), &dpi)) continue;

				this->AddLinks(sta, stb);
				seen_links[to]; // make sure it is created and marked as seen
			}
		}
		if (this->IsPointVisible(pta, &dpi)) {
			this->cached_stations.push_back(std::make_pair(from, supply));
		}
	}
}

/**
 * Determine if a certain point is inside the given DPI, with some lee way.
 * @param pt Point we are looking for.
 * @param dpi Visible area.
 * @param padding Extent of the point.
 * @return If the point or any of its 'extent' is inside the dpi.
 */
inline bool LinkGraphOverlay::IsPointVisible(Point pt, const DrawPixelInfo *dpi, int padding) const
{
	return pt.x > dpi->left - padding && pt.y > dpi->top - padding &&
			pt.x < dpi->left + dpi->width + padding &&
			pt.y < dpi->top + dpi->height + padding;
}

/**
 * Determine if a certain link crosses through the area given by the dpi with some lee way.
 * @param pta First end of the link.
 * @param ptb Second end of the link.
 * @param dpi Visible area.
 * @param padding Width or thickness of the link.
 * @return If the link or any of its "thickness" is visible. This may return false positives.
 */
inline bool LinkGraphOverlay::IsLinkVisible(Point pta, Point ptb, const DrawPixelInfo *dpi, int padding) const
{
	const int left = dpi->left - padding;
	const int right = dpi->left + dpi->width + padding;
	const int top = dpi->top - padding;
	const int bottom = dpi->top + dpi->height + padding;

	/*
	 * This method is an implementation of the Cohen-Sutherland line-clipping algorithm.
	 * See: https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
	 */

	const uint8_t INSIDE = 0; // 0000
	const uint8_t LEFT   = 1; // 0001
	const uint8_t RIGHT  = 2; // 0010
	const uint8_t BOTTOM = 4; // 0100
	const uint8_t TOP    = 8; // 1000

	int x0 = pta.x;
	int y0 = pta.y;
	int x1 = ptb.x;
	int y1 = ptb.y;

	auto out_code = [&](int x, int y) -> uint8_t {
		uint8_t out = INSIDE;
		if (x < left) {
			out |= LEFT;
		} else if (x > right) {
			out |= RIGHT;
		}
		if (y < top) {
			out |= TOP;
		} else if (y > bottom) {
			out |= BOTTOM;
		}
		return out;
	};

	uint8_t c0 = out_code(x0, y0);
	uint8_t c1 = out_code(x1, y1);

	while (true) {
		if (c0 == 0 || c1 == 0) return true;
		if ((c0 & c1) != 0) return false;

		if (c0 & TOP) {           // point 0 is above the clip window
			x0 = x0 + (int)(((int64_t) (x1 - x0)) * ((int64_t) (top - y0)) / ((int64_t) (y1 - y0)));
			y0 = top;
		} else if (c0 & BOTTOM) { // point 0 is below the clip window
			x0 = x0 + (int)(((int64_t) (x1 - x0)) * ((int64_t) (bottom - y0)) / ((int64_t) (y1 - y0)));
			y0 = bottom;
		} else if (c0 & RIGHT) {  // point 0 is to the right of clip window
			y0 = y0 + (int)(((int64_t) (y1 - y0)) * ((int64_t) (right - x0)) / ((int64_t) (x1 - x0)));
			x0 = right;
		} else if (c0 & LEFT) {   // point 0 is to the left of clip window
			y0 = y0 + (int)(((int64_t) (y1 - y0)) * ((int64_t) (left - x0)) / ((int64_t) (x1 - x0)));
			x0 = left;
		}

		c0 = out_code(x0, y0);
	}

	NOT_REACHED();
}

/**
 * Add all "interesting" links between the given stations to the cache.
 * @param from The source station.
 * @param to The destination station.
 */
void LinkGraphOverlay::AddLinks(const Station *from, const Station *to)
{
	for (CargoID c : SetCargoBitIterator(this->cargo_mask)) {
		if (!CargoSpec::Get(c)->IsValid()) continue;
		const GoodsEntry &ge = from->goods[c];
		if (!LinkGraph::IsValidID(ge.link_graph) ||
				ge.link_graph != to->goods[c].link_graph) {
			continue;
		}
		const LinkGraph &lg = *LinkGraph::Get(ge.link_graph);
		if (lg[ge.node].HasEdgeTo(to->goods[c].node)) {
			ConstEdge &edge = lg[ge.node][to->goods[c].node];
			this->AddStats(c, lg.Monthly(edge.capacity), lg.Monthly(edge.usage),
					ge.flows.GetFlowVia(to->index),
					edge.TravelTime() / Ticks::DAY_TICKS,
					from->owner == OWNER_NONE || to->owner == OWNER_NONE,
					this->cached_links[from->index][to->index]);
		}
	}
}

/**
 * Add information from a given pair of link stat and flow stat to the given
 * link properties. The shown usage or plan is always the maximum of all link
 * stats involved.
 * @param new_cap Capacity of the new link.
 * @param new_usg Usage of the new link.
 * @param new_plan Planned flow for the new link.
 * @param new_shared If the new link is shared.
 * @param cargo LinkProperties to write the information to.
 */
/* static */ void LinkGraphOverlay::AddStats(CargoID new_cargo, uint new_cap, uint new_usg, uint new_plan, uint32_t time, bool new_shared, LinkProperties &cargo)
{
	/* multiply the numbers by 32 in order to avoid comparing to 0 too often. */
	if (cargo.capacity == 0 ||
			cargo.Usage() * 32 / (cargo.capacity + 1) < std::max(new_usg, new_plan) * 32 / (new_cap + 1)) {
		cargo.cargo = new_cargo;
		cargo.capacity = new_cap;
		cargo.usage = new_usg;
		cargo.planned = new_plan;
		cargo.time = time;
	}
	if (new_shared) cargo.shared = true;
}

/**
 * Draw the linkgraph overlay or some part of it, in the area given.
 * @param dpi Area to be drawn to.
 */
void LinkGraphOverlay::Draw(const DrawPixelInfo *dpi)
{
	if (this->dirty) {
		this->RebuildCache();
		this->dirty = false;
	}
	this->DrawLinks(dpi);
	this->DrawStationDots(dpi);
}

/**
 * Draw the cached links or part of them into the given area.
 * @param dpi Area to be drawn to.
 */
void LinkGraphOverlay::DrawLinks(const DrawPixelInfo *dpi) const
{
	int width = ScaleGUITrad(this->scale);
	for (const auto &i : this->cached_links) {
		if (!Station::IsValidID(i.first)) continue;
		Point pta = this->GetStationMiddle(Station::Get(i.first));
		for (const auto &j : i.second) {
			if (!Station::IsValidID(j.first)) continue;
			Point ptb = this->GetStationMiddle(Station::Get(j.first));
			if (!this->IsLinkVisible(pta, ptb, dpi, width + 2)) continue;
			this->DrawContent(pta, ptb, j.second);
		}
	}
}

/**
 * Draw one specific link.
 * @param pta Source of the link.
 * @param ptb Destination of the link.
 * @param cargo Properties of the link.
 */
void LinkGraphOverlay::DrawContent(Point pta, Point ptb, const LinkProperties &cargo) const
{
	uint usage_or_plan = std::min(cargo.capacity * 2 + 1, cargo.Usage());
	int colour = LinkGraphOverlay::LINK_COLOURS[_settings_client.gui.linkgraph_colours][usage_or_plan * lengthof(LinkGraphOverlay::LINK_COLOURS[0]) / (cargo.capacity * 2 + 2)];
	int width = ScaleGUITrad(this->scale);
	int dash = cargo.shared ? width * 4 : 0;

	/* Move line a bit 90° against its dominant direction to prevent it from
	 * being hidden below the grey line. */
	int side = _settings_game.vehicle.road_side ? 1 : -1;
	if (abs(pta.x - ptb.x) < abs(pta.y - ptb.y)) {
		int offset_x = (pta.y > ptb.y ? 1 : -1) * side * width;
		GfxDrawLine(pta.x + offset_x, pta.y, ptb.x + offset_x, ptb.y, colour, width, dash);
	} else {
		int offset_y = (pta.x < ptb.x ? 1 : -1) * side * width;
		GfxDrawLine(pta.x, pta.y + offset_y, ptb.x, ptb.y + offset_y, colour, width, dash);
	}

	GfxDrawLine(pta.x, pta.y, ptb.x, ptb.y, _colour_gradient[COLOUR_GREY][1], width);
}

/**
 * Draw dots for stations into the smallmap. The dots' sizes are determined by the amount of
 * cargo produced there, their colours by the type of cargo produced.
 */
void LinkGraphOverlay::DrawStationDots(const DrawPixelInfo *dpi) const
{
	int width = ScaleGUITrad(this->scale);
	for (const auto &i : this->cached_stations) {
		const Station *st = Station::GetIfValid(i.first);
		if (st == nullptr) continue;
		Point pt = this->GetStationMiddle(st);
		if (!this->IsPointVisible(pt, dpi, 3 * width)) continue;

		uint r = width * 2 + width * 2 * std::min(200U, i.second) / 200;

		LinkGraphOverlay::DrawVertex(pt.x, pt.y, r,
				_colour_gradient[st->owner != OWNER_NONE ?
						(Colours)Company::Get(st->owner)->colour : COLOUR_GREY][5],
				_colour_gradient[COLOUR_GREY][1]);
	}
}

/**
 * Draw a square symbolizing a producer of cargo.
 * @param x X coordinate of the middle of the vertex.
 * @param y Y coordinate of the middle of the vertex.
 * @param size Y and y extend of the vertex.
 * @param colour Colour with which the vertex will be filled.
 * @param border_colour Colour for the border of the vertex.
 */
/* static */ void LinkGraphOverlay::DrawVertex(int x, int y, int size, int colour, int border_colour)
{
	size--;
	int w1 = size / 2;
	int w2 = size / 2 + size % 2;

	GfxFillRect(x - w1, y - w1, x + w2, y + w2, colour);

	w1++;
	w2++;
	GfxDrawLine(x - w1, y - w1, x + w2, y - w1, border_colour);
	GfxDrawLine(x - w1, y + w2, x + w2, y + w2, border_colour);
	GfxDrawLine(x - w1, y - w1, x - w1, y + w2, border_colour);
	GfxDrawLine(x + w2, y - w1, x + w2, y + w2, border_colour);
}

bool LinkGraphOverlay::ShowTooltip(Point pt, TooltipCloseCondition close_cond)
{
	for (auto i(this->cached_links.crbegin()); i != this->cached_links.crend(); ++i) {
		if (!Station::IsValidID(i->first)) continue;
		Point pta = this->GetStationMiddle(Station::Get(i->first));
		for (auto j(i->second.crbegin()); j != i->second.crend(); ++j) {
			if (!Station::IsValidID(j->first)) continue;
			if (i->first == j->first) continue;

			/* Check the distance from the cursor to the line defined by the two stations. */
			Point ptb = this->GetStationMiddle(Station::Get(j->first));
			float dist = std::abs((int64_t)(ptb.x - pta.x) * (int64_t)(pta.y - pt.y) - (int64_t)(pta.x - pt.x) * (int64_t)(ptb.y - pta.y)) /
				std::sqrt((int64_t)(ptb.x - pta.x) * (int64_t)(ptb.x - pta.x) + (int64_t)(ptb.y - pta.y) * (int64_t)(ptb.y - pta.y));
			const auto &link = j->second;
			if (dist <= 4 && link.Usage() > 0 &&
					pt.x + 2 >= std::min(pta.x, ptb.x) &&
					pt.x - 2 <= std::max(pta.x, ptb.x) &&
					pt.y + 2 >= std::min(pta.y, ptb.y) &&
					pt.y - 2 <= std::max(pta.y, ptb.y)) {
				static std::string tooltip_extension;
				tooltip_extension.clear();
				/* Fill buf with more information if this is a bidirectional link. */
				uint32_t back_time = 0;
				auto k = this->cached_links[j->first].find(i->first);
				if (k != this->cached_links[j->first].end()) {
					const auto &back = k->second;
					back_time = back.time;
					if (back.Usage() > 0) {
						SetDParam(0, back.cargo);
						SetDParam(1, back.Usage());
						SetDParam(2, back.Usage() * 100 / (back.capacity + 1));
						tooltip_extension = GetString(STR_LINKGRAPH_STATS_TOOLTIP_RETURN_EXTENSION);
					}
				}
				/* Add information about the travel time if known. */
				const auto time = link.time ? back_time ? ((link.time + back_time) / 2) : link.time : back_time;
				if (time > 0) {
					SetDParam(0, time);
					tooltip_extension += GetString(STR_LINKGRAPH_STATS_TOOLTIP_TIME_EXTENSION);
				}
				SetDParam(0, link.cargo);
				SetDParam(1, link.Usage());
				SetDParam(2, i->first);
				SetDParam(3, j->first);
				SetDParam(4, link.Usage() * 100 / (link.capacity + 1));
				SetDParamStr(5, tooltip_extension);
				GuiShowTooltips(this->window, STR_LINKGRAPH_STATS_TOOLTIP, close_cond, 7);
				return true;
			}
		}
	}
	GuiShowTooltips(this->window, STR_NULL, close_cond);
	return false;
}

/**
 * Determine the middle of a station in the current window.
 * @param st The station we're looking for.
 * @return Middle point of the station in the current window.
 */
Point LinkGraphOverlay::GetStationMiddle(const Station *st) const
{
	if (this->window->viewport != nullptr) {
		return GetViewportStationMiddle(this->window->viewport, st);
	} else {
		/* assume this is a smallmap */
		return GetSmallMapStationMiddle(this->window, st);
	}
}

/**
 * Set a new cargo mask and rebuild the cache.
 * @param cargo_mask New cargo mask.
 */
void LinkGraphOverlay::SetCargoMask(CargoTypes cargo_mask)
{
	this->cargo_mask = cargo_mask;
	this->RebuildCache();
	this->window->GetWidget<NWidgetBase>(this->widget_id)->SetDirty(this->window);
}

/**
 * Set a new company mask and rebuild the cache.
 * @param company_mask New company mask.
 */
void LinkGraphOverlay::SetCompanyMask(CompanyMask company_mask)
{
	this->company_mask = company_mask;
	this->RebuildCache();
	this->window->GetWidget<NWidgetBase>(this->widget_id)->SetDirty(this->window);
}

/** Make a number of rows with buttons for each company for the linkgraph legend window. */
std::unique_ptr<NWidgetBase> MakeCompanyButtonRowsLinkGraphGUI()
{
	return MakeCompanyButtonRows(WID_LGL_COMPANY_FIRST, WID_LGL_COMPANY_LAST, COLOUR_GREY, 3, STR_NULL);
}

std::unique_ptr<NWidgetBase> MakeSaturationLegendLinkGraphGUI()
{
	auto panel = std::make_unique<NWidgetVertical>(NC_EQUALSIZE);
	for (uint i = 0; i < lengthof(LinkGraphOverlay::LINK_COLOURS[0]); ++i) {
		auto wid = std::make_unique<NWidgetBackground>(WWT_PANEL, COLOUR_DARK_GREEN, i + WID_LGL_SATURATION_FIRST);
		wid->SetMinimalSize(50, 0);
		wid->SetMinimalTextLines(1, 0, FS_SMALL);
		wid->SetFill(1, 1);
		wid->SetResize(0, 0);
		panel->Add(std::move(wid));
	}
	return panel;
}

std::unique_ptr<NWidgetBase> MakeCargoesLegendLinkGraphGUI()
{
	uint num_cargo = static_cast<uint>(_sorted_cargo_specs.size());
	static const uint ENTRIES_PER_COL = 5;
	auto panel = std::make_unique<NWidgetHorizontal>(NC_EQUALSIZE);
	std::unique_ptr<NWidgetVertical> col = nullptr;

	for (uint i = 0; i < num_cargo; ++i) {
		if (i % ENTRIES_PER_COL == 0) {
			if (col != nullptr) panel->Add(std::move(col));
			col = std::make_unique<NWidgetVertical>(NC_EQUALSIZE);
		}
		auto wid = std::make_unique<NWidgetBackground>(WWT_PANEL, COLOUR_GREY, i + WID_LGL_CARGO_FIRST);
		wid->SetMinimalSize(25, 0);
		wid->SetMinimalTextLines(1, 0, FS_SMALL);
		wid->SetFill(1, 1);
		wid->SetResize(0, 0);
		col->Add(std::move(wid));
	}
	/* Fill up last row */
	for (uint i = num_cargo; i < Ceil(num_cargo, ENTRIES_PER_COL); ++i) {
		auto spc = std::make_unique<NWidgetSpacer>(25, 0);
		spc->SetMinimalTextLines(1, 0, FS_SMALL);
		spc->SetFill(1, 1);
		spc->SetResize(0, 0);
		col->Add(std::move(spc));
	}
	/* If there are no cargo specs defined, then col won't have been created so don't add it. */
	if (col != nullptr) panel->Add(std::move(col));
	return panel;
}


static const NWidgetPart _nested_linkgraph_legend_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_LGL_CAPTION), SetDataTip(STR_LINKGRAPH_LEGEND_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_HORIZONTAL), SetPadding(WidgetDimensions::unscaled.framerect), SetPIP(0, WidgetDimensions::unscaled.framerect.Horizontal(), 0),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_LGL_SATURATION),
				NWidgetFunction(MakeSaturationLegendLinkGraphGUI),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_LGL_COMPANIES),
				NWidget(NWID_VERTICAL, NC_EQUALSIZE),
					NWidgetFunction(MakeCompanyButtonRowsLinkGraphGUI),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_LGL_COMPANIES_ALL), SetDataTip(STR_LINKGRAPH_LEGEND_ALL, STR_NULL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_LGL_COMPANIES_NONE), SetDataTip(STR_LINKGRAPH_LEGEND_NONE, STR_NULL),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_LGL_CARGOES),
				NWidget(NWID_VERTICAL, NC_EQUALSIZE),
					NWidgetFunction(MakeCargoesLegendLinkGraphGUI),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_LGL_CARGOES_ALL), SetDataTip(STR_LINKGRAPH_LEGEND_ALL, STR_NULL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_LGL_CARGOES_NONE), SetDataTip(STR_LINKGRAPH_LEGEND_NONE, STR_NULL),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer()
};

static_assert(WID_LGL_SATURATION_LAST - WID_LGL_SATURATION_FIRST ==
		lengthof(LinkGraphOverlay::LINK_COLOURS[0]) - 1);

static WindowDesc _linkgraph_legend_desc(__FILE__, __LINE__,
	WDP_AUTO, "toolbar_linkgraph", 0, 0,
	WC_LINKGRAPH_LEGEND, WC_NONE,
	0,
	std::begin(_nested_linkgraph_legend_widgets), std::end(_nested_linkgraph_legend_widgets)
);

/**
 * Open a link graph legend window.
 */
void ShowLinkGraphLegend()
{
	AllocateWindowDescFront<LinkGraphLegendWindow>(&_linkgraph_legend_desc, 0);
}

LinkGraphLegendWindow::LinkGraphLegendWindow(WindowDesc *desc, int window_number) : Window(desc)
{
	this->num_cargo = _sorted_cargo_specs.size();

	this->InitNested(window_number);
	this->InvalidateData(0);
	this->SetOverlay(GetMainWindow()->viewport->overlay);
}

/**
 * Set the overlay belonging to this menu and import its company/cargo settings.
 * @param overlay New overlay for this menu.
 */
void LinkGraphLegendWindow::SetOverlay(std::shared_ptr<LinkGraphOverlay> overlay)
{
	this->overlay = overlay;
	CompanyMask companies = this->overlay->GetCompanyMask();
	for (uint c = 0; c < MAX_COMPANIES; c++) {
		if (!this->IsWidgetDisabled(WID_LGL_COMPANY_FIRST + c)) {
			this->SetWidgetLoweredState(WID_LGL_COMPANY_FIRST + c, HasBit(companies, c));
		}
	}
	CargoTypes cargoes = this->overlay->GetCargoMask();
	for (uint c = 0; c < this->num_cargo; c++) {
		this->SetWidgetLoweredState(WID_LGL_CARGO_FIRST + c, HasBit(cargoes, _sorted_cargo_specs[c]->Index()));
	}
}

void LinkGraphLegendWindow::UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize)
{
	if (IsInsideMM(widget, WID_LGL_SATURATION_FIRST, WID_LGL_SATURATION_LAST + 1)) {
		StringID str = STR_NULL;
		if (widget == WID_LGL_SATURATION_FIRST) {
			str = STR_LINKGRAPH_LEGEND_UNUSED;
		} else if (widget == WID_LGL_SATURATION_LAST) {
			str = STR_LINKGRAPH_LEGEND_OVERLOADED;
		} else if (widget == (WID_LGL_SATURATION_LAST + WID_LGL_SATURATION_FIRST) / 2) {
			str = STR_LINKGRAPH_LEGEND_SATURATED;
		}
		if (str != STR_NULL) {
			Dimension dim = GetStringBoundingBox(str, FS_SMALL);
			dim.width += padding.width;
			dim.height += padding.height;
			*size = maxdim(*size, dim);
		}
	}
	if (IsInsideMM(widget, WID_LGL_CARGO_FIRST, WID_LGL_CARGO_LAST + 1)) {
		const CargoSpec *cargo = _sorted_cargo_specs[widget - WID_LGL_CARGO_FIRST];
		Dimension dim = GetStringBoundingBox(cargo->abbrev, FS_SMALL);
		dim.width += padding.width;
		dim.height += padding.height;
		*size = maxdim(*size, dim);
	}
}

void LinkGraphLegendWindow::DrawWidget(const Rect &r, WidgetID widget) const
{
	Rect br = r.Shrink(WidgetDimensions::scaled.bevel);
	if (IsInsideMM(widget, WID_LGL_COMPANY_FIRST, WID_LGL_COMPANY_LAST + 1)) {
		if (this->IsWidgetDisabled(widget)) return;
		CompanyID cid = (CompanyID)(widget - WID_LGL_COMPANY_FIRST);
		Dimension sprite_size = GetSpriteSize(SPR_COMPANY_ICON);
		DrawCompanyIcon(cid, CenterBounds(br.left, br.right, sprite_size.width), CenterBounds(br.top, br.bottom, sprite_size.height));
	}
	if (IsInsideMM(widget, WID_LGL_SATURATION_FIRST, WID_LGL_SATURATION_LAST + 1)) {
		uint8_t colour = LinkGraphOverlay::LINK_COLOURS[_settings_client.gui.linkgraph_colours][widget - WID_LGL_SATURATION_FIRST];
		GfxFillRect(br, colour);
		StringID str = STR_NULL;
		if (widget == WID_LGL_SATURATION_FIRST) {
			str = STR_LINKGRAPH_LEGEND_UNUSED;
		} else if (widget == WID_LGL_SATURATION_LAST) {
			str = STR_LINKGRAPH_LEGEND_OVERLOADED;
		} else if (widget == (WID_LGL_SATURATION_LAST + WID_LGL_SATURATION_FIRST) / 2) {
			str = STR_LINKGRAPH_LEGEND_SATURATED;
		}
		if (str != STR_NULL) {
			DrawString(br.left, br.right, CenterBounds(br.top, br.bottom, GetCharacterHeight(FS_SMALL)), str, GetContrastColour(colour) | TC_FORCED, SA_HOR_CENTER, false, FS_SMALL);
		}
	}
	if (IsInsideMM(widget, WID_LGL_CARGO_FIRST, WID_LGL_CARGO_LAST + 1)) {
		const CargoSpec *cargo = _sorted_cargo_specs[widget - WID_LGL_CARGO_FIRST];
		GfxFillRect(br, cargo->legend_colour);
		DrawString(br.left, br.right, CenterBounds(br.top, br.bottom, GetCharacterHeight(FS_SMALL)), cargo->abbrev, GetContrastColour(cargo->legend_colour, 73), SA_HOR_CENTER, false, FS_SMALL);
	}
}

bool LinkGraphLegendWindow::OnTooltip([[maybe_unused]] Point, WidgetID widget, TooltipCloseCondition close_cond)
{
	if (IsInsideMM(widget, WID_LGL_COMPANY_FIRST, WID_LGL_COMPANY_LAST + 1)) {
		if (this->IsWidgetDisabled(widget)) {
			GuiShowTooltips(this, STR_LINKGRAPH_LEGEND_SELECT_COMPANIES, close_cond);
		} else {
			SetDParam(0, STR_LINKGRAPH_LEGEND_SELECT_COMPANIES);
			SetDParam(1, (CompanyID)(widget - WID_LGL_COMPANY_FIRST));
			GuiShowTooltips(this, STR_LINKGRAPH_LEGEND_COMPANY_TOOLTIP, close_cond, 2);
		}
		return true;
	}
	if (IsInsideMM(widget, WID_LGL_CARGO_FIRST, WID_LGL_CARGO_LAST + 1)) {
		const CargoSpec *cargo = _sorted_cargo_specs[widget - WID_LGL_CARGO_FIRST];
		GuiShowTooltips(this, cargo->name, close_cond);
		return true;
	}
	return false;
}

/**
 * Update the overlay with the new company selection.
 */
void LinkGraphLegendWindow::UpdateOverlayCompanies()
{
	uint32_t mask = 0;
	for (uint c = 0; c < MAX_COMPANIES; c++) {
		if (this->IsWidgetDisabled(c + WID_LGL_COMPANY_FIRST)) continue;
		if (!this->IsWidgetLowered(c + WID_LGL_COMPANY_FIRST)) continue;
		SetBit(mask, c);
	}
	this->overlay->SetCompanyMask(mask);
}

/**
 * Update the overlay with the new cargo selection.
 */
void LinkGraphLegendWindow::UpdateOverlayCargoes()
{
	CargoTypes mask = 0;
	for (uint c = 0; c < num_cargo; c++) {
		if (!this->IsWidgetLowered(c + WID_LGL_CARGO_FIRST)) continue;
		SetBit(mask, _sorted_cargo_specs[c]->Index());
	}
	this->overlay->SetCargoMask(mask);
}

void LinkGraphLegendWindow::OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count)
{
	/* Check which button is clicked */
	if (IsInsideMM(widget, WID_LGL_COMPANY_FIRST, WID_LGL_COMPANY_LAST + 1)) {
		if (!this->IsWidgetDisabled(widget)) {
			this->ToggleWidgetLoweredState(widget);
			this->UpdateOverlayCompanies();
		}
	} else if (widget == WID_LGL_COMPANIES_ALL || widget == WID_LGL_COMPANIES_NONE) {
		for (uint c = 0; c < MAX_COMPANIES; c++) {
			if (this->IsWidgetDisabled(c + WID_LGL_COMPANY_FIRST)) continue;
			this->SetWidgetLoweredState(WID_LGL_COMPANY_FIRST + c, widget == WID_LGL_COMPANIES_ALL);
		}
		this->UpdateOverlayCompanies();
		this->SetDirty();
	} else if (IsInsideMM(widget, WID_LGL_CARGO_FIRST, WID_LGL_CARGO_LAST + 1)) {
		this->ToggleWidgetLoweredState(widget);
		this->UpdateOverlayCargoes();
	} else if (widget == WID_LGL_CARGOES_ALL || widget == WID_LGL_CARGOES_NONE) {
		for (uint c = 0; c < this->num_cargo; c++) {
			this->SetWidgetLoweredState(WID_LGL_CARGO_FIRST + c, widget == WID_LGL_CARGOES_ALL);
		}
		this->UpdateOverlayCargoes();
	}
	this->SetDirty();
}

/**
 * Invalidate the data of this window if the cargoes or companies have changed.
 * @param data ignored
 * @param gui_scope ignored
 */
void LinkGraphLegendWindow::OnInvalidateData([[maybe_unused]] int data, [[maybe_unused]] bool gui_scope)
{
	if (this->num_cargo != _sorted_cargo_specs.size()) {
		this->Close();
		return;
	}

	/* Disable the companies who are not active */
	for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
		this->SetWidgetDisabledState(i + WID_LGL_COMPANY_FIRST, !Company::IsValidID(i));
	}
}
