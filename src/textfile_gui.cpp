/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file textfile_gui.cpp Implementation of textfile window. */

#include "stdafx.h"
#include "core/backup_type.hpp"
#include "fileio_func.h"
#include "fontcache.h"
#include "gfx_type.h"
#include "gfx_func.h"
#include "string_func.h"
#include "textfile_gui.h"
#include "widgets/dropdown_type.h"
#include "gfx_layout.h"
#include "debug.h"
#include "openttd.h"

#include "widgets/misc_widget.h"

#include "table/strings.h"
#include "table/control_codes.h"

#if defined(WITH_ZLIB)
#include <zlib.h>
#endif

#if defined(WITH_LIBLZMA)
#include <lzma.h>
#endif

#include <regex>

#include "safeguards.h"

/** Widgets for the textfile window. */
static const NWidgetPart _nested_textfile_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_PUSHARROWBTN, COLOUR_MAUVE, WID_TF_NAVBACK), SetFill(0, 1), SetMinimalSize(15, 1), SetDataTip(AWV_DECREASE, STR_TEXTFILE_NAVBACK_TOOLTIP),
		NWidget(WWT_PUSHARROWBTN, COLOUR_MAUVE, WID_TF_NAVFORWARD), SetFill(0, 1), SetMinimalSize(15, 1), SetDataTip(AWV_INCREASE, STR_TEXTFILE_NAVFORWARD_TOOLTIP),
		NWidget(WWT_CAPTION, COLOUR_MAUVE, WID_TF_CAPTION), SetDataTip(STR_NULL, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_TEXTBTN, COLOUR_MAUVE, WID_TF_WRAPTEXT), SetDataTip(STR_TEXTFILE_WRAP_TEXT, STR_TEXTFILE_WRAP_TEXT_TOOLTIP),
		NWidget(WWT_DEFSIZEBOX, COLOUR_MAUVE),
	EndContainer(),
	NWidget(NWID_SELECTION, INVALID_COLOUR, WID_TF_SEL_JUMPLIST),
		NWidget(WWT_PANEL, COLOUR_MAUVE),
			NWidget(NWID_HORIZONTAL), SetPIP(WidgetDimensions::unscaled.frametext.left, 0, WidgetDimensions::unscaled.frametext.right),
				/* As this widget can be toggled, it needs to be a multiplier of FS_MONO. So add a spacer that ensures this. */
				NWidget(NWID_SPACER), SetMinimalSize(1, 0), SetMinimalTextLines(2, 0, FS_MONO),
				NWidget(NWID_VERTICAL),
					NWidget(NWID_SPACER), SetFill(1, 1), SetResize(1, 0),
					NWidget(WWT_DROPDOWN, COLOUR_MAUVE, WID_TF_JUMPLIST), SetDataTip(STR_TEXTFILE_JUMPLIST, STR_TEXTFILE_JUMPLIST_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
					NWidget(NWID_SPACER), SetFill(1, 1), SetResize(1, 0),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_MAUVE, WID_TF_BACKGROUND), SetMinimalSize(200, 125), SetResize(1, 12), SetScrollbar(WID_TF_VSCROLLBAR),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_TF_VSCROLLBAR),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HSCROLLBAR, COLOUR_MAUVE, WID_TF_HSCROLLBAR),
		NWidget(WWT_RESIZEBOX, COLOUR_MAUVE),
	EndContainer(),
};

/** Window definition for the textfile window */
static WindowDesc _textfile_desc(__FILE__, __LINE__,
	WDP_CENTER, "textfile", 630, 460,
	WC_TEXTFILE, WC_NONE,
	0,
	std::begin(_nested_textfile_widgets), std::end(_nested_textfile_widgets)
);

TextfileWindow::TextfileWindow(TextfileType file_type) : Window(&_textfile_desc), file_type(file_type)
{
	this->CreateNestedTree();
	this->vscroll = this->GetScrollbar(WID_TF_VSCROLLBAR);
	this->hscroll = this->GetScrollbar(WID_TF_HSCROLLBAR);
	this->GetWidget<NWidgetCore>(WID_TF_CAPTION)->SetDataTip(STR_TEXTFILE_README_CAPTION + file_type, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS);
	this->GetWidget<NWidgetStacked>(WID_TF_SEL_JUMPLIST)->SetDisplayedPlane(SZSP_HORIZONTAL);
	this->FinishInitNested(file_type);

	this->DisableWidget(WID_TF_NAVBACK);
	this->DisableWidget(WID_TF_NAVFORWARD);
	this->hscroll->SetStepSize(10); // Speed up horizontal scrollbar
}

