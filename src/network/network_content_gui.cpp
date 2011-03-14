/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_content_gui.cpp Implementation of the Network Content related GUIs. */

#if defined(ENABLE_NETWORK)
#include "../stdafx.h"
#include "../strings_func.h"
#include "../gfx_func.h"
#include "../window_func.h"
#include "../gui.h"
#include "../ai/ai.hpp"
#include "../base_media_base.h"
#include "../sortlist_type.h"
#include "../querystring_gui.h"
#include "../core/geometry_func.hpp"
#include  "network_content.h"

#include "table/strings.h"
#include "../table/sprites.h"

/** Widgets used by this window */
enum DownloadStatusWindowWidgets {
	NCDSWW_BACKGROUND, ///< Background
	NCDSWW_CANCELOK,   ///< Cancel/OK button
};

/** Nested widgets for the download window. */
static const NWidgetPart _nested_network_content_download_status_window_widgets[] = {
	NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_CONTENT_DOWNLOAD_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_GREY, NCDSWW_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(350, 0), SetMinimalTextLines(3, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM + 30),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(125, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NCDSWW_CANCELOK), SetMinimalSize(101, 12), SetDataTip(STR_BUTTON_CANCEL, STR_NULL),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 4),
	EndContainer(),
};

/** Window description for the download window */
static const WindowDesc _network_content_download_status_window_desc(
	WDP_CENTER, 0, 0,
	WC_NETWORK_STATUS_WINDOW, WC_NONE,
	WDF_MODAL,
	_nested_network_content_download_status_window_widgets, lengthof(_nested_network_content_download_status_window_widgets)
);

/** Window for showing the download status of content */
struct NetworkContentDownloadStatusWindow : public Window, ContentCallback {
private:
	ClientNetworkContentSocketHandler *connection; ///< Our connection with the content server
	SmallVector<ContentType, 4> receivedTypes;     ///< Types we received so we can update their cache

	uint total_files;      ///< Number of files to download
	uint downloaded_files; ///< Number of files downloaded
	uint total_bytes;      ///< Number of bytes to download
	uint downloaded_bytes; ///< Number of bytes downloaded

	uint32 cur_id; ///< The current ID of the downloaded file
	char name[48]; ///< The current name of the downloaded file

public:
	/**
	 * Create a new download window based on a list of content information
	 * with flags whether to download them or not.
	 * @param infos the list to search in
	 */
	NetworkContentDownloadStatusWindow() :
		cur_id(UINT32_MAX)
	{
		this->parent = FindWindowById(WC_NETWORK_WINDOW, 1);

		_network_content_client.AddCallback(this);
		_network_content_client.DownloadSelectedContent(this->total_files, this->total_bytes);

		this->InitNested(&_network_content_download_status_window_desc, 0);
	}

	/** Free whatever we've allocated */
	~NetworkContentDownloadStatusWindow()
	{
		TarScanner::DoScan();

		/* Tell all the backends about what we've downloaded */
		for (ContentType *iter = this->receivedTypes.Begin(); iter != this->receivedTypes.End(); iter++) {
			switch (*iter) {
				case CONTENT_TYPE_AI:
				case CONTENT_TYPE_AI_LIBRARY:
					AI::Rescan();
					SetWindowClassesDirty(WC_AI_DEBUG);
					InvalidateWindowData(WC_AI_LIST, 0, 1);
					break;

				case CONTENT_TYPE_BASE_GRAPHICS:
					BaseGraphics::FindSets();
					SetWindowDirty(WC_GAME_OPTIONS, 0);
					break;

				case CONTENT_TYPE_BASE_SOUNDS:
					BaseSounds::FindSets();
					SetWindowDirty(WC_GAME_OPTIONS, 0);
					break;

				case CONTENT_TYPE_BASE_MUSIC:
					BaseMusic::FindSets();
					SetWindowDirty(WC_GAME_OPTIONS, 0);
					break;

				case CONTENT_TYPE_NEWGRF:
					ScanNewGRFFiles();
					/* Yes... these are the NewGRF windows */
					InvalidateWindowClassesData(WC_SAVELOAD);
					InvalidateWindowData(WC_GAME_OPTIONS, 0, 1);
					break;

				case CONTENT_TYPE_SCENARIO:
				case CONTENT_TYPE_HEIGHTMAP:
					extern void ScanScenarios();
					ScanScenarios();
					InvalidateWindowData(WC_SAVELOAD, 0, 0);
					break;

				default:
					break;
			}
		}

		/* Always invalidate the download window; tell it we are going to be gone */
		InvalidateWindowData(WC_NETWORK_WINDOW, 1, 2);
		_network_content_client.RemoveCallback(this);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != NCDSWW_BACKGROUND) return;

