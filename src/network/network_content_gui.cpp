/* $Id$ */

/** @file network_gui.cpp Implementation of the Network related GUIs. */

#if defined(ENABLE_NETWORK)
#include "../stdafx.h"
#include "../string_func.h"
#include "../strings_func.h"
#include "../gfx_func.h"
#include "../window_func.h"
#include "../window_gui.h"
#include "../gui.h"
#include "../core/smallvec_type.hpp"
#include "../ai/ai.hpp"
#include "../gfxinit.h"
#include  "network_content.h"

#include "table/strings.h"
#include "../table/sprites.h"

/** Widgets for the download window */
static const Widget _network_content_download_status_window_widget[] = {
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,      0,   349,     0,    13, STR_CONTENT_DOWNLOAD_TITLE, STR_018C_WINDOW_TITLE_DRAG_THIS}, // NCDSWW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,      0,   349,    14,    84, 0x0,                        STR_NULL},                        // NCDSWW_BACKGROUND
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_WHITE,   125,   225,    69,    80, STR_012E_CANCEL,            STR_NULL},                        // NCDSWW_CANCELOK
{   WIDGETS_END},
};

/** Window description for the download window */
static const WindowDesc _network_content_download_status_window_desc = {
	WDP_CENTER, WDP_CENTER, 350, 85, 350, 85,
	WC_NETWORK_STATUS_WINDOW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_MODAL,
	_network_content_download_status_window_widget,
};

/** Window for showing the download status of content */
struct NetworkContentDownloadStatusWindow : public Window, ContentCallback {
	/** Widgets used by this window */
	enum Widgets {
		NCDSWW_CAPTION,    ///< Caption of the window
		NCDSWW_BACKGROUND, ///< Background
		NCDSWW_CANCELOK,   ///< Cancel/OK button
	};

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
		Window(&_network_content_download_status_window_desc),
		cur_id(UINT32_MAX)
	{
		this->parent = FindWindowById(WC_NETWORK_WINDOW, 1);

		_network_content_client.AddCallback(this);
		_network_content_client.DownloadSelectedContent(this->total_files, this->total_bytes);

		this->FindWindowPlacementAndResize(&_network_content_download_status_window_desc);
	}

	/** Free whatever we've allocated */
	~NetworkContentDownloadStatusWindow()
	{
		/* Tell all the backends about what we've downloaded */
		for (ContentType *iter = this->receivedTypes.Begin(); iter != this->receivedTypes.End(); iter++) {
			switch (*iter) {
				case CONTENT_TYPE_AI:
				case CONTENT_TYPE_AI_LIBRARY:
					AI::Rescan();
					InvalidateWindowClasses(WC_AI_DEBUG);
					break;

				case CONTENT_TYPE_BASE_GRAPHICS:
					FindGraphicsSets();
					break;

				case CONTENT_TYPE_NEWGRF:
					ScanNewGRFFiles();
					/* Yes... these are the NewGRF windows */
					InvalidateWindowClasses(WC_SAVELOAD);
					InvalidateWindowData(WC_GAME_OPTIONS, 0, 1);
					InvalidateWindowData(WC_NETWORK_WINDOW, 0, 2);
					break;

				default:
					break;
			}
		}

		_network_content_client.RemoveCallback(this);
	}

	virtual void OnPaint()
	{
		/* When downloading is finished change cancel in ok */
		if (this->downloaded_bytes == this->total_bytes) {
			this->widget[NCDSWW_CANCELOK].data = STR_012F_OK;
		}

		this->DrawWidgets();

		/* Draw nice progress bar :) */
		DrawFrameRect(20, 18, (int)((this->width - 20) * this->downloaded_bytes / this->total_bytes), 28, COLOUR_MAUVE, FR_NONE);

		SetDParam(0, this->downloaded_bytes);
		SetDParam(1, this->total_bytes);
		SetDParam(2, this->downloaded_bytes * 100 / this->total_bytes);
		DrawStringCentered(this->width / 2, 35, STR_CONTENT_DOWNLOAD_PROGRESS_SIZE, TC_GREY);

		if  (this->downloaded_bytes == this->total_bytes) {
			DrawStringCentered(this->width / 2, 50, STR_CONTENT_DOWNLOAD_COMPLETE, TC_GREY);
		} else if (!StrEmpty(this->name)) {
			SetDParamStr(0, this->name);
			SetDParam(1, this->downloaded_files);
			SetDParam(2, this->total_files);
			DrawStringMultiCenter(this->width / 2, 50, STR_CONTENT_DOWNLOAD_FILE, this->width);
		} else {
			DrawStringCentered(this->width / 2, 50, STR_CONTENT_DOWNLOAD_INITIALISE, TC_GREY);
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget == NCDSWW_CANCELOK) delete this;
	}