/**
 * Get the total height of the content displayed in this window, if wrapping is disabled.
 * @return the height in pixels
 */
uint TextfileWindow::ReflowContent()
{
	uint height = 0;
	if (!IsWidgetLowered(WID_TF_WRAPTEXT)) {
		for (auto &line : this->lines) {
			line.top = height;
			height++;
			line.bottom = height;
		}
	} else {
		int max_width = this->GetWidget<NWidgetCore>(WID_TF_BACKGROUND)->current_x - WidgetDimensions::scaled.frametext.Horizontal();
		for (auto &line : this->lines) {
			line.top = height;
			height += GetStringHeight(line.text, max_width, FS_MONO) / GetCharacterHeight(FS_MONO);
			line.bottom = height;
		}
	}

	return height;
}

uint TextfileWindow::GetContentHeight()
{
	if (this->lines.empty()) return 0;
	return this->lines.back().bottom;
}

/* virtual */ void TextfileWindow::UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize)
{
	switch (widget) {
		case WID_TF_BACKGROUND:
			resize->height = GetCharacterHeight(FS_MONO);

			size->height = 4 * resize->height + WidgetDimensions::scaled.frametext.Vertical(); // At least 4 lines are visible.
			size->width = std::max(200u, size->width); // At least 200 pixels wide.
			break;
	}
}

/** Set scrollbars to the right lengths. */
void TextfileWindow::SetupScrollbars(bool force_reflow)
{
	if (IsWidgetLowered(WID_TF_WRAPTEXT)) {
		/* Reflow is mandatory if text wrapping is on */
		uint height = this->ReflowContent();
		this->vscroll->SetCount(ClampTo<uint16_t>(height));
		this->hscroll->SetCount(0);
	} else {
		uint height = force_reflow ? this->ReflowContent() : this->GetContentHeight();
		this->vscroll->SetCount(ClampTo<uint16_t>(height));
		this->hscroll->SetCount(this->max_length + WidgetDimensions::scaled.frametext.Horizontal());
	}

	this->SetWidgetDisabledState(WID_TF_HSCROLLBAR, IsWidgetLowered(WID_TF_WRAPTEXT));
}


/** Regular expression that searches for Markdown links. */
static const std::regex _markdown_link_regex{"\\[(.+?)\\]\\((.+?)\\)", std::regex_constants::ECMAScript | std::regex_constants::optimize};

/** Types of link we support in markdown files. */
enum class HyperlinkType {
	Internal, ///< Internal link, or "anchor" in HTML language.
	Web,      ///< Link to an external website.
	File,     ///< Link to a local file.
	Unknown,  ///< Unknown link.
};

/**
 * Classify the type of hyperlink the destination describes.
 *
 * @param destination The hyperlink destination.
 * @param trusted Whether we trust the content of this file.
 * @return HyperlinkType The classification of the link.
 */
static HyperlinkType ClassifyHyperlink(const std::string &destination, bool trusted)
{
	if (destination.empty()) return HyperlinkType::Unknown;
	if (StrStartsWith(destination, "#")) return HyperlinkType::Internal;

	/* Only allow external / internal links for sources we trust. */
	if (!trusted) return HyperlinkType::Unknown;

	if (StrStartsWith(destination, "http://")) return HyperlinkType::Web;
	if (StrStartsWith(destination, "https://")) return HyperlinkType::Web;
	if (StrStartsWith(destination, "./")) return HyperlinkType::File;
	return HyperlinkType::Unknown;
}

/**
 * Create a valid slug for the anchor.
 *
 * @param line The line to create the slug for.
 * @return std::string The slug.
 */
