/* $Id$ */

/** @file texteff.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "macros.h"
#include "strings.h"
#include "gfx.h"
#include "landscape.h"
#include "viewport.h"
#include "saveload.h"
#include "console.h"
#include "string.h"
#include "variables.h"
#include "table/sprites.h"
#include "blitter/factory.hpp"
#include <stdarg.h> /* va_list */
#include "date.h"
#include "texteff.hpp"
#include "video/video_driver.hpp"

enum {
	MAX_TEXTMESSAGE_LENGTH = 200,
	INIT_NUM_TEXT_MESSAGES =  20,
	MAX_CHAT_MESSAGES      =  10,
	MAX_ANIMATED_TILES     = 256,
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


struct TextMessage {
	char message[MAX_TEXTMESSAGE_LENGTH];
	uint16 color;
	Date end_date;
};

static TextEffect *_text_effect_list = NULL;
static TextMessage _textmsg_list[MAX_CHAT_MESSAGES];
TileIndex _animated_tile_list[MAX_ANIMATED_TILES];

static bool _textmessage_dirty = false;
static bool _textmessage_visible = false;
static uint16 _num_text_effects = INIT_NUM_TEXT_MESSAGES;

/* The chatbox grows from the bottom so the coordinates are pixels from
 * the left and pixels from the bottom. The height is the maximum height */
static const Oblong _textmsg_box = {10, 30, 500, 150};
static uint8 _textmessage_backup[150 * 500 * 6]; // (height * width)

static inline uint GetTextMessageCount()
{
	uint i;

	for (i = 0; i < MAX_CHAT_MESSAGES; i++) {
		if (_textmsg_list[i].message[0] == '\0') break;
	}

	return i;
}

/* Add a text message to the 'chat window' to be shown
 * @param color The colour this message is to be shown in
 * @param duration The duration of the chat message in game-days
 * @param message message itself in printf() style */
void CDECL AddTextMessage(uint16 color, uint8 duration, const char *message, ...)
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
	lines = GB(FormatStringLinebreaks(buf, _textmsg_box.width - 8), 0, 16) + 1;
	if (lines >= MAX_CHAT_MESSAGES) return;

	msg_count = GetTextMessageCount();
	/* We want to add more chat messages than there is free space for, remove 'old' */
	if (lines > MAX_CHAT_MESSAGES - msg_count) {
		int i = lines - (MAX_CHAT_MESSAGES - msg_count);
		memmove(&_textmsg_list[0], &_textmsg_list[i], sizeof(_textmsg_list[0]) * (msg_count - i));
		msg_count = MAX_CHAT_MESSAGES - lines;
	}

	for (bufp = buf; lines != 0; lines--) {
		TextMessage *tmsg = &_textmsg_list[msg_count++];
		ttd_strlcpy(tmsg->message, bufp, sizeof(tmsg->message));

		/* The default colour for a message is player colour. Replace this with
		 * white for any additional lines */
		tmsg->color = (bufp == buf && color & IS_PALETTE_COLOR) ? color : (0x1D - 15) | IS_PALETTE_COLOR;
		tmsg->end_date = _date + duration;

		bufp += strlen(bufp) + 1; // jump to 'next line' in the formatted string
	}

	_textmessage_dirty = true;
}

void InitTextMessage()
{
	uint i;

	for (i = 0; i < MAX_CHAT_MESSAGES; i++) {
		_textmsg_list[i].message[0] = '\0';
	}
}

/** Hide the textbox */
void UndrawTextMessage()
{
	if (_textmessage_visible) {
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
			if (_cursor.draw_pos.x + _cursor.draw_size.x >= _textmsg_box.x &&
				_cursor.draw_pos.x <= _textmsg_box.x + _textmsg_box.width &&
				_cursor.draw_pos.y + _cursor.draw_size.y >= _screen.height - _textmsg_box.y - _textmsg_box.height &&
				_cursor.draw_pos.y <= _screen.height - _textmsg_box.y) {
				UndrawMouseCursor();
			}
		}

		int x      = _textmsg_box.x;
		int y      = _screen.height - _textmsg_box.y - _textmsg_box.height;
		int width  = _textmsg_box.width;
		int height = _textmsg_box.height;
		if (y < 0) {
			height = max(height + y, min(_textmsg_box.height, _screen.height));
			y = 0;
		}
		if (x + width >= _screen.width) {
			width = _screen.width - x;
		}
		if (width <= 0 || height <= 0) return;

		_textmessage_visible = false;
		/* Put our 'shot' back to the screen */
		blitter->CopyFromBuffer(blitter->MoveTo(_screen.dst_ptr, x, y), _textmessage_backup, width, height);
		/* And make sure it is updated next time */
		_video_driver->MakeDirty(x, y, width, height);

		_textmessage_dirty = true;
	}
}

