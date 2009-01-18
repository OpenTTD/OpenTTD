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
	char name[32]; ///< The current name of the downloaded file

public:
	/**
	 * Create a new download window based on a list of content information
	 * with flags whether to download them or not.
	 * @param infos the list to search in
	 */
	NetworkContentDownloadStatusWindow(ContentVector &infos) :
		Window(&_network_content_download_status_window_desc),
		cur_id(UINT32_MAX)
	{
		this->parent = FindWindowById(WC_NETWORK_WINDOW, 1);
		this->connection = NetworkContent_Connect(this);

		if (this->connection == NULL) {
			/* When the connection got broken and we can't rebuild it we can't do much :( */
			ShowErrorMessage(STR_CONTENT_ERROR_COULD_NOT_DOWNLOAD, STR_CONTENT_ERROR_COULD_NOT_DOWNLOAD_CONNECTION_LOST, 0, 0);
			delete this;
			return;
		}

		/** Make the list of items to download */
		uint32 *ids = MallocT<uint32>(infos.Length());
		for (ContentIterator iter = infos.Begin(); iter != infos.End(); iter++) {
			const ContentInfo *ci = *iter;
			if (!ci->IsSelected()) continue;

			ids[this->total_files++] = ci->id;
			this->total_bytes += ci->filesize;
		}

		/** And request them for download */
		this->connection->RequestContent(this->total_files, ids);
		free(ids);

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

		NetworkContent_Disconnect(this);
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
			DrawStringCentered(this->width / 2, 46, STR_CONTENT_DOWNLOAD_COMPLETE, TC_GREY);
		} else if (!StrEmpty(this->name)) {
			SetDParamStr(0, this->name);
			SetDParam(1, this->downloaded_files);
			SetDParam(2, this->total_files);
			DrawStringCentered(this->width / 2, 46, STR_CONTENT_DOWNLOAD_FILE, TC_GREY);
		} else {
			DrawStringCentered(this->width / 2, 46, STR_CONTENT_DOWNLOAD_INITIALISE, TC_GREY);
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget == NCDSWW_CANCELOK) delete this;
	}

	virtual void OnDownloadProgress(ContentInfo *ci, uint bytes)
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

	ClientNetworkContentSocketHandler *connection; ///< Connection with the content server
	SmallVector<ContentID, 4> requested;           ///< ContentIDs we already requested (so we don't do it again)
	ContentVector infos;                           ///< All content info we received
	ContentInfo *selected;                         ///< The selected content info
	int list_pos;                                  ///< Our position in the list

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

	/**
	 * Download information of a given Content ID if not already tried
	 * @param cid the ID to try
	 */
	void DownloadContentInfo(ContentID cid)
	{
		/* When we tried to download it already, don't try again */
		if (this->requested.Contains(cid)) return;

		*this->requested.Append() = cid;
		assert(this->requested.Contains(cid));
		this->connection->RequestContentList(1, &cid);
	}


	/**
	 * Get the content info based on a ContentID
	 * @param cid the ContentID to search for
	 * @return the ContentInfo or NULL if not found
	 */
	ContentInfo *GetContent(ContentID cid)
	{
		for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
			ContentInfo *ci = *iter;
			if (ci->id == cid) return ci;
		}
		return NULL;
	}

	/**
	 * Reverse lookup the dependencies of (direct) parents over a given child.
	 * @param parents list to store all parents in (is not cleared)
	 * @param child   the child to search the parents' dependencies for
	 */
	void ReverseLookupDependency(ContentVector &parents, ContentInfo *child)
	{
		for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
			ContentInfo *ci = *iter;
			if (ci == child) continue;

			for (uint i = 0; i < ci->dependency_count; i++) {
				if (ci->dependencies[i] == child->id) {
					*parents.Append() = ci;
					break;
				}
			}
		}
	}

	/**
	 * Reverse lookup the dependencies of all parents over a given child.
	 * @param tree  list to store all parents in (is not cleared)
	 * @param child the child to search the parents' dependencies for
	 */
	void ReverseLookupTreeDependency(ContentVector &tree, ContentInfo *child)
	{
		*tree.Append() = child;

		/* First find all direct parents */
		for (ContentIterator iter = tree.Begin(); iter != tree.End(); iter++) {
			ContentVector parents;
			ReverseLookupDependency(parents, *iter);

			for (ContentIterator piter = parents.Begin(); piter != parents.End(); piter++) {
				tree.Include(*piter);
			}
		}
	}

	/**
	 * Check the dependencies (recursively) of this content info
	 * @param ci the content info to check the dependencies of
	 */
	void CheckDependencyState(ContentInfo *ci)
	{
		if (ci->IsSelected() || ci->state == ContentInfo::ALREADY_HERE) {
			/* Selection is easy; just walk all children and set the
			 * autoselected state. That way we can see what we automatically
			 * selected and thus can unselect when a dependency is removed. */
			for (uint i = 0; i < ci->dependency_count; i++) {
				ContentInfo *c = this->GetContent(ci->dependencies[i]);
				if (c == NULL) {
					DownloadContentInfo(ci->dependencies[i]);
				} else if (c->state == ContentInfo::UNSELECTED) {
					c->state = ContentInfo::AUTOSELECTED;
					this->CheckDependencyState(c);
				}
			}
			return;
		}

		if (ci->state != ContentInfo::UNSELECTED) return;

		/* For unselection we need to find the parents of us. We need to
		 * unselect them. After that we unselect all children that we
		 * depend on and are not used as dependency for us, but only when
		 * we automatically selected them. */
		ContentVector parents;
		ReverseLookupDependency(parents, ci);
		for (ContentIterator iter = parents.Begin(); iter != parents.End(); iter++) {
			ContentInfo *c = *iter;
			if (!c->IsSelected()) continue;

			c->state = ContentInfo::UNSELECTED;
			CheckDependencyState(c);
		}

		for (uint i = 0; i < ci->dependency_count; i++) {
			ContentInfo *c = this->GetContent(ci->dependencies[i]);
			if (c == NULL) {
				DownloadContentInfo(ci->dependencies[i]);
				continue;
			}
			if (c->state != ContentInfo::AUTOSELECTED) continue;

			/* Only unselect when WE are the only parent. */
			parents.Clear();
			ReverseLookupDependency(parents, c);

			/* First check whether anything depends on us */
			int sel_count = 0;
			bool force_selection = false;
			for (ContentIterator iter = parents.Begin(); iter != parents.End(); iter++) {
				if ((*iter)->IsSelected()) sel_count++;
				if ((*iter)->state == ContentInfo::SELECTED) force_selection = true;
			}
			if (sel_count == 0) {
				/* Nothing depends on us */
				c->state = ContentInfo::UNSELECTED;
				this->CheckDependencyState(c);
				continue;
			}
			/* Something manually selected depends directly on us */
			if (force_selection) continue;

			/* "Flood" search to find all items in the dependency graph*/
			parents.Clear();
			ReverseLookupTreeDependency(parents, c);

			/* Is there anything that is "force" selected?, if so... we're done. */
			for (ContentIterator iter = parents.Begin(); iter != parents.End(); iter++) {
				if ((*iter)->state != ContentInfo::SELECTED) continue;

				force_selection = true;
				break;
			}

			/* So something depended directly on us */
			if (force_selection) continue;

			/* Nothing depends on us, mark the whole graph as unselected.
			 * After that's done run over them once again to test their children
			 * to unselect. Don't do it immediatelly because it'll do exactly what
			 * we're doing now. */
			for (ContentIterator iter = parents.Begin(); iter != parents.End(); iter++) {
				ContentInfo *c = *iter;
				if (c->state == ContentInfo::AUTOSELECTED) c->state = ContentInfo::UNSELECTED;
			}
			for (ContentIterator iter = parents.Begin(); iter != parents.End(); iter++) {
				this->CheckDependencyState(*iter);
			}
		}
	}

	/** Toggle the state of a content info and check it's dependencies */
	void ToggleSelectedState()
	{
		switch (this->selected->state) {
			case ContentInfo::SELECTED:
			case ContentInfo::AUTOSELECTED:
				this->selected->state = ContentInfo::UNSELECTED;
				break;

			case ContentInfo::UNSELECTED:
				this->selected->state = ContentInfo::SELECTED;
				break;

			default:
				return;
		}

		this->CheckDependencyState(this->selected);
	}