static std::string MakeAnchorSlug(const std::string &line)
{
	std::string r = "#";
	uint state = 0;
	for (char c : line) {
		if (state == 0) {
			/* State 0: Skip leading hashmarks and spaces. */
			if (c == '#') continue;
			if (c == ' ') continue;
			state = 1;
		}
		if (state == 2) {
			/* State 2: Wait for a non-space/dash character.
			 * When found, output a dash and that character. */
			if (c == ' ' || c == '-') continue;
			r += '-';
			state = 1;
		}
		if (state == 1) {
			/* State 1: Normal text.
			 * Lowercase alphanumerics,
			 * spaces and dashes become dashes,
			 * everything else is removed. */
			if (isalnum(c)) {
				r += tolower(c);
			} else if (c == ' ' || c == '-') {
				state = 2;
			}
		}
	}

	return r;
}

/**
 * Find any hyperlinks in a given line.
 *
 * @param line The line to search for hyperlinks.
 * @param line_index The index of the line.
 */
void TextfileWindow::FindHyperlinksInMarkdown(Line &line, size_t line_index)
{
	std::string::const_iterator last_match_end = line.text.cbegin();
	std::string fixed_line;
	char ccbuf[5];

	std::sregex_iterator matcher{ line.text.cbegin(), line.text.cend(), _markdown_link_regex};
	while (matcher != std::sregex_iterator()) {
		std::smatch match = *matcher;

		Hyperlink link;
		link.line = line_index;
		link.destination = match[2].str();
		this->links.push_back(link);

		HyperlinkType link_type = ClassifyHyperlink(link.destination, this->trusted);
		StringControlCode link_colour;
		switch (link_type) {
			case HyperlinkType::Internal:
				link_colour = SCC_GREEN;
				break;
			case HyperlinkType::Web:
				link_colour = SCC_LTBLUE;
				break;
			case HyperlinkType::File:
				link_colour = SCC_LTBROWN;
				break;
			default:
				/* Don't make other link types fancy as they aren't handled (yet). */
				link_colour = SCC_CONTROL_END;
				break;
		}

		if (link_colour != SCC_CONTROL_END) {
			/* Format the link to look like a link. */
			fixed_line += std::string(last_match_end, match[0].first);
			this->links.back().begin = fixed_line.length();
			fixed_line += std::string(ccbuf, Utf8Encode(ccbuf, SCC_PUSH_COLOUR));
			fixed_line += std::string(ccbuf, Utf8Encode(ccbuf, link_colour));
			fixed_line += match[1].str();
			this->links.back().end = fixed_line.length();
			fixed_line += std::string(ccbuf, Utf8Encode(ccbuf, SCC_POP_COLOUR));
			last_match_end = match[0].second;
		}

		/* Find next link. */
		++matcher;
	}
	if (last_match_end == line.text.cbegin()) return; // nothing found

	/* Add remaining text on line. */
	fixed_line += std::string(last_match_end, line.text.cend());

	/* Overwrite original line text with "fixed" line text. */
	line.text = fixed_line;
}

/**
 * Check if the user clicked on a hyperlink, and handle it if so.
 *
 * @param pt The loation the user clicked.
 */
