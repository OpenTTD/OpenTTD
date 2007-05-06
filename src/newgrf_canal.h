/* $Id$ */

#ifndef NEWGRF_CANAL_H
#define NEWGRF_CANAL_H

/** List of different canal 'features'.
 * Each feature gets an entry in the canal spritegroup table */
enum CanalFeature {
	CF_WATERSLOPE,
	CF_LOCKS,
	CF_DIKES,
	CF_ICON,
	CF_DOCKS,
	CF_END,
};


/** Table of canal 'feature' sprite groups */
extern const SpriteGroup *_canal_sg[CF_END];


/** Lookup the base sprite to use for a canal.
 * @param feature Which canal feature we want.
 * @param tile Tile index of canal, if appropriate.
 * @return Base sprite returned by GRF, or 0 if none.
 */
SpriteID GetCanalSprite(CanalFeature feature, TileIndex tile);

#endif /* NEWGRF_CANAL_H */
