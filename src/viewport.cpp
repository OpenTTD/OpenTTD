/* $Id$ */

/** @file viewport.cpp Handling of all viewports.
 *
 * \verbatim
 * The in-game coordinate system looks like this *
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
 * \endverbatim
 */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "tile_cmd.h"
#include "gui.h"
#include "spritecache.h"
#include "landscape.h"
#include "viewport_func.h"
#include "station_base.h"
#include "town.h"
#include "signs_base.h"
#include "signs_func.h"
#include "waypoint.h"
#include "variables.h"
#include "train.h"
#include "roadveh.h"
#include "vehicle_gui.h"
#include "blitter/factory.hpp"
#include "transparency.h"
#include "strings_func.h"
#include "zoom_func.h"
#include "vehicle_func.h"
#include "player_func.h"
#include "settings_type.h"
#include "station_func.h"
#include "core/alloc_type.hpp"
#include "misc/smallvec.h"
#include "window_func.h"
#include "tilehighlight_func.h"

#include "table/sprites.h"
#include "table/strings.h"

PlaceProc *_place_proc;
Point _tile_fract_coords;
ZoomLevel _saved_scrollpos_zoom;

struct StringSpriteToDraw {
	uint16 string;
	uint16 color;
	int32 x;
	int32 y;
	uint64 params[2];
	uint16 width;
};

struct TileSpriteToDraw {
	SpriteID image;
	SpriteID pal;
	const SubSprite *sub;           ///< only draw a rectangular part of the sprite
	int32 x;
	int32 y;
	byte z;
};

struct ChildScreenSpriteToDraw {
	SpriteID image;
	SpriteID pal;
	const SubSprite *sub;           ///< only draw a rectangular part of the sprite
	int32 x;
	int32 y;
	ChildScreenSpriteToDraw *next;
};

struct ParentSpriteToDraw {
	SpriteID image;                 ///< sprite to draw
	SpriteID pal;                   ///< palette to use
	const SubSprite *sub;           ///< only draw a rectangular part of the sprite

	int32 x;                        ///< screen X coordinate of sprite
	int32 y;                        ///< screen Y coordinate of sprite

	int32 left;                     ///< minimal screen X coordinate of sprite (= x + sprite->x_offs), reference point for child sprites
	int32 top;                      ///< minimal screen Y coordinate of sprite (= y + sprite->y_offs), reference point for child sprites

	int32 xmin;                     ///< minimal world X coordinate of bounding box
	int32 xmax;                     ///< maximal world X coordinate of bounding box
	int32 ymin;                     ///< minimal world Y coordinate of bounding box
	int32 ymax;                     ///< maximal world Y coordinate of bounding box
	int zmin;                       ///< minimal world Z coordinate of bounding box
	int zmax;                       ///< maximal world Z coordinate of bounding box

	int first_child;                ///< the first child to draw.
	int last_child;                 ///< the last sprite to draw.
	bool comparison_done;           ///< Used during sprite sorting: true if sprite has been compared with all other sprites
};

/* Enumeration of multi-part foundations */
enum FoundationPart {
	FOUNDATION_PART_NONE     = 0xFF,  ///< Neither foundation nor groundsprite drawn yet.
	FOUNDATION_PART_NORMAL   = 0,     ///< First part (normal foundation or no foundation)
	FOUNDATION_PART_HALFTILE = 1,     ///< Second part (halftile foundation)
	FOUNDATION_PART_END
};

typedef SmallVector<TileSpriteToDraw, 64> TileSpriteToDrawVector;
typedef SmallVector<StringSpriteToDraw, 4> StringSpriteToDrawVector;
typedef SmallVector<ParentSpriteToDraw, 64> ParentSpriteToDrawVector;
typedef SmallVector<ParentSpriteToDraw*, 64> ParentSpriteToSortVector;
typedef SmallVector<ChildScreenSpriteToDraw, 16> ChildScreenSpriteToDrawVector;

struct ViewportDrawer {
	DrawPixelInfo dpi;

	StringSpriteToDrawVector string_sprites_to_draw;
	TileSpriteToDrawVector tile_sprites_to_draw;
	ParentSpriteToDrawVector parent_sprites_to_draw;
	ParentSpriteToSortVector parent_sprites_to_sort;
	ChildScreenSpriteToDrawVector child_screen_sprites_to_draw;

	int *last_child;

	byte combine_sprites;

	int foundation[FOUNDATION_PART_END];             ///< Foundation sprites (index into parent_sprites_to_draw).
	FoundationPart foundation_part;                  ///< Currently active foundation for ground sprite drawing.
	int *last_foundation_child[FOUNDATION_PART_END]; ///< Tail of ChildSprite list of the foundations. (index into child_screen_sprites_to_draw)
	Point foundation_offset[FOUNDATION_PART_END];    ///< Pixeloffset for ground sprites on the foundations.
};

static ViewportDrawer _vd;

TileHighlightData _thd;
static TileInfo *_cur_ti;
bool _draw_bounding_boxes = false;

static Point MapXYZToViewport(const ViewPort *vp, uint x, uint y, uint z)
{
	Point p = RemapCoords(x, y, z);
	p.x -= vp->virtual_width / 2;
	p.y -= vp->virtual_height / 2;
	return p;
}

void DeleteWindowViewport(Window *w)
{
	free(w->viewport);
	w->viewport = NULL;
}

/**
 * Initialize viewport of the window for use.
 * @param w Window to use/display the viewport in
 * @param x Offset of left edge of viewport with respect to left edge window \a w
 * @param y Offset of top edge of viewport with respect to top edge window \a w
 * @param width Width of the viewport
 * @param height Height of the viewport
 * @param follow_flags Flags controlling the viewport.
 *        - If bit 31 is set, the lower 16 bits are the vehicle that the viewport should follow.
 *        - If bit 31 is clear, it is a tile position.
 * @param zoom Zoomlevel to display
 */
