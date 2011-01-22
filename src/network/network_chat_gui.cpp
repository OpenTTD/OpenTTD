/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_chat_gui.cpp GUI for handling chat messages. */

#include <stdarg.h> /* va_list */

#ifdef ENABLE_NETWORK

#include "../stdafx.h"
#include "../gfx_func.h"
#include "../strings_func.h"
#include "../blitter/factory.hpp"
#include "../console_func.h"
#include "../video/video_driver.hpp"
#include "../table/sprites.h"
#include "../querystring_gui.h"
#include "../town.h"
#include "../window_func.h"
#include "../core/geometry_func.hpp"
#include "network.h"
#include "network_client.h"
#include "network_base.h"

#include "table/strings.h"

/* The draw buffer must be able to contain the chat message, client name and the "[All]" message,
 * some spaces and possible translations of [All] to other languages. */
assert_compile((int)DRAW_STRING_BUFFER >= (int)NETWORK_CHAT_LENGTH + NETWORK_NAME_LENGTH + 40);

static const uint NETWORK_CHAT_LINE_SPACING = 3;

struct ChatMessage {
	char message[DRAW_STRING_BUFFER];
	TextColour colour;
	uint32 remove_time;
};

/* used for chat window */
static ChatMessage *_chatmsg_list = NULL;
static bool _chatmessage_dirty = false;
static bool _chatmessage_visible = false;
static bool _chat_tab_completion_active;
static uint MAX_CHAT_MESSAGES = 0;

/* The chatbox grows from the bottom so the coordinates are pixels from
 * the left and pixels from the bottom. The height is the maximum height */
static PointDimension _chatmsg_box;
static uint8 *_chatmessage_backup = NULL;

static inline uint GetChatMessageCount()
{
	uint i = 0;
	for (; i < MAX_CHAT_MESSAGES; i++) {
		if (_chatmsg_list[i].message[0] == '\0') break;
	}

	return i;
}

/**
 * Add a text message to the 'chat window' to be shown
 * @param colour The colour this message is to be shown in
 * @param duration The duration of the chat message in seconds
 * @param message message itself in printf() style
 */
void CDECL NetworkAddChatMessage(TextColour colour, uint duration, const char *message, ...)
{
	char buf[DRAW_STRING_BUFFER];
	const char *bufp;
	va_list va;
	uint msg_count;
	uint16 lines;

	va_start(va, message);
	vsnprintf(buf, lengthof(buf), message, va);
	va_end(va);

	Utf8TrimString(buf, DRAW_STRING_BUFFER);

	/* Force linebreaks for strings that are too long */
	lines = GB(FormatStringLinebreaks(buf, lastof(buf), _chatmsg_box.width - 8), 0, 16) + 1;
	if (lines >= MAX_CHAT_MESSAGES) return;

	msg_count = GetChatMessageCount();
	/* We want to add more chat messages than there is free space for, remove 'old' */
	if (lines > MAX_CHAT_MESSAGES - msg_count) {
		int i = lines - (MAX_CHAT_MESSAGES - msg_count);
		memmove(&_chatmsg_list[0], &_chatmsg_list[i], sizeof(_chatmsg_list[0]) * (msg_count - i));
		msg_count = MAX_CHAT_MESSAGES - lines;
	}

	for (bufp = buf; lines != 0; lines--) {
		ChatMessage *cmsg = &_chatmsg_list[msg_count++];
		strecpy(cmsg->message, bufp, lastof(cmsg->message));

		/* The default colour for a message is company colour. Replace this with
		 * white for any additional lines */
		cmsg->colour = (bufp == buf && (colour & TC_IS_PALETTE_COLOUR)) ? colour : TC_WHITE;
		cmsg->remove_time = _realtime_tick + duration * 1000;

		bufp += strlen(bufp) + 1; // jump to 'next line' in the formatted string
	}

	_chatmessage_dirty = true;
}

