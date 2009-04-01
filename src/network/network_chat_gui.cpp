/* $Id$ */

/** @file network_chat_gui.cpp GUI for handling chat messages. */

#include <stdarg.h> /* va_list */

#ifdef ENABLE_NETWORK

#include "../stdafx.h"
#include "../date_func.h"
#include "../gfx_func.h"
#include "../strings_func.h"
#include "../blitter/factory.hpp"
#include "../console_func.h"
#include "../video/video_driver.hpp"
#include "../table/sprites.h"
#include "../querystring_gui.h"
#include "../town.h"
#include "../window_func.h"
#include "network_internal.h"
#include "network_client.h"

#include "table/strings.h"

/* The draw buffer must be able to contain the chat message, client name and the "[All]" message,
 * some spaces and possible translations of [All] to other languages. */
assert_compile((int)DRAW_STRING_BUFFER >= (int)NETWORK_CHAT_LENGTH + NETWORK_NAME_LENGTH + 40);

enum {
	NETWORK_CHAT_LINE_HEIGHT = 13,
};

struct ChatMessage {
	char message[DRAW_STRING_BUFFER];
	TextColour colour;
	Date end_date;
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
 * @param duration The duration of the chat message in game-days
 * @param message message itself in printf() style
 */
void CDECL NetworkAddChatMessage(TextColour colour, uint8 duration, const char *message, ...)
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
	lines = GB(FormatStringLinebreaks(buf, _chatmsg_box.width - 8), 0, 16) + 1;
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
		cmsg->colour = (bufp == buf && colour & IS_PALETTE_COLOUR) ? colour : TC_WHITE;
		cmsg->end_date = _date + duration;

		bufp += strlen(bufp) + 1; // jump to 'next line' in the formatted string
	}

	_chatmessage_dirty = true;
}

void NetworkInitChatMessage()
{
	MAX_CHAT_MESSAGES   = _settings_client.gui.network_chat_box_height;

	_chatmsg_list       = ReallocT(_chatmsg_list, _settings_client.gui.network_chat_box_height);
	_chatmsg_box.x      = 10;
	_chatmsg_box.y      = 30;
	_chatmsg_box.width  = _settings_client.gui.network_chat_box_width;
	_chatmsg_box.height = _settings_client.gui.network_chat_box_height * NETWORK_CHAT_LINE_HEIGHT + 2;
	_chatmessage_backup = ReallocT(_chatmessage_backup, _chatmsg_box.width * _chatmsg_box.height * BlitterFactoryBase::GetCurrentBlitter()->GetBytesPerPixel());

	for (uint i = 0; i < MAX_CHAT_MESSAGES; i++) {
		_chatmsg_list[i].message[0] = '\0';
	}
}