void InitializeWindowViewport(Window *w, int x, int y,
	int width, int height, uint32 follow_flags, ZoomLevel zoom)
{
	assert(w->viewport == NULL);

	ViewportData *vp = CallocT<ViewportData>(1);

	vp->left = x + w->left;
	vp->top = y + w->top;
	vp->width = width;
	vp->height = height;

	vp->zoom = zoom;

	vp->virtual_width = ScaleByZoom(width, zoom);
	vp->virtual_height = ScaleByZoom(height, zoom);

	Point pt;

	if (follow_flags & 0x80000000) {
		const Vehicle *veh;

		vp->follow_vehicle = (VehicleID)(follow_flags & 0xFFFF);
		veh = GetVehicle(vp->follow_vehicle);
		pt = MapXYZToViewport(vp, veh->x_pos, veh->y_pos, veh->z_pos);
	} else {
		uint x = TileX(follow_flags) * TILE_SIZE;
		uint y = TileY(follow_flags) * TILE_SIZE;

		vp->follow_vehicle = INVALID_VEHICLE;
		pt = MapXYZToViewport(vp, x, y, GetSlopeZ(x, y));
	}

	vp->scrollpos_x = pt.x;
	vp->scrollpos_y = pt.y;
	vp->dest_scrollpos_x = pt.x;
	vp->dest_scrollpos_y = pt.y;

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

	/* viewport is bound to its left top corner, so it must be rounded down (UnScaleByZoomLower)
	 * else glitch described in FS#1412 will happen (offset by 1 pixel with zoom level > NORMAL)
	 */
	old_left = UnScaleByZoomLower(old_left, vp->zoom);
	old_top = UnScaleByZoomLower(old_top, vp->zoom);
	x = UnScaleByZoomLower(x, vp->zoom);
	y = UnScaleByZoomLower(y, vp->zoom);

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

/**
 * Is a xy position inside the viewport of the window?
 * @param w Window to examine its viewport
 * @param x X coordinate of the xy position
 * @param y Y coordinate of the xy position
 * @return Pointer to the viewport if the xy position is in the viewport of the window,
 *         otherwise \c NULL is returned.
 */
ViewPort *IsPtInWindowViewport(const Window *w, int x, int y)
{
	ViewPort *vp = w->viewport;

	if (vp != NULL &&
			IsInsideMM(x, vp->left, vp->left + vp->width) &&
			IsInsideMM(y, vp->top, vp->top + vp->height))
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
	a = Clamp(a, 0, (int)(MapMaxX() * TILE_SIZE) - 1);
	b = Clamp(b, 0, (int)(MapMaxY() * TILE_SIZE) - 1);

	/* (a, b) is the X/Y-world coordinate that belongs to (x,y) if the landscape would be completely flat on height 0.
	 * Now find the Z-world coordinate by fix point iteration.
	 * This is a bit tricky because the tile height is non-continuous at foundations.
	 * The clicked point should be approached from the back, otherwise there are regions that are not clickable.
	 * (FOUNDATION_HALFTILE_LOWER on SLOPE_STEEP_S hides north halftile completely)
	 * So give it a z-malus of 4 in the first iterations.
	 */
	z = 0;
	for (int i = 0; i < 5; i++) z = GetSlopeZ(a + max(z, 4u) - 4, b + max(z, 4u) - 4) / 2;
	for (uint malus = 3; malus > 0; malus--) z = GetSlopeZ(a + max(z, malus) - malus, b + max(z, malus) - malus) / 2;
	for (int i = 0; i < 5; i++) z = GetSlopeZ(a + z, b + z) / 2;

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
	ViewPort *vp = w->viewport;

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
	w->SetWidgetDisabledState(widget_zoom_in, vp->zoom == ZOOM_LVL_MIN);
	w->InvalidateWidget(widget_zoom_in);

	w->SetWidgetDisabledState(widget_zoom_out, vp->zoom == ZOOM_LVL_MAX);
	w->InvalidateWidget(widget_zoom_out);
}

/**
 * Draws a ground sprite at a specific world-coordinate.
 *
 * @param image the image to draw.
 * @param pal the provided palette.
 * @param x position x of the sprite.
 * @param y position y of the sprite.
 * @param z position z of the sprite.
 * @param sub Only draw a part of the sprite.
 *
 */
void DrawGroundSpriteAt(SpriteID image, SpriteID pal, int32 x, int32 y, byte z, const SubSprite *sub)
{
	assert((image & SPRITE_MASK) < MAX_SPRITES);

	TileSpriteToDraw *ts = _vd.tile_sprites_to_draw.Append();
	ts->image = image;
	ts->pal = pal;
	ts->sub = sub;
	ts->x = x;
	ts->y = y;
	ts->z = z;
}

/**
 * Adds a child sprite to the active foundation.
 *
 * The pixel offset of the sprite relative to the ParentSprite is the sum of the offset passed to OffsetGroundSprite() and extra_offs_?.
 *
 * @param image the image to draw.
 * @param pal the provided palette.
 * @param sub Only draw a part of the sprite.
 * @param foundation_part Foundation part.
 * @param extra_offs_x Pixel X offset for the sprite position.
 * @param extra_offs_y Pixel Y offset for the sprite position.
 */
static void AddChildSpriteToFoundation(SpriteID image, SpriteID pal, const SubSprite *sub, FoundationPart foundation_part, int extra_offs_x, int extra_offs_y)
{
	assert(IsInsideMM(foundation_part, 0, FOUNDATION_PART_END));
	assert(_vd.foundation[foundation_part] != -1);
	Point offs = _vd.foundation_offset[foundation_part];

	/* Change the active ChildSprite list to the one of the foundation */
	int *old_child = _vd.last_child;
	_vd.last_child = _vd.last_foundation_child[foundation_part];

	AddChildSpriteScreen(image, pal, offs.x + extra_offs_x, offs.y + extra_offs_y, false, sub);

	/* Switch back to last ChildSprite list */
	_vd.last_child = old_child;
}

/**
 * Draws a ground sprite for the current tile.
 * If the current tile is drawn on top of a foundation the sprite is added as child sprite to the "foundation"-ParentSprite.
 *
 * @param image the image to draw.
 * @param pal the provided palette.
 * @param sub Only draw a part of the sprite.
 */
void DrawGroundSprite(SpriteID image, SpriteID pal, const SubSprite *sub)
{
	/* Switch to first foundation part, if no foundation was drawn */
	if (_vd.foundation_part == FOUNDATION_PART_NONE) _vd.foundation_part = FOUNDATION_PART_NORMAL;

	if (_vd.foundation[_vd.foundation_part] != -1) {
		AddChildSpriteToFoundation(image, pal, sub, _vd.foundation_part, 0, 0);
	} else {
		DrawGroundSpriteAt(image, pal, _cur_ti->x, _cur_ti->y, _cur_ti->z, sub);
	}
}


/**
 * Called when a foundation has been drawn for the current tile.
 * Successive ground sprites for the current tile will be drawn as child sprites of the "foundation"-ParentSprite, not as TileSprites.
 *
 * @param x sprite x-offset (screen coordinates) of ground sprites relative to the "foundation"-ParentSprite.
 * @param y sprite y-offset (screen coordinates) of ground sprites relative to the "foundation"-ParentSprite.
 */
void OffsetGroundSprite(int x, int y)
{
	/* Switch to next foundation part */
	switch (_vd.foundation_part) {
		case FOUNDATION_PART_NONE:
			_vd.foundation_part = FOUNDATION_PART_NORMAL;
			break;
		case FOUNDATION_PART_NORMAL:
			_vd.foundation_part = FOUNDATION_PART_HALFTILE;
			break;
		default: NOT_REACHED();
	}

	/* _vd.last_child == NULL if foundation sprite was clipped by the viewport bounds */
	if (_vd.last_child != NULL) _vd.foundation[_vd.foundation_part] = _vd.parent_sprites_to_draw.items - 1;

	_vd.foundation_offset[_vd.foundation_part].x = x;
	_vd.foundation_offset[_vd.foundation_part].y = y;
	_vd.last_foundation_child[_vd.foundation_part] = _vd.last_child;
}

/**
 * Adds a child sprite to a parent sprite.
 * In contrast to "AddChildSpriteScreen()" the sprite position is in world coordinates
 *
 * @param image the image to draw.
 * @param pal the provided palette.
 * @param x position x of the sprite.
 * @param y position y of the sprite.
 * @param z position z of the sprite.
 * @param sub Only draw a part of the sprite.
 */
static void AddCombinedSprite(SpriteID image, SpriteID pal, int x, int y, byte z, const SubSprite *sub)
{
	Point pt = RemapCoords(x, y, z);
	const Sprite* spr = GetSprite(image & SPRITE_MASK);

	if (pt.x + spr->x_offs >= _vd.dpi.left + _vd.dpi.width ||
			pt.x + spr->x_offs + spr->width <= _vd.dpi.left ||
			pt.y + spr->y_offs >= _vd.dpi.top + _vd.dpi.height ||
			pt.y + spr->y_offs + spr->height <= _vd.dpi.top)
		return;

	const ParentSpriteToDraw *pstd = _vd.parent_sprites_to_draw.End() - 1;
	AddChildSpriteScreen(image, pal, pt.x - pstd->left, pt.y - pstd->top, false, sub);
}

/** Draw a (transparent) sprite at given coordinates with a given bounding box.
 * The bounding box extends from (x + bb_offset_x, y + bb_offset_y, z + bb_offset_z) to (x + w - 1, y + h - 1, z + dz - 1), both corners included.
 * Bounding boxes with bb_offset_x == w or bb_offset_y == h or bb_offset_z == dz are allowed and produce thin slices.
 *
 * @note Bounding boxes are normally specified with bb_offset_x = bb_offset_y = bb_offset_z = 0. The extent of the bounding box in negative direction is
 *       defined by the sprite offset in the grf file.
 *       However if modifying the sprite offsets is not suitable (e.g. when using existing graphics), the bounding box can be tuned by bb_offset.
 *
 * @pre w >= bb_offset_x, h >= bb_offset_y, dz >= bb_offset_z. Else w, h or dz are ignored.
 *
 * @param image the image to combine and draw,
 * @param pal the provided palette,
 * @param x position X (world) of the sprite,
 * @param y position Y (world) of the sprite,
 * @param w bounding box extent towards positive X (world),
 * @param h bounding box extent towards positive Y (world),
 * @param dz bounding box extent towards positive Z (world),
 * @param z position Z (world) of the sprite,
 * @param transparent if true, switch the palette between the provided palette and the transparent palette,
 * @param bb_offset_x bounding box extent towards negative X (world),
 * @param bb_offset_y bounding box extent towards negative Y (world),
 * @param bb_offset_z bounding box extent towards negative Z (world)
 * @param sub Only draw a part of the sprite.
 */
void AddSortableSpriteToDraw(SpriteID image, SpriteID pal, int x, int y, int w, int h, int dz, int z, bool transparent, int bb_offset_x, int bb_offset_y, int bb_offset_z, const SubSprite *sub)
{
	int32 left, right, top, bottom;

	assert((image & SPRITE_MASK) < MAX_SPRITES);

	/* make the sprites transparent with the right palette */
	if (transparent) {
		SetBit(image, PALETTE_MODIFIER_TRANSPARENT);
		pal = PALETTE_TO_TRANSPARENT;
	}

	if (_vd.combine_sprites == 2) {
		AddCombinedSprite(image, pal, x, y, z, sub);
		return;
	}

	_vd.last_child = NULL;

	Point pt = RemapCoords(x, y, z);
	int tmp_left, tmp_top, tmp_x = pt.x, tmp_y = pt.y;

	/* Compute screen extents of sprite */
	if (image == SPR_EMPTY_BOUNDING_BOX) {
		left = tmp_left = RemapCoords(x + w          , y + bb_offset_y, z + bb_offset_z).x;
		right           = RemapCoords(x + bb_offset_x, y + h          , z + bb_offset_z).x + 1;
		top  = tmp_top  = RemapCoords(x + bb_offset_x, y + bb_offset_y, z + dz         ).y;
		bottom          = RemapCoords(x + w          , y + h          , z + bb_offset_z).y + 1;
	} else {
		const Sprite *spr = GetSprite(image & SPRITE_MASK);
		left = tmp_left = (pt.x += spr->x_offs);
		right           = (pt.x +  spr->width );
		top  = tmp_top  = (pt.y += spr->y_offs);
		bottom          = (pt.y +  spr->height);
	}

	if (_draw_bounding_boxes && (image != SPR_EMPTY_BOUNDING_BOX)) {
		/* Compute maximal extents of sprite and it's bounding box */
		left   = min(left  , RemapCoords(x + w          , y + bb_offset_y, z + bb_offset_z).x);
		right  = max(right , RemapCoords(x + bb_offset_x, y + h          , z + bb_offset_z).x + 1);
		top    = min(top   , RemapCoords(x + bb_offset_x, y + bb_offset_y, z + dz         ).y);
		bottom = max(bottom, RemapCoords(x + w          , y + h          , z + bb_offset_z).y + 1);
	}

	/* Do not add the sprite to the viewport, if it is outside */
	if (left   >= _vd.dpi.left + _vd.dpi.width ||
	    right  <= _vd.dpi.left                 ||
	    top    >= _vd.dpi.top + _vd.dpi.height ||
	    bottom <= _vd.dpi.top) {
		return;
	}

	ParentSpriteToDraw *ps = _vd.parent_sprites_to_draw.Append();
	ps->x = tmp_x;
	ps->y = tmp_y;

	ps->left = tmp_left;
	ps->top  = tmp_top;

	ps->image = image;
	ps->pal = pal;
	ps->sub = sub;
	ps->xmin = x + bb_offset_x;
	ps->xmax = x + max(bb_offset_x, w) - 1;

	ps->ymin = y + bb_offset_y;
	ps->ymax = y + max(bb_offset_y, h) - 1;

	ps->zmin = z + bb_offset_z;
	ps->zmax = z + max(bb_offset_z, dz) - 1;

	ps->comparison_done = false;
	ps->first_child = _vd.child_screen_sprites_to_draw.items;
	ps->last_child  = _vd.child_screen_sprites_to_draw.items;

	_vd.last_child = &ps->last_child;

	if (_vd.combine_sprites == 1) _vd.combine_sprites = 2;
}

void StartSpriteCombine()
{
	_vd.combine_sprites = 1;
}

void EndSpriteCombine()
{
	_vd.combine_sprites = 0;
}

/**
 * Add a child sprite to a parent sprite.
 *
 * @param image the image to draw.
 * @param pal the provided palette.
 * @param x sprite x-offset (screen coordinates) relative to parent sprite.
 * @param y sprite y-offset (screen coordinates) relative to parent sprite.
 * @param transparent if true, switch the palette between the provided palette and the transparent palette,
 * @param sub Only draw a part of the sprite.
 */
void AddChildSpriteScreen(SpriteID image, SpriteID pal, int x, int y, bool transparent, const SubSprite *sub)
{
	assert((image & SPRITE_MASK) < MAX_SPRITES);

	/* If the ParentSprite was clipped by the viewport bounds, do not draw the ChildSprites either */
	if (_vd.last_child == NULL) return;

	/* make the sprites transparent with the right palette */
	if (transparent) {
		SetBit(image, PALETTE_MODIFIER_TRANSPARENT);
		pal = PALETTE_TO_TRANSPARENT;
	}

	/* Append the sprite to the active ChildSprite list.
	 * If the active ParentSprite is a foundation, update last_foundation_child as well. */
	ChildScreenSpriteToDraw *cs = _vd.child_screen_sprites_to_draw.Append();
	cs->image = image;
	cs->pal = pal;
	cs->sub = sub;
	cs->x = x;
	cs->y = y;
	cs->next = NULL;

	*_vd.last_child = _vd.child_screen_sprites_to_draw.items;
}

/* Returns a StringSpriteToDraw */
void AddStringToDraw(int x, int y, StringID string, uint64 params_1, uint64 params_2, uint16 color, uint16 width)
{
	StringSpriteToDraw *ss = _vd.string_sprites_to_draw.Append();
	ss->string = string;
	ss->x = x;
	ss->y = y;
	ss->params[0] = params_1;
	ss->params[1] = params_2;
	ss->width = width;
	ss->color = color;
}


/**
 * Draws sprites between ground sprite and everything above.
 *
 * The sprite is either drawn as TileSprite or as ChildSprite of the active foundation.
 *
 * @param image the image to draw.
 * @param pal the provided palette.
 * @param ti TileInfo Tile that is being drawn
 * @param z_offset Z offset relative to the groundsprite. Only used for the sprite position, not for sprite sorting.
 * @param foundation_part Foundation part the sprite belongs to.
 */
static void DrawSelectionSprite(SpriteID image, SpriteID pal, const TileInfo *ti, int z_offset, FoundationPart foundation_part)
{
	/* FIXME: This is not totally valid for some autorail highlights, that extent over the edges of the tile. */
	if (_vd.foundation[foundation_part] == -1) {
		/* draw on real ground */
		DrawGroundSpriteAt(image, pal, ti->x, ti->y, ti->z + z_offset);
	} else {
		/* draw on top of foundation */
		AddChildSpriteToFoundation(image, pal, NULL, foundation_part, 0, -z_offset);
	}
}

/**
 * Draws a selection rectangle on a tile.
 *
 * @param ti TileInfo Tile that is being drawn
 * @param pal Palette to apply.
 */
static void DrawTileSelectionRect(const TileInfo *ti, SpriteID pal)
{
	SpriteID sel;
	if (IsHalftileSlope(ti->tileh)) {
		Corner halftile_corner = GetHalftileSlopeCorner(ti->tileh);
		SpriteID sel2 = SPR_HALFTILE_SELECTION_FLAT + halftile_corner;
		DrawSelectionSprite(sel2, pal, ti, 7 + TILE_HEIGHT, FOUNDATION_PART_HALFTILE);

		Corner opposite_corner = OppositeCorner(halftile_corner);
		if (IsSteepSlope(ti->tileh)) {
			sel = SPR_HALFTILE_SELECTION_DOWN;
		} else {
			sel = ((ti->tileh & SlopeWithOneCornerRaised(opposite_corner)) != 0 ? SPR_HALFTILE_SELECTION_UP : SPR_HALFTILE_SELECTION_FLAT);
		}
		sel += opposite_corner;
	} else {
		sel = SPR_SELECT_TILE + _tileh_to_sprite[ti->tileh];
	}
	DrawSelectionSprite(sel, pal, ti, 7, FOUNDATION_PART_NORMAL);
}

static bool IsPartOfAutoLine(int px, int py)
{
	px -= _thd.selstart.x;
	py -= _thd.selstart.y;

	if ((_thd.drawstyle & ~HT_DIR_MASK) != HT_LINE) return false;

	switch (_thd.drawstyle & HT_DIR_MASK) {
		case HT_DIR_X:  return py == 0; // x direction
		case HT_DIR_Y:  return px == 0; // y direction
		case HT_DIR_HU: return px == -py || px == -py - 16; // horizontal upper
		case HT_DIR_HL: return px == -py || px == -py + 16; // horizontal lower
		case HT_DIR_VL: return px == py || px == py + 16; // vertival left
		case HT_DIR_VR: return px == py || px == py - 16; // vertical right
		default:
			NOT_REACHED();
	}
}

// [direction][side]
static const HighLightStyle _autorail_type[6][2] = {
	{ HT_DIR_X,  HT_DIR_X },
	{ HT_DIR_Y,  HT_DIR_Y },
	{ HT_DIR_HU, HT_DIR_HL },
	{ HT_DIR_HL, HT_DIR_HU },
	{ HT_DIR_VL, HT_DIR_VR },
	{ HT_DIR_VR, HT_DIR_VL }
};

#include "table/autorail.h"

/**
 * Draws autorail highlights.
 *
 * @param *ti TileInfo Tile that is being drawn
 * @param autorail_type Offset into _AutorailTilehSprite[][]
 */
static void DrawAutorailSelection(const TileInfo *ti, uint autorail_type)
{
	SpriteID image;
	SpriteID pal;
	int offset;

	FoundationPart foundation_part = FOUNDATION_PART_NORMAL;
	Slope autorail_tileh = RemoveHalftileSlope(ti->tileh);
	if (IsHalftileSlope(ti->tileh)) {
		static const uint _lower_rail[4] = { 5U, 2U, 4U, 3U };
		Corner halftile_corner = GetHalftileSlopeCorner(ti->tileh);
		if (autorail_type != _lower_rail[halftile_corner]) {
			foundation_part = FOUNDATION_PART_HALFTILE;
			/* Here we draw the highlights of the "three-corners-raised"-slope. That looks ok to me. */
			autorail_tileh = SlopeWithThreeCornersRaised(OppositeCorner(halftile_corner));
		}
	}

	offset = _AutorailTilehSprite[autorail_tileh][autorail_type];
	if (offset >= 0) {
		image = SPR_AUTORAIL_BASE + offset;
		pal = PAL_NONE;
	} else {
		image = SPR_AUTORAIL_BASE - offset;
		pal = PALETTE_SEL_TILE_RED;
	}

	DrawSelectionSprite(image, _thd.make_square_red ? PALETTE_SEL_TILE_RED : pal, ti, 7, foundation_part);
}

/**
 * Checks if the specified tile is selected and if so draws selection using correct selectionstyle.
 * @param *ti TileInfo Tile that is being drawn
 */
static void DrawTileSelection(const TileInfo *ti)
{
	/* Draw a red error square? */
	bool is_redsq = _thd.redsq != 0 && _thd.redsq == ti->tile;
	if (is_redsq) DrawTileSelectionRect(ti, PALETTE_TILE_RED_PULSATING);

	/* no selection active? */
	if (_thd.drawstyle == 0) return;

	/* Inside the inner area? */
	if (IsInsideBS(ti->x, _thd.pos.x, _thd.size.x) &&
			IsInsideBS(ti->y, _thd.pos.y, _thd.size.y)) {
		if (_thd.drawstyle & HT_RECT) {
			if (!is_redsq) DrawTileSelectionRect(ti, _thd.make_square_red ? PALETTE_SEL_TILE_RED : PAL_NONE);
		} else if (_thd.drawstyle & HT_POINT) {
			/* Figure out the Z coordinate for the single dot. */
			byte z = 0;
			FoundationPart foundation_part = FOUNDATION_PART_NORMAL;
			if (ti->tileh & SLOPE_N) {
				z += TILE_HEIGHT;
				if (RemoveHalftileSlope(ti->tileh) == SLOPE_STEEP_N) z += TILE_HEIGHT;
			}
			if (IsHalftileSlope(ti->tileh)) {
				Corner halftile_corner = GetHalftileSlopeCorner(ti->tileh);
				if ((halftile_corner == CORNER_W) || (halftile_corner == CORNER_E)) z += TILE_HEIGHT;
				if (halftile_corner != CORNER_S) {
					foundation_part = FOUNDATION_PART_HALFTILE;
					if (IsSteepSlope(ti->tileh)) z -= TILE_HEIGHT;
				}
			}
			DrawSelectionSprite(_cur_dpi->zoom <= ZOOM_LVL_DETAIL ? SPR_DOT : SPR_DOT_SMALL, PAL_NONE, ti, z, foundation_part);
		} else if (_thd.drawstyle & HT_RAIL /*&& _thd.place_mode == VHM_RAIL*/) {
			/* autorail highlight piece under cursor */
			uint type = _thd.drawstyle & 0xF;
			assert(type <= 5);
			DrawAutorailSelection(ti, _autorail_type[type][0]);
		} else if (IsPartOfAutoLine(ti->x, ti->y)) {
			/* autorail highlighting long line */
			int dir = _thd.drawstyle & ~0xF0;
			uint side;

			if (dir < 2) {
				side = 0;
			} else {
				TileIndex start = TileVirtXY(_thd.selstart.x, _thd.selstart.y);
				side = Delta(Delta(TileX(start), TileX(ti->tile)), Delta(TileY(start), TileY(ti->tile)));
			}

			DrawAutorailSelection(ti, _autorail_type[dir][side]);
		}
		return;
	}

	/* Check if it's inside the outer area? */
	if (!is_redsq && _thd.outersize.x &&
			_thd.size.x < _thd.size.x + _thd.outersize.x &&
			IsInsideBS(ti->x, _thd.pos.x + _thd.offs.x, _thd.size.x + _thd.outersize.x) &&
			IsInsideBS(ti->y, _thd.pos.y + _thd.offs.y, _thd.size.y + _thd.outersize.y)) {
		/* Draw a blue rect. */
		DrawTileSelectionRect(ti, PALETTE_SEL_TILE_BLUE);
		return;
	}
}

static void ViewportAddLandscape()
{
	int x, y, width, height;
	TileInfo ti;
	bool direction;

	_cur_ti = &ti;

	/* Transform into tile coordinates and round to closest full tile */
	x = ((_vd.dpi.top >> 1) - (_vd.dpi.left >> 2)) & ~0xF;
	y = ((_vd.dpi.top >> 1) + (_vd.dpi.left >> 2) - 0x10) & ~0xF;

	/* determine size of area */
	{
		Point pt = RemapCoords(x, y, 241);
		width = (_vd.dpi.left + _vd.dpi.width - pt.x + 95) >> 6;
		height = (_vd.dpi.top + _vd.dpi.height - pt.y) >> 5 << 1;
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

			_vd.foundation_part = FOUNDATION_PART_NONE;
			_vd.foundation[0] = -1;
			_vd.foundation[1] = -1;
			_vd.last_foundation_child[0] = NULL;
			_vd.last_foundation_child[1] = NULL;

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

	if (!HasBit(_display_opt, DO_SHOW_TOWN_NAMES) || _game_mode == GM_MENU)
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
						left   < t->sign.left + t->sign.width_1 * 2) {
					AddStringToDraw(t->sign.left + 1, t->sign.top + 1,
						_patches.population_in_label ? STR_TOWN_LABEL_POP : STR_TOWN_LABEL,
						t->index, t->population);
				}
			}
			break;

		case ZOOM_LVL_OUT_4X:
		case ZOOM_LVL_OUT_8X:
			right += ScaleByZoom(1, dpi->zoom);
			bottom += ScaleByZoom(1, dpi->zoom) + 1;

			FOR_ALL_TOWNS(t) {
				if (bottom > t->sign.top &&
						top    < t->sign.top + ScaleByZoom(12, dpi->zoom) &&
						right  > t->sign.left &&
						left   < t->sign.left + ScaleByZoom(t->sign.width_2, dpi->zoom)) {
					AddStringToDraw(t->sign.left + 5, t->sign.top + 1, STR_TOWN_LABEL_TINY_BLACK, t->index, 0);
					AddStringToDraw(t->sign.left + 1, t->sign.top - 3, STR_TOWN_LABEL_TINY_WHITE, t->index, 0);
				}
			}
			break;

		default: NOT_REACHED();
	}
}