/** Initialize all font-dependent chat box sizes. */
void NetworkReInitChatBoxSize()
{
	_chatmsg_box.y       = 3 * FONT_HEIGHT_NORMAL;
	_chatmsg_box.height  = MAX_CHAT_MESSAGES * (FONT_HEIGHT_NORMAL + NETWORK_CHAT_LINE_SPACING) + 2;
	_chatmessage_backup  = ReallocT(_chatmessage_backup, _chatmsg_box.width * _chatmsg_box.height * BlitterFactoryBase::GetCurrentBlitter()->GetBytesPerPixel());
}

void NetworkInitChatMessage()
{
	MAX_CHAT_MESSAGES    = _settings_client.gui.network_chat_box_height;

	_chatmsg_list        = ReallocT(_chatmsg_list, _settings_client.gui.network_chat_box_height);
	_chatmsg_box.x       = 10;
	_chatmsg_box.width   = _settings_client.gui.network_chat_box_width;
	NetworkReInitChatBoxSize();
	_chatmessage_visible = false;

	for (uint i = 0; i < MAX_CHAT_MESSAGES; i++) {
		_chatmsg_list[i].message[0] = '\0';
	}
}

/** Hide the chatbox */
void NetworkUndrawChatMessage()
{
	/* Sometimes we also need to hide the cursor
	 *   This is because both textmessage and the cursor take a shot of the
	 *   screen before drawing.
	 *   Now the textmessage takes his shot and paints his data before the cursor
	 *   does, so in the shot of the cursor is the screen-data of the textmessage
	 *   included when the cursor hangs somewhere over the textmessage. To
	 *   avoid wrong repaints, we undraw the cursor in that case, and everything
	 *   looks nicely ;)
	 * (and now hope this story above makes sense to you ;))
	 */
	if (_cursor.visible &&
			_cursor.draw_pos.x + _cursor.draw_size.x >= _chatmsg_box.x &&
			_cursor.draw_pos.x <= _chatmsg_box.x + _chatmsg_box.width &&
			_cursor.draw_pos.y + _cursor.draw_size.y >= _screen.height - _chatmsg_box.y - _chatmsg_box.height &&
			_cursor.draw_pos.y <= _screen.height - _chatmsg_box.y) {
		UndrawMouseCursor();
	}

	if (_chatmessage_visible) {
		Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
		int x      = _chatmsg_box.x;
		int y      = _screen.height - _chatmsg_box.y - _chatmsg_box.height;
		int width  = _chatmsg_box.width;
		int height = _chatmsg_box.height;
		if (y < 0) {
			height = max(height + y, min(_chatmsg_box.height, _screen.height));
			y = 0;
		}
		if (x + width >= _screen.width) {
			width = _screen.width - x;
		}
		if (width <= 0 || height <= 0) return;

		_chatmessage_visible = false;
		/* Put our 'shot' back to the screen */
		blitter->CopyFromBuffer(blitter->MoveTo(_screen.dst_ptr, x, y), _chatmessage_backup, width, height);
		/* And make sure it is updated next time */
		_video_driver->MakeDirty(x, y, width, height);

		_chatmessage_dirty = true;
	}
}

/** Check if a message is expired. */
void NetworkChatMessageLoop()
{
	for (uint i = 0; i < MAX_CHAT_MESSAGES; i++) {
		ChatMessage *cmsg = &_chatmsg_list[i];
		if (cmsg->message[0] == '\0') continue;

		/* Message has expired, remove from the list */
		if (cmsg->remove_time < _realtime_tick) {
			/* Move the remaining messages over the current message */
			if (i != MAX_CHAT_MESSAGES - 1) memmove(cmsg, cmsg + 1, sizeof(*cmsg) * (MAX_CHAT_MESSAGES - i - 1));

			/* Mark the last item as empty */
			_chatmsg_list[MAX_CHAT_MESSAGES - 1].message[0] = '\0';
			_chatmessage_dirty = true;

			/* Go one item back, because we moved the array 1 to the left */
			i--;
		}
	}
}

