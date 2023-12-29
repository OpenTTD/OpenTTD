/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file linkgraph_gui.h Declaration of linkgraph overlay GUI. */

#ifndef LINKGRAPH_GUI_H
#define LINKGRAPH_GUI_H

#include "../company_func.h"
#include "../station_base.h"
#include "../widget_type.h"
#include "../window_gui.h"
#include "linkgraph_base.h"

/**
 * Monthly statistics for a link between two stations.
 * Only the cargo type of the most saturated linkgraph is taken into account.
 */
struct LinkProperties {
	LinkProperties() : cargo(CT_INVALID), capacity(0), usage(0), planned(0), shared(false) {}

	/** Return the usage of the link to display. */
	uint Usage() const { return std::max(this->usage, this->planned); }

	CargoID cargo; ///< Cargo type of the link.
	uint capacity; ///< Capacity of the link.
	uint usage;    ///< Actual usage of the link.
	uint planned;  ///< Planned usage of the link.
	uint32_t time;   ///< Travel time of the link.
	bool shared;   ///< If this is a shared link to be drawn dashed.
};

/**
 * Handles drawing of links into some window.
 * The window must either be a smallmap or have a valid viewport.
 */
class LinkGraphOverlay {
public:
	typedef std::map<StationID, LinkProperties> StationLinkMap;
	typedef std::map<StationID, StationLinkMap> LinkMap;
	typedef std::vector<std::pair<StationID, uint> > StationSupplyList;

	static const uint8_t LINK_COLOURS[][12];

	/**
	 * Create a link graph overlay for the specified window.
	 * @param w Window to be drawn into.
	 * @param wid ID of the widget to draw into.
	 * @param cargo_mask Bitmask of cargoes to be shown.
	 * @param company_mask Bitmask of companies to be shown.
	 * @param scale Desired thickness of lines and size of station dots.
	 */
	LinkGraphOverlay(Window *w, WidgetID wid, CargoTypes cargo_mask, CompanyMask company_mask, uint scale) :
			window(w), widget_id(wid), cargo_mask(cargo_mask), company_mask(company_mask), scale(scale)
	{}

	void Draw(const DrawPixelInfo *dpi);
	void SetCargoMask(CargoTypes cargo_mask);
	void SetCompanyMask(CompanyMask company_mask);

	bool ShowTooltip(Point pt, TooltipCloseCondition close_cond);

	/** Mark the linkgraph dirty to be rebuilt next time Draw() is called. */
	void SetDirty() { this->dirty = true; }

	/** Get a bitmask of the currently shown cargoes. */
	CargoTypes GetCargoMask() { return this->cargo_mask; }

	/** Get a bitmask of the currently shown companies. */
	CompanyMask GetCompanyMask() { return this->company_mask; }

protected:
	Window *window;                    ///< Window to be drawn into.
	const WidgetID widget_id;          ///< ID of Widget in Window to be drawn to.
	CargoTypes cargo_mask;             ///< Bitmask of cargos to be displayed.
	CompanyMask company_mask;          ///< Bitmask of companies to be displayed.
	LinkMap cached_links;              ///< Cache for links to reduce recalculation.
	StationSupplyList cached_stations; ///< Cache for stations to be drawn.
	uint scale;                        ///< Width of link lines.
	bool dirty;                        ///< Set if overlay should be rebuilt.

	Point GetStationMiddle(const Station *st) const;

	void AddLinks(const Station *sta, const Station *stb);
	void DrawLinks(const DrawPixelInfo *dpi) const;
	void DrawStationDots(const DrawPixelInfo *dpi) const;
	void DrawContent(Point pta, Point ptb, const LinkProperties &cargo) const;
	bool IsLinkVisible(Point pta, Point ptb, const DrawPixelInfo *dpi, int padding = 0) const;
	bool IsPointVisible(Point pt, const DrawPixelInfo *dpi, int padding = 0) const;
	void GetWidgetDpi(DrawPixelInfo *dpi) const;
	void RebuildCache();

	static void AddStats(CargoID new_cargo, uint new_cap, uint new_usg, uint new_flow, uint32_t time, bool new_shared, LinkProperties &cargo);
	static void DrawVertex(int x, int y, int size, int colour, int border_colour);
};

void ShowLinkGraphLegend();

/**
 * Menu window to select cargoes and companies to show in a link graph overlay.
 */
struct LinkGraphLegendWindow : Window {
public:
	LinkGraphLegendWindow(WindowDesc *desc, int window_number);
	void SetOverlay(std::shared_ptr<LinkGraphOverlay> overlay);

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override;
	void DrawWidget(const Rect &r, WidgetID widget) const override;
	bool OnTooltip([[maybe_unused]] Point pt, WidgetID widget, TooltipCloseCondition close_cond) override;
	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override;
	void OnInvalidateData(int data = 0, bool gui_scope = true) override;

private:
	std::shared_ptr<LinkGraphOverlay> overlay;
	size_t num_cargo;

	void UpdateOverlayCompanies();
	void UpdateOverlayCargoes();
};

#endif /* LINKGRAPH_GUI_H */