static void AddStation(const Station *st, StringID str, uint16 width)
{
	AddStringToDraw(st->sign.left + 1, st->sign.top + 1, str, st->index, st->facilities, (st->owner == OWNER_NONE || st->facilities == 0) ? 0xE : _player_colors[st->owner], width);
}


static void ViewportAddStationNames(DrawPixelInfo *dpi)
{
	int left, top, right, bottom;
	const Station *st;

	if (!HasBit(_display_opt, DO_SHOW_STATION_NAMES) || _game_mode == GM_MENU)
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
						left   < st->sign.left + st->sign.width_1 * 2) {
					AddStation(st, STR_305C_0, st->sign.width_1);
				}
			}
			break;

		case ZOOM_LVL_OUT_4X:
		case ZOOM_LVL_OUT_8X:
			right += ScaleByZoom(1, dpi->zoom);
			bottom += ScaleByZoom(1, dpi->zoom) + 1;

			FOR_ALL_STATIONS(st) {
				if (bottom > st->sign.top &&
						top    < st->sign.top + ScaleByZoom(12, dpi->zoom) &&
						right  > st->sign.left &&
						left   < st->sign.left + ScaleByZoom(st->sign.width_2, dpi->zoom)) {
					AddStation(st, STR_STATION_SIGN_TINY, st->sign.width_2 | 0x8000);
				}
			}
			break;

		default: NOT_REACHED();
	}
}