	virtual void OnDownloadProgress(const ContentInfo *ci, uint bytes)
	{
		if (ci->id != this->cur_id) {
			strecpy(this->name, ci->filename, lastof(this->name));
			this->cur_id = ci->id;
			this->downloaded_files++;
			this->receivedTypes.Include(ci->type);
		}
		this->downloaded_bytes += bytes;

		this->SetDirty();
	}
};

/** Window that lists the content that's at the content server */
class NetworkContentListWindow : public Window, ContentCallback {
	/** All widgets used */
	enum Widgets {
		NCLWW_CLOSE,         ///< Close 'X' button
		NCLWW_CAPTION,       ///< Caption of the window
		NCLWW_BACKGROUND,    ///< Resize button

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

		NCLWW_RESIZE,        ///< Resize button
	};

	const ContentInfo *selected; ///< The selected content info
	int list_pos;                ///< Our position in the list

	/** Make sure that the currently selected content info is within the visible part of the matrix */
	void ScrollToSelected()
	{
		if (this->selected == NULL) return;

		if (this->list_pos < this->vscroll.pos) {
			/* scroll up to the server */
			this->vscroll.pos = this->list_pos;
		} else if (this->list_pos >= this->vscroll.pos + this->vscroll.cap) {
			/* scroll down so that the server is at the bottom */
			this->vscroll.pos = this->list_pos - this->vscroll.cap + 1;
		}
	}

public:
	/**
	 * Create the content list window.
	 * @param desc the window description to pass to Window's constructor.
	 */
	NetworkContentListWindow(const WindowDesc *desc, bool select_all) : Window(desc, 1), selected(NULL), list_pos(0)
	{
		this->vscroll.cap = 14;
		this->resize.step_height = 14;
		this->resize.step_width = 2;

		_network_content_client.AddCallback(this);
		this->HideWidget(select_all ? NCLWW_SELECT_UPDATE : NCLWW_SELECT_ALL);

		SetVScrollCount(this, _network_content_client.Length());
		this->FindWindowPlacementAndResize(desc);
	}

	/** Free everything we allocated */
	~NetworkContentListWindow()
	{
		_network_content_client.RemoveCallback(this);
	}

