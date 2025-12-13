/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file textfile_gui.h GUI functions related to textfiles. */

#ifndef TEXTFILE_GUI_H
#define TEXTFILE_GUI_H

#include "misc/alternating_iterator.hpp"
#include "fileio_type.h"
#include "strings_func.h"
#include "textfile_type.h"
#include "window_gui.h"

std::optional<std::string> GetTextfile(TextfileType type, Subdirectory dir, std::string_view filename);

/** Window for displaying a textfile */
struct TextfileWindow : public Window, MissingGlyphSearcher {
	TextfileType file_type{}; ///< Type of textfile to view.
	Scrollbar *vscroll = nullptr; ///< Vertical scrollbar.
	Scrollbar *hscroll = nullptr; ///< Horizontal scrollbar.

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override;
	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override;
	bool OnTooltip([[maybe_unused]] Point pt, WidgetID widget, TooltipCloseCondition close_cond) override;
	void DrawWidget(const Rect &r, WidgetID widget) const override;
	void OnResize() override;
	void OnInit() override;
	void OnInvalidateData(int data = 0, bool gui_scope = true) override;
	void OnDropdownSelect(WidgetID widget, int index, int) override;
	void OnRealtimeTick(uint delta_ms) override;
	void OnScrollbarScroll(WidgetID widget) override;

	void Reset() override;
	FontSize DefaultSize() override;
	std::optional<std::string_view> NextString() override;
	bool Monospace() override;
	void SetFontNames(FontCacheSettings *settings, std::string_view font_name, const void *os_data) override;
	void ScrollToLine(size_t line);
	bool IsTextWrapped() const;

	virtual void LoadTextfile(const std::string &textfile, Subdirectory dir);

protected:
	TextfileWindow(Window *parent, TextfileType file_type);
	void ConstructWindow();

	struct Line {
		int num_lines = 1; ///< Number of visual lines for this line.
		int wrapped_width = 0;
		int max_width = -1;
		TextColour colour = TC_WHITE; ///< Colour to render text line in.
		std::string text{};           ///< Contents of the line.

		Line(std::string_view text) : text(text) {}
		Line() {}
	};

	struct Hyperlink {
		size_t line = 0; ///< Which line the link is on.
		size_t begin = 0; ///< Character position on line the link begins.
		size_t end = 0; ///< Character position on line the link end.
		std::string destination{}; ///< Destination for the link.
	};

	struct HistoryEntry {
		std::string filepath;    ///< File the history entry is in.
		int scrollpos;           ///< Scrolling position the file was at at navigation time.
	};

	std::string filename{}; ///< Filename of the textfile.
	std::string filepath{}; ///< Full path to the filename.

	std::vector<Line> lines{}; ///< #text, split into lines in a table with lines.
	std::vector<size_t> jumplist{}; ///< Table of contents list, line numbers.
	std::vector<Hyperlink> links{}; ///< Clickable links in lines.
	std::vector<Hyperlink> link_anchors{}; ///< Anchor names of headings that can be linked to.
	std::vector<HistoryEntry> history{}; ///< Browsing history in this window.
	size_t history_pos = 0; ///< Position in browsing history (for forward movement).
	bool trusted = false; ///< Whether the content is trusted (read: not from content like NewGRFs, etc).

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
	uint search_iterator = 0; ///< Iterator for the font check search.
	int max_width = 0; ///< Maximum length of unwrapped text line.
	size_t num_lines = 0; ///< Number of lines of text, taking account of wrapping.

	using LineIterator = std::vector<Line>::iterator;
	using ReflowIterator = AlternatingIterator<LineIterator>;

	ReflowIterator reflow_iter; ///< Current iterator for reflow.
	ReflowIterator reflow_end; ///< End iterator for reflow.

	LineIterator visible_first; ///< Iterator to first visible element.
	LineIterator visible_last; ///< Iterator to last visible element.

	enum class ReflowState : uint8_t {
		None, ///< Nothing has been reflowed.
		Reflowed, ///< Content has been reflowed.
		VisibleReflowed, ///< Visible content has been reflowed.
	};

	std::vector<TextfileWindow::Line>::iterator GetIteratorFromPosition(int pos);
	void UpdateVisibleIterators();
	void ReflowContent();
	ReflowState ContinueReflow();
	void SetupScrollbars();
	const Hyperlink *GetHyperlink(Point pt) const;

	void AfterLoadMarkdown();
};

#endif /* TEXTFILE_GUI_H */