static void AddSign(const Sign *si, StringID str, uint16 width)
{
	AddStringToDraw(si->sign.left + 1, si->sign.top + 1, str, si->index, 0, (si->owner == OWNER_NONE) ? 14 : _player_colors[si->owner], width);
}


static void ViewportAddSigns(DrawPixelInfo *dpi)
{
	const Sign *si;
	int left, top, right, bottom;

	/* Signs are turned off or are invisible */
	if (!HasBit(_display_opt, DO_SHOW_SIGNS) || IsInvisibilitySet(TO_SIGNS)) return;

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

		case ZOOM_LVL_OUT_4X:
		case ZOOM_LVL_OUT_8X:
			right += ScaleByZoom(1, dpi->zoom);
			bottom += ScaleByZoom(1, dpi->zoom) + 1;

			FOR_ALL_SIGNS(si) {
				if (bottom > si->sign.top &&
						top    < si->sign.top + ScaleByZoom(12, dpi->zoom) &&
						right  > si->sign.left &&
						left   < si->sign.left + ScaleByZoom(si->sign.width_2, dpi->zoom)) {
					AddSign(si, IsTransparencySet(TO_SIGNS) ? STR_2002_WHITE : STR_2002, si->sign.width_2 | 0x8000);
				}
			}
			break;

		default: NOT_REACHED();
	}
}


