/*
 * This file is part of korczis's research fork of OpenTTD and is NOT part of upstream OpenTTD.
 * RESEARCH-ONLY / UNSTABLE.
 */

/** @file prismatic_mcp_hooks.h Read-only research hooks into engine internals that the MCP server's UI model needs and no public header offers. */

#ifndef PRISMATIC_MCP_HOOKS_H
#define PRISMATIC_MCP_HOOKS_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../core/geometry_type.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

struct Window;

namespace PrismaticMCP::Hooks {

/** One item of an open dropdown list, as a person sees it: where it is and what choosing it returns. */
struct DropdownItem {
	int result;     ///< The value the dropdown returns to its parent when this item is chosen.
	bool selectable; ///< Whether a click on it chooses it (masked and separator items do not).
	bool selected;  ///< Whether it is the highlighted item.
	Rect rect;      ///< Its rectangle in screen pixels (the visible part only).
	bool matches;   ///< Whether its text matches the words asked for (the dropdown's own text filter); false when none were asked.
};

/**
 * The visible items of an open dropdown window, in screen pixels, laid out exactly as the
 * window draws and hit-tests them (dropdown.cpp). Read-only.
 * @param w A window; anything other than a dropdown menu answers an empty list.
 * @param match Words to test each item's text against, with the filter a dropdown uses when the
 *              player types into it (StringFilter, every word must occur); empty tests nothing.
 * @return The items, top to bottom, that are at least partly visible.
 */
std::vector<DropdownItem> DropdownItems(const Window *w, std::string_view match = {});

/** A place in a window where a vehicle is drawn, and what a press there does to it. */
struct VehicleCell {
	uint32_t vehicle;        ///< The vehicle (for a group of shared orders, its first vehicle).
	std::string_view action; ///< "drag" (picks the whole vehicle up), "open" (its window), "start_stop", or "open_group".
	Rect rect;               ///< Its rectangle in screen pixels.
	uint count;              ///< How many vehicles the cell stands for (more than one for a group of shared orders).
};

/**
 * The vehicles drawn in a depot window, found with the window's own hit test
 * (DepotWindow::GetVehicleFromDepotWndPt): for each vehicle, where a press opens it, starts or stops
 * it, or picks the whole vehicle up to drop on the sell button. Changes no game state.
 * @param w A window; anything other than a depot window answers an empty list.
 */
std::vector<VehicleCell> DepotCells(const Window *w);

/**
 * The rows of a vehicle list window (the plain list or the groups window's vehicle list), found
 * with its own hit test (the list's scrolled rows):
 * a press on one opens that vehicle (or, grouped by shared orders, the group). Changes nothing.
 * @param w A window; anything other than a vehicle list answers an empty list.
 */
std::vector<VehicleCell> VehicleListCells(const Window *w);

} // namespace PrismaticMCP::Hooks

#endif /* OTTD_PRISMATIC_MCP_SERVER */

#endif /* PRISMATIC_MCP_HOOKS_H */