		/* Draw nice progress bar :) */
		DrawFrameRect(r.left + 20, r.top + 4, r.left + 20 + (int)((this->width - 40LL) * this->downloaded_bytes / this->total_bytes), r.top + 14, COLOUR_MAUVE, FR_NONE);

		int y = r.top + 20;
		SetDParam(0, this->downloaded_bytes);
		SetDParam(1, this->total_bytes);
		SetDParam(2, this->downloaded_bytes * 100LL / this->total_bytes);
		DrawString(r.left + 2, r.right - 2, y, STR_CONTENT_DOWNLOAD_PROGRESS_SIZE, TC_FROMSTRING, SA_HOR_CENTER);

		StringID str;
		if (this->downloaded_bytes == this->total_bytes) {
			str = STR_CONTENT_DOWNLOAD_COMPLETE;
		} else if (!StrEmpty(this->name)) {
			SetDParamStr(0, this->name);
			SetDParam(1, this->downloaded_files);
			SetDParam(2, this->total_files);
			str = STR_CONTENT_DOWNLOAD_FILE;
		} else {
			str = STR_CONTENT_DOWNLOAD_INITIALISE;
		}

		y += FONT_HEIGHT_NORMAL + 5;
		DrawStringMultiLine(r.left + 2, r.right - 2, y, y + FONT_HEIGHT_NORMAL * 2, str, TC_FROMSTRING, SA_CENTER);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		if (widget == NCDSWW_CANCELOK) {
			if (this->downloaded_bytes != this->total_bytes) _network_content_client.Close();
			delete this;
		}
	}

	virtual void OnDownloadProgress(const ContentInfo *ci, int bytes)
	{
		if (ci->id != this->cur_id) {
			strecpy(this->name, ci->filename, lastof(this->name));
			this->cur_id = ci->id;
			this->downloaded_files++;
			this->receivedTypes.Include(ci->type);
		}
		this->downloaded_bytes += bytes;

		/* When downloading is finished change cancel in ok */
		if (this->downloaded_bytes == this->total_bytes) {
			this->GetWidget<NWidgetCore>(NCDSWW_CANCELOK)->widget_data = STR_BUTTON_OK;
		}

		this->SetDirty();
	}
};

/** Widgets of the content list window. */
enum NetworkContentListWindowWidgets {
	NCLWW_BACKGROUND,    ///< Resize button

	NCLWW_FILTER_CAPT,   ///< Caption for the filter editbox
	NCLWW_FILTER,        ///< Filter editbox

	NCLWW_CHECKBOX,      ///< Button above checkboxes
	NCLWW_TYPE,          ///< 'Type' button
	NCLWW_NAME,          ///< 'Name' button

	NCLWW_MATRIX,        ///< Panel with list of content
	NCLWW_SCROLLBAR,     ///< Scrollbar of matrix

	NCLWW_DETAILS,       ///< Panel with content details

	NCLWW_SELECT_ALL,    ///< 'Select all' button
	NCLWW_SELECT_UPDATE, ///< 'Select updates' button
	NCLWW_UNSELECT,      ///< 'Unselect all' button
	NCLWW_CANCEL,        ///< 'Cancel' button
	NCLWW_DOWNLOAD,      ///< 'Download' button

	NCLWW_SEL_ALL_UPDATE, ///< #NWID_SELECTION widget for select all/update buttons.
};

/** Window that lists the content that's at the content server */
class NetworkContentListWindow : public QueryStringBaseWindow, ContentCallback {
	typedef GUIList<const ContentInfo*> GUIContentList;

	static const uint EDITBOX_MAX_SIZE   =  50;
	static const uint EDITBOX_MAX_LENGTH = 300;

	/** Runtime saved values */
	static Listing last_sorting;
	static Filtering last_filtering;
	/** The sorter functions */
	static GUIContentList::SortFunction * const sorter_funcs[];
	static GUIContentList::FilterFunction * const filter_funcs[];
	GUIContentList content;      ///< List with content

	const ContentInfo *selected; ///< The selected content info
	int list_pos;                ///< Our position in the list
	uint filesize_sum;           ///< The sum of all selected file sizes
	Scrollbar *vscroll;

