#include "stdafx.h"
#include "ttd.h"
#include "viewport.h"
#include "window.h"
#include "vehicle.h"
#include "station.h"
#include "gfx.h"
#include "town.h"

#define VIEWPORT_DRAW_MEM (65536 * 2)

static bool _added_tile_sprite;
static bool _offset_ground_sprites;

typedef struct StringSpriteToDraw {
	uint16 string;
	uint16 color;
	struct StringSpriteToDraw *next;
	int16 x;
	int16 y;
	uint32 params[2];
	uint16 width;
} StringSpriteToDraw;

typedef struct TileSpriteToDraw {
	uint32 image;
	struct TileSpriteToDraw *next;
	int16 x, y;
	byte z;
} TileSpriteToDraw;

typedef struct ChildScreenSpriteToDraw {
	uint32 image;
	int16 x,y;
	struct ChildScreenSpriteToDraw *next;
} ChildScreenSpriteToDraw;

typedef struct ParentSpriteToDraw {
	uint32 image;
	int16 left, top, right, bottom;
	int16 tile_x, tile_y;
	int16 tile_right, tile_bottom;
	ChildScreenSpriteToDraw *child;
	byte unk16;
	byte tile_z;
	byte tile_z_bottom;
} ParentSpriteToDraw;

typedef struct ViewportDrawer {
	DrawPixelInfo dpi;

	byte *spritelist_mem, *eof_spritelist_mem;

	StringSpriteToDraw **last_string, *first_string;
	TileSpriteToDraw **last_tile, *first_tile;

	ChildScreenSpriteToDraw **last_child;

	ParentSpriteToDraw **parent_list;
	ParentSpriteToDraw **eof_parent_list;

	byte combine_sprites;

	int offs_x, offs_y; // used when drawing ground sprites relative
	bool ground_child;
} ViewportDrawer;

static ViewportDrawer *_cur_vd;

TileHighlightData * const _thd_ptr = &_thd;
static TileInfo *_cur_ti;


Point MapXYZToViewport(ViewPort *vp, uint x, uint y, uint z)
{
	Point p = RemapCoords(x, y, z);
	p.x -= (vp->virtual_width>>1);
	p.y -= (vp->virtual_height>>1);
	return p;
}

void AssignWindowViewport(Window *w, int x, int y,
	int width, int height, uint32 follow_flags, byte zoom)
{
	ViewPort *vp;
	Point pt;
	byte z;
	uint32 bit;

	for(vp=_viewports,bit=1; ; vp++, bit<<=1) {
		assert(vp!=endof(_viewports));
		if (vp->width == 0)
			break;
	}
	_active_viewports |= bit;

	vp->left = x + w->left;
	vp->top = y + w->top;
	vp->width = width;
	vp->height = height;

	vp->zoom = zoom;

	vp->virtual_width = width << zoom;
	vp->virtual_height = height << zoom;

	if (follow_flags & 0x80000000) {
		Vehicle *veh;
		WP(w,vp_d).follow_vehicle = (VehicleID)(follow_flags & 0xFFFF);
		veh = &_vehicles[WP(w,vp_d).follow_vehicle];
		pt = MapXYZToViewport(vp, veh->x_pos, veh->y_pos, veh->z_pos);
	} else {
		int x = GET_TILE_X(follow_flags) * 16;
		int y = GET_TILE_Y(follow_flags) * 16;
		WP(w,vp_d).follow_vehicle = 0xFFFF;
		z = GetSlopeZ(x,y);
		pt = MapXYZToViewport(vp, x,y, z);
	}

	WP(w,vp_d).scrollpos_x = pt.x;
	WP(w,vp_d).scrollpos_y = pt.y;
	w->viewport = vp;
	vp->virtual_left = 0;//pt.x;
	vp->virtual_top = 0;//pt.y;
}

static Point _vp_move_offs;

static void DoSetViewportPosition(Window *w, int left, int top, int width, int height)
{
	for (; w < _last_window; w++) {
		if (left + width > w->left &&
				w->left+w->width > left &&
				top + height > w->top &&
				w->top+w->height > top) {

			if (left < w->left) {
				DoSetViewportPosition(w, left, top, w->left - left, height);
				DoSetViewportPosition(w, left + (w->left - left), top, width - (w->left - left), height);
				return;
			}

			if (left + width > w->left + w->width) {
				DoSetViewportPosition(w, left, top, (w->left + w->width - left), height);
				DoSetViewportPosition(w, left + (w->left + w->width - left), top, width - (w->left + w->width - left) , height);
				return;
			}

			if (top < w->top) {
				DoSetViewportPosition(w, left, top, width, (w->top - top));
				DoSetViewportPosition(w, left, top + (w->top - top), width, height - (w->top - top));
				return;
			}

			if (top + height > w->top + w->height) {
				DoSetViewportPosition(w, left, top, width, (w->top + w->height - top));
				DoSetViewportPosition(w, left, top + (w->top + w->height - top), width , height - (w->top + w->height - top));
				return;
			}

			return;
		}
	}

	{
		int xo = _vp_move_offs.x;
		int yo = _vp_move_offs.y;

		if (abs(xo) >= width || abs(yo) >= height) {
			/* fully_outside */
			RedrawScreenRect(left, top, left+width, top+height);
			return;
		}

		GfxScroll(left, top, width, height, xo, yo);

		if (xo > 0) {
			RedrawScreenRect(left, top, xo + left, top + height);
			left += xo;
			width -= xo;
		} else if (xo < 0) {
			RedrawScreenRect(left+width+xo, top, left+width, top + height);
			width += xo;
		}

		if (yo > 0) {
			RedrawScreenRect(left, top, width+left, top + yo);
		} else if (yo < 0) {
			RedrawScreenRect(left, top + height + yo, width+left, top + height);
		}
	}
}

void SetViewportPosition(Window *w, int x, int y)
{
	ViewPort *vp = w->viewport;
	int old_left = vp->virtual_left;
	int old_top = vp->virtual_top;
	int i;
	int left, top, width, height;

	vp->virtual_left = x;
	vp->virtual_top = y;

	old_left >>= vp->zoom;
	old_top >>= vp->zoom;
	x >>= vp->zoom;
	y >>= vp->zoom;

	old_left -= x;
	old_top -= y;

	if (old_top == 0 && old_left == 0)
		return;

	_vp_move_offs.x = old_left;
	_vp_move_offs.y = old_top;

	left = vp->left;
	top = vp->top;
	width = vp->width;
	height = vp->height;

	if (left < 0) {
		width += left;
		left = 0;
	}

	if ( (i=(left + width - _screen.width)) >= 0)
		width -= i;

	if (width > 0) {
		if (top < 0) {
			height += top;
			top = 0;
		}

		if ( (i=(top + height - _screen.height)) >= 0) {
			height -= i;
		}

		if (height > 0)
			DoSetViewportPosition(w+1, left, top, width, height);
	}
}


ViewPort *IsPtInWindowViewport(Window *w, int x, int y)
{
	ViewPort *vp = w->viewport;
	if (vp != NULL &&
	    IS_INT_INSIDE(x, vp->left, vp->left + vp->width) &&
			IS_INT_INSIDE(y, vp->top, vp->top + vp->height))
		return vp;

	return NULL;
}

Point TranslateXYToTileCoord(ViewPort *vp, int x, int y) {
	int z;
	Point pt;
	int a,b;

	if ( (uint)(x -= vp->left) >= (uint)vp->width ||
				(uint)(y -= vp->top) >= (uint)vp->height) {
				Point pt = {-1, -1};
				return pt;
	}

	x = ((x << vp->zoom) + vp->virtual_left) >> 2;
	y = ((y << vp->zoom) + vp->virtual_top) >> 1;

#if !defined(NEW_ROTATION)
	a = y-x;
	b = y+x;
#else
	a = x+y;
	b = x-y;
#endif
	z = GetSlopeZ(a, b) >> 1;
	z = GetSlopeZ(a+z, b+z) >> 1;
	z = GetSlopeZ(a+z, b+z) >> 1;
	z = GetSlopeZ(a+z, b+z) >> 1;
	z = GetSlopeZ(a+z, b+z) >> 1;

	pt.x = a+z;
	pt.y = b+z;

	if ((uint)pt.x >= TILE_X_MAX*16 || (uint)pt.y >= TILE_Y_MAX*16) {
		pt.x = pt.y = -1;
	}

	return pt;
}

