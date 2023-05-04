/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file oldloader.h Declarations of strctures and function used in loader of old savegames */

#ifndef OLDLOADER_H
#define OLDLOADER_H

#include "saveload.h"
#include "../tile_type.h"

static const uint BUFFER_SIZE = 4096;
static const uint OLD_MAP_SIZE = 256 * 256;

struct LoadgameState {
	FILE *file;

	uint chunk_size;

	bool decoding;
	byte decode_char;

	uint buffer_count;
	uint buffer_cur;
	byte buffer[BUFFER_SIZE];

	uint total_read;
};

/* OldChunk-Type */
enum OldChunkType {
	OC_SIMPLE    = 0,
	OC_NULL      = 1,
	OC_CHUNK     = 2,
	OC_ASSERT    = 3,
	/* 4 bits allocated (16 max) */

	OC_TTD       = 1 << 4, ///< chunk is valid ONLY for TTD savegames
	OC_TTO       = 1 << 5, ///< -//- TTO (default is neither of these)
	/* 4 bits allocated */

	OC_VAR_I8    = 1 << 8,
	OC_VAR_U8    = 2 << 8,
	OC_VAR_I16   = 3 << 8,
	OC_VAR_U16   = 4 << 8,
	OC_VAR_I32   = 5 << 8,
	OC_VAR_U32   = 6 << 8,
	OC_VAR_I64   = 7 << 8,
	OC_VAR_U64   = 8 << 8,
	/* 8 bits allocated (256 max) */

	OC_FILE_I8   = 1 << 16,
	OC_FILE_U8   = 2 << 16,
	OC_FILE_I16  = 3 << 16,
	OC_FILE_U16  = 4 << 16,
	OC_FILE_I32  = 5 << 16,
	OC_FILE_U32  = 6 << 16,
	/* 8 bits allocated (256 max) */

	OC_INT8      = OC_VAR_I8   | OC_FILE_I8,
	OC_UINT8     = OC_VAR_U8   | OC_FILE_U8,
	OC_INT16     = OC_VAR_I16  | OC_FILE_I16,
	OC_UINT16    = OC_VAR_U16  | OC_FILE_U16,
	OC_INT32     = OC_VAR_I32  | OC_FILE_I32,
	OC_UINT32    = OC_VAR_U32  | OC_FILE_U32,

	OC_TILE      = OC_VAR_U32  | OC_FILE_U16,

	/**
	 * Dereference the pointer once before writing to it,
	 * so we do not have to use big static arrays.
	 */
	OC_DEREFERENCE_POINTER = 1 << 31,

	OC_END       = 0, ///< End of the whole chunk, all 32 bits set to zero
};

DECLARE_ENUM_AS_BIT_SET(OldChunkType)

typedef bool OldChunkProc(LoadgameState *ls, int num);
typedef void *OffsetProc(void *base);

struct OldChunks {
	OldChunkType type;   ///< Type of field
	uint32_t amount;       ///< Amount of fields

	void *ptr;           ///< Pointer where to save the data (takes precedence over #offset)
	OffsetProc *offset;  ///< Pointer to function that returns the actual memory address of a member (ignored if #ptr is not nullptr)
	OldChunkProc *proc;  ///< Pointer to function that is called with OC_CHUNK
};

extern uint _bump_assert_value;
byte ReadByte(LoadgameState *ls);
bool LoadChunk(LoadgameState *ls, void *base, const OldChunks *chunks);

bool LoadTTDMain(LoadgameState *ls);
bool LoadTTOMain(LoadgameState *ls);

static inline uint16_t ReadUint16(LoadgameState *ls)
{
	byte x = ReadByte(ls);
	return x | ReadByte(ls) << 8;
}

static inline uint32_t ReadUint32(LoadgameState *ls)
{
	uint16_t x = ReadUint16(ls);
	return x | ReadUint16(ls) << 16;
}

/* Help:
 *  - OCL_SVAR: load 'type' to offset 'offset' in a struct of type 'base', which must also
 *       be given via base in LoadChunk() as real pointer
 *  - OCL_VAR: load 'type' to a global var
 *  - OCL_END: every struct must end with this
 *  - OCL_NULL: read 'amount' of bytes and send them to /dev/null or something
 *  - OCL_CHUNK: load another proc to load a part of the savegame, 'amount' times
 *  - OCL_ASSERT: to check if we are really at the place we expect to be.. because old savegames are too binary to be sure ;)
 */
#define OCL_SVAR(type, base, offset)         { type,                 1, nullptr, [] (void *b) -> void * { return std::addressof(static_cast<base *>(b)->offset); }, nullptr }
#define OCL_VAR(type, amount, pointer)       { type,            amount, pointer, nullptr, nullptr }
#define OCL_END()                            { OC_END,               0, nullptr, nullptr, nullptr }
#define OCL_CNULL(type, amount)              { OC_NULL | type,  amount, nullptr, nullptr, nullptr }
#define OCL_CCHUNK(type, amount, proc)       { OC_CHUNK | type, amount, nullptr, nullptr, proc }
#define OCL_ASSERT(type, size)               { OC_ASSERT | type,     1, (void *)(size_t)size, nullptr, nullptr }
#define OCL_NULL(amount)        OCL_CNULL((OldChunkType)0, amount)
#define OCL_CHUNK(amount, proc) OCL_CCHUNK((OldChunkType)0, amount, proc)

#endif /* OLDLOADER_H */
