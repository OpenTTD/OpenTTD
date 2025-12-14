/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file help_gui.cpp GUI to access manuals and related. */

#include "stdafx.h"
#include "gui.h"
#include "window_gui.h"
#include "textfile_gui.h"
#include "fileio_func.h"
#include "table/control_codes.h"
#include "string_func.h"
#include "openttd.h"
#include "help_gui.h"

#include "widgets/help_widget.h"
#include "widgets/misc_widget.h"

#include "table/strings.h"

#include "safeguards.h"

static const std::string README_FILENAME = "README.md";
static const std::string CHANGELOG_FILENAME = "changelog.md";
static const std::string KNOWN_BUGS_FILENAME = "known-bugs.md";
static const std::string LICENSE_FILENAME = "COPYING.md";
static const std::string FONTS_FILENAME = "fonts.md";

static const std::string WEBSITE_LINK = "https://www.openttd.org/";
static const std::string WIKI_LINK = "https://wiki.openttd.org/";
static const std::string BUGTRACKER_LINK = "https://bugs.openttd.org/";
static const std::string COMMUNITY_LINK = "https://community.openttd.org/";

/** Only show the first 20 changelog versions in the textfile viewer. */
static constexpr size_t CHANGELOG_VERSIONS_LIMIT = 20;

/**
 * Find the path to the game manual file.
 *
 * @param filename The filename to find.
 * @return std::string The path to the filename if found.
 */
static std::optional<std::string> FindGameManualFilePath(std::string_view filename, Subdirectory subdir)
{
	static const Searchpath searchpaths[] = {
		SP_APPLICATION_BUNDLE_DIR, SP_INSTALLATION_DIR, SP_SHARED_DIR, SP_BINARY_DIR, SP_WORKING_DIR
	};

	for (Searchpath sp : searchpaths) {
		std::string file_path = FioGetDirectory(sp, subdir);
		file_path.append(filename);
		if (FioCheckFileExists(file_path, NO_DIRECTORY)) return file_path;
	}

	return {};
}

/** Window class displaying the game manual textfile viewer. */
struct GameManualTextfileWindow : public TextfileWindow {
	GameManualTextfileWindow(std::string_view filename, Subdirectory subdir) : TextfileWindow(nullptr, TFT_GAME_MANUAL)
	{
		this->ConstructWindow();

		/* Mark the content of these files as trusted. */
		this->trusted = true;

		auto filepath = FindGameManualFilePath(filename, subdir);
		/* The user could, in theory, have moved the file. So just show an empty window if that is the case. */
		if (!filepath.has_value()) {
			return;
		}

		this->filepath = filepath.value();
		this->LoadTextfile(this->filepath, NO_DIRECTORY);
		this->OnClick({ 0, 0 }, WID_TF_WRAPTEXT, 1);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_TF_CAPTION) {
			return GetString(stringid, this->filename);
		}

		return this->Window::GetWidgetString(widget, stringid);
	}

	void AfterLoadText() override
	{
		if (this->filename == CHANGELOG_FILENAME) {
			this->link_anchors.clear();
			this->AfterLoadChangelog();
		}
		this->TextfileWindow::AfterLoadText();
	}

	/**
	 * For changelog files, truncate the file after CHANGELOG_VERSIONS_LIMIT versions.
	 *
	 * This is hardcoded and assumes "###" is used to separate versions.
	 */
	void AfterLoadChangelog()
	{
		uint versions = 0;

		/* Look for lines beginning with ###, they indicate a release name. */
		for (size_t line_index = 0; line_index < this->lines.size(); ++line_index) {
			const Line &line = this->lines[line_index];
			if (!line.text.starts_with("###")) continue;

			if (versions >= CHANGELOG_VERSIONS_LIMIT) {
				this->lines.resize(line_index - 2);
				break;
			}

			++versions;
		}
	}
};

/** Window class displaying the help window. */
struct HelpWindow : public Window {