/** Draw the chat message-box */
void NetworkDrawChatMessage()
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	if (!_chatmessage_dirty) return;

	/* First undraw if needed */
	NetworkUndrawChatMessage();

	if (_iconsole_mode == ICONSOLE_FULL) return;

	/* Check if we have anything to draw at all */
	uint count = GetChatMessageCount();
	if (count == 0) return;

	int x      = _chatmsg_box.x;
	int y      = _screen.height - _chatmsg_box.y - _chatmsg_box.height;
	int width  = _chatmsg_box.width;
	int height = _chatmsg_box.height;
	if (y < 0) {
		height = max(height + y, min(_chatmsg_box.height, _screen.height));
		y = 0;
	}
	if (x + width >= _screen.width) {
		width = _screen.width - x;
	}
	if (width <= 0 || height <= 0) return;

	assert(blitter->BufferSize(width, height) <= (int)(_chatmsg_box.width * _chatmsg_box.height * blitter->GetBytesPerPixel()));

	/* Make a copy of the screen as it is before painting (for undraw) */
	blitter->CopyToBuffer(blitter->MoveTo(_screen.dst_ptr, x, y), _chatmessage_backup, width, height);

	_cur_dpi = &_screen; // switch to _screen painting

	/* Paint a half-transparent box behind the chat messages */
	GfxFillRect(
			_chatmsg_box.x,
			_screen.height - _chatmsg_box.y - count * (FONT_HEIGHT_NORMAL + NETWORK_CHAT_LINE_SPACING) - 2,
			_chatmsg_box.x + _chatmsg_box.width - 1,
			_screen.height - _chatmsg_box.y - 2,
			PALETTE_TO_TRANSPARENT, FILLRECT_RECOLOUR // black, but with some alpha for background
		);

	/* Paint the chat messages starting with the lowest at the bottom */
	for (uint y = FONT_HEIGHT_NORMAL + NETWORK_CHAT_LINE_SPACING; count-- != 0; y += (FONT_HEIGHT_NORMAL + NETWORK_CHAT_LINE_SPACING)) {
		DrawString(_chatmsg_box.x + 3, _chatmsg_box.x + _chatmsg_box.width - 1, _screen.height - _chatmsg_box.y - y + 1, _chatmsg_list[count].message, _chatmsg_list[count].colour);
	}

	/* Make sure the data is updated next flush */
	_video_driver->MakeDirty(x, y, width, height);

	_chatmessage_visible = true;
	_chatmessage_dirty = false;
}


static void SendChat(const char *buf, DestType type, int dest)
{
	if (StrEmpty(buf)) return;
	if (!_network_server) {
		MyClient::SendChat((NetworkAction)(NETWORK_ACTION_CHAT + type), type, dest, buf, 0);
	} else {
		NetworkServerSendChat((NetworkAction)(NETWORK_ACTION_CHAT + type), type, dest, buf, CLIENT_ID_SERVER);
	}
}

/** Widget numbers of the chat window. */
enum NetWorkChatWidgets {
	NWCW_CLOSE,
	NWCW_BACKGROUND,
	NWCW_DESTINATION,
	NWCW_TEXTBOX,
	NWCW_SENDBUTTON,
};

struct NetworkChatWindow : public QueryStringBaseWindow {
	DestType dtype;
	StringID dest_string;
	int dest;

	NetworkChatWindow(const WindowDesc *desc, DestType type, int dest) : QueryStringBaseWindow(NETWORK_CHAT_LENGTH)
	{
		this->dtype   = type;
		this->dest    = dest;
		this->afilter = CS_ALPHANUMERAL;
		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, 0);

		static const StringID chat_captions[] = {
			STR_NETWORK_CHAT_ALL_CAPTION,
			STR_NETWORK_CHAT_COMPANY_CAPTION,
			STR_NETWORK_CHAT_CLIENT_CAPTION
		};
		assert((uint)this->dtype < lengthof(chat_captions));
		this->dest_string = chat_captions[this->dtype];

		this->InitNested(desc, type);

