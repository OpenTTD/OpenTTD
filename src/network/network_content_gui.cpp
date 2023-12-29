/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_content_gui.cpp Implementation of the Network Content related GUIs. */

#include "../stdafx.h"
#include "../strings_func.h"
#include "../gfx_func.h"
#include "../window_func.h"
#include "../error.h"
#include "../ai/ai.hpp"
#include "../game/game.hpp"
#include "../base_media_base.h"
#include "../openttd.h"
#include "../sortlist_type.h"
#include "../stringfilter_type.h"
#include "../querystring_gui.h"
#include "../core/geometry_func.hpp"
#include "../textfile_gui.h"
#include "../fios.h"
#include "network_content_gui.h"


#include "table/strings.h"
#include "../table/sprites.h"

#include <bitset>

#include "../safeguards.h"


/** Whether the user accepted to enter external websites during this session. */
static bool _accepted_external_search = false;


/** Window for displaying the textfile of an item in the content list. */
struct ContentTextfileWindow : public TextfileWindow {
	const ContentInfo *ci; ///< View the textfile of this ContentInfo.

	ContentTextfileWindow(TextfileType file_type, const ContentInfo *ci) : TextfileWindow(file_type), ci(ci)
	{
		auto textfile = this->ci->GetTextfile(file_type);
		this->LoadTextfile(textfile.value(), GetContentInfoSubDir(this->ci->type));
	}

	StringID GetTypeString() const
	{
		switch (this->ci->type) {
			case CONTENT_TYPE_NEWGRF:        return STR_CONTENT_TYPE_NEWGRF;
			case CONTENT_TYPE_BASE_GRAPHICS: return STR_CONTENT_TYPE_BASE_GRAPHICS;
			case CONTENT_TYPE_BASE_SOUNDS:   return STR_CONTENT_TYPE_BASE_SOUNDS;
			case CONTENT_TYPE_BASE_MUSIC:    return STR_CONTENT_TYPE_BASE_MUSIC;
			case CONTENT_TYPE_AI:            return STR_CONTENT_TYPE_AI;
			case CONTENT_TYPE_AI_LIBRARY:    return STR_CONTENT_TYPE_AI_LIBRARY;
			case CONTENT_TYPE_GAME:          return STR_CONTENT_TYPE_GAME_SCRIPT;
			case CONTENT_TYPE_GAME_LIBRARY:  return STR_CONTENT_TYPE_GS_LIBRARY;
			case CONTENT_TYPE_SCENARIO:      return STR_CONTENT_TYPE_SCENARIO;
			case CONTENT_TYPE_HEIGHTMAP:     return STR_CONTENT_TYPE_HEIGHTMAP;
			default: NOT_REACHED();
		}
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_TF_CAPTION) {
			SetDParam(0, this->GetTypeString());
			SetDParamStr(1, this->ci->name);
		}
	}
};

void ShowContentTextfileWindow(TextfileType file_type, const ContentInfo *ci)
{
	CloseWindowById(WC_TEXTFILE, file_type);
	new ContentTextfileWindow(file_type, ci);
}

/** Nested widgets for the download window. */
static const NWidgetPart _nested_network_content_download_status_window_widgets[] = {
	NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_CONTENT_DOWNLOAD_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_NCDS_PROGRESS_BAR), SetFill(1, 0),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_NCDS_PROGRESS_TEXT), SetFill(1, 0), SetMinimalSize(350, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NCDS_CANCELOK), SetDataTip(STR_BUTTON_CANCEL, STR_NULL), SetFill(1, 0),
		EndContainer(),
	EndContainer(),
};

/** Window description for the download window */
static WindowDesc _network_content_download_status_window_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_NETWORK_STATUS_WINDOW, WC_NONE,
	WDF_MODAL,
	std::begin(_nested_network_content_download_status_window_widgets), std::end(_nested_network_content_download_status_window_widgets)
);

BaseNetworkContentDownloadStatusWindow::BaseNetworkContentDownloadStatusWindow(WindowDesc *desc) :
		Window(desc), downloaded_bytes(0), downloaded_files(0), cur_id(UINT32_MAX)
{
	_network_content_client.AddCallback(this);
	_network_content_client.DownloadSelectedContent(this->total_files, this->total_bytes);

	this->InitNested(WN_NETWORK_STATUS_WINDOW_CONTENT_DOWNLOAD);
}

void BaseNetworkContentDownloadStatusWindow::Close([[maybe_unused]] int data)
{
	_network_content_client.RemoveCallback(this);
	this->Window::Close();
}

void BaseNetworkContentDownloadStatusWindow::UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize)
{
	switch (widget) {
		case WID_NCDS_PROGRESS_BAR:
			SetDParamMaxDigits(0, 8);
			SetDParamMaxDigits(1, 8);
			SetDParamMaxDigits(2, 8);
			*size = GetStringBoundingBox(STR_CONTENT_DOWNLOAD_PROGRESS_SIZE);
			/* We need some spacing for the 'border' */
			size->height += WidgetDimensions::scaled.frametext.Horizontal();
			size->width  += WidgetDimensions::scaled.frametext.Vertical();
			break;

		case WID_NCDS_PROGRESS_TEXT:
			size->height = GetCharacterHeight(FS_NORMAL) * 2 + WidgetDimensions::scaled.vsep_normal;
			break;
	}
}