void TextfileWindow::CheckHyperlinkClick(Point pt)
{
	if (this->links.empty()) return;

	/* Which line was clicked. */
	const int clicked_row = this->GetRowFromWidget(pt.y, WID_TF_BACKGROUND, WidgetDimensions::scaled.frametext.top, GetCharacterHeight(FS_MONO)) + this->GetScrollbar(WID_TF_VSCROLLBAR)->GetPosition();
	size_t line_index;
	size_t subline;
	if (IsWidgetLowered(WID_TF_WRAPTEXT)) {
		auto it = std::find_if(std::begin(this->lines), std::end(this->lines), [clicked_row](const Line &l) { return l.top <= clicked_row && l.bottom > clicked_row; });
		if (it == this->lines.cend()) return;
		line_index = it - this->lines.cbegin();
		subline = clicked_row - it->top;
		Debug(misc, 4, "TextfileWindow check hyperlink: clicked_row={}, line_index={}, line.top={}, subline={}", clicked_row, line_index, it->top, subline);
	} else {
		line_index = clicked_row / GetCharacterHeight(FS_MONO);
		subline = 0;
	}

	/* Find hyperlinks in this line. */
	std::vector<Hyperlink> found_links;
	for (const auto &link : this->links) {
		if (link.line == line_index) found_links.push_back(link);
	}
	if (found_links.empty()) return;

	/* Build line layout to figure out character position that was clicked. */
	uint window_width = IsWidgetLowered(WID_TF_WRAPTEXT) ? this->GetWidget<NWidgetCore>(WID_TF_BACKGROUND)->current_x - WidgetDimensions::scaled.frametext.Horizontal() : INT_MAX;
	Layouter layout(this->lines[line_index].text, window_width, this->lines[line_index].colour, FS_MONO);
	assert(subline < layout.size());
	ptrdiff_t char_index = layout.GetCharAtPosition(pt.x - WidgetDimensions::scaled.frametext.left, subline);
	if (char_index < 0) return;
	Debug(misc, 4, "TextfileWindow check hyperlink click: line={}, subline={}, char_index={}", line_index, subline, (int)char_index);

	/* Found character index in line, check if any links are at that position. */
	for (const auto &link : found_links) {
		Debug(misc, 4, "Checking link from char {} to {}", link.begin, link.end);
		if ((size_t)char_index >= link.begin && (size_t)char_index < link.end) {
			Debug(misc, 4, "Activating link with destination: {}", link.destination);
			this->OnHyperlinkClick(link);
			return;
		}
	}
}

/**
 * Append the new location to the history, so the user can go back.
 *
 * @param filepath The location the user is navigating to.
 */
void TextfileWindow::AppendHistory(const std::string &filepath)
{
	this->history.erase(this->history.begin() + this->history_pos + 1, this->history.end());
	this->UpdateHistoryScrollpos();
	this->history.push_back(HistoryEntry{ filepath, 0 });
	this->EnableWidget(WID_TF_NAVBACK);
	this->DisableWidget(WID_TF_NAVFORWARD);
	this->history_pos = this->history.size() - 1;
}

/**
 * Update the scroll position to the current, so we can restore there if we go back.
 */
void TextfileWindow::UpdateHistoryScrollpos()
{
	this->history[this->history_pos].scrollpos = this->GetScrollbar(WID_TF_VSCROLLBAR)->GetPosition();
}

/**
 * Navigate through the history, either forward or backward.
 *
 * @param delta The direction to navigate.
 */
void TextfileWindow::NavigateHistory(int delta)
{
	if (delta == 0) return;
	if (delta < 0 && static_cast<int>(this->history_pos) < -delta) return;
	if (delta > 0 && this->history_pos + delta >= this->history.size()) return;

	this->UpdateHistoryScrollpos();
	this->history_pos += delta;

	if (this->history[this->history_pos].filepath != this->filepath) {
		this->filepath = this->history[this->history_pos].filepath;
		this->filename = this->filepath.substr(this->filepath.find_last_of(PATHSEP) + 1);
		this->LoadTextfile(this->filepath, NO_DIRECTORY);
	}

	this->SetWidgetDisabledState(WID_TF_NAVFORWARD, this->history_pos + 1 >= this->history.size());
	this->SetWidgetDisabledState(WID_TF_NAVBACK, this->history_pos == 0);
	this->GetScrollbar(WID_TF_VSCROLLBAR)->SetPosition(this->history[this->history_pos].scrollpos);
	this->GetScrollbar(WID_TF_HSCROLLBAR)->SetPosition(0);
	this->SetDirty();
}

/* virtual */ void TextfileWindow::OnHyperlinkClick(const Hyperlink &link)
{
	switch (ClassifyHyperlink(link.destination, this->trusted)) {
		case HyperlinkType::Internal:
		{
			auto it = std::find_if(this->link_anchors.cbegin(), this->link_anchors.cend(), [&](const Hyperlink &other) { return link.destination == other.destination; });
			if (it != this->link_anchors.cend()) {
				this->AppendHistory(this->filepath);
				this->ScrollToLine(it->line);
				this->UpdateHistoryScrollpos();
			}
			break;
		}

		case HyperlinkType::Web:
			OpenBrowser(link.destination);
			break;

		case HyperlinkType::File:
			this->NavigateToFile(link.destination, 0);
			break;

		default:
			/* Do nothing */
			break;
	}
}