static Point GetTileFromScreenXY(int x, int y)
{
	Window *w;
	ViewPort *vp;
	Point pt;

	if ( (w = FindWindowFromPt(x, y)) != NULL &&
			 (vp = IsPtInWindowViewport(w, x, y)) != NULL)
				return TranslateXYToTileCoord(vp, x, y);

	pt.y = pt.x = -1;
	return pt;
}

Point GetTileBelowCursor()
{
	return GetTileFromScreenXY(_cursor.pos.x, _cursor.pos.y);
}


Point GetTileZoomCenterWindow(bool in, Window * w)
{
	int x, y;
	ViewPort * vp;

	vp = w->viewport;

	if (in)	{
		x = ( (_cursor.pos.x - vp->left ) >> 1) + (vp->width >> 2);
		y = ( (_cursor.pos.y - vp->top ) >> 1) + (vp->height >> 2);
	}
	else {
		x = vp->width - (_cursor.pos.x - vp->left);
		y = vp->height - (_cursor.pos.y - vp->top);
	}
	return GetTileFromScreenXY(x+vp->left, y+vp->top);
}

void DrawGroundSpriteAt(uint32 image, int16 x, int16 y, byte z)
{
	ViewportDrawer *vd = _cur_vd;
	TileSpriteToDraw *ts;

	assert( (image & 0x3fff) < NUM_SPRITES);

	ts = (TileSpriteToDraw*)vd->spritelist_mem;
	if ((byte*)ts >= vd->eof_spritelist_mem) {
		DEBUG(misc, 0) ("Out of sprite mem\n");
		return;
	}

	vd->spritelist_mem += sizeof(TileSpriteToDraw);

	ts->image = image;
	ts->next = NULL;
	ts->x = x;
	ts->y = y;
	ts->z = z;
	*vd->last_tile = ts;
	vd->last_tile = &ts->next;
}

void DrawGroundSprite(uint32 image)
{
	if (_offset_ground_sprites) {
		// offset ground sprite because of foundation?
		AddChildSpriteScreen(image, _cur_vd->offs_x, _cur_vd->offs_y);
	} else {
			_added_tile_sprite = true;
		DrawGroundSpriteAt(image, _cur_ti->x, _cur_ti->y, _cur_ti->z);
	}
}


void OffsetGroundSprite(int x, int y)
{
	_cur_vd->offs_x = x;
	_cur_vd->offs_y = y;
	_offset_ground_sprites = true;
}

static void AddCombinedSprite(uint32 image, int x, int y, byte z)
{
	ViewportDrawer *vd = _cur_vd;
	int t;
	uint32 image_org = image;
	const SpriteDimension *sd;
	Point pt = RemapCoords(x, y, z);

	sd = GetSpriteDimension(image & 0x3FFF);

	if ((t = pt.x + sd->xoffs) >= vd->dpi.left + vd->dpi.width ||
			(t + sd->xsize) <= vd->dpi.left ||
			(t = pt.y + sd->yoffs) >= vd->dpi.top + vd->dpi.height ||
			(t + sd->ysize) <= vd->dpi.top)
		return;

	AddChildSpriteScreen(image_org, pt.x - vd->parent_list[-1]->left, pt.y - vd->parent_list[-1]->top);
}


void AddSortableSpriteToDraw(uint32 image, int x, int y, int w, int h, byte dz, byte z)
{
	ViewportDrawer *vd = _cur_vd;
	ParentSpriteToDraw *ps;
	const SpriteDimension *sd;
	Point pt;

	assert( (image & 0x3fff) < NUM_SPRITES);

	if (vd->combine_sprites == 2) {
		AddCombinedSprite(image, x, y, z);
		return;
	}

	vd->last_child = NULL;

	ps = (ParentSpriteToDraw*)vd->spritelist_mem;
	if ((byte*)ps >= vd->eof_spritelist_mem) {
		DEBUG(misc, 0) ("Out of sprite mem\n");
		return;
	}
	if (vd->parent_list >= vd->eof_parent_list) {
		// This can happen rarely, mostly when you zoom out completely
		//  and have a lot of stuff that moves (and is added to the
		//  sort-list, this function). To solve it, increase
		//  parent_list somewhere below to a higher number.
		// This can not really hurt you, it just gives some black
		//  spots on the screen ;)
		DEBUG(misc, 0) ("Out of sprite mem (parent_list)\n");
		return;
	}

	vd->spritelist_mem += sizeof(ParentSpriteToDraw);

	ps->image = image;
	ps->tile_x = x;
	ps->tile_right = x + w - 1;

	ps->tile_y = y;
	ps->tile_bottom = y + h - 1;

	ps->tile_z = z;
	ps->tile_z_bottom = z + dz - 1;

	pt = RemapCoords(x, y, z);

	sd = GetSpriteDimension(image & 0x3FFF);
	if ((ps->left = (pt.x += sd->xoffs)) >= vd->dpi.left + vd->dpi.width ||
			(ps->right = (pt.x + sd->xsize)) <= vd->dpi.left ||
			(ps->top = (pt.y += sd->yoffs)) >= vd->dpi.top + vd->dpi.height ||
			(ps->bottom = (pt.y + sd->ysize)) <= vd->dpi.top) {
		return;
	}

	ps->unk16 = 0;
	ps->child = NULL;
	vd->last_child = &ps->child;

	*vd->parent_list++ = ps;

	if (vd->combine_sprites == 1) {
		vd->combine_sprites = 2;
	}
}

void StartSpriteCombine()
{
	_cur_vd->combine_sprites = 1;
}

void EndSpriteCombine()
{
	_cur_vd->combine_sprites = 0;
}

void AddChildSpriteScreen(uint32 image, int x, int y)
{
	ViewportDrawer *vd = _cur_vd;
	ChildScreenSpriteToDraw *cs;

	assert( (image & 0x3fff) < NUM_SPRITES);

	cs = (ChildScreenSpriteToDraw*) vd->spritelist_mem;
	if ((byte*)cs >= vd->eof_spritelist_mem) {
		DEBUG(misc,0) ("Out of sprite mem\n");
		return;
	}

	if (vd->last_child == NULL)
		return;

	vd->spritelist_mem += sizeof(ChildScreenSpriteToDraw);

	*vd->last_child = cs;
	vd->last_child = &cs->next;

	cs->image = image;
	cs->x = x;
	cs->y = y;
	cs->next = NULL;
}

/* Returns a StringSpriteToDraw */
void *AddStringToDraw(int x, int y, StringID string, uint32 params_1, uint32 params_2)
{
	ViewportDrawer *vd = _cur_vd;
	StringSpriteToDraw *ss;

	ss = (StringSpriteToDraw*)vd->spritelist_mem;
	if ((byte*)ss >= vd->eof_spritelist_mem) {
		DEBUG(misc, 0) ("Out of sprite mem\n");
		return NULL;
	}

	vd->spritelist_mem += sizeof(StringSpriteToDraw);

	ss->string = string;
	ss->next = NULL;
	ss->x = x;
	ss->y = y;
	ss->params[0] = params_1;
	ss->params[1] = params_2;
	ss->width = 0;

	*vd->last_string = ss;
	vd->last_string = &ss->next;

	return ss;
}

/* Debugging code */

#ifdef DEBUG_TILE_PUSH
static int _num_push;
static uint _pushed_tile[200];
static int _pushed_track[200];

static uint _stored_tile[200];
static int _stored_track[200];
static int _num_stored;

void dbg_store_path()
{
	memcpy(_stored_tile, _pushed_tile, sizeof(_stored_tile));
	memcpy(_stored_track, _pushed_track, sizeof(_stored_tile));
	_num_stored = _num_push;
	MarkWholeScreenDirty();
}

void dbg_push_tile(uint tile, int track)
{
	_pushed_tile[_num_push] = tile;
	_pushed_track[_num_push++] = track;
	dbg_store_path();
}

