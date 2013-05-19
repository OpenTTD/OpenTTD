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
#include "../company_base.h"
#include "../date_func.h"
#include "linkgraph_gui.h"
#include "../viewport_func.h"

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
 * Rebuild the cache and recalculate which links and stations to be shown.
 */
void LinkGraphOverlay::RebuildCache()
{
	this->cached_links.clear();
	this->cached_stations.clear();
	if (this->company_mask == 0) return;

	DrawPixelInfo dpi;
	this->GetWidgetDpi(&dpi);

	const Station *sta;
	FOR_ALL_STATIONS(sta) {
		/* Show links between stations of selected companies or "neutral" ones like oilrigs. */
		if (sta->owner != OWNER_NONE && !HasBit(this->company_mask, sta->owner)) continue;
		if (sta->rect.IsEmpty()) continue;

		Point pta = this->GetStationMiddle(sta);

		StationID from = sta->index;
		StationLinkMap &seen_links = this->cached_links[from];

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
				if (!Station::IsValidID(to) || seen_links.find(to) != seen_links.end()) {
					continue;
				}
				const Station *stb = Station::Get(to);
				assert(sta != stb);
				if (stb->owner != OWNER_NONE && !HasBit(this->company_mask, stb->owner)) continue;
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
	return !((pta.x < dpi->left - padding && ptb.x < dpi->left - padding) ||
			(pta.y < dpi->top - padding && ptb.y < dpi->top - padding) ||
			(pta.x > dpi->left + dpi->width + padding &&
					ptb.x > dpi->left + dpi->width + padding) ||
			(pta.y > dpi->top + dpi->height + padding &&
					ptb.y > dpi->top + dpi->height + padding));
}

/**
 * Add all "interesting" links between the given stations to the cache.
 * @param from The source station.
 * @param to The destination station.
 */
void LinkGraphOverlay::AddLinks(const Station *from, const Station *to)
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
			this->AddStats(lg.Monthly(edge.Capacity()), lg.Monthly(edge.Usage()),
					this->cached_links[from->index][to->index]);
		}
	}
}

/**
 * Add information from a given pair of link stat and flow stat to the given link properties.
 * @param orig_link Link stat to read the information from.
 * @param new_plan Planned flow for the link.
 * @param cargo LinkProperties to write the information to.
 */
/* static */ void LinkGraphOverlay::AddStats(uint new_cap, uint new_usg, LinkProperties &cargo)
{
	/* multiply the numbers by 32 in order to avoid comparing to 0 too often. */
	if (cargo.capacity == 0 ||
			cargo.usage * 32 / (cargo.capacity + 1) < new_usg * 32 / (new_cap + 1)) {
		cargo.capacity = new_cap;
		cargo.usage = new_usg;
	}
}

/**
 * Draw the linkgraph overlay or some part of it, in the area given.
 * @param dpi Area to be drawn to.
 */
void LinkGraphOverlay::Draw(const DrawPixelInfo *dpi) const
{
	this->DrawLinks(dpi);
	this->DrawStationDots(dpi);
}

/**
 * Draw the cached links or part of them into the given area.
 * @param dpi Area to be drawn to.
 */
void LinkGraphOverlay::DrawLinks(const DrawPixelInfo *dpi) const
{
	for (LinkMap::const_iterator i(this->cached_links.begin()); i != this->cached_links.end(); ++i) {
		if (!Station::IsValidID(i->first)) continue;
		Point pta = this->GetStationMiddle(Station::Get(i->first));
		for (StationLinkMap::const_iterator j(i->second.begin()); j != i->second.end(); ++j) {
			if (!Station::IsValidID(j->first)) continue;
			Point ptb = this->GetStationMiddle(Station::Get(j->first));
			if (!this->IsLinkVisible(pta, ptb, dpi, this->scale + 2)) continue;
			this->DrawContent(pta, ptb, j->second);
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
	int offset_y = (pta.x < ptb.x ? 1 : -1) * this->scale;
	int offset_x = (pta.y > ptb.y ? 1 : -1) * this->scale;

	int colour = LinkGraphOverlay::LINK_COLOURS[cargo.usage * lengthof(LinkGraphOverlay::LINK_COLOURS) / (cargo.capacity * 2 + 2)];

	GfxDrawLine(pta.x + offset_x, pta.y, ptb.x + offset_x, ptb.y, colour, scale);
	GfxDrawLine(pta.x, pta.y + offset_y, ptb.x, ptb.y + offset_y, colour, scale);

	GfxDrawLine(pta.x, pta.y, ptb.x, ptb.y, _colour_gradient[COLOUR_GREY][1], this->scale);
}

/**
 * Draw dots for stations into the smallmap. The dots' sizes are determined by the amount of
 * cargo produced there, their colours by the type of cargo produced.
 */
void LinkGraphOverlay::DrawStationDots(const DrawPixelInfo *dpi) const
{
	for (StationSupplyList::const_iterator i(this->cached_stations.begin()); i != this->cached_stations.end(); ++i) {
		const Station *st = Station::GetIfValid(i->first);
		if (st == NULL) continue;
		Point pt = this->GetStationMiddle(st);
		if (!this->IsPointVisible(pt, dpi, 3 * this->scale)) continue;

		uint r = this->scale * 2 + this->scale * 2 * min(200, i->second) / 200;

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

/**
 * Determine the middle of a station in the current window.
 * @param st The station we're looking for.
 * @return Middle point of the station in the current window.
 */
Point LinkGraphOverlay::GetStationMiddle(const Station *st) const {
	Point dummy;
	dummy.x = dummy.y = 0;
	return dummy;
}

/**
 * Set a new cargo mask and rebuild the cache.
 * @param cargo_mask New cargo mask.
 */
void LinkGraphOverlay::SetCargoMask(uint32 cargo_mask)
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