/**
 * Navigate to the requested file.
 *
 * @param newfile The file to navigate to.
 * @param line The line to scroll to.
 */
void TextfileWindow::NavigateToFile(std::string newfile, size_t line)
{
	/* Double-check that the file link begins with ./ as a relative path. */
	if (!StrStartsWith(newfile, "./")) return;

	/* Get the path portion of the current file path. */
	std::string newpath = this->filepath;
	size_t pos = newpath.find_last_of(PATHSEPCHAR);
	if (pos == std::string::npos) {
		newpath.clear();
	} else {
		newpath.erase(pos + 1);
	}

	/* Check and remove for anchor in link. Do this before we find the filename, as people might have a / after the hash. */
	size_t anchor_pos = newfile.find_first_of('#');
	std::string anchor;
	if (anchor_pos != std::string::npos) {
		anchor = newfile.substr(anchor_pos);
		newfile.erase(anchor_pos);
	}

	/* Now the anchor is gone, check if this is a markdown or textfile. */
	if (!StrEndsWithIgnoreCase(newfile, ".md") && !StrEndsWithIgnoreCase(newfile, ".txt")) return;

	/* Convert link destination to acceptable local filename (replace forward slashes with correct path separator). */
	newfile = newfile.substr(2);
	if (PATHSEPCHAR != '/') {
		for (char &c : newfile) {
			if (c == '/') c = PATHSEPCHAR;
		}
	}

	/* Paste the two together and check file exists. */
	newpath = newpath + newfile;
	if (!FioCheckFileExists(newpath, NO_DIRECTORY)) return;

	/* Update history. */
	this->AppendHistory(newpath);

	/* Load the new file. */
	this->filepath = newpath;
	this->filename = newpath.substr(newpath.find_last_of(PATHSEP) + 1);

	this->LoadTextfile(this->filepath, NO_DIRECTORY);

	this->GetScrollbar(WID_TF_HSCROLLBAR)->SetPosition(0);
	this->GetScrollbar(WID_TF_VSCROLLBAR)->SetPosition(0);

	if (anchor.empty() || line != 0) {
		this->ScrollToLine(line);
	} else {
		auto anchor_dest = std::find_if(this->link_anchors.cbegin(), this->link_anchors.cend(), [&](const Hyperlink &other) { return anchor == other.destination; });
		if (anchor_dest != this->link_anchors.cend()) {
			this->ScrollToLine(anchor_dest->line);
			this->UpdateHistoryScrollpos();
		} else {
			this->ScrollToLine(0);
		}
	}
}

/* virtual */ void TextfileWindow::AfterLoadText()
{
	this->link_anchors.clear();

	if (StrEndsWithIgnoreCase(this->filename, ".md")) this->AfterLoadMarkdown();

	if (this->GetWidget<NWidgetStacked>(WID_TF_SEL_JUMPLIST)->SetDisplayedPlane(this->jumplist.empty() ? SZSP_HORIZONTAL : 0)) this->ReInit();
}

/**
 * Post-processing of markdown files.
 */
void TextfileWindow::AfterLoadMarkdown()
{
	for (size_t line_index = 0; line_index < this->lines.size(); ++line_index) {
		Line &line = this->lines[line_index];

		/* Find and mark all hyperlinks in the line. */
		this->FindHyperlinksInMarkdown(line, line_index);

		/* All lines beginning with # are headings. */
		if (!line.text.empty() && line.text[0] == '#') {
			this->jumplist.push_back(line_index);
			this->lines[line_index].colour = TC_GOLD;
			this->link_anchors.emplace_back(Hyperlink{ line_index, 0, 0, MakeAnchorSlug(line.text) });
		}
	}
}

/* virtual */ void TextfileWindow::OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count)
{
	switch (widget) {
		case WID_TF_WRAPTEXT:
			this->ToggleWidgetLoweredState(WID_TF_WRAPTEXT);
			this->InvalidateData();
			break;

		case WID_TF_JUMPLIST: {
			DropDownList list;
			for (size_t line : this->jumplist) {
				SetDParamStr(0, this->lines[line].text);
				list.push_back(std::make_unique<DropDownListStringItem>(STR_TEXTFILE_JUMPLIST_ITEM, (int)line, false));
			}
			ShowDropDownList(this, std::move(list), -1, widget);
			break;
		}

		case WID_TF_NAVBACK:
			this->NavigateHistory(-1);
			break;

		case WID_TF_NAVFORWARD:
			this->NavigateHistory(+1);
			break;

		case WID_TF_BACKGROUND:
			this->CheckHyperlinkClick(pt);
			break;
	}
}