void dbg_pop_tile()
{
	_num_push--;
}

static const uint16 _dbg_track_sprite[] = {
	0x3F4,
	0x3F3,
	0x3F5,
	0x3F6,
	0x3F8,
	0x3F7,
};

static int dbg_draw_pushed(const TileInfo *ti)
{
	int i;
	if (ti->tile==0)
		return 0;
	for(i=0; i!=_num_stored; i++)
		if (_stored_tile[i] == ti->tile) {
			DrawGroundSpriteAt(_dbg_track_sprite[_stored_track[i]&7], ti->x, ti->y, ti->z);
		}
	return -1;
}

#endif

static void DrawSelectionSprite(uint32 image, const TileInfo *ti)
{
	if (_added_tile_sprite) {
		DrawGroundSpriteAt(image, ti->x, ti->y, ti->z + 7);
	} else {
		AddSortableSpriteToDraw(image, ti->x, ti->y, 0x10, 0x10, 1, ti->z + 7);
	}
}

static bool IsPartOfAutoLine(int px, int py)
{
	const TileHighlightData *thd = _thd_ptr;

	px -= thd->selstart.x;
	py -= thd->selstart.y;

	switch(thd->drawstyle) {
	case HT_LINE | 0: return px == py || px == py + 16;
	case HT_LINE | 1: return px == py || px == py - 16;
	case HT_LINE | 2: return px == -py || px == -py + 16;
	case HT_LINE | 3: return px == -py || px == -py - 16;
	default:
		NOT_REACHED();
	}

	/* useless, but avoids compiler warning this way */
	return 0;
}

static void DrawTileSelection(const TileInfo *ti)
{
	uint32 image;
	const TileHighlightData *thd = _thd_ptr;

#ifdef DEBUG_TILE_PUSH
	dbg_draw_pushed(ti);
#endif

	// Draw a red error square?
	if (thd->redsq != 0 && thd->redsq == (TileIndex)ti->tile) {
		DrawSelectionSprite(0x030382F0 | _tileh_to_sprite[ti->tileh], ti);
		return;
	}

	// no selection active?
	if (thd->drawstyle == 0)
		return;

	// Inside the inner area?
	if (IS_INSIDE_1D(ti->x, thd->pos.x, thd->size.x) && IS_INSIDE_1D(ti->y, thd->pos.y, thd->size.y)) {
		if (thd->drawstyle & HT_RECT) {
			image = 0x2F0 + _tileh_to_sprite[ti->tileh];
			if (thd->make_square_red) image |= 0x3048000;
			DrawSelectionSprite(image, ti);
		} else if (thd->drawstyle & HT_POINT) {
			// Figure out the Z coordinate for the single dot.
			byte z = ti->z;
			if (ti->tileh & 8) {
				z += 8;
				if (!(ti->tileh & 2) && (ti->tileh & 0x10)) {
					z += 8;
				}
			}
			DrawGroundSpriteAt(_cur_dpi->zoom != 2 ? 0x306 : 0xFEE,ti->x, ti->y, z);
		} else {
			if (IsPartOfAutoLine(ti->x, ti->y)) {
				image = 0x2F0 + _tileh_to_sprite[ti->tileh];
				if (thd->make_square_red) image |= 0x3048000;
				DrawSelectionSprite(image, ti);
			}
		}
		return;
	}

	// Check if it's inside the outer area?
	if (thd->outersize.x &&
			thd->size.x < thd->size.x + thd->outersize.x &&
			IS_INSIDE_1D(ti->x, thd->pos.x + thd->offs.x, thd->size.x + thd->outersize.x) &&
			IS_INSIDE_1D(ti->y, thd->pos.y + thd->offs.y, thd->size.y + thd->outersize.y)) {
		// Draw a blue rect.
		DrawSelectionSprite(0x30582F0 + _tileh_to_sprite[ti->tileh], ti);
		return;
	}
}

static void ViewportAddLandscape()
{
	ViewportDrawer *vd = _cur_vd;
	int x, y, width, height;
	TileInfo ti;
	bool direction;

	_cur_ti = &ti;

	// Transform into tile coordinates and round to closest full tile
#if !defined(NEW_ROTATION)
	x = ((vd->dpi.top >> 1) - (vd->dpi.left >> 2)) & ~0xF;
	y = ((vd->dpi.top >> 1) + (vd->dpi.left >> 2) - 0x10) & ~0xF;
#else
	x = ((vd->dpi.top >> 1) + (vd->dpi.left >> 2) - 0x10) & ~0xF;
	y = ((vd->dpi.left >> 2) - (vd->dpi.top >> 1)) & ~0xF;
#endif
	// determine size of area
	{
		Point pt = RemapCoords(x, y, 241);
		width = (vd->dpi.left + vd->dpi.width - pt.x + 95) >> 6;
		height = (vd->dpi.top + vd->dpi.height - pt.y) >> 5 << 1;
	}

	assert(width > 0);
	assert(height > 0);

	direction = false;

	do {
		int width_cur = width;
		int x_cur = x;
		int y_cur = y;

		do {
			FindLandscapeHeight(&ti, x_cur, y_cur);
#if !defined(NEW_ROTATION)
			y_cur += 0x10;
			x_cur -= 0x10;
#else
			y_cur += 0x10;
			x_cur += 0x10;
#endif
			_added_tile_sprite = false;
			_offset_ground_sprites = false;

			DrawTile(&ti);
			DrawTileSelection(&ti);
		} while (--width_cur);

#if !defined(NEW_ROTATION)
		if ( (direction^=1) != 0)
			y += 0x10;
		else
			x += 0x10;
#else
		if ( (direction^=1) != 0)
			x += 0x10;
		else
			y -= 0x10;
#endif
	} while (--height);
}


static void ViewportAddTownNames(DrawPixelInfo *dpi)
{
	Town *t;
	int left, top, right, bottom;

	if (!(_display_opt & DO_SHOW_TOWN_NAMES) || _game_mode == GM_MENU)
		return;

	left = dpi->left;
	top = dpi->top;
	right = left + dpi->width;
	bottom = top + dpi->height;

	if (dpi->zoom < 1) {
		FOR_ALL_TOWNS(t) {
			if (t->xy &&
			    bottom > t->sign.top &&
					top < t->sign.top + 12 &&
					right > t->sign.left &&
					left < t->sign.left + t->sign.width_1) {

				AddStringToDraw(t->sign.left + 1, t->sign.top + 1, STR_2001, t->townnametype, t->townnameparts);
			}
		}
	} else if (dpi->zoom == 1) {
		right += 2;
		bottom += 2;

		FOR_ALL_TOWNS(t) {
			if (t->xy &&
			    bottom > t->sign.top &&
					top < t->sign.top + 24 &&
					right > t->sign.left &&
					left < t->sign.left + t->sign.width_1*2) {

				AddStringToDraw(t->sign.left + 1, t->sign.top + 1, STR_2001, t->townnametype, t->townnameparts);
			}
		}
	} else {
		right += 4;
		bottom += 5;

		assert(dpi->zoom == 2);
		FOR_ALL_TOWNS(t) {
			if (t->xy &&
			    bottom > t->sign.top &&
					top < t->sign.top + 24 &&
					right > t->sign.left &&
					left < t->sign.left + t->sign.width_2*4) {

				AddStringToDraw(t->sign.left + 5, t->sign.top + 1, STR_2002, t->townnametype, t->townnameparts);
				AddStringToDraw(t->sign.left + 1, t->sign.top - 3, STR_2003, t->townnametype, t->townnameparts);
			}
		}
	}
}