/** Hide the chatbox */
void NetworkUndrawChatMessage()
{
	if (_chatmessage_visible) {
		Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
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

		if (_cursor.visible) {
			if (_cursor.draw_pos.x + _cursor.draw_size.x >= _chatmsg_box.x &&
				_cursor.draw_pos.x <= _chatmsg_box.x + _chatmsg_box.width &&
				_cursor.draw_pos.y + _cursor.draw_size.y >= _screen.height - _chatmsg_box.y - _chatmsg_box.height &&
				_cursor.draw_pos.y <= _screen.height - _chatmsg_box.y) {
				UndrawMouseCursor();
			}
		}

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

/** Check if a message is expired every day */
void NetworkChatMessageDailyLoop()
{
	for (uint i = 0; i < MAX_CHAT_MESSAGES; i++) {
		ChatMessage *cmsg = &_chatmsg_list[i];
		if (cmsg->message[0] == '\0') continue;

		/* Message has expired, remove from the list */
		if (cmsg->end_date < _date) {
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
			_screen.height - _chatmsg_box.y - count * NETWORK_CHAT_LINE_HEIGHT - 2,
			_chatmsg_box.x + _chatmsg_box.width - 1,
			_screen.height - _chatmsg_box.y - 2,
			PALETTE_TO_TRANSPARENT, FILLRECT_RECOLOUR // black, but with some alpha for background
		);

	/* Paint the chat messages starting with the lowest at the bottom */
	for (uint y = NETWORK_CHAT_LINE_HEIGHT; count-- != 0; y += NETWORK_CHAT_LINE_HEIGHT) {
		DoDrawString(_chatmsg_list[count].message, _chatmsg_box.x + 3, _screen.height - _chatmsg_box.y - y + 1, _chatmsg_list[count].colour);
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
		SEND_COMMAND(PACKET_CLIENT_CHAT)((NetworkAction)(NETWORK_ACTION_CHAT + type), type, dest, buf, 0);
	} else {
		NetworkServerSendChat((NetworkAction)(NETWORK_ACTION_CHAT + type), type, dest, buf, CLIENT_ID_SERVER);
	}
}

struct NetworkChatWindow : public QueryStringBaseWindow {
private:
	enum NetWorkChatWidgets {
		NWCW_CLOSE,
		NWCW_BACKGROUND,
		NWCW_TEXTBOX,
		NWCW_SENDBUTTON,
	};

public:
	DestType dtype;
	int dest;

	NetworkChatWindow (const WindowDesc *desc, DestType type, int dest) : QueryStringBaseWindow(NETWORK_CHAT_LENGTH, desc)
	{
		this->LowerWidget(NWCW_TEXTBOX);
		this->dtype   = type;
		this->dest    = dest;
		this->afilter = CS_ALPHANUMERAL;
		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, 0);
		this->SetFocusedWidget(NWCW_TEXTBOX);

		InvalidateWindowData(WC_NEWS_WINDOW, 0, this->height);

		_chat_tab_completion_active = false;

		this->FindWindowPlacementAndResize(desc);
	}

	~NetworkChatWindow ()
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
			if (*item < GetNetworkClientInfoPoolSize()) {
				/* Skip inactive clients */
				NetworkClientInfo *ci;
				FOR_ALL_CLIENT_INFOS_FROM(ci, *item) break;
				if (ci != NULL) {
					*item = ci->index;
					return ci->client_name;
				}
			}
			*item = MAX_CLIENT_SLOTS;
		}

		/* Then, try townnames
		 * Not that the following assumes all town indices are adjacent, ie no
		 * towns have been deleted. */
		if (*item <= (uint)MAX_CLIENT_SLOTS + GetMaxTownIndex()) {
			const Town *t;

			FOR_ALL_TOWNS_FROM(t, *item - MAX_CLIENT_SLOTS) {
				/* Get the town-name via the string-system */
				SetDParam(0, t->index);
				GetString(chat_tab_temp_buffer, STR_TOWN, lastof(chat_tab_temp_buffer));
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
				/* We are pressing TAB again on the same name, is there an other name
				 *  that starts with this? */
				if (!second_scan) {
					size_t offset;
					size_t length;

					/* If we are completing at the begin of the line, skip the ': ' we added */
					if (tb_buf == pre_buf) {
						offset = 0;
						length = (tb->size - 1) - 2;
					} else {
						/* Else, find the place we are completing at */
						offset = strlen(pre_buf) + 1;
						length = (tb->size - 1) - offset;
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
		static const StringID chat_captions[] = {
			STR_NETWORK_CHAT_ALL_CAPTION,
			STR_NETWORK_CHAT_COMPANY_CAPTION,
			STR_NETWORK_CHAT_CLIENT_CAPTION
		};

		this->DrawWidgets();

		assert((uint)this->dtype < lengthof(chat_captions));
		DrawStringRightAligned(this->widget[NWCW_TEXTBOX].left - 2, this->widget[NWCW_TEXTBOX].top + 1, chat_captions[this->dtype], TC_BLACK);
		this->DrawEditBox(NWCW_TEXTBOX);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			/* Send */
			case NWCW_SENDBUTTON: SendChat(this->text.buf, this->dtype, this->dest);
			/* FALLTHROUGH */
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
					if (osk != NULL && osk->parent == this) osk->OnInvalidateData();
				} break;
				case HEBR_CONFIRM:
					SendChat(this->text.buf, this->dtype, this->dest);
				/* FALLTHROUGH */
				case HEBR_CANCEL: delete this; break;
				case HEBR_NOT_FOCUSED: break;
			}
		}
		return state;
	}

	virtual void OnOpenOSKWindow(int wid)
	{
		ShowOnScreenKeyboard(this, wid, 0, 3);
	}
};

static const Widget _chat_window_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE,   COLOUR_GREY,   0,  10,  0, 13, STR_00C5,                  STR_018B_CLOSE_WINDOW},
{      WWT_PANEL, RESIZE_RIGHT,  COLOUR_GREY,  11, 319,  0, 13, 0x0,                       STR_NULL}, // background
{    WWT_EDITBOX, RESIZE_RIGHT,  COLOUR_GREY,  75, 257,  1, 12, STR_NETWORK_CHAT_OSKTITLE, STR_NULL}, // text box
{ WWT_PUSHTXTBTN, RESIZE_LR,     COLOUR_GREY, 258, 319,  1, 12, STR_NETWORK_SEND,          STR_NULL}, // send button
{   WIDGETS_END},
};

static const WindowDesc _chat_window_desc(
	WDP_CENTER, -26, 320, 14, 640, 14, // x, y, width, height
	WC_SEND_NETWORK_MSG, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET,
	_chat_window_widgets
);

void ShowNetworkChatQueryWindow(DestType type, int dest)
{
	DeleteWindowById(WC_SEND_NETWORK_MSG, 0);
	new NetworkChatWindow (&_chat_window_desc, type, dest);
}

#endif /* ENABLE_NETWORK */