/* virtual */ void TextfileWindow::DrawWidget(const Rect &r, WidgetID widget) const
{
	if (widget != WID_TF_BACKGROUND) return;

	Rect fr = r.Shrink(WidgetDimensions::scaled.frametext);

	DrawPixelInfo new_dpi;
	if (!FillDrawPixelInfo(&new_dpi, fr)) return;
	AutoRestoreBackup dpi_backup(_cur_dpi, &new_dpi);

	/* Draw content (now coordinates given to DrawString* are local to the new clipping region). */
	fr = fr.Translate(-fr.left, -fr.top);
	int line_height = GetCharacterHeight(FS_MONO);
	int pos = this->vscroll->GetPosition();
	int cap = this->vscroll->GetCapacity();

	for (auto &line : this->lines) {
		if (line.bottom < pos) continue;
		if (line.top > pos + cap) break;

		int y_offset = (line.top - pos) * line_height;
		if (IsWidgetLowered(WID_TF_WRAPTEXT)) {
			DrawStringMultiLine(0, fr.right, y_offset, fr.bottom, line.text, line.colour, SA_TOP | SA_LEFT, false, FS_MONO);
		} else {
			DrawString(-this->hscroll->GetPosition(), fr.right, y_offset, line.text, line.colour, SA_TOP | SA_LEFT, false, FS_MONO);
		}
	}
}

/* virtual */ void TextfileWindow::OnResize()
{
	this->vscroll->SetCapacityFromWidget(this, WID_TF_BACKGROUND, WidgetDimensions::scaled.frametext.Vertical());
	this->hscroll->SetCapacityFromWidget(this, WID_TF_BACKGROUND);

	this->SetupScrollbars(false);
}

/* virtual */ void TextfileWindow::OnInvalidateData([[maybe_unused]] int data, [[maybe_unused]] bool gui_scope)
{
	if (!gui_scope) return;

	this->SetupScrollbars(true);
}

void TextfileWindow::OnDropdownSelect(WidgetID widget, int index)
{
	if (widget != WID_TF_JUMPLIST) return;

	this->ScrollToLine(index);
}

void TextfileWindow::ScrollToLine(size_t line)
{
	Scrollbar *sb = this->GetScrollbar(WID_TF_VSCROLLBAR);
	int newpos;
	if (this->IsWidgetLowered(WID_TF_WRAPTEXT)) {
		newpos = this->lines[line].top;
	} else {
		newpos = static_cast<int>(line);
	}
	sb->SetPosition(std::min(newpos, sb->GetCount() - sb->GetCapacity()));
	this->SetDirty();
}

/* virtual */ void TextfileWindow::Reset()
{
	this->search_iterator = 0;
}

/* virtual */ FontSize TextfileWindow::DefaultSize()
{
	return FS_MONO;
}

/* virtual */ std::optional<std::string_view> TextfileWindow::NextString()
{
	if (this->search_iterator >= this->lines.size()) return std::nullopt;

	return this->lines[this->search_iterator++].text;
}

/* virtual */ bool TextfileWindow::Monospace()
{
	return true;
}

/* virtual */ void TextfileWindow::SetFontNames([[maybe_unused]] FontCacheSettings *settings, [[maybe_unused]] const char *font_name, [[maybe_unused]] const void *os_data)
{
#if defined(WITH_FREETYPE) || defined(_WIN32) || defined(WITH_COCOA)
	settings->mono.font = font_name;
	settings->mono.os_handle = os_data;
#endif
}

#if defined(WITH_ZLIB)

/**
 * Do an in-memory gunzip operation. This works on a raw deflate stream,
 * or a file with gzip or zlib header.
 * @param bufp  A pointer to a buffer containing the input data. This
 *              buffer will be freed and replaced by a buffer containing
 *              the uncompressed data.
 * @param sizep A pointer to the buffer size. Before the call, the value
 *              pointed to should contain the size of the input buffer.
 *              After the call, it contains the size of the uncompressed
 *              data.
 *
 * When decompressing fails, *bufp is set to nullptr and *sizep to 0. The
 * compressed buffer passed in is still freed in this case.
 */