static void ViewportAddStationNames(DrawPixelInfo *dpi)
{
	int left, top, right, bottom;
	Station *st;
	StringSpriteToDraw *sstd;

	if (!(_display_opt & DO_SHOW_STATION_NAMES) || _game_mode == GM_MENU)
		return;

	left = dpi->left;
	top = dpi->top;
	right = left + dpi->width;
	bottom = top + dpi->height;

	if (dpi->zoom < 1) {
		FOR_ALL_STATIONS(st) {
			if (st->xy &&
			    bottom > st->sign.top &&
					top < st->sign.top + 12 &&
					right > st->sign.left &&
					left < st->sign.left + st->sign.width_1) {

				sstd=AddStringToDraw(st->sign.left + 1, st->sign.top + 1, STR_305C_0, st->index, st->facilities);
				if (sstd != NULL) {
					sstd->color = (st->owner == OWNER_NONE || !st->facilities) ? 0xE : _player_colors[st->owner];
					sstd->width = st->sign.width_1;
				}
			}
		}
	} else if (dpi->zoom == 1) {
		right += 2;
		bottom += 2;

		FOR_ALL_STATIONS(st) {
			if (st->xy &&
			    bottom > st->sign.top &&
					top < st->sign.top + 24 &&
					right > st->sign.left &&
					left < st->sign.left + st->sign.width_1*2) {

				sstd=AddStringToDraw(st->sign.left + 1, st->sign.top + 1, STR_305C_0, st->index, st->facilities);
				if (sstd != NULL) {
					sstd->color = (st->owner == OWNER_NONE || !st->facilities) ? 0xE : _player_colors[st->owner];
					sstd->width = st->sign.width_1;
				}
			}
		}

	} else {
		assert(dpi->zoom == 2);

		right += 4;
		bottom += 5;

		FOR_ALL_STATIONS(st) {
			if (st->xy &&
			    bottom > st->sign.top &&
					top < st->sign.top + 24 &&
					right > st->sign.left &&
					left < st->sign.left + st->sign.width_2*4) {

				sstd=AddStringToDraw(st->sign.left + 1, st->sign.top + 1, STR_305D_0, st->index, st->facilities);
				if (sstd != NULL) {
					sstd->color = (st->owner == OWNER_NONE || !st->facilities) ? 0xE : _player_colors[st->owner];
					sstd->width = st->sign.width_2 | 0x8000;
				}
			}
		}
	}
}

static void ViewportAddSigns(DrawPixelInfo *dpi)
{
	SignStruct *ss;
	int left, top, right, bottom;
	StringSpriteToDraw *sstd;

	if (!(_display_opt & DO_SHOW_SIGNS))
		return;

	left = dpi->left;
	top = dpi->top;
	right = left + dpi->width;
	bottom = top + dpi->height;

	if (dpi->zoom < 1) {
		for(ss=_sign_list; ss != endof(_sign_list); ss++) {
			if (ss->str &&
					bottom > ss->sign.top &&
					top < ss->sign.top + 12 &&
					right > ss->sign.left &&
					left < ss->sign.left + ss->sign.width_1) {

				sstd=AddStringToDraw(ss->sign.left + 1, ss->sign.top + 1, STR_2806, ss->str, 0);
				if (sstd != NULL) {
					sstd->width = ss->sign.width_1;
					sstd->color = 14;
				}
			}
		}
	} else if (dpi->zoom == 1) {
		right += 2;
		bottom += 2;
		for(ss=_sign_list; ss != endof(_sign_list); ss++) {
			if (ss->str &&
					bottom > ss->sign.top &&
					top < ss->sign.top + 24 &&
					right > ss->sign.left &&
					left < ss->sign.left + ss->sign.width_1*2) {

				sstd=AddStringToDraw(ss->sign.left + 1, ss->sign.top + 1, STR_2806, ss->str, 0);
				if (sstd != NULL) {
					sstd->width = ss->sign.width_1;
					sstd->color = 14;
				}
			}
		}
	} else {
		right += 4;
		bottom += 5;

		for(ss=_sign_list; ss != endof(_sign_list); ss++) {
			if (ss->str &&
					bottom > ss->sign.top &&
					top < ss->sign.top + 24 &&
					right > ss->sign.left &&
					left < ss->sign.left + ss->sign.width_2*4) {

				sstd=AddStringToDraw(ss->sign.left + 1, ss->sign.top + 1, STR_2807, ss->str, 0);
				if (sstd != NULL) {
					sstd->width = ss->sign.width_2 | 0x8000;
					sstd->color = 14;
				}
			}
		}
	}
}

static void ViewportAddWaypoints(DrawPixelInfo *dpi)
{
	Waypoint *cp;

	int left, top, right, bottom;
	StringSpriteToDraw *sstd;

	if (!(_display_opt & DO_WAYPOINTS))
		return;

	left = dpi->left;
	top = dpi->top;
	right = left + dpi->width;
	bottom = top + dpi->height;

	if (dpi->zoom < 1) {
		for(cp=_waypoints; cp != endof(_waypoints); cp++) {
			if (cp->xy &&
					bottom > cp->sign.top &&
					top < cp->sign.top + 12 &&
					right > cp->sign.left &&
					left < cp->sign.left + cp->sign.width_1) {

				sstd=AddStringToDraw(cp->sign.left + 1, cp->sign.top + 1, STR_WAYPOINT_VIEWPORT, cp - _waypoints, 0);
				if (sstd != NULL) {
					sstd->width = cp->sign.width_1;
					sstd->color = (cp->deleted ? 0xE : 11);
				}
			}
		}
	} else if (dpi->zoom == 1) {
		right += 2;
		bottom += 2;
		for(cp=_waypoints; cp != endof(_waypoints); cp++) {
			if (cp->xy &&
					bottom > cp->sign.top &&
					top < cp->sign.top + 24 &&
					right > cp->sign.left &&
					left < cp->sign.left + cp->sign.width_1*2) {

				sstd=AddStringToDraw(cp->sign.left + 1, cp->sign.top + 1, STR_WAYPOINT_VIEWPORT, cp - _waypoints, 0);
				if (sstd != NULL) {
					sstd->width = cp->sign.width_1;
					sstd->color = (cp->deleted ? 0xE : 11);
				}
			}
		}
	} else {
		right += 4;
		bottom += 5;

		for(cp=_waypoints; cp != endof(_waypoints); cp++) {
			if (cp->xy &&
					bottom > cp->sign.top &&
					top < cp->sign.top + 24 &&
					right > cp->sign.left &&
					left < cp->sign.left + cp->sign.width_2*4) {

				sstd=AddStringToDraw(cp->sign.left + 1, cp->sign.top + 1, STR_WAYPOINT_VIEWPORT_TINY, cp - _waypoints, 0);
				if (sstd != NULL) {
					sstd->width = cp->sign.width_2 | 0x8000;
					sstd->color = (cp->deleted ? 0xE : 11);
				}
			}
		}
	}
}

void UpdateViewportSignPos(ViewportSign *sign, int left, int top, StringID str)
{
	char buffer[128];
	int w;

	sign->top = top;

	GetString(buffer, str);
	w = GetStringWidth(buffer) + 3;
	sign->width_1 = w;
	sign->left = left - (w >> 1);

	_stringwidth_base = 0xE0;
	w = GetStringWidth(buffer);
	_stringwidth_base = 0;
	sign->width_2 = w + 1;
}


static void ViewportDrawTileSprites(TileSpriteToDraw *ts)
{
	do {
		Point pt = RemapCoords(ts->x, ts->y, ts->z);
		DrawSprite(ts->image, pt.x, pt.y);
	} while ( (ts = ts->next) != NULL);
}

static void ViewportSortParentSprites(ParentSpriteToDraw **psd)
{
	ParentSpriteToDraw *ps, *ps2,*ps3, **psd2, **psd3;

	while((ps=*psd) != NULL) {
		if (!(ps->unk16 & 1)) {
			ps->unk16 |= 1;
			psd2 = psd;

			while ( (ps2=*++psd2) != NULL) {
				if (ps2->unk16 & 1)
					continue;

				if (ps->tile_right >= ps2->tile_x &&
						ps->tile_bottom >= ps2->tile_y &&
						ps->tile_z_bottom >= ps2->tile_z && (
						ps->tile_x >= ps2->tile_right ||
						ps->tile_y >= ps2->tile_bottom ||
						ps->tile_z >= ps2->tile_z_bottom
						)) {
					psd3 = psd;
					do {
						ps3 = *psd3;
						*psd3 = ps2;
						ps2 = ps3;

						psd3++;
					} while (psd3 <= psd2);
				}
			}
		} else {
			psd++;
		}
	}
}

