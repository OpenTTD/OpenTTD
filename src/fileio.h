/* $Id$ */

/** @file fileio.h Declarations for Standard In/Out file operations */

#ifndef FILEIO_H
#define FILEIO_H

void FioSeekTo(uint32 pos, int mode);
void FioSeekToFile(uint32 pos);
uint32 FioGetPos();
const char *FioGetFilename();
byte FioReadByte();
uint16 FioReadWord();
uint32 FioReadDword();
void FioCloseAll();
void FioOpenFile(int slot, const char *filename);
void FioReadBlock(void *ptr, uint size);
void FioSkipBytes(int n);

FILE *FioFOpenFile(const char *filename);
bool FioCheckFileExists(const char *filename);
void FioCreateDirectory(const char *filename);

void SanitizeFilename(char *filename);
void AppendPathSeparator(char *buf, size_t buflen);
void DeterminePaths(const char *exe);

#endif /* FILEIO_H */