	virtual void OnPaint()
	{
		/* To sum all the bytes we intend to download */
		uint filesize = 0;
		bool show_select_all = false;
		bool show_select_update = false;
		for (ConstContentIterator iter = _network_content_client.Begin(); iter != _network_content_client.End(); iter++) {
			const ContentInfo *ci = *iter;
			switch (ci->state) {
				case ContentInfo::SELECTED:
				case ContentInfo::AUTOSELECTED:
					filesize += ci->filesize;
					break;

				case ContentInfo::UNSELECTED:
					show_select_all = true;
					show_select_update |= ci->update;
					break;

				default:
					break;
			}
		}

		this->SetWidgetDisabledState(NCLWW_DOWNLOAD, filesize == 0);
		this->SetWidgetDisabledState(NCLWW_UNSELECT, filesize == 0);
		this->SetWidgetDisabledState(NCLWW_SELECT_ALL, !show_select_all);
		this->SetWidgetDisabledState(NCLWW_SELECT_UPDATE, !show_select_update);

		this->DrawWidgets();

		/* Fill the matrix with the information */
		uint y = this->widget[NCLWW_MATRIX].top + 3;
		int cnt = 0;
		for (ConstContentIterator iter = _network_content_client.Get(this->vscroll.pos); iter != _network_content_client.End() && cnt < this->vscroll.cap; iter++, cnt++) {
			const ContentInfo *ci = *iter;

			if (ci == this->selected) GfxFillRect(this->widget[NCLWW_CHECKBOX].left + 1, y - 2, this->widget[NCLWW_NAME].right - 1, y + 9, 10);

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
			DrawSprite(sprite, pal, this->widget[NCLWW_CHECKBOX].left + (pal == PAL_NONE ? 3 : 4), y + (pal == PAL_NONE ? 1 : 0));

			StringID str = STR_CONTENT_TYPE_BASE_GRAPHICS + ci->type - CONTENT_TYPE_BASE_GRAPHICS;
			DrawStringCenteredTruncated(this->widget[NCLWW_TYPE].left, this->widget[NCLWW_TYPE].right, y, str, TC_BLACK);

			SetDParamStr(0, ci->name);
			DrawStringTruncated(this->widget[NCLWW_NAME].left + 5, y, STR_JUST_RAW_STRING, TC_BLACK, this->widget[NCLWW_NAME].right - this->widget[NCLWW_NAME].left - 5);
			y += this->resize.step_height;
		}

		/* Create the nice grayish rectangle at the details top */
		GfxFillRect(this->widget[NCLWW_DETAILS].left + 1, this->widget[NCLWW_DETAILS].top + 1, this->widget[NCLWW_DETAILS].right - 1, this->widget[NCLWW_DETAILS].top + 50, 157);
		DrawStringCentered((this->widget[NCLWW_DETAILS].left + this->widget[NCLWW_DETAILS].right) / 2, this->widget[NCLWW_DETAILS].top + 11, STR_CONTENT_DETAIL_TITLE, TC_FROMSTRING);

		if (this->selected == NULL) return;

		/* And fill the rest of the details when there's information to place there */
		DrawStringMultiCenter((this->widget[NCLWW_DETAILS].left + this->widget[NCLWW_DETAILS].right) / 2, this->widget[NCLWW_DETAILS].top + 32, STR_CONTENT_DETAIL_SUBTITLE_UNSELECTED + this->selected->state, this->widget[NCLWW_DETAILS].right - this->widget[NCLWW_DETAILS].left - 10);

		/* Also show the total download size, so keep some space from the bottom */
		const uint max_y = this->widget[NCLWW_DETAILS].bottom - 15;
		y = this->widget[NCLWW_DETAILS].top + 55;

		if (this->selected->update) {
			SetDParam(0, STR_CONTENT_TYPE_BASE_GRAPHICS + this->selected->type - CONTENT_TYPE_BASE_GRAPHICS);
			y += DrawStringMultiLine(this->widget[NCLWW_DETAILS].left + 5, y, STR_CONTENT_DETAIL_UPDATE, this->widget[NCLWW_DETAILS].right - this->widget[NCLWW_DETAILS].left - 5, max_y - y);
			y += 11;
		}

		SetDParamStr(0, this->selected->name);
		y += DrawStringMultiLine(this->widget[NCLWW_DETAILS].left + 5, y, STR_CONTENT_DETAIL_NAME, this->widget[NCLWW_DETAILS].right - this->widget[NCLWW_DETAILS].left - 5, max_y - y);

		if (!StrEmpty(this->selected->version)) {
			SetDParamStr(0, this->selected->version);
			y += DrawStringMultiLine(this->widget[NCLWW_DETAILS].left + 5, y, STR_CONTENT_DETAIL_VERSION, this->widget[NCLWW_DETAILS].right - this->widget[NCLWW_DETAILS].left - 5, max_y - y);
		}

		if (!StrEmpty(this->selected->description)) {
			SetDParamStr(0, this->selected->description);
			y += DrawStringMultiLine(this->widget[NCLWW_DETAILS].left + 5, y, STR_CONTENT_DETAIL_DESCRIPTION, this->widget[NCLWW_DETAILS].right - this->widget[NCLWW_DETAILS].left - 5, max_y - y);
		}

		if (!StrEmpty(this->selected->url)) {
			SetDParamStr(0, this->selected->url);
			y += DrawStringMultiLine(this->widget[NCLWW_DETAILS].left + 5, y, STR_CONTENT_DETAIL_URL, this->widget[NCLWW_DETAILS].right - this->widget[NCLWW_DETAILS].left - 5, max_y - y);
		}

		SetDParam(0, STR_CONTENT_TYPE_BASE_GRAPHICS + this->selected->type - CONTENT_TYPE_BASE_GRAPHICS);
		y += DrawStringMultiLine(this->widget[NCLWW_DETAILS].left + 5, y, STR_CONTENT_DETAIL_TYPE, this->widget[NCLWW_DETAILS].right - this->widget[NCLWW_DETAILS].left - 5, max_y - y);

		y += 11;
		SetDParam(0, this->selected->filesize);
		y += DrawStringMultiLine(this->widget[NCLWW_DETAILS].left + 5, y, STR_CONTENT_DETAIL_FILESIZE, this->widget[NCLWW_DETAILS].right - this->widget[NCLWW_DETAILS].left - 5, max_y - y);

		if (this->selected->dependency_count != 0) {
			/* List dependencies */
			char buf[8192] = "";
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
			y += DrawStringMultiLine(this->widget[NCLWW_DETAILS].left + 5, y, STR_CONTENT_DETAIL_DEPENDENCIES, this->widget[NCLWW_DETAILS].right - this->widget[NCLWW_DETAILS].left - 5, max_y - y);
		}

		if (this->selected->tag_count != 0) {
			/* List all tags */
			char buf[8192] = "";
			char *p = buf;
			for (uint i = 0; i < this->selected->tag_count; i++) {
				p += seprintf(p, lastof(buf), i == 0 ? "%s" : ", %s", this->selected->tags[i]);
			}
			SetDParamStr(0, buf);
			y += DrawStringMultiLine(this->widget[NCLWW_DETAILS].left + 5, y, STR_CONTENT_DETAIL_TAGS, this->widget[NCLWW_DETAILS].right - this->widget[NCLWW_DETAILS].left - 5, max_y - y);
		}

		if (this->selected->IsSelected()) {
			/* When selected show all manually selected content that depends on this */
			ConstContentVector tree;
			_network_content_client.ReverseLookupTreeDependency(tree, this->selected);

			char buf[8192] = "";
			char *p = buf;
			for (ConstContentIterator iter = tree.Begin(); iter != tree.End(); iter++) {
				const ContentInfo *ci = *iter;
				if (ci == this->selected || ci->state != ContentInfo::SELECTED) continue;

				p += seprintf(p, lastof(buf), buf == p ? "%s" : ", %s", ci->name);
			}
			if (p != buf) {
				SetDParamStr(0, buf);
				y += DrawStringMultiLine(this->widget[NCLWW_DETAILS].left + 5, y, STR_CONTENT_DETAIL_SELECTED_BECAUSE_OF, this->widget[NCLWW_DETAILS].right - this->widget[NCLWW_DETAILS].left - 5, max_y - y);
			}
		}

		/* Draw the total download size */
		SetDParam(0, filesize);
		DrawString(this->widget[NCLWW_DETAILS].left + 5, this->widget[NCLWW_DETAILS].bottom - 12, STR_CONTENT_TOTAL_DOWNLOAD_SIZE, TC_BLACK);
	}