static void ViewportDrawParentSprites(ParentSpriteToDraw **psd)
{
	ParentSpriteToDraw *ps;
	ChildScreenSpriteToDraw *cs;

	for (;(ps=*psd) != NULL;psd++) {
		Point pt = RemapCoords(ps->tile_x, ps->tile_y, ps->tile_z);
		DrawSprite(ps->image, pt.x, pt.y);

		cs = ps->child;
		while (cs) {
			DrawSprite(cs->image, ps->left + cs->x, ps->top + cs->y);
			cs = cs->next;
		}
	}
}

static void ViewportDrawStrings(DrawPixelInfo *dpi, StringSpriteToDraw *ss)
{
	DrawPixelInfo dp;
	byte zoom;

	_cur_dpi = &dp;
	dp = *dpi;

	zoom = (byte)dp.zoom;
	dp.zoom = 0;

	dp.left >>= zoom;
	dp.top >>= zoom;
	dp.width >>= zoom;
	dp.height >>= zoom;

	do {
		if (ss->width != 0) {
			int x, y, w, bottom;

			x = (ss->x >> zoom) - 1;
			y = (ss->y >> zoom) - 1;

			bottom = y + 11;

			w = ss->width;
			if (w & 0x8000) {
				w &= ~0x8000;
				y--;
				bottom -= 6;
				w -= 3;
			}

			DrawFrameRect(x,y, x+w, bottom, ss->color, (_display_opt & DO_TRANS_BUILDINGS) ? 0x9 : 0);
		}

		SET_DPARAM32(0, ss->params[0]);
		SET_DPARAM32(1, ss->params[1]);
		if (_display_opt & DO_TRANS_BUILDINGS && ss->width != 0) {
			/* This is such a frustrating mess - I need to convert
			 * from real color codes to string color codes and guess
			 * what, they are completely different. --pasky */
			static const byte ci2sci[17] = {
				/*  0 */ 16, /*  1 */  0, /*  2 */  5,
				/*  3X*/  2, /*  4 */  8, /*  5 */  3,
				/*  6X*/ 14, /*  7 */  7, /*  8 */  9,
				/*  9 */ 13, /* 10 */ 10, /* 11 */ 15,
				/* 12 */  4, /* 13 */  6, /* 14 */ 11,
				/* 15 */  1, /* 16 */ 12
			};
			byte color = ci2sci[ss->color + 1];

			DrawString(ss->x >> zoom, (ss->y >> zoom) - (ss->width&0x8000?2:0), ss->string, color);
		} else {
			DrawString(ss->x >> zoom, (ss->y >> zoom) - (ss->width&0x8000?2:0), ss->string, 16);
		}
	} while ( (ss = ss->next) != NULL);

	_cur_dpi = dpi;
}

void ViewportDoDraw(const ViewPort *vp, int left, int top, int right, int bottom)
{
	ViewportDrawer vd;
	int mask;
	int x,y;
	DrawPixelInfo *old_dpi;

	byte mem[VIEWPORT_DRAW_MEM];
	ParentSpriteToDraw *parent_list[6144];

	_cur_vd = &vd;

	old_dpi = _cur_dpi;
	_cur_dpi = &vd.dpi;

	vd.dpi.zoom = vp->zoom;
	mask = (-1) << vp->zoom;

	vd.combine_sprites = 0;
	vd.ground_child = 0;

	vd.dpi.width = (right - left) & mask;
	vd.dpi.height = (bottom - top) & mask;
	vd.dpi.left = left & mask;
	vd.dpi.top = top & mask;
	vd.dpi.pitch = old_dpi->pitch;

	x = ((vd.dpi.left - (vp->virtual_left&mask)) >> vp->zoom) + vp->left;
	y = ((vd.dpi.top - (vp->virtual_top&mask)) >> vp->zoom) + vp->top;

	vd.dpi.dst_ptr = old_dpi->dst_ptr + x - old_dpi->left + (y - old_dpi->top) * old_dpi->pitch;

	vd.parent_list = parent_list;
	vd.eof_parent_list = &parent_list[lengthof(parent_list)];
	vd.spritelist_mem = mem;
	vd.eof_spritelist_mem = &mem[sizeof(mem) - 0x40];
	vd.last_string = &vd.first_string;
	vd.first_string = NULL;
	vd.last_tile = &vd.first_tile;
	vd.first_tile = NULL;

	ViewportAddLandscape();
#if !defined(NEW_ROTATION)
	ViewportAddVehicles(&vd.dpi);
	DrawTextEffects(&vd.dpi);

	ViewportAddTownNames(&vd.dpi);
	ViewportAddStationNames(&vd.dpi);
	ViewportAddSigns(&vd.dpi);
	ViewportAddWaypoints(&vd.dpi);
#endif

	// This assert should never happen (because the length of the parent_list
	//  is checked)
	assert(vd.parent_list - parent_list <= lengthof(parent_list));

	if (vd.first_tile != NULL)
		ViewportDrawTileSprites(vd.first_tile);

	/* null terminate parent sprite list */
	*vd.parent_list = NULL;

	ViewportSortParentSprites(parent_list);
	ViewportDrawParentSprites(parent_list);

	if (vd.first_string != NULL)
		ViewportDrawStrings(&vd.dpi, vd.first_string);

	_cur_dpi = old_dpi;
}

// Make sure we don't draw a too big area at a time.
// If we do, the sprite memory will overflow.
static void ViewportDrawChk(ViewPort *vp, int left, int top, int right, int bottom)
{
	if (((bottom - top) * (right - left) << vp->zoom) > 180000) {
		if ((bottom - top) > (right - left)) {
			int t = (top + bottom) >> 1;
			ViewportDrawChk(vp, left, top, right, t);
			ViewportDrawChk(vp, left, t, right, bottom);
		} else {
			int t = (left + right) >> 1;
			ViewportDrawChk(vp, left, top, t, bottom);
			ViewportDrawChk(vp, t, top, right, bottom);
		}
	} else {
		ViewportDoDraw(vp,
			((left - vp->left) << vp->zoom) + vp->virtual_left,
			((top - vp->top) << vp->zoom) + vp->virtual_top,
			((right - vp->left) << vp->zoom) + vp->virtual_left,
			((bottom - vp->top) << vp->zoom) + vp->virtual_top
		);
	}
}

static void INLINE ViewportDraw(ViewPort *vp, int left, int top, int right, int bottom)
{
	int t;

	if (right <= vp->left ||
			bottom <= vp->top)
				return;

	if (left >= (t=vp->left + vp->width))
		return;

	if (left < vp->left) left = vp->left;
	if (right > t) right = t;

	if (top >= (t = vp->top + vp->height))
		return;

	if (top < vp->top) top = vp->top;
	if (bottom > t) bottom = t;

	ViewportDrawChk(vp, left, top, right, bottom);
}

void DrawWindowViewport(Window *w) {
	DrawPixelInfo *dpi = _cur_dpi;

	dpi->left += w->left;
	dpi->top += w->top;

	ViewportDraw(w->viewport, dpi->left, dpi->top, dpi->left + dpi->width, dpi->top + dpi->height);

	dpi->left -= w->left;
	dpi->top -= w->top;

}

void UpdateViewportPosition(Window *w)
{
	ViewPort *vp = w->viewport;

	if (WP(w,vp_d).follow_vehicle != 0xFFFF) {
		Vehicle *veh;
		Point pt;

		veh = &_vehicles[WP(w,vp_d).follow_vehicle];
		pt = MapXYZToViewport(vp, veh->x_pos, veh->y_pos, veh->z_pos);
		SetViewportPosition(w, pt.x, pt.y);
	} else {
		int x,y,t;
		int err;

		x = WP(w,vp_d).scrollpos_x >> 2;
		y = WP(w,vp_d).scrollpos_y >> 1;

#if !defined(NEW_ROTATION)
		t = x;
		x = y - t;
		y = y + t;

		// check if inside bounds?
		t = (-130) << vp->zoom;
		err = 0;
		if (y < t || y > (t += TILE_Y_MAX*16-1)) { y = t; err++; }
		if (x < (t=0) || x > (t=TILE_X_MAX*16-1) ) { x = t; err++; }
#else
		t = x;
		x = x + y;
		y = x - y;
		err= 0;
#endif

		if (err != 0) {
			/* coordinate remap */
			Point pt = RemapCoords(x, y, 0);
			t = (-1) << vp->zoom;
			WP(w,vp_d).scrollpos_x = pt.x & t;
			WP(w,vp_d).scrollpos_y = pt.y & t;
		}
		SetViewportPosition(w, WP(w,vp_d).scrollpos_x, WP(w,vp_d).scrollpos_y);
	}
}

