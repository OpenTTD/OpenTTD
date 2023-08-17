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
	uint search_iterator;            ///< Iterator for the font check search.

	uint max_length;                 ///< Maximum length of unwrapped text line.

	TextfileWindow(TextfileType file_type);

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override;
	void OnClick(Point pt, int widget, int click_count) override;
	void DrawWidget(const Rect &r, int widget) const override;
	void OnResize() override;
	void OnInvalidateData(int data = 0, bool gui_scope = true) override;

	void Reset() override;
	FontSize DefaultSize() override;
	std::optional<std::string_view> NextString() override;
	bool Monospace() override;
	void SetFontNames(FontCacheSettings *settings, const char *font_name, const void *os_data) override;

	virtual void LoadTextfile(const std::string &textfile, Subdirectory dir);

protected:
	void LoadText(std::string_view buf);

private:
	struct Line {
		int top;               ///< Top scroll position.
		int bottom;            ///< Bottom scroll position.
		std::string_view text; ///< Pointer to text buffer.

		Line(int top, std::string_view text) : top(top), bottom(top + 1), text(text) {}
	};

	std::string text;                ///< Lines of text from the NewGRF's textfile.
	std::vector<Line> lines;         ///< #text, split into lines in a table with lines.

	uint ReflowContent();
	uint GetContentHeight();
	void SetupScrollbars(bool force_reflow);
};

#endif /* TEXTFILE_GUI_H */
