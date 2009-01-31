/* $Id$ */

/** @file texteff.cpp Handling of text effects. */

#include "stdafx.h"
#include "openttd.h"
#include "strings_type.h"
#include "texteff.hpp"
#include "core/bitmath_func.hpp"
#include "transparency.h"
#include "strings_func.h"
#include "core/alloc_func.hpp"
#include "functions.h"
#include "viewport_func.h"
#include "settings_type.h"

enum {
	INIT_NUM_TEXT_EFFECTS  =  20,
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

/* used for text effects */
static TextEffect *_text_effect_list = NULL;
static uint16 _num_text_effects = INIT_NUM_TEXT_EFFECTS;

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
					if (te->mode == TE_RISING || (_settings_client.gui.loading_indicators && !IsTransparencySet(TO_LOADING))) {
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
					if (te->mode == TE_RISING || (_settings_client.gui.loading_indicators && !IsTransparencySet(TO_LOADING))) {
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