static void AddWaypoint(const Waypoint *wp, StringID str, uint16 width)
{
	AddStringToDraw(wp->sign.left + 1, wp->sign.top + 1, str, wp->index, 0, (wp->deleted ? 0xE : 11), width);
}


static void ViewportAddWaypoints(DrawPixelInfo *dpi)
{
	const Waypoint *wp;
	int left, top, right, bottom;

	if (!HasBit(_display_opt, DO_WAYPOINTS))
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
						left   < wp->sign.left + wp->sign.width_1 * 2) {
					AddWaypoint(wp, STR_WAYPOINT_VIEWPORT, wp->sign.width_1);
				}
			}
			break;

		case ZOOM_LVL_OUT_4X:
		case ZOOM_LVL_OUT_8X:
			right += ScaleByZoom(1, dpi->zoom);
			bottom += ScaleByZoom(1, dpi->zoom) + 1;

			FOR_ALL_WAYPOINTS(wp) {
				if (bottom > wp->sign.top &&
						top    < wp->sign.top + ScaleByZoom(12, dpi->zoom) &&
						right  > wp->sign.left &&
						left   < wp->sign.left + ScaleByZoom(wp->sign.width_2, dpi->zoom)) {
					AddWaypoint(wp, STR_WAYPOINT_VIEWPORT_TINY, wp->sign.width_2 | 0x8000);
				}
			}
			break;

		default: NOT_REACHED();
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


static void ViewportDrawTileSprites(const TileSpriteToDrawVector *tstdv)
{
	const TileSpriteToDraw *tsend = tstdv->End();
	for (const TileSpriteToDraw *ts = tstdv->Begin(); ts != tsend; ++ts) {
		Point pt = RemapCoords(ts->x, ts->y, ts->z);
		DrawSprite(ts->image, ts->pal, pt.x, pt.y, ts->sub);
	}
}

static void ViewportSortParentSprites(ParentSpriteToSortVector *psdv)
{
	ParentSpriteToDraw **psdvend = psdv->End();
	ParentSpriteToDraw **psd = psdv->Begin();
	while (psd != psdvend) {
		ParentSpriteToDraw *ps = *psd;

		if (ps->comparison_done) {
			psd++;
			continue;
		}

		ps->comparison_done = true;

		for (ParentSpriteToDraw **psd2 = psd + 1; psd2 != psdvend; psd2++) {
			ParentSpriteToDraw *ps2 = *psd2;

			if (ps2->comparison_done) continue;

			/* Decide which comparator to use, based on whether the bounding
			 * boxes overlap
			 */
			if (ps->xmax >= ps2->xmin && ps->xmin <= ps2->xmax && // overlap in X?
					ps->ymax >= ps2->ymin && ps->ymin <= ps2->ymax && // overlap in Y?
					ps->zmax >= ps2->zmin && ps->zmin <= ps2->zmax) { // overlap in Z?
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
				/* We only change the order, if it is definite.
				 * I.e. every single order of X, Y, Z says ps2 is behind ps or they overlap.
				 * That is: If one partial order says ps behind ps2, do not change the order.
				 */
				if (ps->xmax < ps2->xmin ||
						ps->ymax < ps2->ymin ||
						ps->zmax < ps2->zmin) {
					continue;
				}
			}

			/* Swap the two sprites ps and ps2 using bubble-sort algorithm. */
			ParentSpriteToDraw **psd3 = psd;
			do {
				ParentSpriteToDraw *temp = *psd3;
				*psd3 = ps2;
				ps2 = temp;

				psd3++;
			} while (psd3 <= psd2);
		}
	}
}

static void ViewportDrawParentSprites(const ParentSpriteToSortVector *psd, const ChildScreenSpriteToDrawVector *csstdv)
{
	const ParentSpriteToDraw * const *psd_end = psd->End();
	for (const ParentSpriteToDraw * const *it = psd->Begin(); it != psd_end; it++) {
		const ParentSpriteToDraw *ps = *it;
		if (ps->image != SPR_EMPTY_BOUNDING_BOX) DrawSprite(ps->image, ps->pal, ps->x, ps->y, ps->sub);

		const ChildScreenSpriteToDraw *last = csstdv->Get(ps->last_child);
		for (const ChildScreenSpriteToDraw *cs = csstdv->Get(ps->first_child); cs != last; cs++) {
			DrawSprite(cs->image, cs->pal, ps->left + cs->x, ps->top + cs->y, cs->sub);
		}
	}
}

/**
 * Draws the bounding boxes of all ParentSprites
 * @param psd Array of ParentSprites
 */