		this->SetFocusedWidget(NWCW_TEXTBOX);
		InvalidateWindowData(WC_NEWS_WINDOW, 0, this->height);
		_chat_tab_completion_active = false;
	}

	~NetworkChatWindow()
	{
		InvalidateWindowData(WC_NEWS_WINDOW, 0, 0);
	}

	/**
	 * Find the next item of the list of things that can be auto-completed.
	 * @param item The current indexed item to return. This function can, and most
	 *     likely will, alter item, to skip empty items in the arrays.
	 * @return Returns the char that matched to the index.
	 */
	const char *ChatTabCompletionNextItem(uint *item)
	{
		static char chat_tab_temp_buffer[64];

		/* First, try clients */
		if (*item < MAX_CLIENT_SLOTS) {
			/* Skip inactive clients */
			NetworkClientInfo *ci;
			FOR_ALL_CLIENT_INFOS_FROM(ci, *item) {
				*item = ci->index;
				return ci->client_name;
			}
			*item = MAX_CLIENT_SLOTS;
		}

		/* Then, try townnames
		 * Not that the following assumes all town indices are adjacent, ie no
		 * towns have been deleted. */
		if (*item < (uint)MAX_CLIENT_SLOTS + Town::GetPoolSize()) {
			const Town *t;

			FOR_ALL_TOWNS_FROM(t, *item - MAX_CLIENT_SLOTS) {
				/* Get the town-name via the string-system */
				SetDParam(0, t->index);
				GetString(chat_tab_temp_buffer, STR_TOWN_NAME, lastof(chat_tab_temp_buffer));
				return &chat_tab_temp_buffer[0];
			}
		}

		return NULL;
	}

	/**
	 * Find what text to complete. It scans for a space from the left and marks
	 *  the word right from that as to complete. It also writes a \0 at the
	 *  position of the space (if any). If nothing found, buf is returned.
	 */
	static char *ChatTabCompletionFindText(char *buf)
	{
		char *p = strrchr(buf, ' ');
		if (p == NULL) return buf;

		*p = '\0';
		return p + 1;
	}

	/**
	 * See if we can auto-complete the current text of the user.
	 */
	void ChatTabCompletion()
	{
		static char _chat_tab_completion_buf[NETWORK_CHAT_LENGTH];
		assert(this->edit_str_size == lengthof(_chat_tab_completion_buf));

		Textbuf *tb = &this->text;
		size_t len, tb_len;
		uint item;
		char *tb_buf, *pre_buf;
		const char *cur_name;
		bool second_scan = false;

		item = 0;

		/* Copy the buffer so we can modify it without damaging the real data */
		pre_buf = (_chat_tab_completion_active) ? strdup(_chat_tab_completion_buf) : strdup(tb->buf);

		tb_buf  = ChatTabCompletionFindText(pre_buf);
		tb_len  = strlen(tb_buf);

		while ((cur_name = ChatTabCompletionNextItem(&item)) != NULL) {
			item++;

			if (_chat_tab_completion_active) {
				/* We are pressing TAB again on the same name, is there another name
				 *  that starts with this? */
				if (!second_scan) {
					size_t offset;
					size_t length;

					/* If we are completing at the begin of the line, skip the ': ' we added */
					if (tb_buf == pre_buf) {
						offset = 0;
						length = (tb->max_bytes - 1) - 2;
					} else {
						/* Else, find the place we are completing at */
						offset = strlen(pre_buf) + 1;
						length = (tb->max_bytes - 1) - offset;
					}

					/* Compare if we have a match */
					if (strlen(cur_name) == length && strncmp(cur_name, tb->buf + offset, length) == 0) second_scan = true;

					continue;
				}

				/* Now any match we make on _chat_tab_completion_buf after this, is perfect */
			}

			len = strlen(cur_name);
			if (tb_len < len && strncasecmp(cur_name, tb_buf, tb_len) == 0) {
				/* Save the data it was before completion */
				if (!second_scan) snprintf(_chat_tab_completion_buf, lengthof(_chat_tab_completion_buf), "%s", tb->buf);
				_chat_tab_completion_active = true;

				/* Change to the found name. Add ': ' if we are at the start of the line (pretty) */
				if (pre_buf == tb_buf) {
					snprintf(tb->buf, this->edit_str_size, "%s: ", cur_name);
				} else {
					snprintf(tb->buf, this->edit_str_size, "%s %s", pre_buf, cur_name);
				}

				/* Update the textbuffer */
				UpdateTextBufferSize(&this->text);

				this->SetDirty();
				free(pre_buf);
				return;
			}
		}

		if (second_scan) {
			/* We walked all posibilities, and the user presses tab again.. revert to original text */
			strcpy(tb->buf, _chat_tab_completion_buf);
			_chat_tab_completion_active = false;

			/* Update the textbuffer */
			UpdateTextBufferSize(&this->text);

			this->SetDirty();
		}
		free(pre_buf);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
		this->DrawEditBox(NWCW_TEXTBOX);
	}

	virtual Point OnInitialPosition(const WindowDesc *desc, int16 sm_width, int16 sm_height, int window_number)
	{
		Point pt = { (_screen.width - max(sm_width, desc->default_width)) / 2, _screen.height - sm_height - FindWindowById(WC_STATUS_BAR, 0)->height };
		return pt;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != NWCW_DESTINATION) return;

		if (this->dtype == DESTTYPE_CLIENT) {
			SetDParamStr(0, NetworkFindClientInfoFromClientID((ClientID)this->dest)->client_name);
		}
		Dimension d = GetStringBoundingBox(this->dest_string);
		d.width  += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
		d.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
		*size = maxdim(*size, d);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != NWCW_DESTINATION) return;

		if (this->dtype == DESTTYPE_CLIENT) {
			SetDParamStr(0, NetworkFindClientInfoFromClientID((ClientID)this->dest)->client_name);
		}
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, this->dest_string, TC_BLACK, SA_RIGHT);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			/* Send */
			case NWCW_SENDBUTTON: SendChat(this->text.buf, this->dtype, this->dest);
				/* FALL THROUGH */
			case NWCW_CLOSE: /* Cancel */ delete this; break;
		}
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(NWCW_TEXTBOX);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		EventState state = ES_NOT_HANDLED;
		if (keycode == WKC_TAB) {
			ChatTabCompletion();
			state = ES_HANDLED;
		} else {
			_chat_tab_completion_active = false;
			switch (this->HandleEditBoxKey(NWCW_TEXTBOX, key, keycode, state)) {
				default: NOT_REACHED();
				case HEBR_EDITING: {
					Window *osk = FindWindowById(WC_OSK, 0);
					if (osk != NULL && osk->parent == this) osk->InvalidateData();
					break;
				}
				case HEBR_CONFIRM:
					SendChat(this->text.buf, this->dtype, this->dest);
					/* FALL THROUGH */
				case HEBR_CANCEL: delete this; break;
				case HEBR_NOT_FOCUSED: break;
			}
		}
		return state;
	}

	virtual void OnOpenOSKWindow(int wid)
	{
		ShowOnScreenKeyboard(this, wid, NWCW_CLOSE, NWCW_SENDBUTTON);
	}

	virtual void OnInvalidateData(int data)
	{
		if (data == this->dest) delete this;
	}
};

