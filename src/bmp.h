/* $Id$ */

#ifndef BMP_H
#define BMP_H

typedef struct {
	uint32 offset;       ///< offset of bitmap data from .bmp file begining
	uint32 width;        ///< bitmap width
	uint32 height;       ///< bitmap height
	bool os2_bmp;        ///< true if OS/2 1.x or windows 2.x bitmap
	uint16 bpp;          ///< bits per pixel
	uint32 compression;  ///< compression method (0 = none, 1 = 8-bit RLE, 2 = 4-bit RLE)
	uint32 palette_size; ///< number of colors in palette
} BmpInfo;

typedef struct {
	Colour *palette;
	byte   *bitmap;
} BmpData;

#define BMP_BUFFER_SIZE 1024

typedef struct {
	byte data[BMP_BUFFER_SIZE];
	int pos;
	int read;
	FILE *file;
	uint real_pos;
} BmpBuffer;

void BmpInitializeBuffer(BmpBuffer *buffer, FILE *file);
bool BmpReadHeader(BmpBuffer *buffer, BmpInfo *info, BmpData *data);
bool BmpReadBitmap(BmpBuffer *buffer, BmpInfo *info, BmpData *data);
void BmpDestroyData(BmpData *data);

#endif /* BMP_H */
