/* $Id$ */

/** @file texteff.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "landscape.h"
#include "gfx_func.h"
#include "console.h"
#include "variables.h"
#include "blitter/factory.hpp"
#include "texteff.hpp"
#include "video/video_driver.hpp"
#include "transparency.h"
#include "strings_func.h"
#include "core/alloc_func.hpp"
#include "date_func.h"
#include "functions.h"
#include "viewport_func.h"
#include "settings_type.h"

#include "table/sprites.h"

#include <stdarg.h> /* va_list */

enum {
	MAX_TEXTMESSAGE_LENGTH = 200,
	INIT_NUM_TEXT_MESSAGES =  20,
	MAX_CHAT_MESSAGES      =  10,
};

struct TextEffect {
	StringID string_id;
	int32 x;
	int32 y;
	int32 right;
	int32 bottom;
	uint16 duration;
	uint64 params_1;
	uint64 params_2;
	TextEffectMode mode;
};


struct ChatMessage {
	char message[MAX_TEXTMESSAGE_LENGTH];
	uint16 color;
	Date end_date;
};

/* used for text effects */
static TextEffect *_text_effect_list = NULL;
static uint16 _num_text_effects = INIT_NUM_TEXT_MESSAGES;

/* used for chat window */
static ChatMessage _chatmsg_list[MAX_CHAT_MESSAGES];
static bool _chatmessage_dirty = false;
static bool _chatmessage_visible = false;

/* The chatbox grows from the bottom so the coordinates are pixels from
 * the left and pixels from the bottom. The height is the maximum height */
static const PointDimension _chatmsg_box = {10, 30, 500, 150};
static uint8 _chatmessage_backup[150 * 500 * 6]; // (height * width)

static inline uint GetChatMessageCount()
{
	uint i;

	for (i = 0; i < MAX_CHAT_MESSAGES; i++) {
		if (_chatmsg_list[i].message[0] == '\0') break;
	}

	return i;
}

/* Add a text message to the 'chat window' to be shown
 * @param color The colour this message is to be shown in
 * @param duration The duration of the chat message in game-days
 * @param message message itself in printf() style */
void CDECL AddChatMessage(uint16 color, uint8 duration, const char *message, ...)
{
	char buf[MAX_TEXTMESSAGE_LENGTH];
	const char *bufp;
	va_list va;
	uint msg_count;
	uint16 lines;

	va_start(va, message);
	vsnprintf(buf, lengthof(buf), message, va);
	va_end(va);


	Utf8TrimString(buf, MAX_TEXTMESSAGE_LENGTH);

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
		ttd_strlcpy(cmsg->message, bufp, sizeof(cmsg->message));

		/* The default colour for a message is player colour. Replace this with
		 * white for any additional lines */
		cmsg->color = (bufp == buf && color & IS_PALETTE_COLOR) ? color : (0x1D - 15) | IS_PALETTE_COLOR;
		cmsg->end_date = _date + duration;

		bufp += strlen(bufp) + 1; // jump to 'next line' in the formatted string
	}

	_chatmessage_dirty = true;
}

void InitChatMessage()
{
	uint i;

	for (i = 0; i < MAX_CHAT_MESSAGES; i++) {
		_chatmsg_list[i].message[0] = '\0';
	}
}