	/**
	 * (Re)build the network game list as its amount has changed because
	 * an item has been added or deleted for example
	 */
	void BuildContentList()
	{
		if (!this->content.NeedRebuild()) return;

		/* Create temporary array of games to use for listing */
		this->content.Clear();

		for (ConstContentIterator iter = _network_content_client.Begin(); iter != _network_content_client.End(); iter++) {
			*this->content.Append() = *iter;
		}

		this->FilterContentList();
		this->content.Compact();
		this->content.RebuildDone();
		this->SortContentList();

		this->vscroll->SetCount(this->content.Length()); // Update the scrollbar
		this->ScrollToSelected();
	}

	/** Sort content by name. */
	static int CDECL NameSorter(const ContentInfo * const *a, const ContentInfo * const *b)
	{
		return strnatcmp((*a)->name, (*b)->name); // Sort by name (natural sorting).
	}

	/** Sort content by type. */
	static int CDECL TypeSorter(const ContentInfo * const *a, const ContentInfo * const *b)
	{
		int r = 0;
		if ((*a)->type != (*b)->type) {
			char a_str[64];
			char b_str[64];
			GetString(a_str, STR_CONTENT_TYPE_BASE_GRAPHICS + (*a)->type - CONTENT_TYPE_BASE_GRAPHICS, lastof(a_str));
			GetString(b_str, STR_CONTENT_TYPE_BASE_GRAPHICS + (*b)->type - CONTENT_TYPE_BASE_GRAPHICS, lastof(b_str));
			r = strnatcmp(a_str, b_str);
		}
		if (r == 0) r = NameSorter(a, b);
		return r;
	}

	/** Sort content by state. */
	static int CDECL StateSorter(const ContentInfo * const *a, const ContentInfo * const *b)
	{
		int r = (*a)->state - (*b)->state;
		if (r == 0) r = TypeSorter(a, b);
		return r;
	}

	/** Sort the content list */
	void SortContentList()
	{
		if (!this->content.Sort()) return;

		for (ConstContentIterator iter = this->content.Begin(); iter != this->content.End(); iter++) {
			if (*iter == this->selected) {
				this->list_pos = iter - this->content.Begin();
				break;
			}
		}
	}

	/** Filter content by tags/name */
	static bool CDECL TagNameFilter(const ContentInfo * const *a, const char *filter_string)
	{
		for (int i = 0; i < (*a)->tag_count; i++) {
			if (strcasestr((*a)->tags[i], filter_string) != NULL) return true;
		}
		return strcasestr((*a)->name, filter_string) != NULL;
	}

	/** Filter the content list */
	void FilterContentList()
	{
		if (!this->content.Filter(this->edit_str_buf)) return;

		/* update list position */
		for (ConstContentIterator iter = this->content.Begin(); iter != this->content.End(); iter++) {
			if (*iter == this->selected) {
				this->list_pos = iter - this->content.Begin();
				return;
			}
		}

		/* previously selected item not in list anymore */
		this->selected = NULL;
		this->list_pos = 0;
	}

	/** Make sure that the currently selected content info is within the visible part of the matrix */
	void ScrollToSelected()
	{
		if (this->selected == NULL) return;

		this->vscroll->ScrollTowards(this->list_pos);
	}

public:
	/**
	 * Create the content list window.
	 * @param desc the window description to pass to Window's constructor.
	 */
	NetworkContentListWindow(const WindowDesc *desc, bool select_all) :
			QueryStringBaseWindow(EDITBOX_MAX_SIZE),
			selected(NULL),
			list_pos(0)
	{
		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(NCLWW_SCROLLBAR);
		this->FinishInitNested(desc, 1);

		this->GetWidget<NWidgetStacked>(NCLWW_SEL_ALL_UPDATE)->SetDisplayedPlane(select_all);

		this->afilter = CS_ALPHANUMERAL;
		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, EDITBOX_MAX_LENGTH);
		this->SetFocusedWidget(NCLWW_FILTER);

