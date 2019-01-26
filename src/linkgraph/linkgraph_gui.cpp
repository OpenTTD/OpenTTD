/* $Id$ */

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
#include "../date_func.h"
#include "../viewport_func.h"
#include "../smallmap_gui.h"
#include "../core/geometry_func.hpp"
#include "../core/math_func.hpp"
#include "../widgets/link_graph_legend_widget.h"

#include "table/strings.h"

#include "../safeguards.h"

#include <set>
#include <utility>
#include <vector>

/**
 * Colours for the various "load" states of links. Ordered from "unused" to
 * "overloaded".
 */
const uint8 LinkGraphOverlay::LINK_COLOURS[] = {
	0x0f, 0xd1, 0xd0, 0x57,
	0x55, 0x53, 0xbf, 0xbd,
	0xba, 0xb9, 0xb7, 0xb5
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
 * Rebuild the cache of station and link locations for the current company_mask and cargo_mask.
 */
void LinkGraphOverlay::RebuildCache()
{
	this->cached_data.Clear();

	this->ResetBoundaries();

	if (this->company_mask == 0) return;

	const Station *sta;
	FOR_ALL_STATIONS(sta) {
		if (sta->rect.IsEmpty()) continue;
		const Point pta = GetRemappedStationMiddle(sta);
		this->cached_data_boundary.left = min(this->cached_data_boundary.left, pta.x);
		this->cached_data_boundary.right = max(this->cached_data_boundary.right, pta.x);
		this->cached_data_boundary.top = min(this->cached_data_boundary.top, pta.y);
		this->cached_data_boundary.bottom = max(this->cached_data_boundary.bottom, pta.y);
	}

	if (!this->HasNonEmptyBoundaries()) return;
	assert(this->cached_data_boundary.top <= this->cached_data_boundary.bottom);

	const int link_padding = (this->scale * 3 + 1) / 2; // multiplies scale by 1.5 (rounded up), this is the maximum padding required for a link
	const int max_station_padding = this->scale * 2 + 2; // multiplies scale by 2, this is the maximum padding required for a station

	const int padding = max(link_padding, max_station_padding);
	this->cached_data_boundary.left -= padding;
	this->cached_data_boundary.right += padding;
	this->cached_data_boundary.top -= padding;
	this->cached_data_boundary.bottom += padding;

	const uint8 required_tree_height = FindLastBit(((this->cached_data_boundary.bottom - this->cached_data_boundary.top) >> LINK_TREE_UNIT_SHIFT)) + 1;
	const uint8 required_tree_width = FindLastBit(((this->cached_data_boundary.right - this->cached_data_boundary.left) >> LINK_TREE_UNIT_SHIFT)) + 1;
	this->cached_data.Resize(required_tree_height, required_tree_width);

	DrawPixelInfo dpi;
	this->GetWidgetDpi(&dpi);

	FOR_ALL_STATIONS(sta) {
		if (sta->rect.IsEmpty()) continue;

		Point pta = GetRemappedStationMiddle(sta);

		StationID from = sta->index;
		std::set<std::pair<StationID, StationID>> seen_links;

		uint supply = 0;
		CargoID c;
		FOR_EACH_SET_CARGO_ID(c, this->cargo_mask) {
			if (!CargoSpec::Get(c)->IsValid()) continue;
			if (!LinkGraph::IsValidID(sta->goods[c].link_graph)) continue;
			const LinkGraph &lg = *LinkGraph::Get(sta->goods[c].link_graph);

			ConstNode from_node = lg[sta->goods[c].node];
			supply += lg.Monthly(from_node.Supply());
			for (ConstEdgeIterator i = from_node.Begin(); i != from_node.End(); ++i) {
				StationID to = lg[i->first].Station();
				assert(from != to);

				if (!Station::IsValidID(to)) continue;

				if (!seen_links.emplace(from, to).second) continue;

				const Station *stb = Station::Get(to);
				assert(sta != stb);

				Point ptb = GetRemappedStationMiddle(stb);

				/* Show links between stations of selected companies or "neutral" ones like oilrigs. */
				if (stb->owner != OWNER_NONE && sta->owner != OWNER_NONE && !HasBit(this->company_mask, stb->owner)) continue;
				if (stb->rect.IsEmpty()) continue;

				this->AddLinks(sta, stb, pta, ptb, link_padding);
			}
		}

		this->CreateStationInCache(from, pta, (this->CalculateStationDotSizeFromSupply(supply) + 1) / 2) = supply;
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
	return !((pta.x < dpi->left - padding && ptb.x < dpi->left - padding) ||
			(pta.y < dpi->top - padding && ptb.y < dpi->top - padding) ||
			(pta.x > dpi->left + dpi->width + padding &&
					ptb.x > dpi->left + dpi->width + padding) ||
			(pta.y > dpi->top + dpi->height + padding &&
					ptb.y > dpi->top + dpi->height + padding));
}


LinkProperties & LinkGraphOverlay::CreateLinkInCache(StationID from, StationID to, Point from_location, Point to_location, int padding)
{
	const Rect &tree_rect = this->RemappedToTreeCoords(Rect{
		min(from_location.x, to_location.x) - padding,
		min(from_location.y, to_location.y) - padding,
		max(from_location.x, to_location.x) + padding,
		max(from_location.y, to_location.y) + padding });

	LinkGraphOverlay::LinkMapItem &link_map_item = this->cached_data.Emplace(tree_rect.left, tree_rect.top, tree_rect.right, tree_rect.bottom);

	link_map_item.is_station = false;
	link_map_item.link.from = from;
	link_map_item.link.to = to;
	return link_map_item.link.link_properties = LinkProperties();
}

uint & LinkGraphOverlay::CreateStationInCache(StationID id, Point location, int padding)
{
	const Rect &tree_rect = this->RemappedToTreeCoords(Rect{
		location.x - padding,
		location.y - padding,
		location.x + padding,
		location.y + padding });

	LinkGraphOverlay::LinkMapItem &link_map_item = this->cached_data.Emplace(tree_rect.left, tree_rect.top, tree_rect.right, tree_rect.bottom);

	link_map_item.is_station = true;
	link_map_item.station.id = id;
	return link_map_item.station.supply = 0;
}

/**
 * Add all "interesting" links between the given stations to the cache.
 * @param from The source station.
 * @param to The destination station.
 */
void LinkGraphOverlay::AddLinks(const Station *from, const Station *to, Point from_location, Point to_location, int padding)
{
	CargoID c;
	FOR_EACH_SET_CARGO_ID(c, this->cargo_mask) {
		if (!CargoSpec::Get(c)->IsValid()) continue;
		const GoodsEntry &ge = from->goods[c];
		if (!LinkGraph::IsValidID(ge.link_graph) ||
				ge.link_graph != to->goods[c].link_graph) {
			continue;
		}
		const LinkGraph &lg = *LinkGraph::Get(ge.link_graph);
		ConstEdge edge = lg[ge.node][to->goods[c].node];
		if (edge.Capacity() > 0) {
			LinkProperties &link_properties = this->CreateLinkInCache(from->index, to->index, from_location, to_location, padding);
			this->AddStats(lg.Monthly(edge.Capacity()), lg.Monthly(edge.Usage()),
					ge.flows.GetFlowVia(to->index), from->owner == OWNER_NONE || to->owner == OWNER_NONE, link_properties);
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
/* static */ void LinkGraphOverlay::AddStats(uint new_cap, uint new_usg, uint new_plan, bool new_shared, LinkProperties &cargo)
{
	/* multiply the numbers by 32 in order to avoid comparing to 0 too often. */
	if (cargo.capacity == 0 ||
			max(cargo.usage, cargo.planned) * 32 / (cargo.capacity + 1) < max(new_usg, new_plan) * 32 / (new_cap + 1)) {
		cargo.capacity = new_cap;
		cargo.usage = new_usg;
		cargo.planned = new_plan;
	}
	if (new_shared) cargo.shared = true;
}

/**
 * Draw the linkgraph overlay or some part of it, in the area given.
 * @param dpi Area to be drawn to.
 */
void LinkGraphOverlay::Draw(const DrawPixelInfo *dpi) const
{
	if (!HasNonEmptyBoundaries()) return;

	const Point topleft{ dpi->left, dpi->top };
	const Point bottomright{ dpi->left + dpi->width - 1, dpi->top + dpi->height - 1 };
	const Point remapped_topleft = RemappedFromViewportCoords(this->window->viewport, topleft);
	const Point remapped_bottomright = RemappedFromViewportCoords(this->window->viewport, bottomright);

	const Rect &tree_rect = RemappedToTreeCoords(Rect{ remapped_topleft.x, remapped_topleft.y, remapped_bottomright.x, remapped_bottomright.y });

	/* Temporary keep the stations that needs to be drawn.  This allows us to draw stations on top of links. */
	struct StationSizeInfo {
		const Station* st;
		Point middle;
		uint size;
		StationSizeInfo(const Station* st, Point middle, uint size) : st(st), middle(middle), size(size) {}
	};
	std::vector<StationSizeInfo> station_sizes;

	this->cached_data.Query(tree_rect.left, tree_rect.top, tree_rect.right, tree_rect.bottom, [&](const LinkMapItem& link_map_item) {
		if (link_map_item.is_station) {
			const Station *st = Station::GetIfValid(link_map_item.station.id);
			if (st != nullptr) {
				const Point pt = this->GetStationMiddle(st);
				if (this->IsPointVisible(pt, dpi, 3 * this->scale)) {
					const uint size = CalculateStationDotSizeFromSupply(link_map_item.station.supply);
					station_sizes.emplace_back(st, pt, size);
				}
			}
		} else {
			if (Station::IsValidID(link_map_item.link.from) && Station::IsValidID(link_map_item.link.to)) {
				const Point pta = this->GetStationMiddle(Station::Get(link_map_item.link.from));
				const Point ptb = this->GetStationMiddle(Station::Get(link_map_item.link.to));
				if (this->IsLinkVisible(pta, ptb, dpi, (this->scale * 3 + 1) / 2)) {
					this->DrawContent(pta, ptb, link_map_item.link.link_properties);
				}
			}
		}
	});
	for (StationSizeInfo &ssi : station_sizes) {
		LinkGraphOverlay::DrawVertex(ssi.middle.x, ssi.middle.y, ssi.size,
			_colour_gradient[ssi.st->owner != OWNER_NONE ?
			(Colours)Company::Get(ssi.st->owner)->colour : COLOUR_GREY][5],
			_colour_gradient[COLOUR_GREY][1]);
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
	uint usage_or_plan = min(cargo.capacity * 2 + 1, max(cargo.usage, cargo.planned));
	int colour = LinkGraphOverlay::LINK_COLOURS[usage_or_plan * lengthof(LinkGraphOverlay::LINK_COLOURS) / (cargo.capacity * 2 + 2)];
	int dash = cargo.shared ? this->scale * 4 : 0;

	/* Move line a bit 90° against its dominant direction to prevent it from
	 * being hidden below the grey line. */
	int side = _settings_game.vehicle.road_side ? 1 : -1;
	if (abs(pta.x - ptb.x) < abs(pta.y - ptb.y)) {
		int offset_x = (pta.y > ptb.y ? 1 : -1) * side * this->scale;
		GfxDrawLine(pta.x + offset_x, pta.y, ptb.x + offset_x, ptb.y, colour, this->scale, dash);
	} else {
		int offset_y = (pta.x < ptb.x ? 1 : -1) * side * this->scale;
		GfxDrawLine(pta.x, pta.y + offset_y, ptb.x, ptb.y + offset_y, colour, this->scale, dash);
	}

	GfxDrawLine(pta.x, pta.y, ptb.x, ptb.y, _colour_gradient[COLOUR_GREY][1], this->scale);
}

uint LinkGraphOverlay::CalculateStationDotSizeFromSupply(uint supply) const
{
	return this->scale * 2 + this->scale * 2 * min(200, supply) / 200;
}

Rect LinkGraphOverlay::RemappedToTreeCoords(const Rect &rect) const
{
	Rect answer;
	answer.left = (Clamp(rect.left, this->cached_data_boundary.left, this->cached_data_boundary.right) - this->cached_data_boundary.left) >> LINK_TREE_UNIT_SHIFT;
	answer.right = (Clamp(rect.right, this->cached_data_boundary.left, this->cached_data_boundary.right) - this->cached_data_boundary.left) >> LINK_TREE_UNIT_SHIFT;
	answer.top = (Clamp(rect.top, this->cached_data_boundary.top, this->cached_data_boundary.bottom) - this->cached_data_boundary.top) >> LINK_TREE_UNIT_SHIFT;
	answer.bottom = (Clamp(rect.bottom, this->cached_data_boundary.top, this->cached_data_boundary.bottom) - this->cached_data_boundary.top) >> LINK_TREE_UNIT_SHIFT;
	return answer;
}

bool LinkGraphOverlay::HasNonEmptyBoundaries() const
{
	return this->cached_data_boundary.left <= this->cached_data_boundary.right;
}

void LinkGraphOverlay::ResetBoundaries()
{
	this->cached_data_boundary.left = INT_MAX;
	this->cached_data_boundary.right = INT_MIN;
	this->cached_data_boundary.top = INT_MAX;
	this->cached_data_boundary.bottom = INT_MIN;
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

/**
 * Determine the middle of a station in the current window.
 * @param st The station we're looking for.
 * @return Middle point of the station in the current window.
 */
Point LinkGraphOverlay::GetStationMiddle(const Station *st) const
{
	if (this->window->viewport != NULL) {
		return GetViewportStationMiddle(this->window->viewport, st);
	} else {
		/* assume this is a smallmap */
		return static_cast<const SmallMapWindow *>(this->window)->GetStationMiddle(st);
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
void LinkGraphOverlay::SetCompanyMask(uint32 company_mask)
{
	this->company_mask = company_mask;
	this->RebuildCache();
	this->window->GetWidget<NWidgetBase>(this->widget_id)->SetDirty(this->window);
}

/** Make a number of rows with buttons for each company for the linkgraph legend window. */
NWidgetBase *MakeCompanyButtonRowsLinkGraphGUI(int *biggest_index)
{
	return MakeCompanyButtonRows(biggest_index, WID_LGL_COMPANY_FIRST, WID_LGL_COMPANY_LAST, 3, STR_NULL);
}

NWidgetBase *MakeSaturationLegendLinkGraphGUI(int *biggest_index)
{
	NWidgetVertical *panel = new NWidgetVertical(NC_EQUALSIZE);
	for (uint i = 0; i < lengthof(LinkGraphOverlay::LINK_COLOURS); ++i) {
		NWidgetBackground * wid = new NWidgetBackground(WWT_PANEL, COLOUR_DARK_GREEN, i + WID_LGL_SATURATION_FIRST);
		wid->SetMinimalSize(50, FONT_HEIGHT_SMALL);
		wid->SetFill(1, 1);
		wid->SetResize(0, 0);
		panel->Add(wid);
	}
	*biggest_index = WID_LGL_SATURATION_LAST;
	return panel;
}

NWidgetBase *MakeCargoesLegendLinkGraphGUI(int *biggest_index)
{
	static const uint ENTRIES_PER_ROW = CeilDiv(NUM_CARGO, 5);
	NWidgetVertical *panel = new NWidgetVertical(NC_EQUALSIZE);
	NWidgetHorizontal *row = NULL;
	for (uint i = 0; i < NUM_CARGO; ++i) {
		if (i % ENTRIES_PER_ROW == 0) {
			if (row) panel->Add(row);
			row = new NWidgetHorizontal(NC_EQUALSIZE);
		}
		NWidgetBackground * wid = new NWidgetBackground(WWT_PANEL, COLOUR_GREY, i + WID_LGL_CARGO_FIRST);
		wid->SetMinimalSize(25, FONT_HEIGHT_SMALL);
		wid->SetFill(1, 1);
		wid->SetResize(0, 0);
		row->Add(wid);
	}
	/* Fill up last row */
	for (uint i = 0; i < 4 - (NUM_CARGO - 1) % 5; ++i) {
		NWidgetSpacer *spc = new NWidgetSpacer(25, FONT_HEIGHT_SMALL);
		spc->SetFill(1, 1);
		spc->SetResize(0, 0);
		row->Add(spc);
	}
	panel->Add(row);
	*biggest_index = WID_LGL_CARGO_LAST;
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
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_LGL_SATURATION),
				SetPadding(WD_FRAMERECT_TOP, 0, WD_FRAMERECT_BOTTOM, WD_CAPTIONTEXT_LEFT),
				NWidgetFunction(MakeSaturationLegendLinkGraphGUI),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_LGL_COMPANIES),
				SetPadding(WD_FRAMERECT_TOP, 0, WD_FRAMERECT_BOTTOM, WD_CAPTIONTEXT_LEFT),
				NWidget(NWID_VERTICAL, NC_EQUALSIZE),
					NWidgetFunction(MakeCompanyButtonRowsLinkGraphGUI),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_LGL_COMPANIES_ALL), SetDataTip(STR_LINKGRAPH_LEGEND_ALL, STR_NULL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_LGL_COMPANIES_NONE), SetDataTip(STR_LINKGRAPH_LEGEND_NONE, STR_NULL),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_LGL_CARGOES),
				SetPadding(WD_FRAMERECT_TOP, WD_FRAMERECT_RIGHT, WD_FRAMERECT_BOTTOM, WD_CAPTIONTEXT_LEFT),
				NWidget(NWID_VERTICAL, NC_EQUALSIZE),
					NWidgetFunction(MakeCargoesLegendLinkGraphGUI),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_LGL_CARGOES_ALL), SetDataTip(STR_LINKGRAPH_LEGEND_ALL, STR_NULL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_LGL_CARGOES_NONE), SetDataTip(STR_LINKGRAPH_LEGEND_NONE, STR_NULL),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer()
};

assert_compile(WID_LGL_SATURATION_LAST - WID_LGL_SATURATION_FIRST ==
		lengthof(LinkGraphOverlay::LINK_COLOURS) - 1);

static WindowDesc _linkgraph_legend_desc(
	WDP_AUTO, "toolbar_linkgraph", 0, 0,
	WC_LINKGRAPH_LEGEND, WC_NONE,
	0,
	_nested_linkgraph_legend_widgets, lengthof(_nested_linkgraph_legend_widgets)
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
	this->InitNested(window_number);
	this->InvalidateData(0);
	this->SetOverlay(FindWindowById(WC_MAIN_WINDOW, 0)->viewport->overlay);
}

/**
 * Set the overlay belonging to this menu and import its company/cargo settings.
 * @param overlay New overlay for this menu.
 */
void LinkGraphLegendWindow::SetOverlay(LinkGraphOverlay *overlay) {
	this->overlay = overlay;
	uint32 companies = this->overlay->GetCompanyMask();
	for (uint c = 0; c < MAX_COMPANIES; c++) {
		if (!this->IsWidgetDisabled(WID_LGL_COMPANY_FIRST + c)) {
			this->SetWidgetLoweredState(WID_LGL_COMPANY_FIRST + c, HasBit(companies, c));
		}
	}
	CargoTypes cargoes = this->overlay->GetCargoMask();
	for (uint c = 0; c < NUM_CARGO; c++) {
		if (!this->IsWidgetDisabled(WID_LGL_CARGO_FIRST + c)) {
			this->SetWidgetLoweredState(WID_LGL_CARGO_FIRST + c, HasBit(cargoes, c));
		}
	}
}

void LinkGraphLegendWindow::UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
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
			Dimension dim = GetStringBoundingBox(str);
			dim.width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
			dim.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
			*size = maxdim(*size, dim);
		}
	}
	if (IsInsideMM(widget, WID_LGL_CARGO_FIRST, WID_LGL_CARGO_LAST + 1)) {
		CargoSpec *cargo = CargoSpec::Get(widget - WID_LGL_CARGO_FIRST);
		if (cargo->IsValid()) {
			Dimension dim = GetStringBoundingBox(cargo->abbrev);
			dim.width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
			dim.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
			*size = maxdim(*size, dim);
		}
	}
}

void LinkGraphLegendWindow::DrawWidget(const Rect &r, int widget) const
{
	if (IsInsideMM(widget, WID_LGL_COMPANY_FIRST, WID_LGL_COMPANY_LAST + 1)) {
		if (this->IsWidgetDisabled(widget)) return;
		CompanyID cid = (CompanyID)(widget - WID_LGL_COMPANY_FIRST);
		Dimension sprite_size = GetSpriteSize(SPR_COMPANY_ICON);
		DrawCompanyIcon(cid, (r.left + r.right + 1 - sprite_size.width) / 2, (r.top + r.bottom + 1 - sprite_size.height) / 2);
	}
	if (IsInsideMM(widget, WID_LGL_SATURATION_FIRST, WID_LGL_SATURATION_LAST + 1)) {
		GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, LinkGraphOverlay::LINK_COLOURS[widget - WID_LGL_SATURATION_FIRST]);
		StringID str = STR_NULL;
		if (widget == WID_LGL_SATURATION_FIRST) {
			str = STR_LINKGRAPH_LEGEND_UNUSED;
		} else if (widget == WID_LGL_SATURATION_LAST) {
			str = STR_LINKGRAPH_LEGEND_OVERLOADED;
		} else if (widget == (WID_LGL_SATURATION_LAST + WID_LGL_SATURATION_FIRST) / 2) {
			str = STR_LINKGRAPH_LEGEND_SATURATED;
		}
		if (str != STR_NULL) DrawString(r.left, r.right, (r.top + r.bottom + 1 - FONT_HEIGHT_SMALL) / 2, str, TC_FROMSTRING, SA_HOR_CENTER);
	}
	if (IsInsideMM(widget, WID_LGL_CARGO_FIRST, WID_LGL_CARGO_LAST + 1)) {
		if (this->IsWidgetDisabled(widget)) return;
		CargoSpec *cargo = CargoSpec::Get(widget - WID_LGL_CARGO_FIRST);
		GfxFillRect(r.left + 2, r.top + 2, r.right - 2, r.bottom - 2, cargo->legend_colour);
		DrawString(r.left, r.right, (r.top + r.bottom + 1 - FONT_HEIGHT_SMALL) / 2, cargo->abbrev, GetContrastColour(cargo->legend_colour, 73), SA_HOR_CENTER);
	}
}

