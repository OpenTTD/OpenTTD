/* $Id$ */

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
#include "../core/areaquery_type.hpp"
#include "../station_base.h"
#include "../widget_type.h"
#include "../window_gui.h"
#include "linkgraph_base.h"

/**
 * Properties of a link between two stations.
 */
struct LinkProperties {
	LinkProperties() : capacity(0), usage(0), planned(0), shared(false) {}

	uint capacity; ///< Capacity of the link.
	uint usage;    ///< Actual usage of the link.
	uint planned;  ///< Planned usage of the link.
	bool shared;   ///< If this is a shared link to be drawn dashed.
};

/**
 * Handles drawing of links into some window.
 * The window must either be a smallmap or have a valid viewport.
 */
class LinkGraphOverlay {
public:
	//typedef std::map<StationID, LinkProperties> StationLinkMap;
	//typedef std::map<StationID, StationLinkMap> LinkMap;
	//typedef std::vector<std::pair<StationID, uint> > StationSupplyList;

	struct LinkData {
		StationID from;
		StationID to;
		LinkProperties link_properties;
	};

	struct StationData {
		StationID id;
		uint supply;
	};

	struct LinkMapItem {
		bool is_station;
		union {
			LinkData link;
			StationData station;
		};
		LinkMapItem() {}
	};
	typedef AreaQueryTree<LinkMapItem> LinkTree;

	enum {
		LINK_TREE_UNIT_SHIFT = 14
	};

	static const uint8 LINK_COLOURS[];

	/**
	 * Create a link graph overlay for the specified window.
	 * @param w Window to be drawn into.
	 * @param wid ID of the widget to draw into.
	 * @param cargo_mask Bitmask of cargoes to be shown.
	 * @param company_mask Bitmask of companies to be shown.
	 * @param scale Desired thickness of lines and size of station dots.
	 */
	LinkGraphOverlay(const Window *w, uint wid, CargoTypes cargo_mask, uint32 company_mask, uint scale) :
			window(w), widget_id(wid), cargo_mask(cargo_mask), company_mask(company_mask), scale(scale)
	{
		this->ResetBoundaries();
	}

	void RebuildCache();
	void Draw(const DrawPixelInfo *dpi) const;
	void SetCargoMask(CargoTypes cargo_mask);
	void SetCompanyMask(uint32 company_mask);

	/** Get a bitmask of the currently shown cargoes. */
	CargoTypes GetCargoMask() { return this->cargo_mask; }

	/** Get a bitmask of the currently shown companies. */
	uint32 GetCompanyMask() { return this->company_mask; }

protected:
	const Window *window;              ///< Window to be drawn into.
	const uint widget_id;              ///< ID of Widget in Window to be drawn to.
	CargoTypes cargo_mask;             ///< Bitmask of cargos to be displayed.
	uint32 company_mask;               ///< Bitmask of companies to be displayed.
	LinkTree cached_data;              ///< Cache for links and stations for efficient enumeration in a small area.
	Rect cached_data_boundary;         ///< Boundary rectangle of the cache data.

	uint scale;                        ///< Width of link lines.

	Point GetStationMiddle(const Station *st) const;

	LinkProperties & CreateLinkInCache(StationID from, StationID to, Point from_location, Point to_location, int padding);
	uint & CreateStationInCache(StationID id, Point location, int padding);
	void AddLinks(const Station *from, const Station *to, Point from_location, Point to_location, int padding);
	uint CalculateStationDotSizeFromSupply(uint supply) const;
	Rect RemappedToTreeCoords(const Rect &rect) const;
	bool HasNonEmptyBoundaries() const;
	void ResetBoundaries();
	void DrawContent(Point pta, Point ptb, const LinkProperties &cargo) const;
	bool IsLinkVisible(Point pta, Point ptb, const DrawPixelInfo *dpi, int padding = 0) const;
	bool IsPointVisible(Point pt, const DrawPixelInfo *dpi, int padding = 0) const;
	void GetWidgetDpi(DrawPixelInfo *dpi) const;

	static void AddStats(uint new_cap, uint new_usg, uint new_flow, bool new_shared, LinkProperties &cargo);
	static void DrawVertex(int x, int y, int size, int colour, int border_colour);
};

void ShowLinkGraphLegend();

/**
 * Menu window to select cargoes and companies to show in a link graph overlay.
 */
struct LinkGraphLegendWindow : Window {
public:
	LinkGraphLegendWindow(WindowDesc *desc, int window_number);
	void SetOverlay(LinkGraphOverlay *overlay);

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize);
	virtual void DrawWidget(const Rect &r, int widget) const;
	virtual void OnHover(Point pt, int widget);
	virtual bool OnRightClick(Point pt, int widget);
	virtual void OnClick(Point pt, int widget, int click_count);
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true);

private:
	LinkGraphOverlay *overlay;

	void UpdateOverlayCompanies();
	void UpdateOverlayCargoes();
	bool OnHoverCommon(Point pt, int widget, TooltipCloseCondition close_cond);
};

#endif /* LINKGRAPH_GUI_H */
