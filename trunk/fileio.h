#ifndef FILEIO_H
#define FILEIO_H

void FioSeekTo(uint32 pos, int mode);
void FioSeekToFile(uint32 pos);
uint32 FioGetPos();
byte FioReadByte();
uint16 FioReadWord();
uint32 FioReadDword();
void FioCloseAll();
void FioOpenFile(int slot, const char *filename);
void FioReadBlock(void *ptr, uint size);
void FioSkipBytes(int n);

#endif /* FILEIO_H */