void BaseNetworkContentDownloadStatusWindow::DrawWidget(const Rect &r, WidgetID widget) const
{
	switch (widget) {
		case WID_NCDS_PROGRESS_BAR: {
			/* Draw the % complete with a bar and a text */
			DrawFrameRect(r, COLOUR_GREY, FR_BORDERONLY | FR_LOWERED);
			Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
			DrawFrameRect(ir.WithWidth((uint64_t)ir.Width() * this->downloaded_bytes / this->total_bytes, false), COLOUR_MAUVE, FR_NONE);
			SetDParam(0, this->downloaded_bytes);
			SetDParam(1, this->total_bytes);
			SetDParam(2, this->downloaded_bytes * 100LL / this->total_bytes);
			DrawString(ir.left, ir.right, CenterBounds(ir.top, ir.bottom, GetCharacterHeight(FS_NORMAL)), STR_CONTENT_DOWNLOAD_PROGRESS_SIZE, TC_FROMSTRING, SA_HOR_CENTER);
			break;
		}

		case WID_NCDS_PROGRESS_TEXT: {
			StringID str;
			if (this->downloaded_bytes == this->total_bytes) {
				str = STR_CONTENT_DOWNLOAD_COMPLETE;
			} else if (!this->name.empty()) {
				SetDParamStr(0, this->name);
				SetDParam(1, this->downloaded_files);
				SetDParam(2, this->total_files);
				str = STR_CONTENT_DOWNLOAD_FILE;
			} else {
				str = STR_CONTENT_DOWNLOAD_INITIALISE;
			}
			DrawStringMultiLine(r, str, TC_FROMSTRING, SA_CENTER);
			break;
		}
	}
}

void BaseNetworkContentDownloadStatusWindow::OnDownloadProgress(const ContentInfo *ci, int bytes)
{
	if (ci->id != this->cur_id) {
		this->name = ci->filename;
		this->cur_id = ci->id;
		this->downloaded_files++;
	}

	/* A negative value means we are resetting; for example, when retrying or using a fallback. */
	if (bytes < 0) {
		this->downloaded_bytes = 0;
	} else {
		this->downloaded_bytes += bytes;
	}

	this->SetDirty();
}


/** Window for showing the download status of content */
struct NetworkContentDownloadStatusWindow : public BaseNetworkContentDownloadStatusWindow {
private:
	std::vector<ContentType> receivedTypes;     ///< Types we received so we can update their cache

public:
	/**
	 * Create a new download window based on a list of content information
	 * with flags whether to download them or not.
	 */
	NetworkContentDownloadStatusWindow() : BaseNetworkContentDownloadStatusWindow(&_network_content_download_status_window_desc)
	{
		this->parent = FindWindowById(WC_NETWORK_WINDOW, WN_NETWORK_WINDOW_CONTENT_LIST);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		TarScanner::Mode mode = TarScanner::NONE;
		for (auto ctype : this->receivedTypes) {
			switch (ctype) {
				case CONTENT_TYPE_AI:
				case CONTENT_TYPE_AI_LIBRARY:
					/* AI::Rescan calls the scanner. */
					break;
				case CONTENT_TYPE_GAME:
				case CONTENT_TYPE_GAME_LIBRARY:
					/* Game::Rescan calls the scanner. */
					break;

				case CONTENT_TYPE_BASE_GRAPHICS:
				case CONTENT_TYPE_BASE_SOUNDS:
				case CONTENT_TYPE_BASE_MUSIC:
					mode |= TarScanner::BASESET;
					break;

				case CONTENT_TYPE_NEWGRF:
					/* ScanNewGRFFiles calls the scanner. */
					break;

				case CONTENT_TYPE_SCENARIO:
				case CONTENT_TYPE_HEIGHTMAP:
					mode |= TarScanner::SCENARIO;
					break;

				default:
					break;
			}
		}

		TarScanner::DoScan(mode);

		/* Tell all the backends about what we've downloaded */
		for (auto ctype : this->receivedTypes) {
			switch (ctype) {
				case CONTENT_TYPE_AI:
				case CONTENT_TYPE_AI_LIBRARY:
					AI::Rescan();
					break;

				case CONTENT_TYPE_GAME:
				case CONTENT_TYPE_GAME_LIBRARY:
					Game::Rescan();
					break;

				case CONTENT_TYPE_BASE_GRAPHICS:
					BaseGraphics::FindSets();
					SetWindowDirty(WC_GAME_OPTIONS, WN_GAME_OPTIONS_GAME_OPTIONS);
					break;

				case CONTENT_TYPE_BASE_SOUNDS:
					BaseSounds::FindSets();
					SetWindowDirty(WC_GAME_OPTIONS, WN_GAME_OPTIONS_GAME_OPTIONS);
					break;

				case CONTENT_TYPE_BASE_MUSIC:
					BaseMusic::FindSets();
					SetWindowDirty(WC_GAME_OPTIONS, WN_GAME_OPTIONS_GAME_OPTIONS);
					break;

				case CONTENT_TYPE_NEWGRF:
					RequestNewGRFScan();
					break;

				case CONTENT_TYPE_SCENARIO:
				case CONTENT_TYPE_HEIGHTMAP:
					ScanScenarios();
					InvalidateWindowData(WC_SAVELOAD, 0, 0);
					break;

				default:
					break;
			}
		}

		/* Always invalidate the download window; tell it we are going to be gone */
		InvalidateWindowData(WC_NETWORK_WINDOW, WN_NETWORK_WINDOW_CONTENT_LIST, 2);

		this->BaseNetworkContentDownloadStatusWindow::Close();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget == WID_NCDS_CANCELOK) {
			if (this->downloaded_bytes != this->total_bytes) {
				_network_content_client.CloseConnection();
				this->Close();
			} else {
				/* If downloading succeeded, close the online content window. This will close
				 * the current window as well. */
				CloseWindowById(WC_NETWORK_WINDOW, WN_NETWORK_WINDOW_CONTENT_LIST);
			}
		}
	}

	void OnDownloadProgress(const ContentInfo *ci, int bytes) override
	{
		BaseNetworkContentDownloadStatusWindow::OnDownloadProgress(ci, bytes);
		include(this->receivedTypes, ci->type);

		/* When downloading is finished change cancel in ok */
		if (this->downloaded_bytes == this->total_bytes) {
			this->GetWidget<NWidgetCore>(WID_NCDS_CANCELOK)->widget_data = STR_BUTTON_OK;
		}
	}
};

