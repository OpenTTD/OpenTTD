#include "stdafx.h"
#include "ttd.h"
#include "gfx.h"
#include "viewport.h"
#include "saveload.h"

typedef struct TextEffect {
	StringID string_id;
	int16 x,y,right,bottom;
	uint16 duration;
	uint32 params_1;
	uint32 params_2;
} TextEffect;

static TextEffect _text_effect_list[30];
TileIndex _animated_tile_list[256];

static void MarkTextEffectAreaDirty(TextEffect *te)
{
	MarkAllViewportsDirty(
		te->x,
		te->y - 1,
		(te->right - te->x)*2 + te->x + 1,
		(te->bottom - (te->y - 1)) * 2 + (te->y - 1) + 1
	);
}

void AddTextEffect(StringID msg, int x, int y, uint16 duration)
{
	TextEffect *te;
	int w;
	char buffer[100];

	if (_game_mode == GM_MENU)
		return;

	for (te = _text_effect_list; te->string_id != 0xFFFF; ) {
		if (++te == endof(_text_effect_list))
			return;
	}

	te->string_id = msg;
	te->duration = duration;
	te->y = y - 5;
	te->bottom = y + 5;
	te->params_1 = GetDParam(0);
	te->params_2 = GetDParam(4);

	GetString(buffer, msg);
	w = GetStringWidth(buffer);

	te->x = x - (w >> 1);
	te->right = x + (w >> 1) - 1;
	MarkTextEffectAreaDirty(te);
}

static void MoveTextEffect(TextEffect *te)
{
	if (te->duration < 8) {
		te->string_id = 0xFFFF;
	} else {
		te->duration-=8;
		te->y--;
		te->bottom--;
	}
	MarkTextEffectAreaDirty(te);
}

void MoveAllTextEffects()
{
	TextEffect *te;

	for (te = _text_effect_list; te != endof(_text_effect_list); te++ ) {
		if (te->string_id != 0xFFFF)
			MoveTextEffect(te);
	}
}

void InitTextEffects()
{
	TextEffect *te;

	for (te = _text_effect_list; te != endof(_text_effect_list); te++ ) {
		te->string_id = 0xFFFF;
	}
}

void DrawTextEffects(DrawPixelInfo *dpi)
{
	TextEffect *te;

	if (dpi->zoom < 1) {
		for (te = _text_effect_list; te != endof(_text_effect_list); te++ ) {
			if (te->string_id == 0xFFFF)
				continue;

			/* intersection? */
			if ((int16)dpi->left > te->right ||
					(int16)dpi->top > te->bottom ||
					(int16)(dpi->left + dpi->width) <= te->x ||
					(int16)(dpi->top + dpi->height) <= te->y)
						continue;
			AddStringToDraw(te->x, te->y, te->string_id, te->params_1, te->params_2);
		}
	} else if (dpi->zoom == 1) {
		for (te = _text_effect_list; te != endof(_text_effect_list); te++ ) {
			if (te->string_id == 0xFFFF)
				continue;

			/* intersection? */
			if (dpi->left > te->right*2 -  te->x ||
					dpi->top > te->bottom*2 - te->y ||
					(dpi->left + dpi->width) <= te->x ||
					(dpi->top + dpi->height) <= te->y)
						continue;
			AddStringToDraw(te->x, te->y, (StringID)(te->string_id-1), te->params_1, te->params_2);
		}

	}
}

void DeleteAnimatedTile(uint tile)
{
	TileIndex *ti;

	for(ti=_animated_tile_list; ti!=endof(_animated_tile_list); ti++) {
		if ( (TileIndex)tile == *ti) {
			/* remove the hole */
			memmove(ti, ti+1, endof(_animated_tile_list) - 1 - ti);
			/* and clear last item */
			endof(_animated_tile_list)[-1] = 0;
			MarkTileDirtyByTile(tile);
			return;
		}
	}
}

bool AddAnimatedTile(uint tile)
{
	TileIndex *ti;

	for(ti=_animated_tile_list; ti!=endof(_animated_tile_list); ti++) {
		if ( (TileIndex)tile == *ti || *ti == 0) {
			*ti = tile;
			MarkTileDirtyByTile(tile);
			return true;
		}
	}

	return false;
}

void AnimateAnimatedTiles()
{
	TileIndex *ti;
	uint tile;

	for(ti=_animated_tile_list; ti!=endof(_animated_tile_list) && (tile=*ti) != 0; ti++) {
		AnimateTile(tile);
	}
}

void InitializeAnimatedTiles()
{
	memset(_animated_tile_list, 0, sizeof(_animated_tile_list));
}

static void SaveLoad_ANIT()
{
	SlArray(_animated_tile_list, lengthof(_animated_tile_list), SLE_UINT16);
}


const ChunkHandler _animated_tile_chunk_handlers[] = {
	{ 'ANIT', SaveLoad_ANIT, SaveLoad_ANIT, CH_RIFF | CH_LAST},
};


