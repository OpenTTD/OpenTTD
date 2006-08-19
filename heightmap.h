/* $Id$ */

#ifndef HEIGHTMAP_H
#define HEIGHTMAP_H

/*
 * Order of these enums has to be the same as in lang/english.txt
 * Otherwise you will get inconsistent behaviour.
 */
enum {
	HM_COUNTER_CLOCKWISE, //! Rotate the map counter clockwise 45 degrees
	HM_CLOCKWISE,         //! Rotate the map clockwise 45 degrees
};

/**
 * Get the dimensions of a heightmap.
 * @return Returns false if loading of the image failed.
 */
bool GetHeightmapDimensions(char *filename, uint *x, uint *y);

/**
 * Load a heightmap from file and change the map in his current dimensions
 *  to a landscape representing the heightmap.
 * It converts pixels to height. The brighter, the higher.
 */
void LoadHeightmap(char *filename);

/**
 * Make an empty world where all tiles are of height 'tile_height'.
 */
void FlatEmptyWorld(byte tile_height);

#endif /* HEIGHTMAP_H */