/** Filter data for NetworkContentListWindow. */
struct ContentListFilterData {
	StringFilter string_filter; ///< Text filter of content list
	std::bitset<CONTENT_TYPE_END> types; ///< Content types displayed
};

/** Filter criteria for NetworkContentListWindow. */
enum ContentListFilterCriteria {
	CONTENT_FILTER_TEXT = 0,        ///< Filter by query sting
	CONTENT_FILTER_TYPE_OR_SELECTED,///< Filter by being of displayed type or selected for download
};

/** Window that lists the content that's at the content server */
class NetworkContentListWindow : public Window, ContentCallback {
	/** List with content infos. */
	typedef GUIList<const ContentInfo *, std::nullptr_t, ContentListFilterData &> GUIContentList;

	static const uint EDITBOX_MAX_SIZE   =  50; ///< Maximum size of the editbox in characters.

	static Listing last_sorting;     ///< The last sorting setting.
	static Filtering last_filtering; ///< The last filtering setting.
	static GUIContentList::SortFunction * const sorter_funcs[];   ///< Sorter functions
	static GUIContentList::FilterFunction * const filter_funcs[]; ///< Filter functions.
	GUIContentList content;      ///< List with content
	bool auto_select;            ///< Automatically select all content when the meta-data becomes available
	ContentListFilterData filter_data; ///< Filter for content list
	QueryString filter_editbox;  ///< Filter editbox;
	Dimension checkbox_size;     ///< Size of checkbox/"blot" sprite

	const ContentInfo *selected; ///< The selected content info
	int list_pos;                ///< Our position in the list
	uint filesize_sum;           ///< The sum of all selected file sizes
	Scrollbar *vscroll;          ///< Cache of the vertical scrollbar

	static std::string content_type_strs[CONTENT_TYPE_END]; ///< Cached strings for all content types.

	/** Search external websites for content */
	void OpenExternalSearch()
	{
		std::string url;
		url.reserve(1024);

		url += "https://grfsearch.openttd.org/?";

		if (this->auto_select) {
			url += "do=searchgrfid&q=";

			bool first = true;
			for (const ContentInfo *ci : this->content) {
				if (ci->state != ContentInfo::DOES_NOT_EXIST) continue;

				if (!first) url.push_back(',');
				first = false;

				fmt::format_to(std::back_inserter(url), "{:08X}:{}", ci->unique_id, FormatArrayAsHex(ci->md5sum));
			}
		} else {
			url += "do=searchtext&q=";

			/* Escape search term */
			for (const char *search = this->filter_editbox.text.buf; *search != '\0'; search++) {
				/* Remove quotes */
				if (*search == '\'' || *search == '"') continue;

				/* Escape special chars, such as &%,= */
				if (*search < 0x30) {
					fmt::format_to(std::back_inserter(url), "%{:02X}", *search);
				} else {
					url.push_back(*search);
				}
			}
		}

		OpenBrowser(url);
	}

	/**
	 * Callback function for disclaimer about entering external websites.
	 */
	static void ExternalSearchDisclaimerCallback(Window *w, bool accepted)
	{
		if (accepted) {
			_accepted_external_search = true;
			((NetworkContentListWindow*)w)->OpenExternalSearch();
		}
	}

	/**
	 * (Re)build the network game list as its amount has changed because
	 * an item has been added or deleted for example
	 */
	void BuildContentList()
	{
		if (!this->content.NeedRebuild()) return;

		/* Create temporary array of games to use for listing */
		this->content.clear();

		bool all_available = true;

		for (ConstContentIterator iter = _network_content_client.Begin(); iter != _network_content_client.End(); iter++) {
			if ((*iter)->state == ContentInfo::DOES_NOT_EXIST) all_available = false;
			this->content.push_back(*iter);
		}

		this->SetWidgetDisabledState(WID_NCL_SEARCH_EXTERNAL, this->auto_select && all_available);

		this->FilterContentList();
		this->content.shrink_to_fit();
		this->content.RebuildDone();
		this->SortContentList();

		this->vscroll->SetCount(this->content.size()); // Update the scrollbar
		this->ScrollToSelected();
	}

	/** Sort content by name. */
	static bool NameSorter(const ContentInfo * const &a, const ContentInfo * const &b)
	{
		return StrNaturalCompare(a->name, b->name, true) < 0; // Sort by name (natural sorting).
	}

	/** Sort content by type. */
	static bool TypeSorter(const ContentInfo * const &a, const ContentInfo * const &b)
	{
		int r = 0;
		if (a->type != b->type) {
			r = StrNaturalCompare(content_type_strs[a->type], content_type_strs[b->type]);
		}
		if (r == 0) return NameSorter(a, b);
		return r < 0;
	}