	virtual void OnDoubleClick(Point pt, int widget)
	{
		/* Double clicking on a line in the matrix toggles the state of the checkbox */
		if (widget != NCLWW_MATRIX) return;

		pt.x = this->widget[NCLWW_CHECKBOX].left;
		this->OnClick(pt, widget);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case NCLWW_MATRIX: {
				uint32 id_v = (pt.y - this->widget[NCLWW_MATRIX].top) / this->resize.step_height;

				if (id_v >= this->vscroll.cap) return; // click out of bounds
				id_v += this->vscroll.pos;

				if (id_v >= _network_content_client.Length()) return; // click out of bounds

				this->selected = *_network_content_client.Get(id_v);
				this->list_pos = id_v;

				if (pt.x <= this->widget[NCLWW_CHECKBOX].right) _network_content_client.ToggleSelectedState(this->selected);

				this->SetDirty();
			} break;

			case NCLWW_SELECT_ALL:
				_network_content_client.SelectAll();
				this->SetDirty();
				break;

			case NCLWW_SELECT_UPDATE:
				_network_content_client.SelectUpdate();
				this->SetDirty();
				break;

			case NCLWW_UNSELECT:
				this->SetDirty();
				break;

			case NCLWW_CANCEL:
				delete this;
				break;

			case NCLWW_DOWNLOAD:
				new NetworkContentDownloadStatusWindow();
				break;
		}
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		if (_network_content_client.Length() == 0) return ES_HANDLED;

