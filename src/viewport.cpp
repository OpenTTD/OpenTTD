/* $Id$ */

/** @file viewport.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "gui.h"
#include "spritecache.h"
#include "strings.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "landscape.h"
#include "map.h"
#include "viewport.h"
#include "window.h"
#include "vehicle.h"
#include "station.h"
#include "gfx.h"
#include "town.h"
#include "signs.h"
#include "waypoint.h"
#include "variables.h"
#include "train.h"

#define VIEWPORT_DRAW_MEM (65536 * 2)

ZoomLevel _saved_scrollpos_zoom;

/* XXX - maximum viewports is maximum windows - 2 (main toolbar + status bar) */
static ViewPort _viewports[25 - 2];
static uint32 _active_viewports;    ///< bitmasked variable where each bit signifies if a viewport is in use or not
assert_compile(lengthof(_viewports) < sizeof(_active_viewports) * 8);

static bool _added_tile_sprite;
static bool _offset_ground_sprites;

/* The in-game coordiante system looks like this *
 *                                               *
 *                    ^ Z                        *
 *                    |                          *
 *                    |                          *
 *                    |                          *
 *                    |                          *
 *                 /     \                       *
 *              /           \                    *
 *           /                 \                 *
 *        /                       \              *
 *   X <                             > Y         *
 */

struct StringSpriteToDraw {
	uint16 string;
	uint16 color;
	StringSpriteToDraw *next;
	int32 x;
	int32 y;
	uint32 params[2];
	uint16 width;
};

struct TileSpriteToDraw {
	SpriteID image;
	SpriteID pal;
	TileSpriteToDraw *next;
	int32 x;
	int32 y;
	byte z;
};

struct ChildScreenSpriteToDraw {
	SpriteID image;
	SpriteID pal;
	int32 x;
	int32 y;
	ChildScreenSpriteToDraw *next;
};

struct ParentSpriteToDraw {
	SpriteID image;
	SpriteID pal;
	int32 left;
	int32 top;
	int32 right;
	int32 bottom;
	int32 xmin;
	int32 ymin;
	int32 xmax;
	int32 ymax;
	ChildScreenSpriteToDraw *child;
	byte unk16;
	byte zmin;
	byte zmax;
};

/* Quick hack to know how much memory to reserve when allocating from the spritelist
 * to prevent a buffer overflow. */
#define LARGEST_SPRITELIST_STRUCT ParentSpriteToDraw

struct ViewportDrawer {
	DrawPixelInfo dpi;

	byte *spritelist_mem;
	const byte *eof_spritelist_mem;

	StringSpriteToDraw **last_string, *first_string;
	TileSpriteToDraw **last_tile, *first_tile;

	ChildScreenSpriteToDraw **last_child;

	ParentSpriteToDraw **parent_list;
	ParentSpriteToDraw * const *eof_parent_list;

	byte combine_sprites;

	int offs_x, offs_y; // used when drawing ground sprites relative
};

static ViewportDrawer *_cur_vd;

TileHighlightData _thd;
static TileInfo *_cur_ti;

extern void SmallMapCenterOnCurrentPos(Window *w);

static Point MapXYZToViewport(const ViewPort *vp, uint x, uint y, uint z)
{
	Point p = RemapCoords(x, y, z);
	p.x -= vp->virtual_width / 2;
	p.y -= vp->virtual_height / 2;
	return p;
}

void InitViewports() {
	memset(_viewports, 0, sizeof(_viewports));
	_active_viewports = 0;
}

void DeleteWindowViewport(Window *w)
{
	CLRBIT(_active_viewports, w->viewport - _viewports);
	w->viewport->width = 0;
	w->viewport = NULL;
}

void AssignWindowViewport(Window *w, int x, int y,
	int width, int height, uint32 follow_flags, ZoomLevel zoom)
{
	ViewPort *vp;
	Point pt;
	uint32 bit;

	for (vp = _viewports, bit = 0; ; vp++, bit++) {
		assert(vp != endof(_viewports));
		if (vp->width == 0) break;
	}
	SETBIT(_active_viewports, bit);

	vp->left = x + w->left;
	vp->top = y + w->top;
	vp->width = width;
	vp->height = height;

	vp->zoom = zoom;

	vp->virtual_width = width << zoom;
	vp->virtual_height = height << zoom;

	if (follow_flags & 0x80000000) {
		const Vehicle *veh;

		WP(w, vp_d).follow_vehicle = (VehicleID)(follow_flags & 0xFFFF);
		veh = GetVehicle(WP(w, vp_d).follow_vehicle);
		pt = MapXYZToViewport(vp, veh->x_pos, veh->y_pos, veh->z_pos);
	} else {
		uint x = TileX(follow_flags) * TILE_SIZE;
		uint y = TileY(follow_flags) * TILE_SIZE;

		WP(w, vp_d).follow_vehicle = INVALID_VEHICLE;
		pt = MapXYZToViewport(vp, x, y, GetSlopeZ(x, y));
	}

	WP(w, vp_d).scrollpos_x = pt.x;
	WP(w, vp_d).scrollpos_y = pt.y;
	w->viewport = vp;
	vp->virtual_left = 0;//pt.x;
	vp->virtual_top = 0;//pt.y;
}

static Point _vp_move_offs;

