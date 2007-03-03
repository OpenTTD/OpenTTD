/* $Id$ */

/** @file fileio.h Declarations for Standard In/Out file operations */

#ifndef FILEIO_H
#define FILEIO_H

void FioSeekTo(uint32 pos, int mode);
void FioSeekToFile(uint32 pos);
uint32 FioGetPos(void);
byte FioReadByte(void);
uint16 FioReadWord(void);
uint32 FioReadDword(void);
void FioCloseAll(void);
FILE *FioFOpenFile(const char *filename);
void FioOpenFile(int slot, const char *filename);
void FioReadBlock(void *ptr, uint size);
void FioSkipBytes(int n);
bool FioCheckFileExists(const char *filename);

#endif /* FILEIO_H */