		switch (keycode) {
			case WKC_UP:
				/* scroll up by one */
				if (this->list_pos > 0) this->list_pos--;
				break;
			case WKC_DOWN:
				/* scroll down by one */
				if (this->list_pos < (int)_network_content_client.Length() - 1) this->list_pos++;
				break;
			case WKC_PAGEUP:
				/* scroll up a page */
				this->list_pos = (this->list_pos < this->vscroll.cap) ? 0 : this->list_pos - this->vscroll.cap;
				break;
			case WKC_PAGEDOWN:
				/* scroll down a page */
				this->list_pos = min(this->list_pos + this->vscroll.cap, (int)_network_content_client.Length() - 1);
				break;
			case WKC_HOME:
				/* jump to beginning */
				this->list_pos = 0;
				break;
			case WKC_END:
				/* jump to end */
				this->list_pos = _network_content_client.Length() - 1;
				break;

			case WKC_SPACE:
				_network_content_client.ToggleSelectedState(this->selected);
				this->SetDirty();
				return ES_HANDLED;

			default: return ES_NOT_HANDLED;
		}

		this->selected = *_network_content_client.Get(this->list_pos);

		/* scroll to the new server if it is outside the current range */
		this->ScrollToSelected();

		/* redraw window */
		this->SetDirty();
		return ES_HANDLED;
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		this->vscroll.cap += delta.y / (int)this->resize.step_height;

		this->widget[NCLWW_MATRIX].data = (this->vscroll.cap << 8) + 1;

		SetVScrollCount(this, _network_content_client.Length());

		/* Make the matrix and details section grow both bigger (or smaller) */
		delta.x /= 2;
		this->widget[NCLWW_NAME].right      -= delta.x;
		this->widget[NCLWW_MATRIX].right    -= delta.x;
		this->widget[NCLWW_SCROLLBAR].left  -= delta.x;
		this->widget[NCLWW_SCROLLBAR].right -= delta.x;
		this->widget[NCLWW_DETAILS].left    -= delta.x;
	}

	virtual void OnReceiveContentInfo(const ContentInfo *rci)
	{
		SetVScrollCount(this, _network_content_client.Length());
		this->SetDirty();
	}

	virtual void OnDownloadComplete(ContentID cid)
	{
		this->SetDirty();
	}

	virtual void OnConnect(bool success)
	{
		if (!success) {
			ShowErrorMessage(INVALID_STRING_ID, STR_CONTENT_ERROR_COULD_NOT_CONNECT, 0, 0);
			delete this;
		}

		this->SetDirty();
	}
};

/** Widgets used for the content list */
static const Widget _network_content_list_widgets[] = {
/* TOP */
{   WWT_CLOSEBOX,   RESIZE_NONE,   COLOUR_LIGHT_BLUE,     0,    10,     0,    13, STR_00C5,                           STR_018B_CLOSE_WINDOW},                  // NCLWW_CLOSE
{    WWT_CAPTION,   RESIZE_RIGHT,  COLOUR_LIGHT_BLUE,    11,   449,     0,    13, STR_CONTENT_TITLE,                  STR_NULL},                               // NCLWW_CAPTION
{      WWT_PANEL,   RESIZE_RB,     COLOUR_LIGHT_BLUE,     0,   449,    14,   263, 0x0,                                STR_NULL},                               // NCLWW_BACKGROUND

/* LEFT SIDE */
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   COLOUR_WHITE,          8,    20,    22,    33, STR_EMPTY,                          STR_NULL},                               // NCLWW_CHECKBOX
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   COLOUR_WHITE,         21,   110,    22,    33, STR_CONTENT_TYPE_CAPTION,           STR_CONTENT_TYPE_CAPTION_TIP},           // NCLWW_TYPE
{ WWT_PUSHTXTBTN,   RESIZE_RIGHT,  COLOUR_WHITE,        111,   191,    22,    33, STR_CONTENT_NAME_CAPTION,           STR_CONTENT_NAME_CAPTION_TIP},           // NCLWW_NAME