/** Hide the chatbox */
void UndrawChatMessage()
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
void ChatMessageDailyLoop()
{
	uint i;

	for (i = 0; i < MAX_CHAT_MESSAGES; i++) {
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
void DrawChatMessage()
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	if (!_chatmessage_dirty) return;

	/* First undraw if needed */
	UndrawChatMessage();

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

	assert(blitter->BufferSize(width, height) < (int)sizeof(_chatmessage_backup));

	/* Make a copy of the screen as it is before painting (for undraw) */
	blitter->CopyToBuffer(blitter->MoveTo(_screen.dst_ptr, x, y), _chatmessage_backup, width, height);

	_cur_dpi = &_screen; // switch to _screen painting

	/* Paint a half-transparent box behind the chat messages */
	GfxFillRect(
			_chatmsg_box.x,
			_screen.height - _chatmsg_box.y - count * 13 - 2,
			_chatmsg_box.x + _chatmsg_box.width - 1,
			_screen.height - _chatmsg_box.y - 2,
			PALETTE_TO_TRANSPARENT | (1 << USE_COLORTABLE) // black, but with some alpha for background
		);

	/* Paint the chat messages starting with the lowest at the bottom */
	for (uint y = 13; count-- != 0; y += 13) {
		DoDrawString(_chatmsg_list[count].message, _chatmsg_box.x + 3, _screen.height - _chatmsg_box.y - y + 1, _chatmsg_list[count].color);
	}

	/* Make sure the data is updated next flush */
	_video_driver->MakeDirty(x, y, width, height);

	_chatmessage_visible = true;
	_chatmessage_dirty = false;
}

/* Text Effects */
/**
 * Mark the area of the text effect as dirty.
 *
 * This function marks the area of a text effect as dirty for repaint.
 *
 * @param te The TextEffect to mark the area dirty
 * @ingroup dirty
 */
static void MarkTextEffectAreaDirty(TextEffect *te)
{
	/* Width and height of the text effect are doubled, so they are correct in both zoom out levels 1x and 2x. */
	MarkAllViewportsDirty(
		te->x,
		te->y - 1,
		(te->right - te->x)*2 + te->x + 1,
		(te->bottom - (te->y - 1)) * 2 + (te->y - 1) + 1
	);
}

TextEffectID AddTextEffect(StringID msg, int x, int y, uint16 duration, TextEffectMode mode)
{
	TextEffect *te;
	int w;
	char buffer[100];
	TextEffectID i;

	if (_game_mode == GM_MENU) return INVALID_TE_ID;

	/* Look for a free spot in the text effect array */
	for (i = 0; i < _num_text_effects; i++) {
		if (_text_effect_list[i].string_id == INVALID_STRING_ID) break;
	}

	/* If there is none found, we grow the array */
	if (i == _num_text_effects) {
		_num_text_effects += 25;
		_text_effect_list = ReallocT<TextEffect>(_text_effect_list, _num_text_effects);
		for (; i < _num_text_effects; i++) _text_effect_list[i].string_id = INVALID_STRING_ID;
		i = _num_text_effects - 1;
	}

	te = &_text_effect_list[i];

	/* Start defining this object */
	te->string_id = msg;
	te->duration = duration;
	te->y = y - 5;
	te->bottom = y + 5;
	te->params_1 = GetDParam(0);
	te->params_2 = GetDParam(4);
	te->mode = mode;

	GetString(buffer, msg, lastof(buffer));
	w = GetStringBoundingBox(buffer).width;

	te->x = x - (w >> 1);
	te->right = x + (w >> 1) - 1;
	MarkTextEffectAreaDirty(te);

	return i;
}

void UpdateTextEffect(TextEffectID te_id, StringID msg)
{
	assert(te_id < _num_text_effects);
	TextEffect *te;

	/* Update details */
	te = &_text_effect_list[te_id];
	te->string_id = msg;
	te->params_1 = GetDParam(0);
	te->params_2 = GetDParam(4);

	/* Update width of text effect */
	char buffer[100];
	GetString(buffer, msg, lastof(buffer));
	int w = GetStringBoundingBox(buffer).width;

	/* Only allow to make it broader, so it completely covers the old text. That avoids remnants of the old text. */
	int right_new = te->x + w;
	if (te->right < right_new) te->right = right_new;

	MarkTextEffectAreaDirty(te);
}

void RemoveTextEffect(TextEffectID te_id)
{
	assert(te_id < _num_text_effects);
	TextEffect *te;

	te = &_text_effect_list[te_id];
	MarkTextEffectAreaDirty(te);
	te->string_id = INVALID_STRING_ID;
}

static void MoveTextEffect(TextEffect *te)
{
	/* Never expire for duration of 0xFFFF */
	if (te->duration == 0xFFFF) return;
	if (te->duration < 8) {
		te->string_id = INVALID_STRING_ID;
	} else {
		te->duration -= 8;
		te->y--;
		te->bottom--;
	}
	MarkTextEffectAreaDirty(te);
}

void MoveAllTextEffects()
{
	for (TextEffectID i = 0; i < _num_text_effects; i++) {
		TextEffect *te = &_text_effect_list[i];
		if (te->string_id != INVALID_STRING_ID && te->mode == TE_RISING) MoveTextEffect(te);
	}
}

void InitTextEffects()
{
	if (_text_effect_list == NULL) _text_effect_list = MallocT<TextEffect>(_num_text_effects);

	for (TextEffectID i = 0; i < _num_text_effects; i++) _text_effect_list[i].string_id = INVALID_STRING_ID;
}

void DrawTextEffects(DrawPixelInfo *dpi)
{
	switch (dpi->zoom) {
		case ZOOM_LVL_NORMAL:
			for (TextEffectID i = 0; i < _num_text_effects; i++) {
				TextEffect *te = &_text_effect_list[i];
				if (te->string_id != INVALID_STRING_ID &&
						dpi->left <= te->right &&
						dpi->top  <= te->bottom &&
						dpi->left + dpi->width  > te->x &&
						dpi->top  + dpi->height > te->y) {
					if (te->mode == TE_RISING || (_patches.loading_indicators && !IsTransparencySet(TO_LOADING))) {
						AddStringToDraw(te->x, te->y, te->string_id, te->params_1, te->params_2);
					}
				}
			}
			break;

		case ZOOM_LVL_OUT_2X:
			for (TextEffectID i = 0; i < _num_text_effects; i++) {
				TextEffect *te = &_text_effect_list[i];
				if (te->string_id != INVALID_STRING_ID &&
						dpi->left <= te->right  * 2 - te->x &&
						dpi->top  <= te->bottom * 2 - te->y &&
						dpi->left + dpi->width  > te->x &&
						dpi->top  + dpi->height > te->y) {
					if (te->mode == TE_RISING || (_patches.loading_indicators && !IsTransparencySet(TO_LOADING))) {
						AddStringToDraw(te->x, te->y, (StringID)(te->string_id - 1), te->params_1, te->params_2);
					}
				}
			}
			break;

		case ZOOM_LVL_OUT_4X:
		case ZOOM_LVL_OUT_8X:
			break;

		default: NOT_REACHED();
	}
}