static const NWidgetPart _nested_chat_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, NWCW_CLOSE),
		NWidget(WWT_PANEL, COLOUR_GREY, NWCW_BACKGROUND),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXT, COLOUR_GREY, NWCW_DESTINATION), SetMinimalSize(62, 12), SetPadding(1, 0, 1, 0), SetDataTip(STR_NULL, STR_NULL),
				NWidget(WWT_EDITBOX, COLOUR_GREY, NWCW_TEXTBOX), SetMinimalSize(100, 12), SetPadding(1, 0, 1, 0), SetResize(1, 0),
																	SetDataTip(STR_NETWORK_CHAT_OSKTITLE, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, NWCW_SENDBUTTON), SetMinimalSize(62, 12), SetPadding(1, 0, 1, 0), SetDataTip(STR_NETWORK_CHAT_SEND, STR_NULL),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _chat_window_desc(
	WDP_MANUAL, 640, 14, // x, y, width, height
	WC_SEND_NETWORK_MSG, WC_NONE,
	0,
	_nested_chat_window_widgets, lengthof(_nested_chat_window_widgets)
);

void ShowNetworkChatQueryWindow(DestType type, int dest)
{
	DeleteWindowByClass(WC_SEND_NETWORK_MSG);
	new NetworkChatWindow(&_chat_window_desc, type, dest);
}

#endif /* ENABLE_NETWORK */
