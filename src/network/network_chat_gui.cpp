/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_chat_gui.cpp GUI for handling chat messages. */

#include "../stdafx.h"
#include "../strings_func.h"
#include "../blitter/factory.hpp"
#include "../console_func.h"
#include "../video/video_driver.hpp"
#include "../querystring_gui.h"
#include "../town.h"
#include "../window_func.h"
#include "../toolbar_gui.h"
#include "../core/geometry_func.hpp"
#include "../zoom_func.h"
#include "network.h"
#include "network_client.h"
#include "network_base.h"

#include "../widgets/network_chat_widget.h"

#include "table/strings.h"

#include <stdarg.h> /* va_list */
#include <deque>

#include "../safeguards.h"

/** The draw buffer must be able to contain the chat message, client name and the "[All]" message,
 * some spaces and possible translations of [All] to other languages. */
static_assert((int)DRAW_STRING_BUFFER >= (int)NETWORK_CHAT_LENGTH + NETWORK_NAME_LENGTH + 40);

/** Spacing between chat lines. */
static const uint NETWORK_CHAT_LINE_SPACING = 3;

/** Container for a message. */
struct ChatMessage {
	std::string message; ///< The action message.
	TextColour colour;  ///< The colour of the message.
	std::chrono::steady_clock::time_point remove_time; ///< The time to remove the message.
};

/* used for chat window */
static std::deque<ChatMessage> _chatmsg_list; ///< The actual chat message list.
static bool _chatmessage_dirty = false;   ///< Does the chat message need repainting?
static bool _chatmessage_visible = false; ///< Is a chat message visible.
static bool _chat_tab_completion_active;  ///< Whether tab completion is active.
static uint MAX_CHAT_MESSAGES = 0;        ///< The limit of chat messages to show.

/**
 * Time the chat history was marked dirty. This is used to determine if expired
 * messages have recently expired and should cause a redraw to hide them.
 */
static std::chrono::steady_clock::time_point _chatmessage_dirty_time;

/**
 * The chatbox grows from the bottom so the coordinates are pixels from
 * the left and pixels from the bottom. The height is the maximum height.
 */
static PointDimension _chatmsg_box;
static uint8 *_chatmessage_backup = nullptr; ///< Backup in case text is moved.

/**
 * Test if there are any chat messages to display.
 * @param show_all Set if all messages should be included, instead of unexpired only.
 * @return True iff there are chat messages to display.
 */
static inline bool HaveChatMessages(bool show_all)
{
	if (show_all) return _chatmsg_list.size() != 0;

	auto now = std::chrono::steady_clock::now();
	for (auto &cmsg : _chatmsg_list) {
		if (cmsg.remove_time >= now) return true;
	}

	return false;
}

/**
 * Add a text message to the 'chat window' to be shown
 * @param colour The colour this message is to be shown in
 * @param duration The duration of the chat message in seconds
 * @param message message itself in printf() style
 */
void CDECL NetworkAddChatMessage(TextColour colour, uint duration, const std::string &message)
{
	if (_chatmsg_list.size() == MAX_CHAT_MESSAGES) {
		_chatmsg_list.pop_back();
	}

	ChatMessage *cmsg = &_chatmsg_list.emplace_front();
	cmsg->message = message;
	cmsg->colour = colour;
	cmsg->remove_time = std::chrono::steady_clock::now() + std::chrono::seconds(duration);

	_chatmessage_dirty_time = std::chrono::steady_clock::now();
	_chatmessage_dirty = true;
}

/** Initialize all font-dependent chat box sizes. */
void NetworkReInitChatBoxSize()
{
	_chatmsg_box.y       = 3 * FONT_HEIGHT_NORMAL;
	_chatmsg_box.height  = MAX_CHAT_MESSAGES * (FONT_HEIGHT_NORMAL + ScaleGUITrad(NETWORK_CHAT_LINE_SPACING)) + ScaleGUITrad(4);
	_chatmessage_backup  = ReallocT(_chatmessage_backup, _chatmsg_box.width * _chatmsg_box.height * BlitterFactory::GetCurrentBlitter()->GetBytesPerPixel());
}