static void ViewportDrawBoundingBoxes(const ParentSpriteToSortVector *psd)
{
	const ParentSpriteToDraw * const *psd_end = psd->End();
	for (const ParentSpriteToDraw * const *it = psd->Begin(); it != psd_end; it++) {
		const ParentSpriteToDraw *ps = *it;
		Point pt1 = RemapCoords(ps->xmax + 1, ps->ymax + 1, ps->zmax + 1); // top front corner
		Point pt2 = RemapCoords(ps->xmin    , ps->ymax + 1, ps->zmax + 1); // top left corner
		Point pt3 = RemapCoords(ps->xmax + 1, ps->ymin    , ps->zmax + 1); // top right corner
		Point pt4 = RemapCoords(ps->xmax + 1, ps->ymax + 1, ps->zmin    ); // bottom front corner

		DrawBox(        pt1.x,         pt1.y,
		        pt2.x - pt1.x, pt2.y - pt1.y,
		        pt3.x - pt1.x, pt3.y - pt1.y,
		        pt4.x - pt1.x, pt4.y - pt1.y);
	}
}

static void ViewportDrawStrings(DrawPixelInfo *dpi, const StringSpriteToDrawVector *sstdv)
{
	DrawPixelInfo dp;
	ZoomLevel zoom;

	_cur_dpi = &dp;
	dp = *dpi;

	zoom = dp.zoom;
	dp.zoom = ZOOM_LVL_NORMAL;

	dp.left   = UnScaleByZoom(dp.left,   zoom);
	dp.top    = UnScaleByZoom(dp.top,    zoom);
	dp.width  = UnScaleByZoom(dp.width,  zoom);
	dp.height = UnScaleByZoom(dp.height, zoom);

	const StringSpriteToDraw *ssend = sstdv->End();
	for (const StringSpriteToDraw *ss = sstdv->Begin(); ss != ssend; ++ss) {
		uint16 colour;

		if (ss->width != 0) {
			/* Do not draw signs nor station names if they are set invisible */
			if (IsInvisibilitySet(TO_SIGNS) && ss->string != STR_2806) continue;

			int x = UnScaleByZoom(ss->x, zoom) - 1;
			int y = UnScaleByZoom(ss->y, zoom) - 1;
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
			if (!IsTransparencySet(TO_SIGNS) || ss->string == STR_2806) {
				DrawFrameRect(
					x, y, x + w, bottom, ss->color,
					IsTransparencySet(TO_SIGNS) ? FR_TRANSPARENT : FR_NONE
				);
			}
		}

		SetDParam(0, ss->params[0]);
		SetDParam(1, ss->params[1]);
		/* if we didn't draw a rectangle, or if transparant building is on,
		 * draw the text in the color the rectangle would have */
		if (IsTransparencySet(TO_SIGNS) && ss->string != STR_2806 && ss->width != 0) {
			/* Real colors need the IS_PALETTE_COLOR flag
			 * otherwise colors from _string_colormap are assumed. */
			colour = _colour_gradient[ss->color][6] | IS_PALETTE_COLOR;
		} else {
			colour = TC_BLACK;
		}
		DrawString(
			UnScaleByZoom(ss->x, zoom), UnScaleByZoom(ss->y, zoom) - (ss->width & 0x8000 ? 2 : 0),
			ss->string, colour
		);
	}
}

void ViewportDoDraw(const ViewPort *vp, int left, int top, int right, int bottom)
{
	DrawPixelInfo *old_dpi = _cur_dpi;
	_cur_dpi = &_vd.dpi;

	_vd.dpi.zoom = vp->zoom;
	int mask = ScaleByZoom(-1, vp->zoom);

	_vd.combine_sprites = 0;

	_vd.dpi.width = (right - left) & mask;
	_vd.dpi.height = (bottom - top) & mask;
	_vd.dpi.left = left & mask;
	_vd.dpi.top = top & mask;
	_vd.dpi.pitch = old_dpi->pitch;
	_vd.last_child = NULL;

	int x = UnScaleByZoom(_vd.dpi.left - (vp->virtual_left & mask), vp->zoom) + vp->left;
	int y = UnScaleByZoom(_vd.dpi.top - (vp->virtual_top & mask), vp->zoom) + vp->top;

	_vd.dpi.dst_ptr = BlitterFactoryBase::GetCurrentBlitter()->MoveTo(old_dpi->dst_ptr, x - old_dpi->left, y - old_dpi->top);

	ViewportAddLandscape();
	ViewportAddVehicles(&_vd.dpi);
	DrawTextEffects(&_vd.dpi);

	ViewportAddTownNames(&_vd.dpi);
	ViewportAddStationNames(&_vd.dpi);
	ViewportAddSigns(&_vd.dpi);
	ViewportAddWaypoints(&_vd.dpi);

	if (_vd.tile_sprites_to_draw.items != 0) ViewportDrawTileSprites(&_vd.tile_sprites_to_draw);

	ParentSpriteToDraw *psd_end = _vd.parent_sprites_to_draw.End();
	for (ParentSpriteToDraw *it = _vd.parent_sprites_to_draw.Begin(); it != psd_end; it++) {
		*_vd.parent_sprites_to_sort.Append() = it;
	}

	ViewportSortParentSprites(&_vd.parent_sprites_to_sort);
	ViewportDrawParentSprites(&_vd.parent_sprites_to_sort, &_vd.child_screen_sprites_to_draw);

	if (_draw_bounding_boxes) ViewportDrawBoundingBoxes(&_vd.parent_sprites_to_sort);

	if (_vd.string_sprites_to_draw.items != 0) ViewportDrawStrings(&_vd.dpi, &_vd.string_sprites_to_draw);

	_cur_dpi = old_dpi;

	_vd.string_sprites_to_draw.items = 0;
	_vd.tile_sprites_to_draw.items = 0;
	_vd.parent_sprites_to_draw.items = 0;
	_vd.parent_sprites_to_sort.items = 0;
	_vd.child_screen_sprites_to_draw.items = 0;
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

static inline void ClampViewportToMap(const ViewPort *vp, int &x, int &y)
{
	/* Centre of the viewport is hot spot */
	x += vp->virtual_width / 2;
	y += vp->virtual_height / 2;

	/* Convert viewport coordinates to map coordinates
	 * Calculation is scaled by 4 to avoid rounding errors */
	int vx = -x + y * 2;
	int vy =  x + y * 2;

	/* clamp to size of map */
	vx = Clamp(vx, 0, MapMaxX() * TILE_SIZE * 4);
	vy = Clamp(vy, 0, MapMaxY() * TILE_SIZE * 4);

	/* Convert map coordinates to viewport coordinates */
	x = (-vx + vy) / 2;
	y = ( vx + vy) / 4;

	/* Remove centreing */
	x -= vp->virtual_width / 2;
	y -= vp->virtual_height / 2;
}

void UpdateViewportPosition(Window *w)
{
	const ViewPort *vp = w->viewport;

	if (w->viewport->follow_vehicle != INVALID_VEHICLE) {
		const Vehicle* veh = GetVehicle(w->viewport->follow_vehicle);
		Point pt = MapXYZToViewport(vp, veh->x_pos, veh->y_pos, veh->z_pos);

		SetViewportPosition(w, pt.x, pt.y);
	} else {
		/* Ensure the destination location is within the map */
		ClampViewportToMap(vp, w->viewport->dest_scrollpos_x, w->viewport->dest_scrollpos_y);

		int delta_x = w->viewport->dest_scrollpos_x - w->viewport->scrollpos_x;
		int delta_y = w->viewport->dest_scrollpos_y - w->viewport->scrollpos_y;

		if (delta_x != 0 || delta_y != 0) {
			if (_patches.smooth_scroll) {
				int max_scroll = ScaleByMapSize1D(512);
				/* Not at our desired positon yet... */
				w->viewport->scrollpos_x += Clamp(delta_x / 4, -max_scroll, max_scroll);
				w->viewport->scrollpos_y += Clamp(delta_y / 4, -max_scroll, max_scroll);
			} else {
				w->viewport->scrollpos_x = w->viewport->dest_scrollpos_x;
				w->viewport->scrollpos_y = w->viewport->dest_scrollpos_y;
			}
		}

		ClampViewportToMap(vp, w->viewport->scrollpos_x, w->viewport->scrollpos_y);

		SetViewportPosition(w, w->viewport->scrollpos_x, w->viewport->scrollpos_y);
	}
}

/**
 * Marks a viewport as dirty for repaint if it displays (a part of) the area the needs to be repainted.
 * @param vp     The viewport to mark as dirty
 * @param left   Left edge of area to repaint
 * @param top    Top edge of area to repaint
 * @param right  Right edge of area to repaint
 * @param bottom Bottom edge of area to repaint
 * @ingroup dirty
 */
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

/**
 * Mark all viewports that display an area as dirty (in need of repaint).
 * @param left   Left edge of area to repaint
 * @param top    Top edge of area to repaint
 * @param right  Right edge of area to repaint
 * @param bottom Bottom edge of area to repaint
 * @ingroup dirty
 */
void MarkAllViewportsDirty(int left, int top, int right, int bottom)
{
	Window **wz;

	FOR_ALL_WINDOWS(wz) {
		ViewPort *vp = (*wz)->viewport;
		if (vp != NULL) {
			assert(vp->width != 0);
			MarkViewportDirty(vp, left, top, right, bottom);
		}
	}
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

	if (IsInsideMM(x, 0, MapSizeX() * TILE_SIZE) &&
			IsInsideMM(y, 0, MapSizeY() * TILE_SIZE))
		z = GetTileZ(TileVirtXY(x, y));
	pt = RemapCoords(x, y, z);

	MarkAllViewportsDirty(
		pt.x - 31,
		pt.y - 122,
		pt.x - 31 + 67,
		pt.y - 122 + 154
	);
}

/**
 * Marks the selected tiles as dirty.
 *
 * This function marks the selected tiles as dirty for repaint
 *
 * @note Documentation may be wrong (Progman)
 * @ingroup dirty
 */
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

	if (!HasBit(_display_opt, DO_SHOW_TOWN_NAMES)) return false;

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

		case ZOOM_LVL_OUT_4X:
		case ZOOM_LVL_OUT_8X:
			x = ScaleByZoom(x - vp->left + ScaleByZoom(1, vp->zoom) - 1, vp->zoom) + vp->virtual_left;
			y = ScaleByZoom(y - vp->top  + ScaleByZoom(1, vp->zoom) - 1, vp->zoom) + vp->virtual_top;

			FOR_ALL_TOWNS(t) {
				if (y >= t->sign.top &&
						y < t->sign.top + ScaleByZoom(12, vp->zoom) &&
						x >= t->sign.left &&
						x < t->sign.left + ScaleByZoom(t->sign.width_2, vp->zoom)) {
					ShowTownViewWindow(t->index);
					return true;
				}
			}
			break;

		default: NOT_REACHED();
	}

	return false;
}