bool LinkGraphLegendWindow::OnHoverCommon(Point pt, int widget, TooltipCloseCondition close_cond)
{
	if (IsInsideMM(widget, WID_LGL_COMPANY_FIRST, WID_LGL_COMPANY_LAST + 1)) {
		if (this->IsWidgetDisabled(widget)) {
			GuiShowTooltips(this, STR_LINKGRAPH_LEGEND_SELECT_COMPANIES, 0, NULL, close_cond);
		} else {
			uint64 params[2];
			CompanyID cid = (CompanyID)(widget - WID_LGL_COMPANY_FIRST);
			params[0] = STR_LINKGRAPH_LEGEND_SELECT_COMPANIES;
			params[1] = cid;
			GuiShowTooltips(this, STR_LINKGRAPH_LEGEND_COMPANY_TOOLTIP, 2, params, close_cond);
		}
		return true;
	}
	if (IsInsideMM(widget, WID_LGL_CARGO_FIRST, WID_LGL_CARGO_LAST + 1)) {
		if (this->IsWidgetDisabled(widget)) return false;
		CargoSpec *cargo = CargoSpec::Get(widget - WID_LGL_CARGO_FIRST);
		uint64 params[1];
		params[0] = cargo->name;
		GuiShowTooltips(this, STR_BLACK_STRING, 1, params, close_cond);
		return true;
	}
	return false;
}