/** Initialize all buffers of the chat visualisation. */
void NetworkInitChatMessage()
{
	MAX_CHAT_MESSAGES    = _settings_client.gui.network_chat_box_height;

	_chatmsg_list.clear();
	_chatmsg_box.x       = ScaleGUITrad(10);
	_chatmsg_box.width   = _settings_client.gui.network_chat_box_width_pct * _screen.width / 100;
	NetworkReInitChatBoxSize();
	_chatmessage_visible = false;
}

/** Hide the chatbox */
void NetworkUndrawChatMessage()
{
	/* Sometimes we also need to hide the cursor
	 *   This is because both textmessage and the cursor take a shot of the
	 *   screen before drawing.
	 *   Now the textmessage takes its shot and paints its data before the cursor
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
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();
		int x      = _chatmsg_box.x;
		int y      = _screen.height - _chatmsg_box.y - _chatmsg_box.height;
		int width  = _chatmsg_box.width;
		int height = _chatmsg_box.height;
		if (y < 0) {
			height = std::max(height + y, std::min(_chatmsg_box.height, _screen.height));
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
		VideoDriver::GetInstance()->MakeDirty(x, y, width, height);

		_chatmessage_dirty_time = std::chrono::steady_clock::now();
		_chatmessage_dirty = true;
	}
}

/** Check if a message is expired. */
void NetworkChatMessageLoop()
{
	auto now = std::chrono::steady_clock::now();
	for (auto &cmsg : _chatmsg_list) {
		/* Message has expired, remove from the list */
		if (now > cmsg.remove_time && _chatmessage_dirty_time < cmsg.remove_time) {
			_chatmessage_dirty_time = now;
			_chatmessage_dirty = true;
			break;
		}
	}
}

/** Draw the chat message-box */
void NetworkDrawChatMessage()
{
	Blitter *blitter = BlitterFactory::GetCurrentBlitter();
	if (!_chatmessage_dirty) return;

	const Window *w = FindWindowByClass(WC_SEND_NETWORK_MSG);
	bool show_all = (w != nullptr);

	/* First undraw if needed */
	NetworkUndrawChatMessage();

	if (_iconsole_mode == ICONSOLE_FULL) return;

	/* Check if we have anything to draw at all */
	if (!HaveChatMessages(show_all)) return;

	int x      = _chatmsg_box.x;
	int y      = _screen.height - _chatmsg_box.y - _chatmsg_box.height;
	int width  = _chatmsg_box.width;
	int height = _chatmsg_box.height;
	if (y < 0) {
		height = std::max(height + y, std::min(_chatmsg_box.height, _screen.height));
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

	auto now = std::chrono::steady_clock::now();
	int string_height = 0;
	for (auto &cmsg : _chatmsg_list) {
		if (!show_all && cmsg.remove_time < now) continue;
		SetDParamStr(0, cmsg.message);
		string_height += GetStringLineCount(STR_JUST_RAW_STRING, width - 1) * FONT_HEIGHT_NORMAL + NETWORK_CHAT_LINE_SPACING;
	}

	string_height = std::min<uint>(string_height, MAX_CHAT_MESSAGES * (FONT_HEIGHT_NORMAL + NETWORK_CHAT_LINE_SPACING));

	int top = _screen.height - _chatmsg_box.y - string_height - 2;
	int bottom = _screen.height - _chatmsg_box.y - 2;
	/* Paint a half-transparent box behind the chat messages */
	GfxFillRect(_chatmsg_box.x, top - 2, _chatmsg_box.x + _chatmsg_box.width - 1, bottom,
			PALETTE_TO_TRANSPARENT, FILLRECT_RECOLOUR // black, but with some alpha for background
		);

	/* Paint the chat messages starting with the lowest at the bottom */
	int ypos = bottom - 2;

	for (auto &cmsg : _chatmsg_list) {
		if (!show_all && cmsg.remove_time < now) continue;
		ypos = DrawStringMultiLine(_chatmsg_box.x + ScaleGUITrad(3), _chatmsg_box.x + _chatmsg_box.width - 1, top, ypos, cmsg.message, cmsg.colour, SA_LEFT | SA_BOTTOM | SA_FORCE) - NETWORK_CHAT_LINE_SPACING;
		if (ypos < top) break;
	}

	/* Make sure the data is updated next flush */
	VideoDriver::GetInstance()->MakeDirty(x, y, width, height);

	_chatmessage_visible = true;
	_chatmessage_dirty = false;
}

/**
 * Send an actual chat message.
 * @param buf The message to send.
 * @param type The type of destination.
 * @param dest The actual destination index.
 */
static void SendChat(const std::string &buf, DestType type, int dest)
{
	if (buf.empty()) return;
	if (!_network_server) {
		MyClient::SendChat((NetworkAction)(NETWORK_ACTION_CHAT + type), type, dest, buf, 0);
	} else {
		NetworkServerSendChat((NetworkAction)(NETWORK_ACTION_CHAT + type), type, dest, buf, CLIENT_ID_SERVER);
	}
}

/** Window to enter the chat message in. */
struct NetworkChatWindow : public Window {
	DestType dtype;       ///< The type of destination.
	int dest;             ///< The identifier of the destination.
	QueryString message_editbox; ///< Message editbox.

	/**
	 * Create a chat input window.
	 * @param desc Description of the looks of the window.
	 * @param type The type of destination.
	 * @param dest The actual destination index.
	 */
	NetworkChatWindow(WindowDesc *desc, DestType type, int dest) : Window(desc), message_editbox(NETWORK_CHAT_LENGTH)
	{
		this->dtype   = type;
		this->dest    = dest;
		this->querystrings[WID_NC_TEXTBOX] = &this->message_editbox;
		this->message_editbox.cancel_button = WID_NC_CLOSE;
		this->message_editbox.ok_button = WID_NC_SENDBUTTON;

		static const StringID chat_captions[] = {
			STR_NETWORK_CHAT_ALL_CAPTION,
			STR_NETWORK_CHAT_COMPANY_CAPTION,
			STR_NETWORK_CHAT_CLIENT_CAPTION
		};
		assert((uint)this->dtype < lengthof(chat_captions));

		this->CreateNestedTree();
		this->GetWidget<NWidgetCore>(WID_NC_DESTINATION)->widget_data = chat_captions[this->dtype];
		this->FinishInitNested(type);

		this->SetFocusedWidget(WID_NC_TEXTBOX);
		InvalidateWindowData(WC_NEWS_WINDOW, 0, this->height);
		_chat_tab_completion_active = false;

		PositionNetworkChatWindow(this);
	}

	void Close() override
	{
		InvalidateWindowData(WC_NEWS_WINDOW, 0, 0);
		this->Window::Close();
	}

	void FindWindowPlacementAndResize(int def_width, int def_height) override
	{
		Window::FindWindowPlacementAndResize(_toolbar_width, def_height);
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
			for (NetworkClientInfo *ci : NetworkClientInfo::Iterate(*item)) {
				*item = ci->index;
				return ci->client_name.c_str();
			}
			*item = MAX_CLIENT_SLOTS;
		}

		/* Then, try townnames
		 * Not that the following assumes all town indices are adjacent, ie no
		 * towns have been deleted. */
		if (*item < (uint)MAX_CLIENT_SLOTS + Town::GetPoolSize()) {
			for (const Town *t : Town::Iterate(*item - MAX_CLIENT_SLOTS)) {
				/* Get the town-name via the string-system */
				SetDParam(0, t->index);
				GetString(chat_tab_temp_buffer, STR_TOWN_NAME, lastof(chat_tab_temp_buffer));
				return &chat_tab_temp_buffer[0];
			}
		}

		return nullptr;
	}

	/**
	 * Find what text to complete. It scans for a space from the left and marks
	 *  the word right from that as to complete. It also writes a \0 at the
	 *  position of the space (if any). If nothing found, buf is returned.
	 */
	static char *ChatTabCompletionFindText(char *buf)
	{
		char *p = strrchr(buf, ' ');
		if (p == nullptr) return buf;

		*p = '\0';
		return p + 1;
	}

	/**
	 * See if we can auto-complete the current text of the user.
	 */
	void ChatTabCompletion()
	{
		static char _chat_tab_completion_buf[NETWORK_CHAT_LENGTH];
		assert(this->message_editbox.text.max_bytes == lengthof(_chat_tab_completion_buf));

		Textbuf *tb = &this->message_editbox.text;
		size_t len, tb_len;
		uint item;
		char *tb_buf, *pre_buf;
		const char *cur_name;
		bool second_scan = false;

		item = 0;

		/* Copy the buffer so we can modify it without damaging the real data */
		pre_buf = (_chat_tab_completion_active) ? stredup(_chat_tab_completion_buf) : stredup(tb->buf);

		tb_buf  = ChatTabCompletionFindText(pre_buf);
		tb_len  = strlen(tb_buf);

		while ((cur_name = ChatTabCompletionNextItem(&item)) != nullptr) {
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
						length = (tb->bytes - 1) - 2;
					} else {
						/* Else, find the place we are completing at */
						offset = strlen(pre_buf) + 1;
						length = (tb->bytes - 1) - offset;
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
				if (!second_scan) seprintf(_chat_tab_completion_buf, lastof(_chat_tab_completion_buf), "%s", tb->buf);
				_chat_tab_completion_active = true;

				/* Change to the found name. Add ': ' if we are at the start of the line (pretty) */
				if (pre_buf == tb_buf) {
					this->message_editbox.text.Print("%s: ", cur_name);
				} else {
					this->message_editbox.text.Print("%s %s", pre_buf, cur_name);
				}

				this->SetDirty();
				free(pre_buf);
				return;
			}
		}

		if (second_scan) {
			/* We walked all possibilities, and the user presses tab again.. revert to original text */
			this->message_editbox.text.Assign(_chat_tab_completion_buf);
			_chat_tab_completion_active = false;

			this->SetDirty();
		}
		free(pre_buf);
	}

	Point OnInitialPosition(int16 sm_width, int16 sm_height, int window_number) override
	{
		Point pt = { 0, _screen.height - sm_height - FindWindowById(WC_STATUS_BAR, 0)->height };
		return pt;
	}

	void SetStringParameters(int widget) const override
	{
		if (widget != WID_NC_DESTINATION) return;

		if (this->dtype == DESTTYPE_CLIENT) {
			SetDParamStr(0, NetworkClientInfo::GetByClientID((ClientID)this->dest)->client_name);
		}
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (widget) {
			case WID_NC_SENDBUTTON: /* Send */
				SendChat(this->message_editbox.text.buf, this->dtype, this->dest);
				FALLTHROUGH;

			case WID_NC_CLOSE: /* Cancel */
				this->Close();
				break;
		}
	}

	EventState OnKeyPress(WChar key, uint16 keycode) override
	{
		EventState state = ES_NOT_HANDLED;
		if (keycode == WKC_TAB) {
			ChatTabCompletion();
			state = ES_HANDLED;
		}
		return state;
	}

	void OnEditboxChanged(int wid) override
	{
		_chat_tab_completion_active = false;
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		if (data == this->dest) this->Close();
	}
};

