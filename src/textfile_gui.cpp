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

#include "widgets/misc_widget.h"

#include "table/strings.h"

#if defined(WITH_ZLIB)
#include <zlib.h>
#endif

#if defined(WITH_LIBLZMA)
#include <lzma.h>
#endif

#include "safeguards.h"

/** Widgets for the textfile window. */
static const NWidgetPart _nested_textfile_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE, WID_TF_CAPTION), SetDataTip(STR_NULL, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_TEXTBTN, COLOUR_MAUVE, WID_TF_WRAPTEXT), SetDataTip(STR_TEXTFILE_WRAP_TEXT, STR_TEXTFILE_WRAP_TEXT_TOOLTIP),
		NWidget(WWT_DEFSIZEBOX, COLOUR_MAUVE),
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
static WindowDesc _textfile_desc(
	WDP_CENTER, "textfile", 630, 460,
	WC_TEXTFILE, WC_NONE,
	0,
	_nested_textfile_widgets, lengthof(_nested_textfile_widgets)
);

TextfileWindow::TextfileWindow(TextfileType file_type) : Window(&_textfile_desc), file_type(file_type)
{
	this->CreateNestedTree();
	this->vscroll = this->GetScrollbar(WID_TF_VSCROLLBAR);
	this->hscroll = this->GetScrollbar(WID_TF_HSCROLLBAR);
	this->FinishInitNested(file_type);
	this->GetWidget<NWidgetCore>(WID_TF_CAPTION)->SetDataTip(STR_TEXTFILE_README_CAPTION + file_type, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS);

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
			height += GetStringHeight(line.text, max_width, FS_MONO) / FONT_HEIGHT_MONO;
			line.bottom = height;
		}
	}

	return height;
}

uint TextfileWindow::GetContentHeight()
{
	if (this->lines.size() == 0) return 0;
	return this->lines.back().bottom;
}

/* virtual */ void TextfileWindow::UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
{
	switch (widget) {
		case WID_TF_BACKGROUND:
			resize->height = FONT_HEIGHT_MONO;

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

/* virtual */ void TextfileWindow::OnClick(Point pt, int widget, int click_count)
{
	switch (widget) {
		case WID_TF_WRAPTEXT:
			this->ToggleWidgetLoweredState(WID_TF_WRAPTEXT);
			this->InvalidateData();
			break;
	}
}

/* virtual */ void TextfileWindow::DrawWidget(const Rect &r, int widget) const
{
	if (widget != WID_TF_BACKGROUND) return;

	Rect fr = r.Shrink(WidgetDimensions::scaled.frametext);

	DrawPixelInfo new_dpi;
	if (!FillDrawPixelInfo(&new_dpi, fr.left, fr.top, fr.Width(), fr.Height())) return;
	AutoRestoreBackup dpi_backup(_cur_dpi, &new_dpi);

	/* Draw content (now coordinates given to DrawString* are local to the new clipping region). */
	fr = fr.Translate(-fr.left, -fr.top);
	int line_height = FONT_HEIGHT_MONO;
	int pos = this->vscroll->GetPosition();
	int cap = this->vscroll->GetCapacity();

	for (auto &line : this->lines) {
		if (line.bottom < pos) continue;
		if (line.top > pos + cap) break;

		int y_offset = (line.top - pos) * line_height;
		if (IsWidgetLowered(WID_TF_WRAPTEXT)) {
			DrawStringMultiLine(0, fr.right, y_offset, fr.bottom, line.text, TC_BLACK, SA_TOP | SA_LEFT, false, FS_MONO);
		} else {
			DrawString(-this->hscroll->GetPosition(), fr.right, y_offset, line.text, TC_BLACK, SA_TOP | SA_LEFT, false, FS_MONO);
		}
	}
}

/* virtual */ void TextfileWindow::OnResize()
{
	this->vscroll->SetCapacityFromWidget(this, WID_TF_BACKGROUND, WidgetDimensions::scaled.frametext.Vertical());
	this->hscroll->SetCapacityFromWidget(this, WID_TF_BACKGROUND);

	this->SetupScrollbars(false);
}

/* virtual */ void TextfileWindow::OnInvalidateData(int data, bool gui_scope)
{
	if (!gui_scope) return;

	this->SetupScrollbars(true);
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

/* virtual */ void TextfileWindow::SetFontNames(FontCacheSettings *settings, const char *font_name, const void *os_data)
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
	if (textfile.empty()) return;

	this->lines.clear();

	/* Get text from file */
	size_t filesize;
	FILE *handle = FioFOpenFile(textfile, "rb", dir, &filesize);
	if (handle == nullptr) return;

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

	/* Replace any invalid characters with a question-mark. This copies the buf in the process. */
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
	this->text = StrMakeValid(buf, SVS_REPLACE_WITH_QUESTION_MARK | SVS_ALLOW_NEWLINE | SVS_REPLACE_TAB_CR_NL_WITH_SPACE);
	this->lines.clear();

	/* Split the string on newlines. */
	std::string_view p(this->text);
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

	std::string_view prefix = prefixes[type];

	if (filename.empty()) return std::nullopt;

	auto slash = filename.find_last_of(PATHSEPCHAR);
	if (slash == std::string::npos) return std::nullopt;

	std::string_view base_path(filename.data(), slash + 1);

	static const std::initializer_list<std::string_view> extensions{
		"txt",
#if defined(WITH_ZLIB)
		"txt.gz",
#endif
#if defined(WITH_LIBLZMA)
		"txt.xz",
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