static bool CheckClickOnStation(const ViewPort *vp, int x, int y)
{
	const Station *st;

	if (!HasBit(_display_opt, DO_SHOW_STATION_NAMES)) return false;

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

		case ZOOM_LVL_OUT_4X:
		case ZOOM_LVL_OUT_8X:
			x = ScaleByZoom(x - vp->left + ScaleByZoom(1, vp->zoom) - 1, vp->zoom) + vp->virtual_left;
			y = ScaleByZoom(y - vp->top  + ScaleByZoom(1, vp->zoom) - 1, vp->zoom) + vp->virtual_top;

			FOR_ALL_STATIONS(st) {
				if (y >= st->sign.top &&
						y < st->sign.top + ScaleByZoom(12, vp->zoom) &&
						x >= st->sign.left &&
						x < st->sign.left + ScaleByZoom(st->sign.width_2, vp->zoom)) {
					ShowStationViewWindow(st->index);
					return true;
				}
			}
			break;

		default: NOT_REACHED();
	}

	return false;
}


static bool CheckClickOnSign(const ViewPort *vp, int x, int y)
{
	const Sign *si;

	/* Signs are turned off, or they are transparent and invisibility is ON, or player is a spectator */
	if (!HasBit(_display_opt, DO_SHOW_SIGNS) || IsInvisibilitySet(TO_SIGNS) || _current_player == PLAYER_SPECTATOR) return false;

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

		case ZOOM_LVL_OUT_4X:
		case ZOOM_LVL_OUT_8X:
			x = ScaleByZoom(x - vp->left + ScaleByZoom(1, vp->zoom) - 1, vp->zoom) + vp->virtual_left;
			y = ScaleByZoom(y - vp->top  + ScaleByZoom(1, vp->zoom) - 1, vp->zoom) + vp->virtual_top;

			FOR_ALL_SIGNS(si) {
				if (y >= si->sign.top &&
						y <  si->sign.top + ScaleByZoom(12, vp->zoom) &&
						x >= si->sign.left &&
						x <  si->sign.left + ScaleByZoom(si->sign.width_2, vp->zoom)) {
					ShowRenameSignWindow(si);
					return true;
				}
			}
			break;

		default: NOT_REACHED();
	}

	return false;
}


static bool CheckClickOnWaypoint(const ViewPort *vp, int x, int y)
{
	const Waypoint *wp;

	if (!HasBit(_display_opt, DO_WAYPOINTS)) return false;

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

		case ZOOM_LVL_OUT_4X:
		case ZOOM_LVL_OUT_8X:
			x = ScaleByZoom(x - vp->left + ScaleByZoom(1, vp->zoom) - 1, vp->zoom) + vp->virtual_left;
			y = ScaleByZoom(y - vp->top  + ScaleByZoom(1, vp->zoom) - 1, vp->zoom) + vp->virtual_top;

			FOR_ALL_WAYPOINTS(wp) {
				if (y >= wp->sign.top &&
						y < wp->sign.top + ScaleByZoom(12, vp->zoom) &&
						x >= wp->sign.left &&
						x < wp->sign.left + ScaleByZoom(wp->sign.width_2, vp->zoom)) {
					ShowRenameWaypointWindow(wp);
					return true;
				}
			}
			break;

		default: NOT_REACHED();
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
	if (!IsFrontEngine(v)) v = v->First();
	ShowVehicleViewWindow(v);
}

static void SafeShowRoadVehViewWindow(const Vehicle *v)
{
	if (!IsRoadVehFront(v)) v = v->First();
	ShowVehicleViewWindow(v);
}

static void Nop(const Vehicle *v) {}

typedef void OnVehicleClickProc(const Vehicle *v);
static OnVehicleClickProc* const _on_vehicle_click_proc[] = {
	SafeShowTrainViewWindow,
	SafeShowRoadVehViewWindow,
	ShowVehicleViewWindow,
	ShowVehicleViewWindow,
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
	if (w != NULL) w->OnPlaceObject(pt, TileVirtXY(pt.x, pt.y));
}


/* scrolls the viewport in a window to a given location */
bool ScrollWindowTo(int x , int y, Window *w, bool instant)
{
	/* The slope cannot be acquired outside of the map, so make sure we are always within the map. */
	Point pt = MapXYZToViewport(w->viewport, x, y, GetSlopeZ(Clamp(x, 0, MapSizeX()), Clamp(y, 0, MapSizeY())));
	w->viewport->follow_vehicle = INVALID_VEHICLE;

	if (w->viewport->dest_scrollpos_x == pt.x && w->viewport->dest_scrollpos_y == pt.y)
		return false;

	if (instant) {
		w->viewport->scrollpos_x = pt.x;
		w->viewport->scrollpos_y = pt.y;
	}

	w->viewport->dest_scrollpos_x = pt.x;
	w->viewport->dest_scrollpos_y = pt.y;
	return true;
}

