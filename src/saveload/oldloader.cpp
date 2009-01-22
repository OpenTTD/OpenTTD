/* $Id$ */

/** @file oldloader.cpp Functions for handling of TTO/TTD/TTDP savegames. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../tile_type.h"
#include "../debug.h"
#include "../strings_type.h"
#include "../string_func.h"
#include "../settings_type.h"

#include "table/strings.h"

#include "saveload.h"
#include "saveload_internal.h"
#include "oldloader.h"

enum {
	HEADER_SIZE = 49,
};

uint32 _bump_assert_value;

static inline OldChunkType GetOldChunkType(OldChunkType type)     {return (OldChunkType)GB(type, 0, 8);}
static inline OldChunkType GetOldChunkVarType(OldChunkType type)  {return (OldChunkType)(GB(type, 8, 8) << 8);}
static inline OldChunkType GetOldChunkFileType(OldChunkType type) {return (OldChunkType)(GB(type, 16, 8) << 16);}

static inline byte CalcOldVarLen(OldChunkType type)
{
	static const byte type_mem_size[] = {0, 1, 1, 2, 2, 4, 4, 8};
	byte length = GB(type, 8, 8);
	assert(length != 0 && length < lengthof(type_mem_size));
	return type_mem_size[length];
}

/**
 *
 * Reads a byte from a file (do not call yourself, use ReadByte())
 *
 */
static byte ReadByteFromFile(LoadgameState *ls)
{
	/* To avoid slow reads, we read BUFFER_SIZE of bytes per time
	and just return a byte per time */
	if (ls->buffer_cur >= ls->buffer_count) {
		/* Read some new bytes from the file */
		int count = (int)fread(ls->buffer, 1, BUFFER_SIZE, ls->file);

		/* We tried to read, but there is nothing in the file anymore.. */
		if (count == 0) {
			DEBUG(oldloader, 0, "Read past end of file, loading failed");
			ls->failed = true;
		}

		ls->buffer_count = count;
		ls->buffer_cur   = 0;
	}

	return ls->buffer[ls->buffer_cur++];
}

/**
 *
 * Reads a byte from the buffer and decompress if needed
 *
 */
byte ReadByte(LoadgameState *ls)
{
	/* Old savegames have a nice compression algorithm (RLE)
	which means that we have a chunk, which starts with a length
	byte. If that byte is negative, we have to repeat the next byte
	that many times ( + 1). Else, we need to read that amount of bytes.
	Works pretty good if you have many zero's behind eachother */

	if (ls->chunk_size == 0) {
		/* Read new chunk */
		int8 new_byte = ReadByteFromFile(ls);

		if (new_byte < 0) {
			/* Repeat next char for new_byte times */
			ls->decoding    = true;
			ls->decode_char = ReadByteFromFile(ls);
			ls->chunk_size  = -new_byte + 1;
		} else {
			ls->decoding    = false;
			ls->chunk_size  = new_byte + 1;
		}
	}

	ls->total_read++;
	ls->chunk_size--;

	return ls->decoding ? ls->decode_char : ReadByteFromFile(ls);
}

/**
 *
 * Loads a chunk from the old savegame
 *
 */