		_network_content_client.AddCallback(this);
		this->content.SetListing(this->last_sorting);
		this->content.SetFiltering(this->last_filtering);
		this->content.SetSortFuncs(this->sorter_funcs);
		this->content.SetFilterFuncs(this->filter_funcs);
		this->content.ForceRebuild();
		this->FilterContentList();
		this->SortContentList();
		this->InvalidateData();
	}

	/** Free everything we allocated */
	~NetworkContentListWindow()
	{
		_network_content_client.RemoveCallback(this);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case NCLWW_FILTER_CAPT:
				*size = maxdim(*size, GetStringBoundingBox(STR_CONTENT_FILTER_TITLE));
				break;

			case NCLWW_TYPE: {
				Dimension d = *size;
				for (int i = CONTENT_TYPE_BEGIN; i < CONTENT_TYPE_END; i++) {
					d = maxdim(d, GetStringBoundingBox(STR_CONTENT_TYPE_BASE_GRAPHICS + i - CONTENT_TYPE_BASE_GRAPHICS));
				}
				size->width = d.width + WD_MATRIX_RIGHT + WD_MATRIX_LEFT;
				break;
			}

			case NCLWW_MATRIX:
				resize->height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;
				size->height = 10 * resize->height;
				break;
		}
	}


	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case NCLWW_FILTER_CAPT:
				DrawString(r.left, r.right, r.top, STR_CONTENT_FILTER_TITLE, TC_FROMSTRING, SA_RIGHT);
				break;

			case NCLWW_DETAILS:
				this->DrawDetails(r);
				break;

			case NCLWW_MATRIX:
				this->DrawMatrix(r);
				break;
		}
	}

	virtual void OnPaint()
	{
		const SortButtonState arrow = this->content.IsDescSortOrder() ? SBS_DOWN : SBS_UP;

		if (this->content.NeedRebuild()) {
			this->BuildContentList();
		}

		this->DrawWidgets();

		/* Edit box to filter for keywords */
		this->DrawEditBox(NCLWW_FILTER);

		switch (this->content.SortType()) {
			case NCLWW_CHECKBOX - NCLWW_CHECKBOX: this->DrawSortButtonState(NCLWW_CHECKBOX, arrow); break;
			case NCLWW_TYPE     - NCLWW_CHECKBOX: this->DrawSortButtonState(NCLWW_TYPE,     arrow); break;
			case NCLWW_NAME     - NCLWW_CHECKBOX: this->DrawSortButtonState(NCLWW_NAME,     arrow); break;
		}
	}

	void DrawMatrix(const Rect &r) const
	{
		const NWidgetBase *nwi_checkbox = this->GetWidget<NWidgetBase>(NCLWW_CHECKBOX);
		const NWidgetBase *nwi_name = this->GetWidget<NWidgetBase>(NCLWW_NAME);
		const NWidgetBase *nwi_type = this->GetWidget<NWidgetBase>(NCLWW_TYPE);


		/* Fill the matrix with the information */
		int sprite_y_offset = WD_MATRIX_TOP + (FONT_HEIGHT_NORMAL - 10) / 2;
		uint y = r.top;
		int cnt = 0;
		for (ConstContentIterator iter = this->content.Get(this->vscroll->GetPosition()); iter != this->content.End() && cnt < this->vscroll->GetCapacity(); iter++, cnt++) {
			const ContentInfo *ci = *iter;

			if (ci == this->selected) GfxFillRect(r.left + 1, y + 1, r.right - 1, y + this->resize.step_height - 1, 10);

			SpriteID sprite;
			SpriteID pal = PAL_NONE;
			switch (ci->state) {
				case ContentInfo::UNSELECTED:     sprite = SPR_BOX_EMPTY;   break;
				case ContentInfo::SELECTED:       sprite = SPR_BOX_CHECKED; break;
				case ContentInfo::AUTOSELECTED:   sprite = SPR_BOX_CHECKED; break;
				case ContentInfo::ALREADY_HERE:   sprite = SPR_BLOT; pal = PALETTE_TO_GREEN; break;
				case ContentInfo::DOES_NOT_EXIST: sprite = SPR_BLOT; pal = PALETTE_TO_RED;   break;
				default: NOT_REACHED();
			}
			DrawSprite(sprite, pal, nwi_checkbox->pos_x + (pal == PAL_NONE ? 2 : 3), y + sprite_y_offset + (pal == PAL_NONE ? 1 : 0));

			StringID str = STR_CONTENT_TYPE_BASE_GRAPHICS + ci->type - CONTENT_TYPE_BASE_GRAPHICS;
			DrawString(nwi_type->pos_x, nwi_type->pos_x + nwi_type->current_x - 1, y + WD_MATRIX_TOP, str, TC_BLACK, SA_HOR_CENTER);

			DrawString(nwi_name->pos_x + WD_FRAMERECT_LEFT, nwi_name->pos_x + nwi_name->current_x - WD_FRAMERECT_RIGHT, y + WD_MATRIX_TOP, ci->name, TC_BLACK);
			y += this->resize.step_height;
		}
	}

	/**
	 * Helper function to draw the details part of this window.
	 * @param r the rectangle to stay within while drawing
	 */
	void DrawDetails(const Rect &r) const
	{
		static const int DETAIL_LEFT         =  5; ///< Number of pixels at the left
		static const int DETAIL_RIGHT        =  5; ///< Number of pixels at the right
		static const int DETAIL_TOP          =  5; ///< Number of pixels at the top

		/* Height for the title banner */
		int DETAIL_TITLE_HEIGHT = 5 * FONT_HEIGHT_NORMAL;

		/* Create the nice grayish rectangle at the details top */
		GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.top + DETAIL_TITLE_HEIGHT, 157);
		DrawString(r.left + WD_INSET_LEFT, r.right - WD_INSET_RIGHT, r.top + FONT_HEIGHT_NORMAL + WD_INSET_TOP, STR_CONTENT_DETAIL_TITLE, TC_FROMSTRING, SA_HOR_CENTER);

		/* Draw the total download size */
		SetDParam(0, this->filesize_sum);
		DrawString(r.left + DETAIL_LEFT, r.right - DETAIL_RIGHT, r.bottom - FONT_HEIGHT_NORMAL - WD_PAR_VSEP_NORMAL, STR_CONTENT_TOTAL_DOWNLOAD_SIZE);

		if (this->selected == NULL) return;

		/* And fill the rest of the details when there's information to place there */
		DrawStringMultiLine(r.left + WD_INSET_LEFT, r.right - WD_INSET_RIGHT, r.top + DETAIL_TITLE_HEIGHT / 2, r.top + DETAIL_TITLE_HEIGHT, STR_CONTENT_DETAIL_SUBTITLE_UNSELECTED + this->selected->state, TC_FROMSTRING, SA_CENTER);

		/* Also show the total download size, so keep some space from the bottom */
		const uint max_y = r.bottom - FONT_HEIGHT_NORMAL - WD_PAR_VSEP_WIDE;
		int y = r.top + DETAIL_TITLE_HEIGHT + DETAIL_TOP;

		if (this->selected->upgrade) {
			SetDParam(0, STR_CONTENT_TYPE_BASE_GRAPHICS + this->selected->type - CONTENT_TYPE_BASE_GRAPHICS);
			y = DrawStringMultiLine(r.left + DETAIL_LEFT, r.right - DETAIL_RIGHT, y, max_y, STR_CONTENT_DETAIL_UPDATE);
			y += WD_PAR_VSEP_WIDE;
		}

		SetDParamStr(0, this->selected->name);
		y = DrawStringMultiLine(r.left + DETAIL_LEFT, r.right - DETAIL_RIGHT, y, max_y, STR_CONTENT_DETAIL_NAME);

		if (!StrEmpty(this->selected->version)) {
			SetDParamStr(0, this->selected->version);
			y = DrawStringMultiLine(r.left + DETAIL_LEFT, r.right - DETAIL_RIGHT, y, max_y, STR_CONTENT_DETAIL_VERSION);
		}

		if (!StrEmpty(this->selected->description)) {
			SetDParamStr(0, this->selected->description);
			y = DrawStringMultiLine(r.left + DETAIL_LEFT, r.right - DETAIL_RIGHT, y, max_y, STR_CONTENT_DETAIL_DESCRIPTION);
		}

		if (!StrEmpty(this->selected->url)) {
			SetDParamStr(0, this->selected->url);
			y = DrawStringMultiLine(r.left + DETAIL_LEFT, r.right - DETAIL_RIGHT, y, max_y, STR_CONTENT_DETAIL_URL);
		}

		SetDParam(0, STR_CONTENT_TYPE_BASE_GRAPHICS + this->selected->type - CONTENT_TYPE_BASE_GRAPHICS);
		y = DrawStringMultiLine(r.left + DETAIL_LEFT, r.right - DETAIL_RIGHT, y, max_y, STR_CONTENT_DETAIL_TYPE);

		y += WD_PAR_VSEP_WIDE;
		SetDParam(0, this->selected->filesize);
		y = DrawStringMultiLine(r.left + DETAIL_LEFT, r.right - DETAIL_RIGHT, y, max_y, STR_CONTENT_DETAIL_FILESIZE);

		if (this->selected->dependency_count != 0) {
			/* List dependencies */
			char buf[DRAW_STRING_BUFFER] = "";
			char *p = buf;
			for (uint i = 0; i < this->selected->dependency_count; i++) {
				ContentID cid = this->selected->dependencies[i];

				/* Try to find the dependency */
				ConstContentIterator iter = _network_content_client.Begin();
				for (; iter != _network_content_client.End(); iter++) {
					const ContentInfo *ci = *iter;
					if (ci->id != cid) continue;

					p += seprintf(p, lastof(buf), p == buf ? "%s" : ", %s", (*iter)->name);
					break;
				}
			}
			SetDParamStr(0, buf);
			y = DrawStringMultiLine(r.left + DETAIL_LEFT, r.right - DETAIL_RIGHT, y, max_y, STR_CONTENT_DETAIL_DEPENDENCIES);
		}

		if (this->selected->tag_count != 0) {
			/* List all tags */
			char buf[DRAW_STRING_BUFFER] = "";
			char *p = buf;
			for (uint i = 0; i < this->selected->tag_count; i++) {
				p += seprintf(p, lastof(buf), i == 0 ? "%s" : ", %s", this->selected->tags[i]);
			}
			SetDParamStr(0, buf);
			y = DrawStringMultiLine(r.left + DETAIL_LEFT, r.right - DETAIL_RIGHT, y, max_y, STR_CONTENT_DETAIL_TAGS);
		}

		if (this->selected->IsSelected()) {
			/* When selected show all manually selected content that depends on this */
			ConstContentVector tree;
			_network_content_client.ReverseLookupTreeDependency(tree, this->selected);

			char buf[DRAW_STRING_BUFFER] = "";
			char *p = buf;
			for (ConstContentIterator iter = tree.Begin(); iter != tree.End(); iter++) {
				const ContentInfo *ci = *iter;
				if (ci == this->selected || ci->state != ContentInfo::SELECTED) continue;

				p += seprintf(p, lastof(buf), buf == p ? "%s" : ", %s", ci->name);
			}
			if (p != buf) {
				SetDParamStr(0, buf);
				y = DrawStringMultiLine(r.left + DETAIL_LEFT, r.right - DETAIL_RIGHT, y, max_y, STR_CONTENT_DETAIL_SELECTED_BECAUSE_OF);
			}
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case NCLWW_MATRIX: {
				uint id_v = this->vscroll->GetScrolledRowFromWidget(pt.y, this, NCLWW_MATRIX);
				if (id_v >= this->content.Length()) return; // click out of bounds

				this->selected = *this->content.Get(id_v);
				this->list_pos = id_v;

				const NWidgetBase *checkbox = this->GetWidget<NWidgetBase>(NCLWW_CHECKBOX);
				if (click_count > 1 || IsInsideBS(pt.x, checkbox->pos_x, checkbox->current_x)) {
					_network_content_client.ToggleSelectedState(this->selected);
					this->content.ForceResort();
				}

				this->InvalidateData();
				break;
			}

			case NCLWW_CHECKBOX:
			case NCLWW_TYPE:
			case NCLWW_NAME:
				if (this->content.SortType() == widget - NCLWW_CHECKBOX) {
					this->content.ToggleSortOrder();
					this->list_pos = this->content.Length() - this->list_pos - 1;
				} else {
					this->content.SetSortType(widget - NCLWW_CHECKBOX);
					this->content.ForceResort();
					this->SortContentList();
				}
				this->ScrollToSelected();
				this->InvalidateData();
				break;

			case NCLWW_SELECT_ALL:
				_network_content_client.SelectAll();
				this->InvalidateData();
				break;

			case NCLWW_SELECT_UPDATE:
				_network_content_client.SelectUpgrade();
				this->InvalidateData();
				break;

			case NCLWW_UNSELECT:
				_network_content_client.UnselectAll();
				this->InvalidateData();
				break;

			case NCLWW_CANCEL:
				delete this;
				break;

			case NCLWW_DOWNLOAD:
				if (BringWindowToFrontById(WC_NETWORK_STATUS_WINDOW, 0) == NULL) new NetworkContentDownloadStatusWindow();
				break;
		}
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(NCLWW_FILTER);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		switch (keycode) {
			case WKC_UP:
				/* scroll up by one */
				if (this->list_pos > 0) this->list_pos--;
				break;
			case WKC_DOWN:
				/* scroll down by one */
				if (this->list_pos < (int)this->content.Length() - 1) this->list_pos++;
				break;
			case WKC_PAGEUP:
				/* scroll up a page */
				this->list_pos = (this->list_pos < this->vscroll->GetCapacity()) ? 0 : this->list_pos - this->vscroll->GetCapacity();
				break;
			case WKC_PAGEDOWN:
				/* scroll down a page */
				this->list_pos = min(this->list_pos + this->vscroll->GetCapacity(), (int)this->content.Length() - 1);
				break;
			case WKC_HOME:
				/* jump to beginning */
				this->list_pos = 0;
				break;
			case WKC_END:
				/* jump to end */
				this->list_pos = this->content.Length() - 1;
				break;

			case WKC_SPACE:
			case WKC_RETURN:
				if (keycode == WKC_RETURN || !IsWidgetFocused(NCLWW_FILTER)) {
					if (this->selected != NULL) {
						_network_content_client.ToggleSelectedState(this->selected);
						this->content.ForceResort();
						this->InvalidateData();
					}
					return ES_HANDLED;
				}
				/* FALL THROUGH, space is pressed and filter isn't focused. */

			default: {
				/* Handle editbox input */
				EventState state = ES_NOT_HANDLED;
				if (this->HandleEditBoxKey(NCLWW_FILTER, key, keycode, state) == HEBR_EDITING) {
					this->OnOSKInput(NCLWW_FILTER);
				}

				return state;
			}
		}

		if (_network_content_client.Length() == 0) return ES_HANDLED;

		this->selected = *this->content.Get(this->list_pos);

		/* scroll to the new server if it is outside the current range */
		this->ScrollToSelected();

		/* redraw window */
		this->InvalidateData();
		return ES_HANDLED;
	}

	virtual void OnOSKInput(int wid)
	{
		this->content.SetFilterState(!StrEmpty(this->edit_str_buf));
		this->content.ForceRebuild();
		this->InvalidateData();
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, NCLWW_MATRIX);
		this->GetWidget<NWidgetCore>(NCLWW_MATRIX)->widget_data = (this->vscroll->GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}

	virtual void OnReceiveContentInfo(const ContentInfo *rci)
	{
		this->content.ForceRebuild();
		this->InvalidateData();
	}

	virtual void OnDownloadComplete(ContentID cid)
	{
		this->content.ForceResort();
		this->InvalidateData();
	}

	virtual void OnConnect(bool success)
	{
		if (!success) {
			ShowErrorMessage(STR_CONTENT_ERROR_COULD_NOT_CONNECT, INVALID_STRING_ID, WL_ERROR);
			delete this;
			return;
		}

		this->InvalidateData();
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		if (this->content.NeedRebuild()) this->BuildContentList();

		/* To sum all the bytes we intend to download */
		this->filesize_sum = 0;
		bool show_select_all = false;
		bool show_select_upgrade = false;
		for (ConstContentIterator iter = this->content.Begin(); iter != this->content.End(); iter++) {
			const ContentInfo *ci = *iter;
			switch (ci->state) {
				case ContentInfo::SELECTED:
				case ContentInfo::AUTOSELECTED:
					this->filesize_sum += ci->filesize;
					break;

				case ContentInfo::UNSELECTED:
					show_select_all = true;
					show_select_upgrade |= ci->upgrade;
					break;

				default:
					break;
			}
		}

		/* If data == 2 then the status window caused this OnInvalidate */
		this->SetWidgetDisabledState(NCLWW_DOWNLOAD, this->filesize_sum == 0 || (FindWindowById(WC_NETWORK_STATUS_WINDOW, 0) != NULL && data != 2));
		this->SetWidgetDisabledState(NCLWW_UNSELECT, this->filesize_sum == 0);
		this->SetWidgetDisabledState(NCLWW_SELECT_ALL, !show_select_all);
		this->SetWidgetDisabledState(NCLWW_SELECT_UPDATE, !show_select_upgrade);

		this->GetWidget<NWidgetCore>(NCLWW_CANCEL)->widget_data = this->filesize_sum == 0 ? STR_AI_SETTINGS_CLOSE : STR_AI_LIST_CANCEL;
	}
};