	HelpWindow(WindowDesc &desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);

		this->EnableTextfileButton(README_FILENAME, BASE_DIR, WID_HW_README);
		this->EnableTextfileButton(CHANGELOG_FILENAME, BASE_DIR, WID_HW_CHANGELOG);
		this->EnableTextfileButton(KNOWN_BUGS_FILENAME, BASE_DIR, WID_HW_KNOWN_BUGS);
		this->EnableTextfileButton(LICENSE_FILENAME, BASE_DIR, WID_HW_LICENSE);
		this->EnableTextfileButton(FONTS_FILENAME, DOCS_DIR, WID_HW_FONTS);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_HW_README:
				new GameManualTextfileWindow(README_FILENAME, BASE_DIR);
				break;
			case WID_HW_CHANGELOG:
				new GameManualTextfileWindow(CHANGELOG_FILENAME, BASE_DIR);
				break;
			case WID_HW_KNOWN_BUGS:
				new GameManualTextfileWindow(KNOWN_BUGS_FILENAME, BASE_DIR);
				break;
			case WID_HW_LICENSE:
				new GameManualTextfileWindow(LICENSE_FILENAME, BASE_DIR);
				break;
			case WID_HW_FONTS:
				new GameManualTextfileWindow(FONTS_FILENAME, DOCS_DIR);
				break;
			case WID_HW_WEBSITE:
				OpenBrowser(WEBSITE_LINK);
				break;
			case WID_HW_WIKI:
				OpenBrowser(WIKI_LINK);
				break;
			case WID_HW_BUGTRACKER:
				OpenBrowser(BUGTRACKER_LINK);
				break;
			case WID_HW_COMMUNITY:
				OpenBrowser(COMMUNITY_LINK);
				break;
		}
	}

private:
	void EnableTextfileButton(std::string_view filename, Subdirectory subdir, WidgetID button_widget)
	{
		this->GetWidget<NWidgetLeaf>(button_widget)->SetDisabled(!FindGameManualFilePath(filename, subdir).has_value());
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_helpwin_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetStringTip(STR_HELP_WINDOW_CAPTION),
	EndContainer(),

	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse),
			NWidget(WWT_FRAME, COLOUR_DARK_GREEN), SetStringTip(STR_HELP_WINDOW_WEBSITES),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_WEBSITE), SetStringTip(STR_HELP_WINDOW_MAIN_WEBSITE), SetMinimalSize(128, 12), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_WIKI), SetStringTip(STR_HELP_WINDOW_MANUAL_WIKI), SetMinimalSize(128, 12), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_BUGTRACKER), SetStringTip(STR_HELP_WINDOW_BUGTRACKER), SetMinimalSize(128, 12), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_COMMUNITY), SetStringTip(STR_HELP_WINDOW_COMMUNITY), SetMinimalSize(128, 12), SetFill(1, 0),
			EndContainer(),

			NWidget(WWT_FRAME, COLOUR_DARK_GREEN), SetStringTip(STR_HELP_WINDOW_DOCUMENTS),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_README), SetStringTip(STR_HELP_WINDOW_README), SetMinimalSize(128, 12), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_CHANGELOG), SetStringTip(STR_HELP_WINDOW_CHANGELOG), SetMinimalSize(128, 12), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_KNOWN_BUGS),SetStringTip(STR_HELP_WINDOW_KNOWN_BUGS), SetMinimalSize(128, 12), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_LICENSE), SetStringTip(STR_HELP_WINDOW_LICENSE), SetMinimalSize(128, 12), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_FONTS), SetStringTip(STR_HELP_WINDOW_FONTS), SetMinimalSize(128, 12), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _helpwin_desc(
	WDP_CENTER, {}, 0, 0,
	WC_HELPWIN, WC_NONE,
	{},
	_nested_helpwin_widgets
);

void ShowHelpWindow()
{
	AllocateWindowDescFront<HelpWindow>(_helpwin_desc, 0);
}