{     WWT_MATRIX,   RESIZE_RB,     COLOUR_LIGHT_BLUE,     8,   190,    34,   230, (14 << 8) | 1,                      STR_CONTENT_MATRIX_TIP},                 // NCLWW_MATRIX
{  WWT_SCROLLBAR,   RESIZE_LRB,    COLOUR_LIGHT_BLUE,   191,   202,    22,   230, 0x0,                                STR_0190_SCROLL_BAR_SCROLLS_LIST},       // NCLWW_SCROLLBAR

/* RIGHT SIDE */
{      WWT_PANEL,   RESIZE_LRB,    COLOUR_LIGHT_BLUE,   210,   440,    22,   230, 0x0,                                STR_NULL},                               // NCLWW_DETAILS

/* BOTTOM */
{ WWT_PUSHTXTBTN,   RESIZE_TB,     COLOUR_WHITE,         10,   110,   238,   249, STR_CONTENT_SELECT_ALL_CAPTION,     STR_CONTENT_SELECT_ALL_CAPTION_TIP},     // NCLWW_SELECT_ALL
{ WWT_PUSHTXTBTN,   RESIZE_TB,     COLOUR_WHITE,         10,   110,   238,   249, STR_CONTENT_SELECT_UPDATES_CAPTION, STR_CONTENT_SELECT_UPDATES_CAPTION_TIP}, // NCLWW_SELECT_UPDATES
{ WWT_PUSHTXTBTN,   RESIZE_TB,     COLOUR_WHITE,        118,   218,   238,   249, STR_CONTENT_UNSELECT_ALL_CAPTION,   STR_CONTENT_UNSELECT_ALL_CAPTION_TIP},   // NCLWW_UNSELECT
{ WWT_PUSHTXTBTN,   RESIZE_LRTB,   COLOUR_WHITE,        226,   326,   238,   249, STR_012E_CANCEL,                    STR_NULL},                               // NCLWW_CANCEL
{ WWT_PUSHTXTBTN,   RESIZE_LRTB,   COLOUR_WHITE,        334,   434,   238,   249, STR_CONTENT_DOWNLOAD_CAPTION,       STR_CONTENT_DOWNLOAD_CAPTION_TIP},       // NCLWW_DOWNLOAD

{  WWT_RESIZEBOX,   RESIZE_LRTB,   COLOUR_LIGHT_BLUE,   438,   449,   252,   263, 0x0,                                STR_RESIZE_BUTTON },                     // NCLWW_RESIZE

{   WIDGETS_END},
};

/** Window description of the content list */
static const WindowDesc _network_content_list_desc = {
	WDP_CENTER, WDP_CENTER, 450, 264, 630, 460,
	WC_NETWORK_WINDOW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_network_content_list_widgets,
};

/**
 * Show the content list window with a given set of content
 * @param cv the content to show, or NULL when it has to search for itself
 * @param type the type to (only) show
 */
void ShowNetworkContentListWindow(ContentVector *cv, ContentType type)
{
#if defined(WITH_ZLIB)
	if (cv == NULL) {
		if (type == CONTENT_TYPE_END) {
			_network_content_client.RequestContentList(CONTENT_TYPE_BASE_GRAPHICS);
			_network_content_client.RequestContentList(CONTENT_TYPE_AI);
			_network_content_client.RequestContentList(CONTENT_TYPE_NEWGRF);
			_network_content_client.RequestContentList(CONTENT_TYPE_AI_LIBRARY);
		} else {
			_network_content_client.RequestContentList(type);
		}
	} else {
		_network_content_client.RequestContentList(cv, true);
	}

	new NetworkContentListWindow(&_network_content_list_desc, cv != NULL);
#else
	ShowErrorMessage(STR_CONTENT_NO_ZLIB_SUB, STR_CONTENT_NO_ZLIB, 0, 0);
	/* Connection failed... clean up the mess */
	if (cv != NULL) {
		for (ContentIterator iter = cv->Begin(); iter != cv->End(); iter++) delete *iter;
	}
#endif /* WITH_ZLIB */
}

#endif /* ENABLE_NETWORK */