/** Check if a message is expired every day */
void TextMessageDailyLoop()
{
	uint i;

	for (i = 0; i < MAX_CHAT_MESSAGES; i++) {
		TextMessage *tmsg = &_textmsg_list[i];
		if (tmsg->message[0] == '\0') continue;

		/* Message has expired, remove from the list */
		if (tmsg->end_date < _date) {
			/* Move the remaining messages over the current message */
			if (i != MAX_CHAT_MESSAGES - 1) memmove(tmsg, tmsg + 1, sizeof(*tmsg) * (MAX_CHAT_MESSAGES - i - 1));

			/* Mark the last item as empty */
			_textmsg_list[MAX_CHAT_MESSAGES - 1].message[0] = '\0';
			_textmessage_dirty = true;

			/* Go one item back, because we moved the array 1 to the left */
			i--;
		}
	}
}

/** Draw the textmessage-box */
void DrawTextMessage()
{
	Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();
	if (!_textmessage_dirty) return;

	/* First undraw if needed */
	UndrawTextMessage();

	if (_iconsole_mode == ICONSOLE_FULL) return;

	/* Check if we have anything to draw at all */
	uint count = GetTextMessageCount();
	if (count == 0) return;

	int x      = _textmsg_box.x;
	int y      = _screen.height - _textmsg_box.y - _textmsg_box.height;
	int width  = _textmsg_box.width;
	int height = _textmsg_box.height;
	if (y < 0) {
		height = max(height + y, min(_textmsg_box.height, _screen.height));
		y = 0;
	}
	if (x + width >= _screen.width) {
		width = _screen.width - x;
	}
	if (width <= 0 || height <= 0) return;

	assert(blitter->BufferSize(width, height) < (int)sizeof(_textmessage_backup));

	/* Make a copy of the screen as it is before painting (for undraw) */
	blitter->CopyToBuffer(blitter->MoveTo(_screen.dst_ptr, x, y), _textmessage_backup, width, height);

	_cur_dpi = &_screen; // switch to _screen painting

	/* Paint a half-transparent box behind the text messages */
	GfxFillRect(
			_textmsg_box.x,
			_screen.height - _textmsg_box.y - count * 13 - 2,
			_textmsg_box.x + _textmsg_box.width - 1,
			_screen.height - _textmsg_box.y - 2,
			PALETTE_TO_TRANSPARENT | (1 << USE_COLORTABLE) // black, but with some alpha for background
		);

	/* Paint the messages starting with the lowest at the bottom */
	for (uint y = 13; count-- != 0; y += 13) {
		DoDrawString(_textmsg_list[count].message, _textmsg_box.x + 3, _screen.height - _textmsg_box.y - y + 1, _textmsg_list[count].color);
	}

	/* Make sure the data is updated next flush */
	_video_driver->MakeDirty(x, y, width, height);

	_textmessage_visible = true;
	_textmessage_dirty = false;
}

static void MarkTextEffectAreaDirty(TextEffect *te)
{
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
		_text_effect_list = (TextEffect*) realloc(_text_effect_list, _num_text_effects * sizeof(TextEffect));
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
					if (te->mode == TE_RISING || (_patches.loading_indicators && !HASBIT(_transparent_opt, TO_LOADING))) {
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
					if (te->mode == TE_RISING || (_patches.loading_indicators && !HASBIT(_transparent_opt, TO_LOADING))) {
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

void DeleteAnimatedTile(TileIndex tile)
{
	TileIndex *ti;

	for (ti = _animated_tile_list; ti != endof(_animated_tile_list); ti++) {
		if (tile == *ti) {
			/* remove the hole */
			memmove(ti, ti + 1, (lastof(_animated_tile_list) - ti) * sizeof(*ti));
			/* and clear last item */
			*lastof(_animated_tile_list) = 0;
			MarkTileDirtyByTile(tile);
			return;
		}
	}
}

bool AddAnimatedTile(TileIndex tile)
{
	TileIndex *ti;

	for (ti = _animated_tile_list; ti != endof(_animated_tile_list); ti++) {
		if (tile == *ti || *ti == 0) {
			*ti = tile;
			MarkTileDirtyByTile(tile);
			return true;
		}
	}

	return false;
}

void AnimateAnimatedTiles()
{
	const TileIndex* ti;

	for (ti = _animated_tile_list; ti != endof(_animated_tile_list) && *ti != 0; ti++) {
		AnimateTile(*ti);
	}
}

void InitializeAnimatedTiles()
{
	memset(_animated_tile_list, 0, sizeof(_animated_tile_list));
}

static void SaveLoad_ANIT()
{
	/* In pre version 6, we has 16bit per tile, now we have 32bit per tile, convert it ;) */
	if (CheckSavegameVersion(6)) {
		SlArray(_animated_tile_list, lengthof(_animated_tile_list), SLE_FILE_U16 | SLE_VAR_U32);
	} else {
		SlArray(_animated_tile_list, lengthof(_animated_tile_list), SLE_UINT32);
	}
}


extern const ChunkHandler _animated_tile_chunk_handlers[] = {
	{ 'ANIT', SaveLoad_ANIT, SaveLoad_ANIT, CH_RIFF | CH_LAST},
};
