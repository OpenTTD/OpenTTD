/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "fileio.h"
#include "functions.h"
#include "string.h"
#include "macros.h"
#include "variables.h"

/*************************************************/
/* FILE IO ROUTINES ******************************/
/*************************************************/

#define FIO_BUFFER_SIZE 512

typedef struct {
	byte *buffer, *buffer_end;          ///< position pointer in local buffer and last valid byte of buffer
	uint32 pos;                         ///< current (system) position in file
	FILE *cur_fh;                       ///< current file handle
	FILE *handles[64];                  ///< array of file handles we can have open
	byte buffer_start[FIO_BUFFER_SIZE]; ///< local buffer when read from file
} Fio;

static Fio _fio;

// Get current position in file
uint32 FioGetPos(void)
{
	return _fio.pos + (_fio.buffer - _fio.buffer_start) - FIO_BUFFER_SIZE;
}

void FioSeekTo(uint32 pos, int mode)
{
	if (mode == SEEK_CUR) pos += FioGetPos();
	_fio.buffer = _fio.buffer_end = _fio.buffer_start + FIO_BUFFER_SIZE;
	_fio.pos = pos;
	fseek(_fio.cur_fh, _fio.pos, SEEK_SET);
}

// Seek to a file and a position
void FioSeekToFile(uint32 pos)
{
	FILE *f = _fio.handles[pos >> 24];
	assert(f != NULL);
	_fio.cur_fh = f;
	FioSeekTo(GB(pos, 0, 24), SEEK_SET);
}

byte FioReadByte(void)
{
	if (_fio.buffer == _fio.buffer_end) {
		_fio.pos += FIO_BUFFER_SIZE;
		fread(_fio.buffer = _fio.buffer_start, 1, FIO_BUFFER_SIZE, _fio.cur_fh);
	}
	return *_fio.buffer++;
}

void FioSkipBytes(int n)
{
	for (;;) {
		int m = min(_fio.buffer_end - _fio.buffer, n);
		_fio.buffer += m;
		n -= m;
		if (n == 0) break;
		FioReadByte();
		n--;
	}
}

uint16 FioReadWord(void)
{
	byte b = FioReadByte();
	return (FioReadByte() << 8) | b;
}

uint32 FioReadDword(void)
{
	uint b = FioReadWord();
	return (FioReadWord() << 16) | b;
}

void FioReadBlock(void *ptr, uint size)
{
	FioSeekTo(FioGetPos(), SEEK_SET);
	_fio.pos += size;
	fread(ptr, 1, size, _fio.cur_fh);
}

static inline void FioCloseFile(int slot)
{
	if (_fio.handles[slot] != NULL) {
		fclose(_fio.handles[slot]);
		_fio.handles[slot] = NULL;
	}
}

void FioCloseAll(void)
{
	int i;

	for (i = 0; i != lengthof(_fio.handles); i++)
		FioCloseFile(i);
}

bool FioCheckFileExists(const char *filename)
{
	FILE *f = FioFOpenFile(filename);
	if (f == NULL) return false;

	fclose(f);
	return true;
}

FILE *FioFOpenFile(const char *filename)
{
	FILE *f;
	char buf[MAX_PATH];

	snprintf(buf, lengthof(buf), "%s%s", _path.data_dir, filename);

	f = fopen(buf, "rb");
#if !defined(WIN32)
	if (f == NULL) {
		strtolower(buf + strlen(_path.data_dir) - 1);
		f = fopen(buf, "rb");

#if defined SECOND_DATA_DIR
		// tries in the 2nd data directory
		if (f == NULL) {
			snprintf(buf, lengthof(buf), "%s%s", _path.second_data_dir, filename);
			strtolower(buf + strlen(_path.second_data_dir) - 1);
			f = fopen(buf, "rb");
		}
#endif
	}
#endif

	return f;
}

void FioOpenFile(int slot, const char *filename)
{
	FILE *f = FioFOpenFile(filename);

	if (f == NULL) error("Cannot open file '%s%s'", _path.data_dir, filename);

	FioCloseFile(slot); // if file was opened before, close it
	_fio.handles[slot] = f;
	FioSeekToFile(slot << 24);
}