static void Gunzip(byte **bufp, size_t *sizep)
{
	static const int BLOCKSIZE  = 8192;
	byte             *buf       = nullptr;
	size_t           alloc_size = 0;
	z_stream         z;
	int              res;

	memset(&z, 0, sizeof(z));
	z.next_in = *bufp;
	z.avail_in = (uInt)*sizep;

	/* window size = 15, add 32 to enable gzip or zlib header processing */
	res = inflateInit2(&z, 15 + 32);
	/* Z_BUF_ERROR just means we need more space */
	while (res == Z_OK || (res == Z_BUF_ERROR && z.avail_out == 0)) {
		/* When we get here, we're either just starting, or
		 * inflate is out of output space - allocate more */
		alloc_size += BLOCKSIZE;
		z.avail_out += BLOCKSIZE;
		buf = ReallocT(buf, alloc_size);
		z.next_out = buf + alloc_size - z.avail_out;
		res = inflate(&z, Z_FINISH);
	}

	free(*bufp);
	inflateEnd(&z);

	if (res == Z_STREAM_END) {
		*bufp = buf;
		*sizep = alloc_size - z.avail_out;
	} else {
		/* Something went wrong */
		*bufp = nullptr;
		*sizep = 0;
		free(buf);
	}
}
#endif

#if defined(WITH_LIBLZMA)

/**
 * Do an in-memory xunzip operation. This works on a .xz or (legacy)
 * .lzma file.
 * @param bufp  A pointer to a buffer containing the input data. This
 *              buffer will be freed and replaced by a buffer containing
 *              the uncompressed data.
 * @param sizep A pointer to the buffer size. Before the call, the value
 *              pointed to should contain the size of the input buffer.
 *              After the call, it contains the size of the uncompressed
 *              data.
 *
 * When decompressing fails, *bufp is set to nullptr and *sizep to 0. The
 * compressed buffer passed in is still freed in this case.
 */
static void Xunzip(byte **bufp, size_t *sizep)
{
	static const int BLOCKSIZE  = 8192;
	byte             *buf       = nullptr;
	size_t           alloc_size = 0;
	lzma_stream      z = LZMA_STREAM_INIT;
	int              res;

	z.next_in = *bufp;
	z.avail_in = *sizep;

	res = lzma_auto_decoder(&z, UINT64_MAX, LZMA_CONCATENATED);
	/* Z_BUF_ERROR just means we need more space */
	while (res == LZMA_OK || (res == LZMA_BUF_ERROR && z.avail_out == 0)) {
		/* When we get here, we're either just starting, or
		 * inflate is out of output space - allocate more */
		alloc_size += BLOCKSIZE;
		z.avail_out += BLOCKSIZE;
		buf = ReallocT(buf, alloc_size);
		z.next_out = buf + alloc_size - z.avail_out;
		res = lzma_code(&z, LZMA_FINISH);
	}

	free(*bufp);
	lzma_end(&z);

	if (res == LZMA_STREAM_END) {
		*bufp = buf;
		*sizep = alloc_size - z.avail_out;
	} else {
		/* Something went wrong */
		*bufp = nullptr;
		*sizep = 0;
		free(buf);
	}
}
#endif


/**
 * Loads the textfile text from file and setup #lines.
 */