	/** Sort content by state. */
	static bool StateSorter(const ContentInfo * const &a, const ContentInfo * const &b)
	{
		int r = a->state - b->state;
		if (r == 0) return TypeSorter(a, b);
		return r < 0;
	}

	/** Sort the content list */
	void SortContentList()
	{
		if (!this->content.Sort()) return;

		int idx = find_index(this->content, this->selected);
		if (idx >= 0) this->list_pos = idx;
	}

	/** Filter content by tags/name */
	static bool CDECL TagNameFilter(const ContentInfo * const *a, ContentListFilterData &filter)
	{
		filter.string_filter.ResetState();
		for (auto &tag : (*a)->tags) filter.string_filter.AddLine(tag);

		filter.string_filter.AddLine((*a)->name);
		return filter.string_filter.GetState();
	}

	/** Filter content by type, but still show content selected for download. */
	static bool CDECL TypeOrSelectedFilter(const ContentInfo * const *a, ContentListFilterData &filter)
	{
		if (filter.types.none()) return true;
		if (filter.types[(*a)->type]) return true;
		return ((*a)->state == ContentInfo::SELECTED || (*a)->state == ContentInfo::AUTOSELECTED);
	}

	/** Filter the content list */
	void FilterContentList()
	{
		/* Apply filters. */
		bool changed = false;
		if (!this->filter_data.string_filter.IsEmpty()) {
			this->content.SetFilterType(CONTENT_FILTER_TEXT);
			changed |= this->content.Filter(this->filter_data);
		}
		if (this->filter_data.types.any()) {
			this->content.SetFilterType(CONTENT_FILTER_TYPE_OR_SELECTED);
			changed |= this->content.Filter(this->filter_data);
		}
		if (!changed) return;

		/* update list position */
		int idx = find_index(this->content, this->selected);
		if (idx >= 0) {
			this->list_pos = idx;
			return;
		}

		/* previously selected item not in list anymore */
		this->selected = nullptr;
		this->list_pos = 0;
	}

	/**
	 * Update filter state based on current window state.
	 * @return true if filter state was changed, otherwise false.
	 */
	bool UpdateFilterState()
	{
		Filtering old_params = this->content.GetFiltering();
		bool new_state = !this->filter_data.string_filter.IsEmpty() || this->filter_data.types.any();
		if (new_state != old_params.state) {
			this->content.SetFilterState(new_state);
		}
		return new_state != old_params.state;
	}

	/** Make sure that the currently selected content info is within the visible part of the matrix */
	void ScrollToSelected()
	{
		if (this->selected == nullptr) return;

		this->vscroll->ScrollTowards(this->list_pos);
	}