bool LoadChunk(LoadgameState *ls, void *base, const OldChunks *chunks)
{
	const OldChunks *chunk = chunks;
	byte *base_ptr = (byte*)base;

	while (chunk->type != OC_END) {
		byte *ptr = (byte*)chunk->ptr;
		if ((chunk->type & OC_DEREFERENCE_POINTER) != 0) ptr = *(byte**)ptr;

		for (uint i = 0; i < chunk->amount; i++) {
			if (ls->failed) return false;

			/* Handle simple types */
			if (GetOldChunkType(chunk->type) != 0) {
				switch (GetOldChunkType(chunk->type)) {
					/* Just read the byte and forget about it */
					case OC_NULL: ReadByte(ls); break;

					case OC_CHUNK:
						/* Call function, with 'i' as parameter to tell which item we
						 * are going to read */
						if (!chunk->proc(ls, i)) return false;
						break;

					case OC_ASSERT:
						DEBUG(oldloader, 4, "Assert point: 0x%X / 0x%X", ls->total_read, chunk->offset + _bump_assert_value);
						if (ls->total_read != chunk->offset + _bump_assert_value) ls->failed = true;
					default: break;
				}
			} else {
				uint64 res = 0;

				/* Reading from the file: bits 16 to 23 have the FILE type */
				switch (GetOldChunkFileType(chunk->type)) {
					case OC_FILE_I8:  res = (int8)ReadByte(ls); break;
					case OC_FILE_U8:  res = ReadByte(ls); break;
					case OC_FILE_I16: res = (int16)ReadUint16(ls); break;
					case OC_FILE_U16: res = ReadUint16(ls); break;
					case OC_FILE_I32: res = (int32)ReadUint32(ls); break;
					case OC_FILE_U32: res = ReadUint32(ls); break;
					default: NOT_REACHED();
				}

				/* Sanity check */
				assert(base_ptr != NULL || chunk->ptr != NULL);

				/* Writing to the var: bits 8 to 15 have the VAR type */
				if (chunk->ptr == NULL) ptr = base_ptr + chunk->offset;

				/* Write the data */
				switch (GetOldChunkVarType(chunk->type)) {
					case OC_VAR_I8: *(int8  *)ptr = GB(res, 0, 8); break;
					case OC_VAR_U8: *(uint8 *)ptr = GB(res, 0, 8); break;
					case OC_VAR_I16:*(int16 *)ptr = GB(res, 0, 16); break;
					case OC_VAR_U16:*(uint16*)ptr = GB(res, 0, 16); break;
					case OC_VAR_I32:*(int32 *)ptr = res; break;
					case OC_VAR_U32:*(uint32*)ptr = res; break;
					case OC_VAR_I64:*(int64 *)ptr = res; break;
					default: NOT_REACHED();
				}

				/* Increase pointer base for arrays when looping */
				if (chunk->amount > 1 && chunk->ptr != NULL) ptr += CalcOldVarLen(chunk->type);
			}
		}

		chunk++;
	}

	return true;
}

/**
 *
 * Initialize some data before reading
 *
 */
static void InitLoading(LoadgameState *ls)
{
	ls->chunk_size   = 0;
	ls->total_read   = 0;
	ls->failed       = false;

	ls->decoding     = false;
	ls->decode_char  = 0;

	ls->buffer_cur   = 0;
	ls->buffer_count = 0;
	memset(ls->buffer, 0, BUFFER_SIZE);

	_bump_assert_value = 0;

	_settings_game.construction.freeform_edges = false; // disable so we can convert map array (SetTileType is still used)
}



/**
 * Verifies the title has a valid checksum
 * @param title title and checksum
 * @return true iff the title is valid
 * @note the title (incl. checksum) has to be at least 49 (HEADER_SIZE) bytes long!
 */
static bool VerifyOldNameChecksum(char *title)
{
	uint16 sum = 0;
	for (uint i = 0; i < HEADER_SIZE - 2; i++) {
		sum += title[i];
		sum = ROL(sum, 1);
	}

	sum ^= 0xAAAA; // computed checksum

	uint16 sum2 = title[HEADER_SIZE - 2]; // checksum in file
	SB(sum2, 8, 8, title[HEADER_SIZE - 1]);

	return sum == sum2;
}

bool LoadOldSaveGame(const char *file)
{
	LoadgameState ls;

	DEBUG(oldloader, 3, "Trying to load a TTD(Patch) savegame");

	InitLoading(&ls);

	/* Open file */
	ls.file = fopen(file, "rb");

	if (ls.file == NULL) {
		DEBUG(oldloader, 0, "Cannot open file '%s'", file);
		return false;
	}

	char temp[HEADER_SIZE];
	if (fread(temp, 1, HEADER_SIZE, ls.file) != HEADER_SIZE || !VerifyOldNameChecksum(temp) || !LoadOldMain(&ls)) {
		SetSaveLoadError(STR_GAME_SAVELOAD_ERROR_DATA_INTEGRITY_CHECK_FAILED);
		fclose(ls.file);
		return false;
	}

	_pause_game = 2;

	return true;
}

void GetOldSaveGameName(const char *path, const char *file, char *title, const char *last)
{
	char filename[MAX_PATH];
	char temp[HEADER_SIZE];

	snprintf(filename, lengthof(filename), "%s" PATHSEP "%s", path, file);
	FILE *f = fopen(filename, "rb");
	temp[0] = '\0'; // name is nul-terminated in savegame ...

	if (f == NULL) {
		*title = '\0';
		return;
	}

	bool broken = (fread(temp, 1, HEADER_SIZE, f) != HEADER_SIZE || !VerifyOldNameChecksum(temp));

	temp[HEADER_SIZE - 2] = '\0'; // ... but it's better to be sure

	if (broken) title = strecpy(title, "(broken) ", last);
	title = strecpy(title, temp, last);

	fclose(f);
}
