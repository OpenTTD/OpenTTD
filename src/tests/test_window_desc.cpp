/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file test_window_desc.cpp Test WindowDescs for valid widget parts. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../window_gui.h"

/**
 * List of WindowDescs. Defined in window.cpp but not exposed as this unit-test is the only other place that needs it.
 * WindowDesc is a self-registering class so all WindowDescs will be included in the list.
 */
extern std::vector<WindowDesc*> *_window_descs;

TEST_CASE("WindowDesc - ini_key uniqueness")
{
	std::set<std::string> seen;

	for (const WindowDesc *window_desc : *_window_descs) {

		if (window_desc->ini_key == nullptr) continue;

		CAPTURE(window_desc->ini_key);
		CHECK((seen.find(window_desc->ini_key) == std::end(seen)));

		seen.insert(window_desc->ini_key);
	}
}

TEST_CASE("WindowDesc - ini_key validity")
{
	const WindowDesc *window_desc = GENERATE(from_range(std::begin(*_window_descs), std::end(*_window_descs)));

	bool has_inikey = window_desc->ini_key != nullptr;
	bool has_widget = std::any_of(window_desc->nwid_begin, window_desc->nwid_end, [](const NWidgetPart &part) { return part.type == WWT_DEFSIZEBOX || part.type == WWT_STICKYBOX; });

	INFO(fmt::format("{}:{}", window_desc->file, window_desc->line));
	CAPTURE(has_inikey);
	CAPTURE(has_widget);

	CHECK((has_widget == has_inikey));
}