	friend void BuildContentTypeStringList();
public:
	/**
	 * Create the content list window.
	 * @param desc the window description to pass to Window's constructor.
	 * @param select_all Whether the select all button is allowed or not.
	 * @param types the main type of content to display or #CONTENT_TYPE_END.
	 *   When a type other than #CONTENT_TYPE_END is given, dependencies of
	 *   other types are only shown when content that depend on them are
	 *   selected.
	 */
	NetworkContentListWindow(WindowDesc *desc, bool select_all, const std::bitset<CONTENT_TYPE_END> &types) :
			Window(desc),
			auto_select(select_all),
			filter_editbox(EDITBOX_MAX_SIZE),
			selected(nullptr),
			list_pos(0)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_NCL_SCROLLBAR);
		this->FinishInitNested(WN_NETWORK_WINDOW_CONTENT_LIST);

		this->GetWidget<NWidgetStacked>(WID_NCL_SEL_ALL_UPDATE)->SetDisplayedPlane(select_all);

		this->querystrings[WID_NCL_FILTER] = &this->filter_editbox;
		this->filter_editbox.cancel_button = QueryString::ACTION_CLEAR;
		this->SetFocusedWidget(WID_NCL_FILTER);
		this->SetWidgetDisabledState(WID_NCL_SEARCH_EXTERNAL, this->auto_select);
		this->filter_data.types = types;

		_network_content_client.AddCallback(this);
		this->content.SetListing(this->last_sorting);
		this->content.SetFiltering(this->last_filtering);
		this->content.SetSortFuncs(this->sorter_funcs);
		this->content.SetFilterFuncs(this->filter_funcs);
		this->UpdateFilterState();
		this->content.ForceRebuild();
		this->FilterContentList();
		this->SortContentList();
		this->InvalidateData();
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		_network_content_client.RemoveCallback(this);
		this->Window::Close();
	}

	void OnInit() override
	{
		this->checkbox_size = maxdim(maxdim(GetSpriteSize(SPR_BOX_EMPTY), GetSpriteSize(SPR_BOX_CHECKED)), GetSpriteSize(SPR_BLOT));
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_NCL_CHECKBOX:
				size->width = this->checkbox_size.width + padding.width;
				break;

			case WID_NCL_TYPE: {
				Dimension d = *size;
				for (int i = CONTENT_TYPE_BEGIN; i < CONTENT_TYPE_END; i++) {
					d = maxdim(d, GetStringBoundingBox(STR_CONTENT_TYPE_BASE_GRAPHICS + i - CONTENT_TYPE_BASE_GRAPHICS));
				}
				size->width = d.width + padding.width;
				break;
			}

			case WID_NCL_MATRIX:
				resize->height = std::max(this->checkbox_size.height, (uint)GetCharacterHeight(FS_NORMAL)) + padding.height;
				size->height = 10 * resize->height;
				break;
		}
	}


	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_NCL_DETAILS:
				this->DrawDetails(r);
				break;

			case WID_NCL_MATRIX:
				this->DrawMatrix(r);
				break;
		}
	}

	void OnPaint() override
	{
		const SortButtonState arrow = this->content.IsDescSortOrder() ? SBS_DOWN : SBS_UP;

		if (this->content.NeedRebuild()) {
			this->BuildContentList();
		}

		this->DrawWidgets();

		switch (this->content.SortType()) {
			case WID_NCL_CHECKBOX - WID_NCL_CHECKBOX: this->DrawSortButtonState(WID_NCL_CHECKBOX, arrow); break;
			case WID_NCL_TYPE     - WID_NCL_CHECKBOX: this->DrawSortButtonState(WID_NCL_TYPE,     arrow); break;
			case WID_NCL_NAME     - WID_NCL_CHECKBOX: this->DrawSortButtonState(WID_NCL_NAME,     arrow); break;
		}
	}

	/**
	 * Draw/fill the matrix with the list of content to download.
	 * @param r The boundaries of the matrix.
	 */
	void DrawMatrix(const Rect &r) const
	{
		Rect checkbox = this->GetWidget<NWidgetBase>(WID_NCL_CHECKBOX)->GetCurrentRect();
		Rect name = this->GetWidget<NWidgetBase>(WID_NCL_NAME)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
		Rect type = this->GetWidget<NWidgetBase>(WID_NCL_TYPE)->GetCurrentRect();

		/* Fill the matrix with the information */
		int sprite_y_offset = (this->resize.step_height - this->checkbox_size.height) / 2;
		int text_y_offset   = (this->resize.step_height - GetCharacterHeight(FS_NORMAL)) / 2;

		Rect mr = r.WithHeight(this->resize.step_height);
		auto iter = this->content.begin() + this->vscroll->GetPosition();
		size_t last = this->vscroll->GetPosition() + this->vscroll->GetCapacity();
		auto end = (last < this->content.size()) ? this->content.begin() + last : this->content.end();

		for (/**/; iter != end; iter++) {
			const ContentInfo *ci = *iter;

			if (ci == this->selected) GfxFillRect(mr.Shrink(WidgetDimensions::scaled.bevel), PC_GREY);

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
			DrawSprite(sprite, pal, checkbox.left + (sprite == SPR_BLOT ? 3 : 2), mr.top + sprite_y_offset + (sprite == SPR_BLOT ? 0 : 1));

			StringID str = STR_CONTENT_TYPE_BASE_GRAPHICS + ci->type - CONTENT_TYPE_BASE_GRAPHICS;
			DrawString(type.left, type.right, mr.top + text_y_offset, str, TC_BLACK, SA_HOR_CENTER);

			DrawString(name.left, name.right, mr.top + text_y_offset, ci->name, TC_BLACK);
			mr = mr.Translate(0, this->resize.step_height);
		}
	}

	/**
	 * Helper function to draw the details part of this window.
	 * @param r the rectangle to stay within while drawing
	 */
	void DrawDetails(const Rect &r) const
	{
		/* Height for the title banner */
		int HEADER_HEIGHT = 3 * GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.frametext.Vertical();

		Rect hr = r.WithHeight(HEADER_HEIGHT).Shrink(WidgetDimensions::scaled.frametext);
		Rect tr = r.Shrink(WidgetDimensions::scaled.frametext);
		tr.top += HEADER_HEIGHT;

		/* Create the nice grayish rectangle at the details top */
		GfxFillRect(r.WithHeight(HEADER_HEIGHT).Shrink(WidgetDimensions::scaled.bevel.left, WidgetDimensions::scaled.bevel.top, WidgetDimensions::scaled.bevel.right, 0), PC_DARK_BLUE);
		DrawString(hr.left, hr.right, hr.top, STR_CONTENT_DETAIL_TITLE, TC_FROMSTRING, SA_HOR_CENTER);

		/* Draw the total download size */
		SetDParam(0, this->filesize_sum);
		DrawString(tr.left, tr.right, tr.bottom - GetCharacterHeight(FS_NORMAL) + 1, STR_CONTENT_TOTAL_DOWNLOAD_SIZE);

		if (this->selected == nullptr) return;

		/* And fill the rest of the details when there's information to place there */
		DrawStringMultiLine(hr.left, hr.right, hr.top + GetCharacterHeight(FS_NORMAL), hr.bottom, STR_CONTENT_DETAIL_SUBTITLE_UNSELECTED + this->selected->state, TC_FROMSTRING, SA_CENTER);

		/* Also show the total download size, so keep some space from the bottom */
		tr.bottom -= GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_wide;

		if (this->selected->upgrade) {
			SetDParam(0, STR_CONTENT_TYPE_BASE_GRAPHICS + this->selected->type - CONTENT_TYPE_BASE_GRAPHICS);
			tr.top = DrawStringMultiLine(tr, STR_CONTENT_DETAIL_UPDATE);
			tr.top += WidgetDimensions::scaled.vsep_wide;
		}

		SetDParamStr(0, this->selected->name);
		tr.top = DrawStringMultiLine(tr, STR_CONTENT_DETAIL_NAME);

		if (!this->selected->version.empty()) {
			SetDParamStr(0, this->selected->version);
			tr.top = DrawStringMultiLine(tr, STR_CONTENT_DETAIL_VERSION);
		}

		if (!this->selected->description.empty()) {
			SetDParamStr(0, this->selected->description);
			tr.top = DrawStringMultiLine(tr, STR_CONTENT_DETAIL_DESCRIPTION);
		}

		if (!this->selected->url.empty()) {
			SetDParamStr(0, this->selected->url);
			tr.top = DrawStringMultiLine(tr, STR_CONTENT_DETAIL_URL);
		}

		SetDParam(0, STR_CONTENT_TYPE_BASE_GRAPHICS + this->selected->type - CONTENT_TYPE_BASE_GRAPHICS);
		tr.top = DrawStringMultiLine(tr, STR_CONTENT_DETAIL_TYPE);

		tr.top += WidgetDimensions::scaled.vsep_wide;
		SetDParam(0, this->selected->filesize);
		tr.top = DrawStringMultiLine(tr, STR_CONTENT_DETAIL_FILESIZE);

		if (!this->selected->dependencies.empty()) {
			/* List dependencies */
			std::string buf;
			for (auto &cid : this->selected->dependencies) {
				/* Try to find the dependency */
				ConstContentIterator iter = _network_content_client.Begin();
				for (; iter != _network_content_client.End(); iter++) {
					const ContentInfo *ci = *iter;
					if (ci->id != cid) continue;

					if (!buf.empty()) buf += ", ";
					buf += (*iter)->name;
					break;
				}
			}
			SetDParamStr(0, buf);
			tr.top = DrawStringMultiLine(tr, STR_CONTENT_DETAIL_DEPENDENCIES);
		}

		if (!this->selected->tags.empty()) {
			/* List all tags */
			std::string buf;
			for (auto &tag : this->selected->tags) {
				if (!buf.empty()) buf += ", ";
				buf += tag;
			}
			SetDParamStr(0, buf);
			tr.top = DrawStringMultiLine(tr, STR_CONTENT_DETAIL_TAGS);
		}

		if (this->selected->IsSelected()) {
			/* When selected show all manually selected content that depends on this */
			ConstContentVector tree;
			_network_content_client.ReverseLookupTreeDependency(tree, this->selected);

			std::string buf;
			for (const ContentInfo *ci : tree) {
				if (ci == this->selected || ci->state != ContentInfo::SELECTED) continue;

				if (!buf.empty()) buf += ", ";
				buf += ci->name;
			}
			if (!buf.empty()) {
				SetDParamStr(0, buf);
				tr.top = DrawStringMultiLine(tr, STR_CONTENT_DETAIL_SELECTED_BECAUSE_OF);
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget >= WID_NCL_TEXTFILE && widget < WID_NCL_TEXTFILE + TFT_CONTENT_END) {
			if (this->selected == nullptr || this->selected->state != ContentInfo::ALREADY_HERE) return;

			ShowContentTextfileWindow((TextfileType)(widget - WID_NCL_TEXTFILE), this->selected);
			return;
		}

		switch (widget) {
			case WID_NCL_MATRIX: {
				auto it = this->vscroll->GetScrolledItemFromWidget(this->content, pt.y, this, WID_NCL_MATRIX);
				if (it == this->content.end()) return; // click out of bounds

				this->selected = *it;
				this->list_pos = it - this->content.begin();

				const NWidgetBase *checkbox = this->GetWidget<NWidgetBase>(WID_NCL_CHECKBOX);
				if (click_count > 1 || IsInsideBS(pt.x, checkbox->pos_x, checkbox->current_x)) {
					_network_content_client.ToggleSelectedState(this->selected);
					this->content.ForceResort();
				}

				if (this->filter_data.types.any()) {
					this->content.ForceRebuild();
				}

				this->InvalidateData();
				break;
			}

			case WID_NCL_CHECKBOX:
			case WID_NCL_TYPE:
			case WID_NCL_NAME:
				if (this->content.SortType() == widget - WID_NCL_CHECKBOX) {
					this->content.ToggleSortOrder();
					if (!this->content.empty()) this->list_pos = (int)this->content.size() - this->list_pos - 1;
				} else {
					this->content.SetSortType(widget - WID_NCL_CHECKBOX);
					this->content.ForceResort();
					this->SortContentList();
				}
				this->ScrollToSelected();
				this->InvalidateData();
				break;

			case WID_NCL_SELECT_ALL:
				_network_content_client.SelectAll();
				this->InvalidateData();
				break;

			case WID_NCL_SELECT_UPDATE:
				_network_content_client.SelectUpgrade();
				this->InvalidateData();
				break;

			case WID_NCL_UNSELECT:
				_network_content_client.UnselectAll();
				this->InvalidateData();
				break;

			case WID_NCL_CANCEL:
				this->Close();
				break;

			case WID_NCL_OPEN_URL:
				if (this->selected != nullptr) {
					OpenBrowser(this->selected->url);
				}
				break;

			case WID_NCL_DOWNLOAD:
				if (BringWindowToFrontById(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_CONTENT_DOWNLOAD) == nullptr) new NetworkContentDownloadStatusWindow();
				break;

			case WID_NCL_SEARCH_EXTERNAL:
				if (_accepted_external_search) {
					this->OpenExternalSearch();
				} else {
					ShowQuery(STR_CONTENT_SEARCH_EXTERNAL_DISCLAIMER_CAPTION, STR_CONTENT_SEARCH_EXTERNAL_DISCLAIMER, this, ExternalSearchDisclaimerCallback);
				}
				break;
		}
	}

	EventState OnKeyPress([[maybe_unused]] char32_t key, uint16_t keycode) override
	{
		if (this->vscroll->UpdateListPositionOnKeyPress(this->list_pos, keycode) == ES_NOT_HANDLED) {
			switch (keycode) {
				case WKC_SPACE:
				case WKC_RETURN:
					if (keycode == WKC_RETURN || !IsWidgetFocused(WID_NCL_FILTER)) {
						if (this->selected != nullptr) {
							_network_content_client.ToggleSelectedState(this->selected);
							this->content.ForceResort();
							this->InvalidateData();
						}
						if (this->filter_data.types.any()) {
							this->content.ForceRebuild();
							this->InvalidateData();
						}
						return ES_HANDLED;
					}
					/* space is pressed and filter is focused. */
					FALLTHROUGH;

				default:
					return ES_NOT_HANDLED;
			}
		}

		if (this->content.empty()) {
			if (this->UpdateFilterState()) {
				this->content.ForceRebuild();
				this->InvalidateData();
			}
			return ES_HANDLED;
		}

		this->selected = this->content[this->list_pos];

		if (this->UpdateFilterState()) {
			this->content.ForceRebuild();
		} else {
			/* Scroll to the new content if it is outside the current range. */
			this->ScrollToSelected();
		}

		/* redraw window */
		this->InvalidateData();
		return ES_HANDLED;
	}

	void OnEditboxChanged(WidgetID wid) override
	{
		if (wid == WID_NCL_FILTER) {
			this->filter_data.string_filter.SetFilterTerm(this->filter_editbox.text.buf);
			this->UpdateFilterState();
			this->content.ForceRebuild();
			this->InvalidateData();
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_NCL_MATRIX);
	}

	void OnReceiveContentInfo(const ContentInfo *rci) override
	{
		if (this->auto_select && !rci->IsSelected()) _network_content_client.ToggleSelectedState(rci);
		this->content.ForceRebuild();
		this->InvalidateData(0, false);
	}

	void OnDownloadComplete(ContentID) override
	{
		this->content.ForceResort();
		this->InvalidateData();
	}

	void OnConnect(bool success) override
	{
		if (!success) {
			ShowErrorMessage(STR_CONTENT_ERROR_COULD_NOT_CONNECT, INVALID_STRING_ID, WL_ERROR);
			this->Close();
			return;
		}

		this->InvalidateData();
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		if (this->content.NeedRebuild()) this->BuildContentList();

		/* To sum all the bytes we intend to download */
		this->filesize_sum = 0;
		bool show_select_all = false;
		bool show_select_upgrade = false;
		for (const ContentInfo *ci : this->content) {
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
		this->SetWidgetDisabledState(WID_NCL_DOWNLOAD, this->filesize_sum == 0 || (FindWindowById(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_CONTENT_DOWNLOAD) != nullptr && data != 2));
		this->SetWidgetDisabledState(WID_NCL_UNSELECT, this->filesize_sum == 0);
		this->SetWidgetDisabledState(WID_NCL_SELECT_ALL, !show_select_all);
		this->SetWidgetDisabledState(WID_NCL_SELECT_UPDATE, !show_select_upgrade);
		this->SetWidgetDisabledState(WID_NCL_OPEN_URL, this->selected == nullptr || this->selected->url.empty());
		for (TextfileType tft = TFT_CONTENT_BEGIN; tft < TFT_CONTENT_END; tft++) {
			this->SetWidgetDisabledState(WID_NCL_TEXTFILE + tft, this->selected == nullptr || this->selected->state != ContentInfo::ALREADY_HERE || !this->selected->GetTextfile(tft).has_value());
		}

		this->GetWidget<NWidgetCore>(WID_NCL_CANCEL)->widget_data = this->filesize_sum == 0 ? STR_AI_SETTINGS_CLOSE : STR_AI_LIST_CANCEL;
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
	&TypeOrSelectedFilter,
};

std::string NetworkContentListWindow::content_type_strs[CONTENT_TYPE_END];

/**
 * Build array of all strings corresponding to the content types.
 */
void BuildContentTypeStringList()
{
	for (int i = CONTENT_TYPE_BEGIN; i < CONTENT_TYPE_END; i++) {
		NetworkContentListWindow::content_type_strs[i] = GetString(STR_CONTENT_TYPE_BASE_GRAPHICS + i - CONTENT_TYPE_BASE_GRAPHICS);
	}
}

/** The widgets for the content list. */
static const NWidgetPart _nested_network_content_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE), SetDataTip(STR_CONTENT_TITLE, STR_NULL),
		NWidget(WWT_DEFSIZEBOX, COLOUR_LIGHT_BLUE),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, WID_NCL_BACKGROUND),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse_resize),
			/* Top */
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(WWT_TEXT, COLOUR_LIGHT_BLUE, WID_NCL_FILTER_CAPT), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_CONTENT_FILTER_TITLE, STR_NULL), SetAlignment(SA_RIGHT | SA_VERT_CENTER),
				NWidget(WWT_EDITBOX, COLOUR_LIGHT_BLUE, WID_NCL_FILTER), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
			EndContainer(),
			/* Lists and info. */
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				/* Left side. */
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
					NWidget(NWID_HORIZONTAL),
						NWidget(NWID_VERTICAL),
							NWidget(NWID_HORIZONTAL),
								NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NCL_CHECKBOX), SetMinimalSize(13, 1), SetDataTip(STR_EMPTY, STR_NULL),
								NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NCL_TYPE),
										SetDataTip(STR_CONTENT_TYPE_CAPTION, STR_CONTENT_TYPE_CAPTION_TOOLTIP),
								NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NCL_NAME), SetResize(1, 0), SetFill(1, 0),
										SetDataTip(STR_CONTENT_NAME_CAPTION, STR_CONTENT_NAME_CAPTION_TOOLTIP),
							EndContainer(),
							NWidget(WWT_MATRIX, COLOUR_LIGHT_BLUE, WID_NCL_MATRIX), SetResize(1, 14), SetFill(1, 1), SetScrollbar(WID_NCL_SCROLLBAR), SetMatrixDataTip(1, 0, STR_CONTENT_MATRIX_TOOLTIP),
						EndContainer(),
						NWidget(NWID_VSCROLLBAR, COLOUR_LIGHT_BLUE, WID_NCL_SCROLLBAR),
					EndContainer(),
					NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_NCL_SEL_ALL_UPDATE),
							NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NCL_SELECT_UPDATE), SetResize(1, 0), SetFill(1, 0),
									SetDataTip(STR_CONTENT_SELECT_UPDATES_CAPTION, STR_CONTENT_SELECT_UPDATES_CAPTION_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NCL_SELECT_ALL), SetResize(1, 0), SetFill(1, 0),
									SetDataTip(STR_CONTENT_SELECT_ALL_CAPTION, STR_CONTENT_SELECT_ALL_CAPTION_TOOLTIP),
						EndContainer(),
						NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NCL_UNSELECT), SetResize(1, 0), SetFill(1, 0),
									SetDataTip(STR_CONTENT_UNSELECT_ALL_CAPTION, STR_CONTENT_UNSELECT_ALL_CAPTION_TOOLTIP),
					EndContainer(),
				EndContainer(),
				/* Right side. */
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
					NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, WID_NCL_DETAILS), SetResize(1, 1), SetFill(1, 1),
					EndContainer(),
					NWidget(NWID_VERTICAL),
						NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
							NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NCL_OPEN_URL), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_CONTENT_OPEN_URL, STR_CONTENT_OPEN_URL_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NCL_TEXTFILE + TFT_README), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_README, STR_TEXTFILE_VIEW_README_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
							NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NCL_TEXTFILE + TFT_CHANGELOG), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_CHANGELOG, STR_TEXTFILE_VIEW_CHANGELOG_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NCL_TEXTFILE + TFT_LICENSE), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_LICENCE, STR_TEXTFILE_VIEW_LICENCE_TOOLTIP),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),
			/* Bottom. */
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NCL_SEARCH_EXTERNAL), SetResize(1, 0), SetFill(1, 0),
						SetDataTip(STR_CONTENT_SEARCH_EXTERNAL, STR_CONTENT_SEARCH_EXTERNAL_TOOLTIP),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NCL_CANCEL), SetResize(1, 0), SetFill(1, 0),
							SetDataTip(STR_BUTTON_CANCEL, STR_NULL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, WID_NCL_DOWNLOAD), SetResize(1, 0), SetFill(1, 0),
							SetDataTip(STR_CONTENT_DOWNLOAD_CAPTION, STR_CONTENT_DOWNLOAD_CAPTION_TOOLTIP),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		/* Resize button. */
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_RESIZEBOX, COLOUR_LIGHT_BLUE), SetDataTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

