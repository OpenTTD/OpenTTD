/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file textfile_gui.cpp Implementation of textfile window. */

#include "stdafx.h"
#include "fileio_func.h"
#include "fontcache.h"
#include "gfx_type.h"
#include "gfx_func.h"
#include "string_func.h"
#include "textfile_gui.h"

#include "widgets/misc_widget.h"

#include "table/strings.h"


/** Widgets for the textfile window. */
static const NWidgetPart _nested_textfile_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE, WID_TF_CAPTION), SetDataTip(STR_NULL, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
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
static const WindowDesc _textfile_desc(
	WDP_CENTER, 630, 460,
	WC_TEXTFILE, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_textfile_widgets, lengthof(_nested_textfile_widgets)
);

TextfileWindow::TextfileWindow(TextfileType file_type) : Window(), file_type(file_type)
{
	this->CreateNestedTree(&_textfile_desc);
	this->vscroll = this->GetScrollbar(WID_TF_VSCROLLBAR);
	this->hscroll = this->GetScrollbar(WID_TF_HSCROLLBAR);
	this->FinishInitNested(&_textfile_desc);
}

/* virtual */ TextfileWindow::~TextfileWindow()
{
	free(this->text);
}

/* virtual */ void TextfileWindow::UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
{
	switch (widget) {
		case WID_TF_BACKGROUND:
			this->line_height = FONT_HEIGHT_MONO + 2;
			resize->height = this->line_height;

			size->height = 4 * resize->height + TOP_SPACING + BOTTOM_SPACING; // At least 4 lines are visible.
			size->width = max(200u, size->width); // At least 200 pixels wide.
			break;
	}
}

/* virtual */ void TextfileWindow::DrawWidget(const Rect &r, int widget) const
{
	if (widget != WID_TF_BACKGROUND) return;

	int width = r.right - r.left + 1 - WD_BEVEL_LEFT - WD_BEVEL_RIGHT;
	int height = r.bottom - r.top + 1 - WD_BEVEL_LEFT - WD_BEVEL_RIGHT;

	DrawPixelInfo new_dpi;
	if (!FillDrawPixelInfo(&new_dpi, r.left + WD_BEVEL_LEFT, r.top, width, height)) return;
	DrawPixelInfo *old_dpi = _cur_dpi;
	_cur_dpi = &new_dpi;

	int left, right;
	if (_current_text_dir == TD_RTL) {
		left = width + WD_BEVEL_RIGHT - WD_FRAMETEXT_RIGHT - this->hscroll->GetCount();
		right = width + WD_BEVEL_RIGHT - WD_FRAMETEXT_RIGHT - 1 + this->hscroll->GetPosition();
	} else {
		left = WD_FRAMETEXT_LEFT - WD_BEVEL_LEFT - this->hscroll->GetPosition();
		right = WD_FRAMETEXT_LEFT - WD_BEVEL_LEFT + this->hscroll->GetCount() - 1;
	}
	int top = TOP_SPACING;
	for (uint i = 0; i < this->vscroll->GetCapacity() && i + this->vscroll->GetPosition() < this->lines.Length(); i++) {
		DrawString(left, right, top + i * this->line_height, this->lines[i + this->vscroll->GetPosition()], TC_WHITE, SA_LEFT, false, FS_MONO);
	}

	_cur_dpi = old_dpi;
}

/* virtual */ void TextfileWindow::OnResize()
{
	this->vscroll->SetCapacityFromWidget(this, WID_TF_BACKGROUND, TOP_SPACING + BOTTOM_SPACING);
	this->hscroll->SetCapacityFromWidget(this, WID_TF_BACKGROUND);
}

/* virtual */ void TextfileWindow::Reset()
{
	this->search_iterator = 0;
}

/* virtual */ FontSize TextfileWindow::DefaultSize()
{
	return FS_MONO;
}

/* virtual */ const char *TextfileWindow::NextString()
{
	if (this->search_iterator >= this->lines.Length()) return NULL;

	return this->lines[this->search_iterator++];
}

/* virtual */ bool TextfileWindow::Monospace()
{
	return true;
}

/* virtual */ void TextfileWindow::SetFontNames(FreeTypeSettings *settings, const char *font_name)
{
#ifdef WITH_FREETYPE
	strecpy(settings->mono_font, font_name, lastof(settings->mono_font));
#endif /* WITH_FREETYPE */
}

/**
 * Loads the textfile text from file, and setup #lines, #max_length, and both scrollbars.
 */
/* virtual */ void TextfileWindow::LoadTextfile(const char *textfile, Subdirectory dir)
{
	if (textfile == NULL) return;

	this->lines.Clear();

	/* Get text from file */
	size_t filesize;
	FILE *handle = FioFOpenFile(textfile, "rb", dir, &filesize);
	if (handle == NULL) return;

	this->text = ReallocT(this->text, filesize + 1);
	size_t read = fread(this->text, 1, filesize, handle);
	fclose(handle);

	if (read != filesize) return;

	this->text[filesize] = '\0';

	/* Replace tabs and line feeds with a space since str_validate removes those. */
	for (char *p = this->text; *p != '\0'; p++) {
		if (*p == '\t' || *p == '\r') *p = ' ';
	}

	/* Check for the byte-order-mark, and skip it if needed. */
	char *p = this->text + (strncmp("\xEF\xBB\xBF", this->text, 3) == 0 ? 3 : 0);

	/* Make sure the string is a valid UTF-8 sequence. */
	str_validate(p, this->text + filesize, SVS_REPLACE_WITH_QUESTION_MARK | SVS_ALLOW_NEWLINE);

	/* Split the string on newlines. */
	*this->lines.Append() = p;
	for (; *p != '\0'; p++) {
		if (*p == '\n') {
			*p = '\0';
			*this->lines.Append() = p + 1;
		}
	}

	CheckForMissingGlyphs(true, this);

	/* Initialize scrollbars */
	this->vscroll->SetCount(this->lines.Length());

	this->max_length = 0;
	for (uint i = 0; i < this->lines.Length(); i++) {
		this->max_length = max(this->max_length, GetStringBoundingBox(this->lines[i], FS_MONO).width);
	}
	this->hscroll->SetCount(this->max_length + WD_FRAMETEXT_LEFT + WD_FRAMETEXT_RIGHT);
	this->hscroll->SetStepSize(10); // Speed up horizontal scrollbar
}

/**
 * Search a textfile file next to the given content.
 * @param type The type of the textfile to search for.
 * @param dir The subdirectory to search in.
 * @param filename The filename of the content to look for.
 * @return The path to the textfile, \c NULL otherwise.
 */
const char *GetTextfile(TextfileType type, Subdirectory dir, const char *filename)
{
	static const char * const prefixes[] = {
		"readme",
		"changelog",
		"license",
	};
	assert_compile(lengthof(prefixes) == TFT_END);

	const char *prefix = prefixes[type];

	if (filename == NULL) return NULL;

	static char file_path[MAX_PATH];
	strecpy(file_path, filename, lastof(file_path));

	char *slash = strrchr(file_path, PATHSEPCHAR);
	if (slash == NULL) return NULL;

	seprintf(slash + 1, lastof(file_path), "%s_%s.txt", prefix, GetCurrentLanguageIsoCode());
	if (FioCheckFileExists(file_path, dir)) return file_path;

	seprintf(slash + 1, lastof(file_path), "%s_%.2s.txt", prefix, GetCurrentLanguageIsoCode());
	if (FioCheckFileExists(file_path, dir)) return file_path;

	seprintf(slash + 1, lastof(file_path), "%s.txt", prefix);
	return FioCheckFileExists(file_path, dir) ? file_path : NULL;
}