static void MarkViewportDirty(ViewPort *vp, int left, int top, int right, int bottom)
{
	if ( (right -= vp->virtual_left) <= 0)
		return;

	if ( (bottom -= vp->virtual_top) <= 0)
		return;

	if ( (left -= vp->virtual_left) < 0)
		left = 0;

	if ((uint)left >= (uint)vp->virtual_width)
		return;

	if ( (top -= vp->virtual_top) < 0)
		top = 0;

	if ((uint)top >= (uint)vp->virtual_height)
		return;

	SetDirtyBlocks(
		(left >> vp->zoom) + vp->left,
		(top >> vp->zoom) + vp->top,
		(right >> vp->zoom) + vp->left,
		(bottom >> vp->zoom) + vp->top
	);
}

void MarkAllViewportsDirty(int left, int top, int right, int bottom)
{
	ViewPort *vp = _viewports;
	uint32 act = _active_viewports;
	do {
		if (act & 1) {
			assert(vp->width != 0);
			MarkViewportDirty(vp, left, top, right, bottom);
		}
	} while (vp++,act>>=1);
}

void MarkTileDirtyByTile(TileIndex tile) {
	Point pt = RemapCoords(GET_TILE_X(tile) * 16, GET_TILE_Y(tile) * 16, GetTileZ(tile));
	MarkAllViewportsDirty(
		pt.x - 31,
		pt.y - 122,
		pt.x - 31 + 67,
		pt.y - 122 + 154
	);
}

void MarkTileDirty(int x, int y)
{
	int z = 0;
	Point pt;
	if (IS_INT_INSIDE(x, 0, TILES_X*16) && IS_INT_INSIDE(y, 0, TILES_Y*16))
		z = GetTileZ(TILE_FROM_XY(x,y));
	pt = RemapCoords(x, y, z);

	MarkAllViewportsDirty(
		pt.x - 31,
		pt.y - 122,
		pt.x - 31 + 67,
		pt.y - 122 + 154
	);
}

static void SetSelectionTilesDirty()
{
	int y_size, x_size;
	TileHighlightData *thd = _thd_ptr;
	int x = thd->pos.x;
	int y = thd->pos.y;

	x_size=thd->size.x;
	y_size=thd->size.y;

	if (thd->outersize.x) {
		x_size += thd->outersize.x;
		x += thd->offs.x;
		y_size += thd->outersize.y;
		y += thd->offs.y;
	}

	assert(x_size > 0);
	assert(y_size > 0);

	x_size += x;
	y_size += y;

	do {
		int y_bk = y;
		do {
			MarkTileDirty(x, y);
		} while ( (y+=16) != y_size);
		y = y_bk;
	} while ( (x+=16) != x_size);
}


static bool CheckClickOnTown(ViewPort *vp, int x, int y)
{
	Town *t;

	if (!(_display_opt & DO_SHOW_TOWN_NAMES))
		return false;

	if (vp->zoom < 1) {
		x = x - vp->left + vp->virtual_left;
		y = y - vp->top + vp->virtual_top;

		FOR_ALL_TOWNS(t) {
			if (t->xy &&
			    y >= t->sign.top &&
					y < t->sign.top + 12 &&
					x >= t->sign.left &&
					x < t->sign.left + t->sign.width_1) {
				ShowTownViewWindow(t->index);
				return true;
			}
		}
	} else if (vp->zoom == 1) {
		x = (x - vp->left + 1) * 2 + vp->virtual_left;
		y = (y - vp->top + 1) * 2 + vp->virtual_top;
		FOR_ALL_TOWNS(t) {
			if (t->xy &&
			    y >= t->sign.top &&
					y < t->sign.top + 24 &&
					x >= t->sign.left &&
					x < t->sign.left + t->sign.width_1 * 2) {
				ShowTownViewWindow(t->index);
				return true;
			}
		}
	} else {
		x = (x - vp->left + 3) * 4 + vp->virtual_left;
		y = (y - vp->top + 3) * 4 + vp->virtual_top;
		FOR_ALL_TOWNS(t) {
			if (t->xy &&
			    y >= t->sign.top &&
					y < t->sign.top + 24 &&
					x >= t->sign.left &&
					x < t->sign.left + t->sign.width_2 * 4) {
				ShowTownViewWindow(t->index);
				return true;
			}
		}
	}

	return false;
}

static bool CheckClickOnStation(ViewPort *vp, int x, int y)
{
	Station *st;

	if (!(_display_opt & DO_SHOW_STATION_NAMES))
		return false;

	if (vp->zoom < 1) {
		x = x - vp->left + vp->virtual_left;
		y = y - vp->top + vp->virtual_top;

		FOR_ALL_STATIONS(st) {
			if (st->xy &&
			    y >= st->sign.top &&
					y < st->sign.top + 12 &&
					x >= st->sign.left &&
					x < st->sign.left + st->sign.width_1) {
				ShowStationViewWindow(st->index);
				return true;
			}
		}
	} else if (vp->zoom == 1) {
		x = (x - vp->left + 1) * 2 + vp->virtual_left;
		y = (y - vp->top + 1) * 2 + vp->virtual_top;
		FOR_ALL_STATIONS(st) {
			if (st->xy &&
			    y >= st->sign.top &&
					y < st->sign.top + 24 &&
					x >= st->sign.left &&
					x < st->sign.left + st->sign.width_1 * 2) {
				ShowStationViewWindow(st->index);
				return true;
			}
		}
	} else {
		x = (x - vp->left + 3) * 4 + vp->virtual_left;
		y = (y - vp->top + 3) * 4 + vp->virtual_top;
		FOR_ALL_STATIONS(st) {
			if (st->xy &&
			    y >= st->sign.top &&
					y < st->sign.top + 24 &&
					x >= st->sign.left &&
					x < st->sign.left + st->sign.width_2 * 4) {
				ShowStationViewWindow(st->index);
				return true;
			}
		}
	}

	return false;
}

static bool CheckClickOnSign(ViewPort *vp, int x, int y)
{
	SignStruct *ss;

	if (!(_display_opt & DO_SHOW_SIGNS))
		return false;

	if (vp->zoom < 1) {
		x = x - vp->left + vp->virtual_left;
		y = y - vp->top + vp->virtual_top;

		for(ss = _sign_list; ss != endof(_sign_list); ss++) {
			if (ss->str &&
			    y >= ss->sign.top &&
					y < ss->sign.top + 12 &&
					x >= ss->sign.left &&
					x < ss->sign.left + ss->sign.width_1) {
				ShowRenameSignWindow(ss);
				return true;
			}
		}
	} else if (vp->zoom == 1) {
		x = (x - vp->left + 1) * 2 + vp->virtual_left;
		y = (y - vp->top + 1) * 2 + vp->virtual_top;
		for(ss = _sign_list; ss != endof(_sign_list); ss++) {
			if (ss->str &&
			    y >= ss->sign.top &&
					y < ss->sign.top + 24 &&
					x >= ss->sign.left &&
					x < ss->sign.left + ss->sign.width_1 * 2) {
				ShowRenameSignWindow(ss);
				return true;
			}
		}
	} else {
		x = (x - vp->left + 3) * 4 + vp->virtual_left;
		y = (y - vp->top + 3) * 4 + vp->virtual_top;
		for(ss = _sign_list; ss != endof(_sign_list); ss++) {
			if (ss->str &&
			    y >= ss->sign.top &&
					y < ss->sign.top + 24 &&
					x >= ss->sign.left &&
					x < ss->sign.left + ss->sign.width_2 * 4) {
				ShowRenameSignWindow(ss);
				return true;
			}
		}
	}

	return false;
}