/** Window description of the content list */
static WindowDesc _network_content_list_desc(__FILE__, __LINE__,
	WDP_CENTER, "list_content", 630, 460,
	WC_NETWORK_WINDOW, WC_NONE,
	0,
	std::begin(_nested_network_content_list_widgets), std::end(_nested_network_content_list_widgets)
);

/**
 * Show the content list window with a given set of content
 * @param cv the content to show, or nullptr when it has to search for itself
 * @param type1 the first type to (only) show or #CONTENT_TYPE_END to show all.
 * @param type2 the second type to (only) show in addition to type1. If type2 is != #CONTENT_TYPE_END, then also type1 should be != #CONTENT_TYPE_END.
 *   If type2 != #CONTENT_TYPE_END, then type1 != type2 must be true.
 */
void ShowNetworkContentListWindow(ContentVector *cv, ContentType type1, ContentType type2)
{
#if defined(WITH_ZLIB)
	std::bitset<CONTENT_TYPE_END> types;
	_network_content_client.Clear();
	if (cv == nullptr) {
		assert(type1 != CONTENT_TYPE_END || type2 == CONTENT_TYPE_END);
		assert(type1 == CONTENT_TYPE_END || type1 != type2);
		_network_content_client.RequestContentList(type1);
		if (type2 != CONTENT_TYPE_END) _network_content_client.RequestContentList(type2);

		if (type1 != CONTENT_TYPE_END) types[type1] = true;
		if (type2 != CONTENT_TYPE_END) types[type2] = true;
	} else {
		_network_content_client.RequestContentList(cv, true);
	}

	CloseWindowById(WC_NETWORK_WINDOW, WN_NETWORK_WINDOW_CONTENT_LIST);
	new NetworkContentListWindow(&_network_content_list_desc, cv != nullptr, types);
#else
	ShowErrorMessage(STR_CONTENT_NO_ZLIB, STR_CONTENT_NO_ZLIB_SUB, WL_ERROR);
	/* Connection failed... clean up the mess */
	if (cv != nullptr) {
		for (ContentInfo *ci : *cv) delete ci;
	}
#endif /* WITH_ZLIB */
}