Listing NetworkContentListWindow::last_sorting = {false, 1};
Filtering NetworkContentListWindow::last_filtering = {false, 0};

NetworkContentListWindow::GUIContentList::SortFunction * const NetworkContentListWindow::sorter_funcs[] = {
	&StateSorter,
	&TypeSorter,
	&NameSorter,
};

NetworkContentListWindow::GUIContentList::FilterFunction * const NetworkContentListWindow::filter_funcs[] = {
	&TagNameFilter,
};

static const NWidgetPart _nested_network_content_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE), SetDataTip(STR_CONTENT_TITLE, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, NCLWW_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 7), SetResize(1, 0),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(8, 8, 8),
			/* Top */
			NWidget(WWT_EMPTY, COLOUR_LIGHT_BLUE, NCLWW_FILTER_CAPT), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_EDITBOX, COLOUR_LIGHT_BLUE, NCLWW_FILTER), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 7), SetResize(1, 0),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(8, 8, 8),
			/* Left side. */
			NWidget(NWID_VERTICAL),
				NWidget(NWID_HORIZONTAL),
					NWidget(NWID_VERTICAL),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NCLWW_CHECKBOX), SetMinimalSize(13, 1), SetDataTip(STR_EMPTY, STR_NULL),
							NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NCLWW_TYPE),
											SetDataTip(STR_CONTENT_TYPE_CAPTION, STR_CONTENT_TYPE_CAPTION_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NCLWW_NAME), SetResize(1, 0), SetFill(1, 0),
											SetDataTip(STR_CONTENT_NAME_CAPTION, STR_CONTENT_NAME_CAPTION_TOOLTIP),
						EndContainer(),
						NWidget(WWT_MATRIX, COLOUR_LIGHT_BLUE, NCLWW_MATRIX), SetResize(1, 14), SetFill(1, 1), SetScrollbar(NCLWW_SCROLLBAR),
					EndContainer(),
					NWidget(NWID_VSCROLLBAR, COLOUR_LIGHT_BLUE, NCLWW_SCROLLBAR),
				EndContainer(),
			EndContainer(),
			/* Right side. */
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, NCLWW_DETAILS), SetResize(1, 1), SetFill(1, 1), EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 7), SetResize(1, 0),
		/* Bottom. */
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(8, 8, 8),
			NWidget(NWID_SELECTION, INVALID_COLOUR, NCLWW_SEL_ALL_UPDATE), SetResize(1, 0), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NCLWW_SELECT_UPDATE), SetResize(1, 0), SetFill(1, 0),
										SetDataTip(STR_CONTENT_SELECT_UPDATES_CAPTION, STR_CONTENT_SELECT_UPDATES_CAPTION_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NCLWW_SELECT_ALL), SetResize(1, 0), SetFill(1, 0),
										SetDataTip(STR_CONTENT_SELECT_ALL_CAPTION, STR_CONTENT_SELECT_ALL_CAPTION_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NCLWW_UNSELECT), SetResize(1, 0), SetFill(1, 0),
										SetDataTip(STR_CONTENT_UNSELECT_ALL_CAPTION, STR_CONTENT_UNSELECT_ALL_CAPTION_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NCLWW_CANCEL), SetResize(1, 0), SetFill(1, 0),
										SetDataTip(STR_BUTTON_CANCEL, STR_NULL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, NCLWW_DOWNLOAD), SetResize(1, 0), SetFill(1, 0),
										SetDataTip(STR_CONTENT_DOWNLOAD_CAPTION, STR_CONTENT_DOWNLOAD_CAPTION_TOOLTIP),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2), SetResize(1, 0),
		/* Resize button. */
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_RESIZEBOX, COLOUR_LIGHT_BLUE),
		EndContainer(),
	EndContainer(),
};

/** Window description of the content list */
static const WindowDesc _network_content_list_desc(
	WDP_CENTER, 630, 460,
	WC_NETWORK_WINDOW, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_network_content_list_widgets, lengthof(_nested_network_content_list_widgets)
);

/**
 * Show the content list window with a given set of content
 * @param cv the content to show, or NULL when it has to search for itself
 * @param type the type to (only) show
 */
void ShowNetworkContentListWindow(ContentVector *cv, ContentType type)
{
#if defined(WITH_ZLIB)
	_network_content_client.Clear();
	if (cv == NULL) {
		_network_content_client.RequestContentList(type);
	} else {
		_network_content_client.RequestContentList(cv, true);
	}

	DeleteWindowById(WC_NETWORK_WINDOW, 1);
	new NetworkContentListWindow(&_network_content_list_desc, cv != NULL);
#else
	ShowErrorMessage(STR_CONTENT_NO_ZLIB, STR_CONTENT_NO_ZLIB_SUB, WL_ERROR);
	/* Connection failed... clean up the mess */
	if (cv != NULL) {
		for (ContentIterator iter = cv->Begin(); iter != cv->End(); iter++) delete *iter;
	}
#endif /* WITH_ZLIB */
}

#endif /* ENABLE_NETWORK */