/** The widgets of the chat window. */
static const NWidgetPart _nested_chat_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, WID_NC_CLOSE),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_NC_BACKGROUND),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_NC_DESTINATION), SetMinimalSize(62, 12), SetPadding(1, 0, 1, 0), SetAlignment(SA_VERT_CENTER | SA_RIGHT), SetDataTip(STR_NULL, STR_NULL),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WID_NC_TEXTBOX), SetMinimalSize(100, 12), SetPadding(1, 0, 1, 0), SetResize(1, 0),
																	SetDataTip(STR_NETWORK_CHAT_OSKTITLE, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_NC_SENDBUTTON), SetMinimalSize(62, 12), SetPadding(1, 0, 1, 0), SetDataTip(STR_NETWORK_CHAT_SEND, STR_NULL),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** The description of the chat window. */
static WindowDesc _chat_window_desc(
	WDP_MANUAL, nullptr, 0, 0,
	WC_SEND_NETWORK_MSG, WC_NONE,
	0,
	_nested_chat_window_widgets, lengthof(_nested_chat_window_widgets)
);


/**
 * Show the chat window.
 * @param type The type of destination.
 * @param dest The actual destination index.
 */
void ShowNetworkChatQueryWindow(DestType type, int dest)
{
	CloseWindowByClass(WC_SEND_NETWORK_MSG);
	new NetworkChatWindow(&_chat_window_desc, type, dest);
}
