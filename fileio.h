#ifndef FILEIO_H
#define FILEIO_H

void FioSeekTo(uint32 pos, int mode);
void FioSeekToFile(uint32 pos);
uint32 FioGetPos(void);
byte FioReadByte(void);
uint16 FioReadWord(void);
uint32 FioReadDword(void);
void FioCloseAll(void);
void FioOpenFile(int slot, const char *filename);
void FioReadBlock(void *ptr, uint size);
void FioSkipBytes(int n);

#endif /* FILEIO_H */