static bool CheckClickOnWaypoint(ViewPort *vp, int x, int y)
{
	Waypoint *cp;

	if (!(_display_opt & DO_WAYPOINTS))
		return false;

	if (vp->zoom < 1) {
		x = x - vp->left + vp->virtual_left;
		y = y - vp->top + vp->virtual_top;

		for(cp = _waypoints; cp != endof(_waypoints); cp++) {
			if (cp->xy &&
			    y >= cp->sign.top &&
					y < cp->sign.top + 12 &&
					x >= cp->sign.left &&
					x < cp->sign.left + cp->sign.width_1) {
				ShowRenameWaypointWindow(cp);
				return true;
			}
		}
	} else if (vp->zoom == 1) {
		x = (x - vp->left + 1) * 2 + vp->virtual_left;
		y = (y - vp->top + 1) * 2 + vp->virtual_top;
		for(cp = _waypoints; cp != endof(_waypoints); cp++) {
			if (cp->xy &&
			    y >= cp->sign.top &&
					y < cp->sign.top + 24 &&
					x >= cp->sign.left &&
					x < cp->sign.left + cp->sign.width_1 * 2) {
				ShowRenameWaypointWindow(cp);
				return true;
			}
		}
	} else {
		x = (x - vp->left + 3) * 4 + vp->virtual_left;
		y = (y - vp->top + 3) * 4 + vp->virtual_top;
		for(cp = _waypoints; cp != endof(_waypoints); cp++) {
			if (cp->xy &&
			    y >= cp->sign.top &&
					y < cp->sign.top + 24 &&
					x >= cp->sign.left &&
					x < cp->sign.left + cp->sign.width_2 * 4) {
				ShowRenameWaypointWindow(cp);
				return true;
			}
		}
	}

	return false;
}


static void CheckClickOnLandscape(ViewPort *vp, int x, int y)
{
	Point pt = TranslateXYToTileCoord(vp,x,y);
	if (pt.x != -1) {
		uint tile = TILE_FROM_XY(pt.x, pt.y);
		ClickTile(tile);
	}
}

void HandleClickOnTrain(Vehicle *v);
void HandleClickOnRoadVeh(Vehicle *v);
void HandleClickOnAircraft(Vehicle *v);
void HandleClickOnShip(Vehicle *v);
void HandleClickOnSpecialVeh(Vehicle *v) {}
void HandleClickOnDisasterVeh(Vehicle *v);
typedef void OnVehicleClickProc(Vehicle *v);
static OnVehicleClickProc * const _on_vehicle_click_proc[6] = {
	HandleClickOnTrain,
	HandleClickOnRoadVeh,
	HandleClickOnShip,
	HandleClickOnAircraft,
	HandleClickOnSpecialVeh,
	HandleClickOnDisasterVeh,
};

void HandleViewportClicked(ViewPort *vp, int x, int y)
{
	if (CheckClickOnTown(vp, x, y))
		return;

	if (CheckClickOnStation(vp, x, y))
		return;

	if (CheckClickOnSign(vp, x, y))
		return;

	if (CheckClickOnWaypoint(vp, x, y))
		return;

	CheckClickOnLandscape(vp, x, y);

	{
		Vehicle *v = CheckClickOnVehicle(vp, x, y);
		if (v) _on_vehicle_click_proc[v->type - 0x10](v);
	}
}

Vehicle *CheckMouseOverVehicle()
{
	Window *w;
	ViewPort *vp;

	int x = _cursor.pos.x;
	int y = _cursor.pos.y;

	w = FindWindowFromPt(x, y);
	if (w == NULL)
		return NULL;

	vp = IsPtInWindowViewport(w, x, y);
	if (vp) {
		return CheckClickOnVehicle(vp, x, y);
	} else {
		return NULL;
	}
}



void PlaceObject()
{
	Point pt;
	Window *w;
	WindowEvent e;

	pt = GetTileBelowCursor();
	if (pt.x == -1)
		return;

	if (_thd.place_mode == 2) {
		pt.x += 8;
		pt.y += 8;
	}

	_tile_fract_coords.x = pt.x & 0xF;
	_tile_fract_coords.y = pt.y & 0xF;

	if ((w = GetCallbackWnd()) != NULL) {
		e.event = WE_PLACE_OBJ;
		e.place.pt = pt;
		e.place.tile = TILE_FROM_XY(pt.x, pt.y);
		w->wndproc(w, &e);
	}
}


/* scrolls the viewport in a window to a given location */
bool ScrollWindowTo(int x , int y, Window * w)
{
	Point pt;

	pt = MapXYZToViewport(w->viewport, x, y, GetSlopeZ(x, y));
	WP(w,vp_d).follow_vehicle = -1;

	if (WP(w,vp_d).scrollpos_x == pt.x &&
			WP(w,vp_d).scrollpos_y == pt.y)
				return false;

	WP(w,vp_d).scrollpos_x = pt.x;
	WP(w,vp_d).scrollpos_y = pt.y;
	return true;
}

/* scrolls the viewport in a window to a given tile */
bool ScrollWindowToTile(TileIndex tile, Window * w)
{
	return ScrollWindowTo(GET_TILE_X(tile)*16+8, GET_TILE_Y(tile)*16+8, w);
}



bool ScrollMainWindowTo(int x, int y)
{
	Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
	Point pt;

	pt = MapXYZToViewport(w->viewport, x, y, GetSlopeZ(x, y));
	WP(w,vp_d).follow_vehicle = -1;

	if (WP(w,vp_d).scrollpos_x == pt.x &&
			WP(w,vp_d).scrollpos_y == pt.y)
				return false;

	WP(w,vp_d).scrollpos_x = pt.x;
	WP(w,vp_d).scrollpos_y = pt.y;
	return true;
}


bool ScrollMainWindowToTile(TileIndex tile)
{
	return ScrollMainWindowTo(GET_TILE_X(tile)*16+8, GET_TILE_Y(tile)*16+8);
}

void SetRedErrorSquare(TileIndex tile)
{
	TileIndex old;

	old = _thd.redsq;
	_thd.redsq = tile;

	if (tile != old) {
		if (tile != 0) MarkTileDirtyByTile(tile);
		if (old  != 0) MarkTileDirtyByTile(old);
	}
}

void SetTileSelectSize(int w, int h)
{
	TileHighlightData *thd = _thd_ptr;
	thd->new_size.x = w * 16;
	thd->new_size.y = h * 16;
	thd->new_outersize.x = 0;
	thd->new_outersize.y = 0;
}

void SetTileSelectBigSize(int ox, int oy, int sx, int sy) {
	TileHighlightData *thd = _thd_ptr;
	thd->offs.x = ox * 16;
	thd->offs.y = oy * 16;
	thd->new_outersize.x = sx * 16;
	thd->new_outersize.y = sy * 16;
}


void UpdateTileSelection()
{
	TileHighlightData *thd = _thd_ptr;
	Point pt;
	int x1,y1;

	thd->new_drawstyle = 0;

	if (thd->place_mode == 3) {
		x1 = thd->selend.x;
		y1 = thd->selend.y;
		if (x1 != -1) {
			int x2 = thd->selstart.x;
			int y2 = thd->selstart.y;
			x1 &= ~0xF;
			y1 &= ~0xF;

			if (x1 >= x2) intswap(x1,x2);
			if (y1 >= y2) intswap(y1,y2);
			thd->new_pos.x = x1;
			thd->new_pos.y = y1;
			thd->new_size.x = x2 - x1 + 16;
			thd->new_size.y = y2 - y1 + 16;
			thd->new_drawstyle = thd->next_drawstyle;
		}
	} else if (thd->place_mode != 0) {
		pt = GetTileBelowCursor();
		x1 = pt.x;
		y1 = pt.y;
		if (x1 != -1) {
			if (thd->place_mode == 1) {
				thd->new_drawstyle = HT_RECT;
			} else {
				thd->new_drawstyle = HT_POINT;
				x1 += 8;
				y1 += 8;
			}
			thd->new_pos.x = x1 & ~0xF;
			thd->new_pos.y = y1 & ~0xF;
		}
	}

	if (thd->drawstyle != thd->new_drawstyle ||
			thd->pos.x != thd->new_pos.x || thd->pos.y != thd->new_pos.y ||
			thd->size.x != thd->new_size.x || thd->size.y != thd->new_size.y) {

		// clear the old selection?
		if (thd->drawstyle) SetSelectionTilesDirty();

		thd->drawstyle = thd->new_drawstyle;
		thd->pos = thd->new_pos;
		thd->size = thd->new_size;
		thd->outersize = thd->new_outersize;
		thd->dirty = 0xff;

		// draw the new selection?
		if (thd->new_drawstyle) SetSelectionTilesDirty();
	}
}

