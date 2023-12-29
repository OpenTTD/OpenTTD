/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
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

#include "safeguards.h"

static const std::string README_FILENAME = "README.md";
static const std::string CHANGELOG_FILENAME = "changelog.txt";
static const std::string KNOWN_BUGS_FILENAME = "known-bugs.txt";
static const std::string LICENSE_FILENAME = "COPYING.md";

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
static std::optional<std::string> FindGameManualFilePath(std::string_view filename)
{
	static const Searchpath searchpaths[] = {
		SP_APPLICATION_BUNDLE_DIR, SP_INSTALLATION_DIR, SP_SHARED_DIR, SP_BINARY_DIR, SP_WORKING_DIR
	};

	for (Searchpath sp : searchpaths) {
		auto file_path = FioGetDirectory(sp, BASE_DIR) + filename.data();
		if (FioCheckFileExists(file_path, NO_DIRECTORY)) return file_path;
	}

	return {};
}

/** Window class displaying the game manual textfile viewer. */
struct GameManualTextfileWindow : public TextfileWindow {
	GameManualTextfileWindow(std::string_view filename) : TextfileWindow(TFT_GAME_MANUAL)
	{
		/* Mark the content of these files as trusted. */
		this->trusted = true;

		auto filepath = FindGameManualFilePath(filename);
		/* The user could, in theory, have moved the file. So just show an empty window if that is the case. */
		if (!filepath.has_value()) {
			return;
		}

		this->filepath = filepath.value();
		this->LoadTextfile(this->filepath, NO_DIRECTORY);
		this->OnClick({ 0, 0 }, WID_TF_WRAPTEXT, 1);
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_TF_CAPTION) {
			SetDParamStr(0, this->filename);
		}
	}

	void AfterLoadText() override
	{
		if (this->filename == CHANGELOG_FILENAME) {
			this->link_anchors.clear();
			this->AfterLoadChangelog();
			if (this->GetWidget<NWidgetStacked>(WID_TF_SEL_JUMPLIST)->SetDisplayedPlane(this->jumplist.empty() ? SZSP_HORIZONTAL : 0)) this->ReInit();
		} else {
			this->TextfileWindow::AfterLoadText();
		}
	}

	/**
	 * For changelog files, add a jumplist entry for each version.
	 *
	 * This is hardcoded and assumes "---" are used to separate versions.
	 */
	void AfterLoadChangelog()
	{
		/* Look for lines beginning with ---, they indicate that the previous line was a release name. */
		for (size_t line_index = 0; line_index < this->lines.size(); ++line_index) {
			const Line &line = this->lines[line_index];
			if (line.text.find("---", 0) != 0) continue;

			if (this->jumplist.size() >= CHANGELOG_VERSIONS_LIMIT) {
				this->lines.resize(line_index - 2);
				break;
			}

			/* Mark the version header with a colour, and add it to the jumplist. */
			this->lines[line_index - 1].colour = TC_GOLD;
			this->lines[line_index].colour = TC_GOLD;
			this->jumplist.push_back(line_index - 1);
		}
	}
};

/** Window class displaying the help window. */
struct HelpWindow : public Window {

	HelpWindow(WindowDesc *desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);

		this->EnableTextfileButton(README_FILENAME, WID_HW_README);
		this->EnableTextfileButton(CHANGELOG_FILENAME, WID_HW_CHANGELOG);
		this->EnableTextfileButton(KNOWN_BUGS_FILENAME, WID_HW_KNOWN_BUGS);
		this->EnableTextfileButton(LICENSE_FILENAME, WID_HW_LICENSE);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_HW_README:
				new GameManualTextfileWindow(README_FILENAME);
				break;
			case WID_HW_CHANGELOG:
				new GameManualTextfileWindow(CHANGELOG_FILENAME);
				break;
			case WID_HW_KNOWN_BUGS:
				new GameManualTextfileWindow(KNOWN_BUGS_FILENAME);
				break;
			case WID_HW_LICENSE:
				new GameManualTextfileWindow(LICENSE_FILENAME);
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
	void EnableTextfileButton(std::string_view filename, int button_widget)
	{
		this->GetWidget<NWidgetLeaf>(button_widget)->SetDisabled(!FindGameManualFilePath(filename).has_value());
	}
};

static const NWidgetPart _nested_helpwin_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_HELP_WINDOW_CAPTION, STR_NULL),
	EndContainer(),

	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse),
			NWidget(WWT_FRAME, COLOUR_DARK_GREEN), SetDataTip(STR_HELP_WINDOW_WEBSITES, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_WEBSITE), SetDataTip(STR_HELP_WINDOW_MAIN_WEBSITE, STR_NULL), SetMinimalSize(128, 12), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_WIKI), SetDataTip(STR_HELP_WINDOW_MANUAL_WIKI, STR_NULL), SetMinimalSize(128, 12), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_BUGTRACKER), SetDataTip(STR_HELP_WINDOW_BUGTRACKER, STR_NULL), SetMinimalSize(128, 12), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_COMMUNITY), SetDataTip(STR_HELP_WINDOW_COMMUNITY, STR_NULL), SetMinimalSize(128, 12), SetFill(1, 0),
			EndContainer(),

			NWidget(WWT_FRAME, COLOUR_DARK_GREEN), SetDataTip(STR_HELP_WINDOW_DOCUMENTS, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_README), SetDataTip(STR_HELP_WINDOW_README, STR_NULL), SetMinimalSize(128, 12), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_CHANGELOG), SetDataTip(STR_HELP_WINDOW_CHANGELOG, STR_NULL), SetMinimalSize(128, 12), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_KNOWN_BUGS),SetDataTip(STR_HELP_WINDOW_KNOWN_BUGS, STR_NULL), SetMinimalSize(128, 12), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_HW_LICENSE), SetDataTip(STR_HELP_WINDOW_LICENSE, STR_NULL), SetMinimalSize(128, 12), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _helpwin_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_HELPWIN, WC_NONE,
	0,
	std::begin(_nested_helpwin_widgets), std::end(_nested_helpwin_widgets)
);

void ShowHelpWindow()
{
	AllocateWindowDescFront<HelpWindow>(&_helpwin_desc, 0);
}