/* virtual */ void TextfileWindow::LoadTextfile(const std::string &textfile, Subdirectory dir)
{
	this->lines.clear();
	this->jumplist.clear();

	if (this->GetWidget<NWidgetStacked>(WID_TF_SEL_JUMPLIST)->SetDisplayedPlane(SZSP_HORIZONTAL)) this->ReInit();

	if (textfile.empty()) return;

	/* Get text from file */
	size_t filesize;
	FILE *handle = FioFOpenFile(textfile, "rb", dir, &filesize);
	if (handle == nullptr) return;
	/* Early return on empty files. */
	if (filesize == 0) return;

	char *buf = MallocT<char>(filesize);
	size_t read = fread(buf, 1, filesize, handle);
	fclose(handle);

	if (read != filesize) {
		free(buf);
		return;
	}

#if defined(WITH_ZLIB)
	/* In-place gunzip */
	if (StrEndsWith(textfile, ".gz")) Gunzip((byte**)&buf, &filesize);
#endif

#if defined(WITH_LIBLZMA)
	/* In-place xunzip */
	if (StrEndsWith(textfile, ".xz")) Xunzip((byte**)&buf, &filesize);
#endif

	if (buf == nullptr) return;

	std::string_view sv_buf(buf, filesize);

	/* Check for the byte-order-mark, and skip it if needed. */
	if (StrStartsWith(sv_buf, u8"\ufeff")) sv_buf.remove_prefix(3);

	/* Update the filename. */
	this->filepath = textfile;
	this->filename = this->filepath.substr(this->filepath.find_last_of(PATHSEP) + 1);
	/* If it's the first file being loaded, add to history. */
	if (this->history.empty()) this->history.push_back(HistoryEntry{ this->filepath, 0 });

	/* Process the loaded text into lines, and do any further parsing needed. */
	this->LoadText(sv_buf);
	free(buf);
}

/**
 * Load a text into the textfile viewer.
 *
 * This will split the text into newlines and stores it for fast drawing.
 *
 * @param buf The text to load.
 */
void TextfileWindow::LoadText(std::string_view buf)
{
	std::string text = StrMakeValid(buf, SVS_REPLACE_WITH_QUESTION_MARK | SVS_ALLOW_NEWLINE | SVS_REPLACE_TAB_CR_NL_WITH_SPACE);
	this->lines.clear();

	/* Split the string on newlines. */
	std::string_view p(text);
	int row = 0;
	auto next = p.find_first_of('\n');
	while (next != std::string_view::npos) {
		this->lines.emplace_back(row, p.substr(0, next));
		p.remove_prefix(next + 1);

		row++;
		next = p.find_first_of('\n');
	}
	this->lines.emplace_back(row, p);

	/* Calculate maximum text line length. */
	uint max_length = 0;
	for (auto &line : this->lines) {
		max_length = std::max(max_length, GetStringBoundingBox(line.text, FS_MONO).width);
	}
	this->max_length = max_length;

	this->AfterLoadText();

	CheckForMissingGlyphs(true, this);
}

/**
 * Search a textfile file next to the given content.
 * @param type The type of the textfile to search for.
 * @param dir The subdirectory to search in.
 * @param filename The filename of the content to look for.
 * @return The path to the textfile, \c nullptr otherwise.
 */
std::optional<std::string> GetTextfile(TextfileType type, Subdirectory dir, const std::string &filename)
{
	static const char * const prefixes[] = {
		"readme",
		"changelog",
		"license",
	};
	static_assert(lengthof(prefixes) == TFT_CONTENT_END);

	/* Only the generic text file types allowed for this function */
	if (type >= TFT_CONTENT_END) return std::nullopt;

	std::string_view prefix = prefixes[type];

	if (filename.empty()) return std::nullopt;

	auto slash = filename.find_last_of(PATHSEPCHAR);
	if (slash == std::string::npos) return std::nullopt;

	std::string_view base_path(filename.data(), slash + 1);

	static const std::initializer_list<std::string_view> extensions{
		"txt",
		"md",
#if defined(WITH_ZLIB)
		"txt.gz",
		"md.gz",
#endif
#if defined(WITH_LIBLZMA)
		"txt.xz",
		"md.xz",
#endif
	};

	for (auto &extension : extensions) {
		std::string file_path = fmt::format("{}{}_{}.{}", base_path, prefix, GetCurrentLanguageIsoCode(), extension);
		if (FioCheckFileExists(file_path, dir)) return file_path;

		file_path = fmt::format("{}{}_{:.2s}.{}", base_path, prefix, GetCurrentLanguageIsoCode(), extension);
		if (FioCheckFileExists(file_path, dir)) return file_path;

		file_path = fmt::format("{}{}.{}", base_path, prefix, extension);
		if (FioCheckFileExists(file_path, dir)) return file_path;
	}
	return std::nullopt;
}
