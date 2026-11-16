/*
 * This file is part of korczis's research fork of OpenTTD and is NOT part of upstream OpenTTD.
 * RESEARCH-ONLY / UNSTABLE.
 */

/** @file prismatic_mcp_hooks.h Read-only research hooks into engine internals that the MCP server's UI model needs and no public header offers. */

#ifndef PRISMATIC_MCP_HOOKS_H
#define PRISMATIC_MCP_HOOKS_H

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../core/geometry_type.hpp"

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

} // namespace PrismaticMCP::Hooks

#endif /* OTTD_PRISMATIC_MCP_SERVER */

#endif /* PRISMATIC_MCP_HOOKS_H */
