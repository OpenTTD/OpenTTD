/* $Id$ */

#ifndef VOID_MAP_H
#define VOID_MAP_H

static inline void MakeVoid(TileIndex t)
{
	SetTileType(t, MP_VOID);
	SetTileHeight(t, 0);
	_m[t].m1 = 0;
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = 0;
	_m[t].extra = 0;
}

#endif /* VOID_MAP_H */
