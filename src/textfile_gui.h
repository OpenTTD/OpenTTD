/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file textfile_gui.h GUI functions related to textfiles. */

#ifndef TEXTFILE_GUI_H
#define TEXTFILE_GUI_H

#include "fileio_type.h"
#include "strings_func.h"
#include "textfile_type.h"
#include "window_gui.h"

std::optional<std::string> GetTextfile(TextfileType type, Subdirectory dir, const std::string &filename);

/** Window for displaying a textfile */
struct TextfileWindow : public Window, MissingGlyphSearcher {
	TextfileType file_type;          ///< Type of textfile to view.
	Scrollbar *vscroll;              ///< Vertical scrollbar.
	Scrollbar *hscroll;              ///< Horizontal scrollbar.

	TextfileWindow(TextfileType file_type);

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override;
	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override;
	void DrawWidget(const Rect &r, WidgetID widget) const override;
	void OnResize() override;
	void OnInvalidateData(int data = 0, bool gui_scope = true) override;
	void OnDropdownSelect(WidgetID widget, int index) override;

	void Reset() override;
	FontSize DefaultSize() override;
	std::optional<std::string_view> NextString() override;
	bool Monospace() override;
	void SetFontNames(FontCacheSettings *settings, const char *font_name, const void *os_data) override;
	void ScrollToLine(size_t line);

	virtual void LoadTextfile(const std::string &textfile, Subdirectory dir);

protected:
	struct Line {
		int top{0};                  ///< Top scroll position in visual lines.
		int bottom{0};               ///< Bottom scroll position in visual lines.
		std::string text{};          ///< Contents of the line.
		TextColour colour{TC_WHITE}; ///< Colour to render text line in.

		Line(int top, std::string_view text) : top(top), bottom(top + 1), text(text) {}
		Line() {}
	};

	struct Hyperlink {
		size_t line;             ///< Which line the link is on.
		size_t begin;            ///< Character position on line the link begins.
		size_t end;              ///< Character position on line the link end.
		std::string destination; ///< Destination for the link.
	};

	struct HistoryEntry {
		std::string filepath;    ///< File the history entry is in.
		int scrollpos;           ///< Scrolling position the file was at at navigation time.
	};

	std::string filename{};              ///< Filename of the textfile.
	std::string filepath{};              ///< Full path to the filename.

	std::vector<Line> lines;             ///< #text, split into lines in a table with lines.
	std::vector<size_t> jumplist;        ///< Table of contents list, line numbers.
	std::vector<Hyperlink> links;        ///< Clickable links in lines.
	std::vector<Hyperlink> link_anchors; ///< Anchor names of headings that can be linked to.
	std::vector<HistoryEntry> history;   ///< Browsing history in this window.
	size_t history_pos{0};               ///< Position in browsing history (for forward movement).
	bool trusted{false};                 ///< Whether the content is trusted (read: not from content like NewGRFs, etc).

	void LoadText(std::string_view buf);
	void FindHyperlinksInMarkdown(Line &line, size_t line_index);

	/**
	 * Handle the clicking on a hyperlink.
	 *
	 * @param link The link that is clicked on.
	 */
	virtual void OnHyperlinkClick(const Hyperlink &link);

	/**
	 * Post-processing after the text is loaded.
	 */
	virtual void AfterLoadText();

	void NavigateToFile(std::string newfile, size_t line);
	void AppendHistory(const std::string &filepath);
	void UpdateHistoryScrollpos();
	void NavigateHistory(int delta);

private:
	uint search_iterator{0};     ///< Iterator for the font check search.
	uint max_length{0};          ///< Maximum length of unwrapped text line.

	uint ReflowContent();
	uint GetContentHeight();
	void SetupScrollbars(bool force_reflow);
	void CheckHyperlinkClick(Point pt);

	void AfterLoadMarkdown();
};

#endif /* TEXTFILE_GUI_H */