static void DoSetViewportPosition(Window* const *wz, int left, int top, int width, int height)
{

	for (; wz != _last_z_window; wz++) {
		const Window *w = *wz;

		if (left + width > w->left &&
				w->left + w->width > left &&
				top + height > w->top &&
				w->top + w->height > top) {

			if (left < w->left) {
				DoSetViewportPosition(wz, left, top, w->left - left, height);
				DoSetViewportPosition(wz, left + (w->left - left), top, width - (w->left - left), height);
				return;
			}

			if (left + width > w->left + w->width) {
				DoSetViewportPosition(wz, left, top, (w->left + w->width - left), height);
				DoSetViewportPosition(wz, left + (w->left + w->width - left), top, width - (w->left + w->width - left) , height);
				return;
			}

			if (top < w->top) {
				DoSetViewportPosition(wz, left, top, width, (w->top - top));
				DoSetViewportPosition(wz, left, top + (w->top - top), width, height - (w->top - top));
				return;
			}

			if (top + height > w->top + w->height) {
				DoSetViewportPosition(wz, left, top, width, (w->top + w->height - top));
				DoSetViewportPosition(wz, left, top + (w->top + w->height - top), width , height - (w->top + w->height - top));
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
			RedrawScreenRect(left, top, left + width, top + height);
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

static void SetViewportPosition(Window *w, int x, int y)
{
	ViewPort *vp = w->viewport;
	int old_left = vp->virtual_left;
	int old_top = vp->virtual_top;
	int i;
	int left, top, width, height;

	vp->virtual_left = x;
	vp->virtual_top = y;

	old_left = UnScaleByZoom(old_left, vp->zoom);
	old_top = UnScaleByZoom(old_top, vp->zoom);
	x = UnScaleByZoom(x, vp->zoom);
	y = UnScaleByZoom(y, vp->zoom);

	old_left -= x;
	old_top -= y;

	if (old_top == 0 && old_left == 0) return;

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

	i = left + width - _screen.width;
	if (i >= 0) width -= i;

	if (width > 0) {
		if (top < 0) {
			height += top;
			top = 0;
		}

		i = top + height - _screen.height;
		if (i >= 0) height -= i;

		if (height > 0) DoSetViewportPosition(FindWindowZPosition(w) + 1, left, top, width, height);
	}
}


ViewPort *IsPtInWindowViewport(const Window *w, int x, int y)
{
	ViewPort *vp = w->viewport;

	if (vp != NULL &&
	    IS_INT_INSIDE(x, vp->left, vp->left + vp->width) &&
			IS_INT_INSIDE(y, vp->top, vp->top + vp->height))
		return vp;

	return NULL;
}

static Point TranslateXYToTileCoord(const ViewPort *vp, int x, int y)
{
	Point pt;
	int a,b;
	uint z;

	if ( (uint)(x -= vp->left) >= (uint)vp->width ||
				(uint)(y -= vp->top) >= (uint)vp->height) {
				Point pt = {-1, -1};
				return pt;
	}

	x = (ScaleByZoom(x, vp->zoom) + vp->virtual_left) >> 2;
	y = (ScaleByZoom(y, vp->zoom) + vp->virtual_top) >> 1;

	a = y-x;
	b = y+x;

	/* we need to move variables in to the valid range, as the
	 * GetTileZoomCenterWindow() function can call here with invalid x and/or y,
	 * when the user tries to zoom out along the sides of the map */
	a = clamp(a, 0, (int)(MapMaxX() * TILE_SIZE) - 1);
	b = clamp(b, 0, (int)(MapMaxY() * TILE_SIZE) - 1);

	z = GetSlopeZ(a,     b    ) / 2;
	z = GetSlopeZ(a + z, b + z) / 2;
	z = GetSlopeZ(a + z, b + z) / 2;
	z = GetSlopeZ(a + z, b + z) / 2;
	z = GetSlopeZ(a + z, b + z) / 2;

	pt.x = a + z;
	pt.y = b + z;

	return pt;
}

/* When used for zooming, check area below current coordinates (x,y)
 * and return the tile of the zoomed out/in position (zoom_x, zoom_y)
 * when you just want the tile, make x = zoom_x and y = zoom_y */
static Point GetTileFromScreenXY(int x, int y, int zoom_x, int zoom_y)
{
	Window *w;
	ViewPort *vp;
	Point pt;

	if ( (w = FindWindowFromPt(x, y)) != NULL &&
			 (vp = IsPtInWindowViewport(w, x, y)) != NULL)
				return TranslateXYToTileCoord(vp, zoom_x, zoom_y);

	pt.y = pt.x = -1;
	return pt;
}

Point GetTileBelowCursor()
{
	return GetTileFromScreenXY(_cursor.pos.x, _cursor.pos.y, _cursor.pos.x, _cursor.pos.y);
}


Point GetTileZoomCenterWindow(bool in, Window * w)
{
	int x, y;
	ViewPort * vp;

	vp = w->viewport;

	if (in) {
		x = ((_cursor.pos.x - vp->left) >> 1) + (vp->width >> 2);
		y = ((_cursor.pos.y - vp->top) >> 1) + (vp->height >> 2);
	} else {
		x = vp->width - (_cursor.pos.x - vp->left);
		y = vp->height - (_cursor.pos.y - vp->top);
	}
	/* Get the tile below the cursor and center on the zoomed-out center */
	return GetTileFromScreenXY(_cursor.pos.x, _cursor.pos.y, x + vp->left, y + vp->top);
}

/** Update the status of the zoom-buttons according to the zoom-level
 * of the viewport. This will update their status and invalidate accordingly
 * @param w Window pointer to the window that has the zoom buttons
 * @param vp pointer to the viewport whose zoom-level the buttons represent
 * @param widget_zoom_in widget index for window with zoom-in button
 * @param widget_zoom_out widget index for window with zoom-out button */
void HandleZoomMessage(Window *w, const ViewPort *vp, byte widget_zoom_in, byte widget_zoom_out)
{
	SetWindowWidgetDisabledState(w, widget_zoom_in, vp->zoom == ZOOM_LVL_MIN);
	InvalidateWidget(w, widget_zoom_in);

	SetWindowWidgetDisabledState(w, widget_zoom_out, vp->zoom == ZOOM_LVL_MAX);
	InvalidateWidget(w, widget_zoom_out);
}

void DrawGroundSpriteAt(SpriteID image, SpriteID pal, int32 x, int32 y, byte z)
{
	ViewportDrawer *vd = _cur_vd;
	TileSpriteToDraw *ts;

	assert((image & SPRITE_MASK) < MAX_SPRITES);

	if (vd->spritelist_mem >= vd->eof_spritelist_mem) {
		DEBUG(sprite, 0, "Out of sprite memory");
		return;
	}
	ts = (TileSpriteToDraw*)vd->spritelist_mem;

	vd->spritelist_mem += sizeof(TileSpriteToDraw);

	ts->image = image;
	ts->pal = pal;
	ts->next = NULL;
	ts->x = x;
	ts->y = y;
	ts->z = z;
	*vd->last_tile = ts;
	vd->last_tile = &ts->next;
}

void DrawGroundSprite(SpriteID image, SpriteID pal)
{
	if (_offset_ground_sprites) {
		/* offset ground sprite because of foundation? */
		AddChildSpriteScreen(image, pal, _cur_vd->offs_x, _cur_vd->offs_y);
	} else {
		_added_tile_sprite = true;
		DrawGroundSpriteAt(image, pal, _cur_ti->x, _cur_ti->y, _cur_ti->z);
	}
}


void OffsetGroundSprite(int x, int y)
{
	_cur_vd->offs_x = x;
	_cur_vd->offs_y = y;
	_offset_ground_sprites = true;
}

static void AddCombinedSprite(SpriteID image, SpriteID pal, int x, int y, byte z)
{
	const ViewportDrawer *vd = _cur_vd;
	Point pt = RemapCoords(x, y, z);
	const Sprite* spr = GetSprite(image & SPRITE_MASK);

	if (pt.x + spr->x_offs >= vd->dpi.left + vd->dpi.width ||
			pt.x + spr->x_offs + spr->width <= vd->dpi.left ||
			pt.y + spr->y_offs >= vd->dpi.top + vd->dpi.height ||
			pt.y + spr->y_offs + spr->height <= vd->dpi.top)
		return;

	AddChildSpriteScreen(image, pal, pt.x - vd->parent_list[-1]->left, pt.y - vd->parent_list[-1]->top);
}


void AddSortableSpriteToDraw(SpriteID image, SpriteID pal, int x, int y, int w, int h, byte dz, byte z)
{
	ViewportDrawer *vd = _cur_vd;
	ParentSpriteToDraw *ps;
	const Sprite *spr;
	Point pt;

	assert((image & SPRITE_MASK) < MAX_SPRITES);

	if (vd->combine_sprites == 2) {
		AddCombinedSprite(image, pal, x, y, z);
		return;
	}

	vd->last_child = NULL;

	if (vd->spritelist_mem >= vd->eof_spritelist_mem) {
		DEBUG(sprite, 0, "Out of sprite memory");
		return;
	}
	ps = (ParentSpriteToDraw*)vd->spritelist_mem;

	if (vd->parent_list >= vd->eof_parent_list) {
		/* This can happen rarely, mostly when you zoom out completely
		 *  and have a lot of stuff that moves (and is added to the
		 *  sort-list, this function). To solve it, increase
		 *  parent_list somewhere below to a higher number.
		 * This can not really hurt you, it just gives some black
		 *  spots on the screen ;) */
		DEBUG(sprite, 0, "Out of sprite memory (parent_list)");
		return;
	}

	pt = RemapCoords(x, y, z);
	spr = GetSprite(image & SPRITE_MASK);
	if ((ps->left   = (pt.x += spr->x_offs)) >= vd->dpi.left + vd->dpi.width ||
			(ps->right  = (pt.x +  spr->width )) <= vd->dpi.left ||
			(ps->top    = (pt.y += spr->y_offs)) >= vd->dpi.top + vd->dpi.height ||
			(ps->bottom = (pt.y +  spr->height)) <= vd->dpi.top) {
		return;
	}

	vd->spritelist_mem += sizeof(ParentSpriteToDraw);

	ps->image = image;
	ps->pal = pal;
	ps->xmin = x;
	ps->xmax = x + w - 1;

	ps->ymin = y;
	ps->ymax = y + h - 1;

	ps->zmin = z;
	ps->zmax = z + dz - 1;

	ps->unk16 = 0;
	ps->child = NULL;
	vd->last_child = &ps->child;

	*vd->parent_list++ = ps;

	if (vd->combine_sprites == 1) vd->combine_sprites = 2;
}

void StartSpriteCombine()
{
	_cur_vd->combine_sprites = 1;
}

void EndSpriteCombine()
{
	_cur_vd->combine_sprites = 0;
}

void AddChildSpriteScreen(SpriteID image, SpriteID pal, int x, int y)
{
	ViewportDrawer *vd = _cur_vd;
	ChildScreenSpriteToDraw *cs;

	assert((image & SPRITE_MASK) < MAX_SPRITES);

	if (vd->spritelist_mem >= vd->eof_spritelist_mem) {
		DEBUG(sprite, 0, "Out of sprite memory");
		return;
	}
	cs = (ChildScreenSpriteToDraw*)vd->spritelist_mem;

	if (vd->last_child == NULL) return;

	vd->spritelist_mem += sizeof(ChildScreenSpriteToDraw);

	*vd->last_child = cs;
	vd->last_child = &cs->next;

	cs->image = image;
	cs->pal = pal;
	cs->x = x;
	cs->y = y;
	cs->next = NULL;
}

/* Returns a StringSpriteToDraw */
void *AddStringToDraw(int x, int y, StringID string, uint32 params_1, uint32 params_2)
{
	ViewportDrawer *vd = _cur_vd;
	StringSpriteToDraw *ss;

	if (vd->spritelist_mem >= vd->eof_spritelist_mem) {
		DEBUG(sprite, 0, "Out of sprite memory");
		return NULL;
	}
	ss = (StringSpriteToDraw*)vd->spritelist_mem;

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


static void DrawSelectionSprite(SpriteID image, SpriteID pal, const TileInfo *ti)
{
	if (_added_tile_sprite && !(_thd.drawstyle & HT_LINE)) { // draw on real ground
		DrawGroundSpriteAt(image, pal, ti->x, ti->y, ti->z + 7);
	} else { // draw on top of foundation
		AddSortableSpriteToDraw(image, pal, ti->x, ti->y, 0x10, 0x10, 1, ti->z + 7);
	}
}

static bool IsPartOfAutoLine(int px, int py)
{
	px -= _thd.selstart.x;
	py -= _thd.selstart.y;

	switch (_thd.drawstyle) {
	case HT_LINE | HT_DIR_X:  return py == 0; // x direction
	case HT_LINE | HT_DIR_Y:  return px == 0; // y direction
	case HT_LINE | HT_DIR_HU: return px == -py || px == -py - 16; // horizontal upper
	case HT_LINE | HT_DIR_HL: return px == -py || px == -py + 16; // horizontal lower
	case HT_LINE | HT_DIR_VL: return px == py || px == py + 16; // vertival left
	case HT_LINE | HT_DIR_VR: return px == py || px == py - 16; // vertical right
	default:
		NOT_REACHED();
	}

	/* useless, but avoids compiler warning this way */
	return 0;
}

// [direction][side]
static const int _AutorailType[6][2] = {
	{ HT_DIR_X,  HT_DIR_X },
	{ HT_DIR_Y,  HT_DIR_Y },
	{ HT_DIR_HU, HT_DIR_HL },
	{ HT_DIR_HL, HT_DIR_HU },
	{ HT_DIR_VL, HT_DIR_VR },
	{ HT_DIR_VR, HT_DIR_VL }
};

#include "table/autorail.h"

static void DrawTileSelection(const TileInfo *ti)
{
	SpriteID image;
	SpriteID pal;

	/* Draw a red error square? */
	if (_thd.redsq != 0 && _thd.redsq == ti->tile) {
		DrawSelectionSprite(SPR_SELECT_TILE + _tileh_to_sprite[ti->tileh], PALETTE_TILE_RED_PULSATING, ti);
		return;
	}

	/* no selection active? */
	if (_thd.drawstyle == 0) return;

	/* Inside the inner area? */
	if (IS_INSIDE_1D(ti->x, _thd.pos.x, _thd.size.x) &&
			IS_INSIDE_1D(ti->y, _thd.pos.y, _thd.size.y)) {
		if (_thd.drawstyle & HT_RECT) {
			image = SPR_SELECT_TILE + _tileh_to_sprite[ti->tileh];
			DrawSelectionSprite(image, _thd.make_square_red ? PALETTE_SEL_TILE_RED : PAL_NONE, ti);
		} else if (_thd.drawstyle & HT_POINT) {
			/* Figure out the Z coordinate for the single dot. */
			byte z = ti->z;
			if (ti->tileh & SLOPE_N) {
				z += TILE_HEIGHT;
				if (ti->tileh == SLOPE_STEEP_N) z += TILE_HEIGHT;
			}
			DrawGroundSpriteAt(_cur_dpi->zoom <= ZOOM_LVL_DETAIL ? SPR_DOT : SPR_DOT_SMALL, PAL_NONE, ti->x, ti->y, z);
		} else if (_thd.drawstyle & HT_RAIL /*&& _thd.place_mode == VHM_RAIL*/) {
			/* autorail highlight piece under cursor */
			uint type = _thd.drawstyle & 0xF;
			int offset;

			assert(type <= 5);

			offset = _AutorailTilehSprite[ti->tileh][_AutorailType[type][0]];
			if (offset >= 0) {
				image = SPR_AUTORAIL_BASE + offset;
				pal = PAL_NONE;
			} else {
				image = SPR_AUTORAIL_BASE - offset;
				pal = PALETTE_TO_RED;
			}

			DrawSelectionSprite(image, _thd.make_square_red ? PALETTE_SEL_TILE_RED : pal, ti);

		} else if (IsPartOfAutoLine(ti->x, ti->y)) {
			/* autorail highlighting long line */
			int dir = _thd.drawstyle & ~0xF0;
			int offset;
			uint side;

			if (dir < 2) {
				side = 0;
			} else {
				TileIndex start = TileVirtXY(_thd.selstart.x, _thd.selstart.y);
				side = delta(delta(TileX(start), TileX(ti->tile)), delta(TileY(start), TileY(ti->tile)));
			}

			offset = _AutorailTilehSprite[ti->tileh][_AutorailType[dir][side]];
			if (offset >= 0) {
				image = SPR_AUTORAIL_BASE + offset;
				pal = PAL_NONE;
			} else {
				image = SPR_AUTORAIL_BASE - offset;
				pal = PALETTE_TO_RED;
			}

			DrawSelectionSprite(image, _thd.make_square_red ? PALETTE_SEL_TILE_RED : pal, ti);
		}
		return;
	}

	/* Check if it's inside the outer area? */
	if (_thd.outersize.x &&
			_thd.size.x < _thd.size.x + _thd.outersize.x &&
			IS_INSIDE_1D(ti->x, _thd.pos.x + _thd.offs.x, _thd.size.x + _thd.outersize.x) &&
			IS_INSIDE_1D(ti->y, _thd.pos.y + _thd.offs.y, _thd.size.y + _thd.outersize.y)) {
		/* Draw a blue rect. */
		DrawSelectionSprite(SPR_SELECT_TILE + _tileh_to_sprite[ti->tileh], PALETTE_SEL_TILE_BLUE, ti);
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

	/* Transform into tile coordinates and round to closest full tile */
	x = ((vd->dpi.top >> 1) - (vd->dpi.left >> 2)) & ~0xF;
	y = ((vd->dpi.top >> 1) + (vd->dpi.left >> 2) - 0x10) & ~0xF;

	/* determine size of area */
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
			TileType tt;

			ti.x = x_cur;
			ti.y = y_cur;
			if (0 <= x_cur && x_cur < (int)MapMaxX() * TILE_SIZE &&
					0 <= y_cur && y_cur < (int)MapMaxY() * TILE_SIZE) {
				TileIndex tile = TileVirtXY(x_cur, y_cur);

				ti.tile = tile;
				ti.tileh = GetTileSlope(tile, &ti.z);
				tt = GetTileType(tile);
			} else {
				ti.tileh = SLOPE_FLAT;
				ti.tile = 0;
				ti.z = 0;
				tt = MP_VOID;
			}

			y_cur += 0x10;
			x_cur -= 0x10;

			_added_tile_sprite = false;
			_offset_ground_sprites = false;

			_tile_type_procs[tt]->draw_tile_proc(&ti);
			DrawTileSelection(&ti);
		} while (--width_cur);

		if ((direction ^= 1) != 0) {
			y += 0x10;
		} else {
			x += 0x10;
		}
	} while (--height);
}


static void ViewportAddTownNames(DrawPixelInfo *dpi)
{
	Town *t;
	int left, top, right, bottom;

	if (!HASBIT(_display_opt, DO_SHOW_TOWN_NAMES) || _game_mode == GM_MENU)
		return;

	left = dpi->left;
	top = dpi->top;
	right = left + dpi->width;
	bottom = top + dpi->height;

	switch (dpi->zoom) {
		case ZOOM_LVL_NORMAL:
			FOR_ALL_TOWNS(t) {
				if (bottom > t->sign.top &&
						top    < t->sign.top + 12 &&
						right  > t->sign.left &&
						left   < t->sign.left + t->sign.width_1) {
					AddStringToDraw(t->sign.left + 1, t->sign.top + 1,
						_patches.population_in_label ? STR_TOWN_LABEL_POP : STR_TOWN_LABEL,
						t->index, t->population);
				}
			}
			break;

		case ZOOM_LVL_OUT_2X:
			right += 2;
			bottom += 2;

			FOR_ALL_TOWNS(t) {
				if (bottom > t->sign.top &&
						top    < t->sign.top + 24 &&
						right  > t->sign.left &&
						left   < t->sign.left + t->sign.width_1*2) {
					AddStringToDraw(t->sign.left + 1, t->sign.top + 1,
						_patches.population_in_label ? STR_TOWN_LABEL_POP : STR_TOWN_LABEL,
						t->index, t->population);
				}
			}
			break;

		default: NOT_REACHED();
		case ZOOM_LVL_OUT_4X:
			right += 4;
			bottom += 5;

			FOR_ALL_TOWNS(t) {
				if (bottom > t->sign.top &&
						top    < t->sign.top + 24 &&
						right  > t->sign.left &&
						left   < t->sign.left + t->sign.width_2*4) {
					AddStringToDraw(t->sign.left + 5, t->sign.top + 1, STR_TOWN_LABEL_TINY_BLACK, t->index, 0);
					AddStringToDraw(t->sign.left + 1, t->sign.top - 3, STR_TOWN_LABEL_TINY_WHITE, t->index, 0);
				}
			}
			break;
	}
}


static void AddStation(const Station *st, StringID str, uint16 width)
{
	StringSpriteToDraw *sstd;

	sstd = (StringSpriteToDraw*)AddStringToDraw(st->sign.left + 1, st->sign.top + 1, str, st->index, st->facilities);
	if (sstd != NULL) {
		sstd->color = (st->owner == OWNER_NONE || st->facilities == 0) ? 0xE : _player_colors[st->owner];
		sstd->width = width;
	}
}


static void ViewportAddStationNames(DrawPixelInfo *dpi)
{
	int left, top, right, bottom;
	const Station *st;

	if (!HASBIT(_display_opt, DO_SHOW_STATION_NAMES) || _game_mode == GM_MENU)
		return;

	left = dpi->left;
	top = dpi->top;
	right = left + dpi->width;
	bottom = top + dpi->height;

	switch (dpi->zoom) {
		case ZOOM_LVL_NORMAL:
			FOR_ALL_STATIONS(st) {
				if (bottom > st->sign.top &&
						top    < st->sign.top + 12 &&
						right  > st->sign.left &&
						left   < st->sign.left + st->sign.width_1) {
					AddStation(st, STR_305C_0, st->sign.width_1);
				}
			}
			break;

		case ZOOM_LVL_OUT_2X:
			right += 2;
			bottom += 2;
			FOR_ALL_STATIONS(st) {
				if (bottom > st->sign.top &&
						top    < st->sign.top + 24 &&
						right  > st->sign.left &&
						left   < st->sign.left + st->sign.width_1*2) {
					AddStation(st, STR_305C_0, st->sign.width_1);
				}
			}
			break;

		default: NOT_REACHED();
		case ZOOM_LVL_OUT_4X:
			right += 4;
			bottom += 5;
			FOR_ALL_STATIONS(st) {
				if (bottom > st->sign.top &&
						top    < st->sign.top + 24 &&
						right  > st->sign.left &&
						left   < st->sign.left + st->sign.width_2*4) {
					AddStation(st, STR_STATION_SIGN_TINY, st->sign.width_2 | 0x8000);
				}
			}
			break;
	}
}


static void AddSign(const Sign *si, StringID str, uint16 width)
{
	StringSpriteToDraw *sstd;

	sstd = (StringSpriteToDraw*)AddStringToDraw(si->sign.left + 1, si->sign.top + 1, str, si->str, 0);
	if (sstd != NULL) {
		sstd->color = (si->owner == OWNER_NONE) ? 14 : _player_colors[si->owner];
		sstd->width = width;
	}
}


static void ViewportAddSigns(DrawPixelInfo *dpi)
{
	const Sign *si;
	int left, top, right, bottom;

	if (!HASBIT(_display_opt, DO_SHOW_SIGNS))
		return;

	left = dpi->left;
	top = dpi->top;
	right = left + dpi->width;
	bottom = top + dpi->height;

	switch (dpi->zoom) {
		case ZOOM_LVL_NORMAL:
			FOR_ALL_SIGNS(si) {
				if (bottom > si->sign.top &&
						top    < si->sign.top + 12 &&
						right  > si->sign.left &&
						left   < si->sign.left + si->sign.width_1) {
					AddSign(si, STR_2806, si->sign.width_1);
				}
			}
			break;

		case ZOOM_LVL_OUT_2X:
			right += 2;
			bottom += 2;
			FOR_ALL_SIGNS(si) {
				if (bottom > si->sign.top &&
						top    < si->sign.top + 24 &&
						right  > si->sign.left &&
						left   < si->sign.left + si->sign.width_1 * 2) {
					AddSign(si, STR_2806, si->sign.width_1);
				}
			}
			break;

		default: NOT_REACHED();
		case ZOOM_LVL_OUT_4X:
			right += 4;
			bottom += 5;
			FOR_ALL_SIGNS(si) {
				if (bottom > si->sign.top &&
						top    < si->sign.top + 24 &&
						right  > si->sign.left &&
						left   < si->sign.left + si->sign.width_2 * 4) {
					AddSign(si, STR_2002, si->sign.width_2 | 0x8000);
				}
			}
			break;
	}
}


static void AddWaypoint(const Waypoint *wp, StringID str, uint16 width)
{
	StringSpriteToDraw *sstd;

	sstd = (StringSpriteToDraw*)AddStringToDraw(wp->sign.left + 1, wp->sign.top + 1, str, wp->index, 0);
	if (sstd != NULL) {
		sstd->color = (wp->deleted ? 0xE : 11);
		sstd->width = width;
	}
}


static void ViewportAddWaypoints(DrawPixelInfo *dpi)
{
	const Waypoint *wp;
	int left, top, right, bottom;

	if (!HASBIT(_display_opt, DO_WAYPOINTS))
		return;

	left = dpi->left;
	top = dpi->top;
	right = left + dpi->width;
	bottom = top + dpi->height;

	switch (dpi->zoom) {
		case ZOOM_LVL_NORMAL:
			FOR_ALL_WAYPOINTS(wp) {
				if (bottom > wp->sign.top &&
						top    < wp->sign.top + 12 &&
						right  > wp->sign.left &&
						left   < wp->sign.left + wp->sign.width_1) {
					AddWaypoint(wp, STR_WAYPOINT_VIEWPORT, wp->sign.width_1);
				}
			}
			break;

		case ZOOM_LVL_OUT_2X:
			right += 2;
			bottom += 2;
			FOR_ALL_WAYPOINTS(wp) {
				if (bottom > wp->sign.top &&
						top    < wp->sign.top + 24 &&
						right  > wp->sign.left &&
						left   < wp->sign.left + wp->sign.width_1*2) {
					AddWaypoint(wp, STR_WAYPOINT_VIEWPORT, wp->sign.width_1);
				}
			}
			break;

		default: NOT_REACHED();
		case ZOOM_LVL_OUT_4X:
			right += 4;
			bottom += 5;
			FOR_ALL_WAYPOINTS(wp) {
				if (bottom > wp->sign.top &&
						top    < wp->sign.top + 24 &&
						right  > wp->sign.left &&
						left   < wp->sign.left + wp->sign.width_2*4) {
					AddWaypoint(wp, STR_WAYPOINT_VIEWPORT_TINY, wp->sign.width_2 | 0x8000);
				}
			}
			break;
	}
}

void UpdateViewportSignPos(ViewportSign *sign, int left, int top, StringID str)
{
	char buffer[128];
	uint w;

	sign->top = top;

	GetString(buffer, str, lastof(buffer));
	w = GetStringBoundingBox(buffer).width + 3;
	sign->width_1 = w;
	sign->left = left - w / 2;

	/* zoomed out version */
	_cur_fontsize = FS_SMALL;
	w = GetStringBoundingBox(buffer).width + 3;
	_cur_fontsize = FS_NORMAL;
	sign->width_2 = w;
}


static void ViewportDrawTileSprites(TileSpriteToDraw *ts)
{
	do {
		Point pt = RemapCoords(ts->x, ts->y, ts->z);
		DrawSprite(ts->image, ts->pal, pt.x, pt.y);
		ts = ts->next;
	} while (ts != NULL);
}

static void ViewportSortParentSprites(ParentSpriteToDraw *psd[])
{
	while (*psd != NULL) {
		ParentSpriteToDraw* ps = *psd;

		if (!(ps->unk16 & 1)) {
			ParentSpriteToDraw** psd2 = psd;

			ps->unk16 |= 1;

			while (*++psd2 != NULL) {
				ParentSpriteToDraw* ps2 = *psd2;
				ParentSpriteToDraw** psd3;

				if (ps2->unk16 & 1) continue;

				/* Decide which comparator to use, based on whether the bounding
				 * boxes overlap
				 */
				if (ps->xmax > ps2->xmin && ps->xmin < ps2->xmax && // overlap in X?
						ps->ymax > ps2->ymin && ps->ymin < ps2->ymax && // overlap in Y?
						ps->zmax > ps2->zmin && ps->zmin < ps2->zmax) { // overlap in Z?
					/* Use X+Y+Z as the sorting order, so sprites closer to the bottom of
					 * the screen and with higher Z elevation, are drawn in front.
					 * Here X,Y,Z are the coordinates of the "center of mass" of the sprite,
					 * i.e. X=(left+right)/2, etc.
					 * However, since we only care about order, don't actually divide / 2
					 */
					if (ps->xmin + ps->xmax + ps->ymin + ps->ymax + ps->zmin + ps->zmax <=
							ps2->xmin + ps2->xmax + ps2->ymin + ps2->ymax + ps2->zmin + ps2->zmax) {
						continue;
					}
				} else {
					if (ps->xmax < ps2->xmin ||
							ps->ymax < ps2->ymin ||
							ps->zmax < ps2->zmin || (
								ps->xmin < ps2->xmax &&
								ps->ymin < ps2->ymax &&
								ps->zmin < ps2->zmax
							)) {
						continue;
					}
				}

				/* Swap the two sprites ps and ps2 using bubble-sort algorithm. */
				psd3 = psd;
				do {
					ParentSpriteToDraw* temp = *psd3;
					*psd3 = ps2;
					ps2 = temp;

					psd3++;
				} while (psd3 <= psd2);
			}
		} else {
			psd++;
		}
	}
}

static void ViewportDrawParentSprites(ParentSpriteToDraw *psd[])
{
	for (; *psd != NULL; psd++) {
		const ParentSpriteToDraw* ps = *psd;
		Point pt = RemapCoords(ps->xmin, ps->ymin, ps->zmin);
		const ChildScreenSpriteToDraw* cs;

		DrawSprite(ps->image, ps->pal, pt.x, pt.y);

		for (cs = ps->child; cs != NULL; cs = cs->next) {
			DrawSprite(cs->image, cs->pal, ps->left + cs->x, ps->top + cs->y);
		}
	}
}

static void ViewportDrawStrings(DrawPixelInfo *dpi, const StringSpriteToDraw *ss)
{
	DrawPixelInfo dp;
	ZoomLevel zoom;

	_cur_dpi = &dp;
	dp = *dpi;

	zoom = dp.zoom;
	dp.zoom = ZOOM_LVL_NORMAL;

	dp.left >>= zoom;
	dp.top >>= zoom;
	dp.width >>= zoom;
	dp.height >>= zoom;

	do {
		uint16 colour;

		if (ss->width != 0) {
			int x = (ss->x >> zoom) - 1;
			int y = (ss->y >> zoom) - 1;
			int bottom = y + 11;
			int w = ss->width;

			if (w & 0x8000) {
				w &= ~0x8000;
				y--;
				bottom -= 6;
				w -= 3;
			}

		/* Draw the rectangle if 'tranparent station signs' is off,
		 * or if we are drawing a general text sign (STR_2806) */
			if (!HASBIT(_transparent_opt, TO_SIGNS) || ss->string == STR_2806) {
				DrawFrameRect(
					x, y, x + w, bottom, ss->color,
					HASBIT(_transparent_opt, TO_SIGNS) ? FR_TRANSPARENT : FR_NONE
				);
			}
		}

		SetDParam(0, ss->params[0]);
		SetDParam(1, ss->params[1]);
		/* if we didn't draw a rectangle, or if transparant building is on,
		 * draw the text in the color the rectangle would have */
		if (HASBIT(_transparent_opt, TO_SIGNS) && ss->string != STR_2806 && ss->width != 0) {
			/* Real colors need the IS_PALETTE_COLOR flag
			 * otherwise colors from _string_colormap are assumed. */
			colour = _colour_gradient[ss->color][6] | IS_PALETTE_COLOR;
		} else {
			colour = 16;
		}
		DrawString(
			ss->x >> zoom, (ss->y >> zoom) - (ss->width & 0x8000 ? 2 : 0),
			ss->string, colour
		);

		ss = ss->next;
	} while (ss != NULL);
}

void ViewportDoDraw(const ViewPort *vp, int left, int top, int right, int bottom)
{
	ViewportDrawer vd;
	int mask;
	int x;
	int y;
	DrawPixelInfo *old_dpi;

	byte mem[VIEWPORT_DRAW_MEM];
	ParentSpriteToDraw *parent_list[6144];

	_cur_vd = &vd;

	old_dpi = _cur_dpi;
	_cur_dpi = &vd.dpi;

	vd.dpi.zoom = vp->zoom;
	mask = ScaleByZoom(-1, vp->zoom);

	vd.combine_sprites = 0;

	vd.dpi.width = (right - left) & mask;
	vd.dpi.height = (bottom - top) & mask;
	vd.dpi.left = left & mask;
	vd.dpi.top = top & mask;
	vd.dpi.pitch = old_dpi->pitch;

	x = UnScaleByZoom(vd.dpi.left - (vp->virtual_left & mask), vp->zoom) + vp->left;
	y = UnScaleByZoom(vd.dpi.top - (vp->virtual_top & mask), vp->zoom) + vp->top;

	vd.dpi.dst_ptr = old_dpi->dst_ptr + x - old_dpi->left + (y - old_dpi->top) * old_dpi->pitch;

	vd.parent_list = parent_list;
	vd.eof_parent_list = endof(parent_list);
	vd.spritelist_mem = mem;
	vd.eof_spritelist_mem = endof(mem) - sizeof(LARGEST_SPRITELIST_STRUCT);
	vd.last_string = &vd.first_string;
	vd.first_string = NULL;
	vd.last_tile = &vd.first_tile;
	vd.first_tile = NULL;

	ViewportAddLandscape();
	ViewportAddVehicles(&vd.dpi);
	DrawTextEffects(&vd.dpi);

	ViewportAddTownNames(&vd.dpi);
	ViewportAddStationNames(&vd.dpi);
	ViewportAddSigns(&vd.dpi);
	ViewportAddWaypoints(&vd.dpi);

	/* This assert should never happen (because the length of the parent_list
	 *  is checked) */
	assert(vd.parent_list <= endof(parent_list));

	if (vd.first_tile != NULL) ViewportDrawTileSprites(vd.first_tile);

	/* null terminate parent sprite list */
	*vd.parent_list = NULL;

	ViewportSortParentSprites(parent_list);
	ViewportDrawParentSprites(parent_list);

	if (vd.first_string != NULL) ViewportDrawStrings(&vd.dpi, vd.first_string);

	_cur_dpi = old_dpi;
}

/** Make sure we don't draw a too big area at a time.
 * If we do, the sprite memory will overflow. */
static void ViewportDrawChk(const ViewPort *vp, int left, int top, int right, int bottom)
{
	if (ScaleByZoom(bottom - top, vp->zoom) * ScaleByZoom(right - left, vp->zoom) > 180000) {
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
			ScaleByZoom(left - vp->left, vp->zoom) + vp->virtual_left,
			ScaleByZoom(top - vp->top, vp->zoom) + vp->virtual_top,
			ScaleByZoom(right - vp->left, vp->zoom) + vp->virtual_left,
			ScaleByZoom(bottom - vp->top, vp->zoom) + vp->virtual_top
		);
	}
}

static inline void ViewportDraw(const ViewPort *vp, int left, int top, int right, int bottom)
{
	if (right <= vp->left || bottom <= vp->top) return;

	if (left >= vp->left + vp->width) return;

	if (left < vp->left) left = vp->left;
	if (right > vp->left + vp->width) right = vp->left + vp->width;

	if (top >= vp->top + vp->height) return;

	if (top < vp->top) top = vp->top;
	if (bottom > vp->top + vp->height) bottom = vp->top + vp->height;

	ViewportDrawChk(vp, left, top, right, bottom);
}

void DrawWindowViewport(const Window *w)
{
	DrawPixelInfo *dpi = _cur_dpi;

	dpi->left += w->left;
	dpi->top += w->top;

	ViewportDraw(w->viewport, dpi->left, dpi->top, dpi->left + dpi->width, dpi->top + dpi->height);

	dpi->left -= w->left;
	dpi->top -= w->top;
}

void UpdateViewportPosition(Window *w)
{
	const ViewPort *vp = w->viewport;

	if (WP(w, vp_d).follow_vehicle != INVALID_VEHICLE) {
		const Vehicle* veh = GetVehicle(WP(w,vp_d).follow_vehicle);
		Point pt = MapXYZToViewport(vp, veh->x_pos, veh->y_pos, veh->z_pos);

		SetViewportPosition(w, pt.x, pt.y);
	} else {
		int x;
		int y;
		int vx;
		int vy;

		/* Center of the viewport is hot spot */
		x = WP(w,vp_d).scrollpos_x + vp->virtual_width / 2;
		y = WP(w,vp_d).scrollpos_y + vp->virtual_height / 2;
		/* Convert viewport coordinates to map coordinates
		 * Calculation is scaled by 4 to avoid rounding errors */
		vx = -x + y * 2;
		vy =  x + y * 2;
		/* clamp to size of map */
		vx = clamp(vx, 0 * 4, MapMaxX() * TILE_SIZE * 4);
		vy = clamp(vy, 0 * 4, MapMaxY() * TILE_SIZE * 4);
		/* Convert map coordinates to viewport coordinates */
		x = (-vx + vy) / 2;
		y = ( vx + vy) / 4;
		/* Set position */
		WP(w, vp_d).scrollpos_x = x - vp->virtual_width / 2;
		WP(w, vp_d).scrollpos_y = y - vp->virtual_height / 2;

		SetViewportPosition(w, WP(w, vp_d).scrollpos_x, WP(w, vp_d).scrollpos_y);
	}
}

static void MarkViewportDirty(const ViewPort *vp, int left, int top, int right, int bottom)
{
	right -= vp->virtual_left;
	if (right <= 0) return;

	bottom -= vp->virtual_top;
	if (bottom <= 0) return;

	left = max(0, left - vp->virtual_left);

	if (left >= vp->virtual_width) return;

	top = max(0, top - vp->virtual_top);

	if (top >= vp->virtual_height) return;

	SetDirtyBlocks(
		UnScaleByZoom(left, vp->zoom) + vp->left,
		UnScaleByZoom(top, vp->zoom) + vp->top,
		UnScaleByZoom(right, vp->zoom) + vp->left,
		UnScaleByZoom(bottom, vp->zoom) + vp->top
	);
}

void MarkAllViewportsDirty(int left, int top, int right, int bottom)
{
	const ViewPort *vp = _viewports;
	uint32 act = _active_viewports;
	do {
		if (act & 1) {
			assert(vp->width != 0);
			MarkViewportDirty(vp, left, top, right, bottom);
		}
	} while (vp++,act>>=1);
}

void MarkTileDirtyByTile(TileIndex tile)
{
	Point pt = RemapCoords(TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE, GetTileZ(tile));
	MarkAllViewportsDirty(
		pt.x - 31,
		pt.y - 122,
		pt.x - 31 + 67,
		pt.y - 122 + 154
	);
}

void MarkTileDirty(int x, int y)
{
	uint z = 0;
	Point pt;

	if (IS_INT_INSIDE(x, 0, MapSizeX() * TILE_SIZE) &&
			IS_INT_INSIDE(y, 0, MapSizeY() * TILE_SIZE))
		z = GetTileZ(TileVirtXY(x, y));
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
	int x = _thd.pos.x;
	int y = _thd.pos.y;

	x_size = _thd.size.x;
	y_size = _thd.size.y;

	if (_thd.outersize.x) {
		x_size += _thd.outersize.x;
		x += _thd.offs.x;
		y_size += _thd.outersize.y;
		y += _thd.offs.y;
	}

	assert(x_size > 0);
	assert(y_size > 0);

	x_size += x;
	y_size += y;

	do {
		int y_bk = y;
		do {
			MarkTileDirty(x, y);
		} while ( (y += TILE_SIZE) != y_size);
		y = y_bk;
	} while ( (x += TILE_SIZE) != x_size);
}


void SetSelectionRed(bool b)
{
	_thd.make_square_red = b;
	SetSelectionTilesDirty();
}


static bool CheckClickOnTown(const ViewPort *vp, int x, int y)
{
	const Town *t;

	if (!HASBIT(_display_opt, DO_SHOW_TOWN_NAMES)) return false;

	switch (vp->zoom) {
		case ZOOM_LVL_NORMAL:
			x = x - vp->left + vp->virtual_left;
			y = y - vp->top  + vp->virtual_top;
			FOR_ALL_TOWNS(t) {
				if (y >= t->sign.top &&
						y < t->sign.top + 12 &&
						x >= t->sign.left &&
						x < t->sign.left + t->sign.width_1) {
					ShowTownViewWindow(t->index);
					return true;
				}
			}
			break;

		case ZOOM_LVL_OUT_2X:
			x = (x - vp->left + 1) * 2 + vp->virtual_left;
			y = (y - vp->top  + 1) * 2 + vp->virtual_top;
			FOR_ALL_TOWNS(t) {
				if (y >= t->sign.top &&
						y < t->sign.top + 24 &&
						x >= t->sign.left &&
						x < t->sign.left + t->sign.width_1 * 2) {
					ShowTownViewWindow(t->index);
					return true;
				}
			}
			break;

		default: NOT_REACHED();
		case ZOOM_LVL_OUT_4X:
			x = (x - vp->left + 3) * 4 + vp->virtual_left;
			y = (y - vp->top  + 3) * 4 + vp->virtual_top;
			FOR_ALL_TOWNS(t) {
				if (y >= t->sign.top &&
						y < t->sign.top + 24 &&
						x >= t->sign.left &&
						x < t->sign.left + t->sign.width_2 * 4) {
					ShowTownViewWindow(t->index);
					return true;
				}
			}
			break;
	}

	return false;
}


static bool CheckClickOnStation(const ViewPort *vp, int x, int y)
{
	const Station *st;

	if (!HASBIT(_display_opt, DO_SHOW_STATION_NAMES)) return false;

	switch (vp->zoom) {
		case ZOOM_LVL_NORMAL:
			x = x - vp->left + vp->virtual_left;
			y = y - vp->top  + vp->virtual_top;
			FOR_ALL_STATIONS(st) {
				if (y >= st->sign.top &&
						y < st->sign.top + 12 &&
						x >= st->sign.left &&
						x < st->sign.left + st->sign.width_1) {
					ShowStationViewWindow(st->index);
					return true;
				}
			}
			break;

		case ZOOM_LVL_OUT_2X:
			x = (x - vp->left + 1) * 2 + vp->virtual_left;
			y = (y - vp->top  + 1) * 2 + vp->virtual_top;
			FOR_ALL_STATIONS(st) {
				if (y >= st->sign.top &&
						y < st->sign.top + 24 &&
						x >= st->sign.left &&
						x < st->sign.left + st->sign.width_1 * 2) {
					ShowStationViewWindow(st->index);
					return true;
				}
			}
			break;

		default: NOT_REACHED();
		case ZOOM_LVL_OUT_4X:
			x = (x - vp->left + 3) * 4 + vp->virtual_left;
			y = (y - vp->top  + 3) * 4 + vp->virtual_top;
			FOR_ALL_STATIONS(st) {
				if (y >= st->sign.top &&
						y < st->sign.top + 24 &&
						x >= st->sign.left &&
						x < st->sign.left + st->sign.width_2 * 4) {
					ShowStationViewWindow(st->index);
					return true;
				}
			}
			break;
	}

	return false;
}


static bool CheckClickOnSign(const ViewPort *vp, int x, int y)
{
	const Sign *si;

	if (!HASBIT(_display_opt, DO_SHOW_SIGNS) || _current_player == PLAYER_SPECTATOR) return false;

	switch (vp->zoom) {
		case ZOOM_LVL_NORMAL:
			x = x - vp->left + vp->virtual_left;
			y = y - vp->top  + vp->virtual_top;
			FOR_ALL_SIGNS(si) {
				if (y >= si->sign.top &&
						y <  si->sign.top + 12 &&
						x >= si->sign.left &&
						x <  si->sign.left + si->sign.width_1) {
					ShowRenameSignWindow(si);
					return true;
				}
			}
			break;

		case ZOOM_LVL_OUT_2X:
			x = (x - vp->left + 1) * 2 + vp->virtual_left;
			y = (y - vp->top  + 1) * 2 + vp->virtual_top;
			FOR_ALL_SIGNS(si) {
				if (y >= si->sign.top &&
						y <  si->sign.top + 24 &&
						x >= si->sign.left &&
						x <  si->sign.left + si->sign.width_1 * 2) {
					ShowRenameSignWindow(si);
					return true;
				}
			}
			break;

		default: NOT_REACHED();
		case ZOOM_LVL_OUT_4X:
			x = (x - vp->left + 3) * 4 + vp->virtual_left;
			y = (y - vp->top  + 3) * 4 + vp->virtual_top;
			FOR_ALL_SIGNS(si) {
				if (y >= si->sign.top &&
						y <  si->sign.top + 24 &&
						x >= si->sign.left &&
						x <  si->sign.left + si->sign.width_2 * 4) {
					ShowRenameSignWindow(si);
					return true;
				}
			}
			break;
	}

	return false;
}


static bool CheckClickOnWaypoint(const ViewPort *vp, int x, int y)
{
	const Waypoint *wp;

	if (!HASBIT(_display_opt, DO_WAYPOINTS)) return false;

	switch (vp->zoom) {
		case ZOOM_LVL_NORMAL:
			x = x - vp->left + vp->virtual_left;
			y = y - vp->top  + vp->virtual_top;
			FOR_ALL_WAYPOINTS(wp) {
				if (y >= wp->sign.top &&
						y < wp->sign.top + 12 &&
						x >= wp->sign.left &&
						x < wp->sign.left + wp->sign.width_1) {
					ShowRenameWaypointWindow(wp);
					return true;
				}
			}
			break;

		case ZOOM_LVL_OUT_2X:
			x = (x - vp->left + 1) * 2 + vp->virtual_left;
			y = (y - vp->top  + 1) * 2 + vp->virtual_top;
			FOR_ALL_WAYPOINTS(wp) {
				if (y >= wp->sign.top &&
						y < wp->sign.top + 24 &&
						x >= wp->sign.left &&
						x < wp->sign.left + wp->sign.width_1 * 2) {
					ShowRenameWaypointWindow(wp);
					return true;
				}
			}
			break;

		default: NOT_REACHED();
		case ZOOM_LVL_OUT_4X:
			x = (x - vp->left + 3) * 4 + vp->virtual_left;
			y = (y - vp->top  + 3) * 4 + vp->virtual_top;
			FOR_ALL_WAYPOINTS(wp) {
				if (y >= wp->sign.top &&
						y < wp->sign.top + 24 &&
						x >= wp->sign.left &&
						x < wp->sign.left + wp->sign.width_2 * 4) {
					ShowRenameWaypointWindow(wp);
					return true;
				}
			}
			break;
	}

	return false;
}


static void CheckClickOnLandscape(const ViewPort *vp, int x, int y)
{
	Point pt = TranslateXYToTileCoord(vp, x, y);

	if (pt.x != -1) ClickTile(TileVirtXY(pt.x, pt.y));
}


static void SafeShowTrainViewWindow(const Vehicle* v)
{
	if (!IsFrontEngine(v)) v = GetFirstVehicleInChain(v);
	ShowTrainViewWindow(v);
}

static void Nop(const Vehicle *v) {}

typedef void OnVehicleClickProc(const Vehicle *v);
static OnVehicleClickProc* const _on_vehicle_click_proc[] = {
	SafeShowTrainViewWindow,
	ShowRoadVehViewWindow,
	ShowShipViewWindow,
	ShowAircraftViewWindow,
	Nop, // Special vehicles
	Nop  // Disaster vehicles
};

void HandleViewportClicked(const ViewPort *vp, int x, int y)
{
	const Vehicle *v;

	if (CheckClickOnTown(vp, x, y)) return;
	if (CheckClickOnStation(vp, x, y)) return;
	if (CheckClickOnSign(vp, x, y)) return;
	if (CheckClickOnWaypoint(vp, x, y)) return;
	CheckClickOnLandscape(vp, x, y);

	v = CheckClickOnVehicle(vp, x, y);
	if (v != NULL) {
		DEBUG(misc, 2, "Vehicle %d (index %d) at %p", v->unitnumber, v->index, v);
		_on_vehicle_click_proc[v->type](v);
	}
}

Vehicle *CheckMouseOverVehicle()
{
	const Window *w;
	const ViewPort *vp;

	int x = _cursor.pos.x;
	int y = _cursor.pos.y;

	w = FindWindowFromPt(x, y);
	if (w == NULL) return NULL;

	vp = IsPtInWindowViewport(w, x, y);
	return (vp != NULL) ? CheckClickOnVehicle(vp, x, y) : NULL;
}



void PlaceObject()
{
	Point pt;
	Window *w;

	pt = GetTileBelowCursor();
	if (pt.x == -1) return;

	if (_thd.place_mode == VHM_POINT) {
		pt.x += 8;
		pt.y += 8;
	}

	_tile_fract_coords.x = pt.x & 0xF;
	_tile_fract_coords.y = pt.y & 0xF;

	w = GetCallbackWnd();
	if (w != NULL) {
		WindowEvent e;

		e.event = WE_PLACE_OBJ;
		e.we.place.pt = pt;
		e.we.place.tile = TileVirtXY(pt.x, pt.y);
		w->wndproc(w, &e);
	}
}


/* scrolls the viewport in a window to a given location */
bool ScrollWindowTo(int x , int y, Window *w)
{
	Point pt;

	pt = MapXYZToViewport(w->viewport, x, y, GetSlopeZ(x, y));
	WP(w, vp_d).follow_vehicle = INVALID_VEHICLE;

	if (WP(w, vp_d).scrollpos_x == pt.x && WP(w, vp_d).scrollpos_y == pt.y)
		return false;

	WP(w, vp_d).scrollpos_x = pt.x;
	WP(w, vp_d).scrollpos_y = pt.y;
	return true;
}


bool ScrollMainWindowTo(int x, int y)
{
	Window *w;
	bool res = ScrollWindowTo(x, y, FindWindowById(WC_MAIN_WINDOW, 0));

	/* If a user scrolls to a tile (via what way what so ever) and already is on
	 *  that tile (e.g.: pressed twice), move the smallmap to that location,
	 *  so you directly see where you are on the smallmap. */

	if (res) return res;

	w = FindWindowById(WC_SMALLMAP, 0);
	if (w == NULL) return res;

	SmallMapCenterOnCurrentPos(w);

	return res;
}


bool ScrollMainWindowToTile(TileIndex tile)
{
	return ScrollMainWindowTo(TileX(tile) * TILE_SIZE + TILE_SIZE / 2, TileY(tile) * TILE_SIZE + TILE_SIZE / 2);
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
	_thd.new_size.x = w * TILE_SIZE;
	_thd.new_size.y = h * TILE_SIZE;
	_thd.new_outersize.x = 0;
	_thd.new_outersize.y = 0;
}

void SetTileSelectBigSize(int ox, int oy, int sx, int sy)
{
	_thd.offs.x = ox * TILE_SIZE;
	_thd.offs.y = oy * TILE_SIZE;
	_thd.new_outersize.x = sx * TILE_SIZE;
	_thd.new_outersize.y = sy * TILE_SIZE;
}

/** returns the best autorail highlight type from map coordinates */
static byte GetAutorailHT(int x, int y)
{
	return HT_RAIL | _AutorailPiece[x & 0xF][y & 0xF];
}

/** called regular to update tile highlighting in all cases */
void UpdateTileSelection()
{
	int x1;
	int y1;

	_thd.new_drawstyle = 0;

	if (_thd.place_mode == VHM_SPECIAL) {
		x1 = _thd.selend.x;
		y1 = _thd.selend.y;
		if (x1 != -1) {
			int x2 = _thd.selstart.x;
			int y2 = _thd.selstart.y;
			x1 &= ~0xF;
			y1 &= ~0xF;

			if (x1 >= x2) Swap(x1, x2);
			if (y1 >= y2) Swap(y1, y2);
			_thd.new_pos.x = x1;
			_thd.new_pos.y = y1;
			_thd.new_size.x = x2 - x1 + TILE_SIZE;
			_thd.new_size.y = y2 - y1 + TILE_SIZE;
			_thd.new_drawstyle = _thd.next_drawstyle;
		}
	} else if (_thd.place_mode != VHM_NONE) {
		Point pt = GetTileBelowCursor();
		x1 = pt.x;
		y1 = pt.y;
		if (x1 != -1) {
			switch (_thd.place_mode) {
				case VHM_RECT:
					_thd.new_drawstyle = HT_RECT;
					break;
				case VHM_POINT:
					_thd.new_drawstyle = HT_POINT;
					x1 += 8;
					y1 += 8;
					break;
				case VHM_RAIL:
					_thd.new_drawstyle = GetAutorailHT(pt.x, pt.y); // draw one highlighted tile
			}
			_thd.new_pos.x = x1 & ~0xF;
			_thd.new_pos.y = y1 & ~0xF;
		}
	}

	/* redraw selection */
	if (_thd.drawstyle != _thd.new_drawstyle ||
			_thd.pos.x != _thd.new_pos.x || _thd.pos.y != _thd.new_pos.y ||
			_thd.size.x != _thd.new_size.x || _thd.size.y != _thd.new_size.y ||
	    _thd.outersize.x != _thd.new_outersize.x ||
	    _thd.outersize.y != _thd.new_outersize.y) {
		/* clear the old selection? */
		if (_thd.drawstyle) SetSelectionTilesDirty();

		_thd.drawstyle = _thd.new_drawstyle;
		_thd.pos = _thd.new_pos;
		_thd.size = _thd.new_size;
		_thd.outersize = _thd.new_outersize;
		_thd.dirty = 0xff;

		/* draw the new selection? */
		if (_thd.new_drawstyle) SetSelectionTilesDirty();
	}
}

/** highlighting tiles while only going over them with the mouse */
void VpStartPlaceSizing(TileIndex tile, int user)
{
	_thd.userdata = user;
	_thd.selend.x = TileX(tile) * TILE_SIZE;
	_thd.selstart.x = TileX(tile) * TILE_SIZE;
	_thd.selend.y = TileY(tile) * TILE_SIZE;
	_thd.selstart.y = TileY(tile) * TILE_SIZE;
	if (_thd.place_mode == VHM_RECT) {
		_thd.place_mode = VHM_SPECIAL;
		_thd.next_drawstyle = HT_RECT;
	} else if (_thd.place_mode == VHM_RAIL) { // autorail one piece
		_thd.place_mode = VHM_SPECIAL;
		_thd.next_drawstyle = _thd.drawstyle;
	} else {
		_thd.place_mode = VHM_SPECIAL;
		_thd.next_drawstyle = HT_POINT;
	}
	_special_mouse_mode = WSM_SIZING;
}

void VpSetPlaceSizingLimit(int limit)
{
	_thd.sizelimit = limit;
}

/**
* Highlights all tiles between a set of two tiles. Used in dock and tunnel placement
* @param from TileIndex of the first tile to highlight
* @param to TileIndex of the last tile to highlight */
void VpSetPresizeRange(TileIndex from, TileIndex to)
{
	uint distance = DistanceManhattan(from, to) + 1;

	_thd.selend.x = TileX(to) * TILE_SIZE;
	_thd.selend.y = TileY(to) * TILE_SIZE;
	_thd.selstart.x = TileX(from) * TILE_SIZE;
	_thd.selstart.y = TileY(from) * TILE_SIZE;
	_thd.next_drawstyle = HT_RECT;

	/* show measurement only if there is any length to speak of */
	if (distance > 1) GuiShowTooltipsWithArgs(STR_MEASURE_LENGTH, 1, &distance);
}

static void VpStartPreSizing()
{
	_thd.selend.x = -1;
	_special_mouse_mode = WSM_PRESIZE;
}

/** returns information about the 2x1 piece to be build.
 * The lower bits (0-3) are the track type. */
static byte Check2x1AutoRail(int mode)
{
	int fxpy = _tile_fract_coords.x + _tile_fract_coords.y;
	int sxpy = (_thd.selend.x & 0xF) + (_thd.selend.y & 0xF);
	int fxmy = _tile_fract_coords.x - _tile_fract_coords.y;
	int sxmy = (_thd.selend.x & 0xF) - (_thd.selend.y & 0xF);

	switch (mode) {
	case 0: // end piece is lower right
		if (fxpy >= 20 && sxpy <= 12) { /*SwapSelection(); DoRailroadTrack(0); */return 3; }
		if (fxmy < -3 && sxmy > 3) {/* DoRailroadTrack(0); */return 5; }
		return 1;

	case 1:
		if (fxmy > 3 && sxmy < -3) { /*SwapSelection(); DoRailroadTrack(0); */return 4; }
		if (fxpy <= 12 && sxpy >= 20) { /*DoRailroadTrack(0); */return 2; }
		return 1;

	case 2:
		if (fxmy > 3 && sxmy < -3) { /*DoRailroadTrack(3);*/ return 4; }
		if (fxpy >= 20 && sxpy <= 12) { /*SwapSelection(); DoRailroadTrack(0); */return 3; }
		return 0;

	case 3:
		if (fxmy < -3 && sxmy > 3) { /*SwapSelection(); DoRailroadTrack(3);*/ return 5; }
		if (fxpy <= 12 && sxpy >= 20) { /*DoRailroadTrack(0); */return 2; }
		return 0;
	}

	return 0; // avoids compiler warnings
}

/** Check if the direction of start and end tile should be swapped based on
 * the dragging-style. Default directions are:
 * in the case of a line (HT_RAIL, HT_LINE):  DIR_NE, DIR_NW, DIR_N, DIR_E
 * in the case of a rect (HT_RECT, HT_POINT): DIR_S, DIR_E
 * For example dragging a rectangle area from south to north should be swapped to
 * north-south (DIR_S) to obtain the same results with less code. This is what
 * the return value signifies.
 * @param style HighLightStyle dragging style
 * @param start_tile start tile of drag
 * @param end_tile end tile of drag
 * @return boolean value which when true means start/end should be swapped */
static bool SwapDirection(HighLightStyle style, TileIndex start_tile, TileIndex end_tile)
{
	uint start_x = TileX(start_tile);
	uint start_y = TileY(start_tile);
	uint end_x = TileX(end_tile);
	uint end_y = TileY(end_tile);

	switch (style & HT_DRAG_MASK) {
		case HT_RAIL:
		case HT_LINE: return (end_x > start_x || (end_x == start_x && end_y > start_y));

		case HT_RECT:
		case HT_POINT: return (end_x != start_x && end_y < start_y);
		default: NOT_REACHED();
	}

	return false;
}

/** Calculates height difference between one tile and another
* Multiplies the result to suit the standard given by minimap - 50 meters high
* To correctly get the height difference we need the direction we are dragging
* in, as well as with what kind of tool we are dragging. For example a horizontal
* autorail tool that starts in bottom and ends at the top of a tile will need the
* maximum of SW, S and SE, N corners respectively. This is handled by the lookup table below
* See _tileoffs_by_dir in map.c for the direction enums if you can't figure out
* the values yourself.
* @param style HightlightStyle of drag. This includes direction and style (autorail, rect, etc.)
* @param distance amount of tiles dragged, important for horizontal/vertical drags
*        ignored for others
* @param start_tile, end_tile start and end tile of drag operation
* @return height difference between two tiles. Tile measurement tool utilizes
* this value in its tooltips */
static int CalcHeightdiff(HighLightStyle style, uint distance, TileIndex start_tile, TileIndex end_tile)
{
	bool swap = SwapDirection(style, start_tile, end_tile);
	byte style_t;
	uint h0, h1, ht; // start heigth, end height, and temp variable

	if (start_tile == end_tile) return 0;
	if (swap) Swap(start_tile, end_tile);

	switch (style & HT_DRAG_MASK) {
		case HT_RECT: {
			static const TileIndexDiffC heightdiff_area_by_dir[] = {
				/* Start */ {1, 0}, /* Dragging east */ {0, 0}, /* Dragging south */
				/* End   */ {0, 1}, /* Dragging east */ {1, 1}  /* Dragging south */
			};

			/* In the case of an area we can determine whether we were dragging south or
			 * east by checking the X-coordinates of the tiles */
			style_t = (byte)(TileX(end_tile) > TileX(start_tile));
			start_tile = TILE_ADD(start_tile, ToTileIndexDiff(heightdiff_area_by_dir[style_t]));
			end_tile   = TILE_ADD(end_tile, ToTileIndexDiff(heightdiff_area_by_dir[2 + style_t]));
		}
		/* Fallthrough */
		case HT_POINT:
			h0 = TileHeight(start_tile);
			h1 = TileHeight(end_tile);
			break;
		default: { /* All other types, this is mostly only line/autorail */
			static const HighLightStyle flip_style_direction[] = {
				HT_DIR_X, HT_DIR_Y, HT_DIR_HL, HT_DIR_HU, HT_DIR_VR, HT_DIR_VL
			};
			static const TileIndexDiffC heightdiff_line_by_dir[] = {
				/* Start */ {1, 0}, {1, 1}, /* HT_DIR_X  */ {0, 1}, {1, 1}, /* HT_DIR_Y  */
				/* Start */ {1, 0}, {0, 0}, /* HT_DIR_HU */ {1, 0}, {1, 1}, /* HT_DIR_HL */
				/* Start */ {1, 0}, {1, 1}, /* HT_DIR_VL */ {0, 1}, {1, 1}, /* HT_DIR_VR */

				/* Start */ {0, 1}, {0, 0}, /* HT_DIR_X  */ {1, 0}, {0, 0}, /* HT_DIR_Y  */
				/* End   */ {0, 1}, {0, 0}, /* HT_DIR_HU */ {1, 1}, {0, 1}, /* HT_DIR_HL */
				/* End   */ {1, 0}, {0, 0}, /* HT_DIR_VL */ {0, 0}, {0, 1}, /* HT_DIR_VR */
			};

			distance %= 2; // we're only interested if the distance is even or uneven
			style &= HT_DIR_MASK;

			/* To handle autorail, we do some magic to be able to use a lookup table.
			 * Firstly if we drag the other way around, we switch start&end, and if needed
			 * also flip the drag-position. Eg if it was on the left, and the distance is even
			 * that means the end, which is now the start is on the right */
			if (swap && distance == 0) style = flip_style_direction[style];

			/* Use lookup table for start-tile based on HighLightStyle direction */
			style_t = style * 2;
			assert(style_t < lengthof(heightdiff_line_by_dir) - 13);
			h0 = TileHeight(TILE_ADD(start_tile, ToTileIndexDiff(heightdiff_line_by_dir[style_t])));
			ht = TileHeight(TILE_ADD(start_tile, ToTileIndexDiff(heightdiff_line_by_dir[style_t + 1])));
			h0 = max(h0, ht);

			/* Use lookup table for end-tile based on HighLightStyle direction
			 * flip around side (lower/upper, left/right) based on distance */
			if (distance == 0) style_t = flip_style_direction[style] * 2;
			assert(style_t < lengthof(heightdiff_line_by_dir) - 13);
			h1 = TileHeight(TILE_ADD(end_tile, ToTileIndexDiff(heightdiff_line_by_dir[12 + style_t])));
			ht = TileHeight(TILE_ADD(end_tile, ToTileIndexDiff(heightdiff_line_by_dir[12 + style_t + 1])));
			h1 = max(h1, ht);
		} break;
	}

	if (swap) Swap(h0, h1);
	/* Minimap shows height in intervals of 50 meters, let's do the same */
	return (int)(h1 - h0) * 50;
}

static const StringID measure_strings_length[] = {STR_NULL, STR_MEASURE_LENGTH, STR_MEASURE_LENGTH_HEIGHTDIFF};

/** while dragging */
static void CalcRaildirsDrawstyle(TileHighlightData *thd, int x, int y, int method)
{
	HighLightStyle b;
	uint w, h;

	int dx = thd->selstart.x - (thd->selend.x & ~0xF);
	int dy = thd->selstart.y - (thd->selend.y & ~0xF);
	w = myabs(dx) + 16;
	h = myabs(dy) + 16;

	if (TileVirtXY(thd->selstart.x, thd->selstart.y) == TileVirtXY(x, y)) { // check if we're only within one tile
		if (method == VPM_RAILDIRS) {
			b = GetAutorailHT(x, y);
		} else { // rect for autosignals on one tile
			b = HT_RECT;
		}
	} else if (h == 16) { // Is this in X direction?
		if (dx == 16) { // 2x1 special handling
			b = (Check2x1AutoRail(3)) | HT_LINE;
		} else if (dx == -16) {
			b = (Check2x1AutoRail(2)) | HT_LINE;
		} else {
			b = HT_LINE | HT_DIR_X;
		}
		y = thd->selstart.y;
	} else if (w == 16) { // Or Y direction?
		if (dy == 16) { // 2x1 special handling
			b = (Check2x1AutoRail(1)) | HT_LINE;
		} else if (dy == -16) { // 2x1 other direction
			b = (Check2x1AutoRail(0)) | HT_LINE;
		} else {
			b = HT_LINE | HT_DIR_Y;
		}
		x = thd->selstart.x;
	} else if (w > h * 2) { // still count as x dir?
		b = HT_LINE | HT_DIR_X;
		y = thd->selstart.y;
	} else if (h > w * 2) { // still count as y dir?
		b = HT_LINE | HT_DIR_Y;
		x = thd->selstart.x;
	} else { // complicated direction
		int d = w - h;
		thd->selend.x = thd->selend.x & ~0xF;
		thd->selend.y = thd->selend.y & ~0xF;

		// four cases.
		if (x > thd->selstart.x) {
			if (y > thd->selstart.y) {
				// south
				if (d == 0) {
					b = (x & 0xF) > (y & 0xF) ? HT_LINE | HT_DIR_VL : HT_LINE | HT_DIR_VR;
				} else if (d >= 0) {
					x = thd->selstart.x + h;
					b = HT_LINE | HT_DIR_VL;
					// return px == py || px == py + 16;
				} else {
					y = thd->selstart.y + w;
					b = HT_LINE | HT_DIR_VR;
				} // return px == py || px == py - 16;
			} else {
				// west
				if (d == 0) {
					b = (x & 0xF) + (y & 0xF) >= 0x10 ? HT_LINE | HT_DIR_HL : HT_LINE | HT_DIR_HU;
				} else if (d >= 0) {
					x = thd->selstart.x + h;
					b = HT_LINE | HT_DIR_HL;
				} else {
					y = thd->selstart.y - w;
					b = HT_LINE | HT_DIR_HU;
				}
			}
		} else {
			if (y > thd->selstart.y) {
				// east
				if (d == 0) {
					b = (x & 0xF) + (y & 0xF) >= 0x10 ? HT_LINE | HT_DIR_HL : HT_LINE | HT_DIR_HU;
				} else if (d >= 0) {
					x = thd->selstart.x - h;
					b = HT_LINE | HT_DIR_HU;
					// return px == -py || px == -py - 16;
				} else {
					y = thd->selstart.y + w;
					b = HT_LINE | HT_DIR_HL;
				} // return px == -py || px == -py + 16;
			} else {
				// north
				if (d == 0) {
					b = (x & 0xF) > (y & 0xF) ? HT_LINE | HT_DIR_VL : HT_LINE | HT_DIR_VR;
				} else if (d >= 0) {
					x = thd->selstart.x - h;
					b = HT_LINE | HT_DIR_VR;
					// return px == py || px == py - 16;
				} else {
					y = thd->selstart.y - w;
					b = HT_LINE | HT_DIR_VL;
				} //return px == py || px == py + 16;
			}
		}
	}

	if (_patches.measure_tooltip) {
		TileIndex t0 = TileVirtXY(thd->selstart.x, thd->selstart.y);
		TileIndex t1 = TileVirtXY(x, y);
		uint distance = DistanceManhattan(t0, t1) + 1;
		byte index = 0;
		uint params[2];

		if (distance != 1) {
			int heightdiff = CalcHeightdiff(b, distance, t0, t1);
			/* If we are showing a tooltip for horizontal or vertical drags,
			 * 2 tiles have a length of 1. To bias towards the ceiling we add
			 * one before division. It feels more natural to count 3 lengths as 2 */
			if ((b & HT_DIR_MASK) != HT_DIR_X && (b & HT_DIR_MASK) != HT_DIR_Y) {
				distance = (distance + 1) / 2;
			}

			params[index++] = distance;
			if (heightdiff != 0) params[index++] = heightdiff;
		}

		GuiShowTooltipsWithArgs(measure_strings_length[index], index, params);
	}

	thd->selend.x = x;
	thd->selend.y = y;
	thd->next_drawstyle = b;
}

/**
 * Selects tiles while dragging
 * @param x X coordinate of end of selection
 * @param y Y coordinate of end of selection
 * @param method modifies the way tiles are selected. Possible
 * methods are VPM_* in viewport.h */
void VpSelectTilesWithMethod(int x, int y, int method)
{
	int sx, sy;
	HighLightStyle style;

	if (x == -1) {
		_thd.selend.x = -1;
		return;
	}

	/* Special handling of drag in any (8-way) direction */
	if (method == VPM_RAILDIRS || method == VPM_SIGNALDIRS) {
		_thd.selend.x = x;
		_thd.selend.y = y;
		CalcRaildirsDrawstyle(&_thd, x, y, method);
		return;
	}

	if (_thd.next_drawstyle == HT_POINT) {
		x += TILE_SIZE / 2;
		y += TILE_SIZE / 2;
	}

	sx = _thd.selstart.x;
	sy = _thd.selstart.y;

	switch (method) {
		case VPM_X_OR_Y: /* drag in X or Y direction */
			if (myabs(sy - y) < myabs(sx - x)) {
				y = sy;
				style = HT_DIR_X;
			} else {
				x = sx;
				style = HT_DIR_Y;
			}
			goto calc_heightdiff_single_direction;
		case VPM_FIX_X: /* drag in Y direction */
			x = sx;
			style = HT_DIR_Y;
			goto calc_heightdiff_single_direction;
		case VPM_FIX_Y: /* drag in X direction */
			y = sy;
			style = HT_DIR_X;

calc_heightdiff_single_direction:;
			if (_patches.measure_tooltip) {
				TileIndex t0 = TileVirtXY(sx, sy);
				TileIndex t1 = TileVirtXY(x, y);
				uint distance = DistanceManhattan(t0, t1) + 1;
				byte index = 0;
				uint params[2];

				if (distance != 1) {
					/* With current code passing a HT_LINE style to calculate the height
					 * difference is enough. However if/when a point-tool is created
					 * with this method, function should be called with new_style (below)
					 * instead of HT_LINE | style case HT_POINT is handled specially
					 * new_style := (_thd.next_drawstyle & HT_RECT) ? HT_LINE | style : _thd.next_drawstyle; */
					int heightdiff = CalcHeightdiff(HT_LINE | style, 0, t0, t1);

					params[index++] = distance;
					if (heightdiff != 0) params[index++] = heightdiff;
				}

				GuiShowTooltipsWithArgs(measure_strings_length[index], index, params);
			} break;

		case VPM_X_AND_Y_LIMITED: { /* drag an X by Y constrained rect area */
			int limit = (_thd.sizelimit - 1) * TILE_SIZE;
			x = sx + clamp(x - sx, -limit, limit);
			y = sy + clamp(y - sy, -limit, limit);
			} /* Fallthrough */
		case VPM_X_AND_Y: { /* drag an X by Y area */
			if (_patches.measure_tooltip) {
				static const StringID measure_strings_area[] = {
					STR_NULL, STR_NULL, STR_MEASURE_AREA, STR_MEASURE_AREA_HEIGHTDIFF
				};

				TileIndex t0 = TileVirtXY(sx, sy);
				TileIndex t1 = TileVirtXY(x, y);
				uint dx = delta(TileX(t0), TileX(t1)) + 1;
				uint dy = delta(TileY(t0), TileY(t1)) + 1;
				byte index = 0;
				uint params[3];

				/* If dragging an area (eg dynamite tool) and it is actually a single
				 * row/column, change the type to 'line' to get proper calculation for height */
				style = _thd.next_drawstyle;
				if (style & HT_RECT) {
					if (dx == 1) {
						style = HT_LINE | HT_DIR_Y;
					} else if (dy == 1) {
						style = HT_LINE | HT_DIR_X;
					}
				}

				if (dx != 1 || dy != 1) {
					int heightdiff = CalcHeightdiff(style, 0, t0, t1);

					params[index++] = dx;
					params[index++] = dy;
					if (heightdiff != 0) params[index++] = heightdiff;
				}

				GuiShowTooltipsWithArgs(measure_strings_area[index], index, params);
			}
		break;

		}
		default: NOT_REACHED();
	}

	_thd.selend.x = x;
	_thd.selend.y = y;
}

/** while dragging */
bool VpHandlePlaceSizingDrag()
{
	Window *w;
	WindowEvent e;

	if (_special_mouse_mode != WSM_SIZING) return true;

	e.we.place.userdata = _thd.userdata;

	/* stop drag mode if the window has been closed */
	w = FindWindowById(_thd.window_class, _thd.window_number);
	if (w == NULL) {
		ResetObjectToPlace();
		return false;
	}

	/* while dragging execute the drag procedure of the corresponding window (mostly VpSelectTilesWithMethod() ) */
	if (_left_button_down) {
		e.event = WE_PLACE_DRAG;
		e.we.place.pt = GetTileBelowCursor();
		w->wndproc(w, &e);
		return false;
	}

	/* mouse button released..
	 * keep the selected tool, but reset it to the original mode. */
	_special_mouse_mode = WSM_NONE;
	if (_thd.next_drawstyle == HT_RECT) {
		_thd.place_mode = VHM_RECT;
	} else if ((e.we.place.userdata & 0xF) == VPM_SIGNALDIRS) { // some might call this a hack... -- Dominik
		_thd.place_mode = VHM_RECT;
	} else if (_thd.next_drawstyle & HT_LINE) {
		_thd.place_mode = VHM_RAIL;
	} else if (_thd.next_drawstyle & HT_RAIL) {
		_thd.place_mode = VHM_RAIL;
	} else {
		_thd.place_mode = VHM_POINT;
	}
	SetTileSelectSize(1, 1);

	/* and call the mouseup event. */
	e.event = WE_PLACE_MOUSEUP;
	e.we.place.pt = _thd.selend;
	e.we.place.tile = TileVirtXY(e.we.place.pt.x, e.we.place.pt.y);
	e.we.place.starttile = TileVirtXY(_thd.selstart.x, _thd.selstart.y);
	w->wndproc(w, &e);

	return false;
}

void SetObjectToPlaceWnd(CursorID icon, SpriteID pal, byte mode, Window *w)
{
	SetObjectToPlace(icon, pal, mode, w->window_class, w->window_number);
}

#include "table/animcursors.h"

void SetObjectToPlace(CursorID icon, SpriteID pal, byte mode, WindowClass window_class, WindowNumber window_num)
{
	Window *w;

	/* undo clicking on button */
	if (_thd.place_mode != 0) {
		_thd.place_mode = 0;
		w = FindWindowById(_thd.window_class, _thd.window_number);
		if (w != NULL) CallWindowEventNP(w, WE_ABORT_PLACE_OBJ);
	}

	SetTileSelectSize(1, 1);

	_thd.make_square_red = false;

	if (mode == VHM_DRAG) { // mode 4 is for dragdropping trains in the depot window
		mode = 0;
		_special_mouse_mode = WSM_DRAGDROP;
	} else {
		_special_mouse_mode = WSM_NONE;
	}

	_thd.place_mode = mode;
	_thd.window_class = window_class;
	_thd.window_number = window_num;

	if (mode == VHM_SPECIAL) // special tools, like tunnels or docks start with presizing mode
		VpStartPreSizing();

	if ( (int)icon < 0)
		SetAnimatedMouseCursor(_animcursors[~icon]);
	else
		SetMouseCursor(icon, pal);
}

void ResetObjectToPlace()
{
	SetObjectToPlace(SPR_CURSOR_MOUSE, PAL_NONE, VHM_NONE, WC_MAIN_WINDOW, 0);
}