void LinkGraphLegendWindow::OnHover(Point pt, int widget)
{
	this->OnHoverCommon(pt, widget, TCC_HOVER);
}

bool LinkGraphLegendWindow::OnRightClick(Point pt, int widget)
{
	if (_settings_client.gui.hover_delay_ms == 0) {
		return this->OnHoverCommon(pt, widget, TCC_RIGHT_CLICK);
	}
	return false;
}

/**
 * Update the overlay with the new company selection.
 */
void LinkGraphLegendWindow::UpdateOverlayCompanies()
{
	uint32 mask = 0;
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
	for (uint c = 0; c < NUM_CARGO; c++) {
		if (this->IsWidgetDisabled(c + WID_LGL_CARGO_FIRST)) continue;
		if (!this->IsWidgetLowered(c + WID_LGL_CARGO_FIRST)) continue;
		SetBit(mask, c);
	}
	this->overlay->SetCargoMask(mask);
}

void LinkGraphLegendWindow::OnClick(Point pt, int widget, int click_count)
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
		if (!this->IsWidgetDisabled(widget)) {
			this->ToggleWidgetLoweredState(widget);
			this->UpdateOverlayCargoes();
		}
	} else if (widget == WID_LGL_CARGOES_ALL || widget == WID_LGL_CARGOES_NONE) {
		for (uint c = 0; c < NUM_CARGO; c++) {
			if (this->IsWidgetDisabled(c + WID_LGL_CARGO_FIRST)) continue;
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
void LinkGraphLegendWindow::OnInvalidateData(int data, bool gui_scope)
{
	/* Disable the companies who are not active */
	for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
		this->SetWidgetDisabledState(i + WID_LGL_COMPANY_FIRST, !Company::IsValidID(i));
	}
	for (CargoID i = 0; i < NUM_CARGO; i++) {
		this->SetWidgetDisabledState(i + WID_LGL_CARGO_FIRST, !CargoSpec::Get(i)->IsValid());
	}
}