void VpStartPlaceSizing(uint tile, int user)
{
	TileHighlightData *thd;

	thd = _thd_ptr;
	thd->userdata = user;
	thd->selend.x = GET_TILE_X(tile)*16;
	thd->selstart.x = GET_TILE_X(tile)*16;
	thd->selend.y = GET_TILE_Y(tile)*16;
	thd->selstart.y = GET_TILE_Y(tile)*16;
	if (thd->place_mode == 1) {
		thd->place_mode = 3;
		thd->next_drawstyle = HT_RECT;
	} else {
		thd->place_mode = 3;
		thd->next_drawstyle = HT_POINT;
	}
	_special_mouse_mode = WSM_SIZING;
}

void VpSetPlaceSizingLimit(int limit)
{
	_thd.sizelimit = limit;
}

void VpSetPresizeRange(uint from, uint to)
{
	TileHighlightData *thd = _thd_ptr;
	thd->selend.x = GET_TILE_X(to)*16;
	thd->selend.y = GET_TILE_Y(to)*16;
	thd->selstart.x = GET_TILE_X(from)*16;
	thd->selstart.y = GET_TILE_Y(from)*16;
	thd->next_drawstyle = HT_RECT;
}

void VpStartPreSizing()
{
	_thd.selend.x = -1;
	_special_mouse_mode = WSM_PRESIZE;
}

static void CalcRaildirsDrawstyle(TileHighlightData *thd, int x, int y)
{
	int d;
	bool b;
	uint w,h;

	w =	myabs((x & ~0xF) - thd->selstart.x) + 16;
	h = myabs((y & ~0xF) - thd->selstart.y) + 16;

	// vertical and horizontal lines are really simple
	if (w == 16 || h == 16) {
		b = HT_RECT;
	} else if (w * 2 < h) { // see if we're closer to rect?
		x = thd->selstart.x;
		b = HT_RECT;
	} else if (w > h * 2) {
		y = thd->selstart.y;
		b = HT_RECT;
	} else {
		d = w - h;

		// four cases.
		if (x > thd->selstart.x) {
			if (y > thd->selstart.y) {
				// south
				if (d ==0) b = (x & 0xF) > (y & 0xF) ? HT_LINE | 0 : HT_LINE | 1;
				else if (d >= 0) { x = thd->selstart.x + h; b = HT_LINE | 0; } // return px == py || px == py + 16;
				else { y = thd->selstart.y + w; b = HT_LINE | 1; } // return px == py || px == py - 16;
			} else {
				// west
				if (d ==0) b = (x & 0xF) + (y & 0xF) >= 0x10 ? HT_LINE | 2 : HT_LINE | 3;
				else if (d >= 0) { x = thd->selstart.x + h; b = HT_LINE | 2; }
				else { y = thd->selstart.y - w; b = HT_LINE | 3; }
			}
		} else {
			if (y > thd->selstart.y) {
				// east
				if (d ==0) b = (x & 0xF) + (y & 0xF) >= 0x10 ? HT_LINE | 2 : HT_LINE | 3;
				else if (d >= 0) { x = thd->selstart.x - h; b = HT_LINE | 3; } // return px == -py || px == -py - 16;
				else { y = thd->selstart.y + w; b = HT_LINE | 2; } // return px == -py || px == -py + 16;
			} else {
				// north
				if (d ==0) b = (x & 0xF) > (y & 0xF) ? HT_LINE | 0 : HT_LINE | 1;
				else if (d >= 0) { x = thd->selstart.x - h; b = HT_LINE | 1; } // return px == py || px == py - 16;
				else { y = thd->selstart.y - w; b = HT_LINE | 0; } //return px == py || px == py + 16;
			}
		}
	}
	thd->selend.x = x;
	thd->selend.y = y;
	thd->next_drawstyle = b;
}

void VpSelectTilesWithMethod(int x, int y, int method)
{
	TileHighlightData *thd = _thd_ptr;
	int sx,sy;

	if (x == -1) {
		thd->selend.x = -1;
		return;
	}

	// allow drag in any rail direction
	if (method == VPM_RAILDIRS || method == VPM_SIGNALDIRS) {
		CalcRaildirsDrawstyle(thd, x, y);
		return;
	}

	if (_thd.next_drawstyle == HT_POINT) { x += 8; y += 8; }

	//thd->next_drawstyle = HT_RECT;

	sx = thd->selstart.x;
	sy = thd->selstart.y;

	switch(method) {
	case VPM_FIX_X:
		x = sx;
		break;

	case VPM_FIX_Y:
		y = sy;
		break;

	case VPM_X_OR_Y:
		if (myabs(sy - y) < myabs(sx - x)) y = sy; else x = sx;
		break;

	case VPM_X_AND_Y:
		break;

	// limit the selected area to a 10x10 rect.
	case VPM_X_AND_Y_LIMITED: {
		int limit = (thd->sizelimit-1) * 16;
		x = sx + clamp(x - sx, -limit, limit);
		y = sy + clamp(y - sy, -limit, limit);
		break;
	}
	}

	thd->selend.x = x;
	thd->selend.y = y;
}

bool VpHandlePlaceSizingDrag()
{
	Window *w;
	WindowEvent e;

	if (_special_mouse_mode != WSM_SIZING)
		return true;

	e.place.userdata = _thd.userdata;

	w = FindWindowById(_thd.window_class,_thd.window_number);
	if (w == NULL) {
		ResetObjectToPlace();
		return false;
	}

	// while dragging...
	if (_left_button_down) {
		e.event = WE_PLACE_DRAG;
		e.place.pt = GetTileBelowCursor();
		w->wndproc(w, &e);
		return false;
	}

	// mouse button released..
	// keep the selected tool, but reset it to the original mode.
	_special_mouse_mode = WSM_NONE;
	_thd.place_mode = (_thd.next_drawstyle == HT_RECT || _thd.next_drawstyle & HT_LINE) ? 1 : 2;

	SetTileSelectSize(1, 1);

	// and call the mouseup event.
	e.event = WE_PLACE_MOUSEUP;
	e.place.pt = _thd.selend;
	e.place.tile = TILE_FROM_XY(e.place.pt.x, e.place.pt.y);
	e.place.starttile = TILE_FROM_XY(_thd.selstart.x, _thd.selstart.y);
	w->wndproc(w, &e);

	return false;
}

void SetObjectToPlaceWnd(int icon, byte mode, Window *w)
{
	SetObjectToPlace(icon,mode,w->window_class, w->window_number);
}

#include "table/animcursors.h"

void SetObjectToPlace(int icon, byte mode, byte window_class, uint16 window_num)
{
	TileHighlightData *thd = _thd_ptr;
	Window *w;

	if (thd->place_mode != 0) {
		thd->place_mode = 0;
		w = FindWindowById(thd->window_class, thd->window_number);
		if (w != NULL)
			CallWindowEventNP(w, WE_ABORT_PLACE_OBJ);
	}

	SetTileSelectSize(1, 1);

	thd->make_square_red = false;

	if (mode == 4) {
		mode = 0;
		_special_mouse_mode = WSM_DRAGDROP;
	} else {
		_special_mouse_mode = WSM_NONE;
	}

	thd->place_mode = mode;
	thd->window_class = window_class;
	thd->window_number = window_num;

	if (mode == 3)
		VpStartPreSizing();

	if ( (int)icon < 0)
		SetAnimatedMouseCursor(_animcursors[~icon]);
	else
		SetMouseCursor(icon);
}

void ResetObjectToPlace(){
	SetObjectToPlace(0,0,0,0);
}