bool ScrollMainWindowToTile(TileIndex tile, bool instant)
{
	return ScrollMainWindowTo(TileX(tile) * TILE_SIZE + TILE_SIZE / 2, TileY(tile) * TILE_SIZE + TILE_SIZE / 2, instant);
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
static HighLightStyle GetAutorailHT(int x, int y)
{
	return HT_RAIL | _autorail_piece[x & 0xF][y & 0xF];
}

/**
 * Updates tile highlighting for all cases.
 * Uses _thd.selstart and _thd.selend and _thd.place_mode (set elsewhere) to determine _thd.pos and _thd.size
 * Also drawstyle is determined. Uses _thd.new.* as a buffer and calls SetSelectionTilesDirty() twice,
 * Once for the old and once for the new selection.
 * _thd is TileHighlightData, found in viewport.h
 * Called by MouseLoop() in windows.cpp
 */
void UpdateTileSelection()
{
	int x1;
	int y1;

	_thd.new_drawstyle = 0;

	if (_thd.place_mode == VHM_SPECIAL) {
		x1 = _thd.selend.x;
		y1 = _thd.selend.y;
		if (x1 != -1) {
			int x2 = _thd.selstart.x & ~0xF;
			int y2 = _thd.selstart.y & ~0xF;
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
					break;
				default:
					NOT_REACHED();
					break;
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
void VpStartPlaceSizing(TileIndex tile, ViewportPlaceMethod method, ViewportDragDropSelectionProcess process)
{
	_thd.select_method = method;
	_thd.select_proc   = process;
	_thd.selend.x = TileX(tile) * TILE_SIZE;
	_thd.selstart.x = TileX(tile) * TILE_SIZE;
	_thd.selend.y = TileY(tile) * TILE_SIZE;
	_thd.selstart.y = TileY(tile) * TILE_SIZE;

	/* Needed so several things (road, autoroad, bridges, ...) are placed correctly.
	 * In effect, placement starts from the centre of a tile
	 */
	if (method == VPM_X_OR_Y || method == VPM_FIX_X || method == VPM_FIX_Y) {
		_thd.selend.x += TILE_SIZE / 2;
		_thd.selend.y += TILE_SIZE / 2;
		_thd.selstart.x += TILE_SIZE / 2;
		_thd.selstart.y += TILE_SIZE / 2;
	}

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
	uint64 distance = DistanceManhattan(from, to) + 1;

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
static HighLightStyle Check2x1AutoRail(int mode)
{
	int fxpy = _tile_fract_coords.x + _tile_fract_coords.y;
	int sxpy = (_thd.selend.x & 0xF) + (_thd.selend.y & 0xF);
	int fxmy = _tile_fract_coords.x - _tile_fract_coords.y;
	int sxmy = (_thd.selend.x & 0xF) - (_thd.selend.y & 0xF);

	switch (mode) {
		default: NOT_REACHED();
		case 0: // end piece is lower right
			if (fxpy >= 20 && sxpy <= 12) { /*SwapSelection(); DoRailroadTrack(0); */return HT_DIR_HL; }
			if (fxmy < -3 && sxmy > 3) {/* DoRailroadTrack(0); */return HT_DIR_VR; }
			return HT_DIR_Y;

		case 1:
			if (fxmy > 3 && sxmy < -3) { /*SwapSelection(); DoRailroadTrack(0); */return HT_DIR_VL; }
			if (fxpy <= 12 && sxpy >= 20) { /*DoRailroadTrack(0); */return HT_DIR_HU; }
			return HT_DIR_Y;

		case 2:
			if (fxmy > 3 && sxmy < -3) { /*DoRailroadTrack(3);*/ return HT_DIR_VL; }
			if (fxpy >= 20 && sxpy <= 12) { /*SwapSelection(); DoRailroadTrack(0); */return HT_DIR_HL; }
			return HT_DIR_X;

		case 3:
			if (fxmy < -3 && sxmy > 3) { /*SwapSelection(); DoRailroadTrack(3);*/ return HT_DIR_VR; }
			if (fxpy <= 12 && sxpy >= 20) { /*DoRailroadTrack(0); */return HT_DIR_HU; }
			return HT_DIR_X;
	}
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
	w = abs(dx) + 16;
	h = abs(dy) + 16;

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
		uint64 params[2];

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
void VpSelectTilesWithMethod(int x, int y, ViewportPlaceMethod method)
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

	/* Needed so level-land is placed correctly */
	if (_thd.next_drawstyle == HT_POINT) {
		x += TILE_SIZE / 2;
		y += TILE_SIZE / 2;
	}

	sx = _thd.selstart.x;
	sy = _thd.selstart.y;

	switch (method) {
		case VPM_X_OR_Y: /* drag in X or Y direction */
			if (abs(sy - y) < abs(sx - x)) {
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
				uint64 params[2];

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
			x = sx + Clamp(x - sx, -limit, limit);
			y = sy + Clamp(y - sy, -limit, limit);
			} /* Fallthrough */
		case VPM_X_AND_Y: { /* drag an X by Y area */
			if (_patches.measure_tooltip) {
				static const StringID measure_strings_area[] = {
					STR_NULL, STR_NULL, STR_MEASURE_AREA, STR_MEASURE_AREA_HEIGHTDIFF
				};

				TileIndex t0 = TileVirtXY(sx, sy);
				TileIndex t1 = TileVirtXY(x, y);
				uint dx = Delta(TileX(t0), TileX(t1)) + 1;
				uint dy = Delta(TileY(t0), TileY(t1)) + 1;
				byte index = 0;
				uint64 params[3];

				/* If dragging an area (eg dynamite tool) and it is actually a single
				 * row/column, change the type to 'line' to get proper calculation for height */
				style = (HighLightStyle)_thd.next_drawstyle;
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
	if (_special_mouse_mode != WSM_SIZING) return true;

	/* stop drag mode if the window has been closed */
	Window *w = FindWindowById(_thd.window_class, _thd.window_number);
	if (w == NULL) {
		ResetObjectToPlace();
		return false;
	}

	/* while dragging execute the drag procedure of the corresponding window (mostly VpSelectTilesWithMethod() ) */
	if (_left_button_down) {
		w->OnPlaceDrag(_thd.select_method, _thd.select_proc, GetTileBelowCursor());
		return false;
	}

	/* mouse button released..
	 * keep the selected tool, but reset it to the original mode. */
	_special_mouse_mode = WSM_NONE;
	if (_thd.next_drawstyle == HT_RECT) {
		_thd.place_mode = VHM_RECT;
	} else if (_thd.select_method == VPM_SIGNALDIRS) { // some might call this a hack... -- Dominik
		_thd.place_mode = VHM_RECT;
	} else if (_thd.next_drawstyle & HT_LINE) {
		_thd.place_mode = VHM_RAIL;
	} else if (_thd.next_drawstyle & HT_RAIL) {
		_thd.place_mode = VHM_RAIL;
	} else {
		_thd.place_mode = VHM_POINT;
	}
	SetTileSelectSize(1, 1);

	w->OnPlaceMouseUp(_thd.select_method, _thd.select_proc, _thd.selend, TileVirtXY(_thd.selstart.x, _thd.selstart.y), TileVirtXY(_thd.selend.x, _thd.selend.y));

	return false;
}

void SetObjectToPlaceWnd(CursorID icon, SpriteID pal, ViewportHighlightMode mode, Window *w)
{
	SetObjectToPlace(icon, pal, mode, w->window_class, w->window_number);
}

#include "table/animcursors.h"

void SetObjectToPlace(CursorID icon, SpriteID pal, ViewportHighlightMode mode, WindowClass window_class, WindowNumber window_num)
{
	/* undo clicking on button and drag & drop */
	if (_thd.place_mode != VHM_NONE || _special_mouse_mode == WSM_DRAGDROP) {
		Window *w = FindWindowById(_thd.window_class, _thd.window_number);
		if (w != NULL) {
			/* Call the abort function, but set the window class to something
			 * that will never be used to avoid infinite loops. Setting it to
			 * the 'next' window class must not be done because recursion into
			 * this function might in some cases reset the newly set object to
			 * place or not properly reset the original selection. */
			_thd.window_class = WC_INVALID;
			w->OnPlaceObjectAbort();
		}
	}

	SetTileSelectSize(1, 1);

	_thd.make_square_red = false;

	if (mode == VHM_DRAG) { // VHM_DRAG is for dragdropping trains in the depot window
		mode = VHM_NONE;
		_special_mouse_mode = WSM_DRAGDROP;
	} else {
		_special_mouse_mode = WSM_NONE;
	}

	_thd.place_mode = mode;
	_thd.window_class = window_class;
	_thd.window_number = window_num;

	if (mode == VHM_SPECIAL) // special tools, like tunnels or docks start with presizing mode
		VpStartPreSizing();

	if ((int)icon < 0) {
		SetAnimatedMouseCursor(_animcursors[~icon]);
	} else {
		SetMouseCursor(icon, pal);
	}

}

void ResetObjectToPlace()
{
	SetObjectToPlace(SPR_CURSOR_MOUSE, PAL_NONE, VHM_NONE, WC_MAIN_WINDOW, 0);
}