public:
	/**
	 * Create the content list window.
	 * @param desc the window description to pass to Window's constructor.
	 * @param cv the list with content to show; if NULL find content ourselves
	 */
	NetworkContentListWindow(const WindowDesc *desc, ContentVector *cv, ContentType type) : Window(desc, 1), selected(NULL), list_pos(0)
	{
		this->connection = NetworkContent_Connect(this);

		this->vscroll.cap = 14;
		this->resize.step_height = 14;
		this->resize.step_width = 2;

		if (cv == NULL) {
			if (type == CONTENT_TYPE_END) {
				this->connection->RequestContentList(CONTENT_TYPE_BASE_GRAPHICS);
				this->connection->RequestContentList(CONTENT_TYPE_NEWGRF);
				this->connection->RequestContentList(CONTENT_TYPE_AI);
			} else {
				this->connection->RequestContentList(type);
			}
			this->HideWidget(NCLWW_SELECT_ALL);
		} else {
			this->connection->RequestContentList(cv, true);

			for (ContentIterator iter = cv->Begin(); iter != cv->End(); iter++) {
				*this->infos.Append() = *iter;
			}
			this->HideWidget(NCLWW_SELECT_UPDATE);
		}

		this->FindWindowPlacementAndResize(desc);
	}

	/** Free everything we allocated */
	~NetworkContentListWindow()
	{
		NetworkContent_Disconnect(this);

		for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) delete *iter;
	}

	virtual void OnPaint()
	{
		/* To sum all the bytes we intend to download */
		uint filesize = 0;
		bool show_select_all = false;
		bool show_select_update = false;
		for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
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
		for (ContentIterator iter = this->infos.Get(this->vscroll.pos); iter != this->infos.End() && cnt < this->vscroll.cap; iter++, cnt++) {
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
				ContentIterator iter = this->infos.Begin();
				for (; iter != this->infos.End(); iter++) {
					const ContentInfo *ci = *iter;
					if (ci->id != cid) continue;

					p += seprintf(p, lastof(buf), p == buf ? "%s" : ", %s", (*iter)->name);
					break;
				}

				/* We miss the dependency, but we'll only request it if not done before. */
				if (iter == this->infos.End()) DownloadContentInfo(cid);
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
			ContentVector tree;
			ReverseLookupTreeDependency(tree, this->selected);

			char buf[8192] = "";
			char *p = buf;
			for (ContentIterator iter = tree.Begin(); iter != tree.End(); iter++) {
				ContentInfo *ci = *iter;
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

				if (id_v >= this->infos.Length()) return; // click out of bounds

				this->selected = this->infos[id_v];
				this->list_pos = id_v;

				if (pt.x <= this->widget[NCLWW_CHECKBOX].right) this->ToggleSelectedState();

				this->SetDirty();
			} break;

			case NCLWW_SELECT_ALL:
			case NCLWW_SELECT_UPDATE:
				for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
					ContentInfo *ci = *iter;
					if (ci->state == ContentInfo::UNSELECTED && (widget == NCLWW_SELECT_ALL || ci->update)) {
						ci->state = ContentInfo::SELECTED;
						CheckDependencyState(ci);
					}
				}
				this->SetDirty();
				break;

			case NCLWW_UNSELECT:
				for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
					ContentInfo *ci = *iter;
					/* No need to check dependencies; when everything's off nothing can depend */
					if (ci->IsSelected()) ci->state = ContentInfo::UNSELECTED;
				}
				this->SetDirty();
				break;

			case NCLWW_CANCEL:
				delete this;
				break;

			case NCLWW_DOWNLOAD:
				new NetworkContentDownloadStatusWindow(this->infos);
				break;
		}
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		if (this->infos.Length() == 0) return ES_HANDLED;

		switch (keycode) {
			case WKC_UP:
				/* scroll up by one */
				if (this->list_pos > 0) this->list_pos--;
				break;
			case WKC_DOWN:
				/* scroll down by one */
				if (this->list_pos < (int)this->infos.Length() - 1) this->list_pos++;
				break;
			case WKC_PAGEUP:
				/* scroll up a page */
				this->list_pos = (this->list_pos < this->vscroll.cap) ? 0 : this->list_pos - this->vscroll.cap;
				break;
			case WKC_PAGEDOWN:
				/* scroll down a page */
				this->list_pos = min(this->list_pos + this->vscroll.cap, (int)this->infos.Length() - 1);
				break;
			case WKC_HOME:
				/* jump to beginning */
				this->list_pos = 0;
				break;
			case WKC_END:
				/* jump to end */
				this->list_pos = this->infos.Length() - 1;
				break;

			case WKC_SPACE:
				this->ToggleSelectedState();
				this->SetDirty();
				return ES_HANDLED;

			default: return ES_NOT_HANDLED;
		}

		this->selected = this->infos[this->list_pos];

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

		SetVScrollCount(this, this->infos.Length());

		/* Make the matrix and details section grow both bigger (or smaller) */
		delta.x /= 2;
		this->widget[NCLWW_NAME].right      -= delta.x;
		this->widget[NCLWW_MATRIX].right    -= delta.x;
		this->widget[NCLWW_SCROLLBAR].left  -= delta.x;
		this->widget[NCLWW_SCROLLBAR].right -= delta.x;
		this->widget[NCLWW_DETAILS].left    -= delta.x;
	}

	virtual void OnReceiveContentInfo(ContentInfo *rci)
	{
		/* Do we already have a stub for this? */
		for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
			ContentInfo *ci = *iter;
			if (ci->type == rci->type && ci->unique_id == rci->unique_id &&
					memcmp(ci->md5sum, rci->md5sum, sizeof(ci->md5sum)) == 0) {
				/* Preserve the name if possible */
				if (StrEmpty(rci->name)) strecpy(rci->name, ci->name, lastof(rci->name));

				delete ci;
				*iter = rci;

				this->SetDirty();
				return;
			}
		}

		/* Missing content info? Don't list it */
		if (rci->filesize == 0) {
			delete rci;
			return;
		}

		/* Nothing selected, lets select something */
		if (this->selected == NULL) this->selected = rci;

		*this->infos.Append() = rci;

		/* Incoming data means that we might need to reconsider dependencies */
		for (ContentIterator iter = this->infos.Begin(); iter != this->infos.End(); iter++) {
			CheckDependencyState(*iter);
		}

		this->SetDirty();
	}

	virtual void OnDownloadComplete(ContentID cid)
	{
		/* When we know something that completed downloading, we have it! */
		ContentInfo *ci = this->GetContent(cid);
		if (ci != NULL) {
			ci->state = ContentInfo::ALREADY_HERE;
			this->SetDirty();
		}
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

{     WWT_MATRIX,   RESIZE_RB,     COLOUR_LIGHT_BLUE,     8,   190,    34,   230, (14 << 8) | 1,                      STR_NETWORK_CLICK_GAME_TO_SELECT},       // NCLWW_MATRIX
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
	if (NetworkContent_Connect(NULL)) {
		DeleteWindowById(WC_NETWORK_WINDOW, 1);
		new NetworkContentListWindow(&_network_content_list_desc, cv, type);
		NetworkContent_Disconnect(NULL);
	} else {
		ShowErrorMessage(INVALID_STRING_ID, STR_CONTENT_ERROR_COULD_NOT_CONNECT, 0, 0);
#else
	{
		ShowErrorMessage(STR_CONTENT_NO_ZLIB_SUB, STR_CONTENT_NO_ZLIB, 0, 0);
#endif /* WITH_ZLIB */
		/* Connection failed... clean up the mess */
		if (cv != NULL) {
			for (ContentIterator iter = cv->Begin(); iter != cv->End(); iter++) delete *iter;
		}
	}
}

#endif /* ENABLE_NETWORK */
